#include "versusmode.h"

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
	setGameModeFreePlay();
	startFightScreen(fightFinishedCB);
}

static void characterSelectFinishedCB() {
	setVersusScreenNoMatchNumber();
	setVersusScreenFinishedCB(versusScreenFinishedCB);
	setNewScreen(getVersusScreen());
}

static void fightFinishedCB() {
	setCharacterSelectScreenModeName("Free Play Mode");
	setCharacterSelectOnePlayerSelectAll();
	setCharacterSelectStageActive();
	setCharacterSelectFinishedCB(characterSelectFinishedCB);
	setNewScreen(getCharacterSelectScreen());
}

void startFreePlayMode()
{
	fightFinishedCB();
}
