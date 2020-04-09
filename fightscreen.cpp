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
#include "pausecontrollers.h"
#include "trainingmodemenu.h"

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
		setWrapperTimeDilatation(speedFactor * getConfigGameSpeedTimeFactor());
	}
	else if (gameSpeed > 0) {
		double baseFactor = gameSpeed / 9.0;
		double speedFactor = 1 + baseFactor;
		setWrapperTimeDilatation(speedFactor * getConfigGameSpeedTimeFactor());
	}
}

extern int gDebugAssignmentAmount;
extern int gDebugStateControllerAmount;
extern int gDebugStringMapAmount;
extern int gPruneAmount;

static void exitFightScreenCB(void* tCaller);

static void loadFightScreen() {
	setWrapperBetweenScreensCB(exitFightScreenCB, NULL);

	logMemoryState();
	logg("create mem stack");
	gFightScreenData.mMemoryStack = createMemoryStack(1024 * 1024 * 3);
	
	logMemoryState();
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
	
	logMemoryState();
	logg("init custom handlers");

	instantiateActor(getMugenAnimationUtilityHandler());
	instantiateActor(getDreamAIHandler());
	instantiateActor(getProjectileHandler());
	instantiateActor(getDolmexicaSoundHandler());

	instantiateActor(getPreStateMachinePlayersBlueprint());
	instantiateActor(getDreamMugenCommandHandler());
	instantiateActor(getDreamMugenStateHandler());
	if (isMugenDebugActive()) {
		int actorID = instantiateActor(getFightDebug());
		setActorUnpausable(actorID);
	}
	
	logMemoryState();
	logg("init stage");

	instantiateActor(getDreamStageBP());

	logMemoryState();
	logg("init players");

	loadPlayers(&gFightScreenData.mMemoryStack);
	
	instantiateActor(getDreamFightUIBP());
	instantiateActor(getDreamGameLogic());

	instantiateActor(getFightResultDisplay());
	
	logMemoryState();
	logg("shrinking memory stack");
	resizeMemoryStackToCurrentSize(&gFightScreenData.mMemoryStack);
	logMemoryState();
	shutdownDreamAssignmentReader();
	
	loadPlayerSprites();
	setUIFaces();
	
	playDreamStageMusic();
	if (getGameMode() == GAME_MODE_OSU) {
		instantiateActor(getOsuHandler());
	}

	instantiateActor(getPauseControllerHandler());
	instantiateActor(getPostStateMachinePlayersBlueprint());
	instantiateActor(getDreamExplodHandler());

	if (isInDevelopMode()) {
		instantiateActor(getDolmexicaDebug());
	}

	if (getGameMode() == GAME_MODE_TRAINING) {
		int actorID = instantiateActor(getTrainingModeMenu());
		setActorUnpausable(actorID);
	}

	setFightScreenGameSpeed();
	
	changePlayerState(getRootPlayer(0), 5900);
	changePlayerState(getRootPlayer(1), 5900);
	setPlayerStatemachineToUpdateAgain(getRootPlayer(0));
	setPlayerStatemachineToUpdateAgain(getRootPlayer(1));

	logMemoryState();

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
		setWrapperTimeDilatation(getConfigGameSpeedTimeFactor());
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

Screen* getDreamFightScreenForTesting() {
	return getDreamFightScreen();
}