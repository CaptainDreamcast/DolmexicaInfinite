#include "storymode.h"

#include <prism/log.h>
#include <prism/stlutil.h>

#include "characterselectscreen.h"
#include "titlescreen.h"
#include "fightscreen.h"
#include "gamelogic.h"
#include "versusscreen.h"
#include "fightscreen.h"
#include "playerdefinition.h"
#include "fightui.h"
#include "dolmexicastoryscreen.h"
#include "storyscreen.h"
#include "storymode.h"
#include "stage.h"
#include "fightresultdisplay.h"
#include "config.h"

using namespace std;

static struct {
	char mStoryPath[1024];
	int mCurrentState;

	int mNextStateAfterWin;
	int mNextStateAfterLose;
} gStoryModeData;

static MugenDefScriptGroup* getMugenDefStoryScriptGroupByIndex(MugenDefScript* tScript, int tIndex) {
	MugenDefScriptGroup* current = tScript->mFirstGroup;
	if (!current || !current->mNext) return NULL;
	current = current->mNext;

	while (tIndex--) {
		if (!current->mNext) return NULL;
		current = current->mNext;
	}

	return current;
}

static int isMugenStoryboard(const char* tPath) {
	MugenDefScript script;
	loadMugenDefScript(&script, tPath);
	const auto ret = hasMugenDefScriptGroup(&script, "scenedef");
	unloadMugenDefScript(&script);
	return ret;
}

static void mugenStoryScreenFinishedCB() {
	storyModeOverCB(gStoryModeData.mCurrentState + 1);
}

static void loadStoryboardGroup(MugenDefScriptGroup* tGroup) {
	char path[1024];
	char folder[1024];

	getPathToFile(folder, gStoryModeData.mStoryPath);
	char* file = getAllocatedMugenDefStringVariableAsGroup(tGroup, "file");

	sprintf(path, "%s%s%s", getDolmexicaAssetFolder().c_str(), folder, file);
	if (!isFile(path)) {
		logWarningFormat("Unable to open storyboard %s. Returning to title.", path);
		setNewScreen(getDreamTitleScreen());
		return;
	}

	if (isMugenStoryboard(path)) {
		setStoryDefinitionFileAndPrepareScreen(path);
		setStoryScreenFinishedCB(mugenStoryScreenFinishedCB);
		setNewScreen(getStoryScreen());
	}
	else {
		setDolmexicaStoryScreenFileAndPrepareScreen(path);
		setNewScreen(getDolmexicaStoryScreen());
	}
}

static void fightFinishedCB() {
	if(getDreamMatchWinnerIndex()) storyModeOverCB(gStoryModeData.mNextStateAfterLose);
	else storyModeOverCB(gStoryModeData.mNextStateAfterWin);
}

static void internCharacterSelectOverCB() {
	startFightScreen(fightFinishedCB);
}

static void loadFightGroup(MugenDefScriptGroup* tGroup) {
	char path[1024];
	char folder[1024];
	char* file;

	getPathToFile(folder, gStoryModeData.mStoryPath);

	file = getAllocatedMugenDefStringVariableAsGroup(tGroup, "player1");
	int isSelectingFirstCharacter = !strcmp("select", file);
	if (!isSelectingFirstCharacter) {
		sprintf(path, "%schars/%s/%s.def", getDolmexicaAssetFolder().c_str(), file, file);
		setPlayerDefinitionPath(0, path);
	}
	file = getAllocatedMugenDefStringVariableAsGroup(tGroup, "player2");
	sprintf(path, "%schars/%s/%s.def", getDolmexicaAssetFolder().c_str(), file, file);
	setPlayerDefinitionPath(1, path);

	file = getAllocatedMugenDefStringVariableAsGroup(tGroup, "stage");
	sprintf(path, "%s%s", getDolmexicaAssetFolder().c_str(), file);
	string musicPath;
	if (isMugenDefStringVariableAsGroup(tGroup, "bgm")) {
		char* tempFile = getAllocatedMugenDefStringVariableAsGroup(tGroup, "bgm");
		musicPath = tempFile;
		freeMemory(tempFile);
	}
	setDreamStageMugenDefinition(path, musicPath.data());


	gStoryModeData.mNextStateAfterWin = getMugenDefIntegerOrDefaultAsGroup(tGroup, "win", gStoryModeData.mCurrentState + 1);
	char* afterLoseState = getAllocatedMugenDefStringOrDefaultAsGroup(tGroup, "lose", "continue");
	turnStringLowercase(afterLoseState);
	if (!strcmp("continue", afterLoseState)) {
		gStoryModeData.mNextStateAfterLose = -1;
		setFightContinueActive();
	}
	else {
		gStoryModeData.mNextStateAfterLose = atoi(afterLoseState);
		setFightContinueInactive();
	}
	freeMemory(afterLoseState);

	const auto mode = getSTLMugenDefStringOrDefaultAsGroup(tGroup, "mode", "");
	if (mode == "osu") {
		setGameModeOsu();
	}
	else {
		setGameModeStory();
	}

	int palette1 = getMugenDefIntegerOrDefaultAsGroup(tGroup, "palette1", 1);
	setPlayerPreferredPalette(0, palette1);
	int palette2 = getMugenDefIntegerOrDefaultAsGroup(tGroup, "palette2", 2);
	setPlayerPreferredPalette(1, palette2);

	int ailevel = getMugenDefIntegerOrDefaultAsGroup(tGroup, "ailevel", getDifficulty());
	setPlayerArtificial(1, ailevel);

	const auto rounds = getMugenDefIntegerOrDefaultAsGroup(tGroup, "rounds", 0);
	if (rounds > 0) {
		setRoundsToWin(rounds);
	}

	const auto roundTime = getMugenDefIntegerOrDefaultAsGroup(tGroup, "round.time", 0);
	if (roundTime > 0) {
		setTimerDuration(roundTime);
	}

	if (isSelectingFirstCharacter) {
		char* selectName = getAllocatedMugenDefStringOrDefaultAsGroup(tGroup, "selectname", "Select your fighter");
		const auto selectFileName = getSTLMugenDefStringOrDefaultAsGroup(tGroup, "selectfile", "");
		setCharacterSelectCustomSelectFile(selectFileName);
		setCharacterSelectScreenModeName(selectName);
		setCharacterSelectOnePlayer();
		setCharacterSelectStageInactive();
		setCharacterSelectFinishedCB(internCharacterSelectOverCB);
		setCharacterSelectDisableReturnOneTime();
		setNewScreen(getCharacterSelectScreen());
	}
	else {
		for (int i = 0; i < 2; i++) {
			stringstream ss;
			ss << "p" << (i + 1) << ".name";
			if (isMugenDefStringVariableAsGroup(tGroup, ss.str().c_str())) {
				setCustomPlayerDisplayName(i, getSTLMugenDefStringOrDefaultAsGroup(tGroup, ss.str().c_str(), ""));
			}
		}

		internCharacterSelectOverCB();
	}
}

