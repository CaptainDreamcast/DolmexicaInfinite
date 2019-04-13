#include "mugenstagehandler.h"

#include <assert.h>

#include <prism/math.h>
#include <prism/mugenanimationhandler.h>

#include "stage.h"

using namespace std;

typedef struct {
	vector<StaticStageHandlerElement*> mVector;
} StageElementIDList;

static struct {
	list<StaticStageHandlerElement> mStaticElements;

	Position mCameraPosition;
	Position mCameraTargetPosition;

	GeoRectangle mCameraRange;
	Vector3DI mCameraCoordinates;

	Vector3D mCameraSpeed;

	map<int, StageElementIDList> mStageElementsFromID;
} gMugenStageHandlerData;



static void loadMugenStageHandler(void* tData) {
	(void)tData;
	gMugenStageHandlerData.mCameraPosition = makePosition(0,0,0);
	gMugenStageHandlerData.mCameraTargetPosition = makePosition(0, 0, 0);
	gMugenStageHandlerData.mCameraRange = makeGeoRectangle(-INF, -INF, 2*INF, 2*INF);
	gMugenStageHandlerData.mCameraSpeed = makePosition(5, 5, 0);

	gMugenStageHandlerData.mStaticElements.clear();
	gMugenStageHandlerData.mStageElementsFromID.clear();
}

static void unloadSingleStaticElement(void* tCaller, StaticStageHandlerElement& tData) {
	(void)tCaller;
	StaticStageHandlerElement* e = &tData;

	if (e->mOwnsAnimation) {
		destroyMugenAnimation(e->mAnimation);
	}

	e->mAnimationReferences.clear();
}

static void unloadMugenStageHandler(void* tData) {
	(void)tData;

	stl_list_map(gMugenStageHandlerData.mStaticElements, unloadSingleStaticElement);
	gMugenStageHandlerData.mStaticElements.clear();
	gMugenStageHandlerData.mStageElementsFromID.clear();
}

static int getCameraCoordP() {
	return gMugenStageHandlerData.mCameraCoordinates.y;
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
	int x = getTileAmountSingleAxis(e->mTileSize.x, gMugenStageHandlerData.mCameraRange.mTopLeft.x*cameraToElementScale, gMugenStageHandlerData.mCameraRange.mBottomRight.x*cameraToElementScale, e->mCoordinates.x);
	int y = getTileAmountSingleAxis(e->mTileSize.y, gMugenStageHandlerData.mCameraRange.mTopLeft.y*cameraToElementScale, gMugenStageHandlerData.mCameraRange.mBottomRight.y*cameraToElementScale, e->mCoordinates.y);

	return makeVector3DI(x, y, 0);
}

static void updateSingleStaticStageElementTileVelocity(StaticStageHandlerElement* e, StageElementAnimationReference* tSingleAnimation) {
	if (e->mVelocity.x == 0 && e->mVelocity.y == 0) return; // TODO: fix

	tSingleAnimation->mOffset = vecAdd(tSingleAnimation->mOffset, e->mVelocity);

	Vector3DI amount = getTileAmount(e);
	Vector3D offset = getAnimationFirstElementSpriteOffset(e->mAnimation, e->mSprites);

	double cameraToElementScale = e->mCoordinates.y / getCameraCoordP();
	Vector3D minCam;
	minCam.x = gMugenStageHandlerData.mCameraRange.mTopLeft.x*cameraToElementScale;
	minCam.y = gMugenStageHandlerData.mCameraRange.mTopLeft.y*cameraToElementScale;

	Vector3D maxCam;
	maxCam.x = gMugenStageHandlerData.mCameraRange.mBottomRight.x*cameraToElementScale;
	maxCam.y = gMugenStageHandlerData.mCameraRange.mBottomRight.y*cameraToElementScale;

	int totalSizeX, totalSizeY;
	int stepAmount = vector_size(&e->mAnimation->mSteps);
	if (stepAmount > 1) {
		totalSizeX = e->mTileSpacing.x;
		totalSizeY = e->mTileSpacing.y;
	}
	else {
		totalSizeX = e->mTileSize.x + e->mTileSpacing.x;
		totalSizeY = e->mTileSize.y + e->mTileSpacing.y;
	}

	double right = tSingleAnimation->mOffset.x + e->mTileSize.x - offset.x + (e->mCoordinates.x / 2);
	if (right < minCam.x) {
		tSingleAnimation->mOffset.x += amount.x*totalSizeX;
	}
	double left = tSingleAnimation->mOffset.x - offset.x + (e->mCoordinates.x / 2);
	double cameraRight = maxCam.x + e->mCoordinates.x;
	if (left > cameraRight) {
		tSingleAnimation->mOffset.x -= amount.x*totalSizeX;
	}

	double down = tSingleAnimation->mOffset.y + e->mTileSize.y - offset.y;
	if (down < minCam.y) {
		tSingleAnimation->mOffset.y += amount.y*totalSizeY;
	}
	double up = tSingleAnimation->mOffset.y - offset.y;
	double cameraDown = maxCam.y + e->mCoordinates.y;
	if (up > cameraDown) {
		tSingleAnimation->mOffset.y -= amount.y*totalSizeY;
	}
}

