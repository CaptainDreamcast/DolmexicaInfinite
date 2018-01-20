#include <tari/framerateselectscreen.h>
#include <tari/pvr.h>
#include <tari/physics.h>
#include <tari/file.h>
#include <tari/drawing.h>
#include <tari/log.h>
#include <tari/wrapper.h>
#include <tari/system.h>
#include <tari/stagehandler.h>
#include <tari/logoscreen.h>

#include "titlescreen.h"
#include "fightscreen.h"
#include "playerdefinition.h"
#include "mugentexthandler.h"

#ifdef DREAMCAST
KOS_INIT_FLAGS(INIT_DEFAULT);

extern uint8 romdisk[];
KOS_INIT_ROMDISK(romdisk);

#endif


void exitGame() {
	shutdownTariWrapper();

#ifdef DEVELOP
	abortSystem();
#else
	returnToMenu();
#endif
}

void setMainFileSystem() {
#ifdef DEVELOP
	setFileSystem("/pc");
#else
	setFileSystem("/cd");
#endif
}

int main(int argc, char** argv) {
	(void)argc;
	(void)argv;

	setGameName("DOLMEXICA INFINITE");
	setScreenSize(320, 240);
	
	initTariWrapperWithDefaultFlags();
	setFont("$/rd/fonts/segoe.hdr", "$/rd/fonts/segoe.pkg");
	loadMugenTextHandler();

	logg("Check framerate");
	FramerateSelectReturnType framerateReturnType = selectFramerate();
	if (framerateReturnType == FRAMERATE_SCREEN_RETURN_ABORT) {
		exitGame();
	}

	setMainFileSystem();
	
	setPlayerDefinitionPath(0, "assets/kfm/kfm.def");
	setPlayerDefinitionPath(1, "assets/kfm/kfm.def");
	setPlayerHuman(0);
	setPlayerArtificial(1);
	setScreenAfterWrapperLogoScreen(&DreamTitleScreen);
	startScreenHandling(getLogoScreenFromWrapper());

	exitGame();
	
	return 0;
}


