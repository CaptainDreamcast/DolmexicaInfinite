#include "mugenstatereader.h"

#include <assert.h>

#include <prism/log.h>
#include <prism/system.h>
#include <prism/memoryhandler.h>
#include <prism/mugendefreader.h>
#include <prism/math.h>
#include <prism/stlutil.h>

#include "mugenstatecontrollers.h"

using namespace std;

static int isMugenStateDef(const char* tName) {
	char firstW[100];
	sscanf(tName, "%s", firstW);
	turnStringLowercase(firstW);
	return !strcmp("statedef", firstW);
}

static MugenDefScriptGroup* getFirstStateDefGroup(MugenDefScript* tScript) {
	MugenDefScriptGroup* cur = tScript->mFirstGroup;

	while (cur != NULL) {
		if (isMugenStateDef(cur->mName.data())) return cur;
		cur = cur->mNext;
	}

	logWarning("Unable to find first state definition. Returning NULL.");

	return NULL;
}

static struct {
	int mCurrentGroup;

} gMugenStateDefParseState;

static void handleMugenStateDefType(DreamMugenState* tState, MugenDefScriptGroupElement* tElement) {
	char* type = getAllocatedMugenDefStringVariableAsElement(tElement);
	turnStringLowercase(type);

	if (!strcmp("s", type)) {
		tState->mType = MUGEN_STATE_TYPE_STANDING;
	} else if (!strcmp("c", type)) {
		tState->mType = MUGEN_STATE_TYPE_CROUCHING;
	} else if (!strcmp("a", type)) {
		tState->mType = MUGEN_STATE_TYPE_AIR;
	} else if (!strcmp("l", type)) {
		tState->mType = MUGEN_STATE_TYPE_LYING;
	}
	else if (!strcmp("u", type)) {
		tState->mType = MUGEN_STATE_TYPE_UNCHANGED;
	}
	else {
		logWarningFormat("Unable to determine Mugen state type %s. Defaulting to unchanged.", type);
		tState->mType = MUGEN_STATE_TYPE_UNCHANGED;
	}

	freeMemory(type);
}

static void handleMugenStateDefMoveType(DreamMugenState* tState, MugenDefScriptGroupElement* tElement) {
	char* moveType = getAllocatedMugenDefStringVariableAsElement(tElement);
	turnStringLowercase(moveType);

	if (!strcmp("a", moveType)) {
		tState->mMoveType = MUGEN_STATE_MOVE_TYPE_ATTACK;
	} else if (!strcmp("i", moveType)) {
		tState->mMoveType = MUGEN_STATE_MOVE_TYPE_IDLE;
	}
	else if (!strcmp("h", moveType)) {
		tState->mMoveType = MUGEN_STATE_MOVE_TYPE_BEING_HIT;
	}
	else if (!strcmp("u", moveType)) {
		tState->mMoveType = MUGEN_STATE_MOVE_TYPE_UNCHANGED;
	}
	else {
		logWarningFormat("Unable to determine Mugen state move type %s. Defaulting to unchanged.", moveType);
		tState->mMoveType = MUGEN_STATE_MOVE_TYPE_UNCHANGED;
	}

	freeMemory(moveType);
}

static void handleMugenStateDefPhysics(DreamMugenState* tState, MugenDefScriptGroupElement* tElement) {
	char* physics = getAllocatedMugenDefStringVariableAsElement(tElement);
	turnStringLowercase(physics);

	if (!strcmp("u", physics)) {
		tState->mPhysics = MUGEN_STATE_PHYSICS_UNCHANGED;
	} else if (!strcmp("s", physics)) {
		tState->mPhysics = MUGEN_STATE_PHYSICS_STANDING;
	}
	else if (!strcmp("c", physics)) {
		tState->mPhysics = MUGEN_STATE_PHYSICS_CROUCHING;
	}
	else if (!strcmp("a", physics)) {
		tState->mPhysics = MUGEN_STATE_PHYSICS_AIR;
	}
	else if (!strcmp("n", physics)) {
		tState->mPhysics = MUGEN_STATE_PHYSICS_NONE;
	}
	else {
		logWarningFormat("Unable to determine Mugen state physics %s. Defaulting to unchanged.", physics);
		tState->mPhysics = MUGEN_STATE_PHYSICS_UNCHANGED;
	}

	freeMemory(physics);
}


