#include <cstdio>
#include <cstdlib>
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

static struct {
	map<int, RegisteredMugenStateMachine> mRegisteredStates;
	int mActiveCoordinateP;
} gMugenStateHandlerData;

static void loadStateHandler(void* tData) {
	(void)tData;
	setProfilingSectionMarkerCurrentFunction();
	gMugenStateHandlerData.mRegisteredStates.clear();

	setActiveStateMachineCoordinateP(320);
}

int gDolmexicaDeferCount = 0;

static void unloadStateHandler(void* tData) {
	(void)tData;
	setProfilingSectionMarkerCurrentFunction();
	gMugenStateHandlerData.mRegisteredStates.clear();
}

// Imitate Mugen's state machine loop limit
#define STATE_MACHINE_CHANGE_STATE_LOOP_LIMIT 2500

typedef struct {
	RegisteredMugenStateMachine* mRegisteredState;
	DreamMugenState* mState;

	int mIsHitPaused;
	int mHasChangedState;
} MugenStateControllerCaller;

static int isStateMachineInHitPause(RegisteredMugenStateMachine* tRegisteredState) {
	return !tRegisteredState->mIsInStoryMode && tRegisteredState->mPlayer && isPlayer(tRegisteredState->mPlayer) && isPlayerHitPaused(tRegisteredState->mPlayer);
}

static int evaluateTrigger(DreamMugenStateControllerTrigger* tTrigger, DreamPlayer* tPlayer) {
	setProfilingSectionMarkerCurrentFunction();
	return evaluateDreamAssignment(&tTrigger->mAssignment, tPlayer);
}

static void updateSingleController(void* tCaller, void* tData) {
	setProfilingSectionMarkerCurrentFunction();
	MugenStateControllerCaller* caller = (MugenStateControllerCaller*)tCaller;
	DreamMugenStateController* controller = (DreamMugenStateController*)tData;
	
	if (!caller->mRegisteredState->mIsInStoryMode && caller->mRegisteredState->mPlayer && isPlayerDestroyed(caller->mRegisteredState->mPlayer)) return;
	if (caller->mHasChangedState) return;
	if (caller->mIsHitPaused && !controller->mIgnoreHitPause) return;
	if (!evaluateTrigger(&controller->mTrigger, caller->mRegisteredState->mPlayer)) return;

	controller->mAccessAmount++;
	int testValue = controller->mAccessAmount - 1;
	if (controller->mPersistence) {
		if (testValue % controller->mPersistence != 0) return;
	}
	else {
		if (testValue) return;
	}

	DreamPlayer* target;
	switch (controller->mTarget)
	{
	case MUGEN_STATE_CONTROLLER_TARGET_PLAYER1:
		target = getRootPlayer(0);
		break;
	case MUGEN_STATE_CONTROLLER_TARGET_PLAYER2:
		target = getRootPlayer(1);
		break;
	default:
		target = caller->mRegisteredState->mPlayer;
	}

	caller->mHasChangedState = handleDreamMugenStateControllerAndReturnWhetherStateChanged(controller, target);
}

static DreamMugenStates* getCurrentStateMachineStates(RegisteredMugenStateMachine* tRegisteredState) {
	if (tRegisteredState->mIsUsingTemporaryOtherStateMachine) {
		return tRegisteredState->mTemporaryStates;
	}
	else {
		return tRegisteredState->mStates;
	}
}

static void applyRegisteredStateMachineStateChange(RegisteredMugenStateMachine* tRegisteredState, int tNewState);

static void applyRegisteredStateMachinePendingStateChange(RegisteredMugenStateMachine* tRegisteredState) {
	tRegisteredState->mHasPendingStateChange = 0;
	if (tRegisteredState->mIsPendingStateChangeToOwnStates) {
		tRegisteredState->mIsUsingTemporaryOtherStateMachine = 0;
	}
	applyRegisteredStateMachineStateChange(tRegisteredState, tRegisteredState->mPendingStateChange);
}

