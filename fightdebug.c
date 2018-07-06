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
} gData;

static void loadPlayerDebugData(Position tBasePosition, MugenTextAlignment tAlignment) {
	PlayerDebugData* e = &gData.mPlayer;
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

	loadPlayerDebugData(makePosition(5, 235, DEBUG_Z), MUGEN_TEXT_ALIGNMENT_LEFT);

	setSpeedLevel();
	setDebugTextColor();
}

static void unloadFightDebug(void* tData) {
	(void)tData;
	setWrapperTimeDilatation(1);
}

static void setPlayerTextActive() {
	gData.mPlayer.mTarget = getRootPlayer(0);
}

static void setDebugTextActive() {
	setPlayerTextActive();
	gData.mPlayer.mActive = 1;
}

static void setPlayerTextInactive() {
	PlayerDebugData* e = &gData.mPlayer;
	
	char text[3];
	text[0] = '\0';
	int j;
	for (j = 0; j < PLAYER_TEXT_AMOUNT; j++) {
		changeMugenText(e->mTextIDs[j], text);
	}
}

static void setDebugTextInactive() {
	setPlayerTextInactive();

	gData.mPlayer.mActive = 0;
}

static void setTargetPlayer() {
	gData.mPlayer.mTarget = getPlayerByIndex(gData.mPlayer.mTargetIndex);
}

static void switchDebugTextActivity() {
	if (gData.mPlayer.mActive) {
		gData.mPlayer.mTargetIndex++;
		if (gData.mPlayer.mTargetIndex >= getTotalPlayerAmount()) {
			setDebugTextInactive();
		}
		else {
			setTargetPlayer();
		}

	}
	else {
		gData.mPlayer.mTargetIndex = 0;
		setTargetPlayer();
		setDebugTextActive();
	}
}

