#pragma once

#include <prism/actorhandler.h>
#include <prism/mugenanimationhandler.h>

#include "mugencommandreader.h"
#include "playerdefinition.h"

int registerDreamMugenCommands(int tControllerID, DreamMugenCommands* tCommands);

int isDreamCommandActive(int tID, const char* tCommandName);
int isDreamCommandActiveByLookupIndex(int tID, int tLookupIndex);
int isDreamCommandForLookup(int tID, const char* tCommandName, int* oLookupIndex);
int getDreamCommandMinimumDuration(int tID, const char* tCommandName);
void setDreamPlayerCommandActiveForAI(int tID, const char* tCommandName, int tBufferTime);
int setDreamPlayerCommandNumberActiveForDebug(int tID, int tCommandNumber);
int getDreamPlayerCommandAmount(int tID);

void setDreamMugenCommandFaceDirection(int tID, FaceDirection tDirection);
void allowOsuPlayerCommandInputOneFrame(int tRootIndex);
void resetOsuPlayerCommandInputAllowed(int tRootIndex);
int isOsuPlayerCommandInputAllowed(int tRootIndex);

ActorBlueprint getDreamMugenCommandHandler();

void setDreamButtonAActiveForPlayer(int tControllerIndex);
void setDreamButtonBActiveForPlayer(int tControllerIndex);
void setDreamButtonCActiveForPlayer(int tControllerIndex);
void setDreamButtonXActiveForPlayer(int tControllerIndex);
void setDreamButtonYActiveForPlayer(int tControllerIndex);
void setDreamButtonZActiveForPlayer(int tControllerIndex);
void setDreamButtonStartActiveForPlayer(int tControllerIndex);

void updateCommandNetplayReceive(int tID);
void updateCommandNetplaySend(int tID);
void setDreamCommandInputControllerUsed(int i, int tControllerIndex);