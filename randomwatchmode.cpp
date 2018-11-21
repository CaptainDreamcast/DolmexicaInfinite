#include "watchmode.h"

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
	setGameModeWatch();
	setFightScreenFinishedCBs(fightFinishedCB, NULL);
	startFightScreen();
}

static void fightFinishedCB() {
	MugenDefScript script = loadMugenDefScript("assets/data/select.def");

	setCharacterRandom(&script, 0);
	setCharacterRandom(&script, 1);
	setStageRandom(&script);

	setVersusScreenFinishedCB(versusScreenFinishedCB);
	setNewScreen(&VersusScreen);
}

void startRandomWatchMode()
{
	fightFinishedCB();
}
