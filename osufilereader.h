#pragma once

#include <stdint.h>

#include <prism/datastructures.h>
#include <prism/geometry.h>

typedef int OsuMilliSecond;

typedef struct {
	char* mAudioFileName;
	OsuMilliSecond mAudioLeadIn;
	OsuMilliSecond mPreviewTime;
	int mCountdown;
	double mStackLeniency;
	

} OsuFileGeneral;

typedef struct {
	int mHPDrainRate;
	double mCircleSize;
	int mOverallDifficulty;
	double mApproachRate;
	double mSliderMultiplier;
	int mSliderTickRate;

} OsuFileDifficulty;

typedef struct {
	OsuMilliSecond mStart;
	OsuMilliSecond mEnd;

} OsuEventBreak;

typedef struct {
	List mBreaks; // contains OsuEventBreak


} OsuEvents;

typedef struct {
	OsuMilliSecond mOffset;
	double mMillisecondsPerBeat;
	int mMeter;
	int mSampleIndex;
	int mVolume;
	int mInherited;
	int mKiaiMode;

} OsuTimingPoint;

typedef struct {
	double mR;
	double mG;
	double mB;
} OsuColor;


#define OSU_TYPE_MASK_NEW_COMBO		(1 << 2)
#define OSU_TYPE_MASK_HIT_OBJECT	(1 << 3)
#define OSU_TYPE_MASK_SLIDER		(1 << 4)
#define OSU_TYPE_MASK_SPINNER		(1 << 5)

typedef struct {
	int mX;
	int mY;
	OsuMilliSecond mTime;
	uint8_t mType;
	uint8_t mHitSound;
} OsuHitObject;

typedef struct {
	int mX;
	int mY;
	OsuMilliSecond mTime;
	uint8_t mType;
	uint8_t mHitSound;
	Vector3DI mEndPosition;
	int mRepeat;
	double mPixelLength;
} OsuSliderObject;

typedef struct {
	int mX;
	int mY;
	OsuMilliSecond mTime;
	uint8_t mType;
	uint8_t mHitSound;

	OsuMilliSecond mEndTime;
} OsuSpinnerObject;

typedef struct {
	OsuFileGeneral mGeneral;
	OsuFileDifficulty mDifficulty;
	OsuEvents mEvents;
	List mOsuTimingPoints; // contains OsuTimingPoint
	List mOsuColors; // contains OsuColor
	List mOsuHitObjects; // contains OsuHitObject/OsuSpinnerObject/OsuSliderObject
} OsuFile;

OsuFile loadOsuFile(char* tPath);