static void loadTitleGroup() {
	setNewScreen(getDreamTitleScreen());
}

static void loadStoryModeScreen() {
	char path[1024];
	sprintf(path, "%s%s", getDolmexicaAssetFolder().c_str(), gStoryModeData.mStoryPath);
	MugenDefScript script; 
	loadMugenDefScript(&script, path);

	MugenDefScriptGroup* group = getMugenDefStoryScriptGroupByIndex(&script, gStoryModeData.mCurrentState);
	if (!group || !isMugenDefStringVariableAsGroup(group, "type")) {
		logWarningFormat("Unable to read story state %d (from %s). Returning to title.", gStoryModeData.mCurrentState, gStoryModeData.mStoryPath);
		setNewScreen(getDreamTitleScreen());
		return;
	}

	char* type = getAllocatedMugenDefStringVariableAsGroup(group, "type");
	if (!strcmp("storyboard", type)) {
		loadStoryboardGroup(group);
	} else if (!strcmp("fight", type)) {
		loadFightGroup(group);
	} else if (!strcmp("title", type)) {
		loadTitleGroup();
	}
	else {
		logWarningFormat("Unable to read story state %d type %s (from %s). Returning to title.", gStoryModeData.mCurrentState, type, gStoryModeData.mStoryPath);
		setNewScreen(getDreamTitleScreen());
	}
	freeMemory(type);
}

static Screen gStoryModeScreen;

static Screen* getStoryModeScreen() {
	gStoryModeScreen = makeScreen(loadStoryModeScreen);
	return &gStoryModeScreen;
};

static void loadStoryHeader() {

	char path[1024];
	sprintf(path, "%s%s", getDolmexicaAssetFolder().c_str(), gStoryModeData.mStoryPath);
	MugenDefScript storyScript;
	loadMugenDefScript(&storyScript, path);

	gStoryModeData.mCurrentState = getMugenDefIntegerOrDefault(&storyScript, "info", "startstate", 0);
}

static void startFirstStoryElement(MugenDefScriptGroup* tStories) {
	MugenDefScriptGroupElement* story = (MugenDefScriptGroupElement*)list_front(&tStories->mOrderedElementList);

	if (story->mType != MUGEN_DEF_SCRIPT_GROUP_STRING_ELEMENT) {
		setNewScreen(getDreamTitleScreen());
		return;
	}

	MugenDefScriptStringElement* stringElement = (MugenDefScriptStringElement*)story->mData;
	setStoryModeStoryPath(stringElement->mString);
	loadStoryHeader();
	setNewScreen(getStoryModeScreen());
}

static void characterSelectFinishedCB() {
	loadStoryHeader();
	setNewScreen(getStoryModeScreen());
}

void startStoryMode()
{
	MugenDefScript selectScript; 
	loadMugenDefScript(&selectScript, getDolmexicaAssetFolder() + "data/select.def");
	if (!stl_string_map_contains_array(selectScript.mGroups, "stories")) {
		setNewScreen(getDreamTitleScreen());
		return;
	}
	
	MugenDefScriptGroup* stories = &selectScript.mGroups["stories"];
	if (!list_size(&stories->mOrderedElementList)) {
		setNewScreen(getDreamTitleScreen());
		return;
	}

	if (list_size(&stories->mOrderedElementList) == 1) {
		startFirstStoryElement(stories);
	}
	else {
		setCharacterSelectScreenModeName("Story");
		setCharacterSelectStory();
		setCharacterSelectStageInactive();
		setCharacterSelectFinishedCB(characterSelectFinishedCB);
		setNewScreen(getCharacterSelectScreen());
	}
}

void storyModeOverCB(int tStep)
{
	gStoryModeData.mCurrentState = tStep;
	setNewScreen(getStoryModeScreen());
}

void setStoryModeStoryPath(const char * tPath)
{
	strcpy(gStoryModeData.mStoryPath, tPath);
}

void startStoryModeWithForcedStartState(const char * tPath, int tStartState)
{
	setStoryModeStoryPath(tPath);
	loadStoryHeader();
	gStoryModeData.mCurrentState = tStartState;
	setNewScreen(getStoryModeScreen());
}