static void updateSingleState(RegisteredMugenStateMachine* tRegisteredState, int tState, int tForceOwnStates, int tIsHitPaused) {
	setProfilingSectionMarkerCurrentFunction();
	if (!tRegisteredState->mIsInStoryMode && tRegisteredState->mPlayer && (!isPlayer(tRegisteredState->mPlayer) || isPlayerDestroyed(tRegisteredState->mPlayer))) return;

	int changeStateAmount = 0;
	int isEvaluating = 1;
	while (isEvaluating) {
		if (!tRegisteredState->mIsInStoryMode) {
			if (tRegisteredState->mIsUsingTemporaryOtherStateMachine && !tForceOwnStates) {
				setActiveStateMachineCoordinateP(getPlayerCoordinateP(getPlayerOtherPlayer(tRegisteredState->mPlayer)));
			}
			else {
				setActiveStateMachineCoordinateP(getPlayerCoordinateP(tRegisteredState->mPlayer));
			}
		}
		DreamMugenStates* states = tForceOwnStates ? tRegisteredState->mStates : getCurrentStateMachineStates(tRegisteredState);
		if (!stl_map_contains(states->mStates, tState)) break;
		DreamMugenState* state = &states->mStates[tState];
		MugenStateControllerCaller caller;
		caller.mRegisteredState = tRegisteredState;
		caller.mState = state;
		caller.mIsHitPaused = tIsHitPaused;
		caller.mHasChangedState = 0;
		vector_map(&state->mControllers, updateSingleController, &caller);

		if (!caller.mHasChangedState) break;
		else {
			// A state change in a special state aborts the rest of that state, processing only for non-special ones
			if (tState < 0) break;
			// State changes made during a hitpause only take effect once the hitpause ends.
			if (tRegisteredState->mHasPendingStateChange) break;
			changeStateAmount++;
			if (changeStateAmount >= STATE_MACHINE_CHANGE_STATE_LOOP_LIMIT) {
				logWarningFormat("ID %d state machine stuck in loop (stopped after %d loops): %d -> %d. Aborting evaluation for this tick.", tRegisteredState->mID, STATE_MACHINE_CHANGE_STATE_LOOP_LIMIT, tRegisteredState->mPreviousState, tRegisteredState->mState);
				break;
			}
			tState = tRegisteredState->mState;
		}
	}
}

static void updateSingleStateMachineSpecialStates(RegisteredMugenStateMachine* tRegisteredState, int tIsHitPaused)
{
	if (!tRegisteredState->mIsInHelperMode)
	{
		if (!tRegisteredState->mIsUsingTemporaryOtherStateMachine)
		{
			updateSingleState(tRegisteredState, -3, 1, tIsHitPaused);
		}
		updateSingleState(tRegisteredState, -2, 1, tIsHitPaused);
	}
	if (!tRegisteredState->mIsInputControlDisabled && !tRegisteredState->mIsUsingTemporaryOtherStateMachine)
	{
		updateSingleState(tRegisteredState, -1, 1, tIsHitPaused);
	}
}

static void updateSingleStateMachineByReference(RegisteredMugenStateMachine* tRegisteredState, int tDoesIncrementTimeInState) {
	setProfilingSectionMarkerCurrentFunction();
	if (tRegisteredState->mIsPaused) return;
	assert(tRegisteredState->mIsInStoryMode || !isPlayerProjectile(tRegisteredState->mPlayer));

	const auto isHitPaused = isStateMachineInHitPause(tRegisteredState);
	if (!isHitPaused) {
		if (tRegisteredState->mHasPendingStateChange) {
			// A state change deferred during a hitpause takes effect after the pause has finished, starts at 0 for this tick
			applyRegisteredStateMachinePendingStateChange(tRegisteredState);
		}
		else if (tDoesIncrementTimeInState) {
			tRegisteredState->mTimeInState++;
		}
	}

	tRegisteredState->mIsBeingProcessed = 1;
	updateSingleStateMachineSpecialStates(tRegisteredState, isHitPaused);
	updateSingleState(tRegisteredState, tRegisteredState->mState, 0, isHitPaused);
	tRegisteredState->mIsBeingProcessed = 0;
}

