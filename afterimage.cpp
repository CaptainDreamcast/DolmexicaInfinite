#include "afterimage.h"

#include <algorithm>

#include <prism/stlutil.h>
#include <prism/physicshandler.h>

#include "playerdefinition.h"
#include "gamelogic.h"
#include "mugenstagehandler.h"
#include "stage.h"

void initPlayerAfterImage(DreamPlayer* p)
{
	p->mAfterImage.mIsActive = 0;
	p->mAfterImage.mHistoryBuffer.clear();
}

static void addHistoryBufferElement(DreamPlayer* tPlayer) {
	DreamPlayerAfterImage& afterImage = tPlayer->mAfterImage;
	AfterImageHistoryBufferEntry e;
	const auto sprite = getMugenAnimationSprite(tPlayer->mAnimationElement);
	e.mAnimation = createOneFrameMugenAnimationForSprite(sprite.x, sprite.y);
	auto p = getDreamStageCoordinateSystemOffset(getDreamMugenStageHandlerCameraCoordinateP()) + getHandledPhysicsPosition(tPlayer->mPhysicsElement);
	p.z = PLAYER_Z;
	e.mAnimationElement = addMugenAnimation(e.mAnimation, &tPlayer->mHeader->mFiles.mSprites, p);
	setMugenAnimationDrawScale(e.mAnimationElement, makePosition(getPlayerScaleX(tPlayer), getPlayerScaleY(tPlayer), 1) * tPlayer->mTempScale * getPlayerToCameraScale(tPlayer));
	setMugenAnimationCameraPositionReference(e.mAnimationElement, getDreamMugenStageHandlerCameraPositionReference());
	setMugenAnimationCameraEffectPositionReference(e.mAnimationElement, getDreamMugenStageHandlerCameraEffectPositionReference());
	setMugenAnimationCameraScaleReference(e.mAnimationElement, getDreamMugenStageHandlerCameraZoomReference());

	setMugenAnimationFaceDirection(e.mAnimationElement, getPlayerIsFacingRight(tPlayer));
	if (tPlayer->mIsAngleActive) {
		setMugenAnimationDrawAngle(e.mAnimationElement, degreesToRadians(tPlayer->mAngle));
	}
	if (afterImage.mBlendType == BLEND_TYPE_NORMAL) {
		setMugenAnimationBlendType(e.mAnimationElement, getMugenAnimationBlendType(tPlayer->mAnimationElement));
	}
	else {
		setMugenAnimationBlendType(e.mAnimationElement, afterImage.mBlendType);
	}
	setMugenAnimationTransparency(e.mAnimationElement, getMugenAnimationTransparency(tPlayer->mAnimationElement));
	e.mWasVisible = getMugenAnimationVisibility(tPlayer->mAnimationElement);
	setMugenAnimationVisibility(e.mAnimationElement, 0);
	setMugenAnimationColor(e.mAnimationElement, afterImage.mStartColor.x, afterImage.mStartColor.y, afterImage.mStartColor.z);
	setMugenAnimationColorInverted(e.mAnimationElement, afterImage.mIsColorInverted);
	afterImage.mHistoryBuffer.push_front(e);
}

static void unloadHistoryBufferElement(const AfterImageHistoryBufferEntry& tEntry) {
	removeMugenAnimation(tEntry.mAnimationElement);
	destroyMugenAnimation(tEntry.mAnimation);
}

void removePlayerAfterImage(DreamPlayer* p)
{
	DreamPlayerAfterImage& afterImage = p->mAfterImage;
	while (!p->mAfterImage.mHistoryBuffer.empty()) {
		unloadHistoryBufferElement(afterImage.mHistoryBuffer.back());
		afterImage.mHistoryBuffer.pop_back();
	}
}

