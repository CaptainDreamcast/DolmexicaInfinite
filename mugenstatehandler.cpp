#include "mugenstatehandler.h"

#include <assert.h>

#include <prism/datastructures.h>
#include <prism/system.h>
#include <prism/stlutil.h>
#include <prism/log.h>

#include "playerdefinition.h"
#include "pausecontrollers.h"
#include "mugenassignmentevaluator.h"
#include "mugenstatecontrollers.h"
#include "playerhitdata.h"

using namespace std;

typedef struct {
	DreamMugenStates* mStates;
	int mIsUsingTemporaryOtherStateMachine;
	DreamMugenStates* mTemporaryStates;

	int mPreviousState;
	int mState;
	int mTimeInState;
	DreamPlayer* mPlayer;

	int mIsPaused;
	int mIsInHelperMode;
	int mIsInputControlDisabled;
	int mIsDisabled;
	int mWasUpdatedOutsideHandler;

	int mCurrentJugglePoints;

	double mTimeDilatationNow;
	double mTimeDilatation;
} RegisteredState;

static struct {
	map<int, RegisteredState> mRegisteredStates;
	int mIsInStoryMode;
	int mActiveCoordinateP;
} gMugenStateHandlerData;

static void loadStateHandler(void* tData) {
	(void)tData;
	setProfilingSectionMarkerCurrentFunction();
	gMugenStateHandlerData.mRegisteredStates.clear();

	setActiveStateMachineCoordinateP(320);
}

static void unloadStateHandler(void* tData) {
	(void)tData;
	setProfilingSectionMarkerCurrentFunction();
	gMugenStateHandlerData.mRegisteredStates.clear();
}

typedef struct {
	RegisteredState* mRegisteredState;
	DreamMugenState* mState;

	int mHasChangedState;
} MugenStateControllerCaller;

static int evaluateTrigger(DreamMugenStateControllerTrigger* tTrigger, DreamPlayer* tPlayer) {
	setProfilingSectionMarkerCurrentFunction();
	return evaluateDreamAssignment(&tTrigger->mAssignment, tPlayer);
}

static void updateSingleController(void* tCaller, void* tData) {
	setProfilingSectionMarkerCurrentFunction();
	MugenStateControllerCaller* caller = (MugenStateControllerCaller*)tCaller;
	DreamMugenStateController* controller = (DreamMugenStateController*)tData;
	
	if (!gMugenStateHandlerData.mIsInStoryMode && caller->mRegisteredState->mPlayer && isPlayerDestroyed(caller->mRegisteredState->mPlayer)) return;
	if (caller->mHasChangedState) return;
	if (!evaluateTrigger(&controller->mTrigger, caller->mRegisteredState->mPlayer)) return;

	controller->mAccessAmount++;
	int testValue = controller->mAccessAmount - 1;
	if (controller->mPersistence) {
		if (testValue % controller->mPersistence != 0) return;
	}
	else {
		if (testValue) return;
	}

	caller->mHasChangedState = handleDreamMugenStateControllerAndReturnWhetherStateChanged(controller, caller->mRegisteredState->mPlayer);
}

static DreamMugenStates* getCurrentStateMachineStates(RegisteredState* tRegisteredState) {
	if (tRegisteredState->mIsUsingTemporaryOtherStateMachine) {
		return tRegisteredState->mTemporaryStates;
	}
	else {
		return tRegisteredState->mStates;
	}
}

static void updateSingleState(RegisteredState* tRegisteredState, int tState, int tForceOwnStates) {
	setProfilingSectionMarkerCurrentFunction();
	if (!gMugenStateHandlerData.mIsInStoryMode && tRegisteredState->mPlayer && (!isPlayer(tRegisteredState->mPlayer) || isPlayerDestroyed(tRegisteredState->mPlayer))) return;

	set<int> visitedStates;
	
	int isEvaluating = 1;
	while (isEvaluating) {
		if (!gMugenStateHandlerData.mIsInStoryMode) {
			if (tRegisteredState->mIsUsingTemporaryOtherStateMachine && !tForceOwnStates) {
				setActiveStateMachineCoordinateP(getPlayerCoordinateP(getPlayerOtherPlayer(tRegisteredState->mPlayer)));
			}
			else {
				setActiveStateMachineCoordinateP(getPlayerCoordinateP(tRegisteredState->mPlayer));
			}
		}
		DreamMugenStates* states = tForceOwnStates ? tRegisteredState->mStates : getCurrentStateMachineStates(tRegisteredState);
		if (!stl_map_contains(states->mStates, tState)) break;
		visitedStates.insert(tState);
		DreamMugenState* state = &states->mStates[tState];
		MugenStateControllerCaller caller;
		caller.mRegisteredState = tRegisteredState;
		caller.mState = state;
		caller.mHasChangedState = 0;
		vector_map(&state->mControllers, updateSingleController, &caller);
		
		if (!caller.mHasChangedState) break;
		else {
			if (tState < 0) break;
			if (stl_set_contains(visitedStates, tRegisteredState->mState)) {
				tRegisteredState->mTimeInState--;
				break;
			}
			tState = tRegisteredState->mState;
		}
	}
}

