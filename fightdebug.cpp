#include "fightdebug.h"

#include <prism/mugentexthandler.h>
#include <prism/input.h>
#include <prism/wrapper.h>
#include <prism/drawing.h>
#include <prism/system.h>

#include "playerdefinition.h"
#include "stage.h"
#include "gamelogic.h"
#include "fightui.h"
#include "dolmexicadebug.h"
#include "config.h"
#include "trainingmodemenu.h"

#define DEBUG_Z 79

#define PLAYER_TEXT_AMOUNT 4

typedef struct {
	int mActive;
	int mTargetIndex;

	Position mBasePosition;
	DreamPlayer* mTarget;
	int mTextIDs[PLAYER_TEXT_AMOUNT];

} PlayerDebugData;

static struct {
	PlayerDebugData mPlayer;
	int mTextColorStep;

	int mSpeedLevel;
	int mIsTimeFrozen;
} gFightDebugData;

static void loadPlayerDebugData(const Position& tBasePosition, MugenTextAlignment tAlignment) {
	PlayerDebugData* e = &gFightDebugData.mPlayer;
	e->mBasePosition = tBasePosition;

	Position pos = e->mBasePosition;
	double dy = 6;

	char text[3];
	text[0] = '\0';

	int j;
	for (j = PLAYER_TEXT_AMOUNT - 1; j >= 0; j--) {
		e->mTextIDs[j] = addMugenText(text, pos, -1);
		setMugenTextAlignment(e->mTextIDs[j], tAlignment);
		pos.y -= dy;
	}
}



static void setSpeedLevel();
static void setDebugTextColor();

static void loadFightDebug(void* tData) {
	(void)tData;
	setProfilingSectionMarkerCurrentFunction();

	loadPlayerDebugData(makePosition(5, 235, DEBUG_Z), MUGEN_TEXT_ALIGNMENT_LEFT);

	setSpeedLevel();
	setDebugTextColor();
}

static void unloadFightDebug(void* tData) {
	(void)tData;
	setProfilingSectionMarkerCurrentFunction();

	if (!isDebugOverridingTimeDilatation()) {
		setWrapperTimeDilatation(getConfigGameSpeedTimeFactor());
	}
}

static void setPlayerTextActive() {
	gFightDebugData.mPlayer.mTarget = getRootPlayer(0);
}

static void setDebugTextActive() {
	setPlayerTextActive();
	gFightDebugData.mPlayer.mActive = 1;
}

static void setPlayerTextInactive() {
	PlayerDebugData* e = &gFightDebugData.mPlayer;
	
	char text[3];
	text[0] = '\0';
	int j;
	for (j = 0; j < PLAYER_TEXT_AMOUNT; j++) {
		changeMugenText(e->mTextIDs[j], text);
	}
}

static void setDebugTextInactive() {
	setPlayerTextInactive();

	gFightDebugData.mPlayer.mActive = 0;
}

static void setTargetPlayer() {
	gFightDebugData.mPlayer.mTarget = getPlayerByIndex(gFightDebugData.mPlayer.mTargetIndex);
}

void setFightDebugToPlayerOneBeforeFight()
{
	gFightDebugData.mPlayer.mTargetIndex = 0;
	gFightDebugData.mPlayer.mTarget = getRootPlayer(0);
	gFightDebugData.mPlayer.mActive = 1;
}

void switchFightDebugTextActivity() {
	if (gFightDebugData.mPlayer.mActive) {
		gFightDebugData.mPlayer.mTargetIndex++;
		if (gFightDebugData.mPlayer.mTargetIndex >= getTotalPlayerAmount()) {
			setDebugTextInactive();
		}
		else {
			setTargetPlayer();
		}

	}
	else {
		gFightDebugData.mPlayer.mTargetIndex = 0;
		setTargetPlayer();
		setDebugTextActive();
	}
}

