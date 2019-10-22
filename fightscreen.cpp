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
#include "osuhandler.h"
#include "mugensound.h"

static struct {
	void(*mWinCB)();
	void(*mLoseCB)();
	MemoryStack mMemoryStack;
} gFightScreenData;

static void setFightScreenGameSpeed() {
	if (isDebugOverridingTimeDilatation()) return;

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

	logMemoryPlatform();
	logg("create mem stack");
	gFightScreenData.mMemoryStack = createMemoryStack(1024 * 1024 * 3);
	
	logMemoryPlatform();
	logg("init evaluators");
	
	gDebugAssignmentAmount = 0;
	gDebugStateControllerAmount = 0;
	gDebugStringMapAmount = 0;
	gPruneAmount = 0;

	setupDreamGameCollisions();
	setupDreamAssignmentReader(&gFightScreenData.mMemoryStack);
	setupDreamAssignmentEvaluator();
	setupDreamMugenStateControllerHandler(&gFightScreenData.mMemoryStack);
	
	setStateMachineHandlerToFight();
	
	logMemoryPlatform();
	logg("init custom handlers");

	instantiateActor(getMugenAnimationUtilityHandler());
	instantiateActor(getDreamAIHandler());
	instantiateActor(getHitDataHandler());
	instantiateActor(getProjectileHandler());
	instantiateActor(getDolmexicaSoundHandler());

	instantiateActor(getPreStateMachinePlayersBlueprint());
	instantiateActor(getDreamMugenCommandHandler());
	instantiateActor(getDreamMugenStateHandler());
	if (isMugenDebugActive()) {
		int actorID = instantiateActor(getFightDebug());
		setActorUnpausable(actorID);
	}
	instantiateActor(getDreamExplodHandler());
	
	logMemoryPlatform();
	logg("init stage");

	instantiateActor(getDreamStageBP());

	logMemoryPlatform();
	logg("init players");

	loadPlayers(&gFightScreenData.mMemoryStack);
	
	instantiateActor(getDreamFightUIBP());
	instantiateActor(getDreamGameLogic());

	instantiateActor(getFightResultDisplay());
	
	logMemoryPlatform();
	logg("shrinking memory stack");
	resizeMemoryStackToCurrentSize(&gFightScreenData.mMemoryStack);
	logMemoryPlatform();
	shutdownDreamAssignmentReader();
	
	loadPlayerSprites();
	setUIFaces();
	
	playDreamStageMusic();
	if (getGameMode() == GAME_MODE_OSU) {
		instantiateActor(getOsuHandler());
	}

	instantiateActor(getPostStateMachinePlayersBlueprint());
	if (isInDevelopMode()) {
		instantiateActor(getDolmexicaDebug());
	}

	setFightScreenGameSpeed();
	
	changePlayerState(getRootPlayer(0), 5900);
	changePlayerState(getRootPlayer(1), 5900);

	logMemoryPlatform();

	logFormat("assignments: %d", gDebugAssignmentAmount);
	logFormat("controllers: %d", gDebugStateControllerAmount);
	logFormat("maps: %d", gDebugStringMapAmount);
	logFormat("memory blocks: %d", getAllocatedMemoryBlockAmount());
	logFormat("memory stack used: %d", (int)gFightScreenData.mMemoryStack.mOffset);
}

static void unloadFightScreen() {
	unloadPlayers();
	resetGameMode();
	shutdownDreamMugenStateControllerHandler();
	shutdownDreamAssignmentEvaluator();
}

static void drawFightScreen() {
	drawPlayers();
}

static Screen gDreamFightScreen;

static Screen* getDreamFightScreen() {
	gDreamFightScreen = makeScreen(loadFightScreen, NULL, drawFightScreen, unloadFightScreen);
	return &gDreamFightScreen;
}

static void loadFightFonts(void* tCaller) {
	(void)tCaller;
	unloadMugenFonts();
	loadMugenFightFonts();
}

static void exitFightScreenCB(void* /*tCaller*/) {
	if (!isDebugOverridingTimeDilatation()) {
		setWrapperTimeDilatation(1);
	}
	unloadMugenFonts();
	loadMugenSystemFonts();
}

void startFightScreen(void(*tWinCB)(), void(*tLoseCB)()) {
	gFightScreenData.mWinCB = tWinCB;
	gFightScreenData.mLoseCB = tLoseCB;
	setWrapperBetweenScreensCB(loadFightFonts, NULL);
	setNewScreen(getDreamFightScreen());
}

void reloadFightScreen()
{
	startFightScreen(gFightScreenData.mWinCB, gFightScreenData.mLoseCB);
}

void stopFightScreenWin() {
	setWrapperBetweenScreensCB(exitFightScreenCB, NULL);
	if (!gFightScreenData.mWinCB) return;

	gFightScreenData.mWinCB();
}

void stopFightScreenLose()
{
	setWrapperBetweenScreensCB(exitFightScreenCB, NULL);
	if (!gFightScreenData.mLoseCB) {
		setNewScreen(getDreamTitleScreen());
		return;
	}

	gFightScreenData.mLoseCB();
}

void stopFightScreenToFixedScreen(Screen* tNextScreen) {
	setWrapperBetweenScreensCB(exitFightScreenCB, NULL);
	setNewScreen(tNextScreen);
}
