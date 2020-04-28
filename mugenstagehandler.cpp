#include "mugenstagehandler.h"

#include <assert.h>

#include <prism/math.h>
#include <prism/mugenanimationhandler.h>
#include <prism/system.h>

#include "stage.h"
#include "mugenanimationutilities.h"

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
	setProfilingSectionMarkerCurrentFunction();

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
	setProfilingSectionMarkerCurrentFunction();

	stl_list_map(gMugenStageHandlerData.mStaticElements, unloadSingleStaticElement);
	gMugenStageHandlerData.mStaticElements.clear();
	gMugenStageHandlerData.mStageElementsFromID.clear();
}

typedef struct{
	StaticStageHandlerElement* e;
} updateStageTileCaller;

static int getTileAmountSingleAxis(int tSize, int tScreenSize, double tDelta, double tInvertedMinimumWidthFactor) {
	const auto length = (int)(((2 * tScreenSize) + (tSize * tInvertedMinimumWidthFactor)) * (1 / tDelta));
	return length / tSize + 1;
}

Vector3DI getTileAmount(StaticStageHandlerElement* e) {
	const auto deltaX = !e->mDelta.x ? 1 : e->mDelta.x;
	const auto deltaY = !e->mDelta.y ? 1 : e->mDelta.y;
	const auto x = getTileAmountSingleAxis(e->mTileSize.x + e->mTileSpacing.x, e->mCoordinates.x, deltaX, e->mInvertedMinimumWidthFactor);
	const auto y = getTileAmountSingleAxis(e->mTileSize.y + e->mTileSpacing.y, e->mCoordinates.y, deltaY, 1.0);
	return makeVector3DI(x, y, 0);
}

