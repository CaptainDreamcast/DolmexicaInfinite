#include "osuhandler.h"

#include <string.h>

#include <prism/memoryhandler.h>
#include <prism/mugenanimationhandler.h>
#include <prism/sound.h>
#include <prism/input.h>
#include <prism/system.h>
#include <prism/log.h>
#include <prism/math.h>
#include <prism/mugensoundfilereader.h>

#include "ai.h"
#include "osufilereader.h"
#include "mugencommandhandler.h"
#include "fightui.h"
#include "config.h"

typedef struct {
	int mHasResponded;
	MugenAnimationHandlerElement* mResponseAnimationElement;

	int mHasAIDecided;
	int mAIDecidedLevel;

} ActiveHitObjectPlayerResponse;

typedef struct {
	OsuHitObject* mObject;
	int mIsObjectOwned;

	OsuColor* mColor;

	MugenAnimationHandlerElement* mBodyAnimationElement;
	MugenAnimationHandlerElement* mCircleAnimationElement;

	ActiveHitObjectPlayerResponse mPlayerResponse[2];

} ActiveHitObject;

typedef struct {
	OsuSliderObject* mObject;

	int mRepeatNow;

} ActiveSliderObject;

typedef struct {
	OsuSpinnerObject* mObject;
	int mIsObjectOwned;
	
	MugenAnimationHandlerElement* mEncouragementAnimationElement;

} ActiveSpinnerObject;

typedef struct {
	ListIterator mPreviousTimingPointWithPositiveBeat;
	ListIterator mCurrentTimingPoint;

	double mCurrentMillisecondsPerBeat;

} TimingPointData;

typedef struct {
	OsuMilliSecond mNow;
	OsuMilliSecond mTime;

} OsuPlayerAI;

struct OsuParameters {
	std::string mSpriteFilePath;
	std::string mAnimationFilePath;
	std::string mSoundFilePath;

	int mHitCircleAnimation;
	int mApproachCircleAnimation;
	int mFreestyleAnimationPre;
	int mFreestyleAnimationDuring;
	int mFreestyleAnimationPost;
	Vector2D mFreestylePosition;
	int mHitAnimationBase;
	Vector2DI mHitSound;
};

static struct {
	char mPath[200];

	OsuParameters mParameters;

	MugenSpriteFile mSprites;
	MugenAnimations mAnimations;
	MugenSounds mSounds;

	OsuFile mOsu;
	List mActiveHitObjects; // contains ActiveHitObject 
	List mActiveSliderObjects; // contains ActiveSliderObject 
	List mActiveSpinnerObjects; // contains ActiveSpinnerObject 

	ListIterator mCurrentHitObjectListPosition;
	ListIterator mCurrentColor;

	TimingPointData mTimingPoint;

	double mGlobalZCounter;

	OsuPlayerAI mPlayerAI[2];

	int mIsActive;
} gOsuHandlerData;

static void loadTimingPointData() {
	gOsuHandlerData.mTimingPoint.mCurrentTimingPoint = list_iterator_begin(&gOsuHandlerData.mOsu.mOsuTimingPoints);
	gOsuHandlerData.mTimingPoint.mPreviousTimingPointWithPositiveBeat = list_iterator_begin(&gOsuHandlerData.mOsu.mOsuTimingPoints);
	OsuTimingPoint* e = (OsuTimingPoint*)list_iterator_get(gOsuHandlerData.mTimingPoint.mCurrentTimingPoint);
	gOsuHandlerData.mTimingPoint.mCurrentMillisecondsPerBeat = e->mMillisecondsPerBeat;
}

static void playOsuMusicFile()
{
	char path[1024];
	char folder[1024];

	getPathToFile(folder, gOsuHandlerData.mPath);
	sprintf(path, "%s%s", folder, gOsuHandlerData.mOsu.mGeneral.mAudioFileName);

	streamMusicFileOnce(path);
	gOsuHandlerData.mIsActive = 1;
}


static int getPreempt();

int isOsuHandlerActive()
{
	return gOsuHandlerData.mIsActive;
}

int shouldPlayOsuMusicInTheBeginning() {
	ListIterator it = list_iterator_begin(&gOsuHandlerData.mOsu.mOsuHitObjects);
	OsuHitObject* e = (OsuHitObject*)list_iterator_get(it);

	int delta = 10000 - (e->mTime - getPreempt());
	return (delta <= 0);
}

void startPlayingOsuSong() {
	if (isPlayingStreamingMusic()) stopStreamingMusicFile();
	playOsuMusicFile();
}

static void resetSinglePlayerAI(int i) {
	gOsuHandlerData.mPlayerAI[i].mNow = 0;
	gOsuHandlerData.mPlayerAI[i].mTime= 0;

}

