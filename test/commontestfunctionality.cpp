#include "commontestfunctionality.h"

#define Rectangle Rectangle2
#include <Windows.h>
#undef Rectangle

#include <prism/wrapper.h>
#include <prism/mugentexthandler.h>
#include <prism/sound.h>
#include <prism/soundeffect.h>
#include <prism/debug.h>
#include <prism/log.h>

#include "config.h"
#include "dolmexicadebug.h"

void setupTestForScreenTestInAssetsFolder()
{
	setDevelopMode();
	setMinimumLogType(LOG_TYPE_NONE);
	initPrismWrapperWithMugenFlags();
	loadMugenConfig();
	loadGlobalVariables(PrismSaveSlot::AMOUNT);
	disableWrapperErrorRecovery();
	initDolmexicaDebug();
	loadMugenSystemFonts();
	setMemoryHandlerCompressionActive();
	setUnscaledGameWavVolume(0);
	setUnscaledGameMidiVolume(0);
}

void tearDownTestForScreenTestInAssetsFolder()
{
	shutdownPrismWrapper();
}

void initForAutomatedFightScreenTest()
{
	unloadMugenFonts();
	loadMugenFightFonts();
}
