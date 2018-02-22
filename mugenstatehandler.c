#include "mugenstatehandler.h"

#include <assert.h>

#include <prism/datastructures.h>

#include "playerdefinition.h"
#include "pausecontrollers.h"
#include "mugenassignmentevaluator.h"
#include "mugenstatecontrollers.h"
#include "playerhitdata.h"

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
} RegisteredState;

static struct {
	int mPauseHandler;
	IntMap mRegisteredStates;

} gData;

static void loadStateHandler(void* tData) {
	(void)tData;

	gData.mPauseHandler = instantiateActor(DreamPauseHandler);
	
	gData.mRegisteredStates = new_int_map();
}

typedef struct {
	RegisteredState* mRegisteredState;
	DreamMugenState* mState;

	int mHasChangedState;
} MugenStateControllerCaller;

static int evaluateTrigger(DreamMugenStateControllerTrigger* tTrigger, DreamPlayer* tPlayer) {
	return evaluateDreamAssignment(tTrigger->mAssignment, tPlayer);
}

static void updateSingleController(void* tCaller, void* tData) {
	MugenStateControllerCaller* caller = tCaller;
	DreamMugenStateController* controller = tData;
	
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

static void updateSingleState(RegisteredState* tRegisteredState, int tState, DreamMugenStates* tStates) {

	int isEvaluating = 1;
	while (isEvaluating) {
		// printf("%d %d eval: %d\n", tRegisteredState->mPlayer->mRootID, tRegisteredState->mPlayer->mID, tState);
		if (!int_map_contains(&tStates->mStates, tState)) return;
		DreamMugenState* state = int_map_get(&tStates->mStates, tState);
		MugenStateControllerCaller caller;
		caller.mRegisteredState = tRegisteredState;
		caller.mState = state;
		caller.mHasChangedState = 0;
		vector_map(&state->mControllers, updateSingleController, &caller);
		
		if (!caller.mHasChangedState) break;
		else {
			if (tState < 0 || tState == tRegisteredState->mState) break;
			tState = tRegisteredState->mState;
		}
	}
}

static void updateSingleStateMachineByReference(RegisteredState* tRegisteredState) {
	if (tRegisteredState->mIsPaused) return;

	DreamMugenStates* ownStates = tRegisteredState->mStates;
	DreamMugenStates* activeStates = getCurrentStateMachineStates(tRegisteredState);

	tRegisteredState->mTimeInState++;
	if (!tRegisteredState->mIsInHelperMode) {
		if (!tRegisteredState->mIsUsingTemporaryOtherStateMachine) {
			updateSingleState(tRegisteredState, -3, ownStates);
		}
		updateSingleState(tRegisteredState, -2, ownStates);
	}
	if (!tRegisteredState->mIsInputControlDisabled) {
		updateSingleState(tRegisteredState, -1, ownStates);
	}
	updateSingleState(tRegisteredState, tRegisteredState->mState, activeStates);
}

static void updateSingleStateMachine(void* tCaller, void* tData) {
	(void)tCaller;
	RegisteredState* registeredState = tData;
	updateSingleStateMachineByReference(registeredState);
}

static void updateStateHandler(void* tData) {
	(void)tData;
	int_map_map(&gData.mRegisteredStates, updateSingleStateMachine, NULL);
}

ActorBlueprint DreamMugenStateHandler = {
	.mLoad = loadStateHandler,
	.mUpdate = updateStateHandler,
};

int registerDreamMugenStateMachine(DreamMugenStates * tStates, DreamPlayer* tPlayer)
{
	RegisteredState* e = allocMemory(sizeof(RegisteredState));
	e->mStates = tStates;
	e->mIsUsingTemporaryOtherStateMachine = 0;
	e->mPreviousState = 0;
	e->mState = 0;
	e->mTimeInState = 0;
	e->mPlayer = tPlayer;
	e->mIsPaused = 0;
	e->mIsInHelperMode = 0;
	e->mIsInputControlDisabled = 0;
	e->mIsDisabled = 0;

	return int_map_push_back_owned(&gData.mRegisteredStates, e);
}

void removeDreamRegisteredStateMachine(int tID)
{
	assert(int_map_contains(&gData.mRegisteredStates, tID));
	int_map_remove(&gData.mRegisteredStates, tID);
}

int getDreamRegisteredStateState(int tID)
{
	assert(int_map_contains(&gData.mRegisteredStates, tID));
	RegisteredState* e = int_map_get(&gData.mRegisteredStates, tID);

	return e->mState;
}

int getDreamRegisteredStatePreviousState(int tID)
{
	assert(int_map_contains(&gData.mRegisteredStates, tID));
	RegisteredState* e = int_map_get(&gData.mRegisteredStates, tID);

	return e->mPreviousState;
}

void pauseDreamRegisteredStateMachine(int tID)
{
	assert(int_map_contains(&gData.mRegisteredStates, tID));
	RegisteredState* e = int_map_get(&gData.mRegisteredStates, tID);
	e->mIsPaused = 1;
}

void unpauseDreamRegisteredStateMachine(int tID)
{
	assert(int_map_contains(&gData.mRegisteredStates, tID));
	RegisteredState* e = int_map_get(&gData.mRegisteredStates, tID);
	if (e->mIsDisabled) return;
	e->mIsPaused = 0;
}

void disableDreamRegisteredStateMachine(int tID)
{
	assert(int_map_contains(&gData.mRegisteredStates, tID));
	RegisteredState* e = int_map_get(&gData.mRegisteredStates, tID);
	e->mIsDisabled = 1;
	pauseDreamRegisteredStateMachine(tID);
}

int getDreamRegisteredStateTimeInState(int tID)
{
	assert(int_map_contains(&gData.mRegisteredStates, tID));
	RegisteredState* e = int_map_get(&gData.mRegisteredStates, tID);

	// printf("%d %d time in state %d\n", e->mPlayer->mRootID, e->mPlayer->mID, e->mTimeInState);
	return e->mTimeInState;
}

void setDreamRegisteredStateTimeInState(int tID, int tTime)
{
	assert(int_map_contains(&gData.mRegisteredStates, tID));
	RegisteredState* e = int_map_get(&gData.mRegisteredStates, tID);

	e->mTimeInState = tTime;
}

void setDreamRegisteredStateToHelperMode(int tID)
{
	assert(int_map_contains(&gData.mRegisteredStates, tID));
	RegisteredState* e = int_map_get(&gData.mRegisteredStates, tID);
	e->mIsInHelperMode = 1;
}

void setDreamRegisteredStateDisableCommandState(int tID)
{
	assert(int_map_contains(&gData.mRegisteredStates, tID));
	RegisteredState* e = int_map_get(&gData.mRegisteredStates, tID);
	e->mIsInputControlDisabled = 1;
}

int hasDreamHandledStateMachineState(int tID, int tNewState)
{
	assert(int_map_contains(&gData.mRegisteredStates, tID));
	RegisteredState* e = int_map_get(&gData.mRegisteredStates, tID);
	
	DreamMugenStates* states = getCurrentStateMachineStates(e);
	return int_map_contains(&states->mStates, tNewState);
}

int hasDreamHandledStateMachineStateSelf(int tID, int tNewState)
{
	assert(int_map_contains(&gData.mRegisteredStates, tID));
	RegisteredState* e = int_map_get(&gData.mRegisteredStates, tID);
	return int_map_contains(&e->mStates->mStates, tNewState);
}

static void resetSingleStateController(void* tCaller, void* tData) {
	(void)tCaller;
	DreamMugenStateController* controller = tData;
	controller->mAccessAmount = 0;
}

static void resetStateControllers(DreamMugenState* e) {
	vector_map(&e->mControllers, resetSingleStateController, NULL);
}

void changeDreamHandledStateMachineState(int tID, int tNewState)
{
	assert(int_map_contains(&gData.mRegisteredStates, tID));
	RegisteredState* e = int_map_get(&gData.mRegisteredStates, tID);
	resetPlayerMoveContactCounter(e->mPlayer);
	e->mTimeInState = 0;

	e->mPreviousState = e->mState;
	e->mState = tNewState;

	printf("Changing %d %d from state %d to %d\n", e->mPlayer->mRootID, e->mPlayer->mID, e->mPreviousState, e->mState);

	DreamMugenStates* states = getCurrentStateMachineStates(e);
	assert(int_map_contains(&states->mStates, e->mState));
	DreamMugenState* newState = int_map_get(&states->mStates, e->mState);

	setPlayerStateType(e->mPlayer, newState->mType);
	setPlayerStateMoveType(e->mPlayer, newState->mMoveType);
	setPlayerPhysics(e->mPlayer, newState->mPhysics);

	if (!newState->mDoMoveHitInfosPersist) {
		setPlayerMoveHitReset(e->mPlayer);
	}

	if (!newState->mDoesHitCountPersist) {
		resetPlayerHitCount(e->mPlayer);
	}

	if (!newState->mDoHitDefinitionsPersist) {
		setHitDataInactive(e->mPlayer);
	}

	if (newState->mIsChangingAnimation) {
		int anim = evaluateDreamAssignmentAndReturnAsInteger(newState->mAnimation, e->mPlayer);
		changePlayerAnimation(e->mPlayer, anim);
	}

	if (newState->mIsChangingControl) {
		int control = evaluateDreamAssignmentAndReturnAsInteger(newState->mControl, e->mPlayer);
		setPlayerControl(e->mPlayer, control);
	}

	if (newState->mIsSettingVelocity) {
		Vector3D vel = evaluateDreamAssignmentAndReturnAsVector3D(newState->mVelocity, e->mPlayer);
		setPlayerVelocityX(e->mPlayer, vel.x, getPlayerCoordinateP(e->mPlayer));
		setPlayerVelocityY(e->mPlayer, vel.y, getPlayerCoordinateP(e->mPlayer));
	}

	if (newState->mIsAddingPower) {
		int power = evaluateDreamAssignmentAndReturnAsInteger(newState->mPowerAdd, e->mPlayer);
		addPlayerPower(e->mPlayer, power);
	}

	resetStateControllers(newState);
}

void changeDreamHandledStateMachineStateToOtherPlayerStateMachine(int tID, int tTemporaryID, int tNewState)
{
	assert(int_map_contains(&gData.mRegisteredStates, tID));
	RegisteredState* e = int_map_get(&gData.mRegisteredStates, tID);

	assert(int_map_contains(&gData.mRegisteredStates, tTemporaryID));
	RegisteredState* borrowFromState = int_map_get(&gData.mRegisteredStates, tTemporaryID);

	e->mIsUsingTemporaryOtherStateMachine = 1;
	e->mTemporaryStates = borrowFromState->mStates;
	changeDreamHandledStateMachineState(tID, tNewState);
}

void changeDreamHandledStateMachineStateToOwnStateMachine(int tID, int tNewState)
{
	assert(int_map_contains(&gData.mRegisteredStates, tID));
	RegisteredState* e = int_map_get(&gData.mRegisteredStates, tID);
	e->mIsUsingTemporaryOtherStateMachine = 0;

	changeDreamHandledStateMachineState(tID, tNewState);
}

void updateDreamSingleStateMachineByID(int tID) {
	assert(int_map_contains(&gData.mRegisteredStates, tID));
	RegisteredState* e = int_map_get(&gData.mRegisteredStates, tID);
	updateSingleStateMachineByReference(e);
}
