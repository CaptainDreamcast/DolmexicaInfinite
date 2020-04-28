#include "mugenanimationutilities.h"

#include <stdio.h>

#include <prism/datastructures.h>
#include <prism/mugenanimationhandler.h>
#include <prism/mugentexthandler.h>
#include <prism/log.h>
#include <prism/math.h>

typedef enum {
	ACTIVE_ELEMENT_TYPE_ANIMATION,
	ACTIVE_ELEMENT_TYPE_TEXT,
} ActiveVisibilityElementType;

typedef struct {
	MugenAnimationHandlerElement* mElement;
	int mIsInvisible;
} ActiveVisibilityAnimation;

typedef struct {
	int mID;
	int mIsInvisible;
} ActiveVisibilityText;

typedef struct {
	ActiveVisibilityElementType mType;
	void* mData;
} ActiveVisibilityElement;

typedef struct {
	MugenAnimationHandlerElement* mAnimationElement;
	int mNow;
	int mDuration;
	Vector3D mAddition;
	Vector3D mMultiplier;
	Vector3D mSineAmplitude;
	int mSinePeriod;
	int mInvertAll;
	double mColorFactor;
} ActivePaletteElement;

static struct {
	IntMap mActiveVisibilityElements;
	std::map<MugenAnimationHandlerElement*, ActivePaletteElement> mActivePaletteElements;
} gMugenAnimationUtilityData;

static void loadVisibilityUtility() {
	gMugenAnimationUtilityData.mActiveVisibilityElements = new_int_map();
}

static void loadPaletteUtility() {
	gMugenAnimationUtilityData.mActivePaletteElements.clear();
}

static void loadUtilityHandler(void*) {
	setProfilingSectionMarkerCurrentFunction();
	loadVisibilityUtility();
	loadPaletteUtility();
}

static int unloadSingleActiveVisibilityElement(void* tCaller, void* tData) {
	(void)tCaller;
	ActiveVisibilityElement* e = (ActiveVisibilityElement*)tData;
	freeMemory(e->mData);
	return 1;
}

static void unloadVisibilityUtility() {
	int_map_remove_predicate(&gMugenAnimationUtilityData.mActiveVisibilityElements, unloadSingleActiveVisibilityElement, NULL);
	delete_int_map(&gMugenAnimationUtilityData.mActiveVisibilityElements);
}

static void unloadPaletteUtility() {
	gMugenAnimationUtilityData.mActivePaletteElements.clear();
}

static void unloadUtilityHandler(void*) {
	setProfilingSectionMarkerCurrentFunction();
	unloadVisibilityUtility();
	unloadPaletteUtility();
}

static void updateSingleActiveVisibilityAnimation(ActiveVisibilityAnimation* e) {
	if (e->mIsInvisible && isRegisteredMugenAnimation(e->mElement)) {
		setMugenAnimationVisibility(e->mElement, 1);
	}
}

static void updateSingleActiveVisibilityText(ActiveVisibilityText* e) {
	if (e->mIsInvisible) {
		setMugenTextVisibility(e->mID, 1);
	}
}

static int updateSingleActiveVisibilityElement(void* tCaller, void* tData) {
	(void)tCaller;
	ActiveVisibilityElement* e = (ActiveVisibilityElement*)tData;
	if (e->mType == ACTIVE_ELEMENT_TYPE_ANIMATION) {
		updateSingleActiveVisibilityAnimation((ActiveVisibilityAnimation*)e->mData);
	}
	else if (e->mType == ACTIVE_ELEMENT_TYPE_TEXT) {
		updateSingleActiveVisibilityText((ActiveVisibilityText*)e->mData);
	}
	else {
		logWarningFormat("Unrecognized format %d", e->mType);
	}
	freeMemory(e->mData);

	return 1;
}

static void updateInvisibilityUtility() {
	int_map_remove_predicate(&gMugenAnimationUtilityData.mActiveVisibilityElements, updateSingleActiveVisibilityElement, NULL);
}

static void resetPaletteEffectsForAnimation(MugenAnimationHandlerElement* e) {
	setMugenAnimationColor(e, 1, 1, 1);
	setMugenAnimationColorOffset(e, 0, 0, 0);
	setMugenAnimationColorInverted(e, 0);
	setMugenAnimationColorFactor(e, 1);
}

static void updateSinglePaletteForPaletteEffect(ActivePaletteElement& tElement) {
	auto addition = tElement.mAddition + tElement.mSineAmplitude * std::sin(2 * M_PI * (tElement.mNow / double(tElement.mSinePeriod)));
	setMugenAnimationColorOffset(tElement.mAnimationElement, addition.x, addition.y, addition.z);
	setMugenAnimationColor(tElement.mAnimationElement, tElement.mMultiplier.x, tElement.mMultiplier.y, tElement.mMultiplier.z);
	setMugenAnimationColorInverted(tElement.mAnimationElement, tElement.mInvertAll);
	setMugenAnimationColorFactor(tElement.mAnimationElement, tElement.mColorFactor);
}