static void setSpeedLevel() {
	if (isDebugOverridingTimeDilatation()) return;

	if (gFightDebugData.mIsTimeFrozen) {
		pauseWrapper();
		return;
	}
	else {
		resumeWrapper();
	}

	switch (gFightDebugData.mSpeedLevel) {
	case 0:
		setWrapperTimeDilatation(1);
		break;
	case 1:
		setWrapperTimeDilatation(0.5);
		break;
	case 2:
		setWrapperTimeDilatation(0.1);
		break;
	case 3:
		setWrapperTimeDilatation(1 / 60.0f);
		break;
	default:
		break;
	}
}

static void switchDebugTimeDilatation() {
	gFightDebugData.mSpeedLevel = (gFightDebugData.mSpeedLevel + 1) % 4;
	setSpeedLevel();
}

void switchDebugTimeOff() {
	gFightDebugData.mIsTimeFrozen ^= 1;
	if (getGameMode() == GAME_MODE_TRAINING) {
		setTrainingModeMenuVisibility(gFightDebugData.mIsTimeFrozen);
	}
	setSpeedLevel();
}

static void setPlayerTextColor(double tR, double tG, double tB) {
	PlayerDebugData* e = &gFightDebugData.mPlayer;

	int j;
	for (j = 0; j < PLAYER_TEXT_AMOUNT; j++) {
		setMugenTextColorRGB(e->mTextIDs[j], tR, tG, tB);
	}
}

static void setDebugTextColor() {
	double r, g, b;
	if (gFightDebugData.mTextColorStep == 0) {
		r = g = b = 0;
	}
	else if (gFightDebugData.mTextColorStep == 1) {
		r = g = b = 1;
	}
	else {
		r = 1;
		g = b = 0;
	}

	setPlayerTextColor(r, g, b);
}

static void switchDebugTextColor() {
	gFightDebugData.mTextColorStep = (gFightDebugData.mTextColorStep + 1) % 3;
	setDebugTextColor();
}

void switchFightCollisionDebugActivity() {
	setPlayerCollisionDebug(!isPlayerCollisionDebugActive());
}

static void saveDolmexicaScreenshot() {
	std::string title = getGameTitle();
	removeInvalidFileNameElementsFromString(title);
	for (int i = 1; i < 1000; i++) {
		std::stringstream ss;
		ss << title << "_" << i << ".png";
		if (!isFile(ss.str().c_str())) {
			saveScreenShot(ss.str().c_str());
			break;
		}
	}
}