static void handleMugenStateDefAnimation(DreamMugenState* tState, MugenDefScriptGroupElement* tElement, MugenDefScriptGroup* tGroup) {
	tState->mIsChangingAnimation = 1;
	assert(fetchDreamAssignmentFromGroupAndReturnWhetherItExists(tElement->mName.data(), tGroup, &tState->mAnimation));
} 

static void handleMugenStateDefVelocitySetting(DreamMugenState* tState, MugenDefScriptGroupElement* tElement, MugenDefScriptGroup* tGroup) {
	tState->mIsSettingVelocity = 1;
	assert(fetchDreamAssignmentFromGroupAndReturnWhetherItExists(tElement->mName.data(), tGroup, &tState->mVelocity));
}

static void handleMugenStateDefControl(DreamMugenState* tState, MugenDefScriptGroupElement* tElement, MugenDefScriptGroup* tGroup) {
	tState->mIsChangingControl = 1;
	assert(fetchDreamAssignmentFromGroupAndReturnWhetherItExists(tElement->mName.data(), tGroup, &tState->mControl));
}

static void handleMugenStateSpritePriority(DreamMugenState* tState, MugenDefScriptGroupElement* tElement, MugenDefScriptGroup* tGroup) {
	tState->mIsChangingSpritePriority = 1;
	assert(fetchDreamAssignmentFromGroupAndReturnWhetherItExists(tElement->mName.data(), tGroup, &tState->mSpritePriority));
}

static void handleMugenStatePowerAdd(DreamMugenState* tState, MugenDefScriptGroupElement* tElement, MugenDefScriptGroup* tGroup) {
	tState->mIsAddingPower = 1;
	assert(fetchDreamAssignmentFromGroupAndReturnWhetherItExists(tElement->mName.data(), tGroup, &tState->mPowerAdd));
}

static void handleMugenStateJuggle(DreamMugenState* tState, MugenDefScriptGroupElement* tElement, MugenDefScriptGroup* tGroup) {
	tState->mDoesRequireJuggle = 1;
	assert(fetchDreamAssignmentFromGroupAndReturnWhetherItExists(tElement->mName.data(), tGroup, &tState->mJuggleRequired));
}

static void handleMugenStateHitDefPersistence(DreamMugenState* tState, MugenDefScriptGroupElement* tElement, MugenDefScriptGroup* tGroup) {
	tState->mDoesHaveHitDefinitionsPersist = 1;
	assert(fetchDreamAssignmentFromGroupAndReturnWhetherItExists(tElement->mName.data(), tGroup, &tState->mDoHitDefinitionsPersist));
}

static void handleMugenStateMoveHitPersistence(DreamMugenState* tState, MugenDefScriptGroupElement* tElement, MugenDefScriptGroup* tGroup) {
	tState->mDoesHaveMoveHitInfosPersist = 1;
	assert(fetchDreamAssignmentFromGroupAndReturnWhetherItExists(tElement->mName.data(), tGroup, &tState->mDoMoveHitInfosPersist));
}

static void handleMugenStateHitCountPersistence(DreamMugenState* tState, MugenDefScriptGroupElement* tElement, MugenDefScriptGroup* tGroup) {
	tState->mDoesHaveHitCountPersist = 1;
	assert(fetchDreamAssignmentFromGroupAndReturnWhetherItExists(tElement->mName.data(), tGroup, &tState->mDoesHitCountPersist));
}

static void handleMugenStateFacePlayer2(DreamMugenState* tState, MugenDefScriptGroupElement* tElement, MugenDefScriptGroup* tGroup) {
	tState->mHasFacePlayer2Info = 1;
	assert(fetchDreamAssignmentFromGroupAndReturnWhetherItExists(tElement->mName.data(), tGroup, &tState->mDoesFacePlayer2));
}

