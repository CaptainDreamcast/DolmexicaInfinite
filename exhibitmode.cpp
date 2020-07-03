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
#include "config.h"

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

	gExhibitModeData.mIsSelectEnabled = getMugenDefIntegerOrDefault(tScript, "demo mode", "select.enabled", 0);
	gExhibitModeData.mIsVersusEnabled = getMugenDefIntegerOrDefault(tScript, "demo mode", "vsscreen.enabled", 0);
	gExhibitModeData.mFightEndTime = getMugenDefIntegerOrDefault(tScript, "demo mode", "fight.endtime", 1500);
	gExhibitModeData.mIsPlayingFightBGM = getMugenDefIntegerOrDefault(tScript, "demo mode", "fight.playbgm", 0);
	gExhibitModeData.mIsDisplayingFightBars = getMugenDefIntegerOrDefault(tScript, "demo mode", "fight.bars.display", 0);
	gExhibitModeData.mIntroCycleAmount = getMugenDefIntegerOrDefault(tScript, "demo mode", "intro.waitcycles", 1);

	gExhibitModeData.mIsShowingDebugInfo = getMugenDefIntegerOrDefault(tScript, "demo mode", "debuginfo", 0);
}

static int loadRandomCharactersAndReturnIfSuccessful() {
	MugenDefScript script;
	loadMugenDefScript(&script, getDolmexicaAssetFolder() + "data/select.def");
	if (!setCharacterRandomAndReturnIfSuccessful(&script, 0)) return 0;
	if (!setCharacterRandomAndReturnIfSuccessful(&script, 1)) return 0;
	setStageRandom(&script);
	return 1;
}

void startExhibitMode()
{
	char scriptPath[1000];
	strcpy(scriptPath, (getDolmexicaAssetFolder() + getMotifPath()).c_str());

	MugenDefScript script; 
	loadMugenDefScript(&script, scriptPath);
	loadExhibitHeader(&script);
	unloadMugenDefScript(&script);

	if (!loadRandomCharactersAndReturnIfSuccessful()) {
		setNewScreen(getDreamTitleScreen());
		return;
	}
	if (gExhibitModeData.mIsVersusEnabled) {
		setVersusScreenNoMatchNumber();
		setVersusScreenFinishedCB(versusScreenFinishedCB);
		setNewScreen(getVersusScreen());
	}
	else {
		versusScreenFinishedCB();
	}
}
