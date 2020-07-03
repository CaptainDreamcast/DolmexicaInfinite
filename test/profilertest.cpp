#include <gtest/gtest.h>

#include <prism/profiling.h>

#include "commontestfunctionality.h"

#include "config.h"
#include "gamelogic.h"
#include "playerdefinition.h"
#include "stage.h"
#include "fightscreen.h"
#include "mugenassignmentevaluator.h"

class ProfilerTest : public ::testing::Test {
protected:
	void SetUp() override {
		setupTestForScreenTestInAssetsFolder();
	}

	void TearDown() override {
		tearDownTestForScreenTestInAssetsFolder();
	}
};

TEST_F(ProfilerTest, TriggerProfiling) {
	initForAutomatedFightScreenTest();
	setPlayerDefinitionPath(0, (getDolmexicaAssetFolder() + "chars/kfm/kfm.def").c_str());
	setPlayerDefinitionPath(1, (getDolmexicaAssetFolder() + "chars/kfm/kfm.def").c_str());
	setDreamStageMugenDefinition((getDolmexicaAssetFolder() + "stages/kfm.def").c_str(), "");
	setGameModeTraining();
	const auto screen = getDreamFightScreenForTesting();
	initPrismWrapperScreenForDebug(screen);
	auto player = getRootPlayer(0);
	setActiveStateMachineCoordinateP(getPlayerCoordinateP(player));
	auto assignment = parseDreamMugenAssignmentFromString("(var(40):= var(40) +64*(var(40)&1 && command != \"holda\") +128*(var(40)&2 && command != \"holdb\") +256*(var(40)&4 && command != \"holdc\") +512*(var(40)&8 && command != \"holdx\") +1024*(var(40)&16 && command != \"holdy\") +2048*(var(40)&32 && command != \"holdz\")) - var(40)");
	std::string result;
	startProfilingCapture();
	evaluateDreamAssignmentAndReturnAsString(result, &assignment, player);
	stopProfilingCapture();
	saveProfilingCapture("TriggerProfiling");
	destroyDreamMugenAssignment(assignment);
	unloadPrismWrapperScreenForDebug();
}