static void updateSingleStateMachine(void* tCaller, RegisteredMugenStateMachine& tData) {
	(void)tCaller;
	setProfilingSectionMarkerCurrentFunction();
	RegisteredMugenStateMachine* registeredState = &tData;
	registeredState->mTimeDilatationNow += registeredState->mTimeDilatation;
	int updateAmount = (int)registeredState->mTimeDilatationNow;
	registeredState->mTimeDilatationNow -= updateAmount;
	while (updateAmount--) {
		if (!registeredState->mWasUpdatedOutsideHandler) {
			updateSingleStateMachineByReference(registeredState, 1);
		}
		else {
			registeredState->mWasUpdatedOutsideHandler = 0;
		}
	}
}

static void updateStateHandler(void* tData) {
	(void)tData;
	setProfilingSectionMarkerCurrentFunction();
	stl_int_map_map(gMugenStateHandlerData.mRegisteredStates, updateSingleStateMachine);
}

ActorBlueprint getDreamMugenStateHandler() {
	return makeActorBlueprint(loadStateHandler, unloadStateHandler, updateStateHandler);
}

RegisteredMugenStateMachine* registerDreamMugenStateMachine(DreamMugenStates * tStates, DreamPlayer* tPlayer, int tIsInStoryMode)
{
	auto id = stl_int_map_get_id();
	assert(gMugenStateHandlerData.mRegisteredStates.find(id) == gMugenStateHandlerData.mRegisteredStates.end());
	RegisteredMugenStateMachine& e = gMugenStateHandlerData.mRegisteredStates[id];
	e.mID = id;
	e.mIsInStoryMode = tIsInStoryMode;
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
	e.mIsBeingProcessed = 0;
	e.mHasPendingStateChange = 0;
	e.mPendingStateChange = 0;
	e.mIsPendingStateChangeToOwnStates = 0;
	e.mCurrentJugglePoints = 0;
	e.mTimeDilatationNow = 0.0;
	e.mTimeDilatation = 1.0;
	return &e;
}

RegisteredMugenStateMachine* registerDreamMugenStoryStateMachine(DreamMugenStates * tStates, StoryInstance* tInstance)
{
	auto e = registerDreamMugenStateMachine(tStates, (DreamPlayer*)tInstance, 1);
	setDreamRegisteredStateToHelperMode(e);
	setDreamRegisteredStateDisableCommandState(e);
	return e;
}

void removeDreamRegisteredStateMachine(RegisteredMugenStateMachine* e)
{
	assert(stl_map_contains(gMugenStateHandlerData.mRegisteredStates, e->mID));
	gMugenStateHandlerData.mRegisteredStates.erase(e->mID);
}

int isValidDreamRegisteredStateMachine(RegisteredMugenStateMachine* e)
{
	return stl_map_contains(gMugenStateHandlerData.mRegisteredStates, e->mID);
}

int getDreamRegisteredStateState(RegisteredMugenStateMachine* e)
{
	setProfilingSectionMarkerCurrentFunction();
	assert(stl_map_contains(gMugenStateHandlerData.mRegisteredStates, e->mID));
	return e->mState;
}

int getDreamRegisteredStatePreviousState(RegisteredMugenStateMachine* e)
{
	assert(stl_map_contains(gMugenStateHandlerData.mRegisteredStates, e->mID));
	return e->mPreviousState;
}

int isDreamRegisteredStateMachinePaused(RegisteredMugenStateMachine* e)
{
	assert(stl_map_contains(gMugenStateHandlerData.mRegisteredStates, e->mID));
	return e->mIsPaused;
}

void pauseDreamRegisteredStateMachine(RegisteredMugenStateMachine* e)
{
	assert(stl_map_contains(gMugenStateHandlerData.mRegisteredStates, e->mID));
	e->mIsPaused = 1;
}

void unpauseDreamRegisteredStateMachine(RegisteredMugenStateMachine* e)
{
	assert(stl_map_contains(gMugenStateHandlerData.mRegisteredStates, e->mID));
	if (e->mIsDisabled) return;
	e->mIsPaused = 0;
}

void setDreamRegisteredStateMachinePauseStatus(RegisteredMugenStateMachine* e, int tIsPaused)
{
	if (tIsPaused) pauseDreamRegisteredStateMachine(e);
	else unpauseDreamRegisteredStateMachine(e);
}

void disableDreamRegisteredStateMachine(RegisteredMugenStateMachine* e)
{
	assert(stl_map_contains(gMugenStateHandlerData.mRegisteredStates, e->mID));
	e->mIsDisabled = 1;
	pauseDreamRegisteredStateMachine(e);
}

