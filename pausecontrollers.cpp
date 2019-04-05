#include "pausecontrollers.h"

#include <assert.h>

#include <prism/mugenanimationreader.h>
#include <prism/mugenanimationhandler.h>

#include "playerdefinition.h"
#include "fightui.h"
#include "mugenstagehandler.h"
#include "stage.h"

#define SUPERPAUSE_Z 52



typedef struct {
	

	IntMap mPauseData;

} SuperPauseHandler;

static struct {
	SuperPauseHandler mSuperPause;

} gData;

void setDreamSuperPauseActive(DreamPlayer * tPlayer)
{
	// TODO: check multiple
	PlayerPauseData* e = &tPlayer->mPause;

	setPlayerSuperPaused(tPlayer);
	// TODO: stuff

	e->mIsActive = 1;
}

int initPlayerPauseData(DreamPlayer * tPlayer)
{
	PlayerPauseData* e = &tPlayer->mPause;
	return e->mIsActive = 0;
}

static void setSuperPauseInactive(DreamPlayer* tPlayer) {
	PlayerPauseData* e = &tPlayer->mPause;

	if (e->mHasAnimation) {
		removeMugenAnimation(e->mMugenAnimationID);
	}

	setPlayerUnSuperPaused(tPlayer);

	e->mIsActive = 0;
}

void updatePlayerPause(DreamPlayer* tPlayer) {
	PlayerPauseData* e = &tPlayer->mPause;
	if (!e->mIsActive) return;

	e->mNow++;
	if (e->mNow >= e->mDuration)  // TODO: fix
	{
		setSuperPauseInactive(tPlayer);
	}
}

void setDreamSuperPauseTime(DreamPlayer* tPlayer, int tTime)
{
	PlayerPauseData* e = &tPlayer->mPause;

	e->mNow = 0;
	e->mDuration = tTime;
}

void setDreamSuperPauseBufferTimeForCommandsDuringPauseEnd(DreamPlayer* tPlayer, int tBufferTime)
{
	PlayerPauseData* e = &tPlayer->mPause;
	e->mBufferTimeForCommandsDuringPauseEnd = tBufferTime;
}

void setDreamSuperPauseMoveTime(DreamPlayer* tPlayer, int tMoveTime)
{
	PlayerPauseData* e = &tPlayer->mPause;
	e->mMoveTime = tMoveTime; // TODO: do something with this
}

void setDreamSuperPauseIsPausingBG(DreamPlayer* tPlayer, int tIsPausingBG)
{
	PlayerPauseData* e = &tPlayer->mPause;
	e->mIsPausingBG = tIsPausingBG;
}

void setDreamSuperPauseAnimation(DreamPlayer* tPlayer, int tIsInPlayerFile, int tAnimationNumber)
{
	PlayerPauseData* e = &tPlayer->mPause;

	if (tAnimationNumber == -1) {
		e->mHasAnimation = 0;
		return;
	}

	MugenAnimation* animation;
	MugenSpriteFile* sprites;
	if (tIsInPlayerFile) {
		if (!doesPlayerHaveAnimationHimself(tPlayer, tAnimationNumber)) {
			e->mHasAnimation = 0;
			return;
		}
		animation = getPlayerAnimation(tPlayer, tAnimationNumber);
		sprites = getPlayerSprites(tPlayer);
	}
	else {
		animation = getDreamFightEffectAnimation(tAnimationNumber);
		sprites = getDreamFightEffectSprites();
	}

	e->mHasAnimation = 1;
	e->mMugenAnimationID = addMugenAnimation(animation, sprites, makePosition(0, 0, 0));
	setMugenAnimationBasePosition(e->mMugenAnimationID, &e->mAnimationReferencePosition);
	setMugenAnimationCameraPositionReference(e->mMugenAnimationID, getDreamMugenStageHandlerCameraPositionReference());
}

void setDreamSuperPauseSound(DreamPlayer* tPlayer, int tIsInPlayerFile, int tSoundGroup, int tSoundItem)
{
	(void)tIsInPlayerFile;
	(void)tSoundGroup;
	(void)tSoundItem;
	// TODO
}

void setDreamSuperPausePosition(DreamPlayer* tPlayer, double tX, double tY)
{
	PlayerPauseData* e = &tPlayer->mPause;

	int isPlayerFacingRight = getPlayerIsFacingRight(tPlayer);
	if (!isPlayerFacingRight) tX *= -1;

	Position mPlayerPosition = getDreamStageCoordinateSystemOffset(getPlayerCoordinateP(tPlayer)) + getPlayerPosition(tPlayer, getPlayerCoordinateP(tPlayer));
	e->mAnimationReferencePosition = vecAdd(makePosition(tX, tY, 0), mPlayerPosition);
	e->mAnimationReferencePosition.z = SUPERPAUSE_Z; // TODO: better
}

void setDreamSuperPauseDarkening(DreamPlayer* tPlayer, int tIsDarkening)
{
	PlayerPauseData* e = &tPlayer->mPause;
	e->mIsDarkening = tIsDarkening;
}

void setDreamSuperPausePlayer2DefenseMultiplier(DreamPlayer* tPlayer, double tMultiplier)
{
	PlayerPauseData* e = &tPlayer->mPause;
	e->mPlayer2DefenseMultiplier = tMultiplier;
}

void setDreamSuperPausePowerToAdd(DreamPlayer* tPlayer, int tPowerToAdd)
{
	addPlayerPower(tPlayer, tPowerToAdd);
}

void setDreamSuperPausePlayerUnhittability(DreamPlayer* tPlayer, int tIsUnhittable)
{
	PlayerPauseData* e = &tPlayer->mPause;
	e->mIsSettingPlayerUnhittable = tIsUnhittable;
}