static void handleMugenStatePriority(DreamMugenState* tState, MugenDefScriptGroupElement* tElement, MugenDefScriptGroup* tGroup) {
	tState->mHasPriority = 1;
	assert(fetchDreamAssignmentFromGroupAndReturnWhetherItExists(tElement->mName.data(), tGroup, &tState->mPriority));
}

typedef struct {
	DreamMugenState* mState;
	MugenDefScriptGroup* mGroup;
} MugenStateDefCaller;


static void handleSingleMugenStateDefElement(MugenStateDefCaller* tCaller, const string& tKey, MugenDefScriptGroupElement& tData) {
	(void)tKey;
	MugenStateDefCaller* caller = (MugenStateDefCaller*)tCaller;
	DreamMugenState* state = caller->mState;
	MugenDefScriptGroup* group = caller->mGroup;

	MugenDefScriptGroupElement* e = &tData;

	if (e->mName == "type") {
		handleMugenStateDefType(state, e);
	} else if (e->mName == "movetype") {
		handleMugenStateDefMoveType(state, e);
	}
	else if (e->mName == "physics") {
		handleMugenStateDefPhysics(state, e);
	}
	else if (e->mName == "anim") {
		handleMugenStateDefAnimation(state, e, group);
	}
	else if (e->mName == "velset") {
		handleMugenStateDefVelocitySetting(state, e, group);
	}
	else if (e->mName == "ctrl") {
		handleMugenStateDefControl(state, e, group);
	}
	else if (e->mName == "sprpriority") {
		handleMugenStateSpritePriority(state, e, group);
	}
	else if (e->mName == "poweradd") {
		handleMugenStatePowerAdd(state, e, group);
	}
	else if (e->mName == "juggle") {
		handleMugenStateJuggle(state, e, group);
	}
	else if (e->mName == "hitdefpersist") {
		handleMugenStateHitDefPersistence(state, e, group);
	}
	else if (e->mName == "movehitpersist") {
		handleMugenStateMoveHitPersistence(state, e, group);
	}
	else if (e->mName == "hitcountpersist") {
		handleMugenStateHitCountPersistence(state, e, group);
	}
	else if (e->mName == "facep2") {
		handleMugenStateFacePlayer2(state, e, group);
	}
	else if (e->mName == "priority") {
		handleMugenStatePriority(state, e, group);
	}
	else {
		logWarning("Unable to determine state def element.");
		logWarningString(e->mName.data());
	}
}

static void removeState(DreamMugenStates* tStates, int tState) {
	DreamMugenState* e = &tStates->mStates[tState];
	// unloadSingleState(e); // TODO: reinsert
	tStates->mStates.erase(tState);
}

static void handleMugenStateDef(DreamMugenStates* tStates, MugenDefScriptGroup* tGroup) {

	DreamMugenState state;

	char dummy[100];
	sscanf(tGroup->mName.data(), "%s %d", dummy, &state.mID);
	gMugenStateDefParseState.mCurrentGroup = state.mID;

	state.mType = MUGEN_STATE_TYPE_STANDING;
	state.mMoveType = MUGEN_STATE_MOVE_TYPE_IDLE;
	state.mPhysics = MUGEN_STATE_PHYSICS_NONE;
	state.mIsChangingAnimation = 0;
	state.mIsSettingVelocity = 0;
	state.mIsChangingControl = 0;
	state.mIsChangingSpritePriority = 0;
	state.mIsAddingPower = 0;
	state.mDoesRequireJuggle = 0;
	state.mDoesHaveHitDefinitionsPersist = 0;
	state.mDoesHaveMoveHitInfosPersist = 0;
	state.mDoesHaveHitCountPersist = 0;
	state.mHasFacePlayer2Info = 0;
	state.mHasPriority = 0;

	state.mControllers = new_vector();

	MugenStateDefCaller caller;
	caller.mState = &state;
	caller.mGroup = tGroup;
	stl_string_map_map(tGroup->mElements, handleSingleMugenStateDefElement, &caller);

	if (stl_map_contains(tStates->mStates, state.mID)) {
		removeState(tStates, state.mID); // TODO
	}
	tStates->mStates[state.mID] = state;
}

