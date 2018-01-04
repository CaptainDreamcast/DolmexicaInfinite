#include "pausecontrollers.h"

#include <assert.h>

#include <tari/mugenanimationreader.h>

#include "playerdefinition.h"
#include "fightui.h"
#include "mugenanimationhandler.h"
#include "mugenstagehandler.h"

typedef struct {
	int mIsActive;
	DreamPlayer* mPlayer;

	int mNow;
	int mDuration;

	int mBufferTimeForCommandsDuringPauseEnd;
	int mMoveTime;
	int mIsPausingBG;

	int mHasAnimation;
	int mMugenAnimationID;
	// TODO: sound

	Position mAnimationReferencePosition;

	int mIsDarkening;
	double mPlayer2DefenseMultiplier;

	int mIsSettingPlayerUnhittable;

} SuperPauseHandler;

static struct {
	SuperPauseHandler mSuperPause;

} gData;

static void loadSuperPauseHandler() {
	gData.mSuperPause.mIsActive = 0;
}

static void loadPauseHandler(void* tData) {
	(void)tData;
	loadSuperPauseHandler();
}

void setDreamSuperPauseActive()
{
	// TODO: check multiple

	setPlayerSuperPaused(gData.mSuperPause.mPlayer);
	// TODO: stuff

	gData.mSuperPause.mIsActive = 1;
}


static void setSuperPauseInactive() {
	if (gData.mSuperPause.mHasAnimation) {
		removeDreamRegisteredAnimation(gData.mSuperPause.mMugenAnimationID);
	}

	setPlayerUnSuperPaused(gData.mSuperPause.mPlayer);

	gData.mSuperPause.mIsActive = 0;
}

static void updateSuperPause() {
	if (!gData.mSuperPause.mIsActive) return;

	gData.mSuperPause.mNow++;
	if (gData.mSuperPause.mNow >= gData.mSuperPause.mDuration)  // TODO: fix
	{
		setSuperPauseInactive();
	}
}

static void updatePauseHandler(void* tData) {
	(void)tData;
	updateSuperPause();
}

ActorBlueprint DreamPauseHandler = {
	.mLoad = loadPauseHandler,
	.mUpdate = updatePauseHandler,
};

void setDreamSuperPausePlayer(DreamPlayer* tPlayer)
{
	gData.mSuperPause.mPlayer = tPlayer;
}

void setDreamSuperPauseTime(int tTime)
{
	gData.mSuperPause.mNow = 0;
	gData.mSuperPause.mDuration = tTime;
}

void setDreamSuperPauseBufferTimeForCommandsDuringPauseEnd(int tBufferTime)
{
	gData.mSuperPause.mBufferTimeForCommandsDuringPauseEnd = tBufferTime;
}

void setDreamSuperPauseMoveTime(int tMoveTime)
{
	gData.mSuperPause.mMoveTime = tMoveTime; // TODO: do something with this
}

void setDreamSuperPauseIsPausingBG(int tIsPausingBG)
{
	gData.mSuperPause.mIsPausingBG = tIsPausingBG;
}

void setDreamSuperPauseAnimation(int tIsInPlayerFile, int tAnimationNumber)
{
	if (tAnimationNumber == -1) {
		gData.mSuperPause.mHasAnimation = 0;
		return;
	}

	MugenAnimation* animation;
	MugenSpriteFile* sprites;
	if (tIsInPlayerFile) {
		if (!doesPlayerHaveAnimationHimself(gData.mSuperPause.mPlayer, tAnimationNumber)) {
			gData.mSuperPause.mHasAnimation = 0;
			return;
		}
		animation = getPlayerAnimation(gData.mSuperPause.mPlayer, tAnimationNumber);
		sprites = getPlayerSprites(gData.mSuperPause.mPlayer);
	}
	else {
		animation = getDreamFightEffectAnimation(tAnimationNumber);
		sprites = getDreamFightEffectSprites();
	}

	gData.mSuperPause.mHasAnimation = 1;
	gData.mSuperPause.mMugenAnimationID = addDreamRegisteredAnimation(NULL, animation, sprites, &gData.mSuperPause.mAnimationReferencePosition, getPlayerCoordinateP(gData.mSuperPause.mPlayer), getPlayerCoordinateP(gData.mSuperPause.mPlayer));
	setDreamRegisteredAnimationCameraPositionReference(gData.mSuperPause.mMugenAnimationID, getDreamMugenStageHandlerCameraPositionReference());
	setDreamRegisteredAnimationToUseFixedZ(gData.mSuperPause.mMugenAnimationID);
}

void setDreamSuperPauseSound(int tIsInPlayerFile, int tSoundGroup, int tSoundItem)
{
	(void)tIsInPlayerFile;
	(void)tSoundGroup;
	(void)tSoundItem;
	// TODO
}

void setDreamSuperPausePosition(Position tPosition)
{
	int isPlayerFacingRight = getPlayerIsFacingRight(gData.mSuperPause.mPlayer);
	if (!isPlayerFacingRight) tPosition.x *= -1;

	Position mPlayerPosition = getPlayerPosition(gData.mSuperPause.mPlayer, getPlayerCoordinateP(gData.mSuperPause.mPlayer));
	gData.mSuperPause.mAnimationReferencePosition = vecAdd(tPosition, mPlayerPosition);
	gData.mSuperPause.mAnimationReferencePosition.z = 20; // TODO: better
}

void setDreamSuperPauseDarkening(int tIsDarkening)
{
	gData.mSuperPause.mIsDarkening = tIsDarkening;
}

void setDreamSuperPausePlayer2DefenseMultiplier(double tMultiplier)
{
	gData.mSuperPause.mPlayer2DefenseMultiplier = tMultiplier;
}

void setDreamSuperPausePowerToAdd(int tPowerToAdd)
{
	addPlayerPower(gData.mSuperPause.mPlayer, tPowerToAdd);
}

void setDreamSuperPausePlayerUnhittability(int tIsUnhittable)
{
	gData.mSuperPause.mIsSettingPlayerUnhittable = tIsUnhittable;
}
