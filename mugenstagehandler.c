#include "mugenstagehandler.h"

#include <assert.h>

#include <prism/datastructures.h>
#include <prism/math.h>
#include <prism/mugenanimationhandler.h>
#include <prism/geometry.h>

#include "stage.h"

typedef struct {
	int mID;
	Position mReferencePosition;
	Position mOffset;

} StageElementAnimationReference;

typedef struct {
	
	Position mStart;
	Position mDelta;
	Vector3DI mCoordinates;
	Vector3D mVelocity;

	MugenSpriteFile* mSprites;
	MugenAnimation* mAnimation;
	int mOwnsAnimation;
	List mAnimationReferences;

	double mStartScaleY;
	double mScaleDeltaY;

	Vector3DI mTileSize;
	int mLayerNo;

	int mInvisibleFlag;
} StaticStageHandlerElement;

static struct {
	Vector mStaticElements;

	Position mCameraPosition;
	Position mCameraTargetPosition;

	GeoRectangle mCameraRange;
	Vector3DI mCameraCoordinates;

	Vector3D mCameraSpeed;
} gData;



static void loadMugenStageHandler(void* tData) {
	(void)tData;
	gData.mCameraPosition = makePosition(0,0,0);
	gData.mCameraTargetPosition = makePosition(0, 0, 0);
	gData.mCameraRange = makeGeoRectangle(-INF, -INF, 2*INF, 2*INF);
	gData.mCameraSpeed = makePosition(5, 5, 0);

	gData.mStaticElements = new_vector();

}

static void unloadSingleStaticElement(void* tCaller, void* tData) {
	(void)tCaller;
	StaticStageHandlerElement* e = tData;

	if (e->mOwnsAnimation) {
		destroyMugenAnimation(e->mAnimation);
	}

	delete_list(&e->mAnimationReferences);
}

static void unloadMugenStageHandler(void* tData) {
	(void)tData;

	vector_map(&gData.mStaticElements, unloadSingleStaticElement, NULL);
	delete_vector(&gData.mStaticElements);
}

static int getCameraCoordP() {
	return gData.mCameraCoordinates.y;
}

typedef struct{
	StaticStageHandlerElement* e;
} updateStageTileCaller;

int getTileAmountSingleAxis(int tSize, double tMinCam, double tMaxCam, int tScreenSize) {
	int length = (int)(tMaxCam - (tMinCam - tSize)) + tScreenSize;
	return length / tSize + 1;
}

Vector3DI getTileAmount(StaticStageHandlerElement* e) {
	double cameraToElementScale = e->mCoordinates.y / getCameraCoordP();
	int x = getTileAmountSingleAxis(e->mTileSize.x, gData.mCameraRange.mTopLeft.x*cameraToElementScale, gData.mCameraRange.mBottomRight.x*cameraToElementScale, e->mCoordinates.x);
	int y = getTileAmountSingleAxis(e->mTileSize.y, gData.mCameraRange.mTopLeft.y*cameraToElementScale, gData.mCameraRange.mBottomRight.y*cameraToElementScale, e->mCoordinates.y);

	return makeVector3DI(x, y, 0);
}

static void updateSingleStaticStageElementTileVelocity(StaticStageHandlerElement* e, StageElementAnimationReference* tSingleAnimation) {
	if (e->mVelocity.x == 0 && e->mVelocity.y == 0) return; // TODO: fix

	tSingleAnimation->mOffset = vecAdd(tSingleAnimation->mOffset, e->mVelocity);

	Vector3DI amount = getTileAmount(e);
	Vector3D offset = getAnimationFirstElementSpriteOffset(e->mAnimation, e->mSprites);

	double cameraToElementScale = e->mCoordinates.y / getCameraCoordP();
	Vector3D minCam;
	minCam.x = gData.mCameraRange.mTopLeft.x*cameraToElementScale;
	minCam.y = gData.mCameraRange.mTopLeft.y*cameraToElementScale;

	Vector3D maxCam;
	maxCam.x = gData.mCameraRange.mBottomRight.x*cameraToElementScale;
	maxCam.y = gData.mCameraRange.mBottomRight.y*cameraToElementScale;

	double right = tSingleAnimation->mOffset.x + e->mTileSize.x - offset.x + (e->mCoordinates.x / 2);
	if (right < minCam.x) {
		tSingleAnimation->mOffset.x += amount.x*e->mTileSize.x;
	}
	double left = tSingleAnimation->mOffset.x - offset.x + (e->mCoordinates.x / 2);
	double cameraRight = maxCam.x + e->mCoordinates.x;
	if (left > cameraRight) {
		tSingleAnimation->mOffset.x -= amount.x*e->mTileSize.x;
	}

	double down = tSingleAnimation->mOffset.y + e->mTileSize.y - offset.y;
	if (down < minCam.y) {
		tSingleAnimation->mOffset.y += amount.y*e->mTileSize.y;
	}
	double up = tSingleAnimation->mOffset.y - offset.y;
	double cameraDown = maxCam.y + e->mCoordinates.y;
	if (up > cameraDown) {
		tSingleAnimation->mOffset.y -= amount.y*e->mTileSize.y;
	}
}

