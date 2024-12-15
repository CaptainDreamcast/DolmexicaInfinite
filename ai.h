#pragma once

#include <prism/actorhandler.h>

#include "playerdefinition.h"

using namespace prism;

void setDreamAIActive(DreamPlayer* p);
void activateRandomAICommand(int tPlayerIndex);

ActorBlueprint getDreamAIHandler();