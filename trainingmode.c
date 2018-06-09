#include "trainingmode.h"

#include "characterselectscreen.h"
#include "titlescreen.h"
#include "fightscreen.h"
#include "gamelogic.h"
#include "versusscreen.h"
#include "fightscreen.h"
#include "playerdefinition.h"
#include "fightui.h"

static void fightFinishedCB();

static void versusScreenFinishedCB() {
	setFightContinueActive();
	setTimerInfinite();
	setPlayersToTrainingMode();
	setPlayerHuman(0);
	setPlayerHuman(1);
	setPlayerPreferredPalette(0, 1);
	setPlayerPreferredPalette(1, 2);
	setFightScreenFinishedCB(fightFinishedCB);
	startFightScreen();
}

static void characterSelectFinishedCB() {
	setVersusScreenFinishedCB(versusScreenFinishedCB);
	setNewScreen(&VersusScreen);
}

static void fightFinishedCB() {
	setCharacterSelectScreenModeName("Training Mode");
	setCharacterSelectOnePlayerSelectAll();
	setCharacterSelectStageActive();
	setCharacterSelectFinishedCB(characterSelectFinishedCB);
	setNewScreen(&CharacterSelectScreen);
}

void startTrainingMode()
{
	fightFinishedCB();
}
