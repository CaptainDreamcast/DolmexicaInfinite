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
#include "config.h"

static struct {
	int mCurrentMatch;
} gRandomWatchModeData;

static void fightFinishedCB();

static void versusScreenFinishedCB() {
	setGameModeWatch();
	startFightScreen(fightFinishedCB);
}

static void fightFinishedCB() {
	MugenDefScript script; 
	loadMugenDefScript(&script, getDolmexicaAssetFolder() + "data/select.def");

	setCharacterRandomAndReturnIfSuccessful(&script, 0);
	setCharacterRandomAndReturnIfSuccessful(&script, 1);
	setStageRandom(&script);

	gRandomWatchModeData.mCurrentMatch++;
	setVersusScreenMatchNumber(gRandomWatchModeData.mCurrentMatch);
	setVersusScreenFinishedCB(versusScreenFinishedCB);
	setNewScreen(getVersusScreen());
}

void startRandomWatchMode()
{
	gRandomWatchModeData.mCurrentMatch = 0;
	fightFinishedCB();
}