static void resetOsuHandlerData() {
	gOsuHandlerData.mCurrentHitObjectListPosition = list_iterator_begin(&gOsuHandlerData.mOsu.mOsuHitObjects);
	gOsuHandlerData.mCurrentColor = list_iterator_begin(&gOsuHandlerData.mOsu.mOsuColors);
	if (!gOsuHandlerData.mCurrentColor) {
		logError("No Osu colors defined.");
		recoverFromError();
	}
	loadTimingPointData();

	gOsuHandlerData.mGlobalZCounter = 80;

	resetSinglePlayerAI(0);
	resetSinglePlayerAI(1);

	gOsuHandlerData.mIsActive = 0;

	if (shouldPlayOsuMusicInTheBeginning()) {
		startPlayingOsuSong();
	}
}

static void emptyOsuHandler();

void resetOsuHandler() {
	emptyOsuHandler();
	resetOsuHandlerData();
}

static void loadOsuHandler(void* /*tData*/) {
	setProfilingSectionMarkerCurrentFunction();
	gOsuHandlerData.mSprites = loadMugenSpriteFileWithoutPalette(gOsuHandlerData.mParameters.mSpriteFilePath);
	gOsuHandlerData.mAnimations = loadMugenAnimationFile(gOsuHandlerData.mParameters.mAnimationFilePath);
	gOsuHandlerData.mSounds = loadMugenSoundFile(gOsuHandlerData.mParameters.mSoundFilePath.c_str());

	gOsuHandlerData.mOsu = loadOsuFile(gOsuHandlerData.mPath);
	gOsuHandlerData.mActiveHitObjects = new_list();
	gOsuHandlerData.mActiveSliderObjects = new_list();
	gOsuHandlerData.mActiveSpinnerObjects = new_list();

	resetOsuHandlerData();
}

static int getPreempt() {
	if (gOsuHandlerData.mOsu.mDifficulty.mApproachRate < 5) return (int)(1200 + 600 * ((5.0 - gOsuHandlerData.mOsu.mDifficulty.mApproachRate) / 5));
	else if (gOsuHandlerData.mOsu.mDifficulty.mApproachRate == 5) return 1200;
	else return (int)(1200 - 750 * ((gOsuHandlerData.mOsu.mDifficulty.mApproachRate - 5.0) / 5));
}

static int getFadeInTime() {
	if (gOsuHandlerData.mOsu.mDifficulty.mApproachRate < 5) return (int)(800 + 400 * ((5.0 - gOsuHandlerData.mOsu.mDifficulty.mApproachRate) / 5));
	else if (gOsuHandlerData.mOsu.mDifficulty.mApproachRate == 5) return 800;
	else return (int)(800 - 500 * ((gOsuHandlerData.mOsu.mDifficulty.mApproachRate - 5.0) / 5));
}

static double getCircleSizeScale() {
	double size = 54.4 - 4.48 * gOsuHandlerData.mOsu.mDifficulty.mCircleSize;
	return size / 32;

}

static int getHitWindow50() {
	return (int)(150 + 50 * ((5.0 - gOsuHandlerData.mOsu.mDifficulty.mOverallDifficulty) / 5));
}

static int getHitWindow100() {
	return (int)(100 + 40 * ((5.0 - gOsuHandlerData.mOsu.mDifficulty.mOverallDifficulty) / 5));
}

static int getHitWindow300() {
	return (int)(50 + 30 * ((5.0 - gOsuHandlerData.mOsu.mDifficulty.mOverallDifficulty) / 5));
}

static int parsePositionX(int tX) {
	tX += 64;
	tX /= 2;
	return tX;
}

static int parsePositionY(int tY) {
	tY += 48;
	tY /= 2;
	return tY;
}


static void increaseOsuColor() {
	if (!list_has_next(gOsuHandlerData.mCurrentColor)) {
		gOsuHandlerData.mCurrentColor = list_iterator_begin(&gOsuHandlerData.mOsu.mOsuColors);
	}
	else {
		list_iterator_increase(&gOsuHandlerData.mCurrentColor);
	}
}

