#include "mugenstatehandler.h"

#include <assert.h>

#include <prism/datastructures.h>
#include <prism/system.h>

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

	int mCurrentJugglePoints;
} RegisteredState;

static struct {
	IntMap mRegisteredStates;

} gData;

static void loadStateHandler(void* tData) {
	(void)tData;
	
	gData.mRegisteredStates = new_int_map();
}

static void unloadStateHandler(void* tData) {
	(void)tData;
	delete_int_map(&gData.mRegisteredStates);
}

typedef struct {
	RegisteredState* mRegisteredState;
	DreamMugenState* mState;

	int mHasChangedState;
} MugenStateControllerCaller;

static int evaluateTrigger(DreamMugenStateControllerTrigger* tTrigger, DreamPlayer* tPlayer) {
	return evaluateDreamAssignment(&tTrigger->mAssignment, tPlayer);
}

static void updateSingleController(void* tCaller, void* tData) {
	MugenStateControllerCaller* caller = (MugenStateControllerCaller*)tCaller;
	DreamMugenStateController* controller = (DreamMugenStateController*)tData;
	
	if (caller->mRegisteredState->mPlayer && isPlayerDestroyed(caller->mRegisteredState->mPlayer)) return;
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
	if (!isPlayer(tRegisteredState->mPlayer) || isPlayerDestroyed(tRegisteredState->mPlayer)) return;

	IntMap visitedStates = new_int_map();
	
	int isEvaluating = 1;
	while (isEvaluating) {
		if (!int_map_contains(&tStates->mStates, tState)) break;
		int_map_push(&visitedStates, tState, NULL);
		DreamMugenState* state = (DreamMugenState*)int_map_get(&tStates->mStates, tState);
		MugenStateControllerCaller caller;
		caller.mRegisteredState = tRegisteredState;
		caller.mState = state;
		caller.mHasChangedState = 0;
		vector_map(&state->mControllers, updateSingleController, &caller);
		
		if (!caller.mHasChangedState) break;
		else {
			if (tState < 0 || int_map_contains(&visitedStates, tRegisteredState->mState)) break;
			tState = tRegisteredState->mState;
		}
	}

	delete_int_map(&visitedStates);
}

static int updateSingleStateMachineByReference(RegisteredState* tRegisteredState) {
	if (tRegisteredState->mIsPaused) return 0;

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

	return tRegisteredState->mPlayer && isPlayerDestroyed(tRegisteredState->mPlayer);
}

static int updateSingleStateMachine(void* tCaller, void* tData) {
	(void)tCaller;
	RegisteredState* registeredState = (RegisteredState*)tData;
	return updateSingleStateMachineByReference(registeredState);
}

static void updateStateHandler(void* tData) {
	(void)tData;
	int_map_remove_predicate(&gData.mRegisteredStates, updateSingleStateMachine, NULL);
}

ActorBlueprint getDreamMugenStateHandler() {
	return makeActorBlueprint(loadStateHandler, unloadStateHandler, updateStateHandler);
}

int registerDreamMugenStateMachine(DreamMugenStates * tStates, DreamPlayer* tPlayer)
{
	RegisteredState* e = (RegisteredState*)allocMemory(sizeof(RegisteredState));
	e->mStates = tStates;
	e->mIsUsingTemporaryOtherStateMachine = 0;
	e->mPreviousState = 0;
	e->mState = 0;
	e->mTimeInState = -1;
	e->mPlayer = tPlayer;
	e->mIsPaused = 0;
	e->mIsInHelperMode = 0;
	e->mIsInputControlDisabled = 0;
	e->mIsDisabled = 0;
	e->mCurrentJugglePoints = 0;

	return int_map_push_back_owned(&gData.mRegisteredStates, e);
}

int registerDreamMugenStoryStateMachine(DreamMugenStates * tStates)
{
	int id = registerDreamMugenStateMachine(tStates, NULL);
	setDreamRegisteredStateToHelperMode(id);
	setDreamRegisteredStateDisableCommandState(id);

	return id;
}

