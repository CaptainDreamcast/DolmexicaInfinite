#include "mugenstatecontrollers.h"

#include <assert.h>
#include <string.h>

#define LOGGER_WARNINGS_DISABLED

#include <prism/memoryhandler.h>
#include <prism/log.h>
#include <prism/system.h>
#include <prism/clipboardhandler.h>
#include <prism/input.h>
#include <prism/math.h>
#include <prism/mugenanimationhandler.h>
#include <prism/screeneffect.h>

#include "playerdefinition.h"
#include "playerhitdata.h"
#include "mugenexplod.h"
#include "stage.h"
#include "mugenstagehandler.h"
#include "config.h"
#include "pausecontrollers.h"
#include "mugenassignmentevaluator.h"
#include "fightui.h"
#include "gamelogic.h"
#include "projectile.h"
#include "dolmexicastoryscreen.h"

typedef void(*StateControllerParseFunction)(DreamMugenStateController*, MugenDefScriptGroup*);
typedef int(*StateControllerHandleFunction)(DreamMugenStateController*, DreamPlayer*); // return 1 iff state changed
typedef void(*StateControllerUnloadFunction)(DreamMugenStateController*);

static struct {
	StringMap mStateControllerParsers; // contains StateControllerParseFunction
	IntMap mStateControllerHandlers; // contains StateControllerHandleFunction
	IntMap mStateControllerUnloaders; // contains StateControllerUnloadFunction
	MemoryStack* mMemoryStack;
} gVariableHandler;


typedef struct {

	char mName[100];
	DreamMugenAssignment** mTriggerRoot;
} TriggerParseCaller;

static void checkSingleElementForTrigger(void* tCaller, void* tData) {
	TriggerParseCaller* caller = tCaller;
	MugenDefScriptGroupElement* e = tData;

	if (strcmp(e->mName, caller->mName)) return;

	char* text = getAllocatedMugenDefStringVariableAsElement(e);

	DreamMugenAssignment* trigger = parseDreamMugenAssignmentFromString(text);
	freeMemory(text);

	*caller->mTriggerRoot = makeDreamAndMugenAssignment(*caller->mTriggerRoot, trigger);
}

static int parseTriggerAndReturnIfFound(char* tName, DreamMugenAssignment** tRoot, MugenDefScriptGroup* tGroup) {
	if (!string_map_contains(&tGroup->mElements, tName)) return 0;

	DreamMugenAssignment* triggerRoot = NULL;
	TriggerParseCaller caller;
	strcpy(caller.mName, tName);
	caller.mTriggerRoot = &triggerRoot;
	list_map(&tGroup->mOrderedElementList, checkSingleElementForTrigger, &caller);

	*tRoot = makeDreamOrMugenAssignment(*tRoot, triggerRoot);

	return 1;
}

int parseNumberedTriggerAndReturnIfFound(int i, DreamMugenAssignment** tRoot, MugenDefScriptGroup* tGroup) {
	char name[100];
	sprintf(name, "trigger%d", i);

	return parseTriggerAndReturnIfFound(name, tRoot, tGroup);
}

void parseStateControllerTriggers(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) {
	DreamMugenAssignment* allRoot = NULL;
	parseTriggerAndReturnIfFound("triggerall", &allRoot, tGroup);

	DreamMugenAssignment* root = NULL;
	int i = 1;
	while (parseNumberedTriggerAndReturnIfFound(i, &root, tGroup)) i++;

	root = makeDreamAndMugenAssignment(allRoot, root);
	if (!root) {
		root = makeDreamTrueMugenAssignment();
	}
	tController->mTrigger.mAssignment = root;
}

static void* allocMemoryOnMemoryStackOrMemory(uint32_t tSize) {
	if (gVariableHandler.mMemoryStack) return allocMemoryOnMemoryStack(gVariableHandler.mMemoryStack, tSize);
	else return allocMemory(tSize);
}

typedef struct {
	DreamMugenAssignment* x;
	DreamMugenAssignment* y;

	uint8_t mIsSettingX;
	uint8_t mIsSettingY;
} Set2DPhysicsController;


static void parse2DPhysicsController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup, DreamMugenStateControllerType tType) {
	Set2DPhysicsController* e = allocMemoryOnMemoryStackOrMemory(sizeof(Set2DPhysicsController));
	
	e->mIsSettingX = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("x", tGroup, &e->x);
	e->mIsSettingY = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("y", tGroup, &e->y);

	tController->mType = tType;
	tController->mData = e;
}

static void unload2DPhysicsController(DreamMugenStateController* tController) {
	Set2DPhysicsController* e = tController->mData;
	if (e->mIsSettingX) {
		destroyDreamMugenAssignment(e->x);
	}
	if (e->mIsSettingY) {
		destroyDreamMugenAssignment(e->y);
	}
	freeMemory(e);
}

typedef struct {
	DreamMugenAssignment* mState;
	DreamMugenAssignment* mControl;

	uint8_t mIsChangingControl;
} ChangeStateController;


static void parseChangeStateController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup, DreamMugenStateControllerType tType) {
	ChangeStateController* e = allocMemoryOnMemoryStackOrMemory(sizeof(ChangeStateController));
	assert(fetchDreamAssignmentFromGroupAndReturnWhetherItExists("value", tGroup, &e->mState));

	e->mIsChangingControl = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("ctrl", tGroup, &e->mControl);

	tController->mType = tType;
	tController->mData = e;
}

static void unloadChangeStateController(DreamMugenStateController* tController) {
	ChangeStateController* e = tController->mData;
	destroyDreamMugenAssignment(e->mState);

	if (e->mIsChangingControl) {
		destroyDreamMugenAssignment(e->mControl);
	} 

	freeMemory(e);
}

static void fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString(char* tName, MugenDefScriptGroup* tGroup, DreamMugenAssignment** tDst, char* tDefault) {
	if (!fetchDreamAssignmentFromGroupAndReturnWhetherItExists(tName, tGroup, tDst)) {
		*tDst = makeDreamStringMugenAssignment(tDefault);
	}
}

typedef struct {
	DreamMugenAssignment* mState;
	DreamMugenAssignment* mControl;
	DreamMugenAssignment* mID;

	uint8_t mIsChangingControl;
} TargetChangeStateController;

static void parseTargetChangeStateController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) {
	TargetChangeStateController* e = allocMemoryOnMemoryStackOrMemory(sizeof(TargetChangeStateController));
	assert(fetchDreamAssignmentFromGroupAndReturnWhetherItExists("value", tGroup, &e->mState));
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("id", tGroup, &e->mID, "");

	e->mIsChangingControl = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("ctrl", tGroup, &e->mControl);

	tController->mType = MUGEN_STATE_CONTROLLER_TYPE_SET_TARGET_STATE;
	tController->mData = e;
}

static void unloadTargetChangeStateController(DreamMugenStateController* tController) {
	TargetChangeStateController* e = tController->mData;
	destroyDreamMugenAssignment(e->mState);
	destroyDreamMugenAssignment(e->mID);

	if (e->mIsChangingControl) {
		destroyDreamMugenAssignment(e->mControl);
	}

	freeMemory(e);
}

typedef struct {
	DreamMugenAssignment* mAttribute;
	DreamMugenAssignment* mHitFlag;

	DreamMugenAssignment* mGuardFlag;
	DreamMugenAssignment* mAffectTeam;
	DreamMugenAssignment* mAnimationType;
	DreamMugenAssignment* mAirAnimationType;
	DreamMugenAssignment* mFallAnimationType;

	DreamMugenAssignment* mPriority;
	DreamMugenAssignment* mDamage;
	DreamMugenAssignment* mPauseTime;
	DreamMugenAssignment* mGuardPauseTime;
	DreamMugenAssignment* mSparkNumber;
	DreamMugenAssignment* mGuardSparkNumber;

	DreamMugenAssignment* mSparkXY;
	DreamMugenAssignment* mHitSound;
	DreamMugenAssignment* mGuardSound;
	DreamMugenAssignment* mGroundType;
	DreamMugenAssignment* mAirType;

	DreamMugenAssignment* mGroundSlideTime;
	DreamMugenAssignment* mGuardSlideTime;
	DreamMugenAssignment* mGroundHitTime;
	DreamMugenAssignment* mGuardHitTime;
	DreamMugenAssignment* mAirHitTime;

	DreamMugenAssignment* mGuardControlTime;
	DreamMugenAssignment* mGuardDistance;
	DreamMugenAssignment* mYAccel;
	DreamMugenAssignment* mGroundVelocity;
	DreamMugenAssignment* mGuardVelocity;
	DreamMugenAssignment* mAirVelocity;
	DreamMugenAssignment* mAirGuardVelocity;

	DreamMugenAssignment* mGroundCornerPushVelocityOffset;
	DreamMugenAssignment* mAirCornerPushVelocityOffset;
	DreamMugenAssignment* mDownCornerPushVelocityOffset;
	DreamMugenAssignment* mGuardCornerPushVelocityOffset;
	DreamMugenAssignment* mAirGuardCornerPushVelocityOffset;

	DreamMugenAssignment* mAirGuardControlTime;
	DreamMugenAssignment* mAirJuggle;
	DreamMugenAssignment* mMinimumDistance;
	DreamMugenAssignment* mMaximumDistance;
	DreamMugenAssignment* mSnap;

	DreamMugenAssignment* mPlayerSpritePriority1;
	DreamMugenAssignment* mPlayerSpritePriority2;

	DreamMugenAssignment* mPlayer1ChangeFaceDirection;
	DreamMugenAssignment* mPlayer1ChangeFaceDirectionRelativeToPlayer2;
	DreamMugenAssignment* mPlayer2ChangeFaceDirectionRelativeToPlayer1;

	DreamMugenAssignment* mPlayer1StateNumber;
	DreamMugenAssignment* mPlayer2StateNumber;
	DreamMugenAssignment* mPlayer2CapableOfGettingPlayer1State;
	DreamMugenAssignment* mForceStanding;

	DreamMugenAssignment* mFall;
	DreamMugenAssignment* mFallXVelocity;
	DreamMugenAssignment* mFallYVelocity;
	DreamMugenAssignment* mFallCanBeRecovered;
	DreamMugenAssignment* mFallRecoveryTime;
	DreamMugenAssignment* mFallDamage;

	DreamMugenAssignment* mAirFall;
	DreamMugenAssignment* mForceNoFall;
	DreamMugenAssignment* mDownVelocity;
	DreamMugenAssignment* mDownHitTime;
	DreamMugenAssignment* mDownBounce;

	DreamMugenAssignment* mHitID;
	DreamMugenAssignment* mChainID;
	DreamMugenAssignment* mNoChainID;
	DreamMugenAssignment* mHitOnce;

	DreamMugenAssignment* mKill;
	DreamMugenAssignment* mGuardKill;
	DreamMugenAssignment* mFallKill;
	DreamMugenAssignment* mNumberOfHits;

	DreamMugenAssignment* mGetPower;
	DreamMugenAssignment* mGivePower;

	DreamMugenAssignment* mPaletteEffectTime;
	DreamMugenAssignment* mPaletteEffectMultiplication;
	DreamMugenAssignment* mPaletteEffectAddition;

	DreamMugenAssignment* mEnvironmentShakeTime;
	DreamMugenAssignment* mEnvironmentShakeFrequency;
	DreamMugenAssignment* mEnvironmentShakeAmplitude;
	DreamMugenAssignment* mEnvironmentShakePhase;

	DreamMugenAssignment* mFallEnvironmentShakeTime;
	DreamMugenAssignment* mFallEnvironmentShakeFrequency;
	DreamMugenAssignment* mFallEnvironmentShakeAmplitude;
	DreamMugenAssignment* mFallEnvironmentShakePhase;
} HitDefinitionController;

