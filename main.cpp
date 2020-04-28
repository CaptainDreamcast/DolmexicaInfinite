#include <prism/framerateselectscreen.h>
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
#include "dolmexicastoryscreen.h"
#include "stage.h"
#include "config.h"
#include "dolmexicadebug.h"
#include "initscreen.h"

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

int main(int argc, char** argv) {
	(void)argc;
	(void)argv;

#ifdef DEVELOP
	setDevelopMode();
	setMinimumLogType(isOnDreamcast() ? LOG_TYPE_NONE : LOG_TYPE_NORMAL);
#else
	setMinimumLogType(LOG_TYPE_NONE);
#endif

	setGameName("DOLMEXICA INFINITE");
	setScreenSize(320, 240);

	if (!isOnDreamcast()) {
		setMugenSpriteFileReaderSubTextureSplit(8, 1024);
	}

	initPrismWrapperWithMugenFlags();
	loadMugenConfig();
	loadGlobalVariables(PrismSaveSlot::AMOUNT);
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
	setScreenAfterWrapperLogoScreen(getInitScreen());
	
#ifdef DEVELOP	
	setUnscaledGameWavVolume(0);
	setUnscaledGameMidiVolume(0);
	// setDisplayedScreenSize(320, 240);
	disableWrapperErrorRecovery();
	initDolmexicaDebug();
#endif

	startScreenHandling(getInitScreen());
	
	exitGame();
	
	return 0;
}
