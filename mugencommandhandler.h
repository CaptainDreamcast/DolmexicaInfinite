#pragma once

#include <prism/actorhandler.h>
#include <prism/mugenanimationhandler.h>

#include "mugencommandreader.h"
#include "playerdefinition.h"

int registerDreamMugenCommands(int tControllerID, DreamMugenCommands* tCommands);

int isDreamCommandActive(int tID, const char* tCommandName);
void setDreamPlayerCommandActiveForAI(int tID, const char* tCommandName, Duration tBufferTime);

void setDreamMugenCommandFaceDirection(int tID, FaceDirection tDirection);

ActorBlueprint getDreamMugenCommandHandler();