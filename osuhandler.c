#include "osuhandler.h"

#include <string.h>

#include <prism/memoryhandler.h>
#include <prism/mugenanimationhandler.h>
#include <prism/sound.h>
#include <prism/input.h>
#include <prism/system.h>
#include <prism/log.h>

#include "osufilereader.h"

typedef struct {
	int mHasResponded;
	int mResponseAnimationID;

} ActiveHitObjectPlayerResponse;

typedef struct {
	OsuHitObject* mObject;
	int mIsObjectOwned;

	int mBodyAnimationID;
	int mCircleAnimationID;

	ActiveHitObjectPlayerResponse mPlayerResponse[2];

} ActiveHitObject;

typedef struct {
	OsuSliderObject* mObject;
	int mRepeatNow;

} ActiveSliderObject;

typedef struct {
	OsuSpinnerObject* mObject;
	int mEncouragementAnimationID;

} ActiveSpinnerObject;

typedef struct {
	ListIterator mPreviousTimingPointWithPositiveBeat;
	ListIterator mCurrentTimingPoint;

	double mCurrentMillisecondsPerBeat;

} TimingPointData;

static struct {
	char mPath[200];

	MugenSpriteFile mSprites;
	MugenAnimations mAnimations;

	OsuFile mOsu;
	List mActiveHitObjects; // contains ActiveHitObject 
	List mActiveSliderObjects; // contains ActiveSliderObject 
	List mActiveSpinnerObjects; // contains ActiveSpinnerObject 

	ListIterator mCurrentHitObjectListPosition;
	ListIterator mCurrentColor;

	TimingPointData mTimingPoint;

	double mGlobalZCounter;

} gData;

static void loadTimingPointData() {
	gData.mTimingPoint.mCurrentTimingPoint = list_iterator_begin(&gData.mOsu.mOsuTimingPoints);
	gData.mTimingPoint.mPreviousTimingPointWithPositiveBeat = list_iterator_begin(&gData.mOsu.mOsuTimingPoints);
	OsuTimingPoint* e = list_iterator_get(gData.mTimingPoint.mCurrentTimingPoint);
	gData.mTimingPoint.mCurrentMillisecondsPerBeat = e->mMillisecondsPerBeat;
}

static void loadOsuHandler(void* tData) {
	gData.mSprites = loadMugenSpriteFileWithoutPalette("assets/osu/OSU.sff");
	gData.mAnimations = loadMugenAnimationFile("assets/osu/OSU.air");

	gData.mOsu = loadOsuFile(gData.mPath);
	gData.mActiveHitObjects = new_list();
	gData.mActiveSliderObjects = new_list();
	gData.mActiveSpinnerObjects = new_list();

	gData.mCurrentHitObjectListPosition = list_iterator_begin(&gData.mOsu.mOsuHitObjects);
	gData.mCurrentColor = list_iterator_begin(&gData.mOsu.mOsuColors);
	if (!gData.mCurrentColor) {
		logError("No Osu colors defined.");
		recoverFromError();
	}
	loadTimingPointData();

	gData.mGlobalZCounter = 70;
}

static int getPreempt() {
	if (gData.mOsu.mDifficulty.mApproachRate < 5) return (int)(1200 + 600 * ((5.0 - gData.mOsu.mDifficulty.mApproachRate) / 5));
	else if (gData.mOsu.mDifficulty.mApproachRate == 5) return 1200;
	else return (int)(1200 - 750 * ((gData.mOsu.mDifficulty.mApproachRate - 5.0) / 5));
}

static int getFadeInTime() {
	if (gData.mOsu.mDifficulty.mApproachRate < 5) return (int)(800 + 400 * ((5.0 - gData.mOsu.mDifficulty.mApproachRate) / 5));
	else if (gData.mOsu.mDifficulty.mApproachRate == 5) return 800;
	else return (int)(800 - 500 * ((gData.mOsu.mDifficulty.mApproachRate - 5.0) / 5));
}

static double getCircleSizeScale() {
	double size = 54.4 - 4.48 * gData.mOsu.mDifficulty.mCircleSize;
	return size / 32; // TODO

}

static int getHitWindow50() {
	return (int)(150 + 50 * ((5.0 - gData.mOsu.mDifficulty.mOverallDifficulty) / 5));
}

static int getHitWindow100() {
	return (int)(100 + 40 * ((5.0 - gData.mOsu.mDifficulty.mOverallDifficulty) / 5));
}

