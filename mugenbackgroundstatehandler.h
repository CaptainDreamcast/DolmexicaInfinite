#pragma once

#include <prism/datastructures.h>
#include <prism/actorhandler.h>

#include "mugenstatecontrollers.h"
#include "mugenassignment.h"

using namespace prism;

ActorBlueprint getBackgroundStateHandler();

void setBackgroundStatesFromScript(MugenDefScript* tScript);

void resetBackgroundStateHandler();