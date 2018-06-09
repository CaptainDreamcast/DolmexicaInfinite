#include "versusmode.h"

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
	setFightContinueInactive();
	setTimerFinite();
	setPlayersToRealFightMode();
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
	setCharacterSelectScreenModeName("Versus Mode");
	setCharacterSelectTwoPlayers();
	setCharacterSelectStageActive();
	setCharacterSelectFinishedCB(characterSelectFinishedCB);
	setNewScreen(&CharacterSelectScreen);
}

void startVersusMode()
{
	fightFinishedCB();
}
