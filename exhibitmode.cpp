#include "exhibitmode.h"

#include "characterselectscreen.h"
#include "titlescreen.h"
#include "fightscreen.h"
#include "gamelogic.h"
#include "versusscreen.h"
#include "fightscreen.h"
#include "playerdefinition.h"
#include "fightui.h"
#include "fightresultdisplay.h"


static struct {
	int mCurrentCycle;

	int mIsSelectEnabled;
	int mIsVersusEnabled;
	int mFightEndTime;
	int mIsPlayingFightBGM;
	int mIsDisplayingFightBars;
	int mIntroCycleAmount;

	int mIsShowingDebugInfo; // TODO
} gData;

static void fightFinishedCB();

static void versusScreenFinishedCB() {
	setGameModeExhibit(gData.mFightEndTime, gData.mIsDisplayingFightBars);
	startFightScreen(fightFinishedCB);
}

static void fightFinishedCB() {
	setNewScreen(getDreamTitleScreen());
}

static void loadExhibitHeader(MugenDefScript* tScript) {

	gData.mIsSelectEnabled = getMugenDefIntegerOrDefault(tScript, "Demo Mode", "select.enabled", 0);
	gData.mIsVersusEnabled = getMugenDefIntegerOrDefault(tScript, "Demo Mode", "vsscreen.enabled", 0);
	gData.mFightEndTime = getMugenDefIntegerOrDefault(tScript, "Demo Mode", "fight.endtime", 1500);
	gData.mIsPlayingFightBGM = getMugenDefIntegerOrDefault(tScript, "Demo Mode", "fight.playbgm", 0);
	gData.mIsDisplayingFightBars = getMugenDefIntegerOrDefault(tScript, "Demo Mode", "fight.bars.display", 0);
	gData.mIntroCycleAmount = getMugenDefIntegerOrDefault(tScript, "Demo Mode", "intro.waitcycles", 1);

	gData.mIsShowingDebugInfo = getMugenDefIntegerOrDefault(tScript, "Demo Mode", "debuginfo", 0);
}

static void loadRandomCharacters() {
	MugenDefScript script;
	loadMugenDefScript(&script, "assets/data/select.def");
	setCharacterRandom(&script, 0);
	setCharacterRandom(&script, 1);
	setStageRandom(&script);
}

void startExhibitMode()
{
	char scriptPath[1000];
	strcpy(scriptPath, "assets/data/system.def");

	MugenDefScript script; 
	loadMugenDefScript(&script, scriptPath);
	loadExhibitHeader(&script);
	unloadMugenDefScript(script);

	loadRandomCharacters();
	if (gData.mIsVersusEnabled) {
		setVersusScreenFinishedCB(versusScreenFinishedCB);
		setNewScreen(getVersusScreen());
	}
	else {
		versusScreenFinishedCB();
	}
}
