#include <gtest/gtest.h>

#include <vector>
#include <map>
#include <cstdio>
#include <cstdlib>

#include <prism/wrapper.h>
#include <prism/log.h>
#include <prism/math.h>
#include <prism/datastructures.h>

#include "commontestfunctionality.h"
#include "censuscommon.h"

#include "config.h"
#include "playerdefinition.h"
#include "stage.h"
#include "gamelogic.h"
#include "mugenexplod.h"
#include "fightscreen.h"

/* Run in superwatch mode and check entities (roots, helpers, projectiles) for weird behavior (might not fit for all characters and get adjusted as it goes along)
 * Flags:
 *   - any helper/projectile in a common gethit state (5000-5999) or
 *     showing a gethit/liedown anim (5000-5299)
 *   - any root stuck in the same gethit state for a long time
 *   - any root sitting far above the ground for a long time
 *   - entity count high-water marks and round transitions
 * Configuration via environment:
 *   DOLMEXICA_CHAR=Billy_Kane      character folder/def name (default kfm)
 *   DOLMEXICA_STAGE=stages/kfm.def stage def path (default kfm)
 *   DOLMEXICA_SEEDS=1+8            first seed + amount
 *   DOLMEXICA_FRAMES=32400         frames per seed
 *   DOLMEXICA_INJECT_STATES=...    comma list for StressInjection (else
 *                                  common normals/specials/supers that
 *                                  exist on the character are used)
 *   DOLMEXICA_TRACE_TARGETS=1      TargetState redirection traces
 *   DOLMEXICA_TRACE_PENDING=1      deferred-state-change traces */

class EntityCensusTest : public ::testing::Test {
protected:
	void SetUp() override {
		setupTestForScreenTestInAssetsFolder();
	}

	void TearDown() override {
		tearDownTestForScreenTestInAssetsFolder();
	}
};

static std::string getCensusCharName() {
	const char* charEnv = getenv("DOLMEXICA_CHAR");
	return charEnv ? charEnv : "kfm";
}

static std::string getCensusCharDefPath() {
	const auto name = getCensusCharName();
	return getDolmexicaAssetFolder() + "chars/" + name + "/" + name + ".def";
}

static std::string getCensusStageDefPath() {
	const char* stageEnv = getenv("DOLMEXICA_STAGE");
	return getDolmexicaAssetFolder() + (stageEnv ? stageEnv : "stages/kfm.def");
}

static int isGetHitState(int tState) { return tState >= 5000 && tState <= 5999; }
static int isGetHitAnim(int tAnim) { return tAnim >= 5000 && tAnim <= 5299; }

static int gAnomalies;
static int gFrame;
static int gRound;

static void printCensusAnomaliesAddedThisFrame(int tAddedAmount) {
	const auto& anomalies = getCensusAnomalies();
	for (size_t i = anomalies.size() - tAddedAmount; i < anomalies.size(); i++) {
		printf("ANOMALY frame %d round %d: %s\n", anomalies[i].mFrame, anomalies[i].mRound, anomalies[i].mText.c_str());
	}
	fflush(stdout);
}

static int runMirrorMatchCensus(unsigned int tSeed, int tIterations) {
	printf("=== seed %u (%s), %d frames ===\n", tSeed, getCensusCharName().c_str(), tIterations);
	fflush(stdout);

	initForAutomatedFightScreenTest();
	setPlayerDefinitionPath(0, getCensusCharDefPath().c_str());
	setPlayerDefinitionPath(1, getCensusCharDefPath().c_str());
	setDreamStageMugenDefinition(getCensusStageDefPath().c_str(), "");
	setDifficulty(8);
	setGameModeSuperWatch();
	const auto screen = getDreamFightScreenForTesting();
	initPrismWrapperScreenForDebug(screen);
	setRandomSeed(tSeed);

	gAnomalies = 0;
	gRound = 1;
	extern int gDolmexicaDeferCount;
	gDolmexicaDeferCount = 0;
	resetCensus();

	for (gFrame = 0; gFrame < tIterations; gFrame++) {
		updatePrismWrapperScreenForDebugWithIterations(1);

		const int round = getDreamRoundNumber();
		if (round != gRound) {
			printf("round %d -> %d at frame %d\n", gRound, round, gFrame);
			fflush(stdout);
			gRound = round;
		}

		const int addedAnomalies = updateCensusWithCurrentFrame(gFrame, gRound);
		if (addedAnomalies) {
			printCensusAnomaliesAddedThisFrame(addedAnomalies);
			gAnomalies += addedAnomalies;
		}

		if (gFrame && gFrame % 21600 == 0) {
			const int total = getTotalPlayerAmount();
			int explods = 0;
			for (int i = 0; i < total; i++) {
				DreamPlayer* p = getPlayerByIndex(i);
				if (p) explods += getExplodAmount(p);
			}
			printf("checkpoint frame %d (min %d): entities %d, explods %d\n", gFrame, gFrame / 3600, total, explods);
			fflush(stdout);
		}
	}

	for (const auto& dwell : getCensusHelperStateDwellsOfAtLeast(3600)) {
		printf("note: helper id %d spent %d frames total in state %d\n", dwell.mHelperID, dwell.mFrames, dwell.mState);
	}
	printf("activity: root gethit entries %d/%d, hitpause deferrals %d\n",
		getCensusRootGetHitEntryAmount(0), getCensusRootGetHitEntryAmount(1), gDolmexicaDeferCount);
	printf("=== seed %u done: max entities %d, anomalies %d ===\n", tSeed, getCensusMaximumEntityAmount(), gAnomalies);
	fflush(stdout);

	unloadPrismWrapperScreenForDebug();
	return gAnomalies;
}

