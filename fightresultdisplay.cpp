#include "fightresultdisplay.h"

#include <prism/log.h>

#include <prism/mugentexthandler.h>
#include <prism/screeneffect.h>
#include <prism/animation.h>
#include <prism/system.h>
#include <prism/profiling.h>

#include "playerdefinition.h"
#include "gamelogic.h"
#include "mugenstagehandler.h"


#define BLACK_BG_Z 33
#define DISPLAY_TEXT_LOWEST_Z 31
#define DISPLAY_TEXT_MEDIUM_Z 51
#define DISPLAY_TEXT_HIGHEST_Z 79


typedef enum {
	FIGHT_RESULT_STAGE_INACTIVE,
	FIGHT_RESULT_STAGE_ACTIVE
} FightResultStage;

static struct {
	int mIsEnabled;
	char mMessage[100];
	Vector3DI mFont; 
	Position2D mOffset;
	int mTextDisplayTime;
	int mLayerNo; 
	int mFadeInTime; 
	int mShowTime; 
	int mFadeOutTime; 
	int mIsShowingWinPose;

	FightResultStage mStage;
	int mIsDisplayUsed;

	TextureData mWhiteTexture;
	AnimationHandlerElement* mBlackBGAnimationElement;
	int mNow;

	int mIsShowingText;
	int mTextID;

	void(*mNextScreenCB)(void*);
	void(*mTitleScreenCB)(void*);
} gFightResultDisplayData;

static void loadFightResultDisplay(void* tData) {
	(void)tData;
	setProfilingSectionMarkerCurrentFunction();
	gFightResultDisplayData.mWhiteTexture = getEmptyWhiteTexture();
	gFightResultDisplayData.mStage = FIGHT_RESULT_STAGE_INACTIVE;
}

static void updateTextDisplay() {
	if (!gFightResultDisplayData.mIsShowingText) return;

	if (gFightResultDisplayData.mTextDisplayTime != -1 && gFightResultDisplayData.mNow >= gFightResultDisplayData.mTextDisplayTime) {
		removeMugenText(gFightResultDisplayData.mTextID);
		gFightResultDisplayData.mIsShowingText = 0;
	}
}

static void updateDisplayOver() {
	if (gFightResultDisplayData.mNow >= gFightResultDisplayData.mShowTime) {
		if(getGameMode() == GAME_MODE_SURVIVAL) addFadeOut(gFightResultDisplayData.mFadeOutTime, gFightResultDisplayData.mTitleScreenCB, NULL);
		else addFadeOut(gFightResultDisplayData.mFadeOutTime, gFightResultDisplayData.mNextScreenCB, NULL);
		gFightResultDisplayData.mStage = FIGHT_RESULT_STAGE_INACTIVE;
	}
}

static void updateDisplayTime() {
	gFightResultDisplayData.mNow++;
}

static void updateFightResultDisplay(void* tData) {
	(void)tData;
	setProfilingSectionMarkerCurrentFunction();
	if (!gFightResultDisplayData.mStage) return;

	updateTextDisplay();
	updateDisplayOver();
	updateDisplayTime();
}

ActorBlueprint getFightResultDisplay() {
	return makeActorBlueprint(loadFightResultDisplay, NULL, updateFightResultDisplay);
};

void setFightResultActive(int tIsActive) {
	gFightResultDisplayData.mIsDisplayUsed = tIsActive;
}

void setFightResultData(int tIsEnabled, char* tMessage, const Vector3DI& tFont, const Position2D& tOffset, int tTextDisplayTime, int tLayerNo, int tFadeInTime, int tShowTime, int tFadeOutTime, int tIsShowingWinPose) {
	gFightResultDisplayData.mIsEnabled = tIsEnabled;
	sprintf(gFightResultDisplayData.mMessage, "%.99s", tMessage);
	gFightResultDisplayData.mFont = tFont;
	gFightResultDisplayData.mOffset = tOffset;
	gFightResultDisplayData.mTextDisplayTime = tTextDisplayTime;
	gFightResultDisplayData.mLayerNo = tLayerNo;
	gFightResultDisplayData.mFadeInTime = tFadeInTime;
	gFightResultDisplayData.mShowTime = tShowTime;
	gFightResultDisplayData.mFadeOutTime = tFadeOutTime;
	gFightResultDisplayData.mIsShowingWinPose = tIsShowingWinPose;

	gFightResultDisplayData.mIsDisplayUsed = 1;
}

