#include "osumode.h"

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
	setGameModeOsu();
	startFightScreen(fightFinishedCB);
}

static void characterSelectFinishedCB() {
	setVersusScreenNoMatchNumber();
	setVersusScreenFinishedCB(versusScreenFinishedCB);
	setNewScreen(getVersusScreen());
}

static void fightFinishedCB() {
	setCharacterSelectScreenModeName("Osu Mode");
	setCharacterSelectOnePlayerSelectAll();
	setCharacterSelectStageActive(1);
	setCharacterSelectFinishedCB(characterSelectFinishedCB);
	setNewScreen(getCharacterSelectScreen());
}

void startOsuMode()
{
	fightFinishedCB();
}
