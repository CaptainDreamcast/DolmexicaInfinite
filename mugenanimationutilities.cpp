#include "mugenanimationutilities.h"

#include <stdio.h>

#include <prism/datastructures.h>
#include <prism/mugenanimationhandler.h>
#include <prism/mugentexthandler.h>
#include <prism/log.h>

typedef enum {
	ACTIVE_ELEMENT_TYPE_ANIMATION,
	ACTIVE_ELEMENT_TYPE_TEXT,

} ActiveElementType;

typedef struct {
	MugenAnimationHandlerElement* mElement;
	int mIsInvisible;
} ActiveAnimation;

typedef struct {
	int mID;
	int mIsInvisible;
} ActiveText;

typedef struct {
	ActiveElementType mType;
	void* mData;
} ActiveElement;

static struct {
	IntMap mActiveElements;

} gMugenAnimationUtilityData;

static void loadUtilityHandler(void* tData) {
	(void)tData;
	gMugenAnimationUtilityData.mActiveElements = new_int_map();
}

static int unloadSingleActiveElement(void* tCaller, void* tData) {
	(void)tCaller;
	ActiveElement* e = (ActiveElement*)tData;
	freeMemory(e->mData);
	return 1;
}

static void unloadUtilityHandler(void* /*tData*/) {
	int_map_remove_predicate(&gMugenAnimationUtilityData.mActiveElements, unloadSingleActiveElement, NULL);
	delete_int_map(&gMugenAnimationUtilityData.mActiveElements);
}

static void updateSingleActiveAnimation(ActiveAnimation* e) {
	if (e->mIsInvisible && isRegisteredMugenAnimation(e->mElement)) {
		setMugenAnimationVisibility(e->mElement, 1);
	}
}

static void updateSingleActiveText(ActiveText* e) {
	if (e->mIsInvisible) {
		setMugenTextVisibility(e->mID, 1);
	}
}


static int updateSingleActiveElement(void* tCaller, void* tData) {
	(void)tCaller;
	ActiveElement* e = (ActiveElement*)tData;
	if (e->mType == ACTIVE_ELEMENT_TYPE_ANIMATION) {
		updateSingleActiveAnimation((ActiveAnimation*)e->mData);
	}
	else if (e->mType == ACTIVE_ELEMENT_TYPE_TEXT) {
		updateSingleActiveText((ActiveText*)e->mData);
	}
	else {
		logWarningFormat("Unrecognized format %d", e->mType);
	}
	freeMemory(e->mData);

	return 1;
}

static void updateUtilityHandler(void* tData) {
	(void)tData;
	int_map_remove_predicate(&gMugenAnimationUtilityData.mActiveElements, updateSingleActiveElement, NULL);
}

ActorBlueprint getMugenAnimationUtilityHandler() {
	return makeActorBlueprint(loadUtilityHandler, unloadUtilityHandler, updateUtilityHandler);
};

static void addActiveElement(ActiveElementType tType, void* tData) {
	ActiveElement* e = (ActiveElement*)allocMemory(sizeof(ActiveElement));
	e->mType = tType;
	e->mData = tData;
	int_map_push_back_owned(&gMugenAnimationUtilityData.mActiveElements, e);
}

static void addActiveAnimation(MugenAnimationHandlerElement* tElement) {
	ActiveAnimation* e = (ActiveAnimation*)allocMemory(sizeof(ActiveAnimation));
	e->mElement = tElement;
	e->mIsInvisible = 1;

	addActiveElement(ACTIVE_ELEMENT_TYPE_ANIMATION, e);
}

static void addActiveText(int tID) {
	ActiveText* e = (ActiveText*)allocMemory(sizeof(ActiveText));
	e->mID = tID;
	e->mIsInvisible = 1;

	addActiveElement(ACTIVE_ELEMENT_TYPE_TEXT, e);
}

void setMugenAnimationInvisibleForOneFrame(MugenAnimationHandlerElement* tElement)
{
	setMugenAnimationVisibility(tElement, 0);
	addActiveAnimation(tElement);
}

void setMugenTextInvisibleForOneFrame(int tID)
{
	setMugenTextVisibility(tID, 0);
	addActiveText(tID);
}