static void readHitDefinitionFromGroup(HitDefinitionController* e, MugenDefScriptGroup* tGroup) {
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("attr", tGroup, &e->mAttribute, "S , NA");

	if (!fetchDreamAssignmentFromGroupAndReturnWhetherItExists("hitflag", tGroup, &e->mHitFlag)) {
		e->mHitFlag = makeDreamStringMugenAssignment("MAF");
	}
	if (!fetchDreamAssignmentFromGroupAndReturnWhetherItExists("guardflag", tGroup, &e->mGuardFlag)) {
		e->mGuardFlag = makeDreamStringMugenAssignment("");
	}
	if (!fetchDreamAssignmentFromGroupAndReturnWhetherItExists("affectteam", tGroup, &e->mAffectTeam)) {
		e->mAffectTeam = makeDreamStringMugenAssignment("E");
	}
	if (!fetchDreamAssignmentFromGroupAndReturnWhetherItExists("animtype", tGroup, &e->mAnimationType)) {
		e->mAnimationType = makeDreamStringMugenAssignment("Light");
	}
	if (!fetchDreamAssignmentFromGroupAndReturnWhetherItExists("air.animtype", tGroup, &e->mAirAnimationType)) {
		e->mAirAnimationType = makeDreamStringMugenAssignment("");
	}
	if (!fetchDreamAssignmentFromGroupAndReturnWhetherItExists("fall.animtype", tGroup, &e->mFallAnimationType)) {
		e->mFallAnimationType = makeDreamStringMugenAssignment("");
	}
	if (!fetchDreamAssignmentFromGroupAndReturnWhetherItExists("priority", tGroup, &e->mPriority)) {
		e->mPriority = makeDreamStringMugenAssignment("");
	}
	if (!fetchDreamAssignmentFromGroupAndReturnWhetherItExists("damage", tGroup, &e->mDamage)) {
		e->mDamage = makeDreamStringMugenAssignment("0 , 0");
	}
	if (!fetchDreamAssignmentFromGroupAndReturnWhetherItExists("pausetime", tGroup, &e->mPauseTime)) {
		e->mPauseTime = makeDreamStringMugenAssignment("0 , 0");
	}
	if (!fetchDreamAssignmentFromGroupAndReturnWhetherItExists("guard.pausetime", tGroup, &e->mGuardPauseTime)) {
		e->mGuardPauseTime = makeDreamStringMugenAssignment("");
	}
	if (!fetchDreamAssignmentFromGroupAndReturnWhetherItExists("sparkno", tGroup, &e->mSparkNumber)) {
		e->mSparkNumber = makeDreamStringMugenAssignment("");
	}
	if (!fetchDreamAssignmentFromGroupAndReturnWhetherItExists("guard.sparkno", tGroup, &e->mGuardSparkNumber)) {
		e->mGuardSparkNumber = makeDreamStringMugenAssignment("");
	}
	if (!fetchDreamAssignmentFromGroupAndReturnWhetherItExists("sparkxy", tGroup, &e->mSparkXY)) {
		e->mSparkXY = makeDreamStringMugenAssignment("0 , 0");
	}
	if (!fetchDreamAssignmentFromGroupAndReturnWhetherItExists("hitsound", tGroup, &e->mHitSound)) {
		e->mHitSound = makeDreamStringMugenAssignment("");
	}
	if (!fetchDreamAssignmentFromGroupAndReturnWhetherItExists("guardsound", tGroup, &e->mGuardSound)) {
		e->mGuardSound = makeDreamStringMugenAssignment("");
	}
	if (!fetchDreamAssignmentFromGroupAndReturnWhetherItExists("ground.type", tGroup, &e->mGroundType)) {
		e->mGroundType = makeDreamStringMugenAssignment("High");
	}
	if (!fetchDreamAssignmentFromGroupAndReturnWhetherItExists("air.type", tGroup, &e->mAirType)) {
		e->mAirType = makeDreamStringMugenAssignment("");
	}
	if (!fetchDreamAssignmentFromGroupAndReturnWhetherItExists("ground.slidetime", tGroup, &e->mGroundSlideTime)) {
		e->mGroundSlideTime = makeDreamStringMugenAssignment("0");
	}
	if (!fetchDreamAssignmentFromGroupAndReturnWhetherItExists("guard.slidetime", tGroup, &e->mGuardSlideTime)) {
		e->mGuardSlideTime = makeDreamStringMugenAssignment("");
	}
	if (!fetchDreamAssignmentFromGroupAndReturnWhetherItExists("ground.hittime", tGroup, &e->mGroundHitTime)) {
		e->mGroundHitTime = makeDreamStringMugenAssignment("0");
	}
	if (!fetchDreamAssignmentFromGroupAndReturnWhetherItExists("guard.hittime", tGroup, &e->mGuardHitTime)) {
		e->mGuardHitTime = makeDreamStringMugenAssignment("");
	}
	if (!fetchDreamAssignmentFromGroupAndReturnWhetherItExists("air.hittime", tGroup, &e->mAirHitTime)) {
		e->mAirHitTime = makeDreamStringMugenAssignment("20");
	}
	if (!fetchDreamAssignmentFromGroupAndReturnWhetherItExists("guard.ctrltime", tGroup, &e->mGuardControlTime)) {
		e->mGuardControlTime = makeDreamStringMugenAssignment("");
	}
	if (!fetchDreamAssignmentFromGroupAndReturnWhetherItExists("guard.dist", tGroup, &e->mGuardDistance)) {
		e->mGuardDistance = makeDreamStringMugenAssignment("");
	}
	if (!fetchDreamAssignmentFromGroupAndReturnWhetherItExists("yaccel", tGroup, &e->mYAccel)) {
		e->mYAccel = makeDreamStringMugenAssignment("");
	}
	if (!fetchDreamAssignmentFromGroupAndReturnWhetherItExists("ground.velocity", tGroup, &e->mGroundVelocity)) {
		e->mGroundVelocity = makeDreamStringMugenAssignment("0 , 0");
	}
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("guard.velocity", tGroup, &e->mGuardVelocity, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("air.velocity", tGroup, &e->mAirVelocity, "0 , 0");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("airguard.velocity", tGroup, &e->mAirGuardVelocity, "");

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("ground.cornerpush.veloff", tGroup, &e->mGroundCornerPushVelocityOffset, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("air.cornerpush.veloff", tGroup, &e->mAirCornerPushVelocityOffset, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("down.cornerpush.veloff", tGroup, &e->mDownCornerPushVelocityOffset, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("guard.cornerpush.veloff", tGroup, &e->mGuardCornerPushVelocityOffset, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("airguard.cornerpush.veloff", tGroup, &e->mAirGuardCornerPushVelocityOffset, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("airguard.ctrltime", tGroup, &e->mAirGuardControlTime, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("air.juggle", tGroup, &e->mAirJuggle, "0");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("mindist", tGroup, &e->mMinimumDistance, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("maxdist", tGroup, &e->mMaximumDistance, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("snap", tGroup, &e->mSnap, "");

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("p1sprpriority", tGroup, &e->mPlayerSpritePriority1, "1");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("p2sprpriority", tGroup, &e->mPlayerSpritePriority2, "0");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("p1facing", tGroup, &e->mPlayer1ChangeFaceDirection, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("p1getp2facing", tGroup, &e->mPlayer1ChangeFaceDirectionRelativeToPlayer2, "0");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("p2facing", tGroup, &e->mPlayer2ChangeFaceDirectionRelativeToPlayer1, "0");

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("p1stateno", tGroup, &e->mPlayer1StateNumber, "-1");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("p2stateno", tGroup, &e->mPlayer2StateNumber, "-1");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("p2getp1state", tGroup, &e->mPlayer2CapableOfGettingPlayer1State, "1");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("forcestand", tGroup, &e->mForceStanding, "");

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("fall", tGroup, &e->mFall, "0");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("fall.xvelocity", tGroup, &e->mFallXVelocity, "0");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("fall.yvelocity", tGroup, &e->mFallYVelocity, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("fall.recover", tGroup, &e->mFallCanBeRecovered, "1");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("fall.recovertime", tGroup, &e->mFallRecoveryTime, "4");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("fall.damage", tGroup, &e->mFallDamage, "0");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("air.fall", tGroup, &e->mAirFall, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("forcenofall", tGroup, &e->mForceNoFall, "0");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("down.velocity", tGroup, &e->mDownVelocity, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("down.hittime", tGroup, &e->mDownHitTime, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("down.bounce", tGroup, &e->mDownBounce, "0");

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("id", tGroup, &e->mHitID, "0");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("chainID", tGroup, &e->mChainID, "-1");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("nochainID", tGroup, &e->mNoChainID, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("hitonce", tGroup, &e->mHitOnce, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("kill", tGroup, &e->mKill, "1");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("guard.kill", tGroup, &e->mGuardKill, "1");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("fall.kill", tGroup, &e->mFallKill, "1");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("numhits", tGroup, &e->mNumberOfHits, "1");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("getpower", tGroup, &e->mGetPower, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("givepower", tGroup, &e->mGivePower, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("palfx.time", tGroup, &e->mPaletteEffectTime, "0");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("palfx.mul", tGroup, &e->mPaletteEffectMultiplication, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("palfx.add", tGroup, &e->mPaletteEffectAddition, "");

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("envshake.time", tGroup, &e->mEnvironmentShakeTime, "0");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("envshake.freq", tGroup, &e->mEnvironmentShakeFrequency, "0");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("envshake.ampl", tGroup, &e->mEnvironmentShakeAmplitude, "0");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("envshake.phase", tGroup, &e->mEnvironmentShakePhase, "0");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("fall.envshake.time", tGroup, &e->mFallEnvironmentShakeTime, "0");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("fall.envshake.freq", tGroup, &e->mFallEnvironmentShakeFrequency, "0");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("fall.envshake.ampl", tGroup, &e->mFallEnvironmentShakeAmplitude, "0");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("fall.envshake.phase", tGroup, &e->mFallEnvironmentShakePhase, "0");

}

static void parseHitDefinitionController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) {
	HitDefinitionController* e = allocMemoryOnMemoryStackOrMemory(sizeof(HitDefinitionController));
	readHitDefinitionFromGroup(e, tGroup);

	tController->mType = MUGEN_STATE_CONTROLLER_TYPE_HIT_DEFINITION;
	tController->mData = e;
}

static void unloadHitDefinitionData(HitDefinitionController* e) {
	destroyDreamMugenAssignment(e->mAttribute);
	destroyDreamMugenAssignment(e->mHitFlag);

	destroyDreamMugenAssignment(e->mGuardFlag);
	destroyDreamMugenAssignment(e->mAffectTeam);
	destroyDreamMugenAssignment(e->mAnimationType);
	destroyDreamMugenAssignment(e->mAirAnimationType);
	destroyDreamMugenAssignment(e->mFallAnimationType);


	destroyDreamMugenAssignment(e->mPriority);
	destroyDreamMugenAssignment(e->mDamage);
	destroyDreamMugenAssignment(e->mPauseTime);
	destroyDreamMugenAssignment(e->mGuardPauseTime);
	destroyDreamMugenAssignment(e->mSparkNumber);
	destroyDreamMugenAssignment(e->mGuardSparkNumber);


	destroyDreamMugenAssignment(e->mSparkXY);
	destroyDreamMugenAssignment(e->mHitSound);
	destroyDreamMugenAssignment(e->mGuardSound);
	destroyDreamMugenAssignment(e->mGroundType);
	destroyDreamMugenAssignment(e->mAirType);

	destroyDreamMugenAssignment(e->mGroundSlideTime);
	destroyDreamMugenAssignment(e->mGuardSlideTime);
	destroyDreamMugenAssignment(e->mGroundHitTime);
	destroyDreamMugenAssignment(e->mGuardHitTime);
	destroyDreamMugenAssignment(e->mAirHitTime);

	destroyDreamMugenAssignment(e->mGuardControlTime);
	destroyDreamMugenAssignment(e->mGuardDistance);
	destroyDreamMugenAssignment(e->mYAccel);
	destroyDreamMugenAssignment(e->mGroundVelocity);
	destroyDreamMugenAssignment(e->mGuardVelocity);
	destroyDreamMugenAssignment(e->mAirVelocity);
	destroyDreamMugenAssignment(e->mAirGuardVelocity);

	destroyDreamMugenAssignment(e->mGroundCornerPushVelocityOffset);
	destroyDreamMugenAssignment(e->mAirCornerPushVelocityOffset);
	destroyDreamMugenAssignment(e->mDownCornerPushVelocityOffset);
	destroyDreamMugenAssignment(e->mGuardCornerPushVelocityOffset);
	destroyDreamMugenAssignment(e->mAirGuardCornerPushVelocityOffset);

	destroyDreamMugenAssignment(e->mAirGuardControlTime);
	destroyDreamMugenAssignment(e->mAirJuggle);
	destroyDreamMugenAssignment(e->mMinimumDistance);
	destroyDreamMugenAssignment(e->mMaximumDistance);
	destroyDreamMugenAssignment(e->mSnap);

	destroyDreamMugenAssignment(e->mPlayerSpritePriority1);
	destroyDreamMugenAssignment(e->mPlayerSpritePriority2);

	destroyDreamMugenAssignment(e->mPlayer1ChangeFaceDirection);
	destroyDreamMugenAssignment(e->mPlayer1ChangeFaceDirectionRelativeToPlayer2);
	destroyDreamMugenAssignment(e->mPlayer2ChangeFaceDirectionRelativeToPlayer1);

	destroyDreamMugenAssignment(e->mPlayer1StateNumber);
	destroyDreamMugenAssignment(e->mPlayer2StateNumber);
	destroyDreamMugenAssignment(e->mPlayer2CapableOfGettingPlayer1State);
	destroyDreamMugenAssignment(e->mForceStanding);

	destroyDreamMugenAssignment(e->mFall);
	destroyDreamMugenAssignment(e->mFallXVelocity);
	destroyDreamMugenAssignment(e->mFallYVelocity);
	destroyDreamMugenAssignment(e->mFallCanBeRecovered);
	destroyDreamMugenAssignment(e->mFallRecoveryTime);
	destroyDreamMugenAssignment(e->mFallDamage);

	destroyDreamMugenAssignment(e->mAirFall);
	destroyDreamMugenAssignment(e->mForceNoFall);
	destroyDreamMugenAssignment(e->mDownVelocity);
	destroyDreamMugenAssignment(e->mDownHitTime);
	destroyDreamMugenAssignment(e->mDownBounce);

	destroyDreamMugenAssignment(e->mHitID);
	destroyDreamMugenAssignment(e->mChainID);
	destroyDreamMugenAssignment(e->mNoChainID);
	destroyDreamMugenAssignment(e->mHitOnce);

	destroyDreamMugenAssignment(e->mKill);
	destroyDreamMugenAssignment(e->mGuardKill);
	destroyDreamMugenAssignment(e->mFallKill);
	destroyDreamMugenAssignment(e->mNumberOfHits);

	destroyDreamMugenAssignment(e->mGetPower);
	destroyDreamMugenAssignment(e->mGivePower);

	destroyDreamMugenAssignment(e->mPaletteEffectTime);
	destroyDreamMugenAssignment(e->mPaletteEffectMultiplication);
	destroyDreamMugenAssignment(e->mPaletteEffectAddition);

	destroyDreamMugenAssignment(e->mEnvironmentShakeTime);
	destroyDreamMugenAssignment(e->mEnvironmentShakeFrequency);
	destroyDreamMugenAssignment(e->mEnvironmentShakeAmplitude);
	destroyDreamMugenAssignment(e->mEnvironmentShakePhase);

	destroyDreamMugenAssignment(e->mFallEnvironmentShakeTime);
	destroyDreamMugenAssignment(e->mFallEnvironmentShakeFrequency);
	destroyDreamMugenAssignment(e->mFallEnvironmentShakeAmplitude);
	destroyDreamMugenAssignment(e->mFallEnvironmentShakePhase);
}

static void unloadHitDefinitionController(DreamMugenStateController* tController) {
	HitDefinitionController* e = tController->mData;

	unloadHitDefinitionData(e);

	freeMemory(e);
}

typedef struct {
	DreamMugenAssignment* mValue;

	DreamMugenAssignment* mVolumeScale;
	DreamMugenAssignment* mChannel;
	DreamMugenAssignment* mLowPriority;
	DreamMugenAssignment* mFrequencyMultiplier;
	DreamMugenAssignment* mLoop;
	DreamMugenAssignment* mPanning;
	DreamMugenAssignment* mAbsolutePanning;

} PlaySoundController;


static void parsePlaySoundController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) {
	PlaySoundController* e = allocMemoryOnMemoryStackOrMemory(sizeof(PlaySoundController));

	assert(fetchDreamAssignmentFromGroupAndReturnWhetherItExists("value", tGroup, &e->mValue));

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("volumescale", tGroup, &e->mVolumeScale, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("channel", tGroup, &e->mChannel, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("lowpriority", tGroup, &e->mLowPriority, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("freqmul", tGroup, &e->mFrequencyMultiplier, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("loop", tGroup, &e->mLoop, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("pan", tGroup, &e->mPanning, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("abspan", tGroup, &e->mAbsolutePanning, "");

	tController->mType = MUGEN_STATE_CONTROLLER_TYPE_PLAY_SOUND;
	tController->mData = e;
}

static void unloadPlaySoundController(DreamMugenStateController* tController) {
	PlaySoundController* e = tController->mData;

	destroyDreamMugenAssignment(e->mValue);

	destroyDreamMugenAssignment(e->mVolumeScale);
	destroyDreamMugenAssignment(e->mChannel);
	destroyDreamMugenAssignment(e->mLowPriority);
	destroyDreamMugenAssignment(e->mFrequencyMultiplier);
	destroyDreamMugenAssignment(e->mLoop);
	destroyDreamMugenAssignment(e->mPanning);
	destroyDreamMugenAssignment(e->mAbsolutePanning);

	freeMemory(e);
}

typedef struct {
	DreamMugenAssignment* mValue;

	DreamMugenAssignment* mEdge;
	DreamMugenAssignment* mPlayer;

	uint8_t mHasValue;
} WidthController;


static void parseWidthController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) {
	WidthController* e = allocMemoryOnMemoryStackOrMemory(sizeof(WidthController));
	
	e->mHasValue = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("value", tGroup, &e->mValue);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("edge", tGroup, &e->mEdge, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("player", tGroup, &e->mPlayer, "");

	tController->mType = MUGEN_STATE_CONTROLLER_TYPE_WIDTH;
	tController->mData = e;
}

static void unloadWidthController(DreamMugenStateController* tController) {
	WidthController* e = tController->mData;

	if (e->mHasValue) {
		destroyDreamMugenAssignment(e->mValue);
	}

	destroyDreamMugenAssignment(e->mEdge);
	destroyDreamMugenAssignment(e->mPlayer);

	freeMemory(e);
}

typedef struct {
	DreamMugenAssignment* tNewAnimation;

	DreamMugenAssignment* tStep;
} ChangeAnimationController;


static void parseChangeAnimationController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup, DreamMugenStateControllerType tType) {
	ChangeAnimationController* e = allocMemoryOnMemoryStackOrMemory(sizeof(ChangeAnimationController));

	assert(fetchDreamAssignmentFromGroupAndReturnWhetherItExists("value", tGroup, &e->tNewAnimation));


	if (!fetchDreamAssignmentFromGroupAndReturnWhetherItExists("elem", tGroup, &e->tStep)) {
		e->tStep = makeDreamNumberMugenAssignment(0);
	}

	tController->mType = tType;
	tController->mData = e;
}

static void unloadChangeAnimationController(DreamMugenStateController* tController) {
	ChangeAnimationController* e = tController->mData;

	destroyDreamMugenAssignment(e->tNewAnimation);
	destroyDreamMugenAssignment(e->tStep);

	freeMemory(e);
}

typedef struct {
	DreamMugenAssignment* tValue;
} ControlSettingController;


static void parseControlSettingController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) {
	ControlSettingController* e = allocMemoryOnMemoryStackOrMemory(sizeof(ControlSettingController));

	assert(fetchDreamAssignmentFromGroupAndReturnWhetherItExists("value", tGroup, &e->tValue));

	tController->mType = MUGEN_STATE_CONTROLLER_TYPE_SET_CONTROL;
	tController->mData = e;
}

static void unloadControlSettingController(DreamMugenStateController* tController) {
	ControlSettingController* e = tController->mData;

	destroyDreamMugenAssignment(e->tValue);

	freeMemory(e);
}

typedef struct {
	DreamMugenAssignment* tValue;
} SpritePriorityController;


static void parseSpritePriorityController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) {
	SpritePriorityController* e = allocMemoryOnMemoryStackOrMemory(sizeof(SpritePriorityController));

	assert(fetchDreamAssignmentFromGroupAndReturnWhetherItExists("value", tGroup, &e->tValue));

	tController->mType = MUGEN_STATE_CONTROLLER_TYPE_SPRITE_PRIORITY;
	tController->mData = e;
}

static void unloadSpritePriorityController(DreamMugenStateController* tController) {
	SpritePriorityController* e = tController->mData;

	destroyDreamMugenAssignment(e->tValue);

	freeMemory(e);
}

typedef struct {
	DreamMugenAssignment* mFlag;
	DreamMugenAssignment* mFlag2;
	DreamMugenAssignment* mFlag3;

	uint8_t mHasFlag2;
	uint8_t mHasFlag3;
} SpecialAssertController;


static void parseSpecialAssertController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) {
	SpecialAssertController* e = allocMemoryOnMemoryStackOrMemory(sizeof(SpecialAssertController));

	assert(fetchDreamAssignmentFromGroupAndReturnWhetherItExists("flag", tGroup, &e->mFlag));
	e->mHasFlag2 = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("flag2", tGroup, &e->mFlag2);
	e->mHasFlag3 = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("flag3", tGroup, &e->mFlag3);

	tController->mType = MUGEN_STATE_CONTROLLER_TYPE_ASSERT_SPECIAL;
	tController->mData = e;
}

static void unloadSpecialAssertController(DreamMugenStateController* tController) {
	SpecialAssertController* e = tController->mData;

	destroyDreamMugenAssignment(e->mFlag);
	if (e->mHasFlag2) {
		destroyDreamMugenAssignment(e->mFlag2);
	}
	if (e->mHasFlag3) {
		destroyDreamMugenAssignment(e->mFlag3);
	}

	freeMemory(e);
}

typedef struct {
	DreamMugenAssignment* mPositionOffset;
	DreamMugenAssignment* mPositionOffset2;
	DreamMugenAssignment* mSpacing;

	uint8_t mHasSecondDustCloud;
} MakeDustController;


static void parseMakeDustController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) {
	MakeDustController* e = allocMemoryOnMemoryStackOrMemory(sizeof(MakeDustController));

	if (!fetchDreamAssignmentFromGroupAndReturnWhetherItExists("pos", tGroup, &e->mPositionOffset)) {
		e->mPositionOffset = makeDream2DVectorMugenAssignment(makePosition(0, 0, 0));
	}

	e->mHasSecondDustCloud = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("pos2", tGroup, &e->mPositionOffset2);

	if (!fetchDreamAssignmentFromGroupAndReturnWhetherItExists("spacing", tGroup, &e->mSpacing)) {
		e->mSpacing = makeDreamNumberMugenAssignment(1);
	}

	tController->mType = MUGEN_STATE_CONTROLLER_TYPE_MAKE_DUST;
	tController->mData = e;
}

static void unloadMakeDustController(DreamMugenStateController* tController) {
	MakeDustController* e = tController->mData;

	destroyDreamMugenAssignment(e->mPositionOffset);
	if (e->mHasSecondDustCloud) {
		destroyDreamMugenAssignment(e->mPositionOffset2);
	}
	destroyDreamMugenAssignment(e->mSpacing);

	freeMemory(e);
}

typedef struct {
	uint8_t mDummy;
} NullController;


static void parseNullController(DreamMugenStateController* tController, DreamMugenStateControllerType tType) {
	NullController* e = allocMemoryOnMemoryStackOrMemory(sizeof(NullController));
	e->mDummy = 0;

	tController->mType = tType;
	tController->mData = e;
}

static void unloadNullController(DreamMugenStateController* tController) {
	freeMemory(tController->mData);
}

typedef enum {
	VAR_SET_TYPE_SYSTEM,
	VAR_SET_TYPE_SYSTEM_FLOAT,
	VAR_SET_TYPE_INTEGER,
	VAR_SET_TYPE_FLOAT
} VarSetType;

typedef struct {
	int mID;
	DreamMugenAssignment* mAssignment;

	uint8_t mType;
} VarSetControllerEntry;

typedef struct {
	Vector mVarSets;
} VarSetController;

typedef struct {
	VarSetController* mController;
	MugenDefScriptGroup* mGroup;
} VarSetControllerCaller;

static void parseSingleVarSetControllerEntry(void* tCaller, char* tName, void* tData) {
	VarSetControllerCaller* caller = tCaller;
	(void)tData;

	int isEntry = strchr(tName, '(') != NULL;
	if (!isEntry) return;

	VarSetControllerEntry* e = allocMemoryOnMemoryStackOrMemory(sizeof(VarSetControllerEntry));

	char name[100];
	strcpy(name, tName);
	char* start = strchr(name, '(');
	assert(start != NULL);
	*start = '\0';

	char value[100];
	strcpy(value, start + 1);
	char* end = strchr(value, ')');
	assert(end != NULL);
	*end = '\0';


	if (!strcmp("var", name)) {
		e->mType = VAR_SET_TYPE_INTEGER;
	}
	else if (!strcmp("sysvar", name)) {
		e->mType = VAR_SET_TYPE_SYSTEM;
	}
	else if (!strcmp("sysfvar", name)) {
		e->mType = VAR_SET_TYPE_SYSTEM_FLOAT;
	}
	else if (!strcmp("fvar", name)) {
		e->mType = VAR_SET_TYPE_FLOAT;
	}
	else {
		logWarningFormat("Unrecognized variable setting name %s. Default to var.", name);
		e->mType = VAR_SET_TYPE_INTEGER;
	}

	e->mID = atoi(value);

	assert(fetchDreamAssignmentFromGroupAndReturnWhetherItExists(tName, caller->mGroup, &e->mAssignment));

	vector_push_back_owned(&caller->mController->mVarSets, e);
}

static void loadSingleOriginalVarSetController(Vector* tDst, MugenDefScriptGroup* tGroup, MugenDefScriptGroupElement* tIDElement, VarSetType tType) {
	VarSetControllerEntry* e = allocMemoryOnMemoryStackOrMemory(sizeof(VarSetControllerEntry));
	e->mType = tType;
	e->mID = getMugenDefNumberVariableAsElement(tIDElement);
	assert(fetchDreamAssignmentFromGroupAndReturnWhetherItExists("value", tGroup, &e->mAssignment));

	vector_push_back_owned(tDst, e);
}

static void unloadSingleVarSetEntry(void* tCaller, void* tData) {
	(void)tCaller;
	VarSetControllerEntry* e = tData;
	destroyDreamMugenAssignment(e->mAssignment);
}

static void parseVarSetController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup, DreamMugenStateControllerType tType) {
	int isIntegerVersion = string_map_contains(&tGroup->mElements, "v");
	int isFloatVersion = string_map_contains(&tGroup->mElements, "fv");

	VarSetController* e = allocMemoryOnMemoryStackOrMemory(sizeof(VarSetController));
	e->mVarSets = new_vector();

	if (isIntegerVersion) {
		loadSingleOriginalVarSetController(&e->mVarSets, tGroup, string_map_get(&tGroup->mElements, "v"), VAR_SET_TYPE_INTEGER);
	}
	else if (isFloatVersion) {
		loadSingleOriginalVarSetController(&e->mVarSets, tGroup, string_map_get(&tGroup->mElements, "fv"), VAR_SET_TYPE_FLOAT);
	}
	else {
		VarSetControllerCaller caller;
		caller.mController = e;
		caller.mGroup = tGroup;

		string_map_map(&tGroup->mElements, parseSingleVarSetControllerEntry, &caller);
	}

	if (vector_size(&e->mVarSets) != 1) {
		logWarning("Unable to parse VarSetController. Missing elements. Defaulting to Null controller.");
		vector_map(&e->mVarSets, unloadSingleVarSetEntry, NULL);
		delete_vector(&e->mVarSets);
		freeMemory(e);
		parseNullController(tController, MUGEN_STATE_CONTROLLER_TYPE_NULL);
		return;
	}

	tController->mType = tType;
	tController->mData = e;
}



static void unloadVarSetController(DreamMugenStateController* tController) {
	VarSetController* e = tController->mData;
	vector_map(&e->mVarSets, unloadSingleVarSetEntry, NULL);
	delete_vector(&e->mVarSets);
	freeMemory(e);
}

typedef struct {
	DreamMugenAssignment* mValue;
	DreamMugenAssignment* mFirst;
	DreamMugenAssignment* mLast;

	uint8_t mType;
} VarRangeSetController;

static void parseVarRangeSetController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) {
	VarRangeSetController* e = allocMemoryOnMemoryStackOrMemory(sizeof(VarRangeSetController));

	int isIntegerVersion = string_map_contains(&tGroup->mElements, "value");

	if (isIntegerVersion) {
		e->mType = VAR_SET_TYPE_INTEGER;
		assert(fetchDreamAssignmentFromGroupAndReturnWhetherItExists("value", tGroup, &e->mValue));
	}
	else {
		e->mType = VAR_SET_TYPE_FLOAT;
		assert(fetchDreamAssignmentFromGroupAndReturnWhetherItExists("fvalue", tGroup, &e->mValue));
	}

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("first", tGroup, &e->mFirst, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("last", tGroup, &e->mLast, "");
 
	tController->mType = MUGEN_STATE_CONTROLLER_TYPE_SET_VARIABLE_RANGE;
	tController->mData = e;
}

static void unloadVarRangeSetController(DreamMugenStateController* tController) {
	VarRangeSetController* e = tController->mData;
	destroyDreamMugenAssignment(e->mValue);
	destroyDreamMugenAssignment(e->mFirst);
	destroyDreamMugenAssignment(e->mLast);
	freeMemory(e);
}

typedef struct {
	DreamMugenAssignment* mStateType;
	DreamMugenAssignment* mMoveType;
	DreamMugenAssignment* mPhysics;

	uint8_t mHasStateType;
	uint8_t mHasMoveType;
	uint8_t mHasPhysics;
} StateTypeSetController;


static void parseStateTypeSetController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) {
	StateTypeSetController* e = allocMemoryOnMemoryStackOrMemory(sizeof(StateTypeSetController));

	e->mHasStateType = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("statetype", tGroup, &e->mStateType);
	e->mHasMoveType = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("movetype", tGroup, &e->mMoveType);
	e->mHasPhysics = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("physics", tGroup, &e->mPhysics);

	tController->mType = MUGEN_STATE_CONTROLLER_TYPE_SET_STATE_TYPE;
	tController->mData = e;
}

static void unloadStateTypeSetController(DreamMugenStateController* tController) {
	StateTypeSetController* e = tController->mData;

	if (e->mHasStateType) {
		destroyDreamMugenAssignment(e->mStateType);
	}
	if (e->mHasMoveType) {
		destroyDreamMugenAssignment(e->mMoveType);
	}
	if (e->mHasPhysics) {
		destroyDreamMugenAssignment(e->mPhysics);
	}

	freeMemory(e);
}

typedef struct {
	DreamMugenAssignment* mWaveform;
	DreamMugenAssignment* mTime;
	DreamMugenAssignment* mFrequency;
	DreamMugenAssignment* mAmplitude;
	DreamMugenAssignment* mSelf;

} ForceFeedbackController;

static void parseForceFeedbackController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) {
	ForceFeedbackController*e = allocMemoryOnMemoryStackOrMemory(sizeof(ForceFeedbackController));

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("waveform", tGroup, &e->mWaveform, "sine");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("time", tGroup, &e->mTime, "sine");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("freq", tGroup, &e->mFrequency, "sine");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("ampl", tGroup, &e->mAmplitude, "sine");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("self", tGroup, &e->mSelf, "sine");

	tController->mType = MUGEN_STATE_CONTROLLER_TYPE_FORCE_FEEDBACK;
	tController->mData = e;
}

static void unloadForceFeedbackController(DreamMugenStateController* tController) {
	ForceFeedbackController*e = tController->mData;

	destroyDreamMugenAssignment(e->mWaveform);
	destroyDreamMugenAssignment(e->mTime);
	destroyDreamMugenAssignment(e->mFrequency);
	destroyDreamMugenAssignment(e->mAmplitude);
	destroyDreamMugenAssignment(e->mSelf);

	freeMemory(e);
}

typedef struct {
	DreamMugenAssignment* mValue;
} DefenseMultiplierController;

static void parseDefenseMultiplierController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) {
	DefenseMultiplierController* e = allocMemoryOnMemoryStackOrMemory(sizeof(DefenseMultiplierController));

	assert(fetchDreamAssignmentFromGroupAndReturnWhetherItExists("value", tGroup, &e->mValue));

	tController->mType = MUGEN_STATE_CONTROLLER_TYPE_SET_DEFENSE_MULTIPLIER;
	tController->mData = e;
}

static void unloadDefenseMultiplierController(DreamMugenStateController* tController) {
	DefenseMultiplierController* e = tController->mData;
	destroyDreamMugenAssignment(e->mValue);
	freeMemory(e);
}


typedef struct {
	DreamMugenAssignment* mAnim;

	DreamMugenAssignment* mID;
	DreamMugenAssignment* mPosition;
	DreamMugenAssignment* mPositionType;
	DreamMugenAssignment* mHorizontalFacing;
	DreamMugenAssignment* mVerticalFacing;
	DreamMugenAssignment* mBindTime;
	DreamMugenAssignment* mVelocity;
	DreamMugenAssignment* mAcceleration;
	DreamMugenAssignment* mRandomOffset;
	DreamMugenAssignment* mRemoveTime;
	DreamMugenAssignment* mSuperMove;
	DreamMugenAssignment* mSuperMoveTime;
	DreamMugenAssignment* mPauseMoveTime;
	DreamMugenAssignment* mScale;
	DreamMugenAssignment* mSpritePriority;
	DreamMugenAssignment* mOnTop;
	DreamMugenAssignment* mShadow;
	DreamMugenAssignment* mOwnPalette;
	DreamMugenAssignment* mIsRemovedOnGetHit;
	DreamMugenAssignment* mIgnoreHitPause;
	DreamMugenAssignment* mTransparencyType;

	uint8_t mHasTransparencyType;
} ExplodController;

static void parseExplodController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) {
	ExplodController* e = allocMemoryOnMemoryStackOrMemory(sizeof(ExplodController));

	assert(fetchDreamAssignmentFromGroupAndReturnWhetherItExists("anim", tGroup, &e->mAnim));
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("id", tGroup, &e->mID, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("pos", tGroup, &e->mPosition, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("postype", tGroup, &e->mPositionType, "p1");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("facing", tGroup, &e->mHorizontalFacing, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("vfacing", tGroup, &e->mVerticalFacing, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("bindtime", tGroup, &e->mBindTime, "");
	if (string_map_contains(&tGroup->mElements, "vel")) {
		fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("vel", tGroup, &e->mVelocity, "");
	}
	else {
		fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("velocity", tGroup, &e->mVelocity, "");
	}
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("accel", tGroup, &e->mAcceleration, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("random", tGroup, &e->mRandomOffset, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("removetime", tGroup, &e->mRemoveTime, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("supermove", tGroup, &e->mSuperMove, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("supermovetime", tGroup, &e->mSuperMoveTime, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("pausemovetime", tGroup, &e->mPauseMoveTime, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("scale", tGroup, &e->mScale, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("sprpriority", tGroup, &e->mSpritePriority, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("ontop", tGroup, &e->mOnTop, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("shadow", tGroup, &e->mShadow, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("ownpal", tGroup, &e->mOwnPalette, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("removeongethit", tGroup, &e->mIsRemovedOnGetHit, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("ignorehitpause", tGroup, &e->mIgnoreHitPause, "");
	e->mHasTransparencyType = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("trans", tGroup, &e->mTransparencyType);

	tController->mType = MUGEN_STATE_CONTROLLER_TYPE_EXPLOD;
	tController->mData = e;
}

static void unloadExplodController(DreamMugenStateController* tController) {
	ExplodController* e = tController->mData;

	destroyDreamMugenAssignment(e->mAnim);

	destroyDreamMugenAssignment(e->mID);
	destroyDreamMugenAssignment(e->mPosition);
	destroyDreamMugenAssignment(e->mPositionType);
	destroyDreamMugenAssignment(e->mHorizontalFacing);
	destroyDreamMugenAssignment(e->mVerticalFacing);
	destroyDreamMugenAssignment(e->mBindTime);
	destroyDreamMugenAssignment(e->mVelocity);
	destroyDreamMugenAssignment(e->mAcceleration);
	destroyDreamMugenAssignment(e->mRandomOffset);
	destroyDreamMugenAssignment(e->mRemoveTime);
	destroyDreamMugenAssignment(e->mSuperMove);
	destroyDreamMugenAssignment(e->mSuperMoveTime);
	destroyDreamMugenAssignment(e->mPauseMoveTime);
	destroyDreamMugenAssignment(e->mScale);
	destroyDreamMugenAssignment(e->mSpritePriority);
	destroyDreamMugenAssignment(e->mOnTop);
	destroyDreamMugenAssignment(e->mShadow);
	destroyDreamMugenAssignment(e->mOwnPalette);
	destroyDreamMugenAssignment(e->mIsRemovedOnGetHit);
	destroyDreamMugenAssignment(e->mIgnoreHitPause);

	if (e->mHasTransparencyType) {
		destroyDreamMugenAssignment(e->mTransparencyType);
	}

	freeMemory(e);
}

static void parseModifyExplodController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) {
	ExplodController* e = allocMemoryOnMemoryStackOrMemory(sizeof(ExplodController));

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("anim", tGroup, &e->mAnim, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("id", tGroup, &e->mID, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("pos", tGroup, &e->mPosition, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("postype", tGroup, &e->mPositionType, "p1");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("facing", tGroup, &e->mHorizontalFacing, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("vfacing", tGroup, &e->mVerticalFacing, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("bindtime", tGroup, &e->mBindTime, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("vel", tGroup, &e->mVelocity, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("accel", tGroup, &e->mAcceleration, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("random", tGroup, &e->mRandomOffset, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("removetime", tGroup, &e->mRemoveTime, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("supermove", tGroup, &e->mSuperMove, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("supermovetime", tGroup, &e->mSuperMoveTime, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("pausemovetime", tGroup, &e->mPauseMoveTime, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("scale", tGroup, &e->mScale, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("sprpriority", tGroup, &e->mSpritePriority, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("ontop", tGroup, &e->mOnTop, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("shadow", tGroup, &e->mShadow, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("ownpal", tGroup, &e->mOwnPalette, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("removeongethit", tGroup, &e->mIsRemovedOnGetHit, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("ignorehitpause", tGroup, &e->mIgnoreHitPause, "");
	e->mHasTransparencyType = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("trans", tGroup, &e->mTransparencyType);

	tController->mType = MUGEN_STATE_CONTROLLER_TYPE_MODIFY_EXPLOD;
	tController->mData = e;
}

static void unloadModifyExplodController(DreamMugenStateController* tController) {
	unloadExplodController(tController);
}

typedef struct {
	DreamMugenAssignment* mValue;
} PositionFreezeController;

static void parsePositionFreezeController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) {
	PositionFreezeController* e = allocMemoryOnMemoryStackOrMemory(sizeof(PositionFreezeController));

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("value", tGroup, &e->mValue, "1");

	tController->mType = MUGEN_STATE_CONTROLLER_TYPE_FREEZE_POSITION;
	tController->mData = e;
}

static void unloadPositionFreezeController(DreamMugenStateController* tController) {
	PositionFreezeController* e = tController->mData;
	destroyDreamMugenAssignment(e->mValue);
	freeMemory(e);
}

typedef struct {
	MugenStringVector mValue;
	MugenStringVector mValue2;
	DreamMugenAssignment* mTime;

	uint8_t mHasValue;
	uint8_t mHasValue2;
} NotHitByController;

static void readMugenDefStringVector(MugenStringVector* tDst, MugenDefScriptGroup* tGroup, char* tName, uint8_t* oHasValue) {
	if (string_map_contains(&tGroup->mElements, tName)) {
		MugenDefScriptGroupElement* elem = string_map_get(&tGroup->mElements, tName);
		*oHasValue = 1;
		if (isMugenDefStringVectorVariableAsElement(elem)) {
			*tDst = copyMugenDefStringVectorVariableAsElement(elem);
		}
		else {
			char* text = getAllocatedMugenDefStringVariableAsElement(elem);
			tDst->mSize = 1;
			tDst->mElement = allocMemoryOnMemoryStackOrMemory(sizeof(char*));
			tDst->mElement[0] = allocMemoryOnMemoryStackOrMemory(strlen(text) + 10);
			strcpy(tDst->mElement[0], text);
			freeMemory(text);
		}
	}
	else {
		*oHasValue = 0;
	}

}

static void parseNotHitByController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup, DreamMugenStateControllerType tType) {
	NotHitByController* e = allocMemoryOnMemoryStackOrMemory(sizeof(NotHitByController));

	readMugenDefStringVector(&e->mValue, tGroup, "value", &e->mHasValue);
	readMugenDefStringVector(&e->mValue2, tGroup, "value2", &e->mHasValue2);
	assert(&e->mHasValue || e->mHasValue2);

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("time", tGroup, &e->mTime, "1");
	
	tController->mType = tType;
	tController->mData = e;
}

static void unloadMugenDefStringVector(MugenStringVector tVector) {
	int i;
	for (i = 0; i < tVector.mSize; i++) {
		freeMemory(tVector.mElement[i]);
	}

	freeMemory(tVector.mElement);
}

static void unloadNotHitByController(DreamMugenStateController* tController) {
	NotHitByController* e = tController->mData;

	if (e->mHasValue) {
		unloadMugenDefStringVector(e->mValue);
	}
	if (e->mHasValue2) {
		unloadMugenDefStringVector(e->mValue2);
	}
	destroyDreamMugenAssignment(e->mTime);

	freeMemory(e);
}

typedef struct {
	DreamMugenAssignment* mValue;
	DreamMugenAssignment* mXVelocity;
	DreamMugenAssignment* mYVelocity;

	uint8_t mHasXVelocity;
	uint8_t mHasYVelocity;
} HitFallSetController;

static void parseHitFallSetController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) {
	HitFallSetController* e = allocMemoryOnMemoryStackOrMemory(sizeof(HitFallSetController));

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("value", tGroup, &e->mValue, "-1");
	e->mHasXVelocity = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("xvel", tGroup, &e->mXVelocity);
	e->mHasYVelocity = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("yvel", tGroup, &e->mYVelocity);

	tController->mType = MUGEN_STATE_CONTROLLER_TYPE_SET_HIT_FALL;
	tController->mData = e;
}

static void unloadHitFallSetController(DreamMugenStateController* tController) {
	HitFallSetController* e = tController->mData;

	destroyDreamMugenAssignment(e->mValue);
	if (e->mHasXVelocity) {
		destroyDreamMugenAssignment(e->mXVelocity);
	}
	if (e->mHasYVelocity) {
		destroyDreamMugenAssignment(e->mYVelocity);
	}

	freeMemory(e);
}

typedef struct {
	DreamMugenAssignment* mValue;
} SingleRequiredValueController;

static void parseSingleRequiredValueController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup, DreamMugenStateControllerType tType) {
	SingleRequiredValueController* e = allocMemoryOnMemoryStackOrMemory(sizeof(SingleRequiredValueController));

	assert(fetchDreamAssignmentFromGroupAndReturnWhetherItExists("value", tGroup, &e->mValue));

	tController->mType = tType;
	tController->mData = e;
}

static void unloadSingleRequiredValueController(DreamMugenStateController* tController) {
	SingleRequiredValueController* e = tController->mData;
	destroyDreamMugenAssignment(e->mValue);
	freeMemory(e);
}

typedef struct {
	DreamMugenAssignment* mTime;

	DreamMugenAssignment* mFrequency;
	DreamMugenAssignment* mAmplitude;
	DreamMugenAssignment* mPhaseOffset;
} EnvironmentShakeController;

static void parseEnvironmentShakeController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) {
	EnvironmentShakeController* e = allocMemoryOnMemoryStackOrMemory(sizeof(EnvironmentShakeController));

	assert(fetchDreamAssignmentFromGroupAndReturnWhetherItExists("time", tGroup, &e->mTime));
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("freq", tGroup, &e->mFrequency, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("ampl", tGroup, &e->mAmplitude, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("phase", tGroup, &e->mPhaseOffset, "");

	tController->mType = MUGEN_STATE_CONTROLLER_TYPE_ENVIRONMENT_SHAKE;
	tController->mData = e;
}

static void unloadEnvironmentShakeController(DreamMugenStateController* tController) {
	EnvironmentShakeController* e = tController->mData;

	destroyDreamMugenAssignment(e->mTime);
	destroyDreamMugenAssignment(e->mFrequency);
	destroyDreamMugenAssignment(e->mAmplitude);
	destroyDreamMugenAssignment(e->mPhaseOffset);

	freeMemory(e);
}

typedef struct {
	DreamMugenAssignment* mTime;
	DreamMugenAssignment* mBufferTimeForCommandsDuringPauseEnd;
	DreamMugenAssignment* mMoveTime;
	DreamMugenAssignment* mDoesPauseBackground;

	DreamMugenAssignment* mAnim;
	DreamMugenAssignment* mSound;
	DreamMugenAssignment* mPosition;
	DreamMugenAssignment* mIsDarkening;
	DreamMugenAssignment* mPlayer2DefenseMultiplier;
	DreamMugenAssignment* mPowerToAdd;
	DreamMugenAssignment* mSetPlayerUnhittable;

} SuperPauseController;

static void parseSuperPauseController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) {
	SuperPauseController* e = allocMemoryOnMemoryStackOrMemory(sizeof(SuperPauseController));

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("time", tGroup, &e->mTime, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("endcmdbuftime", tGroup, &e->mBufferTimeForCommandsDuringPauseEnd, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("movetime", tGroup, &e->mMoveTime, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("pausebg", tGroup, &e->mDoesPauseBackground, "");

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("anim", tGroup, &e->mAnim, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("sound", tGroup, &e->mSound, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("pos", tGroup, &e->mPosition, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("darken", tGroup, &e->mIsDarkening, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("p2defmul", tGroup, &e->mPlayer2DefenseMultiplier, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("poweradd", tGroup, &e->mPowerToAdd, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("unhittable", tGroup, &e->mSetPlayerUnhittable, "");

	tController->mType = MUGEN_STATE_CONTROLLER_TYPE_SUPER_PAUSE;
	tController->mData = e;
}

static void unloadSuperPauseController(DreamMugenStateController* tController) {
	SuperPauseController* e = tController->mData;

	destroyDreamMugenAssignment(e->mTime);
	destroyDreamMugenAssignment(e->mBufferTimeForCommandsDuringPauseEnd);
	destroyDreamMugenAssignment(e->mMoveTime);
	destroyDreamMugenAssignment(e->mDoesPauseBackground);

	destroyDreamMugenAssignment(e->mAnim);
	destroyDreamMugenAssignment(e->mSound);
	destroyDreamMugenAssignment(e->mPosition);
	destroyDreamMugenAssignment(e->mIsDarkening);
	destroyDreamMugenAssignment(e->mPlayer2DefenseMultiplier);
	destroyDreamMugenAssignment(e->mPowerToAdd);
	destroyDreamMugenAssignment(e->mSetPlayerUnhittable);

	freeMemory(e);
}

typedef struct {
	DreamMugenAssignment* mType;
	char* mName;
	DreamMugenAssignment* mID;
	DreamMugenAssignment* mPosition;
	DreamMugenAssignment* mPositionType;

	DreamMugenAssignment* mFacing;
	DreamMugenAssignment* mStateNumber;
	DreamMugenAssignment* mCanControl;

	DreamMugenAssignment* mHasOwnPalette;
	DreamMugenAssignment* mSuperMoveTime;
	DreamMugenAssignment* mPauseMoveTime;

	DreamMugenAssignment* mSizeScaleX;
	DreamMugenAssignment* mSizeScaleY;
	DreamMugenAssignment* mSizeGroundBack;
	DreamMugenAssignment* mSizeGroundFront;
	DreamMugenAssignment* mSizeAirBack;
	DreamMugenAssignment* mSizeAirFront;

	DreamMugenAssignment* mSizeHeight;
	DreamMugenAssignment* mSizeProjectilesDoScale;
	DreamMugenAssignment* mSizeHeadPosition;
	DreamMugenAssignment* mSizeMiddlePosition;
	DreamMugenAssignment* mSizeShadowOffset;

} HelperController;

static void parseHelpeControllerName(HelperController* e, MugenDefScriptGroup* tGroup) {
	if (!string_map_contains(&tGroup->mElements, "name")) {
		e->mName = allocMemoryOnMemoryStackOrMemory(2);
		e->mName[0] = '\0';
		return;
	}
	MugenDefScriptGroupElement* elem = string_map_get(&tGroup->mElements, "name");
	if (!isMugenDefStringVariableAsElement(elem)) {
		e->mName = allocMemoryOnMemoryStackOrMemory(2);
		e->mName[0] = '\0';
		return;
	}

	char* text = getAllocatedMugenDefStringVariableAsElement(elem);
	e->mName = allocMemoryOnMemoryStackOrMemory(strlen(text)+2);
	strcpy(e->mName, text);
	freeMemory(text);
}

static void parseHelperController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) {
	HelperController* e = allocMemoryOnMemoryStackOrMemory(sizeof(HelperController));

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("helpertype", tGroup, &e->mType, "normal");
	parseHelpeControllerName(e, tGroup);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("id", tGroup, &e->mID, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("pos", tGroup, &e->mPosition, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("postype", tGroup, &e->mPositionType, "p1");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("facing", tGroup, &e->mFacing, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("stateno", tGroup, &e->mStateNumber, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("keyctrl", tGroup, &e->mCanControl, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("ownpal", tGroup, &e->mHasOwnPalette, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("supermovetime", tGroup, &e->mSuperMoveTime, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("pausemovetime", tGroup, &e->mPauseMoveTime, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("size.xscale", tGroup, &e->mSizeScaleX, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("size.yscale", tGroup, &e->mSizeScaleY, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("size.ground.back", tGroup, &e->mSizeGroundBack, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("size.ground.front", tGroup, &e->mSizeGroundFront, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("size.air.back", tGroup, &e->mSizeAirBack, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("size.air.front", tGroup, &e->mSizeAirFront, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("size.height", tGroup, &e->mSizeHeight, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("size.proj.doscale", tGroup, &e->mSizeProjectilesDoScale, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("size.head.pos", tGroup, &e->mSizeHeadPosition, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("size.mid.pos", tGroup, &e->mSizeMiddlePosition, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("size.shadowoffset", tGroup, &e->mSizeShadowOffset, "");

	tController->mType = MUGEN_STATE_CONTROLLER_TYPE_HELPER;
	tController->mData = e;
}

static void unloadHelperController(DreamMugenStateController* tController) {
	HelperController* e = tController->mData;

	destroyDreamMugenAssignment(e->mType);
	freeMemory(&e->mName);
	destroyDreamMugenAssignment(e->mID);
	destroyDreamMugenAssignment(e->mPosition);
	destroyDreamMugenAssignment(e->mPositionType);

	destroyDreamMugenAssignment(e->mFacing);
	destroyDreamMugenAssignment(e->mStateNumber);
	destroyDreamMugenAssignment(e->mCanControl);

	destroyDreamMugenAssignment(e->mHasOwnPalette);
	destroyDreamMugenAssignment(e->mSuperMoveTime);
	destroyDreamMugenAssignment(e->mPauseMoveTime);

	destroyDreamMugenAssignment(e->mSizeScaleX);
	destroyDreamMugenAssignment(e->mSizeScaleY);
	destroyDreamMugenAssignment(e->mSizeGroundBack);
	destroyDreamMugenAssignment(e->mSizeGroundFront);
	destroyDreamMugenAssignment(e->mSizeAirBack);
	destroyDreamMugenAssignment(e->mSizeAirFront);

	destroyDreamMugenAssignment(e->mSizeHeight);
	destroyDreamMugenAssignment(e->mSizeProjectilesDoScale);
	destroyDreamMugenAssignment(e->mSizeHeadPosition);
	destroyDreamMugenAssignment(e->mSizeMiddlePosition);
	destroyDreamMugenAssignment(e->mSizeShadowOffset);

	freeMemory(e);
}

typedef struct {
	DreamMugenAssignment* mValue;
	DreamMugenAssignment* mCanKill;
	DreamMugenAssignment* mIsAbsolute;

} LifeAddController;

static void parseLifeAddController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) {
	LifeAddController* e = allocMemoryOnMemoryStackOrMemory(sizeof(LifeAddController));

	assert(fetchDreamAssignmentFromGroupAndReturnWhetherItExists("value", tGroup, &e->mValue));
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("kill", tGroup, &e->mCanKill, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("absolute", tGroup, &e->mIsAbsolute, "");

	tController->mType = MUGEN_STATE_CONTROLLER_TYPE_ADD_LIFE;
	tController->mData = e;
}

static void unloadLifeAddController(DreamMugenStateController* tController) {
	LifeAddController* e = tController->mData;

	destroyDreamMugenAssignment(e->mValue);
	destroyDreamMugenAssignment(e->mCanKill);
	destroyDreamMugenAssignment(e->mIsAbsolute);

	freeMemory(e);
}

typedef struct {
	DreamMugenAssignment* mValue;
	DreamMugenAssignment* mID;
	DreamMugenAssignment* mCanKill;
	DreamMugenAssignment* mIsAbsolute;

} TargetLifeAddController;

static void parseTargetLifeAddController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) {
	TargetLifeAddController* e = allocMemoryOnMemoryStackOrMemory(sizeof(TargetLifeAddController));

	assert(fetchDreamAssignmentFromGroupAndReturnWhetherItExists("value", tGroup, &e->mValue));
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("id", tGroup, &e->mID, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("kill", tGroup, &e->mCanKill, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("absolute", tGroup, &e->mIsAbsolute, "");

	tController->mType = MUGEN_STATE_CONTROLLER_TYPE_ADD_TARGET_LIFE;
	tController->mData = e;
}

static void unloadTargetLifeAddController(DreamMugenStateController* tController) {
	TargetLifeAddController* e = tController->mData;

	destroyDreamMugenAssignment(e->mValue);
	destroyDreamMugenAssignment(e->mID);
	destroyDreamMugenAssignment(e->mCanKill);
	destroyDreamMugenAssignment(e->mIsAbsolute);

	freeMemory(e);
}


typedef struct {
	DreamMugenAssignment* mID;

	uint8_t mHasID;
} RemoveExplodController;

static void parseRemoveExplodController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) {
	RemoveExplodController* e = allocMemoryOnMemoryStackOrMemory(sizeof(RemoveExplodController));

	e->mHasID = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("id", tGroup, &e->mID);

	tController->mType = MUGEN_STATE_CONTROLLER_TYPE_REMOVE_EXPLOD;
	tController->mData = e;
}

static void unloadRemoveExplodController(DreamMugenStateController* tController) {
	RemoveExplodController* e = tController->mData;

	if (e->mHasID) {
		destroyDreamMugenAssignment(e->mID);
	}

	freeMemory(e);
}

typedef struct {
	DreamMugenAssignment* mValue;
	DreamMugenAssignment* mScale;

	uint8_t mHasValue;
	uint8_t mHasScale;
} AngleDrawController;

static void parseAngleDrawController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) {
	AngleDrawController* e = allocMemoryOnMemoryStackOrMemory(sizeof(AngleDrawController));

	e->mHasValue = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("value", tGroup, &e->mValue);
	e->mHasScale = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("scale", tGroup, &e->mScale);

	tController->mType = MUGEN_STATE_CONTROLLER_TYPE_DRAW_ANGLE;
	tController->mData = e;
}

static void unloadAngleDrawController(DreamMugenStateController* tController) {
	AngleDrawController* e = tController->mData;

	if (e->mHasValue) {
		destroyDreamMugenAssignment(e->mValue);
	}
	if (e->mHasScale) {
		destroyDreamMugenAssignment(e->mScale);
	}

	freeMemory(e);
}

typedef struct {
	DreamMugenAssignment* mTime;
	DreamMugenAssignment* mFacing;
	DreamMugenAssignment* mID;
	DreamMugenAssignment* mPosition;

} BindController;

static void parseBindController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup, DreamMugenStateControllerType tType) {
	BindController* e = allocMemoryOnMemoryStackOrMemory(sizeof(BindController));

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("time", tGroup, &e->mTime, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("facing", tGroup, &e->mFacing, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("pos", tGroup, &e->mPosition, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("id", tGroup, &e->mID, "");

	tController->mType = tType;
	tController->mData = e;
}

static void unloadBindController(DreamMugenStateController* tController) {
	BindController* e = tController->mData;

	destroyDreamMugenAssignment(e->mTime);
	destroyDreamMugenAssignment(e->mFacing);
	destroyDreamMugenAssignment(e->mID);
	destroyDreamMugenAssignment(e->mPosition);
	
	freeMemory(e);
}

typedef struct {
	DreamMugenAssignment* mValue;
	DreamMugenAssignment* mMoveCameraFlags;

} ScreenBoundController;

static void parseScreenBoundController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) {
	ScreenBoundController* e = allocMemoryOnMemoryStackOrMemory(sizeof(ScreenBoundController));

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("value", tGroup, &e->mValue, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("movecamera", tGroup, &e->mMoveCameraFlags, "");

	tController->mType = MUGEN_STATE_CONTROLLER_TYPE_SCREEN_BOUND;
	tController->mData = e;
}

static void unloadScreenBoundController(DreamMugenStateController* tController) {
	ScreenBoundController* e = tController->mData;

	destroyDreamMugenAssignment(e->mValue);
	destroyDreamMugenAssignment(e->mMoveCameraFlags);

	freeMemory(e);
}

typedef struct {
	DreamMugenAssignment* mValue;
	DreamMugenAssignment* mID;

} SetTargetFacingController;

static void parseSetTargetFacingController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) {
	SetTargetFacingController* e = allocMemoryOnMemoryStackOrMemory(sizeof(SetTargetFacingController));

	assert(fetchDreamAssignmentFromGroupAndReturnWhetherItExists("value", tGroup, &e->mValue));
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("id", tGroup, &e->mID, "");

	tController->mType = MUGEN_STATE_CONTROLLER_TYPE_SET_TARGET_FACING;
	tController->mData = e;
}

static void unloadSetTargetFacingController(DreamMugenStateController* tController) {
	SetTargetFacingController* e = tController->mData;

	destroyDreamMugenAssignment(e->mValue);
	destroyDreamMugenAssignment(e->mID);

	freeMemory(e);
}

typedef struct {
	MugenStringVector mAttributes;
} ReversalDefinitionController;

static void parseReversalDefinitionController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) {
	ReversalDefinitionController* e = allocMemoryOnMemoryStackOrMemory(sizeof(ReversalDefinitionController));

	uint8_t hasString;
	readMugenDefStringVector(&e->mAttributes, tGroup, "reversal.attr", &hasString);
	assert(hasString);

	tController->mType = MUGEN_STATE_CONTROLLER_TYPE_REVERSAL_DEFINITION;
	tController->mData = e;
}

static void unloadReversalDefinitionController(DreamMugenStateController* tController) {
	ReversalDefinitionController* e = tController->mData;

	unloadMugenDefStringVector(e->mAttributes);

	freeMemory(e);
}

typedef struct {
	HitDefinitionController mHitDef;

	DreamMugenAssignment* mID;
	DreamMugenAssignment* mAnimation;
	DreamMugenAssignment* mHitAnimation;
	DreamMugenAssignment* mRemoveAnimation;
	DreamMugenAssignment* mCancelAnimation;

	DreamMugenAssignment* mScale;
	DreamMugenAssignment* mIsRemovingProjectileAfterHit;
	DreamMugenAssignment* mRemoveTime;
	DreamMugenAssignment* mVelocity;
	DreamMugenAssignment* mRemoveVelocity;
	DreamMugenAssignment* mAcceleration;
	DreamMugenAssignment* mVelocityMultipliers;
	DreamMugenAssignment* mHitAmountBeforeVanishing;

	DreamMugenAssignment* mMissTime;
	DreamMugenAssignment* mPriority;
	DreamMugenAssignment* mSpriteSpriority;

	DreamMugenAssignment* mEdgeBound;
	DreamMugenAssignment* mStageBound;
	DreamMugenAssignment* mHeightBoundValues;
	DreamMugenAssignment* mOffset;
	DreamMugenAssignment* mPositionType;

	DreamMugenAssignment* mShadow;
	DreamMugenAssignment* mSuperMoveTime;
	DreamMugenAssignment* mPauseMoveTime;
	DreamMugenAssignment* mHasOwnPalette;

	DreamMugenAssignment* mRemapPalette;
	DreamMugenAssignment* mAfterImageTime;
	DreamMugenAssignment* mAfterImageLength;
	DreamMugenAssignment* mAfterImage;
} ProjectileController;

static void parseProjectileController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) {
	ProjectileController* e = allocMemoryOnMemoryStackOrMemory(sizeof(ProjectileController));
	readHitDefinitionFromGroup(&e->mHitDef, tGroup);

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("projid", tGroup, &e->mID, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("projanim", tGroup, &e->mAnimation, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("projhitanim", tGroup, &e->mHitAnimation, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("projremanim", tGroup, &e->mRemoveAnimation, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("projcancelanim", tGroup, &e->mCancelAnimation, "");

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("projscale", tGroup, &e->mScale, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("projremove", tGroup, &e->mIsRemovingProjectileAfterHit, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("projremovetime", tGroup, &e->mRemoveTime, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("velocity", tGroup, &e->mVelocity, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("remvelocity", tGroup, &e->mRemoveVelocity, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("accel", tGroup, &e->mAcceleration, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("velmul", tGroup, &e->mVelocityMultipliers, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("projhits", tGroup, &e->mHitAmountBeforeVanishing, "");

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("projmisstime", tGroup, &e->mMissTime, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("projpriority", tGroup, &e->mPriority, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("projsprpriority", tGroup, &e->mSpriteSpriority, "");

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("projedgebound", tGroup, &e->mEdgeBound, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("projstagebound", tGroup, &e->mStageBound, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("projheightbound", tGroup, &e->mHeightBoundValues, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("offset", tGroup, &e->mOffset, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("postype", tGroup, &e->mPositionType, "");

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("projshadow", tGroup, &e->mShadow, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("supermovetime", tGroup, &e->mSuperMoveTime, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("pausemovetime", tGroup, &e->mPauseMoveTime, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("ownpal", tGroup, &e->mHasOwnPalette, "");

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("remappal", tGroup, &e->mRemapPalette, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("afterimage.time", tGroup, &e->mAfterImageTime, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("afterimage.length", tGroup, &e->mAfterImageLength, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("afterimage", tGroup, &e->mAfterImage, "");

	tController->mType = MUGEN_STATE_CONTROLLER_TYPE_PROJECTILE;
	tController->mData = e;
}

static void unloadProjectileController(DreamMugenStateController* tController) {
	ProjectileController* e = tController->mData;
	unloadHitDefinitionData(&e->mHitDef);

	destroyDreamMugenAssignment(e->mID);
	destroyDreamMugenAssignment(e->mAnimation);
	destroyDreamMugenAssignment(e->mHitAnimation);
	destroyDreamMugenAssignment(e->mRemoveAnimation);
	destroyDreamMugenAssignment(e->mCancelAnimation);

	destroyDreamMugenAssignment(e->mScale);
	destroyDreamMugenAssignment(e->mIsRemovingProjectileAfterHit);
	destroyDreamMugenAssignment(e->mRemoveTime);
	destroyDreamMugenAssignment(e->mVelocity);
	destroyDreamMugenAssignment(e->mRemoveVelocity);
	destroyDreamMugenAssignment(e->mAcceleration);
	destroyDreamMugenAssignment(e->mVelocityMultipliers);
	destroyDreamMugenAssignment(e->mHitAmountBeforeVanishing);

	destroyDreamMugenAssignment(e->mMissTime);
	destroyDreamMugenAssignment(e->mPriority);
	destroyDreamMugenAssignment(e->mSpriteSpriority);

	destroyDreamMugenAssignment(e->mEdgeBound);
	destroyDreamMugenAssignment(e->mStageBound);
	destroyDreamMugenAssignment(e->mHeightBoundValues);
	destroyDreamMugenAssignment(e->mOffset);
	destroyDreamMugenAssignment(e->mPositionType);

	destroyDreamMugenAssignment(e->mShadow);
	destroyDreamMugenAssignment(e->mSuperMoveTime);
	destroyDreamMugenAssignment(e->mPauseMoveTime);
	destroyDreamMugenAssignment(e->mHasOwnPalette);

	destroyDreamMugenAssignment(e->mRemapPalette);
	destroyDreamMugenAssignment(e->mAfterImageTime);
	destroyDreamMugenAssignment(e->mAfterImageLength);
	destroyDreamMugenAssignment(e->mAfterImage);

	freeMemory(e);
}

typedef struct {
	DreamMugenAssignment* mTime;
	DreamMugenAssignment* mLength;

	DreamMugenAssignment* mPalColor;
	DreamMugenAssignment* mPalInvertAll;
	DreamMugenAssignment* mPalBright;
	DreamMugenAssignment* mPalContrast;
	DreamMugenAssignment* mPalPostBright;
	DreamMugenAssignment* mPalAdd;
	DreamMugenAssignment* mPalMul;
	DreamMugenAssignment* mTimeGap;
	DreamMugenAssignment* mFrameGap;
	DreamMugenAssignment* mTrans;

} AfterImageController;

static void parseAfterImageController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) {
	AfterImageController* e = allocMemoryOnMemoryStackOrMemory(sizeof(AfterImageController));
	
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("time", tGroup, &e->mTime, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("length", tGroup, &e->mLength, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("palcolor", tGroup, &e->mPalColor, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("palinvertall", tGroup, &e->mPalInvertAll, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("palbright", tGroup, &e->mPalBright, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("palcontrast", tGroup, &e->mPalContrast, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("palpostbright", tGroup, &e->mPalPostBright, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("paladd", tGroup, &e->mPalAdd, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("palmul", tGroup, &e->mPalMul, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("timegap", tGroup, &e->mTimeGap, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("trans", tGroup, &e->mTrans, "");

	tController->mType = MUGEN_STATE_CONTROLLER_TYPE_AFTER_IMAGE;
	tController->mData = e;
}

static void unloadAfterImageController(DreamMugenStateController* tController) {
	AfterImageController* e = tController->mData;

	destroyDreamMugenAssignment(e->mTime);
	destroyDreamMugenAssignment(e->mLength);
	destroyDreamMugenAssignment(e->mPalColor);
	destroyDreamMugenAssignment(e->mPalInvertAll);
	destroyDreamMugenAssignment(e->mPalBright);
	destroyDreamMugenAssignment(e->mPalContrast);
	destroyDreamMugenAssignment(e->mPalPostBright);
	destroyDreamMugenAssignment(e->mPalAdd);
	destroyDreamMugenAssignment(e->mPalMul);
	destroyDreamMugenAssignment(e->mTimeGap);
	destroyDreamMugenAssignment(e->mTrans);

	freeMemory(e);
}


typedef struct {
	DreamMugenAssignment* mTime;
} AfterImageTimeController;

static void parseAfterImageTimeController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) {
	AfterImageTimeController* e = allocMemoryOnMemoryStackOrMemory(sizeof(AfterImageTimeController));

	if (isMugenDefNumberVariableAsGroup(tGroup, "time")) {
		fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("time", tGroup, &e->mTime, "");
	}
	else {
		fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("value", tGroup, &e->mTime, "");
	}

	tController->mType = MUGEN_STATE_CONTROLLER_TYPE_AFTER_IMAGE_TIME;
	tController->mData = e;
}

static void unloadAfterImageTimeController(DreamMugenStateController* tController) {
	AfterImageTimeController* e = tController->mData;
	destroyDreamMugenAssignment(e->mTime);
	freeMemory(e);
}


typedef struct {
	DreamMugenAssignment* mTime;
	DreamMugenAssignment* mAdd;
	DreamMugenAssignment* mMul;
	DreamMugenAssignment* mSinAdd;
	DreamMugenAssignment* mInvertAll;
	DreamMugenAssignment* mColor;

} PalFXController;

static void parsePalFXController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup, DreamMugenStateControllerType tType) {
	PalFXController* e = allocMemoryOnMemoryStackOrMemory(sizeof(PalFXController));

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("time", tGroup, &e->mTime, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("add", tGroup, &e->mAdd, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("mul", tGroup, &e->mMul, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("sinadd", tGroup, &e->mSinAdd, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("invertall", tGroup, &e->mInvertAll, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("color", tGroup, &e->mColor, "");

	tController->mType = tType;
	tController->mData = e;
}

static void unloadPalFXController(DreamMugenStateController* tController) {
	PalFXController* e = tController->mData;

	destroyDreamMugenAssignment(e->mTime);
	destroyDreamMugenAssignment(e->mAdd);
	destroyDreamMugenAssignment(e->mMul);
	destroyDreamMugenAssignment(e->mSinAdd);
	destroyDreamMugenAssignment(e->mInvertAll);
	destroyDreamMugenAssignment(e->mColor);

	freeMemory(e);
}


typedef struct {
	DreamMugenAssignment* mText;
	DreamMugenAssignment* mParams;
} ClipboardController;

static void parseClipboardController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup, DreamMugenStateControllerType tType) {
	ClipboardController* e = allocMemoryOnMemoryStackOrMemory(sizeof(ClipboardController));

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("text", tGroup, &e->mText, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("params", tGroup, &e->mParams, "");

	tController->mType = tType;
	tController->mData = e;
}

static void unloadClipboardController(DreamMugenStateController* tController) {
	ClipboardController* e = tController->mData;

	destroyDreamMugenAssignment(e->mText);
	destroyDreamMugenAssignment(e->mParams);

	freeMemory(e);
}

typedef struct {
	DreamMugenAssignment* mRecursive;
	DreamMugenAssignment* mRemoveExplods;

} DestroySelfController;

static void parseDestroySelfController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) {
	DestroySelfController* e = allocMemoryOnMemoryStackOrMemory(sizeof(DestroySelfController));

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("recursive", tGroup, &e->mRecursive, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("removeexplods", tGroup, &e->mRemoveExplods, "");

	tController->mType = MUGEN_STATE_CONTROLLER_TYPE_DESTROY_SELF;
	tController->mData = e;
}

static void unloadDestroySelfController(DreamMugenStateController* tController) {
	DestroySelfController* e = tController->mData;

	destroyDreamMugenAssignment(e->mRecursive);
	destroyDreamMugenAssignment(e->mRemoveExplods);

	freeMemory(e);
}

typedef struct {
	DreamMugenAssignment* mValue;
	DreamMugenAssignment* mTime;
	DreamMugenAssignment* mUnder;

} EnvironmentColorController;

static void parseEnvColorController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) {
	EnvironmentColorController* e = allocMemoryOnMemoryStackOrMemory(sizeof(EnvironmentColorController));

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("value", tGroup, &e->mValue, "255 , 255 , 255");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("time", tGroup, &e->mTime, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("under", tGroup, &e->mUnder, "");

	tController->mType = MUGEN_STATE_CONTROLLER_TYPE_ENVIRONMENT_COLOR;
	tController->mData = e;
	
}

static void unloadEnvColorController(DreamMugenStateController* tController) {
	EnvironmentColorController* e = tController->mData;

	destroyDreamMugenAssignment(e->mValue);
	destroyDreamMugenAssignment(e->mTime);
	destroyDreamMugenAssignment(e->mUnder);

	freeMemory(e);

}

typedef struct {
	DreamMugenAssignment* mID;
	DreamMugenAssignment* mTime;

} ExplodBindTimeController;

static void parseExplodBindTimeController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) {
	ExplodBindTimeController* e = allocMemoryOnMemoryStackOrMemory(sizeof(ExplodBindTimeController));

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("id", tGroup, &e->mID, "");

	if (isMugenDefNumberVariableAsGroup(tGroup, "time")) {
		fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("time", tGroup, &e->mTime, "");
	}
	else {
		fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("value", tGroup, &e->mTime, "");
	}

	tController->mType = MUGEN_STATE_CONTROLLER_TYPE_EXPLOD_BIND_TIME;
	tController->mData = e;
}

static void unloadExplodBindTimeController(DreamMugenStateController* tController) {
	ExplodBindTimeController* e = tController->mData;

	destroyDreamMugenAssignment(e->mID);
	destroyDreamMugenAssignment(e->mTime);

	freeMemory(e);
}

typedef struct {
	DreamMugenAssignment* mValue;
	DreamMugenAssignment* mIsUnderPlayer;
	DreamMugenAssignment* mPosOffset;
	DreamMugenAssignment* mRandomOffset;

} GameMakeAnimController;

static void parseGameMakeAnimController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) {
	GameMakeAnimController* e = allocMemoryOnMemoryStackOrMemory(sizeof(GameMakeAnimController));

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("value", tGroup, &e->mValue, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("under", tGroup, &e->mIsUnderPlayer, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("pos", tGroup, &e->mPosOffset, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("random", tGroup, &e->mRandomOffset, "");

	tController->mType = MUGEN_STATE_CONTROLLER_TYPE_MAKE_GAME_ANIMATION;
	tController->mData = e;
}

static void unloadGameMakeAnimController(DreamMugenStateController* tController) {
	GameMakeAnimController* e = tController->mData;

	destroyDreamMugenAssignment(e->mValue);
	destroyDreamMugenAssignment(e->mIsUnderPlayer);
	destroyDreamMugenAssignment(e->mPosOffset);
	destroyDreamMugenAssignment(e->mRandomOffset);

	freeMemory(e);
}

typedef struct {
	DreamMugenAssignment* mAttributeString;
	DreamMugenAssignment* mStateNo;

	DreamMugenAssignment* mSlot;
	DreamMugenAssignment* mTime;
	DreamMugenAssignment* mForceAir;
} HitOverrideController;

static void parseHitOverrideController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) {
	HitOverrideController* e = allocMemoryOnMemoryStackOrMemory(sizeof(HitOverrideController));

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("attr", tGroup, &e->mAttributeString, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("stateno", tGroup, &e->mStateNo, "");

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("slot", tGroup, &e->mSlot, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("time", tGroup, &e->mTime, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("forceair", tGroup, &e->mForceAir, "");

	tController->mType = MUGEN_STATE_CONTROLLER_TYPE_HIT_OVERRIDE;
	tController->mData = e;
}

static void unloadHitOverrideController(DreamMugenStateController* tController) {
	HitOverrideController* e = tController->mData;

	destroyDreamMugenAssignment(e->mAttributeString);
	destroyDreamMugenAssignment(e->mStateNo);

	destroyDreamMugenAssignment(e->mSlot);
	destroyDreamMugenAssignment(e->mTime);
	destroyDreamMugenAssignment(e->mForceAir);

	freeMemory(e);
}

typedef struct {
	DreamMugenAssignment* mTime;

	DreamMugenAssignment* mEndCommandBufferTime;
	DreamMugenAssignment* mMoveTime;
	DreamMugenAssignment* mPauseBG;
} PauseController;

static void parsePauseController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) {
	PauseController* e = allocMemoryOnMemoryStackOrMemory(sizeof(PauseController));

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("time", tGroup, &e->mTime, "");

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("endcmdbuftime", tGroup, &e->mEndCommandBufferTime, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("movetime", tGroup, &e->mMoveTime, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("pausebg", tGroup, &e->mPauseBG, "");

	tController->mType = MUGEN_STATE_CONTROLLER_TYPE_PAUSE;
	tController->mData = e;
}

static void unloadPauseController(DreamMugenStateController* tController) {
	PauseController* e = tController->mData;

	destroyDreamMugenAssignment(e->mTime);

	destroyDreamMugenAssignment(e->mEndCommandBufferTime);
	destroyDreamMugenAssignment(e->mMoveTime);
	destroyDreamMugenAssignment(e->mPauseBG);

	freeMemory(e);
}

typedef struct {
	DreamMugenAssignment* mSource;
	DreamMugenAssignment* mDestination;
} RemapPaletteController;

static void parseRemapPaletteController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) {
	RemapPaletteController* e = allocMemoryOnMemoryStackOrMemory(sizeof(RemapPaletteController));

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("source", tGroup, &e->mSource, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("dest", tGroup, &e->mDestination, "");

	tController->mType = MUGEN_STATE_CONTROLLER_TYPE_REMAP_PALETTE;
	tController->mData = e;
}

static void unloadRemapPaletteController(DreamMugenStateController* tController) {
	RemapPaletteController* e = tController->mData;

	destroyDreamMugenAssignment(e->mSource);
	destroyDreamMugenAssignment(e->mDestination);

	freeMemory(e);
}

typedef struct {
	DreamMugenAssignment* mChannel;
	DreamMugenAssignment* mPan;
} SoundPanController;

static void parseSoundPanController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) {
	SoundPanController* e = allocMemoryOnMemoryStackOrMemory(sizeof(SoundPanController));

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("channel", tGroup, &e->mChannel, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("pan", tGroup, &e->mPan, "");

	tController->mType = MUGEN_STATE_CONTROLLER_TYPE_PAN_SOUND;
	tController->mData = e;
}

static void unloadSoundPanController(DreamMugenStateController* tController) {
	SoundPanController* e = tController->mData;

	destroyDreamMugenAssignment(e->mChannel);
	destroyDreamMugenAssignment(e->mPan);

	freeMemory(e);
}

typedef struct {
	DreamMugenAssignment* mChannel;
} SoundStopController;

static void parseStopSoundController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) {
	SoundStopController* e = allocMemoryOnMemoryStackOrMemory(sizeof(SoundStopController));

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("channel", tGroup, &e->mChannel, "");

	tController->mType = MUGEN_STATE_CONTROLLER_TYPE_STOP_SOUND;
	tController->mData = e;
}

static void unloadStopSoundController(DreamMugenStateController* tController) {
	SoundStopController* e = tController->mData;

	destroyDreamMugenAssignment(e->mChannel);

	freeMemory(e);
}

typedef struct {
	DreamMugenAssignment* mExcludeID;
	DreamMugenAssignment* mKeepOne;

} TargetDropController;

static void parseTargetDropController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) {
	TargetDropController* e = allocMemoryOnMemoryStackOrMemory(sizeof(TargetDropController));

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("excludeid", tGroup, &e->mExcludeID, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("keepone", tGroup, &e->mKeepOne, "");

	tController->mType = MUGEN_STATE_CONTROLLER_TYPE_DROP_TARGET;
	tController->mData = e;
}

static void unloadTargetDropController(DreamMugenStateController* tController) {
	TargetDropController* e = tController->mData;

	destroyDreamMugenAssignment(e->mExcludeID);
	destroyDreamMugenAssignment(e->mKeepOne);

	freeMemory(e);
}


typedef struct {
	DreamMugenAssignment* mValue;
	DreamMugenAssignment* mID;
} TargetPowerAddController;

static void parseTargetPowerAddController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) {
	TargetPowerAddController* e = allocMemoryOnMemoryStackOrMemory(sizeof(TargetPowerAddController));

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("value", tGroup, &e->mValue, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("id", tGroup, &e->mID, "");

	tController->mType = MUGEN_STATE_CONTROLLER_TYPE_ADD_TARGET_POWER;
	tController->mData = e;
}

static void unloadTargetPowerAddController(DreamMugenStateController* tController) {
	TargetPowerAddController* e = tController->mData;

	destroyDreamMugenAssignment(e->mValue);
	destroyDreamMugenAssignment(e->mID);

	freeMemory(e);
}

typedef struct {
	DreamMugenAssignment* mID;
	DreamMugenAssignment* x;
	DreamMugenAssignment* y;

	uint8_t mIsSettingX;
	uint8_t mIsSettingY;
} Target2DPhysicsController;

static void parseTarget2DPhysicsController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup, DreamMugenStateControllerType tType) {
	Target2DPhysicsController* e = allocMemoryOnMemoryStackOrMemory(sizeof(Target2DPhysicsController));

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("id", tGroup, &e->mID, "");
	e->mIsSettingX = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("x", tGroup, &e->x);
	e->mIsSettingY = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("y", tGroup, &e->y);

	tController->mType = tType;
	tController->mData = e;
}

static void unloadTarget2DPhysicsController(DreamMugenStateController* tController) {
	Target2DPhysicsController* e = tController->mData;

	destroyDreamMugenAssignment(e->mID);
	if (e->mIsSettingX) {
		destroyDreamMugenAssignment(e->x);
	}
	if (e->mIsSettingY) {
		destroyDreamMugenAssignment(e->y);
	}

	freeMemory(e);
}

typedef struct {
	DreamMugenAssignment* mTransparency;
	DreamMugenAssignment* mAlpha;
} TransparencyController;

static void parseTransparencyController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) {
	TransparencyController* e = allocMemoryOnMemoryStackOrMemory(sizeof(TransparencyController));

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("trans", tGroup, &e->mTransparency, "default");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("alpha", tGroup, &e->mAlpha, "");

	tController->mType = MUGEN_STATE_CONTROLLER_TYPE_TRANSPARENCY;
	tController->mData = e;
}

static void unloadTransparencyController(DreamMugenStateController* tController) {
	TransparencyController* e = tController->mData;

	destroyDreamMugenAssignment(e->mTransparency);
	destroyDreamMugenAssignment(e->mAlpha);

	freeMemory(e);
}

typedef struct {
	DreamMugenAssignment* mValue;
	DreamMugenAssignment* mRange;
} VarRandomController;

static void parseVarRandomController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) {
	VarRandomController* e = allocMemoryOnMemoryStackOrMemory(sizeof(VarRandomController));

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("v", tGroup, &e->mValue, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("range", tGroup, &e->mRange, "0 , 1000");

	tController->mType = MUGEN_STATE_CONTROLLER_TYPE_SET_VARIABLE_RANDOM;
	tController->mData = e;
}

static void unloadVarRandomController(DreamMugenStateController* tController) {
	VarRandomController* e = tController->mData;

	destroyDreamMugenAssignment(e->mValue);
	destroyDreamMugenAssignment(e->mRange);

	freeMemory(e);
}

typedef struct {
	DreamMugenAssignment* mValue;
} VictoryQuoteController;

static void parseVictoryQuoteController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) {
	VictoryQuoteController* e = allocMemoryOnMemoryStackOrMemory(sizeof(VictoryQuoteController));

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("value", tGroup, &e->mValue, "");

	tController->mType = MUGEN_STATE_CONTROLLER_TYPE_VICTORY_QUOTE;
	tController->mData = e;
}

static void unloadVictoryQuoteController(DreamMugenStateController* tController) {
	VictoryQuoteController* e = tController->mData;
	destroyDreamMugenAssignment(e->mValue);
	freeMemory(e);
}

static void parseStateControllerType(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) {
	assert(string_map_contains(&tGroup->mElements, "type"));
	MugenDefScriptGroupElement* e = string_map_get(&tGroup->mElements, "type");
	tController->mData = NULL;

	char* type = getAllocatedMugenDefStringVariableAsElement(e);
	turnStringLowercase(type);

	if(!string_map_contains(&gVariableHandler.mStateControllerParsers, type)) {
		logWarningFormat("Unable to determine state controller type %s. Defaulting to null.", type);
		freeMemory(type);
		type = allocMemory(10);
		strcpy(type, "null");
	}

	StateControllerParseFunction func = string_map_get(&gVariableHandler.mStateControllerParsers, type);
	func(tController, tGroup);

	freeMemory(type);
}

static void parseStateControllerPersistence(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) {
	tController->mPersistence = getMugenDefIntegerOrDefaultAsGroup(tGroup, "persistent", 1);
	tController->mAccessAmount = 0;
}

int gDebugStateControllerAmount;

DreamMugenStateController * parseDreamMugenStateControllerFromGroup(MugenDefScriptGroup* tGroup)
{
	DreamMugenStateController* ret = allocMemoryOnMemoryStackOrMemory(sizeof(DreamMugenStateController));
	gDebugStateControllerAmount++;
	parseStateControllerType(ret, tGroup);
	parseStateControllerTriggers(ret, tGroup);
	parseStateControllerPersistence(ret, tGroup);

	return ret;
}

static void unloadStateControllerType(DreamMugenStateController* tController) {
	if (!int_map_contains(&gVariableHandler.mStateControllerUnloaders, tController->mType)) {
		logWarningFormat("Unable to determine state controller type %d. Defaulting to null.", tController->mType);
		tController->mType = MUGEN_STATE_CONTROLLER_TYPE_NULL;
	}

	StateControllerUnloadFunction func = int_map_get(&gVariableHandler.mStateControllerUnloaders, tController->mType);
	func(tController);
}

void unloadDreamMugenStateController(DreamMugenStateController * tController)
{
	destroyDreamMugenAssignment(tController->mTrigger.mAssignment);
	unloadStateControllerType(tController);
}

static int handleVelocitySetting(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	Set2DPhysicsController* e = tController->mData;

	if (e->mIsSettingX) {
		double x = evaluateDreamAssignmentAndReturnAsFloat(&e->x, tPlayer);
		setPlayerVelocityX(tPlayer, x, getPlayerCoordinateP(tPlayer));
	}

	if (e->mIsSettingY) {
		double y = evaluateDreamAssignmentAndReturnAsFloat(&e->y, tPlayer);
		setPlayerVelocityY(tPlayer, y, getPlayerCoordinateP(tPlayer));
	}

	return 0;
}

static int handleVelocityMultiplication(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	Set2DPhysicsController* e = tController->mData;

	if (e->mIsSettingX) {
		double x = evaluateDreamAssignmentAndReturnAsFloat(&e->x, tPlayer);
		multiplyPlayerVelocityX(tPlayer, x, getPlayerCoordinateP(tPlayer));
	}

	if (e->mIsSettingY) {
		double y = evaluateDreamAssignmentAndReturnAsFloat(&e->y, tPlayer);
		multiplyPlayerVelocityY(tPlayer, y, getPlayerCoordinateP(tPlayer));
	}

	return 0;
}

static int handleVelocityAddition(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	Set2DPhysicsController* e = tController->mData;

	if (e->mIsSettingX) {
		double x = evaluateDreamAssignmentAndReturnAsFloat(&e->x, tPlayer);
		addPlayerVelocityX(tPlayer, x, getPlayerCoordinateP(tPlayer));
	}

	if (e->mIsSettingY) {
		double y = evaluateDreamAssignmentAndReturnAsFloat(&e->y, tPlayer);
		addPlayerVelocityY(tPlayer, y, getPlayerCoordinateP(tPlayer));
	}

	return 0;
}

static int handlePositionSetting(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	Set2DPhysicsController* e = tController->mData;

	if (e->mIsSettingX) {
		double x = evaluateDreamAssignmentAndReturnAsFloat(&e->x, tPlayer);
		setPlayerPositionBasedOnScreenCenterX(tPlayer, x, getPlayerCoordinateP(tPlayer));
	}

	if (e->mIsSettingY) {
		double y = evaluateDreamAssignmentAndReturnAsFloat(&e->y, tPlayer);
		setPlayerPositionY(tPlayer, y, getPlayerCoordinateP(tPlayer));
	}

	return 0;
}

static int handlePositionAdding(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	Set2DPhysicsController* e = tController->mData;

	if (e->mIsSettingX) {
		double x = evaluateDreamAssignmentAndReturnAsFloat(&e->x, tPlayer);
		addPlayerPositionX(tPlayer, x, getPlayerCoordinateP(tPlayer));
	}

	if (e->mIsSettingY) {
		double y = evaluateDreamAssignmentAndReturnAsFloat(&e->y, tPlayer);
		addPlayerPositionY(tPlayer, y, getPlayerCoordinateP(tPlayer));
	}

	return 0;
}


static int handleStateChange(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	ChangeStateController* e = tController->mData;

	int state = evaluateDreamAssignmentAndReturnAsInteger(&e->mState, tPlayer);
	changePlayerStateBeforeImmediatelyEvaluatingIt(tPlayer, state);

	if (e->mIsChangingControl) {
		int control = evaluateDreamAssignmentAndReturnAsInteger(&e->mControl, tPlayer);
		setPlayerControl(tPlayer, control);
	}

	return 1;
}

static int handleSelfStateChange(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	ChangeStateController* e = tController->mData;

	int state = evaluateDreamAssignmentAndReturnAsInteger(&e->mState, tPlayer);

	if (e->mIsChangingControl) {
		int control = evaluateDreamAssignmentAndReturnAsInteger(&e->mControl, tPlayer);
		setPlayerControl(tPlayer, control);
	}

	changePlayerStateToSelfBeforeImmediatelyEvaluatingIt(tPlayer, state);

	return 1;
}

static void getSingleIntegerValueOrDefault(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* tDst, int tDefault) {
	char* flag = evaluateDreamAssignmentAndReturnAsAllocatedString(tAssignment, tPlayer);

	if (!strcmp("", flag)) *tDst = tDefault;
	else *tDst = atoi(flag);

	freeMemory(flag);
}

static void getSingleFloatValueOrDefault(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, double* tDst, double tDefault) {
	char* flag = evaluateDreamAssignmentAndReturnAsAllocatedString(tAssignment, tPlayer);

	if (!strcmp("", flag)) *tDst = tDefault;
	else *tDst = atof(flag);

	freeMemory(flag);
}

static int handleTargetStateChange(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	TargetChangeStateController* e = tController->mData;

	int id;
	getSingleIntegerValueOrDefault(&e->mID, tPlayer, &id, -1);

	if (e->mIsChangingControl) {
		int control = evaluateDreamAssignmentAndReturnAsInteger(&e->mControl, tPlayer);
		setPlayerTargetControl(tPlayer, id, control);
	}

	int state = evaluateDreamAssignmentAndReturnAsInteger(&e->mState, tPlayer);
	changePlayerTargetState(tPlayer, id, state);

	return 0;
}

static void handleSoundEffectValue(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer) {
	char* flag = evaluateDreamAssignmentAndReturnAsAllocatedString(tAssignment, tPlayer);
	turnStringLowercase(flag);

	MugenSounds* soundFile;
	int group;
	int item;

	char firstW[20], comma[10];
	int items = sscanf(flag, "%s", firstW);
	if (items != 1) {
		logWarningFormat("Unable to parse flag: %s. Abort.");
		freeMemory(flag);
		return;
	}

	if (!strcmp("isinotherfilef", firstW)) {
		soundFile = getDreamCommonSounds();
		int items = sscanf(flag, "%s %d %s %d", firstW, &group, comma, &item);
		if (items != 4) {
			logWarningFormat("Unable to parse flag: %s. Abort.");
			freeMemory(flag);
			return;
		}
	} else if (!strcmp("isinotherfiles", firstW)) {
		soundFile = getPlayerSounds(tPlayer);
		int items = sscanf(flag, "%s %d %s %d", firstW, &group, comma, &item);
		if (items != 4) {
			logWarningFormat("Unable to parse flag: %s. Abort.");
			freeMemory(flag);
			return;
		}
	}
	else {
		soundFile = getPlayerSounds(tPlayer);
		int items = sscanf(flag, "%d %s %d", &group, comma, &item);
		if (items != 3) {
			logWarningFormat("Unable to parse flag: %s. Abort.");
			freeMemory(flag);
			return;
		}
	}


	tryPlayMugenSound(soundFile, group, item);

	freeMemory(flag);
}

static int handlePlaySound(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	PlaySoundController* e = tController->mData;

	handleSoundEffectValue(&e->mValue, tPlayer); // TODO: other parameters

	return 0;
}

static void getHitDefinitionAttributeValuesFromString(char* attr, DreamMugenStateType* tStateType, MugenAttackClass* tAttackClass, MugenAttackType* tAttackType) {
	char arg1[20], comma[10], arg2[20];
	sscanf(attr, "%s %s %s", arg1, comma, arg2);
	assert(strcmp("", arg1));
	assert(strlen(arg2) == 2);
	assert(!strcmp(",", comma));

	turnStringLowercase(arg1);
	if (strchr(arg1, 's') != NULL) *tStateType = MUGEN_STATE_TYPE_STANDING;
	else if (strchr(arg1, 'c') != NULL) *tStateType = MUGEN_STATE_TYPE_CROUCHING;
	else if (strchr(arg1, 'a') != NULL) *tStateType = MUGEN_STATE_TYPE_AIR;
	else {
		logWarning("Unable to parse hitdef attr 1.");
		logWarningString(arg1);
		*tStateType = MUGEN_STATE_TYPE_UNCHANGED;
	}

	turnStringLowercase(arg2);
	if (arg2[0] == 'n') *tAttackClass = MUGEN_ATTACK_CLASS_NORMAL;
	else if (arg2[0] == 's') *tAttackClass = MUGEN_ATTACK_CLASS_SPECIAL;
	else if (arg2[0] == 'h') *tAttackClass = MUGEN_ATTACK_CLASS_HYPER;
	else {
		logWarning("Unable to parse hitdef attr 2.");
		logWarningString(arg2);
		*tAttackClass = MUGEN_ATTACK_CLASS_NORMAL;
	}

	if (arg2[1] == 'a') *tAttackType = MUGEN_ATTACK_TYPE_ATTACK;
	else if (arg2[1] == 't') *tAttackType = MUGEN_ATTACK_TYPE_THROW;
	else if (arg2[1] == 'p')  *tAttackType = MUGEN_ATTACK_TYPE_PROJECTILE;
	else {
		logWarning("Unable to parse hitdef attr 2.");
		logWarningString(arg2);
		*tAttackType = MUGEN_ATTACK_TYPE_ATTACK;
	}

}

static void handleHitDefinitionAttribute(HitDefinitionController* e, DreamPlayer* tPlayer) {
	char* attr = evaluateDreamAssignmentAndReturnAsAllocatedString(&e->mAttribute, tPlayer);

	DreamMugenStateType stateType;
	MugenAttackClass attackClass;
	MugenAttackType attackType;

	getHitDefinitionAttributeValuesFromString(attr, &stateType, &attackClass, &attackType);

	setHitDataType(tPlayer, stateType);
	setHitDataAttackClass(tPlayer, attackClass);
	setHitDataAttackType(tPlayer, attackType);

	freeMemory(attr);
}

static void handleHitDefinitionSingleHitFlag(DreamMugenAssignment** tFlagAssignment, DreamPlayer* tPlayer, void(tSetFunc)(DreamPlayer* tPlayer, char*)) {
	char* flag = evaluateDreamAssignmentAndReturnAsAllocatedString(tFlagAssignment, tPlayer);
	tSetFunc(tPlayer, flag);
	freeMemory(flag);
}

static void handleHitDefinitionAffectTeam(DreamMugenAssignment** tAffectAssignment, DreamPlayer* tPlayer) {
	char* flag = evaluateDreamAssignmentAndReturnAsAllocatedString(tAffectAssignment, tPlayer);
	assert(strlen(flag) == 1);
	turnStringLowercase(flag);

	if (*flag == 'b') setHitDataAffectTeam(tPlayer, MUGEN_AFFECT_TEAM_BOTH);
	else if (*flag == 'e') setHitDataAffectTeam(tPlayer, MUGEN_AFFECT_TEAM_ENEMY);
	else if (*flag == 'f') setHitDataAffectTeam(tPlayer, MUGEN_AFFECT_TEAM_FRIENDLY);
	else {
		logWarningFormat("Unable to parse hitdef affectteam %s. Set to enemy.", flag);
		setHitDataAffectTeam(tPlayer, MUGEN_AFFECT_TEAM_ENEMY);
	}

	freeMemory(flag);
}

static void handleHitDefinitionSingleAnimationType(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, void(tFunc)(DreamPlayer*, MugenHitAnimationType), MugenHitAnimationType tDefault) {
	char* flag = evaluateDreamAssignmentAndReturnAsAllocatedString(tAssignment, tPlayer);
	turnStringLowercase(flag);

	if (!strcmp("", flag)) tFunc(tPlayer, tDefault);
	else if (!strcmp("light", flag)) tFunc(tPlayer, MUGEN_HIT_ANIMATION_TYPE_LIGHT);
	else if (!strcmp("medium", flag) || !strcmp("med", flag)) tFunc(tPlayer, MUGEN_HIT_ANIMATION_TYPE_MEDIUM);
	else if (!strcmp("hard", flag)) tFunc(tPlayer, MUGEN_HIT_ANIMATION_TYPE_HARD);
	else if (!strcmp("heavy", flag)) tFunc(tPlayer, MUGEN_HIT_ANIMATION_TYPE_HEAVY);
	else if (!strcmp("back", flag)) tFunc(tPlayer, MUGEN_HIT_ANIMATION_TYPE_BACK);
	else if (!strcmp("up", flag)) tFunc(tPlayer, MUGEN_HIT_ANIMATION_TYPE_UP);
	else if (!strcmp("diagup", flag)) tFunc(tPlayer, MUGEN_HIT_ANIMATION_TYPE_DIAGONAL_UP);
	else {
		logWarningFormat("Unable to parse hitdef animation type %s. Setting to default", flag);
		tFunc(tPlayer, tDefault);
	}

	freeMemory(flag);
}

static void handleHitDefinitionPriority(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer) {
	char* flag = evaluateDreamAssignmentAndReturnAsAllocatedString(tAssignment, tPlayer);
	turnStringLowercase(flag);

	char prioString[20], comma[10], typeString[20];
	int items = sscanf(flag, "%s %s %s", prioString, comma, typeString);

	int prio;
	if (items < 1 || !strcmp("", prioString)) prio = 4;
	else prio = atoi(prioString);

	MugenHitPriorityType type = MUGEN_HIT_PRIORITY_HIT;
	if (items < 3 || !strcmp("", typeString)) type = MUGEN_HIT_PRIORITY_HIT;
	else if (!strcmp("hit", typeString)) type = MUGEN_HIT_PRIORITY_HIT;
	else if (!strcmp("miss", typeString)) type = MUGEN_HIT_PRIORITY_MISS;
	else if (!strcmp("dodge", typeString)) type = MUGEN_HIT_PRIORITY_DODGE;
	else {
		logWarningFormat("Unable to parse hitdef priority type %s with string %s. Defaulting to hit.", flag, typeString);
		type = MUGEN_HIT_PRIORITY_HIT;
	}

	setHitDataPriority(tPlayer, prio, type);

	freeMemory(flag);
}

static void getTwoIntegerValuesWithDefaultValues(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* v1, int* v2, int tDefault1, int tDefault2) {
	char* flag = evaluateDreamAssignmentAndReturnAsAllocatedString(tAssignment, tPlayer);

	char string1[20], comma[10], string2[20];
	int items = sscanf(flag, "%s %s %s", string1, comma, string2);

	if (items < 1 || !strcmp("", string1)) *v1 = tDefault1;
	else *v1 = atoi(string1);
	if (items < 3 || !strcmp("", string2)) *v2 = tDefault2;
	else *v2 = atoi(string2);

	freeMemory(flag);
}

static void getTwoFloatValuesWithDefaultValues(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, double* v1, double* v2, double tDefault1, double tDefault2) {
	char* flag = evaluateDreamAssignmentAndReturnAsAllocatedString(tAssignment, tPlayer);

	char string1[20], comma[10], string2[20];
	int items = sscanf(flag, "%s %s %s", string1, comma, string2);

	if (items < 1 || !strcmp("", string1)) *v1 = tDefault1;
	else *v1 = atof(string1);
	if (items < 3 || !strcmp("", string2)) *v2 = tDefault2;
	else *v2 = atof(string2);

	freeMemory(flag);
}

static void handleHitDefinitionDamage(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer) {
	int damage, guardDamage;

	getTwoIntegerValuesWithDefaultValues(tAssignment, tPlayer, &damage, &guardDamage, 0, 0);
	setHitDataDamage(tPlayer, damage, guardDamage);
}

static void handleHitDefinitionSinglePauseTime(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, void(tFunc)(DreamPlayer*, int, int), int tDefault1, int tDefault2) {
	int p1PauseTime, p2PauseTime;

	getTwoIntegerValuesWithDefaultValues(tAssignment, tPlayer, &p1PauseTime, &p2PauseTime, tDefault1, tDefault2);
	tFunc(tPlayer, p1PauseTime, p2PauseTime);
}

static void handleHitDefinitionSparkNumberSingle(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, void(tFunc)(DreamPlayer*, int, int), int tDefaultIsInFile, int tDefaultNumber) {
	char* flag = evaluateDreamAssignmentAndReturnAsAllocatedString(tAssignment, tPlayer);
	turnStringLowercase(flag);

	char firstW[200];
	int which = -1;

	int items = sscanf(flag, "%s %d", firstW, &which);

	int isInPlayerFile;
	int number;
	if (items < 1) {
		isInPlayerFile = tDefaultIsInFile;
		number = tDefaultNumber;
	}
	else if (!strcmp("isinotherfilef", firstW) || !strcmp("isinotherfiles", firstW)) {
		assert(items == 2);
		isInPlayerFile = 1;
		number = which;
	}
	else {
		assert(items == 1);
		isInPlayerFile = 0;
		number = atoi(flag);
	}

	tFunc(tPlayer, isInPlayerFile, number);

	freeMemory(flag);
}

static void handleHitDefinitionSparkXY(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer) {
	int x, y;

	getTwoIntegerValuesWithDefaultValues(tAssignment, tPlayer, &x, &y, 0, 0);
	setHitDataSparkXY(tPlayer, x, y);
}

static void handleHitDefinitionSingleSound(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, void(tFunc)(DreamPlayer*, int, int, int), int tDefaultGroup, int tDefaultItem) {
	char* flag = evaluateDreamAssignmentAndReturnAsAllocatedString(tAssignment, tPlayer);
	turnStringLowercase(flag);

	char firstW[200], comma[10];
	int items = sscanf(flag, "%s", firstW);
	
	int group;
	int item;
	int isInPlayerFile;
	if (items < 1) {
		isInPlayerFile = 0;
		group = tDefaultGroup;
		item = tDefaultItem;
	}
	else if (!strcmp("isinotherfilef", firstW)) {
		isInPlayerFile = 0;
		int fullItems = sscanf(flag, "%s %d %s %d", firstW, &group, comma, &item);
		assert(fullItems >= 2);

		if (fullItems < 3) {
			item = tDefaultItem;
		}
	}
	else if (!strcmp("isinotherfiles", firstW)) {
		isInPlayerFile = 1;
		int fullItems = sscanf(flag, "%s %d %s %d", firstW, &group, comma, &item);
		if (fullItems < 2) {
			logWarningFormat("Unable to parse hit definition sound flag %s. Defaulting.", flag);
			group = tDefaultGroup;
			item = tDefaultItem;
		}
		else if (fullItems < 3) {
			item = tDefaultItem;
		}
	}
	else {
		isInPlayerFile = 0;
		int fullItems = sscanf(flag, "%d %s %d", &group, comma, &item);
		if (fullItems < 1) {
			logWarningFormat("Unable to parse hit definition sound flag %s. Defaulting.", flag);
			group = tDefaultGroup;
			item = tDefaultItem;
		}
		else if (fullItems < 2) {
			item = tDefaultItem;
		}
	}

	tFunc(tPlayer, isInPlayerFile, group, item);

	freeMemory(flag);
}

static void handleHitDefinitionSingleAttackHeight(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, void(tFunc)(DreamPlayer*, MugenAttackHeight), MugenAttackHeight tDefault) {
	char* flag = evaluateDreamAssignmentAndReturnAsAllocatedString(tAssignment, tPlayer);
	turnStringLowercase(flag);

	if (!strcmp("", flag)) tFunc(tPlayer, tDefault);
	else if (!strcmp("high", flag)) tFunc(tPlayer, MUGEN_ATTACK_HEIGHT_HIGH);
	else if (!strcmp("low", flag)) tFunc(tPlayer, MUGEN_ATTACK_HEIGHT_LOW);
	else if (!strcmp("trip", flag)) tFunc(tPlayer, MUGEN_ATTACK_HEIGHT_TRIP);
	else if (!strcmp("heavy", flag)) tFunc(tPlayer, MUGEN_ATTACK_HEIGHT_HEAVY);
	else if (!strcmp("none", flag)) tFunc(tPlayer, MUGEN_ATTACK_HEIGHT_NONE);
	else {
		logWarningFormat("Unable to parse hitdef attack height type %s. Defaulting.", flag);
		tFunc(tPlayer, tDefault);
	}

	freeMemory(flag);
}

static void handleExplodOneIntegerElement(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int tID, void(tFunc)(int, int), int tDefault) {
	char* flag = evaluateDreamAssignmentAndReturnAsAllocatedString(tAssignment, tPlayer);

	if (!strcmp("", flag)) tFunc(tID, tDefault);
	else tFunc(tID, atoi(flag));

	freeMemory(flag);
}

static void handleExplodTwoIntegerElements(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int tID, void(tFunc)(int, int, int), int tDefault1, int tDefault2) {
	char* flag = evaluateDreamAssignmentAndReturnAsAllocatedString(tAssignment, tPlayer);

	char string1[20], comma[10], string2[20];
	int items = sscanf(flag, "%s %s %s", string1, comma, string2);

	int val1;
	if (items < 1 || !strcmp("", string1)) val1 = tDefault1;
	else val1 = atoi(string1);

	int val2;
	if (items < 3 || !strcmp("", string2)) val2 = tDefault2;
	else val2 = atoi(string2);

	tFunc(tID, val1, val2);

	freeMemory(flag);
}

static void handleExplodThreeIntegerElements(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int tID, void(tFunc)(int, int, int, int), int tDefault1, int tDefault2, int tDefault3) {
	char* flag = evaluateDreamAssignmentAndReturnAsAllocatedString(tAssignment, tPlayer);

	char string[3][20], comma[2][10];
	int items = sscanf(flag, "%s %s %s %s %s", string[0], comma[0], string[1], comma[1], string[2]);

	int defaults[3];
	defaults[0] = tDefault1;
	defaults[1] = tDefault2;
	defaults[2] = tDefault3;

	int vals[3];
	int j;
	for (j = 0; j < 3; j++) {
		if (items < (1 + j * 2) || !strcmp("", string[j])) vals[j] = defaults[j];
		else vals[j] = atoi(string[j]);
	}

	tFunc(tID, vals[0], vals[1], vals[2]);

	freeMemory(flag);
}

static void handleExplodTwoFloatElements(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int tID, void(tFunc)(int, double, double), double tDefault1, double tDefault2) {
	char* flag = evaluateDreamAssignmentAndReturnAsAllocatedString(tAssignment, tPlayer);

	char string1[20], comma[10], string2[20];
	int items = sscanf(flag, "%s %s %s", string1, comma, string2);

	double val1;
	if (items < 1 || !strcmp("", string1)) val1 = tDefault1;
	else val1 = atof(string1);

	double val2;
	if (items < 3 || !strcmp("", string2)) val2 = tDefault2;
	else val2 = atof(string2);

	tFunc(tID, val1, val2);

	freeMemory(flag);
}

static void handleHitDefinitionOneIntegerElement(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, void(tFunc)(DreamPlayer*, int), int tDefault) {
	char* flag = evaluateDreamAssignmentAndReturnAsAllocatedString(tAssignment, tPlayer);

	if (!strcmp("", flag)) tFunc(tPlayer, tDefault);
	else tFunc(tPlayer, atoi(flag));

	freeMemory(flag);
}

static void handleHitDefinitionTwoIntegerElements(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, void(tFunc)(DreamPlayer*, int, int), int tDefault1, int tDefault2) {
	char* flag = evaluateDreamAssignmentAndReturnAsAllocatedString(tAssignment, tPlayer);

	char string1[20], comma[10], string2[20];
	int items = sscanf(flag, "%s %s %s", string1, comma, string2);

	int val1;
	if (items < 1 || !strcmp("", string1)) val1 = tDefault1;
	else val1 = atoi(string1);

	int val2;
	if (items < 3 || !strcmp("", string2)) val2 = tDefault2;
	else val2 = atoi(string2);

	tFunc(tPlayer, val1, val2);

	freeMemory(flag);
}

static void handleHitDefinitionThreeIntegerElements(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, void(tFunc)(DreamPlayer*, int, int, int), int tDefault1, int tDefault2, int tDefault3) {
	char* flag = evaluateDreamAssignmentAndReturnAsAllocatedString(tAssignment, tPlayer);

	char string[3][20], comma[2][10];
	int items = sscanf(flag, "%s %s %s %s %s", string[0], comma[0], string[1], comma[1], string[2]);

	int defaults[3];
	defaults[0] = tDefault1;
	defaults[1] = tDefault2;
	defaults[2] = tDefault3;

	int vals[3];
	int j;
	for (j = 0; j < 3; j++) {
		if (items < (1 + j * 2) || !strcmp("", string[j])) vals[j] = defaults[j];
		else vals[j] = atoi(string[j]);
	}

	tFunc(tPlayer, vals[0], vals[1], vals[2]);

	freeMemory(flag);
}

static void handleHitDefinitionOneFloatElement(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, void(tFunc)(DreamPlayer*, double), double tDefault) {
	char* flag = evaluateDreamAssignmentAndReturnAsAllocatedString(tAssignment, tPlayer);

	if (!strcmp("", flag)) tFunc(tPlayer, tDefault);
	else tFunc(tPlayer, atof(flag));

	freeMemory(flag);
}

static void handleHitDefinitionTwoFloatElements(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, void(tFunc)(DreamPlayer*, double, double), double tDefault1, double tDefault2) {
	char* flag = evaluateDreamAssignmentAndReturnAsAllocatedString(tAssignment, tPlayer);

	char string1[20], comma[10], string2[20];
	int items = sscanf(flag, "%s %s %s", string1, comma, string2);

	double val1;
	if (items < 1 || !strcmp("", string1)) val1 = tDefault1;
	else val1 = atof(string1);

	double val2;
	if (items < 3 || !strcmp("", string2)) val2 = tDefault2;
	else val2 = atof(string2);

	tFunc(tPlayer, val1, val2);

	freeMemory(flag);
}

static void handleHitDefinitionTwoOptionalIntegerElements(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, void(tFunc)(DreamPlayer*, int, int), void(tFuncDisable)(DreamPlayer*)) {
	char* flag = evaluateDreamAssignmentAndReturnAsAllocatedString(tAssignment, tPlayer);

	if (!strcmp("", flag)) {
		tFuncDisable(tPlayer);
	}

	freeMemory(flag);

	int x, y;
	getTwoIntegerValuesWithDefaultValues(tAssignment, tPlayer, &x, &y, 0, 0);

	tFunc(tPlayer, x, y);
}

static void handleHitDefinitionSinglePowerAddition(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, void(tFunc)(DreamPlayer*, int, int), double tDefaultFactor, int tDamage) {

	char* flag = evaluateDreamAssignmentAndReturnAsAllocatedString(tAssignment, tPlayer);

	char string1[20], comma[10], string2[20];
	int items = sscanf(flag, "%s %s %s", string1, comma, string2);

	int val1;
	if (items < 1 || !strcmp("", string1)) val1 = (int)(tDamage*tDefaultFactor);
	else val1 = atoi(string1);

	int val2;
	if (items < 3 || !strcmp("", string2)) val2 = val1 / 2;
	else val2 = atoi(string2);

	tFunc(tPlayer, val1, val2);

	freeMemory(flag);
}

static void handleHitDefinitionWithController(HitDefinitionController* e, DreamPlayer* tPlayer) {
	setHitDataVelocityX(tPlayer, 0);
	setHitDataVelocityY(tPlayer, 0);

	handleHitDefinitionAttribute(e, tPlayer);
	handleHitDefinitionSingleHitFlag(&e->mHitFlag, tPlayer, setHitDataHitFlag);
	handleHitDefinitionSingleHitFlag(&e->mGuardFlag, tPlayer, setHitDataGuardFlag);
	handleHitDefinitionAffectTeam(&e->mAffectTeam, tPlayer);
	handleHitDefinitionSingleAnimationType(&e->mAnimationType, tPlayer, setHitDataAnimationType, MUGEN_HIT_ANIMATION_TYPE_LIGHT);
	handleHitDefinitionSingleAnimationType(&e->mAirAnimationType, tPlayer, setHitDataAirAnimationType, getHitDataAnimationType(tPlayer));
	handleHitDefinitionSingleAnimationType(&e->mFallAnimationType, tPlayer, setHitDataFallAnimationType, getHitDataAirAnimationType(tPlayer) == MUGEN_HIT_ANIMATION_TYPE_UP ? MUGEN_HIT_ANIMATION_TYPE_UP : MUGEN_HIT_ANIMATION_TYPE_BACK);
	handleHitDefinitionPriority(&e->mPriority, tPlayer);
	handleHitDefinitionDamage(&e->mDamage, tPlayer);
	handleHitDefinitionSinglePauseTime(&e->mPauseTime, tPlayer, setHitDataPauseTime, 0, 0);
	handleHitDefinitionSinglePauseTime(&e->mGuardPauseTime, tPlayer, setHitDataGuardPauseTime, getHitDataPlayer1PauseTime(tPlayer), getHitDataPlayer2PauseTime(tPlayer));

	handleHitDefinitionSparkNumberSingle(&e->mSparkNumber, tPlayer, setHitDataSparkNumber, getDefaultPlayerSparkNumberIsInPlayerFile(tPlayer), getDefaultPlayerSparkNumber(tPlayer));
	handleHitDefinitionSparkNumberSingle(&e->mGuardSparkNumber, tPlayer, setHitDataGuardSparkNumber, getDefaultPlayerGuardSparkNumberIsInPlayerFile(tPlayer), getDefaultPlayerGuardSparkNumber(tPlayer));
	handleHitDefinitionSparkXY(&e->mSparkXY, tPlayer);
	handleHitDefinitionSingleSound(&e->mHitSound, tPlayer, setHitDataHitSound, 5, 0); // TODO: proper default
	handleHitDefinitionSingleSound(&e->mGuardSound, tPlayer, setHitDataGuardSound, 6, 0); // TODO: proper default

	handleHitDefinitionSingleAttackHeight(&e->mGroundType, tPlayer, setHitDataGroundType, MUGEN_ATTACK_HEIGHT_HIGH);
	handleHitDefinitionSingleAttackHeight(&e->mAirType, tPlayer, setHitDataAirType, getHitDataGroundType(tPlayer));

	handleHitDefinitionOneIntegerElement(&e->mGroundHitTime, tPlayer, setHitDataGroundHitTime, 0);
	handleHitDefinitionOneIntegerElement(&e->mGroundSlideTime, tPlayer, setHitDataGroundSlideTime, 0);
	handleHitDefinitionOneIntegerElement(&e->mGuardHitTime, tPlayer, setHitDataGuardHitTime, getHitDataGroundHitTime(tPlayer));
	handleHitDefinitionOneIntegerElement(&e->mGuardSlideTime, tPlayer, setHitDataGuardSlideTime, getHitDataGuardHitTime(tPlayer));
	handleHitDefinitionOneIntegerElement(&e->mAirHitTime, tPlayer, setHitDataAirHitTime, 20);
	handleHitDefinitionOneIntegerElement(&e->mGuardControlTime, tPlayer, setHitDataGuardControlTime, getHitDataGuardSlideTime(tPlayer));
	handleHitDefinitionOneIntegerElement(&e->mGuardDistance, tPlayer, setHitDataGuardDistance, getDefaultPlayerAttackDistance(tPlayer));
	handleHitDefinitionOneFloatElement(&e->mYAccel, tPlayer, setHitDataYAccel, transformDreamCoordinates(0.7, 480, getPlayerCoordinateP(tPlayer)));
	handleHitDefinitionTwoFloatElements(&e->mGroundVelocity, tPlayer, setHitDataGroundVelocity, 0, 0);
	handleHitDefinitionOneFloatElement(&e->mGuardVelocity, tPlayer, setHitDataGuardVelocity, getHitDataGroundVelocityX(tPlayer));
	handleHitDefinitionTwoFloatElements(&e->mAirVelocity, tPlayer, setHitDataAirVelocity, 0, 0);
	handleHitDefinitionTwoFloatElements(&e->mAirGuardVelocity, tPlayer, setHitDataAirGuardVelocity, getHitDataAirVelocityX(tPlayer) * 1.5, getHitDataAirVelocityY(tPlayer) / 2);

	handleHitDefinitionOneFloatElement(&e->mGroundCornerPushVelocityOffset, tPlayer, setGroundCornerPushVelocityOffset, getHitDataAttackType(tPlayer) == MUGEN_ATTACK_TYPE_ATTACK ? 0 : 1.3*getHitDataGuardVelocity(tPlayer));
	handleHitDefinitionOneFloatElement(&e->mAirCornerPushVelocityOffset, tPlayer, setAirCornerPushVelocityOffset, getGroundCornerPushVelocityOffset(tPlayer));
	handleHitDefinitionOneFloatElement(&e->mDownCornerPushVelocityOffset, tPlayer, setDownCornerPushVelocityOffset, getGroundCornerPushVelocityOffset(tPlayer));
	handleHitDefinitionOneFloatElement(&e->mGuardCornerPushVelocityOffset, tPlayer, setGuardCornerPushVelocityOffset, getGroundCornerPushVelocityOffset(tPlayer));
	handleHitDefinitionOneFloatElement(&e->mAirGuardCornerPushVelocityOffset, tPlayer, setAirGuardCornerPushVelocityOffset, getGuardCornerPushVelocityOffset(tPlayer));

	handleHitDefinitionOneIntegerElement(&e->mAirGuardControlTime, tPlayer, setHitDataAirGuardControlTime, getHitDataGuardControlTime(tPlayer));
	handleHitDefinitionOneIntegerElement(&e->mAirJuggle, tPlayer, setHitDataAirJuggle, 0);

	handleHitDefinitionTwoOptionalIntegerElements(&e->mMinimumDistance, tPlayer, setHitDataMinimumDistance, setHitDataMinimumDistanceInactive);
	handleHitDefinitionTwoOptionalIntegerElements(&e->mMaximumDistance, tPlayer, setHitDataMaximumDistance, setHitDataMaximumDistanceInactive);
	handleHitDefinitionTwoOptionalIntegerElements(&e->mSnap, tPlayer, setHitDataSnap, setHitDataSnapInactive);

	handleHitDefinitionOneIntegerElement(&e->mPlayerSpritePriority1, tPlayer, setHitDataPlayer1SpritePriority, 1);
	handleHitDefinitionOneIntegerElement(&e->mPlayerSpritePriority2, tPlayer, setHitDataPlayer2SpritePriority, 0);

	handleHitDefinitionOneIntegerElement(&e->mPlayer1ChangeFaceDirection, tPlayer, setHitDataPlayer1FaceDirection, 0);
	handleHitDefinitionOneIntegerElement(&e->mPlayer1ChangeFaceDirectionRelativeToPlayer2, tPlayer, setHitDataPlayer1ChangeFaceDirectionRelativeToPlayer2, 0);
	handleHitDefinitionOneIntegerElement(&e->mPlayer2ChangeFaceDirectionRelativeToPlayer1, tPlayer, setHitDataPlayer2ChangeFaceDirectionRelativeToPlayer1, 0);

	handleHitDefinitionOneIntegerElement(&e->mPlayer1StateNumber, tPlayer, setPlayer1StateNumber, -1);
	handleHitDefinitionOneIntegerElement(&e->mPlayer2StateNumber, tPlayer, setPlayer2StateNumber, -1);
	handleHitDefinitionOneIntegerElement(&e->mPlayer2CapableOfGettingPlayer1State, tPlayer, setHitDataPlayer2CapableOfGettingPlayer1State, 1);
	handleHitDefinitionOneIntegerElement(&e->mForceStanding, tPlayer, setHitDataForceStanding, getHitDataGroundVelocityY(tPlayer) != 0 ? 1 : 0);

	handleHitDefinitionOneIntegerElement(&e->mFall, tPlayer, setHitDataFall, 0);
	handleHitDefinitionOneFloatElement(&e->mFallXVelocity, tPlayer, setHitDataFallXVelocity, 0);
	handleHitDefinitionOneFloatElement(&e->mFallYVelocity, tPlayer, setHitDataFallYVelocity, transformDreamCoordinates(-9, 480, getPlayerCoordinateP(tPlayer))); 
	handleHitDefinitionOneIntegerElement(&e->mFallCanBeRecovered, tPlayer, setHitDataFallRecovery, 1);
	handleHitDefinitionOneIntegerElement(&e->mFallRecoveryTime, tPlayer, setHitDataFallRecoveryTime, 4);
	handleHitDefinitionOneIntegerElement(&e->mFallDamage, tPlayer, setHitDataFallDamage, 0);
	handleHitDefinitionOneIntegerElement(&e->mAirFall, tPlayer, setHitDataAirFall, getHitDataFall(tPlayer));
	handleHitDefinitionOneIntegerElement(&e->mForceNoFall, tPlayer, setHitDataForceNoFall, 0);

	handleHitDefinitionTwoFloatElements(&e->mDownVelocity, tPlayer, setHitDataDownVelocity, getHitDataAirVelocityX(tPlayer), getHitDataAirVelocityY(tPlayer));
	handleHitDefinitionOneIntegerElement(&e->mDownHitTime, tPlayer, setHitDataDownHitTime, 0);
	handleHitDefinitionOneIntegerElement(&e->mDownBounce, tPlayer, setHitDataDownBounce, 0);

	handleHitDefinitionOneIntegerElement(&e->mHitID, tPlayer, setHitDataHitID, 0);
	handleHitDefinitionOneIntegerElement(&e->mChainID, tPlayer, setHitDataChainID, -1);
	handleHitDefinitionTwoIntegerElements(&e->mNoChainID, tPlayer, setHitDataNoChainID, -1, -1);
	handleHitDefinitionOneIntegerElement(&e->mHitOnce, tPlayer, setHitDataHitOnce, 1);

	handleHitDefinitionOneIntegerElement(&e->mKill, tPlayer, setHitDataKill, 1);
	handleHitDefinitionOneIntegerElement(&e->mGuardKill, tPlayer, setHitDataGuardKill, 1);
	handleHitDefinitionOneIntegerElement(&e->mFallKill, tPlayer, setHitDataFallKill, 1);
	handleHitDefinitionOneIntegerElement(&e->mNumberOfHits, tPlayer, setHitDataNumberOfHits, 1);
	handleHitDefinitionSinglePowerAddition(&e->mGetPower, tPlayer, setHitDataGetPower, getDreamDefaultAttackDamageDoneToPowerMultiplier(), getHitDataDamage(tPlayer));
	handleHitDefinitionSinglePowerAddition(&e->mGivePower, tPlayer, setHitDataGivePower, getDreamDefaultAttackDamageReceivedToPowerMultiplier(), getHitDataDamage(tPlayer));

	handleHitDefinitionOneIntegerElement(&e->mPaletteEffectTime, tPlayer, setHitDataPaletteEffectTime, 0);
	handleHitDefinitionThreeIntegerElements(&e->mPaletteEffectMultiplication, tPlayer, setHitDataPaletteEffectMultiplication, 1, 1, 1);
	handleHitDefinitionThreeIntegerElements(&e->mPaletteEffectAddition, tPlayer, setHitDataPaletteEffectAddition, 0, 0, 0);

	handleHitDefinitionOneIntegerElement(&e->mEnvironmentShakeTime, tPlayer, setHitDataEnvironmentShakeTime, 0); // TODO: proper defaults for the whole screen shake block
	handleHitDefinitionOneFloatElement(&e->mEnvironmentShakeFrequency, tPlayer, setHitDataEnvironmentShakeFrequency, 0);
	handleHitDefinitionOneIntegerElement(&e->mEnvironmentShakeAmplitude, tPlayer, setHitDataEnvironmentShakeAmplitude, 0);
	handleHitDefinitionOneFloatElement(&e->mEnvironmentShakePhase, tPlayer, setHitDataEnvironmentShakePhase, 0);

	handleHitDefinitionOneIntegerElement(&e->mFallEnvironmentShakeTime, tPlayer, setHitDataFallEnvironmentShakeTime, 0); // TODO: proper defaults for the whole screen shake block
	handleHitDefinitionOneFloatElement(&e->mFallEnvironmentShakeFrequency, tPlayer, setHitDataFallEnvironmentShakeFrequency, 0);
	handleHitDefinitionOneIntegerElement(&e->mFallEnvironmentShakeAmplitude, tPlayer, setHitDataFallEnvironmentShakeAmplitude, 0);
	handleHitDefinitionOneFloatElement(&e->mFallEnvironmentShakePhase, tPlayer, setHitDataFallEnvironmentShakePhase, 0);

	setHitDataIsFacingRight(tPlayer, getPlayerIsFacingRight(tPlayer));

	setHitDataActive(tPlayer);
}

static int handleHitDefinition(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	HitDefinitionController* e = tController->mData;

	handleHitDefinitionWithController(e, tPlayer);

	return 0;
}

static int handleAnimationChange(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	ChangeAnimationController* e = tController->mData;

	int animation = evaluateDreamAssignmentAndReturnAsInteger(&e->tNewAnimation, tPlayer);
	int step = evaluateDreamAssignmentAndReturnAsInteger(&e->tStep, tPlayer);
	changePlayerAnimationWithStartStep(tPlayer, animation, step);

	return 0;
}

static int handleAnimationChange2(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	ChangeAnimationController* e = tController->mData;

	int animation = evaluateDreamAssignmentAndReturnAsInteger(&e->tNewAnimation, tPlayer);
	int step = evaluateDreamAssignmentAndReturnAsInteger(&e->tStep, tPlayer);
	changePlayerAnimationToPlayer2AnimationWithStartStep(tPlayer, animation, step);

	return 0;
}

static int handleControlSetting(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	ControlSettingController* e = tController->mData;

	int control = evaluateDreamAssignmentAndReturnAsInteger(&e->tValue, tPlayer);
	setPlayerControl(tPlayer, control);

	return 0;
}

static int handleWidth(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	WidthController* e = tController->mData;

	// TODO: work out how width works
	(void)e;
	(void)tPlayer;

	return 0;
}

static int handleSpritePriority(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	SpritePriorityController* e = tController->mData;

	int value = evaluateDreamAssignmentAndReturnAsInteger(&e->tValue, tPlayer);
	setPlayerSpritePriority(tPlayer, value);

	return 0;
}

static void handleSingleSpecialAssert(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer) {
	char* flag = evaluateDreamAssignmentAndReturnAsAllocatedString(tAssignment, tPlayer);
	turnStringLowercase(flag);

	if (!strcmp("intro", flag)) {
		setPlayerIntroFlag(tPlayer);
	}
	else if (!strcmp("invisible", flag)) {
		setPlayerInvisibleFlag(tPlayer);
	}
	else if (!strcmp("roundnotover", flag)) {
		setDreamRoundNotOverFlag();
	}
	else if (!strcmp("nobardisplay", flag)) {
		setDreamBarInvisibleForOneFrame();
	}
	else if (!strcmp("nobg", flag)) {
		setDreamStageInvisibleForOneFrame();
	}
	else if (!strcmp("nofg", flag)) {
		setDreamStageLayer1InvisibleForOneFrame();
	}
	else if (!strcmp("nostandguard", flag)) {
		setPlayerNoStandGuardFlag(tPlayer);
	}
	else if (!strcmp("nocrouchguard", flag)) {
		setPlayerNoCrouchGuardFlag(tPlayer);
	}
	else if (!strcmp("noairguard", flag)) {
		setPlayerNoAirGuardFlag(tPlayer);
	}
	else if (!strcmp("noautoturn", flag)) {
		setPlayerNoAutoTurnFlag(tPlayer);
	}
	else if (!strcmp("nojugglecheck", flag)) {
		setPlayerNoJuggleCheckFlag(tPlayer);
	}
	else if (!strcmp("nokosnd", flag)) {
		setPlayerNoKOSoundFlag(tPlayer);
	}
	else if (!strcmp("nokoslow", flag)) {
		setPlayerNoKOSlowdownFlag(tPlayer);
	}
	else if (!strcmp("noshadow", flag)) {
		setPlayerNoShadow(tPlayer);
	}
	else if (!strcmp("globalnoshadow", flag)) {
		setAllPlayersNoShadow();
	}
	else if (!strcmp("nomusic", flag)) {
		setDreamNoMusicFlag();
	}
	else if (!strcmp("nowalk", flag)) {
		setPlayerNoWalkFlag(tPlayer);
	}
	else if (!strcmp("timerfreeze", flag)) {
		setTimerFreezeFlag();
	}
	else if (!strcmp("unguardable", flag)) {
		setPlayerUnguardableFlag(tPlayer);
	}
	else {
		logWarningFormat("Unrecognized special assert flag %s. Ignoring.", flag);
	}
	freeMemory(flag);
}

static int handleSpecialAssert(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	SpecialAssertController* e = tController->mData;

	handleSingleSpecialAssert(&e->mFlag, tPlayer);

	if (e->mHasFlag2) {
		handleSingleSpecialAssert(&e->mFlag2, tPlayer);
	}
	if (e->mHasFlag3) {
		handleSingleSpecialAssert(&e->mFlag3, tPlayer);
	}

	return 0;
}

static int handleMakeDust(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	MakeDustController* e = tController->mData;

	Vector3D pos = evaluateDreamAssignmentAndReturnAsVector3D(&e->mPositionOffset, tPlayer);
	int spacing = evaluateDreamAssignmentAndReturnAsInteger(&e->mSpacing, tPlayer);

	addPlayerDust(tPlayer, 0, pos, spacing);

	if (e->mHasSecondDustCloud) {
		pos = evaluateDreamAssignmentAndReturnAsVector3D(&e->mPositionOffset2, tPlayer);
		addPlayerDust(tPlayer, 1, pos, spacing);
	}

	return 0;
}

typedef struct {
	DreamPlayer* mPlayer;

} VarSetHandlingCaller;

static void handleSettingSingleVariable(void* tCaller, void* tData) {
	VarSetHandlingCaller* caller = tCaller;
	VarSetControllerEntry* e = tData;

	if (e->mType == VAR_SET_TYPE_SYSTEM) {
		int val = evaluateDreamAssignmentAndReturnAsInteger(&e->mAssignment, caller->mPlayer);
		setPlayerSystemVariable(caller->mPlayer, e->mID, val);
	}
	else if (e->mType == VAR_SET_TYPE_INTEGER) {
		int val = evaluateDreamAssignmentAndReturnAsInteger(&e->mAssignment, caller->mPlayer);
		setPlayerVariable(caller->mPlayer, e->mID, val);
	}
	else if (e->mType == VAR_SET_TYPE_SYSTEM_FLOAT) {
		double val = evaluateDreamAssignmentAndReturnAsFloat(&e->mAssignment, caller->mPlayer);
		setPlayerSystemFloatVariable(caller->mPlayer, e->mID, val);
	}
	else if (e->mType == VAR_SET_TYPE_FLOAT) {
		double val = evaluateDreamAssignmentAndReturnAsFloat(&e->mAssignment, caller->mPlayer);
		setPlayerFloatVariable(caller->mPlayer, e->mID, val);
	}
	else {
		logWarningFormat("Unrecognized variable type %d. Ignoring.", e->mType);
	}
}

static int handleSettingVariable(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	if (!tPlayer) return 0;

	VarSetController* e = tController->mData;
	VarSetHandlingCaller caller;
	caller.mPlayer = tPlayer;

	vector_map(&e->mVarSets, handleSettingSingleVariable, &caller);

	return 0;
}

static void handleAddingSingleVariable(void* tCaller, void* tData) {
	VarSetHandlingCaller* caller = tCaller;
	VarSetControllerEntry* e = tData;

	if (e->mType == VAR_SET_TYPE_SYSTEM) {
		int val = evaluateDreamAssignmentAndReturnAsInteger(&e->mAssignment, caller->mPlayer);
		addPlayerSystemVariable(caller->mPlayer, e->mID, val);
	}
	else if (e->mType == VAR_SET_TYPE_INTEGER) {
		int val = evaluateDreamAssignmentAndReturnAsInteger(&e->mAssignment, caller->mPlayer);
		addPlayerVariable(caller->mPlayer, e->mID, val);
	}
	else if (e->mType == VAR_SET_TYPE_SYSTEM_FLOAT) {
		double val = evaluateDreamAssignmentAndReturnAsFloat(&e->mAssignment, caller->mPlayer);
		addPlayerSystemFloatVariable(caller->mPlayer, e->mID, val);
	}
	else if (e->mType == VAR_SET_TYPE_FLOAT) {
		double val = evaluateDreamAssignmentAndReturnAsFloat(&e->mAssignment, caller->mPlayer);
		addPlayerFloatVariable(caller->mPlayer, e->mID, val);
	}
	else {
		logWarningFormat("Unrecognized variable type %d. Ignoring.", e->mType);
	}
}

static int handleAddingVariable(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	if (!tPlayer) return 0;
	
	VarSetController* e = tController->mData;
	VarSetHandlingCaller caller;
	caller.mPlayer = tPlayer;

	vector_map(&e->mVarSets, handleAddingSingleVariable, &caller);

	return 0;
}

static int handleNull() { return 0; }

static DreamMugenStateType handleStateTypeAssignment(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer) {
	DreamMugenStateType ret;

	char* text = evaluateDreamAssignmentAndReturnAsAllocatedString(tAssignment, tPlayer);
	turnStringLowercase(text);
	
	if (!strcmp("a", text)) ret = MUGEN_STATE_TYPE_AIR;
	else if (!strcmp("s", text)) ret = MUGEN_STATE_TYPE_STANDING;
	else if (!strcmp("c", text)) ret = MUGEN_STATE_TYPE_CROUCHING;
	else {
		logWarningFormat("Unrecognized state type %s. Defaulting to not changing state.", text);
		ret = MUGEN_STATE_TYPE_UNCHANGED;
	}
	freeMemory(text);

	return ret;
}

static DreamMugenStateType handleStatePhysicsAssignment(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer) {
	DreamMugenStatePhysics ret;

	char* text = evaluateDreamAssignmentAndReturnAsAllocatedString(tAssignment, tPlayer);
	turnStringLowercase(text);
	if (!strcmp("s", text)) ret = MUGEN_STATE_PHYSICS_STANDING;
	else if (!strcmp("a", text)) ret = MUGEN_STATE_PHYSICS_AIR;
	else if (!strcmp("c", text)) ret = MUGEN_STATE_PHYSICS_CROUCHING;
	else if (!strcmp("n", text)) ret = MUGEN_STATE_PHYSICS_NONE;
	else {
		logWarning("Unrecognized state physics type");
		logWarningString(text);
		ret = MUGEN_STATE_PHYSICS_UNCHANGED;
	}
	freeMemory(text);

	return ret;
}

static DreamMugenStateMoveType handleStateMoveTypeAssignment(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer) {
	DreamMugenStateMoveType ret;

	char* text = evaluateDreamAssignmentAndReturnAsAllocatedString(tAssignment, tPlayer);
	turnStringLowercase(text);
	if (!strcmp("a", text)) ret = MUGEN_STATE_MOVE_TYPE_ATTACK;
	else if (!strcmp("i", text)) ret = MUGEN_STATE_MOVE_TYPE_IDLE;
	else {
		logWarning("Unrecognized state move type");
		logWarningString(text);
		ret = MUGEN_STATE_MOVE_TYPE_UNCHANGED;
	}
	freeMemory(text);

	return ret;
}

static int handleStateTypeSet(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	StateTypeSetController* e = tController->mData;

	if (e->mHasStateType) {
		DreamMugenStateType type = handleStateTypeAssignment(&e->mStateType, tPlayer);
		setPlayerStateType(tPlayer, type);
	}

	if (e->mHasMoveType) {
		DreamMugenStateMoveType moveType = handleStateMoveTypeAssignment(&e->mMoveType, tPlayer);
		setPlayerStateMoveType(tPlayer, moveType);
	}

	if (e->mHasPhysics) {
		DreamMugenStatePhysics physics = handleStatePhysicsAssignment(&e->mPhysics, tPlayer);
		setPlayerPhysics(tPlayer, physics);
	}

	return 0;
}

static int handleHitVelocitySetting(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	Set2DPhysicsController* e = tController->mData;

	if (e->mIsSettingX) {
		double x = evaluateDreamAssignmentAndReturnAsFloat(&e->x, tPlayer);
		setActiveHitDataVelocityX(tPlayer, x); // TODO: check
	}

	if (e->mIsSettingY) {
		double y = evaluateDreamAssignmentAndReturnAsFloat(&e->y, tPlayer);
		setActiveHitDataVelocityY(tPlayer, y); // TODO: check
	}

	return 0;
}

static int handleDefenseMultiplier(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	DefenseMultiplierController* e = tController->mData;

	double val = evaluateDreamAssignmentAndReturnAsFloat(&e->mValue, tPlayer);
	setPlayerDefenseMultiplier(tPlayer, val);

	return 0;
}

static int handleFallEnvironmentShake(DreamPlayer* tPlayer) {

	if (!isActiveHitDataActive(tPlayer)) return 0;

	int time = getActiveHitDataFallEnvironmentShakeTime(tPlayer);
	setActiveHitDataFallEnvironmentShakeTime(tPlayer, 0);
	double freq = getActiveHitDataFallEnvironmentShakeFrequency(tPlayer);
	int ampl = getActiveHitDataFallEnvironmentShakeAmplitude(tPlayer);
	double phase = getActiveHitDataFallEnvironmentShakePhase(tPlayer);

	setEnvironmentShake(time, freq, ampl, phase);

	return 0;
}

static void handleExplodAnimation(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int tID) {
	char* text = evaluateDreamAssignmentAndReturnAsAllocatedString(tAssignment, tPlayer);
	turnStringLowercase(text);

	char firstW[100];
	int anim;
	int items = sscanf(text, "%s %d", firstW, &anim);

	int isInFightDefFile;
	if (items > 1 && !strcmp("isinotherfilef", firstW)) {
		isInFightDefFile = 1;
		if (items < 2) anim = -1;
	}
	else {
		isInFightDefFile = 0;
		anim = atoi(text);
	}

	setExplodAnimation(tID, isInFightDefFile, anim);

	freeMemory(text);
}

DreamExplodPositionType getPositionTypeFromAssignment(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer) {
	char* text = evaluateDreamAssignmentAndReturnAsAllocatedString(tAssignment, tPlayer);

	DreamExplodPositionType type;
	if (!strcmp("p1", text)) {
		type = EXPLOD_POSITION_TYPE_RELATIVE_TO_P1;
	}
	else if (!strcmp("p2", text)) {
		type = EXPLOD_POSITION_TYPE_RELATIVE_TO_P2;
	}
	else if (!strcmp("front", text)) {
		type = EXPLOD_POSITION_TYPE_RELATIVE_TO_FRONT;
	}
	else if (!strcmp("back", text)) {
		type = EXPLOD_POSITION_TYPE_RELATIVE_TO_BACK;
	}
	else if (!strcmp("left", text)) {
		type = EXPLOD_POSITION_TYPE_RELATIVE_TO_LEFT;
	}
	else if (!strcmp("right", text)) {
		type = EXPLOD_POSITION_TYPE_RELATIVE_TO_RIGHT;
	}
	else if (!strcmp("none", text)) {
		type = EXPLOD_POSITION_TYPE_NONE;
	}
	else {
		logWarningFormat("Unable to determine position type %s. Defaulting to EXPLOD_POSITION_TYPE_RELATIVE_TO_P1.", text);
		type = EXPLOD_POSITION_TYPE_RELATIVE_TO_P1;
	}

	freeMemory(text);

	return type;
}

static void handleExplodPositionType(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int tID) {

	DreamExplodPositionType type = getPositionTypeFromAssignment(tAssignment, tPlayer);
	setExplodPositionType(tID, type);
}

static void handleExplodTransparencyType(DreamMugenAssignment** tAssignment, int isUsed, DreamPlayer* tPlayer, int tID) {
	if (!isUsed) {
		setExplodTransparencyType(tID, 0, EXPLOD_TRANSPARENCY_TYPE_ALPHA);
		return;
	}

	char* text = evaluateDreamAssignmentAndReturnAsAllocatedString(tAssignment, tPlayer);

	DreamExplodTransparencyType type;
	if (!strcmp("alpha", text)) {
		type = EXPLOD_TRANSPARENCY_TYPE_ALPHA;
	}
	else if (!strcmp("addalpha", text)) {
		type = EXPLOD_TRANSPARENCY_TYPE_ADD_ALPHA;
	}
	else {
		logWarningFormat("Unable to determine explod transparency type %s. Default to alpha.", text);
		type = EXPLOD_TRANSPARENCY_TYPE_ALPHA;
	}

	setExplodTransparencyType(tID, 1, type);

	freeMemory(text);
}

static int handleExplod(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	ExplodController* e = tController->mData;

	int id = addExplod(tPlayer);
	handleExplodAnimation(&e->mAnim, tPlayer, id);
	handleExplodOneIntegerElement(&e->mID, tPlayer, id, setExplodID, -1);
	handleExplodTwoIntegerElements(&e->mPosition, tPlayer, id, setExplodPosition, 0, 0);
	handleExplodPositionType(&e->mPositionType, tPlayer, id);
	handleExplodOneIntegerElement(&e->mHorizontalFacing, tPlayer, id, setExplodHorizontalFacing, 1);
	handleExplodOneIntegerElement(&e->mVerticalFacing, tPlayer, id, setExplodVerticalFacing, 1);
	handleExplodOneIntegerElement(&e->mBindTime, tPlayer, id, setExplodBindTime, -1);
	handleExplodTwoFloatElements(&e->mVelocity, tPlayer, id, setExplodVelocity, 0, 0);
	handleExplodTwoFloatElements(&e->mAcceleration, tPlayer, id, setExplodAcceleration, 0, 0);
	handleExplodTwoIntegerElements(&e->mRandomOffset, tPlayer, id, setExplodRandomOffset, 0, 0);

	handleExplodOneIntegerElement(&e->mRemoveTime, tPlayer, id, setExplodRemoveTime, -2);
	handleExplodOneIntegerElement(&e->mSuperMove, tPlayer, id, setExplodSuperMove, 0);
	handleExplodOneIntegerElement(&e->mSuperMoveTime, tPlayer, id, setExplodSuperMoveTime, 0);
	handleExplodOneIntegerElement(&e->mPauseMoveTime, tPlayer, id, setExplodPauseMoveTime, 0);
	handleExplodTwoFloatElements(&e->mScale, tPlayer, id, setExplodScale, 1, 1);
	handleExplodOneIntegerElement(&e->mSpritePriority, tPlayer, id, setExplodSpritePriority, 0);
	handleExplodOneIntegerElement(&e->mOnTop, tPlayer, id, setExplodOnTop, 0);
	handleExplodThreeIntegerElements(&e->mShadow, tPlayer, id, setExplodShadow, 0, 0, 0);
	handleExplodOneIntegerElement(&e->mOwnPalette, tPlayer, id, setExplodOwnPalette, 0);
	handleExplodOneIntegerElement(&e->mIsRemovedOnGetHit, tPlayer, id, setExplodRemoveOnGetHit, 0);
	handleExplodOneIntegerElement(&e->mIgnoreHitPause, tPlayer, id, setExplodIgnoreHitPause, 1);
	handleExplodTransparencyType(&e->mTransparencyType, e->mHasTransparencyType, tPlayer, id);

	finalizeExplod(id);

	return 0;
}

static int modifyExplod(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	ExplodController* e = tController->mData;
	return 0;

	int id;
	getSingleIntegerValueOrDefault(&e->mID, tPlayer, &id, -1);
	removeExplodsWithID(tPlayer, id);

	handleExplod(tController, tPlayer);

	return 0;
}

static int handleHitFallDamage(DreamPlayer* tPlayer) {
	if (!isActiveHitDataActive(tPlayer)) return 0;

	int fallDamage = getActiveHitDataFallDamage(tPlayer);
	addPlayerDamage(tPlayer, fallDamage);

	return 0;
}

static int handlePositionFreeze(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	PositionFreezeController* e = tController->mData;

	int val = evaluateDreamAssignment(&e->mValue, tPlayer);
	if (val) setPlayerPositionFrozen(tPlayer);

	return 0;
}

static int handleHitFallVelocity(DreamPlayer* tPlayer) {

	addPlayerVelocityX(tPlayer, getActiveHitDataFallXVelocity(tPlayer), getPlayerCoordinateP(tPlayer));
	addPlayerVelocityY(tPlayer, getActiveHitDataFallYVelocity(tPlayer), getPlayerCoordinateP(tPlayer)); // TODO: check

	return 0;
}


static void handleSingleNotHitBy(int tSlot, int tHasValue, MugenStringVector tValue, int tTime, DreamPlayer* tPlayer, void(*tResetFunc)(DreamPlayer*, int)) {
	if (!tHasValue) return;
	tResetFunc(tPlayer, tSlot);

	char* flag1 = tValue.mElement[0];
	setPlayerNotHitByFlag1(tPlayer, tSlot, flag1);
	int i;
	for (i = 1; i < tValue.mSize; i++) {
		char* flag2 = tValue.mElement[i];
		addPlayerNotHitByFlag2(tPlayer, tSlot, flag2);
	}

	setPlayerNotHitByTime(tPlayer, tSlot, tTime);
}

static void handleReversalDefinitionEntry(MugenStringVector tValue, DreamPlayer* tPlayer) {
	resetHitDataReversalDef(tPlayer);
	
	char* flag1 = tValue.mElement[0];
	setHitDataReversalDefFlag1(tPlayer, flag1);
	int i;
	for (i = 1; i < tValue.mSize; i++) {
		char* flag2 = tValue.mElement[i];
		addHitDataReversalDefFlag2(tPlayer, flag2);
	}
}



static int handleNotHitBy(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	NotHitByController* e = tController->mData;

	int time = evaluateDreamAssignmentAndReturnAsInteger(&e->mTime, tPlayer);

	handleSingleNotHitBy(0, e->mHasValue, e->mValue, time, tPlayer, resetPlayerNotHitBy);
	handleSingleNotHitBy(1, e->mHasValue2, e->mValue2, time, tPlayer, resetPlayerNotHitBy);

	return 0;
}

static int handleHitBy(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	NotHitByController* e = tController->mData;

	int time = evaluateDreamAssignmentAndReturnAsInteger(&e->mTime, tPlayer);

	handleSingleNotHitBy(0, e->mHasValue, e->mValue, time, tPlayer, resetPlayerHitBy);
	handleSingleNotHitBy(1, e->mHasValue2, e->mValue2, time, tPlayer, resetPlayerHitBy);

	return 0;
}

static int handleHitFallSet(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	HitFallSetController* e = tController->mData;
	DreamPlayer* otherPlayer = getPlayerOtherPlayer(tPlayer);

	int val = evaluateDreamAssignmentAndReturnAsInteger(&e->mValue, tPlayer);
	if (val != -1) {
		setActiveHitDataFall(otherPlayer, val);
	}

	if (e->mHasXVelocity) {
		double x = evaluateDreamAssignmentAndReturnAsFloat(&e->mXVelocity, tPlayer);
		setActiveHitDataFallXVelocity(otherPlayer, x);
	}

	if (e->mHasYVelocity) {
		double y = evaluateDreamAssignmentAndReturnAsFloat(&e->mYVelocity, tPlayer);
		setActiveHitDataFallYVelocity(otherPlayer, y);
	}

	return 0;
}


static int handleAttackMultiplierSetting(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	SingleRequiredValueController* e = tController->mData;

	double value = evaluateDreamAssignmentAndReturnAsFloat(&e->mValue, tPlayer);

	setPlayerAttackMultiplier(tPlayer, value);

	return 0;
}

static int handlePowerAddition(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	SingleRequiredValueController* e = tController->mData;

	int value = evaluateDreamAssignmentAndReturnAsInteger(&e->mValue, tPlayer);
	addPlayerPower(tPlayer, value);

	return 0;
}

static int handlePowerSettingController(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	SingleRequiredValueController* e = tController->mData;

	int value = evaluateDreamAssignmentAndReturnAsInteger(&e->mValue, tPlayer);
	setPlayerPower(tPlayer, value);

	return 0;
}

static void getSingleIntegerValueOrDefaultFunctionCall(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, void(tFunc)(int), int tDefault) {
	char* flag = evaluateDreamAssignmentAndReturnAsAllocatedString(tAssignment, tPlayer);

	int val;
	if (!strcmp("", flag)) val = tDefault;
	else val = atoi(flag);
	tFunc(val);

	freeMemory(flag);
}

static void getSingleFloatValueOrDefaultFunctionCall(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, void(tFunc)(double), double tDefault) {
	char* flag = evaluateDreamAssignmentAndReturnAsAllocatedString(tAssignment, tPlayer);

	double val;
	if (!strcmp("", flag)) val = tDefault;
	else val = atof(flag);
	tFunc(val);

	freeMemory(flag);
}

static void getSingleVector3DValueOrDefaultFunctionCall(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, void(tFunc)(Vector3D), Vector3D tDefault) {
	char* flag = evaluateDreamAssignmentAndReturnAsAllocatedString(tAssignment, tPlayer);

	double x, y, z;
	char tx[20], comma1[10], ty[20], comma2[10], tz[20];
	int items = sscanf(flag, "%s %s %s %s %s", tx, comma1, ty, comma2, tz);

	if (items < 1) x = tDefault.x;
	else x = atof(tx);

	if (items < 3) y = tDefault.y;
	else y = atof(ty);

	if (items < 5) z = tDefault.z;
	else z = atof(tz);

	tFunc(makePosition(x, y, z));

	freeMemory(flag);
}

static void handleSuperPauseAnimation(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer) {
	char* flag = evaluateDreamAssignmentAndReturnAsAllocatedString(tAssignment, tPlayer);
	turnStringLowercase(flag);

	char firstW[200];
	int which = -1;

	int items = sscanf(flag, "%s %d", firstW, &which);

	int isInPlayerFile;
	int id;
	if (items < 1) {
		isInPlayerFile = 0;
		id = 30;
	}
	else if (!strcmp("isinotherfilef", firstW) || !strcmp("isinotherfiles", firstW)) {
		assert(items == 2);
		isInPlayerFile = 1;
		id = which;
	}
	else {
		assert(items == 1);
		isInPlayerFile = 0;
		id = atoi(flag);
	}

	setDreamSuperPauseAnimation(isInPlayerFile, id);

	freeMemory(flag);
}

static void handleSuperPauseSound(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer) {
	char* flag = evaluateDreamAssignmentAndReturnAsAllocatedString(tAssignment, tPlayer);
	turnStringLowercase(flag);

	int isInPlayerFile;
	int group;
	int item;
	if (!strcmp("", flag)) {
		isInPlayerFile = 0;
		group = -1;
		item = -1;
	}
	else {
		char firstW[20], comma[10];
		int items = sscanf(flag, "%s", firstW);
		assert(items == 1);

		if (!strcmp("isinotherfilef", firstW)) {
			isInPlayerFile = 0;
			int items = sscanf(flag, "%s %d %s %d", firstW, &group, comma, &item);
			assert(items == 4);
		}
		else if (!strcmp("isinotherfiles", firstW)) {
			isInPlayerFile = 1;
			int items = sscanf(flag, "%s %d %s %d", firstW, &group, comma, &item);
			assert(items == 4);
		}
		else {
			isInPlayerFile = 0;
			int items = sscanf(flag, "%d %s %d", &group, comma, &item);
			if (items != 3) {
				logWarningFormat("Unable to parse super pause flag %s. Ignoring.", flag);
				freeMemory(flag);
				return;
			}
		}
	}

	setDreamSuperPauseSound(isInPlayerFile, group, item);

	freeMemory(flag);
}

static int handleSuperPause(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	SuperPauseController* e = tController->mData;

	setDreamSuperPausePlayer(tPlayer);
	getSingleIntegerValueOrDefaultFunctionCall(&e->mTime, tPlayer, setDreamSuperPauseTime, 30);
	getSingleIntegerValueOrDefaultFunctionCall(&e->mBufferTimeForCommandsDuringPauseEnd, tPlayer, setDreamSuperPauseBufferTimeForCommandsDuringPauseEnd, 0);
	getSingleIntegerValueOrDefaultFunctionCall(&e->mMoveTime, tPlayer, setDreamSuperPauseMoveTime, 0);
	getSingleIntegerValueOrDefaultFunctionCall(&e->mDoesPauseBackground, tPlayer, setDreamSuperPauseIsPausingBG, 1);

	handleSuperPauseAnimation(&e->mAnim, tPlayer);
	handleSuperPauseSound(&e->mSound, tPlayer);
	getSingleVector3DValueOrDefaultFunctionCall(&e->mPosition, tPlayer, setDreamSuperPausePosition, makePosition(0, 0, 0));
	getSingleIntegerValueOrDefaultFunctionCall(&e->mIsDarkening, tPlayer, setDreamSuperPauseDarkening, 1);
	getSingleFloatValueOrDefaultFunctionCall(&e->mPlayer2DefenseMultiplier, tPlayer, setDreamSuperPausePlayer2DefenseMultiplier, 0);
	getSingleIntegerValueOrDefaultFunctionCall(&e->mPowerToAdd, tPlayer, setDreamSuperPausePowerToAdd, 0);
	getSingleIntegerValueOrDefaultFunctionCall(&e->mSetPlayerUnhittable, tPlayer, setDreamSuperPausePlayerUnhittability, 1);

	setDreamSuperPauseActive();

	return 0;
}

static int handlePauseController(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	(void)tController;
	(void)tPlayer;
	// TODO: rework pause system

	return 0;
}

static void handleHelperOneFloatElement(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, DreamPlayer* tHelper, void(tFunc)(DreamPlayer*, double), double tDefault) {
	double val;
	getSingleFloatValueOrDefault(tAssignment, tPlayer, &val, tDefault);
	tFunc(tHelper, val);
}

static void handleHelperOneIntegerElement(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, DreamPlayer* tHelper, void(tFunc)(DreamPlayer*, int), int tDefault) {
	int val;
	getSingleIntegerValueOrDefault(tAssignment, tPlayer, &val, tDefault);
	tFunc(tHelper, val);
}

static void handleHelperTwoFloatElements(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, DreamPlayer* tHelper, void(tFunc)(DreamPlayer*, double, double), double tDefault1, double tDefault2) {
	double val1, val2;
	getTwoFloatValuesWithDefaultValues(tAssignment, tPlayer, &val1, &val2, tDefault1, tDefault2);
	tFunc(tHelper, val1, val2);
}

static void handleHelperFacing(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, DreamPlayer* tHelper) {
	int facing;
	getSingleIntegerValueOrDefault(tAssignment, tPlayer, &facing, 1);

	if (facing == 1) {
		setPlayerIsFacingRight(tHelper, getPlayerIsFacingRight(tPlayer));
	}
	else {
		setPlayerIsFacingRight(tHelper, !getPlayerIsFacingRight(tPlayer));
	}

}

static int handleHelper(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	// return 0; // TODO

	HelperController* e = tController->mData;
	DreamPlayer* helper = clonePlayerAsHelper(tPlayer);

	handleHelperOneIntegerElement(&e->mID, tPlayer, helper, setPlayerID, 0);

	Vector3DI mOffset = makeVector3DI(0, 0, 0);
	getTwoIntegerValuesWithDefaultValues(&e->mPosition, tPlayer, &mOffset.x, &mOffset.y, 0, 0);
	DreamExplodPositionType positionType = getPositionTypeFromAssignment(&e->mPositionType, tPlayer);
	Position position = getFinalPositionFromPositionType(positionType, makePosition(mOffset.x, mOffset.y, mOffset.z), tPlayer);

	handleHelperFacing(&e->mFacing, tPlayer, helper);


	handleHelperOneIntegerElement(&e->mCanControl, tPlayer, helper, setPlayerHelperControl, 0);
	// TODO: own palette
	// TODO: supermovetime
	// TODO: pausemovetime

	handleHelperOneFloatElement(&e->mSizeScaleX, tPlayer, helper, setPlayerScaleX, getPlayerScaleX(tPlayer));
	handleHelperOneFloatElement(&e->mSizeScaleY, tPlayer, helper, setPlayerScaleY, getPlayerScaleY(tPlayer));
	handleHelperOneIntegerElement(&e->mSizeGroundBack, tPlayer, helper, setPlayerGroundSizeBack, getPlayerGroundSizeBack(tPlayer));
	handleHelperOneIntegerElement(&e->mSizeGroundFront, tPlayer, helper, setPlayerGroundSizeFront, getPlayerGroundSizeFront(tPlayer));
	handleHelperOneIntegerElement(&e->mSizeAirBack, tPlayer, helper, setPlayerAirSizeBack, getPlayerAirSizeBack(tPlayer));
	handleHelperOneIntegerElement(&e->mSizeAirFront, tPlayer, helper, setPlayerAirSizeFront, getPlayerAirSizeFront(tPlayer));
	handleHelperOneIntegerElement(&e->mSizeHeight, tPlayer, helper, setPlayerHeight, getPlayerHeight(tPlayer));

	// TODO: scale projectiles

	handleHelperTwoFloatElements(&e->mSizeHeadPosition, tPlayer, helper, setPlayerHeadPosition, getPlayerHeadPositionX(tPlayer), getPlayerHeadPositionY(tPlayer));
	handleHelperTwoFloatElements(&e->mSizeMiddlePosition, tPlayer, helper, setPlayerMiddlePosition, getPlayerMiddlePositionX(tPlayer), getPlayerMiddlePositionY(tPlayer));
	handleHelperOneIntegerElement(&e->mSizeShadowOffset, tPlayer, helper, setPlayerShadowOffset, getPlayerShadowOffset(tPlayer));

	char* type = evaluateDreamAssignmentAndReturnAsAllocatedString(&e->mType, tPlayer);
	turnStringLowercase(type);
	if (!strcmp("player", type)) {
		setPlayerScreenBound(helper, 1, 1, 1);
	}
	else {
		setPlayerScreenBound(helper, 0, 0, 0);
	}
	freeMemory(type);

	setPlayerPosition(helper, position, getPlayerCoordinateP(helper));
	handleHelperOneIntegerElement(&e->mStateNumber, tPlayer, helper, changePlayerState, 0);



	return 0;
}

static int handleDestroySelf(DreamPlayer* tPlayer) {
	printf("%d %d destroying self\n", tPlayer->mRootID, tPlayer->mID);

	destroyPlayer(tPlayer);

	return 1;
}

static int handleAddingLife(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	LifeAddController* e = tController->mData;

	int val, canKill, isAbsolute;
	getSingleIntegerValueOrDefault(&e->mValue, tPlayer, &val, 0);
	getSingleIntegerValueOrDefault(&e->mCanKill, tPlayer, &canKill, 1);
	getSingleIntegerValueOrDefault(&e->mIsAbsolute, tPlayer, &isAbsolute, 0);

	// TODO absolute
	(void)isAbsolute;

	int playerLife = getPlayerLife(tPlayer);
	if (!canKill && playerLife + val <= 0) {
		val = -(playerLife - 1);
	}

	addPlayerDamage(tPlayer, -val);

	return 0;
}

static int handleAddingTargetLife(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	TargetLifeAddController* e = tController->mData;

	int val, canKill, isAbsolute, id;
	getSingleIntegerValueOrDefault(&e->mValue, tPlayer, &val, 0);
	getSingleIntegerValueOrDefault(&e->mID, tPlayer, &id, -1);
	getSingleIntegerValueOrDefault(&e->mCanKill, tPlayer, &canKill, 1);
	getSingleIntegerValueOrDefault(&e->mIsAbsolute, tPlayer, &isAbsolute, 0);

	addPlayerTargetLife(tPlayer, id, val, canKill, isAbsolute);

	return 0;
}

static int handleAddingTargetPower(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	TargetPowerAddController* e = tController->mData;

	int val, id;
	getSingleIntegerValueOrDefault(&e->mValue, tPlayer, &val, 0);
	getSingleIntegerValueOrDefault(&e->mID, tPlayer, &id, -1);

	addPlayerTargetPower(tPlayer, id, val);

	return 0;
}

static int handleTargetVelocityAddController(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	Target2DPhysicsController* e = tController->mData;

	int id;
	getSingleIntegerValueOrDefault(&e->mID, tPlayer, &id, -1);

	if (e->mIsSettingX) {
		double x = evaluateDreamAssignmentAndReturnAsFloat(&e->x, tPlayer);
		addPlayerTargetVelocityX(tPlayer, id, x, getPlayerCoordinateP(tPlayer));
	}

	if (e->mIsSettingY) {
		double y = evaluateDreamAssignmentAndReturnAsFloat(&e->y, tPlayer);
		addPlayerTargetVelocityY(tPlayer, id, y, getPlayerCoordinateP(tPlayer));
	}

	return 0;
}

static int handleTargetVelocitySetController(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	Target2DPhysicsController* e = tController->mData;

	int id;
	getSingleIntegerValueOrDefault(&e->mID, tPlayer, &id, -1);

	if (e->mIsSettingX) {
		double x = evaluateDreamAssignmentAndReturnAsFloat(&e->x, tPlayer);
		setPlayerTargetVelocityX(tPlayer, id, x, getPlayerCoordinateP(tPlayer));
	}

	if (e->mIsSettingY) {
		double y = evaluateDreamAssignmentAndReturnAsFloat(&e->y, tPlayer);
		setPlayerTargetVelocityY(tPlayer, id, y, getPlayerCoordinateP(tPlayer));
	}

	return 0;
}

static int handleAngleDrawController(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	AngleDrawController* e = tController->mData;

	if (e->mHasScale) {
		Vector3D scale = evaluateDreamAssignmentAndReturnAsVector3D(&e->mScale, tPlayer);
		setPlayerDrawScale(tPlayer, scale);
	}

	if (e->mHasValue) {
		double val = evaluateDreamAssignmentAndReturnAsFloat(&e->mValue, tPlayer);
		setPlayerDrawAngle(tPlayer, val);
	}

	return 0;
}

static int handleAngleAddController(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	SingleRequiredValueController* e = tController->mData;

	double angle = evaluateDreamAssignmentAndReturnAsFloat(&e->mValue, tPlayer);
	addPlayerDrawAngle(tPlayer, angle);

	return 0;
}

static int handleAngleMulController(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	SingleRequiredValueController* e = tController->mData;

	double angle = evaluateDreamAssignmentAndReturnAsFloat(&e->mValue, tPlayer);
	multiplyPlayerDrawAngle(tPlayer, angle);

	return 0;
}

static int handleAngleSetController(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	SingleRequiredValueController* e = tController->mData;

	double angle = evaluateDreamAssignmentAndReturnAsFloat(&e->mValue, tPlayer);
	setPlayerFixedDrawAngle(tPlayer, angle);

	return 0;
}

static int handleRemovingExplod(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	RemoveExplodController* e = tController->mData;

	if (e->mHasID) {
		int id = evaluateDreamAssignmentAndReturnAsInteger(&e->mID, tPlayer);
		removeExplodsWithID(tPlayer, id);
	}
	else {
		removeAllExplods(tPlayer);
	}

	return 0;
}

static int handleBindToRootController(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	BindController* e = tController->mData;

	int time, facing;
	Vector3D offset = makePosition(0, 0, 0);
	getSingleIntegerValueOrDefault(&e->mTime, tPlayer, &time, 1);
	getSingleIntegerValueOrDefault(&e->mFacing, tPlayer, &facing, 0);
	getTwoFloatValuesWithDefaultValues(&e->mPosition, tPlayer, &offset.x, &offset.y, 0, 0);

	bindPlayerToRoot(tPlayer, time, facing, offset);

	return 0;
}

static int handleBindToParentController(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	BindController* e = tController->mData;

	int time, facing;
	Vector3D offset = makePosition(0, 0, 0);
	getSingleIntegerValueOrDefault(&e->mTime, tPlayer, &time, 1);
	getSingleIntegerValueOrDefault(&e->mFacing, tPlayer, &facing, 0);
	getTwoFloatValuesWithDefaultValues(&e->mPosition, tPlayer, &offset.x, &offset.y, 0, 0);

	bindPlayerToParent(tPlayer, time, facing, offset);

	return 0;
}

static DreamPlayerBindPositionType handleBindToTargetPositionType(DreamMugenAssignment** tPosition, DreamPlayer* tPlayer) {
	char* text = evaluateDreamAssignmentAndReturnAsAllocatedString(tPosition, tPlayer);

	char val1[20], val2[20], comma1[10], comma2[10], postype[30];
	int items = sscanf(text, "%s %s %s %s %s", val1, comma1, val2, comma2, postype);
	freeMemory(text);

	if (items < 5) {
		return PLAYER_BIND_POSITION_TYPE_AXIS;
	}
	else {
		assert(!strcmp(comma1, ","));
		assert(!strcmp(comma2, ","));
		turnStringLowercase(postype);

		if (!strcmp("foot", postype)) {
			return PLAYER_BIND_POSITION_TYPE_AXIS;
		} else if (!strcmp("mid", postype)) {
			return PLAYER_BIND_POSITION_TYPE_MID;
		} else if (!strcmp("head", postype)) {
			return PLAYER_BIND_POSITION_TYPE_HEAD;
		}
		else {
			logWarningFormat("Unrecognized postype: %s. Defaulting to PLAYER_BIND_POSITION_TYPE_AXIS.", postype);
			return PLAYER_BIND_POSITION_TYPE_AXIS;
		}
	}

}

static int handleBindToTargetController(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	BindController* e = tController->mData;

	int time, id;
	Vector3D offset = makePosition(0, 0, 0);
	getSingleIntegerValueOrDefault(&e->mTime, tPlayer, &time, 1);
	getSingleIntegerValueOrDefault(&e->mID, tPlayer, &id, -1);
	getTwoFloatValuesWithDefaultValues(&e->mPosition, tPlayer, &offset.x, &offset.y, 0, 0);
	DreamPlayerBindPositionType bindType = handleBindToTargetPositionType(&e->mPosition, tPlayer);

	bindPlayerToTarget(tPlayer, time, offset, id, bindType);

	return 0;
}

static int handleTurnController(DreamPlayer* tPlayer) {
	turnPlayerAround(tPlayer);

	return 0;
}

static int handlePushPlayerController(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	SingleRequiredValueController* e = tController->mData;

	int isPushDisabled = evaluateDreamAssignment(&e->mValue, tPlayer);

	setPlayerPushDisabledFlag(tPlayer, isPushDisabled);

	return 0;
}

static int handleSettingVariableRange(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	VarRangeSetController* e = tController->mData;

	if (e->mType == VAR_SET_TYPE_INTEGER) {
		int value, first, last;
		getSingleIntegerValueOrDefault(&e->mValue, tPlayer, &value, 0);
		getSingleIntegerValueOrDefault(&e->mFirst, tPlayer, &first, 0);
		getSingleIntegerValueOrDefault(&e->mLast, tPlayer, &last, 59);
		int i;
		for (i = first; i <= last; i++) {
			setPlayerVariable(tPlayer, i, value);
		}
	}
	else if (e->mType == VAR_SET_TYPE_FLOAT) {
		double value;
		int first, last;
		getSingleFloatValueOrDefault(&e->mValue, tPlayer, &value, 0);
		getSingleIntegerValueOrDefault(&e->mFirst, tPlayer, &first, 0);
		getSingleIntegerValueOrDefault(&e->mLast, tPlayer, &last, 39);
		int i;
		for (i = first; i <= last; i++) {
			setPlayerFloatVariable(tPlayer, i, value);
		}
	}
	else {
		logWarningFormat("Unrecognized var type %d. Ignoring.", e->mType);
	}

	return 0;
}

static int handleScreenBound(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	ScreenBoundController* e = tController->mData;

	int val;
	int moveCameraX, moveCameraY;

	getSingleIntegerValueOrDefault(&e->mValue, tPlayer, &val, 0);
	getTwoIntegerValuesWithDefaultValues(&e->mMoveCameraFlags, tPlayer, &moveCameraX, &moveCameraY, 0, 0);

	setPlayerScreenBound(tPlayer, val, moveCameraX, moveCameraY);

	return 0;
}

static int handleMoveHitReset(DreamPlayer* tPlayer) {
	setPlayerMoveHitReset(tPlayer);

	return 0;
}

static int handleGravity(DreamPlayer* tPlayer) {
	double accel = getPlayerVerticalAcceleration(tPlayer);
	addPlayerVelocityY(tPlayer, accel, getPlayerCoordinateP(tPlayer));

	return 0;
}

static int handleSettingAttackDistance(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	SingleRequiredValueController* e = tController->mData;

	int value = evaluateDreamAssignmentAndReturnAsInteger(&e->mValue, tPlayer);
	setHitDataGuardDistance(tPlayer, value);

	return 0;
}

static int handleTargetBindController(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	BindController* e = tController->mData;

	int time, id;
	Vector3D offset = makePosition(0, 0, 0);
	getSingleIntegerValueOrDefault(&e->mTime, tPlayer, &time, 1);
	getSingleIntegerValueOrDefault(&e->mID, tPlayer, &id, -1);
	getTwoFloatValuesWithDefaultValues(&e->mPosition, tPlayer, &offset.x, &offset.y, 0, 0);

	bindPlayerTargetToPlayer(tPlayer, time, offset, id);

	return 0;
}

static int handleSetTargetFacing(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	SetTargetFacingController* e = tController->mData;

	int value = evaluateDreamAssignmentAndReturnAsInteger(&e->mValue, tPlayer);
	int id;
	getSingleIntegerValueOrDefault(&e->mID, tPlayer, &id, -1);

	setPlayerTargetFacing(tPlayer, id, value);

	return 0;
}

static int handleReversalDefinition(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	ReversalDefinitionController* e = tController->mData;

	handleReversalDefinitionEntry(e->mAttributes, tPlayer);

	return 0;
}

static int handleProjectile(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	//return 0; // TODO

	ProjectileController* e = tController->mData;

	DreamPlayer* p = createNewProjectileFromPlayer(tPlayer);

	handleHitDefinitionOneIntegerElement(&e->mID, p, setProjectileID, -1);
	handleHitDefinitionOneIntegerElement(&e->mAnimation, p, setProjectileAnimation, 0);
	handleHitDefinitionOneIntegerElement(&e->mHitAnimation, p, setProjectileHitAnimation, -1);
	handleHitDefinitionOneIntegerElement(&e->mRemoveAnimation, p, setProjectileRemoveAnimation, getProjectileHitAnimation(p));
	handleHitDefinitionOneIntegerElement(&e->mCancelAnimation, p, setProjectileCancelAnimation, getProjectileRemoveAnimation(p));
	handleHitDefinitionTwoFloatElements(&e->mScale, p, setProjectileScale, 1, 1);
	handleHitDefinitionOneIntegerElement(&e->mIsRemovingProjectileAfterHit, p, setProjectileRemoveAfterHit, 1);
	handleHitDefinitionOneIntegerElement(&e->mRemoveTime, p, setProjectileRemoveTime, -1);
	handleHitDefinitionTwoFloatElements(&e->mVelocity, p, setProjectileVelocity, 0, 0);
	handleHitDefinitionTwoFloatElements(&e->mRemoveVelocity, p, setProjectileRemoveVelocity, 0, 0);
	handleHitDefinitionTwoFloatElements(&e->mAcceleration, p, setProjectileAcceleration, 0, 0);
	handleHitDefinitionTwoFloatElements(&e->mVelocityMultipliers, p, setProjectileVelocityMultipliers, 1, 1);


	handleHitDefinitionOneIntegerElement(&e->mHitAmountBeforeVanishing, p, setProjectileHitAmountBeforeVanishing, 1);
	handleHitDefinitionOneIntegerElement(&e->mMissTime, p, setProjectilMisstime, 0);
	handleHitDefinitionOneIntegerElement(&e->mPriority, p, setProjectilePriority, 1);
	handleHitDefinitionOneIntegerElement(&e->mSpriteSpriority, p, setProjectileSpritePriority, 3);

	handleHitDefinitionOneIntegerElement(&e->mEdgeBound, p, setProjectileEdgeBound, (int)transformDreamCoordinates(40, 240, getPlayerCoordinateP(p)));
	handleHitDefinitionOneIntegerElement(&e->mStageBound, p, setProjectileStageBound, (int)transformDreamCoordinates(40, 240, getPlayerCoordinateP(p)));
	handleHitDefinitionTwoIntegerElements(&e->mHeightBoundValues, p, setProjectileHeightBoundValues, (int)transformDreamCoordinates(-240, 240, getPlayerCoordinateP(p)), (int)transformDreamCoordinates(1, 240, getPlayerCoordinateP(p)));

	Position offset;
	getTwoFloatValuesWithDefaultValues(&e->mOffset, p, &offset.x, &offset.y, 0, 0);
	offset.z = 0;
	int positionType;
	getSingleIntegerValueOrDefault(&e->mPositionType, p, &positionType, EXPLOD_POSITION_TYPE_RELATIVE_TO_P1);
	Position pos = getFinalPositionFromPositionType(positionType, offset, tPlayer);
	setProjectilePosition(p, pos);

	handleHitDefinitionOneIntegerElement(&e->mShadow, p, setProjectileShadow, 0);
	handleHitDefinitionOneIntegerElement(&e->mSuperMoveTime, p, setProjectileSuperMoveTime, 0);
	handleHitDefinitionOneIntegerElement(&e->mPauseMoveTime, p, setProjectilePauseMoveTime, 0);

	handleHitDefinitionOneIntegerElement(&e->mHasOwnPalette, p, setProjectileHasOwnPalette, 0);
	handleHitDefinitionTwoIntegerElements(&e->mRemapPalette, p, setProjectileRemapPalette, -1, 0);
	handleHitDefinitionOneIntegerElement(&e->mAfterImageTime, p, setProjectileAfterImageTime, 0);
	handleHitDefinitionOneIntegerElement(&e->mAfterImageLength, p, setProjectileAfterImageLength, 0);
	handleHitDefinitionOneIntegerElement(&e->mAfterImage, p, setProjectileAfterImage, 0);

	handleHitDefinitionWithController(&e->mHitDef, p);

	return 0;
}

int handleDreamMugenStateControllerAndReturnWhetherStateChanged(DreamMugenStateController * tController, DreamPlayer* tPlayer)
{

	if (!int_map_contains(&gVariableHandler.mStateControllerHandlers, tController->mType)) {
		logWarningFormat("Unrecognized state controller %d. Ignoring.", tController->mType);
		return 0;
	}

	StateControllerHandleFunction func = int_map_get(&gVariableHandler.mStateControllerHandlers, tController->mType);
	return func(tController, tPlayer);
}


static int handleAfterImage(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	(void)tController;
	(void)tPlayer;
	// TODO

	return 0;
}

static int handleAfterImageTime(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	(void)tController;
	(void)tPlayer;
	// TODO

	return 0;
}

static int handlePaletteEffect(DreamMugenStateController* tController, DreamPlayer* tPlayer, int tDoesAffectCharacters, int tDoesAffectBackground) {
	(void)tController;
	(void)tPlayer;
	(void)tDoesAffectCharacters;
	(void)tDoesAffectBackground;
	// TODO

	return 0;
}

static int handleAppendToClipboardController(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	ClipboardController* e = tController->mData;

	char* formatString = evaluateDreamAssignmentAndReturnAsAllocatedString(&e->mText, tPlayer);
	char* parameterString = evaluateDreamAssignmentAndReturnAsAllocatedString(&e->mParams, tPlayer);

	addClipboardLineFormatString(formatString, parameterString);

	freeMemory(formatString);
	freeMemory(parameterString);

	return 0;
}

static int handlePalFXController(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	// TODO: Palettes
	return 0;
}

static int handleBGPalFXController(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	// TODO: Palettes
	return 0;
}

static int handleClearClipboardController() {
	clearClipboard();
	return 0;
}

static int handleDisplayToClipboardController(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	clearClipboard();
	handleAppendToClipboardController(tController, tPlayer); // TODO: clean up
	
	return 0;
}

static int handleEnvironmentColorController(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	EnvironmentColorController* e = tController->mData;

	int time;
	int isUnderCharacters;
	Vector3DI colors = evaluateDreamAssignmentAndReturnAsVector3DI(&e->mValue, tPlayer);
	getSingleIntegerValueOrDefault(&e->mTime, tPlayer, &time, 1);
	getSingleIntegerValueOrDefault(&e->mUnder, tPlayer, &isUnderCharacters, 1);
	
	setEnvironmentColor(colors, time, isUnderCharacters);

	return 0;
}

static int handleEnvironmentShakeController(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	EnvironmentShakeController* e = tController->mData;

	int time, ampl;
	double freq, phase;

	getSingleIntegerValueOrDefault(&e->mTime, tPlayer, &time, 1);
	getSingleFloatValueOrDefault(&e->mFrequency, tPlayer, &freq, 60);
	getSingleIntegerValueOrDefault(&e->mAmplitude, tPlayer, &ampl, (int)transformDreamCoordinates(-4, 240, getDreamStageCoordinateP()));
	getSingleFloatValueOrDefault(&e->mPhaseOffset, tPlayer, &phase, freq >= 90 ? 90 : 0);

	setEnvironmentShake(time, freq, ampl, phase);

	return 0;
}

static int handleExplodBindTimeController(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	ExplodBindTimeController* e = tController->mData;
	
	int id, time;

	getSingleIntegerValueOrDefault(&e->mID, tPlayer, &id, -1);
	getSingleIntegerValueOrDefault(&e->mTime, tPlayer, &time, 1);

	setExplodBindTimeForID(tPlayer, id, time);

	return 0;
}

static int handleForceFeedbackController(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	ForceFeedbackController* e = tController->mData;

	int time;
	getSingleIntegerValueOrDefault(&e->mTime, tPlayer, &time, 1);
	int freq1, freq2;
	getTwoIntegerValuesWithDefaultValues(&e->mFrequency, tPlayer, &freq1, &freq2, 128, 0);
	int ampl1, ampl2;
	getTwoIntegerValuesWithDefaultValues(&e->mAmplitude, tPlayer, &ampl1, &ampl2, 128, 0);
	int self;
	getSingleIntegerValueOrDefault(&e->mSelf, tPlayer, &self, 1);

	int i = self ? tPlayer->mRootID : getPlayerOtherPlayer(tPlayer)->mRootID;
	addControllerRumbleSingle(i, time, freq1, ampl1 / 255.0);

	return 0;
}

static int handleGameMakeAnimController(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	GameMakeAnimController* e = tController->mData;
	int animationNumber;
	getSingleIntegerValueOrDefault(&e->mValue, tPlayer, &animationNumber, 0);

	int isUnderPlayer;
	getSingleIntegerValueOrDefault(&e->mIsUnderPlayer, tPlayer, &isUnderPlayer, 0);

	Position pos = makePosition(0, 0, 0);
	getTwoFloatValuesWithDefaultValues(&e->mPosOffset, tPlayer, &pos.x, &pos.y, 0, 0);

	int random;
	getSingleIntegerValueOrDefault(&e->mRandomOffset, tPlayer, &random, 0);

	pos = vecAdd(pos, makePosition(randfrom(-random / 2.0, random / 2.0), randfrom(-random / 2.0, random / 2.0), 0));
	pos = vecAdd(pos, getPlayerPosition(tPlayer, getPlayerCoordinateP(tPlayer)));
	pos = vecAdd(pos, getDreamStageCoordinateSystemOffset(getPlayerCoordinateP(tPlayer)));

	if (isUnderPlayer) pos.z = 10;
	else pos.z = 70;

	int id = addMugenAnimation(getDreamFightEffectAnimation(animationNumber), getDreamFightEffectSprites(), pos);
	setMugenAnimationCameraPositionReference(id, getDreamMugenStageHandlerCameraPositionReference());
	setMugenAnimationNoLoop(id);

	return 0;
}

static int handleHitAddController(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	SingleRequiredValueController* e = tController->mData;

	int value;
	getSingleIntegerValueOrDefault(&e->mValue, tPlayer, &value, 0);

	// TODO: add to combo counter
	(void)value;

	return 0;
}

static int handleHitOverrideController(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	HitOverrideController* e = tController->mData;
	char* attr = evaluateDreamAssignmentAndReturnAsAllocatedString(&e->mAttributeString, tPlayer);

	DreamMugenStateType stateType;
	MugenAttackClass attackClass;
	MugenAttackType attackType;
	getHitDefinitionAttributeValuesFromString(attr, &stateType, &attackClass, &attackType);
	freeMemory(attr);

	int stateno, slot, time, forceAir;
	getSingleIntegerValueOrDefault(&e->mStateNo, tPlayer, &stateno, 0);
	getSingleIntegerValueOrDefault(&e->mSlot, tPlayer, &slot, 0);
	getSingleIntegerValueOrDefault(&e->mTime, tPlayer, &time, 1);
	getSingleIntegerValueOrDefault(&e->mForceAir, tPlayer, &forceAir, 0);

	setPlayerHitOverride(tPlayer, stateType, attackClass, attackType, stateno, slot, time, forceAir);

	return 0;
}

static int handleSetLifeController(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	SingleRequiredValueController* e = tController->mData;

	int value;
	getSingleIntegerValueOrDefault(&e->mValue, tPlayer, &value, 0);

	setPlayerLife(tPlayer, value);

	return 0;
}

static int handleDrawOffsetController(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	Set2DPhysicsController* e = tController->mData;

	if (e->mIsSettingX) {
		double x = evaluateDreamAssignmentAndReturnAsFloat(&e->x, tPlayer);
		setPlayerDrawOffsetX(tPlayer, x, getPlayerCoordinateP(tPlayer));
	}

	if (e->mIsSettingY) {
		double y = evaluateDreamAssignmentAndReturnAsFloat(&e->y, tPlayer);
		setPlayerDrawOffsetY(tPlayer, y, getPlayerCoordinateP(tPlayer));
	}

	return 0;
}

static int handleRemapPaletteController(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	(void)tController;
	(void)tPlayer;
	// TODO: palettes

	return 0;
}

static int handleSoundPanController(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	(void)tController;
	(void)tPlayer;
	// TODO: more advanced sound stuff

	return 0;
}

static int handleStopSoundController(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	(void)tController;
	(void)tPlayer;
	// TODO: more advanced sound stuff

	return 0;
}

static int handleTargetDropController(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	(void)tController;
	(void)tPlayer;
	// TODO: rework targeting system

	return 0;
}

static int handleVictoryQuoteController(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	(void)tController;
	(void)tPlayer;
	// TODO: victory quotes

	return 0;
}


static BlendType handleTransparencyType(DreamMugenAssignment** tType, DreamPlayer* tPlayer, int* tAlphaDefaultSrc, int* tAlphaDefaultDst) {
	char* text = evaluateDreamAssignmentAndReturnAsAllocatedString(tType, tPlayer);
	turnStringLowercase(text);

	BlendType ret;
	if (!strcmp("default", text) || !strcmp("none", text)) {
		ret = BLEND_TYPE_NORMAL;
		*tAlphaDefaultSrc = 256;
		*tAlphaDefaultDst = 256;
	}
	else if (!strcmp("add", text)) {
		ret = BLEND_TYPE_ADDITION;
		*tAlphaDefaultSrc = 256;
		*tAlphaDefaultDst = 256;
	}
	else if (!strcmp("addalpha", text)) {
		ret = BLEND_TYPE_ADDITION;
		*tAlphaDefaultSrc = 256;
		*tAlphaDefaultDst = 0;
	}
	else if (!strcmp("add1", text)) {
		ret = BLEND_TYPE_ADDITION;
		*tAlphaDefaultSrc = 256;
		*tAlphaDefaultDst = 128;
	}
	else  if (!strcmp("sub", text)) {
		ret = BLEND_TYPE_SUBTRACTION;
		*tAlphaDefaultSrc = 256;
		*tAlphaDefaultDst = 256;
	}
	else {
		logWarningFormat("Unrecognized transparency format: %s. Default to normal blending.", text);
		ret = BLEND_TYPE_NORMAL;
		*tAlphaDefaultSrc = 256;
		*tAlphaDefaultDst = 256;
	}

	freeMemory(text);

	return ret;
}

static int handleTransparencyController(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	TransparencyController* e = tController->mData;
	
	int alphaDefaultSrc, alphaDefaultDst;
	BlendType type = handleTransparencyType(&e->mTransparency, tPlayer, &alphaDefaultSrc, &alphaDefaultDst);

	int alphaSource, alphaDest;
	getTwoIntegerValuesWithDefaultValues(&e->mAlpha, tPlayer, &alphaSource, &alphaDest, alphaDefaultSrc, alphaDefaultDst);

	setPlayerOneFrameTransparency(tPlayer, type, alphaSource, alphaDest);

	return 0;
}

static int handleRandomVariableController(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	VarRandomController* e = tController->mData;
	
	int index;
	getSingleIntegerValueOrDefault(&e->mValue, tPlayer, &index, 0);

	char* rangeText = evaluateDreamAssignmentAndReturnAsAllocatedString(&e->mRange, tPlayer);
	char comma[10];
	int val1, val2;
	int items = sscanf(rangeText, "%d %s %d", &val1, comma, &val2);
	freeMemory(rangeText);

	int value;
	if (items == 3) {
		value = randfromInteger(val1, val2);
	}
	else {
		value = randfromInteger(0, val1);
	}

	setPlayerVariable(tPlayer, index, value);

	return 0;
}

int afterImageHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleAfterImage(tController, tPlayer); }
int afterImageTimeHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleAfterImageTime(tController, tPlayer); }
int allPalFXHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handlePaletteEffect(tController, tPlayer, 1, 1); }
int angleAddHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleAngleAddController(tController, tPlayer); }
int angleDrawHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleAngleDrawController(tController, tPlayer); }
int angleMulHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleAngleMulController(tController, tPlayer); }
int angleSetHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleAngleSetController(tController, tPlayer); }
int appendToClipboardHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleAppendToClipboardController(tController, tPlayer); }
int assertSpecialHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleSpecialAssert(tController, tPlayer); }
int attackDistHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleSettingAttackDistance(tController, tPlayer); }
int attackMulSetHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleAttackMultiplierSetting(tController, tPlayer); }
int bgPalFXHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleBGPalFXController(tController, tPlayer); }
int bindToParentHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleBindToParentController(tController, tPlayer); }
int bindToRootHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleBindToRootController(tController, tPlayer); }
int bindToTargetHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleBindToTargetController(tController, tPlayer);}
int changeAnimHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleAnimationChange(tController, tPlayer); }
int changeAnim2HandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleAnimationChange2(tController, tPlayer); }
int changeStateHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleStateChange(tController, tPlayer); }
int clearClipboardHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleClearClipboardController(); }
int ctrlSetHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleControlSetting(tController, tPlayer); }
int defenceMulSetHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleDefenseMultiplier(tController, tPlayer); }
int destroySelfHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleDestroySelf(tPlayer); }
int displayToClipboardHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleDisplayToClipboardController(tController, tPlayer); }
int envColorHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleEnvironmentColorController(tController, tPlayer); }
int envShakeHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleEnvironmentShakeController(tController, tPlayer); }
int explodHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleExplod(tController, tPlayer); }
int explodBindTimeHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleExplodBindTimeController(tController, tPlayer); }
int forceFeedbackHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleForceFeedbackController(tController, tPlayer); }
int fallEnvShakeHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleFallEnvironmentShake(tPlayer); }
int gameMakeAnimHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleGameMakeAnimController(tController, tPlayer); }
int gravityHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleGravity(tPlayer); }
int helperHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleHelper(tController, tPlayer); }
int hitAddHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleHitAddController(tController, tPlayer); }
int hitByHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleHitBy(tController, tPlayer); }
int hitDefHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleHitDefinition(tController, tPlayer); }
int hitFallDamageHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleHitFallDamage(tPlayer); }
int hitFallSetHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleHitFallSet(tController, tPlayer); }
int hitFallVelHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleHitFallVelocity(tPlayer); }
int hitOverrideHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleHitOverrideController(tController, tPlayer); }
int hitVelSetHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleHitVelocitySetting(tController, tPlayer); }
int lifeAddHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleAddingLife(tController, tPlayer); }
int lifeSetHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleSetLifeController(tController, tPlayer); }
int makeDustHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleMakeDust(tController, tPlayer); }
int modifyExplodHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return modifyExplod(tController, tPlayer); }
int moveHitResetHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleMoveHitReset(tPlayer); }
int notHitByHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleNotHitBy(tController, tPlayer); }
int nullHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleNull(); }
int offsetHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleDrawOffsetController(tController, tPlayer); }
int palFXHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handlePalFXController(tController, tPlayer); }
int parentVarAddHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleAddingVariable(tController, getPlayerParent(tPlayer)); }
int parentVarSetHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleSettingVariable(tController, getPlayerParent(tPlayer)); }
int pauseHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handlePauseController(tController, tPlayer); }
int playerPushHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handlePushPlayerController(tController, tPlayer); }
int playSndHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handlePlaySound(tController, tPlayer); }
int posAddHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handlePositionAdding(tController, tPlayer); }
int posFreezeHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handlePositionFreeze(tController, tPlayer); }
int posSetHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handlePositionSetting(tController, tPlayer); }
int powerAddHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handlePowerAddition(tController, tPlayer); }
int powerSetHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handlePowerSettingController(tController, tPlayer); }
int projectileHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleProjectile(tController, tPlayer); }
int remapPalHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleRemapPaletteController(tController, tPlayer); }
int removeExplodHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleRemovingExplod(tController, tPlayer); }
int reversalDefHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleReversalDefinition(tController, tPlayer); }
int screenBoundHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleScreenBound(tController, tPlayer); }
int selfStateHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleSelfStateChange(tController, tPlayer); }
int sprPriorityHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleSpritePriority(tController, tPlayer); }
int stateTypeSetHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleStateTypeSet(tController, tPlayer); }
int sndPanHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleSoundPanController(tController, tPlayer); }
int stopSndHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleStopSoundController(tController, tPlayer); }
int superPauseHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleSuperPause(tController, tPlayer); }
int targetBindHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleTargetBindController(tController, tPlayer); }
int targetDropHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleTargetDropController(tController, tPlayer); }
int targetFacingHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleSetTargetFacing(tController, tPlayer); }
int targetLifeAddHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleAddingTargetLife(tController, tPlayer); }
int targetPowerAddHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleAddingTargetPower(tController, tPlayer); }
int targetStateHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleTargetStateChange(tController, tPlayer); }
int targetVelAddHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleTargetVelocityAddController(tController, tPlayer); }
int targetVelSetHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleTargetVelocitySetController(tController, tPlayer); }
int transHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleTransparencyController(tController, tPlayer); }
int turnHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleTurnController(tPlayer); }
int varAddHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleAddingVariable(tController, tPlayer); }
int varRandomHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleRandomVariableController(tController, tPlayer); }
int varRangeSetHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleSettingVariableRange(tController, tPlayer); }
int varSetHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleSettingVariable(tController, tPlayer); }
int velAddHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleVelocityAddition(tController, tPlayer); }
int velMulHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleVelocityMultiplication(tController, tPlayer); }
int velSetHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleVelocitySetting(tController, tPlayer); }
int victoryQuoteHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleVictoryQuoteController(tController, tPlayer); }
int widthHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleWidth(tController, tPlayer); }


static void setupStateControllerHandlers() {
	gVariableHandler.mStateControllerHandlers = new_int_map();

	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_AFTER_IMAGE, afterImageHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_AFTER_IMAGE_TIME, afterImageTimeHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_PALETTE_EFFECT_ALL, allPalFXHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_ADD_ANGLE, angleAddHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_DRAW_ANGLE, angleDrawHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_MUL_ANGLE, angleMulHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_SET_ANGLE, angleSetHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_APPEND_TO_CLIPBOARD, appendToClipboardHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_ASSERT_SPECIAL, assertSpecialHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_SET_ATTACK_DISTANCE, attackDistHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_SET_ATTACK_MULTIPLIER, attackMulSetHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_PALETTE_EFFECT_BACKGROUND, bgPalFXHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_BIND_TO_PARENT, bindToParentHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_BIND_TO_ROOT, bindToRootHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_BIND_TO_TARGET, bindToTargetHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_CHANGE_ANIMATION, changeAnimHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_CHANGE_ANIMATION_2, changeAnim2HandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_CHANGE_STATE, changeStateHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_CLEAR_CLIPBOARD, clearClipboardHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_SET_CONTROL, ctrlSetHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_SET_DEFENSE_MULTIPLIER, defenceMulSetHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_DESTROY_SELF, destroySelfHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_DISPLAY_TO_CLIPBOARD, displayToClipboardHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_ENVIRONMENT_COLOR, envColorHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_ENVIRONMENT_SHAKE, envShakeHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_EXPLOD, explodHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_EXPLOD_BIND_TIME, explodBindTimeHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_FORCE_FEEDBACK, forceFeedbackHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_FALL_ENVIRONMENT_SHAKE, fallEnvShakeHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_MAKE_GAME_ANIMATION, gameMakeAnimHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_GRAVITY, gravityHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_HELPER, helperHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_ADD_HIT, hitAddHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_HIT_BY, hitByHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_HIT_DEFINITION, hitDefHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_HIT_FALL_DAMAGE, hitFallDamageHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_SET_HIT_FALL, hitFallSetHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_HIT_FALL_VELOCITY, hitFallVelHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_HIT_OVERRIDE, hitOverrideHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_SET_HIT_VELOCITY, hitVelSetHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_ADD_LIFE, lifeAddHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_SET_LIFE, lifeSetHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_MAKE_DUST, makeDustHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_MODIFY_EXPLOD, modifyExplodHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_RESET_MOVE_HIT, moveHitResetHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_NOT_HIT_BY, notHitByHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_NULL, nullHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_SET_OFFSET, offsetHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_PALETTE_EFFECT, palFXHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_PARENT_ADD_VARIABLE, parentVarAddHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_SET_PARENT_VARIABLE, parentVarSetHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_PAUSE, pauseHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_PLAYER_PUSH, playerPushHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_PLAY_SOUND, playSndHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_ADD_POSITION, posAddHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_FREEZE_POSITION, posFreezeHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_SET_POSITION, posSetHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_ADD_POWER, powerAddHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_SET_POWER, powerSetHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_PROJECTILE, projectileHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_REMAP_PALETTE, remapPalHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_REMOVE_EXPLOD, removeExplodHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_REVERSAL_DEFINITION, reversalDefHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_SCREEN_BOUND, screenBoundHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_SET_SELF_STATE, selfStateHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_SPRITE_PRIORITY, sprPriorityHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_SET_STATE_TYPE, stateTypeSetHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_PAN_SOUND, sndPanHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_STOP_SOUND, stopSndHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_SUPER_PAUSE, superPauseHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_BIND_TARGET, targetBindHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_DROP_TARGET, targetDropHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_SET_TARGET_FACING, targetFacingHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_ADD_TARGET_LIFE, targetLifeAddHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_ADD_TARGET_POWER, targetPowerAddHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_SET_TARGET_STATE, targetStateHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_TARGET_ADD_VELOCITY, targetVelAddHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_TARGET_SET_VELOCITY, targetVelSetHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_TRANSPARENCY, transHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_TURN, turnHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_ADD_VARIABLE, varAddHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_SET_VARIABLE_RANDOM, varRandomHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_SET_VARIABLE_RANGE, varRangeSetHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_SET_VARIABLE, varSetHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_ADD_VELOCITY, velAddHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_MULTIPLY_VELOCITY, velMulHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_SET_VELOCITY, velSetHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_VICTORY_QUOTE, victoryQuoteHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STATE_CONTROLLER_TYPE_WIDTH, widthHandleFunction);

}

void afterImageParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseAfterImageController(tController, tGroup);}
void afterImageTimeParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseAfterImageTimeController(tController, tGroup); }
void allPalFXParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parsePalFXController(tController, tGroup, MUGEN_STATE_CONTROLLER_TYPE_PALETTE_EFFECT_ALL); }
void angleAddParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseSingleRequiredValueController(tController, tGroup, MUGEN_STATE_CONTROLLER_TYPE_ADD_ANGLE); }
void angleDrawParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseAngleDrawController(tController, tGroup); }
void angleMulParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseSingleRequiredValueController(tController, tGroup, MUGEN_STATE_CONTROLLER_TYPE_MUL_ANGLE); }
void angleSetParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseSingleRequiredValueController(tController, tGroup, MUGEN_STATE_CONTROLLER_TYPE_SET_ANGLE); }
void appendToClipboardParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseClipboardController(tController, tGroup, MUGEN_STATE_CONTROLLER_TYPE_DISPLAY_TO_CLIPBOARD); }
void assertSpecialParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseSpecialAssertController(tController, tGroup); }
void attackDistParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseSingleRequiredValueController(tController, tGroup, MUGEN_STATE_CONTROLLER_TYPE_SET_ATTACK_DISTANCE); }
void attackMulSetParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseSingleRequiredValueController(tController, tGroup, MUGEN_STATE_CONTROLLER_TYPE_SET_ATTACK_MULTIPLIER); }
void bgPalFXParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parsePalFXController(tController, tGroup, MUGEN_STATE_CONTROLLER_TYPE_PALETTE_EFFECT_BACKGROUND); }
void bindToParentParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseBindController(tController, tGroup, MUGEN_STATE_CONTROLLER_TYPE_BIND_TO_PARENT); }
void bindToRootParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseBindController(tController, tGroup, MUGEN_STATE_CONTROLLER_TYPE_BIND_TO_ROOT); }
void bindToTargetParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseBindController(tController, tGroup, MUGEN_STATE_CONTROLLER_TYPE_BIND_TO_TARGET); }
void changeAnimParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseChangeAnimationController(tController, tGroup, MUGEN_STATE_CONTROLLER_TYPE_CHANGE_ANIMATION); }
void changeAnim2ParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseChangeAnimationController(tController, tGroup, MUGEN_STATE_CONTROLLER_TYPE_CHANGE_ANIMATION_2); }
void changeStateParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseChangeStateController(tController, tGroup, MUGEN_STATE_CONTROLLER_TYPE_CHANGE_STATE); }
void clearClipboardParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseNullController(tController, MUGEN_STATE_CONTROLLER_TYPE_CLEAR_CLIPBOARD); }
void ctrlSetParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseControlSettingController(tController, tGroup); }
void defenceMulSetParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseDefenseMultiplierController(tController, tGroup); }
void destroySelfParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseDestroySelfController(tController, tGroup); }
void displayToClipboardParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseClipboardController(tController, tGroup, MUGEN_STATE_CONTROLLER_TYPE_DISPLAY_TO_CLIPBOARD); }
void envColorParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseEnvColorController(tController, tGroup); }
void envShakeParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseEnvironmentShakeController(tController, tGroup); }
void explodParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseExplodController(tController, tGroup); }
void explodBindTimeParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseExplodBindTimeController(tController, tGroup); }
void forceFeedbackParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseForceFeedbackController(tController, tGroup); }
void fallEnvShakeParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseNullController(tController, MUGEN_STATE_CONTROLLER_TYPE_FALL_ENVIRONMENT_SHAKE); }
void gameMakeAnimParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseGameMakeAnimController(tController, tGroup); }
void gravityParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseNullController(tController, MUGEN_STATE_CONTROLLER_TYPE_GRAVITY); }
void helperParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseHelperController(tController, tGroup); }
void hitAddParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseSingleRequiredValueController(tController, tGroup, MUGEN_STATE_CONTROLLER_TYPE_ADD_HIT); }
void hitByParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseNotHitByController(tController, tGroup, MUGEN_STATE_CONTROLLER_TYPE_HIT_BY); }
void hitDefParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseHitDefinitionController(tController, tGroup); }
void hitFallDamageParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseNullController(tController, MUGEN_STATE_CONTROLLER_TYPE_HIT_FALL_DAMAGE); }
void hitFallSetParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseHitFallSetController(tController, tGroup); }
void hitFallVelParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseNullController(tController, MUGEN_STATE_CONTROLLER_TYPE_HIT_FALL_VELOCITY); }
void hitOverrideParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseHitOverrideController(tController, tGroup); }
void hitVelSetParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parse2DPhysicsController(tController, tGroup, MUGEN_STATE_CONTROLLER_TYPE_SET_HIT_VELOCITY); }
void lifeAddParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseLifeAddController(tController, tGroup); }
void lifeSetParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseSingleRequiredValueController(tController, tGroup, MUGEN_STATE_CONTROLLER_TYPE_SET_LIFE); }
void makeDustParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseMakeDustController(tController, tGroup); }
void modifyExplodParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseModifyExplodController(tController, tGroup); }
void moveHitResetParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseNullController(tController, MUGEN_STATE_CONTROLLER_TYPE_RESET_MOVE_HIT); }
void notHitByParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseNotHitByController(tController, tGroup, MUGEN_STATE_CONTROLLER_TYPE_NOT_HIT_BY); }
void nullParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseNullController(tController, MUGEN_STATE_CONTROLLER_TYPE_NULL); }
void offsetParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parse2DPhysicsController(tController, tGroup, MUGEN_STATE_CONTROLLER_TYPE_SET_OFFSET); }
void palFXParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parsePalFXController(tController, tGroup, MUGEN_STATE_CONTROLLER_TYPE_PALETTE_EFFECT); }
void parentVarAddParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseVarSetController(tController, tGroup, MUGEN_STATE_CONTROLLER_TYPE_PARENT_ADD_VARIABLE); }
void parentVarSetParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseVarSetController(tController, tGroup, MUGEN_STATE_CONTROLLER_TYPE_SET_PARENT_VARIABLE); }
void pauseParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parsePauseController(tController, tGroup); }
void playerPushParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseSingleRequiredValueController(tController, tGroup, MUGEN_STATE_CONTROLLER_TYPE_PLAYER_PUSH); }
void playSndParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parsePlaySoundController(tController, tGroup); }
void posAddParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parse2DPhysicsController(tController, tGroup, MUGEN_STATE_CONTROLLER_TYPE_ADD_POSITION); }
void posFreezeParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parsePositionFreezeController(tController, tGroup); }
void posSetParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parse2DPhysicsController(tController, tGroup, MUGEN_STATE_CONTROLLER_TYPE_SET_POSITION); }
void powerAddParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseSingleRequiredValueController(tController, tGroup, MUGEN_STATE_CONTROLLER_TYPE_ADD_POWER); }
void powerSetParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseSingleRequiredValueController(tController, tGroup, MUGEN_STATE_CONTROLLER_TYPE_SET_POWER); }
void projectileParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseProjectileController(tController, tGroup); }
void remapPalParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseRemapPaletteController(tController, tGroup); }
void removeExplodParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseRemoveExplodController(tController, tGroup); }
void reversalDefParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseReversalDefinitionController(tController, tGroup); }
void screenBoundParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseScreenBoundController(tController, tGroup); }
void selfStateParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseChangeStateController(tController, tGroup, MUGEN_STATE_CONTROLLER_TYPE_SET_SELF_STATE); }
void sprPriorityParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseSpritePriorityController(tController, tGroup); }
void stateTypeSetParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseStateTypeSetController(tController, tGroup); }
void sndPanParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseSoundPanController(tController, tGroup); }
void stopSndParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseStopSoundController(tController, tGroup); }
void superPauseParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseSuperPauseController(tController, tGroup); }
void targetBindParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseBindController(tController, tGroup, MUGEN_STATE_CONTROLLER_TYPE_BIND_TARGET); }
void targetDropParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseTargetDropController(tController, tGroup); }
void targetFacingParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseSetTargetFacingController(tController, tGroup); }
void targetLifeAddParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseTargetLifeAddController(tController, tGroup); }
void targetPowerAddParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseTargetPowerAddController(tController, tGroup); }
void targetStateParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseTargetChangeStateController(tController, tGroup); }
void targetVelAddParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseTarget2DPhysicsController(tController, tGroup, MUGEN_STATE_CONTROLLER_TYPE_TARGET_ADD_VELOCITY); }
void targetVelSetParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseTarget2DPhysicsController(tController, tGroup, MUGEN_STATE_CONTROLLER_TYPE_TARGET_SET_VELOCITY); }
void transParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseTransparencyController(tController, tGroup); }
void turnParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseNullController(tController, MUGEN_STATE_CONTROLLER_TYPE_TURN); }
void varAddParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseVarSetController(tController, tGroup, MUGEN_STATE_CONTROLLER_TYPE_ADD_VARIABLE); }
void varRandomParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseVarRandomController(tController, tGroup); }
void varRangeSetParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseVarRangeSetController(tController, tGroup); }
void varSetParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseVarSetController(tController, tGroup, MUGEN_STATE_CONTROLLER_TYPE_SET_VARIABLE); }
void velAddParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parse2DPhysicsController(tController, tGroup, MUGEN_STATE_CONTROLLER_TYPE_ADD_VELOCITY); }
void velMulParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parse2DPhysicsController(tController, tGroup, MUGEN_STATE_CONTROLLER_TYPE_MULTIPLY_VELOCITY); }
void velSetParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parse2DPhysicsController(tController, tGroup, MUGEN_STATE_CONTROLLER_TYPE_SET_VELOCITY); }
void victoryQuoteParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseVictoryQuoteController(tController, tGroup); }
void widthParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseWidthController(tController, tGroup); }