void removeDreamRegisteredStateMachine(int tID)
{
	assert(int_map_contains(&gData.mRegisteredStates, tID));
	int_map_remove(&gData.mRegisteredStates, tID);
}

int getDreamRegisteredStateState(int tID)
{
	assert(int_map_contains(&gData.mRegisteredStates, tID));
	RegisteredState* e = (RegisteredState*)int_map_get(&gData.mRegisteredStates, tID);

	return e->mState;
}

int getDreamRegisteredStatePreviousState(int tID)
{
	assert(int_map_contains(&gData.mRegisteredStates, tID));
	RegisteredState* e = (RegisteredState*)int_map_get(&gData.mRegisteredStates, tID);

	return e->mPreviousState;
}

void pauseDreamRegisteredStateMachine(int tID)
{
	assert(int_map_contains(&gData.mRegisteredStates, tID));
	RegisteredState* e = (RegisteredState*)int_map_get(&gData.mRegisteredStates, tID);
	e->mIsPaused = 1;
}

void unpauseDreamRegisteredStateMachine(int tID)
{
	assert(int_map_contains(&gData.mRegisteredStates, tID));
	RegisteredState* e = (RegisteredState*)int_map_get(&gData.mRegisteredStates, tID);
	if (e->mIsDisabled) return;
	e->mIsPaused = 0;
}

void disableDreamRegisteredStateMachine(int tID)
{
	assert(int_map_contains(&gData.mRegisteredStates, tID));
	RegisteredState* e = (RegisteredState*)int_map_get(&gData.mRegisteredStates, tID);
	e->mIsDisabled = 1;
	pauseDreamRegisteredStateMachine(tID);
}

int getDreamRegisteredStateJugglePoints(int tID)
{
	assert(int_map_contains(&gData.mRegisteredStates, tID));
	RegisteredState* e = (RegisteredState*)int_map_get(&gData.mRegisteredStates, tID);

	return e->mCurrentJugglePoints;
}

int getDreamRegisteredStateTimeInState(int tID)
{
	assert(int_map_contains(&gData.mRegisteredStates, tID));
	RegisteredState* e = (RegisteredState*)int_map_get(&gData.mRegisteredStates, tID);

	return e->mTimeInState;
}

void setDreamRegisteredStateTimeInState(int tID, int tTime)
{
	assert(int_map_contains(&gData.mRegisteredStates, tID));
	RegisteredState* e = (RegisteredState*)int_map_get(&gData.mRegisteredStates, tID);

	e->mTimeInState = tTime;
}

void setDreamRegisteredStateToHelperMode(int tID)
{
	assert(int_map_contains(&gData.mRegisteredStates, tID));
	RegisteredState* e = (RegisteredState*)int_map_get(&gData.mRegisteredStates, tID);
	e->mIsInHelperMode = 1;
}

void setDreamRegisteredStateDisableCommandState(int tID)
{
	assert(int_map_contains(&gData.mRegisteredStates, tID));
	RegisteredState* e = (RegisteredState*)int_map_get(&gData.mRegisteredStates, tID);
	e->mIsInputControlDisabled = 1;
}

int hasDreamHandledStateMachineState(int tID, int tNewState)
{
	assert(int_map_contains(&gData.mRegisteredStates, tID));
	RegisteredState* e = (RegisteredState*)int_map_get(&gData.mRegisteredStates, tID);
	
	DreamMugenStates* states = getCurrentStateMachineStates(e);
	return int_map_contains(&states->mStates, tNewState);
}

int hasDreamHandledStateMachineStateSelf(int tID, int tNewState)
{
	assert(int_map_contains(&gData.mRegisteredStates, tID));
	RegisteredState* e = (RegisteredState*)int_map_get(&gData.mRegisteredStates, tID);
	return int_map_contains(&e->mStates->mStates, tNewState);
}

