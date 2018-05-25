#include "arcademode.h"

#include <assert.h>

#include <prism/mugendefreader.h>
#include <prism/math.h>

#include "characterselectscreen.h"
#include "titlescreen.h"
#include "storyscreen.h"
#include "playerdefinition.h"
#include "versusscreen.h"
#include "fightscreen.h"
#include "gamelogic.h"
#include "stage.h"
#include "fightui.h"

typedef struct {
	char mDefinitionPath[1024];
} ArcadeCharacter;

static struct {
	int mEnemyAmount;
	ArcadeCharacter mEnemies[100];
	int mCurrentEnemy;
} gData;

static void fightFinishedCB();

static void versusScreenFinishedCB() {
	setTimerFinite();
	setPlayersToRealFightMode();
	setFightScreenFinishedCB(fightFinishedCB); 
	setPlayerHuman(0);
	setPlayerArtificial(1);
	setPlayerPreferredPalette(0, 1);
	setPlayerPreferredPalette(1, 2);
	setDreamStageMugenDefinition("assets/stages/kfm.def"); // TODO: remove
	startFightScreen();
}

typedef struct {
	int mIsSelected;
	int mOrder;
	char mDefinitionPath[1024];
} SingleArcadeEnemy;

typedef struct {
	Vector mEnemies;
} Order;

typedef struct {
	IntMap mOrders;
} AddEnemyCaller;

static void addNewEnemyOrderToCaller(AddEnemyCaller* tCaller, int tOrder) {
	Order* e = allocMemory(sizeof(Order));
	e->mEnemies = new_vector();

	int_map_push_owned(&tCaller->mOrders, tOrder, e);
}

static void addSingleEnemyToSelection(void* tCaller, void* tData) {
	AddEnemyCaller* caller = tCaller;
	MugenDefScriptGroupElement* element = tData;

	assert(isMugenDefStringVectorVariableAsElement(element));
	MugenStringVector stringVector = getMugenDefStringVectorVariableAsElement(element);
	assert(stringVector.mSize >= 2);

	// TODO: get real order
	int orderIndex = 1;

	if (!int_map_contains(&caller->mOrders, orderIndex)) {
		addNewEnemyOrderToCaller(caller, orderIndex);
	}
	Order* order = int_map_get(&caller->mOrders, orderIndex);

	SingleArcadeEnemy* e = allocMemory(sizeof(SingleArcadeEnemy));
	e->mIsSelected = 0;
	e->mOrder = orderIndex;
	sprintf(e->mDefinitionPath, "assets/chars/%s/%s.def", stringVector.mElement[0], stringVector.mElement[0]); // TODO: fix

	vector_push_back_owned(&order->mEnemies, e);
}

static void addArcadeEnemy(SingleArcadeEnemy* tEnemy) {
	int index = gData.mEnemyAmount;
	ArcadeCharacter* e = &gData.mEnemies[index];
	strcpy(e->mDefinitionPath, tEnemy->mDefinitionPath);

	gData.mEnemyAmount++;
}

static void addOrderEnemies(int tOrder, int tAmount, AddEnemyCaller* tCaller) {
	if (!int_map_contains(&tCaller->mOrders, tOrder)) return;

	Order* order = int_map_get(&tCaller->mOrders, tOrder);

	tAmount = min(tAmount, vector_size(&order->mEnemies));

	int i;
	for (i = 0; i < tAmount; i++) {
		int j;
		for (j = 0; j < 100; j++) {
			int index = randfromInteger(0, vector_size(&order->mEnemies) - 1);
			SingleArcadeEnemy* enemy = vector_get(&order->mEnemies, index);

			if (enemy->mIsSelected) continue;

			addArcadeEnemy(enemy);
			enemy->mIsSelected = 1;
			break;
		}
	}
}

static int unloadSingleOrder(void* tCaller, void* tData) {
	(void)tCaller;
	Order* e = tData;
	delete_vector(&e->mEnemies);
	return 1;
}

static void generateEnemies() {
	MugenDefScript script = loadMugenDefScript("assets/data/select.def");

	MugenStringVector enemyTypeAmountVector = getMugenDefStringVectorVariable(&script, "Options", "arcade.maxmatches");

	MugenDefScriptGroup* group = string_map_get(&script.mGroups, "Characters");
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

static void endingScreenFinishedCB() {
	startArcadeMode();
}

static void startEnding() {
	char folder[1024];
	char path[1024];
	getPlayerDefinitionPath(path, 0);
	getPathToFile(folder, path);
	MugenDefScript script = loadMugenDefScript(path);
	int hasEnding;
	char* endingDefinitionFile;
	hasEnding = isMugenDefStringVariable(&script, "Arcade", "ending.storyboard");


	if (hasEnding) {
		endingDefinitionFile = getAllocatedMugenDefStringVariable(&script, "Arcade", "ending.storyboard");
		sprintf(path, "%s%s", folder, endingDefinitionFile);
		if (isFile(path)) {
			setStoryDefinitionFile(path);
			setStoryScreenFinishedCB(endingScreenFinishedCB);
			setNewScreen(&StoryScreen);
			return;
		}
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
	// TODO: set stage
	setVersusScreenFinishedCB(versusScreenFinishedCB);
	setNewScreen(&VersusScreen);
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

	int isGoingToStory;

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
		setNewScreen(&StoryScreen);
	}
	else {
		introScreenFinishedCB();
	}
}

void startArcadeMode()
{
	setCharacterSelectScreenModeName("Arcade");
	setCharacterSelectOnePlayer();
	setCharacterSelectStageInactive();
	setCharacterSelectFinishedCB(characterSelectFinishedCB);
	setNewScreen(&CharacterSelectScreen);
}
