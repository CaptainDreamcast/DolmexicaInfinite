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
} RegisteredState;

static struct {
	map<int, RegisteredState> mRegisteredStates;
	int mIsInStoryMode;
} gMugenStateHandlerData;

static void loadStateHandler(void* tData) {
	(void)tData;
	
	gMugenStateHandlerData.mRegisteredStates.clear();
}

static void unloadStateHandler(void* tData) {
	(void)tData;
	gMugenStateHandlerData.mRegisteredStates.clear();
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
	if (!gMugenStateHandlerData.mIsInStoryMode && tRegisteredState->mPlayer && (!isPlayer(tRegisteredState->mPlayer) || isPlayerDestroyed(tRegisteredState->mPlayer))) return;

	set<int> visitedStates;
	
	int isEvaluating = 1;
	while (isEvaluating) {
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
	RegisteredState* registeredState = &tData;
	if (!registeredState->mWasUpdatedOutsideHandler) {
		return updateSingleStateMachineByReference(registeredState);
	}
	else {
		registeredState->mWasUpdatedOutsideHandler = 0;
		return 0;
	}
}

static void updateStateHandler(void* tData) {
	(void)tData;
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
	assert(stl_map_contains(gMugenStateHandlerData.mRegisteredStates, tID));
	gMugenStateHandlerData.mRegisteredStates.erase(tID);
}

int getDreamRegisteredStateState(int tID)
{
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

	int moveHitInfosPersist = newState->mDoesHaveMoveHitInfosPersist ? evaluateDreamAssignmentAndReturnAsInteger(&newState->mDoMoveHitInfosPersist, e->mPlayer) : 0;
	if (!moveHitInfosPersist) {
		setPlayerMoveHitReset(e->mPlayer);
	}

	int hitCountPersists = newState->mDoesHaveHitCountPersist ? evaluateDreamAssignmentAndReturnAsInteger(&newState->mDoesHitCountPersist, e->mPlayer) : 0;
	if (!hitCountPersists) {
		resetPlayerHitCount(e->mPlayer);
	}

	int hitDefinitionsPersist = newState->mDoesHaveHitDefinitionsPersist ? evaluateDreamAssignmentAndReturnAsInteger(&newState->mDoHitDefinitionsPersist, e->mPlayer) : 0;
	if (!hitDefinitionsPersist) {
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

void updateDreamSingleStateMachineByID(int tID) {
	assert(stl_map_contains(gMugenStateHandlerData.mRegisteredStates, tID));
	RegisteredState* e = &gMugenStateHandlerData.mRegisteredStates[tID];
	updateSingleStateMachineByReference(e);
	e->mWasUpdatedOutsideHandler = 1;
}

void setStateMachineHandlerToStory()
{
	gMugenStateHandlerData.mIsInStoryMode = 1;
}

void setStateMachineHandlerToFight()
{
	gMugenStateHandlerData.mIsInStoryMode = 0;
}