static int isMugenStateController(const char* tName) {
	char firstW[100];
	sscanf(tName, "%s", firstW);
	turnStringLowercase(firstW);
	return !strcmp("state", firstW);
}

static void handleMugenStateControllerInDefGroup(DreamMugenStates* tStates, MugenDefScriptGroup* tGroup) {
	DreamMugenState* state = &tStates->mStates[gMugenStateDefParseState.mCurrentGroup];

	DreamMugenStateController* controller = parseDreamMugenStateControllerFromGroup(tGroup);

	vector_push_back_owned(&state->mControllers, controller);
}

static void handleSingleMugenStateDefGroup(DreamMugenStates* tStates, MugenDefScriptGroup* tGroup) {

	if (isMugenStateDef(tGroup->mName.data())) {
		handleMugenStateDef(tStates, tGroup);
	} else if (isMugenStateController(tGroup->mName.data())) {
		handleMugenStateControllerInDefGroup(tStates, tGroup);
	}
	else {
		logWarningFormat("Unable to determine state def group type %s. Ignoring.", tGroup->mName.data());
	}

}

static void loadMugenStateDefinitionsFromScript(DreamMugenStates* tStates, MugenDefScript* tScript) {
	MugenDefScriptGroup* current = getFirstStateDefGroup(tScript);

	while (current != NULL) {
		handleSingleMugenStateDefGroup(tStates, current);
		
		current = current->mNext;
	}
}

void loadDreamMugenStateDefinitionsFromFile(DreamMugenStates* tStates, char* tPath) {
	MugenDefScript script; 
	loadMugenDefScript(&script, tPath);
	loadMugenStateDefinitionsFromScript(tStates, &script);
	unloadMugenDefScript(script);
}

DreamMugenStates createEmptyMugenStates() {
	DreamMugenStates ret;
	stl_new_map(ret.mStates);
	return ret;
}

static DreamMugenConstants makeEmptyMugenConstants() {
	DreamMugenConstants ret;
	ret.mStates = createEmptyMugenStates();
	return ret;
}


static void loadSingleSparkNumber(int* oIsSparkInPlayerFile, int* oSparkNumber, MugenDefScript* tScript, char* tGroup, char* tName, int tDefaultNumber) {
	if (!isMugenDefStringVariable(tScript, tGroup, tName)) {
		*oIsSparkInPlayerFile = 0;
		*oSparkNumber = tDefaultNumber;
		return;
	}

	char* item = getAllocatedMugenDefStringVariable(tScript, tGroup, tName);
	turnStringLowercase(item);

	if (item[0] == 's') {
		*oIsSparkInPlayerFile = 1;
		*oSparkNumber = atoi(item + 1);
	}
	else {
		*oIsSparkInPlayerFile = 0;
		*oSparkNumber = atoi(item);
	}

	freeMemory(item);
}

static void loadMugenConstantsHeader(DreamMugenConstantsHeader* tHeader, MugenDefScript* tScript) {
	tHeader->mLife = getMugenDefIntegerOrDefault(tScript, "Data", "life", 1000);
	tHeader->mPower = getMugenDefIntegerOrDefault(tScript, "Data", "power", 3000);
	tHeader->mAttack = getMugenDefIntegerOrDefault(tScript, "Data", "attack", 100);
	tHeader->mDefense = getMugenDefIntegerOrDefault(tScript, "Data", "defence", 100);
	tHeader->mFallDefenseUp = getMugenDefIntegerOrDefault(tScript, "Data", "fall.defence_up", 50);
	tHeader->mLiedownTime = getMugenDefFloatOrDefault(tScript, "Data", "liedown.time", 60);
	tHeader->mAirJugglePoints = getMugenDefIntegerOrDefault(tScript, "Data", "airjuggle", 15);

	loadSingleSparkNumber(&tHeader->mIsSparkNoInPlayerFile, &tHeader->mSparkNo, tScript, "Data", "sparkno", 2);
	loadSingleSparkNumber(&tHeader->mIsGuardSparkNoInPlayerFile, &tHeader->mGuardSparkNo, tScript, "Data", "guard.sparkno", 40);
	tHeader->mKOEcho = getMugenDefIntegerOrDefault(tScript, "Data", "KO.echo", 0);
	tHeader->mVolume = getMugenDefIntegerOrDefault(tScript, "Data", "volume", 0);

	tHeader->mIntPersistIndex = getMugenDefIntegerOrDefault(tScript, "Data", "IntPersistIndex", 60);
	tHeader->mFloatPersistIndex = getMugenDefIntegerOrDefault(tScript, "Data", "FloatPersistIndex", 40);
}

