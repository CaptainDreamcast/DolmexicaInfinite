#pragma once

#include <stdint.h>

#include <prism/datastructures.h>

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
	OsuMilliSecond mMillisecondsPerBeat;
	int mMeter;
	int mSampleIndex;
	int mVolume;
	int mInherited;
	int mKiaiMode;

} OsuTimingPoint;

typedef struct {
	int mX;
	int mY;
	OsuMilliSecond mTime;
	uint8_t mType;
	uint8_t mHitSound;

} OsuHitObject;

typedef struct {
	OsuFileGeneral mGeneral;
	OsuFileDifficulty mDifficulty;
	OsuEvents mEvents;
	List mOsuTimingPoints; // contains OsuTimingPoint
	List mOsuHitObjects; // contains OsuHitObject
} OsuFile;

OsuFile loadOsuFile(char* tPath);