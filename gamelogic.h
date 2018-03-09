#pragma once

#include <prism/actorhandler.h>
#include <prism/wrapper.h>

extern ActorBlueprint DreamGameLogic;

int getDreamGameTime();

int getDreamRoundNumber();
int getDreamRoundStateNumber();
int getDreamMatchNumber();

int isDreamMatchOver();
void setDreamRoundNotOverFlag();

void setDreamGameModeSinglePlayer();
void setDreamGameModeTwoPlayer();

int getDreamTicksPerSecond();