static int updateSingleStateMachineByReference(RegisteredState* tRegisteredState) {
	setProfilingSectionMarkerCurrentFunction();
	if (tRegisteredState->mIsPaused) return 0;

	tRegisteredState->mTimeInState++;
	if (!tRegisteredState->mIsInHelperMode) {
		if (!tRegisteredState->mIsUsingTemporaryOtherStateMachine) {
			updateSingleState(tRegisteredState, -3, 1);
		}
		updateSingleState(tRegisteredState, -2, 1);
	}
	if (!tRegisteredState->mIsInputControlDisabled) {
		updateSingleState(tRegisteredState, -1, 1);
	}
	updateSingleState(tRegisteredState, tRegisteredState->mState, 0);

	return !gMugenStateHandlerData.mIsInStoryMode && tRegisteredState->mPlayer && isPlayerDestroyed(tRegisteredState->mPlayer);
}

static int updateSingleStateMachine(void* tCaller, RegisteredState& tData) {
	(void)tCaller;
	setProfilingSectionMarkerCurrentFunction();
	RegisteredState* registeredState = &tData;
	registeredState->mTimeDilatationNow += registeredState->mTimeDilatation;
	int updateAmount = (int)registeredState->mTimeDilatationNow;
	registeredState->mTimeDilatationNow -= updateAmount;
	while (updateAmount--) {
		int ret;
		if (!registeredState->mWasUpdatedOutsideHandler) {
			ret = updateSingleStateMachineByReference(registeredState);
		}
		else {
			registeredState->mWasUpdatedOutsideHandler = 0;
			ret = 0;
		}
		if (ret) return 1;
	}
	return 0;
}

static void updateStateHandler(void* tData) {
	(void)tData;
	setProfilingSectionMarkerCurrentFunction();
	stl_int_map_remove_predicate(gMugenStateHandlerData.mRegisteredStates, updateSingleStateMachine);
}

ActorBlueprint getDreamMugenStateHandler() {
	return makeActorBlueprint(loadStateHandler, unloadStateHandler, updateStateHandler);
}

int registerDreamMugenStateMachine(DreamMugenStates * tStates, DreamPlayer* tPlayer)
{
	RegisteredState e;
	e.mStates = tStates;
	e.mIsUsingTemporaryOtherStateMachine = 0;
	e.mPreviousState = 0;
	e.mState = 0;
	e.mTimeInState = -1;
	e.mPlayer = tPlayer;
	e.mIsPaused = 0;
	e.mIsInHelperMode = 0;
	e.mIsInputControlDisabled = 0;
	e.mIsDisabled = 0;
	e.mWasUpdatedOutsideHandler = 0;
	e.mCurrentJugglePoints = 0;
	e.mTimeDilatationNow = 0.0;
	e.mTimeDilatation = 1.0;
	return stl_int_map_push_back(gMugenStateHandlerData.mRegisteredStates, e);
}

int registerDreamMugenStoryStateMachine(DreamMugenStates * tStates, StoryInstance* tInstance)
{
	int id = registerDreamMugenStateMachine(tStates, (DreamPlayer*)tInstance);
	setDreamRegisteredStateToHelperMode(id);
	setDreamRegisteredStateDisableCommandState(id);

	return id;
}

void removeDreamRegisteredStateMachine(int tID)
{
	if (!stl_map_contains(gMugenStateHandlerData.mRegisteredStates, tID))
	{
		logWarningFormat("Unable to remove state machine handler %d, does not exist.", tID);
		return;
	}
	gMugenStateHandlerData.mRegisteredStates.erase(tID);
}

