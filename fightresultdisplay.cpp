#include "fightresultdisplay.h"

#include <prism/log.h>

#include <prism/mugentexthandler.h>
#include <prism/screeneffect.h>
#include <prism/animation.h>
#include <prism/system.h>

#include "playerdefinition.h"
#include "gamelogic.h"
#include "mugenstagehandler.h"


#define BLACK_BG_Z 33
#define DISPLAY_TEXT_Z 79


typedef enum {
	FIGHT_RESULT_STAGE_INACTIVE,
	FIGHT_RESULT_STAGE_ACTIVE
} FightResultStage;

static struct {
	int mIsEnabled;
	char mMessage[100];
	Vector3DI mFont; 
	Position mOffset;
	int mTextDisplayTime;
	int mLayerNo; 
	int mFadeInTime; 
	int mShowTime; 
	int mFadeOutTime; 
	int mIsShowingWinPose;

	FightResultStage mStage;
	int mIsDisplayUsed;

	TextureData mWhiteTexture;
	int mBlackBGAnimationID;
	int mNow;

	int mIsShowingText;
	int mTextID;

	void(*mNextScreenCB)(void*);
	void(*mTitleScreenCB)(void*);
} gData;

static void loadFightResultDisplay(void* tData) {
	(void)tData;
	gData.mWhiteTexture = getEmptyWhiteTexture();
	gData.mStage = FIGHT_RESULT_STAGE_INACTIVE;
}

static void updateTextDisplay() {
	if (!gData.mIsShowingText) return;

	if (gData.mTextDisplayTime != -1 && gData.mNow >= gData.mTextDisplayTime) {
		removeMugenText(gData.mTextID);
		gData.mIsShowingText = 0;
	}
}

static void updateDisplayOver() {
	if (gData.mNow >= gData.mShowTime) {
		if(getGameMode() == GAME_MODE_SURVIVAL) addFadeOut(gData.mFadeOutTime, gData.mTitleScreenCB, NULL);
		else addFadeOut(gData.mFadeOutTime, gData.mNextScreenCB, NULL);
		gData.mStage = FIGHT_RESULT_STAGE_INACTIVE;
	}
}

static void updateDisplayTime() {
	gData.mNow++;
}

static void updateFightResultDisplay(void* tData) {
	(void)tData;
	if (!gData.mStage) return;

	updateTextDisplay();
	updateDisplayOver();
	updateDisplayTime();
}

ActorBlueprint getFightResultDisplay() {
	return makeActorBlueprint(loadFightResultDisplay, NULL, updateFightResultDisplay);
};

void setFightResultActive(int tIsActive) {
	gData.mIsDisplayUsed = tIsActive;
}

void setFightResultData(int tIsEnabled, char* tMessage, Vector3DI tFont, Position tOffset, int tTextDisplayTime, int tLayerNo, int tFadeInTime, int tShowTime, int tFadeOutTime, int tIsShowingWinPose) {
	gData.mIsEnabled = tIsEnabled;
	sprintf(gData.mMessage, "%.99s", tMessage);
	gData.mFont = tFont;
	gData.mOffset = tOffset;
	gData.mTextDisplayTime = tTextDisplayTime;
	gData.mLayerNo = tLayerNo;
	gData.mFadeInTime = tFadeInTime;
	gData.mShowTime = tShowTime;
	gData.mFadeOutTime = tFadeOutTime;
	gData.mIsShowingWinPose = tIsShowingWinPose;

	gData.mIsDisplayUsed = 1;
}

void setFightResultMessage(char * tMessage)
{
	sprintf(gData.mMessage, "%.99s", tMessage);
}

void setFightResultIsShowingWinPose(int tIsShowingWinPose)
{
	gData.mIsShowingWinPose = tIsShowingWinPose;
}

int isShowingFightResults() {
	return gData.mIsDisplayUsed && gData.mIsEnabled;
}

static void movePlayersOutOfScreen() {
	int amount = getTotalPlayerAmount();
	int i;
	for (i = 0; i < amount; i++) {
		DreamPlayer* player = getPlayerByIndex(i);
		setPlayerPositionX(player, 161, getPlayerCoordinateP(player));
		setPlayerPositionY(player, -10000, getPlayerCoordinateP(player)); // TODO: think better
	}

}

static void fadeToResultFinishedCB(void* tData) {
	gData.mBlackBGAnimationID = playOneFrameAnimationLoop(makePosition(0, 0, BLACK_BG_Z), &gData.mWhiteTexture);
	setAnimationColor(gData.mBlackBGAnimationID, 0, 0, 0);
	setAnimationTransparency(gData.mBlackBGAnimationID, 0.6);
	ScreenSize sz = getScreenSize();
	setAnimationSize(gData.mBlackBGAnimationID, makePosition(sz.x, sz.y, 1), makePosition(0, 0, 0));

	gData.mOffset.z = DISPLAY_TEXT_Z; // TODO: layerNo
	gData.mFont.x = 1; // TODO: use system fonts
	gData.mTextID = addMugenTextMugenStyle(gData.mMessage, gData.mOffset, gData.mFont);
	gData.mIsShowingText = 0;

	movePlayersOutOfScreen();
	DreamPlayer* player = getRootPlayer(0);
	setPlayerPositionX(player, 160, getPlayerCoordinateP(player));
	setPlayerPositionY(player, 0, getPlayerCoordinateP(player));
	setPlayerIsFacingRight(player, 1);
	int nextState = gData.mIsShowingWinPose ? 180 : 5500;
	if (hasPlayerStateSelf(player, nextState)) {
		changePlayerState(player, nextState);
	}

	resetDreamMugenStageHandlerCameraPosition();

	gData.mNow = 0;
	gData.mStage = FIGHT_RESULT_STAGE_ACTIVE;

	enableDrawing();
	addFadeIn(gData.mFadeInTime, NULL, NULL);
}

void showFightResults(void(*tNextScreenCB)(void*), void(*tTitleScreenCB)(void*)) {
	gData.mNextScreenCB = tNextScreenCB;
	gData.mTitleScreenCB = tTitleScreenCB;

	addFadeOut(gData.mFadeOutTime, fadeToResultFinishedCB, NULL);

}
