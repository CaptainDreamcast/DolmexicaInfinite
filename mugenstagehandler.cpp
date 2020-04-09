#include "mugenstagehandler.h"

#include <assert.h>

#include <prism/math.h>
#include <prism/mugenanimationhandler.h>
#include <prism/system.h>

#include "stage.h"

using namespace std;

typedef struct {
	vector<StaticStageHandlerElement*> mVector;
} StageElementIDList;

static struct {
	list<StaticStageHandlerElement> mStaticElements;

	Position mCameraPosition;
	Position mCameraPositionPreEffects;
	Position mCameraTargetPosition;

	GeoRectangle mCameraRange;
	Vector3DI mCameraCoordinates;

	Vector3D mCameraSpeed;

	Position mCameraEffectPosition;
	Position mCameraZoom;

	double mTimeDilatationNow;
	double mTimeDilatation;

	Position mCameraShakeOffset;

	map<int, StageElementIDList> mStageElementsFromID;
} gMugenStageHandlerData;

static void loadMugenStageHandler(void* tData) {
	(void)tData;
	gMugenStageHandlerData.mCameraShakeOffset = makePosition(0,0,0);
	gMugenStageHandlerData.mCameraPosition = makePosition(0,0,0);
	gMugenStageHandlerData.mCameraTargetPosition = makePosition(0, 0, 0);
	gMugenStageHandlerData.mCameraPositionPreEffects = makePosition(0, 0, 0);
	gMugenStageHandlerData.mCameraRange = makeGeoRectangle(-INF, -INF, 2*INF, 2*INF);
	gMugenStageHandlerData.mCameraSpeed = makePosition(5, 5, 0);

	gMugenStageHandlerData.mCameraEffectPosition = makePosition(0, 0, 0);
	gMugenStageHandlerData.mCameraZoom = makePosition(1.0, 1.0, 1.0);

	gMugenStageHandlerData.mTimeDilatationNow = 0.0;
	gMugenStageHandlerData.mTimeDilatation = 1.0;

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

int getTileAmountSingleAxis(int tSize, int tScreenSize, double tDelta) {
	const auto length = (int)((2 * tScreenSize + tSize) * (1 / tDelta));
	return length / tSize + 1;
}

Vector3DI getTileAmount(StaticStageHandlerElement* e) {
	const auto deltaX = !e->mDelta.x ? 1 : e->mDelta.x;
	const auto deltaY = !e->mDelta.y ? 1 : e->mDelta.y;
	const auto x = getTileAmountSingleAxis(e->mTileSize.x + e->mTileSpacing.x, e->mCoordinates.x, deltaX);
	const auto y = getTileAmountSingleAxis(e->mTileSize.y + e->mTileSpacing.y, e->mCoordinates.y, deltaY);
	return makeVector3DI(x, y, 0);
}

static void updateSingleStaticStageElementTileVelocityAndTiling(StaticStageHandlerElement* e, StageElementAnimationReference* tSingleAnimation) {
	if (e->mVelocity.x != 0 || e->mVelocity.y != 0) {
		tSingleAnimation->mOffset = vecAdd(tSingleAnimation->mOffset, e->mVelocity);
	}

	const auto amount = getTileAmount(e);
	const auto offset = getAnimationFirstElementSpriteOffset(e->mAnimation, e->mSprites);

	int totalSizeX, totalSizeY;
	const auto stepAmount = vector_size(&e->mAnimation->mSteps);
	if (stepAmount > 1) {
		totalSizeX = e->mTileSpacing.x;
		totalSizeY = e->mTileSpacing.y;
	}
	else {
		totalSizeX = e->mTileSize.x + e->mTileSpacing.x;
		totalSizeY = e->mTileSize.y + e->mTileSpacing.y;
	}

	const auto sz = getScreenSize();
	if (e->mTile.x == 1) {
		const auto right = tSingleAnimation->mReferencePosition.x + e->mTileSize.x - offset.x;
		if (right < 0) {
			tSingleAnimation->mOffset.x += amount.x*totalSizeX;
		}
		const auto left = tSingleAnimation->mReferencePosition.x - offset.x;
		if (left > sz.x) {
			tSingleAnimation->mOffset.x -= amount.x*totalSizeX;
		}
	}

	if (e->mTile.y == 1) {
		const auto down = tSingleAnimation->mReferencePosition.y + e->mTileSize.y - offset.y;
		if (down < 0) {
			tSingleAnimation->mOffset.y += amount.y*totalSizeY;
		}
		const auto up = tSingleAnimation->mReferencePosition.y - offset.y;
		if (up > sz.y) {
			tSingleAnimation->mOffset.y -= amount.y*totalSizeY;
		}
	}
}

static void updateSingleStaticStageElementTileReferencePosition(StaticStageHandlerElement* e, StageElementAnimationReference* tSingleAnimation) {
	Vector3D deltaInCameraSpace = makePosition(getDreamCameraPositionX(getCameraCoordP()), getDreamCameraPositionY(getCameraCoordP()), 0);
	deltaInCameraSpace = vecScale(deltaInCameraSpace, -1);
	deltaInCameraSpace = vecScale3D(deltaInCameraSpace, e->mDelta);
	deltaInCameraSpace.z = 0;

	Vector3D deltaInElementSpace = deltaInCameraSpace * e->mDrawScale;
	tSingleAnimation->mReferencePosition = vecAdd(e->mStart, deltaInElementSpace);
	tSingleAnimation->mReferencePosition = vecAdd(tSingleAnimation->mReferencePosition, tSingleAnimation->mOffset);
	tSingleAnimation->mReferencePosition = vecSub(tSingleAnimation->mReferencePosition, makePosition(-e->mCoordinates.x / 2, 0, 0));
	tSingleAnimation->mReferencePosition = vecScale3D(tSingleAnimation->mReferencePosition, e->mGlobalScale);
	tSingleAnimation->mReferencePosition.z++;

	const auto cameraStartPosition = getDreamCameraStartPosition(e->mCoordinates.y);
	const auto heightDelta = cameraStartPosition.y - getDreamCameraPositionY(e->mCoordinates.y);
	const auto heightScale = e->mStartScaleY + (e->mScaleDeltaY * heightDelta);
	setMugenAnimationDrawScale(tSingleAnimation->mElement, e->mDrawScale * e->mGlobalScale * makePosition(1, heightScale, 1));
}

static void updateSingleStaticStageElementTileVisibility(StaticStageHandlerElement* e, StageElementAnimationReference* tSingleAnimation) { 
		setMugenAnimationVisibility(tSingleAnimation->mElement, !(e->mInvisibleFlag || e->mIsInvisible || !e->mIsEnabled));
}

static void updateSingleStaticStageElementTile(StaticStageHandlerElement* e, StageElementAnimationReference* tSingleAnimation) {
	updateSingleStaticStageElementTileVisibility(e, tSingleAnimation);
	if (!e->mIsEnabled) return;

	updateSingleStaticStageElementTileVelocityAndTiling(e, tSingleAnimation);
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
	Vector3D delta = vecSub(gMugenStageHandlerData.mCameraTargetPosition, gMugenStageHandlerData.mCameraPositionPreEffects);
	delta.z = 0;

	gMugenStageHandlerData.mCameraPositionPreEffects = vecAdd(gMugenStageHandlerData.mCameraPositionPreEffects, vecScale(delta, 0.95));
	gMugenStageHandlerData.mCameraPosition = gMugenStageHandlerData.mCameraPositionPreEffects + gMugenStageHandlerData.mCameraShakeOffset;
}

static void updateMugenStageHandler(void* tData) {
	(void)tData;
	gMugenStageHandlerData.mTimeDilatationNow += gMugenStageHandlerData.mTimeDilatation;
	int updateAmount = (int)gMugenStageHandlerData.mTimeDilatationNow;
	gMugenStageHandlerData.mTimeDilatationNow -= updateAmount;
	while (updateAmount--) {
		updateCamera();

		stl_list_map(gMugenStageHandlerData.mStaticElements, updateSingleStaticStageElementCB);
	}
}

ActorBlueprint getDreamMugenStageHandler() {
	return makeActorBlueprint(loadMugenStageHandler, unloadMugenStageHandler, updateMugenStageHandler);
}

Vector3DI getDreamMugenStageHandlerCameraCoordinates()
{
	return gMugenStageHandlerData.mCameraCoordinates;
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
	gMugenStageHandlerData.mCameraPositionPreEffects.x = gMugenStageHandlerData.mCameraTargetPosition.x;

}

void setDreamMugenStageHandlerCameraPositionX(double tX)
{
	gMugenStageHandlerData.mCameraTargetPosition.x = tX;
	gMugenStageHandlerData.mCameraTargetPosition = clampPositionToGeoRectangle(gMugenStageHandlerData.mCameraTargetPosition, gMugenStageHandlerData.mCameraRange);
	gMugenStageHandlerData.mCameraPositionPreEffects.x = gMugenStageHandlerData.mCameraTargetPosition.x;
}

void addDreamMugenStageHandlerCameraPositionY(double tY) {
	gMugenStageHandlerData.mCameraTargetPosition.y += tY;
	gMugenStageHandlerData.mCameraTargetPosition = clampPositionToGeoRectangle(gMugenStageHandlerData.mCameraTargetPosition, gMugenStageHandlerData.mCameraRange);
	gMugenStageHandlerData.mCameraPositionPreEffects.y = gMugenStageHandlerData.mCameraTargetPosition.y;
}

void setDreamMugenStageHandlerCameraPositionY(double tY)
{
	gMugenStageHandlerData.mCameraTargetPosition.y = tY;
	gMugenStageHandlerData.mCameraTargetPosition = clampPositionToGeoRectangle(gMugenStageHandlerData.mCameraTargetPosition, gMugenStageHandlerData.mCameraRange);
	gMugenStageHandlerData.mCameraPositionPreEffects.y = gMugenStageHandlerData.mCameraTargetPosition.y;
}

void setDreamMugenStageHandlerScreenShake(const Position& tScreenShake)
{
	gMugenStageHandlerData.mCameraShakeOffset = tScreenShake;
}

void resetDreamMugenStageHandlerCameraPosition()
{
	const auto startPos = getDreamCameraStartPosition(getDreamStageCoordinateP());
	gMugenStageHandlerData.mCameraPosition.x = gMugenStageHandlerData.mCameraPositionPreEffects.x = gMugenStageHandlerData.mCameraTargetPosition.x = startPos.x;
	gMugenStageHandlerData.mCameraPosition.y = gMugenStageHandlerData.mCameraPositionPreEffects.y = gMugenStageHandlerData.mCameraTargetPosition.y = startPos.y;
}

static void handleSingleTile(int tTile, int* tStart, int* tAmount, int tSize, int tSpacing, int tCoordinates, double tDelta) {
	if (!tTile) {
		*tStart = 0;
		*tAmount = 1;
	}
	else if (tTile == 1) {		
		*tAmount = getTileAmountSingleAxis(tSize + tSpacing, tCoordinates, tDelta);
		*tStart = 0 - (*tAmount) / 2;
	}
	else {
		*tStart = 0;
		*tAmount = tTile;
	}
}

static void addSingleMugenStageHandlerBackgroundElementTile(StaticStageHandlerElement* e, MugenSpriteFile* tSprites, BlendType tBlendType, const Vector3D& tAlpha, GeoRectangle tConstraintRectangle, Vector3D tOffset) {
	e->mAnimationReferences.push_back(StageElementAnimationReference());
	StageElementAnimationReference& newAnimation = e->mAnimationReferences.back();
	newAnimation.mElement = addMugenAnimation(e->mAnimation, tSprites, makePosition(0, 0, 0));
	setMugenAnimationBasePosition(newAnimation.mElement, &newAnimation.mReferencePosition);
	newAnimation.mOffset = tOffset;
	setMugenAnimationBlendType(newAnimation.mElement, tBlendType);
	setMugenAnimationTransparency(newAnimation.mElement, tAlpha.x);
	setMugenAnimationDestinationTransparency(newAnimation.mElement, tAlpha.y);
	setMugenAnimationConstraintRectangle(newAnimation.mElement, tConstraintRectangle);
	setMugenAnimationDrawScale(newAnimation.mElement, e->mDrawScale * e->mGlobalScale * makePosition(1, e->mStartScaleY, 1));
	setMugenAnimationCameraEffectPositionReference(newAnimation.mElement, getDreamMugenStageHandlerCameraEffectPositionReference());
	setMugenAnimationCameraScaleReference(newAnimation.mElement, getDreamMugenStageHandlerCameraZoomReference());
}

static void addMugenStageHandlerBackgroundElementTiles(StaticStageHandlerElement* e, MugenSpriteFile* tSprites, Vector3DI tTile, BlendType tBlendType, const Vector3D& tAlpha, GeoRectangle tConstraintRectangle) {
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

	const auto deltaX = !e->mDelta.x ? 1 : e->mDelta.x;
	const auto deltaY = !e->mDelta.y ? 1 : e->mDelta.y;
	handleSingleTile(tTile.x, &startX, &amountX, size.x, e->mTileSpacing.x, e->mCoordinates.x, deltaX);
	handleSingleTile(tTile.y, &startY, &amountY, size.y, e->mTileSpacing.y, e->mCoordinates.y, deltaY);
	
	Vector3D offset = makePosition(startX, startY, 0);
	int j;
	for (j = 0; j < amountY; j++) {
		int i;
		offset.x = startX;
		for (i = 0; i < amountX; i++) {
			addSingleMugenStageHandlerBackgroundElementTile(e, tSprites, tBlendType, tAlpha, tConstraintRectangle, offset);
			offset.x += size.x + e->mTileSpacing.x;
			offset.z += 0.0001;
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

void addDreamMugenStageHandlerAnimatedBackgroundElement(Position tStart, MugenAnimation* tAnimation, int tOwnsAnimation, MugenSpriteFile * tSprites, Position tDelta, Vector3DI tTile, Vector3DI tTileSpacing, BlendType tBlendType, const Vector3D& tAlpha, GeoRectangle tConstraintRectangle, Vector3D tVelocity, double tStartScaleY, double tScaleDeltaY, Position tDrawScale, int tLayerNo, int tID, Vector3DI tCoordinates)
{
	gMugenStageHandlerData.mStaticElements.push_back(StaticStageHandlerElement());
	StaticStageHandlerElement* e = &gMugenStageHandlerData.mStaticElements.back();
	e->mStart = tStart;
	e->mDelta = tDelta;
	e->mCoordinates = tCoordinates;
	const auto sz = getScreenSize();
	const auto coordScale = sz.y / double(tCoordinates.y);
	e->mGlobalScale = makePosition(coordScale, coordScale, 1);
	e->mDrawScale = tDrawScale;

	e->mVelocity = tVelocity;

	e->mSprites = tSprites;
	e->mAnimation = tAnimation;
	e->mOwnsAnimation = tOwnsAnimation;
	e->mAnimationReferences.clear();
	e->mTile = tTile;
	e->mTileSize = getAnimationFirstElementSpriteSize(e->mAnimation, tSprites);
	if (!e->mTileSize.x || !e->mTileSize.y) e->mTileSize = makeVector3DI(1, 1, 0);
	e->mTileSpacing = tTileSpacing;

	e->mStartScaleY = tStartScaleY;
	e->mScaleDeltaY = tScaleDeltaY;
	e->mLayerNo = tLayerNo;

	e->mIsEnabled = 1;
	e->mInvisibleFlag = 0;
	e->mIsInvisible = 0;

	addMugenStageHandlerBackgroundElementTiles(e, tSprites, tTile, tBlendType, tAlpha, tConstraintRectangle);
	updateSingleStaticStageElement(e);

	addStaticElementToIDList(e, tID);
}

Position* getDreamMugenStageHandlerCameraPositionReference()
{
	return &gMugenStageHandlerData.mCameraPosition;
}

Position* getDreamMugenStageHandlerCameraEffectPositionReference()
{
	return &gMugenStageHandlerData.mCameraEffectPosition;
}

void setDreamMugenStageHandlerCameraEffectPositionX(double tX)
{
	gMugenStageHandlerData.mCameraEffectPosition.x = tX;
}

void setDreamMugenStageHandlerCameraEffectPositionY(double tY)
{
	gMugenStageHandlerData.mCameraEffectPosition.y = tY;
}

Position* getDreamMugenStageHandlerCameraTargetPositionReference()
{
	return &gMugenStageHandlerData.mCameraTargetPosition;
}

Position* getDreamMugenStageHandlerCameraZoomReference()
{
	return &gMugenStageHandlerData.mCameraZoom;
}

void setDreamMugenStageHandlerCameraZoom(double tZoom)
{
	gMugenStageHandlerData.mCameraZoom = makePosition(tZoom, tZoom, 1);
}

typedef struct {
	double mSpeed;
} SetMugenStageHandlerSpeedCaller;

static void setSingleStaticStageElementTileSpeedCB(SetMugenStageHandlerSpeedCaller* tCaller, StageElementAnimationReference& tData) {
	setMugenAnimationSpeed(tData.mElement, tCaller->mSpeed);
}

static void setSingleStaticStageElementSpeedCB(SetMugenStageHandlerSpeedCaller* tCaller, StaticStageHandlerElement& tData) {
	stl_list_map(tData.mAnimationReferences, setSingleStaticStageElementTileSpeedCB, tCaller);
}

void setDreamMugenStageHandlerSpeed(double tSpeed)
{
	SetMugenStageHandlerSpeedCaller caller;
	caller.mSpeed = tSpeed;
	stl_list_map(gMugenStageHandlerData.mStaticElements, setSingleStaticStageElementSpeedCB, &caller);

	gMugenStageHandlerData.mTimeDilatation = tSpeed;
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
	pauseMugenAnimation(e->mElement);
}

static void unpauseSingleMugenAnimationReference(void* tCaller, StageElementAnimationReference& tData) {
	(void)tCaller;
	StageElementAnimationReference* e = &tData;
	unpauseMugenAnimation(e->mElement);
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
	changeMugenAnimation(e->mElement, getMugenAnimation(getStageAnimations(), *tCaller));
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