static void setSpeedLevel() {
	if (gData.mIsTimeFrozen) {
		setWrapperTimeDilatation(1); // TODO
		return;
	}

	switch (gData.mSpeedLevel) {
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
	gData.mSpeedLevel = (gData.mSpeedLevel + 1) % 4;
	setSpeedLevel();
}

static void switchDebugTimeOff() {
	gData.mIsTimeFrozen ^= 1;
	setSpeedLevel();
}

static void setPlayerTextColor(double tR, double tG, double tB) {
	PlayerDebugData* e = &gData.mPlayer;

	int j;
	for (j = 0; j < PLAYER_TEXT_AMOUNT; j++) {
		setMugenTextColorRGB(e->mTextIDs[j], tR, tG, tB);
	}
}

static void setDebugTextColor() {
	double r, g, b;
	if (gData.mTextColorStep == 0) {
		r = g = b = 0;
	}
	else if (gData.mTextColorStep == 1) {
		r = g = b = 1;
	}
	else {
		r = 1;
		g = b = 0;
	}

	setPlayerTextColor(r, g, b);
}

static void switchDebugTextColor() {
	gData.mTextColorStep = (gData.mTextColorStep + 1) % 3;
	setDebugTextColor();
}

static void switchCollisionDebugActivity() {
	setPlayerCollisionDebug(!isPlayerCollisionDebugActive());
}



static void updateDebugInputWindows() {
	if (hasPressedKeyboardMultipleKeyFlank(2, KEYBOARD_CTRL_LEFT_PRISM, KEYBOARD_D_PRISM)) { // TODO: proper
		switchDebugTextActivity();
	} 

	if (hasPressedKeyboardMultipleKeyFlank(2, KEYBOARD_SHIFT_LEFT_PRISM, KEYBOARD_D_PRISM)) {
		switchDebugTextColor();
	}

	if (hasPressedKeyboardMultipleKeyFlank(2, KEYBOARD_CTRL_LEFT_PRISM, KEYBOARD_C_PRISM)) {
		switchCollisionDebugActivity();
	} 
	
	if (hasPressedKeyboardKeyFlank(KEYBOARD_F1_PRISM)) {
		setPlayerLife(getRootPlayer(1), 0);
	}

	if (hasPressedKeyboardMultipleKeyFlank(2, KEYBOARD_CTRL_LEFT_PRISM, KEYBOARD_F1_PRISM)) {
		setPlayerLife(getRootPlayer(0), 0);
	}

	if (hasPressedKeyboardKeyFlank(KEYBOARD_F2_PRISM)) {
		setPlayerLife(getRootPlayer(0), 1);
		setPlayerLife(getRootPlayer(1), 1);
	}

	if (hasPressedKeyboardMultipleKeyFlank(2, KEYBOARD_CTRL_LEFT_PRISM, KEYBOARD_F2_PRISM)) {
		setPlayerLife(getRootPlayer(0), 1);
	}

	if (hasPressedKeyboardMultipleKeyFlank(2, KEYBOARD_SHIFT_LEFT_PRISM, KEYBOARD_F2_PRISM)) {
		setPlayerLife(getRootPlayer(0), 1);
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

static void updateDebugInputDreamcast() {
	int wasStartPressed = hasPressedStartFlank(); // TODO: remove when fixed

	if (wasStartPressed && hasPressedR()) { // TODO: proper
		switchDebugTextActivity();
	} 

	if (hasPressedKeyboardMultipleKeyFlank(2, KEYBOARD_SHIFT_LEFT_PRISM, KEYBOARD_D_PRISM)) {
		switchDebugTextColor();
	}

	if (hasPressedKeyboardMultipleKeyFlank(2, KEYBOARD_CTRL_LEFT_PRISM, KEYBOARD_C_PRISM)) {
		switchCollisionDebugActivity();
	} 
	
	if (wasStartPressed && hasPressedL()) {
		setPlayerLife(getRootPlayer(1), 0);
	}

	if (hasPressedKeyboardMultipleKeyFlank(2, KEYBOARD_CTRL_LEFT_PRISM, KEYBOARD_F1_PRISM)) {
		setPlayerLife(getRootPlayer(0), 0);
	}

	if (hasPressedKeyboardKeyFlank(KEYBOARD_F2_PRISM)) {
		setPlayerLife(getRootPlayer(0), 1);
		setPlayerLife(getRootPlayer(1), 1);
	}

	if (hasPressedKeyboardMultipleKeyFlank(2, KEYBOARD_CTRL_LEFT_PRISM, KEYBOARD_F2_PRISM)) {
		setPlayerLife(getRootPlayer(0), 1);
	}

	if (hasPressedKeyboardMultipleKeyFlank(2, KEYBOARD_SHIFT_LEFT_PRISM, KEYBOARD_F2_PRISM)) {
		setPlayerLife(getRootPlayer(0), 1);
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
	PlayerDebugData* e = &gData.mPlayer;
	DreamPlayer* player = gData.mPlayer.mTarget;

	int j = 0;
	char text[1000];
	sprintf(text, "FRAMES: %d (%.1f FPS) VRET: 0, SPEED: 0, SKIP: A", getDreamGameTime(), getRealFramerate());
	changeMugenText(e->mTextIDs[j++], text);

	if (player) {
		sprintf(text, "%s %d (%d)", getPlayerDisplayName(player), gData.mPlayer.mTargetIndex, getPlayerID(getPlayerRoot(player)));
		changeMugenText(e->mTextIDs[j++], text);
		sprintf(text, "ACTIONID: %d; SPR: %d,%d; ELEMNO: %d/%d; TIME: %d/%d", getPlayerAnimationNumber(player), getPlayerSpriteGroup(player), getPlayerSpriteElement(player), getPlayerAnimationStep(player) + 1, getPlayerAnimationStepAmount(player), getPlayerAnimationTime(player), getPlayerAnimationDuration(player));
		changeMugenText(e->mTextIDs[j++], text);
		sprintf(text, "STATE NO: %d; CTRL: %d; TYPE: %d; MOVETYPE %d; TIME: %d", getPlayerState(player), getPlayerControl(player), getPlayerStateType(player), getPlayerStateMoveType(player), getPlayerTimeInState(player));
		changeMugenText(e->mTextIDs[j++], text);
	}
	else {
		sprintf(text, "DESTROYED %d", gData.mPlayer.mTargetIndex);
		changeMugenText(e->mTextIDs[j++], text);
		sprintf(text, " ");
		changeMugenText(e->mTextIDs[j++], text);
		sprintf(text, " ");
		changeMugenText(e->mTextIDs[j++], text);
	}
}

static void updateTarget() {
	if (!isPlayer(gData.mPlayer.mTarget)) gData.mPlayer.mTarget = NULL;
}

static void updateDebugText() {
	if (!gData.mPlayer.mActive) return;

	updateTarget();
	updateSingleDebugText();
}

static void updateFightDebug(void* tData) {
	updateDebugInput();
	updateDebugText();
}

ActorBlueprint FightDebug = {
	.mLoad = loadFightDebug,
	.mUnload = unloadFightDebug,
	.mUpdate = updateFightDebug,
};
