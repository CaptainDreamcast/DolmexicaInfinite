#pragma once

#include <prism/actorhandler.h>

using namespace prism;

void setFightDebugToPlayerOneBeforeFight();
void switchFightDebugTextActivity();
void switchDebugTimeOff();
void switchFightCollisionDebugActivity();

extern ActorBlueprint getFightDebug();