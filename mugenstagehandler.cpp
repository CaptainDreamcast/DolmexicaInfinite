#include "mugenstagehandler.h"

#include <assert.h>

#include <prism/math.h>
#include <prism/mugenanimationhandler.h>

#include "stage.h"

typedef struct {
	int mID;
	Position mReferencePosition;
	Position mOffset;

} StageElementAnimationReference;

typedef struct {
	Vector mVector;

} StageElementIDList;

static struct {
	Vector mStaticElements;

	Position mCameraPosition;
	Position mCameraTargetPosition;

	GeoRectangle mCameraRange;
	Vector3DI mCameraCoordinates;

	Vector3D mCameraSpeed;

	IntMap mStageElementsFromID; // contains StageElementIDList
} gData;



static void loadMugenStageHandler(void* tData) {
	(void)tData;
	gData.mCameraPosition = makePosition(0,0,0);
	gData.mCameraTargetPosition = makePosition(0, 0, 0);
	gData.mCameraRange = makeGeoRectangle(-INF, -INF, 2*INF, 2*INF);
	gData.mCameraSpeed = makePosition(5, 5, 0);

	gData.mStaticElements = new_vector();
	gData.mStageElementsFromID = new_int_map();
}

static void unloadSingleStaticElement(void* tCaller, void* tData) {
	(void)tCaller;
	StaticStageHandlerElement* e = (StaticStageHandlerElement*)tData;

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
		setMugenAnimationVisibility(tSingleAnimation->mID, !(e->mInvisibleFlag || e->mIsInvisible || !e->mIsEnabled));
}

static void updateSingleStaticStageElementTile(StaticStageHandlerElement* e, StageElementAnimationReference* tSingleAnimation) {
	updateSingleStaticStageElementTileVisibility(e, tSingleAnimation);
	if (!e->mIsEnabled) return;

	updateSingleStaticStageElementTileVelocity(e, tSingleAnimation);
	updateSingleStaticStageElementTileReferencePosition(e, tSingleAnimation);
}

