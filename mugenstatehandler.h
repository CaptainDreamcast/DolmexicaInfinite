#pragma once

#include <prism/actorhandler.h>

#include "mugenstatereader.h"

struct DreamPlayer;
struct StoryInstance;

struct RegisteredMugenStateMachine {
	int mID;
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
};

ActorBlueprint getDreamMugenStateHandler();

RegisteredMugenStateMachine* registerDreamMugenStateMachine(DreamMugenStates* tStates, DreamPlayer* tPlayer);
RegisteredMugenStateMachine* registerDreamMugenStoryStateMachine(DreamMugenStates * tStates, StoryInstance* tInstance);
void removeDreamRegisteredStateMachine(RegisteredMugenStateMachine* tRegisteredState);
int isValidDreamRegisteredStateMachine(RegisteredMugenStateMachine* tRegisteredState);
int getDreamRegisteredStateState(RegisteredMugenStateMachine* tRegisteredState);
int getDreamRegisteredStatePreviousState(RegisteredMugenStateMachine* tRegisteredState);
int isDreamRegisteredStateMachinePaused(RegisteredMugenStateMachine* tRegisteredState);
void pauseDreamRegisteredStateMachine(RegisteredMugenStateMachine* tRegisteredState);
void unpauseDreamRegisteredStateMachine(RegisteredMugenStateMachine* tRegisteredState);
void setDreamRegisteredStateMachinePauseStatus(RegisteredMugenStateMachine* tRegisteredState, int tIsPaused);
void disableDreamRegisteredStateMachine(RegisteredMugenStateMachine* tRegisteredState);
int getDreamRegisteredStateJugglePoints(RegisteredMugenStateMachine* tRegisteredState);

int getDreamRegisteredStateTimeInState(RegisteredMugenStateMachine* tRegisteredState);
void setDreamRegisteredStateTimeInState(RegisteredMugenStateMachine* tRegisteredState, int tTime);
void setDreamRegisteredStateToHelperMode(RegisteredMugenStateMachine* tRegisteredState);
void setDreamRegisteredStateDisableCommandState(RegisteredMugenStateMachine* tRegisteredState);

int hasDreamHandledStateMachineState(RegisteredMugenStateMachine* tRegisteredState, int tNewState);
int hasDreamHandledStateMachineStateSelf(RegisteredMugenStateMachine* tRegisteredState, int tNewState);
int isInOwnStateMachine(RegisteredMugenStateMachine* tRegisteredState);
void changeDreamHandledStateMachineState(RegisteredMugenStateMachine* tRegisteredState, int tNewState);
void changeDreamHandledStateMachineStateToOtherPlayerStateMachine(RegisteredMugenStateMachine* tRegisteredState, RegisteredMugenStateMachine* tBorrowState, int tNewState);
void changeDreamHandledStateMachineStateToOwnStateMachine(RegisteredMugenStateMachine* tRegisteredState, int tNewState);
void changeDreamHandledStateMachineStateToOwnStateMachineWithoutChangingState(RegisteredMugenStateMachine* tRegisteredState);
void setDreamHandledStateMachineSpeed(RegisteredMugenStateMachine* tRegisteredState, double tSpeed);

void updateDreamSingleStateMachineByID(RegisteredMugenStateMachine* tRegisteredState);
void setDreamSingleStateMachineToUpdateAgainByID(RegisteredMugenStateMachine* tRegisteredState);
void setStateMachineHandlerToStory();
void setStateMachineHandlerToFight();

int getActiveStateMachineCoordinateP();
void setActiveStateMachineCoordinateP(int tCoordinateP);