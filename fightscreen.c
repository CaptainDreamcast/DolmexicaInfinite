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

static struct {
	void(*mFinishedCB)();
	MemoryStack mMemoryStack;
} gData;

extern int gDebugAssignmentAmount;
extern int gDebugStateControllerAmount;
extern int gDebugStringMapAmount;

static void loadFightScreen() {
	gData.mMemoryStack = createMemoryStack(1024 * 1024 * 3);

	gDebugAssignmentAmount = 0;
	gDebugStateControllerAmount = 0;
	gDebugStringMapAmount = 0;

	setupDreamGameCollisions();
	setupDreamAssignmentReader(&gData.mMemoryStack);
	setupDreamAssignmentEvaluator();
	setupDreamMugenStateControllerHandler(&gData.mMemoryStack);

	instantiateActor(getMugenAnimationHandlerActorBlueprint());
	instantiateActor(MugenTextHandler);
	instantiateActor(ClipboardHandler);

	instantiateActor(MugenAnimationUtilityHandler);
	instantiateActor(DreamAIHandler);
	instantiateActor(DreamMugenConfig);
	instantiateActor(HitDataHandler);
	instantiateActor(ProjectileHandler);

	instantiateActor(DreamMugenCommandHandler);
	instantiateActor(DreamMugenStateHandler);
	instantiateActor(DreamExplodHandler);

	instantiateActor(DreamStageBP);

	loadPlayers(&gData.mMemoryStack);

	instantiateActor(DreamFightUIBP);
	instantiateActor(DreamGameLogic);

	if (isOnWindows()) {
		instantiateActor(FightDebug);
	}

	playDreamStageMusic();

	malloc_stats();

	printf("assignments: %d\n", gDebugAssignmentAmount);
	printf("controllers: %d\n", gDebugStateControllerAmount);
	printf("maps: %d\n", gDebugStringMapAmount);
	printf("memory blocks: %d\n", getAllocatedMemoryBlockAmount());
	printf("memory stack used: %d\n", (int)gData.mMemoryStack.mOffset);
}

static void unloadFightScreen() {
	shutdownDreamMugenStoryStateControllerHandler();
	shutdownDreamAssignmentEvaluator();
	shutdownDreamAssignmentReader();

	return; // TODO: reevaluate
	unloadPlayers();
}

static void updateFightScreen() {
	updatePlayers();

	if (hasPressedAbortFlank()) {
		stopFightScreenToFixedScreen(&DreamTitleScreen);
	}
}

static void drawFightScreen() {
	drawPlayers();
}

static Screen DreamFightScreen = {
	.mLoad = loadFightScreen,
	.mUnload = unloadFightScreen,
	.mUpdate = updateFightScreen,
	.mDraw = drawFightScreen,
};

static void loadFightFonts(void* tCaller) {
	(void)tCaller;
	unloadMugenFonts();
	loadMugenFightFonts();
}

static void loadSystemFonts(void* tCaller) {
	unloadMugenFonts();
	loadMugenSystemFonts();
}

void startFightScreen() {
	setWrapperBetweenScreensCB(loadFightFonts, NULL);
	setNewScreen(&DreamFightScreen);
}

void stopFightScreen() {
	setWrapperBetweenScreensCB(loadSystemFonts, NULL);
	if (!gData.mFinishedCB) return;

	gData.mFinishedCB();
 }


void stopFightScreenToFixedScreen(Screen* tNextScreen) {
	setWrapperBetweenScreensCB(loadSystemFonts, NULL);
	setNewScreen(tNextScreen);
}

void setFightScreenFinishedCB(void(*tCB)()) {
	gData.mFinishedCB = tCB;
}