static int getHitWindow300() {
	return (int)(50 + 30 * ((5.0 - gData.mOsu.mDifficulty.mOverallDifficulty) / 5));
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
	if (!list_has_next(gData.mCurrentColor)) {
		gData.mCurrentColor = list_iterator_begin(&gData.mOsu.mOsuColors);
	}
	else {
		list_iterator_increase(&gData.mCurrentColor);
	}
}

static void addActiveHitObject(OsuHitObject* tObject, int tIsOwned) {
	ActiveHitObject* e = allocMemory(sizeof(ActiveHitObject));
	e->mObject = tObject;
	e->mIsObjectOwned = tIsOwned;

	if (e->mObject->mType & OSU_TYPE_MASK_NEW_COMBO) {
		increaseOsuColor();
	}
	OsuColor* color = list_iterator_get(gData.mCurrentColor);

	e->mCircleAnimationID = addMugenAnimation(getMugenAnimation(&gData.mAnimations, 1001), &gData.mSprites, makePosition(parsePositionX(tObject->mX), parsePositionY(tObject->mY), gData.mGlobalZCounter));
	setMugenAnimationTransparency(e->mCircleAnimationID, 0);
	setMugenAnimationBaseDrawScale(e->mCircleAnimationID, getCircleSizeScale());
	setMugenAnimationColor(e->mCircleAnimationID, color->mR, color->mG, color->mB);
	e->mBodyAnimationID = addMugenAnimation(getMugenAnimation(&gData.mAnimations, 1000), &gData.mSprites, makePosition(parsePositionX(tObject->mX), parsePositionY(tObject->mY), gData.mGlobalZCounter + 0.001));
	setMugenAnimationTransparency(e->mBodyAnimationID, 0);
	setMugenAnimationBaseDrawScale(e->mBodyAnimationID, getCircleSizeScale());
	setMugenAnimationColor(e->mBodyAnimationID, color->mR, color->mG, color->mB);
	gData.mGlobalZCounter += 0.003;

	int i;
	for (i = 0; i < 2; i++) {
		e->mPlayerResponse[i].mHasResponded = 0;
	}

	list_push_back_owned(&gData.mActiveHitObjects, e);
}

static void unloadActiveHitObject(ActiveHitObject* e) {
	removeMugenAnimation(e->mCircleAnimationID);
	removeMugenAnimation(e->mBodyAnimationID);

	int i;
	for (i = 0; i < 2; i++) {
		if (e->mPlayerResponse[i].mHasResponded) {
			removeMugenAnimation(e->mPlayerResponse[i].mResponseAnimationID);
		}
	}

	if (e->mIsObjectOwned) {
		freeMemory(e->mObject);
	}
}

static void addActiveSliderObject(OsuSliderObject* tObject) {
	ActiveSliderObject* e = allocMemory(sizeof(ActiveSliderObject));
	e->mObject = tObject;
	e->mRepeatNow = 0;

	list_push_back_owned(&gData.mActiveSliderObjects, e);
}

static void unloadActiveSliderObject(ActiveSliderObject* e) {}

static void addActiveSpinnerObject(OsuSpinnerObject* tObject) {
	ActiveSpinnerObject* e = allocMemory(sizeof(ActiveSpinnerObject));
	e->mObject = tObject;
	e->mEncouragementAnimationID = addMugenAnimation(getMugenAnimation(&gData.mAnimations, 2000), &gData.mSprites, makePosition(160, 40, 90));

	list_push_back_owned(&gData.mActiveSpinnerObjects, e);
}

static void unloadActiveSpinnerObject(ActiveSpinnerObject* e) {
	removeMugenAnimation(e->mEncouragementAnimationID);
}


static void increaseCurrentHitObject() {
	if (list_has_next(gData.mCurrentHitObjectListPosition)) {
		list_iterator_increase(&gData.mCurrentHitObjectListPosition);
	}
	else {
		gData.mCurrentHitObjectListPosition = NULL;
	}
}

static void updateTimingPoint() {
	if (!list_has_next(gData.mTimingPoint.mCurrentTimingPoint)) return;

	ListIterator testIterator = gData.mTimingPoint.mCurrentTimingPoint;
	list_iterator_increase(&testIterator);
	OsuTimingPoint* next = list_iterator_get(testIterator);
	
	int time = (int)getStreamingSoundTimeElapsedInMilliseconds();
	if (next->mOffset > time) return;

	gData.mTimingPoint.mCurrentTimingPoint = testIterator;
	
	if (next->mMillisecondsPerBeat > 0) {
		gData.mTimingPoint.mPreviousTimingPointWithPositiveBeat = gData.mTimingPoint.mCurrentTimingPoint;
		gData.mTimingPoint.mCurrentMillisecondsPerBeat = next->mMillisecondsPerBeat;
	}
	else {
		OsuTimingPoint* previousWithPositive = list_iterator_get(gData.mTimingPoint.mPreviousTimingPointWithPositiveBeat);
		double percentage = -next->mMillisecondsPerBeat;

		gData.mTimingPoint.mCurrentMillisecondsPerBeat = previousWithPositive->mMillisecondsPerBeat * (percentage / 100);
	}


}