static void updateSingleStaticStageElementTileCB(void* tCaller, void* tData) {
	updateStageTileCaller* caller = (updateStageTileCaller*)tCaller;
	StageElementAnimationReference* singleAnimation = (StageElementAnimationReference*)tData;

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
	StaticStageHandlerElement* e = (StaticStageHandlerElement*)tData;

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

ActorBlueprint getDreamMugenStageHandler() {
	return makeActorBlueprint(loadMugenStageHandler, unloadMugenStageHandler, updateMugenStageHandler);
}


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

static void handleSingleTile(int tTile, int* tStart, int* tAmount, int tSize, int tSpacing, double tMinCam, double tMaxCam, double tDeltaScale, double tCoordinates) {
	if (!tTile) {
		*tStart = 0;
		*tAmount = 1;
	}
	else if (tTile == 1) {
		*tStart = (int)tMinCam - tSize - (int)(tCoordinates / 2);
		int length = (int)(((tMaxCam - (tMinCam - tSize)) + tCoordinates)*tDeltaScale);
		*tAmount = length / (tSize + tSpacing) + 1; 
	}
	else {
		*tStart = 0;
		*tAmount = tTile;
	}
}

static void addSingleMugenStageHandlerBackgroundElementTile(StaticStageHandlerElement* e, MugenSpriteFile* tSprites, BlendType tBlendType, GeoRectangle tConstraintRectangle, Vector3D tOffset) {
	StageElementAnimationReference* newAnimation = (StageElementAnimationReference*)allocMemory(sizeof(StageElementAnimationReference));
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
	int stepAmount = vector_size(&e->mAnimation->mSteps);
	Vector3DI size;
	if (stepAmount > 1) {
		size = makeVector3DI(0,0,0);
	}
	else {
		size = getAnimationFirstElementSpriteSize(e->mAnimation, tSprites);
	}

	double deltaScaleX = e->mDelta.x ? (1 / e->mDelta.x) : 1;
	double deltaScaleY = e->mDelta.y ? (1 / e->mDelta.y) : 1;
	double cameraToElementScale = e->mCoordinates.y / getCameraCoordP();
	handleSingleTile(tTile.x, &startX, &amountX, size.x, tTileSpacing.x, gData.mCameraRange.mTopLeft.x*cameraToElementScale, gData.mCameraRange.mBottomRight.x*cameraToElementScale, deltaScaleX, e->mCoordinates.x);
	handleSingleTile(tTile.y, &startY, &amountY, size.y, tTileSpacing.y, gData.mCameraRange.mTopLeft.y*cameraToElementScale, gData.mCameraRange.mBottomRight.y*cameraToElementScale, deltaScaleY, e->mCoordinates.y);
	
	Vector3D offset = makePosition(startX, startY, 0);
	int j;
	for (j = 0; j < amountY; j++) {
		int i;
		offset.x = startX;
		for (i = 0; i < amountX; i++) {
			addSingleMugenStageHandlerBackgroundElementTile(e, tSprites, tBlendType, tConstraintRectangle, offset);
			offset.x += size.x + tTileSpacing.x;
			offset.z += 0.001;
		}
		offset.y += size.y + tTileSpacing.y;
	}

}


static void addStaticElementToIDList(StaticStageHandlerElement* e, int tID) {
	StageElementIDList* elementList;
	if (!int_map_contains(&gData.mStageElementsFromID, tID)) {
		elementList = (StageElementIDList*)allocMemory(sizeof(StageElementIDList));
		elementList->mVector = new_vector();
		int_map_push_owned(&gData.mStageElementsFromID, tID, elementList);
	}
	else {
		elementList = (StageElementIDList*)int_map_get(&gData.mStageElementsFromID, tID);
	}

	vector_push_back(&elementList->mVector, e);
}

void addDreamMugenStageHandlerAnimatedBackgroundElement(Position tStart, MugenAnimation* tAnimation, int tOwnsAnimation, MugenSpriteFile * tSprites, Position tDelta, Vector3DI tTile, Vector3DI tTileSpacing, BlendType tBlendType, GeoRectangle tConstraintRectangle, Vector3D tVelocity, double tStartScaleY, double tScaleDeltaY, int tLayerNo, int tID, Vector3DI tCoordinates)
{
	StaticStageHandlerElement* e = (StaticStageHandlerElement*)allocMemory(sizeof(StaticStageHandlerElement));
	e->mStart = tStart;
	e->mDelta = tDelta;
	e->mCoordinates = tCoordinates;

	e->mVelocity = tVelocity;

	e->mSprites = tSprites;
	e->mAnimation = tAnimation;
	e->mOwnsAnimation = tOwnsAnimation;
	e->mAnimationReferences = new_list();
	e->mTileSize = getAnimationFirstElementSpriteSize(e->mAnimation, tSprites);
	if (!e->mTileSize.x || !e->mTileSize.y) e->mTileSize = makeVector3DI(1, 1, 0);

	e->mStartScaleY = tStartScaleY;
	e->mScaleDeltaY = tScaleDeltaY;
	e->mLayerNo = tLayerNo;

	e->mIsEnabled = 1;
	e->mInvisibleFlag = 0;
	e->mIsInvisible = 0;

	addMugenStageHandlerBackgroundElementTiles(e, tSprites, tTile, tTileSpacing, tBlendType, tConstraintRectangle);
	updateSingleStaticStageElement(e);

	addStaticElementToIDList(e, tID);

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
	StaticStageHandlerElement* e = (StaticStageHandlerElement*)tData;
	SetBackgroundFlagCaller* caller = (SetBackgroundFlagCaller*)tCaller;

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

void setStageElementInvisible(StaticStageHandlerElement* tElement, int tIsInvisible) {
	tElement->mIsInvisible = tIsInvisible;
}

static void pauseSingleMugenAnimationReference(void* tCaller, void* tData) {
	(void)tCaller;
	StageElementAnimationReference* e = (StageElementAnimationReference*)tData;
	pauseMugenAnimation(e->mID);
}

static void unpauseSingleMugenAnimationReference(void* tCaller, void* tData) {
	(void)tCaller;
	StageElementAnimationReference* e = (StageElementAnimationReference*)tData;
	unpauseMugenAnimation(e->mID);
}

void setStageElementEnabled(StaticStageHandlerElement* tElement, int tIsEnabled) {

	if (!tIsEnabled) {
		list_map(&tElement->mAnimationReferences, pauseSingleMugenAnimationReference, NULL);
	}
	else {
		list_map(&tElement->mAnimationReferences, unpauseSingleMugenAnimationReference, NULL);
	}

	tElement->mIsEnabled = tIsEnabled;
}

void setStageElementVelocityX(StaticStageHandlerElement * tElement, double tVelocityX)
{
	tElement->mVelocity.x = tVelocityX;
}

void setStageElementVelocityY(StaticStageHandlerElement * tElement, double tVelocityY)
{
	tElement->mVelocity.y = tVelocityY;
}

void addStageElementVelocityX(StaticStageHandlerElement * tElement, double tVelocityX)
{
	tElement->mVelocity.x += tVelocityX;
}

void addStageElementVelocityY(StaticStageHandlerElement * tElement, double tVelocityY)
{
	tElement->mVelocity.y += tVelocityY;
}

void setStageElementPositionX(StaticStageHandlerElement * tElement, double tPositionX)
{
	tElement->mStart.x = tPositionX;
}

void setStageElementPositionY(StaticStageHandlerElement * tElement, double tPositionY)
{
	tElement->mStart.y = tPositionY;
}

void addStageElementPositionX(StaticStageHandlerElement * tElement, double tPositionX)
{
	tElement->mStart.x += tPositionX;
}

void addStageElementPositionY(StaticStageHandlerElement * tElement, double tPositionY)
{
	tElement->mStart.y += tPositionY;
}

static void changeSingleMugenAnimationReference(void* tCaller, void* tData) {
	int* value = (int*)tCaller;
	StageElementAnimationReference* e = (StageElementAnimationReference*)tData;
	changeMugenAnimation(e->mID, getMugenAnimation(getStageAnimations(), *value));
}

void setStageElementAnimation(StaticStageHandlerElement * tElement, int tAnimation)
{
	list_map(&tElement->mAnimationReferences, unpauseSingleMugenAnimationReference, &tAnimation);
}

Vector * getStageHandlerElementsWithID(int tID)
{
	StageElementIDList* elementList = (StageElementIDList*)int_map_get(&gData.mStageElementsFromID, tID);
	return &elementList->mVector;
}