int getDreamRegisteredStateState(int tID)
{
	setProfilingSectionMarkerCurrentFunction();
	assert(stl_map_contains(gMugenStateHandlerData.mRegisteredStates, tID));
	RegisteredState* e = &gMugenStateHandlerData.mRegisteredStates[tID];

	return e->mState;
}

int getDreamRegisteredStatePreviousState(int tID)
{
	assert(stl_map_contains(gMugenStateHandlerData.mRegisteredStates, tID));
	RegisteredState* e = &gMugenStateHandlerData.mRegisteredStates[tID];

	return e->mPreviousState;
}

int isDreamRegisteredStateMachinePaused(int tID)
{
	assert(stl_map_contains(gMugenStateHandlerData.mRegisteredStates, tID));
	RegisteredState* e = &gMugenStateHandlerData.mRegisteredStates[tID];
	return e->mIsPaused;
}

void pauseDreamRegisteredStateMachine(int tID)
{
	assert(stl_map_contains(gMugenStateHandlerData.mRegisteredStates, tID));
	RegisteredState* e = &gMugenStateHandlerData.mRegisteredStates[tID];
	e->mIsPaused = 1;
}

void unpauseDreamRegisteredStateMachine(int tID)
{
	assert(stl_map_contains(gMugenStateHandlerData.mRegisteredStates, tID));
	RegisteredState* e = &gMugenStateHandlerData.mRegisteredStates[tID];
	if (e->mIsDisabled) return;
	e->mIsPaused = 0;
}

void setDreamRegisteredStateMachinePauseStatus(int tID, int tIsPaused)
{
	if (tIsPaused) pauseDreamRegisteredStateMachine(tID);
	else unpauseDreamRegisteredStateMachine(tID);
}

void disableDreamRegisteredStateMachine(int tID)
{
	assert(stl_map_contains(gMugenStateHandlerData.mRegisteredStates, tID));
	RegisteredState* e = &gMugenStateHandlerData.mRegisteredStates[tID];
	e->mIsDisabled = 1;
	pauseDreamRegisteredStateMachine(tID);
}

int getDreamRegisteredStateJugglePoints(int tID)
{
	assert(stl_map_contains(gMugenStateHandlerData.mRegisteredStates, tID));
	RegisteredState* e = &gMugenStateHandlerData.mRegisteredStates[tID];

	return e->mCurrentJugglePoints;
}

int getDreamRegisteredStateTimeInState(int tID)
{
	assert(stl_map_contains(gMugenStateHandlerData.mRegisteredStates, tID));
	RegisteredState* e = &gMugenStateHandlerData.mRegisteredStates[tID];

	return e->mTimeInState;
}

void setDreamRegisteredStateTimeInState(int tID, int tTime)
{
	assert(stl_map_contains(gMugenStateHandlerData.mRegisteredStates, tID));
	RegisteredState* e = &gMugenStateHandlerData.mRegisteredStates[tID];

	e->mTimeInState = tTime;
}

void setDreamRegisteredStateToHelperMode(int tID)
{
	assert(stl_map_contains(gMugenStateHandlerData.mRegisteredStates, tID));
	RegisteredState* e = &gMugenStateHandlerData.mRegisteredStates[tID];
	e->mIsInHelperMode = 1;
}

void setDreamRegisteredStateDisableCommandState(int tID)
{
	assert(stl_map_contains(gMugenStateHandlerData.mRegisteredStates, tID));
	RegisteredState* e = &gMugenStateHandlerData.mRegisteredStates[tID];
	e->mIsInputControlDisabled = 1;
}

int hasDreamHandledStateMachineState(int tID, int tNewState)
{
	assert(stl_map_contains(gMugenStateHandlerData.mRegisteredStates, tID));
	RegisteredState* e = &gMugenStateHandlerData.mRegisteredStates[tID];
	
	DreamMugenStates* states = getCurrentStateMachineStates(e);
	return stl_map_contains(states->mStates, tNewState);
}

int hasDreamHandledStateMachineStateSelf(int tID, int tNewState)
{
	assert(stl_map_contains(gMugenStateHandlerData.mRegisteredStates, tID));
	RegisteredState* e = &gMugenStateHandlerData.mRegisteredStates[tID];
	return stl_map_contains(e->mStates->mStates, tNewState);
}

