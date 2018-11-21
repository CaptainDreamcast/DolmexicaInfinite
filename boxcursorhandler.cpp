#include "boxcursorhandler.h"

#include <prism/datastructures.h>
#include <prism/animation.h>
#include <prism/screeneffect.h>
#include <prism/tweening.h>
#include <prism/log.h>
typedef struct {
	int mAnimationID;
	int mTweenID;

	Position mBasePosition;
} BoxCursor;

static struct {
	TextureData mWhiteTexture;
	
	IntMap mBoxCursors;

} gData;

static void loadBoxCursorHandler(void* tData) {
	(void)tData;
	gData.mWhiteTexture = getEmptyWhiteTexture();
	gData.mBoxCursors = new_int_map();
}

static void updateBoxCursorHandler(void* tData) {
	(void)tData;
}


ActorBlueprint BoxCursorHandler = {
	.mLoad = loadBoxCursorHandler,
    .mUnload = NULL,
	.mUpdate = updateBoxCursorHandler,
    .mDraw = NULL,
    .mIsActive = NULL
};


static void boxCursorCB1(void* tCaller);

static void boxCursorCB2(void* tCaller) {
	BoxCursor* e = tCaller;
	e->mTweenID = tweenDouble(getAnimationTransparencyReference(e->mAnimationID), 0.2, 0.1, linearTweeningFunction, 20, boxCursorCB1, e);
}

static void boxCursorCB1(void* tCaller) {
	BoxCursor* e = tCaller;
	e->mTweenID = tweenDouble(getAnimationTransparencyReference(e->mAnimationID), 0.1, 0.2, linearTweeningFunction, 20, boxCursorCB2, e);
}

int addBoxCursor(Position tStartPosition, Position tOffset, GeoRectangle tRectangle)
{
	BoxCursor* e = allocMemory(sizeof(BoxCursor));
	tRectangle.mTopLeft.z = 0;
	tOffset = vecAdd(tOffset, tRectangle.mTopLeft);
	e->mAnimationID = playOneFrameAnimationLoop(tOffset, &gData.mWhiteTexture); 
	e->mBasePosition = tStartPosition;
	double w = tRectangle.mBottomRight.x - tRectangle.mTopLeft.x;
	double h = tRectangle.mBottomRight.y - tRectangle.mTopLeft.y;
	setAnimationSize(e->mAnimationID, makePosition(w, h, 1), makePosition(0, 0, 0));
	setAnimationBasePositionReference(e->mAnimationID, &e->mBasePosition);
	setAnimationColor(e->mAnimationID, 0, 1, 1);
	setAnimationTransparency(e->mAnimationID, 1);
	boxCursorCB1(e);
	
	return int_map_push_back_owned(&gData.mBoxCursors, e);
}

void removeBoxCursor(int tID)
{
	if (!int_map_contains(&gData.mBoxCursors, tID)) {
		logWarningFormat("Attempting to remove non-existant box cursor %d. Abort.", tID);
		return;
	}
	BoxCursor* e = int_map_get(&gData.mBoxCursors, tID);
	removeTween(e->mTweenID);
	removeHandledAnimation(e->mAnimationID);

	int_map_remove(&gData.mBoxCursors, tID);
}

void setBoxCursorPosition(int tID, Position tPosition)
{
	if (!int_map_contains(&gData.mBoxCursors, tID)) {
		logWarningFormat("Attempting to use non-existant box cursor %d. Abort.", tID);
		return;
	}

	BoxCursor* e = int_map_get(&gData.mBoxCursors, tID);
	e->mBasePosition = tPosition;
}