static int getMugenDefVectorIAndReturnWhetherItExists(MugenDefScript* tScript, char* tGroupName, char* tVariableName, Vector3DI* oVector) {
	if (!isMugenDefVectorIVariable(tScript, tGroupName, tVariableName)) return 0;
	*oVector = getMugenDefVectorIVariable(tScript, tGroupName, tVariableName);
	return 1;
}

static void loadMugenConstantsSizeData(DreamMugenConstantsSizeData* tSizeData, MugenDefScript* tScript) {
	tSizeData->mScale = makePosition(1, 1, 1);
	tSizeData->mScale.x = getMugenDefFloatOrDefault(tScript, "Size", "xscale", 1);
	tSizeData->mScale.y = getMugenDefFloatOrDefault(tScript, "Size", "yscale", 1);
	tSizeData->mGroundBackWidth = getMugenDefIntegerOrDefault(tScript, "Size", "ground.back", 15);
	tSizeData->mGroundFrontWidth = getMugenDefIntegerOrDefault(tScript, "Size", "ground.front", 16);
	tSizeData->mAirBackWidth = getMugenDefIntegerOrDefault(tScript, "Size", "air.back", 12);
	tSizeData->mAirFrontWidth = getMugenDefIntegerOrDefault(tScript, "Size", "air.front", 12);
	tSizeData->mHeight = getMugenDefIntegerOrDefault(tScript, "Size", "height", 60);
	tSizeData->mAttackDistance = getMugenDefIntegerOrDefault(tScript, "Size", "attack.dist", 160);
	tSizeData->mProjectileAttackDistance = getMugenDefIntegerOrDefault(tScript, "Size", "proj.attack.dist", 90);
	tSizeData->mDoesScaleProjectiles = getMugenDefIntegerOrDefault(tScript, "Size", "proj.doscale", 0);
	tSizeData->mHeadPosition = getMugenDefVectorOrDefault(tScript, "Size", "head.pos", makePosition(-5, -90, 0));
	tSizeData->mMidPosition = getMugenDefVectorOrDefault(tScript, "Size", "mid.pos", makePosition(-5, -60, 0));
	tSizeData->mShadowOffset = getMugenDefIntegerOrDefault(tScript, "Size", "shadowoffset", 0);
	tSizeData->mDrawOffset = getMugenDefVectorIOrDefault(tScript, "Size", "draw.offset", makeVector3DI(0, 0, 0));

	tSizeData->mHasAttackWidth = getMugenDefVectorIAndReturnWhetherItExists(tScript, "Size", "attack.width", &tSizeData->mAttackWidth);
	
}


