#include "trainingmode.h"

#include "characterselectscreen.h"
#include "titlescreen.h"
#include "fightscreen.h"
#include "gamelogic.h"
#include "versusscreen.h"
#include "fightscreen.h"
#include "playerdefinition.h"
#include "fightui.h"
#include "fightresultdisplay.h"

static void fightFinishedCB();

static void versusScreenFinishedCB() {
	setGameModeTraining();
	setFightScreenFinishedCBs(fightFinishedCB, NULL);
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
