#include "arcademode.h"

#include <assert.h>
#include <algorithm>

#include <prism/mugendefreader.h>
#include <prism/math.h>
#include <prism/log.h>

#include "characterselectscreen.h"
#include "titlescreen.h"
#include "storyscreen.h"
#include "playerdefinition.h"
#include "versusscreen.h"
#include "fightscreen.h"
#include "gamelogic.h"
#include "stage.h"
#include "fightui.h"
#include "fightresultdisplay.h"

using namespace std;

typedef struct {
	char mDefinitionPath[300];
	char mStagePath[300];
	char mMusicPath[300];
} ArcadeCharacter;

static struct {
	int mEnemyAmount;
	ArcadeCharacter mEnemies[100];
	int mCurrentEnemy;
	int mHasEnding;

	int mHasGameOver;
	char mGameOverPath[300];

	int mHasDefaultEnding;
	char mDefaultEndingPath[300];

	int mHasCredits;
	char mCreditsPath[300];
} gData;

static void fightFinishedCB();

static void gameOverFinishedCB() {
	setNewScreen(getDreamTitleScreen());
}

static void fightLoseCB() {
	if (gData.mHasGameOver) {
		setStoryDefinitionFile(gData.mGameOverPath);
		setStoryScreenFinishedCB(gameOverFinishedCB);
		setNewScreen(getStoryScreen());
	}
	else {
		gameOverFinishedCB();
	}
}

static void versusScreenFinishedCB() {

	int isFinalFight = gData.mCurrentEnemy == gData.mEnemyAmount - 1;
	setGameModeArcade();
	setFightResultActive(isFinalFight && !gData.mHasEnding);
	setFightScreenFinishedCBs(fightFinishedCB, fightLoseCB); 
	startFightScreen();
}

typedef struct {
	int mIsSelected;
	int mOrder;
	char mDefinitionPath[300];
	char mStagePath[300];
	char mMusicPath[300];
} SingleArcadeEnemy;

typedef struct {
	Vector mEnemies;
} Order;

typedef struct {
	IntMap mOrders;
} AddEnemyCaller;

static void addNewEnemyOrderToCaller(AddEnemyCaller* tCaller, int tOrder) {
	Order* e = (Order*)allocMemory(sizeof(Order));
	e->mEnemies = new_vector();

	int_map_push_owned(&tCaller->mOrders, tOrder, e);
}

static void addSingleEnemyToSelection(void* tCaller, void* tData) {
	AddEnemyCaller* caller = (AddEnemyCaller*)tCaller;
	MugenDefScriptGroupElement* element = (MugenDefScriptGroupElement*)tData;

	if (!isMugenDefStringVectorVariableAsElement(element)) return;

	MugenStringVector stringVector = getMugenDefStringVectorVariableAsElement(element);
	assert(stringVector.mSize >= 2);

	SingleArcadeEnemy* e = (SingleArcadeEnemy*)allocMemory(sizeof(SingleArcadeEnemy));
	e->mIsSelected = 0;
	e->mOrder = 1;
	getCharacterSelectNamePath(stringVector.mElement[0], e->mDefinitionPath);
	if (!isFile(e->mDefinitionPath)) {
		logWarningFormat("Unable to find def file %s. Ignoring character.", e->mDefinitionPath);
		freeMemory(e);
		return;
	}
	if (!strcmp("random", stringVector.mElement[1])) { // TODO: fix when assets removed
		strcpy(e->mStagePath, stringVector.mElement[1]);
	}
	else {
		sprintf(e->mStagePath, "assets/%s", stringVector.mElement[1]);
	}
	*e->mMusicPath = '\0';

	parseOptionalCharacterSelectParameters(stringVector, &e->mOrder, NULL, e->mMusicPath);

	if (!int_map_contains(&caller->mOrders, e->mOrder)) {
		addNewEnemyOrderToCaller(caller, e->mOrder);
	}
	Order* order = (Order*)int_map_get(&caller->mOrders, e->mOrder);

	vector_push_back_owned(&order->mEnemies, e);
}

static void addArcadeEnemy(SingleArcadeEnemy* tEnemy) {
	int index = gData.mEnemyAmount;
	ArcadeCharacter* e = &gData.mEnemies[index];
	strcpy(e->mDefinitionPath, tEnemy->mDefinitionPath);
	strcpy(e->mStagePath, tEnemy->mStagePath);
	strcpy(e->mMusicPath, tEnemy->mMusicPath);

	gData.mEnemyAmount++;
}