static void updateSingleStaticStageElementTileReferencePosition(StaticStageHandlerElement* e, StageElementAnimationReference* tSingleAnimation) {
	double scale = e->mCoordinates.y / (double)gMugenStageHandlerData.mCameraCoordinates.y;

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

static void updateSingleStaticStageElementTileCB(updateStageTileCaller* tCaller, StageElementAnimationReference& tData) {
	StageElementAnimationReference* singleAnimation = &tData;

	updateSingleStaticStageElementTile(tCaller->e, singleAnimation);
}

static void updateSingleStaticStageElementVisibilityFlag(StaticStageHandlerElement* e) {
	e->mInvisibleFlag = 0;
}

static void updateSingleStaticStageElement(StaticStageHandlerElement* e) {
	updateStageTileCaller caller;
	caller.e = e;
	stl_list_map(e->mAnimationReferences, updateSingleStaticStageElementTileCB, &caller);	
	updateSingleStaticStageElementVisibilityFlag(e);
}

static void updateSingleStaticStageElementCB(void* tCaller, StaticStageHandlerElement& tData) {
	(void)tCaller;
	StaticStageHandlerElement* e = &tData;

	updateSingleStaticStageElement(e);
}

static void updateCamera() {
	Vector3D delta = vecSub(gMugenStageHandlerData.mCameraTargetPosition, gMugenStageHandlerData.mCameraPosition);
	delta.z = 0;
	if (vecLength(delta) < 1e-6) return;
	
	gMugenStageHandlerData.mCameraPosition = vecAdd(gMugenStageHandlerData.mCameraPosition, vecScale(delta, 0.95));
}

static void updateMugenStageHandler(void* tData) {
	(void)tData;

	updateCamera();

	stl_list_map(gMugenStageHandlerData.mStaticElements, updateSingleStaticStageElementCB);
}

ActorBlueprint getDreamMugenStageHandler() {
	return makeActorBlueprint(loadMugenStageHandler, unloadMugenStageHandler, updateMugenStageHandler);
}


void setDreamMugenStageHandlerCameraCoordinates(Vector3DI tCoordinates)
{
	gMugenStageHandlerData.mCameraCoordinates = tCoordinates;
}

void setDreamMugenStageHandlerCameraRange(GeoRectangle tRect)
{
	gMugenStageHandlerData.mCameraRange = tRect;
}

void setDreamMugenStageHandlerCameraPosition(Position p)
{
	gMugenStageHandlerData.mCameraTargetPosition = p;
}

void addDreamMugenStageHandlerCameraPositionX(double tX)
{
	gMugenStageHandlerData.mCameraTargetPosition.x += tX;
	gMugenStageHandlerData.mCameraTargetPosition = clampPositionToGeoRectangle(gMugenStageHandlerData.mCameraTargetPosition, gMugenStageHandlerData.mCameraRange);
	gMugenStageHandlerData.mCameraPosition.x = gMugenStageHandlerData.mCameraTargetPosition.x;

}

void setDreamMugenStageHandlerCameraPositionX(double tX)
{
	gMugenStageHandlerData.mCameraTargetPosition.x = tX;
	gMugenStageHandlerData.mCameraTargetPosition = clampPositionToGeoRectangle(gMugenStageHandlerData.mCameraTargetPosition, gMugenStageHandlerData.mCameraRange);
	gMugenStageHandlerData.mCameraPosition.x = gMugenStageHandlerData.mCameraTargetPosition.x;
}

void addDreamMugenStageHandlerCameraPositionY(double tY) {
	gMugenStageHandlerData.mCameraTargetPosition.y += tY;
	gMugenStageHandlerData.mCameraTargetPosition = clampPositionToGeoRectangle(gMugenStageHandlerData.mCameraTargetPosition, gMugenStageHandlerData.mCameraRange);
	gMugenStageHandlerData.mCameraPosition.y = gMugenStageHandlerData.mCameraTargetPosition.y;
}

void setDreamMugenStageHandlerCameraPositionY(double tY)
{
	gMugenStageHandlerData.mCameraTargetPosition.y = tY;
	gMugenStageHandlerData.mCameraTargetPosition = clampPositionToGeoRectangle(gMugenStageHandlerData.mCameraTargetPosition, gMugenStageHandlerData.mCameraRange);
	gMugenStageHandlerData.mCameraPosition.y = gMugenStageHandlerData.mCameraTargetPosition.y;
}

void resetDreamMugenStageHandlerCameraPosition()
{
	gMugenStageHandlerData.mCameraPosition.x = gMugenStageHandlerData.mCameraTargetPosition.x = 0; // TODO: properly
	gMugenStageHandlerData.mCameraPosition.y = gMugenStageHandlerData.mCameraTargetPosition.y = 0;
	
}

static void handleSingleTile(int tTile, int* tStart, int* tAmount, int tSize, int tSpacing, double tMinCam, double tMaxCam, double tDeltaScale, double tCoordinates) {
	if (!tTile) {
		*tStart = 0;
		*tAmount = 1;
	}
	else if (tTile == 1) {
		*tStart = (int)tMinCam - (tSize + tSpacing) - (int)(tCoordinates / 2);
		int length = (int)(((tMaxCam - (tMinCam - (tSize + tSpacing))) + tCoordinates)*tDeltaScale);
		*tAmount = length / (tSize + tSpacing) + 1; 
	}
	else {
		*tStart = 0;
		*tAmount = tTile;
	}
}

static void addSingleMugenStageHandlerBackgroundElementTile(StaticStageHandlerElement* e, MugenSpriteFile* tSprites, BlendType tBlendType, GeoRectangle tConstraintRectangle, Vector3D tOffset) {
	e->mAnimationReferences.push_back(StageElementAnimationReference());
	StageElementAnimationReference& newAnimation = e->mAnimationReferences.back();
	newAnimation.mID = addMugenAnimation(e->mAnimation, tSprites, makePosition(0, 0, 0));
	setMugenAnimationBasePosition(newAnimation.mID, &newAnimation.mReferencePosition);
	newAnimation.mOffset = tOffset;
	setMugenAnimationBlendType(newAnimation.mID, tBlendType);
	setMugenAnimationConstraintRectangle(newAnimation.mID, tConstraintRectangle);
	setMugenAnimationDrawScale(newAnimation.mID, makePosition(1, e->mStartScaleY, 1));
}

static void addMugenStageHandlerBackgroundElementTiles(StaticStageHandlerElement* e, MugenSpriteFile* tSprites, Vector3DI tTile, BlendType tBlendType, GeoRectangle tConstraintRectangle) {
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
	handleSingleTile(tTile.x, &startX, &amountX, size.x, e->mTileSpacing.x, gMugenStageHandlerData.mCameraRange.mTopLeft.x*cameraToElementScale, gMugenStageHandlerData.mCameraRange.mBottomRight.x*cameraToElementScale, deltaScaleX, e->mCoordinates.x);
	handleSingleTile(tTile.y, &startY, &amountY, size.y, e->mTileSpacing.y, gMugenStageHandlerData.mCameraRange.mTopLeft.y*cameraToElementScale, gMugenStageHandlerData.mCameraRange.mBottomRight.y*cameraToElementScale, deltaScaleY, e->mCoordinates.y);
	
	Vector3D offset = makePosition(startX, startY, 0);
	int j;
	for (j = 0; j < amountY; j++) {
		int i;
		offset.x = startX;
		for (i = 0; i < amountX; i++) {
			addSingleMugenStageHandlerBackgroundElementTile(e, tSprites, tBlendType, tConstraintRectangle, offset);
			offset.x += size.x + e->mTileSpacing.x;
			offset.z += 0.001;
		}
		offset.y += size.y + e->mTileSpacing.y;
	}

}


static void addStaticElementToIDList(StaticStageHandlerElement* e, int tID) {
	StageElementIDList* elementList;
	if (!stl_map_contains(gMugenStageHandlerData.mStageElementsFromID, tID)) {
		elementList = &gMugenStageHandlerData.mStageElementsFromID[tID];
		elementList->mVector.clear();
	}
	else {
		elementList = &gMugenStageHandlerData.mStageElementsFromID[tID];
	}

	elementList->mVector.push_back(e);
}

void addDreamMugenStageHandlerAnimatedBackgroundElement(Position tStart, MugenAnimation* tAnimation, int tOwnsAnimation, MugenSpriteFile * tSprites, Position tDelta, Vector3DI tTile, Vector3DI tTileSpacing, BlendType tBlendType, GeoRectangle tConstraintRectangle, Vector3D tVelocity, double tStartScaleY, double tScaleDeltaY, int tLayerNo, int tID, Vector3DI tCoordinates)
{
	gMugenStageHandlerData.mStaticElements.push_back(StaticStageHandlerElement());
	StaticStageHandlerElement* e = &gMugenStageHandlerData.mStaticElements.back();
	e->mStart = tStart;
	e->mDelta = tDelta;
	e->mCoordinates = tCoordinates;

	e->mVelocity = tVelocity;

	e->mSprites = tSprites;
	e->mAnimation = tAnimation;
	e->mOwnsAnimation = tOwnsAnimation;
	e->mAnimationReferences.clear();
	e->mTileSize = getAnimationFirstElementSpriteSize(e->mAnimation, tSprites);
	if (!e->mTileSize.x || !e->mTileSize.y) e->mTileSize = makeVector3DI(1, 1, 0);
	e->mTileSpacing = tTileSpacing;

	e->mStartScaleY = tStartScaleY;
	e->mScaleDeltaY = tScaleDeltaY;
	e->mLayerNo = tLayerNo;

	e->mIsEnabled = 1;
	e->mInvisibleFlag = 0;
	e->mIsInvisible = 0;

	addMugenStageHandlerBackgroundElementTiles(e, tSprites, tTile, tBlendType, tConstraintRectangle);
	updateSingleStaticStageElement(e);

	addStaticElementToIDList(e, tID);
}

Position * getDreamMugenStageHandlerCameraPositionReference()
{
	return &gMugenStageHandlerData.mCameraPosition;
}

typedef struct {
	int mIgnoredLayer;
} SetBackgroundFlagCaller;

static void setSingleStaticElementInvisibleForOneFrame(SetBackgroundFlagCaller* tCaller, StaticStageHandlerElement& tData) {
	StaticStageHandlerElement* e = &tData;

	if (e->mLayerNo == tCaller->mIgnoredLayer) return;

	e->mInvisibleFlag = 1;
}

void setDreamStageInvisibleForOneFrame()
{
	SetBackgroundFlagCaller caller;
	caller.mIgnoredLayer = -1;
	stl_list_map(gMugenStageHandlerData.mStaticElements, setSingleStaticElementInvisibleForOneFrame, &caller);
}

void setDreamStageLayer1InvisibleForOneFrame()
{
	SetBackgroundFlagCaller caller;
	caller.mIgnoredLayer = 0;
	stl_list_map(gMugenStageHandlerData.mStaticElements, setSingleStaticElementInvisibleForOneFrame, &caller);
}

void setStageElementInvisible(StaticStageHandlerElement* tElement, int tIsInvisible) {
	tElement->mIsInvisible = tIsInvisible;
}

static void pauseSingleMugenAnimationReference(void* tCaller, StageElementAnimationReference& tData) {
	(void)tCaller;
	StageElementAnimationReference* e = &tData;
	pauseMugenAnimation(e->mID);
}

static void unpauseSingleMugenAnimationReference(void* tCaller, StageElementAnimationReference& tData) {
	(void)tCaller;
	StageElementAnimationReference* e = &tData;
	unpauseMugenAnimation(e->mID);
}

void setStageElementEnabled(StaticStageHandlerElement* tElement, int tIsEnabled) {

	if (!tIsEnabled) {
		stl_list_map(tElement->mAnimationReferences, pauseSingleMugenAnimationReference);
	}
	else {
		stl_list_map(tElement->mAnimationReferences, unpauseSingleMugenAnimationReference);
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

static void changeSingleMugenAnimationReference(int* tCaller, StageElementAnimationReference& tData) {
	StageElementAnimationReference* e = &tData;
	changeMugenAnimation(e->mID, getMugenAnimation(getStageAnimations(), *tCaller));
}

void setStageElementAnimation(StaticStageHandlerElement * tElement, int tAnimation)
{
	stl_list_map(tElement->mAnimationReferences, changeSingleMugenAnimationReference, &tAnimation);
}

std::vector<StaticStageHandlerElement*>& getStageHandlerElementsWithID(int tID)
{
	StageElementIDList* elementList = &gMugenStageHandlerData.mStageElementsFromID[tID];
	return elementList->mVector;
}
