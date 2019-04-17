#include "storymode.h"

#include <prism/log.h>

#include "characterselectscreen.h"
#include "titlescreen.h"
#include "fightscreen.h"
#include "gamelogic.h"
#include "versusscreen.h"
#include "fightscreen.h"
#include "playerdefinition.h"
#include "fightui.h"
#include "dolmexicastoryscreen.h"
#include "storymode.h"
#include "stage.h"
#include "fightresultdisplay.h"
#include "config.h"

static struct {
	char mStoryPath[1024];
	int mCurrentState;

	int mNextStateAfterWin;
	int mNextStateAfterLose;
} gData;

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

static void loadStoryboardGroup(MugenDefScriptGroup* tGroup) {
	char path[1024];
	char folder[1024];

	getPathToFile(folder, gData.mStoryPath);
	char* file = getAllocatedMugenDefStringVariableAsGroup(tGroup, "file");

	sprintf(path, "assets/%s%s", folder, file);
	if (!isFile(path)) {
		logWarningFormat("Unable to open storyboard %s. Returning to title.", path);
		setNewScreen(getDreamTitleScreen());
		return;
	}

	setDolmexicaStoryScreenFile(path);
	setNewScreen(getDolmexicaStoryScreen());
}

static void fightFinishedCB() {
	if(getDreamMatchWinnerIndex()) storyModeOverCB(gData.mNextStateAfterLose);
	else storyModeOverCB(gData.mNextStateAfterWin);
}

static void internCharacterSelectOverCB() {
	startFightScreen(fightFinishedCB);
}

static void loadFightGroup(MugenDefScriptGroup* tGroup) {
	char path[1024];
	char folder[1024];
	char* file;

	getPathToFile(folder, gData.mStoryPath);

	file = getAllocatedMugenDefStringVariableAsGroup(tGroup, "player1");
	int isSelectingFirstCharacter = !strcmp("select", file);
	if (!isSelectingFirstCharacter) {
		sprintf(path, "assets/chars/%s/%s.def", file, file);
		setPlayerDefinitionPath(0, path);
	}
	file = getAllocatedMugenDefStringVariableAsGroup(tGroup, "player2");
	sprintf(path, "assets/chars/%s/%s.def", file, file);
	setPlayerDefinitionPath(1, path);


	int palette1 = getMugenDefIntegerOrDefaultAsGroup(tGroup, "palette1", 1);
	setPlayerPreferredPalette(0, palette1);
	int palette2 = getMugenDefIntegerOrDefaultAsGroup(tGroup, "palette2", 2);
	setPlayerPreferredPalette(1, palette2);

	file = getAllocatedMugenDefStringVariableAsGroup(tGroup, "stage");
	sprintf(path, "assets/%s", file);
	char dummyMusicPath[2];
	*dummyMusicPath = '\0';
	setDreamStageMugenDefinition(path, dummyMusicPath);



	gData.mNextStateAfterWin = getMugenDefIntegerOrDefaultAsGroup(tGroup, "win", gData.mCurrentState + 1);
	char* afterLoseState = getAllocatedMugenDefStringOrDefaultAsGroup(tGroup, "lose", "continue");
	turnStringLowercase(afterLoseState);
	if (!strcmp("continue", afterLoseState)) {
		gData.mNextStateAfterLose = -1;
		setFightContinueActive();
	}
	else {
		gData.mNextStateAfterLose = atoi(afterLoseState);
		setFightContinueInactive();
	}
	freeMemory(afterLoseState);

	setGameModeStory();
	int ailevel = getMugenDefIntegerOrDefaultAsGroup(tGroup, "ailevel", getDifficulty());
	setPlayerArtificial(1, ailevel);


	if (isSelectingFirstCharacter) {
		char* selectName = getAllocatedMugenDefStringOrDefaultAsGroup(tGroup, "selectname", "Select your fighter");
		setCharacterSelectScreenModeName(selectName);
		setCharacterSelectOnePlayer();
		setCharacterSelectStageInactive();
		setCharacterSelectFinishedCB(internCharacterSelectOverCB);
		setNewScreen(getCharacterSelectScreen());
	}
	else {
		internCharacterSelectOverCB();
	}
}

static void loadTitleGroup() {
	setNewScreen(getDreamTitleScreen());
}

static void loadStoryModeScreen() {
	char path[1024];
	sprintf(path, "assets/%s", gData.mStoryPath);
	MugenDefScript script; 
	loadMugenDefScript(&script, path);

	MugenDefScriptGroup* group = getMugenDefStoryScriptGroupByIndex(&script, gData.mCurrentState);
	if (!group || !isMugenDefStringVariableAsGroup(group, "type")) {
		logWarningFormat("Unable to read story state %d (from %s). Returning to title.", gData.mCurrentState, gData.mStoryPath);
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
		logWarningFormat("Unable to read story state %d type %s (from %s). Returning to title.", gData.mCurrentState, type, gData.mStoryPath);
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
	sprintf(path, "assets/%s", gData.mStoryPath);
	MugenDefScript storyScript;
	loadMugenDefScript(&storyScript, path);

	gData.mCurrentState = getMugenDefIntegerOrDefault(&storyScript, "Info", "startstate", 0);
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
	loadMugenDefScript(&selectScript, "assets/data/select.def");
	if (!stl_string_map_contains_array(selectScript.mGroups, "Stories")) {
		setNewScreen(getDreamTitleScreen());
		return;
	}
	
	MugenDefScriptGroup* stories = &selectScript.mGroups["Stories"];
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
	gData.mCurrentState = tStep;
	setNewScreen(getStoryModeScreen());
}

void setStoryModeStoryPath(char * tPath)
{
	strcpy(gData.mStoryPath, tPath);
}
