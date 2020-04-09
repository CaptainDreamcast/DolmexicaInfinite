#include <gtest/gtest.h>

#define Rectangle Rectangle2
#include <Windows.h>
#undef Rectangle

#include <prism/wrapper.h>
#include <prism/file.h>

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
	WIN32_FIND_DATAA ffd;
	HANDLE hFind = FindFirstFileA(folder, &ffd);
	if (hFind != INVALID_HANDLE_VALUE) {
		do {
			if (strcmp(ffd.cFileName, ".") == 0 || strcmp(ffd.cFileName, "..") == 0) continue;
			if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
				testCharacter(ffd.cFileName);
			}
		} while (FindNextFileA(hFind, &ffd) != 0);
		FindClose(hFind);
	}
}

TEST_F(CrashTest, CharacterCrashTest) {
	processCharacterAssets("assets\\chars\\*");
}

static void testStage(const std::string& tStageDefPath) {
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
	const auto searchString = folder + "*";
	WIN32_FIND_DATAA ffd;
	HANDLE hFind = FindFirstFileA(searchString.c_str(), &ffd);
	if (hFind != INVALID_HANDLE_VALUE) {
		do {
			if (strcmp(ffd.cFileName, ".") == 0 || strcmp(ffd.cFileName, "..") == 0) continue;
			if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
				processStageAssetsRecursively(folder + ffd.cFileName + "\\");
			}
			else {
				if (!hasFileExtension(ffd.cFileName)) continue;
				const auto extension = getFileExtension(ffd.cFileName);
				if (std::string(extension) != "def") continue;
				testStage(folder + ffd.cFileName);
			}
		} while (FindNextFileA(hFind, &ffd) != 0);
		FindClose(hFind);
	}
}

TEST_F(CrashTest, StageCrashTest) {
	processStageAssetsRecursively("assets\\stages\\");
}