static void loadMugenConstantsVelocityData(DreamMugenConstantsVelocityData* tVelocityData, MugenDefScript* tScript) {
	tVelocityData->mWalkForward = getMugenDefVectorOrDefault(tScript, "Velocity", "walk.fwd", makePosition(2.4, 0, 0));
	tVelocityData->mWalkBackward = getMugenDefVectorOrDefault(tScript, "Velocity", "walk.back", makePosition(-2.2, 0, 0));

	tVelocityData->mRunForward = getMugenDefVectorOrDefault(tScript, "Velocity", "run.fwd", makePosition(4.6, 0, 0));
	tVelocityData->mRunBackward = getMugenDefVectorOrDefault(tScript, "Velocity", "run.back", makePosition(-4.5, -3.8, 0));

	tVelocityData->mJumpNeutral = getMugenDefVectorOrDefault(tScript, "Velocity", "jump.neu", makePosition(0, -8.4, 0));
	tVelocityData->mJumpBackward = getMugenDefVectorOrDefault(tScript, "Velocity", "jump.back", makePosition(-2.55, 0, 0));
	tVelocityData->mJumpForward = getMugenDefVectorOrDefault(tScript, "Velocity", "jump.fwd", makePosition(2.5, 0, 0));

	tVelocityData->mRunJumpBackward = getMugenDefVectorOrDefault(tScript, "Velocity", "runjump.back", makePosition(-2.55, -8.1, 0));
	tVelocityData->mRunJumpForward = getMugenDefVectorOrDefault(tScript, "Velocity", "runjump.fwd", makePosition(4, -8.1, 0));

	tVelocityData->mAirJumpNeutral = getMugenDefVectorOrDefault(tScript, "Velocity", "airjump.neu", makePosition(0, -8.1, 0));
	tVelocityData->mAirJumpBackward = getMugenDefVectorOrDefault(tScript, "Velocity", "airjump.back", makePosition(-2.55, 0, 0));
	tVelocityData->mAirJumpForward = getMugenDefVectorOrDefault(tScript, "Velocity", "airjump.fwd", makePosition(2.5, 0, 0));

	tVelocityData->mAirGetHitGroundRecovery = getMugenDefVectorOrDefault(tScript, "Velocity", "air.gethit.groundrecover", makePosition(-0.15, -3.5, 0));
	tVelocityData->mAirGetHitAirRecoveryMultiplier = getMugenDefVectorOrDefault(tScript, "Velocity", "air.gethit.airrecover.mul", makePosition(0.5, 0.2, 1));
	tVelocityData->mAirGetHitAirRecoveryOffset = getMugenDefVectorOrDefault(tScript, "Velocity", "air.gethit.airrecover.add", makePosition(0, -4.5, 0));

	tVelocityData->mAirGetHitExtraXWhenHoldingBackward = getMugenDefFloatOrDefault(tScript, "Velocity", "air.gethit.airrecover.back", -1);
	tVelocityData->mAirGetHitExtraXWhenHoldingForward = getMugenDefFloatOrDefault(tScript, "Velocity", "air.gethit.airrecover.fwd", 0);
	tVelocityData->mAirGetHitExtraYWhenHoldingUp = getMugenDefFloatOrDefault(tScript, "Velocity", "air.gethit.airrecover.up", -2);
	tVelocityData->mAirGetHitExtraYWhenHoldingDown = getMugenDefFloatOrDefault(tScript, "Velocity", "air.gethit.airrecover.down", 1.5);
}

static void loadMugenConstantsMovementData(DreamMugenConstantsMovementData* tMovementData, MugenDefScript* tScript) {
	tMovementData->mAllowedAirJumpAmount = getMugenDefIntegerOrDefault(tScript, "Movement", "airjump.num", 1);
	tMovementData->mAirJumpMinimumHeight = getMugenDefIntegerOrDefault(tScript, "Movement", "airjump.height", 35);

	tMovementData->mVerticalAcceleration = getMugenDefFloatOrDefault(tScript, "Movement", "yaccel", .44);
	tMovementData->mStandFiction = getMugenDefFloatOrDefault(tScript, "Movement", "stand.friction", 0.85);
	tMovementData->mCrouchFriction = getMugenDefFloatOrDefault(tScript, "Movement", "crouch.friction", 0.82);
	tMovementData->mStandFrictionThreshold = getMugenDefFloatOrDefault(tScript, "Movement", "stand.friction.threshold", 2);
	tMovementData->mCrouchFrictionThreshold = getMugenDefFloatOrDefault(tScript, "Movement", "crouch.friction.threshold", 0.05);
	tMovementData->mJumpChangeAnimThreshold = getMugenDefFloatOrDefault(tScript, "Movement", "jump.changeanim.threshold", INF);

	tMovementData->mAirGetHitGroundLevelY = getMugenDefIntegerOrDefault(tScript, "Movement", "air.gethit.groundlevel", 25);
	tMovementData->mAirGetHitGroundRecoveryGroundYTheshold = getMugenDefIntegerOrDefault(tScript, "Movement", "air.gethit.groundrecover.ground.threshold", -20);
	tMovementData->mAirGetHitGroundRecoveryGroundGoundLevelY = getMugenDefIntegerOrDefault(tScript, "Movement", "air.gethit.groundrecover.groundlevel", 10);
	tMovementData->mAirGetHitAirRecoveryVelocityYThreshold = getMugenDefFloatOrDefault(tScript, "Movement", "air.gethit.airrecover.threshold", -1);
	tMovementData->mAirGetHitAirRecoveryVerticalAcceleration = getMugenDefFloatOrDefault(tScript, "Movement", "air.gethit.airrecover.yaccel", 0.35);
	tMovementData->mAirGetHitTripGroundLevelY = getMugenDefIntegerOrDefault(tScript, "Movement", "air.gethit.trip.groundlevel", 15);

	tMovementData->mBounceOffset = getMugenDefVectorOrDefault(tScript, "Movement", "down.bounce.offset", makePosition(0, 20, 0));
	tMovementData->mVerticalBounceAcceleration = getMugenDefFloatOrDefault(tScript, "Movement", "down.bounce.yaccel", 0.4);
	tMovementData->mBounceGroundLevel = getMugenDefIntegerOrDefault(tScript, "Movement", "down.bounce.groundlevel", 12);
	tMovementData->mLyingDownFrictionThreshold = getMugenDefFloatOrDefault(tScript, "Movement", "down.friction.threshold", 0.05);
}

