#include "fightscreen.h"

#include <stdio.h>

#include <prism/input.h>
#include <prism/stagehandler.h>
#include <prism/collisionhandler.h>
#include <prism/system.h>
#include <prism/mugenanimationreader.h>
#include <prism/mugenanimationhandler.h>
#include <prism/mugentexthandler.h>
#include <prism/clipboardhandler.h>
#include <prism/memorystack.h>
#include <prism/debug.h>

#include <prism/log.h>

#include "stage.h"
#include "mugencommandreader.h"
#include "mugenstatereader.h"
#include "playerdefinition.h"
#include "mugencommandhandler.h"
#include "mugenstatehandler.h"
#include "collision.h"
#include "fightui.h"
#include "mugenexplod.h"
#include "gamelogic.h"
#include "config.h"
#include "playerhitdata.h"
#include "titlescreen.h"
#include "projectile.h"
#include "ai.h"
#include "titlescreen.h"
#include "mugenassignmentevaluator.h"
#include "mugenstatecontrollers.h"
#include "mugenanimationutilities.h"
#include "fightdebug.h"
#include "fightresultdisplay.h"
#include "dolmexicadebug.h"

static struct {
	void(*mWinCB)();
	void(*mLoseCB)();
	MemoryStack mMemoryStack;
} gData;

static void setFightScreenGameSpeed() {
	int gameSpeed = getGlobalGameSpeed();
	if (gameSpeed < 0) {
		double baseFactor = (-gameSpeed) / 9.0;
		double speedFactor = 1 - 0.75 * baseFactor;
		setWrapperTimeDilatation(speedFactor);
	}
	else if (gameSpeed > 0) {
		double baseFactor = gameSpeed / 9.0;
		double speedFactor = 1 + baseFactor;
		setWrapperTimeDilatation(speedFactor);
	}
}

extern int gDebugAssignmentAmount;
extern int gDebugStateControllerAmount;
extern int gDebugStringMapAmount;
extern int gPruneAmount;

static void exitFightScreenCB(void* tCaller);

static void loadFightScreen() {
	setWrapperBetweenScreensCB(exitFightScreenCB, NULL);

	malloc_stats();
	logg("create mem stack\n");
	gData.mMemoryStack = createMemoryStack(1024 * 1024 * 5); // should be 3
	
	malloc_stats();
	logg("init evaluators\n");
	
	gDebugAssignmentAmount = 0;
	gDebugStateControllerAmount = 0;
	gDebugStringMapAmount = 0;
	gPruneAmount = 0;

	setupDreamGameCollisions();
	setupDreamAssignmentReader(&gData.mMemoryStack);
	setupDreamAssignmentEvaluator();
	setupDreamMugenStateControllerHandler(&gData.mMemoryStack);
	
	setStateMachineHandlerToFight();
	
	malloc_stats();
	logg("init custom handlers\n");

	instantiateActor(getMugenAnimationUtilityHandler());
	instantiateActor(getDreamAIHandler());
	instantiateActor(getHitDataHandler());
	instantiateActor(getProjectileHandler());

	instantiateActor(getPreStateMachinePlayersBlueprint());
	instantiateActor(getDreamMugenCommandHandler());
	instantiateActor(getDreamMugenStateHandler());
	instantiateActor(getDreamExplodHandler());
	
	malloc_stats();
	logg("init stage\n");

	instantiateActor(getDreamStageBP());

	malloc_stats();
	logg("init players\n");

	loadPlayers(&gData.mMemoryStack);
	
	instantiateActor(getDreamFightUIBP());
	instantiateActor(getDreamGameLogic());

	instantiateActor(getFightResultDisplay());
	
	if (isMugenDebugActive()) {
		instantiateActor(getFightDebug());
	}
	if (isInDevelopMode()) {
		instantiateActor(getDolmexicaDebug());
	}

	malloc_stats();
	logg("shrinking memory stack\n");
	resizeMemoryStackToCurrentSize(&gData.mMemoryStack); // TODO: test extensively
	malloc_stats();
	
	loadPlayerSprites();
	setUIFaces();
	
	playDreamStageMusic();
	
	setFightScreenGameSpeed();

	malloc_stats();

	logFormat("assignments: %d\n", gDebugAssignmentAmount);
	logFormat("controllers: %d\n", gDebugStateControllerAmount);
	logFormat("maps: %d\n", gDebugStringMapAmount);
	logFormat("memory blocks: %d\n", getAllocatedMemoryBlockAmount());
	logFormat("memory stack used: %d\n", (int)gData.mMemoryStack.mOffset);
}

static void unloadFightScreen() {
	unloadPlayers();
	shutdownDreamMugenStoryStateControllerHandler();
	shutdownDreamAssignmentEvaluator();
	shutdownDreamAssignmentReader();
}

static void updateFightScreen() {
	updatePlayers();
}

static void drawFightScreen() {
	drawPlayers();
}

static Screen gDreamFightScreen;

static Screen* getDreamFightScreen() {
	gDreamFightScreen = makeScreen(loadFightScreen, updateFightScreen, drawFightScreen, unloadFightScreen);
	return &gDreamFightScreen;
}

static void loadFightFonts(void* tCaller) {
	(void)tCaller;
	unloadMugenFonts();
	loadMugenFightFonts();
}

static void exitFightScreenCB(void* tCaller) {
	setWrapperTimeDilatation(1);
	unloadMugenFonts();
	loadMugenSystemFonts();
}

void startFightScreen() {
	setWrapperBetweenScreensCB(loadFightFonts, NULL);
	setNewScreen(getDreamFightScreen());
}

void stopFightScreenWin() {
	setWrapperBetweenScreensCB(exitFightScreenCB, NULL);
	if (!gData.mWinCB) return;

	gData.mWinCB();
}

void stopFightScreenLose()
{
	setWrapperBetweenScreensCB(exitFightScreenCB, NULL);
	if (!gData.mLoseCB) {
		setNewScreen(getDreamTitleScreen());
		return;
	}

	gData.mLoseCB();
}

void stopFightScreenToFixedScreen(Screen* tNextScreen) {
	setWrapperBetweenScreensCB(exitFightScreenCB, NULL);
	setNewScreen(tNextScreen);
}

void setFightScreenFinishedCBs(void(*tWinCB)(), void(*tLoseCB)()) {
	gData.mWinCB = tWinCB;
	gData.mLoseCB = tLoseCB;
}