static void addActiveHitObject(OsuHitObject* tObject, int tIsOwned) {
	ActiveHitObject* e = (ActiveHitObject*)allocMemory(sizeof(ActiveHitObject));
	e->mObject = tObject;
	e->mIsObjectOwned = tIsOwned;

	if (e->mObject->mType & OSU_TYPE_MASK_NEW_COMBO) {
		increaseOsuColor();
	}
	e->mColor = (OsuColor*)list_iterator_get(gOsuHandlerData.mCurrentColor);

	e->mCircleAnimationElement = addMugenAnimation(getMugenAnimation(&gOsuHandlerData.mAnimations, gOsuHandlerData.mParameters.mApproachCircleAnimation), &gOsuHandlerData.mSprites, Vector3D(parsePositionX(tObject->mX), parsePositionY(tObject->mY), gOsuHandlerData.mGlobalZCounter));
	setMugenAnimationTransparency(e->mCircleAnimationElement, 0);
	setMugenAnimationBaseDrawScale(e->mCircleAnimationElement, getCircleSizeScale());
	setMugenAnimationColor(e->mCircleAnimationElement, e->mColor->mR, e->mColor->mG, e->mColor->mB);
	e->mBodyAnimationElement = addMugenAnimation(getMugenAnimation(&gOsuHandlerData.mAnimations, gOsuHandlerData.mParameters.mHitCircleAnimation), &gOsuHandlerData.mSprites, Vector3D(parsePositionX(tObject->mX), parsePositionY(tObject->mY), gOsuHandlerData.mGlobalZCounter + 0.001));
	setMugenAnimationTransparency(e->mBodyAnimationElement, 0);
	setMugenAnimationBaseDrawScale(e->mBodyAnimationElement, getCircleSizeScale());
	setMugenAnimationColor(e->mBodyAnimationElement, e->mColor->mR*0.8, e->mColor->mG*0.8, e->mColor->mB*0.8);
	gOsuHandlerData.mGlobalZCounter -= 0.003;

	int i;
	for (i = 0; i < 2; i++) {
		e->mPlayerResponse[i].mHasResponded = 0;
		e->mPlayerResponse[i].mHasAIDecided = 0;
	}

	list_push_back_owned(&gOsuHandlerData.mActiveHitObjects, e);
}

static void unloadActiveHitObject(ActiveHitObject* e) {
	removeMugenAnimation(e->mCircleAnimationElement);
	removeMugenAnimation(e->mBodyAnimationElement);

	int i;
	for (i = 0; i < 2; i++) {
		if (e->mPlayerResponse[i].mHasResponded) {
			removeMugenAnimation(e->mPlayerResponse[i].mResponseAnimationElement);
		}
	}

	if (e->mIsObjectOwned) {
		freeMemory(e->mObject);
	}
}

static void addActiveSliderObject(OsuSliderObject* tObject) {
	ActiveSliderObject* e = (ActiveSliderObject*)allocMemory(sizeof(ActiveSliderObject));
	e->mObject = tObject;
	e->mRepeatNow = 0;

	list_push_back_owned(&gOsuHandlerData.mActiveSliderObjects, e);
}

static void unloadActiveSliderObject(ActiveSliderObject* /*e*/) { }

static void addActiveSpinnerObject(OsuSpinnerObject* tObject, int tIsObjectOwned) {
	ActiveSpinnerObject* e = (ActiveSpinnerObject*)allocMemory(sizeof(ActiveSpinnerObject));
	e->mObject = tObject;
	e->mIsObjectOwned = tIsObjectOwned;
	e->mEncouragementAnimationElement = addMugenAnimation(getMugenAnimation(&gOsuHandlerData.mAnimations, gOsuHandlerData.mParameters.mFreestyleAnimationPre), &gOsuHandlerData.mSprites, gOsuHandlerData.mParameters.mFreestylePosition.xyz(90));

	list_push_back_owned(&gOsuHandlerData.mActiveSpinnerObjects, e);
}

static void unloadActiveSpinnerObject(ActiveSpinnerObject* e) {
	removeMugenAnimation(e->mEncouragementAnimationElement);

	if (e->mIsObjectOwned) {
		freeMemory(e->mObject);
	}
}

static int removeSingleActiveHitObject(void* tCaller, void* tData) {
	(void)tCaller;
	ActiveHitObject* e = (ActiveHitObject*)tData;
	unloadActiveHitObject(e);
	return 1;
}

static int removeSingleActiveSliderObject(void* tCaller, void* tData) {
	(void)tCaller;
	ActiveSliderObject* e = (ActiveSliderObject*)tData;
	unloadActiveSliderObject(e);
	return 1;
}

static int removeSingleActiveSpinnerObject(void* tCaller, void* tData) {
	(void)tCaller;
	ActiveSpinnerObject* e = (ActiveSpinnerObject*)tData;
	unloadActiveSpinnerObject(e);
	return 1;
}

static void emptyOsuHandler() {
	list_remove_predicate(&gOsuHandlerData.mActiveHitObjects, removeSingleActiveHitObject, NULL);
	list_remove_predicate(&gOsuHandlerData.mActiveSliderObjects, removeSingleActiveSliderObject, NULL);
	list_remove_predicate(&gOsuHandlerData.mActiveSpinnerObjects, removeSingleActiveSpinnerObject, NULL);
}

static void increaseCurrentHitObject() {
	if (list_has_next(gOsuHandlerData.mCurrentHitObjectListPosition)) {
		list_iterator_increase(&gOsuHandlerData.mCurrentHitObjectListPosition);
	}
	else {
		gOsuHandlerData.mCurrentHitObjectListPosition = NULL;
	}
}

