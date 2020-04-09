#include "superwatchmode.h"

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
	setGameModeSuperWatch();
	startFightScreen(fightFinishedCB);
}

static void characterSelectFinishedCB() {
	setVersusScreenNoMatchNumber();
	setVersusScreenFinishedCB(versusScreenFinishedCB);
	setNewScreen(getVersusScreen());
}

static void fightFinishedCB() {
	setCharacterSelectScreenModeName("Super Watch Mode");
	setCharacterSelectOnePlayerSelectAll();
	setCharacterSelectStageActive();
	setCharacterSelectFinishedCB(characterSelectFinishedCB);
	setNewScreen(getCharacterSelectScreen());
}

void startSuperWatchMode()
{
	fightFinishedCB();
}
