#include <gtest/gtest.h>

#include <prism/system.h>
#include <prism/log.h>

#include "commontestfunctionality.h"

#include "titlescreen.h"
#include "config.h"
#include "gamelogic.h"
#include "playerdefinition.h"
#include "stage.h"
#include "fightscreen.h"

class PerformanceTest : public ::testing::Test {
protected:
	void SetUp() override {
		setupTestForScreenTestInAssetsFolder();
	}

	void TearDown() override {
		tearDownTestForScreenTestInAssetsFolder();
	}
};

static const auto PERFORMANCE_TEST_ITERATIONS = 1000;
static const auto PERFORMANCE_EPSILON = 2.0 * 1e-2;

static void performanceTestScreen(Screen* tScreen, double tExpectedAverageTimerPerFrame) {
	initPrismWrapperScreenForDebug(tScreen);
	const auto startTime = getSystemTicks();
	updatePrismWrapperScreenForDebugWithIterations(PERFORMANCE_TEST_ITERATIONS);
	const auto endTime = getSystemTicks();
	const auto delta = endTime - startTime;
	const auto averageTimePerFrame = delta / double(PERFORMANCE_TEST_ITERATIONS);
	unloadPrismWrapperScreenForDebug();
	logFormat("Average frame performance: %f", averageTimePerFrame);
	ASSERT_NEAR(averageTimePerFrame, tExpectedAverageTimerPerFrame, PERFORMANCE_EPSILON);
}

TEST_F(PerformanceTest, IdleTitleScreen) {
	const auto screen = getDreamTitleScreen();
	performanceTestScreen(screen, 0.12);
}

TEST_F(PerformanceTest, IdleFightScreen) {
	initForAutomatedFightScreenTest();
	setPlayerDefinitionPath(0, (getDolmexicaAssetFolder() + "chars/kfm/kfm.def").c_str());
	setPlayerDefinitionPath(1, (getDolmexicaAssetFolder() + "chars/kfm/kfm.def").c_str());
	setDreamStageMugenDefinition((getDolmexicaAssetFolder() + "stages/kfm.def").c_str(), "");
	setGameModeTraining();
	const auto screen = getDreamFightScreenForTesting();
	performanceTestScreen(screen, 0.540);
}