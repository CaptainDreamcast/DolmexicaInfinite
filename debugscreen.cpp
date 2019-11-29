#include "debugscreen.h"

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
#include "fightresultdisplay.h"

static struct {
	MemoryStack mMemoryStack;

} gDebugScreenData;

static void loadDebugScreen() {
	setPlayerDefinitionPath(0, "assets/chars/baldhead/baldhead.def");
	setPlayerDefinitionPath(1, "assets/chars/baldhead/baldhead.def");
	setDreamStageMugenDefinition("assets/stages/kfm.def", "");
	setGameModeTraining();

	logMemoryPlatform();
	logg("create mem stack\n");
	gDebugScreenData.mMemoryStack = createMemoryStack(1024 * 1024 * 5); // should be 3

	setupDreamGameCollisions();
	setupDreamAssignmentReader(&gDebugScreenData.mMemoryStack);
	setupDreamAssignmentEvaluator();
	setupDreamMugenStateControllerHandler(&gDebugScreenData.mMemoryStack);

	setStateMachineHandlerToFight();

	logMemoryPlatform();
	logg("init custom handlers\n");

	instantiateActor(getMugenAnimationUtilityHandler());
	instantiateActor(getDreamAIHandler());
	instantiateActor(getProjectileHandler());

	instantiateActor(getPreStateMachinePlayersBlueprint());
	instantiateActor(getDreamMugenCommandHandler());
	instantiateActor(getDreamMugenStateHandler());
	instantiateActor(getDreamExplodHandler());

	logMemoryPlatform();
	logg("init stage\n");

	instantiateActor(getDreamStageBP());

	logMemoryPlatform();
	logg("init players\n");

	loadPlayers(&gDebugScreenData.mMemoryStack);

	instantiateActor(getDreamFightUIBP());
	instantiateActor(getDreamGameLogic());

	instantiateActor(getFightResultDisplay());

	if (isMugenDebugActive()) {
		instantiateActor(getFightDebug());
	}

	logMemoryPlatform();
	logg("shrinking memory stack\n");
	resizeMemoryStackToCurrentSize(&gDebugScreenData.mMemoryStack);
}

static void updateDebugScreen() {
	abortScreenHandling();
	return;
}

Screen gDebugScreen;
Screen * getDebugScreen()
{
	gDebugScreen = makeScreen(loadDebugScreen, updateDebugScreen);
	return &gDebugScreen;
}