TEST_F(EntityCensusTest, MirrorMatchCensus) {
	const char* seedsEnv = getenv("DOLMEXICA_SEEDS");
	const char* framesEnv = getenv("DOLMEXICA_FRAMES");
	const unsigned int seedStart = (unsigned int)(seedsEnv ? atoi(seedsEnv) : 1);
	const unsigned int seedEnd = seedStart + (unsigned int)((seedsEnv && strchr(seedsEnv, '+')) ? atoi(strchr(seedsEnv, '+') + 1) : 4) - 1;
	const int frames = framesEnv ? atoi(framesEnv) : 60 * 60 * 3;

	int totalAnomalies = 0;
	for (unsigned int seed = seedStart; seed <= seedEnd; seed++) {
		totalAnomalies += runMirrorMatchCensus(seed, frames);
	}
	ASSERT_EQ(totalAnomalies, 0);
}

// Force through supers/throws other special attack states to see if anything gets stuck
TEST_F(EntityCensusTest, StressInjection) {
	const char* seedsEnv = getenv("DOLMEXICA_SEEDS");
	const char* framesEnv = getenv("DOLMEXICA_FRAMES");
	const unsigned int seedStart = (unsigned int)(seedsEnv ? atoi(seedsEnv) : 1);
	const unsigned int seedEnd = seedStart + (unsigned int)((seedsEnv && strchr(seedsEnv, '+')) ? atoi(strchr(seedsEnv, '+') + 1) : 4) - 1;
	const int frames = framesEnv ? atoi(framesEnv) : 60 * 60 * 2;

	// Candidates for super moves/special states
	static const int INJECT_CANDIDATES[] = {
		200, 210, 220, 230, 240, 400, 410, 420, 430, 440, 600, 610, 620,
		1000, 1010, 1050, 1100, 1110, 1150, 1200, 1250, 1300, 1400,
		2000, 2100, 2200, 3000, 3100, 3200, 3250, 3251,
	};
	static const int INJECT_CANDIDATE_AMOUNT = sizeof(INJECT_CANDIDATES) / sizeof(INJECT_CANDIDATES[0]);

	int totalAnomalies = 0;
	for (unsigned int seed = seedStart; seed <= seedEnd; seed++) {
		printf("=== stress seed %u (%s) ===\n", seed, getCensusCharName().c_str());
		fflush(stdout);
		initForAutomatedFightScreenTest();
		setPlayerDefinitionPath(0, (getDolmexicaAssetFolder() + "chars/kfm/kfm.def").c_str());
		setPlayerDefinitionPath(1, (getDolmexicaAssetFolder() + "chars/kfm/kfm.def").c_str());
		setDreamStageMugenDefinition((getDolmexicaAssetFolder() + "stages/kfm.def").c_str(), "");
		setDifficulty(8);
	setGameModeSuperWatch();
		const auto screen = getDreamFightScreenForTesting();
		initPrismWrapperScreenForDebug(screen);
		setRandomSeed(seed);

		std::vector<int> injectStates;
		const char* injectEnv = getenv("DOLMEXICA_INJECT_STATES");
		if (injectEnv) {
			for (const char* c = injectEnv; *c;) {
				injectStates.push_back(atoi(c));
				const char* comma = strchr(c, ',');
				if (!comma) break;
				c = comma + 1;
			}
		}
		else {
			for (int i = 0; i < INJECT_CANDIDATE_AMOUNT; i++) {
				if (hasPlayerStateSelf(getRootPlayer(0), INJECT_CANDIDATES[i])) {
					injectStates.push_back(INJECT_CANDIDATES[i]);
				}
			}
		}
		if (seed == seedStart) {
			printf("injecting %d states\n", (int)injectStates.size());
			fflush(stdout);
		}

		gAnomalies = 0;
		gRound = 1;
		int nextInject[2] = { 60, 90 };
		long long helperGetHitFrames = 0;

		for (gFrame = 0; gFrame < frames; gFrame++) {
			updatePrismWrapperScreenForDebugWithIterations(1);

			for (int side = 0; side < 2; side++) {
				if (gFrame < nextInject[side]) continue;
				nextInject[side] = gFrame + randfromInteger(30, 90);
				DreamPlayer* p = getRootPlayer(side);
				// only inject if they're not down
				const int st = getPlayerState(p);
				if (st >= 5000 && st <= 5999) continue;
				if (injectStates.empty()) continue;
				const int target = injectStates[randfromInteger(0, (int)injectStates.size() - 1)];
				changePlayerState(p, target);
			}

			const int total = getTotalPlayerAmount();
			for (int i = 0; i < total; i++) {
				DreamPlayer* p = getPlayerByIndex(i);
				if (!p || !isPlayerHelper(p)) continue;
				const int state = getPlayerState(p);
				const int anim = getPlayerAnimationNumber(p);
				if (isGetHitState(state) || isGetHitAnim(anim)) {
					helperGetHitFrames++;
					if (helperGetHitFrames == 1 || helperGetHitFrames % 60 == 0) {
						printf("ANOMALY frame %d: helper(root=%d id=%d) in gethit state %d anim %d (frames %lld)\n",
							gFrame, p->mRootID, p->mID, state, anim, helperGetHitFrames);
						fflush(stdout);
						gAnomalies++;
					}
				}
			}
		}

		printf("=== stress seed %u done: anomalies %d ===\n", seed, gAnomalies);
		fflush(stdout);
		totalAnomalies += gAnomalies;
		unloadPrismWrapperScreenForDebug();
	}
	ASSERT_EQ(totalAnomalies, 0);
}