static void setupStateControllerParsers() {
	gVariableHandler.mStateControllerParsers = new_string_map();

	string_map_push(&gVariableHandler.mStateControllerParsers, "afterimage", afterImageParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "afterimagetime", afterImageTimeParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "allpalfx", allPalFXParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "angleadd", angleAddParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "angledraw", angleDrawParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "anglemul", angleMulParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "angleset", angleSetParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "appendtoclipboard", appendToClipboardParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "assertspecial", assertSpecialParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "attackdist", attackDistParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "attackmulset", attackMulSetParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "bgpalfx", bgPalFXParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "bindtoparent", bindToParentParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "bindtoroot", bindToRootParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "bindtotarget", bindToTargetParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "changeanim", changeAnimParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "changeanim2", changeAnim2ParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "changestate", changeStateParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "clearclipboard", clearClipboardParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "ctrlset", ctrlSetParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "defencemulset", defenceMulSetParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "destroyself", destroySelfParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "displaytoclipboard", displayToClipboardParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "envcolor", envColorParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "envshake", envShakeParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "explod", explodParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "explodbindtime", explodBindTimeParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "forcefeedback", forceFeedbackParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "fallenvshake", fallEnvShakeParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "gamemakeanim", gameMakeAnimParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "gravity", gravityParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "helper", helperParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "hitadd", hitAddParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "hitby", hitByParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "hitdef", hitDefParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "hitfalldamage", hitFallDamageParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "hitfallset", hitFallSetParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "hitfallvel", hitFallVelParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "hitoverride", hitOverrideParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "hitvelset", hitVelSetParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "lifeadd", lifeAddParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "lifeset", lifeSetParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "makedust", makeDustParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "modifyexplod", modifyExplodParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "movehitreset", moveHitResetParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "nothitby", notHitByParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "null", nullParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "offset", offsetParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "palfx", palFXParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "parentvaradd", parentVarAddParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "parentvarset", parentVarSetParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "pause", pauseParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "playerpush", playerPushParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "playsnd", playSndParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "posadd", posAddParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "posfreeze", posFreezeParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "posset", posSetParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "poweradd", powerAddParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "powerset", powerSetParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "projectile", projectileParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "remappal", remapPalParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "removeexplod", removeExplodParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "reversaldef", reversalDefParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "screenbound", screenBoundParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "selfstate", selfStateParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "sprpriority", sprPriorityParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "statetypeset", stateTypeSetParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "sndpan", sndPanParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "stopsnd", stopSndParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "superpause", superPauseParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "targetbind", targetBindParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "targetdrop", targetDropParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "targetfacing", targetFacingParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "targetlifeadd", targetLifeAddParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "targetpoweradd", targetPowerAddParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "targetstate", targetStateParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "targetveladd", targetVelAddParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "targetvelset", targetVelSetParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "trans", transParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "turn", turnParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "varadd", varAddParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "varrandom", varRandomParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "varrangeset", varRangeSetParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "varset", varSetParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "veladd", velAddParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "velmul", velMulParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "velset", velSetParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "victoryquote", victoryQuoteParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "width", widthParseFunction);
}