int getDreamRegisteredStateJugglePoints(RegisteredMugenStateMachine* e)
{
	assert(stl_map_contains(gMugenStateHandlerData.mRegisteredStates, e->mID));
	return e->mCurrentJugglePoints;
}

int getDreamRegisteredStateTimeInState(RegisteredMugenStateMachine* e)
{
	assert(stl_map_contains(gMugenStateHandlerData.mRegisteredStates, e->mID));
	return e->mTimeInState;
}

void setDreamRegisteredStateTimeInState(RegisteredMugenStateMachine* e, int tTime)
{
	assert(stl_map_contains(gMugenStateHandlerData.mRegisteredStates, e->mID));
	e->mTimeInState = tTime;
}

void setDreamRegisteredStateToHelperMode(RegisteredMugenStateMachine* e)
{
	assert(stl_map_contains(gMugenStateHandlerData.mRegisteredStates, e->mID));
	e->mIsInHelperMode = 1;
}

void setDreamRegisteredStateDisableCommandState(RegisteredMugenStateMachine* e)
{
	assert(stl_map_contains(gMugenStateHandlerData.mRegisteredStates, e->mID));
	e->mIsInputControlDisabled = 1;
}

int hasDreamHandledStateMachineState(RegisteredMugenStateMachine* e, int tNewState)
{
	assert(stl_map_contains(gMugenStateHandlerData.mRegisteredStates, e->mID));	
	DreamMugenStates* states = getCurrentStateMachineStates(e);
	return stl_map_contains(states->mStates, tNewState);
}

int hasDreamHandledStateMachineStateSelf(RegisteredMugenStateMachine* e, int tNewState)
{
	assert(stl_map_contains(gMugenStateHandlerData.mRegisteredStates, e->mID));
	return stl_map_contains(e->mStates->mStates, tNewState);
}

