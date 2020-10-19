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
	int items = sscanf(tName, "%s", firstW);
	turnStringLowercase(firstW);
	return items == 1 && !strcmp("statedef", firstW);
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
	int mHasValidGroup;
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
	setPrismFlag(tState->mFlags, MUGEN_STATE_PROPERTY_CHANGING_ANIMATION);
	fetchDreamAssignmentFromGroupAndReturnWhetherItExists(tElement->mName.data(), tGroup, &tState->mAnimation);
} 

static void handleMugenStateDefVelocitySetting(DreamMugenState* tState, MugenDefScriptGroupElement* tElement, MugenDefScriptGroup* tGroup) {
	setPrismFlag(tState->mFlags, MUGEN_STATE_PROPERTY_SETTING_VELOCITY);
	fetchDreamAssignmentFromGroupAndReturnWhetherItExists(tElement->mName.data(), tGroup, &tState->mVelocity);
}

static void handleMugenStateDefControl(DreamMugenState* tState, MugenDefScriptGroupElement* tElement, MugenDefScriptGroup* tGroup) {
	setPrismFlag(tState->mFlags, MUGEN_STATE_PROPERTY_CHANGING_CONTROL);
	fetchDreamAssignmentFromGroupAndReturnWhetherItExists(tElement->mName.data(), tGroup, &tState->mControl);
}

static void handleMugenStateSpritePriority(DreamMugenState* tState, MugenDefScriptGroupElement* tElement, MugenDefScriptGroup* tGroup) {
	setPrismFlag(tState->mFlags, MUGEN_STATE_PROPERTY_CHANGING_SPRITE_PRIORITY);
	fetchDreamAssignmentFromGroupAndReturnWhetherItExists(tElement->mName.data(), tGroup, &tState->mSpritePriority);
}

static void handleMugenStatePowerAdd(DreamMugenState* tState, MugenDefScriptGroupElement* tElement, MugenDefScriptGroup* tGroup) {
	setPrismFlag(tState->mFlags, MUGEN_STATE_PROPERTY_ADDING_POWER);
	fetchDreamAssignmentFromGroupAndReturnWhetherItExists(tElement->mName.data(), tGroup, &tState->mPowerAdd);
}

static void handleMugenStateJuggle(DreamMugenState* tState, MugenDefScriptGroupElement* tElement, MugenDefScriptGroup* tGroup) {
	setPrismFlag(tState->mFlags, MUGEN_STATE_PROPERTY_JUGGLE_REQUIREMENT);
	fetchDreamAssignmentFromGroupAndReturnWhetherItExists(tElement->mName.data(), tGroup, &tState->mJuggleRequired);
}

static void handleMugenStateHitDefPersistence(DreamMugenState* tState, MugenDefScriptGroupElement* tElement, MugenDefScriptGroup* tGroup) {
	setPrismFlag(tState->mFlags, MUGEN_STATE_PROPERTY_HIT_DEFINITION_PERSISTENCE);
	fetchDreamAssignmentFromGroupAndReturnWhetherItExists(tElement->mName.data(), tGroup, &tState->mDoHitDefinitionsPersist);
}

static void handleMugenStateMoveHitPersistence(DreamMugenState* tState, MugenDefScriptGroupElement* tElement, MugenDefScriptGroup* tGroup) {
	setPrismFlag(tState->mFlags, MUGEN_STATE_PROPERTY_MOVE_HIT_INFO_PERSISTENCE);
	fetchDreamAssignmentFromGroupAndReturnWhetherItExists(tElement->mName.data(), tGroup, &tState->mDoMoveHitInfosPersist);
}

static void handleMugenStateHitCountPersistence(DreamMugenState* tState, MugenDefScriptGroupElement* tElement, MugenDefScriptGroup* tGroup) {
	setPrismFlag(tState->mFlags, MUGEN_STATE_PROPERTY_HIT_COUNT_PERSISTENCE);
	fetchDreamAssignmentFromGroupAndReturnWhetherItExists(tElement->mName.data(), tGroup, &tState->mDoesHitCountPersist);
}

static void handleMugenStateFacePlayer2(DreamMugenState* tState, MugenDefScriptGroupElement* tElement, MugenDefScriptGroup* tGroup) {
	setPrismFlag(tState->mFlags, MUGEN_STATE_PROPERTY_FACE_PLAYER_2_INFO);
	fetchDreamAssignmentFromGroupAndReturnWhetherItExists(tElement->mName.data(), tGroup, &tState->mDoesFacePlayer2);
}