int isInOwnStateMachine(int tID)
{
	assert(int_map_contains(&gData.mRegisteredStates, tID));
	RegisteredState* e = (RegisteredState*)int_map_get(&gData.mRegisteredStates, tID);
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
	assert(int_map_contains(&gData.mRegisteredStates, tID));
	RegisteredState* e = (RegisteredState*)int_map_get(&gData.mRegisteredStates, tID);
	e->mTimeInState = 0;

	printf("%d %d %d->%d\n", e->mPlayer->mRootID, e->mPlayer->mID, e->mState, tNewState);

	e->mPreviousState = e->mState;
	e->mState = tNewState;

	DreamMugenStates* states = getCurrentStateMachineStates(e);
	assert(int_map_contains(&states->mStates, e->mState));
	
	DreamMugenState* newState = (DreamMugenState*)int_map_get(&states->mStates, e->mState);
	resetStateControllers(newState);
	
	if (!e->mPlayer) return;

	resetPlayerMoveContactCounter(e->mPlayer);
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
		int anim = evaluateDreamAssignmentAndReturnAsInteger(&newState->mAnimation, e->mPlayer);
		changePlayerAnimation(e->mPlayer, anim);
	}

	if (newState->mIsChangingControl) {
		int control = evaluateDreamAssignmentAndReturnAsInteger(&newState->mControl, e->mPlayer);
		setPlayerControl(e->mPlayer, control);
	}

	if (newState->mIsSettingVelocity) {
		Vector3D vel = evaluateDreamAssignmentAndReturnAsVector3D(&newState->mVelocity, e->mPlayer);
		setPlayerVelocityX(e->mPlayer, vel.x, getPlayerCoordinateP(e->mPlayer));
		setPlayerVelocityY(e->mPlayer, vel.y, getPlayerCoordinateP(e->mPlayer));
	}

	if (newState->mIsAddingPower) {
		int power = evaluateDreamAssignmentAndReturnAsInteger(&newState->mPowerAdd, e->mPlayer);
		addPlayerPower(e->mPlayer, power);
	}

	if (newState->mDoesRequireJuggle) {
		e->mCurrentJugglePoints = evaluateDreamAssignmentAndReturnAsInteger(&newState->mJuggleRequired, e->mPlayer);
	}

	if (newState->mIsChangingSpritePriority) {
		int spritePriority = evaluateDreamAssignmentAndReturnAsInteger(&newState->mSpritePriority, e->mPlayer);
		setPlayerSpritePriority(e->mPlayer, spritePriority);
	}

	setPlayerPositionUnfrozen(e->mPlayer); // TODO: check if correct

}

void changeDreamHandledStateMachineStateToOtherPlayerStateMachine(int tID, int tTemporaryID, int tNewState)
{
	assert(int_map_contains(&gData.mRegisteredStates, tID));
	RegisteredState* e = (RegisteredState*)int_map_get(&gData.mRegisteredStates, tID);

	assert(int_map_contains(&gData.mRegisteredStates, tTemporaryID));
	RegisteredState* borrowFromState = (RegisteredState*)int_map_get(&gData.mRegisteredStates, tTemporaryID);

	e->mIsUsingTemporaryOtherStateMachine = 1;
	e->mTemporaryStates = borrowFromState->mStates;
	changeDreamHandledStateMachineState(tID, tNewState);
}

void changeDreamHandledStateMachineStateToOwnStateMachine(int tID, int tNewState)
{
	assert(int_map_contains(&gData.mRegisteredStates, tID));
	RegisteredState* e = (RegisteredState*)int_map_get(&gData.mRegisteredStates, tID);
	e->mIsUsingTemporaryOtherStateMachine = 0;

	changeDreamHandledStateMachineState(tID, tNewState);
}

void changeDreamHandledStateMachineStateToOwnStateMachineWithoutChangingState(int tID)
{
	assert(int_map_contains(&gData.mRegisteredStates, tID));
	RegisteredState* e = (RegisteredState*)int_map_get(&gData.mRegisteredStates, tID);
	e->mIsUsingTemporaryOtherStateMachine = 0;
}

void updateDreamSingleStateMachineByID(int tID) {
	assert(int_map_contains(&gData.mRegisteredStates, tID));
	RegisteredState* e = (RegisteredState*)int_map_get(&gData.mRegisteredStates, tID);
	updateSingleStateMachineByReference(e);
}