static void updateDebugInputWindows() {
	if (isMugenDebugAllowingDebugModeSwitch() && hasPressedKeyboardMultipleKeyFlank(2, KEYBOARD_CTRL_LEFT_PRISM, KEYBOARD_D_PRISM)) {
		switchFightDebugTextActivity();
	}

	if (!gFightDebugData.mPlayer.mActive && !isMugenDebugAllowingDebugKeysOutsideDebugMode()) {
		return;
	}

	if (hasPressedKeyboardMultipleKeyFlank(2, KEYBOARD_SHIFT_LEFT_PRISM, KEYBOARD_D_PRISM)) {
		switchDebugTextColor();
	}

	if (hasPressedKeyboardMultipleKeyFlank(2, KEYBOARD_CTRL_LEFT_PRISM, KEYBOARD_C_PRISM)) {
		switchFightCollisionDebugActivity();
	}

	if (hasPressedKeyboardMultipleKeyFlank(2, KEYBOARD_CTRL_LEFT_PRISM, KEYBOARD_F1_PRISM)) {
		setPlayerLife(getRootPlayer(0), getRootPlayer(0), 0);
	}
	else if (hasPressedKeyboardKeyFlank(KEYBOARD_F1_PRISM)) {
		setPlayerLife(getRootPlayer(1), getRootPlayer(1), 0);
	}

	if (hasPressedKeyboardMultipleKeyFlank(2, KEYBOARD_CTRL_LEFT_PRISM, KEYBOARD_F2_PRISM)) {
		setPlayerLife(getRootPlayer(0), getRootPlayer(0), 1);
	}
	else if (hasPressedKeyboardKeyFlank(KEYBOARD_F2_PRISM)) {
		setPlayerLife(getRootPlayer(0), getRootPlayer(0), 1);
		setPlayerLife(getRootPlayer(1), getRootPlayer(1), 1);
	}

	if (hasPressedKeyboardMultipleKeyFlank(2, KEYBOARD_SHIFT_LEFT_PRISM, KEYBOARD_F2_PRISM)) {
		setPlayerLife(getRootPlayer(0), getRootPlayer(0), 1);
	}

	if (hasPressedKeyboardKeyFlank(KEYBOARD_F3_PRISM)) {
		setPlayerPower(getRootPlayer(0), getPlayerPowerMax(getRootPlayer(0)));
		setPlayerPower(getRootPlayer(1), getPlayerPowerMax(getRootPlayer(1)));
	}

	if (hasPressedKeyboardKeyFlank(KEYBOARD_F4_PRISM)) {
		resetRound();
	}

	if (hasPressedKeyboardMultipleKeyFlank(2, KEYBOARD_SHIFT_LEFT_PRISM, KEYBOARD_F4_PRISM)) {
		reloadFight();
	}

	if (hasPressedKeyboardKeyFlank(KEYBOARD_F5_PRISM)) {
		setTimerFinished();
	}

	if (hasPressedKeyboardMultipleKeyFlank(2, KEYBOARD_CTRL_LEFT_PRISM, KEYBOARD_F6_PRISM)) {
		switchDebugTimeDilatation();
	}

	if (hasPressedKeyboardMultipleKeyFlank(2, KEYBOARD_SHIFT_LEFT_PRISM, KEYBOARD_F6_PRISM)) {
		switchDebugTimeOff();
	}

	if (hasPressedKeyboardKeyFlank(KEYBOARD_F12_PRISM)) {
		saveDolmexicaScreenshot();
	}
}

static void updateDebugInputDreamcast() {
	int wasStartPressed = hasPressedStartFlank();

	if (isMugenDebugAllowingDebugModeSwitch() && wasStartPressed && hasPressedR()) {
		switchFightDebugTextActivity();
	} 

	if (!gFightDebugData.mPlayer.mActive && !isMugenDebugAllowingDebugKeysOutsideDebugMode()) {
		return;
	}

	if (hasPressedKeyboardMultipleKeyFlank(2, KEYBOARD_SHIFT_LEFT_PRISM, KEYBOARD_D_PRISM)) {
		switchDebugTextColor();
	}

	if (hasPressedKeyboardMultipleKeyFlank(2, KEYBOARD_CTRL_LEFT_PRISM, KEYBOARD_C_PRISM)) {
		switchFightCollisionDebugActivity();
	} 
	
	if (wasStartPressed && hasPressedL()) {
		setPlayerLife(getRootPlayer(1), getRootPlayer(1), 0);
	}

	if (hasPressedKeyboardMultipleKeyFlank(2, KEYBOARD_CTRL_LEFT_PRISM, KEYBOARD_F1_PRISM)) {
		setPlayerLife(getRootPlayer(0), getRootPlayer(0), 0);
	}

	if (hasPressedKeyboardKeyFlank(KEYBOARD_F2_PRISM)) {
		setPlayerLife(getRootPlayer(0), getRootPlayer(0), 1);
		setPlayerLife(getRootPlayer(1), getRootPlayer(1), 1);
	}

	if (hasPressedKeyboardMultipleKeyFlank(2, KEYBOARD_CTRL_LEFT_PRISM, KEYBOARD_F2_PRISM)) {
		setPlayerLife(getRootPlayer(0), getRootPlayer(0), 1);
	}

	if (hasPressedKeyboardMultipleKeyFlank(2, KEYBOARD_SHIFT_LEFT_PRISM, KEYBOARD_F2_PRISM)) {
		setPlayerLife(getRootPlayer(0), getRootPlayer(0), 1);
	}

	if (hasPressedKeyboardKeyFlank(KEYBOARD_F3_PRISM)) {
		setPlayerPower(getRootPlayer(0), getPlayerPowerMax(getRootPlayer(0)));
		setPlayerPower(getRootPlayer(1), getPlayerPowerMax(getRootPlayer(1)));
	}

	if (hasPressedKeyboardKeyFlank(KEYBOARD_F4_PRISM)) {
		resetRound();
	}

	if (hasPressedKeyboardMultipleKeyFlank(2, KEYBOARD_SHIFT_LEFT_PRISM, KEYBOARD_F4_PRISM)) {
		reloadFight();
	}

	if (hasPressedKeyboardKeyFlank(KEYBOARD_F5_PRISM)) {
		setTimerFinished();
	}

	if (hasPressedKeyboardMultipleKeyFlank(2, KEYBOARD_CTRL_LEFT_PRISM, KEYBOARD_PAUSE_PRISM)) {
		switchDebugTimeDilatation();
	}

	if (hasPressedKeyboardMultipleKeyFlank(2, KEYBOARD_SHIFT_LEFT_PRISM, KEYBOARD_PAUSE_PRISM)) {
		switchDebugTimeOff();
	}
}