static void updateSingleStaticStageElementTileReferencePosition(StaticStageHandlerElement* e, StageElementAnimationReference* tSingleAnimation) {
	double scale = e->mCoordinates.y / (double)gData.mCameraCoordinates.y;

	Vector3D deltaInCameraSpace = makePosition(getDreamCameraPositionX(getCameraCoordP()), getDreamCameraPositionY(getCameraCoordP()), 0);
	deltaInCameraSpace = vecScale(deltaInCameraSpace, -1);
	deltaInCameraSpace = vecScale3D(deltaInCameraSpace, e->mDelta);
	deltaInCameraSpace.z = 0;


	Vector3D deltaInElementSpace = vecScale(deltaInCameraSpace, scale); // TODO: fix
	tSingleAnimation->mReferencePosition = vecAdd(e->mStart, deltaInElementSpace);
	tSingleAnimation->mReferencePosition = vecAdd(tSingleAnimation->mReferencePosition, tSingleAnimation->mOffset);
	tSingleAnimation->mReferencePosition = vecSub(tSingleAnimation->mReferencePosition, makePosition(-e->mCoordinates.x / 2, 0, 0));
	tSingleAnimation->mReferencePosition.z++;

	double heightDelta = -getDreamCameraPositionY(e->mCoordinates.y); // TODO: camera start position
	double heightScale = e->mStartScaleY + (e->mScaleDeltaY * heightDelta);
	setMugenAnimationDrawScale(tSingleAnimation->mID, makePosition(1, heightScale, 1));
}

static void updateSingleStaticStageElementTileVisibility(StaticStageHandlerElement* e, StageElementAnimationReference* tSingleAnimation) { 
		setMugenAnimationVisibility(tSingleAnimation->mID, !e->mInvisibleFlag);
}

static void updateSingleStaticStageElementTile(StaticStageHandlerElement* e, StageElementAnimationReference* tSingleAnimation) {
	updateSingleStaticStageElementTileVelocity(e, tSingleAnimation);
	updateSingleStaticStageElementTileReferencePosition(e, tSingleAnimation);
	updateSingleStaticStageElementTileVisibility(e, tSingleAnimation);
}

static void updateSingleStaticStageElementTileCB(void* tCaller, void* tData) {
	updateStageTileCaller* caller = tCaller;
	StageElementAnimationReference* singleAnimation = tData;

	updateSingleStaticStageElementTile(caller->e, singleAnimation);
}

static void updateSingleStaticStageElementVisibilityFlag(StaticStageHandlerElement* e) {
	e->mInvisibleFlag = 0;
}

static void updateSingleStaticStageElement(StaticStageHandlerElement* e) {
	updateStageTileCaller caller;
	caller.e = e;
	list_map(&e->mAnimationReferences, updateSingleStaticStageElementTileCB, &caller);	
	updateSingleStaticStageElementVisibilityFlag(e);
}

static void updateSingleStaticStageElementCB(void* tCaller, void* tData) {
	(void)tCaller;
	StaticStageHandlerElement* e = tData;

	updateSingleStaticStageElement(e);
}

static void updateCamera() {
	Vector3D delta = vecSub(gData.mCameraTargetPosition, gData.mCameraPosition);
	delta.z = 0;
	if (vecLength(delta) < 1e-6) return;
	
	gData.mCameraPosition = vecAdd(gData.mCameraPosition, vecScale(delta, 0.95));
}

