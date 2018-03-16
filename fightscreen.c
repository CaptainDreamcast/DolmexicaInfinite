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

static struct {
	void(*mFinishedCB)();

} gData;

static void loadFightScreen() {
	setupDreamGameCollisions();
	setupDreamAssignmentEvaluator();
	setupDreamMugenStateControllerHandler();
	instantiateActor(getMugenAnimationHandlerActorBlueprint());
	instantiateActor(MugenTextHandler);
	instantiateActor(ClipboardHandler);

	instantiateActor(DreamAIHandler);
	instantiateActor(DreamMugenConfig);
	instantiateActor(HitDataHandler);
	instantiateActor(ProjectileHandler);
	instantiateActor(DreamMugenCommandHandler);
	instantiateActor(DreamMugenStateHandler);
	instantiateActor(DreamExplodHandler);

	setDreamStageMugenDefinition("assets/stages/kfm.def");
	instantiateActor(DreamStageBP);

	
	//setPlayerDefinitionPath(0, "assets/main/fight/Mima_RP/Mima_RP.def");
	//setPlayerDefinitionPath(0, "assets/main/fight/Ryu/Ryu.def");
	//setPlayerDefinitionPath(0, "assets/main/fight/Beat/Beat.def");
	//setPlayerDefinitionPath(0, "assets/main/fight/kfm/kfm.def");
	//setPlayerDefinitionPath(0, "assets/main/fight/SonicV2/SonicV2.def");
	//setPlayerDefinitionPath(0, "assets/main/fight/Sonicth/Sonicth.def");

	//setPlayerDefinitionPath(1, "assets/main/fight/Ryu/Ryu.def");
	//setPlayerDefinitionPath(1, "assets/main/fight/liukang/liukang.def");
	//setPlayerDefinitionPath(1, "assets/main/fight/Beat/Beat.def");
	//setPlayerDefinitionPath(1, "assets/main/fight/kfm/kfm.def");
	//setPlayerDefinitionPath(1, "assets/main/fight/SonicV2/SonicV2.def");


	loadPlayers();

	instantiateActor(DreamFightUIBP);
	instantiateActor(DreamGameLogic);
	
	// activateCollisionHandlerDebugMode();
}


static void updateFightScreen() {
	updatePlayers();

	if (hasPressedAbortFlank()) {
		stopFightScreenToFixedScreen(&DreamTitleScreen);
	}
}

static Screen DreamFightScreen = {
	.mLoad = loadFightScreen,
	.mUpdate = updateFightScreen,
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