static void updateTimingPoint() {
	if (!list_has_next(gOsuHandlerData.mTimingPoint.mCurrentTimingPoint)) return;

	ListIterator testIterator = gOsuHandlerData.mTimingPoint.mCurrentTimingPoint;
	list_iterator_increase(&testIterator);
	OsuTimingPoint* next = (OsuTimingPoint*)list_iterator_get(testIterator);
	
	int time = (int)getStreamingSoundTimeElapsedInMilliseconds();
	if (next->mOffset > time) return;

	gOsuHandlerData.mTimingPoint.mCurrentTimingPoint = testIterator;
	
	if (next->mMillisecondsPerBeat > 0) {
		gOsuHandlerData.mTimingPoint.mPreviousTimingPointWithPositiveBeat = gOsuHandlerData.mTimingPoint.mCurrentTimingPoint;
		gOsuHandlerData.mTimingPoint.mCurrentMillisecondsPerBeat = next->mMillisecondsPerBeat;
	}
	else {
		OsuTimingPoint* previousWithPositive = (OsuTimingPoint*)list_iterator_get(gOsuHandlerData.mTimingPoint.mPreviousTimingPointWithPositiveBeat);
		double percentage = -next->mMillisecondsPerBeat;

		gOsuHandlerData.mTimingPoint.mCurrentMillisecondsPerBeat = previousWithPositive->mMillisecondsPerBeat * (percentage / 100);
	}


}

static void updateAddingBeatmapSounds() {
	while (gOsuHandlerData.mCurrentHitObjectListPosition) {
		OsuHitObject* testObject = (OsuHitObject*)list_iterator_get(gOsuHandlerData.mCurrentHitObjectListPosition);
		int startTime = testObject->mTime - getPreempt();
		if (startTime > (int)getStreamingSoundTimeElapsedInMilliseconds()) break;

		if (testObject->mType & OSU_TYPE_MASK_HIT_OBJECT) {
			addActiveHitObject(testObject, 0);
		}
		else if (testObject->mType & OSU_TYPE_MASK_SLIDER) {
			addActiveSliderObject((OsuSliderObject*)testObject);
		}
		else if (testObject->mType & OSU_TYPE_MASK_SPINNER) {
			addActiveSpinnerObject((OsuSpinnerObject*)testObject, 0);
		}

		increaseCurrentHitObject();
	}
}

static void updateAddingFinalSpinner() {
	if (list_size(&gOsuHandlerData.mActiveHitObjects)) return;
	if (list_size(&gOsuHandlerData.mActiveSliderObjects)) return;
	if (list_size(&gOsuHandlerData.mActiveSpinnerObjects)) return;


	int time = (int)getStreamingSoundTimeElapsedInMilliseconds();
	OsuSpinnerObject* e = (OsuSpinnerObject*)allocMemory(sizeof(OsuSpinnerObject));
	e->mX = 0;
	e->mY = 0;
	e->mTime = time + getPreempt();
	e->mType = OSU_TYPE_MASK_SPINNER;
	e->mHitSound = 0;
	e->mEndTime = time + 10000000;

	addActiveSpinnerObject(e, 1);
}


static void updateNewObjects() {
	if (gOsuHandlerData.mCurrentHitObjectListPosition) {
		updateAddingBeatmapSounds();
	}
	else {
		updateAddingFinalSpinner();
	}

}

typedef struct {
	int mHasPlayerHadRelevantObject[2];

} UpdateActiveHitObjectCaller;



static void updateActiveHitObjectFadeIn(ActiveHitObject* e) {
	int time = (int)getStreamingSoundTimeElapsedInMilliseconds();
	int preempt = getPreempt();
	int fadeIn = getFadeInTime();
	int start = e->mObject->mTime - preempt;
	int end = start + fadeIn;

	double transparency = (time - start) / (double)(end - start);
	transparency = std::min(1.0, transparency);

	setMugenAnimationTransparency(e->mCircleAnimationElement, transparency);
	setMugenAnimationTransparency(e->mBodyAnimationElement, transparency);
}

#define HIT_OBJECT_FADE_OUT 200

static void updateActiveHitObjectFadeOut(ActiveHitObject* e) {
	int time = (int)getStreamingSoundTimeElapsedInMilliseconds();
	int start = e->mObject->mTime + getHitWindow50();
	int end = start + HIT_OBJECT_FADE_OUT;

	double transparency = (time - start) / (double)(end - start);
	transparency = std::min(1.0, transparency);
	transparency = 1 - transparency;

	setMugenAnimationTransparency(e->mCircleAnimationElement, 0);
	setMugenAnimationTransparency(e->mBodyAnimationElement, transparency);
}

