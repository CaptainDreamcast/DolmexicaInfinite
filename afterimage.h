#pragma once

#include <prism/mugenanimationhandler.h>
#include <prism/stlutil.h>

using namespace prism;

struct DreamPlayer;

struct AfterImageHistoryBufferEntry {
	MugenAnimation* mAnimation;
	MugenAnimationHandlerElement* mAnimationElement;
	int mWasVisible;
};

struct DreamPlayerAfterImage{
	int mIsActive;
	std::list<AfterImageHistoryBufferEntry> mHistoryBuffer;

	int mHistoryBufferLength;
	int mNow;
	int mDuration;
	int mTimeGap;
	int mFrameGap;
	Vector3D mStartColor;
	Vector3D mColorAdd;
	Vector3D mColorMultiply;
	int mIsColorInverted;
	BlendType mBlendType;
};

void initPlayerAfterImage(DreamPlayer* p);
void removePlayerAfterImage(DreamPlayer* p);
void addAfterImage(DreamPlayer* tPlayer, int tHistoryBufferLength, int tDuration, int tTimeGap, int tFrameGap, const Vector3D& tStartColor, const Vector3D& tColorAdd, const Vector3D& tColorMultiply, int tIsColorInverted, BlendType tBlendType);
void setAfterImageDuration(DreamPlayer* tPlayer, int tDuration);
void updateAfterImage(DreamPlayer* tPlayer);