static void handleMugenStatePriority(DreamMugenState* tState, MugenDefScriptGroupElement* tElement, MugenDefScriptGroup* tGroup) {
	setPrismFlag(tState->mFlags, MUGEN_STATE_PROPERTY_PRIORITY);
	fetchDreamAssignmentFromGroupAndReturnWhetherItExists(tElement->mName.data(), tGroup, &tState->mPriority);
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
	tStates->mStates.erase(tState);
}

static void handleMugenStateDef(DreamMugenStates* tStates, MugenDefScriptGroup* tGroup, int tIsOverwritable) {

	DreamMugenState state;

	char dummy[100];
	int items = sscanf(tGroup->mName.data(), "%s %d", dummy, &state.mID);
	if (items != 2) {
		logWarningFormat("Unable to parse statedef id: %s", tGroup->mName.data());
		state.mID = -5;
	}

	gMugenStateDefParseState.mCurrentGroup = state.mID;
	if (stl_map_contains(tStates->mStates, state.mID)) {
		const auto& previousState = tStates->mStates[state.mID];
		if (!hasPrismFlag(previousState.mFlags, MUGEN_STATE_PROPERTY_OVERWRITABLE)) {
			gMugenStateDefParseState.mHasValidGroup = 0;
			return;
		}
		removeState(tStates, state.mID);
	}
	gMugenStateDefParseState.mHasValidGroup = 1;
	
	state.mType = MUGEN_STATE_TYPE_STANDING;
	state.mMoveType = MUGEN_STATE_MOVE_TYPE_IDLE;
	state.mPhysics = MUGEN_STATE_PHYSICS_NONE;
	state.mFlags = 0;
	setPrismFlag(state.mFlags, MUGEN_STATE_PROPERTY_OVERWRITABLE * tIsOverwritable);

	state.mControllers = new_vector();

	MugenStateDefCaller caller;
	caller.mState = &state;
	caller.mGroup = tGroup;
	stl_string_map_map(tGroup->mElements, handleSingleMugenStateDefElement, &caller);
	tStates->mStates[state.mID] = state;
}

static int isMugenStateController(const char* tName) {
	char firstW[100];
	int items = sscanf(tName, "%s", firstW);
	turnStringLowercase(firstW);
	return items == 1 && !strcmp("state", firstW);
}

static void handleMugenStateControllerInDefGroup(DreamMugenStates* tStates, MugenDefScriptGroup* tGroup) {
	if (!gMugenStateDefParseState.mHasValidGroup) return;

	DreamMugenState* state = &tStates->mStates[gMugenStateDefParseState.mCurrentGroup];

	DreamMugenStateController* controller = parseDreamMugenStateControllerFromGroup(tGroup);

	vector_push_back_owned(&state->mControllers, controller);
}

static void handleSingleMugenStateDefGroup(DreamMugenStates* tStates, MugenDefScriptGroup* tGroup, int tIsOverwritable) {

	if (isMugenStateDef(tGroup->mName.data())) {
		handleMugenStateDef(tStates, tGroup, tIsOverwritable);
	} else if (isMugenStateController(tGroup->mName.data())) {
		handleMugenStateControllerInDefGroup(tStates, tGroup);
	}
	else {
		logWarningFormat("Unable to determine state def group type %s. Ignoring.", tGroup->mName.data());
	}

}

static void loadMugenStateDefinitionsFromScript(DreamMugenStates* tStates, MugenDefScript* tScript, int tIsOverwritable) {
	MugenDefScriptGroup* current = getFirstStateDefGroup(tScript);

	while (current != NULL) {
		handleSingleMugenStateDefGroup(tStates, current, tIsOverwritable);
		
		current = current->mNext;
	}
}