void afterImageUnloadFunction(DreamMugenStateController* tController) { unloadAfterImageController(tController); }
void afterImageTimeUnloadFunction(DreamMugenStateController* tController) { unloadAfterImageTimeController(tController); }
void allPalFXUnloadFunction(DreamMugenStateController* tController) { unloadPalFXController(tController); }
void angleAddUnloadFunction(DreamMugenStateController* tController) { unloadSingleRequiredValueController(tController); }
void angleDrawUnloadFunction(DreamMugenStateController* tController) { unloadAngleDrawController(tController); }
void angleMulUnloadFunction(DreamMugenStateController* tController) { unloadSingleRequiredValueController(tController); }
void angleSetUnloadFunction(DreamMugenStateController* tController) { unloadSingleRequiredValueController(tController); }
void appendToClipboardUnloadFunction(DreamMugenStateController* tController) { unloadClipboardController(tController); }
void assertSpecialUnloadFunction(DreamMugenStateController* tController) { unloadSpecialAssertController(tController); }
void attackDistUnloadFunction(DreamMugenStateController* tController) { unloadSingleRequiredValueController(tController); }
void attackMulSetUnloadFunction(DreamMugenStateController* tController) { unloadSingleRequiredValueController(tController); }
void bgPalFXUnloadFunction(DreamMugenStateController* tController) { unloadPalFXController(tController); }
void bindToParentUnloadFunction(DreamMugenStateController* tController) { unloadBindController(tController); }
void bindToRootUnloadFunction(DreamMugenStateController* tController) { unloadBindController(tController); }
void bindToTargetUnloadFunction(DreamMugenStateController* tController) { unloadBindController(tController); }
void changeAnimUnloadFunction(DreamMugenStateController* tController) { unloadChangeAnimationController(tController); }
void changeAnim2UnloadFunction(DreamMugenStateController* tController) { unloadChangeAnimationController(tController); }
void changeStateUnloadFunction(DreamMugenStateController* tController) { unloadChangeStateController(tController); }
void clearClipboardUnloadFunction(DreamMugenStateController* tController) { unloadNullController(tController); }
void ctrlSetUnloadFunction(DreamMugenStateController* tController) { unloadControlSettingController(tController); }
void defenceMulSetUnloadFunction(DreamMugenStateController* tController) { unloadDefenseMultiplierController(tController); }
void destroySelfUnloadFunction(DreamMugenStateController* tController) { unloadDestroySelfController(tController); }
void displayToClipboardUnloadFunction(DreamMugenStateController* tController) { unloadClipboardController(tController); }
void envColorUnloadFunction(DreamMugenStateController* tController) { unloadEnvColorController(tController); }
void envShakeUnloadFunction(DreamMugenStateController* tController) { unloadEnvironmentShakeController(tController); }
void explodUnloadFunction(DreamMugenStateController* tController) { unloadExplodController(tController); }
void explodBindTimeUnloadFunction(DreamMugenStateController* tController) { unloadExplodBindTimeController(tController); }
void forceFeedbackUnloadFunction(DreamMugenStateController* tController) { unloadForceFeedbackController(tController); }
void fallEnvShakeUnloadFunction(DreamMugenStateController* tController) { unloadNullController(tController); }
void gameMakeAnimUnloadFunction(DreamMugenStateController* tController) { unloadGameMakeAnimController(tController); }
void gravityUnloadFunction(DreamMugenStateController* tController) { unloadNullController(tController); }
void helperUnloadFunction(DreamMugenStateController* tController) { unloadHelperController(tController); }
void hitAddUnloadFunction(DreamMugenStateController* tController) { unloadSingleRequiredValueController(tController); }
void hitByUnloadFunction(DreamMugenStateController* tController) { unloadNotHitByController(tController); }
void hitDefUnloadFunction(DreamMugenStateController* tController) { unloadHitDefinitionController(tController); }
void hitFallDamageUnloadFunction(DreamMugenStateController* tController) { unloadNullController(tController); }
void hitFallSetUnloadFunction(DreamMugenStateController* tController) { unloadHitFallSetController(tController); }
void hitFallVelUnloadFunction(DreamMugenStateController* tController) { unloadNullController(tController); }
void hitOverrideUnloadFunction(DreamMugenStateController* tController) { unloadHitOverrideController(tController); }
void hitVelSetUnloadFunction(DreamMugenStateController* tController) { unload2DPhysicsController(tController); }
void lifeAddUnloadFunction(DreamMugenStateController* tController) { unloadLifeAddController(tController); }
void lifeSetUnloadFunction(DreamMugenStateController* tController) { unloadSingleRequiredValueController(tController); }
void makeDustUnloadFunction(DreamMugenStateController* tController) { unloadMakeDustController(tController); }
void modifyExplodUnloadFunction(DreamMugenStateController* tController) { unloadModifyExplodController(tController); }
void moveHitResetUnloadFunction(DreamMugenStateController* tController) { unloadNullController(tController); }
void notHitByUnloadFunction(DreamMugenStateController* tController) { unloadNotHitByController(tController); }
void nullUnloadFunction(DreamMugenStateController* tController) { unloadNullController(tController); }
void offsetUnloadFunction(DreamMugenStateController* tController) { unload2DPhysicsController(tController); }
void palFXUnloadFunction(DreamMugenStateController* tController) { unloadPalFXController(tController); }
void parentVarAddUnloadFunction(DreamMugenStateController* tController) { unloadVarSetController(tController); }
void parentVarSetUnloadFunction(DreamMugenStateController* tController) { unloadVarSetController(tController); }
void pauseUnloadFunction(DreamMugenStateController* tController) { unloadPauseController(tController); }
void playerPushUnloadFunction(DreamMugenStateController* tController) { unloadSingleRequiredValueController(tController); }
void playSndUnloadFunction(DreamMugenStateController* tController) { unloadPlaySoundController(tController); }
void posAddUnloadFunction(DreamMugenStateController* tController) { unload2DPhysicsController(tController); }
void posFreezeUnloadFunction(DreamMugenStateController* tController) { unloadPositionFreezeController(tController); }
void posSetUnloadFunction(DreamMugenStateController* tController) { unload2DPhysicsController(tController); }
void powerAddUnloadFunction(DreamMugenStateController* tController) { unloadSingleRequiredValueController(tController); }
void powerSetUnloadFunction(DreamMugenStateController* tController) { unloadSingleRequiredValueController(tController); }
void projectileUnloadFunction(DreamMugenStateController* tController) { unloadProjectileController(tController); }
void remapPalUnloadFunction(DreamMugenStateController* tController) { unloadRemapPaletteController(tController); }
void removeExplodUnloadFunction(DreamMugenStateController* tController) { unloadRemoveExplodController(tController); }
void reversalDefUnloadFunction(DreamMugenStateController* tController) { unloadReversalDefinitionController(tController); }
void screenBoundUnloadFunction(DreamMugenStateController* tController) { unloadScreenBoundController(tController); }
void selfStateUnloadFunction(DreamMugenStateController* tController) { unloadChangeStateController(tController); }
void sprPriorityUnloadFunction(DreamMugenStateController* tController) { unloadSpritePriorityController(tController); }
void stateTypeSetUnloadFunction(DreamMugenStateController* tController) { unloadStateTypeSetController(tController); }
void sndPanUnloadFunction(DreamMugenStateController* tController) { unloadSoundPanController(tController); }
void stopSndUnloadFunction(DreamMugenStateController* tController) { unloadStopSoundController(tController); }
void superPauseUnloadFunction(DreamMugenStateController* tController) { unloadSuperPauseController(tController); }
void targetBindUnloadFunction(DreamMugenStateController* tController) { unloadBindController(tController); }
void targetDropUnloadFunction(DreamMugenStateController* tController) { unloadTargetDropController(tController); }
void targetFacingUnloadFunction(DreamMugenStateController* tController) { unloadSetTargetFacingController(tController); }
void targetLifeAddUnloadFunction(DreamMugenStateController* tController) { unloadTargetLifeAddController(tController); }
void targetPowerAddUnloadFunction(DreamMugenStateController* tController) { unloadTargetPowerAddController(tController); }
void targetStateUnloadFunction(DreamMugenStateController* tController) { unloadTargetChangeStateController(tController); }
void targetVelAddUnloadFunction(DreamMugenStateController* tController) { unloadTarget2DPhysicsController(tController); }
void targetVelSetUnloadFunction(DreamMugenStateController* tController) { unloadTarget2DPhysicsController(tController); }
void transUnloadFunction(DreamMugenStateController* tController) { unloadTransparencyController(tController); }
void turnUnloadFunction(DreamMugenStateController* tController) { unloadNullController(tController); }
void varAddUnloadFunction(DreamMugenStateController* tController) { unloadVarSetController(tController); }
void varRandomUnloadFunction(DreamMugenStateController* tController) { unloadVarRandomController(tController); }
void varRangeSetUnloadFunction(DreamMugenStateController* tController) { unloadVarRangeSetController(tController); }
void varSetUnloadFunction(DreamMugenStateController* tController) { unloadVarSetController(tController); }
void velAddUnloadFunction(DreamMugenStateController* tController) { unload2DPhysicsController(tController); }
void velMulUnloadFunction(DreamMugenStateController* tController) { unload2DPhysicsController(tController); }
void velSetUnloadFunction(DreamMugenStateController* tController) { unload2DPhysicsController(tController); }
void victoryQuoteUnloadFunction(DreamMugenStateController* tController) { unloadVictoryQuoteController(tController); }
void widthUnloadFunction(DreamMugenStateController* tController) { unloadWidthController(tController); }