int isInOwnStateMachine(RegisteredMugenStateMachine* e)
{
	assert(stl_map_contains(gMugenStateHandlerData.mRegisteredStates, e->mID));
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

static void applyRegisteredStateMachineStateChange(RegisteredMugenStateMachine* e, int tNewState)
{
	setProfilingSectionMarkerCurrentFunction();
	assert(stl_map_contains(gMugenStateHandlerData.mRegisteredStates, e->mID));
	{
		// this hangs on Dreamcast if we don't init it delayed
		static int tracePending = -1;
		if (tracePending < 0) tracePending = getenv("DOLMEXICA_TRACE_PENDING") ? 1 : 0;
		if (tracePending && e->mHasPendingStateChange && e->mPendingStateChange != tNewState) {
			printf("TRACE pending-clobbered: machine %d (state %d) pending change to %d (own=%d) overwritten by immediate change to %d\n",
				e->mID, e->mState, e->mPendingStateChange, e->mIsPendingStateChangeToOwnStates, tNewState);
			fflush(stdout);
		}
	}
	e->mHasPendingStateChange = 0; // changing state clears pending state changes
	DreamMugenStates* states = getCurrentStateMachineStates(e);
	if (!stl_map_contains(states->mStates, tNewState)) {
		if (!e->mPlayer || e->mIsInStoryMode) {
			logWarningFormat("ID %d trying to change into nonexistant state %d. Ignoring.", e->mID, tNewState);
		}
		else {
			logWarningFormat("Player %d %d trying to change into nonexistant state %d. Ignoring.", e->mPlayer->mRootID, e->mPlayer->mID, tNewState);
		}
		return;
	}

	e->mTimeInState = 0;

	logFormat("%d %d->%d", e->mID, e->mState, tNewState);

	e->mPreviousState = e->mState;
	e->mState = tNewState;
	
	DreamMugenState* newState = &states->mStates[e->mState];
	resetStateControllers(newState);

	if (!e->mPlayer || e->mIsInStoryMode) return;

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

static int isDeferringStateMachineStateChange(RegisteredMugenStateMachine* e) {
	return e->mIsBeingProcessed && isStateMachineInHitPause(e);
}

void changeDreamHandledStateMachineState(RegisteredMugenStateMachine* e, int tNewState)
{
	setProfilingSectionMarkerCurrentFunction();
	assert(stl_map_contains(gMugenStateHandlerData.mRegisteredStates, e->mID));
	if (isDeferringStateMachineStateChange(e)) {
		extern int gDolmexicaDeferCount; gDolmexicaDeferCount++;
		// this hangs on Dreamcast if we don't init it delayed
		static int tracePending = -1;
		if (tracePending < 0) tracePending = getenv("DOLMEXICA_TRACE_PENDING") ? 1 : 0;
		if (tracePending && e->mHasPendingStateChange && e->mPendingStateChange != tNewState) {
			printf("TRACE pending-replaced: machine %d (state %d) pending %d replaced by pending %d\n",
				e->mID, e->mState, e->mPendingStateChange, tNewState);
			fflush(stdout);
		}
		e->mHasPendingStateChange = 1;
		e->mPendingStateChange = tNewState;
		e->mIsPendingStateChangeToOwnStates = 0;
		return;
	}
	applyRegisteredStateMachineStateChange(e, tNewState);
}

void changeDreamHandledStateMachineStateToOtherPlayerStateMachine(RegisteredMugenStateMachine* e, RegisteredMugenStateMachine* tBorrowState, int tNewState)
{
	assert(stl_map_contains(gMugenStateHandlerData.mRegisteredStates, e->mID));
	assert(stl_map_contains(gMugenStateHandlerData.mRegisteredStates, tBorrowState->mID));
	e->mIsUsingTemporaryOtherStateMachine = 1;
	e->mTemporaryStates = tBorrowState->mStates;
	e->mHasPendingStateChange = 0;
	applyRegisteredStateMachineStateChange(e, tNewState);
}

void changeDreamHandledStateMachineStateToOwnStateMachine(RegisteredMugenStateMachine* e, int tNewState)
{
	assert(stl_map_contains(gMugenStateHandlerData.mRegisteredStates, e->mID));
	if (isDeferringStateMachineStateChange(e)) {
		e->mHasPendingStateChange = 1;
		e->mPendingStateChange = tNewState;
		e->mIsPendingStateChangeToOwnStates = 1;
		return;
	}
	e->mIsUsingTemporaryOtherStateMachine = 0;
	applyRegisteredStateMachineStateChange(e, tNewState);
}

void changeDreamHandledStateMachineStateToOwnStateMachineWithoutChangingState(RegisteredMugenStateMachine* e)
{
	assert(stl_map_contains(gMugenStateHandlerData.mRegisteredStates, e->mID));
	e->mIsUsingTemporaryOtherStateMachine = 0;
}

void setDreamHandledStateMachineSpeed(RegisteredMugenStateMachine* e, float tSpeed)
{
	if (!stl_map_contains(gMugenStateHandlerData.mRegisteredStates, e->mID)) return;
	e->mTimeDilatation = tSpeed;
}

void updateDreamSingleStateMachineByID(RegisteredMugenStateMachine* e, int tDoesIncrementTimeInState) {
	assert(stl_map_contains(gMugenStateHandlerData.mRegisteredStates, e->mID));
	updateSingleStateMachineByReference(e, tDoesIncrementTimeInState);
	e->mWasUpdatedOutsideHandler = 1;
}

void setDreamSingleStateMachineToUpdateAgainByID(RegisteredMugenStateMachine* e)
{
	assert(stl_map_contains(gMugenStateHandlerData.mRegisteredStates, e->mID));
	updateSingleStateMachineByReference(e, 1);
	e->mWasUpdatedOutsideHandler = 0;
}

void resetDreamRegisteredStateMachineControllerPersistence(RegisteredMugenStateMachine* e)
{
	assert(stl_map_contains(gMugenStateHandlerData.mRegisteredStates, e->mID));
	for (auto& statePair : e->mStates->mStates) {
		resetStateControllers(&statePair.second);
	}
	e->mHasPendingStateChange = 0;
}

int getActiveStateMachineCoordinateP()
{
	return gMugenStateHandlerData.mActiveCoordinateP;
}

void setActiveStateMachineCoordinateP(int tCoordinateP)
{
	gMugenStateHandlerData.mActiveCoordinateP = tCoordinateP;
}