void setFightResultMessage(char * tMessage)
{
	sprintf(gFightResultDisplayData.mMessage, "%.99s", tMessage);
}

void setFightResultIsShowingWinPose(int tIsShowingWinPose)
{
	gFightResultDisplayData.mIsShowingWinPose = tIsShowingWinPose;
}

int isShowingFightResults() {
	return gFightResultDisplayData.mIsDisplayUsed && gFightResultDisplayData.mIsEnabled;
}

static void movePlayersOutOfScreen() {
	int amount = getTotalPlayerAmount();
	int i;
	for (i = 0; i < amount; i++) {
		DreamPlayer* player = getPlayerByIndex(i);
		if (!player) continue;
		setPlayerPositionX(player, 161, getPlayerCoordinateP(player));
		setPlayerPositionY(player, -10000, getPlayerCoordinateP(player));
	}

}

static void fadeToResultFinishedCB(void* /*tData*/) {
	gFightResultDisplayData.mBlackBGAnimationElement = playOneFrameAnimationLoop(Vector3D(0, 0, BLACK_BG_Z), &gFightResultDisplayData.mWhiteTexture);
	setAnimationColor(gFightResultDisplayData.mBlackBGAnimationElement, 0, 0, 0);
	setAnimationTransparency(gFightResultDisplayData.mBlackBGAnimationElement, 0.6);
	ScreenSize sz = getScreenSize();
	setAnimationSize(gFightResultDisplayData.mBlackBGAnimationElement, Vector3D(sz.x, sz.y, 1), Vector3D(0, 0, 0));

	double z;
	if (gFightResultDisplayData.mLayerNo == 0) {
		z = DISPLAY_TEXT_LOWEST_Z;
	} 
	else if (gFightResultDisplayData.mLayerNo == 1) {
		z = DISPLAY_TEXT_MEDIUM_Z;
	}
	else {
		z = DISPLAY_TEXT_HIGHEST_Z;
	}
	gFightResultDisplayData.mTextID = addMugenTextMugenStyle(gFightResultDisplayData.mMessage, gFightResultDisplayData.mOffset.xyz(z), gFightResultDisplayData.mFont);
	gFightResultDisplayData.mIsShowingText = 0;

	movePlayersOutOfScreen();
	DreamPlayer* player = getRootPlayer(0);
	setPlayerPositionX(player, 160, getPlayerCoordinateP(player));
	setPlayerPositionY(player, 0, getPlayerCoordinateP(player));
	setPlayerIsFacingRight(player, 1);
	int nextState = gFightResultDisplayData.mIsShowingWinPose ? 180 : 5500;
	if (hasPlayerStateSelf(player, nextState)) {
		changePlayerState(player, nextState);
	}

	resetDreamMugenStageHandlerCameraPosition();

	gFightResultDisplayData.mNow = 0;
	gFightResultDisplayData.mStage = FIGHT_RESULT_STAGE_ACTIVE;

	enableDrawing();
	addFadeIn(gFightResultDisplayData.mFadeInTime, NULL, NULL);
}

void showFightResults(void(*tNextScreenCB)(void*), void(*tTitleScreenCB)(void*)) {
	gFightResultDisplayData.mNextScreenCB = tNextScreenCB;
	gFightResultDisplayData.mTitleScreenCB = tTitleScreenCB;

	addFadeOut(gFightResultDisplayData.mFadeOutTime, fadeToResultFinishedCB, NULL);

}