static void addOrderEnemies(int tOrder, int tAmount, AddEnemyCaller* tCaller) {
	if (!int_map_contains(&tCaller->mOrders, tOrder)) return;

	Order* order = (Order*)int_map_get(&tCaller->mOrders, tOrder);

	tAmount = min(tAmount, vector_size(&order->mEnemies));

	int i;
	for (i = 0; i < tAmount; i++) {
		int j;
		for (j = 0; j < 100; j++) {
			int index = randfromInteger(0, vector_size(&order->mEnemies) - 1);
			SingleArcadeEnemy* enemy = (SingleArcadeEnemy*)vector_get(&order->mEnemies, index);

			if (enemy->mIsSelected) continue;

			addArcadeEnemy(enemy);
			enemy->mIsSelected = 1;
			break;
		}
	}
}

static int unloadSingleOrder(void* tCaller, void* tData) {
	(void)tCaller;
	Order* e = (Order*)tData;
	delete_vector(&e->mEnemies);
	return 1;
}

static void generateEnemies() {
	MugenDefScript script = loadMugenDefScript("assets/data/select.def");

	MugenStringVector enemyTypeAmountVector = getMugenDefStringVectorVariable(&script, "Options", "arcade.maxmatches");

	MugenDefScriptGroup* group = &script.mGroups["Characters"];
	AddEnemyCaller caller;
	caller.mOrders = new_int_map();
	list_map(&group->mOrderedElementList, addSingleEnemyToSelection, &caller);


	gData.mEnemyAmount = 0;
	int i;
	for (i = 0; i < enemyTypeAmountVector.mSize; i++) {
		addOrderEnemies(i+1, atoi(enemyTypeAmountVector.mElement[i]), &caller);
	}

	int_map_remove_predicate(&caller.mOrders, unloadSingleOrder, NULL);
	delete_int_map(&caller.mOrders);

	unloadMugenDefScript(script);
}

static void creditsFinishedCB() {
	startArcadeMode();

}

static void endingScreenFinishedCB() {
	if (gData.mHasCredits) {
		setStoryDefinitionFile(gData.mCreditsPath);
		setStoryScreenFinishedCB(creditsFinishedCB);
		setNewScreen(getStoryScreen());
		return;
	}

	creditsFinishedCB();
}

static void startEnding() {
	char folder[1024];
	char path[1024];
	getPlayerDefinitionPath(path, 0);
	getPathToFile(folder, path);
	MugenDefScript script = loadMugenDefScript(path);
	char* endingDefinitionFile;

	if (gData.mHasEnding) {
		endingDefinitionFile = getAllocatedMugenDefStringVariable(&script, "Arcade", "ending.storyboard");
		sprintf(path, "%s%s", folder, endingDefinitionFile);
		if (isFile(path)) {
			setStoryDefinitionFile(path);
			setStoryScreenFinishedCB(endingScreenFinishedCB);
			setNewScreen(getStoryScreen());
			return;
		}
	}
	else if (gData.mHasDefaultEnding) {
		setStoryDefinitionFile(gData.mDefaultEndingPath);
		setStoryScreenFinishedCB(endingScreenFinishedCB);
		setNewScreen(getStoryScreen());
		return;
	}

	endingScreenFinishedCB();

}

static void fightFinishedCB() {
	gData.mCurrentEnemy++;

	if (gData.mCurrentEnemy == gData.mEnemyAmount) {
		startEnding();
		return;
	}
		
	setPlayerDefinitionPath(1, gData.mEnemies[gData.mCurrentEnemy].mDefinitionPath);

	if (!strcmp("random", gData.mEnemies[gData.mCurrentEnemy].mStagePath)) {
		MugenDefScript script = loadMugenDefScript("assets/data/select.def");
		setStageRandom(&script);
	}
	else {
		setDreamStageMugenDefinition(gData.mEnemies[gData.mCurrentEnemy].mStagePath, gData.mEnemies[gData.mCurrentEnemy].mMusicPath);
	}
	setVersusScreenFinishedCB(versusScreenFinishedCB);
	setNewScreen(getVersusScreen());
}

static void introScreenFinishedCB() {
	gData.mCurrentEnemy = -1;
	fightFinishedCB();
}