static void updateSingleStaticStageElementTileTiling(StaticStageHandlerElement* e, StageElementAnimationReference* tSingleAnimation) {
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
		const auto right = tSingleAnimation->mReferencePosition.x + e->mTileSize.x - offset.x + std::max(0.0, getMugenAnimationShearLowerOffsetX(tSingleAnimation->mElement));
		if (right < 0) {
			tSingleAnimation->mOffset.x += amount.x*totalSizeX;
		}
		const auto left = tSingleAnimation->mReferencePosition.x - offset.x + std::min(0.0, getMugenAnimationShearLowerOffsetX(tSingleAnimation->mElement));
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

static void updateSingleStaticStageElementTilePositionAndScale(StaticStageHandlerElement* e, StageElementAnimationReference* tSingleAnimation) {
	tSingleAnimation->mReferencePosition = vecAdd(e->mTileBasePosition, tSingleAnimation->mOffset);
	tSingleAnimation->mReferencePosition = vecScale3D(tSingleAnimation->mReferencePosition, e->mGlobalScale);
	tSingleAnimation->mReferencePosition.z++;

	const auto cameraStartPosition = getDreamCameraStartPosition(e->mCoordinates.x);
	const auto heightDelta = cameraStartPosition.y - getDreamCameraPositionY(e->mCoordinates.x);
	const auto heightScale = e->mScaleStartY + (e->mScaleDeltaY * heightDelta);
	if (e->mIsParallax) {
		const auto deltaInCameraSpaceDrawScaled = (e->mTileBasePosition + makePosition(-e->mCoordinates.x / 2, 0, 0)) - e->mStart - e->mSinOffset - e->mSinOffsetInternal;
		const auto deltaInElementSpaceBottom = deltaInCameraSpaceDrawScaled * e->mXScale.y;
		auto bottomPosition = vecAdd(e->mStart + e->mSinOffset + e->mSinOffsetInternal, deltaInElementSpaceBottom);
		bottomPosition = vecSub(bottomPosition, makePosition(-e->mCoordinates.x / 2, 0, 0));
		bottomPosition = vecAdd(bottomPosition, tSingleAnimation->mOffset);
		bottomPosition = vecScale3D(bottomPosition, e->mGlobalScale);
		double shearOffsetToFitTiling;
		if (e->mAnimationReferences.size() > 1) {
			const auto centerOffset = (e->mCoordinates.x / 2) - tSingleAnimation->mReferencePosition.x;
			static const auto SHEAR_OFFSET_SCALE_EPSILON = 1.01;
			shearOffsetToFitTiling = centerOffset * e->mWidth.y * SHEAR_OFFSET_SCALE_EPSILON;
		}
		else {
			shearOffsetToFitTiling = 0;
		}
		setMugenAnimationShearX(tSingleAnimation->mElement, e->mWidth.y, bottomPosition.x - tSingleAnimation->mReferencePosition.x + shearOffsetToFitTiling);
	}
	setMugenAnimationDrawScale(tSingleAnimation->mElement, e->mDrawScale * e->mGlobalScale * makePosition(1, heightScale, 1) * e->mTileBaseScale);
}

static void updateSingleStaticStageElementTileConstraintRectangle(StaticStageHandlerElement* e, StageElementAnimationReference* tSingleAnimation) {
	if (e->mConstraintRectangleDelta.x == 0.0 && e->mConstraintRectangleDelta.y == 0.0) return;
	const auto windowDelta = (makePosition(getDreamCameraPositionX(getDreamMugenStageHandlerCameraCoordinateP()), getDreamCameraPositionY(getDreamMugenStageHandlerCameraCoordinateP()), 0) * -1.0) * e->mConstraintRectangleDelta;
	const auto movedConstraintRectangle = e->mConstraintRectangle + windowDelta;
	setMugenAnimationConstraintRectangle(tSingleAnimation->mElement, movedConstraintRectangle);
}

static void updateSingleStaticStageElementTileVisibility(StaticStageHandlerElement* e, StageElementAnimationReference* tSingleAnimation) { 
		setMugenAnimationVisibility(tSingleAnimation->mElement, !(e->mInvisibleFlag || e->mIsInvisible || !e->mIsEnabled));
}

static void updateSingleStaticStageElementTile(StaticStageHandlerElement* e, StageElementAnimationReference* tSingleAnimation) {
	updateSingleStaticStageElementTileVisibility(e, tSingleAnimation);
	if (!e->mIsEnabled) return;

	updateSingleStaticStageElementTilePositionAndScale(e, tSingleAnimation);
	updateSingleStaticStageElementTileTiling(e, tSingleAnimation);
	updateSingleStaticStageElementTileConstraintRectangle(e, tSingleAnimation);
}

static void updateSingleStaticStageElementTileCB(updateStageTileCaller* tCaller, StageElementAnimationReference& tData) {
	StageElementAnimationReference* singleAnimation = &tData;

	updateSingleStaticStageElementTile(tCaller->e, singleAnimation);
}

static void updateSingleStaticStageElementVisibilityFlag(StaticStageHandlerElement* e) {
	e->mInvisibleFlag = 0;
}

static void updateSingleStaticStageElementSingleSin(StaticStageHandlerElement* e, double& oTarget, const Vector3D& tSin) {
	if (!tSin.x) return;
	oTarget = calculateStageElementSinOffset(e->mSinTime, tSin.x, tSin.y, tSin.z);
}

static void updateSingleStaticStageElementSin(StaticStageHandlerElement* e) {
	if (!e->mIsEnabled) return;
	updateSingleStaticStageElementSingleSin(e, e->mSinOffsetInternal.x, e->mSinX);
	updateSingleStaticStageElementSingleSin(e, e->mSinOffsetInternal.y, e->mSinY);
	e->mSinTime++;
}

static void updateSingleStaticStageElementVelocity(StaticStageHandlerElement* e) {
	if (!e->mIsEnabled) return;

	if (e->mVelocity.x != 0 || e->mVelocity.y != 0) {
		e->mStart = vecAdd(e->mStart, e->mVelocity);
	}
}

static void updateSingleStaticStageElementBasePosition(StaticStageHandlerElement* e) {
	if (!e->mIsEnabled) return;

	const auto cameraOffset = makePosition(getDreamCameraPositionX(getDreamMugenStageHandlerCameraCoordinateP()), getDreamCameraPositionY(getDreamMugenStageHandlerCameraCoordinateP()), 0) * -1.0;
	Position positionDelta;
	if (e->mPositionLinkElement) {
		positionDelta = e->mPositionLinkElement->mTileBasePosition + makePosition(-e->mCoordinates.x / 2, 0, 0);
	}
	else {
		positionDelta = cameraOffset * e->mDelta;
	}
	const auto positionDeltaInElementSpace = positionDelta * e->mDrawScale;
	e->mTileBasePosition = (e->mStart + e->mSinOffset + e->mSinOffsetInternal + positionDeltaInElementSpace) - makePosition(-e->mCoordinates.x / 2, 0, 0);

	const auto scaleDelta = cameraOffset * e->mScaleDelta;
	e->mTileBaseScale = e->mScaleStart + scaleDelta;
}

static void updateSingleStaticStageElement(StaticStageHandlerElement* e) {
	updateSingleStaticStageElementVelocity(e);
	updateSingleStaticStageElementBasePosition(e);
	
	updateStageTileCaller caller;
	caller.e = e;
	stl_list_map(e->mAnimationReferences, updateSingleStaticStageElementTileCB, &caller);	
	updateSingleStaticStageElementVisibilityFlag(e);
	updateSingleStaticStageElementSin(e);
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
	setProfilingSectionMarkerCurrentFunction();

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

int getDreamMugenStageHandlerCameraCoordinateP()
{
	return gMugenStageHandlerData.mCameraCoordinates.x;
}

void setDreamMugenStageHandlerCameraCoordinates(const Vector3DI& tCoordinates)
{
	gMugenStageHandlerData.mCameraCoordinates = tCoordinates;
}

void setDreamMugenStageHandlerCameraRange(const GeoRectangle& tRect)
{
	gMugenStageHandlerData.mCameraRange = tRect;
}

void setDreamMugenStageHandlerCameraPosition(const Position& p)
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

void resetDreamMugenStageHandler()
{
	for (auto& staticElement : gMugenStageHandlerData.mStaticElements)
	{
		staticElement.mStart = staticElement.mStartResetValue;
		staticElement.mVelocity = staticElement.mVelocityResetValue;
		staticElement.mSinOffset = makePosition(0, 0, 0);
		staticElement.mSinOffsetInternal = makePosition(0, 0, 0);
		staticElement.mIsInvisible = 0;
		staticElement.mIsEnabled = 1;
		for (auto& animationElement : staticElement.mAnimationReferences)
		{
			animationElement.mOffset = animationElement.mStartPosition;
			removeMugenAnimationPaletteEffectIfExists(animationElement.mElement);
			unpauseMugenAnimation(animationElement.mElement);
			resetMugenAnimation(animationElement.mElement);
		}
		updateSingleStaticStageElement(&staticElement);
	}
}

static void handleSingleTile(int tTile, int* tStart, int* tAmount, int tSize, int tSpacing, int tCoordinates, double tDelta, double tInvertedMinimumWidthFactor) {
	if (!tTile) {
		*tStart = 0;
		*tAmount = 1;
	}
	else if (tTile == 1) {		
		*tAmount = getTileAmountSingleAxis(tSize + tSpacing, tCoordinates, tDelta, tInvertedMinimumWidthFactor);
		*tStart = 0 - (*tAmount) / 2;
	}
	else {
		*tStart = 0;
		*tAmount = tTile;
	}
}

static void addSingleMugenStageHandlerBackgroundElementTile(StaticStageHandlerElement* e, MugenSpriteFile* tSprites, BlendType tBlendType, const Vector3D& tAlpha, double tZoomDelta, const Vector3D& tOffset) {
	e->mAnimationReferences.push_back(StageElementAnimationReference());
	StageElementAnimationReference& newAnimation = e->mAnimationReferences.back();
	newAnimation.mElement = addMugenAnimation(e->mAnimation, tSprites, makePosition(0, 0, 0));
	setMugenAnimationBasePosition(newAnimation.mElement, &newAnimation.mReferencePosition);
	newAnimation.mOffset = newAnimation.mStartPosition = tOffset;
	setMugenAnimationBlendType(newAnimation.mElement, tBlendType);
	setMugenAnimationTransparency(newAnimation.mElement, tAlpha.x);
	setMugenAnimationDestinationTransparency(newAnimation.mElement, tAlpha.y);
	setMugenAnimationConstraintRectangle(newAnimation.mElement, e->mConstraintRectangle);
	setMugenAnimationDrawScale(newAnimation.mElement, e->mDrawScale * e->mGlobalScale * makePosition(1, e->mScaleStartY, 1));
	setMugenAnimationCameraEffectPositionReference(newAnimation.mElement, getDreamMugenStageHandlerCameraEffectPositionReference());
	setMugenAnimationCameraScaleReference(newAnimation.mElement, getDreamMugenStageHandlerCameraZoomReference());
	setMugenAnimationCameraScaleFactor(newAnimation.mElement, tZoomDelta);
}

static void addMugenStageHandlerBackgroundElementTiles(StaticStageHandlerElement* e, MugenSpriteFile* tSprites, const Vector3DI& tTile, BlendType tBlendType, const Vector3D& tAlpha, double tZoomDelta) {
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
	handleSingleTile(tTile.x, &startX, &amountX, size.x, e->mTileSpacing.x, e->mCoordinates.x, deltaX, e->mInvertedMinimumWidthFactor);
	handleSingleTile(tTile.y, &startY, &amountY, size.y, e->mTileSpacing.y, e->mCoordinates.y, deltaY, 1.0);
	
	Vector3D offset = makePosition(startX, startY, 0);
	int j;
	for (j = 0; j < amountY; j++) {
		int i;
		offset.x = startX;
		for (i = 0; i < amountX; i++) {
			addSingleMugenStageHandlerBackgroundElementTile(e, tSprites, tBlendType, tAlpha, tZoomDelta, offset);
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

void addDreamMugenStageHandlerAnimatedBackgroundElement(const Position& tStart, MugenAnimation* tAnimation, int tOwnsAnimation, MugenSpriteFile * tSprites, const Position& tDelta, const Vector3DI& tTile, const Vector3DI& tTileSpacing, BlendType tBlendType, const Vector3D& tAlpha, const GeoRectangle& tConstraintRectangle, const Position& tConstraintRectangleDelta, const Vector3D& tVelocity, const Vector3D& tSinX, const Vector3D& tSinY, double tScaleStartY, double tScaleDeltaY, const Position& tScaleStart, const Position& tScaleDelta, const Position& tDrawScale, int tLayerNo, int tID, int tIsParallax, const Vector3DI& tWidth, const Vector3D& tXScale, double tZoomDelta, int tPositionLink, const Vector3DI& tCoordinates)
{
	gMugenStageHandlerData.mStaticElements.push_back(StaticStageHandlerElement());
	StaticStageHandlerElement* e = &gMugenStageHandlerData.mStaticElements.back();
	e->mStart = e->mStartResetValue = tStart;
	e->mSinOffset = makePosition(0, 0, 0);
	e->mSinOffsetInternal = makePosition(0, 0, 0);
	e->mDelta = tDelta;
	e->mCoordinates = tCoordinates;
	const auto sz = getScreenSize();
	const auto coordScale = sz.x / double(tCoordinates.x);
	e->mGlobalScale = makePosition(coordScale, coordScale, 1);
	e->mDrawScale = tDrawScale;

	e->mVelocity = e->mVelocityResetValue = tVelocity;

	e->mSprites = tSprites;
	e->mAnimation = tAnimation;
	e->mOwnsAnimation = tOwnsAnimation;
	e->mAnimationReferences.clear();
	e->mTile = tTile;
	const auto spriteSize = e->mAnimation ? getAnimationFirstElementSpriteSize(e->mAnimation, tSprites) : makeVector3DI(1, 1, 0);
	e->mTileSize = spriteSize;
	if (!e->mTileSize.x || !e->mTileSize.y) e->mTileSize = makeVector3DI(1, 1, 0);
	e->mTileSpacing = tTileSpacing;

	e->mScaleStartY = tScaleStartY;
	e->mScaleDeltaY = tScaleDeltaY;
	e->mScaleStart = tScaleStart;
	e->mScaleDelta = tScaleDelta;
	e->mLayerNo = tLayerNo;

	e->mIsEnabled = 1;
	e->mInvisibleFlag = 0;
	e->mIsInvisible = 0;

	e->mIsParallax = tIsParallax;
	e->mXScale = tXScale;
	if (e->mIsParallax) {
		e->mWidth = makePosition(spriteSize.x ? (tWidth.x / double(spriteSize.x)) : 1.0, spriteSize.y ? (tWidth.y / double(spriteSize.y)) : 1.0, 1);
		e->mDrawScale.x *= e->mWidth.x;
		e->mDelta.x *= e->mXScale.x;
		e->mXScale.y /= e->mXScale.x;
		e->mWidth.y /= e->mWidth.x;
		e->mWidth.x = 1;
	}
	else {
		e->mWidth = makePosition(1.0, 1.0, 1.0);
	}
	e->mInvertedMinimumWidthFactor = 1.0 / std::min(e->mWidth.x, e->mWidth.y);

	e->mConstraintRectangle = tConstraintRectangle;
	e->mConstraintRectangleDelta = tConstraintRectangleDelta;
	e->mSinTime = 0;
	e->mSinX = tSinX;
	e->mSinY = tSinY;

	e->mTileBasePosition = makePosition(0, 0, 0);
	e->mTileBaseScale = makePosition(1.0, 1.0, 1.0);
	if (!tPositionLink || gMugenStageHandlerData.mStaticElements.size() <= 1) {
		e->mPositionLinkElement = NULL;
	} else {
		auto it = gMugenStageHandlerData.mStaticElements.end();
		it--; it--;
		e->mPositionLinkElement = &(*it);
	}

	if (e->mAnimation) {
		addMugenStageHandlerBackgroundElementTiles(e, tSprites, tTile, tBlendType, tAlpha, tZoomDelta);
	}
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

typedef struct {
	int mDuration;
	Vector3D mAddition;
	Vector3D mMultiplier;
	Vector3D mSineAmplitude;
	int mSinePeriod;
	int mInvertAll;
	double mColorFactor;
} SetBackgroundPaletteEffectCaller;

static void setSingleStaticStageElementTilePaletteEffectCB(SetBackgroundPaletteEffectCaller* tCaller, StageElementAnimationReference& tData) {
	setMugenAnimationPaletteEffectForDuration(tData.mElement, tCaller->mDuration, tCaller->mAddition, tCaller->mMultiplier, tCaller->mSineAmplitude, tCaller->mSinePeriod, tCaller->mInvertAll, tCaller->mColorFactor);
}

static void setSingleStaticElementPaletteEffect(SetBackgroundPaletteEffectCaller* tCaller, StaticStageHandlerElement& tData) {
	stl_list_map(tData.mAnimationReferences, setSingleStaticStageElementTilePaletteEffectCB, tCaller);
}

void setDreamStagePaletteEffects(int tDuration, const Vector3D& tAddition, const Vector3D& tMultiplier, const Vector3D& tSineAmplitude, int tSinePeriod, int tInvertAll, double tColorFactor)
{
	SetBackgroundPaletteEffectCaller caller;
	caller.mDuration = tDuration;
	caller.mAddition = tAddition;
	caller.mMultiplier = tMultiplier;
	caller.mSineAmplitude = tSineAmplitude;
	caller.mSinePeriod = tSinePeriod;
	caller.mInvertAll = tInvertAll;
	caller.mColorFactor = tColorFactor;
	stl_list_map(gMugenStageHandlerData.mStaticElements, setSingleStaticElementPaletteEffect, &caller);
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

void setStageElementSinOffsetX(StaticStageHandlerElement * tElement, double tOffsetX)
{
	tElement->mSinOffset.x = tOffsetX;
}

void setStageElementSinOffsetY(StaticStageHandlerElement * tElement, double tOffsetY)
{
	tElement->mSinOffset.y = tOffsetY;
}

static void changeSingleMugenAnimationReference(int* tCaller, StageElementAnimationReference& tData) {
	StageElementAnimationReference* e = &tData;
	changeMugenAnimation(e->mElement, getMugenAnimation(getStageAnimations(), *tCaller));
}

void setStageElementAnimation(StaticStageHandlerElement * tElement, int tAnimation)
{
	stl_list_map(tElement->mAnimationReferences, changeSingleMugenAnimationReference, &tAnimation);
}

double calculateStageElementSinOffset(int tTick, double tAmplitude, double tPeriod, double tPhase) {
	tPeriod = max(1.0, tPeriod);
	return tAmplitude * std::sin((((tTick + int(tPhase)) % int(tPeriod)) / tPeriod) * M_PI * 2);
}

std::vector<StaticStageHandlerElement*>& getStageHandlerElementsWithID(int tID)
{
	StageElementIDList* elementList = &gMugenStageHandlerData.mStageElementsFromID[tID];
	return elementList->mVector;
}
