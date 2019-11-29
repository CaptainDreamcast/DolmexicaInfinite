#include "pausecontrollers.h"

#include <assert.h>

#include <prism/mugenanimationreader.h>
#include <prism/mugenanimationhandler.h>
#include <prism/screeneffect.h>
#include <prism/system.h>

#include "playerdefinition.h"
#include "fightui.h"
#include "mugenstagehandler.h"
#include "stage.h"

#define SUPERPAUSE_DARKENING_Z 30
#define SUPERPAUSE_Z 52

typedef struct {
	int mIsActive;
	DreamPlayer* mPlayer;

	int mNow;
	int mDuration;

	int mBufferTimeForCommandsDuringPauseEnd;
	int mMoveTime;
	int mIsPausingBG;

	int mHasAnimation;
	MugenAnimationHandlerElement* mMugenAnimationElement;

	Position mAnimationReferencePosition;

	int mIsDarkening;
	AnimationHandlerElement* mDarkeningAnimationElement;

	double mPlayer2DefenseMultiplier;

	int mIsSettingPlayerUnhittable;
} SuperPauseControllerData;

typedef struct {
	int mIsActive;
	DreamPlayer* mPlayer;

	int mNow;
	int mDuration;

	int mBufferTimeForCommandsDuringPauseEnd;
	int mMoveTime;
	int mIsPausingBG;
} PauseControllerData;

static struct {
	SuperPauseControllerData mSuperPause;
	PauseControllerData mPause;
} gPauseControllerData;

static void loadPauseControllerHandler(void*) {
	gPauseControllerData.mSuperPause.mIsActive = 0;
	gPauseControllerData.mPause.mIsActive = 0;
}

static void setSuperPauseInactive();
static void setPauseInactive();

static void updateSuperPause() {
	if (!gPauseControllerData.mSuperPause.mIsActive) return;

	if (gPauseControllerData.mSuperPause.mNow >= gPauseControllerData.mSuperPause.mDuration)
	{
		setSuperPauseInactive();
	}
	gPauseControllerData.mSuperPause.mNow++;
}

static void updatePause() {
	if (!gPauseControllerData.mPause.mIsActive) return;
	if (gPauseControllerData.mSuperPause.mIsActive) return;

	if (gPauseControllerData.mPause.mNow >= gPauseControllerData.mPause.mDuration)
	{
		setPauseInactive();
	}
	gPauseControllerData.mPause.mNow++;
}

static void updatePauseControllerHandler(void*) {
	updateSuperPause();
	updatePause();
}

ActorBlueprint getPauseControllerHandler() {
	return makeActorBlueprint(loadPauseControllerHandler, NULL, updatePauseControllerHandler);
}

void setDreamSuperPauseActive(DreamPlayer * tPlayer)
{
	if (gPauseControllerData.mSuperPause.mIsActive)
	{
		setSuperPauseInactive();
	}
	gPauseControllerData.mSuperPause.mIsActive = 1;
	gPauseControllerData.mSuperPause.mPlayer = tPlayer;
	setPlayersSpeed(0);
}

int isDreamSuperPauseActive()
{
	return gPauseControllerData.mSuperPause.mIsActive;
}

static void setSuperPauseInactive() {
	if (gPauseControllerData.mSuperPause.mHasAnimation) {
		removeMugenAnimation(gPauseControllerData.mSuperPause.mMugenAnimationElement);
	}
	if (gPauseControllerData.mSuperPause.mIsDarkening) {
		removeHandledAnimation(gPauseControllerData.mSuperPause.mDarkeningAnimationElement);
	}
	gPauseControllerData.mSuperPause.mIsActive = 0;

	if (!isDreamAnyPauseActive()) {
		setPlayersSpeed(1);
		setDreamMugenStageHandlerSpeed(1);
	}
}

void setDreamSuperPauseTime(DreamPlayer* /*tPlayer*/, int tTime)
{
	gPauseControllerData.mSuperPause.mNow = 0;
	gPauseControllerData.mSuperPause.mDuration = tTime;
}

void setDreamSuperPauseBufferTimeForCommandsDuringPauseEnd(DreamPlayer* /*tPlayer*/, int tBufferTime)
{
	gPauseControllerData.mSuperPause.mBufferTimeForCommandsDuringPauseEnd = tBufferTime;
}

void setDreamSuperPauseMoveTime(DreamPlayer* /*tPlayer*/, int tMoveTime)
{
	gPauseControllerData.mSuperPause.mMoveTime = tMoveTime;
}

void setDreamSuperPauseIsPausingBG(DreamPlayer* /*tPlayer*/, int tIsPausingBG)
{
	if (tIsPausingBG) {
		setDreamMugenStageHandlerSpeed(0);
	}
	gPauseControllerData.mSuperPause.mIsPausingBG = tIsPausingBG;
}

