#include "fightscreen.h"

#include <stdio.h>

#include <tari/input.h>
#include <tari/stagehandler.h>
#include <tari/collisionhandler.h>
#include <tari/system.h>
#include <tari/mugenanimationreader.h>
#include <tari/mugenanimationhandler.h>

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

static Screen* getNextFightScreenScreen() {
	if (hasPressedAbortFlank()) {
		return &DreamTitleScreen;
	}

	return NULL;
}

static void loadFightScreen() {
	setupDreamGameCollisions();
	
	instantiateActor(DreamAIHandler);
	instantiateActor(DreamMugenConfig);
	instantiateActor(HitDataHandler);
	instantiateActor(ProjectileHandler);
	instantiateActor(getMugenAnimationHandlerActorBlueprint());
	instantiateActor(DreamMugenCommandHandler);
	instantiateActor(DreamMugenStateHandler);
	instantiateActor(DreamExplodHandler);

	setDreamStageMugenDefinition("assets/stages/stage0.def");
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
}

Screen DreamFightScreen = {
	.mLoad = loadFightScreen,
	.mGetNextScreen = getNextFightScreenScreen,
	.mUpdate = updateFightScreen,
};