static void updateDebugInput() {
	if(isOnDreamcast()) updateDebugInputDreamcast();
	else updateDebugInputWindows();
}

static void updateSingleDebugText() {
	PlayerDebugData* e = &gFightDebugData.mPlayer;
	DreamPlayer* player = gFightDebugData.mPlayer.mTarget;

	int j = 0;
	char text[1000];
	sprintf(text, "FRAMES: %d (%.1f FPS) VRET: 0, SPEED: 0, SKIP: A", getDreamGameTime(), getRealFramerate());
	changeMugenText(e->mTextIDs[j++], text);

	if (player) {
		sprintf(text, "%s %d (%d)", getPlayerDisplayName(player), gFightDebugData.mPlayer.mTargetIndex, getPlayerID(getPlayerRoot(player)));
		changeMugenText(e->mTextIDs[j++], text);
		sprintf(text, "ACTIONID: %d; SPR: %d,%d; ELEMNO: %d/%d; TIME: %d/%d", getPlayerAnimationNumber(player), getPlayerSpriteGroup(player), getPlayerSpriteElement(player), getPlayerAnimationStep(player) + 1, getPlayerAnimationStepAmount(player), getPlayerAnimationTime(player), getPlayerAnimationDuration(player));
		changeMugenText(e->mTextIDs[j++], text);
		sprintf(text, "STATE NO: %d; CTRL: %d; TYPE: %d; MOVETYPE %d; TIME: %d", getPlayerState(player), getPlayerControl(player), getPlayerStateType(player), getPlayerStateMoveType(player), getPlayerTimeInState(player));
		changeMugenText(e->mTextIDs[j++], text);
	}
	else {
		sprintf(text, "DESTROYED %d", gFightDebugData.mPlayer.mTargetIndex);
		changeMugenText(e->mTextIDs[j++], text);
		sprintf(text, " ");
		changeMugenText(e->mTextIDs[j++], text);
		sprintf(text, " ");
		changeMugenText(e->mTextIDs[j++], text);
	}
}

static void updateTarget() {
	if (!isPlayer(gFightDebugData.mPlayer.mTarget)) gFightDebugData.mPlayer.mTarget = NULL;
}

static void updateDebugText() {
	if (!gFightDebugData.mPlayer.mActive) return;

	updateTarget();
	updateSingleDebugText();
}

static void updateFightDebug(void* /*tData*/) {
	setProfilingSectionMarkerCurrentFunction();
	updateDebugInput();
	updateDebugText();
}

ActorBlueprint getFightDebug() {
	return makeActorBlueprint(loadFightDebug, unloadFightDebug, updateFightDebug);
};