int isInOwnStateMachine(int tID)
{
	assert(stl_map_contains(gMugenStateHandlerData.mRegisteredStates, tID));
	RegisteredState* e = &gMugenStateHandlerData.mRegisteredStates[tID];
	return !e->mIsUsingTemporaryOtherStateMachine;
}

static void resetSingleStateController(void* tCaller, void* tData) {
	(void)tCaller;
	DreamMugenStateController* controller = (DreamMugenStateController*)tData;
	controller->mAccessAmount = 0;
}

static void resetStateControllers(DreamMugenState* e) {
	vector_map(&e->mControllers, resetSingleStateController, NULL);
}

void changeDreamHandledStateMachineState(int tID, int tNewState)
{
	setProfilingSectionMarkerCurrentFunction();
	assert(stl_map_contains(gMugenStateHandlerData.mRegisteredStates, tID));
	RegisteredState* e = &gMugenStateHandlerData.mRegisteredStates[tID];
	DreamMugenStates* states = getCurrentStateMachineStates(e);
	if (!stl_map_contains(states->mStates, tNewState)) {
		if (!e->mPlayer || gMugenStateHandlerData.mIsInStoryMode) {
			logWarningFormat("ID %d trying to change into nonexistant state %d. Ignoring.", tID, tNewState);
		}
		else {
			logWarningFormat("Player %d %d trying to change into nonexistant state %d. Ignoring.", e->mPlayer->mRootID, e->mPlayer->mID, tNewState);
		}
		return;
	}

	e->mTimeInState = 0;

	logFormat("%d %d->%d", tID, e->mState, tNewState);

	e->mPreviousState = e->mState;
	e->mState = tNewState;
	
	DreamMugenState* newState = &states->mStates[e->mState];
	resetStateControllers(newState);
	
	if (!e->mPlayer || gMugenStateHandlerData.mIsInStoryMode) return;

	resetPlayerMoveContactCounter(e->mPlayer);
	setPlayerStateType(e->mPlayer, newState->mType);
	setPlayerStateMoveType(e->mPlayer, newState->mMoveType);
	setPlayerPhysics(e->mPlayer, newState->mPhysics);

	int moveHitInfosPersist = hasPrismFlag(newState->mFlags, MUGEN_STATE_PROPERTY_MOVE_HIT_INFO_PERSISTENCE) ? evaluateDreamAssignmentAndReturnAsInteger(&newState->mDoMoveHitInfosPersist, e->mPlayer) : 0;
	if (!moveHitInfosPersist) {
		setPlayerMoveHitReset(e->mPlayer);
	}

	int hitCountPersists = hasPrismFlag(newState->mFlags, MUGEN_STATE_PROPERTY_HIT_COUNT_PERSISTENCE) ? evaluateDreamAssignmentAndReturnAsInteger(&newState->mDoesHitCountPersist, e->mPlayer) : 0;
	if (!hitCountPersists) {
		resetPlayerHitCount(e->mPlayer);
	}

	int hitDefinitionsPersist = hasPrismFlag(newState->mFlags, MUGEN_STATE_PROPERTY_HIT_DEFINITION_PERSISTENCE) ? evaluateDreamAssignmentAndReturnAsInteger(&newState->mDoHitDefinitionsPersist, e->mPlayer) : 0;
	if (!hitDefinitionsPersist) {
		setHitDataInactive(e->mPlayer);
	}

	if (hasPrismFlag(newState->mFlags, MUGEN_STATE_PROPERTY_CHANGING_ANIMATION)) {
		int anim = evaluateDreamAssignmentAndReturnAsInteger(&newState->mAnimation, e->mPlayer);
		changePlayerAnimation(e->mPlayer, anim);
	}

	if (hasPrismFlag(newState->mFlags, MUGEN_STATE_PROPERTY_CHANGING_CONTROL)) {
		int control = evaluateDreamAssignmentAndReturnAsInteger(&newState->mControl, e->mPlayer);
		setPlayerControl(e->mPlayer, control);
	}

	if (hasPrismFlag(newState->mFlags, MUGEN_STATE_PROPERTY_SETTING_VELOCITY)) {
		Vector3D vel = evaluateDreamAssignmentAndReturnAsVector3D(&newState->mVelocity, e->mPlayer);
		setPlayerVelocityX(e->mPlayer, vel.x, getActiveStateMachineCoordinateP());
		setPlayerVelocityY(e->mPlayer, vel.y, getActiveStateMachineCoordinateP());
	}

	if (hasPrismFlag(newState->mFlags, MUGEN_STATE_PROPERTY_ADDING_POWER)) {
		int power = evaluateDreamAssignmentAndReturnAsInteger(&newState->mPowerAdd, e->mPlayer);
		addPlayerPower(e->mPlayer, power);
	}

	if (hasPrismFlag(newState->mFlags, MUGEN_STATE_PROPERTY_JUGGLE_REQUIREMENT)) {
		e->mCurrentJugglePoints = evaluateDreamAssignmentAndReturnAsInteger(&newState->mJuggleRequired, e->mPlayer);
	}

	if (hasPrismFlag(newState->mFlags, MUGEN_STATE_PROPERTY_CHANGING_SPRITE_PRIORITY)) {
		int spritePriority = evaluateDreamAssignmentAndReturnAsInteger(&newState->mSpritePriority, e->mPlayer);
		setPlayerSpritePriority(e->mPlayer, spritePriority);
	}
	
	setPlayerPositionUnfrozen(e->mPlayer);
}