static void updateActiveHitObjectTransparency(ActiveHitObject* e) { 
	int time = (int)getStreamingSoundTimeElapsedInMilliseconds();
	if (time < e->mObject->mTime) {
		updateActiveHitObjectFadeIn(e);
	}
	else {
		updateActiveHitObjectFadeOut(e);
	}

}

static void updateActiveHitObjectBrightness(ActiveHitObject* e) {
	int time = (int)getStreamingSoundTimeElapsedInMilliseconds();
	int preempt = getPreempt();
	int start = e->mObject->mTime - preempt;
	int end = e->mObject->mTime;

	double t = (time - start) / (double)(end - start);
	double scale = 4 - (4 - 1)*t;
	scale *= getCircleSizeScale();
	setMugenAnimationBaseDrawScale(e->mCircleAnimationElement, scale);
}

static void updateActiveHitObjectRingScale(ActiveHitObject* e) {
	int time = (int)getStreamingSoundTimeElapsedInMilliseconds();
	int preempt = getPreempt();
	int start = e->mObject->mTime - preempt;
	int end = e->mObject->mTime;

	double t = (time - start) / (double)(end - start);
	double scale = 4-(4-1)*t;
	scale *= getCircleSizeScale();
	setMugenAnimationBaseDrawScale(e->mCircleAnimationElement, scale);
}

static int hasPressedOsuButtonFlank(int i){
	return hasPressedAFlankSingle(i) || hasPressedBFlankSingle(i) || hasPressedXFlankSingle(i) || hasPressedYFlankSingle(i) || hasPressedStartFlankSingle(i);

}

static void addResponse(int i, ActiveHitObject* e, int tLevel) {
	
	Position pos = getMugenAnimationPosition(e->mBodyAnimationElement);

	pos.x += 20 * (i ? 1 : -1);
	pos.z += 0.001;

	e->mPlayerResponse[i].mResponseAnimationElement = addMugenAnimation(getMugenAnimation(&gOsuHandlerData.mAnimations, gOsuHandlerData.mParameters.mHitAnimationBase + tLevel), &gOsuHandlerData.mSprites, pos);

	if (tLevel) {
		tryPlayMugenSoundAdvanced(&gOsuHandlerData.mSounds, gOsuHandlerData.mParameters.mHitSound.x, gOsuHandlerData.mParameters.mHitSound.y, parseGameMidiVolumeToPrism(getGameMidiVolume()));
	}

	if (tLevel > 0) allowOsuPlayerCommandInputOneFrame(i);
	if (tLevel > 0 && getPlayerAILevel(getRootPlayer(i))) activateRandomAICommand(i);

	e->mPlayerResponse[i].mHasResponded = 1;
}


static void updateSinglePlayerActiveHitObjectHit(int i, ActiveHitObject* e, UpdateActiveHitObjectCaller* tCaller, int tTime) {
	if (e->mPlayerResponse[i].mHasResponded) return;
	tCaller->mHasPlayerHadRelevantObject[i] = 1;
	if (!hasPressedOsuButtonFlank(i)) return;

	int level = 0;

	int hitWindow50 = getHitWindow50();
	int startWindow50 = e->mObject->mTime - hitWindow50;
	if (tTime >= startWindow50) level = 1;

	int hitWindow100 = getHitWindow100();
	int startWindow100 = e->mObject->mTime - hitWindow100;
	int endWindow100 = e->mObject->mTime + hitWindow100;
	if (tTime >= startWindow100 && tTime <= endWindow100) level = 2;

	int hitWindow300 = getHitWindow300();
	int startWindow300 = e->mObject->mTime - hitWindow300;
	int endWindow300 = e->mObject->mTime + hitWindow300;
	if (tTime >= startWindow300 && tTime <= endWindow300) level = 3;

	addResponse(i, e, level);
}

static void setActiveHitObjectBright(ActiveHitObject* e) {
	setMugenAnimationColor(e->mBodyAnimationElement, e->mColor->mR, e->mColor->mG, e->mColor->mB);
}

#define HIT_OBJECT_HIT_BEGIN 500

static void updateActiveHitObjectHit(ActiveHitObject* e, UpdateActiveHitObjectCaller* tCaller) {
	int time = (int)getStreamingSoundTimeElapsedInMilliseconds();

	int overallStart = e->mObject->mTime - HIT_OBJECT_HIT_BEGIN;

	int hitWindow50 = getHitWindow50();
	int endWindow50 = e->mObject->mTime + hitWindow50;
	if (time < overallStart || time > endWindow50) return;
	setActiveHitObjectBright(e);

	int i;
	for (i = 0; i < 2; i++) {
		updateSinglePlayerActiveHitObjectHit(i, e, tCaller, time);
	}

}