static void updateMugenStageHandler(void* tData) {
	(void)tData;

	updateCamera();

	vector_map(&gData.mStaticElements, updateSingleStaticStageElementCB, NULL);
}

ActorBlueprint DreamMugenStageHandler = {
	.mLoad = loadMugenStageHandler,
	.mUnload = unloadMugenStageHandler,
	.mUpdate = updateMugenStageHandler,
};


void setDreamMugenStageHandlerCameraCoordinates(Vector3DI tCoordinates)
{
	gData.mCameraCoordinates = tCoordinates;
}

void setDreamMugenStageHandlerCameraRange(GeoRectangle tRect)
{
	gData.mCameraRange = tRect;
}

void setDreamMugenStageHandlerCameraPosition(Position p)
{
	gData.mCameraTargetPosition = p;
}

void addDreamMugenStageHandlerCameraPositionX(double tX)
{
	gData.mCameraTargetPosition.x += tX;
	gData.mCameraTargetPosition = clampPositionToGeoRectangle(gData.mCameraTargetPosition, gData.mCameraRange);
	gData.mCameraPosition.x = gData.mCameraTargetPosition.x;

}

void setDreamMugenStageHandlerCameraPositionX(double tX)
{
	gData.mCameraTargetPosition.x = tX;
	gData.mCameraTargetPosition = clampPositionToGeoRectangle(gData.mCameraTargetPosition, gData.mCameraRange);
	gData.mCameraPosition.x = gData.mCameraTargetPosition.x;
}

void addDreamMugenStageHandlerCameraPositionY(double tY) {
	gData.mCameraTargetPosition.y += tY;
	gData.mCameraTargetPosition = clampPositionToGeoRectangle(gData.mCameraTargetPosition, gData.mCameraRange);
	gData.mCameraPosition.y = gData.mCameraTargetPosition.y;
}

void setDreamMugenStageHandlerCameraPositionY(double tY)
{
	gData.mCameraTargetPosition.y = tY;
	gData.mCameraTargetPosition = clampPositionToGeoRectangle(gData.mCameraTargetPosition, gData.mCameraRange);
	gData.mCameraPosition.y = gData.mCameraTargetPosition.y;
}

void resetDreamMugenStageHandlerCameraPosition()
{
	gData.mCameraPosition.x = gData.mCameraTargetPosition.x = 0; // TODO: properly
	gData.mCameraPosition.y = gData.mCameraTargetPosition.y = 0;
	
}

static void handleSingleTile(int tTile, int* tStart, int* tAmount, int tSize, double tMinCam, double tMaxCam, double tDeltaScale, double tCoordinates) {
	if (!tTile) {
		*tStart = 0;
		*tAmount = 1;
	}
	else if (tTile == 1) {
		*tStart = (int)tMinCam - tSize;
		int length = (int)(((tMaxCam - (tMinCam - tSize)) + tCoordinates)*tDeltaScale);
		*tAmount = length / tSize + 1;
	}
	else {
		*tStart = 0;
		*tAmount = tTile;
	}
}

static void addSingleMugenStageHandlerBackgroundElementTile(StaticStageHandlerElement* e, MugenSpriteFile* tSprites, BlendType tBlendType, GeoRectangle tConstraintRectangle, Vector3D tOffset) {
	StageElementAnimationReference* newAnimation = allocMemory(sizeof(StageElementAnimationReference));
	newAnimation->mID = addMugenAnimation(e->mAnimation, tSprites, makePosition(0, 0, 0));
	setMugenAnimationBasePosition(newAnimation->mID, &newAnimation->mReferencePosition);
	newAnimation->mOffset = tOffset;
	setMugenAnimationBlendType(newAnimation->mID, tBlendType);
	setMugenAnimationConstraintRectangle(newAnimation->mID, tConstraintRectangle);
	setMugenAnimationDrawScale(newAnimation->mID, makePosition(1, e->mStartScaleY, 1));

	list_push_back_owned(&e->mAnimationReferences, newAnimation);
}