void addAfterImage(DreamPlayer* tPlayer, int tHistoryBufferLength, int tDuration, int tTimeGap, int tFrameGap, const Vector3D& tStartColor, const Vector3D& tColorAdd, const Vector3D& tColorMultiply, int tIsColorInverted, BlendType tBlendType)
{
	DreamPlayerAfterImage& afterImage = tPlayer->mAfterImage;
	afterImage.mHistoryBufferLength = tHistoryBufferLength;
	afterImage.mNow = 0;
	afterImage.mDuration = tDuration;
	afterImage.mTimeGap = std::max(1, tTimeGap);
	afterImage.mFrameGap = std::max(1, tFrameGap);
	afterImage.mStartColor = tStartColor;
	afterImage.mColorAdd = tColorAdd;
	afterImage.mColorMultiply = tColorMultiply;
	afterImage.mIsColorInverted = tIsColorInverted;
	afterImage.mBlendType = tBlendType;
	afterImage.mIsActive = 1;
}

void setAfterImageDuration(DreamPlayer * tPlayer, int tDuration)
{
	DreamPlayerAfterImage& afterImage = tPlayer->mAfterImage;
	afterImage.mNow = 0;
	afterImage.mDuration = tDuration;
}

static void updateAddingSingleAfterImage(DreamPlayer* tPlayer) {
	DreamPlayerAfterImage& afterImage = tPlayer->mAfterImage;
	if (!afterImage.mIsActive) return;

	static auto tick = getDreamGameTime();
	if (tick % afterImage.mTimeGap) return;

	addHistoryBufferElement(tPlayer);
}

static void updateRemovingAfterImage(DreamPlayer* tPlayer) {
	DreamPlayerAfterImage& afterImage = tPlayer->mAfterImage;
	if (afterImage.mHistoryBuffer.empty()) return;

	static auto tick = getDreamGameTime();
	if (tick % afterImage.mTimeGap) return;

	if (!afterImage.mIsActive || int(afterImage.mHistoryBuffer.size()) > afterImage.mHistoryBufferLength) {
		unloadHistoryBufferElement(afterImage.mHistoryBuffer.back());
		afterImage.mHistoryBuffer.pop_back();
	}
}

typedef struct {
	DreamPlayer* mPlayer;
	int mIndex;
	Vector3D mColor;
} HistoryBufferUpdateCaller;

static void updateSingleHistoryBuffer(HistoryBufferUpdateCaller* tCaller, AfterImageHistoryBufferEntry& e) {
	DreamPlayerAfterImage& afterImage = tCaller->mPlayer->mAfterImage;

	if (tCaller->mIndex++ % afterImage.mFrameGap) {
		setMugenAnimationVisibility(e.mAnimationElement, 0);
		return;
	}
	auto p = getMugenAnimationPositionReference(e.mAnimationElement);
	p->z = getMugenAnimationPosition(tCaller->mPlayer->mAnimationElement).z - ((tCaller->mIndex / afterImage.mFrameGap) + 1) * 0.01;
	setMugenAnimationVisibility(e.mAnimationElement, e.mWasVisible);
	setMugenAnimationColor(e.mAnimationElement, tCaller->mColor.x, tCaller->mColor.y, tCaller->mColor.z);
	tCaller->mColor = (tCaller->mColor + afterImage.mColorAdd) * afterImage.mColorMultiply;
}

static void updateActiveHistoryBuffer(DreamPlayer* tPlayer) {
	DreamPlayerAfterImage& afterImage = tPlayer->mAfterImage;
	if (afterImage.mHistoryBuffer.empty()) return;

	HistoryBufferUpdateCaller caller;
	caller.mPlayer = tPlayer;
	caller.mIndex = 0;
	caller.mColor = afterImage.mStartColor;
	stl_list_map(afterImage.mHistoryBuffer, updateSingleHistoryBuffer, &caller);
}

static void updateAfterImageOver(DreamPlayer* tPlayer) {
	if (isPlayerPaused(tPlayer)) return;
	DreamPlayerAfterImage& afterImage = tPlayer->mAfterImage;
	if (!afterImage.mIsActive) return;
	afterImage.mNow++;
	if (afterImage.mDuration != -1 && afterImage.mNow >= afterImage.mDuration) {
		afterImage.mIsActive = 0;
	}
}

void updateAfterImage(DreamPlayer* tPlayer) {
	updateAddingSingleAfterImage(tPlayer);
	updateRemovingAfterImage(tPlayer);
	updateActiveHistoryBuffer(tPlayer);
	updateAfterImageOver(tPlayer);
}