static void setupStateControllerUnloaders() {
	gVariableHandler.mStateControllerUnloaders = new_int_map();

	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_AFTER_IMAGE, afterImageUnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_AFTER_IMAGE_TIME, afterImageTimeUnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_PALETTE_EFFECT_ALL, allPalFXUnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_ADD_ANGLE, angleAddUnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_DRAW_ANGLE, angleDrawUnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_MUL_ANGLE, angleMulUnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_SET_ANGLE, angleSetUnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_APPEND_TO_CLIPBOARD, appendToClipboardUnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_ASSERT_SPECIAL, assertSpecialUnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_SET_ATTACK_DISTANCE, attackDistUnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_SET_ATTACK_MULTIPLIER, attackMulSetUnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_PALETTE_EFFECT_BACKGROUND, bgPalFXUnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_BIND_TO_PARENT, bindToParentUnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_BIND_TO_ROOT, bindToRootUnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_BIND_TO_TARGET, bindToTargetUnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_CHANGE_ANIMATION, changeAnimUnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_CHANGE_ANIMATION_2, changeAnim2UnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_CHANGE_STATE, changeStateUnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_CLEAR_CLIPBOARD, clearClipboardUnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_SET_CONTROL, ctrlSetUnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_SET_DEFENSE_MULTIPLIER, defenceMulSetUnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_DESTROY_SELF, destroySelfUnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_DISPLAY_TO_CLIPBOARD, displayToClipboardUnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_ENVIRONMENT_COLOR, envColorUnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_ENVIRONMENT_SHAKE, envShakeUnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_EXPLOD, explodUnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_EXPLOD_BIND_TIME, explodBindTimeUnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_FORCE_FEEDBACK, forceFeedbackUnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_FALL_ENVIRONMENT_SHAKE, fallEnvShakeUnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_MAKE_GAME_ANIMATION, gameMakeAnimUnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_GRAVITY, gravityUnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_HELPER, helperUnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_ADD_HIT, hitAddUnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_HIT_BY, hitByUnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_HIT_DEFINITION, hitDefUnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_HIT_FALL_DAMAGE, hitFallDamageUnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_SET_HIT_FALL, hitFallSetUnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_HIT_FALL_VELOCITY, hitFallVelUnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_HIT_OVERRIDE, hitOverrideUnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_SET_HIT_VELOCITY, hitVelSetUnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_ADD_LIFE, lifeAddUnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_SET_LIFE, lifeSetUnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_MAKE_DUST, makeDustUnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_MODIFY_EXPLOD, modifyExplodUnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_RESET_MOVE_HIT, moveHitResetUnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_NOT_HIT_BY, notHitByUnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_NULL, nullUnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_SET_OFFSET, offsetUnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_PALETTE_EFFECT, palFXUnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_PARENT_ADD_VARIABLE, parentVarAddUnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_SET_PARENT_VARIABLE, parentVarSetUnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_PAUSE, pauseUnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_PLAYER_PUSH, playerPushUnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_PLAY_SOUND, playSndUnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_ADD_POSITION, posAddUnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_FREEZE_POSITION, posFreezeUnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_SET_POSITION, posSetUnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_ADD_POWER, powerAddUnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_SET_POWER, powerSetUnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_PROJECTILE, projectileUnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_REMAP_PALETTE, remapPalUnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_REMOVE_EXPLOD, removeExplodUnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_REVERSAL_DEFINITION, reversalDefUnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_SCREEN_BOUND, screenBoundUnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_SET_SELF_STATE, selfStateUnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_SPRITE_PRIORITY, sprPriorityUnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_SET_STATE_TYPE, stateTypeSetUnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_PAN_SOUND, sndPanUnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_STOP_SOUND, stopSndUnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_SUPER_PAUSE, superPauseUnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_BIND_TARGET, targetBindUnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_DROP_TARGET, targetDropUnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_SET_TARGET_FACING, targetFacingUnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_ADD_TARGET_LIFE, targetLifeAddUnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_ADD_TARGET_POWER, targetPowerAddUnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_SET_TARGET_STATE, targetStateUnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_TARGET_ADD_VELOCITY, targetVelAddUnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_TARGET_SET_VELOCITY, targetVelSetUnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_TRANSPARENCY, transUnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_TURN, turnUnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_ADD_VARIABLE, varAddUnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_SET_VARIABLE_RANDOM, varRandomUnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_SET_VARIABLE_RANGE, varRangeSetUnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_SET_VARIABLE, varSetUnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_ADD_VELOCITY, velAddUnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_MULTIPLY_VELOCITY, velMulUnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_SET_VELOCITY, velSetUnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_VICTORY_QUOTE, victoryQuoteUnloadFunction);
	int_map_push(&gVariableHandler.mStateControllerUnloaders, MUGEN_STATE_CONTROLLER_TYPE_WIDTH, widthUnloadFunction);
}

