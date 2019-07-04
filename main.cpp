#include <prism/framerateselectscreen.h>
#include <prism/pvr.h>
#include <prism/physics.h>
#include <prism/file.h>
#include <prism/drawing.h>
#include <prism/log.h>
#include <prism/wrapper.h>
#include <prism/system.h>
#include <prism/stagehandler.h>
#include <prism/logoscreen.h>
#include <prism/mugentexthandler.h>
#include <prism/clipboardhandler.h>
#include <prism/sound.h>
#include <prism/screeneffect.h>
#include <prism/profiling.h>
#include <prism/debug.h>
#include <prism/soundeffect.h>

#include "titlescreen.h"
#include "fightscreen.h"
#include "playerdefinition.h"
#include "warningscreen.h"
#include "dolmexicastoryscreen.h"
#include "stage.h"
#include "config.h"
#include "dolmexicadebug.h"
#include "debugscreen.h"

char romdisk_buffer[1];
int romdisk_buffer_length;

#define DEVELOP

#ifdef DREAMCAST
KOS_INIT_FLAGS(INIT_DEFAULT);

extern uint8 romdisk[];
KOS_INIT_ROMDISK(romdisk);

#endif

int isInDevelopMode() {
#ifdef DEVELOP
    return 1;
#else
    return 0;
#endif
}

void exitGame() {
	shutdownPrismWrapper();

#ifdef DEVELOP
	if (isOnDreamcast()) {
		abortSystem();
	}
	else {
		returnToMenu();
	}
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

#ifdef DEVELOP	
	setMinimumLogType(LOG_TYPE_NORMAL);
#else
	setMinimumLogType(LOG_TYPE_NONE);
#endif

	setGameName("DOLMEXICA INFINITE");
	setScreenSize(320, 240);

	setMainFileSystem();	
	loadMugenTextHandler();
	initPrismWrapperWithMugenFlags();

	setFont("$/rd/fonts/segoe.hdr", "$/rd/fonts/segoe.pkg");
	loadMugenSystemFonts();
	logg("Check framerate");
	FramerateSelectReturnType framerateReturnType = selectFramerate();
	if (framerateReturnType == FRAMERATE_SCREEN_RETURN_ABORT) {
		exitGame();
	}

	setMemoryHandlerCompressionActive();
	initClipboardForGame();
	setScreenEffectZ(99);
	loadMugenConfig();
	setScreenAfterWrapperLogoScreen(getDreamTitleScreen());
	
#ifdef DEVELOP	
	//setVolume(0);
	//setSoundEffectVolume(0);
	// setDisplayedScreenSize(320, 240);
	disableWrapperErrorRecovery();
	initDolmexicaDebug();
#endif

	startScreenHandling(getDreamTitleScreen());
	
	exitGame();
	
	return 0;
}
