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
#include "mugenassignmentevaluator.h"
#include "storyhelper.h"

using namespace std;

struct ActiveStory {
	char mStoryPath[1024];
	int mCurrentState;
};

static struct {
	std::vector<ActiveStory> mActiveStories;
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
	storyModeOverCB(gStoryModeData.mActiveStories.back().mCurrentState + 1);
}

static void loadStoryboardGroup(MugenDefScriptGroup* tGroup) {
	char path[1024];
	char folder[1024];

	getPathToFile(folder, gStoryModeData.mActiveStories.back().mStoryPath);
	const auto file = evaluateMugenDefStringOrDefaultAsGroup(tGroup, "file", "");

	sprintf(path, "%s%s%s", getDolmexicaAssetFolder().c_str(), folder, file.c_str());
	if (!isFile(path)) {
		logWarningFormat("Unable to open storyboard %s. Returning to title.", path);
		gStoryModeData.mActiveStories.clear();
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
	std::string file;

	getPathToFile(folder, gStoryModeData.mActiveStories.back().mStoryPath);

	file = evaluateMugenDefStringOrDefaultAsGroup(tGroup, "player1", "");
	int isSelectingFirstCharacter = file == "select";
	if (!isSelectingFirstCharacter) {
		sprintf(path, "%schars/%s/%s.def", getDolmexicaAssetFolder().c_str(), file.c_str(), file.c_str());
		setPlayerDefinitionPath(0, path);
	}
	file = evaluateMugenDefStringOrDefaultAsGroup(tGroup, "player2", "");
	sprintf(path, "%schars/%s/%s.def", getDolmexicaAssetFolder().c_str(), file.c_str(), file.c_str());
	setPlayerDefinitionPath(1, path);

	file = evaluateMugenDefStringOrDefaultAsGroup(tGroup, "stage", "");
	sprintf(path, "%s%s", getDolmexicaAssetFolder().c_str(), file.c_str());
	string musicPath = "";
	if (isMugenDefStringVariableAsGroup(tGroup, "bgm")) {
		musicPath = evaluateMugenDefStringOrDefaultAsGroup(tGroup, "bgm", "");
	}
	setDreamStageMugenDefinition(path, musicPath.data());


	gStoryModeData.mNextStateAfterWin = evaluateMugenDefIntegerOrDefaultAsGroup(tGroup, "win", gStoryModeData.mActiveStories.back().mCurrentState + 1);
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

	const auto mode = evaluateMugenDefStringOrDefaultAsGroup(tGroup, "mode", "");
	if (mode == "osu") {
		setGameModeOsu();
	}
	else {
		setGameModeStory();
	}

	const auto customMotif = evaluateMugenDefStringOrDefaultAsGroup(tGroup, "fight.motif", "");
	if (!customMotif.empty()) {
		setCustomFightMotif(customMotif);
	}

	const auto storyHelper = evaluateMugenDefStringOrDefaultAsGroup(tGroup, "storyhelper", "");
	if (!storyHelper.empty()) {
		setStoryHelperPath(getDolmexicaAssetFolder() + storyHelper);
	}

	int palette1 = evaluateMugenDefIntegerOrDefaultAsGroup(tGroup, "palette1", 1);
	setPlayerPreferredPalette(0, palette1);
	int palette2 = evaluateMugenDefIntegerOrDefaultAsGroup(tGroup, "palette2", 2);
	setPlayerPreferredPalette(1, palette2);

	int ailevel = evaluateMugenDefIntegerOrDefaultAsGroup(tGroup, "ailevel", getDifficulty());
	setPlayerArtificial(1, ailevel);

	const auto startLife1 = evaluateMugenDefFloatOrDefaultAsGroup(tGroup, "life1", 1.0);
	setPlayerStartLifePercentage(0, startLife1);
	const auto startLife2 = evaluateMugenDefFloatOrDefaultAsGroup(tGroup, "life2", 1.0);
	setPlayerStartLifePercentage(1, startLife2);

	const auto rounds = evaluateMugenDefIntegerOrDefaultAsGroup(tGroup, "rounds", 0);
	if (rounds > 0) {
		setRoundsToWin(rounds);
	}

	const auto roundTime = evaluateMugenDefIntegerOrDefaultAsGroup(tGroup, "round.time", 0);
	if (roundTime > 0) {
		setTimerDuration(roundTime);
	}
	else if (roundTime == -1) {
		setTimerInfinite();
	}

	if (isSelectingFirstCharacter) {
		const auto selectName = evaluateMugenDefStringOrDefaultAsGroup(tGroup, "selectname", "Select your fighter");
		const auto selectFileName = evaluateMugenDefStringOrDefaultAsGroup(tGroup, "selectfile", "");
		setCharacterSelectCustomSelectFile(selectFileName);
		setCharacterSelectScreenModeName(selectName.c_str());
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
				setCustomPlayerDisplayName(i, evaluateMugenDefStringOrDefaultAsGroup(tGroup, ss.str().c_str(), ""));
			}
		}

		internCharacterSelectOverCB();
	}
}

static void loadTitleGroup() {
	gStoryModeData.mActiveStories.clear();
	setNewScreen(getDreamTitleScreen());
}

