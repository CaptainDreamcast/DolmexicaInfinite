#include "commontestfunctionality.h"

#include <algorithm>
#include <cstdlib>

#include <prism/wrapper.h>
#include <prism/mugenanimationhandler.h>
#include <prism/mugenspritefilereader.h>
#include <prism/mugentexthandler.h>
#include <prism/screeneffect.h>
#include <prism/sound.h>
#include <prism/soundeffect.h>
#include <prism/system.h>
#include <prism/debug.h>
#include <prism/log.h>

#include "config.h"
#include "dolmexicadebug.h"

static void setupTestEngineLikeTheGame()
{
	setScreenSize(320, 240);
	setMugenSpriteFileReaderSubTextureSplit(8, 1024);
	initPrismWrapperWithMugenFlags();
	loadMugenConfig();
	loadGlobalVariables(PrismSaveSlot::AMOUNT);
	loadMugenSystemFonts();
	setScreenEffectZ(99);
	setMugenAnimationHandlerPixelCenter(Vector2D(0.0, 0.0));
	setMemoryHandlerCompressionActive();
	setSoundEffectCompression(1);
}

void setupTestForScreenTestInAssetsFolder()
{
	setDevelopMode();
	setMinimumLogType(LOG_TYPE_NONE);
	setPrismDebugUserScriptEnabled(0);
	setupTestEngineLikeTheGame();
	disableWrapperErrorRecovery();
	setDebugMinusCheckEnabled(1);
	initDolmexicaDebug();
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

static std::string turnTestAssetNameLowercase(const std::string& tText) {
	std::string ret = tText;
	std::transform(ret.begin(), ret.end(), ret.begin(), [](unsigned char c) { return (char)tolower(c); });
	return ret;
}

int isAssetIncludedInTestRun(const std::string& tAssetName)
{
	const char* filterEnv = getenv("DOLMEXICA_ASSET_FILTER");
	if (!filterEnv || !*filterEnv) return 1;

	return turnTestAssetNameLowercase(tAssetName).find(turnTestAssetNameLowercase(filterEnv)) != std::string::npos;
}
