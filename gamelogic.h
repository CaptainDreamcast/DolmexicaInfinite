#pragma once

#include <prism/actorhandler.h>
#include <prism/wrapper.h>

typedef enum {
	GAME_MODE_ARCADE,
	GAME_MODE_FREE_PLAY,
	GAME_MODE_STORY,
	GAME_MODE_VERSUS,
	GAME_MODE_SURVIVAL,
	GAME_MODE_TRAINING,
	GAME_MODE_WATCH,
	GAME_MODE_SUPER_WATCH,
	GAME_MODE_EXHIBIT
} GameMode;

ActorBlueprint getDreamGameLogic();

int getDreamGameTime();

int getDreamRoundNumber();
int getRoundsToWin();
int getDreamRoundStateNumber();
int getDreamMatchNumber();

int isDreamMatchOver();
void setDreamRoundNotOverFlag();

int isDreamGameModeTwoPlayer();
void setDreamGameModeSinglePlayer();
void setDreamGameModeTwoPlayer();

int getDreamTicksPerSecond();
int getDreamMatchWinnerIndex();

void resetRound();
void reloadFight();
void skipFightIntroWithoutFading();

void setFightContinueActive();
void setFightContinueInactive();

void setGameModeArcade();
void setGameModeFreePlay();
void setGameModeVersus();
void setGameModeSurvival(double tLifePercentage, int tRound);
void setGameModeTraining(); 
void setGameModeWatch();
void setGameModeSuperWatch();
void setGameModeExhibit(int tEndTime, int tIsDisplayingBars);
void setGameModeStory();

GameMode getGameMode();