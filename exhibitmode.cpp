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

	int mIsShowingDebugInfo;
} gExhibitModeData;

static void fightFinishedCB();

static void versusScreenFinishedCB() {
	setGameModeExhibit(gExhibitModeData.mFightEndTime, gExhibitModeData.mIsDisplayingFightBars, gExhibitModeData.mIsShowingDebugInfo);
	startFightScreen(fightFinishedCB);
}

static void fightFinishedCB() {
	setNewScreen(getDreamTitleScreen());
}

static void loadExhibitHeader(MugenDefScript* tScript) {

	gExhibitModeData.mIsSelectEnabled = getMugenDefIntegerOrDefault(tScript, "Demo Mode", "select.enabled", 0);
	gExhibitModeData.mIsVersusEnabled = getMugenDefIntegerOrDefault(tScript, "Demo Mode", "vsscreen.enabled", 0);
	gExhibitModeData.mFightEndTime = getMugenDefIntegerOrDefault(tScript, "Demo Mode", "fight.endtime", 1500);
	gExhibitModeData.mIsPlayingFightBGM = getMugenDefIntegerOrDefault(tScript, "Demo Mode", "fight.playbgm", 0);
	gExhibitModeData.mIsDisplayingFightBars = getMugenDefIntegerOrDefault(tScript, "Demo Mode", "fight.bars.display", 0);
	gExhibitModeData.mIntroCycleAmount = getMugenDefIntegerOrDefault(tScript, "Demo Mode", "intro.waitcycles", 1);

	gExhibitModeData.mIsShowingDebugInfo = getMugenDefIntegerOrDefault(tScript, "Demo Mode", "debuginfo", 0);
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
	if (gExhibitModeData.mIsVersusEnabled) {
		setVersusScreenFinishedCB(versusScreenFinishedCB);
		setNewScreen(getVersusScreen());
	}
	else {
		versusScreenFinishedCB();
	}
}