static void characterSelectFinishedCB() {
	generateEnemies();

	char folder[1024];
	char path[1024];
	getPlayerDefinitionPath(path, 0);
	getPathToFile(folder, path);
	MugenDefScript script = loadMugenDefScript(path);
	int hasIntro;
	char* introDefinitionFile;
	hasIntro = isMugenDefStringVariable(&script, "Arcade", "intro.storyboard");
	gData.mHasEnding = isMugenDefStringVariable(&script, "Arcade", "ending.storyboard");

	int isGoingToStory = 0;

	if (hasIntro) {
		introDefinitionFile = getAllocatedMugenDefStringVariable(&script, "Arcade", "intro.storyboard");
		sprintf(path, "%s%s", folder, introDefinitionFile);
		if (isFile(path)) {
			setStoryDefinitionFile(path);
			setStoryScreenFinishedCB(introScreenFinishedCB);
			isGoingToStory = 1;
		}
		freeMemory(introDefinitionFile);
	}


	unloadMugenDefScript(script);

	if (isGoingToStory) {
		setNewScreen(getStoryScreen());
	}
	else {
		introScreenFinishedCB();
	}
}

static void loadWinScreenFromScript(MugenDefScript* tScript) {
	int isEnabled = getMugenDefIntegerOrDefault(tScript, "Win Screen", "enabled", 0);
	char* message = getAllocatedMugenDefStringOrDefault(tScript, "Win Screen", "wintext.text", "Congratulations!");
	Vector3DI font = getMugenDefVectorIOrDefault(tScript, "Win Screen", "wintext.font", makeVector3DI(2, 0, 0));
	Position offset = getMugenDefVectorOrDefault(tScript, "Win Screen", "wintext.offset", makePosition(0, 0, 0));
	int displayTime = getMugenDefIntegerOrDefault(tScript, "Win Screen", "wintext.displaytime", -1);
	int layerNo = getMugenDefIntegerOrDefault(tScript, "Win Screen", "wintext.layerno", 2);
	int fadeInTime = getMugenDefIntegerOrDefault(tScript, "Win Screen", "fadein.time", 30);
	int poseTime = getMugenDefIntegerOrDefault(tScript, "Win Screen", "pose.time", 300);
	int fadeOutTime = getMugenDefIntegerOrDefault(tScript, "Win Screen", "fadeout.time", 30);

	setFightResultData(isEnabled, message, font, offset, displayTime, layerNo, fadeInTime, poseTime, fadeOutTime, 1);

	freeMemory(message);
}

static void loadSingleStoryboard(MugenDefScript* tScript, char* tFolder, char* tGroup, int* oHasStoryboard, char* oStoryBoardPath) {

	*oHasStoryboard = getMugenDefIntegerOrDefault(tScript, tGroup, "enabled", 0);
	if (!(*oHasStoryboard)) return;

	char* name = getAllocatedMugenDefStringOrDefault(tScript, tGroup, "storyboard", "");
	sprintf(oStoryBoardPath, "%s%s", tFolder, name); // TODO: non-relative
	freeMemory(name);
}

static void loadStoryboardsFromScript(MugenDefScript* tScript, char* tFolder) {
	
	loadSingleStoryboard(tScript, tFolder, "Game Over Screen", &gData.mHasGameOver, gData.mGameOverPath);
	loadSingleStoryboard(tScript, tFolder, "Default Ending", &gData.mHasDefaultEnding, gData.mDefaultEndingPath);
	loadSingleStoryboard(tScript, tFolder, "End Credits", &gData.mHasCredits, gData.mCreditsPath);
}

static void loadArcadeModeHeaderFromScript() {
	char scriptPath[1000];
	char folder[1000];

	strcpy(scriptPath, "assets/data/system.def");
	getPathToFile(folder, scriptPath);

	MugenDefScript script = loadMugenDefScript(scriptPath);

	loadWinScreenFromScript(&script);
	loadStoryboardsFromScript(&script, folder);

	unloadMugenDefScript(script);
}

void startArcadeMode()
{
	loadArcadeModeHeaderFromScript();

	setCharacterSelectScreenModeName("Arcade");
	setCharacterSelectOnePlayer();
	setCharacterSelectStageInactive();
	setCharacterSelectFinishedCB(characterSelectFinishedCB);
	setNewScreen(getCharacterSelectScreen());
}