static void updateActiveHitObjectMiss(ActiveHitObject* e, UpdateActiveHitObjectCaller* /*tCaller*/) {
	int time = (int)getStreamingSoundTimeElapsedInMilliseconds();
	int hitWindow50 = getHitWindow50();
	int endWindow50 = e->mObject->mTime + hitWindow50;

	if (time > endWindow50) {
		int i;
		for (i = 0; i < 2; i++) {
			if (e->mPlayerResponse[i].mHasResponded) continue;
			addResponse(i, e, 0);
		}
	}

	

}

static int isHitObjectOver(ActiveHitObject* e) {
	int time = (int)getStreamingSoundTimeElapsedInMilliseconds();
	return time >= e->mObject->mTime + getHitWindow50() + HIT_OBJECT_FADE_OUT;
}


static int updateSingleActiveHitObject(void* tCaller, void* tData) {
	UpdateActiveHitObjectCaller* caller = (UpdateActiveHitObjectCaller*)tCaller;
	ActiveHitObject* e = (ActiveHitObject*)tData;

	updateActiveHitObjectTransparency(e);
	updateActiveHitObjectBrightness(e);
	updateActiveHitObjectRingScale(e);
	updateActiveHitObjectHit(e, caller);
	updateActiveHitObjectMiss(e, caller);

	if (isHitObjectOver(e)) {
		unloadActiveHitObject(e);
		return 1;
	}

	return 0;
}

static void updateActiveHitObjects() {
	UpdateActiveHitObjectCaller caller;
	
	int i;
	for(i = 0; i < 2; i++) caller.mHasPlayerHadRelevantObject[i] = 0;

	list_remove_predicate(&gOsuHandlerData.mActiveHitObjects, updateSingleActiveHitObject, &caller);
}

static double getSliderDuration(OsuSliderObject* tObject) {
	double duration = tObject->mPixelLength / (100.0 * gOsuHandlerData.mOsu.mDifficulty.mSliderMultiplier) * gOsuHandlerData.mTimingPoint.mCurrentMillisecondsPerBeat;
	return duration;
}

static int updateSingleActiveSliderObject(void* tCaller, void* tData) {
	(void)tCaller;
	ActiveSliderObject* e = (ActiveSliderObject*)tData;
	
	double sliderDuration = getSliderDuration(e->mObject);
	int time = (int)(e->mObject->mTime + e->mRepeatNow * sliderDuration);
	int startTime = time - getPreempt();
	if (startTime > (int)getStreamingSoundTimeElapsedInMilliseconds()) return 0;

	OsuHitObject* object = (OsuHitObject*)allocMemory(sizeof(OsuHitObject));
	*object = *((OsuHitObject*)e->mObject);
	object->mTime = time;
	if (e->mRepeatNow % 2) {
		object->mX = e->mObject->mX;
		object->mY = e->mObject->mY;
	}
	else {
		object->mX = e->mObject->mEndPosition.x;
		object->mY = e->mObject->mEndPosition.y;
	}

	addActiveHitObject(object, 1);

	e->mRepeatNow++;
	if (e->mRepeatNow == e->mObject->mRepeat) {
		unloadActiveSliderObject(e);
		return 1;
	}

	return 0;
}

static void updateActiveSliders() {
	list_remove_predicate(&gOsuHandlerData.mActiveSliderObjects, updateSingleActiveSliderObject, NULL);
}

#define ACTIVE_SPINNER_POST_TIME 500

static void updateSpinnerAICommand(int i) {
	if (!getPlayerAILevel(getRootPlayer(i))) return;

	gOsuHandlerData.mPlayerAI[i].mNow++;
	if (gOsuHandlerData.mPlayerAI[i].mNow >= gOsuHandlerData.mPlayerAI[i].mTime) {
		gOsuHandlerData.mPlayerAI[i].mNow = 0;
		gOsuHandlerData.mPlayerAI[i].mTime = randfromInteger(30, 45);

		activateRandomAICommand(i);
	}
}

static void updateActiveSpinnerControl(ActiveSpinnerObject* e) {
	int time = (int)getStreamingSoundTimeElapsedInMilliseconds();
	if (time < e->mObject->mTime || time > e->mObject->mEndTime) return;

	int i;
	for (i = 0; i < 2; i++) {
		allowOsuPlayerCommandInputOneFrame(i);
		updateSpinnerAICommand(i);
	}
}


static void updateActiveSpinnerTextDisplay(ActiveSpinnerObject* e, int tTime) {
	if (tTime >= e->mObject->mTime && getMugenAnimationAnimationNumber(e->mEncouragementAnimationElement) < gOsuHandlerData.mParameters.mFreestyleAnimationDuring) {
		changeMugenAnimation(e->mEncouragementAnimationElement, getMugenAnimation(&gOsuHandlerData.mAnimations, gOsuHandlerData.mParameters.mFreestyleAnimationDuring));
	}
	if (tTime >= e->mObject->mEndTime && getMugenAnimationAnimationNumber(e->mEncouragementAnimationElement) < gOsuHandlerData.mParameters.mFreestyleAnimationPost) {
		changeMugenAnimation(e->mEncouragementAnimationElement, getMugenAnimation(&gOsuHandlerData.mAnimations, gOsuHandlerData.mParameters.mFreestyleAnimationPost));
	}
}

