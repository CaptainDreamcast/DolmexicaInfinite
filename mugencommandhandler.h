#pragma once

#include <prism/actorhandler.h>
#include <prism/mugenanimationhandler.h>

#include "mugencommandreader.h"
#include "playerdefinition.h"

int registerDreamMugenCommands(DreamPlayer* tPlayer, DreamMugenCommands* tCommands);

int isDreamCommandActive(int tID, char* tCommandName);
void setDreamPlayerCommandActiveForAI(int tID, char* tCommandName, Duration tBufferTime);

void setDreamMugenCommandFaceDirection(int tID, FaceDirection tDirection);

ActorBlueprint getDreamMugenCommandHandler();