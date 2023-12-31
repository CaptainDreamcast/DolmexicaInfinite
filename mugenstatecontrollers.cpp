#include "mugenstatecontrollers.h"

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <string>

#define LOGGER_WARNINGS_DISABLED

#include <prism/memoryhandler.h>
#include <prism/log.h>
#include <prism/system.h>
#include <prism/clipboardhandler.h>
#include <prism/input.h>
#include <prism/math.h>
#include <prism/mugenanimationhandler.h>
#include <prism/screeneffect.h>
#include <prism/soundeffect.h>

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
#include "titlescreen.h"
#include "intro.h"
#include "mugensound.h"
#include "afterimage.h"
#include "mugenstatehandler.h"

#define GAME_MAKE_ANIM_UNDER_Z 31
#define GAME_MAKE_ANIM_OVER_Z 51

using namespace std;

typedef void(*StateControllerParseFunction)(DreamMugenStateController*, MugenDefScriptGroup*);
typedef int(*StateControllerHandleFunction)(DreamMugenStateController*, DreamPlayer*); // return 1 iff state changed
typedef void(*StateControllerUnloadFunction)(DreamMugenStateController*);

static struct {
	map<string, StateControllerParseFunction> mStateControllerParsers; 
	map<int, StateControllerHandleFunction> mStateControllerHandlers;
	map<int, StateControllerUnloadFunction> mStateControllerUnloaders; 
	MemoryStack* mMemoryStack;
} gMugenStateControllerVariableHandler;


typedef struct {

	char mName[100];
	DreamMugenAssignment** mTriggerRoot;
} TriggerParseCaller;

static void checkSingleElementForTrigger(void* tCaller, void* tData) {
	TriggerParseCaller* caller = (TriggerParseCaller*)tCaller;
	MugenDefScriptGroupElement* e = (MugenDefScriptGroupElement*)tData;

	if (strcmp(e->mName.data(), caller->mName)) return;

	char* text = getAllocatedMugenDefStringVariableAsElement(e);

	DreamMugenAssignment* trigger = parseDreamMugenAssignmentFromString(text);
	freeMemory(text);

	*caller->mTriggerRoot = makeDreamAndMugenAssignment(*caller->mTriggerRoot, trigger);
}

static int parseTriggerAndReturnIfFound(const char* tName, DreamMugenAssignment** tRoot, MugenDefScriptGroup* tGroup) {
	if (!stl_string_map_contains_array(tGroup->mElements, tName)) return 0;

	DreamMugenAssignment* triggerRoot = NULL;
	TriggerParseCaller caller;
	strcpy(caller.mName, tName);
	caller.mTriggerRoot = &triggerRoot;
	list_map(&tGroup->mOrderedElementList, checkSingleElementForTrigger, &caller);

	*tRoot = makeDreamOrMugenAssignment(*tRoot, triggerRoot);

	return 1;
}

static int parseNumberedTriggerAndReturnIfFound(int i, DreamMugenAssignment** tRoot, MugenDefScriptGroup* tGroup) {
	char name[100];
	sprintf(name, "trigger%d", i);

	return parseTriggerAndReturnIfFound(name, tRoot, tGroup);
}

static void parseStateControllerTriggers(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) {
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
	if (gMugenStateControllerVariableHandler.mMemoryStack && canFitOnMemoryStack(gMugenStateControllerVariableHandler.mMemoryStack, tSize)) return allocMemoryOnMemoryStack(gMugenStateControllerVariableHandler.mMemoryStack, tSize);
	else return allocMemory(tSize);
}

typedef struct {
	DreamMugenAssignment* x;
	DreamMugenAssignment* y;

	uint8_t mIsSettingX;
	uint8_t mIsSettingY;
} Set2DPhysicsController;


static void parse2DPhysicsController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup, DreamMugenStateControllerType tType) {
	Set2DPhysicsController* e = (Set2DPhysicsController*)allocMemoryOnMemoryStackOrMemory(sizeof(Set2DPhysicsController));
	
	e->mIsSettingX = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("x", tGroup, &e->x);
	e->mIsSettingY = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("y", tGroup, &e->y);

	tController->mType = tType;
	tController->mData = e;
}

static void unload2DPhysicsController(DreamMugenStateController* tController) {
	Set2DPhysicsController* e = (Set2DPhysicsController*)tController->mData;
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
	DreamMugenAssignment* mAnimation;

	uint8_t mIsChangingControl;
	uint8_t mIsChangingAnimation;
} ChangeStateController;


static void parseChangeStateController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup, DreamMugenStateControllerType tType) {
	ChangeStateController* e = (ChangeStateController*)allocMemoryOnMemoryStackOrMemory(sizeof(ChangeStateController));
	fetchDreamAssignmentFromGroupAndReturnWhetherItExists("value", tGroup, &e->mState);

	e->mIsChangingControl = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("ctrl", tGroup, &e->mControl);
	e->mIsChangingAnimation = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("anim", tGroup, &e->mAnimation);

	tController->mType = tType;
	tController->mData = e;
}

static void unloadChangeStateController(DreamMugenStateController* tController) {
	ChangeStateController* e = (ChangeStateController*)tController->mData;
	destroyDreamMugenAssignment(e->mState);

	if (e->mIsChangingControl) {
		destroyDreamMugenAssignment(e->mControl);
	} 

	if (e->mIsChangingAnimation) {
		destroyDreamMugenAssignment(e->mAnimation);
	}

	freeMemory(e);
}

static void fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString(const char* tName, MugenDefScriptGroup* tGroup, DreamMugenAssignment** tDst, const char* tDefault = NULL) {
	if (!fetchDreamAssignmentFromGroupAndReturnWhetherItExists(tName, tGroup, tDst)) {
		if (tDefault) {
			*tDst = makeDreamStringMugenAssignment(tDefault);
		}
		else {
			*tDst = NULL;
		}
	}
}

typedef struct {
	DreamMugenAssignment* mState;
	DreamMugenAssignment* mControl;
	DreamMugenAssignment* mID;

	uint8_t mIsChangingControl;
} TargetChangeStateController;

static void parseTargetChangeStateController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) {
	TargetChangeStateController* e = (TargetChangeStateController*)allocMemoryOnMemoryStackOrMemory(sizeof(TargetChangeStateController));
	fetchDreamAssignmentFromGroupAndReturnWhetherItExists("value", tGroup, &e->mState);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("id", tGroup, &e->mID);

	e->mIsChangingControl = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("ctrl", tGroup, &e->mControl);

	tController->mType = MUGEN_STATE_CONTROLLER_TYPE_SET_TARGET_STATE;
	tController->mData = e;
}

static void unloadTargetChangeStateController(DreamMugenStateController* tController) {
	TargetChangeStateController* e = (TargetChangeStateController*)tController->mData;
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
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("attr", tGroup, &e->mAttribute, "s , na");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("hitflag", tGroup, &e->mHitFlag, "maf");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("guardflag", tGroup, &e->mGuardFlag);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("affectteam", tGroup, &e->mAffectTeam, "e");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("animtype", tGroup, &e->mAnimationType, "light");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("air.animtype", tGroup, &e->mAirAnimationType);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("fall.animtype", tGroup, &e->mFallAnimationType);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("priority", tGroup, &e->mPriority);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("damage", tGroup, &e->mDamage, "0 , 0");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("pausetime", tGroup, &e->mPauseTime, "0 , 0");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("guard.pausetime", tGroup, &e->mGuardPauseTime);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("sparkno", tGroup, &e->mSparkNumber);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("guard.sparkno", tGroup, &e->mGuardSparkNumber);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("sparkxy", tGroup, &e->mSparkXY, "0 , 0");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("hitsound", tGroup, &e->mHitSound);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("guardsound", tGroup, &e->mGuardSound);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("ground.type", tGroup, &e->mGroundType, "high");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("air.type", tGroup, &e->mAirType);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("ground.slidetime", tGroup, &e->mGroundSlideTime, "0");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("guard.slidetime", tGroup, &e->mGuardSlideTime);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("ground.hittime", tGroup, &e->mGroundHitTime, "0");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("guard.hittime", tGroup, &e->mGuardHitTime);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("air.hittime", tGroup, &e->mAirHitTime, "20");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("guard.ctrltime", tGroup, &e->mGuardControlTime);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("guard.dist", tGroup, &e->mGuardDistance);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("yaccel", tGroup, &e->mYAccel);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("ground.velocity", tGroup, &e->mGroundVelocity, "0 , 0");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("guard.velocity", tGroup, &e->mGuardVelocity);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("air.velocity", tGroup, &e->mAirVelocity, "0 , 0");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("airguard.velocity", tGroup, &e->mAirGuardVelocity);

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("ground.cornerpush.veloff", tGroup, &e->mGroundCornerPushVelocityOffset);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("air.cornerpush.veloff", tGroup, &e->mAirCornerPushVelocityOffset);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("down.cornerpush.veloff", tGroup, &e->mDownCornerPushVelocityOffset);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("guard.cornerpush.veloff", tGroup, &e->mGuardCornerPushVelocityOffset);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("airguard.cornerpush.veloff", tGroup, &e->mAirGuardCornerPushVelocityOffset);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("airguard.ctrltime", tGroup, &e->mAirGuardControlTime);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("air.juggle", tGroup, &e->mAirJuggle, "0");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("mindist", tGroup, &e->mMinimumDistance);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("maxdist", tGroup, &e->mMaximumDistance);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("snap", tGroup, &e->mSnap);

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("p1sprpriority", tGroup, &e->mPlayerSpritePriority1, "1");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("p2sprpriority", tGroup, &e->mPlayerSpritePriority2, "0");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("p1facing", tGroup, &e->mPlayer1ChangeFaceDirection);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("p1getp2facing", tGroup, &e->mPlayer1ChangeFaceDirectionRelativeToPlayer2, "0");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("p2facing", tGroup, &e->mPlayer2ChangeFaceDirectionRelativeToPlayer1, "0");

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("p1stateno", tGroup, &e->mPlayer1StateNumber, "-1");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("p2stateno", tGroup, &e->mPlayer2StateNumber, "-1");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("p2getp1state", tGroup, &e->mPlayer2CapableOfGettingPlayer1State, "1");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("forcestand", tGroup, &e->mForceStanding);

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("fall", tGroup, &e->mFall, "0");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("fall.xvelocity", tGroup, &e->mFallXVelocity, "0");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("fall.yvelocity", tGroup, &e->mFallYVelocity);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("fall.recover", tGroup, &e->mFallCanBeRecovered, "1");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("fall.recovertime", tGroup, &e->mFallRecoveryTime, "4");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("fall.damage", tGroup, &e->mFallDamage, "0");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("air.fall", tGroup, &e->mAirFall);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("forcenofall", tGroup, &e->mForceNoFall, "0");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("down.velocity", tGroup, &e->mDownVelocity);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("down.hittime", tGroup, &e->mDownHitTime);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("down.bounce", tGroup, &e->mDownBounce, "0");

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("id", tGroup, &e->mHitID, "0");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("chainID", tGroup, &e->mChainID, "-1");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("nochainID", tGroup, &e->mNoChainID);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("hitonce", tGroup, &e->mHitOnce);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("kill", tGroup, &e->mKill, "1");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("guard.kill", tGroup, &e->mGuardKill, "1");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("fall.kill", tGroup, &e->mFallKill, "1");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("numhits", tGroup, &e->mNumberOfHits, "1");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("getpower", tGroup, &e->mGetPower);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("givepower", tGroup, &e->mGivePower);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("palfx.time", tGroup, &e->mPaletteEffectTime, "0");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("palfx.mul", tGroup, &e->mPaletteEffectMultiplication);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("palfx.add", tGroup, &e->mPaletteEffectAddition);

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("envshake.time", tGroup, &e->mEnvironmentShakeTime);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("envshake.freq", tGroup, &e->mEnvironmentShakeFrequency);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("envshake.ampl", tGroup, &e->mEnvironmentShakeAmplitude);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("envshake.phase", tGroup, &e->mEnvironmentShakePhase);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("fall.envshake.time", tGroup, &e->mFallEnvironmentShakeTime);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("fall.envshake.freq", tGroup, &e->mFallEnvironmentShakeFrequency);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("fall.envshake.ampl", tGroup, &e->mFallEnvironmentShakeAmplitude);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("fall.envshake.phase", tGroup, &e->mFallEnvironmentShakePhase);

}

static void parseHitDefinitionController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) {
	HitDefinitionController* e = (HitDefinitionController*)allocMemoryOnMemoryStackOrMemory(sizeof(HitDefinitionController));
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
	HitDefinitionController* e = (HitDefinitionController*)tController->mData;

	unloadHitDefinitionData(e);

	freeMemory(e);
}

typedef struct {
	DreamMugenAssignment* mValue;

	int mIsVolumeScale;
	DreamMugenAssignment* mVolume;
	DreamMugenAssignment* mChannel;
	DreamMugenAssignment* mLowPriority;
	DreamMugenAssignment* mFrequencyMultiplier;
	DreamMugenAssignment* mLoop;
	int mIsAbspan;
	DreamMugenAssignment* mPan;

} PlaySoundController;


static void parsePlaySoundController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup, DreamMugenStateControllerType tType) {
	PlaySoundController* e = (PlaySoundController*)allocMemoryOnMemoryStackOrMemory(sizeof(PlaySoundController));

	fetchDreamAssignmentFromGroupAndReturnWhetherItExists("value", tGroup, &e->mValue);
	e->mIsVolumeScale = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("volumescale", tGroup, &e->mVolume);
	if (!e->mIsVolumeScale)
	{
		fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("volume", tGroup, &e->mVolume);
	}
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("channel", tGroup, &e->mChannel);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("lowpriority", tGroup, &e->mLowPriority);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("freqmul", tGroup, &e->mFrequencyMultiplier);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("loop", tGroup, &e->mLoop);
	e->mIsAbspan = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("abspan", tGroup, &e->mPan);
	if (!e->mIsAbspan)
	{
		fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("pan", tGroup, &e->mPan);
	}

	tController->mType = tType;
	tController->mData = e;
}

static void unloadPlaySoundController(DreamMugenStateController* tController) {
	PlaySoundController* e = (PlaySoundController*)tController->mData;

	destroyDreamMugenAssignment(e->mValue);

	destroyDreamMugenAssignment(e->mVolume);
	destroyDreamMugenAssignment(e->mChannel);
	destroyDreamMugenAssignment(e->mLowPriority);
	destroyDreamMugenAssignment(e->mFrequencyMultiplier);
	destroyDreamMugenAssignment(e->mLoop);
	destroyDreamMugenAssignment(e->mPan);

	freeMemory(e);
}

typedef struct {
	DreamMugenAssignment* mValue;

	DreamMugenAssignment* mEdge;
	DreamMugenAssignment* mPlayer;

	uint8_t mHasValue;
} WidthController;

static void parseWidthController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) {
	WidthController* e = (WidthController*)allocMemoryOnMemoryStackOrMemory(sizeof(WidthController));
	
	e->mHasValue = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("value", tGroup, &e->mValue);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("edge", tGroup, &e->mEdge);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("player", tGroup, &e->mPlayer);

	tController->mType = MUGEN_STATE_CONTROLLER_TYPE_WIDTH;
	tController->mData = e;
}

static void unloadWidthController(DreamMugenStateController* tController) {
	WidthController* e = (WidthController*)tController->mData;

	if (e->mHasValue) {
		destroyDreamMugenAssignment(e->mValue);
	}

	destroyDreamMugenAssignment(e->mEdge);
	destroyDreamMugenAssignment(e->mPlayer);

	freeMemory(e);
}

typedef struct {
	DreamMugenAssignment* mScale;
	DreamMugenAssignment* mPos;
} ZoomController;

static void parseZoomController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) {
	ZoomController* e = (ZoomController*)allocMemoryOnMemoryStackOrMemory(sizeof(ZoomController));

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("scale", tGroup, &e->mScale);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("pos", tGroup, &e->mPos);

	tController->mType = MUGEN_STATE_CONTROLLER_TYPE_ZOOM;
	tController->mData = e;
}

static void unloadZoomController(DreamMugenStateController* tController) {
	ZoomController* e = (ZoomController*)tController->mData;
	destroyDreamMugenAssignment(e->mScale);
	destroyDreamMugenAssignment(e->mPos);
	freeMemory(e);
}

typedef struct {
	DreamMugenAssignment* tNewAnimation;

	DreamMugenAssignment* tStep;
} ChangeAnimationController;


static void parseChangeAnimationController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup, DreamMugenStateControllerType tType) {
	ChangeAnimationController* e = (ChangeAnimationController*)allocMemoryOnMemoryStackOrMemory(sizeof(ChangeAnimationController));

	fetchDreamAssignmentFromGroupAndReturnWhetherItExists("value", tGroup, &e->tNewAnimation);


	if (!fetchDreamAssignmentFromGroupAndReturnWhetherItExists("elem", tGroup, &e->tStep)) {
		e->tStep = makeDreamNumberMugenAssignment(1);
	}

	tController->mType = tType;
	tController->mData = e;
}

static void unloadChangeAnimationController(DreamMugenStateController* tController) {
	ChangeAnimationController* e = (ChangeAnimationController*)tController->mData;

	destroyDreamMugenAssignment(e->tNewAnimation);
	destroyDreamMugenAssignment(e->tStep);

	freeMemory(e);
}

typedef struct {
	DreamMugenAssignment* tValue;
} ControlSettingController;


static void parseControlSettingController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) {
	ControlSettingController* e = (ControlSettingController*)allocMemoryOnMemoryStackOrMemory(sizeof(ControlSettingController));

	fetchDreamAssignmentFromGroupAndReturnWhetherItExists("value", tGroup, &e->tValue);

	tController->mType = MUGEN_STATE_CONTROLLER_TYPE_SET_CONTROL;
	tController->mData = e;
}

static void unloadControlSettingController(DreamMugenStateController* tController) {
	ControlSettingController* e = (ControlSettingController*)tController->mData;

	destroyDreamMugenAssignment(e->tValue);

	freeMemory(e);
}

typedef struct {
	DreamMugenAssignment* tValue;
} SpritePriorityController;


static void parseSpritePriorityController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) {
	SpritePriorityController* e = (SpritePriorityController*)allocMemoryOnMemoryStackOrMemory(sizeof(SpritePriorityController));

	fetchDreamAssignmentFromGroupAndReturnWhetherItExists("value", tGroup, &e->tValue);

	tController->mType = MUGEN_STATE_CONTROLLER_TYPE_SPRITE_PRIORITY;
	tController->mData = e;
}

static void unloadSpritePriorityController(DreamMugenStateController* tController) {
	SpritePriorityController* e = (SpritePriorityController*)tController->mData;

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
	SpecialAssertController* e = (SpecialAssertController*)allocMemoryOnMemoryStackOrMemory(sizeof(SpecialAssertController));

	fetchDreamAssignmentFromGroupAndReturnWhetherItExists("flag", tGroup, &e->mFlag);
	e->mHasFlag2 = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("flag2", tGroup, &e->mFlag2);
	e->mHasFlag3 = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("flag3", tGroup, &e->mFlag3);

	tController->mType = MUGEN_STATE_CONTROLLER_TYPE_ASSERT_SPECIAL;
	tController->mData = e;
}

static void unloadSpecialAssertController(DreamMugenStateController* tController) {
	SpecialAssertController* e = (SpecialAssertController*)tController->mData;

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
	MakeDustController* e = (MakeDustController*)allocMemoryOnMemoryStackOrMemory(sizeof(MakeDustController));

	if (!fetchDreamAssignmentFromGroupAndReturnWhetherItExists("pos", tGroup, &e->mPositionOffset)) {
		e->mPositionOffset = makeDream2DVectorMugenAssignment(Vector2D(0, 0));
	}

	e->mHasSecondDustCloud = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("pos2", tGroup, &e->mPositionOffset2);

	if (!fetchDreamAssignmentFromGroupAndReturnWhetherItExists("spacing", tGroup, &e->mSpacing)) {
		e->mSpacing = makeDreamNumberMugenAssignment(1);
	}

	tController->mType = MUGEN_STATE_CONTROLLER_TYPE_MAKE_DUST;
	tController->mData = e;
}

static void unloadMakeDustController(DreamMugenStateController* tController) {
	MakeDustController* e = (MakeDustController*)tController->mData;

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
	NullController* e = (NullController*)allocMemoryOnMemoryStackOrMemory(sizeof(NullController));
	e->mDummy = 0;

	tController->mType = tType;
	tController->mData = e;
}

static void unloadNullController(DreamMugenStateController* tController) {
	freeMemory(tController->mData);
}

enum VarSetType : uint8_t {
	VAR_SET_TYPE_SYSTEM,
	VAR_SET_TYPE_SYSTEM_FLOAT,
	VAR_SET_TYPE_INTEGER,
	VAR_SET_TYPE_FLOAT
};

typedef struct {
	DreamMugenAssignment* mID;
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

static void parseSingleVarSetControllerEntry(VarSetControllerCaller* tCaller, const string& tName, MugenDefScriptGroupElement& /*tElement*/) {
	VarSetControllerCaller* caller = (VarSetControllerCaller*)tCaller;

	int isEntry = tName.find('(') != string::npos;
	if (!isEntry) return;

	VarSetControllerEntry* e = (VarSetControllerEntry*)allocMemoryOnMemoryStackOrMemory(sizeof(VarSetControllerEntry));

	char name[100];
	strcpy(name, tName.data());
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

	e->mID = makeDreamNumberMugenAssignment(atoi(value));

	fetchDreamAssignmentFromGroupAndReturnWhetherItExists(tName.data(), caller->mGroup, &e->mAssignment);

	vector_push_back(&caller->mController->mVarSets, e);
}

static void loadSingleOriginalVarSetController(Vector* tDst, MugenDefScriptGroup* tGroup, MugenDefScriptGroupElement* tIDElement, VarSetType tType) {
	VarSetControllerEntry* e = (VarSetControllerEntry*)allocMemoryOnMemoryStackOrMemory(sizeof(VarSetControllerEntry));
	e->mType = tType;
	fetchDreamAssignmentFromGroupAsElement(tIDElement, &e->mID);
	fetchDreamAssignmentFromGroupAndReturnWhetherItExists("value", tGroup, &e->mAssignment);

	vector_push_back(tDst, e);
}

static void unloadSingleVarSetEntry(void* tCaller, void* tData) {
	(void)tCaller;
	VarSetControllerEntry* e = (VarSetControllerEntry*)tData;
	destroyDreamMugenAssignment(e->mAssignment);
}

static void parseVarSetController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup, DreamMugenStateControllerType tType) {
	int isIntegerVersion = stl_string_map_contains_array(tGroup->mElements, "v");
	int isFloatVersion = stl_string_map_contains_array(tGroup->mElements, "fv");

	VarSetController* e = (VarSetController*)allocMemoryOnMemoryStackOrMemory(sizeof(VarSetController));
	e->mVarSets = new_vector();

	if (isIntegerVersion) {
		loadSingleOriginalVarSetController(&e->mVarSets, tGroup, &tGroup->mElements["v"], VAR_SET_TYPE_INTEGER);
	}
	else if (isFloatVersion) {
		loadSingleOriginalVarSetController(&e->mVarSets, tGroup, &tGroup->mElements["fv"], VAR_SET_TYPE_FLOAT);
	}
	else {
		VarSetControllerCaller caller;
		caller.mController = e;
		caller.mGroup = tGroup;

		stl_string_map_map(tGroup->mElements, parseSingleVarSetControllerEntry, &caller);
	}

	if (vector_size(&e->mVarSets) != 1) {
		logWarning("Unable to parse VarSetController. Missing elements. Defaulting to Null controller.");
		// unable to delete mugen assignments if on memory stack
		delete_vector(&e->mVarSets);
		parseNullController(tController, MUGEN_STATE_CONTROLLER_TYPE_NULL);
		return;
	}

	tController->mType = tType;
	tController->mData = e;
}



static void unloadVarSetController(DreamMugenStateController* tController) {
	VarSetController* e = (VarSetController*)tController->mData;
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
	VarRangeSetController* e = (VarRangeSetController*)allocMemoryOnMemoryStackOrMemory(sizeof(VarRangeSetController));

	int isIntegerVersion = stl_string_map_contains_array(tGroup->mElements, "value");

	if (isIntegerVersion) {
		e->mType = VAR_SET_TYPE_INTEGER;
		fetchDreamAssignmentFromGroupAndReturnWhetherItExists("value", tGroup, &e->mValue);
	}
	else {
		e->mType = VAR_SET_TYPE_FLOAT;
		fetchDreamAssignmentFromGroupAndReturnWhetherItExists("fvalue", tGroup, &e->mValue);
	}

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("first", tGroup, &e->mFirst);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("last", tGroup, &e->mLast);
 
	tController->mType = MUGEN_STATE_CONTROLLER_TYPE_SET_VARIABLE_RANGE;
	tController->mData = e;
}

static void unloadVarRangeSetController(DreamMugenStateController* tController) {
	VarRangeSetController* e = (VarRangeSetController*)tController->mData;
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
	StateTypeSetController* e = (StateTypeSetController*)allocMemoryOnMemoryStackOrMemory(sizeof(StateTypeSetController));

	e->mHasStateType = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("statetype", tGroup, &e->mStateType);
	e->mHasMoveType = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("movetype", tGroup, &e->mMoveType);
	e->mHasPhysics = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("physics", tGroup, &e->mPhysics);

	tController->mType = MUGEN_STATE_CONTROLLER_TYPE_SET_STATE_TYPE;
	tController->mData = e;
}

static void unloadStateTypeSetController(DreamMugenStateController* tController) {
	StateTypeSetController* e = (StateTypeSetController*)tController->mData;

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
	ForceFeedbackController*e = (ForceFeedbackController*)allocMemoryOnMemoryStackOrMemory(sizeof(ForceFeedbackController));

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("waveform", tGroup, &e->mWaveform, "sine");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("time", tGroup, &e->mTime, "sine");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("freq", tGroup, &e->mFrequency, "sine");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("ampl", tGroup, &e->mAmplitude, "sine");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("self", tGroup, &e->mSelf, "sine");

	tController->mType = MUGEN_STATE_CONTROLLER_TYPE_FORCE_FEEDBACK;
	tController->mData = e;
}

static void unloadForceFeedbackController(DreamMugenStateController* tController) {
	ForceFeedbackController*e = (ForceFeedbackController*)tController->mData;

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
	DefenseMultiplierController* e = (DefenseMultiplierController*)allocMemoryOnMemoryStackOrMemory(sizeof(DefenseMultiplierController));

	fetchDreamAssignmentFromGroupAndReturnWhetherItExists("value", tGroup, &e->mValue);

	tController->mType = MUGEN_STATE_CONTROLLER_TYPE_SET_DEFENSE_MULTIPLIER;
	tController->mData = e;
}

static void unloadDefenseMultiplierController(DreamMugenStateController* tController) {
	DefenseMultiplierController* e = (DefenseMultiplierController*)tController->mData;
	destroyDreamMugenAssignment(e->mValue);
	freeMemory(e);
}

typedef struct {
	DreamMugenAssignment* mAnim;

	DreamMugenAssignment* mID;
	DreamMugenAssignment* mSpace;
	DreamMugenAssignment* mPosition;
	DreamMugenAssignment* mPositionType;
	DreamMugenAssignment* mHorizontalFacing;
	DreamMugenAssignment* mVerticalFacing;
	DreamMugenAssignment* mBindTime;
	DreamMugenAssignment* mVelocity;
	DreamMugenAssignment* mAcceleration;
	DreamMugenAssignment* mRandomOffset;
	DreamMugenAssignment* mRemoveTime;
	int mHasSuperMove;
	DreamMugenAssignment* mSuperMoveAndMoveTime;
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
	ExplodController* e = (ExplodController*)allocMemoryOnMemoryStackOrMemory(sizeof(ExplodController));

	fetchDreamAssignmentFromGroupAndReturnWhetherItExists("anim", tGroup, &e->mAnim);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("id", tGroup, &e->mID);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("space", tGroup, &e->mSpace);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("pos", tGroup, &e->mPosition);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("postype", tGroup, &e->mPositionType, "p1");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("facing", tGroup, &e->mHorizontalFacing);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("vfacing", tGroup, &e->mVerticalFacing);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("bindtime", tGroup, &e->mBindTime);
	if (stl_string_map_contains_array(tGroup->mElements, "vel")) {
		fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("vel", tGroup, &e->mVelocity);
	}
	else {
		fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("velocity", tGroup, &e->mVelocity);
	}
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("accel", tGroup, &e->mAcceleration);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("random", tGroup, &e->mRandomOffset);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("removetime", tGroup, &e->mRemoveTime);
	e->mHasSuperMove = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("supermove", tGroup, &e->mSuperMoveAndMoveTime);
	if (!e->mHasSuperMove) {
		fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("supermovetime", tGroup, &e->mSuperMoveAndMoveTime);
	}
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("pausemovetime", tGroup, &e->mPauseMoveTime);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("scale", tGroup, &e->mScale);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("sprpriority", tGroup, &e->mSpritePriority);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("ontop", tGroup, &e->mOnTop);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("shadow", tGroup, &e->mShadow);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("ownpal", tGroup, &e->mOwnPalette);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("removeongethit", tGroup, &e->mIsRemovedOnGetHit);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("ignorehitpause", tGroup, &e->mIgnoreHitPause);
	e->mHasTransparencyType = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("trans", tGroup, &e->mTransparencyType);

	tController->mType = MUGEN_STATE_CONTROLLER_TYPE_EXPLOD;
	tController->mData = e;
}

static void unloadExplodController(DreamMugenStateController* tController) {
	ExplodController* e = (ExplodController*)tController->mData;

	destroyDreamMugenAssignment(e->mAnim);

	destroyDreamMugenAssignment(e->mID);
	destroyDreamMugenAssignment(e->mSpace);
	destroyDreamMugenAssignment(e->mPosition);
	destroyDreamMugenAssignment(e->mPositionType);
	destroyDreamMugenAssignment(e->mHorizontalFacing);
	destroyDreamMugenAssignment(e->mVerticalFacing);
	destroyDreamMugenAssignment(e->mBindTime);
	destroyDreamMugenAssignment(e->mVelocity);
	destroyDreamMugenAssignment(e->mAcceleration);
	destroyDreamMugenAssignment(e->mRandomOffset);
	destroyDreamMugenAssignment(e->mRemoveTime);
	destroyDreamMugenAssignment(e->mSuperMoveAndMoveTime);
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

typedef struct {
	DreamMugenAssignment* mAnim;

	DreamMugenAssignment* mID;
	DreamMugenAssignment* mSpace;
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

	uint8_t mHasAnim;
	uint8_t mHasSpace;
	uint8_t mHasPosition;
	uint8_t mHasPositionType;
	uint8_t mHasHorizontalFacing;
	uint8_t mHasVerticalFacing;
	uint8_t mHasBindTime;
	uint8_t mHasVelocity;
	uint8_t mHasAcceleration;
	uint8_t mHasRandomOffset;
	uint8_t mHasRemoveTime;
	uint8_t mHasSuperMove;
	uint8_t mHasSuperMoveTime;
	uint8_t mHasPauseMoveTime;
	uint8_t mHasScale;
	uint8_t mHasSpritePriority;
	uint8_t mHasOnTop;
	uint8_t mHasShadow;
	uint8_t mHasOwnPalette;
	uint8_t mHasIsRemovedOnGetHit;
	uint8_t mHasIgnoreHitPause;
	uint8_t mHasTransparencyType;
} ModifyExplodController;

static void parseModifyExplodController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) {
	ModifyExplodController* e = (ModifyExplodController*)allocMemoryOnMemoryStackOrMemory(sizeof(ModifyExplodController));

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("id", tGroup, &e->mID);
	e->mHasSpace = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("space", tGroup, &e->mSpace);
	e->mHasAnim = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("anim", tGroup, &e->mAnim);
	e->mHasPosition = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("pos", tGroup, &e->mPosition);
	e->mHasPositionType = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("postype", tGroup, &e->mPositionType);
	e->mHasHorizontalFacing = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("facing", tGroup, &e->mHorizontalFacing);
	e->mHasVerticalFacing = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("vfacing", tGroup, &e->mVerticalFacing);
	e->mHasBindTime = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("bindtime", tGroup, &e->mBindTime);
	e->mHasVelocity = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("vel", tGroup, &e->mVelocity);
	e->mHasAcceleration = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("accel", tGroup, &e->mAcceleration);
	e->mHasRandomOffset = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("random", tGroup, &e->mRandomOffset);
	e->mHasRemoveTime = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("removetime", tGroup, &e->mRemoveTime);
	e->mHasSuperMove = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("supermove", tGroup, &e->mSuperMove);
	e->mHasSuperMoveTime = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("supermovetime", tGroup, &e->mSuperMoveTime);
	e->mHasPauseMoveTime = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("pausemovetime", tGroup, &e->mPauseMoveTime);
	e->mHasScale = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("scale", tGroup, &e->mScale);
	e->mHasSpritePriority = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("sprpriority", tGroup, &e->mSpritePriority);
	e->mHasOnTop = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("ontop", tGroup, &e->mOnTop);
	e->mHasShadow = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("shadow", tGroup, &e->mShadow);
	e->mHasOwnPalette = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("ownpal", tGroup, &e->mOwnPalette);
	e->mHasIsRemovedOnGetHit = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("removeongethit", tGroup, &e->mIsRemovedOnGetHit);
	e->mHasIgnoreHitPause = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("ignorehitpause", tGroup, &e->mIgnoreHitPause);
	e->mHasTransparencyType = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("trans", tGroup, &e->mTransparencyType);

	tController->mType = MUGEN_STATE_CONTROLLER_TYPE_MODIFY_EXPLOD;
	tController->mData = e;
}

static void unloadModifyExplodController(DreamMugenStateController* tController) {
	ModifyExplodController* e = (ModifyExplodController*)tController->mData;

	destroyDreamMugenAssignment(e->mID);
	if (e->mHasAnim) {
		destroyDreamMugenAssignment(e->mAnim);
	}
	if (e->mHasSpace) {
		destroyDreamMugenAssignment(e->mSpace);
	}
	if (e->mHasPositionType) {
		destroyDreamMugenAssignment(e->mPositionType);
	}
	if (e->mHasPosition) {
		destroyDreamMugenAssignment(e->mPosition);
	}
	if (e->mHasHorizontalFacing) {
		destroyDreamMugenAssignment(e->mHorizontalFacing);
	}
	if (e->mHasVerticalFacing) {
		destroyDreamMugenAssignment(e->mVerticalFacing);
	}
	if (e->mHasBindTime) {
		destroyDreamMugenAssignment(e->mBindTime);
	}
	if (e->mHasVelocity) {
		destroyDreamMugenAssignment(e->mVelocity);
	}
	if (e->mHasAcceleration) {
		destroyDreamMugenAssignment(e->mAcceleration);
	}
	if (e->mHasRandomOffset) {
		destroyDreamMugenAssignment(e->mRandomOffset);
	}
	if (e->mHasRemoveTime) {
		destroyDreamMugenAssignment(e->mRemoveTime);
	}
	if (e->mHasSuperMove) {
		destroyDreamMugenAssignment(e->mSuperMove);
	}
	if (e->mHasSuperMoveTime) {
		destroyDreamMugenAssignment(e->mSuperMoveTime);
	}
	if (e->mHasPauseMoveTime) {
		destroyDreamMugenAssignment(e->mPauseMoveTime);
	}
	if (e->mHasScale) {
		destroyDreamMugenAssignment(e->mScale);
	}
	if (e->mHasSpritePriority) {
		destroyDreamMugenAssignment(e->mSpritePriority);
	}
	if (e->mHasOnTop) {
		destroyDreamMugenAssignment(e->mOnTop);
	}
	if (e->mHasShadow) {
		destroyDreamMugenAssignment(e->mShadow);
	}
	if (e->mHasOwnPalette) {
		destroyDreamMugenAssignment(e->mOwnPalette);
	}
	if (e->mHasIsRemovedOnGetHit) {
		destroyDreamMugenAssignment(e->mIsRemovedOnGetHit);
	}
	if (e->mHasIgnoreHitPause) {
		destroyDreamMugenAssignment(e->mIgnoreHitPause);
	}
	if (e->mHasTransparencyType) {
		destroyDreamMugenAssignment(e->mTransparencyType);
	}

	freeMemory(e);
}

typedef struct {
	DreamMugenAssignment* mValue;
} PositionFreezeController;

static void parsePositionFreezeController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) {
	PositionFreezeController* e = (PositionFreezeController*)allocMemoryOnMemoryStackOrMemory(sizeof(PositionFreezeController));

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("value", tGroup, &e->mValue, "1");

	tController->mType = MUGEN_STATE_CONTROLLER_TYPE_FREEZE_POSITION;
	tController->mData = e;
}

static void unloadPositionFreezeController(DreamMugenStateController* tController) {
	PositionFreezeController* e = (PositionFreezeController*)tController->mData;
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

static void readMugenDefStringVector(MugenStringVector* tDst, MugenDefScriptGroup* tGroup, const char* tName, uint8_t* oHasValue) {
	if (stl_string_map_contains_array(tGroup->mElements, tName)) {
		MugenDefScriptGroupElement* elem = &tGroup->mElements[tName];
		*oHasValue = 1;
		if (isMugenDefStringVectorVariableAsElement(elem)) {
			*tDst = copyMugenDefStringVectorVariableAsElement(elem);
		}
		else {
			char* text = getAllocatedMugenDefStringVariableAsElement(elem);
			tDst->mSize = 1;
			tDst->mElement = (char**)allocMemoryOnMemoryStackOrMemory(sizeof(char*));
			tDst->mElement[0] = (char*)allocMemoryOnMemoryStackOrMemory(uint32_t(strlen(text) + 10));
			strcpy(tDst->mElement[0], text);
			freeMemory(text);
		}
	}
	else {
		*oHasValue = 0;
	}

}

static void parseNotHitByController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup, DreamMugenStateControllerType tType) {
	NotHitByController* e = (NotHitByController*)allocMemoryOnMemoryStackOrMemory(sizeof(NotHitByController));

	readMugenDefStringVector(&e->mValue, tGroup, "value", &e->mHasValue);
	readMugenDefStringVector(&e->mValue2, tGroup, "value2", &e->mHasValue2);
	assert(e->mHasValue || e->mHasValue2);

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("time", tGroup, &e->mTime, "1");
	
	tController->mType = tType;
	tController->mData = e;
}

static void unloadMugenDefStringVector(MugenStringVector& tVector) {
	int i;
	for (i = 0; i < tVector.mSize; i++) {
		freeMemory(tVector.mElement[i]);
	}

	freeMemory(tVector.mElement);
}

static void unloadNotHitByController(DreamMugenStateController* tController) {
	NotHitByController* e = (NotHitByController*)tController->mData;

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
	HitFallSetController* e = (HitFallSetController*)allocMemoryOnMemoryStackOrMemory(sizeof(HitFallSetController));

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("value", tGroup, &e->mValue, "-1");
	e->mHasXVelocity = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("xvel", tGroup, &e->mXVelocity);
	e->mHasYVelocity = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("yvel", tGroup, &e->mYVelocity);

	tController->mType = MUGEN_STATE_CONTROLLER_TYPE_SET_HIT_FALL;
	tController->mData = e;
}

static void unloadHitFallSetController(DreamMugenStateController* tController) {
	HitFallSetController* e = (HitFallSetController*)tController->mData;

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
	SingleRequiredValueController* e = (SingleRequiredValueController*)allocMemoryOnMemoryStackOrMemory(sizeof(SingleRequiredValueController));

	fetchDreamAssignmentFromGroupAndReturnWhetherItExists("value", tGroup, &e->mValue);

	tController->mType = tType;
	tController->mData = e;
}

static void unloadSingleRequiredValueController(DreamMugenStateController* tController) {
	SingleRequiredValueController* e = (SingleRequiredValueController*)tController->mData;
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
	EnvironmentShakeController* e = (EnvironmentShakeController*)allocMemoryOnMemoryStackOrMemory(sizeof(EnvironmentShakeController));

	fetchDreamAssignmentFromGroupAndReturnWhetherItExists("time", tGroup, &e->mTime);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("freq", tGroup, &e->mFrequency);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("ampl", tGroup, &e->mAmplitude);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("phase", tGroup, &e->mPhaseOffset);

	tController->mType = MUGEN_STATE_CONTROLLER_TYPE_ENVIRONMENT_SHAKE;
	tController->mData = e;
}

static void unloadEnvironmentShakeController(DreamMugenStateController* tController) {
	EnvironmentShakeController* e = (EnvironmentShakeController*)tController->mData;

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
	SuperPauseController* e = (SuperPauseController*)allocMemoryOnMemoryStackOrMemory(sizeof(SuperPauseController));

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("time", tGroup, &e->mTime);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("endcmdbuftime", tGroup, &e->mBufferTimeForCommandsDuringPauseEnd);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("movetime", tGroup, &e->mMoveTime);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("pausebg", tGroup, &e->mDoesPauseBackground);

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("anim", tGroup, &e->mAnim);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("sound", tGroup, &e->mSound);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("pos", tGroup, &e->mPosition);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("darken", tGroup, &e->mIsDarkening);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("p2defmul", tGroup, &e->mPlayer2DefenseMultiplier);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("poweradd", tGroup, &e->mPowerToAdd);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("unhittable", tGroup, &e->mSetPlayerUnhittable);

	tController->mType = MUGEN_STATE_CONTROLLER_TYPE_SUPER_PAUSE;
	tController->mData = e;
}

static void unloadSuperPauseController(DreamMugenStateController* tController) {
	SuperPauseController* e = (SuperPauseController*)tController->mData;

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
	DreamMugenAssignment* mName;
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

static void parseHelperController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) {
	HelperController* e = (HelperController*)allocMemoryOnMemoryStackOrMemory(sizeof(HelperController));

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("helpertype", tGroup, &e->mType, "normal");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("name", tGroup, &e->mType, "normal");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("id", tGroup, &e->mID);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("pos", tGroup, &e->mPosition);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("postype", tGroup, &e->mPositionType, "p1");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("facing", tGroup, &e->mFacing);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("stateno", tGroup, &e->mStateNumber);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("keyctrl", tGroup, &e->mCanControl);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("ownpal", tGroup, &e->mHasOwnPalette);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("supermovetime", tGroup, &e->mSuperMoveTime);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("pausemovetime", tGroup, &e->mPauseMoveTime);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("size.xscale", tGroup, &e->mSizeScaleX);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("size.yscale", tGroup, &e->mSizeScaleY);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("size.ground.back", tGroup, &e->mSizeGroundBack);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("size.ground.front", tGroup, &e->mSizeGroundFront);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("size.air.back", tGroup, &e->mSizeAirBack);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("size.air.front", tGroup, &e->mSizeAirFront);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("size.height", tGroup, &e->mSizeHeight);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("size.proj.doscale", tGroup, &e->mSizeProjectilesDoScale);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("size.head.pos", tGroup, &e->mSizeHeadPosition);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("size.mid.pos", tGroup, &e->mSizeMiddlePosition);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("size.shadowoffset", tGroup, &e->mSizeShadowOffset);

	tController->mType = MUGEN_STATE_CONTROLLER_TYPE_HELPER;
	tController->mData = e;
}

static void unloadHelperController(DreamMugenStateController* tController) {
	HelperController* e = (HelperController*)tController->mData;

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
	LifeAddController* e = (LifeAddController*)allocMemoryOnMemoryStackOrMemory(sizeof(LifeAddController));

	fetchDreamAssignmentFromGroupAndReturnWhetherItExists("value", tGroup, &e->mValue);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("kill", tGroup, &e->mCanKill);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("absolute", tGroup, &e->mIsAbsolute);

	tController->mType = MUGEN_STATE_CONTROLLER_TYPE_ADD_LIFE;
	tController->mData = e;
}

static void unloadLifeAddController(DreamMugenStateController* tController) {
	LifeAddController* e = (LifeAddController*)tController->mData;

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
	TargetLifeAddController* e = (TargetLifeAddController*)allocMemoryOnMemoryStackOrMemory(sizeof(TargetLifeAddController));

	fetchDreamAssignmentFromGroupAndReturnWhetherItExists("value", tGroup, &e->mValue);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("id", tGroup, &e->mID);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("kill", tGroup, &e->mCanKill);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("absolute", tGroup, &e->mIsAbsolute);

	tController->mType = MUGEN_STATE_CONTROLLER_TYPE_ADD_TARGET_LIFE;
	tController->mData = e;
}

static void unloadTargetLifeAddController(DreamMugenStateController* tController) {
	TargetLifeAddController* e = (TargetLifeAddController*)tController->mData;

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
	RemoveExplodController* e = (RemoveExplodController*)allocMemoryOnMemoryStackOrMemory(sizeof(RemoveExplodController));

	e->mHasID = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("id", tGroup, &e->mID);

	tController->mType = MUGEN_STATE_CONTROLLER_TYPE_REMOVE_EXPLOD;
	tController->mData = e;
}

static void unloadRemoveExplodController(DreamMugenStateController* tController) {
	RemoveExplodController* e = (RemoveExplodController*)tController->mData;

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
	AngleDrawController* e = (AngleDrawController*)allocMemoryOnMemoryStackOrMemory(sizeof(AngleDrawController));

	e->mHasValue = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("value", tGroup, &e->mValue);
	e->mHasScale = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("scale", tGroup, &e->mScale);

	tController->mType = MUGEN_STATE_CONTROLLER_TYPE_DRAW_ANGLE;
	tController->mData = e;
}

static void unloadAngleDrawController(DreamMugenStateController* tController) {
	AngleDrawController* e = (AngleDrawController*)tController->mData;

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
	BindController* e = (BindController*)allocMemoryOnMemoryStackOrMemory(sizeof(BindController));

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("time", tGroup, &e->mTime);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("facing", tGroup, &e->mFacing);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("pos", tGroup, &e->mPosition);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("id", tGroup, &e->mID);

	tController->mType = tType;
	tController->mData = e;
}

static void unloadBindController(DreamMugenStateController* tController) {
	BindController* e = (BindController*)tController->mData;

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
	ScreenBoundController* e = (ScreenBoundController*)allocMemoryOnMemoryStackOrMemory(sizeof(ScreenBoundController));

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("value", tGroup, &e->mValue);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("movecamera", tGroup, &e->mMoveCameraFlags);

	tController->mType = MUGEN_STATE_CONTROLLER_TYPE_SCREEN_BOUND;
	tController->mData = e;
}

static void unloadScreenBoundController(DreamMugenStateController* tController) {
	ScreenBoundController* e = (ScreenBoundController*)tController->mData;

	destroyDreamMugenAssignment(e->mValue);
	destroyDreamMugenAssignment(e->mMoveCameraFlags);

	freeMemory(e);
}

typedef struct {
	DreamMugenAssignment* mValue;
	DreamMugenAssignment* mID;

} SetTargetFacingController;

static void parseSetTargetFacingController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) {
	SetTargetFacingController* e = (SetTargetFacingController*)allocMemoryOnMemoryStackOrMemory(sizeof(SetTargetFacingController));

	fetchDreamAssignmentFromGroupAndReturnWhetherItExists("value", tGroup, &e->mValue);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("id", tGroup, &e->mID);

	tController->mType = MUGEN_STATE_CONTROLLER_TYPE_SET_TARGET_FACING;
	tController->mData = e;
}

static void unloadSetTargetFacingController(DreamMugenStateController* tController) {
	SetTargetFacingController* e = (SetTargetFacingController*)tController->mData;

	destroyDreamMugenAssignment(e->mValue);
	destroyDreamMugenAssignment(e->mID);

	freeMemory(e);
}

typedef struct {
	DreamMugenAssignment* mAttributes;
	DreamMugenAssignment* mPauseTime;
	DreamMugenAssignment* mSparkNumber;
	DreamMugenAssignment* mSparkXY;
	DreamMugenAssignment* mHitSound;
	int mHasP1StateNo;
	DreamMugenAssignment* mP1StateNo;
	int mHasP2StateNo;
	DreamMugenAssignment* mP2StateNo;
} ReversalDefinitionController;

static void parseReversalDefinitionController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) {
	ReversalDefinitionController* e = (ReversalDefinitionController*)allocMemoryOnMemoryStackOrMemory(sizeof(ReversalDefinitionController));

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("reversal.attr", tGroup, &e->mAttributes);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("pausetime", tGroup, &e->mPauseTime);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("sparkno", tGroup, &e->mSparkNumber);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("sparkxy", tGroup, &e->mSparkXY);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("hitsound", tGroup, &e->mHitSound);
	e->mHasP1StateNo = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("p1stateno", tGroup, &e->mP1StateNo);
	e->mHasP2StateNo = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("p2stateno", tGroup, &e->mP2StateNo);

	tController->mType = MUGEN_STATE_CONTROLLER_TYPE_REVERSAL_DEFINITION;
	tController->mData = e;
}

static void unloadReversalDefinitionController(DreamMugenStateController* tController) {
	ReversalDefinitionController* e = (ReversalDefinitionController*)tController->mData;

	destroyDreamMugenAssignment(e->mAttributes);
	destroyDreamMugenAssignment(e->mPauseTime);
	destroyDreamMugenAssignment(e->mSparkNumber);
	destroyDreamMugenAssignment(e->mSparkXY);
	destroyDreamMugenAssignment(e->mHitSound);
	if (e->mHasP1StateNo) {
		destroyDreamMugenAssignment(e->mP1StateNo);
	}
	if (e->mHasP2StateNo) {
		destroyDreamMugenAssignment(e->mP2StateNo);
	}

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
	AfterImageController mAfterImage;
} ProjectileController;

static void parseProjectileController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) {
	ProjectileController* e = (ProjectileController*)allocMemoryOnMemoryStackOrMemory(sizeof(ProjectileController));
	readHitDefinitionFromGroup(&e->mHitDef, tGroup);

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("projid", tGroup, &e->mID);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("projanim", tGroup, &e->mAnimation);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("projhitanim", tGroup, &e->mHitAnimation);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("projremanim", tGroup, &e->mRemoveAnimation);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("projcancelanim", tGroup, &e->mCancelAnimation);

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("projscale", tGroup, &e->mScale);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("projremove", tGroup, &e->mIsRemovingProjectileAfterHit);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("projremovetime", tGroup, &e->mRemoveTime);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("velocity", tGroup, &e->mVelocity);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("remvelocity", tGroup, &e->mRemoveVelocity);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("accel", tGroup, &e->mAcceleration);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("velmul", tGroup, &e->mVelocityMultipliers);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("projhits", tGroup, &e->mHitAmountBeforeVanishing);

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("projmisstime", tGroup, &e->mMissTime);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("projpriority", tGroup, &e->mPriority);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("projsprpriority", tGroup, &e->mSpriteSpriority);

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("projedgebound", tGroup, &e->mEdgeBound);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("projstagebound", tGroup, &e->mStageBound);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("projheightbound", tGroup, &e->mHeightBoundValues);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("offset", tGroup, &e->mOffset);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("postype", tGroup, &e->mPositionType);

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("projshadow", tGroup, &e->mShadow);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("supermovetime", tGroup, &e->mSuperMoveTime);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("pausemovetime", tGroup, &e->mPauseMoveTime);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("ownpal", tGroup, &e->mHasOwnPalette);

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("remappal", tGroup, &e->mRemapPalette);

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("afterimage.time", tGroup, &e->mAfterImage.mTime);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("afterimage.length", tGroup, &e->mAfterImage.mLength);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("afterimage.palcolor", tGroup, &e->mAfterImage.mPalColor);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("afterimage.palinvertall", tGroup, &e->mAfterImage.mPalInvertAll);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("afterimage.palbright", tGroup, &e->mAfterImage.mPalBright);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("afterimage.palcontrast", tGroup, &e->mAfterImage.mPalContrast);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("afterimage.palpostbright", tGroup, &e->mAfterImage.mPalPostBright);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("afterimage.paladd", tGroup, &e->mAfterImage.mPalAdd);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("afterimage.palmul", tGroup, &e->mAfterImage.mPalMul);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("afterimage.timegap", tGroup, &e->mAfterImage.mTimeGap);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("afterimage.framegap", tGroup, &e->mAfterImage.mFrameGap);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("afterimage.trans", tGroup, &e->mAfterImage.mTrans, "none");

	tController->mType = MUGEN_STATE_CONTROLLER_TYPE_PROJECTILE;
	tController->mData = e;
}

static void unloadProjectileController(DreamMugenStateController* tController) {
	ProjectileController* e = (ProjectileController*)tController->mData;
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

	destroyDreamMugenAssignment(e->mAfterImage.mTime);
	destroyDreamMugenAssignment(e->mAfterImage.mLength);
	destroyDreamMugenAssignment(e->mAfterImage.mPalColor);
	destroyDreamMugenAssignment(e->mAfterImage.mPalInvertAll);
	destroyDreamMugenAssignment(e->mAfterImage.mPalBright);
	destroyDreamMugenAssignment(e->mAfterImage.mPalContrast);
	destroyDreamMugenAssignment(e->mAfterImage.mPalPostBright);
	destroyDreamMugenAssignment(e->mAfterImage.mPalAdd);
	destroyDreamMugenAssignment(e->mAfterImage.mPalMul);
	destroyDreamMugenAssignment(e->mAfterImage.mTimeGap);
	destroyDreamMugenAssignment(e->mAfterImage.mFrameGap);
	destroyDreamMugenAssignment(e->mAfterImage.mTrans);

	freeMemory(e);
}

static void parseAfterImageController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) {
	AfterImageController* e = (AfterImageController*)allocMemoryOnMemoryStackOrMemory(sizeof(AfterImageController));
	
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("time", tGroup, &e->mTime);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("length", tGroup, &e->mLength);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("palcolor", tGroup, &e->mPalColor);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("palinvertall", tGroup, &e->mPalInvertAll);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("palbright", tGroup, &e->mPalBright);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("palcontrast", tGroup, &e->mPalContrast);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("palpostbright", tGroup, &e->mPalPostBright);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("paladd", tGroup, &e->mPalAdd);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("palmul", tGroup, &e->mPalMul);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("timegap", tGroup, &e->mTimeGap);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("framegap", tGroup, &e->mFrameGap);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("trans", tGroup, &e->mTrans, "none");

	tController->mType = MUGEN_STATE_CONTROLLER_TYPE_AFTER_IMAGE;
	tController->mData = e;
}

static void unloadAfterImageController(DreamMugenStateController* tController) {
	AfterImageController* e = (AfterImageController*)tController->mData;

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
	destroyDreamMugenAssignment(e->mFrameGap);
	destroyDreamMugenAssignment(e->mTrans);

	freeMemory(e);
}


typedef struct {
	DreamMugenAssignment* mTime;
} AfterImageTimeController;

static void parseAfterImageTimeController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) {
	AfterImageTimeController* e = (AfterImageTimeController*)allocMemoryOnMemoryStackOrMemory(sizeof(AfterImageTimeController));

	if (isMugenDefNumberVariableAsGroup(tGroup, "time")) {
		fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("time", tGroup, &e->mTime);
	}
	else {
		fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("value", tGroup, &e->mTime);
	}

	tController->mType = MUGEN_STATE_CONTROLLER_TYPE_AFTER_IMAGE_TIME;
	tController->mData = e;
}

static void unloadAfterImageTimeController(DreamMugenStateController* tController) {
	AfterImageTimeController* e = (AfterImageTimeController*)tController->mData;
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
	PalFXController* e = (PalFXController*)allocMemoryOnMemoryStackOrMemory(sizeof(PalFXController));

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("time", tGroup, &e->mTime);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("add", tGroup, &e->mAdd);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("mul", tGroup, &e->mMul);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("sinadd", tGroup, &e->mSinAdd);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("invertall", tGroup, &e->mInvertAll);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("color", tGroup, &e->mColor);

	tController->mType = tType;
	tController->mData = e;
}

static void unloadPalFXController(DreamMugenStateController* tController) {
	PalFXController* e = (PalFXController*)tController->mData;

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
	ClipboardController* e = (ClipboardController*)allocMemoryOnMemoryStackOrMemory(sizeof(ClipboardController));

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("text", tGroup, &e->mText);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("params", tGroup, &e->mParams);

	tController->mType = tType;
	tController->mData = e;
}

static void unloadClipboardController(DreamMugenStateController* tController) {
	ClipboardController* e = (ClipboardController*)tController->mData;

	destroyDreamMugenAssignment(e->mText);
	destroyDreamMugenAssignment(e->mParams);

	freeMemory(e);
}

typedef struct {
	DreamMugenAssignment* mRecursive;
	DreamMugenAssignment* mRemoveExplods;

} DestroySelfController;

static void parseDestroySelfController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) {
	DestroySelfController* e = (DestroySelfController*)allocMemoryOnMemoryStackOrMemory(sizeof(DestroySelfController));

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("recursive", tGroup, &e->mRecursive);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("removeexplods", tGroup, &e->mRemoveExplods);

	tController->mType = MUGEN_STATE_CONTROLLER_TYPE_DESTROY_SELF;
	tController->mData = e;
}

static void unloadDestroySelfController(DreamMugenStateController* tController) {
	DestroySelfController* e = (DestroySelfController*)tController->mData;

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
	EnvironmentColorController* e = (EnvironmentColorController*)allocMemoryOnMemoryStackOrMemory(sizeof(EnvironmentColorController));

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("value", tGroup, &e->mValue, "255 , 255 , 255");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("time", tGroup, &e->mTime);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("under", tGroup, &e->mUnder);

	tController->mType = MUGEN_STATE_CONTROLLER_TYPE_ENVIRONMENT_COLOR;
	tController->mData = e;
	
}

static void unloadEnvColorController(DreamMugenStateController* tController) {
	EnvironmentColorController* e = (EnvironmentColorController*)tController->mData;

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
	ExplodBindTimeController* e = (ExplodBindTimeController*)allocMemoryOnMemoryStackOrMemory(sizeof(ExplodBindTimeController));

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("id", tGroup, &e->mID);

	if (isMugenDefNumberVariableAsGroup(tGroup, "time")) {
		fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("time", tGroup, &e->mTime);
	}
	else {
		fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("value", tGroup, &e->mTime);
	}

	tController->mType = MUGEN_STATE_CONTROLLER_TYPE_EXPLOD_BIND_TIME;
	tController->mData = e;
}

static void unloadExplodBindTimeController(DreamMugenStateController* tController) {
	ExplodBindTimeController* e = (ExplodBindTimeController*)tController->mData;

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
	GameMakeAnimController* e = (GameMakeAnimController*)allocMemoryOnMemoryStackOrMemory(sizeof(GameMakeAnimController));

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("value", tGroup, &e->mValue);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("under", tGroup, &e->mIsUnderPlayer);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("pos", tGroup, &e->mPosOffset);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("random", tGroup, &e->mRandomOffset);

	tController->mType = MUGEN_STATE_CONTROLLER_TYPE_MAKE_GAME_ANIMATION;
	tController->mData = e;
}

static void unloadGameMakeAnimController(DreamMugenStateController* tController) {
	GameMakeAnimController* e = (GameMakeAnimController*)tController->mData;

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
	HitOverrideController* e = (HitOverrideController*)allocMemoryOnMemoryStackOrMemory(sizeof(HitOverrideController));

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("attr", tGroup, &e->mAttributeString);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("stateno", tGroup, &e->mStateNo);

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("slot", tGroup, &e->mSlot);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("time", tGroup, &e->mTime);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("forceair", tGroup, &e->mForceAir);

	tController->mType = MUGEN_STATE_CONTROLLER_TYPE_HIT_OVERRIDE;
	tController->mData = e;
}

static void unloadHitOverrideController(DreamMugenStateController* tController) {
	HitOverrideController* e = (HitOverrideController*)tController->mData;

	destroyDreamMugenAssignment(e->mAttributeString);
	destroyDreamMugenAssignment(e->mStateNo);

	destroyDreamMugenAssignment(e->mSlot);
	destroyDreamMugenAssignment(e->mTime);
	destroyDreamMugenAssignment(e->mForceAir);

	freeMemory(e);
}

typedef struct {
	DreamMugenAssignment* mTime;
	DreamMugenAssignment* mBufferTimeForCommandsDuringPauseEnd;
	DreamMugenAssignment* mMoveTime;
	DreamMugenAssignment* mDoesPauseBackground;
} PauseController;

static void parsePauseController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) {
	PauseController* e = (PauseController*)allocMemoryOnMemoryStackOrMemory(sizeof(PauseController));

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("time", tGroup, &e->mTime);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("endcmdbuftime", tGroup, &e->mBufferTimeForCommandsDuringPauseEnd);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("movetime", tGroup, &e->mMoveTime);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("pausebg", tGroup, &e->mDoesPauseBackground);

	tController->mType = MUGEN_STATE_CONTROLLER_TYPE_PAUSE;
	tController->mData = e;
}

static void unloadPauseController(DreamMugenStateController* tController) {
	PauseController* e = (PauseController*)tController->mData;

	destroyDreamMugenAssignment(e->mTime);
	destroyDreamMugenAssignment(e->mBufferTimeForCommandsDuringPauseEnd);
	destroyDreamMugenAssignment(e->mMoveTime);
	destroyDreamMugenAssignment(e->mDoesPauseBackground);

	freeMemory(e);
}

typedef struct {
	DreamMugenAssignment* mSource;
	DreamMugenAssignment* mDestination;
} RemapPaletteController;

static void parseRemapPaletteController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) {
	RemapPaletteController* e = (RemapPaletteController*)allocMemoryOnMemoryStackOrMemory(sizeof(RemapPaletteController));

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("source", tGroup, &e->mSource);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("dest", tGroup, &e->mDestination);

	tController->mType = MUGEN_STATE_CONTROLLER_TYPE_REMAP_PALETTE;
	tController->mData = e;
}

static void unloadRemapPaletteController(DreamMugenStateController* tController) {
	RemapPaletteController* e = (RemapPaletteController*)tController->mData;

	destroyDreamMugenAssignment(e->mSource);
	destroyDreamMugenAssignment(e->mDestination);

	freeMemory(e);
}

typedef struct {
	DreamMugenAssignment* mChannel;
	int mIsAbspan;
	DreamMugenAssignment* mPan;
} SoundPanController;

static void parseSoundPanController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup, DreamMugenStateControllerType tType) {
	SoundPanController* e = (SoundPanController*)allocMemoryOnMemoryStackOrMemory(sizeof(SoundPanController));

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("channel", tGroup, &e->mChannel);
	e->mIsAbspan = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("abspan", tGroup, &e->mPan);
	if (!e->mIsAbspan)
	{
		fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("pan", tGroup, &e->mPan);
	}

	tController->mType = tType;
	tController->mData = e;
}

static void unloadSoundPanController(DreamMugenStateController* tController) {
	SoundPanController* e = (SoundPanController*)tController->mData;

	destroyDreamMugenAssignment(e->mChannel);
	destroyDreamMugenAssignment(e->mPan);

	freeMemory(e);
}

typedef struct {
	DreamMugenAssignment* mChannel;
} SoundStopController;

static void parseStopSoundController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup, DreamMugenStateControllerType tType) {
	SoundStopController* e = (SoundStopController*)allocMemoryOnMemoryStackOrMemory(sizeof(SoundStopController));

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("channel", tGroup, &e->mChannel);

	tController->mType = tType;
	tController->mData = e;
}

static void unloadStopSoundController(DreamMugenStateController* tController) {
	SoundStopController* e = (SoundStopController*)tController->mData;

	destroyDreamMugenAssignment(e->mChannel);

	freeMemory(e);
}

typedef struct {
	DreamMugenAssignment* mExcludeID;
	DreamMugenAssignment* mKeepOne;

} TargetDropController;

static void parseTargetDropController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) {
	TargetDropController* e = (TargetDropController*)allocMemoryOnMemoryStackOrMemory(sizeof(TargetDropController));

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("excludeid", tGroup, &e->mExcludeID);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("keepone", tGroup, &e->mKeepOne);

	tController->mType = MUGEN_STATE_CONTROLLER_TYPE_DROP_TARGET;
	tController->mData = e;
}

static void unloadTargetDropController(DreamMugenStateController* tController) {
	TargetDropController* e = (TargetDropController*)tController->mData;

	destroyDreamMugenAssignment(e->mExcludeID);
	destroyDreamMugenAssignment(e->mKeepOne);

	freeMemory(e);
}


typedef struct {
	DreamMugenAssignment* mValue;
	DreamMugenAssignment* mID;
} TargetPowerAddController;

static void parseTargetPowerAddController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) {
	TargetPowerAddController* e = (TargetPowerAddController*)allocMemoryOnMemoryStackOrMemory(sizeof(TargetPowerAddController));

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("value", tGroup, &e->mValue);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("id", tGroup, &e->mID);

	tController->mType = MUGEN_STATE_CONTROLLER_TYPE_ADD_TARGET_POWER;
	tController->mData = e;
}

static void unloadTargetPowerAddController(DreamMugenStateController* tController) {
	TargetPowerAddController* e = (TargetPowerAddController*)tController->mData;

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
	Target2DPhysicsController* e = (Target2DPhysicsController*)allocMemoryOnMemoryStackOrMemory(sizeof(Target2DPhysicsController));

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("id", tGroup, &e->mID);
	e->mIsSettingX = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("x", tGroup, &e->x);
	e->mIsSettingY = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("y", tGroup, &e->y);

	tController->mType = tType;
	tController->mData = e;
}

static void unloadTarget2DPhysicsController(DreamMugenStateController* tController) {
	Target2DPhysicsController* e = (Target2DPhysicsController*)tController->mData;

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
	TransparencyController* e = (TransparencyController*)allocMemoryOnMemoryStackOrMemory(sizeof(TransparencyController));

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("trans", tGroup, &e->mTransparency, "default");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("alpha", tGroup, &e->mAlpha);

	tController->mType = MUGEN_STATE_CONTROLLER_TYPE_TRANSPARENCY;
	tController->mData = e;
}

static void unloadTransparencyController(DreamMugenStateController* tController) {
	TransparencyController* e = (TransparencyController*)tController->mData;

	destroyDreamMugenAssignment(e->mTransparency);
	destroyDreamMugenAssignment(e->mAlpha);

	freeMemory(e);
}

typedef struct {
	DreamMugenAssignment* mValue;
	DreamMugenAssignment* mRange;
} VarRandomController;

static void parseVarRandomController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) {
	VarRandomController* e = (VarRandomController*)allocMemoryOnMemoryStackOrMemory(sizeof(VarRandomController));

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("v", tGroup, &e->mValue);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("range", tGroup, &e->mRange, "0 , 1000");

	tController->mType = MUGEN_STATE_CONTROLLER_TYPE_SET_VARIABLE_RANDOM;
	tController->mData = e;
}

static void unloadVarRandomController(DreamMugenStateController* tController) {
	VarRandomController* e = (VarRandomController*)tController->mData;

	destroyDreamMugenAssignment(e->mValue);
	destroyDreamMugenAssignment(e->mRange);

	freeMemory(e);
}

typedef struct {
	DreamMugenAssignment* mValue;
} VictoryQuoteController;

static void parseVictoryQuoteController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) {
	VictoryQuoteController* e = (VictoryQuoteController*)allocMemoryOnMemoryStackOrMemory(sizeof(VictoryQuoteController));

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("value", tGroup, &e->mValue);

	tController->mType = MUGEN_STATE_CONTROLLER_TYPE_VICTORY_QUOTE;
	tController->mData = e;
}

static void unloadVictoryQuoteController(DreamMugenStateController* tController) {
	VictoryQuoteController* e = (VictoryQuoteController*)tController->mData;
	destroyDreamMugenAssignment(e->mValue);
	freeMemory(e);
}

static void parseStateControllerType(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) {
	assert(stl_string_map_contains_array(tGroup->mElements, "type"));
	MugenDefScriptGroupElement* e = &tGroup->mElements["type"];
	tController->mData = NULL;

	char* type = getAllocatedMugenDefStringVariableAsElement(e);
	turnStringLowercase(type);

	if(!stl_string_map_contains_array(gMugenStateControllerVariableHandler.mStateControllerParsers, type)) {
		logWarningFormat("Unable to determine state controller type %s. Defaulting to null.", type);
		freeMemory(type);
		type = (char*)allocMemory(10);
		strcpy(type, "null");
	}

	StateControllerParseFunction func = gMugenStateControllerVariableHandler.mStateControllerParsers[type];
	func(tController, tGroup);

	freeMemory(type);
}

static void parseStateControllerPersistence(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) {
	tController->mPersistence = (int16_t)getMugenDefIntegerOrDefaultAsGroup(tGroup, "persistent", 1);
	tController->mAccessAmount = 0;
}

static void parseStateControllerTarget(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) {
	if (!isMugenDefStringVariableAsGroup(tGroup, "override.target"))
	{
		tController->mTarget = MUGEN_STATE_CONTROLLER_TARGET_DEFAULT;
	}
	else
	{
		const auto value = getSTLMugenDefStringVariableAsGroup(tGroup, "override.target");
		if (value == "p1") {
			tController->mTarget = MUGEN_STATE_CONTROLLER_TARGET_PLAYER1;
		}
		else if (value == "p2") {
			tController->mTarget = MUGEN_STATE_CONTROLLER_TARGET_PLAYER2;
		}
		else {
			tController->mTarget = MUGEN_STATE_CONTROLLER_TARGET_DEFAULT;
		}
	}
}

int gDebugStateControllerAmount;

DreamMugenStateController * parseDreamMugenStateControllerFromGroup(MugenDefScriptGroup* tGroup)
{
	DreamMugenStateController* ret = (DreamMugenStateController*)allocMemoryOnMemoryStackOrMemory(sizeof(DreamMugenStateController));
	gDebugStateControllerAmount++;
	parseStateControllerType(ret, tGroup);
	parseStateControllerTriggers(ret, tGroup);
	parseStateControllerPersistence(ret, tGroup);
	parseStateControllerTarget(ret, tGroup);

	return ret;
}

static void unloadStateControllerType(DreamMugenStateController* tController) {
	if (!stl_map_contains(gMugenStateControllerVariableHandler.mStateControllerUnloaders, (int)tController->mType)) {
		logWarningFormat("Unable to determine state controller type %d. Defaulting to null.", tController->mType);
		tController->mType = MUGEN_STATE_CONTROLLER_TYPE_NULL;
	}

	StateControllerUnloadFunction func = gMugenStateControllerVariableHandler.mStateControllerUnloaders[tController->mType];
	func(tController);
}

void unloadDreamMugenStateController(DreamMugenStateController * tController)
{
	destroyDreamMugenAssignment(tController->mTrigger.mAssignment);
	unloadStateControllerType(tController);
}

static void handleHelperSetOneIntegerElement(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, DreamPlayer* tHelper, void(tFunc)(DreamPlayer*, int), int tDefault) {
	int val;
	evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(tAssignment, tPlayer, &val, tDefault);
	tFunc(tHelper, val);
}

static void handleHelperSetOneIntegerElementWithCoordinateP(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, DreamPlayer* tHelper, void(tFunc)(DreamPlayer*, int, int), int tDefault) {
	int val;
	evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(tAssignment, tPlayer, &val, tDefault);
	tFunc(tHelper, val, getActiveStateMachineCoordinateP());
}

static void handleHelperSetTwoIntegerElements(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, DreamPlayer* tHelper, void(tFunc)(DreamPlayer*, int, int), int tDefault1, int tDefault2) {
	int val1, val2;
	evaluateDreamAssignmentAndReturnAsTwoIntegersWithDefaultValues(tAssignment, tPlayer, &val1, &val2, tDefault1, tDefault2);
	tFunc(tHelper, val1, val2);
}

static void handleHelperSetTwoIntegerElementsWithCoordinateP(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, DreamPlayer* tHelper, void(tFunc)(DreamPlayer*, int, int, int), int tDefault1, int tDefault2) {
	int val1, val2;
	evaluateDreamAssignmentAndReturnAsTwoIntegersWithDefaultValues(tAssignment, tPlayer, &val1, &val2, tDefault1, tDefault2);
	tFunc(tHelper, val1, val2, getActiveStateMachineCoordinateP());
}

static void handleHelperSetTwoOptionalIntegerElements(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, DreamPlayer* tHelper, void(tFunc)(DreamPlayer*, int, int), void(tFuncDisable)(DreamPlayer*)) {
	if (!(*tAssignment)) {
		tFuncDisable(tHelper);
		return;
	}

	int x, y;
	evaluateDreamAssignmentAndReturnAsTwoIntegersWithDefaultValues(tAssignment, tPlayer, &x, &y, 0, 0);

	tFunc(tHelper, x, y);
}

static void handleHelperSetThreeIntegerElements(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, DreamPlayer* tHelper, void(tFunc)(DreamPlayer*, int, int, int), int tDefault1, int tDefault2, int tDefault3) {
	int val1, val2, val3;
	evaluateDreamAssignmentAndReturnAsThreeIntegersWithDefaultValues(tAssignment, tPlayer, &val1, &val2, &val3, tDefault1, tDefault2, tDefault3);
	tFunc(tHelper, val1, val2, val3);
}

static void handleHelperSetOneFloatElement(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, DreamPlayer* tHelper, void(tFunc)(DreamPlayer*, double), double tDefault) {
	double val;
	evaluateDreamAssignmentAndReturnAsOneFloatWithDefaultValue(tAssignment, tPlayer, &val, tDefault);
	tFunc(tHelper, val);
}

static void handleHelperSetTwoFloatElements(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, DreamPlayer* tHelper, void(tFunc)(DreamPlayer*, double, double), double tDefault1, double tDefault2) {
	double val1, val2;
	evaluateDreamAssignmentAndReturnAsTwoFloatsWithDefaultValues(tAssignment, tPlayer, &val1, &val2, tDefault1, tDefault2);
	tFunc(tHelper, val1, val2);
}

static void handleHelperSetTwoFloatElementsWithCoordinateP(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, DreamPlayer* tHelper, void(tFunc)(DreamPlayer*, double, double, int), double tDefault1, double tDefault2) {
	double val1, val2;
	evaluateDreamAssignmentAndReturnAsTwoFloatsWithDefaultValues(tAssignment, tPlayer, &val1, &val2, tDefault1, tDefault2);
	tFunc(tHelper, val1, val2, getActiveStateMachineCoordinateP());
}

static void handlePlayerSetOneIntegerElement(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, void(tFunc)(DreamPlayer*, int), int tDefault) {
	handleHelperSetOneIntegerElement(tAssignment, tPlayer, tPlayer, tFunc, tDefault);
}

static void handlePlayerSetOneFloatElement(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, void(tFunc)(DreamPlayer*, double), double tDefault) {
	handleHelperSetOneFloatElement(tAssignment, tPlayer, tPlayer, tFunc, tDefault);

}

static void handlePlayerSetTwoFloatElementsWithCoordinateP(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, void(tFunc)(DreamPlayer*, double, double, int), double tDefault1, double tDefault2) {
	handleHelperSetTwoFloatElementsWithCoordinateP(tAssignment, tPlayer, tPlayer, tFunc, tDefault1, tDefault2);
}

static void handleIDSetOneIntegerElement(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int tID, void(tFunc)(int, int), int tDefault) {
	int val;
	evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(tAssignment, tPlayer, &val, tDefault);
	tFunc(tID, val);
}

static void handleIDSetTwoIntegerElements(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int tID, void(tFunc)(int, int, int), int tDefault1, int tDefault2) {
	int val1, val2;
	evaluateDreamAssignmentAndReturnAsTwoIntegersWithDefaultValues(tAssignment, tPlayer, &val1, &val2, tDefault1, tDefault2);
	tFunc(tID, val1, val2);
}

static void handleIDSetThreeIntegerElements(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int tID, void(tFunc)(int, int, int, int), int tDefault1, int tDefault2, int tDefault3) {
	int vals[3];
	evaluateDreamAssignmentAndReturnAsThreeIntegersWithDefaultValues(tAssignment, tPlayer, &vals[0], &vals[1], &vals[2], tDefault1, tDefault2, tDefault3);
	tFunc(tID, vals[0], vals[1], vals[2]);
}

static void handleIDSetTwoFloatElements(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int tID, void(tFunc)(int, double, double), double tDefault1, double tDefault2) {
	double val1, val2;
	evaluateDreamAssignmentAndReturnAsTwoFloatsWithDefaultValues(tAssignment, tPlayer, &val1, &val2, tDefault1, tDefault2);
	tFunc(tID, val1, val2);
}

static void handlePlayerIDSetOneIntegerElement(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int tID, void(tFunc)(DreamPlayer*, int, int), int tDefault) {
	int val;
	evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(tAssignment, tPlayer, &val, tDefault);
	tFunc(tPlayer, tID, val);
}

static void handlePlayerIDSetTwoIntegerElements(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int tID, void(tFunc)(DreamPlayer*, int, int, int), int tDefault1, int tDefault2) {
	int val1, val2;
	evaluateDreamAssignmentAndReturnAsTwoIntegersWithDefaultValues(tAssignment, tPlayer, &val1, &val2, tDefault1, tDefault2);
	tFunc(tPlayer, tID, val1, val2);
}

static void handlePlayerIDSetThreeIntegerElements(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int tID, void(tFunc)(DreamPlayer*, int, int, int, int), int tDefault1, int tDefault2, int tDefault3) {
	int vals[3];
	evaluateDreamAssignmentAndReturnAsThreeIntegersWithDefaultValues(tAssignment, tPlayer, &vals[0], &vals[1], &vals[2], tDefault1, tDefault2, tDefault3);
	tFunc(tPlayer, tID, vals[0], vals[1], vals[2]);
}

static void handlePlayerIDSetTwoFloatElements(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int tID, void(tFunc)(DreamPlayer*, int, double, double), double tDefault1, double tDefault2) {
	double val1, val2;
	evaluateDreamAssignmentAndReturnAsTwoFloatsWithDefaultValues(tAssignment, tPlayer, &val1, &val2, tDefault1, tDefault2);
	tFunc(tPlayer, tID, val1, val2);
}

static int handleVelocitySetting(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	Set2DPhysicsController* e = (Set2DPhysicsController*)tController->mData;

	if (e->mIsSettingX) {
		double x = evaluateDreamAssignmentAndReturnAsFloat(&e->x, tPlayer);
		setPlayerVelocityX(tPlayer, x, getActiveStateMachineCoordinateP());
	}

	if (e->mIsSettingY) {
		double y = evaluateDreamAssignmentAndReturnAsFloat(&e->y, tPlayer);
		setPlayerVelocityY(tPlayer, y, getActiveStateMachineCoordinateP());
	}

	return 0;
}

static int handleVelocityMultiplication(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	Set2DPhysicsController* e = (Set2DPhysicsController*)tController->mData;

	if (e->mIsSettingX) {
		double x = evaluateDreamAssignmentAndReturnAsFloat(&e->x, tPlayer);
		multiplyPlayerVelocityX(tPlayer, x);
	}

	if (e->mIsSettingY) {
		double y = evaluateDreamAssignmentAndReturnAsFloat(&e->y, tPlayer);
		multiplyPlayerVelocityY(tPlayer, y);
	}

	return 0;
}

static int handleVelocityAddition(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	Set2DPhysicsController* e = (Set2DPhysicsController*)tController->mData;

	if (e->mIsSettingX) {
		double x = evaluateDreamAssignmentAndReturnAsFloat(&e->x, tPlayer);
		addPlayerVelocityX(tPlayer, x, getActiveStateMachineCoordinateP());
	}

	if (e->mIsSettingY) {
		double y = evaluateDreamAssignmentAndReturnAsFloat(&e->y, tPlayer);
		addPlayerVelocityY(tPlayer, y, getActiveStateMachineCoordinateP());
	}

	return 0;
}

static int handlePositionSetting(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	Set2DPhysicsController* e = (Set2DPhysicsController*)tController->mData;

	if (e->mIsSettingX) {
		double x = evaluateDreamAssignmentAndReturnAsFloat(&e->x, tPlayer);
		setPlayerPositionBasedOnScreenCenterX(tPlayer, x, getActiveStateMachineCoordinateP());
	}

	if (e->mIsSettingY) {
		double y = evaluateDreamAssignmentAndReturnAsFloat(&e->y, tPlayer);
		setPlayerPositionY(tPlayer, y, getActiveStateMachineCoordinateP());
	}

	return 0;
}

static int handlePositionAdding(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	Set2DPhysicsController* e = (Set2DPhysicsController*)tController->mData;

	if (e->mIsSettingX) {
		double x = evaluateDreamAssignmentAndReturnAsFloat(&e->x, tPlayer);
		addPlayerPositionX(tPlayer, x, getActiveStateMachineCoordinateP());
	}

	if (e->mIsSettingY) {
		double y = evaluateDreamAssignmentAndReturnAsFloat(&e->y, tPlayer);
		addPlayerPositionY(tPlayer, y, getActiveStateMachineCoordinateP());
	}

	return 0;
}


static int handleStateChange(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	ChangeStateController* e = (ChangeStateController*)tController->mData;

	if (e->mIsChangingControl) {
		int control = evaluateDreamAssignmentAndReturnAsInteger(&e->mControl, tPlayer);
		setPlayerControl(tPlayer, control);
	}

	int state = evaluateDreamAssignmentAndReturnAsInteger(&e->mState, tPlayer);
	changePlayerStateBeforeImmediatelyEvaluatingIt(tPlayer, state);

	if (e->mIsChangingAnimation) {
		int animation = evaluateDreamAssignmentAndReturnAsInteger(&e->mAnimation, tPlayer);
		changePlayerAnimation(tPlayer, animation);
	}

	return 1;
}

static int handleSelfStateChange(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	ChangeStateController* e = (ChangeStateController*)tController->mData;

	int state = evaluateDreamAssignmentAndReturnAsInteger(&e->mState, tPlayer);

	if (e->mIsChangingControl) {
		int control = evaluateDreamAssignmentAndReturnAsInteger(&e->mControl, tPlayer);
		setPlayerControl(tPlayer, control);
	}

	if (e->mIsChangingAnimation) {
		int animation = evaluateDreamAssignmentAndReturnAsInteger(&e->mAnimation, tPlayer);
		changePlayerAnimation(tPlayer, animation);
	}

	changePlayerStateToSelfBeforeImmediatelyEvaluatingIt(tPlayer, state);

	return 1;
}

static int handleTargetStateChange(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	TargetChangeStateController* e = (TargetChangeStateController*)tController->mData;

	int id;
	evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mID, tPlayer, &id, -1);

	if (e->mIsChangingControl) {
		int control = evaluateDreamAssignmentAndReturnAsInteger(&e->mControl, tPlayer);
		setPlayerTargetControl(tPlayer, id, control);
	}

	setPlayerTargetHitOver(tPlayer, id);
	int state = evaluateDreamAssignmentAndReturnAsInteger(&e->mState, tPlayer);
	changePlayerTargetState(tPlayer, id, state);

	return 0;
}

static void handleSoundEffectValue(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, double tPrismVolume, int tChannel, double tFrequencyMultiplier, int tIsLooping, double tPanning) {
	string flag;
	evaluateDreamAssignmentAndReturnAsString(flag, tAssignment, tPlayer);

	MugenSounds* soundFile;
	int group;
	int item;

	char firstW[20], comma[10];
	int items = sscanf(flag.data(), "%s", firstW);
	if (items != 1) {
		logWarningFormat("Unable to parse flag: %s. Abort.");
		return;
	}

	if (!strcmp("isinotherfilef", firstW)) {
		soundFile = getDreamCommonSounds();
		items = sscanf(flag.data(), "%s %d %s %d", firstW, &group, comma, &item);
		if (items != 4) {
			logWarningFormat("Unable to parse flag: %s. Abort.");
			return;
		}
	} else if (!strcmp("isinotherfiles", firstW)) {
		soundFile = getPlayerSounds(tPlayer);
		items = sscanf(flag.data(), "%s %d %s %d", firstW, &group, comma, &item);
		if (items != 4) {
			logWarningFormat("Unable to parse flag: %s. Abort.");
			return;
		}
	}
	else {
		soundFile = getPlayerSounds(tPlayer);
		items = sscanf(flag.data(), "%d %s %d", &group, comma, &item);
		if (items != 3) {
			logWarningFormat("Unable to parse flag: %s. Abort.");
			return;
		}
	}

	tryPlayMugenSoundAdvanced(soundFile, group, item, tPrismVolume, tChannel, tFrequencyMultiplier, tIsLooping, tPanning);
}

static double handlePanningValue(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int tIsAbspan) {
	int pan;
	evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(tAssignment, tPlayer, &pan, 0);
	if (!getSoundAreStereoEffectsActive()) return 0.0;

	double pos;
	const auto screenWidth = getDreamScreenWidth(getActiveStateMachineCoordinateP());
	if (tIsAbspan) {
		pos = (screenWidth / 2.f) + pan;
	}
	else {
		const auto playerPos = getPlayerScreenPositionX(tPlayer, getActiveStateMachineCoordinateP());
		pos = playerPos + pan;
	}

	return clamp((pos / screenWidth) * 2.0 - 1.0, -1.0, 1.0) * getSoundPanningWidthFactor();
}

static int handlePlaySound(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	PlaySoundController* e = (PlaySoundController*)tController->mData;

	int channel, isLowPriority, loop;
	double frequencyMultiplier;
	evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mChannel, tPlayer, &channel, -1);
	channel = parsePlayerSoundEffectChannel(channel, tPlayer);
	evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mLowPriority, tPlayer, &isLowPriority, 0);

	if (channel != -1 && isSoundEffectPlayingOnChannel(channel) && isLowPriority) {
		return 0;
	}

	auto prismVolume = getPlayerMidiVolumeForPrism(tPlayer);
	if (e->mIsVolumeScale) {
		double volumeScale;
		evaluateDreamAssignmentAndReturnAsOneFloatWithDefaultValue(&e->mVolume, tPlayer, &volumeScale, 100.0);
		prismVolume *=  (volumeScale / 100.0);
	}
	else {
		int volume;
		evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mVolume, tPlayer, &volume, 0);
		prismVolume *= ((volume + 100) / double(100));
	}

	evaluateDreamAssignmentAndReturnAsOneFloatWithDefaultValue(&e->mFrequencyMultiplier, tPlayer, &frequencyMultiplier, 1.0);
	evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mLoop, tPlayer, &loop, 0);
	const auto panning = handlePanningValue(&e->mPan, tPlayer, e->mIsAbspan);

	handleSoundEffectValue(&e->mValue, tPlayer, prismVolume, channel, frequencyMultiplier, loop, panning);
	return 0;
}

static void getHitDefinitionAttributeValuesFromString(const char* attr, DreamMugenStateType* tStateType, MugenAttackClass* tAttackClass, MugenAttackType* tAttackType) {
	char arg1[20], comma[10], arg2[20];
	int items = sscanf(attr, "%s %s %s", arg1, comma, arg2);

	if (items != 3) {
		logWarning("Unable to parse hitdef attributes.");
		logWarningString(attr);
		*tStateType = MUGEN_STATE_TYPE_UNCHANGED;
		*tAttackClass = MUGEN_ATTACK_CLASS_NORMAL;
		*tAttackType = MUGEN_ATTACK_TYPE_ATTACK;
	}

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

static void handleHitDefinitionAttribute(HitDefinitionController* e, DreamPlayer* tPlayer, DreamPlayer* tHelper) {
	string attr;
	evaluateDreamAssignmentAndReturnAsString(attr, &e->mAttribute, tPlayer);

	DreamMugenStateType stateType;
	MugenAttackClass attackClass;
	MugenAttackType attackType;

	getHitDefinitionAttributeValuesFromString(attr.data(), &stateType, &attackClass, &attackType);

	setHitDataType(tHelper, stateType);
	setHitDataAttackClass(tHelper, attackClass);
	setHitDataAttackType(tHelper, attackType);
}

static void handleHitDefinitionSingleHitFlag(DreamMugenAssignment** tFlagAssignment, DreamPlayer* tPlayer, DreamPlayer* tHelper, void(tSetFunc)(DreamPlayer* tPlayer, const char*)) {
	if (!(*tFlagAssignment)) {
		tSetFunc(tHelper, "");
		return;
	}
	
	string flag;
	evaluateDreamAssignmentAndReturnAsString(flag, tFlagAssignment, tPlayer);
	tSetFunc(tHelper, flag.data());
}

static void handleHitDefinitionAffectTeam(DreamMugenAssignment** tAffectAssignment, DreamPlayer* tPlayer, DreamPlayer* tHelper) {
	string flag;
	evaluateDreamAssignmentAndReturnAsString(flag, tAffectAssignment, tPlayer);
	if (flag.size() != 1) {
		logWarningFormat("Unable to parse hitdef affectteam %s. Set to enemy.", flag);
		setHitDataAffectTeam(tHelper, MUGEN_AFFECT_TEAM_ENEMY);
		return;
	}

	if (flag[0] == 'b') setHitDataAffectTeam(tHelper, MUGEN_AFFECT_TEAM_BOTH);
	else if (flag[0] == 'e') setHitDataAffectTeam(tHelper, MUGEN_AFFECT_TEAM_ENEMY);
	else if (flag[0] == 'f') setHitDataAffectTeam(tHelper, MUGEN_AFFECT_TEAM_FRIENDLY);
	else {
		logWarningFormat("Unable to parse hitdef affectteam %s. Set to enemy.", flag);
		setHitDataAffectTeam(tHelper, MUGEN_AFFECT_TEAM_ENEMY);
	}
}

static void handleHitDefinitionSingleAnimationType(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, DreamPlayer* tHelper, void(tFunc)(DreamPlayer*, MugenHitAnimationType), MugenHitAnimationType tDefault) {
	if (!(*tAssignment)) {
		tFunc(tHelper, tDefault);
		return;
	}
	
	string flag;
	evaluateDreamAssignmentAndReturnAsString(flag, tAssignment, tPlayer);

	if ("light" == flag) tFunc(tHelper, MUGEN_HIT_ANIMATION_TYPE_LIGHT);
	else if (("medium" == flag) || ("med" == flag)) tFunc(tHelper, MUGEN_HIT_ANIMATION_TYPE_MEDIUM);
	else if ("hard" == flag) tFunc(tHelper, MUGEN_HIT_ANIMATION_TYPE_HARD);
	else if ("heavy" == flag) tFunc(tHelper, MUGEN_HIT_ANIMATION_TYPE_HEAVY);
	else if ("back" == flag) tFunc(tHelper, MUGEN_HIT_ANIMATION_TYPE_BACK);
	else if ("up" == flag) tFunc(tHelper, MUGEN_HIT_ANIMATION_TYPE_UP);
	else if ("diagup" == flag) tFunc(tHelper, MUGEN_HIT_ANIMATION_TYPE_DIAGONAL_UP);
	else {
		logWarningFormat("Unable to parse hitdef animation type %s. Setting to default", flag);
		tFunc(tHelper, tDefault);
	}
}

static void handleHitDefinitionPriority(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, DreamPlayer* tHelper) {
	int prio;
	MugenHitPriorityType type;
	if (!(*tAssignment)) {
		prio = 4;
		type = MUGEN_HIT_PRIORITY_HIT;
	}
	else {
		string flag;
		evaluateDreamAssignmentAndReturnAsString(flag, tAssignment, tPlayer);

		char prioString[20], comma[10], typeString[20];
		int items = sscanf(flag.data(), "%s %s %s", prioString, comma, typeString);

		if (items < 1 || !strcmp("", prioString)) prio = 4;
		else prio = atoi(prioString);

		if (items < 3 || !strcmp("", typeString)) type = MUGEN_HIT_PRIORITY_HIT;
		else if (!strcmp("hit", typeString)) type = MUGEN_HIT_PRIORITY_HIT;
		else if (!strcmp("miss", typeString)) type = MUGEN_HIT_PRIORITY_MISS;
		else if (!strcmp("dodge", typeString)) type = MUGEN_HIT_PRIORITY_DODGE;
		else {
			logWarningFormat("Unable to parse hitdef priority type %s with string %s. Defaulting to hit.", flag, typeString);
			type = MUGEN_HIT_PRIORITY_HIT;
		}
	}
	setHitDataPriority(tHelper, prio, type);
}

static void handleHitDefinitionDamage(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, DreamPlayer* tHelper) {
	int damage, guardDamage;

	evaluateDreamAssignmentAndReturnAsTwoIntegersWithDefaultValues(tAssignment, tPlayer, &damage, &guardDamage, 0, 0);
	setHitDataDamage(tHelper, damage, guardDamage);
}

static void handleHitDefinitionSinglePauseTime(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, DreamPlayer* tHelper, void(tFunc)(DreamPlayer*, int, int), int tDefault1, int tDefault2) {
	int p1PauseTime, p2PauseTime;

	evaluateDreamAssignmentAndReturnAsTwoIntegersWithDefaultValues(tAssignment, tPlayer, &p1PauseTime, &p2PauseTime, tDefault1, tDefault2);
	tFunc(tHelper, p1PauseTime, p2PauseTime);
}

static void handleHitDefinitionSparkNumberSingle(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, DreamPlayer* tHelper, void(tFunc)(DreamPlayer*, int, int), int tDefaultIsInFile, int tDefaultNumber) {
	int isInPlayerFile;
	int number;
	if (!(*tAssignment)) {
		isInPlayerFile = tDefaultIsInFile;
		number = tDefaultNumber;
	}
	else {
		string flag;
		evaluateDreamAssignmentAndReturnAsString(flag, tAssignment, tPlayer);

		char firstW[200];
		int which = -1;

		int items = sscanf(flag.data(), "%s %d", firstW, &which);

		if (items < 1) {
			isInPlayerFile = tDefaultIsInFile;
			number = tDefaultNumber;
		}
		else if (!strcmp("isinotherfiles", firstW)) {
			if (items != 2) {
				logWarningFormat("Unable to parse hit definition spark number: %s", flag.data());
				which = 0;
			}
			isInPlayerFile = 1;
			number = which;
		}
		else if (!strcmp("isinotherfilef", firstW)) {
			if (items != 2) {
				logWarningFormat("Unable to parse hit definition spark number: %s", flag.data());
				which = 0;
			}
			isInPlayerFile = 0;
			number = which;
		}
		else {
			if (items != 1) {
				logWarningFormat("Unable to properly parse hit definition spark number: %s", flag.data());
			}
			isInPlayerFile = 0;
			number = atoi(flag.data());
		}
	}

	tFunc(tHelper, isInPlayerFile, number);
}

static void handleHitDefinitionSparkXY(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, DreamPlayer* tHelper, void(tFunc)(DreamPlayer*, int, int)) {
	int x, y;

	evaluateDreamAssignmentAndReturnAsTwoIntegersWithDefaultValues(tAssignment, tPlayer, &x, &y, 0, 0);
	tFunc(tHelper, x, y);
}

static void handleHitDefinitionSingleSound(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, DreamPlayer* tHelper, void(tFunc)(DreamPlayer*, int, int, int), int tDefaultGroup, int tDefaultItem) {
	int group;
	int item;
	int isInPlayerFile;
	if (!(*tAssignment)) {
		isInPlayerFile = 0;
		group = tDefaultGroup;
		item = tDefaultItem;
	}
	else {
		string flag;
		evaluateDreamAssignmentAndReturnAsString(flag, tAssignment, tPlayer);

		char firstW[200], comma[10];
		int items = sscanf(flag.data(), "%s", firstW);

		if (items < 1) {
			isInPlayerFile = 0;
			group = tDefaultGroup;
			item = tDefaultItem;
		}
		else if (!strcmp("isinotherfilef", firstW)) {
			isInPlayerFile = 0;
			int fullItems = sscanf(flag.data(), "%s %d %s %d", firstW, &group, comma, &item);
			assert(fullItems >= 2);

			if (fullItems < 3) {
				item = tDefaultItem;
			}
		}
		else if (!strcmp("isinotherfiles", firstW)) {
			isInPlayerFile = 1;
			int fullItems = sscanf(flag.data(), "%s %d %s %d", firstW, &group, comma, &item);
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
			int fullItems = sscanf(flag.data(), "%d %s %d", &group, comma, &item);
			if (fullItems < 1) {
				logWarningFormat("Unable to parse hit definition sound flag %s. Defaulting.", flag);
				group = tDefaultGroup;
				item = tDefaultItem;
			}
			else if (fullItems < 2) {
				item = tDefaultItem;
			}
		}
	}

	tFunc(tHelper, isInPlayerFile, group, item);
}

static void handleHitDefinitionSingleAttackHeight(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, DreamPlayer* tHelper, void(tFunc)(DreamPlayer*, MugenAttackHeight), MugenAttackHeight tDefault) {
	if (!(*tAssignment)) {
		tFunc(tHelper, tDefault);
		return;
	}

	string flag;
	evaluateDreamAssignmentAndReturnAsString(flag, tAssignment, tPlayer);

	if ("high" == flag) tFunc(tHelper, MUGEN_ATTACK_HEIGHT_HIGH);
	else if ("low" == flag) tFunc(tHelper, MUGEN_ATTACK_HEIGHT_LOW);
	else if ("trip" == flag) tFunc(tHelper, MUGEN_ATTACK_HEIGHT_TRIP);
	else if ("heavy" == flag) tFunc(tHelper, MUGEN_ATTACK_HEIGHT_HEAVY);
	else if ("none" == flag) tFunc(tHelper, MUGEN_ATTACK_HEIGHT_NONE);
	else {
		logWarningFormat("Unable to parse hitdef attack height type %s. Defaulting.", flag);
		tFunc(tHelper, tDefault);
	}
}

static void handleHitDefinitionSinglePowerAddition(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, DreamPlayer* tHelper, void(tFunc)(DreamPlayer*, int, int), double tDefaultFactor, int tDamage) {
	int val1, val2;
	if (!(*tAssignment)) {
		val1 = (int)(tDamage*tDefaultFactor);
		val2 = val1 / 2;
	}
	else {
		string flag;
		evaluateDreamAssignmentAndReturnAsString(flag, tAssignment, tPlayer);

		char string1[20], comma[10], string2[20];
		int items = sscanf(flag.data(), "%s %s %s", string1, comma, string2);

		if (items < 1 || !strcmp("", string1)) val1 = (int)(tDamage*tDefaultFactor);
		else val1 = atoi(string1);

		if (items < 3 || !strcmp("", string2)) val2 = val1 / 2;
		else val2 = atoi(string2);
	}

	tFunc(tHelper, val1, val2);
}

static void handleHitDefinitionWithController(HitDefinitionController* e, DreamPlayer* tPlayer, DreamPlayer* tHelper) {
	setHitDataVelocityX(tHelper, 0);
	setHitDataVelocityY(tHelper, 0);

	handleHitDefinitionAttribute(e, tPlayer, tHelper);
	handleHitDefinitionSingleHitFlag(&e->mHitFlag, tPlayer, tHelper, setHitDataHitFlag);
	handleHitDefinitionSingleHitFlag(&e->mGuardFlag, tPlayer, tHelper, setHitDataGuardFlag);
	handleHitDefinitionAffectTeam(&e->mAffectTeam, tPlayer, tHelper);
	handleHitDefinitionSingleAnimationType(&e->mAnimationType, tPlayer, tHelper, setHitDataAnimationType, MUGEN_HIT_ANIMATION_TYPE_LIGHT);
	handleHitDefinitionSingleAnimationType(&e->mAirAnimationType, tPlayer, tHelper, setHitDataAirAnimationType, getHitDataAnimationType(tHelper));
	handleHitDefinitionSingleAnimationType(&e->mFallAnimationType, tPlayer, tHelper, setHitDataFallAnimationType, getHitDataAirAnimationType(tHelper) == MUGEN_HIT_ANIMATION_TYPE_UP ? MUGEN_HIT_ANIMATION_TYPE_UP : MUGEN_HIT_ANIMATION_TYPE_BACK);
	handleHitDefinitionPriority(&e->mPriority, tPlayer, tHelper);
	handleHitDefinitionDamage(&e->mDamage, tPlayer, tHelper);
	handleHitDefinitionSinglePauseTime(&e->mPauseTime, tPlayer, tHelper, setHitDataPauseTime, 0, 0);
	handleHitDefinitionSinglePauseTime(&e->mGuardPauseTime, tPlayer, tHelper, setHitDataGuardPauseTime, getHitDataPlayer1PauseTime(tHelper), getHitDataPlayer2PauseTime(tHelper));

	handleHitDefinitionSparkNumberSingle(&e->mSparkNumber, tPlayer, tHelper, setHitDataSparkNumber, getDefaultPlayerSparkNumberIsInPlayerFile(tHelper), getDefaultPlayerSparkNumber(tHelper));
	handleHitDefinitionSparkNumberSingle(&e->mGuardSparkNumber, tPlayer, tHelper, setHitDataGuardSparkNumber, getDefaultPlayerGuardSparkNumberIsInPlayerFile(tHelper), getDefaultPlayerGuardSparkNumber(tHelper));
	handleHitDefinitionSparkXY(&e->mSparkXY, tPlayer, tHelper, setHitDataSparkXY);
	handleHitDefinitionSingleSound(&e->mHitSound, tPlayer, tHelper, setHitDataHitSound, 5, 0);
	handleHitDefinitionSingleSound(&e->mGuardSound, tPlayer, tHelper, setHitDataGuardSound, 6, 0);

	handleHitDefinitionSingleAttackHeight(&e->mGroundType, tPlayer, tHelper, setHitDataGroundType, MUGEN_ATTACK_HEIGHT_HIGH);
	handleHitDefinitionSingleAttackHeight(&e->mAirType, tPlayer, tHelper, setHitDataAirType, getHitDataGroundType(tHelper));

	handleHelperSetOneIntegerElement(&e->mGroundHitTime, tPlayer, tHelper, setHitDataGroundHitTime, 0);
	handleHelperSetOneIntegerElement(&e->mGroundSlideTime, tPlayer, tHelper, setHitDataGroundSlideTime, 0);
	handleHelperSetOneIntegerElement(&e->mGuardHitTime, tPlayer, tHelper, setHitDataGuardHitTime, getHitDataGroundHitTime(tHelper));
	handleHelperSetOneIntegerElement(&e->mGuardSlideTime, tPlayer, tHelper, setHitDataGuardSlideTime, getHitDataGuardHitTime(tHelper));
	handleHelperSetOneIntegerElement(&e->mAirHitTime, tPlayer, tHelper, setHitDataAirHitTime, 20);
	handleHelperSetOneIntegerElement(&e->mGuardControlTime, tPlayer, tHelper, setHitDataGuardControlTime, getHitDataGuardSlideTime(tHelper));
	handleHelperSetOneIntegerElement(&e->mGuardDistance, tPlayer, tHelper, setHitDataGuardDistance, getDefaultPlayerAttackDistance(tHelper, getActiveStateMachineCoordinateP()));
	handleHelperSetOneFloatElement(&e->mYAccel, tPlayer, tHelper, setHitDataYAccel, transformDreamCoordinates(0.7, 640, getActiveStateMachineCoordinateP()));
	handleHelperSetTwoFloatElements(&e->mGroundVelocity, tPlayer, tHelper, setHitDataGroundVelocity, 0, 0);
	handleHelperSetOneFloatElement(&e->mGuardVelocity, tPlayer, tHelper, setHitDataGuardVelocity, getHitDataGroundVelocityX(tHelper));
	handleHelperSetTwoFloatElements(&e->mAirVelocity, tPlayer, tHelper, setHitDataAirVelocity, 0, 0);
	handleHelperSetTwoFloatElements(&e->mAirGuardVelocity, tPlayer, tHelper, setHitDataAirGuardVelocity, getHitDataAirVelocityX(tHelper) * 1.5, getHitDataAirVelocityY(tHelper) / 2);

	handleHelperSetOneFloatElement(&e->mGroundCornerPushVelocityOffset, tPlayer, tHelper, setGroundCornerPushVelocityOffset, getHitDataAttackType(tHelper) == MUGEN_ATTACK_TYPE_ATTACK ? 0.0 : 1.3*getHitDataGuardVelocity(tHelper));
	handleHelperSetOneFloatElement(&e->mAirCornerPushVelocityOffset, tPlayer, tHelper, setAirCornerPushVelocityOffset, getGroundCornerPushVelocityOffset(tHelper));
	handleHelperSetOneFloatElement(&e->mDownCornerPushVelocityOffset, tPlayer, tHelper, setDownCornerPushVelocityOffset, getGroundCornerPushVelocityOffset(tHelper));
	handleHelperSetOneFloatElement(&e->mGuardCornerPushVelocityOffset, tPlayer, tHelper, setGuardCornerPushVelocityOffset, getGroundCornerPushVelocityOffset(tHelper));
	handleHelperSetOneFloatElement(&e->mAirGuardCornerPushVelocityOffset, tPlayer, tHelper, setAirGuardCornerPushVelocityOffset, getGuardCornerPushVelocityOffset(tHelper));

	handleHelperSetOneIntegerElement(&e->mAirGuardControlTime, tPlayer, tHelper, setHitDataAirGuardControlTime, getHitDataGuardControlTime(tHelper));
	handleHelperSetOneIntegerElement(&e->mAirJuggle, tPlayer, tHelper, setHitDataAirJuggle, 0);

	handleHelperSetTwoOptionalIntegerElements(&e->mMinimumDistance, tPlayer, tHelper, setHitDataMinimumDistance, setHitDataMinimumDistanceInactive);
	handleHelperSetTwoOptionalIntegerElements(&e->mMaximumDistance, tPlayer, tHelper, setHitDataMaximumDistance, setHitDataMaximumDistanceInactive);
	handleHelperSetTwoOptionalIntegerElements(&e->mSnap, tPlayer, tHelper, setHitDataSnap, setHitDataSnapInactive);

	handleHelperSetOneIntegerElement(&e->mPlayerSpritePriority1, tPlayer, tHelper, setHitDataPlayer1SpritePriority, 1);
	handleHelperSetOneIntegerElement(&e->mPlayerSpritePriority2, tPlayer, tHelper, setHitDataPlayer2SpritePriority, 0);

	handleHelperSetOneIntegerElement(&e->mPlayer1ChangeFaceDirection, tPlayer, tHelper, setHitDataPlayer1FaceDirection, 0);
	handleHelperSetOneIntegerElement(&e->mPlayer1ChangeFaceDirectionRelativeToPlayer2, tPlayer, tHelper, setHitDataPlayer1ChangeFaceDirectionRelativeToPlayer2, 0);
	handleHelperSetOneIntegerElement(&e->mPlayer2ChangeFaceDirectionRelativeToPlayer1, tPlayer, tHelper, setHitDataPlayer2ChangeFaceDirectionRelativeToPlayer1, 0);

	handleHelperSetOneIntegerElement(&e->mPlayer1StateNumber, tPlayer, tHelper, setPlayer1StateNumber, -1);
	handleHelperSetOneIntegerElement(&e->mPlayer2StateNumber, tPlayer, tHelper, setPlayer2StateNumber, -1);
	handleHelperSetOneIntegerElement(&e->mPlayer2CapableOfGettingPlayer1State, tPlayer, tHelper, setHitDataPlayer2CapableOfGettingPlayer1State, 1);
	handleHelperSetOneIntegerElement(&e->mForceStanding, tPlayer, tHelper, setHitDataForceStanding, getHitDataGroundVelocityY(tHelper) != 0.0 ? 1 : 0);

	handleHelperSetOneIntegerElement(&e->mFall, tPlayer, tHelper, setHitDataFall, 0);
	handleHelperSetOneFloatElement(&e->mFallXVelocity, tPlayer, tHelper, setHitDataFallXVelocity, 0);
	handleHelperSetOneFloatElement(&e->mFallYVelocity, tPlayer, tHelper, setHitDataFallYVelocity, transformDreamCoordinates(-9, 640, getActiveStateMachineCoordinateP()));
	handleHelperSetOneIntegerElement(&e->mFallCanBeRecovered, tPlayer, tHelper, setHitDataFallRecovery, 1);
	handleHelperSetOneIntegerElement(&e->mFallRecoveryTime, tPlayer, tHelper, setHitDataFallRecoveryTime, 4);
	handleHelperSetOneIntegerElement(&e->mFallDamage, tPlayer, tHelper, setHitDataFallDamage, 0);
	handleHelperSetOneIntegerElement(&e->mAirFall, tPlayer, tHelper, setHitDataAirFall, getHitDataFall(tHelper));
	handleHelperSetOneIntegerElement(&e->mForceNoFall, tPlayer, tHelper, setHitDataForceNoFall, 0);

	handleHelperSetTwoFloatElements(&e->mDownVelocity, tPlayer, tHelper, setHitDataDownVelocity, getHitDataAirVelocityX(tHelper), getHitDataAirVelocityY(tHelper));
	handleHelperSetOneIntegerElement(&e->mDownHitTime, tPlayer, tHelper, setHitDataDownHitTime, 0);
	handleHelperSetOneIntegerElement(&e->mDownBounce, tPlayer, tHelper, setHitDataDownBounce, 0);

	handleHelperSetOneIntegerElement(&e->mHitID, tPlayer, tHelper, setHitDataHitID, 0);
	handleHelperSetOneIntegerElement(&e->mChainID, tPlayer, tHelper, setHitDataChainID, -1);
	handleHelperSetTwoIntegerElements(&e->mNoChainID, tPlayer, tHelper, setHitDataNoChainID, -1, -1);
	handleHelperSetOneIntegerElement(&e->mHitOnce, tPlayer, tHelper, setHitDataHitOnce, 1);

	handleHelperSetOneIntegerElement(&e->mKill, tPlayer, tHelper, setHitDataKill, 1);
	handleHelperSetOneIntegerElement(&e->mGuardKill, tPlayer, tHelper, setHitDataGuardKill, 1);
	handleHelperSetOneIntegerElement(&e->mFallKill, tPlayer, tHelper, setHitDataFallKill, 1);
	handleHelperSetOneIntegerElement(&e->mNumberOfHits, tPlayer, tHelper, setHitDataNumberOfHits, 1);
	handleHitDefinitionSinglePowerAddition(&e->mGetPower, tPlayer, tHelper, setHitDataGetPower, getDreamDefaultAttackDamageDoneToPowerMultiplier(), getHitDataDamage(tHelper));
	handleHitDefinitionSinglePowerAddition(&e->mGivePower, tPlayer, tHelper, setHitDataGivePower, getDreamDefaultAttackDamageReceivedToPowerMultiplier(), getHitDataDamage(tHelper));

	handleHelperSetOneIntegerElement(&e->mPaletteEffectTime, tPlayer, tHelper, setHitDataPaletteEffectTime, 0);
	handleHelperSetThreeIntegerElements(&e->mPaletteEffectMultiplication, tPlayer, tHelper, setHitDataPaletteEffectMultiplication, 256, 256, 256);
	handleHelperSetThreeIntegerElements(&e->mPaletteEffectAddition, tPlayer, tHelper, setHitDataPaletteEffectAddition, 0, 0, 0);

	handleHelperSetOneIntegerElement(&e->mEnvironmentShakeTime, tPlayer, tHelper, setHitDataEnvironmentShakeTime, 0);
	handleHelperSetOneFloatElement(&e->mEnvironmentShakeFrequency, tPlayer, tHelper, setHitDataEnvironmentShakeFrequency, 60.0);
	handleHelperSetOneIntegerElement(&e->mEnvironmentShakeAmplitude, tPlayer, tHelper, setHitDataEnvironmentShakeAmplitude, (int)transformDreamCoordinates(-4.0, 320, getDreamStageCoordinateP()));
	handleHelperSetOneFloatElement(&e->mEnvironmentShakePhase, tPlayer, tHelper, setHitDataEnvironmentShakePhase, (getHitDataEnvironmentShakeFrequency(tHelper) >= 90.0) ? 90.0 : 0.0);

	handleHelperSetOneIntegerElement(&e->mFallEnvironmentShakeTime, tPlayer, tHelper, setHitDataFallEnvironmentShakeTime, 0);
	handleHelperSetOneFloatElement(&e->mFallEnvironmentShakeFrequency, tPlayer, tHelper, setHitDataFallEnvironmentShakeFrequency, 60.0);
	handleHelperSetOneIntegerElement(&e->mFallEnvironmentShakeAmplitude, tPlayer, tHelper, setHitDataFallEnvironmentShakeAmplitude, (int)transformDreamCoordinates(-4.0, 320, getDreamStageCoordinateP()));
	handleHelperSetOneFloatElement(&e->mFallEnvironmentShakePhase, tPlayer, tHelper, setHitDataFallEnvironmentShakePhase, getHitDataFallEnvironmentShakeFrequency(tHelper) >= 90.0 ? 90.0 : 0);

	setHitDataIsFacingRight(tHelper, getPlayerIsFacingRight(tHelper));

	setHitDataActive(tHelper);
}

static int handleHitDefinition(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	HitDefinitionController* e = (HitDefinitionController*)tController->mData;

	handleHitDefinitionWithController(e, tPlayer, tPlayer);

	return 0;
}

static int handleAnimationChange(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	ChangeAnimationController* e = (ChangeAnimationController*)tController->mData;

	int animation = evaluateDreamAssignmentAndReturnAsInteger(&e->tNewAnimation, tPlayer);
	int step = evaluateDreamAssignmentAndReturnAsInteger(&e->tStep, tPlayer);
	changePlayerAnimationWithStartStep(tPlayer, animation, step);

	return 0;
}

static int handleAnimationChange2(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	ChangeAnimationController* e = (ChangeAnimationController*)tController->mData;

	int animation = evaluateDreamAssignmentAndReturnAsInteger(&e->tNewAnimation, tPlayer);
	int step = evaluateDreamAssignmentAndReturnAsInteger(&e->tStep, tPlayer);
	changePlayerAnimationToPlayer2AnimationWithStartStep(tPlayer, animation, step);

	return 0;
}

static int handleControlSetting(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	ControlSettingController* e = (ControlSettingController*)tController->mData;

	int control = evaluateDreamAssignmentAndReturnAsInteger(&e->tValue, tPlayer);
	setPlayerControl(tPlayer, control);

	return 0;
}

static int handleWidth(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	WidthController* e = (WidthController*)tController->mData;

	Vector2DI stage;
	Vector2DI player;

	if (e->mHasValue) {
		stage = player = evaluateDreamAssignmentAndReturnAsVector2DI(&e->mValue, tPlayer);	
	}
	else {
		evaluateDreamAssignmentAndReturnAsTwoIntegersWithDefaultValues(&e->mEdge, tPlayer, &stage.x, &stage.y, 0, 0);
		evaluateDreamAssignmentAndReturnAsTwoIntegersWithDefaultValues(&e->mPlayer, tPlayer, &player.x, &player.y, 0, 0);
	}

	setPlayerWidthOneFrame(tPlayer, stage, player, getActiveStateMachineCoordinateP());

	return 0;
}

static int handleZoom(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	ZoomController* e = (ZoomController*)tController->mData;

	double scale;
	Position2D pos;
	evaluateDreamAssignmentAndReturnAsOneFloatWithDefaultValue(&e->mScale, tPlayer, &scale, 1.0);
	evaluateDreamAssignmentAndReturnAsTwoFloatsWithDefaultValues(&e->mPos, tPlayer, &pos.x, &pos.y, 0, 0);

	setDreamStageZoomOneFrame(scale, pos);

	return 0;
}

static int handleSpritePriority(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	SpritePriorityController* e = (SpritePriorityController*)tController->mData;

	int value = evaluateDreamAssignmentAndReturnAsInteger(&e->tValue, tPlayer);
	setPlayerSpritePriority(tPlayer, value);

	return 0;
}

static void handleSingleSpecialAssert(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer) {
	string flag;
	evaluateDreamAssignmentAndReturnAsString(flag, tAssignment, tPlayer);

	if ("intro" == flag) {
		setPlayerIntroFlag(tPlayer);
	}
	else if ("invisible" == flag) {
		setPlayerInvisibleFlag(tPlayer);
	}
	else if ("roundnotover" == flag) {
		setDreamRoundNotOverFlag();
	}
	else if ("nobardisplay" == flag) {
		setDreamBarInvisibleForOneFrame();
	}
	else if ("nobg" == flag) {
		setDreamStageInvisibleForOneFrame();
	}
	else if ("nofg" == flag) {
		setDreamStageLayer1InvisibleForOneFrame();
	}
	else if ("nostandguard" == flag) {
		setPlayerNoStandGuardFlag(tPlayer);
	}
	else if ("nocrouchguard" == flag) {
		setPlayerNoCrouchGuardFlag(tPlayer);
	}
	else if ("noairguard" == flag) {
		setPlayerNoAirGuardFlag(tPlayer);
	}
	else if ("noautoturn" == flag) {
		setPlayerNoAutoTurnFlag(tPlayer);
	}
	else if ("nojugglecheck" == flag) {
		setPlayerNoJuggleCheckFlag(tPlayer);
	}
	else if ("nokosnd" == flag) {
		setPlayerNoKOSoundFlag(tPlayer);
	}
	else if ("nokoslow" == flag) {
		setPlayerNoKOSlowdownFlag(tPlayer);
	}
	else if ("noshadow" == flag) {
		setPlayerNoShadow(tPlayer);
	}
	else if ("globalnoshadow" == flag) {
		setAllPlayersNoShadow();
		setAllExplodsNoShadow();
	}
	else if ("nomusic" == flag) {
		setNoMusicFlag();
	}
	else if ("nowalk" == flag) {
		setPlayerNoWalkFlag(tPlayer);
	}
	else if ("timerfreeze" == flag) {
		setTimerFreezeFlag();
	}
	else if ("unguardable" == flag) {
		setPlayerUnguardableFlag(tPlayer);
	}
	else {
		logWarningFormat("Unrecognized special assert flag %s. Ignoring.", flag);
	}
}

static int handleSpecialAssert(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	SpecialAssertController* e = (SpecialAssertController*)tController->mData;

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
	MakeDustController* e = (MakeDustController*)tController->mData;

	Vector2D pos = evaluateDreamAssignmentAndReturnAsVector2D(&e->mPositionOffset, tPlayer);
	int spacing = evaluateDreamAssignmentAndReturnAsInteger(&e->mSpacing, tPlayer);

	addPlayerDust(tPlayer, 0, pos, spacing, getActiveStateMachineCoordinateP());

	if (e->mHasSecondDustCloud) {
		pos = evaluateDreamAssignmentAndReturnAsVector2D(&e->mPositionOffset2, tPlayer);
		addPlayerDust(tPlayer, 1, pos, spacing, getActiveStateMachineCoordinateP());
	}

	return 0;
}

typedef struct {
	DreamPlayer* mPlayer;
	DreamPlayer* mTarget;

} VarSetHandlingCaller;

static void handleSettingSingleVariable(void* tCaller, void* tData) {
	VarSetHandlingCaller* caller = (VarSetHandlingCaller*)tCaller;
	VarSetControllerEntry* e = (VarSetControllerEntry*)tData;

	int id = evaluateDreamAssignmentAndReturnAsInteger(&e->mID, caller->mPlayer);

	if (e->mType == VAR_SET_TYPE_SYSTEM) {
		int val = evaluateDreamAssignmentAndReturnAsInteger(&e->mAssignment, caller->mPlayer);
		setPlayerSystemVariable(caller->mTarget, id, val);
	}
	else if (e->mType == VAR_SET_TYPE_INTEGER) {
		int val = evaluateDreamAssignmentAndReturnAsInteger(&e->mAssignment, caller->mPlayer);
		setPlayerVariable(caller->mTarget, id, val);
	}
	else if (e->mType == VAR_SET_TYPE_SYSTEM_FLOAT) {
		double val = evaluateDreamAssignmentAndReturnAsFloat(&e->mAssignment, caller->mPlayer);
		setPlayerSystemFloatVariable(caller->mTarget, id, val);
	}
	else if (e->mType == VAR_SET_TYPE_FLOAT) {
		double val = evaluateDreamAssignmentAndReturnAsFloat(&e->mAssignment, caller->mPlayer);
		setPlayerFloatVariable(caller->mTarget, id, val);
	}
	else {
		logWarningFormat("Unrecognized variable type %d. Ignoring.", e->mType);
	}
}

static int handleSettingVariable(DreamMugenStateController* tController, DreamPlayer* tPlayer, DreamPlayer* tTarget) {
	if (!tPlayer) return 0;

	VarSetController* e = (VarSetController*)tController->mData;
	VarSetHandlingCaller caller;
	caller.mPlayer = tPlayer;
	caller.mTarget = tTarget;

	vector_map(&e->mVarSets, handleSettingSingleVariable, &caller);

	return 0;
}

static void handleSettingSingleGlobalVariable(void* tCaller, void* tData) {
	VarSetHandlingCaller* caller = (VarSetHandlingCaller*)tCaller;
	VarSetControllerEntry* e = (VarSetControllerEntry*)tData;

	int id = evaluateDreamAssignmentAndReturnAsInteger(&e->mID, caller->mPlayer);

	if (e->mType == VAR_SET_TYPE_SYSTEM) {
		int val = evaluateDreamAssignmentAndReturnAsInteger(&e->mAssignment, caller->mPlayer);
		logWarning("Trying to set global system variable, defaulting to normal global variable.");
		setGlobalVariable(id, val);
	}
	else if (e->mType == VAR_SET_TYPE_INTEGER) {
		int val = evaluateDreamAssignmentAndReturnAsInteger(&e->mAssignment, caller->mPlayer);
		setGlobalVariable(id, val);
	}
	else if (e->mType == VAR_SET_TYPE_SYSTEM_FLOAT) {
		double val = evaluateDreamAssignmentAndReturnAsFloat(&e->mAssignment, caller->mPlayer);
		logWarning("Trying to set global system variable, defaulting to normal global variable.");
		setGlobalFloatVariable(id, val);
	}
	else if (e->mType == VAR_SET_TYPE_FLOAT) {
		double val = evaluateDreamAssignmentAndReturnAsFloat(&e->mAssignment, caller->mPlayer);
		setGlobalFloatVariable(id, val);
	}
	else {
		logWarningFormat("Unrecognized variable type %d. Ignoring.", e->mType);
	}
}

static int handleSettingGlobalVariable(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	if (!tPlayer) return 0;

	VarSetController* e = (VarSetController*)tController->mData;
	VarSetHandlingCaller caller;
	caller.mPlayer = tPlayer;
	caller.mTarget = NULL;

	vector_map(&e->mVarSets, handleSettingSingleGlobalVariable, &caller);

	return 0;
}

static void handleAddingSingleVariable(void* tCaller, void* tData) {
	VarSetHandlingCaller* caller = (VarSetHandlingCaller*)tCaller;
	VarSetControllerEntry* e = (VarSetControllerEntry*)tData;
	
	int id = evaluateDreamAssignmentAndReturnAsInteger(&e->mID, caller->mPlayer);

	if (e->mType == VAR_SET_TYPE_SYSTEM) {
		int val = evaluateDreamAssignmentAndReturnAsInteger(&e->mAssignment, caller->mPlayer);
		addPlayerSystemVariable(caller->mTarget, id, val);
	}
	else if (e->mType == VAR_SET_TYPE_INTEGER) {
		int val = evaluateDreamAssignmentAndReturnAsInteger(&e->mAssignment, caller->mPlayer);
		addPlayerVariable(caller->mTarget, id, val);
	}
	else if (e->mType == VAR_SET_TYPE_SYSTEM_FLOAT) {
		double val = evaluateDreamAssignmentAndReturnAsFloat(&e->mAssignment, caller->mPlayer);
		addPlayerSystemFloatVariable(caller->mTarget, id, val);
	}
	else if (e->mType == VAR_SET_TYPE_FLOAT) {
		double val = evaluateDreamAssignmentAndReturnAsFloat(&e->mAssignment, caller->mPlayer);
		addPlayerFloatVariable(caller->mTarget, id, val);
	}
	else {
		logWarningFormat("Unrecognized variable type %d. Ignoring.", e->mType);
	}
}

static int handleAddingVariable(DreamMugenStateController* tController, DreamPlayer* tPlayer, DreamPlayer* tTarget) {
	if (!tPlayer) return 0;
	
	VarSetController* e = (VarSetController*)tController->mData;
	VarSetHandlingCaller caller;
	caller.mPlayer = tPlayer;
	caller.mTarget = tTarget;

	vector_map(&e->mVarSets, handleAddingSingleVariable, &caller);

	return 0;
}

static void handleAddingSingleGlobalVariable(void* tCaller, void* tData) {
	VarSetHandlingCaller* caller = (VarSetHandlingCaller*)tCaller;
	VarSetControllerEntry* e = (VarSetControllerEntry*)tData;

	int id = evaluateDreamAssignmentAndReturnAsInteger(&e->mID, caller->mPlayer);

	if (e->mType == VAR_SET_TYPE_SYSTEM) {
		int val = evaluateDreamAssignmentAndReturnAsInteger(&e->mAssignment, caller->mPlayer);
		logWarning("Trying to set global system variable, defaulting to normal global variable.");
		addGlobalVariable(id, val);
	}
	else if (e->mType == VAR_SET_TYPE_INTEGER) {
		int val = evaluateDreamAssignmentAndReturnAsInteger(&e->mAssignment, caller->mPlayer);
		addGlobalVariable(id, val);
	}
	else if (e->mType == VAR_SET_TYPE_SYSTEM_FLOAT) {
		double val = evaluateDreamAssignmentAndReturnAsFloat(&e->mAssignment, caller->mPlayer);
		logWarning("Trying to set global system variable, defaulting to normal global variable.");
		addGlobalFloatVariable(id, val);
	}
	else if (e->mType == VAR_SET_TYPE_FLOAT) {
		double val = evaluateDreamAssignmentAndReturnAsFloat(&e->mAssignment, caller->mPlayer);
		addGlobalFloatVariable(id, val);
	}
	else {
		logWarningFormat("Unrecognized variable type %d. Ignoring.", e->mType);
	}
}

static int handleAddingGlobalVariable(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	if (!tPlayer) return 0;

	VarSetController* e = (VarSetController*)tController->mData;
	VarSetHandlingCaller caller;
	caller.mPlayer = tPlayer;
	caller.mTarget = NULL;

	vector_map(&e->mVarSets, handleAddingSingleGlobalVariable, &caller);

	return 0;
}

static int handleNull() { return 0; }

static DreamMugenStateType handleStateTypeAssignment(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer) {
	DreamMugenStateType ret;

	string text;
	evaluateDreamAssignmentAndReturnAsString(text, tAssignment, tPlayer);
	
	if ("a" == text) ret = MUGEN_STATE_TYPE_AIR;
	else if ("s" == text) ret = MUGEN_STATE_TYPE_STANDING;
	else if ("c" == text) ret = MUGEN_STATE_TYPE_CROUCHING;
	else {
		logWarningFormat("Unrecognized state type %s. Defaulting to not changing state.", text);
		ret = MUGEN_STATE_TYPE_UNCHANGED;
	}

	return ret;
}

static DreamMugenStatePhysics handleStatePhysicsAssignment(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer) {
	DreamMugenStatePhysics ret;

	string text;
	evaluateDreamAssignmentAndReturnAsString(text, tAssignment, tPlayer);
	if ("s" == text) ret = MUGEN_STATE_PHYSICS_STANDING;
	else if ("a" == text) ret = MUGEN_STATE_PHYSICS_AIR;
	else if ("c" == text) ret = MUGEN_STATE_PHYSICS_CROUCHING;
	else if ("n" == text) ret = MUGEN_STATE_PHYSICS_NONE;
	else {
		logWarning("Unrecognized state physics type");
		logWarningString(text);
		ret = MUGEN_STATE_PHYSICS_UNCHANGED;
	}

	return ret;
}

static DreamMugenStateMoveType handleStateMoveTypeAssignment(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer) {
	DreamMugenStateMoveType ret;

	string text;
	evaluateDreamAssignmentAndReturnAsString(text, tAssignment, tPlayer);
	if ("a" == text) ret = MUGEN_STATE_MOVE_TYPE_ATTACK;
	else if ("i" == text) ret = MUGEN_STATE_MOVE_TYPE_IDLE;
	else {
		logWarning("Unrecognized state move type");
		logWarningString(text);
		ret = MUGEN_STATE_MOVE_TYPE_UNCHANGED;
	}

	return ret;
}

static int handleStateTypeSet(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	StateTypeSetController* e = (StateTypeSetController*)tController->mData;

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
	Set2DPhysicsController* e = (Set2DPhysicsController*)tController->mData;

	if (e->mIsSettingX) {
		double x = evaluateDreamAssignmentAndReturnAsFloat(&e->x, tPlayer);
		setActiveHitDataVelocityX(tPlayer, x, getActiveStateMachineCoordinateP());
	}

	if (e->mIsSettingY) {
		double y = evaluateDreamAssignmentAndReturnAsFloat(&e->y, tPlayer);
		setActiveHitDataVelocityY(tPlayer, y, getActiveStateMachineCoordinateP());
	}

	return 0;
}

static int handleDefenseMultiplier(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	DefenseMultiplierController* e = (DefenseMultiplierController*)tController->mData;

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

	setEnvironmentShake(time, freq, ampl, phase, getPlayerCoordinateP(tPlayer));

	return 0;
}

static void handleExplodAnimationGeneral(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int& tAnim, int& tIsInFightDefFile) {
	string text;
	evaluateDreamAssignmentAndReturnAsString(text, tAssignment, tPlayer);

	char firstW[100];
	int items = sscanf(text.data(), "%s %d", firstW, &tAnim);

	if (items > 1 && !strcmp("isinotherfilef", firstW)) {
		tIsInFightDefFile = 1;
		if (items < 2) tAnim = -1;
	}
	else {
		tIsInFightDefFile = 0;
		tAnim = atoi(text.data());
	}
}

static void handleExplodAnimationSet(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int tID) {
	int anim, isInFightDefFile;
	handleExplodAnimationGeneral(tAssignment, tPlayer, anim, isInFightDefFile);
	setExplodAnimation(tID, isInFightDefFile, anim);
}

static void handleExplodAnimationUpdate(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int tID) {
	int anim, isInFightDefFile;
	handleExplodAnimationGeneral(tAssignment, tPlayer, anim, isInFightDefFile);
	updateExplodAnimation(tPlayer, tID, isInFightDefFile, anim);
}

static DreamExplodSpace handleExplodSpaceGeneral(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer) {
	std::string spaceString;
	evaluateDreamAssignmentAndReturnAsString(spaceString, tAssignment, tPlayer);
	DreamExplodSpace space;
	if (spaceString == "") {
		space = EXPLOD_SPACE_NONE;
	}
	else if (spaceString == "screen") {
		space = EXPLOD_SPACE_SCREEN;
	}
	else {
		space = EXPLOD_SPACE_STAGE;
	}
	return space;
}

static void handleExplodSpaceSet(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int tID) {
	const auto space = handleExplodSpaceGeneral(tAssignment, tPlayer);
	setExplodSpace(tID, space);
}

static void handleExplodSpaceUpdate(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int tID) {
	const auto space = handleExplodSpaceGeneral(tAssignment, tPlayer);
	updateExplodSpace(tPlayer, tID, space);
}

DreamExplodPositionType getPositionTypeFromAssignment(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer) {
	string text;
	evaluateDreamAssignmentAndReturnAsString(text, tAssignment, tPlayer);

	DreamExplodPositionType type;
	if ("p1" == text) {
		type = EXPLOD_POSITION_TYPE_RELATIVE_TO_P1;
	}
	else if ("p2" == text) {
		type = EXPLOD_POSITION_TYPE_RELATIVE_TO_P2;
	}
	else if ("front" == text) {
		type = EXPLOD_POSITION_TYPE_RELATIVE_TO_FRONT;
	}
	else if ("back" == text) {
		type = EXPLOD_POSITION_TYPE_RELATIVE_TO_BACK;
	}
	else if ("left" == text) {
		type = EXPLOD_POSITION_TYPE_RELATIVE_TO_LEFT;
	}
	else if ("right" == text) {
		type = EXPLOD_POSITION_TYPE_RELATIVE_TO_RIGHT;
	}
	else if ("none" == text) {
		type = EXPLOD_POSITION_TYPE_NONE;
	}
	else {
		logWarningFormat("Unable to determine position type %s. Defaulting to EXPLOD_POSITION_TYPE_RELATIVE_TO_P1.", text);
		type = EXPLOD_POSITION_TYPE_RELATIVE_TO_P1;
	}

	return type;
}

static void handleExplodPositionTypeSet(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int tID) {
	DreamExplodPositionType type = getPositionTypeFromAssignment(tAssignment, tPlayer);
	setExplodPositionType(tID, type);
}

static void handleExplodPositionTypeUpdate(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int tID) {
	DreamExplodPositionType type = getPositionTypeFromAssignment(tAssignment, tPlayer);
	updateExplodPositionType(tPlayer, tID, type);
}

static DreamExplodTransparencyType handleExplodTransparencyTypeGeneral(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer) {
	string text;
	evaluateDreamAssignmentAndReturnAsString(text, tAssignment, tPlayer);

	DreamExplodTransparencyType type;
	if ("alpha" == text) {
		type = EXPLOD_TRANSPARENCY_TYPE_ALPHA;
	}
	else if ("addalpha" == text) {
		type = EXPLOD_TRANSPARENCY_TYPE_ADD_ALPHA;
	}
	else {
		logWarningFormat("Unable to determine explod transparency type %s. Default to alpha.", text);
		type = EXPLOD_TRANSPARENCY_TYPE_ALPHA;
	}
	return type;
}

static void handleExplodTransparencyTypeSet(DreamMugenAssignment** tAssignment, int isUsed, DreamPlayer* tPlayer, int tID) {
	if (!isUsed) {
		setExplodTransparencyType(tID, 0, EXPLOD_TRANSPARENCY_TYPE_ALPHA);
		return;
	}
	const auto type = handleExplodTransparencyTypeGeneral(tAssignment, tPlayer);
	setExplodTransparencyType(tID, 1, type);
}

static void handleExplodTransparencyTypeUpdate(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int tID) {
	const auto type = handleExplodTransparencyTypeGeneral(tAssignment, tPlayer);
	updateExplodTransparencyType(tPlayer, tID, type);
}

static int handleExplod(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	ExplodController* e = (ExplodController*)tController->mData;

	int id = addExplod(tPlayer);
	handleExplodAnimationSet(&e->mAnim, tPlayer, id);
	handleIDSetOneIntegerElement(&e->mID, tPlayer, id, setExplodID, -1);
	handleExplodSpaceSet(&e->mSpace, tPlayer, id);
	handleIDSetTwoIntegerElements(&e->mPosition, tPlayer, id, setExplodPosition, 0, 0);
	handleExplodPositionTypeSet(&e->mPositionType, tPlayer, id);
	handleIDSetOneIntegerElement(&e->mHorizontalFacing, tPlayer, id, setExplodHorizontalFacing, 1);
	handleIDSetOneIntegerElement(&e->mVerticalFacing, tPlayer, id, setExplodVerticalFacing, 1);
	handleIDSetOneIntegerElement(&e->mBindTime, tPlayer, id, setExplodBindTime, 0);
	handleIDSetTwoFloatElements(&e->mVelocity, tPlayer, id, setExplodVelocity, 0, 0);
	handleIDSetTwoFloatElements(&e->mAcceleration, tPlayer, id, setExplodAcceleration, 0, 0);
	handleIDSetTwoIntegerElements(&e->mRandomOffset, tPlayer, id, setExplodRandomOffset, 0, 0);

	handleIDSetOneIntegerElement(&e->mRemoveTime, tPlayer, id, setExplodRemoveTime, -2);
	if (e->mHasSuperMove) {
		handleIDSetOneIntegerElement(&e->mSuperMoveAndMoveTime, tPlayer, id, setExplodSuperMove, 0);
	}
	else {
		handleIDSetOneIntegerElement(&e->mSuperMoveAndMoveTime, tPlayer, id, setExplodSuperMoveTime, 0);
	}
	handleIDSetOneIntegerElement(&e->mPauseMoveTime, tPlayer, id, setExplodPauseMoveTime, 0);
	handleIDSetTwoFloatElements(&e->mScale, tPlayer, id, setExplodScale, 1, 1);
	handleIDSetOneIntegerElement(&e->mSpritePriority, tPlayer, id, setExplodSpritePriority, 0);
	handleIDSetOneIntegerElement(&e->mOnTop, tPlayer, id, setExplodOnTop, 0);
	handleIDSetThreeIntegerElements(&e->mShadow, tPlayer, id, setExplodShadow, 0, 0, 0);
	handleIDSetOneIntegerElement(&e->mOwnPalette, tPlayer, id, setExplodOwnPalette, 0);
	handleIDSetOneIntegerElement(&e->mIsRemovedOnGetHit, tPlayer, id, setExplodRemoveOnGetHit, 0);
	handleIDSetOneIntegerElement(&e->mIgnoreHitPause, tPlayer, id, setExplodIgnoreHitPause, 1);
	handleExplodTransparencyTypeSet(&e->mTransparencyType, e->mHasTransparencyType, tPlayer, id);

	finalizeExplod(id);

	return 0;
}

static int handleModifyExplod(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	ModifyExplodController* e = (ModifyExplodController*)tController->mData;
	int id;
	evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mID, tPlayer, &id, -1);
	
	if (e->mHasAnim) {
		handleExplodAnimationUpdate(&e->mAnim, tPlayer, id);
	}
	if (e->mHasSpace) {
		handleExplodSpaceUpdate(&e->mSpace, tPlayer, id);
	}
	if (e->mHasPositionType) {
		handleExplodPositionTypeUpdate(&e->mPositionType, tPlayer, id);
	}
	if (e->mHasPosition) {
		handlePlayerIDSetTwoIntegerElements(&e->mPosition, tPlayer, id, updateExplodPosition, 0, 0);
	}
	if (e->mHasHorizontalFacing) {
		handlePlayerIDSetOneIntegerElement(&e->mHorizontalFacing, tPlayer, id, updateExplodHorizontalFacing, 1);
	}
	if (e->mHasVerticalFacing) {
		handlePlayerIDSetOneIntegerElement(&e->mVerticalFacing, tPlayer, id, updateExplodVerticalFacing, 1);
	}
	if (e->mHasBindTime) {
		handlePlayerIDSetOneIntegerElement(&e->mBindTime, tPlayer, id, updateExplodBindTime, 0);
	}
	if (e->mHasVelocity) {
		handlePlayerIDSetTwoFloatElements(&e->mVelocity, tPlayer, id, updateExplodVelocity, 0, 0);
	}
	if (e->mHasAcceleration) {
		handlePlayerIDSetTwoFloatElements(&e->mAcceleration, tPlayer, id, updateExplodAcceleration, 0, 0);
	}
	if (e->mHasRandomOffset) {
		handlePlayerIDSetTwoIntegerElements(&e->mRandomOffset, tPlayer, id, updateExplodRandomOffset, 0, 0);
	}
	if (e->mHasRemoveTime) {
		handlePlayerIDSetOneIntegerElement(&e->mRemoveTime, tPlayer, id, updateExplodRemoveTime, -2);
	}
	if (e->mHasSuperMove) {
		handlePlayerIDSetOneIntegerElement(&e->mSuperMove, tPlayer, id, updateExplodSuperMove, 0);
	}
	if (e->mHasSuperMoveTime) {
		handlePlayerIDSetOneIntegerElement(&e->mSuperMoveTime, tPlayer, id, updateExplodSuperMoveTime, 0);
	}
	if (e->mHasPauseMoveTime) {
		handlePlayerIDSetOneIntegerElement(&e->mPauseMoveTime, tPlayer, id, updateExplodPauseMoveTime, 0);
	}
	if (e->mHasScale) {
		handlePlayerIDSetTwoFloatElements(&e->mScale, tPlayer, id, updateExplodScale, 1, 1);
	}
	if (e->mHasSpritePriority) {
		handlePlayerIDSetOneIntegerElement(&e->mSpritePriority, tPlayer, id, updateExplodSpritePriority, 0);
	}
	if (e->mHasOnTop) {
		handlePlayerIDSetOneIntegerElement(&e->mOnTop, tPlayer, id, updateExplodOnTop, 0);
	}
	if (e->mHasShadow) {
		handlePlayerIDSetThreeIntegerElements(&e->mShadow, tPlayer, id, updateExplodShadow, 0, 0, 0);
	}
	if (e->mHasOwnPalette) {
		handlePlayerIDSetOneIntegerElement(&e->mOwnPalette, tPlayer, id, updateExplodOwnPalette, 0);
	}
	if (e->mHasIsRemovedOnGetHit) {
		handlePlayerIDSetOneIntegerElement(&e->mIsRemovedOnGetHit, tPlayer, id, updateExplodRemoveOnGetHit, 0);
	}
	if (e->mHasIgnoreHitPause) {
		handlePlayerIDSetOneIntegerElement(&e->mIgnoreHitPause, tPlayer, id, updateExplodIgnoreHitPause, 1);
	}
	if (e->mHasTransparencyType) {
		handleExplodTransparencyTypeUpdate(&e->mTransparencyType, tPlayer, id);
	}

	return 0;
}

static int handleHitFallDamage(DreamPlayer* tPlayer) {
	if (!isActiveHitDataActive(tPlayer)) return 0;

	int fallDamage = getActiveHitDataFallDamage(tPlayer);
	addPlayerDamage(tPlayer, getPlayerOtherPlayer(tPlayer), fallDamage);

	return 0;
}

static int handlePositionFreeze(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	PositionFreezeController* e = (PositionFreezeController*)tController->mData;

	int val = evaluateDreamAssignment(&e->mValue, tPlayer);
	if (val) setPlayerPositionFrozen(tPlayer);

	return 0;
}

static int handleHitFallVelocity(DreamPlayer* tPlayer) {

	addPlayerVelocityX(tPlayer, getActiveHitDataFallXVelocity(tPlayer), getPlayerCoordinateP(tPlayer));
	addPlayerVelocityY(tPlayer, getActiveHitDataFallYVelocity(tPlayer), getPlayerCoordinateP(tPlayer));

	return 0;
}

static void handleSingleNotHitBy(int tSlot, int tHasValue, MugenStringVector& tValue, int tTime, DreamPlayer* tPlayer, void(*tResetFunc)(DreamPlayer*, int)) {
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

static int handleNotHitBy(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	NotHitByController* e = (NotHitByController*)tController->mData;

	int time = evaluateDreamAssignmentAndReturnAsInteger(&e->mTime, tPlayer);

	handleSingleNotHitBy(0, e->mHasValue, e->mValue, time, tPlayer, resetPlayerNotHitBy);
	handleSingleNotHitBy(1, e->mHasValue2, e->mValue2, time, tPlayer, resetPlayerNotHitBy);

	return 0;
}

static int handleHitBy(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	NotHitByController* e = (NotHitByController*)tController->mData;

	int time = evaluateDreamAssignmentAndReturnAsInteger(&e->mTime, tPlayer);

	handleSingleNotHitBy(0, e->mHasValue, e->mValue, time, tPlayer, resetPlayerHitBy);
	handleSingleNotHitBy(1, e->mHasValue2, e->mValue2, time, tPlayer, resetPlayerHitBy);

	return 0;
}

static int handleHitFallSet(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	HitFallSetController* e = (HitFallSetController*)tController->mData;
	DreamPlayer* otherPlayer = getPlayerOtherPlayer(tPlayer);

	int val = evaluateDreamAssignmentAndReturnAsInteger(&e->mValue, tPlayer);
	if (val != -1) {
		setActiveHitDataFall(otherPlayer, val);
	}

	if (e->mHasXVelocity) {
		double x = evaluateDreamAssignmentAndReturnAsFloat(&e->mXVelocity, tPlayer);
		setActiveHitDataFallXVelocity(otherPlayer, x, getActiveStateMachineCoordinateP());
	}

	if (e->mHasYVelocity) {
		double y = evaluateDreamAssignmentAndReturnAsFloat(&e->mYVelocity, tPlayer);
		setActiveHitDataFallYVelocity(otherPlayer, y, getActiveStateMachineCoordinateP());
	}

	return 0;
}


static int handleAttackMultiplierSetting(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	SingleRequiredValueController* e = (SingleRequiredValueController*)tController->mData;

	double value = evaluateDreamAssignmentAndReturnAsFloat(&e->mValue, tPlayer);

	setPlayerAttackMultiplier(tPlayer, value);

	return 0;
}

static int handlePowerAddition(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	SingleRequiredValueController* e = (SingleRequiredValueController*)tController->mData;

	int value = evaluateDreamAssignmentAndReturnAsInteger(&e->mValue, tPlayer);
	addPlayerPower(tPlayer, value);

	return 0;
}

static int handlePowerSettingController(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	SingleRequiredValueController* e = (SingleRequiredValueController*)tController->mData;

	int value = evaluateDreamAssignmentAndReturnAsInteger(&e->mValue, tPlayer);
	setPlayerPower(tPlayer, value);

	return 0;
}

static void handleSuperPauseAnimation(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer) {
	int isInPlayerFile;
	int id;
	if (!(*tAssignment)) {
		isInPlayerFile = 0;
		id = 30;
	}
	else {
		string flag;
		evaluateDreamAssignmentAndReturnAsString(flag, tAssignment, tPlayer);

		char firstW[200];
		int which = -1;

		int items = sscanf(flag.data(), "%s %d", firstW, &which);

		if (items < 1) {
			isInPlayerFile = 0;
			id = 30;
		}
		else if (!strcmp("isinotherfiles", firstW)) {
			assert(items == 2);
			isInPlayerFile = 1;
			id = which;
		}
		else if (!strcmp("isinotherfilef", firstW)) {
			assert(items == 2);
			isInPlayerFile = 0;
			id = which;
		}
		else {
			assert(items == 1);
			isInPlayerFile = 0;
			id = atoi(flag.data());
		}
	}

	setDreamSuperPauseAnimation(tPlayer, isInPlayerFile, id);
}

static void handleSuperPauseSound(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer) {
	int isInPlayerFile;
	int group;
	int item;
	if (!(*tAssignment)) {
		isInPlayerFile = 0;
		group = -1;
		item = -1;
	}
	else {
		string flag;
		evaluateDreamAssignmentAndReturnAsString(flag, tAssignment, tPlayer);

		char firstW[20], comma[10];
		int items = sscanf(flag.data(), "%s", firstW);
		(void)items;
		assert(items == 1);

		if (!strcmp("isinotherfilef", firstW)) {
			isInPlayerFile = 0;
			items = sscanf(flag.data(), "%s %d %s %d", firstW, &group, comma, &item);
			if (items < 2) { 
				group = 0;
			}
			if (items < 4) {
				item = 0;
			}
		}
		else if (!strcmp("isinotherfiles", firstW)) {
			isInPlayerFile = 1;
			items = sscanf(flag.data(), "%s %d %s %d", firstW, &group, comma, &item);
			if (items < 2) { 
				group = 0;
			}
			if (items < 4) {
				item = 0;
			}
		}
		else {
			isInPlayerFile = 0;
			items = sscanf(flag.data(), "%d %s %d", &group, comma, &item);
			if (items != 3) {
				logWarningFormat("Unable to parse super pause flag %s. Ignoring.", flag);
				return;
			}
		}
	}

	setDreamSuperPauseSound(tPlayer, isInPlayerFile, group, item);
}

static int handleSuperPause(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	SuperPauseController* e = (SuperPauseController*)tController->mData;

	if (!setDreamSuperPauseActiveAndReturnIfWorked(tPlayer)) {
		return 0;
	}

	handlePlayerSetOneIntegerElement(&e->mTime, tPlayer, setDreamSuperPauseTime, 30);
	handlePlayerSetOneIntegerElement(&e->mBufferTimeForCommandsDuringPauseEnd, tPlayer, setDreamSuperPauseBufferTimeForCommandsDuringPauseEnd, 0);
	handlePlayerSetOneIntegerElement(&e->mMoveTime, tPlayer, setDreamSuperPauseMoveTime, 0);
	handlePlayerSetOneIntegerElement(&e->mDoesPauseBackground, tPlayer, setDreamSuperPauseIsPausingBG, 1);

	handleSuperPauseAnimation(&e->mAnim, tPlayer);
	handleSuperPauseSound(&e->mSound, tPlayer);

	handlePlayerSetTwoFloatElementsWithCoordinateP(&e->mPosition, tPlayer, setDreamSuperPausePosition, 0, 0);
	handlePlayerSetOneIntegerElement(&e->mIsDarkening, tPlayer, setDreamSuperPauseDarkening, 1);
	handlePlayerSetOneFloatElement(&e->mPlayer2DefenseMultiplier, tPlayer, setDreamSuperPausePlayer2DefenseMultiplier, 0);
	handlePlayerSetOneIntegerElement(&e->mPowerToAdd, tPlayer, setDreamSuperPausePowerToAdd, 0);
	handlePlayerSetOneIntegerElement(&e->mSetPlayerUnhittable, tPlayer, setDreamSuperPausePlayerUnhittability, 1);

	return 0;
}

static int handlePauseController(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	PauseController* e = (PauseController*)tController->mData;

	if (!setDreamPauseActiveAndReturnIfWorked(tPlayer)) {
		return 0;
	}

	handlePlayerSetOneIntegerElement(&e->mTime, tPlayer, setDreamPauseTime, 30);
	handlePlayerSetOneIntegerElement(&e->mBufferTimeForCommandsDuringPauseEnd, tPlayer, setDreamPauseBufferTimeForCommandsDuringPauseEnd, 0);
	handlePlayerSetOneIntegerElement(&e->mMoveTime, tPlayer, setDreamPauseMoveTime, 0);
	handlePlayerSetOneIntegerElement(&e->mDoesPauseBackground, tPlayer, setDreamPauseIsPausingBG, 1);

	return 0;
}

static void handleHelperFacing(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, DreamPlayer* tHelper) {
	int facing;
	evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(tAssignment, tPlayer, &facing, 1);

	if (facing == 1) {
		setPlayerIsFacingRight(tHelper, getPlayerIsFacingRight(tPlayer));
	}
	else {
		setPlayerIsFacingRight(tHelper, !getPlayerIsFacingRight(tPlayer));
	}

}

static Position2D getFinalHelperPositionFromPositionType(DreamExplodPositionType tPositionType, Position2D mOffset, DreamPlayer* tPlayer) {
	const auto p1 = getPlayerPosition(tPlayer, getActiveStateMachineCoordinateP());

	if (tPositionType == EXPLOD_POSITION_TYPE_RELATIVE_TO_P1) {
		DreamPlayer* target = tPlayer;
		int isReversed = !getPlayerIsFacingRight(target);
		if (isReversed) mOffset.x *= -1;
		return p1 + mOffset;
	}
	else if (tPositionType == EXPLOD_POSITION_TYPE_RELATIVE_TO_P2) {
		DreamPlayer* target = getPlayerOtherPlayer(tPlayer);
		const auto p2 = getPlayerPosition(target, getActiveStateMachineCoordinateP());
		int isReversed = !getPlayerIsFacingRight(target);
		if (isReversed) mOffset.x *= -1;
		return p2 + mOffset;
	}
	else if (tPositionType == EXPLOD_POSITION_TYPE_RELATIVE_TO_FRONT) {
		DreamPlayer* target = tPlayer;
		const auto p = Vector2D(getPlayerScreenEdgeInFrontX(target, getActiveStateMachineCoordinateP()), p1.y);
		int isReversed = getPlayerIsFacingRight(target);
		if (isReversed) mOffset.x *= -1;
		return p + mOffset;
	}
	else if (tPositionType == EXPLOD_POSITION_TYPE_RELATIVE_TO_BACK) {
		DreamPlayer* target = tPlayer;
		const auto p = Vector2D(getPlayerScreenEdgeInBackX(target, getActiveStateMachineCoordinateP()), p1.y);
		int isReversed = !getPlayerIsFacingRight(target);
		if (isReversed) mOffset.x *= -1;
		return p + mOffset;
	}
	else if (tPositionType == EXPLOD_POSITION_TYPE_RELATIVE_TO_LEFT) {
		if (getPlayerIsFacingRight(tPlayer)) {
			const auto p = Vector2D(getDreamStageLeftOfScreenBasedOnPlayer(getActiveStateMachineCoordinateP()), p1.y);
			return p + mOffset;
		}
		else {
			auto p = Vector2D(getDreamStageRightOfScreenBasedOnPlayer(getActiveStateMachineCoordinateP()), p1.y);
			p.x -= mOffset.x;
			p.y += mOffset.y;
			return p;
		}
	}
	else if (tPositionType == EXPLOD_POSITION_TYPE_RELATIVE_TO_RIGHT) {
		if (getPlayerIsFacingRight(tPlayer)) {
			const auto p = Vector2D(getDreamStageRightOfScreenBasedOnPlayer(getActiveStateMachineCoordinateP()), p1.y);
			return p + mOffset;
		}
		else {
			auto p = Vector2D(getDreamStageLeftOfScreenBasedOnPlayer(getActiveStateMachineCoordinateP()), p1.y);
			p.x -= mOffset.x;
			p.y += mOffset.y;
			return p;
		}
	}
	else if (tPositionType == EXPLOD_POSITION_TYPE_NONE) {
		const auto p = Vector2D(getDreamGameWidth(getActiveStateMachineCoordinateP()) / 2, p1.y);
		return p + mOffset;
	}
	else {
		logWarningFormat("Unrecognized position type %d. Defaulting to EXPLOD_POSITION_TYPE_RELATIVE_TO_P1.", tPositionType);
		DreamPlayer* target = tPlayer;
		int isReversed = !getPlayerIsFacingRight(target);
		if (isReversed) mOffset.x *= -1;
		return p1 + mOffset;
	}
}

static void handleHelperScale(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, DreamPlayer* tHelper, void(tFunc)(DreamPlayer*, double), double tParentScale) {
	double val;
	evaluateDreamAssignmentAndReturnAsOneFloatWithDefaultValue(tAssignment, tPlayer, &val, 1.0);
	tFunc(tHelper, val*tParentScale);
}

static int handleHelper(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	HelperController* e = (HelperController*)tController->mData;
	DreamPlayer* helper = clonePlayerAsHelper(tPlayer);

	handleHelperSetOneIntegerElement(&e->mID, tPlayer, helper, setPlayerID, 0);

	Vector2DI offset;
	evaluateDreamAssignmentAndReturnAsTwoIntegersWithDefaultValues(&e->mPosition, tPlayer, &offset.x, &offset.y, 0, 0);
	DreamExplodPositionType positionType = getPositionTypeFromAssignment(&e->mPositionType, tPlayer);
	const auto position = getFinalHelperPositionFromPositionType(positionType, offset.f(), tPlayer);

	handleHelperFacing(&e->mFacing, tPlayer, helper);

	handleHelperSetOneIntegerElement(&e->mCanControl, tPlayer, helper, setPlayerHelperControl, 0);
	handleHelperSetOneIntegerElement(&e->mHasOwnPalette, tPlayer, helper, setPlayerHasOwnPalette, 0);
	handleHelperSetOneIntegerElement(&e->mSuperMoveTime, tPlayer, helper, setPlayerSuperMoveTime, 0);
	handleHelperSetOneIntegerElement(&e->mPauseMoveTime, tPlayer, helper, setPlayerPauseMoveTime, 0);

	handleHelperScale(&e->mSizeScaleX, tPlayer, helper, setPlayerScaleX, getPlayerScaleX(tPlayer));
	handleHelperScale(&e->mSizeScaleY, tPlayer, helper, setPlayerScaleY, getPlayerScaleY(tPlayer));
	handleHelperSetOneIntegerElementWithCoordinateP(&e->mSizeGroundBack, tPlayer, helper, setPlayerGroundSizeBack, getPlayerGroundSizeBack(tPlayer, getActiveStateMachineCoordinateP()));
	handleHelperSetOneIntegerElementWithCoordinateP(&e->mSizeGroundFront, tPlayer, helper, setPlayerGroundSizeFront, getPlayerGroundSizeFront(tPlayer, getActiveStateMachineCoordinateP()));
	handleHelperSetOneIntegerElementWithCoordinateP(&e->mSizeAirBack, tPlayer, helper, setPlayerAirSizeBack, getPlayerAirSizeBack(tPlayer, getActiveStateMachineCoordinateP()));
	handleHelperSetOneIntegerElementWithCoordinateP(&e->mSizeAirFront, tPlayer, helper, setPlayerAirSizeFront, getPlayerAirSizeFront(tPlayer, getActiveStateMachineCoordinateP()));
	handleHelperSetOneIntegerElementWithCoordinateP(&e->mSizeHeight, tPlayer, helper, setPlayerHeight, getPlayerHeight(tPlayer, getActiveStateMachineCoordinateP()));
	handleHelperSetOneIntegerElement(&e->mSizeProjectilesDoScale, tPlayer, helper, setPlayerDoesScaleProjectiles, getPlayerDoesScaleProjectiles(tPlayer));
	handleHelperSetTwoFloatElementsWithCoordinateP(&e->mSizeHeadPosition, tPlayer, helper, setPlayerHeadPosition, getPlayerHeadPositionX(tPlayer, getActiveStateMachineCoordinateP()), getPlayerHeadPositionY(tPlayer, getActiveStateMachineCoordinateP()));
	handleHelperSetTwoFloatElementsWithCoordinateP(&e->mSizeMiddlePosition, tPlayer, helper, setPlayerMiddlePosition, getPlayerMiddlePositionX(tPlayer, getActiveStateMachineCoordinateP()), getPlayerMiddlePositionY(tPlayer, getActiveStateMachineCoordinateP()));
	handleHelperSetOneIntegerElementWithCoordinateP(&e->mSizeShadowOffset, tPlayer, helper, setPlayerShadowOffset, getPlayerShadowOffset(tPlayer, getActiveStateMachineCoordinateP()));

	string type;
	evaluateDreamAssignmentAndReturnAsString(type, &e->mType, tPlayer);
	if ("player" == type) {
		setPlayerScreenBoundForever(helper, 1);
	}
	else {
		setPlayerScreenBoundForever(helper, 0);
	}

	setPlayerPosition(helper, position, getActiveStateMachineCoordinateP());
	handleHelperSetOneIntegerElement(&e->mStateNumber, tPlayer, helper, changePlayerState, 0);

	return 0;
}

static int handleDestroySelf(DreamPlayer* tPlayer) {
	logFormat("%d %d destroying self\n", tPlayer->mRootID, tPlayer->mID);
	return destroyPlayer(tPlayer);
}

static int handleAddingLife(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	LifeAddController* e = (LifeAddController*)tController->mData;

	int val, canKill, isAbsolute;
	evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mValue, tPlayer, &val, 0);
	evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mCanKill, tPlayer, &canKill, 1);
	evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mIsAbsolute, tPlayer, &isAbsolute, 0);

	if (!isAbsolute) {
		const auto defenseMultiplier = getInvertedPlayerDefenseMultiplier(tPlayer);
		if (defenseMultiplier != 1.0) val = int(val * defenseMultiplier);
	}

	int playerLife = getPlayerLife(tPlayer);
	if (!canKill && playerLife + val <= 0) {
		val = -(playerLife - 1);
	}

	addPlayerDamage(tPlayer, tPlayer, -val);

	return 0;
}

static int handleAddingTargetLife(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	TargetLifeAddController* e = (TargetLifeAddController*)tController->mData;

	int val, canKill, isAbsolute, id;
	evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mValue, tPlayer, &val, 0);
	evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mID, tPlayer, &id, -1);
	evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mCanKill, tPlayer, &canKill, 1);
	evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mIsAbsolute, tPlayer, &isAbsolute, 0);

	addPlayerTargetLife(tPlayer, tPlayer, id, val, canKill, isAbsolute);

	return 0;
}

static int handleAddingTargetPower(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	TargetPowerAddController* e = (TargetPowerAddController*)tController->mData;

	int val, id;
	evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mValue, tPlayer, &val, 0);
	evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mID, tPlayer, &id, -1);

	addPlayerTargetPower(tPlayer, id, val);

	return 0;
}

static int handleTargetVelocityAddController(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	Target2DPhysicsController* e = (Target2DPhysicsController*)tController->mData;

	int id;
	evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mID, tPlayer, &id, -1);

	if (e->mIsSettingX) {
		double x = evaluateDreamAssignmentAndReturnAsFloat(&e->x, tPlayer);
		addPlayerTargetVelocityX(tPlayer, id, x, getActiveStateMachineCoordinateP());
	}

	if (e->mIsSettingY) {
		double y = evaluateDreamAssignmentAndReturnAsFloat(&e->y, tPlayer);
		addPlayerTargetVelocityY(tPlayer, id, y, getActiveStateMachineCoordinateP());
	}

	return 0;
}

static int handleTargetVelocitySetController(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	Target2DPhysicsController* e = (Target2DPhysicsController*)tController->mData;

	int id;
	evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mID, tPlayer, &id, -1);

	if (e->mIsSettingX) {
		double x = evaluateDreamAssignmentAndReturnAsFloat(&e->x, tPlayer);
		setPlayerTargetVelocityX(tPlayer, id, x, getActiveStateMachineCoordinateP());
	}

	if (e->mIsSettingY) {
		double y = evaluateDreamAssignmentAndReturnAsFloat(&e->y, tPlayer);
		setPlayerTargetVelocityY(tPlayer, id, y, getActiveStateMachineCoordinateP());
	}

	return 0;
}

static int handleAngleDrawController(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	AngleDrawController* e = (AngleDrawController*)tController->mData;

	if (e->mHasScale) {
		const auto scale = evaluateDreamAssignmentAndReturnAsVector2D(&e->mScale, tPlayer);
		setPlayerTempScaleActive(tPlayer, scale);
	}

	if (e->mHasValue) {
		double val = evaluateDreamAssignmentAndReturnAsFloat(&e->mValue, tPlayer);
		setPlayerDrawAngleValue(tPlayer, val);
	}

	setPlayerDrawAngleActive(tPlayer);

	return 0;
}

static int handleAngleAddController(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	SingleRequiredValueController* e = (SingleRequiredValueController*)tController->mData;

	double angle = evaluateDreamAssignmentAndReturnAsFloat(&e->mValue, tPlayer);
	addPlayerDrawAngle(tPlayer, angle);

	return 0;
}

static int handleAngleMulController(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	SingleRequiredValueController* e = (SingleRequiredValueController*)tController->mData;

	double angle = evaluateDreamAssignmentAndReturnAsFloat(&e->mValue, tPlayer);
	multiplyPlayerDrawAngle(tPlayer, angle);

	return 0;
}

static int handleAngleSetController(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	SingleRequiredValueController* e = (SingleRequiredValueController*)tController->mData;

	double angle = evaluateDreamAssignmentAndReturnAsFloat(&e->mValue, tPlayer);
	setPlayerDrawAngleValue(tPlayer, angle);

	return 0;
}

static int handleRemovingExplod(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	RemoveExplodController* e = (RemoveExplodController*)tController->mData;

	if (e->mHasID) {
		int id = evaluateDreamAssignmentAndReturnAsInteger(&e->mID, tPlayer);
		removeExplodsWithID(tPlayer, id);
	}
	else {
		removeAllExplodsForPlayer(tPlayer);
	}

	return 0;
}

static int handleBindToRootController(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	BindController* e = (BindController*)tController->mData;

	int time, facing;
	Vector2D offset;
	evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mTime, tPlayer, &time, 1);
	evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mFacing, tPlayer, &facing, 0);
	evaluateDreamAssignmentAndReturnAsTwoFloatsWithDefaultValues(&e->mPosition, tPlayer, &offset.x, &offset.y, 0, 0);

	bindPlayerToRoot(tPlayer, time, facing, offset, getActiveStateMachineCoordinateP());

	return 0;
}

static int handleBindToParentController(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	BindController* e = (BindController*)tController->mData;

	int time, facing;
	Vector2D offset;
	evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mTime, tPlayer, &time, 1);
	evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mFacing, tPlayer, &facing, 0);
	evaluateDreamAssignmentAndReturnAsTwoFloatsWithDefaultValues(&e->mPosition, tPlayer, &offset.x, &offset.y, 0, 0);

	bindPlayerToParent(tPlayer, time, facing, offset, getActiveStateMachineCoordinateP());

	return 0;
}

static DreamPlayerBindPositionType handleBindToTargetPositionType(DreamMugenAssignment** tPosition, DreamPlayer* tPlayer) {
	string text;
	evaluateDreamAssignmentAndReturnAsString(text, tPosition, tPlayer);

	char val1[20], val2[20], comma1[10], comma2[10], postype[30];
	int items = sscanf(text.data(), "%s %s %s %s %s", val1, comma1, val2, comma2, postype);

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
	BindController* e = (BindController*)tController->mData;

	int time, id;
	Vector2D offset;
	evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mTime, tPlayer, &time, 1);
	evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mID, tPlayer, &id, -1);
	evaluateDreamAssignmentAndReturnAsTwoFloatsWithDefaultValues(&e->mPosition, tPlayer, &offset.x, &offset.y, 0, 0);
	DreamPlayerBindPositionType bindType = handleBindToTargetPositionType(&e->mPosition, tPlayer);

	bindPlayerToTarget(tPlayer, time, offset, bindType, id, getActiveStateMachineCoordinateP());

	return 0;
}

static int handleTurnController(DreamPlayer* tPlayer) {
	turnPlayerAround(tPlayer);

	return 0;
}

static int handlePushPlayerController(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	SingleRequiredValueController* e = (SingleRequiredValueController*)tController->mData;

	const auto isPushEnabled = evaluateDreamAssignment(&e->mValue, tPlayer);
	setPlayerPushDisabledFlag(tPlayer, !isPushEnabled);

	return 0;
}

static int handleSettingVariableRange(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	VarRangeSetController* e = (VarRangeSetController*)tController->mData;

	if (e->mType == VAR_SET_TYPE_INTEGER) {
		int value, first, last;
		evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mValue, tPlayer, &value, 0);
		evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mFirst, tPlayer, &first, 0);
		evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mLast, tPlayer, &last, 59);
		int i;
		for (i = first; i <= last; i++) {
			setPlayerVariable(tPlayer, i, value);
		}
	}
	else if (e->mType == VAR_SET_TYPE_FLOAT) {
		double value;
		int first, last;
		evaluateDreamAssignmentAndReturnAsOneFloatWithDefaultValue(&e->mValue, tPlayer, &value, 0);
		evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mFirst, tPlayer, &first, 0);
		evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mLast, tPlayer, &last, 39);
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
	ScreenBoundController* e = (ScreenBoundController*)tController->mData;

	int val;
	int moveCameraX, moveCameraY;

	evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mValue, tPlayer, &val, 0);
	evaluateDreamAssignmentAndReturnAsTwoIntegersWithDefaultValues(&e->mMoveCameraFlags, tPlayer, &moveCameraX, &moveCameraY, 0, 0);

	setPlayerScreenBoundForTick(tPlayer, val, moveCameraX, moveCameraY);

	return 0;
}

static int handleMoveHitReset(DreamPlayer* tPlayer) {
	setPlayerMoveHitReset(tPlayer);

	return 0;
}

static int handleGravity(DreamPlayer* tPlayer) {
	double accel = getPlayerVerticalAcceleration(tPlayer, getActiveStateMachineCoordinateP());
	addPlayerVelocityY(tPlayer, accel, getActiveStateMachineCoordinateP());

	return 0;
}

static int handleSettingAttackDistance(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	SingleRequiredValueController* e = (SingleRequiredValueController*)tController->mData;

	int value = evaluateDreamAssignmentAndReturnAsInteger(&e->mValue, tPlayer);
	setHitDataGuardDistance(tPlayer, value);

	return 0;
}

static int handleTargetBindController(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	BindController* e = (BindController*)tController->mData;

	int time, id;
	Vector2D offset;
	evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mTime, tPlayer, &time, 1);
	evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mID, tPlayer, &id, -1);
	evaluateDreamAssignmentAndReturnAsTwoFloatsWithDefaultValues(&e->mPosition, tPlayer, &offset.x, &offset.y, 0, 0);

	bindPlayerTargetToPlayer(tPlayer, time, offset, id, getActiveStateMachineCoordinateP());

	return 0;
}

static int handleSetTargetFacing(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	SetTargetFacingController* e = (SetTargetFacingController*)tController->mData;

	int value = evaluateDreamAssignmentAndReturnAsInteger(&e->mValue, tPlayer);
	int id;
	evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mID, tPlayer, &id, -1);

	setPlayerTargetFacing(tPlayer, id, value);

	return 0;
}

static void handleReversalDefinitionEntry(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer) {
	std::string attrString;
	evaluateDreamAssignmentAndReturnAsString(attrString, tAssignment, tPlayer);
	auto commaPos = attrString.find(',');
	const auto flag1 = attrString.substr(0, commaPos);
	setHitDataReversalDefFlag1(tPlayer, flag1.c_str());

	while (commaPos != std::string::npos) {
		commaPos++;
		const auto endPos = attrString.find(',', commaPos);
		const auto flag2 = attrString.substr(commaPos, (endPos - commaPos - 1));
		addHitDataReversalDefFlag2(tPlayer, flag2.c_str());
		commaPos = endPos;
	}
}

static int handleReversalDefinition(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	ReversalDefinitionController* e = (ReversalDefinitionController*)tController->mData;

	setHitDataReversalDefActive(tPlayer);
	handleReversalDefinitionEntry(&e->mAttributes, tPlayer);
	handleHelperSetTwoIntegerElements(&e->mPauseTime, tPlayer, tPlayer, setReversalDefPauseTime, 0, 0);
	handleHitDefinitionSparkNumberSingle(&e->mSparkNumber, tPlayer, tPlayer, setReversalDefSparkNumber, getDefaultPlayerSparkNumberIsInPlayerFile(tPlayer), getDefaultPlayerSparkNumber(tPlayer));
	handleHitDefinitionSparkXY(&e->mSparkXY, tPlayer, tPlayer, setReversalDefSparkXY);
	handleHitDefinitionSingleSound(&e->mHitSound, tPlayer, tPlayer, setReversalDefHitSound, 5, 0);
	if (e->mHasP1StateNo) {
		int p1stateNo;
		evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mP1StateNo, tPlayer, &p1stateNo, 0);
		setReversalDefP1StateNo(tPlayer, 1, p1stateNo);
	}
	else {
		setReversalDefP1StateNo(tPlayer, 0);
	}
	if (e->mHasP2StateNo) {
		int p2stateNo;
		evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mP2StateNo, tPlayer, &p2stateNo, 0);
		setReversalDefP2StateNo(tPlayer, 1, p2stateNo);
	}
	else {
		setReversalDefP2StateNo(tPlayer, 0);
	}

	return 0;
}

static Position2D getFinalProjectilePositionFromPositionType(DreamExplodPositionType tPositionType, Position2D mOffset, DreamPlayer* tPlayer) {
	const auto p1 = getPlayerPosition(tPlayer, getActiveStateMachineCoordinateP());

	if (tPositionType == EXPLOD_POSITION_TYPE_RELATIVE_TO_P1) {
		DreamPlayer* target = tPlayer;
		int isReversed = !getPlayerIsFacingRight(target);
		if (isReversed) mOffset.x *= -1;
		return p1 + mOffset;
	}
	else if (tPositionType == EXPLOD_POSITION_TYPE_RELATIVE_TO_P2) {
		DreamPlayer* target = getPlayerOtherPlayer(tPlayer);
		const auto p2 = getPlayerPosition(target, getActiveStateMachineCoordinateP());
		int isReversed = !getPlayerIsFacingRight(target);
		if (isReversed) mOffset.x *= -1;
		return p2 + mOffset;
	}
	else if (tPositionType == EXPLOD_POSITION_TYPE_RELATIVE_TO_FRONT) {
		DreamPlayer* target = tPlayer;
		const auto p = Vector2D(getPlayerScreenEdgeInFrontX(target, getActiveStateMachineCoordinateP()), p1.y);
		int isReversed = getPlayerIsFacingRight(target);
		if (isReversed) mOffset.x *= -1;
		return p + mOffset;
	}
	else if (tPositionType == EXPLOD_POSITION_TYPE_RELATIVE_TO_BACK) {
		DreamPlayer* target = tPlayer;
		const auto p = Vector2D(getPlayerScreenEdgeInBackX(target, getActiveStateMachineCoordinateP()), p1.y);
		int isReversed = !getPlayerIsFacingRight(target);
		if (isReversed) mOffset.x *= -1;
		return p + mOffset;
	}
	else if (tPositionType == EXPLOD_POSITION_TYPE_RELATIVE_TO_LEFT) {
		if (getPlayerIsFacingRight(tPlayer)) {
			const auto p = Vector2D(getDreamStageLeftOfScreenBasedOnPlayer(getActiveStateMachineCoordinateP()), p1.y);
			return p + mOffset;
		}
		else {
			auto p = Vector2D(getDreamStageRightOfScreenBasedOnPlayer(getActiveStateMachineCoordinateP()), p1.y);
			p.x -= mOffset.x;
			p.y += mOffset.y;
			return p;
		}
	}
	else if (tPositionType == EXPLOD_POSITION_TYPE_RELATIVE_TO_RIGHT) {
		if (getPlayerIsFacingRight(tPlayer)) {
			const auto p = Vector2D(getDreamStageRightOfScreenBasedOnPlayer(getActiveStateMachineCoordinateP()), p1.y);
			return p + mOffset;
		}
		else {
			auto p = Vector2D(getDreamStageLeftOfScreenBasedOnPlayer(getActiveStateMachineCoordinateP()), p1.y);
			p.x -= mOffset.x;
			p.y += mOffset.y;
			return p;
		}
	}
	else if (tPositionType == EXPLOD_POSITION_TYPE_NONE) {
		const auto p = Vector2D(getDreamGameWidth(getActiveStateMachineCoordinateP()) / 2, p1.y);
		return p + mOffset;
	}
	else {
		logWarningFormat("Unrecognized position type %d. Defaulting to EXPLOD_POSITION_TYPE_RELATIVE_TO_P1.", tPositionType);
		DreamPlayer* target = tPlayer;
		int isReversed = !getPlayerIsFacingRight(target);
		if (isReversed) mOffset.x *= -1;
		return p1 + mOffset;
	}

}

static int handleAfterImageGeneral(AfterImageController* e, DreamPlayer* tPlayer, DreamPlayer* tHelper, int tDefaultDuration, int tDefaultBufferLength) {
	if (isOnDreamcast()) return 0;

	int historyBufferLength, duration, timeGap, frameGap;
	Vector3DI palBright, palContrast, palPostBright, palAdd;
	Vector3D colorMul;
	int palColor, palInvertAll;
	std::string blendTypeString;
	BlendType blendType;
	evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mTime, tPlayer, &duration, tDefaultDuration);
	if (!duration) return 0;
	evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mLength, tPlayer, &historyBufferLength, tDefaultBufferLength);
	if (!historyBufferLength) return 0;
	historyBufferLength = std::min(60, historyBufferLength);
	evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mPalColor, tPlayer, &palColor, 256);
	evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mPalInvertAll, tPlayer, &palInvertAll, 0);
	evaluateDreamAssignmentAndReturnAsThreeIntegersWithDefaultValues(&e->mPalBright, tPlayer, &palBright.x, &palBright.y, &palBright.z, 30, 30, 30);
	evaluateDreamAssignmentAndReturnAsThreeIntegersWithDefaultValues(&e->mPalContrast, tPlayer, &palContrast.x, &palContrast.y, &palContrast.z, 120, 120, 220);
	evaluateDreamAssignmentAndReturnAsThreeIntegersWithDefaultValues(&e->mPalPostBright, tPlayer, &palPostBright.x, &palPostBright.y, &palPostBright.z, 0, 0, 0);
	const auto startColor = (((Vector3DI(palColor, palColor, palColor) + palBright) * palContrast) / 256.0 + palPostBright) / 255.0;
	evaluateDreamAssignmentAndReturnAsThreeIntegersWithDefaultValues(&e->mPalAdd, tPlayer, &palAdd.x, &palAdd.y, &palAdd.z, 10, 10, 25);
	const auto colorAdd = palAdd / 255.0;
	evaluateDreamAssignmentAndReturnAsThreeFloatsWithDefaultValues(&e->mPalMul, tPlayer, &colorMul.x, &colorMul.y, &colorMul.z, 0.65, 0.65, 0.75);
	evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mTimeGap, tPlayer, &timeGap, 1);
	evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mFrameGap, tPlayer, &frameGap, 4);
	evaluateDreamAssignmentAndReturnAsString(blendTypeString, &e->mTrans, tPlayer);

	if (blendTypeString == "none") {
		blendType = BLEND_TYPE_NORMAL;
	}
	else if (blendTypeString == "add" || blendTypeString == "add1") {
		blendType = BLEND_TYPE_ADDITION;
	}
	else {
		blendType = BLEND_TYPE_SUBTRACTION;
	}

	addAfterImage(tHelper, historyBufferLength, duration, timeGap, frameGap, startColor, colorAdd, colorMul, palInvertAll, blendType);
	return 0;
}

static int handleProjectile(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	ProjectileController* e = (ProjectileController*)tController->mData;

	DreamPlayer* p = createNewProjectileFromPlayer(tPlayer);
	if (!getPlayerDoesScaleProjectiles(tPlayer)) {
		setPlayerScaleX(p, 1);
		setPlayerScaleY(p, 1);
	}

	handleHelperSetOneIntegerElement(&e->mID, tPlayer, p, setProjectileID, -1);
	handleHelperSetOneIntegerElement(&e->mAnimation, tPlayer, p, setProjectileAnimation, 0);
	handleHelperSetOneIntegerElement(&e->mHitAnimation, tPlayer, p, setProjectileHitAnimation, -1);
	handleHelperSetOneIntegerElement(&e->mRemoveAnimation, tPlayer, p, setProjectileRemoveAnimation, getProjectileHitAnimation(p));
	handleHelperSetOneIntegerElement(&e->mCancelAnimation, tPlayer, p, setProjectileCancelAnimation, getProjectileRemoveAnimation(p));
	handleHelperSetTwoFloatElements(&e->mScale, tPlayer, p, setProjectileScale, 1, 1);
	handleHelperSetOneIntegerElement(&e->mIsRemovingProjectileAfterHit, tPlayer, p, setProjectileRemoveAfterHit, 1);
	handleHelperSetOneIntegerElement(&e->mRemoveTime, tPlayer, p, setProjectileRemoveTime, -1);
	handleHelperSetTwoFloatElementsWithCoordinateP(&e->mVelocity, tPlayer, p, setProjectileVelocity, 0, 0);
	handleHelperSetTwoFloatElementsWithCoordinateP(&e->mRemoveVelocity, tPlayer, p, setProjectileRemoveVelocity, 0, 0);
	handleHelperSetTwoFloatElementsWithCoordinateP(&e->mAcceleration, tPlayer, p, setProjectileAcceleration, 0, 0);
	handleHelperSetTwoFloatElements(&e->mVelocityMultipliers, tPlayer, p, setProjectileVelocityMultipliers, 1, 1);

	handleHelperSetOneIntegerElement(&e->mHitAmountBeforeVanishing, tPlayer, p, setProjectileHitAmountBeforeVanishing, 1);
	handleHelperSetOneIntegerElement(&e->mMissTime, tPlayer, p, setProjectilMisstime, 0);
	handleHelperSetOneIntegerElement(&e->mPriority, tPlayer, p, setProjectilePriority, 1);
	handleHelperSetOneIntegerElement(&e->mSpriteSpriority, tPlayer, p, setProjectileSpritePriority, 3);

	handleHelperSetOneIntegerElementWithCoordinateP(&e->mEdgeBound, tPlayer, p, setProjectileEdgeBound, (int)transformDreamCoordinates(40, 320, getActiveStateMachineCoordinateP()));
	handleHelperSetOneIntegerElementWithCoordinateP(&e->mStageBound, tPlayer, p, setProjectileStageBound, (int)transformDreamCoordinates(40, 320, getActiveStateMachineCoordinateP()));
	handleHelperSetTwoIntegerElementsWithCoordinateP(&e->mHeightBoundValues, tPlayer, p, setProjectileHeightBoundValues, (int)transformDreamCoordinates(-240, 320, getActiveStateMachineCoordinateP()), (int)transformDreamCoordinates(1, 320, getActiveStateMachineCoordinateP()));

	Position2D offset;
	evaluateDreamAssignmentAndReturnAsTwoFloatsWithDefaultValues(&e->mOffset, p, &offset.x, &offset.y, 0, 0);
	int positionType;
	evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mPositionType, p, &positionType, EXPLOD_POSITION_TYPE_RELATIVE_TO_P1);
	const auto pos = getFinalProjectilePositionFromPositionType((DreamExplodPositionType)positionType, offset, tPlayer);
	setProjectilePosition(p, pos, getActiveStateMachineCoordinateP());

	handleHelperSetOneIntegerElement(&e->mShadow, tPlayer, p, setProjectileShadow, 0);
	handleHelperSetOneIntegerElement(&e->mSuperMoveTime, tPlayer, p, setProjectileSuperMoveTime, 0);
	handleHelperSetOneIntegerElement(&e->mPauseMoveTime, tPlayer, p, setProjectilePauseMoveTime, 0);

	handleHelperSetOneIntegerElement(&e->mHasOwnPalette, tPlayer, p, setProjectileHasOwnPalette, 0);
	handleHelperSetTwoIntegerElements(&e->mRemapPalette, tPlayer, p, setProjectileRemapPalette, -1, 0);

	handleAfterImageGeneral(&e->mAfterImage, tPlayer, p, 0, 0);

	handleHitDefinitionWithController(&e->mHitDef, tPlayer, p);

	return 0;
}

int handleDreamMugenStateControllerAndReturnWhetherStateChanged(DreamMugenStateController * tController, DreamPlayer* tPlayer)
{

	if (!stl_map_contains(gMugenStateControllerVariableHandler.mStateControllerHandlers, (int)tController->mType)) {
		logWarningFormat("Unrecognized state controller %d. Ignoring.", tController->mType);
		return 0;
	}

	StateControllerHandleFunction func = gMugenStateControllerVariableHandler.mStateControllerHandlers[tController->mType];
	return func(tController, tPlayer);
}

static int handleAfterImage(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	AfterImageController* e = (AfterImageController*)tController->mData;
	return handleAfterImageGeneral(e, tPlayer, tPlayer, 1, 20);
}

static int handleAfterImageTime(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	AfterImageTimeController* e = (AfterImageTimeController*)tController->mData;
	int duration;
	evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mTime, tPlayer, &duration, 1);
	setAfterImageDuration(tPlayer, duration);
	return 0;
}

static int handleAppendToClipboardController(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	ClipboardController* e = (ClipboardController*)tController->mData;
	string formatString, parameterString;
	if(e->mText) evaluateDreamAssignmentAndReturnAsString(formatString, &e->mText, tPlayer);
	if (e->mParams) evaluateDreamAssignmentAndReturnAsString(parameterString, &e->mParams, tPlayer);

	addClipboardLineFormatString(formatString.data(), parameterString.data());

	return 0;
}

static void parsePalFXControllerGeneral(PalFXController* e, DreamPlayer* tPlayer, int& tTime, Vector3DI& tAddition, Vector3DI& tMultiplierInteger, Vector3DI& tSineAmplitude, int& tSinePeriod, int& tInvertAll, int& tColorInteger) {
	evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mTime, tPlayer, &tTime, 1);
	evaluateDreamAssignmentAndReturnAsThreeIntegersWithDefaultValues(&e->mAdd, tPlayer, &tAddition.x, &tAddition.y, &tAddition.z, 0, 0, 0);
	evaluateDreamAssignmentAndReturnAsThreeIntegersWithDefaultValues(&e->mMul, tPlayer, &tMultiplierInteger.x, &tMultiplierInteger.y, &tMultiplierInteger.z, 256, 256, 256);
	evaluateDreamAssignmentAndReturnAsFourIntegersWithDefaultValues(&e->mSinAdd, tPlayer, &tSineAmplitude.x, &tSineAmplitude.y, &tSineAmplitude.z, &tSinePeriod, 0, 0, 0, 1);
	evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mInvertAll, tPlayer, &tInvertAll, 0);
	evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mColor, tPlayer, &tColorInteger, 256);
}

static int handleAllPalFXController(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	PalFXController* e = (PalFXController*)tController->mData;
	int time;
	Vector3DI addition, multiplierInteger, sineAmplitude;
	int sinePeriod, invertAll, colorInteger;
	parsePalFXControllerGeneral(e, tPlayer, time, addition, multiplierInteger, sineAmplitude, sinePeriod, invertAll, colorInteger);
	setPlayerPaletteEffect(getRootPlayer(0), time, addition / 256.0, multiplierInteger / 256.0, sineAmplitude / 256.0, sinePeriod, invertAll, colorInteger / 256.0, 1);
	setPlayerPaletteEffect(getRootPlayer(1), time, addition / 256.0, multiplierInteger / 256.0, sineAmplitude / 256.0, sinePeriod, invertAll, colorInteger / 256.0, 1);
	setDreamStagePaletteEffects(time, addition / 256.0, multiplierInteger / 256.0, sineAmplitude / 256.0, sinePeriod, invertAll, colorInteger / 256.0);
	setDreamBarPaletteEffects(time, addition / 256.0, multiplierInteger / 256.0, sineAmplitude / 256.0, sinePeriod, invertAll, colorInteger / 256.0);
	return 0;
}

static int handlePalFXController(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	PalFXController* e = (PalFXController*)tController->mData;
	int time;
	Vector3DI addition, multiplierInteger, sineAmplitude;
	int sinePeriod, invertAll, colorInteger;
	parsePalFXControllerGeneral(e, tPlayer, time, addition, multiplierInteger, sineAmplitude, sinePeriod, invertAll, colorInteger);
	setPlayerPaletteEffect(tPlayer, time, addition / 256.0, multiplierInteger / 256.0, sineAmplitude / 256.0, sinePeriod, invertAll, colorInteger / 256.0, 0);
	return 0;
}

static int handleBGPalFXController(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	PalFXController* e = (PalFXController*)tController->mData;
	int time;
	Vector3DI addition, multiplierInteger, sineAmplitude;
	int sinePeriod, invertAll, colorInteger;
	parsePalFXControllerGeneral(e, tPlayer, time, addition, multiplierInteger, sineAmplitude, sinePeriod, invertAll, colorInteger);
	setDreamStagePaletteEffects(time, addition / 256.0, multiplierInteger / 256.0, sineAmplitude / 256.0, sinePeriod, invertAll, colorInteger / 256.0);
	return 0;
}

static int handleClearClipboardController() {
	clearClipboard();
	return 0;
}

static int handleDisplayToClipboardController(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	clearClipboard();
	handleAppendToClipboardController(tController, tPlayer);
	
	return 0;
}

static int handleEnvironmentColorController(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	EnvironmentColorController* e = (EnvironmentColorController*)tController->mData;

	int time;
	int isUnderCharacters;
	Vector3DI colors = evaluateDreamAssignmentAndReturnAsVector3DI(&e->mValue, tPlayer);
	evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mTime, tPlayer, &time, 1);
	evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mUnder, tPlayer, &isUnderCharacters, 1);
	
	setEnvironmentColor(colors, time, isUnderCharacters);
	return 0;
}

static int handleEnvironmentShakeController(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	EnvironmentShakeController* e = (EnvironmentShakeController*)tController->mData;

	int time, ampl;
	double freq, phase;
	evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mTime, tPlayer, &time, 1);
	evaluateDreamAssignmentAndReturnAsOneFloatWithDefaultValue(&e->mFrequency, tPlayer, &freq, 60);
	evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mAmplitude, tPlayer, &ampl, (int)transformDreamCoordinates(-4.0, 320, getDreamStageCoordinateP()));
	evaluateDreamAssignmentAndReturnAsOneFloatWithDefaultValue(&e->mPhaseOffset, tPlayer, &phase, freq >= 90.0 ? 90.0 : 0.0);

	setEnvironmentShake(time, freq, ampl, phase, getActiveStateMachineCoordinateP());
	return 0;
}

static int handleExplodBindTimeController(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	ExplodBindTimeController* e = (ExplodBindTimeController*)tController->mData;
	
	int id, time;
	evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mID, tPlayer, &id, -1);
	evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mTime, tPlayer, &time, 1);

	updateExplodBindTime(tPlayer, id, time);

	return 0;
}

static int handleForceFeedbackController(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	ForceFeedbackController* e = (ForceFeedbackController*)tController->mData;

	int time;
	evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mTime, tPlayer, &time, 1);
	int freq1, freq2;
	evaluateDreamAssignmentAndReturnAsTwoIntegersWithDefaultValues(&e->mFrequency, tPlayer, &freq1, &freq2, 128, 0);
	int ampl1, ampl2;
	evaluateDreamAssignmentAndReturnAsTwoIntegersWithDefaultValues(&e->mAmplitude, tPlayer, &ampl1, &ampl2, 128, 0);
	int self;
	evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mSelf, tPlayer, &self, 1);

	int i = self ? tPlayer->mRootID : getPlayerOtherPlayer(tPlayer)->mRootID;
	addControllerRumbleSingle(i, time, freq1, ampl1 / 255.0);

	return 0;
}

static int handleGameMakeAnimController(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	GameMakeAnimController* e = (GameMakeAnimController*)tController->mData;
	int animationNumber;
	evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mValue, tPlayer, &animationNumber, 0);

	int isUnderPlayer;
	evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mIsUnderPlayer, tPlayer, &isUnderPlayer, 0);

	Position2D pos;
	evaluateDreamAssignmentAndReturnAsTwoFloatsWithDefaultValues(&e->mPosOffset, tPlayer, &pos.x, &pos.y, 0, 0);

	int random;
	evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mRandomOffset, tPlayer, &random, 0);

	pos = pos + Vector2D(randfrom(-random / 2.0, random / 2.0), randfrom(-random / 2.0, random / 2.0));
	pos = pos + getPlayerPosition(tPlayer, getDreamMugenStageHandlerCameraCoordinateP());
	pos = pos + getDreamStageCoordinateSystemOffset(getDreamMugenStageHandlerCameraCoordinateP());

	const auto coordinateDrawScale = getDreamMugenStageHandlerCameraCoordinateP() / double(getActiveStateMachineCoordinateP());
	auto element = addMugenAnimation(getDreamFightEffectAnimation(animationNumber), getDreamFightEffectSprites(), pos.xyz(isUnderPlayer ? GAME_MAKE_ANIM_UNDER_Z : GAME_MAKE_ANIM_OVER_Z));
	setMugenAnimationCameraPositionReference(element, getDreamMugenStageHandlerCameraPositionReference());
	setMugenAnimationNoLoop(element);
	setMugenAnimationBaseDrawScale(element, (getScreenSize().y / double(getDreamUICoordinateP())) * getDreamUIFightFXScale() * coordinateDrawScale);
	setMugenAnimationCameraEffectPositionReference(element, getDreamMugenStageHandlerCameraEffectPositionReference());
	setMugenAnimationCameraScaleReference(element, getDreamMugenStageHandlerCameraZoomReference());

	return 0;
}

static int handleHitAddController(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	SingleRequiredValueController* e = (SingleRequiredValueController*)tController->mData;

	int value;
	evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mValue, tPlayer, &value, 0);
	increasePlayerComboCounter(tPlayer, value);

	return 0;
}

static void parseSingleHitOverrideFlag2(const char* tFlag2, MugenAttackClassFlags* tAttackClassFlags, MugenAttackType* tAttackType) {
	if (tFlag2[0] == 'a') *tAttackClassFlags = MUGEN_ATTACK_CLASS_ALL_FLAG;
	else if (tFlag2[0] == 'n') *tAttackClassFlags = MUGEN_ATTACK_CLASS_NORMAL_FLAG;
	else if (tFlag2[0] == 's') *tAttackClassFlags = MUGEN_ATTACK_CLASS_SPECIAL_FLAG;
	else if (tFlag2[0] == 'h') *tAttackClassFlags = MUGEN_ATTACK_CLASS_HYPER_FLAG;
	else {
		logWarning("Unable to parse hitoverride attr 2.");
		logWarningString(tFlag2);
		*tAttackClassFlags = MUGEN_ATTACK_CLASS_NORMAL_FLAG;
	}

	if (tFlag2[1] == 'a') *tAttackType = MUGEN_ATTACK_TYPE_ATTACK;
	else if (tFlag2[1] == 't') *tAttackType = MUGEN_ATTACK_TYPE_THROW;
	else if (tFlag2[1] == 'p')  *tAttackType = MUGEN_ATTACK_TYPE_PROJECTILE;
	else {
		logWarning("Unable to parse hitoverride attr 2.");
		logWarningString(arg2);
		*tAttackType = MUGEN_ATTACK_TYPE_ATTACK;
	}
}

static void handleHitOverrideAttributeString(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, uint32_t* tStateTypeFlags, std::vector<std::pair<MugenAttackClassFlags, MugenAttackType>>& tAttackClassTypePairs) {
	std::string attrString;
	evaluateDreamAssignmentAndReturnAsString(attrString, tAssignment, tPlayer);
	auto commaPos = attrString.find(',');
	const auto flag1 = attrString.substr(0, commaPos);
	*tStateTypeFlags = MUGEN_STATE_TYPE_NO_FLAG;
	if (flag1.find('s') != flag1.npos) *tStateTypeFlags |= int(MUGEN_STATE_TYPE_STANDING_FLAG);
	if (flag1.find('c') != flag1.npos) *tStateTypeFlags |= int(MUGEN_STATE_TYPE_CROUCHING_FLAG);
	if (flag1.find('a') != flag1.npos) *tStateTypeFlags |= int(MUGEN_STATE_TYPE_AIR_FLAG);

	while (commaPos != std::string::npos) {
		commaPos++;
		const auto endPos = attrString.find(',', commaPos);
		auto flag2 = attrString.substr(commaPos, (endPos - commaPos - 1));
		turnStringLowercase(flag2);
		flag2 = copyOverCleanHitDefAttributeFlag(flag2.c_str());
		if (flag2.size() != 2) {
			logWarningFormat("Invalid HitOverride flag2 %s. Ignoring.", flag2.c_str());
		}
		else {
			MugenAttackClassFlags attackClassFlags;
			MugenAttackType attackType;
			parseSingleHitOverrideFlag2(flag2.c_str(), &attackClassFlags, &attackType);
			tAttackClassTypePairs.push_back(std::make_pair(attackClassFlags, attackType));
		}
		commaPos = endPos;
	}
}


static int handleHitOverrideController(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	HitOverrideController* e = (HitOverrideController*)tController->mData;
	uint32_t stateTypeFlags;
	std::vector<std::pair<MugenAttackClassFlags, MugenAttackType>> attackClassTypePairs;
	handleHitOverrideAttributeString(&e->mAttributeString, tPlayer, &stateTypeFlags, attackClassTypePairs);

	int stateno, slot, time, forceAir;
	evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mStateNo, tPlayer, &stateno, 0);
	evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mSlot, tPlayer, &slot, 0);
	evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mTime, tPlayer, &time, 1);
	evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mForceAir, tPlayer, &forceAir, 0);

	setPlayerHitOverride(tPlayer, DreamMugenStateTypeFlags(stateTypeFlags), attackClassTypePairs, stateno, slot, time, forceAir);

	return 0;
}

static int handleSetLifeController(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	SingleRequiredValueController* e = (SingleRequiredValueController*)tController->mData;

	int value;
	evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mValue, tPlayer, &value, 0);

	setPlayerLife(tPlayer, tPlayer, value);

	return 0;
}

static int handleDrawOffsetController(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	Set2DPhysicsController* e = (Set2DPhysicsController*)tController->mData;

	if (e->mIsSettingX) {
		double x = evaluateDreamAssignmentAndReturnAsFloat(&e->x, tPlayer);
		setPlayerDrawOffsetX(tPlayer, x, getActiveStateMachineCoordinateP());
	}

	if (e->mIsSettingY) {
		double y = evaluateDreamAssignmentAndReturnAsFloat(&e->y, tPlayer);
		setPlayerDrawOffsetY(tPlayer, y, getActiveStateMachineCoordinateP());
	}

	return 0;
}

static int handleRemapPaletteController(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	RemapPaletteController* e = (RemapPaletteController*)tController->mData;
	Vector2DI source, destination;
	evaluateDreamAssignmentAndReturnAsTwoIntegersWithDefaultValues(&e->mSource, tPlayer, &source.x, &source.y, 1, 1);
	evaluateDreamAssignmentAndReturnAsTwoIntegersWithDefaultValues(&e->mDestination, tPlayer, &destination.x, &destination.y, 1, 1);
	remapPlayerPalette(tPlayer, source, destination);
	return 0;
}

static int handleSoundPanController(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	SoundPanController* e = (SoundPanController*)tController->mData;

	int channel;
	evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mChannel, tPlayer, &channel, -1);
	channel = parsePlayerSoundEffectChannel(channel, tPlayer);
	const auto panning = handlePanningValue(&e->mPan, tPlayer, e->mIsAbspan);
	panSoundEffect(channel, panning);

	return 0;
}

static int handleStopSoundController(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	SoundStopController* e = (SoundStopController*)tController->mData;

	int channel;
	evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mChannel, tPlayer, &channel, -1);
	channel = parsePlayerSoundEffectChannel(channel, tPlayer);

	if (channel == -1) {
		stopAllSoundEffects();
	}
	else {
		stopSoundEffect(channel);
	}

	return 0;
}

static int handleTargetDropController(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	TargetDropController* e = (TargetDropController*)tController->mData;
	int excludeID, isKeepingOneAtMost;
	evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mExcludeID, tPlayer, &excludeID, -1);
	evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mKeepOne, tPlayer, &isKeepingOneAtMost, 1);

	dropPlayerTargets(tPlayer, excludeID, isKeepingOneAtMost);

	return 0;
}

static int handleVictoryQuoteController(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	VictoryQuoteController* e = (VictoryQuoteController*)tController->mData;
	int index;
	evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mValue, tPlayer, &index, -1);
	setPlayerVictoryQuoteIndex(tPlayer, index);
	return 0;
}


static BlendType handleTransparencyType(DreamMugenAssignment** tType, DreamPlayer* tPlayer, int* tAlphaDefaultSrc, int* tAlphaDefaultDst) {
	string text;
	evaluateDreamAssignmentAndReturnAsString(text, tType, tPlayer);

	BlendType ret;
	if (("default" == text) || "none" == text) {
		ret = BLEND_TYPE_NORMAL;
		*tAlphaDefaultSrc = 256;
		*tAlphaDefaultDst = 256;
	}
	else if ("add" == text) {
		ret = BLEND_TYPE_ADDITION;
		*tAlphaDefaultSrc = 256;
		*tAlphaDefaultDst = 256;
	}
	else if ("addalpha" == text) {
		ret = BLEND_TYPE_ADDITION;
		*tAlphaDefaultSrc = 256;
		*tAlphaDefaultDst = 0;
	}
	else if ("add1" == text) {
		ret = BLEND_TYPE_ADDITION;
		*tAlphaDefaultSrc = 256;
		*tAlphaDefaultDst = 128;
	}
	else  if ("sub" == text) {
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

	return ret;
}

static int handleTransparencyController(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	TransparencyController* e = (TransparencyController*)tController->mData;
	
	int alphaDefaultSrc, alphaDefaultDst;
	BlendType type = handleTransparencyType(&e->mTransparency, tPlayer, &alphaDefaultSrc, &alphaDefaultDst);

	int alphaSource, alphaDest;
	evaluateDreamAssignmentAndReturnAsTwoIntegersWithDefaultValues(&e->mAlpha, tPlayer, &alphaSource, &alphaDest, alphaDefaultSrc, alphaDefaultDst);

	setPlayerOneFrameTransparency(tPlayer, type, alphaSource, alphaDest);

	return 0;
}

static int handleRandomVariableController(DreamMugenStateController* tController, DreamPlayer* tPlayer) {
	VarRandomController* e = (VarRandomController*)tController->mData;
	
	int index;
	evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mValue, tPlayer, &index, 0);

	string rangeText;
	evaluateDreamAssignmentAndReturnAsString(rangeText, &e->mRange, tPlayer);
	char comma[10];
	int val1, val2;
	int items = sscanf(rangeText.data(), "%d %s %d", &val1, comma, &val2);

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
int allPalFXHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleAllPalFXController(tController, tPlayer); }
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
int changeStateStoryHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer);
int changeTextStoryHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer);
int clearClipboardHandleFunction(DreamMugenStateController* /*tController*/, DreamPlayer* /*tPlayer*/) { return handleClearClipboardController(); }
int createHelperStoryHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer);
int createTextStoryHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer);
int ctrlSetHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleControlSetting(tController, tPlayer); }
int defenceMulSetHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleDefenseMultiplier(tController, tPlayer); }
int destroySelfHandleFunction(DreamMugenStateController* /*tController*/, DreamPlayer* tPlayer) { return handleDestroySelf(tPlayer); }
int displayToClipboardHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleDisplayToClipboardController(tController, tPlayer); }
int envColorHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleEnvironmentColorController(tController, tPlayer); }
int envShakeHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleEnvironmentShakeController(tController, tPlayer); }
int explodHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleExplod(tController, tPlayer); }
int explodBindTimeHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleExplodBindTimeController(tController, tPlayer); }
int forceFeedbackHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleForceFeedbackController(tController, tPlayer); }
int fallEnvShakeHandleFunction(DreamMugenStateController* /*tController*/, DreamPlayer* tPlayer) { return handleFallEnvironmentShake(tPlayer); }
int gameMakeAnimHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleGameMakeAnimController(tController, tPlayer); }
int gravityHandleFunction(DreamMugenStateController* /*tController*/, DreamPlayer* tPlayer) { return handleGravity(tPlayer); }
int helperHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleHelper(tController, tPlayer); }
int hitAddHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleHitAddController(tController, tPlayer); }
int hitByHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleHitBy(tController, tPlayer); }
int hitDefHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleHitDefinition(tController, tPlayer); }
int hitFallDamageHandleFunction(DreamMugenStateController* /*tController*/, DreamPlayer* tPlayer) { return handleHitFallDamage(tPlayer); }
int hitFallSetHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleHitFallSet(tController, tPlayer); }
int hitFallVelHandleFunction(DreamMugenStateController* /*tController*/, DreamPlayer* tPlayer) { return handleHitFallVelocity(tPlayer); }
int hitOverrideHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleHitOverrideController(tController, tPlayer); }
int hitVelSetHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleHitVelocitySetting(tController, tPlayer); }
int lifeAddHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleAddingLife(tController, tPlayer); }
int lifeSetHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleSetLifeController(tController, tPlayer); }
int makeDustHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleMakeDust(tController, tPlayer); }
int modifyExplodHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleModifyExplod(tController, tPlayer); }
int moveHitResetHandleFunction(DreamMugenStateController* /*tController*/, DreamPlayer* tPlayer) { return handleMoveHitReset(tPlayer); }
int notHitByHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleNotHitBy(tController, tPlayer); }
int nullHandleFunction(DreamMugenStateController* /*tController*/, DreamPlayer* /*tPlayer*/) { return handleNull(); }
int offsetHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleDrawOffsetController(tController, tPlayer); }
int palFXHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handlePalFXController(tController, tPlayer); }
int parentVarAddHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleAddingVariable(tController, tPlayer, getPlayerParent(tPlayer)); }
int parentVarSetHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleSettingVariable(tController, tPlayer, getPlayerParent(tPlayer)); }
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
int removeTextStoryHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer);
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
int textSetPositionStoryHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer);
int textAddPositionStoryHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer);
int transHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleTransparencyController(tController, tPlayer); }
int turnHandleFunction(DreamMugenStateController* /*tController*/, DreamPlayer* tPlayer) { return handleTurnController(tPlayer); }
int varAddHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleAddingVariable(tController, tPlayer, tPlayer); }
int varRandomHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleRandomVariableController(tController, tPlayer); }
int varRangeSetHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleSettingVariableRange(tController, tPlayer); }
int varSetHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleSettingVariable(tController, tPlayer, tPlayer); }
int globalVarSetHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleSettingGlobalVariable(tController, tPlayer); }
int globalVarAddHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleAddingGlobalVariable(tController, tPlayer); }
int velAddHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleVelocityAddition(tController, tPlayer); }
int velMulHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleVelocityMultiplication(tController, tPlayer); }
int velSetHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleVelocitySetting(tController, tPlayer); }
int victoryQuoteHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleVictoryQuoteController(tController, tPlayer); }
int widthHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleWidth(tController, tPlayer); }
int zoomHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleZoom(tController, tPlayer); }

static void setupStateControllerHandlers() {
	gMugenStateControllerVariableHandler.mStateControllerHandlers.clear();

	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_AFTER_IMAGE] = afterImageHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_AFTER_IMAGE_TIME] = afterImageTimeHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_PALETTE_EFFECT_ALL] = allPalFXHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_ADD_ANGLE] = angleAddHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_DRAW_ANGLE] = angleDrawHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_MUL_ANGLE] = angleMulHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_SET_ANGLE] = angleSetHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_APPEND_TO_CLIPBOARD] = appendToClipboardHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_ASSERT_SPECIAL] = assertSpecialHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_SET_ATTACK_DISTANCE] = attackDistHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_SET_ATTACK_MULTIPLIER] = attackMulSetHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_PALETTE_EFFECT_BACKGROUND] = bgPalFXHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_BIND_TO_PARENT] = bindToParentHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_BIND_TO_ROOT] = bindToRootHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_BIND_TO_TARGET] = bindToTargetHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_CHANGE_ANIMATION] = changeAnimHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_CHANGE_ANIMATION_2] = changeAnim2HandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_CHANGE_STATE] = changeStateHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_CHANGE_STORY_STATE] = changeStateStoryHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_CHANGE_TEXT] = changeTextStoryHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_CLEAR_CLIPBOARD] = clearClipboardHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_CREATE_STORY_HELPER] = createHelperStoryHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_CREATE_TEXT] = createTextStoryHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_SET_CONTROL] = ctrlSetHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_SET_DEFENSE_MULTIPLIER] = defenceMulSetHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_DESTROY_SELF] = destroySelfHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_DISPLAY_TO_CLIPBOARD] = displayToClipboardHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_ENVIRONMENT_COLOR] = envColorHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_ENVIRONMENT_SHAKE] = envShakeHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_EXPLOD] = explodHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_EXPLOD_BIND_TIME] = explodBindTimeHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_FORCE_FEEDBACK] = forceFeedbackHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_FALL_ENVIRONMENT_SHAKE] = fallEnvShakeHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_MAKE_GAME_ANIMATION] = gameMakeAnimHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_GRAVITY] = gravityHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_HELPER] = helperHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_ADD_HIT] = hitAddHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_HIT_BY] = hitByHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_HIT_DEFINITION] = hitDefHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_HIT_FALL_DAMAGE] = hitFallDamageHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_SET_HIT_FALL] = hitFallSetHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_HIT_FALL_VELOCITY] = hitFallVelHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_HIT_OVERRIDE] = hitOverrideHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_SET_HIT_VELOCITY] = hitVelSetHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_ADD_LIFE] = lifeAddHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_SET_LIFE] = lifeSetHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_MAKE_DUST] = makeDustHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_MODIFY_EXPLOD] = modifyExplodHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_RESET_MOVE_HIT] = moveHitResetHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_NOT_HIT_BY] = notHitByHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_NULL] = nullHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_SET_OFFSET] = offsetHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_PALETTE_EFFECT] = palFXHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_PARENT_ADD_VARIABLE] = parentVarAddHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_SET_PARENT_VARIABLE] = parentVarSetHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_PAUSE] = pauseHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_PLAYER_PUSH] = playerPushHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_PLAY_SOUND] = playSndHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_ADD_POSITION] = posAddHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_FREEZE_POSITION] = posFreezeHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_SET_POSITION] = posSetHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_ADD_POWER] = powerAddHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_SET_POWER] = powerSetHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_PROJECTILE] = projectileHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_REMAP_PALETTE] = remapPalHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_REMOVE_EXPLOD] = removeExplodHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_REMOVE_TEXT] = removeTextStoryHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_REVERSAL_DEFINITION] = reversalDefHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_SCREEN_BOUND] = screenBoundHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_SET_SELF_STATE] = selfStateHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_SPRITE_PRIORITY] = sprPriorityHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_SET_STATE_TYPE] = stateTypeSetHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_PAN_SOUND] = sndPanHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_STOP_SOUND] = stopSndHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_SUPER_PAUSE] = superPauseHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_BIND_TARGET] = targetBindHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_DROP_TARGET] = targetDropHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_SET_TARGET_FACING] = targetFacingHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_ADD_TARGET_LIFE] = targetLifeAddHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_ADD_TARGET_POWER] = targetPowerAddHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_SET_TARGET_STATE] = targetStateHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_TARGET_ADD_VELOCITY] = targetVelAddHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_TARGET_SET_VELOCITY] = targetVelSetHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_TEXT_ADD_POSITION] = textAddPositionStoryHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_TEXT_SET_POSITION] = textSetPositionStoryHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_TRANSPARENCY] = transHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_TURN] = turnHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_ADD_VARIABLE] = varAddHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_SET_VARIABLE_RANDOM] = varRandomHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_SET_VARIABLE_RANGE] = varRangeSetHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_SET_VARIABLE] = varSetHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_GLOBAL_VAR_SET] = globalVarSetHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_GLOBAL_VAR_ADD] = globalVarAddHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_ADD_VELOCITY] = velAddHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_MULTIPLY_VELOCITY] = velMulHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_SET_VELOCITY] = velSetHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_VICTORY_QUOTE] = victoryQuoteHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_WIDTH] = widthHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STATE_CONTROLLER_TYPE_ZOOM] = zoomHandleFunction;
}

static void parseCreateHelperStoryController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup, DreamMugenStateControllerType tType);
static void parseCreateTextStoryController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup, DreamMugenStateControllerType tType);
static void parseRemoveElementStoryController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup, DreamMugenStateControllerType tType);
static void parseChangeTextStoryController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup, DreamMugenStateControllerType tType);
static void parseStoryTarget2DPhysicsController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup, DreamMugenStateControllerType tType);

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
void changeStoryStateParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseSingleRequiredValueController(tController, tGroup, MUGEN_STATE_CONTROLLER_TYPE_CHANGE_STORY_STATE); }
void changeTextParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseChangeTextStoryController(tController, tGroup, MUGEN_STATE_CONTROLLER_TYPE_CHANGE_TEXT); }
void clearClipboardParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* /*tGroup*/) { parseNullController(tController, MUGEN_STATE_CONTROLLER_TYPE_CLEAR_CLIPBOARD); }
void createHelperParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseCreateHelperStoryController(tController, tGroup, MUGEN_STATE_CONTROLLER_TYPE_CREATE_STORY_HELPER); }
void createTextParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseCreateTextStoryController(tController, tGroup, MUGEN_STATE_CONTROLLER_TYPE_CREATE_TEXT); }
void ctrlSetParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseControlSettingController(tController, tGroup); }
void defenceMulSetParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseDefenseMultiplierController(tController, tGroup); }
void destroySelfParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseDestroySelfController(tController, tGroup); }
void displayToClipboardParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseClipboardController(tController, tGroup, MUGEN_STATE_CONTROLLER_TYPE_DISPLAY_TO_CLIPBOARD); }
void envColorParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseEnvColorController(tController, tGroup); }
void envShakeParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseEnvironmentShakeController(tController, tGroup); }
void explodParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseExplodController(tController, tGroup); }
void explodBindTimeParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseExplodBindTimeController(tController, tGroup); }
void forceFeedbackParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseForceFeedbackController(tController, tGroup); }
void fallEnvShakeParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* /*tGroup*/) { parseNullController(tController, MUGEN_STATE_CONTROLLER_TYPE_FALL_ENVIRONMENT_SHAKE); }
void gameMakeAnimParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseGameMakeAnimController(tController, tGroup); }
void gravityParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* /*tGroup*/) { parseNullController(tController, MUGEN_STATE_CONTROLLER_TYPE_GRAVITY); }
void helperParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseHelperController(tController, tGroup); }
void hitAddParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseSingleRequiredValueController(tController, tGroup, MUGEN_STATE_CONTROLLER_TYPE_ADD_HIT); }
void hitByParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseNotHitByController(tController, tGroup, MUGEN_STATE_CONTROLLER_TYPE_HIT_BY); }
void hitDefParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseHitDefinitionController(tController, tGroup); }
void hitFallDamageParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* /*tGroup*/) { parseNullController(tController, MUGEN_STATE_CONTROLLER_TYPE_HIT_FALL_DAMAGE); }
void hitFallSetParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseHitFallSetController(tController, tGroup); }
void hitFallVelParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* /*tGroup*/) { parseNullController(tController, MUGEN_STATE_CONTROLLER_TYPE_HIT_FALL_VELOCITY); }
void hitOverrideParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseHitOverrideController(tController, tGroup); }
void hitVelSetParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parse2DPhysicsController(tController, tGroup, MUGEN_STATE_CONTROLLER_TYPE_SET_HIT_VELOCITY); }
void lifeAddParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseLifeAddController(tController, tGroup); }
void lifeSetParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseSingleRequiredValueController(tController, tGroup, MUGEN_STATE_CONTROLLER_TYPE_SET_LIFE); }
void makeDustParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseMakeDustController(tController, tGroup); }
void modifyExplodParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseModifyExplodController(tController, tGroup); }
void moveHitResetParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* /*tGroup*/) { parseNullController(tController, MUGEN_STATE_CONTROLLER_TYPE_RESET_MOVE_HIT); }
void notHitByParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseNotHitByController(tController, tGroup, MUGEN_STATE_CONTROLLER_TYPE_NOT_HIT_BY); }
void nullParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* /*tGroup*/) { parseNullController(tController, MUGEN_STATE_CONTROLLER_TYPE_NULL); }
void offsetParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parse2DPhysicsController(tController, tGroup, MUGEN_STATE_CONTROLLER_TYPE_SET_OFFSET); }
void palFXParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parsePalFXController(tController, tGroup, MUGEN_STATE_CONTROLLER_TYPE_PALETTE_EFFECT); }
void parentVarAddParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseVarSetController(tController, tGroup, MUGEN_STATE_CONTROLLER_TYPE_PARENT_ADD_VARIABLE); }
void parentVarSetParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseVarSetController(tController, tGroup, MUGEN_STATE_CONTROLLER_TYPE_SET_PARENT_VARIABLE); }
void pauseParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parsePauseController(tController, tGroup); }
void playerPushParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseSingleRequiredValueController(tController, tGroup, MUGEN_STATE_CONTROLLER_TYPE_PLAYER_PUSH); }
void playSndParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parsePlaySoundController(tController, tGroup, MUGEN_STATE_CONTROLLER_TYPE_PLAY_SOUND); }
void posAddParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parse2DPhysicsController(tController, tGroup, MUGEN_STATE_CONTROLLER_TYPE_ADD_POSITION); }
void posFreezeParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parsePositionFreezeController(tController, tGroup); }
void posSetParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parse2DPhysicsController(tController, tGroup, MUGEN_STATE_CONTROLLER_TYPE_SET_POSITION); }
void powerAddParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseSingleRequiredValueController(tController, tGroup, MUGEN_STATE_CONTROLLER_TYPE_ADD_POWER); }
void powerSetParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseSingleRequiredValueController(tController, tGroup, MUGEN_STATE_CONTROLLER_TYPE_SET_POWER); }
void projectileParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseProjectileController(tController, tGroup); }
void remapPalParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseRemapPaletteController(tController, tGroup); }
void removeExplodParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseRemoveExplodController(tController, tGroup); }
void removeTextParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseRemoveElementStoryController(tController, tGroup, MUGEN_STATE_CONTROLLER_TYPE_REMOVE_TEXT); }
void reversalDefParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseReversalDefinitionController(tController, tGroup); }
void screenBoundParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseScreenBoundController(tController, tGroup); }
void selfStateParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseChangeStateController(tController, tGroup, MUGEN_STATE_CONTROLLER_TYPE_SET_SELF_STATE); }
void sprPriorityParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseSpritePriorityController(tController, tGroup); }
void stateTypeSetParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseStateTypeSetController(tController, tGroup); }
void sndPanParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseSoundPanController(tController, tGroup, MUGEN_STATE_CONTROLLER_TYPE_PAN_SOUND); }
void stopSndParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseStopSoundController(tController, tGroup, MUGEN_STATE_CONTROLLER_TYPE_STOP_SOUND); }
void superPauseParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseSuperPauseController(tController, tGroup); }
void targetBindParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseBindController(tController, tGroup, MUGEN_STATE_CONTROLLER_TYPE_BIND_TARGET); }
void targetDropParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseTargetDropController(tController, tGroup); }
void targetFacingParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseSetTargetFacingController(tController, tGroup); }
void targetLifeAddParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseTargetLifeAddController(tController, tGroup); }
void targetPowerAddParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseTargetPowerAddController(tController, tGroup); }
void targetStateParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseTargetChangeStateController(tController, tGroup); }
void targetVelAddParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseTarget2DPhysicsController(tController, tGroup, MUGEN_STATE_CONTROLLER_TYPE_TARGET_ADD_VELOCITY); }
void targetVelSetParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseTarget2DPhysicsController(tController, tGroup, MUGEN_STATE_CONTROLLER_TYPE_TARGET_SET_VELOCITY); }
void textPositionAddParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseStoryTarget2DPhysicsController(tController, tGroup, MUGEN_STATE_CONTROLLER_TYPE_TEXT_ADD_POSITION); }
void textPositionSetParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseStoryTarget2DPhysicsController(tController, tGroup, MUGEN_STATE_CONTROLLER_TYPE_TEXT_SET_POSITION); }
void transParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseTransparencyController(tController, tGroup); }
void turnParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* /*tGroup*/) { parseNullController(tController, MUGEN_STATE_CONTROLLER_TYPE_TURN); }
void varAddParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseVarSetController(tController, tGroup, MUGEN_STATE_CONTROLLER_TYPE_ADD_VARIABLE); }
void varRandomParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseVarRandomController(tController, tGroup); }
void varRangeSetParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseVarRangeSetController(tController, tGroup); }
void varSetParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseVarSetController(tController, tGroup, MUGEN_STATE_CONTROLLER_TYPE_SET_VARIABLE); }
void globalVarSetParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseVarSetController(tController, tGroup, MUGEN_STATE_CONTROLLER_TYPE_GLOBAL_VAR_SET); }
void globalVarAddParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseVarSetController(tController, tGroup, MUGEN_STATE_CONTROLLER_TYPE_GLOBAL_VAR_ADD); }
void velAddParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parse2DPhysicsController(tController, tGroup, MUGEN_STATE_CONTROLLER_TYPE_ADD_VELOCITY); }
void velMulParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parse2DPhysicsController(tController, tGroup, MUGEN_STATE_CONTROLLER_TYPE_MULTIPLY_VELOCITY); }
void velSetParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parse2DPhysicsController(tController, tGroup, MUGEN_STATE_CONTROLLER_TYPE_SET_VELOCITY); }
void victoryQuoteParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseVictoryQuoteController(tController, tGroup); }
void widthParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseWidthController(tController, tGroup); }
void zoomParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseZoomController(tController, tGroup); }

static void setupStateControllerParsers() {
	gMugenStateControllerVariableHandler.mStateControllerParsers.clear();

	gMugenStateControllerVariableHandler.mStateControllerParsers["afterimage"] = afterImageParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["afterimagetime"] = afterImageTimeParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["allpalfx"] = allPalFXParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["angleadd"] = angleAddParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["angledraw"] = angleDrawParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["anglemul"] = angleMulParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["angleset"] = angleSetParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["appendtoclipboard"] = appendToClipboardParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["assertspecial"] = assertSpecialParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["attackdist"] = attackDistParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["attackmulset"] = attackMulSetParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["bgpalfx"] = bgPalFXParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["bindtoparent"] = bindToParentParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["bindtoroot"] = bindToRootParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["bindtotarget"] = bindToTargetParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["changeanim"] = changeAnimParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["changeanim2"] = changeAnim2ParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["changestate"] = changeStateParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["changestorystate"] = changeStoryStateParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["changetext"] = changeTextParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["clearclipboard"] = clearClipboardParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["createstoryhelper"] = createHelperParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["createtext"] = createTextParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["ctrlset"] = ctrlSetParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["defencemulset"] = defenceMulSetParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["destroyself"] = destroySelfParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["displaytoclipboard"] = displayToClipboardParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["envcolor"] = envColorParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["envshake"] = envShakeParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["explod"] = explodParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["explodbindtime"] = explodBindTimeParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["forcefeedback"] = forceFeedbackParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["fallenvshake"] = fallEnvShakeParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["gamemakeanim"] = gameMakeAnimParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["gravity"] = gravityParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["helper"] = helperParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["hitadd"] = hitAddParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["hitby"] = hitByParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["hitdef"] = hitDefParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["hitfalldamage"] = hitFallDamageParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["hitfallset"] = hitFallSetParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["hitfallvel"] = hitFallVelParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["hitoverride"] = hitOverrideParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["hitvelset"] = hitVelSetParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["lifeadd"] = lifeAddParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["lifeset"] = lifeSetParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["makedust"] = makeDustParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["modifyexplod"] = modifyExplodParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["movehitreset"] = moveHitResetParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["nothitby"] = notHitByParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["null"] = nullParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["offset"] = offsetParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["palfx"] = palFXParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["parentvaradd"] = parentVarAddParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["parentvarset"] = parentVarSetParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["pause"] = pauseParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["playerpush"] = playerPushParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["playsnd"] = playSndParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["posadd"] = posAddParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["posfreeze"] = posFreezeParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["posset"] = posSetParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["poweradd"] = powerAddParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["powerset"] = powerSetParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["projectile"] = projectileParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["remappal"] = remapPalParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["removeexplod"] = removeExplodParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["removetext"] = removeTextParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["reversaldef"] = reversalDefParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["screenbound"] = screenBoundParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["selfstate"] = selfStateParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["sprpriority"] = sprPriorityParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["statetypeset"] = stateTypeSetParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["sndpan"] = sndPanParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["stopsnd"] = stopSndParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["superpause"] = superPauseParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["targetbind"] = targetBindParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["targetdrop"] = targetDropParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["targetfacing"] = targetFacingParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["targetlifeadd"] = targetLifeAddParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["targetpoweradd"] = targetPowerAddParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["targetstate"] = targetStateParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["targetveladd"] = targetVelAddParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["targetvelset"] = targetVelSetParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["textposadd"] = textPositionAddParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["textposset"] = textPositionSetParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["trans"] = transParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["turn"] = turnParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["varadd"] = varAddParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["varrandom"] = varRandomParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["varrangeset"] = varRangeSetParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["varset"] = varSetParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["veladd"] = velAddParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["globalvarset"] = globalVarSetParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["globalvaradd"] = globalVarAddParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["velmul"] = velMulParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["velset"] = velSetParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["victoryquote"] = victoryQuoteParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["width"] = widthParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["zoom"] = zoomParseFunction;
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
void zoomUnloadFunction(DreamMugenStateController* tController) { unloadZoomController(tController); }

static void setupStateControllerUnloaders() {
	gMugenStateControllerVariableHandler.mStateControllerUnloaders.clear();

	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_AFTER_IMAGE] = afterImageUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_AFTER_IMAGE_TIME] = afterImageTimeUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_PALETTE_EFFECT_ALL] = allPalFXUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_ADD_ANGLE] = angleAddUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_DRAW_ANGLE] = angleDrawUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_MUL_ANGLE] = angleMulUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_SET_ANGLE] = angleSetUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_APPEND_TO_CLIPBOARD] = appendToClipboardUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_ASSERT_SPECIAL] = assertSpecialUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_SET_ATTACK_DISTANCE] = attackDistUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_SET_ATTACK_MULTIPLIER] = attackMulSetUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_PALETTE_EFFECT_BACKGROUND] = bgPalFXUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_BIND_TO_PARENT] = bindToParentUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_BIND_TO_ROOT] = bindToRootUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_BIND_TO_TARGET] = bindToTargetUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_CHANGE_ANIMATION] = changeAnimUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_CHANGE_ANIMATION_2] = changeAnim2UnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_CHANGE_STATE] = changeStateUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_CLEAR_CLIPBOARD] = clearClipboardUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_SET_CONTROL] = ctrlSetUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_SET_DEFENSE_MULTIPLIER] = defenceMulSetUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_DESTROY_SELF] = destroySelfUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_DISPLAY_TO_CLIPBOARD] = displayToClipboardUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_ENVIRONMENT_COLOR] = envColorUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_ENVIRONMENT_SHAKE] = envShakeUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_EXPLOD] = explodUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_EXPLOD_BIND_TIME] = explodBindTimeUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_FORCE_FEEDBACK] = forceFeedbackUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_FALL_ENVIRONMENT_SHAKE] = fallEnvShakeUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_MAKE_GAME_ANIMATION] = gameMakeAnimUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_GRAVITY] = gravityUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_HELPER] = helperUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_ADD_HIT] = hitAddUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_HIT_BY] = hitByUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_HIT_DEFINITION] = hitDefUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_HIT_FALL_DAMAGE] = hitFallDamageUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_SET_HIT_FALL] = hitFallSetUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_HIT_FALL_VELOCITY] = hitFallVelUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_HIT_OVERRIDE] = hitOverrideUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_SET_HIT_VELOCITY] = hitVelSetUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_ADD_LIFE] = lifeAddUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_SET_LIFE] = lifeSetUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_MAKE_DUST] = makeDustUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_MODIFY_EXPLOD] = modifyExplodUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_RESET_MOVE_HIT] = moveHitResetUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_NOT_HIT_BY] = notHitByUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_NULL] = nullUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_SET_OFFSET] = offsetUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_PALETTE_EFFECT] = palFXUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_PARENT_ADD_VARIABLE] = parentVarAddUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_SET_PARENT_VARIABLE] = parentVarSetUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_PAUSE] = pauseUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_PLAYER_PUSH] = playerPushUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_PLAY_SOUND] = playSndUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_ADD_POSITION] = posAddUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_FREEZE_POSITION] = posFreezeUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_SET_POSITION] = posSetUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_ADD_POWER] = powerAddUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_SET_POWER] = powerSetUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_PROJECTILE] = projectileUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_REMAP_PALETTE] = remapPalUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_REMOVE_EXPLOD] = removeExplodUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_REVERSAL_DEFINITION] = reversalDefUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_SCREEN_BOUND] = screenBoundUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_SET_SELF_STATE] = selfStateUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_SPRITE_PRIORITY] = sprPriorityUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_SET_STATE_TYPE] = stateTypeSetUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_PAN_SOUND] = sndPanUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_STOP_SOUND] = stopSndUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_SUPER_PAUSE] = superPauseUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_BIND_TARGET] = targetBindUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_DROP_TARGET] = targetDropUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_SET_TARGET_FACING] = targetFacingUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_ADD_TARGET_LIFE] = targetLifeAddUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_ADD_TARGET_POWER] = targetPowerAddUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_SET_TARGET_STATE] = targetStateUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_TARGET_ADD_VELOCITY] = targetVelAddUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_TARGET_SET_VELOCITY] = targetVelSetUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_TRANSPARENCY] = transUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_TURN] = turnUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_ADD_VARIABLE] = varAddUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_SET_VARIABLE_RANDOM] = varRandomUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_SET_VARIABLE_RANGE] = varRangeSetUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_SET_VARIABLE] = varSetUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_ADD_VELOCITY] = velAddUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_MULTIPLY_VELOCITY] = velMulUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_SET_VELOCITY] = velSetUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_VICTORY_QUOTE] = victoryQuoteUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_WIDTH] = widthUnloadFunction;
	gMugenStateControllerVariableHandler.mStateControllerUnloaders[MUGEN_STATE_CONTROLLER_TYPE_ZOOM] = zoomUnloadFunction;
}

void setupDreamMugenStateControllerHandler(MemoryStack* tMemoryStack) {
	setupStateControllerParsers();
	setupStateControllerHandlers();
	setupStateControllerUnloaders();
	gMugenStateControllerVariableHandler.mMemoryStack = tMemoryStack;
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
	CreateAnimationStoryController* e = (CreateAnimationStoryController*)allocMemoryOnMemoryStackOrMemory(sizeof(CreateAnimationStoryController));

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("id", tGroup, &e->mID);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("anim", tGroup, &e->mAnimation);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("loop", tGroup, &e->mIsLooping);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("pos", tGroup, &e->mPosition);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("stage", tGroup, &e->mIsBoundToStage);
	e->mHasShadow = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("shadow", tGroup, &e->mShadowBasePositionY);

	tController->mType = MUGEN_STORY_STATE_CONTROLLER_TYPE_CREATE_ANIMATION;
	tController->mData = e;
}

typedef struct {
	DreamMugenAssignment* mID;
	DreamMugenAssignment* mAnimation;

	int mHasTarget;
	DreamMugenAssignment* mTarget;
} ChangeAnimationStoryController;

static void parseChangeAnimationStoryController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup, DreamMugenStateControllerType tType) {
	ChangeAnimationStoryController* e = (ChangeAnimationStoryController*)allocMemoryOnMemoryStackOrMemory(sizeof(ChangeAnimationStoryController));

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("id", tGroup, &e->mID);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("anim", tGroup, &e->mAnimation);
	e->mHasTarget = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("target", tGroup, &e->mTarget);

	tController->mType = tType;
	tController->mData = e;

}

typedef struct {
	DreamMugenAssignment* mID;
	DreamMugenAssignment* mPosition;

	DreamMugenAssignment* mTextOffset;

	int mHasBackgroundSprite;
	DreamMugenAssignment* mBackgroundSprite;
	int mHasBackgroundAnimation;
	DreamMugenAssignment* mBackgroundAnimation;
	DreamMugenAssignment* mBackgroundOffset;
	DreamMugenAssignment* mBackgroundScale;

	int mHasFaceSprite;
	DreamMugenAssignment* mFaceSprite;
	int mHasFaceAnimation;
	DreamMugenAssignment* mFaceAnimation;
	DreamMugenAssignment* mFaceOffset;
	DreamMugenAssignment* mFaceScale;

	int mHasContinueSprite;
	DreamMugenAssignment* mContinueSprite;
	int mHasContinueAnimation;
	DreamMugenAssignment* mContinueAnimation;
	int mHasContinueSound;
	DreamMugenAssignment* mContinueSound;
	DreamMugenAssignment* mContinueOffset;
	DreamMugenAssignment* mContinueScale;

	int mHasName;
	DreamMugenAssignment* mName;
	DreamMugenAssignment* mNameFont;
	DreamMugenAssignment* mNameOffset;
	DreamMugenAssignment* mNameScale;

	DreamMugenAssignment* mText;
	DreamMugenAssignment* mTextScale;
	int mHasTextSound;
	DreamMugenAssignment* mTextSound;
	DreamMugenAssignment* mTextSoundFrequency;
	DreamMugenAssignment* mFont;
	DreamMugenAssignment* mWidth;

	DreamMugenAssignment* mIsBuildingUp;

	int mHasNextState;
	DreamMugenAssignment* mNextState;
} CreateTextStoryController;

static void parseCreateTextStoryController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup, DreamMugenStateControllerType tType) {
	CreateTextStoryController* e = (CreateTextStoryController*)allocMemoryOnMemoryStackOrMemory(sizeof(CreateTextStoryController));

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("id", tGroup, &e->mID);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("pos", tGroup, &e->mPosition);

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("text.offset", tGroup, &e->mTextOffset);

	e->mHasBackgroundSprite = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("bg.spr", tGroup, &e->mBackgroundSprite);
	e->mHasBackgroundAnimation = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("bg.anim", tGroup, &e->mBackgroundAnimation);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("bg.offset", tGroup, &e->mBackgroundOffset);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("bg.scale", tGroup, &e->mBackgroundScale);

	e->mHasFaceSprite = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("face.spr", tGroup, &e->mFaceSprite);
	e->mHasFaceAnimation = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("face.anim", tGroup, &e->mFaceAnimation);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("face.offset", tGroup, &e->mFaceOffset);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("face.scale", tGroup, &e->mFaceScale);

	e->mHasContinueSprite = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("continue.spr", tGroup, &e->mContinueSprite);
	e->mHasContinueAnimation = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("continue.anim", tGroup, &e->mContinueAnimation);
	e->mHasContinueSound = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("continue.snd", tGroup, &e->mContinueSound);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("continue.offset", tGroup, &e->mContinueOffset);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("continue.scale", tGroup, &e->mContinueScale);

	e->mHasName = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("name", tGroup, &e->mName);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("name.font", tGroup, &e->mNameFont);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("name.offset", tGroup, &e->mNameOffset);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("name.scale", tGroup, &e->mNameScale);

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("text", tGroup, &e->mText);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("text.scale", tGroup, &e->mTextScale);
	e->mHasTextSound = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("text.snd", tGroup, &e->mTextSound);
	if (e->mHasTextSound)
	{
		fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("text.snd.freq", tGroup, &e->mTextSoundFrequency);
	}
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("font", tGroup, &e->mFont, "1 , 0 , 0");
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("width", tGroup, &e->mWidth);

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("buildup", tGroup, &e->mIsBuildingUp);

	e->mHasNextState = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("nextstate", tGroup, &e->mNextState);

	tController->mType = tType;
	tController->mData = e;
}

typedef struct {
	DreamMugenAssignment* mID;
} RemoveElementStoryController;

static void parseRemoveElementStoryController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup, DreamMugenStateControllerType tType) {
	RemoveElementStoryController* e = (RemoveElementStoryController*)allocMemoryOnMemoryStackOrMemory(sizeof(RemoveElementStoryController));

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("id", tGroup, &e->mID);
	
	tController->mType = tType;
	tController->mData = e;
}

typedef struct {
	DreamMugenAssignment* mID;
	int mHasTarget;
	DreamMugenAssignment* mTarget;

	int mDoesChangePosition;
	DreamMugenAssignment* mPosition;

	int mDoesChangeBackgroundSprite;
	DreamMugenAssignment* mBackgroundSprite;
	int mDoesChangeBackgroundOffset;
	DreamMugenAssignment* mBackgroundOffset;
	int mDoesChangeBackgroundScale;
	DreamMugenAssignment* mBackgroundScale;

	int mDoesChangeFaceSprite;
	DreamMugenAssignment* mFaceSprite;
	int mDoesChangeFaceOffset;
	DreamMugenAssignment* mFaceOffset;
	int mDoesChangeFaceScale;
	DreamMugenAssignment* mFaceScale;

	int mDoesChangeContinueAnimation;
	DreamMugenAssignment* mContinueAnimation;
	int mDoesChangeContinueSound;
	DreamMugenAssignment* mContinueSound;
	int mDoesChangeContinueOffset;
	DreamMugenAssignment* mContinueOffset;
	int mDoesChangeContinueScale;
	DreamMugenAssignment* mContinueScale;

	int mDoesChangeName;
	DreamMugenAssignment* mName;
	int mDoesChangeNameFont;
	DreamMugenAssignment* mNameFont;
	int mDoesChangeNameOffset;
	DreamMugenAssignment* mNameOffset;
	int mDoesChangeNameScale;
	DreamMugenAssignment* mNameScale;

	int mDoesChangeText;
	DreamMugenAssignment* mText;
	int mDoesChangeFont;
	DreamMugenAssignment* mFont;
	int mDoesChangeTextScale;
	DreamMugenAssignment* mTextScale;
	int mDoesChangeTextSound;
	DreamMugenAssignment* mTextSound;
	int mDoesChangeTextSoundFrequency;
	DreamMugenAssignment* mTextSoundFrequency;

	int mDoesChangeTextOffset;
	DreamMugenAssignment* mTextOffset;

	int mDoesChangeNextState;
	DreamMugenAssignment* mNextState;

	DreamMugenAssignment* mIsBuildingUp;
} ChangeTextStoryController;

static void parseChangeTextStoryController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup, DreamMugenStateControllerType tType) {
	ChangeTextStoryController* e = (ChangeTextStoryController*)allocMemoryOnMemoryStackOrMemory(sizeof(ChangeTextStoryController));

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("id", tGroup, &e->mID);
	e->mHasTarget = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("target", tGroup, &e->mTarget);
	e->mDoesChangePosition = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("pos", tGroup, &e->mPosition);

	e->mDoesChangeBackgroundSprite = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("bg.spr", tGroup, &e->mBackgroundSprite);
	e->mDoesChangeBackgroundOffset = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("bg.offset", tGroup, &e->mBackgroundOffset);
	e->mDoesChangeBackgroundScale = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("bg.scale", tGroup, &e->mBackgroundScale);

	e->mDoesChangeFaceSprite = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("face.spr", tGroup, &e->mFaceSprite);
	e->mDoesChangeFaceOffset = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("face.offset", tGroup, &e->mFaceOffset);
	e->mDoesChangeFaceScale = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("face.scale", tGroup, &e->mFaceScale);

	e->mDoesChangeContinueAnimation = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("continue.anim", tGroup, &e->mContinueAnimation);
	e->mDoesChangeContinueSound = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("continue.snd", tGroup, &e->mContinueSound);
	e->mDoesChangeContinueOffset = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("continue.offset", tGroup, &e->mContinueOffset);
	e->mDoesChangeContinueScale = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("continue.scale", tGroup, &e->mContinueScale);

	e->mDoesChangeName = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("name", tGroup, &e->mName);
	e->mDoesChangeNameFont = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("name.font", tGroup, &e->mNameFont);
	e->mDoesChangeNameOffset = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("name.offset", tGroup, &e->mNameOffset);
	e->mDoesChangeNameScale = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("name.scale", tGroup, &e->mNameScale);

	e->mDoesChangeText = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("text", tGroup, &e->mText);
	e->mDoesChangeFont = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("font", tGroup, &e->mFont);
	e->mDoesChangeTextScale = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("text.scale", tGroup, &e->mTextScale);
	e->mDoesChangeTextSound = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("text.snd", tGroup, &e->mTextSound);
	e->mDoesChangeTextSoundFrequency = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("text.snd.freq", tGroup, &e->mTextSoundFrequency);
	e->mDoesChangeTextOffset = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("text.offset", tGroup, &e->mTextOffset);

	e->mDoesChangeNextState = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("nextstate", tGroup, &e->mNextState);

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("buildup", tGroup, &e->mIsBuildingUp);

	tController->mType = tType;
	tController->mData = e;
}

typedef struct {
	DreamMugenAssignment* mDuration;
	DreamMugenAssignment* mColor;

} FadeStoryController;

static void parseFadeStoryController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup, DreamMugenStateControllerType tType) {
	FadeStoryController* e = (FadeStoryController*)allocMemoryOnMemoryStackOrMemory(sizeof(FadeStoryController));

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("duration", tGroup, &e->mDuration);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("color", tGroup, &e->mColor, "0,0,0");

	tController->mType = tType;
	tController->mData = e;
}

typedef struct {
	DreamMugenAssignment* mID;
	DreamMugenAssignment* mFacing;
	
	int mHasTarget;
	DreamMugenAssignment* mTarget;

} AnimationSetFaceDirectionStoryController;

static void parseAnimationSetFaceDirectionStoryController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup, DreamMugenStateControllerType tType) {
	AnimationSetFaceDirectionStoryController* e = (AnimationSetFaceDirectionStoryController*)allocMemoryOnMemoryStackOrMemory(sizeof(AnimationSetFaceDirectionStoryController));

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("id", tGroup, &e->mID);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("facing", tGroup, &e->mFacing);
	e->mHasTarget = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("target", tGroup, &e->mTarget);

	tController->mType = tType;
	tController->mData = e;
}

static void parseAnimationAngleStoryController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup, DreamMugenStateControllerType tType) {
	AnimationSetFaceDirectionStoryController* e = (AnimationSetFaceDirectionStoryController*)allocMemoryOnMemoryStackOrMemory(sizeof(AnimationSetFaceDirectionStoryController));

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("id", tGroup, &e->mID);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("angle", tGroup, &e->mFacing);
	e->mHasTarget = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("target", tGroup, &e->mTarget);

	tController->mType = tType;
	tController->mData = e;
}

typedef struct {
	DreamMugenAssignment* mID;
	DreamMugenAssignment* mValue;
	int mHasTarget;
	DreamMugenAssignment* mTarget;

} AnimationSetSingleValueStoryController;

static void parseAnimationSetColorStoryController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup, DreamMugenStateControllerType tType) {
	AnimationSetSingleValueStoryController* e = (AnimationSetSingleValueStoryController*)allocMemoryOnMemoryStackOrMemory(sizeof(AnimationSetSingleValueStoryController));

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("id", tGroup, &e->mID);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("color", tGroup, &e->mValue, "1,1,1");
	e->mHasTarget = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("target", tGroup, &e->mTarget);

	tController->mType = tType;
	tController->mData = e;
}

static void parseAnimationSetOpacityStoryController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup, DreamMugenStateControllerType tType) {
	AnimationSetSingleValueStoryController* e = (AnimationSetSingleValueStoryController*)allocMemoryOnMemoryStackOrMemory(sizeof(AnimationSetSingleValueStoryController));

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("id", tGroup, &e->mID);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("opacity", tGroup, &e->mValue, "1");
	e->mHasTarget = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("target", tGroup, &e->mTarget);

	tController->mType = tType;
	tController->mData = e;
}

typedef struct {
	DreamMugenAssignment* mID;

	DreamMugenAssignment* mName;
	DreamMugenAssignment* mPreferredPalette;
	DreamMugenAssignment* mStartAnimationNumber;
	DreamMugenAssignment* mPosition;
	DreamMugenAssignment* mIsBoundToStage;

	int mHasShadow;
	DreamMugenAssignment* mShadowBasePositionY;
} CreateCharacterStoryController;

static void parseCreateCharStoryController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) {
	CreateCharacterStoryController* e = (CreateCharacterStoryController*)allocMemoryOnMemoryStackOrMemory(sizeof(CreateCharacterStoryController));

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("id", tGroup, &e->mID);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("name", tGroup, &e->mName);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("palette", tGroup, &e->mPreferredPalette);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("anim", tGroup, &e->mStartAnimationNumber);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("pos", tGroup, &e->mPosition);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("stage", tGroup, &e->mIsBoundToStage);
	e->mHasShadow = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("shadow", tGroup, &e->mShadowBasePositionY);

	tController->mType = MUGEN_STORY_STATE_CONTROLLER_TYPE_CREATE_CHARACTER;
	tController->mData = e;
}

typedef struct {
	DreamMugenAssignment* mID;
	DreamMugenAssignment* mState;
} CreateHelperStoryController;

static void parseCreateHelperStoryController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup, DreamMugenStateControllerType tType) {
	CreateHelperStoryController* e = (CreateHelperStoryController*)allocMemoryOnMemoryStackOrMemory(sizeof(CreateHelperStoryController));

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("id", tGroup, &e->mID);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("state", tGroup, &e->mState);

	tController->mType = tType;
	tController->mData = e;
}

typedef struct {
	DreamMugenAssignment* mID;
	DreamMugenAssignment* mCharacterID;
	DreamMugenAssignment* mOffset;
} LockTextToCharacterStoryController;

static void parseLockTextToCharacterStoryController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) {
	LockTextToCharacterStoryController* e = (LockTextToCharacterStoryController*)allocMemoryOnMemoryStackOrMemory(sizeof(LockTextToCharacterStoryController));

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("id", tGroup, &e->mID);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("character", tGroup, &e->mCharacterID);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("offset", tGroup, &e->mOffset);

	tController->mType = MUGEN_STORY_STATE_CONTROLLER_TYPE_LOCK_TEXT_TO_CHARACTER;
	tController->mData = e;
}

typedef struct {
	DreamMugenAssignment* mID;
	DreamMugenAssignment* mName;
} NameTextStoryController;

static void parseNameIDStoryController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) {
	NameTextStoryController* e = (NameTextStoryController*)allocMemoryOnMemoryStackOrMemory(sizeof(NameTextStoryController));

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("id", tGroup, &e->mID);
	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("name", tGroup, &e->mName);

	tController->mType = MUGEN_STORY_STATE_CONTROLLER_TYPE_NAME_ID;
	tController->mData = e;
}


enum StoryVarSetType : uint8_t {
	STORY_VAR_SET_TYPE_INTEGER,
	STORY_VAR_SET_TYPE_FLOAT,
	STORY_VAR_SET_TYPE_STRING,
};

typedef struct {
	DreamMugenAssignment* mID;
	DreamMugenAssignment* mAssignment;

	uint8_t mType;
} StoryVarSetControllerEntry;

typedef struct {
	Vector mStoryVarSets;
} StoryVarSetController;

typedef struct {
	StoryVarSetController* mController;
	MugenDefScriptGroup* mGroup;
} StoryVarSetControllerCaller;

static void parseSingleStoryVarSetControllerEntry(StoryVarSetControllerCaller* tCaller, const string& tName, MugenDefScriptGroupElement& tData) {
	StoryVarSetControllerCaller* caller = (StoryVarSetControllerCaller*)tCaller;
	(void)tData;

	int isEntry = tName.find('(') != string::npos;
	if (!isEntry) return;

	StoryVarSetControllerEntry* e = (StoryVarSetControllerEntry*)allocMemoryOnMemoryStackOrMemory(sizeof(StoryVarSetControllerEntry));

	char name[100];
	strcpy(name, tName.data());
	char* start = strchr(name, '(');
	assert(start != NULL);
	*start = '\0';

	char value[100];
	strcpy(value, start + 1);
	char* end = strchr(value, ')');
	assert(end != NULL);
	*end = '\0';


	if (!strcmp("var", name)) {
		e->mType = STORY_VAR_SET_TYPE_INTEGER;
	}
	else if (!strcmp("fvar", name)) {
		e->mType = STORY_VAR_SET_TYPE_FLOAT;
	}
	else if (!strcmp("svar", name)) {
		e->mType = STORY_VAR_SET_TYPE_STRING;
	}
	else {
		logWarningFormat("Unrecognized variable setting name %s. Default to var.", name);
		e->mType = STORY_VAR_SET_TYPE_INTEGER;
	}
	
	e->mID = parseDreamMugenAssignmentFromString(value);
	fetchDreamAssignmentFromGroupAndReturnWhetherItExists(tName.data(), caller->mGroup, &e->mAssignment);

	vector_push_back(&caller->mController->mStoryVarSets, e);
}

static void loadSingleOriginalStoryVarSetController(Vector* tDst, MugenDefScriptGroup* tGroup, MugenDefScriptGroupElement* tIDElement, StoryVarSetType tType) {
	StoryVarSetControllerEntry* e = (StoryVarSetControllerEntry*)allocMemoryOnMemoryStackOrMemory(sizeof(StoryVarSetControllerEntry));
	e->mType = tType;
	fetchDreamAssignmentFromGroupAsElement(tIDElement, &e->mID);
	fetchDreamAssignmentFromGroupAndReturnWhetherItExists("value", tGroup, &e->mAssignment);

	vector_push_back(tDst, e);
}

static void parseStoryVarSetController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup, DreamMugenStoryStateControllerType tType) {
	int isIntegerVersion = stl_string_map_contains_array(tGroup->mElements, "v");
	int isFloatVersion = stl_string_map_contains_array(tGroup->mElements, "fv");
	int isStringVersion = stl_string_map_contains_array(tGroup->mElements, "sv");

	StoryVarSetController* e = (StoryVarSetController*)allocMemoryOnMemoryStackOrMemory(sizeof(StoryVarSetController));
	e->mStoryVarSets = new_vector();

	if (isIntegerVersion) {
		loadSingleOriginalStoryVarSetController(&e->mStoryVarSets, tGroup, &tGroup->mElements["v"], STORY_VAR_SET_TYPE_INTEGER);
	}
	else if (isFloatVersion) {
		loadSingleOriginalStoryVarSetController(&e->mStoryVarSets, tGroup, &tGroup->mElements["fv"], STORY_VAR_SET_TYPE_FLOAT);
	}
	else if (isStringVersion) {
		loadSingleOriginalStoryVarSetController(&e->mStoryVarSets, tGroup, &tGroup->mElements["sv"], STORY_VAR_SET_TYPE_STRING);
	}
	else {
		StoryVarSetControllerCaller caller;
		caller.mController = e;
		caller.mGroup = tGroup;

		stl_string_map_map(tGroup->mElements, parseSingleStoryVarSetControllerEntry, &caller);
	}

	if (vector_size(&e->mStoryVarSets) != 1) {
		logWarning("Unable to parse StoryVarSetController. Missing elements. Defaulting to Null controller.");
		// unable to delete mugen assignments if on memory stack
		delete_vector(&e->mStoryVarSets);
		parseNullController(tController, MUGEN_STATE_CONTROLLER_TYPE_NULL);
		return;
	}

	tController->mType = tType;
	tController->mData = e;
}

typedef struct {
	DreamMugenAssignment* mID;

	int mHasTarget;
	DreamMugenAssignment* mTarget;
	DreamMugenAssignment* x;
	DreamMugenAssignment* y;

	uint8_t mIsSettingX;
	uint8_t mIsSettingY;
} StoryTarget2DPhysicsController;

static void parseStoryTarget2DPhysicsController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup, DreamMugenStateControllerType tType) {
	StoryTarget2DPhysicsController* e = (StoryTarget2DPhysicsController*)allocMemoryOnMemoryStackOrMemory(sizeof(StoryTarget2DPhysicsController));

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("id", tGroup, &e->mID);
	e->mIsSettingX = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("x", tGroup, &e->x);
	e->mIsSettingY = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("y", tGroup, &e->y);
	e->mHasTarget = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("target", tGroup, &e->mTarget);

	tController->mType = tType;
	tController->mData = e;
}

typedef struct {
	DreamMugenAssignment* mID;

	int mHasTarget;
	DreamMugenAssignment* mTarget;
	DreamMugenAssignment* x;
	DreamMugenAssignment* y;
	DreamMugenAssignment* z;

	uint8_t mIsSettingX;
	uint8_t mIsSettingY;
	uint8_t mIsSettingZ;
} StoryTarget3DPhysicsController;

static void parseStoryTarget3DPhysicsController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup, DreamMugenStateControllerType tType) {
	StoryTarget3DPhysicsController* e = (StoryTarget3DPhysicsController*)allocMemoryOnMemoryStackOrMemory(sizeof(StoryTarget3DPhysicsController));

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("id", tGroup, &e->mID);
	e->mIsSettingX = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("x", tGroup, &e->x);
	e->mIsSettingY = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("y", tGroup, &e->y);
	e->mIsSettingZ = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("z", tGroup, &e->z);
	e->mHasTarget = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("target", tGroup, &e->mTarget);

	tController->mType = tType;
	tController->mData = e;
}

typedef struct {
	DreamMugenAssignment* mPath;
} StoryPlayMusicController;

static void parseStoryPlayMusicController(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) {
	StoryPlayMusicController* e = (StoryPlayMusicController*)allocMemoryOnMemoryStackOrMemory(sizeof(StoryPlayMusicController));

	fetchAssignmentFromGroupAndReturnWhetherItExistsDefaultString("path", tGroup, &e->mPath);

	tController->mType = MUGEN_STORY_STATE_CONTROLLER_TYPE_PLAY_MUSIC;
	tController->mData = e;
}

void nullStoryParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* /*tGroup*/) { parseNullController(tController, (DreamMugenStateControllerType)MUGEN_STORY_STATE_CONTROLLER_TYPE_NULL); }
void createAnimationStoryParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseCreateAnimationStoryController(tController, tGroup); }
void removeAnimationStoryParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseRemoveElementStoryController(tController, tGroup, (DreamMugenStateControllerType)MUGEN_STORY_STATE_CONTROLLER_TYPE_REMOVE_ANIMATION); }
void changeAnimationStoryParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseChangeAnimationStoryController(tController, tGroup, (DreamMugenStateControllerType)MUGEN_STORY_STATE_CONTROLLER_TYPE_CHANGE_ANIMATION); }
void createTextStoryParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseCreateTextStoryController(tController, tGroup, (DreamMugenStateControllerType)MUGEN_STORY_STATE_CONTROLLER_TYPE_CREATE_TEXT); }
void removeTextStoryParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseRemoveElementStoryController(tController, tGroup, (DreamMugenStateControllerType)MUGEN_STORY_STATE_CONTROLLER_TYPE_REMOVE_TEXT); }
void changeTextStoryParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseChangeTextStoryController(tController, tGroup, (DreamMugenStateControllerType)MUGEN_STORY_STATE_CONTROLLER_TYPE_CHANGE_TEXT); }
void lockTextToCharacterStoryParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseLockTextToCharacterStoryController(tController, tGroup); }
void textPositionSetStoryParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseStoryTarget2DPhysicsController(tController, tGroup, (DreamMugenStateControllerType)MUGEN_STORY_STATE_CONTROLLER_TYPE_TEXT_SET_POSITION); }
void textPositionAddStoryParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseStoryTarget2DPhysicsController(tController, tGroup, (DreamMugenStateControllerType)MUGEN_STORY_STATE_CONTROLLER_TYPE_TEXT_ADD_POSITION); }
void nameIDStoryParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseNameIDStoryController(tController, tGroup); }
void changeStateStoryParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseSingleRequiredValueController(tController, tGroup, (DreamMugenStateControllerType)MUGEN_STORY_STATE_CONTROLLER_TYPE_CHANGE_STATE); }
void changeStateParentStoryParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseSingleRequiredValueController(tController, tGroup, (DreamMugenStateControllerType)MUGEN_STORY_STATE_CONTROLLER_TYPE_CHANGE_STATE_PARENT); }
void changeStateRootStoryParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseSingleRequiredValueController(tController, tGroup, (DreamMugenStateControllerType)MUGEN_STORY_STATE_CONTROLLER_TYPE_CHANGE_STATE_ROOT); }
void fadeInStoryParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseFadeStoryController(tController, tGroup, (DreamMugenStateControllerType)MUGEN_STORY_STATE_CONTROLLER_TYPE_FADE_IN); }
void fadeOutStoryParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseFadeStoryController(tController, tGroup, (DreamMugenStateControllerType)MUGEN_STORY_STATE_CONTROLLER_TYPE_FADE_OUT); }
void gotoStoryStepStoryParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseSingleRequiredValueController(tController, tGroup, (DreamMugenStateControllerType)MUGEN_STORY_STATE_CONTROLLER_TYPE_GOTO_STORY_STEP); }
void gotoIntroStoryParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* /*tGroup*/) { parseNullController(tController, (DreamMugenStateControllerType)MUGEN_STORY_STATE_CONTROLLER_TYPE_GOTO_INTRO); }
void gotoTitleStoryParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* /*tGroup*/) { parseNullController(tController, (DreamMugenStateControllerType)MUGEN_STORY_STATE_CONTROLLER_TYPE_GOTO_TITLE); }
void animationSetPositionStoryParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseStoryTarget2DPhysicsController(tController, tGroup, (DreamMugenStateControllerType)MUGEN_STORY_STATE_CONTROLLER_TYPE_ANIMATION_SET_POSITION); }
void animationAddPositionStoryParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseStoryTarget2DPhysicsController(tController, tGroup, (DreamMugenStateControllerType)MUGEN_STORY_STATE_CONTROLLER_TYPE_ANIMATION_ADD_POSITION); }
void animationSetStagePositionStoryParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseStoryTarget2DPhysicsController(tController, tGroup, (DreamMugenStateControllerType)MUGEN_STORY_STATE_CONTROLLER_TYPE_ANIMATION_SET_STAGE_POSITION); }
void animationSetScaleStoryParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseStoryTarget2DPhysicsController(tController, tGroup, (DreamMugenStateControllerType)MUGEN_STORY_STATE_CONTROLLER_TYPE_ANIMATION_SET_SCALE); }
void animationSetFaceDirectionStoryParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseAnimationSetFaceDirectionStoryController(tController, tGroup, (DreamMugenStateControllerType)MUGEN_STORY_STATE_CONTROLLER_TYPE_ANIMATION_SET_FACEDIRECTION); }
void animationSetAngleStoryParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseAnimationAngleStoryController(tController, tGroup, (DreamMugenStateControllerType)MUGEN_STORY_STATE_CONTROLLER_TYPE_ANIMATION_SET_ANGLE); }
void animationAddAngleStoryParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseAnimationAngleStoryController(tController, tGroup, (DreamMugenStateControllerType)MUGEN_STORY_STATE_CONTROLLER_TYPE_ANIMATION_ADD_ANGLE); }
void animationSetColorStoryParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseAnimationSetColorStoryController(tController, tGroup, (DreamMugenStateControllerType)MUGEN_STORY_STATE_CONTROLLER_TYPE_ANIMATION_SET_COLOR); }
void animationSetOpacityStoryParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseAnimationSetOpacityStoryController(tController, tGroup, (DreamMugenStateControllerType)MUGEN_STORY_STATE_CONTROLLER_TYPE_ANIMATION_SET_OPACITY); }
void endStoryboardStoryParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseSingleRequiredValueController(tController, tGroup, (DreamMugenStateControllerType)MUGEN_STORY_STATE_CONTROLLER_TYPE_END_STORYBOARD); }
void moveStageStoryParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parse2DPhysicsController(tController, tGroup, (DreamMugenStateControllerType)MUGEN_STORY_STATE_CONTROLLER_TYPE_MOVE_STAGE); }
void createCharStoryParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseCreateCharStoryController(tController, tGroup); }
void removeCharStoryParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseRemoveElementStoryController(tController, tGroup, (DreamMugenStateControllerType)MUGEN_STORY_STATE_CONTROLLER_TYPE_REMOVE_CHARACTER);  }
void charChangeAnimStoryParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseChangeAnimationStoryController(tController, tGroup, (DreamMugenStateControllerType)MUGEN_STORY_STATE_CONTROLLER_TYPE_CHARACTER_CHANGE_ANIMATION); }
void charSetPosStoryParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseStoryTarget3DPhysicsController(tController, tGroup, (DreamMugenStateControllerType)MUGEN_STORY_STATE_CONTROLLER_TYPE_CHARACTER_SET_POSITION); }
void charAddPosStoryParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseStoryTarget2DPhysicsController(tController, tGroup, (DreamMugenStateControllerType)MUGEN_STORY_STATE_CONTROLLER_TYPE_CHARACTER_ADD_POSITION); }
void charSetStagePosStoryParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseStoryTarget2DPhysicsController(tController, tGroup, (DreamMugenStateControllerType)MUGEN_STORY_STATE_CONTROLLER_TYPE_CHARACTER_SET_STAGE_POSITION); }
void charSetScaleStoryParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseStoryTarget2DPhysicsController(tController, tGroup, (DreamMugenStateControllerType)MUGEN_STORY_STATE_CONTROLLER_TYPE_CHARACTER_SET_SCALE); }
void charSetFaceDirectionStoryParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseAnimationSetFaceDirectionStoryController(tController, tGroup, (DreamMugenStateControllerType)MUGEN_STORY_STATE_CONTROLLER_TYPE_CHARACTER_SET_FACEDIRECTION); }
void charSetColorStoryParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseAnimationSetColorStoryController(tController, tGroup, (DreamMugenStateControllerType)MUGEN_STORY_STATE_CONTROLLER_TYPE_CHARACTER_SET_COLOR); }
void charSetOpacityStoryParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseAnimationSetOpacityStoryController(tController, tGroup, (DreamMugenStateControllerType)MUGEN_STORY_STATE_CONTROLLER_TYPE_CHARACTER_SET_OPACITY); }
void charSetAngleStoryParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseAnimationAngleStoryController(tController, tGroup, (DreamMugenStateControllerType)MUGEN_STORY_STATE_CONTROLLER_TYPE_CHARACTER_SET_ANGLE); }
void charAddAngleStoryParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseAnimationAngleStoryController(tController, tGroup, (DreamMugenStateControllerType)MUGEN_STORY_STATE_CONTROLLER_TYPE_CHARACTER_ADD_ANGLE); }
void createHelperStoryParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseCreateHelperStoryController(tController, tGroup, (DreamMugenStateControllerType)MUGEN_STORY_STATE_CONTROLLER_TYPE_CREATE_HELPER); }
void removeHelperStoryParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseRemoveElementStoryController(tController, tGroup, (DreamMugenStateControllerType)MUGEN_STORY_STATE_CONTROLLER_TYPE_REMOVE_HELPER); }
void varSetStoryParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseStoryVarSetController(tController, tGroup, MUGEN_STORY_STATE_CONTROLLER_TYPE_VAR_SET); }
void varAddStoryParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseStoryVarSetController(tController, tGroup, MUGEN_STORY_STATE_CONTROLLER_TYPE_VAR_ADD); }
void parentVarSetStoryParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseStoryVarSetController(tController, tGroup, MUGEN_STORY_STATE_CONTROLLER_TYPE_PARENT_VAR_SET); }
void parentVarAddStoryParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseStoryVarSetController(tController, tGroup, MUGEN_STORY_STATE_CONTROLLER_TYPE_PARENT_VAR_ADD); }
void globalVarSetStoryParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseStoryVarSetController(tController, tGroup, MUGEN_STORY_STATE_CONTROLLER_TYPE_GLOBAL_VAR_SET); }
void globalVarAddStoryParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseStoryVarSetController(tController, tGroup, MUGEN_STORY_STATE_CONTROLLER_TYPE_GLOBAL_VAR_ADD); }
void playMusicStoryParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseStoryPlayMusicController(tController, tGroup); }
void stopMusicStoryParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* /*tGroup*/) { parseNullController(tController, (DreamMugenStateControllerType)MUGEN_STORY_STATE_CONTROLLER_TYPE_STOP_MUSIC); }
void pauseMusicStoryParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* /*tGroup*/) { parseNullController(tController, (DreamMugenStateControllerType)MUGEN_STORY_STATE_CONTROLLER_TYPE_PAUSE_MUSIC); }
void resumeMusicStoryParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* /*tGroup*/) { parseNullController(tController, (DreamMugenStateControllerType)MUGEN_STORY_STATE_CONTROLLER_TYPE_RESUME_MUSIC); }
void destroySelfStoryParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* /*tGroup*/) { parseNullController(tController, (DreamMugenStateControllerType)MUGEN_STORY_STATE_CONTROLLER_TYPE_DESTROY_SELF); }
void cameraFocusStoryParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parse2DPhysicsController(tController, tGroup, (DreamMugenStateControllerType)MUGEN_STORY_STATE_CONTROLLER_TYPE_CAMERA_FOCUS); }
void cameraZoomStoryParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseSingleRequiredValueController(tController, tGroup, (DreamMugenStateControllerType)MUGEN_STORY_STATE_CONTROLLER_TYPE_CAMERA_ZOOM); }
void playSoundStoryParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parsePlaySoundController(tController, tGroup, (DreamMugenStateControllerType)MUGEN_STORY_STATE_CONTROLLER_TYPE_PLAY_SOUND); }
void panSoundStoryParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseSoundPanController(tController, tGroup, (DreamMugenStateControllerType)MUGEN_STORY_STATE_CONTROLLER_TYPE_PAN_SOUND); }
void stopSoundStoryParseFunction(DreamMugenStateController* tController, MugenDefScriptGroup* tGroup) { parseStopSoundController(tController, tGroup, (DreamMugenStateControllerType)MUGEN_STORY_STATE_CONTROLLER_TYPE_STOP_SOUND); }

static void setupStoryStateControllerParsers() {
	gMugenStateControllerVariableHandler.mStateControllerParsers.clear();
	
	gMugenStateControllerVariableHandler.mStateControllerParsers["null"] = nullStoryParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["createanim"] = createAnimationStoryParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["removeanim"] = removeAnimationStoryParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["changeanim"] = changeAnimationStoryParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["createtext"] = createTextStoryParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["removetext"] = removeTextStoryParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["changetext"] = changeTextStoryParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["locktext"] = lockTextToCharacterStoryParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["textposset"] = textPositionSetStoryParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["textposadd"] = textPositionAddStoryParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["nameid"] = nameIDStoryParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["changestate"] = changeStateStoryParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["changestateparent"] = changeStateParentStoryParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["changestateroot"] = changeStateRootStoryParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["fadein"] = fadeInStoryParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["fadeout"] = fadeOutStoryParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["gotostorystep"] = gotoStoryStepStoryParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["gotointro"] = gotoIntroStoryParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["gototitle"] = gotoTitleStoryParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["animposset"] = animationSetPositionStoryParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["animposadd"] = animationAddPositionStoryParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["animstageposset"] = animationSetStagePositionStoryParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["animscaleset"] = animationSetScaleStoryParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["animsetfacing"] = animationSetFaceDirectionStoryParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["animsetangle"] = animationSetAngleStoryParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["animaddangle"] = animationAddAngleStoryParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["animsetcolor"] = animationSetColorStoryParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["animsetopacity"] = animationSetOpacityStoryParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["endstoryboard"] = endStoryboardStoryParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["movestage"] = moveStageStoryParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["createchar"] = createCharStoryParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["removechar"] = removeCharStoryParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["charchangeanim"] = charChangeAnimStoryParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["charposset"] = charSetPosStoryParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["charposadd"] = charAddPosStoryParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["charstageposset"] = charSetStagePosStoryParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["charscaleset"] = charSetScaleStoryParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["charsetfacing"] = charSetFaceDirectionStoryParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["charsetcolor"] = charSetColorStoryParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["charsetopacity"] = charSetOpacityStoryParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["charsetangle"] = charSetAngleStoryParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["charaddangle"] = charAddAngleStoryParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["createhelper"] = createHelperStoryParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["removehelper"] = removeHelperStoryParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["varset"] = varSetStoryParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["varadd"] = varAddStoryParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["parentvarset"] = parentVarSetStoryParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["parentvaradd"] = parentVarAddStoryParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["globalvarset"] = globalVarSetStoryParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["globalvaradd"] = globalVarAddStoryParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["playmusic"] = playMusicStoryParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["stopmusic"] = stopMusicStoryParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["pausemusic"] = pauseMusicStoryParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["resumemusic"] = resumeMusicStoryParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["destroyself"] = destroySelfStoryParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["camerafocus"] = cameraFocusStoryParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["camerazoom"] = cameraZoomStoryParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["playsnd"] = playSoundStoryParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["sndpan"] = panSoundStoryParseFunction;
	gMugenStateControllerVariableHandler.mStateControllerParsers["stopsnd"] = stopSoundStoryParseFunction;
}

static int getDolmexicaStoryIDFromAssignment(DreamMugenAssignment** tAssignment, StoryInstance* tInstance) {
	string val;
	evaluateDreamAssignmentAndReturnAsString(val, tAssignment, (DreamPlayer*)tInstance);
	return getDolmexicaStoryIDFromString(val.data(), tInstance);
}

static int handleCreateAnimationStoryController(DreamMugenStateController* tController, StoryInstance* tInstance) {
	CreateAnimationStoryController* e = (CreateAnimationStoryController*)tController->mData;

	int id, animation, isLooping, isBoundToStage;
	Position2D position;
	id = getDolmexicaStoryIDFromAssignment(&e->mID, tInstance);
	evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mAnimation, (DreamPlayer*)tInstance, &animation, 0);
	evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mIsLooping, (DreamPlayer*)tInstance, &isLooping, 1);
	evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mIsBoundToStage, (DreamPlayer*)tInstance, &isBoundToStage, 0);
	evaluateDreamAssignmentAndReturnAsTwoFloatsWithDefaultValues(&e->mPosition, (DreamPlayer*)tInstance, &position.x, &position.y, 0, 0);

	addDolmexicaStoryAnimation(tInstance, id, animation, position);
	if (e->mHasShadow) {
		double shadowBasePosition;
		evaluateDreamAssignmentAndReturnAsOneFloatWithDefaultValue(&e->mShadowBasePositionY, (DreamPlayer*)tInstance, &shadowBasePosition, 0);
		setDolmexicaStoryAnimationShadow(tInstance, id, shadowBasePosition);
	}
	setDolmexicaStoryAnimationLooping(tInstance, id, isLooping);
	setDolmexicaStoryAnimationBoundToStage(tInstance, id, isBoundToStage);


	return 0;
}

static int handleRemoveAnimationStoryController(DreamMugenStateController* tController, StoryInstance* tInstance) {
	RemoveElementStoryController* e = (RemoveElementStoryController*)tController->mData;

	int id;
	id = getDolmexicaStoryIDFromAssignment(&e->mID, tInstance);

	removeDolmexicaStoryAnimation(tInstance, id);

	return 0;
}

static int handleChangeAnimationStoryController(DreamMugenStateController* tController, StoryInstance* tInstance) {
	ChangeAnimationStoryController* e = (ChangeAnimationStoryController*)tController->mData;

	int id, animation;
	id = getDolmexicaStoryIDFromAssignment(&e->mID, tInstance);
	evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mAnimation, (DreamPlayer*)tInstance, &animation, 0);

	changeDolmexicaStoryAnimation(tInstance, id, animation);

	return 0;
}

static void handleSingleStoryTextSpriteOrAnimation(int id, int tHasSprite, DreamMugenAssignment** tSprite, void(*tSpriteFunc)(StoryInstance*, int, const Vector2DI&, const Position&, const Vector2D&), int tHasAnimation, DreamMugenAssignment** tAnimation, void(*tAnimationFunc)(StoryInstance*, int, int, const Position&, const Vector2D&), DreamMugenAssignment** tOffset, double tDefaultZDelta, DreamMugenAssignment** tScale, StoryInstance* tInstance) {
	if (tHasAnimation) {
		int animation;
		Position offset;
		Vector2D scale;
		evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(tAnimation, (DreamPlayer*)tInstance, &animation, 0);
		evaluateDreamAssignmentAndReturnAsThreeFloatsWithDefaultValues(tOffset, (DreamPlayer*)tInstance, &offset.x, &offset.y, &offset.z, 0, 0, tDefaultZDelta);
		evaluateDreamAssignmentAndReturnAsTwoFloatsWithDefaultValues(tScale, (DreamPlayer*)tInstance, &scale.x, &scale.y, 1.0, 1.0);
		tAnimationFunc(tInstance, id, animation, offset, scale);
	}
	else if (tHasSprite) {
		Vector2DI sprite;
		Position offset;
		Vector2D scale;
		evaluateDreamAssignmentAndReturnAsTwoIntegersWithDefaultValues(tSprite, (DreamPlayer*)tInstance, &sprite.x, &sprite.y, 0, 0);
		evaluateDreamAssignmentAndReturnAsThreeFloatsWithDefaultValues(tOffset, (DreamPlayer*)tInstance, &offset.x, &offset.y, &offset.z, 0, 0, tDefaultZDelta);
		evaluateDreamAssignmentAndReturnAsTwoFloatsWithDefaultValues(tScale, (DreamPlayer*)tInstance, &scale.x, &scale.y, 1.0, 1.0);
		tSpriteFunc(tInstance, id, sprite, offset, scale);
	}
}

static void handleSingleStoryTextSound(int id, int tHasSound, DreamMugenAssignment** tSound, void(*tSoundFunc)(StoryInstance*, int, const Vector2DI&), StoryInstance* tInstance) {
	if (!tHasSound) return;
	const auto snd = evaluateDreamAssignmentAndReturnAsVector2DI(tSound, (DreamPlayer*)tInstance);
	tSoundFunc(tInstance, id, snd);
}

static int isStringEmptyOrWhitespace(const char * tString)
{
	for (; *tString; tString++) {
		if (*tString != ' ') return 0;
	}
	return 1;
}

static void handleSingleStoryTextName(int id, int tHasName, DreamMugenAssignment** tText, DreamMugenAssignment** tFont, DreamMugenAssignment** tOffset, DreamMugenAssignment** tScale, StoryInstance* tInstance) {
	if (!tHasName) return;

	string text;
	evaluateDreamAssignmentAndReturnAsString(text, tText, (DreamPlayer*)tInstance);
	Vector3DI font = evaluateDreamAssignmentAndReturnAsVector3DI(tFont, (DreamPlayer*)tInstance);
	double x, y, scale;
	evaluateDreamAssignmentAndReturnAsTwoFloatsWithDefaultValues(tOffset, (DreamPlayer*)tInstance, &x, &y, 0, 0);
	evaluateDreamAssignmentAndReturnAsOneFloatWithDefaultValue(tScale, (DreamPlayer*)tInstance, &scale, 1.0);
	setDolmexicaStoryTextName(tInstance, id, text.data(), font, Vector2D(x, y), scale);
}

static int handleCreateTextStoryController(DreamMugenStateController* tController, StoryInstance* tInstance) {
	CreateTextStoryController* e = (CreateTextStoryController*)tController->mData;

	int id;
	Position2D basePosition;
	Position2D textOffset;
	id = getDolmexicaStoryIDFromAssignment(&e->mID, tInstance);
	evaluateDreamAssignmentAndReturnAsTwoFloatsWithDefaultValues(&e->mPosition, (DreamPlayer*)tInstance, &basePosition.x, &basePosition.y, 0, 0);
	evaluateDreamAssignmentAndReturnAsTwoFloatsWithDefaultValues(&e->mTextOffset, (DreamPlayer*)tInstance, &textOffset.x, &textOffset.y, 0, 0);

	double width, scale;
	evaluateDreamAssignmentAndReturnAsOneFloatWithDefaultValue(&e->mWidth, (DreamPlayer*)tInstance, &width, INF);
	const auto font = evaluateDreamAssignmentAndReturnAsVector3DI(&e->mFont, (DreamPlayer*)tInstance);
	evaluateDreamAssignmentAndReturnAsOneFloatWithDefaultValue(&e->mTextScale, (DreamPlayer*)tInstance, &scale, 1.0);
	string text;
	evaluateDreamAssignmentAndReturnAsString(text, &e->mText, (DreamPlayer*)tInstance);
	int isEmpty = isStringEmptyOrWhitespace(text.data());
	addDolmexicaStoryText(tInstance, id, text.data(), font, scale, basePosition, textOffset, width);
	handleSingleStoryTextSound(id, e->mHasTextSound, &e->mTextSound, setDolmexicaStoryTextSound, tInstance);
	if (e->mHasTextSound)
	{
		int freq;
		evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mTextSoundFrequency, (DreamPlayer*)tInstance, &freq, 7);
		setDolmexicaStoryTextSoundFrequency(tInstance, id, freq);
	}

	handleSingleStoryTextSpriteOrAnimation(id, e->mHasBackgroundSprite, &e->mBackgroundSprite, setDolmexicaStoryTextBackground, e->mHasBackgroundAnimation, &e->mBackgroundAnimation, setDolmexicaStoryTextBackground, &e->mBackgroundOffset, DOLMEXICA_STORY_TEXT_BACKGROUND_DEFAULT_Z_DELTA, &e->mBackgroundScale, tInstance);
	handleSingleStoryTextSpriteOrAnimation(id, e->mHasFaceSprite, &e->mFaceSprite, setDolmexicaStoryTextFace, e->mHasFaceAnimation, &e->mFaceAnimation, setDolmexicaStoryTextFace, &e->mFaceOffset, DOLMEXICA_STORY_TEXT_FACE_DEFAULT_Z_DELTA, &e->mFaceScale, tInstance);
	handleSingleStoryTextSpriteOrAnimation(id, e->mHasContinueSprite, &e->mContinueSprite, setDolmexicaStoryTextContinue, e->mHasContinueAnimation, &e->mContinueAnimation, setDolmexicaStoryTextContinue, &e->mContinueOffset, DOLMEXICA_STORY_TEXT_CONTINUE_DEFAULT_Z_DELTA, &e->mContinueScale, tInstance);
	handleSingleStoryTextSound(id, e->mHasContinueSound, &e->mContinueSound, setDolmexicaStoryTextContinueSound, tInstance);
	handleSingleStoryTextName(id, e->mHasName, &e->mName, &e->mNameFont, &e->mNameOffset, &e->mNameScale, tInstance);

	int buildUp;
	evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mIsBuildingUp, (DreamPlayer*)tInstance, &buildUp, 1);

	if (e->mHasNextState) {
		int nextState = evaluateDreamAssignmentAndReturnAsInteger(&e->mNextState, (DreamPlayer*)tInstance);
		setDolmexicaStoryTextNextState(tInstance, id, nextState);
	}

	if (!buildUp) {
		setDolmexicaStoryTextBuiltUp(tInstance, id);
	}

	if (isEmpty) {
		setDolmexicaStoryTextInactive(tInstance, id);
	}

	return 0;
}

static int handleRemoveTextStoryController(DreamMugenStateController* tController, StoryInstance* tInstance) {
	RemoveElementStoryController* e = (RemoveElementStoryController*)tController->mData;

	int id;
	id = getDolmexicaStoryIDFromAssignment(&e->mID, tInstance);
	removeDolmexicaStoryText(tInstance, id);

	return 0;
}

static StoryInstance* getTargetInstanceFromAssignment(DreamMugenAssignment** tAssignment, StoryInstance* tInstance) {
	string target;
	evaluateDreamAssignmentAndReturnAsString(target, tAssignment, (DreamPlayer*)tInstance);
	int targetNumber;
	if (target == "root") {
		targetNumber = -1;
	}
	else {
		targetNumber = atoi(target.data());
	}
	return getDolmexicaStoryHelperInstance(targetNumber);
}

static int handleChangeTextStoryController(DreamMugenStateController* tController, StoryInstance* tInstance) {
	ChangeTextStoryController* e = (ChangeTextStoryController*)tController->mData;

	int id;
	id = getDolmexicaStoryIDFromAssignment(&e->mID, tInstance);

	StoryInstance* targetInstance = tInstance;
	if (e->mHasTarget) {
		targetInstance = getTargetInstanceFromAssignment(&e->mTarget, tInstance);
	}
	if (e->mDoesChangePosition) {
		const auto offset = evaluateDreamAssignmentAndReturnAsVector2D(&e->mPosition, (DreamPlayer*)tInstance);
		setDolmexicaStoryTextBasePosition(targetInstance, id, offset);
	}
	if (e->mDoesChangeText) {
		string text;
		evaluateDreamAssignmentAndReturnAsString(text, &e->mText, (DreamPlayer*)tInstance);
		setDolmexicaStoryTextText(targetInstance, id, text.data());
	}
	if (e->mDoesChangeFont) {
		const auto font = evaluateDreamAssignmentAndReturnAsVector3DI(&e->mFont, (DreamPlayer*)tInstance);
		setDolmexicaStoryTextFont(targetInstance, id, font);
	}
	if (e->mDoesChangeTextScale) {
		const auto scale = evaluateDreamAssignmentAndReturnAsFloat(&e->mTextScale, (DreamPlayer*)tInstance);
		setDolmexicaStoryTextScale(targetInstance, id, scale);
	}
	if (e->mDoesChangeTextSound) {
		const auto snd = evaluateDreamAssignmentAndReturnAsVector2DI(&e->mTextSound, (DreamPlayer*)tInstance);
		setDolmexicaStoryTextSound(targetInstance, id, snd);
	}
	if (e->mDoesChangeTextSoundFrequency) {
		const auto freq = evaluateDreamAssignmentAndReturnAsInteger(&e->mTextSoundFrequency, (DreamPlayer*)tInstance);
		setDolmexicaStoryTextSoundFrequency(targetInstance, id, freq);
	}
	if (e->mDoesChangeTextOffset) {
		const auto offset = evaluateDreamAssignmentAndReturnAsVector2D(&e->mTextOffset, (DreamPlayer*)tInstance);
		setDolmexicaStoryTextTextOffset(targetInstance, id, offset);
	}
	if (e->mDoesChangeBackgroundSprite) {
		const auto sprite = evaluateDreamAssignmentAndReturnAsVector2DI(&e->mBackgroundSprite, (DreamPlayer*)tInstance);
		setDolmexicaStoryTextBackgroundSprite(targetInstance, id, sprite);
	}
	if (e->mDoesChangeBackgroundOffset) {
		Position offset;
		evaluateDreamAssignmentAndReturnAsThreeFloatsWithDefaultValues(&e->mBackgroundOffset, (DreamPlayer*)tInstance, &offset.x, &offset.y, &offset.z, 0, 0, DOLMEXICA_STORY_TEXT_BACKGROUND_DEFAULT_Z_DELTA);
		setDolmexicaStoryTextBackgroundOffset(targetInstance, id, offset);
	}
	if (e->mDoesChangeBackgroundScale) {
		const auto scale = evaluateDreamAssignmentAndReturnAsVector2D(&e->mBackgroundScale, (DreamPlayer*)tInstance);
		setDolmexicaStoryTextBackgroundScale(targetInstance, id, scale);
	}
	if (e->mDoesChangeFaceSprite) {
		const auto sprite = evaluateDreamAssignmentAndReturnAsVector2DI(&e->mFaceSprite, (DreamPlayer*)tInstance);
		setDolmexicaStoryTextFaceSprite(targetInstance, id, sprite);
	}
	if (e->mDoesChangeFaceOffset) {
		Position offset;
		evaluateDreamAssignmentAndReturnAsThreeFloatsWithDefaultValues(&e->mFaceOffset, (DreamPlayer*)tInstance, &offset.x, &offset.y, &offset.z, 0, 0, DOLMEXICA_STORY_TEXT_FACE_DEFAULT_Z_DELTA);
		setDolmexicaStoryTextFaceOffset(targetInstance, id, offset);
	}
	if (e->mDoesChangeFaceScale) {
		const auto scale = evaluateDreamAssignmentAndReturnAsVector2D(&e->mFaceScale, (DreamPlayer*)tInstance);
		setDolmexicaStoryTextFaceScale(targetInstance, id, scale);
	}
	if (e->mDoesChangeContinueAnimation) {
		int anim = evaluateDreamAssignmentAndReturnAsInteger(&e->mContinueAnimation, (DreamPlayer*)tInstance);
		setDolmexicaStoryTextContinueAnimation(targetInstance, id, anim);
	}
	if (e->mDoesChangeContinueSound) {
		const auto snd = evaluateDreamAssignmentAndReturnAsVector2DI(&e->mContinueSound, (DreamPlayer*)tInstance);
		setDolmexicaStoryTextContinueSound(targetInstance, id, snd);
	}
	if (e->mDoesChangeContinueOffset) {
		Position offset;
		evaluateDreamAssignmentAndReturnAsThreeFloatsWithDefaultValues(&e->mContinueOffset, (DreamPlayer*)tInstance, &offset.x, &offset.y, &offset.z, 0, 0, DOLMEXICA_STORY_TEXT_CONTINUE_DEFAULT_Z_DELTA);
		setDolmexicaStoryTextContinueOffset(targetInstance, id, offset);
	}
	if (e->mDoesChangeContinueScale) {
		const auto scale = evaluateDreamAssignmentAndReturnAsVector2D(&e->mContinueScale, (DreamPlayer*)tInstance);
		setDolmexicaStoryTextContinueScale(targetInstance, id, scale);
	}
	if (e->mDoesChangeName) {
		string text;
		evaluateDreamAssignmentAndReturnAsString(text, &e->mName, (DreamPlayer*)tInstance);
		setDolmexicaStoryTextNameText(targetInstance, id, text.data());
	}
	if (e->mDoesChangeNameFont) {
		const auto font = evaluateDreamAssignmentAndReturnAsVector3DI(&e->mNameFont, (DreamPlayer*)tInstance);
		setDolmexicaStoryTextNameFont(targetInstance, id, font);
	}
	if (e->mDoesChangeNameOffset) {
		const auto offset = evaluateDreamAssignmentAndReturnAsVector2D(&e->mNameOffset, (DreamPlayer*)tInstance);
		setDolmexicaStoryTextNameOffset(targetInstance, id, offset);
	}
	if (e->mDoesChangeNameScale) {
		const auto scale = evaluateDreamAssignmentAndReturnAsFloat(&e->mNameScale, (DreamPlayer*)tInstance);
		setDolmexicaStoryTextNameScale(targetInstance, id, scale);
	}
	if (e->mDoesChangeNextState) {
		int nextState = evaluateDreamAssignmentAndReturnAsInteger(&e->mNextState, (DreamPlayer*)tInstance);
		setDolmexicaStoryTextNextState(targetInstance, id, nextState);
	}

	int buildUp;
	evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mIsBuildingUp, (DreamPlayer*)tInstance, &buildUp, 1);
	if (!buildUp) {
		setDolmexicaStoryTextBuiltUp(targetInstance, id);
	}

	return 0;
}

static int handleChangeStateStoryController(DreamMugenStateController* tController, StoryInstance* tInstance) {
	SingleRequiredValueController* e = (SingleRequiredValueController*)tController->mData;
	
	int val = evaluateDreamAssignmentAndReturnAsInteger(&e->mValue, (DreamPlayer*)tInstance);
	changeDolmexicaStoryState(tInstance, val);

	return 1;
}

static int handleChangeStateParentStoryController(DreamMugenStateController* tController, StoryInstance* tInstance) {
	SingleRequiredValueController* e = (SingleRequiredValueController*)tController->mData;

	int val = evaluateDreamAssignmentAndReturnAsInteger(&e->mValue, (DreamPlayer*)tInstance);
	changeDolmexicaStoryStateOutsideStateHandler(getDolmexicaStoryInstanceParent(tInstance), val);
	return 0;
}

static int handleChangeStateRootStoryController(DreamMugenStateController* tController, StoryInstance* tInstance) {
	SingleRequiredValueController* e = (SingleRequiredValueController*)tController->mData;

	int val = evaluateDreamAssignmentAndReturnAsInteger(&e->mValue, (DreamPlayer*)tInstance);
	changeDolmexicaStoryStateOutsideStateHandler(getDolmexicaStoryRootInstance(), val);
	return 0;
}

static int handleFadeInStoryController(DreamMugenStateController* tController, StoryInstance* tInstance) {
	FadeStoryController* e = (FadeStoryController*)tController->mData;

	double duration;
	evaluateDreamAssignmentAndReturnAsOneFloatWithDefaultValue(&e->mDuration, (DreamPlayer*)tInstance, &duration, 20);
	addFadeIn(duration, NULL, NULL);

	return 0;
}

static int handleFadeOutStoryController(DreamMugenStateController* tController, StoryInstance* tInstance) {
	FadeStoryController* e = (FadeStoryController*)tController->mData;

	double duration;
	Vector3D color;
	evaluateDreamAssignmentAndReturnAsOneFloatWithDefaultValue(&e->mDuration, (DreamPlayer*)tInstance, &duration, 20);
	color = evaluateDreamAssignmentAndReturnAsVector3D(&e->mColor, (DreamPlayer*)tInstance);
	setFadeColorRGB(color.x, color.y, color.z);
	addFadeOut(duration, NULL, NULL);

	return 0;
}

static int handleGotoStoryStepStoryController(DreamMugenStateController* tController, StoryInstance* tInstance) {
	SingleRequiredValueController* e = (SingleRequiredValueController*)tController->mData;
	int newStep = evaluateDreamAssignmentAndReturnAsInteger(&e->mValue, (DreamPlayer*)tInstance);
	endDolmexicaStoryboard(tInstance, newStep);

	return 0;
}

static int handleGotoIntroStoryController() {
	playIntroStoryboard();
	return 0;
}

static int handleGotoTitleStoryController() {
	setNewScreen(getDreamTitleScreen());
	return 0;
}

static int handleAnimationSetPositionStoryController(DreamMugenStateController* tController, StoryInstance* tInstance) {
	StoryTarget2DPhysicsController* e = (StoryTarget2DPhysicsController*)tController->mData;

	int id;
	id = getDolmexicaStoryIDFromAssignment(&e->mID, tInstance);

	StoryInstance* targetInstance = tInstance;
	if (e->mHasTarget) {
		targetInstance = getTargetInstanceFromAssignment(&e->mTarget, tInstance);
	}

	if (e->mIsSettingX) {
		double x = evaluateDreamAssignmentAndReturnAsFloat(&e->x, (DreamPlayer*)tInstance);
		setDolmexicaStoryAnimationPositionX(targetInstance, id, x);
	}

	if (e->mIsSettingY) {
		double y = evaluateDreamAssignmentAndReturnAsFloat(&e->y, (DreamPlayer*)tInstance);
		setDolmexicaStoryAnimationPositionY(targetInstance, id, y);
	}
	return 0;
}

static int handleAnimationAddPositionStoryController(DreamMugenStateController* tController, StoryInstance* tInstance) {
	StoryTarget2DPhysicsController* e = (StoryTarget2DPhysicsController*)tController->mData;

	int id;
	id = getDolmexicaStoryIDFromAssignment(&e->mID, tInstance);

	StoryInstance* targetInstance = tInstance;
	if (e->mHasTarget) {
		targetInstance = getTargetInstanceFromAssignment(&e->mTarget, tInstance);
	}

	if (e->mIsSettingX) {
		double x = evaluateDreamAssignmentAndReturnAsFloat(&e->x, (DreamPlayer*)tInstance);
		addDolmexicaStoryAnimationPositionX(targetInstance, id, x);
	}

	if (e->mIsSettingY) {
		double y = evaluateDreamAssignmentAndReturnAsFloat(&e->y, (DreamPlayer*)tInstance);
		addDolmexicaStoryAnimationPositionY(targetInstance, id, y);
	}
	return 0;
}

static int handleAnimationSetStagePositionStoryController(DreamMugenStateController* tController, StoryInstance* tInstance) {
	StoryTarget2DPhysicsController* e = (StoryTarget2DPhysicsController*)tController->mData;

	int id;
	id = getDolmexicaStoryIDFromAssignment(&e->mID, tInstance);

	StoryInstance* targetInstance = tInstance;
	if (e->mHasTarget) {
		targetInstance = getTargetInstanceFromAssignment(&e->mTarget, tInstance);
	}

	if (e->mIsSettingX) {
		double x = evaluateDreamAssignmentAndReturnAsFloat(&e->x, (DreamPlayer*)tInstance);
		setDolmexicaStoryAnimationStagePositionX(targetInstance, id, x);
	}

	if (e->mIsSettingY) {
		double y = evaluateDreamAssignmentAndReturnAsFloat(&e->y, (DreamPlayer*)tInstance);
		setDolmexicaStoryAnimationStagePositionY(targetInstance, id, y);
	}
	return 0;
}

static int handleAnimationSetScaleStoryController(DreamMugenStateController* tController, StoryInstance* tInstance) {
	StoryTarget2DPhysicsController* e = (StoryTarget2DPhysicsController*)tController->mData;

	int id;
	id = getDolmexicaStoryIDFromAssignment(&e->mID, tInstance);

	StoryInstance* targetInstance = tInstance;
	if (e->mHasTarget) {
		targetInstance = getTargetInstanceFromAssignment(&e->mTarget, tInstance);
	}

	if (e->mIsSettingX) {
		double x = evaluateDreamAssignmentAndReturnAsFloat(&e->x, (DreamPlayer*)tInstance);
		setDolmexicaStoryAnimationScaleX(targetInstance, id, x);
	}

	if (e->mIsSettingY) {
		double y = evaluateDreamAssignmentAndReturnAsFloat(&e->y, (DreamPlayer*)tInstance);
		setDolmexicaStoryAnimationScaleY(targetInstance, id, y);
	}
	return 0;
}

static int handleAnimationSetFaceDirectionStoryController(DreamMugenStateController* tController, StoryInstance* tInstance) {
	AnimationSetFaceDirectionStoryController* e = (AnimationSetFaceDirectionStoryController*)tController->mData;

	int id, faceDirection;
	id = getDolmexicaStoryIDFromAssignment(&e->mID, tInstance);
	evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mFacing, (DreamPlayer*)tInstance, &faceDirection, 1);

	if (!faceDirection) return 0;

	StoryInstance* targetInstance = tInstance;
	if (e->mHasTarget) {
		targetInstance = getTargetInstanceFromAssignment(&e->mTarget, tInstance);
	}

	setDolmexicaStoryAnimationIsFacingRight(targetInstance, id, faceDirection == 1);

	return 0;
}

static int handleAnimationSetAngleStoryController(DreamMugenStateController* tController, StoryInstance* tInstance) {
	AnimationSetFaceDirectionStoryController* e = (AnimationSetFaceDirectionStoryController*)tController->mData;

	int id;
	double angle;
	id = getDolmexicaStoryIDFromAssignment(&e->mID, tInstance);
	evaluateDreamAssignmentAndReturnAsOneFloatWithDefaultValue(&e->mFacing, (DreamPlayer*)tInstance, &angle, 0);

	setDolmexicaStoryAnimationAngle(tInstance, id, angle);

	return 0;
}

static int handleAnimationAddAngleStoryController(DreamMugenStateController* tController, StoryInstance* tInstance) {
	AnimationSetFaceDirectionStoryController* e = (AnimationSetFaceDirectionStoryController*)tController->mData;

	int id;
	double angle;
	id = getDolmexicaStoryIDFromAssignment(&e->mID, tInstance);
	evaluateDreamAssignmentAndReturnAsOneFloatWithDefaultValue(&e->mFacing, (DreamPlayer*)tInstance, &angle, 0);

	addDolmexicaStoryAnimationAngle(tInstance, id, angle);

	return 0;
}

static int handleAnimationSetColorStoryController(DreamMugenStateController* tController, StoryInstance* tInstance) {
	AnimationSetSingleValueStoryController* e = (AnimationSetSingleValueStoryController*)tController->mData;

	int id;
	Vector3D color;
	id = getDolmexicaStoryIDFromAssignment(&e->mID, tInstance);
	color = evaluateDreamAssignmentAndReturnAsVector3D(&e->mValue, (DreamPlayer*)tInstance);

	StoryInstance* targetInstance = tInstance;
	if (e->mHasTarget) {
		targetInstance = getTargetInstanceFromAssignment(&e->mTarget, tInstance);
	}
	setDolmexicaStoryAnimationColor(targetInstance, id, color);

	return 0;
}

static int handleAnimationSetOpacityStoryController(DreamMugenStateController* tController, StoryInstance* tInstance) {
	AnimationSetSingleValueStoryController* e = (AnimationSetSingleValueStoryController*)tController->mData;

	int id;
	double opacity;
	id = getDolmexicaStoryIDFromAssignment(&e->mID, tInstance);
	opacity = evaluateDreamAssignmentAndReturnAsFloat(&e->mValue, (DreamPlayer*)tInstance);

	StoryInstance* targetInstance = tInstance;
	if (e->mHasTarget) {
		targetInstance = getTargetInstanceFromAssignment(&e->mTarget, tInstance);
	}
	setDolmexicaStoryAnimationOpacity(targetInstance, id, opacity);

	return 0;
}

static int handleEndStoryboardController(DreamMugenStateController* tController, StoryInstance* tInstance) {
	SingleRequiredValueController* e = (SingleRequiredValueController*)tController->mData;

	int nextState;
	evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mValue, (DreamPlayer*)tInstance, &nextState, 1);

	endDolmexicaStoryboard(tInstance, nextState);

	return 0;
}

static int handleMoveStageStoryController(DreamMugenStateController* tController, StoryInstance* tInstance) {
	Set2DPhysicsController* e = (Set2DPhysicsController*)tController->mData;

	if (e->mIsSettingX) {
		double x = evaluateDreamAssignmentAndReturnAsFloat(&e->x, (DreamPlayer*)tInstance);
		addDreamMugenStageHandlerCameraPositionX(x);
	}

	if (e->mIsSettingY) {
		double y = evaluateDreamAssignmentAndReturnAsFloat(&e->y, (DreamPlayer*)tInstance);
		addDreamMugenStageHandlerCameraPositionY(y);
	}
	return 0;
}

static int handleCreateCharacterStoryController(DreamMugenStateController* tController, StoryInstance* tInstance) {
	CreateCharacterStoryController* e = (CreateCharacterStoryController*)tController->mData;

	int id, animation, isBoundToStage, preferredPalette;
	Position2D position;
	id = getDolmexicaStoryIDFromAssignment(&e->mID, tInstance);
	evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mStartAnimationNumber, (DreamPlayer*)tInstance, &animation, 0);
	evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mIsBoundToStage, (DreamPlayer*)tInstance, &isBoundToStage, 0);
	evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mPreferredPalette, (DreamPlayer*)tInstance, &preferredPalette, 1);
	evaluateDreamAssignmentAndReturnAsTwoFloatsWithDefaultValues(&e->mPosition, (DreamPlayer*)tInstance, &position.x, &position.y, 0, 0);
	string name;
	evaluateDreamAssignmentAndReturnAsString(name, &e->mName, (DreamPlayer*)tInstance);

	addDolmexicaStoryCharacter(tInstance, id, name.data(), preferredPalette, animation, position);
	if (e->mHasShadow) {
		double shadowBasePosition;
		evaluateDreamAssignmentAndReturnAsOneFloatWithDefaultValue(&e->mShadowBasePositionY, (DreamPlayer*)tInstance, &shadowBasePosition, 0);
		setDolmexicaStoryCharacterShadow(tInstance, id, shadowBasePosition);
	}
	setDolmexicaStoryCharacterBoundToStage(tInstance, id, isBoundToStage);
	

	return 0;
}

static int handleRemoveCharacterStoryController(DreamMugenStateController* tController, StoryInstance* tInstance) {
	RemoveElementStoryController* e = (RemoveElementStoryController*)tController->mData;
	
	int id;
	id = getDolmexicaStoryIDFromAssignment(&e->mID, tInstance);

	removeDolmexicaStoryCharacter(tInstance, id);

	return 0;
}

static int handleChangeCharacterAnimStoryController(DreamMugenStateController* tController, StoryInstance* tInstance) {
	ChangeAnimationStoryController* e = (ChangeAnimationStoryController*)tController->mData;

	int id, animation;
	id = getDolmexicaStoryIDFromAssignment(&e->mID, tInstance);
	evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mAnimation, (DreamPlayer*)tInstance, &animation, 0);

	if (e->mHasTarget) {
		tInstance = getTargetInstanceFromAssignment(&e->mTarget, tInstance);
	}

	changeDolmexicaStoryCharacterAnimation(tInstance, id, animation);

	return 0;
}

static int handleSetCharacterPosStoryController(DreamMugenStateController* tController, StoryInstance* tInstance) {
	StoryTarget3DPhysicsController* e = (StoryTarget3DPhysicsController*)tController->mData;

	int id;
	id = getDolmexicaStoryIDFromAssignment(&e->mID, tInstance);

	StoryInstance* targetInstance = tInstance;
	if (e->mHasTarget) {
		targetInstance = getTargetInstanceFromAssignment(&e->mTarget, tInstance);
	}

	if (e->mIsSettingX) {
		double x = evaluateDreamAssignmentAndReturnAsFloat(&e->x, (DreamPlayer*)tInstance);
		setDolmexicaStoryCharacterPositionX(targetInstance, id, x);
	}

	if (e->mIsSettingY) {
		double y = evaluateDreamAssignmentAndReturnAsFloat(&e->y, (DreamPlayer*)tInstance);
		setDolmexicaStoryCharacterPositionY(targetInstance, id, y);
	}

	if (e->mIsSettingZ) {
		double z = evaluateDreamAssignmentAndReturnAsFloat(&e->z, (DreamPlayer*)tInstance);
		setDolmexicaStoryCharacterPositionZ(targetInstance, id, z);
	}
	return 0;
}

static int handleAddCharacterPosStoryController(DreamMugenStateController* tController, StoryInstance* tInstance) {
	StoryTarget2DPhysicsController* e = (StoryTarget2DPhysicsController*)tController->mData;

	int id;
	id = getDolmexicaStoryIDFromAssignment(&e->mID, tInstance);

	StoryInstance* targetInstance = tInstance;
	if (e->mHasTarget) {
		targetInstance = getTargetInstanceFromAssignment(&e->mTarget, tInstance);
	}

	if (e->mIsSettingX) {
		double x = evaluateDreamAssignmentAndReturnAsFloat(&e->x, (DreamPlayer*)tInstance);
		addDolmexicaStoryCharacterPositionX(targetInstance, id, x);
	}

	if (e->mIsSettingY) {
		double y = evaluateDreamAssignmentAndReturnAsFloat(&e->y, (DreamPlayer*)tInstance);
		addDolmexicaStoryCharacterPositionY(targetInstance, id, y);
	}
	return 0;
}

static int handleSetCharacterStagePositionStoryController(DreamMugenStateController* tController, StoryInstance* tInstance) {
	StoryTarget2DPhysicsController* e = (StoryTarget2DPhysicsController*)tController->mData;

	int id;
	id = getDolmexicaStoryIDFromAssignment(&e->mID, tInstance);

	StoryInstance* targetInstance = tInstance;
	if (e->mHasTarget) {
		targetInstance = getTargetInstanceFromAssignment(&e->mTarget, tInstance);
	}

	if (e->mIsSettingX) {
		double x = evaluateDreamAssignmentAndReturnAsFloat(&e->x, (DreamPlayer*)tInstance);
		setDolmexicaStoryCharacterStagePositionX(targetInstance, id, x);
	}

	if (e->mIsSettingY) {
		double y = evaluateDreamAssignmentAndReturnAsFloat(&e->y, (DreamPlayer*)tInstance);
		setDolmexicaStoryCharacterStagePositionY(targetInstance, id, y);
	}
	return 0;
}

static int handleSetCharacterScaleStoryController(DreamMugenStateController* tController, StoryInstance* tInstance) {
	StoryTarget2DPhysicsController* e = (StoryTarget2DPhysicsController*)tController->mData;

	int id;
	id = getDolmexicaStoryIDFromAssignment(&e->mID, tInstance);

	StoryInstance* targetInstance = tInstance;
	if (e->mHasTarget) {
		targetInstance = getTargetInstanceFromAssignment(&e->mTarget, tInstance);
	}

	if (e->mIsSettingX) {
		double x = evaluateDreamAssignmentAndReturnAsFloat(&e->x, (DreamPlayer*)tInstance);
		setDolmexicaStoryCharacterScaleX(targetInstance, id, x);
	}

	if (e->mIsSettingY) {
		double y = evaluateDreamAssignmentAndReturnAsFloat(&e->y, (DreamPlayer*)tInstance);
		setDolmexicaStoryCharacterScaleY(targetInstance, id, y);
	}
	return 0;
}

static int handleSetCharacterFaceDirectionStoryController(DreamMugenStateController* tController, StoryInstance* tInstance) {
	AnimationSetFaceDirectionStoryController* e = (AnimationSetFaceDirectionStoryController*)tController->mData;

	int id, faceDirection;
	id = getDolmexicaStoryIDFromAssignment(&e->mID, tInstance);
	evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mFacing, (DreamPlayer*)tInstance, &faceDirection, 1);

	if (!faceDirection) return 0;

	StoryInstance* targetInstance = tInstance;
	if (e->mHasTarget) {
		targetInstance = getTargetInstanceFromAssignment(&e->mTarget, tInstance);
	}

	setDolmexicaStoryCharacterIsFacingRight(targetInstance, id, faceDirection == 1);
	
	return 0;
}

static int handleSetCharacterColorStoryController(DreamMugenStateController* tController, StoryInstance* tInstance) {
	AnimationSetSingleValueStoryController* e = (AnimationSetSingleValueStoryController*)tController->mData;

	StoryInstance* targetInstance = tInstance;
	if (e->mHasTarget) {
		targetInstance = getTargetInstanceFromAssignment(&e->mTarget, tInstance);
	}

	int id;
	Vector3D color;
	id = getDolmexicaStoryIDFromAssignment(&e->mID, tInstance);
	color = evaluateDreamAssignmentAndReturnAsVector3D(&e->mValue, (DreamPlayer*)tInstance);

	setDolmexicaStoryCharacterColor(targetInstance, id, color);

	return 0;
}

static int handleSetCharacterOpacityStoryController(DreamMugenStateController* tController, StoryInstance* tInstance) {
	AnimationSetSingleValueStoryController* e = (AnimationSetSingleValueStoryController*)tController->mData;

	StoryInstance* targetInstance = tInstance;
	if (e->mHasTarget) {
		targetInstance = getTargetInstanceFromAssignment(&e->mTarget, tInstance);
	}

	int id;
	double opacity;
	id = getDolmexicaStoryIDFromAssignment(&e->mID, tInstance);
	opacity = evaluateDreamAssignmentAndReturnAsFloat(&e->mValue, (DreamPlayer*)tInstance);

	setDolmexicaStoryCharacterOpacity(targetInstance, id, opacity);

	return 0;
}

static int handleSetCharacterAngleStoryController(DreamMugenStateController* tController, StoryInstance* tInstance) {
	AnimationSetFaceDirectionStoryController* e = (AnimationSetFaceDirectionStoryController*)tController->mData;

	int id;
	double angle;

	StoryInstance* targetInstance = tInstance;
	if (e->mHasTarget) {
		targetInstance = getTargetInstanceFromAssignment(&e->mTarget, tInstance);
	}
	id = getDolmexicaStoryIDFromAssignment(&e->mID, tInstance);
	evaluateDreamAssignmentAndReturnAsOneFloatWithDefaultValue(&e->mFacing, (DreamPlayer*)tInstance, &angle, 0);

	setDolmexicaStoryCharacterAngle(targetInstance, id, angle);

	return 0;
}

static int handleAddCharacterAngleStoryController(DreamMugenStateController* tController, StoryInstance* tInstance) {
	AnimationSetFaceDirectionStoryController* e = (AnimationSetFaceDirectionStoryController*)tController->mData;

	int id;
	double angle;

	StoryInstance* targetInstance = tInstance;
	if (e->mHasTarget) {
		targetInstance = getTargetInstanceFromAssignment(&e->mTarget, tInstance);
	}
	id = getDolmexicaStoryIDFromAssignment(&e->mID, tInstance);
	evaluateDreamAssignmentAndReturnAsOneFloatWithDefaultValue(&e->mFacing, (DreamPlayer*)tInstance, &angle, 0);

	addDolmexicaStoryCharacterAngle(targetInstance, id, angle);

	return 0;
}

static int handleCreateHelperStoryController(DreamMugenStateController* tController, StoryInstance* tInstance) {
	CreateHelperStoryController* e = (CreateHelperStoryController*)tController->mData;

	int id, state;
	id = getDolmexicaStoryIDFromAssignment(&e->mID, tInstance);
	evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mState, (DreamPlayer*)tInstance, &state, 0);

	addDolmexicaStoryHelper(id, state, tInstance);

	return 0;
}

static int handleRemoveHelperStoryController(DreamMugenStateController* tController, StoryInstance* tInstance) {
	CreateHelperStoryController* e = (CreateHelperStoryController*)tController->mData;

	int id;
	id = getDolmexicaStoryIDFromAssignment(&e->mID, tInstance);

	removeDolmexicaStoryHelper(id);

	return 0;
}

static void getStoryCharacterAndHelperFromAssignment(DreamMugenAssignment** tCharacterAssignment, StoryInstance* tInstance, int& oCharacter, int& oHelper) {
	string text;
	evaluateDreamAssignmentAndReturnAsString(text, tCharacterAssignment, (DreamPlayer*)tInstance);
	char characterString[100], comma[20], helperString[100];
	int items = sscanf(text.data(), "%s %s %s", characterString, comma, helperString);
	assert(items >= 1);

	if (items == 3) {
		oHelper = atoi(helperString);
	}
	else {
		oHelper = -1;	
	}

	oCharacter = getDolmexicaStoryIDFromString(characterString, tInstance);
}

static int handleLockTextToCharacterStoryController(DreamMugenStateController* tController, StoryInstance* tInstance) {
	LockTextToCharacterStoryController* e = (LockTextToCharacterStoryController*)tController->mData;

	int id;
	int character, helper;
	double dX, dY;
	id = getDolmexicaStoryIDFromAssignment(&e->mID, tInstance);
	getStoryCharacterAndHelperFromAssignment(&e->mCharacterID, tInstance, character, helper);
	evaluateDreamAssignmentAndReturnAsTwoFloatsWithDefaultValues(&e->mOffset, (DreamPlayer*)tInstance, &dX, &dY, 0, 0);

	if (helper == -1) {
		setDolmexicaStoryTextLockToCharacter(tInstance, id, character, Vector2D(dX, dY));
	}
	else {
		setDolmexicaStoryTextLockToCharacter(tInstance, id, character, Vector2D(dX, dY), helper);
	}

	return 0;
}

static int handleTextSetPositionStoryController(DreamMugenStateController* tController, StoryInstance* tInstance) {
	StoryTarget2DPhysicsController* e = (StoryTarget2DPhysicsController*)tController->mData;

	int id;
	id = getDolmexicaStoryIDFromAssignment(&e->mID, tInstance);

	StoryInstance* targetInstance = tInstance;
	if (e->mHasTarget) {
		targetInstance = getTargetInstanceFromAssignment(&e->mTarget, tInstance);
	}

	if (e->mIsSettingX) {
		double x = evaluateDreamAssignmentAndReturnAsFloat(&e->x, (DreamPlayer*)tInstance);
		setDolmexicaStoryTextPositionX(targetInstance, id, x);
	}

	if (e->mIsSettingY) {
		double y = evaluateDreamAssignmentAndReturnAsFloat(&e->y, (DreamPlayer*)tInstance);
		setDolmexicaStoryTextPositionY(targetInstance, id, y);
	}
	return 0;
}

static int handleTextAddPositionStoryController(DreamMugenStateController* tController, StoryInstance* tInstance) {
	StoryTarget2DPhysicsController* e = (StoryTarget2DPhysicsController*)tController->mData;

	int id;
	id = getDolmexicaStoryIDFromAssignment(&e->mID, tInstance);

	StoryInstance* targetInstance = tInstance;
	if (e->mHasTarget) {
		targetInstance = getTargetInstanceFromAssignment(&e->mTarget, tInstance);
	}

	if (e->mIsSettingX) {
		double x = evaluateDreamAssignmentAndReturnAsFloat(&e->x, (DreamPlayer*)tInstance);
		addDolmexicaStoryTextPositionX(targetInstance, id, x);
	}

	if (e->mIsSettingY) {
		double y = evaluateDreamAssignmentAndReturnAsFloat(&e->y, (DreamPlayer*)tInstance);
		addDolmexicaStoryTextPositionY(targetInstance, id, y);
	}
	return 0;
}

static int handleNameIDStoryController(DreamMugenStateController* tController, StoryInstance* tInstance) {
	NameTextStoryController* e = (NameTextStoryController*)tController->mData;

	int id;
	id = getDolmexicaStoryIDFromAssignment(&e->mID, tInstance);

	string name;
	evaluateDreamAssignmentAndReturnAsString(name, &e->mName, (DreamPlayer*)tInstance);
	setDolmexicaStoryIDName(tInstance, id, name);

	return 0;
}

typedef struct {
	StoryInstance* mPlayer;
	StoryInstance* mTarget;

} StoryVarSetHandlingCaller;

static void handleSettingSingleStoryVariable(void* tCaller, void* tData) {
	StoryVarSetHandlingCaller* caller = (StoryVarSetHandlingCaller*)tCaller;
	StoryVarSetControllerEntry* e = (StoryVarSetControllerEntry*)tData;

	int id = evaluateDreamAssignmentAndReturnAsInteger(&e->mID, (DreamPlayer*)caller->mPlayer);

	if (e->mType == STORY_VAR_SET_TYPE_INTEGER) {
		int val = evaluateDreamAssignmentAndReturnAsInteger(&e->mAssignment, (DreamPlayer*)caller->mPlayer);
		setDolmexicaStoryIntegerVariable(caller->mTarget, id, val);
	}
	else if (e->mType == STORY_VAR_SET_TYPE_FLOAT) {
		double val = evaluateDreamAssignmentAndReturnAsFloat(&e->mAssignment, (DreamPlayer*)caller->mPlayer);
		setDolmexicaStoryFloatVariable(caller->mTarget, id, val);
	}
	else if (e->mType == STORY_VAR_SET_TYPE_STRING) {
		string val;
		evaluateDreamAssignmentAndReturnAsString(val, &e->mAssignment, (DreamPlayer*)caller->mPlayer);
		setDolmexicaStoryStringVariable(caller->mTarget, id, val);
	}
	else {
		logWarningFormat("Unrecognized story variable type %d. Ignoring.", e->mType);
	}
}

static int handleSettingStoryVariable(DreamMugenStateController* tController, StoryInstance* tPlayer, StoryInstance* tTarget) {
	if (!tPlayer) return 0;

	StoryVarSetController* e = (StoryVarSetController*)tController->mData;
	StoryVarSetHandlingCaller caller;
	caller.mPlayer = tPlayer;
	caller.mTarget = tTarget;

	vector_map(&e->mStoryVarSets, handleSettingSingleStoryVariable, &caller);

	return 0;
}

static void handleSettingSingleGlobalStoryVariable(void* tCaller, void* tData) {
	StoryVarSetHandlingCaller* caller = (StoryVarSetHandlingCaller*)tCaller;
	StoryVarSetControllerEntry* e = (StoryVarSetControllerEntry*)tData;

	int id = evaluateDreamAssignmentAndReturnAsInteger(&e->mID, (DreamPlayer*)caller->mPlayer);

	if (e->mType == STORY_VAR_SET_TYPE_INTEGER) {
		int val = evaluateDreamAssignmentAndReturnAsInteger(&e->mAssignment, (DreamPlayer*)caller->mPlayer);
		setGlobalVariable(id, val);
	}
	else if (e->mType == STORY_VAR_SET_TYPE_FLOAT) {
		double val = evaluateDreamAssignmentAndReturnAsFloat(&e->mAssignment, (DreamPlayer*)caller->mPlayer);
		setGlobalFloatVariable(id, val);
	}
	else if (e->mType == STORY_VAR_SET_TYPE_STRING) {
		string val;
		evaluateDreamAssignmentAndReturnAsString(val, &e->mAssignment, (DreamPlayer*)caller->mPlayer);
		setGlobalStringVariable(id, val);
	}
	else {
		logWarningFormat("Unrecognized story variable type %d. Ignoring.", e->mType);
	}
}

static int handleSettingGlobalStoryVariable(DreamMugenStateController* tController, StoryInstance* tPlayer) {
	if (!tPlayer) return 0;

	StoryVarSetController* e = (StoryVarSetController*)tController->mData;
	StoryVarSetHandlingCaller caller;
	caller.mPlayer = tPlayer;
	caller.mTarget = NULL;

	vector_map(&e->mStoryVarSets, handleSettingSingleGlobalStoryVariable, &caller);

	return 0;
}

static void handleAddingSingleStoryVariable(void* tCaller, void* tData) {
	StoryVarSetHandlingCaller* caller = (StoryVarSetHandlingCaller*)tCaller;
	StoryVarSetControllerEntry* e = (StoryVarSetControllerEntry*)tData;

	int id = evaluateDreamAssignmentAndReturnAsInteger(&e->mID, (DreamPlayer*)caller->mPlayer);

	if (e->mType == STORY_VAR_SET_TYPE_INTEGER) {
		int val = evaluateDreamAssignmentAndReturnAsInteger(&e->mAssignment, (DreamPlayer*)caller->mPlayer);
		addDolmexicaStoryIntegerVariable(caller->mTarget, id, val);
	}
	else if (e->mType == STORY_VAR_SET_TYPE_FLOAT) {
		double val = evaluateDreamAssignmentAndReturnAsFloat(&e->mAssignment, (DreamPlayer*)caller->mPlayer);
		addDolmexicaStoryFloatVariable(caller->mTarget, id, val);
	}
	else if (e->mType == STORY_VAR_SET_TYPE_STRING) {
		string val;
		evaluateDreamAssignmentAndReturnAsString(val, &e->mAssignment, (DreamPlayer*)caller->mPlayer);
		if (val[0] == '$') {
			int numVal = atoi(val.data() + 1);
			addDolmexicaStoryStringVariable(caller->mTarget, id, numVal);
		}
		else {
			addDolmexicaStoryStringVariable(caller->mTarget, id, val);
		}
	}
	else {
		logWarningFormat("Unrecognized story variable type %d. Ignoring.", e->mType);
	}
}

static int handleAddingStoryVariable(DreamMugenStateController* tController, StoryInstance* tPlayer, StoryInstance* tTarget) {
	if (!tPlayer) return 0;

	StoryVarSetController* e = (StoryVarSetController*)tController->mData;
	StoryVarSetHandlingCaller caller;
	caller.mPlayer = tPlayer;
	caller.mTarget = tTarget;

	vector_map(&e->mStoryVarSets, handleAddingSingleStoryVariable, &caller);

	return 0;
}

static void handleAddingSingleGlobalStoryVariable(void* tCaller, void* tData) {
	StoryVarSetHandlingCaller* caller = (StoryVarSetHandlingCaller*)tCaller;
	StoryVarSetControllerEntry* e = (StoryVarSetControllerEntry*)tData;

	int id = evaluateDreamAssignmentAndReturnAsInteger(&e->mID, (DreamPlayer*)caller->mPlayer);

	if (e->mType == STORY_VAR_SET_TYPE_INTEGER) {
		int val = evaluateDreamAssignmentAndReturnAsInteger(&e->mAssignment, (DreamPlayer*)caller->mPlayer);
		addGlobalVariable(id, val);
	}
	else if (e->mType == STORY_VAR_SET_TYPE_FLOAT) {
		double val = evaluateDreamAssignmentAndReturnAsFloat(&e->mAssignment, (DreamPlayer*)caller->mPlayer);
		addGlobalFloatVariable(id, val);
	}
	else if (e->mType == STORY_VAR_SET_TYPE_STRING) {
		string val;
		evaluateDreamAssignmentAndReturnAsString(val, &e->mAssignment, (DreamPlayer*)caller->mPlayer);
		if (val[0] == '$') {
			int numVal = atoi(val.data() + 1);
			addGlobalStringVariable(id, numVal);
		}
		else {
			addGlobalStringVariable(id, val);
		}
	}
	else {
		logWarningFormat("Unrecognized story variable type %d. Ignoring.", e->mType);
	}
}

static int handleAddingGlobalStoryVariable(DreamMugenStateController* tController, StoryInstance* tPlayer) {
	if (!tPlayer) return 0;

	StoryVarSetController* e = (StoryVarSetController*)tController->mData;
	StoryVarSetHandlingCaller caller;
	caller.mPlayer = tPlayer;
	caller.mTarget = NULL;

	vector_map(&e->mStoryVarSets, handleAddingSingleGlobalStoryVariable, &caller);

	return 0;
}

static int handlePlayMusicStoryController(DreamMugenStateController* tController, StoryInstance* tPlayer) {
	if (!tPlayer) return 0;

	StoryPlayMusicController* e = (StoryPlayMusicController*)tController->mData;
	string path;
	evaluateDreamAssignmentAndReturnAsString(path, &e->mPath, (DreamPlayer*)tPlayer);
	playDolmexicaStoryMusic(path);
	return 0;
}

static int handleStopMusicStoryController(StoryInstance* tPlayer) {
	if (!tPlayer) return 0;
	stopDolmexicaStoryMusic();
	return 0;
}

static int handlePauseMusicStoryController(StoryInstance* tPlayer) {
	if (!tPlayer) return 0;
	pauseDolmexicaStoryMusic();
	return 0;
}

static int handleResumeMusicStoryController(StoryInstance* tPlayer) {
	if (!tPlayer) return 0;
	resumeDolmexicaStoryMusic();
	return 0;
}

static int handleDestroySelfStoryController(StoryInstance* tPlayer) {
	if (!tPlayer) return 0;
	destroyDolmexicaStoryHelper(tPlayer);
	return 0;
}

static int handleCameraFocusStoryController(DreamMugenStateController* tController, StoryInstance* tPlayer) {
	if (!tPlayer) return 0;
	Set2DPhysicsController* e = (Set2DPhysicsController*)tController->mData;
	
	if (e->mIsSettingX) {
		double x = evaluateDreamAssignmentAndReturnAsFloat(&e->x, (DreamPlayer*)tPlayer);
		setDolmexicaStoryCameraFocusX(x);
	}

	if (e->mIsSettingY) {
		double y = evaluateDreamAssignmentAndReturnAsFloat(&e->y, (DreamPlayer*)tPlayer);
		setDolmexicaStoryCameraFocusY(y);
	}
	return 0;
}

static int handleCameraZoomStoryController(DreamMugenStateController* tController, StoryInstance* tPlayer) {
	if (!tPlayer) return 0;
	SingleRequiredValueController* e = (SingleRequiredValueController*)tController->mData;
	double zoom;
	evaluateDreamAssignmentAndReturnAsOneFloatWithDefaultValue(&e->mValue, (DreamPlayer*)tPlayer, &zoom, 1.0);
	setDolmexicaStoryCameraZoom(zoom);

	return 0;
}

static double handleStoryPanningValue(DreamMugenAssignment** tAssignment, StoryInstance* tPlayer, int tIsAbspan) {
	int pan;
	evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(tAssignment, (DreamPlayer*)tPlayer, &pan, 0);
	if (!getSoundAreStereoEffectsActive()) return 0.0;

	double pos;
	const auto screenWidth = getDreamScreenWidth(getDolmexicaStoryCoordinateP());
	if (tIsAbspan) {
		pos = (screenWidth / 2.f) + pan;
	}
	else {
		return 0.0;
	}

	return clamp((pos / screenWidth) * 2.0 - 1.0, -1.0, 1.0) * getSoundPanningWidthFactor();
}

static void handleStorySoundEffectValue(DreamMugenAssignment** tAssignment, StoryInstance* tPlayer, double tPrismVolume, int tChannel, double tFrequencyMultiplier, int tIsLooping, double tPanning) {
	std::string flag;
	evaluateDreamAssignmentAndReturnAsString(flag, tAssignment, (DreamPlayer*)tPlayer);
	int group, item;
	char comma[20];

	const auto soundFile = getDolmexicaStorySounds();
	const auto items = sscanf(flag.data(), "%d %19s %d", &group, comma, &item);
	if (items != 3) {
		logWarningFormat("Unable to parse story sound value: %s. Abort.");
		return;
	}

	tryPlayMugenSoundAdvanced(soundFile, group, item, tPrismVolume, tChannel, tFrequencyMultiplier, tIsLooping, tPanning);
}

static int handlePlaySoundStoryController(DreamMugenStateController* tController, StoryInstance* tPlayer) {
	if (!tPlayer) return 0;
	PlaySoundController* e = (PlaySoundController*)tController->mData;

	int channel, isLowPriority, loop;
	double frequencyMultiplier;
	evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mChannel, (DreamPlayer*)tPlayer, &channel, -1);
	evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mLowPriority, (DreamPlayer*)tPlayer, &isLowPriority, 0);

	if (channel != -1 && isSoundEffectPlayingOnChannel(channel) && isLowPriority) {
		return 0;
	}

	auto prismVolume = parseGameMidiVolumeToPrism(getGameMidiVolume());
	if (e->mIsVolumeScale) {
		double volumeScale;
		evaluateDreamAssignmentAndReturnAsOneFloatWithDefaultValue(&e->mVolume, (DreamPlayer*)tPlayer, &volumeScale, 100.0);
		prismVolume *= (volumeScale / 100.0);
	}
	else {
		int volume;
		evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mVolume, (DreamPlayer*)tPlayer, &volume, 0);
		prismVolume *= ((volume + 100) / double(100));
	}

	evaluateDreamAssignmentAndReturnAsOneFloatWithDefaultValue(&e->mFrequencyMultiplier, (DreamPlayer*)tPlayer, &frequencyMultiplier, 1.0);
	evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mLoop, (DreamPlayer*)tPlayer, &loop, 0);
	const auto panning = handleStoryPanningValue(&e->mPan, tPlayer, e->mIsAbspan);

	handleStorySoundEffectValue(&e->mValue, tPlayer, prismVolume, channel, frequencyMultiplier, loop, panning);
	return 0;
}

static int handleSoundPanStoryController(DreamMugenStateController* tController, StoryInstance* tPlayer) {
	SoundPanController* e = (SoundPanController*)tController->mData;

	int channel;
	evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mChannel, (DreamPlayer*)tPlayer, &channel, -1);
	const auto panning = handleStoryPanningValue(&e->mPan, tPlayer, e->mIsAbspan);
	panSoundEffect(channel, panning);

	return 0;
}

static int handleStopSoundStoryController(DreamMugenStateController* tController, StoryInstance* tPlayer) {
	SoundStopController* e = (SoundStopController*)tController->mData;

	int channel;
	evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(&e->mChannel, (DreamPlayer*)tPlayer, &channel, -1);
	if (channel == -1) {
		stopAllSoundEffects();
	}
	else {
		stopSoundEffect(channel);
	}

	return 0;
}

int nullStoryHandleFunction(DreamMugenStateController* /*tController*/, DreamPlayer* /*tPlayer*/) { return handleNull(); }
int createAnimationStoryHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleCreateAnimationStoryController(tController, (StoryInstance*)tPlayer); }
int removeAnimationStoryHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleRemoveAnimationStoryController(tController, (StoryInstance*)tPlayer); }
int changeAnimationStoryHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleChangeAnimationStoryController(tController, (StoryInstance*)tPlayer); }
int createTextStoryHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleCreateTextStoryController(tController, (StoryInstance*)tPlayer); }
int removeTextStoryHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleRemoveTextStoryController(tController, (StoryInstance*)tPlayer); }
int changeTextStoryHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleChangeTextStoryController(tController, (StoryInstance*)tPlayer); }
int lockTextToCharacterStoryHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleLockTextToCharacterStoryController(tController, (StoryInstance*)tPlayer); }
int textSetPositionStoryHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleTextSetPositionStoryController(tController, (StoryInstance*)tPlayer); }
int textAddPositionStoryHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleTextAddPositionStoryController(tController, (StoryInstance*)tPlayer); }
int nameIDStoryHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleNameIDStoryController(tController, (StoryInstance*)tPlayer); }
int changeStateStoryHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleChangeStateStoryController(tController, (StoryInstance*)tPlayer); }
int changeStateParentStoryHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleChangeStateParentStoryController(tController, (StoryInstance*)tPlayer); }
int changeStateRootStoryHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleChangeStateRootStoryController(tController, (StoryInstance*)tPlayer); }
int fadeInStoryHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleFadeInStoryController(tController, (StoryInstance*)tPlayer); }
int fadeOutStoryHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleFadeOutStoryController(tController, (StoryInstance*)tPlayer); }
int gotoStoryStepStoryHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleGotoStoryStepStoryController(tController, (StoryInstance*)tPlayer); }
int gotoIntroStoryHandleFunction(DreamMugenStateController* /*tController*/, DreamPlayer* /*tPlayer*/) { return handleGotoIntroStoryController(); }
int gotoTitleStoryHandleFunction(DreamMugenStateController* /*tController*/, DreamPlayer* /*tPlayer*/) { return handleGotoTitleStoryController(); }
int animationSetPositionStoryHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleAnimationSetPositionStoryController(tController, (StoryInstance*)tPlayer); }
int animationAddPositionStoryHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleAnimationAddPositionStoryController(tController, (StoryInstance*)tPlayer); }
int animationSetStagePositionStoryHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleAnimationSetStagePositionStoryController(tController, (StoryInstance*)tPlayer); }
int animationSetScaleStoryHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleAnimationSetScaleStoryController(tController, (StoryInstance*)tPlayer); }
int animationSetFaceDirectionStoryHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleAnimationSetFaceDirectionStoryController(tController, (StoryInstance*)tPlayer); }
int animationSetAngleStoryHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleAnimationSetAngleStoryController(tController, (StoryInstance*)tPlayer); }
int animationAddAngleStoryHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleAnimationAddAngleStoryController(tController, (StoryInstance*)tPlayer); }
int animationSetColorStoryHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleAnimationSetColorStoryController(tController, (StoryInstance*)tPlayer); }
int animationSetOpacityStoryHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleAnimationSetOpacityStoryController(tController, (StoryInstance*)tPlayer); }
int endStoryboardStoryHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleEndStoryboardController(tController, (StoryInstance*)tPlayer); }
int moveStageStoryHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleMoveStageStoryController(tController, (StoryInstance*)tPlayer); }
int createCharacterStoryHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleCreateCharacterStoryController(tController, (StoryInstance*)tPlayer); }
int removeCharacterStoryHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleRemoveCharacterStoryController(tController, (StoryInstance*)tPlayer); }
int changeCharacterAnimStoryHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleChangeCharacterAnimStoryController(tController, (StoryInstance*)tPlayer); }
int setCharacterPosStoryHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleSetCharacterPosStoryController(tController, (StoryInstance*)tPlayer); }
int addCharacterPosStoryHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleAddCharacterPosStoryController(tController, (StoryInstance*)tPlayer); }
int setCharacterStagePositionStoryHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleSetCharacterStagePositionStoryController(tController, (StoryInstance*)tPlayer); }
int setCharacterScaleStoryHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleSetCharacterScaleStoryController(tController, (StoryInstance*)tPlayer); }
int setCharacterFaceDirectionStoryHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleSetCharacterFaceDirectionStoryController(tController, (StoryInstance*)tPlayer); }
int setCharacterColorStoryHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleSetCharacterColorStoryController(tController, (StoryInstance*)tPlayer); }
int setCharacterOpacityStoryHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleSetCharacterOpacityStoryController(tController, (StoryInstance*)tPlayer); }
int setCharacterAngleStoryHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleSetCharacterAngleStoryController(tController, (StoryInstance*)tPlayer); }
int addCharacterAngleStoryHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleAddCharacterAngleStoryController(tController, (StoryInstance*)tPlayer); }
int createHelperStoryHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleCreateHelperStoryController(tController, (StoryInstance*)tPlayer); }
int removeHelperStoryHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleRemoveHelperStoryController(tController, (StoryInstance*)tPlayer); }
int setVarStoryHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleSettingStoryVariable(tController, (StoryInstance*)tPlayer, (StoryInstance*)tPlayer); }
int addVarStoryHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleAddingStoryVariable(tController, (StoryInstance*)tPlayer, (StoryInstance*)tPlayer); }
int setParentVarStoryHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleSettingStoryVariable(tController, (StoryInstance*)tPlayer, getDolmexicaStoryInstanceParent((StoryInstance*)tPlayer)); }
int addParentVarStoryHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleAddingStoryVariable(tController, (StoryInstance*)tPlayer, getDolmexicaStoryInstanceParent((StoryInstance*)tPlayer)); }
int setGlobalVarStoryHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleSettingGlobalStoryVariable(tController, (StoryInstance*)tPlayer); }
int addGlobalVarStoryHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleAddingGlobalStoryVariable(tController, (StoryInstance*)tPlayer); }
int playMusicStoryHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handlePlayMusicStoryController(tController, (StoryInstance*)tPlayer); }
int stopMusicStoryHandleFunction(DreamMugenStateController* /*tController*/, DreamPlayer* tPlayer) { return handleStopMusicStoryController((StoryInstance*)tPlayer); }
int pauseMusicStoryHandleFunction(DreamMugenStateController* /*tController*/, DreamPlayer* tPlayer) { return handlePauseMusicStoryController((StoryInstance*)tPlayer); }
int resumeMusicStoryHandleFunction(DreamMugenStateController* /*tController*/, DreamPlayer* tPlayer) { return handleResumeMusicStoryController((StoryInstance*)tPlayer); }
int destroySelfStoryHandleFunction(DreamMugenStateController* /*tController*/, DreamPlayer* tPlayer) { return handleDestroySelfStoryController((StoryInstance*)tPlayer); }
int cameraFocusStoryHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleCameraFocusStoryController(tController, (StoryInstance*)tPlayer); }
int cameraZoomStoryHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleCameraZoomStoryController(tController, (StoryInstance*)tPlayer); }
int playSoundStoryHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handlePlaySoundStoryController(tController, (StoryInstance*)tPlayer); }
int panSoundStoryHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleSoundPanStoryController(tController, (StoryInstance*)tPlayer); }
int stopSoundStoryHandleFunction(DreamMugenStateController* tController, DreamPlayer* tPlayer) { return handleStopSoundStoryController(tController, (StoryInstance*)tPlayer); }

static void setupStoryStateControllerHandlers() {
	gMugenStateControllerVariableHandler.mStateControllerHandlers.clear();
	
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STORY_STATE_CONTROLLER_TYPE_NULL] = nullStoryHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STORY_STATE_CONTROLLER_TYPE_CREATE_ANIMATION] = createAnimationStoryHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STORY_STATE_CONTROLLER_TYPE_REMOVE_ANIMATION] = removeAnimationStoryHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STORY_STATE_CONTROLLER_TYPE_CHANGE_ANIMATION] = changeAnimationStoryHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STORY_STATE_CONTROLLER_TYPE_CREATE_TEXT] = createTextStoryHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STORY_STATE_CONTROLLER_TYPE_REMOVE_TEXT] = removeTextStoryHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STORY_STATE_CONTROLLER_TYPE_CHANGE_TEXT] = changeTextStoryHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STORY_STATE_CONTROLLER_TYPE_LOCK_TEXT_TO_CHARACTER] = lockTextToCharacterStoryHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STORY_STATE_CONTROLLER_TYPE_TEXT_SET_POSITION] = textSetPositionStoryHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STORY_STATE_CONTROLLER_TYPE_TEXT_ADD_POSITION] = textAddPositionStoryHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STORY_STATE_CONTROLLER_TYPE_NAME_ID] = nameIDStoryHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STORY_STATE_CONTROLLER_TYPE_CHANGE_STATE] = changeStateStoryHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STORY_STATE_CONTROLLER_TYPE_CHANGE_STATE_PARENT] = changeStateParentStoryHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STORY_STATE_CONTROLLER_TYPE_CHANGE_STATE_ROOT] = changeStateRootStoryHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STORY_STATE_CONTROLLER_TYPE_FADE_IN] = fadeInStoryHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STORY_STATE_CONTROLLER_TYPE_FADE_OUT] = fadeOutStoryHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STORY_STATE_CONTROLLER_TYPE_GOTO_STORY_STEP] = gotoStoryStepStoryHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STORY_STATE_CONTROLLER_TYPE_GOTO_INTRO] = gotoIntroStoryHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STORY_STATE_CONTROLLER_TYPE_GOTO_TITLE] = gotoTitleStoryHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STORY_STATE_CONTROLLER_TYPE_ANIMATION_SET_POSITION] = animationSetPositionStoryHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STORY_STATE_CONTROLLER_TYPE_ANIMATION_ADD_POSITION] = animationAddPositionStoryHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STORY_STATE_CONTROLLER_TYPE_ANIMATION_SET_STAGE_POSITION] = animationSetStagePositionStoryHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STORY_STATE_CONTROLLER_TYPE_ANIMATION_SET_SCALE] = animationSetScaleStoryHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STORY_STATE_CONTROLLER_TYPE_ANIMATION_SET_FACEDIRECTION] = animationSetFaceDirectionStoryHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STORY_STATE_CONTROLLER_TYPE_ANIMATION_SET_ANGLE] = animationSetAngleStoryHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STORY_STATE_CONTROLLER_TYPE_ANIMATION_ADD_ANGLE] = animationAddAngleStoryHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STORY_STATE_CONTROLLER_TYPE_ANIMATION_SET_COLOR] = animationSetColorStoryHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STORY_STATE_CONTROLLER_TYPE_ANIMATION_SET_OPACITY] = animationSetOpacityStoryHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STORY_STATE_CONTROLLER_TYPE_END_STORYBOARD] = endStoryboardStoryHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STORY_STATE_CONTROLLER_TYPE_MOVE_STAGE] = moveStageStoryHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STORY_STATE_CONTROLLER_TYPE_CREATE_CHARACTER] = createCharacterStoryHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STORY_STATE_CONTROLLER_TYPE_REMOVE_CHARACTER] = removeCharacterStoryHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STORY_STATE_CONTROLLER_TYPE_CHARACTER_CHANGE_ANIMATION] = changeCharacterAnimStoryHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STORY_STATE_CONTROLLER_TYPE_CHARACTER_SET_POSITION] = setCharacterPosStoryHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STORY_STATE_CONTROLLER_TYPE_CHARACTER_ADD_POSITION] = addCharacterPosStoryHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STORY_STATE_CONTROLLER_TYPE_CHARACTER_SET_STAGE_POSITION] = setCharacterStagePositionStoryHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STORY_STATE_CONTROLLER_TYPE_CHARACTER_SET_SCALE] = setCharacterScaleStoryHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STORY_STATE_CONTROLLER_TYPE_CHARACTER_SET_FACEDIRECTION] = setCharacterFaceDirectionStoryHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STORY_STATE_CONTROLLER_TYPE_CHARACTER_SET_COLOR] = setCharacterColorStoryHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STORY_STATE_CONTROLLER_TYPE_CHARACTER_SET_OPACITY] = setCharacterOpacityStoryHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STORY_STATE_CONTROLLER_TYPE_CHARACTER_SET_ANGLE] = setCharacterAngleStoryHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STORY_STATE_CONTROLLER_TYPE_CHARACTER_ADD_ANGLE] = addCharacterAngleStoryHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STORY_STATE_CONTROLLER_TYPE_CREATE_HELPER] = createHelperStoryHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STORY_STATE_CONTROLLER_TYPE_REMOVE_HELPER] = removeHelperStoryHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STORY_STATE_CONTROLLER_TYPE_VAR_SET] = setVarStoryHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STORY_STATE_CONTROLLER_TYPE_VAR_ADD] = addVarStoryHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STORY_STATE_CONTROLLER_TYPE_PARENT_VAR_SET] = setParentVarStoryHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STORY_STATE_CONTROLLER_TYPE_PARENT_VAR_ADD] = addParentVarStoryHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STORY_STATE_CONTROLLER_TYPE_GLOBAL_VAR_SET] = setGlobalVarStoryHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STORY_STATE_CONTROLLER_TYPE_GLOBAL_VAR_ADD] = addGlobalVarStoryHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STORY_STATE_CONTROLLER_TYPE_PLAY_MUSIC] = playMusicStoryHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STORY_STATE_CONTROLLER_TYPE_STOP_MUSIC] = stopMusicStoryHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STORY_STATE_CONTROLLER_TYPE_PAUSE_MUSIC] = pauseMusicStoryHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STORY_STATE_CONTROLLER_TYPE_RESUME_MUSIC] = resumeMusicStoryHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STORY_STATE_CONTROLLER_TYPE_DESTROY_SELF] = destroySelfStoryHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STORY_STATE_CONTROLLER_TYPE_CAMERA_FOCUS] = cameraFocusStoryHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STORY_STATE_CONTROLLER_TYPE_CAMERA_ZOOM] = cameraZoomStoryHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STORY_STATE_CONTROLLER_TYPE_PLAY_SOUND] = playSoundStoryHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STORY_STATE_CONTROLLER_TYPE_PAN_SOUND] = panSoundStoryHandleFunction;
	gMugenStateControllerVariableHandler.mStateControllerHandlers[MUGEN_STORY_STATE_CONTROLLER_TYPE_STOP_SOUND] = stopSoundStoryHandleFunction;
}

void setupDreamMugenStoryStateControllerHandler()
{
	setupStoryStateControllerParsers();
	setupStoryStateControllerHandlers();
}

void shutdownDreamMugenStateControllerHandler()
{
	gMugenStateControllerVariableHandler.mStateControllerParsers.clear();
	gMugenStateControllerVariableHandler.mStateControllerHandlers.clear();
	gMugenStateControllerVariableHandler.mStateControllerUnloaders.clear();
	gMugenStateControllerVariableHandler.mMemoryStack = NULL;
}

#ifdef _WIN32
#include <imgui/imgui.h>
#include "prism/windows/debugimgui_win.h"

static const char* MUGEN_STORY_STATE_CONTROLLER_NAMES[] = { 
	"Null", 
	"CreateAnim", 
	"RemoveAnim", 
	"ChangeAnim",
	"CreateText",
	"RemoveText",
	"ChangeText",
	"LockText",
	"TextPosSet",
	"TextPosAdd",
	"NameID",
	"ChangeState",
	"ChangeStateParent",
	"ChangeStateRoot",
	"FadeIn",
	"FadeOut",
	"GoToStoryStep",
	"GoToIntro",
	"GoToTitle",
	"AnimPosSet",
	"AnimPosAdd",
	"AnimStagePosSet",
	"AnimScaleSet",
	"AnimSetFacing",
	"AnimSetAngle",
	"AnimAddAngle",
	"AnimSetColor",
	"AnimSetOpacity",
	"EndStoryboard",
	"MoveStage",
	"CreateChar",
	"RemoveChar",
	"CharChangeAnim",
	"CharPosSet",
	"CharPosAdd",
	"CharStagePosSet",
	"CharScaleSet",
	"CharSetFacing",
	"CharSetColor",
	"CharSetOpacity",
	"CharSetAngle",
	"CharAddAngle",
	"CreateHelper",
	"RemoveHelper",
	"VarSet",
	"VarAdd",
	"ParentVarSet",
	"ParentVarAdd",
	"GlobalVarSet",
	"GlobalVarAdd",
	"PlayMusic",
	"StopMusic",	
	"PauseMusic",
	"ResumeMusic",
	"DestroySelf",
	"CameraFocus",
	"CameraZoom",
	"PlaySnd",
	"SndPan",
	"StopSnd",
};

static const char* MUGEN_STATE_CONTROLLER_TARGETS[] = {
	"Default",
	"Player1",
	"Player2",
};

static void imguiMugenStoryStateControllerChangeText(void* tData, const std::string_view& tScriptPath, const std::string_view& tGroupName, size_t tGroupOffset)
{
	auto e = (ChangeTextStoryController*)tData;
	ImGui::Text("DoesChangeText = %d", e->mDoesChangeText);
	if (e->mDoesChangeText)
	{
		imguiDreamAssignment("Text", &e->mText, tScriptPath, tGroupName, tGroupOffset);
	}
}

static void imguiMugenStoryStateControllerData(DreamMugenStateControllerType tType, void* tData, const std::string_view& tScriptPath, const std::string_view& tGroupName, size_t tGroupOffset)
{
	if (tType == MUGEN_STORY_STATE_CONTROLLER_TYPE_NULL)
	{
		return;
	}
	else if (tType == MUGEN_STORY_STATE_CONTROLLER_TYPE_CHANGE_TEXT)
	{
		imguiMugenStoryStateControllerChangeText(tData, tScriptPath, tGroupName, tGroupOffset);
	}
}

void imguiMugenStateController(int tIndex, void* tData, const std::string_view& tScriptPath, const std::string_view& tGroupName)
{
	auto controllerBase = (DreamMugenStateController*)tData;
	if (ImGui::TreeNode((std::to_string(tIndex) + " - " + MUGEN_STORY_STATE_CONTROLLER_NAMES[int(controllerBase->mType)]).c_str()))
	{
		int type = int(controllerBase->mType);
		ImGui::Combo("Type", &type, MUGEN_STORY_STATE_CONTROLLER_NAMES, IM_ARRAYSIZE(MUGEN_STORY_STATE_CONTROLLER_NAMES));
		controllerBase->mType = uint8_t(type);
		int target = int(controllerBase->mTarget);
		ImGui::Combo("Target", &target, MUGEN_STATE_CONTROLLER_TARGETS, IM_ARRAYSIZE(MUGEN_STATE_CONTROLLER_TARGETS));
		controllerBase->mTarget = uint8_t(target);
		ImGui::Text("Persistence = %d", controllerBase->mPersistence);
		ImGui::Text("AccessAmount = %d", controllerBase->mAccessAmount);

		imguiDreamAssignment("Trigger", &controllerBase->mTrigger.mAssignment);

		imguiMugenStoryStateControllerData(DreamMugenStateControllerType(controllerBase->mType), controllerBase->mData, tScriptPath, tGroupName, size_t(tIndex));

		ImGui::TreePop();
	}
}
#endif