#pragma once

#include <prism/actorhandler.h>

#include "mugenstatereader.h"
#include "playerdefinition.h"

ActorBlueprint getDreamMugenStateHandler();

int registerDreamMugenStateMachine(DreamMugenStates* tStates, DreamPlayer* tPlayer);
int registerDreamMugenStoryStateMachine(DreamMugenStates* tStates);
void removeDreamRegisteredStateMachine(int tID);
int getDreamRegisteredStateState(int tID);
int getDreamRegisteredStatePreviousState(int tID);
void pauseDreamRegisteredStateMachine(int tID);
void unpauseDreamRegisteredStateMachine(int tID);
void disableDreamRegisteredStateMachine(int tID);
int getDreamRegisteredStateJugglePoints(int tID);

int getDreamRegisteredStateTimeInState(int tID);
void setDreamRegisteredStateTimeInState(int tID, int tTime);
void setDreamRegisteredStateToHelperMode(int tID);
void setDreamRegisteredStateDisableCommandState(int tID);

int hasDreamHandledStateMachineState(int tID, int tNewState);
int hasDreamHandledStateMachineStateSelf(int tID, int tNewState);
int isInOwnStateMachine(int tID);
void changeDreamHandledStateMachineState(int tID, int tNewState);
void changeDreamHandledStateMachineStateToOtherPlayerStateMachine(int tID, int tTemporaryID, int tNewState);
void changeDreamHandledStateMachineStateToOwnStateMachine(int tID, int tNewState);
void changeDreamHandledStateMachineStateToOwnStateMachineWithoutChangingState(int tID);


void updateDreamSingleStateMachineByID(int tID);

