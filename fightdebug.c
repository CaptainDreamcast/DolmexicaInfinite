#include "fightdebug.h"

#include <prism/mugentexthandler.h>
#include <prism/input.h>
#include <prism/wrapper.h>

#include "playerdefinition.h"
#include "stage.h"

#define PLAYER_TEXT_AMOUNT 10

typedef struct {
	Position mBasePosition;

	int mTextIDs[PLAYER_TEXT_AMOUNT];

} PlayerDebugData;

static struct {
	PlayerDebugData mPlayer[2];
	int mTextActive;
	int mTextColorStep;

	int mSpeedLevel;
	int mIsTimeFrozen;
} gData;

static void loadPlayerDebugData(int i, Position tBasePosition, MugenTextAlignment tAlignment) {
	PlayerDebugData* e = &gData.mPlayer[i];
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
	loadPlayerDebugData(0, makePosition(5, 235, 80), MUGEN_TEXT_ALIGNMENT_LEFT);
	loadPlayerDebugData(1, makePosition(315, 235, 80), MUGEN_TEXT_ALIGNMENT_RIGHT);

	setSpeedLevel();
	setDebugTextColor();
}

static void unloadFightDebug(void* tData) {
	(void)tData;
	setWrapperTimeDilatation(1);
}

static void setDebugTextActive() {
	gData.mTextActive = 1;
}

static void setPlayerTextInactive(int i) {
	PlayerDebugData* e = &gData.mPlayer[i];
	
	char text[3];
	text[0] = '\0';
	int j;
	for (j = 0; j < PLAYER_TEXT_AMOUNT; j++) {
		changeMugenText(e->mTextIDs[j], text);
	}
}

static void setDebugTextInactive() {
	setPlayerTextInactive(0);
	setPlayerTextInactive(1);

	gData.mTextActive = 0;
}

static void switchDebugTextActivity() {
	if (gData.mTextActive) setDebugTextInactive();
	else setDebugTextActive();
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

static void setPlayerTextColor(int i, double tR, double tG, double tB) {
	PlayerDebugData* e = &gData.mPlayer[i];

	int j;
	for (j = 0; j < PLAYER_TEXT_AMOUNT; j++) {
		setMugenTextColorRGB(e->mTextIDs[j], tR, tG, tB);
	}
}

static void setDebugTextColor() {
	double r, g, b;
	if (gData.mTextColorStep == 0) {
		r = g = b = 1;
	}
	else if (gData.mTextColorStep == 1) {
		r = g = b = 0;
	}
	else {
		r = 1;
		g = b = 0;
	}

	setPlayerTextColor(0, r, g, b);
	setPlayerTextColor(1, r, g, b);
}

static void switchDebugTextColor() {
	gData.mTextColorStep = (gData.mTextColorStep + 1) % 3;
	setDebugTextColor();
}

static void updateDebugInput() {
	if (hasPressedKeyboardKeyFlank(KEYBOARD_F2_PRISM)) {
		switchDebugTextActivity();
	} 

	if (hasPressedKeyboardKeyFlank(KEYBOARD_F3_PRISM)) {
		switchDebugTextColor();
	}

	if (hasPressedKeyboardKeyFlank(KEYBOARD_F4_PRISM)) {
		switchDebugTimeDilatation();
	}

	if (hasPressedKeyboardKeyFlank(KEYBOARD_F5_PRISM)) {
		switchDebugTimeOff();
	}
}

static void updateSingleDebugText(int i) {
	PlayerDebugData* e = &gData.mPlayer[i];
	DreamPlayer* player = getRootPlayer(i);

	int j = 0;

	char text[1000];
	strcpy(text, "Type: Player");
	changeMugenText(e->mTextIDs[j++], text);
	sprintf(text, "ID: %d", getPlayerID(player));
	changeMugenText(e->mTextIDs[j++], text);
	sprintf(text, "Control: %d", getPlayerControl(player));
	changeMugenText(e->mTextIDs[j++], text);
	sprintf(text, "Position: %.2f %.2f", getPlayerPositionX(player, getDreamStageCoordinateP()), getPlayerPositionY(player, getDreamStageCoordinateP()));
	changeMugenText(e->mTextIDs[j++], text);
	sprintf(text, "State: %d; Time in state: %d", getPlayerState(player), getPlayerTimeInState(player));
	changeMugenText(e->mTextIDs[j++], text);
	sprintf(text, "Animation: %d", getPlayerAnimationNumber(player));
	changeMugenText(e->mTextIDs[j++], text);
	sprintf(text, "Time left in animation: %d", getRemainingPlayerAnimationTime(player));
	changeMugenText(e->mTextIDs[j++], text);
	sprintf(text, "Total helper amount: %d", getPlayerTotalHelperAmount(player));
	changeMugenText(e->mTextIDs[j++], text);
	sprintf(text, "Helper amount: %d", getPlayerHelperAmount(player));
	changeMugenText(e->mTextIDs[j++], text);
	sprintf(text, "Projectile amount: %d", getPlayerProjectileAmount(player));
	changeMugenText(e->mTextIDs[j++], text);
}

static void updateDebugText() {
	if (!gData.mTextActive) return;

	updateSingleDebugText(0);
	updateSingleDebugText(1);
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