static void updateActiveSpinnerTextFadeIn(ActiveSpinnerObject* e, int tTime) {
	int preempt = getPreempt();
	int fadeIn = getFadeInTime();
	int start = e->mObject->mTime - preempt;
	int end = start + fadeIn;

	double transparency = (tTime - start) / (double)(end - start);
	transparency = std::min(1.0, transparency);

	setMugenAnimationTransparency(e->mEncouragementAnimationElement, transparency);
}

static void updateActiveSpinnerTextFadeOut(ActiveSpinnerObject* e, int tTime) {
	int end = e->mObject->mEndTime + ACTIVE_SPINNER_POST_TIME;
	int start = end - HIT_OBJECT_FADE_OUT;

	double transparency = 1 - (tTime - start) / (double)(end - start);
	transparency = std::max(0.0, transparency);

	setMugenAnimationTransparency(e->mEncouragementAnimationElement, transparency);
}

static double pulseTween(double t) {
	double duration = 0.1;
	double change = 0.2;

	if (t < 1 - duration) return 1;
	if (t < 1 - (duration / 2)) {
		t -= 1 - duration;
		t /= duration / 2;
		return 1 - t*change;
	}
	else {
		t -= 1 - (duration / 2);
		t /= duration / 2;
		return (1 - change) + t*change;
	}


}

static void updateActiveSpinnerPulse(ActiveSpinnerObject* e, int tTime) {
	int timeDelta = abs(e->mObject->mTime - tTime);
	double milliSecondsPerBeat = gOsuHandlerData.mTimingPoint.mCurrentMillisecondsPerBeat;
	int cycleLength = (int)(milliSecondsPerBeat * 1);
	double cyclePosition = (timeDelta % cycleLength) / (double)cycleLength;
	double t = pulseTween(cyclePosition);

	setMugenAnimationBaseDrawScale(e->mEncouragementAnimationElement, t);
}

static void updateActiveSpinnerText(ActiveSpinnerObject* e) {
	int time = (int)getStreamingSoundTimeElapsedInMilliseconds();
	updateActiveSpinnerTextDisplay(e, time);
	updateActiveSpinnerTextFadeIn(e, time);
	updateActiveSpinnerTextFadeOut(e, time);
	updateActiveSpinnerPulse(e, time);
}

static int isActiveSpinnerOver(ActiveSpinnerObject* e) {
	int time = (int)getStreamingSoundTimeElapsedInMilliseconds();
	return time > e->mObject->mEndTime + ACTIVE_SPINNER_POST_TIME;
}

static int updateSingleActiveSpinnerObject(void* tCaller, void* tData) {
	(void)tCaller;
	ActiveSpinnerObject* e = (ActiveSpinnerObject*)tData;

	updateActiveSpinnerControl(e);
	updateActiveSpinnerText(e);

	if (isActiveSpinnerOver(e)) {
		unloadActiveSpinnerObject(e);
		return 1;
	}

	return 0;
}

static void updateActiveSpinners() {
	list_remove_predicate(&gOsuHandlerData.mActiveSpinnerObjects, updateSingleActiveSpinnerObject, NULL);
}

typedef struct {
	int mTime;
	int i;
} FindHitObjectCaller;

static int isRelevantAIHitObjectCB(void* tCaller, void* tData) {
	FindHitObjectCaller* caller = (FindHitObjectCaller*)tCaller;
	ActiveHitObject* e = (ActiveHitObject*)tData;

	if (e->mPlayerResponse[caller->i].mHasResponded) return 0;

	int hitWindow50 = getHitWindow50();
	int startWindow50 = e->mObject->mTime - hitWindow50;
	int endWindow50 = e->mObject->mTime + hitWindow50;
	return (caller->mTime >= startWindow50 && caller->mTime <= endWindow50);
}

static ActiveHitObject* findAIRelevantActiveHitObject(int i, int tTime) {
	FindHitObjectCaller caller;
	caller.mTime = tTime;
	caller.i = i;

	ListIterator iterator = list_find_first_predicate(&gOsuHandlerData.mActiveHitObjects, isRelevantAIHitObjectCB, &caller);
	if (!iterator) return NULL;

	return (ActiveHitObject*)list_iterator_get(iterator);
}

static void decideAIResponse(int i, ActiveHitObject* e) {

	e->mPlayerResponse[i].mAIDecidedLevel = randfromInteger(0, 3);
	e->mPlayerResponse[i].mHasAIDecided = 1;
}