static int updateSinglePaletteEffect(ActivePaletteElement& tElement) {
	if (!isRegisteredMugenAnimation(tElement.mAnimationElement)) return 1;

	if (tElement.mSineAmplitude.x || tElement.mSineAmplitude.y || tElement.mSineAmplitude.z) {
		updateSinglePaletteForPaletteEffect(tElement);
	}

	tElement.mNow++;
	if (tElement.mDuration != -1 && tElement.mNow >= tElement.mDuration) {
		resetPaletteEffectsForAnimation(tElement.mAnimationElement);
		return 1;
	}

	return 0;
}

static void updatePaletteUtility() {
	auto it = gMugenAnimationUtilityData.mActivePaletteElements.begin();
	while (it != gMugenAnimationUtilityData.mActivePaletteElements.end()) {
		auto next = it;
		next++;
		if (updateSinglePaletteEffect(it->second)) {
			gMugenAnimationUtilityData.mActivePaletteElements.erase(it);
		}
		it = next;
	}
}

static void updateUtilityHandler(void*) {
	setProfilingSectionMarkerCurrentFunction();
	updateInvisibilityUtility();
	updatePaletteUtility();
}

ActorBlueprint getMugenAnimationUtilityHandler() {
	return makeActorBlueprint(loadUtilityHandler, unloadUtilityHandler, updateUtilityHandler);
};

static void addActiveVisibilityElement(ActiveVisibilityElementType tType, void* tData) {
	ActiveVisibilityElement* e = (ActiveVisibilityElement*)allocMemory(sizeof(ActiveVisibilityElement));
	e->mType = tType;
	e->mData = tData;
	int_map_push_back_owned(&gMugenAnimationUtilityData.mActiveVisibilityElements, e);
}

static void addActiveVisibilityAnimation(MugenAnimationHandlerElement* tElement) {
	ActiveVisibilityAnimation* e = (ActiveVisibilityAnimation*)allocMemory(sizeof(ActiveVisibilityAnimation));
	e->mElement = tElement;
	e->mIsInvisible = 1;

	addActiveVisibilityElement(ACTIVE_ELEMENT_TYPE_ANIMATION, e);
}

static void addActiveVisibilityText(int tID) {
	ActiveVisibilityText* e = (ActiveVisibilityText*)allocMemory(sizeof(ActiveVisibilityText));
	e->mID = tID;
	e->mIsInvisible = 1;

	addActiveVisibilityElement(ACTIVE_ELEMENT_TYPE_TEXT, e);
}

void setMugenAnimationInvisibleForOneFrame(MugenAnimationHandlerElement* tElement)
{
	if (!getMugenAnimationVisibility(tElement)) return;
	setMugenAnimationVisibility(tElement, 0);
	addActiveVisibilityAnimation(tElement);
}

void setMugenTextInvisibleForOneFrame(int tID)
{
	if (!getMugenTextVisibility(tID)) return;
	setMugenTextVisibility(tID, 0);
	addActiveVisibilityText(tID);
}

void setMugenAnimationPaletteEffectForDuration(MugenAnimationHandlerElement* tElement, int tDuration, const Vector3D& tAddition, const Vector3D& tMultiplier, const Vector3D& tSineAmplitude, int tSinePeriod, int tInvertAll, double tColorFactor)
{
	if (!tDuration) {
		if (stl_map_contains(gMugenAnimationUtilityData.mActivePaletteElements, tElement)) {
			resetPaletteEffectsForAnimation(tElement);
			gMugenAnimationUtilityData.mActivePaletteElements.erase(tElement);
		}
		return;
	}

	ActivePaletteElement& e = gMugenAnimationUtilityData.mActivePaletteElements[tElement];
	e.mAnimationElement = tElement;
	e.mNow = 0;
	e.mDuration = tDuration;
	e.mAddition = tAddition;
	e.mMultiplier = tMultiplier;
	e.mSineAmplitude = tSineAmplitude;
	e.mSinePeriod = tSinePeriod;
	e.mInvertAll = tInvertAll;
	e.mColorFactor = tColorFactor;

	updateSinglePaletteForPaletteEffect(e);
}

void removeMugenAnimationPaletteEffectIfExists(MugenAnimationHandlerElement * tElement)
{
	if (stl_map_contains(gMugenAnimationUtilityData.mActivePaletteElements, tElement)) {
		resetPaletteEffectsForAnimation(tElement);
		gMugenAnimationUtilityData.mActivePaletteElements.erase(tElement);
	}
}
