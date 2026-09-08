#include <gtest/gtest.h>

#include <cstdio>
#include <cstdlib>

#include <prism/wrapper.h>
#include <prism/log.h>
#include <prism/math.h>

#include "commontestfunctionality.h"

#include "config.h"
#include "playerdefinition.h"
#include "stage.h"
#include "gamelogic.h"
#include "fightscreen.h"
#include "fightdeterminism.h"

/* Records a superwatch (AI vs AI) match's input masks + per-frame fight
 * state checksums, then replays it on a freshly loaded fight screen and
 * asserts the checksum streams match frame-for-frame.
 * Configuration via environment:
 *   DOLMEXICA_CHAR=kfm               character folder/def name (default kfm)
 *   DOLMEXICA_STAGE=stages/kfm.def   stage def path (default kfm)
 *   DOLMEXICA_SEEDS=1+4              first seed + amount
 *   DOLMEXICA_FRAMES=7200            frames per seed
 *   DOLMEXICA_REPLAY=path            where the replay file is written
 *   DOLMEXICA_REPLAY_VERIFY=path     verify an existing replay file from a
 *                                    previous process run (cross-process
 *                                    determinism check) instead of skipping */

class DeterminismTest : public ::testing::Test {
protected:
	void SetUp() override {
		setupTestForScreenTestInAssetsFolder();
	}

	void TearDown() override {
		tearDownTestForScreenTestInAssetsFolder();
	}
};

static std::string getDeterminismCharDefPath() {
	const char* charEnv = getenv("DOLMEXICA_CHAR");
	const std::string name = charEnv ? charEnv : "kfm";
	return getDolmexicaAssetFolder() + "chars/" + name + "/" + name + ".def";
}

static std::string getDeterminismStageDefPath() {
	const char* stageEnv = getenv("DOLMEXICA_STAGE");
	return getDolmexicaAssetFolder() + (stageEnv ? stageEnv : "stages/kfm.def");
}

static void loadDeterminismFightScreen(unsigned int tSeed, const char* tCharDefPath0, const char* tCharDefPath1) {
	initForAutomatedFightScreenTest();
	setPlayerDefinitionPath(0, tCharDefPath0);
	setPlayerDefinitionPath(1, tCharDefPath1);
	setDreamStageMugenDefinition(getDeterminismStageDefPath().c_str(), "");
	setDifficulty(8);
	setGameModeSuperWatch();
	const auto screen = getDreamFightScreenForTesting();
	initPrismWrapperScreenForDebug(screen);
	setRandomSeed(tSeed);
}

static void recordSuperWatchMatch(const char* tReplayPath, unsigned int tSeed, int tFrames) {
	const auto charPath = getDeterminismCharDefPath();
	loadDeterminismFightScreen(tSeed, charPath.c_str(), charPath.c_str());
	ASSERT_TRUE(startFightInputRecording(tReplayPath));
	updatePrismWrapperScreenForDebugWithIterations(tFrames);
	stopFightInputRecording();
	unloadPrismWrapperScreenForDebug();
}

static void verifyReplayChecksums(const char* tReplayPath, unsigned int tSeed) {
	ASSERT_TRUE(startFightInputPlayback(tReplayPath));
	char charPath0[1024], charPath1[1024];
	getFightInputPlaybackPlayerDefinitionPath(charPath0, 0);
	getFightInputPlaybackPlayerDefinitionPath(charPath1, 1);
	loadDeterminismFightScreen(tSeed, charPath0, charPath1);

	const int frameAmount = getFightInputPlaybackFrameAmount();
	ASSERT_TRUE(frameAmount > 0);
	updatePrismWrapperScreenForDebugWithIterations(frameAmount);

	EXPECT_EQ(getFightInputPlaybackFrame(), frameAmount);
	EXPECT_EQ(getFightInputPlaybackChecksumMismatchAmount(), 0);
	if (getFightInputPlaybackChecksumMismatchAmount()) {
		printf("first divergence at frame %d of %d\n",
			getFightInputPlaybackFirstChecksumMismatchFrame(), frameAmount);
	}
	stopFightInputPlayback();
	unloadPrismWrapperScreenForDebug();
}

TEST_F(DeterminismTest, RecordAndReplayChecksum) {
	const char* seedsEnv = getenv("DOLMEXICA_SEEDS");
	const char* framesEnv = getenv("DOLMEXICA_FRAMES");
	const char* replayEnv = getenv("DOLMEXICA_REPLAY");
	const unsigned int seedStart = (unsigned int)(seedsEnv ? atoi(seedsEnv) : 1);
	const unsigned int seedEnd = seedStart + (unsigned int)((seedsEnv && strchr(seedsEnv, '+')) ? atoi(strchr(seedsEnv, '+') + 1) : 4) - 1;
	const int frames = framesEnv ? atoi(framesEnv) : 60 * 60 * 2;
	const std::string replayPathBase = replayEnv ? replayEnv : "determinism_test_replay.dxr";

	for (unsigned int seed = seedStart; seed <= seedEnd; seed++) {
		const std::string replayPath = replayPathBase + "." + std::to_string(seed);
		printf("=== determinism seed %u, %d frames ===\n", seed, frames);
		fflush(stdout);
		recordSuperWatchMatch(replayPath.c_str(), seed, frames);
		// Same seed as recording: part of the first frame runs before the input hook can restore the header's random state, so load-time and early-frame RNG consumption must start from the same seed
		verifyReplayChecksums(replayPath.c_str(), seed);
		printf("=== determinism seed %u done: %d mismatches ===\n", seed, getFightInputPlaybackChecksumMismatchAmount());
		fflush(stdout);
		if (!replayEnv) remove(replayPath.c_str());
	}
}

TEST_F(DeterminismTest, VerifyReplayFileCrossProcess) {
	const char* verifyEnv = getenv("DOLMEXICA_REPLAY_VERIFY");
	if (!verifyEnv) {
		printf("DOLMEXICA_REPLAY_VERIFY not set, skipping cross-process verification\n");
		return;
	}
	verifyReplayChecksums(verifyEnv, 98765);
}