static void loadMugenConstantsFromScript(DreamMugenConstants* tConstants, MugenDefScript* tScript) {
	loadMugenConstantsHeader(&tConstants->mHeader, tScript);
	loadMugenConstantsSizeData(&tConstants->mSizeData, tScript);
	loadMugenConstantsVelocityData(&tConstants->mVelocityData, tScript);
	loadMugenConstantsMovementData(&tConstants->mMovementData, tScript);
}

DreamMugenConstants loadDreamMugenConstantsFile(char * tPath)
{
	MugenDefScript script; 
	loadMugenDefScript(&script, tPath);
	DreamMugenConstants ret = makeEmptyMugenConstants();
	loadMugenConstantsFromScript(&ret, &script);
	unloadMugenDefScript(script);
	return ret;
}

static void unloadSingleController(void* tCaller, void* tData) {
	(void)tCaller;
	DreamMugenStateController* e = (DreamMugenStateController*)tData;
	unloadDreamMugenStateController(e);
}

static void unloadSingleState(DreamMugenState& e) {
	if (e.mIsChangingAnimation) {
		destroyDreamMugenAssignment(e.mAnimation);
	}
	if (e.mIsSettingVelocity) {
		destroyDreamMugenAssignment(e.mVelocity);
	}
	if (e.mIsChangingControl) {
		destroyDreamMugenAssignment(e.mControl);
	}
	if (e.mIsChangingSpritePriority) {
		destroyDreamMugenAssignment(e.mSpritePriority);
	}
	if (e.mIsAddingPower) {
		destroyDreamMugenAssignment(e.mPowerAdd);
	}
	if (e.mDoesRequireJuggle) {
		destroyDreamMugenAssignment(e.mJuggleRequired);
	}
	if (e.mHasFacePlayer2Info) {
		destroyDreamMugenAssignment(e.mDoesFacePlayer2);
	}
	if (e.mHasPriority) {
		destroyDreamMugenAssignment(e.mPriority);
	}

	vector_map(&e.mControllers, unloadSingleController, NULL);
	delete_vector(&e.mControllers);
}

static int unloadSingleStateCB(void* tCaller, DreamMugenState& e) {
	(void)tCaller;

	unloadSingleState(e);

	return 1;
}

static void unloadMugenStates(DreamMugenStates* tStates) {
	stl_int_map_remove_predicate(tStates->mStates, unloadSingleStateCB);
	stl_delete_map(tStates->mStates);
}

void unloadDreamMugenConstantsFile(DreamMugenConstants * tConstants)
{
	unloadMugenStates(&tConstants->mStates);
}