void setupDreamMugenStateControllerHandler(MemoryStack* tMemoryStack) {
	setupStateControllerParsers();
	setupStateControllerHandlers();
	setupStateControllerUnloaders();
	gVariableHandler.mMemoryStack = tMemoryStack;
}


typedef struct {
	DreamMugenAssignment* mID;

	DreamMugenAssignment* mAnimation;
	DreamMugenAssignment* mIsLooping;
	DreamMugenAssignment* mPosition;
	DreamMugenAssignment* mIsBoundToStage;

	int mHasShadow;
	DreamMugenAssignment* mShadowBasePositionY;

} CreateAnimationStoryController;

static void parseCreateAnimationStoryController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) {
	CreateAnimationStoryController* e = allocMemoryOnMemoryStackOrMemory(sizeof(CreateAnimationStoryController));

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("id", tGroup, &e->mID, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("anim", tGroup, &e->mAnimation, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("loop", tGroup, &e->mIsLooping, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("pos", tGroup, &e->mPosition, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("stage", tGroup, &e->mIsBoundToStage, "");
	e->mHasShadow = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("shadow", tGroup, &e->mShadowBasePositionY);

	tController->mType = MUGEN_STORY_STATE_CONTROLLER_TYPE_CREATE_ANIMATION;
	tController->mData = e;
}

typedef struct {
	DreamMugenAssignment* mID;
} RemoveAnimationStoryController;

static void parseRemoveAnimationStoryController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) {
	RemoveAnimationStoryController* e = allocMemoryOnMemoryStackOrMemory(sizeof(RemoveAnimationStoryController));

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("id", tGroup, &e->mID, "");

	tController->mType = MUGEN_STORY_STATE_CONTROLLER_TYPE_REMOVE_ANIMATION;
	tController->mData = e;
}


typedef struct {
	DreamMugenAssignment* mID;

	DreamMugenAssignment* mAnimation;
} ChangeAnimationStoryController;

static void parseChangeAnimationStoryController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) {
	ChangeAnimationStoryController* e = allocMemoryOnMemoryStackOrMemory(sizeof(ChangeAnimationStoryController));

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("id", tGroup, &e->mID, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("anim", tGroup, &e->mAnimation, "");

	tController->mType = MUGEN_STORY_STATE_CONTROLLER_TYPE_CHANGE_ANIMATION;
	tController->mData = e;

}

typedef struct {
	DreamMugenAssignment* mID;
	DreamMugenAssignment* mPosition;

	DreamMugenAssignment* mTextOffset;

	int mHasBackgroundSprite;
	DreamMugenAssignment* mBackgroundSprite;
	DreamMugenAssignment* mBackgroundOffset;

	int mHasFaceSprite;
	DreamMugenAssignment* mFaceSprite;
	DreamMugenAssignment* mFaceOffset;

	DreamMugenAssignment* mText;
	DreamMugenAssignment* mFont;
	DreamMugenAssignment* mWidth;

	int mHasNextState;
	DreamMugenAssignment* mNextState;
} CreateTextStoryController;

static void parseCreateTextStoryController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) {
	CreateTextStoryController* e = allocMemoryOnMemoryStackOrMemory(sizeof(CreateTextStoryController));

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("id", tGroup, &e->mID, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("pos", tGroup, &e->mPosition, "");

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("text.offset", tGroup, &e->mTextOffset, "");


	e->mHasBackgroundSprite = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("bg.spr", tGroup, &e->mBackgroundSprite);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("bg.offset", tGroup, &e->mBackgroundOffset, "");

	e->mHasFaceSprite = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("face.spr", tGroup, &e->mFaceSprite);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("face.offset", tGroup, &e->mFaceOffset, "");

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("text", tGroup, &e->mText, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("font", tGroup, &e->mFont, "1 , 0 , 0");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("width", tGroup, &e->mWidth, "");

	e->mHasNextState = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("nextstate", tGroup, &e->mNextState);

	tController->mType = MUGEN_STORY_STATE_CONTROLLER_TYPE_CREATE_TEXT;
	tController->mData = e;
}

typedef struct {
	DreamMugenAssignment* mID;
} RemoveElementStoryController;

static void parseRemoveElementStoryController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup, DreamMugenStateControllerType tType) {
	RemoveElementStoryController* e = allocMemoryOnMemoryStackOrMemory(sizeof(RemoveElementStoryController));

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("id", tGroup, &e->mID, "");
	
	tController->mType = tType;
	tController->mData = e;
}

typedef struct {
	DreamMugenAssignment* mID;

	int mDoesChangePosition;
	DreamMugenAssignment* mPosition;

	int mDoesChangeBackgroundSprite;
	DreamMugenAssignment* mBackgroundSprite;
	int mDoesChangeBackgroundOffset;
	DreamMugenAssignment* mBackgroundOffset;

	int mDoesChangeFaceSprite;
	DreamMugenAssignment* mFaceSprite;
	int mDoesChangeFaceOffset;
	DreamMugenAssignment* mFaceOffset;

	int mDoesChangeText;
	DreamMugenAssignment* mText;

	int mDoesChangeNextState;
	DreamMugenAssignment* mNextState;
} ChangeTextStoryController;

static void parseChangeTextStoryController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) {
	ChangeTextStoryController* e = allocMemoryOnMemoryStackOrMemory(sizeof(ChangeTextStoryController));

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("id", tGroup, &e->mID, "");
	e->mDoesChangePosition = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("pos", tGroup, &e->mPosition);

	e->mDoesChangeBackgroundSprite = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("bg.spr", tGroup, &e->mBackgroundSprite);
	e->mDoesChangeBackgroundOffset = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("bg.offset", tGroup, &e->mBackgroundOffset);

	e->mDoesChangeFaceSprite = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("face.spr", tGroup, &e->mFaceSprite);
	e->mDoesChangeFaceOffset = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("face.offset", tGroup, &e->mFaceOffset);

	e->mDoesChangeText = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("text", tGroup, &e->mText);

	e->mDoesChangeNextState = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("nextstate", tGroup, &e->mNextState);

	tController->mType = MUGEN_STORY_STATE_CONTROLLER_TYPE_CHANGE_TEXT;
	tController->mData = e;
}

typedef struct {
	DreamMugenAssignment* mDuration;

} FadeStoryController;

static void parseFadeStoryController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup, DreamMugenStateControllerType tType) {
	FadeStoryController* e = allocMemoryOnMemoryStackOrMemory(sizeof(FadeStoryController));

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("duration", tGroup, &e->mDuration, "");

	tController->mType = tType;
	tController->mData = e;
}

typedef struct {
	DreamMugenAssignment* mID;
	DreamMugenAssignment* mFacing;

} AnimationSetFaceDirectionStoryController;

static void parseAnimationSetFaceDirectionStoryController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) {
	AnimationSetFaceDirectionStoryController* e = allocMemoryOnMemoryStackOrMemory(sizeof(AnimationSetFaceDirectionStoryController));

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("id", tGroup, &e->mID, "");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("facing", tGroup, &e->mFacing, "");

	tController->mType = MUGEN_STORY_STATE_CONTROLLER_TYPE_ANIMATION_SET_FACEDIRECTION;
	tController->mData = e;
}

void nullStoryParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseNullController(tController, MUGEN_STORY_STATE_CONTROLLER_TYPE_NULL); }
void createAnimationStoryParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseCreateAnimationStoryController(tController, tGroup); }
void removeAnimationStoryParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseRemoveAnimationStoryController(tController, tGroup); }
void changeAnimationStoryParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseChangeAnimationStoryController(tController, tGroup); }
void createTextStoryParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseCreateTextStoryController(tController, tGroup); }
void removeTextStoryParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseRemoveElementStoryController(tController, tGroup, MUGEN_STORY_STATE_CONTROLLER_TYPE_REMOVE_TEXT); }
void changeTextStoryParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseChangeTextStoryController(tController, tGroup); }
void changeStateStoryParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseSingleRequiredValueController(tController, tGroup, MUGEN_STORY_STATE_CONTROLLER_TYPE_CHANGE_STATE); }
void fadeInStoryParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseFadeStoryController(tController, tGroup, MUGEN_STORY_STATE_CONTROLLER_TYPE_FADE_IN); }
void fadeOutStoryParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseFadeStoryController(tController, tGroup, MUGEN_STORY_STATE_CONTROLLER_TYPE_FADE_OUT); }
void gotoStoryStepStoryParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseSingleRequiredValueController(tController, tGroup, MUGEN_STORY_STATE_CONTROLLER_TYPE_GOTO_STORY_STEP); }
void animationSetPositionStoryParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseTarget2DPhysicsController(tController, tGroup, MUGEN_STORY_STATE_CONTROLLER_TYPE_ANIMATION_SET_POSITION); }
void animationAddPositionStoryParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseTarget2DPhysicsController(tController, tGroup, MUGEN_STORY_STATE_CONTROLLER_TYPE_ANIMATION_ADD_POSITION); }
void animationSetScaleStoryParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseTarget2DPhysicsController(tController, tGroup, MUGEN_STORY_STATE_CONTROLLER_TYPE_ANIMATION_SET_SCALE); }
void animationSetFaceDirectionStoryParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseAnimationSetFaceDirectionStoryController(tController, tGroup); }
void endStoryboardStoryParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseSingleRequiredValueController(tController, tGroup, MUGEN_STORY_STATE_CONTROLLER_TYPE_END_STORYBOARD); }
void moveStageStoryParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parse2DPhysicsController(tController, tGroup, MUGEN_STORY_STATE_CONTROLLER_TYPE_MOVE_STAGE); }


