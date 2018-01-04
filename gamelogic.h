#pragma once

#include <tari/actorhandler.h>
#include <tari/wrapper.h>

extern ActorBlueprint DreamGameLogic;

int getDreamGameTime();

int getDreamRoundNumber();
int getDreamRoundStateNumber();
int getDreamMatchNumber();

int isDreamMatchOver();
void setDreamRoundNotOverFlag();

void setDreamScreenAfterFightScreen(Screen* tScreen);
void setDreamGameModeStory();
void setDreamGameModeTwoPlayer();