static void updateNewObjects() {

	while (gData.mCurrentHitObjectListPosition) {
		OsuHitObject* testObject = list_iterator_get(gData.mCurrentHitObjectListPosition);
		int startTime = testObject->mTime - getPreempt();
		if (startTime > (int)getStreamingSoundTimeElapsedInMilliseconds()) break;
		
		if (testObject->mType & OSU_TYPE_MASK_HIT_OBJECT) {
			addActiveHitObject(testObject, 0);
		}
		else if(testObject->mType & OSU_TYPE_MASK_SLIDER){
			addActiveSliderObject((OsuSliderObject*)testObject);
		}
		else if (testObject->mType & OSU_TYPE_MASK_SPINNER) {
			addActiveSpinnerObject((OsuSpinnerObject*)testObject);
		}

		increaseCurrentHitObject();
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
	transparency = min(1, transparency);

	setMugenAnimationTransparency(e->mCircleAnimationID, transparency);
	setMugenAnimationTransparency(e->mBodyAnimationID, transparency);
}

#define HIT_OBJECT_FADE_OUT 200

static void updateActiveHitObjectFadeOut(ActiveHitObject* e) {
	int time = (int)getStreamingSoundTimeElapsedInMilliseconds();
	int start = e->mObject->mTime + getHitWindow50();
	int end = start + HIT_OBJECT_FADE_OUT;

	double transparency = (time - start) / (double)(end - start);
	transparency = min(1, transparency);
	transparency = 1 - transparency;

	setMugenAnimationTransparency(e->mCircleAnimationID, 0);
	setMugenAnimationTransparency(e->mBodyAnimationID, transparency);
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

static void updateActiveHitObjectRingScale(ActiveHitObject* e) {
	int time = (int)getStreamingSoundTimeElapsedInMilliseconds();
	int preempt = getPreempt();
	int start = e->mObject->mTime - preempt;
	int end = e->mObject->mTime;

	double t = (time - start) / (double)(end - start);
	double scale = 4-(4-1)*t;
	scale *= getCircleSizeScale();
	setMugenAnimationBaseDrawScale(e->mCircleAnimationID, scale);
}

static int hasPressedOsuButtonFlank(int i){
	return hasPressedAFlankSingle(i) || hasPressedBFlankSingle(i) || hasPressedXFlankSingle(i) || hasPressedYFlankSingle(i) || hasPressedStartFlankSingle(i);

}

static void addResponse(int i, ActiveHitObject* e, int tLevel) {
	Position pos = getMugenAnimationPosition(e->mBodyAnimationID);

	pos.x += 20 * (i ? 1 : -1);
	pos.z += 0.001;

	e->mPlayerResponse[i].mResponseAnimationID = addMugenAnimation(getMugenAnimation(&gData.mAnimations, 1500 + tLevel), &gData.mSprites, pos);
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

	// TODO: set control active

	addResponse(i, e, level);
}

static void updateActiveHitObjectHit(ActiveHitObject* e, UpdateActiveHitObjectCaller* tCaller) {
	int time = (int)getStreamingSoundTimeElapsedInMilliseconds();

	int overallStart = e->mObject->mTime - 500;

	int hitWindow50 = getHitWindow50();
	int endWindow50 = e->mObject->mTime + hitWindow50;
	if (time < overallStart || time > endWindow50) return;

	int i;
	for (i = 0; i < 2; i++) {
		updateSinglePlayerActiveHitObjectHit(i, e, tCaller, time);
	}

}

static void updateActiveHitObjectMiss(ActiveHitObject* e, UpdateActiveHitObjectCaller* tCaller) {
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
	UpdateActiveHitObjectCaller* caller = tCaller;
	ActiveHitObject* e = tData;

	updateActiveHitObjectTransparency(e);
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

	list_remove_predicate(&gData.mActiveHitObjects, updateSingleActiveHitObject, &caller);
}

static double getSliderDuration(OsuSliderObject* tObject) {
	double duration = tObject->mPixelLength / (100.0 * gData.mOsu.mDifficulty.mSliderMultiplier) * gData.mTimingPoint.mCurrentMillisecondsPerBeat;
	return duration;
}

static int updateSingleActiveSliderObject(void* tCaller, void* tData) {
	(void)tCaller;
	ActiveSliderObject* e = tData;
	
	double sliderDuration = getSliderDuration(e->mObject);
	int time = (int)(e->mObject->mTime + e->mRepeatNow * sliderDuration);
	int startTime = time - getPreempt();
	if (startTime > (int)getStreamingSoundTimeElapsedInMilliseconds()) return 0;

	OsuHitObject* object = allocMemory(sizeof(OsuHitObject));
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
	list_remove_predicate(&gData.mActiveSliderObjects, updateSingleActiveSliderObject, NULL);
}

#define ACTIVE_SPINNER_POST_TIME 500

static void updateActiveSpinnerControl(ActiveSpinnerObject* e) {
	int time = (int)getStreamingSoundTimeElapsedInMilliseconds();
	if (time < e->mObject->mTime || time > e->mObject->mEndTime) return;

	// TODO: allow controls

}


static void updateActiveSpinnerTextDisplay(ActiveSpinnerObject* e, int tTime) {
	if (tTime >= e->mObject->mTime && getMugenAnimationAnimationNumber(e->mEncouragementAnimationID) < 2001) {
		changeMugenAnimation(e->mEncouragementAnimationID, getMugenAnimation(&gData.mAnimations, 2001));
	}
	if (tTime >= e->mObject->mEndTime && getMugenAnimationAnimationNumber(e->mEncouragementAnimationID) < 2002) {
		changeMugenAnimation(e->mEncouragementAnimationID, getMugenAnimation(&gData.mAnimations, 2002));
	}
}

static void updateActiveSpinnerTextFadeIn(ActiveSpinnerObject* e, int tTime) {
	int preempt = getPreempt();
	int fadeIn = getFadeInTime();
	int start = e->mObject->mTime - preempt;
	int end = start + fadeIn;

	double transparency = (tTime - start) / (double)(end - start);
	transparency = min(1, transparency);

	setMugenAnimationTransparency(e->mEncouragementAnimationID, transparency);
}

static void updateActiveSpinnerTextFadeOut(ActiveSpinnerObject* e, int tTime) {
	int end = e->mObject->mEndTime + ACTIVE_SPINNER_POST_TIME;
	int start = end - HIT_OBJECT_FADE_OUT;

	double transparency = 1 - (tTime - start) / (double)(end - start);
	transparency = max(0, transparency);

	setMugenAnimationTransparency(e->mEncouragementAnimationID, transparency);
}

static void updateActiveSpinnerText(ActiveSpinnerObject* e) {
	int time = (int)getStreamingSoundTimeElapsedInMilliseconds();
	updateActiveSpinnerTextDisplay(e, time);
	updateActiveSpinnerTextFadeIn(e, time);
	updateActiveSpinnerTextFadeOut(e, time);
}

static int isActiveSpinnerOver(ActiveSpinnerObject* e) {
	int time = (int)getStreamingSoundTimeElapsedInMilliseconds();
	return time > e->mObject->mEndTime + ACTIVE_SPINNER_POST_TIME;
}

static int updateSingleActiveSpinnerObject(void* tCaller, void* tData) {
	(void)tCaller;
	ActiveSpinnerObject* e = tData;

	updateActiveSpinnerControl(e);
	updateActiveSpinnerText(e);

	if (isActiveSpinnerOver(e)) {
		unloadActiveSpinnerObject(e);
		return 1;
	}

	return 0;
}

static void updateActiveSpinners() {
	list_remove_predicate(&gData.mActiveSpinnerObjects, updateSingleActiveSpinnerObject, NULL);
}


static void updateOsuHandler(void* tData) {
	(void)tData;
	updateTimingPoint();
	updateNewObjects();
	updateActiveSliders();
	updateActiveHitObjects();
	updateActiveSpinners();
}

ActorBlueprint OsuHandler = {
	.mLoad = loadOsuHandler,
	.mUpdate = updateOsuHandler,
};

void setOsuFile(char * tPath)
{
	strcpy(gData.mPath, tPath);
}

void playOsuMusicFile()
{
	char path[1024];
	char folder[1024];

	getPathToFile(folder, gData.mPath);
	sprintf(path, "%s%s", folder, gData.mOsu.mGeneral.mAudioFileName);

	streamMusicFileOnce(path);
}