static void setupStoryStateControllerParsers() {
	gVariableHandler.mStateControllerParsers = new_string_map();
	
	string_map_push(&gVariableHandler.mStateControllerParsers, "null", nullStoryParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "createanim", createAnimationStoryParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "removeanim", removeAnimationStoryParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "changeanim", changeAnimationStoryParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "createtext", createTextStoryParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "removetext", removeTextStoryParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "changetext", changeTextStoryParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "changestate", changeStateStoryParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "fadein", fadeInStoryParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "fadeout", fadeOutStoryParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "gotostorystep", gotoStoryStepStoryParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "animposset", animationSetPositionStoryParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "animposadd", animationAddPositionStoryParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "animscaleset", animationSetScaleStoryParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "animsetfacing", animationSetFaceDirectionStoryParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "endstoryboard", endStoryboardStoryParseFunction);
	string_map_push(&gVariableHandler.mStateControllerParsers, "movestage", moveStageStoryParseFunction);
}

static int handleCreateAnimationStoryController(DreamMugenStateController* tController) {
	CreateAnimationStoryController* e = tController->mData;

	int id, animation, isLooping, isBoundToStage;
	Position position = makePosition(0, 0, 0);


	getSingleIntegerValueOrDefault(&e->mID, NULL, &id, 1);
	getSingleIntegerValueOrDefault(&e->mAnimation, NULL, &animation, 0);
	getSingleIntegerValueOrDefault(&e->mIsLooping, NULL, &isLooping, 1);
	getSingleIntegerValueOrDefault(&e->mIsBoundToStage, NULL, &isBoundToStage, 0);
	getTwoFloatValuesWithDefaultValues(&e->mPosition, NULL, &position.x, &position.y, 0, 0);

	addDolmexicaStoryAnimation(id, animation, position);
	if (e->mHasShadow) {
		double shadowBasePosition;
		getSingleFloatValueOrDefault(&e->mShadowBasePositionY, NULL, &shadowBasePosition, 0);
		setDolmexicaStoryAnimationShadow(id, shadowBasePosition);
	}
	setDolmexicaStoryAnimationLooping(id, isLooping);
	setDolmexicaStoryAnimationBoundToStage(id, isBoundToStage);


	return 0;
}

static int handleRemoveAnimationStoryController(DreamMugenStateController* tController) {
	RemoveAnimationStoryController* e = tController->mData;

	int id;
	getSingleIntegerValueOrDefault(&e->mID, NULL, &id, 1);


	removeDolmexicaStoryAnimation(id);

	return 0;
}

static int handleChangeAnimationStoryController(DreamMugenStateController* tController) {
	ChangeAnimationStoryController* e = tController->mData;

	int id, animation;
	getSingleIntegerValueOrDefault(&e->mID, NULL, &id, 1);
	getSingleIntegerValueOrDefault(&e->mAnimation, NULL, &animation, 0);

	changeDolmexicaStoryAnimation(id, animation);

	return 0;
}

static void handleSingleStoryTextSprite(int id, int tHasSprite, DreamMugenAssignment** tSprite, DreamMugenAssignment** tOffset, void(*tFunc)(int, Vector3DI, Position)) {
	if (!tHasSprite) return;

	Vector3DI sprite = makeVector3DI(0, 0, 0);
	Position offset = makePosition(0, 0, 0);
	getTwoIntegerValuesWithDefaultValues(tSprite, NULL, &sprite.x, &sprite.y, 0, 0);
	getTwoFloatValuesWithDefaultValues(tOffset, NULL, &offset.x, &offset.y, 0, 0);
	tFunc(id, sprite, offset);
}

static int handleCreateTextStoryController(DreamMugenStateController* tController) {
	CreateTextStoryController* e = tController->mData;

	int id;
	Position basePosition = makePosition(0, 0, 0);
	Position textOffset = makePosition(0, 0, 0);


	getSingleIntegerValueOrDefault(&e->mID, NULL, &id, 1);
	getTwoFloatValuesWithDefaultValues(&e->mPosition, NULL, &basePosition.x, &basePosition.y, 0, 0);
	getTwoFloatValuesWithDefaultValues(&e->mTextOffset, NULL, &textOffset.x, &textOffset.y, 0, 0);

	double width;
	getSingleFloatValueOrDefault(&e->mWidth, NULL, &width, INF);
	Vector3DI font = evaluateDreamAssignmentAndReturnAsVector3DI(&e->mFont, NULL);
	char* text = evaluateDreamAssignmentAndReturnAsAllocatedString(&e->mText, NULL);
	addDolmexicaStoryText(id, text, font, basePosition, textOffset, width);
	freeMemory(text);

	handleSingleStoryTextSprite(id, e->mHasBackgroundSprite, &e->mBackgroundSprite, &e->mBackgroundOffset, setDolmexicaStoryTextBackground);
	handleSingleStoryTextSprite(id, e->mHasFaceSprite, &e->mFaceSprite, &e->mFaceOffset, setDolmexicaStoryTextFace);

	if (e->mHasNextState) {
		int nextState = evaluateDreamAssignmentAndReturnAsInteger(&e->mNextState, NULL);
		setDolmexicaStoryTextNextState(id, nextState);
	}

	return 0;
}

static int handleRemoveTextStoryController(DreamMugenStateController* tController) {
	RemoveElementStoryController* e = tController->mData;

	int id;
	getSingleIntegerValueOrDefault(&e->mID, NULL, &id, 1);
	removeDolmexicaStoryText(id);

	return 0;
}

static int handleChangeTextStoryController(DreamMugenStateController* tController) {
	ChangeTextStoryController* e = tController->mData;

	int id;
	getSingleIntegerValueOrDefault(&e->mID, NULL, &id, 1);

	if (e->mDoesChangePosition) {
		Position offset = evaluateDreamAssignmentAndReturnAsVector3D(&e->mPosition, NULL);
		setDolmexicaStoryTextBasePosition(id, offset);
	}
	if (e->mDoesChangeText) {
		char* text = evaluateDreamAssignmentAndReturnAsAllocatedString(&e->mText, NULL);
		setDolmexicaStoryTextText(id, text);
		freeMemory(text);
	}
	if (e->mDoesChangeBackgroundSprite) {
		Vector3DI sprite = evaluateDreamAssignmentAndReturnAsVector3DI(&e->mBackgroundSprite, NULL);
		setDolmexicaStoryTextBackgroundSprite(id, sprite);
	}
	if (e->mDoesChangeBackgroundOffset) {
		Position offset = evaluateDreamAssignmentAndReturnAsVector3D(&e->mBackgroundOffset, NULL);
		setDolmexicaStoryTextBackgroundOffset(id, offset);
	}
	if (e->mDoesChangeFaceSprite) {
		Vector3DI sprite = evaluateDreamAssignmentAndReturnAsVector3DI(&e->mFaceSprite, NULL);
		setDolmexicaStoryTextFaceSprite(id, sprite);
	}
	if (e->mDoesChangeFaceOffset) {
		Position offset = evaluateDreamAssignmentAndReturnAsVector3D(&e->mFaceOffset, NULL);
		setDolmexicaStoryTextFaceOffset(id, offset);
	}
	if (e->mDoesChangeNextState) {
		int nextState = evaluateDreamAssignmentAndReturnAsInteger(&e->mNextState, NULL);
		setDolmexicaStoryTextNextState(id, nextState);
	}


	return 0;
}

static int handleChangeStateStoryController(DreamMugenStateController* tController) {
	SingleRequiredValueController* e = tController->mData;
	
	int val = evaluateDreamAssignmentAndReturnAsInteger(&e->mValue, NULL);
	changeDolmexicaStoryState(val);

	return 1;
}

static int handleFadeInStoryController(DreamMugenStateController* tController) {
	FadeStoryController* e = tController->mData;

	double duration;
	getSingleFloatValueOrDefault(&e->mDuration, NULL, &duration, 20);
	addFadeIn(duration, NULL, NULL);

	return 0;
}

static int handleFadeOutStoryController(DreamMugenStateController* tController) {
	FadeStoryController* e = tController->mData;

	double duration;
	getSingleFloatValueOrDefault(&e->mDuration, NULL, &duration, 20);
	addFadeOut(duration, NULL, NULL);

	return 0;
}

static int handleGotoStoryStepStoryController(DreamMugenStateController* tController) {
	SingleRequiredValueController* e = tController->mData;

	int newStep = evaluateDreamAssignmentAndReturnAsInteger(&e->mValue, NULL);
	(void)newStep; // TODO

	return 0;
}

static int handleAnimationSetPositionStoryController(DreamMugenStateController* tController) {
	Target2DPhysicsController* e = tController->mData;

	int id;
	getSingleIntegerValueOrDefault(&e->mID, NULL, &id, 1);

	if (e->mIsSettingX) {
		double x = evaluateDreamAssignmentAndReturnAsFloat(&e->x, NULL);
		setDolmexicaStoryAnimationPositionX(id, x);
	}

	if (e->mIsSettingY) {
		double y = evaluateDreamAssignmentAndReturnAsFloat(&e->y, NULL);
		setDolmexicaStoryAnimationPositionY(id, y);
	}
	return 0;
}

static int handleAnimationAddPositionStoryController(DreamMugenStateController* tController) {
	Target2DPhysicsController* e = tController->mData;

	int id;
	getSingleIntegerValueOrDefault(&e->mID, NULL, &id, 1);

	if (e->mIsSettingX) {
		double x = evaluateDreamAssignmentAndReturnAsFloat(&e->x, NULL);
		addDolmexicaStoryAnimationPositionX(id, x);
	}

	if (e->mIsSettingY) {
		double y = evaluateDreamAssignmentAndReturnAsFloat(&e->y, NULL);
		addDolmexicaStoryAnimationPositionY(id, y);
	}
	return 0;
}

static int handleAnimationSetScaleStoryController(DreamMugenStateController* tController) {
	Target2DPhysicsController* e = tController->mData;

	int id;
	getSingleIntegerValueOrDefault(&e->mID, NULL, &id, 1);

	if (e->mIsSettingX) {
		double x = evaluateDreamAssignmentAndReturnAsFloat(&e->x, NULL);
		setDolmexicaStoryAnimationScaleX(id, x);
	}

	if (e->mIsSettingY) {
		double y = evaluateDreamAssignmentAndReturnAsFloat(&e->y, NULL);
		setDolmexicaStoryAnimationScaleY(id, y);
	}
	return 0;
}

static int handleAnimationSetFaceDirectionController(DreamMugenStateController* tController) {
	AnimationSetFaceDirectionStoryController* e = tController->mData;

	int id, faceDirection;
	getSingleIntegerValueOrDefault(&e->mID, NULL, &id, 1);
	getSingleIntegerValueOrDefault(&e->mFacing, NULL, &faceDirection, 1);

	if (!faceDirection) return 0;

	setDolmexicaStoryAnimationIsFacingRight(id, faceDirection == 1);

	return 0;
}

static int handleEndStoryboardController(DreamMugenStateController* tController) {
	SingleRequiredValueController* e = tController->mData;

	int nextState;
	getSingleIntegerValueOrDefault(&e->mValue, NULL, &nextState, 1);

	endDolmexicaStoryboard(nextState);

	return 0;
}

static int handleMoveStageStoryController(DreamMugenStateController* tController) {
	Set2DPhysicsController* e = tController->mData;

	if (e->mIsSettingX) {
		double x = evaluateDreamAssignmentAndReturnAsFloat(&e->x, NULL);
		addDreamMugenStageHandlerCameraPositionX(x);
	}

	if (e->mIsSettingY) {
		double y = evaluateDreamAssignmentAndReturnAsFloat(&e->y, NULL);
		addDreamMugenStageHandlerCameraPositionY(y);
	}
	return 0;
}

int nullStoryHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleNull(); }
int createAnimationStoryHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleCreateAnimationStoryController(tController); }
int removeAnimationStoryHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleRemoveAnimationStoryController(tController); }
int changeAnimationStoryHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleChangeAnimationStoryController(tController); }
int createTextStoryHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleCreateTextStoryController(tController); }
int removeTextStoryHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleRemoveTextStoryController(tController); }
int changeTextStoryHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleChangeTextStoryController(tController); }
int changeStateStoryHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleChangeStateStoryController(tController); }
int fadeInStoryHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleFadeInStoryController(tController); }
int fadeOutStoryHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleFadeOutStoryController(tController); }
int gotoStoryStepStoryHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleGotoStoryStepStoryController(tController); }
int animationSetPositionStoryHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleAnimationSetPositionStoryController(tController); }
int animationAddPositionStoryHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleAnimationAddPositionStoryController(tController); }
int animationSetScaleStoryHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleAnimationSetScaleStoryController(tController); }
int animationSetFaceDirectionStoryHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleAnimationSetFaceDirectionController(tController); }
int endStoryboardStoryHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleEndStoryboardController(tController); }
int moveStageStoryHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleMoveStageStoryController(tController); }


static void setupStoryStateControllerHandlers() {
	gVariableHandler.mStateControllerHandlers = new_int_map();
	
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STORY_STATE_CONTROLLER_TYPE_NULL, nullStoryHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STORY_STATE_CONTROLLER_TYPE_CREATE_ANIMATION, createAnimationStoryHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STORY_STATE_CONTROLLER_TYPE_REMOVE_ANIMATION, removeAnimationStoryHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STORY_STATE_CONTROLLER_TYPE_CHANGE_ANIMATION, changeAnimationStoryHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STORY_STATE_CONTROLLER_TYPE_CREATE_TEXT, createTextStoryHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STORY_STATE_CONTROLLER_TYPE_REMOVE_TEXT, removeTextStoryHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STORY_STATE_CONTROLLER_TYPE_CHANGE_TEXT, changeTextStoryHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STORY_STATE_CONTROLLER_TYPE_CHANGE_STATE, changeStateStoryHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STORY_STATE_CONTROLLER_TYPE_FADE_IN, fadeInStoryHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STORY_STATE_CONTROLLER_TYPE_FADE_OUT, fadeOutStoryHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STORY_STATE_CONTROLLER_TYPE_GOTO_STORY_STEP, gotoStoryStepStoryHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STORY_STATE_CONTROLLER_TYPE_ANIMATION_SET_POSITION, animationSetPositionStoryHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STORY_STATE_CONTROLLER_TYPE_ANIMATION_ADD_POSITION, animationAddPositionStoryHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STORY_STATE_CONTROLLER_TYPE_ANIMATION_SET_SCALE, animationSetScaleStoryHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STORY_STATE_CONTROLLER_TYPE_ANIMATION_SET_FACEDIRECTION, animationSetFaceDirectionStoryHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STORY_STATE_CONTROLLER_TYPE_END_STORYBOARD, endStoryboardStoryHandleFunction);
	int_map_push(&gVariableHandler.mStateControllerHandlers, MUGEN_STORY_STATE_CONTROLLER_TYPE_MOVE_STAGE, moveStageStoryHandleFunction);

}

void setupDreamMugenStoryStateControllerHandler()
{
	setupStoryStateControllerParsers();
	setupStoryStateControllerHandlers();
}

void shutdownDreamMugenStoryStateControllerHandler()
{
	delete_string_map(&gVariableHandler.mStateControllerParsers);
	delete_int_map(&gVariableHandler.mStateControllerHandlers);
	delete_int_map(&gVariableHandler.mStateControllerUnloaders);
	gVariableHandler.mMemoryStack = NULL;
}