void changeDreamHandledStateMachineStateToOtherPlayerStateMachine(int tID, int tTemporaryID, int tNewState)
{
	assert(stl_map_contains(gMugenStateHandlerData.mRegisteredStates, tID));
	RegisteredState* e = &gMugenStateHandlerData.mRegisteredStates[tID];

	assert(stl_map_contains(gMugenStateHandlerData.mRegisteredStates, tTemporaryID));
	RegisteredState* borrowFromState = &gMugenStateHandlerData.mRegisteredStates[tTemporaryID];

	e->mIsUsingTemporaryOtherStateMachine = 1;
	e->mTemporaryStates = borrowFromState->mStates;
	changeDreamHandledStateMachineState(tID, tNewState);
}

void changeDreamHandledStateMachineStateToOwnStateMachine(int tID, int tNewState)
{
	assert(stl_map_contains(gMugenStateHandlerData.mRegisteredStates, tID));
	RegisteredState* e = &gMugenStateHandlerData.mRegisteredStates[tID];
	e->mIsUsingTemporaryOtherStateMachine = 0;

	changeDreamHandledStateMachineState(tID, tNewState);
}

void changeDreamHandledStateMachineStateToOwnStateMachineWithoutChangingState(int tID)
{
	assert(stl_map_contains(gMugenStateHandlerData.mRegisteredStates, tID));
	RegisteredState* e = &gMugenStateHandlerData.mRegisteredStates[tID];
	e->mIsUsingTemporaryOtherStateMachine = 0;
}

void setDreamHandledStateMachineSpeed(int tID, double tSpeed)
{
	if (!stl_map_contains(gMugenStateHandlerData.mRegisteredStates, tID)) return;
	RegisteredState* e = &gMugenStateHandlerData.mRegisteredStates[tID];
	e->mTimeDilatation = tSpeed;
}

void updateDreamSingleStateMachineByID(int tID) {
	assert(stl_map_contains(gMugenStateHandlerData.mRegisteredStates, tID));
	RegisteredState* e = &gMugenStateHandlerData.mRegisteredStates[tID];
	updateSingleStateMachineByReference(e);
	e->mWasUpdatedOutsideHandler = 1;
}

void setDreamSingleStateMachineToUpdateAgainByID(int tID)
{
	assert(stl_map_contains(gMugenStateHandlerData.mRegisteredStates, tID));
	RegisteredState* e = &gMugenStateHandlerData.mRegisteredStates[tID];
	updateSingleStateMachineByReference(e);
	e->mWasUpdatedOutsideHandler = 0;
}

void setStateMachineHandlerToStory()
{
	gMugenStateHandlerData.mIsInStoryMode = 1;
}

void setStateMachineHandlerToFight()
{
	gMugenStateHandlerData.mIsInStoryMode = 0;
}

int getActiveStateMachineCoordinateP()
{
	return gMugenStateHandlerData.mActiveCoordinateP;
}

void setActiveStateMachineCoordinateP(int tCoordinateP)
{
	gMugenStateHandlerData.mActiveCoordinateP = tCoordinateP;
}