void loadDreamMugenStateDefinitionsFromFile(DreamMugenStates* tStates, const char* tPath, int tIsOverwritable) {
	MugenDefScript script; 
	loadMugenDefScript(&script, tPath);
	loadMugenStateDefinitionsFromScript(tStates, &script, tIsOverwritable);
	unloadMugenDefScript(&script);
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


static void loadSingleSparkNumber(int* oIsSparkInPlayerFile, int* oSparkNumber, MugenDefScript* tScript, const char* tGroup, const char* tName, int tDefaultNumber) {
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
	tHeader->mLife = getMugenDefIntegerOrDefault(tScript, "data", "life", 1000);
	tHeader->mPower = getMugenDefIntegerOrDefault(tScript, "data", "power", 3000);
	tHeader->mAttack = getMugenDefIntegerOrDefault(tScript, "data", "attack", 100);
	tHeader->mDefense = getMugenDefIntegerOrDefault(tScript, "data", "defence", 100);
	tHeader->mFallDefenseUp = getMugenDefIntegerOrDefault(tScript, "data", "fall.defence_up", 50);
	tHeader->mLiedownTime = getMugenDefIntegerOrDefault(tScript, "data", "liedown.time", 60);
	tHeader->mAirJugglePoints = getMugenDefIntegerOrDefault(tScript, "data", "airjuggle", 15);

	loadSingleSparkNumber(&tHeader->mIsSparkNoInPlayerFile, &tHeader->mSparkNo, tScript, "data", "sparkno", 2);
	loadSingleSparkNumber(&tHeader->mIsGuardSparkNoInPlayerFile, &tHeader->mGuardSparkNo, tScript, "data", "guard.sparkno", 40);
	tHeader->mKOEcho = getMugenDefIntegerOrDefault(tScript, "data", "ko.echo", 0);
	tHeader->mVolume = getMugenDefIntegerOrDefault(tScript, "data", "volume", 0);

	tHeader->mIntPersistIndex = getMugenDefIntegerOrDefault(tScript, "data", "intpersistindex", 60);
	tHeader->mFloatPersistIndex = getMugenDefIntegerOrDefault(tScript, "data", "floatpersistindex", 40);
}

static int getMugenDefVectorIAndReturnWhetherItExists(MugenDefScript* tScript, const char* tGroupName, const char* tVariableName, Vector2DI* oVector) {
	if (!isMugenDefVector2DIVariable(tScript, tGroupName, tVariableName)) return 0;
	*oVector = getMugenDefVector2DIVariable(tScript, tGroupName, tVariableName);
	return 1;
}

static void loadMugenConstantsSizeData(DreamMugenConstantsSizeData* tSizeData, MugenDefScript* tScript) {
	tSizeData->mScale.x = getMugenDefFloatOrDefault(tScript, "size", "xscale", 1);
	tSizeData->mScale.y = getMugenDefFloatOrDefault(tScript, "size", "yscale", 1);
	tSizeData->mGroundBackWidth = getMugenDefIntegerOrDefault(tScript, "size", "ground.back", 15);
	tSizeData->mGroundFrontWidth = getMugenDefIntegerOrDefault(tScript, "size", "ground.front", 16);
	tSizeData->mAirBackWidth = getMugenDefIntegerOrDefault(tScript, "size", "air.back", 12);
	tSizeData->mAirFrontWidth = getMugenDefIntegerOrDefault(tScript, "size", "air.front", 12);
	tSizeData->mHeight = getMugenDefIntegerOrDefault(tScript, "size", "height", 60);
	tSizeData->mAttackDistance = getMugenDefIntegerOrDefault(tScript, "size", "attack.dist", 160);
	tSizeData->mProjectileAttackDistance = getMugenDefIntegerOrDefault(tScript, "size", "proj.attack.dist", 90);
	tSizeData->mDoesScaleProjectiles = getMugenDefIntegerOrDefault(tScript, "size", "proj.doscale", 0);
	tSizeData->mHeadPosition = getMugenDefVector2DOrDefault(tScript, "size", "head.pos", Vector2D(-5, -90));
	tSizeData->mMidPosition = getMugenDefVector2DOrDefault(tScript, "size", "mid.pos", Vector2D(-5, -60));
	tSizeData->mShadowOffset = getMugenDefIntegerOrDefault(tScript, "size", "shadowoffset", 0);
	tSizeData->mDrawOffset = getMugenDefVector2DIOrDefault(tScript, "size", "draw.offset", Vector2DI(0, 0));

	tSizeData->mHasAttackWidth = getMugenDefVectorIAndReturnWhetherItExists(tScript, "size", "attack.width", &tSizeData->mAttackWidth);
	
}


static void loadMugenConstantsVelocityData(DreamMugenConstantsVelocityData* tVelocityData, MugenDefScript* tScript) {
	tVelocityData->mWalkForward = getMugenDefVector2DOrDefault(tScript, "velocity", "walk.fwd", Vector2D(2.4, 0));
	tVelocityData->mWalkBackward = getMugenDefVector2DOrDefault(tScript, "velocity", "walk.back", Vector2D(-2.2, 0));

	tVelocityData->mRunForward = getMugenDefVector2DOrDefault(tScript, "velocity", "run.fwd", Vector2D(4.6, 0));
	tVelocityData->mRunBackward = getMugenDefVector2DOrDefault(tScript, "velocity", "run.back", Vector2D(-4.5, -3.8));

	tVelocityData->mJumpNeutral = getMugenDefVector2DOrDefault(tScript, "velocity", "jump.neu", Vector2D(0, -8.4));
	tVelocityData->mJumpBackward = getMugenDefVector2DOrDefault(tScript, "velocity", "jump.back", Vector2D(-2.55, 0));
	tVelocityData->mJumpForward = getMugenDefVector2DOrDefault(tScript, "velocity", "jump.fwd", Vector2D(2.5, 0));

	tVelocityData->mRunJumpBackward = getMugenDefVector2DOrDefault(tScript, "velocity", "runjump.back", Vector2D(-2.55, -8.1));
	tVelocityData->mRunJumpForward = getMugenDefVector2DOrDefault(tScript, "velocity", "runjump.fwd", Vector2D(4, -8.1));

	tVelocityData->mAirJumpNeutral = getMugenDefVector2DOrDefault(tScript, "velocity", "airjump.neu", Vector2D(0, -8.1));
	tVelocityData->mAirJumpBackward = getMugenDefVector2DOrDefault(tScript, "velocity", "airjump.back", Vector2D(-2.55, 0));
	tVelocityData->mAirJumpForward = getMugenDefVector2DOrDefault(tScript, "velocity", "airjump.fwd", Vector2D(2.5, 0));

	tVelocityData->mAirGetHitGroundRecovery = getMugenDefVector2DOrDefault(tScript, "velocity", "air.gethit.groundrecover", Vector2D(-0.15, -3.5));
	tVelocityData->mAirGetHitAirRecoveryMultiplier = getMugenDefVector2DOrDefault(tScript, "velocity", "air.gethit.airrecover.mul", Vector2D(0.5, 0.2));
	tVelocityData->mAirGetHitAirRecoveryOffset = getMugenDefVector2DOrDefault(tScript, "velocity", "air.gethit.airrecover.add", Vector2D(0, -4.5));

	tVelocityData->mAirGetHitExtraXWhenHoldingBackward = getMugenDefFloatOrDefault(tScript, "velocity", "air.gethit.airrecover.back", -1);
	tVelocityData->mAirGetHitExtraXWhenHoldingForward = getMugenDefFloatOrDefault(tScript, "velocity", "air.gethit.airrecover.fwd", 0);
	tVelocityData->mAirGetHitExtraYWhenHoldingUp = getMugenDefFloatOrDefault(tScript, "velocity", "air.gethit.airrecover.up", -2);
	tVelocityData->mAirGetHitExtraYWhenHoldingDown = getMugenDefFloatOrDefault(tScript, "velocity", "air.gethit.airrecover.down", 1.5);
}

static void loadMugenConstantsMovementData(DreamMugenConstantsMovementData* tMovementData, MugenDefScript* tScript) {
	tMovementData->mAllowedAirJumpAmount = getMugenDefIntegerOrDefault(tScript, "movement", "airjump.num", 1);
	tMovementData->mAirJumpMinimumHeight = getMugenDefIntegerOrDefault(tScript, "movement", "airjump.height", 35);

	tMovementData->mVerticalAcceleration = getMugenDefFloatOrDefault(tScript, "movement", "yaccel", .44);
	tMovementData->mStandFiction = getMugenDefFloatOrDefault(tScript, "movement", "stand.friction", 0.85);
	tMovementData->mCrouchFriction = getMugenDefFloatOrDefault(tScript, "movement", "crouch.friction", 0.82);
	tMovementData->mStandFrictionThreshold = getMugenDefFloatOrDefault(tScript, "movement", "stand.friction.threshold", 2);
	tMovementData->mCrouchFrictionThreshold = getMugenDefFloatOrDefault(tScript, "movement", "crouch.friction.threshold", 0.05);
	tMovementData->mJumpChangeAnimThreshold = getMugenDefFloatOrDefault(tScript, "movement", "jump.changeanim.threshold", INF);

	tMovementData->mAirGetHitGroundLevelY = getMugenDefIntegerOrDefault(tScript, "movement", "air.gethit.groundlevel", 25);
	tMovementData->mAirGetHitGroundRecoveryGroundYTheshold = getMugenDefIntegerOrDefault(tScript, "movement", "air.gethit.groundrecover.ground.threshold", -20);
	tMovementData->mAirGetHitGroundRecoveryGroundGoundLevelY = getMugenDefIntegerOrDefault(tScript, "movement", "air.gethit.groundrecover.groundlevel", 10);
	tMovementData->mAirGetHitAirRecoveryVelocityYThreshold = getMugenDefFloatOrDefault(tScript, "movement", "air.gethit.airrecover.threshold", -1);
	tMovementData->mAirGetHitAirRecoveryVerticalAcceleration = getMugenDefFloatOrDefault(tScript, "movement", "air.gethit.airrecover.yaccel", 0.35);
	tMovementData->mAirGetHitTripGroundLevelY = getMugenDefIntegerOrDefault(tScript, "movement", "air.gethit.trip.groundlevel", 15);

	tMovementData->mBounceOffset = getMugenDefVector2DOrDefault(tScript, "movement", "down.bounce.offset", Vector2D(0, 20));
	tMovementData->mVerticalBounceAcceleration = getMugenDefFloatOrDefault(tScript, "movement", "down.bounce.yaccel", 0.4);
	tMovementData->mBounceGroundLevel = getMugenDefIntegerOrDefault(tScript, "movement", "down.bounce.groundlevel", 12);
	tMovementData->mLyingDownFrictionThreshold = getMugenDefFloatOrDefault(tScript, "movement", "down.friction.threshold", 0.05);
}

static void loadMugenConstantsPlayerVictoryQuote(DreamMugenConstantsQuoteData* tQuote) {
	tQuote->mVictoryQuoteIndex = -1;
}

static void loadMugenConstantsFromScript(DreamMugenConstants* tConstants, MugenDefScript* tScript) {
	loadMugenConstantsHeader(&tConstants->mHeader, tScript);
	loadMugenConstantsSizeData(&tConstants->mSizeData, tScript);
	loadMugenConstantsVelocityData(&tConstants->mVelocityData, tScript);
	loadMugenConstantsMovementData(&tConstants->mMovementData, tScript);
	loadMugenConstantsPlayerVictoryQuote(&tConstants->mQuoteData);
}

DreamMugenConstants loadDreamMugenConstantsFile(const char * tPath)
{
	MugenDefScript script; 
	loadMugenDefScript(&script, tPath);
	DreamMugenConstants ret = makeEmptyMugenConstants();
	loadMugenConstantsFromScript(&ret, &script);
	unloadMugenDefScript(&script);
	return ret;
}

static void unloadSingleController(void* tCaller, void* tData) {
	(void)tCaller;
	DreamMugenStateController* e = (DreamMugenStateController*)tData;
	unloadDreamMugenStateController(e);
}

static void unloadSingleState(DreamMugenState& e) {
	if (hasPrismFlag(e.mFlags, MUGEN_STATE_PROPERTY_CHANGING_ANIMATION)) {
		destroyDreamMugenAssignment(e.mAnimation);
	}
	if (hasPrismFlag(e.mFlags, MUGEN_STATE_PROPERTY_SETTING_VELOCITY)) {
		destroyDreamMugenAssignment(e.mVelocity);
	}
	if (hasPrismFlag(e.mFlags, MUGEN_STATE_PROPERTY_CHANGING_CONTROL)) {
		destroyDreamMugenAssignment(e.mControl);
	}
	if (hasPrismFlag(e.mFlags, MUGEN_STATE_PROPERTY_CHANGING_SPRITE_PRIORITY)) {
		destroyDreamMugenAssignment(e.mSpritePriority);
	}
	if (hasPrismFlag(e.mFlags, MUGEN_STATE_PROPERTY_ADDING_POWER)) {
		destroyDreamMugenAssignment(e.mPowerAdd);
	}
	if (hasPrismFlag(e.mFlags, MUGEN_STATE_PROPERTY_JUGGLE_REQUIREMENT)) {
		destroyDreamMugenAssignment(e.mJuggleRequired);
	}
	if (hasPrismFlag(e.mFlags, MUGEN_STATE_PROPERTY_FACE_PLAYER_2_INFO)) {
		destroyDreamMugenAssignment(e.mDoesFacePlayer2);
	}
	if (hasPrismFlag(e.mFlags, MUGEN_STATE_PROPERTY_PRIORITY)) {
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

DreamMugenStateTypeFlags convertDreamMugenStateTypeToFlag(DreamMugenStateType tType)
{
	switch (tType) {
	case MUGEN_STATE_TYPE_STANDING:
		return MUGEN_STATE_TYPE_STANDING_FLAG;
	case MUGEN_STATE_TYPE_CROUCHING:
		return MUGEN_STATE_TYPE_CROUCHING_FLAG;
	case MUGEN_STATE_TYPE_AIR:
		return MUGEN_STATE_TYPE_AIR_FLAG;
	default:
		return MUGEN_STATE_TYPE_NO_FLAG;
	}
}