static void addMugenStageHandlerBackgroundElementTiles(StaticStageHandlerElement* e, MugenSpriteFile* tSprites, Vector3DI tTile, Vector3DI tTileSpacing, BlendType tBlendType, GeoRectangle tConstraintRectangle) {
	int startX;
	int startY;
	int amountX;
	int amountY;
	Vector3DI size = getAnimationFirstElementSpriteSize(e->mAnimation, tSprites);

	assert(!tTileSpacing.x); // TODO
	assert(!tTileSpacing.y); // TODO

	double deltaScaleX = e->mDelta.x ? (1 / e->mDelta.x) : 1;
	double deltaScaleY = e->mDelta.y ? (1 / e->mDelta.y) : 1;
	double cameraToElementScale = e->mCoordinates.y / getCameraCoordP();
	handleSingleTile(tTile.x, &startX, &amountX, size.x, gData.mCameraRange.mTopLeft.x*cameraToElementScale, gData.mCameraRange.mBottomRight.x*cameraToElementScale, deltaScaleX, e->mCoordinates.x);
	handleSingleTile(tTile.y, &startY, &amountY, size.y, gData.mCameraRange.mTopLeft.y*cameraToElementScale, gData.mCameraRange.mBottomRight.y*cameraToElementScale, deltaScaleY, e->mCoordinates.y);

	Vector3D offset = makePosition(0, startY, 0);
	int j;
	for (j = 0; j < amountY; j++) {
		int i;
		offset.x = startX;
		for (i = 0; i < amountX; i++) {
			addSingleMugenStageHandlerBackgroundElementTile(e, tSprites, tBlendType, tConstraintRectangle, offset);
			offset.x += size.x;
			offset.z += 0.001;
		}
		offset.y += size.y;
	}

}


void addDreamMugenStageHandlerAnimatedBackgroundElement(Position tStart, MugenAnimation* tAnimation, int tOwnsAnimation, MugenSpriteFile * tSprites, Position tDelta, Vector3DI tTile, Vector3DI tTileSpacing, BlendType tBlendType, GeoRectangle tConstraintRectangle, Vector3D tVelocity, double tStartScaleY, double tScaleDeltaY, int tLayerNo, Vector3DI tCoordinates)
{
	StaticStageHandlerElement* e = allocMemory(sizeof(StaticStageHandlerElement));
	e->mStart = tStart;
	e->mDelta = tDelta;
	e->mCoordinates = tCoordinates;

	e->mVelocity = tVelocity;

	e->mSprites = tSprites;
	e->mAnimation = tAnimation;
	e->mOwnsAnimation = tOwnsAnimation;
	e->mAnimationReferences = new_list();
	e->mTileSize = getAnimationFirstElementSpriteSize(e->mAnimation, tSprites);

	e->mStartScaleY = tStartScaleY;
	e->mScaleDeltaY = tScaleDeltaY;
	e->mLayerNo = tLayerNo;

	e->mInvisibleFlag = 0;

	addMugenStageHandlerBackgroundElementTiles(e, tSprites, tTile, tTileSpacing, tBlendType, tConstraintRectangle);
	updateSingleStaticStageElement(e);

	vector_push_back_owned(&gData.mStaticElements, e);
}

Position * getDreamMugenStageHandlerCameraPositionReference()
{
	return &gData.mCameraPosition;
}

typedef struct {
	int mIgnoredLayer;
} SetBackgroundFlagCaller;

static void setSingleStaticElementInvisibleForOneFrame(void* tCaller, void* tData) {
	StaticStageHandlerElement* e = tData;
	SetBackgroundFlagCaller* caller = tCaller;

	if (e->mLayerNo == caller->mIgnoredLayer) return;

	e->mInvisibleFlag = 1;
}

void setDreamStageInvisibleForOneFrame()
{
	SetBackgroundFlagCaller caller;
	caller.mIgnoredLayer = -1;
	vector_map(&gData.mStaticElements, setSingleStaticElementInvisibleForOneFrame, &caller);
}

void setDreamStageLayer1InvisibleForOneFrame()
{
	SetBackgroundFlagCaller caller;
	caller.mIgnoredLayer = 0;
	vector_map(&gData.mStaticElements, setSingleStaticElementInvisibleForOneFrame, &caller);
}
