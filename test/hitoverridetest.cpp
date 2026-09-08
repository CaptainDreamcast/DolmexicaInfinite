#include <gtest/gtest.h>

#include <string>

#include <prism/wrapper.h>
#include <prism/math.h>

#include "commontestfunctionality.h"

#include "config.h"
#include "playerdefinition.h"
#include "stage.h"
#include "gamelogic.h"
#include "fightscreen.h"

/* Mukin (Potemkin) has an armor HitOverride in state -3 that redirects every
 * standing hit into state 9900, which changes back to state 0 immediately.
 * Sagat's Tiger Uppercut (state 1300) launches with ground.velocity = -3,-7.
 * If the engine applies those get-hit velocities anyway, Mukin leaves the
 * ground in a standing state, where nothing adds gravity, and floats up
 * forever. */

class HitOverrideTest : public ::testing::Test {
protected:
	void SetUp() override { setupTestForScreenTestInAssetsFolder(); }
	void TearDown() override { tearDownTestForScreenTestInAssetsFolder(); }
};

static const int ARMOR_TEST_ATTACKER_STATE = 1300;
static const int ARMOR_TEST_HIT_FRAME = 200;
static const int ARMOR_TEST_FRAME_AMOUNT = 400;

static std::string getCharacterDefinitionPath(const std::string& tName) {
	return getDolmexicaAssetFolder() + "chars/" + tName + "/" + tName + ".def";
}

static void loadArmorTestFight(const std::string& tAttacker, const std::string& tVictim) {
	initForAutomatedFightScreenTest();
	setPlayerDefinitionPath(0, getCharacterDefinitionPath(tAttacker).c_str());
	setPlayerDefinitionPath(1, getCharacterDefinitionPath(tVictim).c_str());
	setDreamStageMugenDefinition((getDolmexicaAssetFolder() + "stages/kfm.def").c_str(), "");
	setDifficulty(1);
	setGameModeSuperWatch();
	initPrismWrapperScreenForDebug(getDreamFightScreenForTesting());
	setRandomSeed(1);
}

static void forceAttackOnVictim(DreamPlayer* tAttacker, DreamPlayer* tVictim) {
	setPlayerPositionY(tAttacker, 0.0f, 320);
	setPlayerPositionY(tVictim, 0.0f, 320);
	setPlayerPositionX(tAttacker, getPlayerPositionX(tVictim, 320) - 30.0f, 320);
	changePlayerState(tAttacker, ARMOR_TEST_ATTACKER_STATE);
}

static int isPlayerFloatingInGroundState(DreamPlayer* tPlayer) {
	if (getPlayerStateType(tPlayer) == MUGEN_STATE_TYPE_AIR) return 0;
	return getPlayerPositionY(tPlayer, 320) < 0.0f;
}

static int countFloatingFramesAfterArmoredUppercut() {
	loadArmorTestFight("Sagat", "Mukin");

	int floatingFrames = 0;
	for (int frame = 0; frame < ARMOR_TEST_FRAME_AMOUNT; frame++) {
		updatePrismWrapperScreenForDebugWithIterations(1);
		DreamPlayer* attacker = getRootPlayer(0);
		DreamPlayer* victim = getRootPlayer(1);
		if (!attacker || !victim) continue;

		if (frame == ARMOR_TEST_HIT_FRAME) {
			forceAttackOnVictim(attacker, victim);
		}
		if (frame > ARMOR_TEST_HIT_FRAME && isPlayerFloatingInGroundState(victim)) {
			floatingFrames++;
		}
	}

	unloadPrismWrapperScreenForDebug();
	return floatingFrames;
}

TEST_F(HitOverrideTest, ArmoredVictimKeepsItsVelocity) {
	ASSERT_EQ(countFloatingFramesAfterArmoredUppercut(), 0);
}
