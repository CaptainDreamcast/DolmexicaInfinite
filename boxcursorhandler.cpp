#include "boxcursorhandler.h"

#include <prism/datastructures.h>
#include <prism/animation.h>
#include <prism/screeneffect.h>
#include <prism/tweening.h>
#include <prism/log.h>
typedef struct {
	AnimationHandlerElement* mAnimationElement;
	int mTweenID;
	int mIsPaused;
	Position mBasePosition;
} BoxCursor;

static struct {
	TextureData mWhiteTexture;
	
	IntMap mBoxCursors;

} gBoxCursorHandlerData;

static void loadBoxCursorHandler(void* tData) {
	(void)tData;
	setProfilingSectionMarkerCurrentFunction();
	gBoxCursorHandlerData.mWhiteTexture = getEmptyWhiteTexture();
	gBoxCursorHandlerData.mBoxCursors = new_int_map();
}

static void updateBoxCursorHandler(void* tData) {
	(void)tData;
	setProfilingSectionMarkerCurrentFunction();
}

ActorBlueprint getBoxCursorHandler() {
	return makeActorBlueprint(loadBoxCursorHandler, NULL, updateBoxCursorHandler);
}


static void boxCursorCB1(void* tCaller);

static void boxCursorCB2(void* tCaller) {
	BoxCursor* e = (BoxCursor*)tCaller;
	e->mTweenID = tweenDouble(getAnimationTransparencyReference(e->mAnimationElement), 0.2, 0.1, linearTweeningFunction, 20, boxCursorCB1, e);
}

static void boxCursorCB1(void* tCaller) {
	BoxCursor* e = (BoxCursor*)tCaller;
	e->mTweenID = tweenDouble(getAnimationTransparencyReference(e->mAnimationElement), 0.1, 0.2, linearTweeningFunction, 20, boxCursorCB2, e);
}

int addBoxCursor(const Position& tStartPosition, const Position& tOffset, const GeoRectangle2D& tRectangle)
{
	BoxCursor* e = (BoxCursor*)allocMemory(sizeof(BoxCursor));
	e->mAnimationElement = playOneFrameAnimationLoop(tOffset + tRectangle.mTopLeft, &gBoxCursorHandlerData.mWhiteTexture);
	e->mBasePosition = tStartPosition;
	double w = tRectangle.mBottomRight.x - tRectangle.mTopLeft.x;
	double h = tRectangle.mBottomRight.y - tRectangle.mTopLeft.y;
	setAnimationSize(e->mAnimationElement, Vector3D(w, h, 1), Vector3D(0, 0, 0));
	setAnimationBasePositionReference(e->mAnimationElement, &e->mBasePosition);
	setAnimationColor(e->mAnimationElement, 0, 1, 1);
	setAnimationTransparency(e->mAnimationElement, 1);
	boxCursorCB1(e);
	e->mIsPaused = 0;
	
	return int_map_push_back_owned(&gBoxCursorHandlerData.mBoxCursors, e);
}

void removeBoxCursor(int tID)
{
	if (!int_map_contains(&gBoxCursorHandlerData.mBoxCursors, tID)) {
		logWarningFormat("Attempting to remove non-existant box cursor %d. Abort.", tID);
		return;
	}
	BoxCursor* e = (BoxCursor*)int_map_get(&gBoxCursorHandlerData.mBoxCursors, tID);
	if(!e->mIsPaused) removeTween(e->mTweenID);
	removeHandledAnimation(e->mAnimationElement);

	int_map_remove(&gBoxCursorHandlerData.mBoxCursors, tID);
}

void setBoxCursorPosition(int tID, const Position& tPosition)
{
	if (!int_map_contains(&gBoxCursorHandlerData.mBoxCursors, tID)) {
		logWarningFormat("Attempting to use non-existant box cursor %d. Abort.", tID);
		return;
	}

	BoxCursor* e = (BoxCursor*)int_map_get(&gBoxCursorHandlerData.mBoxCursors, tID);
	e->mBasePosition = tPosition;
}

void pauseBoxCursor(int tID)
{
	if (!int_map_contains(&gBoxCursorHandlerData.mBoxCursors, tID)) {
		logWarningFormat("Attempting to pause non-existant box cursor %d. Abort.", tID);
		return;
	}
	BoxCursor* e = (BoxCursor*)int_map_get(&gBoxCursorHandlerData.mBoxCursors, tID);
	if (e->mIsPaused) return;

	setAnimationColor(e->mAnimationElement, 0, 0.6, 0.6);
	removeTween(e->mTweenID);
	setAnimationTransparency(e->mAnimationElement, 0.2);
	e->mIsPaused = 1;
}

void resumeBoxCursor(int tID)
{
	if (!int_map_contains(&gBoxCursorHandlerData.mBoxCursors, tID)) {
		logWarningFormat("Attempting to resume non-existant box cursor %d. Abort.", tID);
		return;
	}
	BoxCursor* e = (BoxCursor*)int_map_get(&gBoxCursorHandlerData.mBoxCursors, tID);
	if (!e->mIsPaused) return;

	setAnimationColor(e->mAnimationElement, 0, 1, 1);
	boxCursorCB1(e);
	e->mIsPaused = 0;
}