static void updateSingleArtificialIntelligence(int i) {
	if (!getPlayerAILevel(getRootPlayer(i))) return;

	int time = (int)getStreamingSoundTimeElapsedInMilliseconds();
	ActiveHitObject* e = findAIRelevantActiveHitObject(i, time);
	if (!e) return;

	if (!e->mPlayerResponse[i].mHasAIDecided) {
		decideAIResponse(i, e);
	}

	if (e->mPlayerResponse[i].mAIDecidedLevel == 1) {
		addResponse(i, e, e->mPlayerResponse[i].mAIDecidedLevel);
	}
	else if (e->mPlayerResponse[i].mAIDecidedLevel == 2) {
		int hitWindow100 = getHitWindow100();
		int startWindow100 = e->mObject->mTime - hitWindow100;
		int endWindow100 = e->mObject->mTime + hitWindow100;
		if (time >= startWindow100 && time <= endWindow100) {
			addResponse(i, e, e->mPlayerResponse[i].mAIDecidedLevel);
		}
	} else if (e->mPlayerResponse[i].mAIDecidedLevel == 3) {
		int hitWindow300 = getHitWindow300();
		int startWindow300 = e->mObject->mTime - hitWindow300;
		int endWindow300 = e->mObject->mTime + hitWindow300;
		if (time >= startWindow300 && time <= endWindow300) {
			addResponse(i, e, e->mPlayerResponse[i].mAIDecidedLevel);
		}
	}
}

static void updateArtificialIntelligence() {
	int i;
	for (i = 0; i < 2; i++) {
		updateSingleArtificialIntelligence(i);
	}
}

static void updateFightEnd() {
	if (isPlayingStreamingMusic()) return;

	setTimerFinished();
	emptyOsuHandler();
	gOsuHandlerData.mIsActive = 0;
}

static void resetOsuHandlerInputAllowed() {
	for (int i = 0; i < 2; i++) {
		resetOsuPlayerCommandInputAllowed(i);
	}
}

static void updateOsuHandler(void* tData) {
	(void)tData;
	setProfilingSectionMarkerCurrentFunction();

	resetOsuHandlerInputAllowed();
	if (gOsuHandlerData.mIsActive) {
		updateTimingPoint();
		updateNewObjects();
		updateFightEnd();
	}
	updateActiveSliders();
	updateActiveHitObjects();
	updateActiveSpinners();
	updateArtificialIntelligence();
}

ActorBlueprint getOsuHandler() {
	return makeActorBlueprint(loadOsuHandler, NULL, updateOsuHandler);
}

void setOsuFile(const char * tPath)
{
	strcpy(gOsuHandlerData.mPath, tPath);
}

void stopOsuHandler()
{
	emptyOsuHandler();
	gOsuHandlerData.mIsActive = 0;
}

void loadOsuParametersFromScript(MugenDefScript* tScript, const char* tFightPath) {
	auto& parameters = gOsuHandlerData.mParameters;
	std::string folder;
	getPathToFile(folder, tFightPath);

	parameters.mSpriteFilePath = getSTLMugenDefStringOrDefault(tScript, "files", "osu.sff", "osu.sff");
	parameters.mSpriteFilePath = findMugenSystemOrFightFilePath(parameters.mSpriteFilePath, folder);
	parameters.mAnimationFilePath = getSTLMugenDefStringOrDefault(tScript, "files", "osu.air", "osu.air");
	parameters.mAnimationFilePath = findMugenSystemOrFightFilePath(parameters.mAnimationFilePath, folder);
	parameters.mSoundFilePath = getSTLMugenDefStringOrDefault(tScript, "files", "osu.snd", "osu.snd");
	parameters.mSoundFilePath = findMugenSystemOrFightFilePath(parameters.mSoundFilePath, folder);

	parameters.mHitCircleAnimation = getMugenDefIntegerOrDefault(tScript, "osu", "hitcircle.anim", 1000);
	parameters.mApproachCircleAnimation = getMugenDefIntegerOrDefault(tScript, "osu", "approachcircle.anim", 1001);
	parameters.mFreestyleAnimationPre = getMugenDefIntegerOrDefault(tScript, "osu", "freestyle.pre.anim", 2000);
	parameters.mFreestyleAnimationDuring = getMugenDefIntegerOrDefault(tScript, "osu", "freestyle.during.anim", 2001);
	parameters.mFreestyleAnimationPost = getMugenDefIntegerOrDefault(tScript, "osu", "freestyle.post.anim", 2002);
	parameters.mFreestylePosition = getMugenDefVector2DOrDefault(tScript, "osu", "freestyle.pos", Vector2D(160, 50));
	parameters.mHitAnimationBase = getMugenDefIntegerOrDefault(tScript, "osu", "hit.animbase", 1500);
	parameters.mHitSound = getMugenDefVector2DIOrDefault(tScript, "osu", "hit.snd", Vector2DI(1, 0));
}