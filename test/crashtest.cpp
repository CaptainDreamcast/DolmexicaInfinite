#include <gtest/gtest.h>

#include <filesystem>

#include <prism/wrapper.h>
#include <prism/file.h>
#include <prism/system.h>
#include <prism/log.h>

#include "commontestfunctionality.h"

#include "config.h"
#include "playerdefinition.h"
#include "stage.h"
#include "gamelogic.h"
#include "fightscreen.h"

class CrashTest : public ::testing::Test {
protected:
	void SetUp() override {
		setupTestForScreenTestInAssetsFolder();
	}

	void TearDown() override {
		tearDownTestForScreenTestInAssetsFolder();
	}
};

static void testCharacter(const std::string& tCharacterName) {
	setMinimumLogType(LOG_TYPE_NORMAL);
	logFormat("Testing %s", tCharacterName.c_str());
	setMinimumLogType(LOG_TYPE_ERROR);

	updateGameName(("Testing " + tCharacterName).c_str());
	initForAutomatedFightScreenTest();
	setPlayerDefinitionPath(0, (getDolmexicaAssetFolder() + "chars/" + tCharacterName + "/" + tCharacterName + ".def").c_str());
	setPlayerDefinitionPath(1, (getDolmexicaAssetFolder() + "chars/" + tCharacterName + "/" + tCharacterName + ".def").c_str());
	setDreamStageMugenDefinition((getDolmexicaAssetFolder() + "stages/kfm.def").c_str(), "");
	setGameModeSuperWatch();
	const auto screen = getDreamFightScreenForTesting();
	initPrismWrapperScreenForDebug(screen);
	static const auto CRASH_TEST_CHARACTER_ITERATIONS = 60 * 60 * 3;
	updatePrismWrapperScreenForDebugWithIterations(CRASH_TEST_CHARACTER_ITERATIONS);
	unloadPrismWrapperScreenForDebug();
}

static void processCharacterAssets(const char* folder)
{
	std::error_code ec;
	for (const auto& entry : std::filesystem::directory_iterator(folder, ec)) {
		if (!entry.is_directory()) continue;
		const auto characterName = entry.path().filename().string();
		if (!isAssetIncludedInTestRun(characterName)) continue;
		testCharacter(characterName);
	}
}

TEST_F(CrashTest, CharacterCrashTest) {
	processCharacterAssets((getDolmexicaAssetFolder() + "chars").c_str());
}

static void testStage(const std::string& tStageDefPath) {
	updateGameName(("Testing " + tStageDefPath).c_str());
	initForAutomatedFightScreenTest();
	setPlayerDefinitionPath(0, (getDolmexicaAssetFolder() + "chars/kfm/kfm.def").c_str());
	setPlayerDefinitionPath(1, (getDolmexicaAssetFolder() + "chars/kfm/kfm.def").c_str());
	std::string cleanedStageDef = tStageDefPath;
	cleanPathSlashes(cleanedStageDef);
	setDreamStageMugenDefinition(cleanedStageDef.c_str(), "");
	setGameModeTraining();
	const auto screen = getDreamFightScreenForTesting();
	initPrismWrapperScreenForDebug(screen);
	static const auto CRASH_TEST_STAGE_ITERATIONS = 60 * 30;
	updatePrismWrapperScreenForDebugWithIterations(CRASH_TEST_STAGE_ITERATIONS);
	unloadPrismWrapperScreenForDebug();
}

static void processStageAssetsRecursively(const std::string& folder)
{
	std::error_code ec;
	for (const auto& entry : std::filesystem::recursive_directory_iterator(folder, ec)) {
		if (!entry.is_regular_file()) continue;
		if (entry.path().extension() != ".def") continue;
		if (!isAssetIncludedInTestRun(entry.path().generic_string())) continue;
		testStage(entry.path().generic_string());
	}
}

TEST_F(CrashTest, StageCrashTest) {
	processStageAssetsRecursively(getDolmexicaAssetFolder() + "stages/");
}