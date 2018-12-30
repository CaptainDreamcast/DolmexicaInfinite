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

#include "titlescreen.h"
#include "fightscreen.h"
#include "playerdefinition.h"
#include "warningscreen.h"
#include "dolmexicastoryscreen.h"
#include "stage.h"
#include "config.h"

char romdisk_buffer[1];
int romdisk_buffer_length;

#define DEVELOP

#ifdef DREAMCAST
KOS_INIT_FLAGS(INIT_DEFAULT);

extern uint8 romdisk[];
KOS_INIT_ROMDISK(romdisk);

#endif

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
	
	initPrismWrapperWithMugenFlags();
	
	setMainFileSystem();	
	setFont("$/rd/fonts/segoe.hdr", "$/rd/fonts/segoe.pkg");
	loadMugenTextHandler();
	loadMugenSystemFonts();
	logg("Check framerate");
	FramerateSelectReturnType framerateReturnType = selectFramerate();
	if (framerateReturnType == FRAMERATE_SCREEN_RETURN_ABORT) {
		exitGame();
	}
	
#ifdef DEVELOP	
	setVolume(0);
	// setDisplayedScreenSize(320, 240);
	 disableWrapperErrorRecovery();
#endif

	setMemoryHandlerCompressionActive();
	initClipboardForGame();
	setScreenEffectZ(99);
	loadMugenConfig();
	setScreenAfterWrapperLogoScreen(getDreamTitleScreen());

	startScreenHandling(getDreamWarningScreen());
	
	exitGame();
	
	return 0;
}
