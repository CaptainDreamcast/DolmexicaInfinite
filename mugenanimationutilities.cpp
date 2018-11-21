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
	int mID;
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

} gData;

static void loadUtilityHandler(void* tData) {
	(void)tData;
	gData.mActiveElements = new_int_map();
}

static int unloadSingleActiveElement(void* tCaller, void* tData) {
	(void)tCaller;
	ActiveElement* e = tData;
	freeMemory(e->mData);
	return 1;
}

static void unloadUtilityHandler(void* tData) {
	int_map_remove_predicate(&gData.mActiveElements, unloadSingleActiveElement, NULL);
	delete_int_map(&gData.mActiveElements);
}

static void updateSingleActiveAnimation(ActiveAnimation* e) {
	if (e->mIsInvisible && isRegisteredMugenAnimation(e->mID)) {
		setMugenAnimationVisibility(e->mID, 1);
	}
}

static void updateSingleActiveText(ActiveText* e) {
	if (e->mIsInvisible) {
		setMugenTextVisibility(e->mID, 1);
	}
}


static int updateSingleActiveElement(void* tCaller, void* tData) {
	(void)tCaller;
	ActiveElement* e = tData;
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
	int_map_remove_predicate(&gData.mActiveElements, updateSingleActiveElement, NULL);
}

ActorBlueprint MugenAnimationUtilityHandler = {
	.mLoad = loadUtilityHandler,
	.mUnload = unloadUtilityHandler,
	.mUpdate = updateUtilityHandler,
};

static void addActiveElement(ActiveElementType tType, void* tData) {
	ActiveElement* e = allocMemory(sizeof(ActiveElement));
	e->mType = tType;
	e->mData = tData;
	int_map_push_back_owned(&gData.mActiveElements, e);
}

static void addActiveAnimation(int tID, int tIsInvisible) {
	ActiveAnimation* e = allocMemory(sizeof(ActiveAnimation));
	e->mID = tID;
	e->mIsInvisible = 1;

	addActiveElement(ACTIVE_ELEMENT_TYPE_ANIMATION, e);
}

static void addActiveText(int tID, int tIsInvisible) {
	ActiveText* e = allocMemory(sizeof(ActiveText));
	e->mID = tID;
	e->mIsInvisible = 1;

	addActiveElement(ACTIVE_ELEMENT_TYPE_TEXT, e);
}

void setMugenAnimationInvisibleForOneFrame(int tID)
{
	setMugenAnimationVisibility(tID, 0);
	addActiveAnimation(tID, 1);
}

void setMugenTextInvisibleForOneFrame(int tID)
{
	setMugenTextVisibility(tID, 0);
	addActiveText(tID, 1);
}