static Screen* getStoryModeScreen();

static void loadSubStoryGroup(MugenDefScriptGroup* tGroup) {
	std::string folder;
	getPathToFile(folder, gStoryModeData.mActiveStories.back().mStoryPath);
	const auto file = evaluateMugenDefStringOrDefaultAsGroup(tGroup, "file", "");
	auto path = folder + file;
	if (!isFile(getDolmexicaAssetFolder() + path)) {
		path = file;
		if (!isFile(getDolmexicaAssetFolder() + path)) {
			logWarningFormat("Unable to open sub story file %s. Returning to title.", file.c_str());
			gStoryModeData.mActiveStories.clear();
			setNewScreen(getDreamTitleScreen());
			return;
		}
	}

	gStoryModeData.mActiveStories.push_back(ActiveStory());
	strcpy(gStoryModeData.mActiveStories.back().mStoryPath, path.c_str());
	gStoryModeData.mActiveStories.back().mCurrentState = evaluateMugenDefIntegerOrDefaultAsGroup(tGroup, "startstate", 0);
	setNewScreen(getStoryModeScreen());
}

static void loadReturnGroup(MugenDefScriptGroup* tGroup) {
	gStoryModeData.mActiveStories.pop_back();
	if (gStoryModeData.mActiveStories.empty()) {
		setNewScreen(getDreamTitleScreen());
		return;
	}

	gStoryModeData.mActiveStories.back().mCurrentState = evaluateMugenDefIntegerOrDefaultAsGroup(tGroup, "value", 0);
	setNewScreen(getStoryModeScreen());
}

static void loadStoryModeScreen() {
	setupDreamGlobalAssignmentEvaluator();

	char path[1024];
	sprintf(path, "%s%s", getDolmexicaAssetFolder().c_str(), gStoryModeData.mActiveStories.back().mStoryPath);
	MugenDefScript script; 
	loadMugenDefScript(&script, path);

	MugenDefScriptGroup* group = getMugenDefStoryScriptGroupByIndex(&script, gStoryModeData.mActiveStories.back().mCurrentState);
	if (!group || !isMugenDefStringVariableAsGroup(group, "type")) {
		logWarningFormat("Unable to read story state %d (from %s). Returning to title.", gStoryModeData.mActiveStories.back().mCurrentState, gStoryModeData.mActiveStories.back().mStoryPath);
		gStoryModeData.mActiveStories.clear();
		setNewScreen(getDreamTitleScreen());
		return;
	}

	auto type = evaluateMugenDefStringOrDefaultAsGroup(group, "type", "");
	turnStringLowercase(type);
	if (type == "storyboard") {
		loadStoryboardGroup(group);
	} else if (type == "fight") {
		loadFightGroup(group);
	} else if (type == "title") {
		loadTitleGroup();
	}
	else if (type == "substory") {
		loadSubStoryGroup(group);
	}
	else if (type == "return") {
		loadReturnGroup(group);
	}
	else {
		logWarningFormat("Unable to read story state %d type %s (from %s). Returning to title.", gStoryModeData.mActiveStories.back().mCurrentState, type.c_str(), gStoryModeData.mActiveStories.back().mStoryPath);
		gStoryModeData.mActiveStories.clear();
		setNewScreen(getDreamTitleScreen());
	}
}

static void unloadStoryModeScreen() {
	shutdownDreamAssignmentEvaluator();
}

static Screen gStoryModeScreen;

static Screen* getStoryModeScreen() {
	gStoryModeScreen = makeScreen(loadStoryModeScreen, NULL, NULL, unloadStoryModeScreen);
	return &gStoryModeScreen;
};

static void loadStoryHeader() {

	char path[1024];
	sprintf(path, "%s%s", getDolmexicaAssetFolder().c_str(), gStoryModeData.mActiveStories.back().mStoryPath);
	MugenDefScript storyScript;
	loadMugenDefScript(&storyScript, path);

	gStoryModeData.mActiveStories.back().mCurrentState = evaluateMugenDefIntegerOrDefault(&storyScript, "info", "startstate", 0);
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
		gStoryModeData.mActiveStories.clear();
		setNewScreen(getDreamTitleScreen());
		return;
	}
	
	MugenDefScriptGroup* stories = &selectScript.mGroups["stories"];
	if (!list_size(&stories->mOrderedElementList)) {
		gStoryModeData.mActiveStories.clear();
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
	gStoryModeData.mActiveStories.back().mCurrentState = tStep;
	setNewScreen(getStoryModeScreen());
}

void setStoryModeStoryPath(const char * tPath)
{
	if (gStoryModeData.mActiveStories.empty()) {
		gStoryModeData.mActiveStories.push_back(ActiveStory());
	}
	strcpy(gStoryModeData.mActiveStories.back().mStoryPath, tPath);
	gStoryModeData.mActiveStories.back().mCurrentState = 0;
}

void startStoryModeWithForcedStartState(const char * tPath, int tStartState)
{
	setStoryModeStoryPath(tPath);
	loadStoryHeader();
	gStoryModeData.mActiveStories.back().mCurrentState = tStartState;
	setNewScreen(getStoryModeScreen());
}