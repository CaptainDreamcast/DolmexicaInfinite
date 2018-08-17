#pragma once

#include <prism/datastructures.h>
#include <prism/actorhandler.h>

#include "mugenstatecontrollers.h"
#include "mugenassignment.h"

extern ActorBlueprint BackgroundStateHandler;

// TODO: make faster by mixing with stage loading
void setBackgroundStatesFromScript(MugenDefScript* tScript);


