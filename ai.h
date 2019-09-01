#pragma once

#include <prism/actorhandler.h>

#include "playerdefinition.h"

void setDreamAIActive(DreamPlayer* p);
void activateRandomAICommand(int tPlayerIndex);

ActorBlueprint getDreamAIHandler();