void setDreamSuperPauseAnimation(DreamPlayer* tPlayer, int tIsInPlayerFile, int tAnimationNumber)
{
	if (tAnimationNumber == -1) {
		gPauseControllerData.mSuperPause.mHasAnimation = 0;
		return;
	}

	MugenAnimation* animation;
	MugenSpriteFile* sprites;
	if (tIsInPlayerFile) {
		if (!doesPlayerHaveAnimationHimself(tPlayer, tAnimationNumber)) {
			gPauseControllerData.mSuperPause.mHasAnimation = 0;
			return;
		}
		animation = getPlayerAnimation(tPlayer, tAnimationNumber);
		sprites = getPlayerSprites(tPlayer);
	}
	else {
		animation = getDreamFightEffectAnimation(tAnimationNumber);
		sprites = getDreamFightEffectSprites();
	}

	gPauseControllerData.mSuperPause.mHasAnimation = 1;
	gPauseControllerData.mSuperPause.mMugenAnimationElement = addMugenAnimation(animation, sprites, makePosition(0, 0, 0));
	setMugenAnimationBasePosition(gPauseControllerData.mSuperPause.mMugenAnimationElement, &gPauseControllerData.mSuperPause.mAnimationReferencePosition);
	setMugenAnimationCameraPositionReference(gPauseControllerData.mSuperPause.mMugenAnimationElement, getDreamMugenStageHandlerCameraPositionReference());
}

void setDreamSuperPauseSound(DreamPlayer* tPlayer, int tIsInPlayerFile, int tSoundGroup, int tSoundItem)
{
	MugenSounds* soundFile;
	if (tIsInPlayerFile) {
		soundFile = getPlayerSounds(tPlayer);
	}
	else {
		soundFile = getDreamCommonSounds();
	}

	tryPlayMugenSound(soundFile, tSoundGroup, tSoundItem);
}

void setDreamSuperPausePosition(DreamPlayer* tPlayer, double tX, double tY)
{
	int isPlayerFacingRight = getPlayerIsFacingRight(tPlayer);
	if (!isPlayerFacingRight) tX *= -1;

	Position mPlayerPosition = getDreamStageCoordinateSystemOffset(getPlayerCoordinateP(tPlayer)) + getPlayerPosition(tPlayer, getPlayerCoordinateP(tPlayer));
	gPauseControllerData.mSuperPause.mAnimationReferencePosition = vecAdd(makePosition(tX, tY, 0), mPlayerPosition);
	gPauseControllerData.mSuperPause.mAnimationReferencePosition.z = SUPERPAUSE_Z;
}

void setDreamSuperPauseDarkening(DreamPlayer* /*tPlayer*/, int tIsDarkening)
{
	gPauseControllerData.mSuperPause.mIsDarkening = tIsDarkening;
	if (tIsDarkening) {
		gPauseControllerData.mSuperPause.mDarkeningAnimationElement = playOneFrameAnimationLoop(makePosition(0, 0, SUPERPAUSE_DARKENING_Z), getEmptyWhiteTextureReference());
		const auto sz = getScreenSize();
		setAnimationSize(gPauseControllerData.mSuperPause.mDarkeningAnimationElement, makePosition(sz.x, sz.y, 1), makePosition(0, 0, 0));
		setAnimationTransparency(gPauseControllerData.mSuperPause.mDarkeningAnimationElement, 0.7);
		setAnimationColor(gPauseControllerData.mSuperPause.mDarkeningAnimationElement, 0.0, 0.0, 0.1);
	}
}

void setDreamSuperPausePlayer2DefenseMultiplier(DreamPlayer* /*tPlayer*/, double tMultiplier)
{
	gPauseControllerData.mSuperPause.mPlayer2DefenseMultiplier = tMultiplier;
}

void setDreamSuperPausePowerToAdd(DreamPlayer* tPlayer, int tPowerToAdd)
{
	addPlayerPower(tPlayer, tPowerToAdd);
}

void setDreamSuperPausePlayerUnhittability(DreamPlayer* /*tPlayer*/, int tIsUnhittable)
{
	gPauseControllerData.mSuperPause.mIsSettingPlayerUnhittable = tIsUnhittable;
}

void setDreamPauseTime(DreamPlayer* /*tPlayer*/, int tTime)
{
	gPauseControllerData.mPause.mNow = 0;
	gPauseControllerData.mPause.mDuration = tTime;
}

void setDreamPauseBufferTimeForCommandsDuringPauseEnd(DreamPlayer * /*tPlayer*/, int tBufferTime)
{
	gPauseControllerData.mPause.mBufferTimeForCommandsDuringPauseEnd = tBufferTime;

}

void setDreamPauseMoveTime(DreamPlayer* /*tPlayer*/, int tMoveTime)
{
	gPauseControllerData.mPause.mMoveTime = tMoveTime;
}

void setDreamPauseIsPausingBG(DreamPlayer* /*tPlayer*/, int tIsPausingBG)
{
	if (tIsPausingBG) {
		setDreamMugenStageHandlerSpeed(0);
	}
	gPauseControllerData.mPause.mIsPausingBG = tIsPausingBG;
}

void setDreamPauseActive(DreamPlayer * tPlayer)
{
	if (gPauseControllerData.mPause.mIsActive)
	{
		setPauseInactive();
	}
	gPauseControllerData.mPause.mIsActive = 1;
	gPauseControllerData.mPause.mPlayer = tPlayer;
	setPlayersSpeed(0);
}

static void setPauseInactive() {
	gPauseControllerData.mPause.mIsActive = 0;

	if (!isDreamAnyPauseActive()) {
		setPlayersSpeed(1);
		setDreamMugenStageHandlerSpeed(1);
	}
}

int isDreamPauseActive()
{
	return gPauseControllerData.mPause.mIsActive;
}

int isDreamAnyPauseActive()
{
	return isDreamSuperPauseActive() || isDreamPauseActive();
}