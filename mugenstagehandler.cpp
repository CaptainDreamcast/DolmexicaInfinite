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
	Position2D mCameraPositionPreEffects;
	Position2D mCameraTargetPosition;

	GeoRectangle2D mCameraRange;
	Vector2DI mCameraCoordinates;

	Vector2D mCameraSpeed;

	Position2D mCameraEffectPosition;
	Position mCameraZoom;

	double mTimeDilatationNow;
	double mTimeDilatation;

	Position2D mCameraShakeOffset;

	map<int, StageElementIDList> mStageElementsFromID;
} gMugenStageHandlerData;

static void loadMugenStageHandler(void* tData) {
	(void)tData;
	setProfilingSectionMarkerCurrentFunction();

	gMugenStageHandlerData.mCameraShakeOffset = Vector2D(0, 0);
	gMugenStageHandlerData.mCameraPosition = Vector3D(0, 0, 0);
	gMugenStageHandlerData.mCameraTargetPosition = Vector2D(0, 0);
	gMugenStageHandlerData.mCameraPositionPreEffects = Vector2D(0, 0);
	gMugenStageHandlerData.mCameraRange = GeoRectangle2D(-INF, -INF, 2*INF, 2*INF);
	gMugenStageHandlerData.mCameraSpeed = Vector2D(5, 5);

	gMugenStageHandlerData.mCameraEffectPosition = Vector2D(0, 0);
	gMugenStageHandlerData.mCameraZoom = Vector3D(1.0, 1.0, 1.0);

	gMugenStageHandlerData.mTimeDilatationNow = 0.0;
	gMugenStageHandlerData.mTimeDilatation = 1.0;

	gMugenStageHandlerData.mStaticElements.clear();
	gMugenStageHandlerData.mStageElementsFromID.clear();
}

static void unloadSingleStaticElement(void* tCaller, StaticStageHandlerElement& tData) {
	(void)tCaller;
	StaticStageHandlerElement* e = &tData;

	for (auto& animationElement : e->mAnimationReferences)
	{
		removeMugenAnimation(animationElement.mElement);
	}
	if (e->mOwnsAnimation) {
		destroyMugenAnimation(e->mAnimation);
	}

	e->mAnimationReferences.clear();
}

static void unloadMugenStageHandler(void* tData = NULL) {
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
	return int(length / (tSize * (1.0 / tInvertedMinimumWidthFactor)) + 1);
}

static Vector2DI getTileAmount(StaticStageHandlerElement* e) {
	const auto deltaX = !e->mDelta.x ? 1 : e->mDelta.x;
	const auto deltaY = !e->mDelta.y ? 1 : e->mDelta.y;
	const auto x = getTileAmountSingleAxis(e->mTileSize.x + e->mTileSpacing.x, e->mCoordinates.x, deltaX, e->mInvertedMinimumWidthFactor);
	const auto y = getTileAmountSingleAxis(e->mTileSize.y + e->mTileSpacing.y, e->mCoordinates.y, deltaY, 1.0);
	return Vector2DI(x, y);
}

static void updateSingleStaticStageElementTileTiling(StaticStageHandlerElement* e, StageElementAnimationReference* tSingleAnimation, const Vector2D& tTotalScale) {
	const auto amount = getTileAmount(e);
	auto offset = getAnimationFirstElementSpriteOffset(e->mAnimation, e->mSprites);
	if (e->mIsParallax) {
		offset.x = getAnimationFirstElementSpriteSize(e->mAnimation, e->mSprites).x / 2.0;
	}

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
		const auto right = tSingleAnimation->mReferencePosition.x * (1.0 / tTotalScale.x) + e->mTileSize.x - offset.x + std::max(0.0, getMugenAnimationShearLowerOffsetX(tSingleAnimation->mElement));
		if (right < 0) {
			tSingleAnimation->mOffset.x += amount.x*totalSizeX;
		}
		const auto left = tSingleAnimation->mReferencePosition.x * (1.0 / tTotalScale.x) - offset.x + std::min(0.0, getMugenAnimationShearLowerOffsetX(tSingleAnimation->mElement));
		if (left > sz.x) {
			tSingleAnimation->mOffset.x -= amount.x*totalSizeX;
		}
	}

	if (e->mTile.y == 1) {
		const auto down = tSingleAnimation->mReferencePosition.y * (1.0 / tTotalScale.y) + e->mTileSize.y - offset.y;
		if (down < 0) {
			tSingleAnimation->mOffset.y += amount.y*totalSizeY;
		}
		const auto up = tSingleAnimation->mReferencePosition.y * (1.0 / tTotalScale.y) - offset.y;
		if (up > sz.y) {
			tSingleAnimation->mOffset.y -= amount.y*totalSizeY;
		}
	}
}

static void updateSingleStaticStageElementTilePositionAndScale(StaticStageHandlerElement* e, StageElementAnimationReference* tSingleAnimation, Vector2D& oTotalScale) {
	tSingleAnimation->mReferencePosition = vecAdd(e->mTileBasePosition, tSingleAnimation->mOffset);
	tSingleAnimation->mReferencePosition = vecScale2D(tSingleAnimation->mReferencePosition, e->mGlobalScale * e->mParallaxScale);
	tSingleAnimation->mReferencePosition.z++;

	const auto cameraStartPosition = getDreamCameraStartPosition(e->mCoordinates.x);
	const auto heightDelta = cameraStartPosition.y - getDreamCameraPositionY(e->mCoordinates.x);
	const auto heightScale = e->mScaleStartY + (e->mScaleDeltaY * heightDelta);
	oTotalScale = e->mDrawScale * e->mGlobalScale * Vector2D(1, heightScale) * e->mTileBaseScale * e->mParallaxScale;
	setMugenAnimationDrawScale(tSingleAnimation->mElement, oTotalScale);

	if (e->mIsParallax) {
		const auto deltaInCameraSpaceDrawScaled = (e->mTileBasePosition + Vector2D(-e->mCoordinates.x / 2, 0)) - e->mStart - e->mSinOffset - e->mSinOffsetInternal;
		const auto deltaInElementSpaceBottom = deltaInCameraSpaceDrawScaled * e->mXScale.y;
		auto bottomPosition = vecAdd(e->mStart + e->mSinOffset + e->mSinOffsetInternal, deltaInElementSpaceBottom);
		bottomPosition.x -= (-e->mCoordinates.x / 2);
		bottomPosition = vecAdd(bottomPosition, tSingleAnimation->mOffset);
		bottomPosition = vecScale2D(bottomPosition, e->mGlobalScale * e->mParallaxScale);
		double shearOffsetToFitTiling;
		if (e->mAnimationReferences.size() > 1) {
			const auto centerOffset = (e->mCoordinates.x / 2) - tSingleAnimation->mReferencePosition.x * (1.0 / oTotalScale.x);
			shearOffsetToFitTiling = centerOffset * (1.0 - e->mWidth.y);
		}
		else {
			shearOffsetToFitTiling = 0;
		}
		setMugenAnimationShearX(tSingleAnimation->mElement, e->mWidth.y, bottomPosition.x - tSingleAnimation->mReferencePosition.x + shearOffsetToFitTiling);
	}
}

static void updateSingleStaticStageElementTileConstraintRectangle(StaticStageHandlerElement* e, StageElementAnimationReference* tSingleAnimation) {
	if (e->mConstraintRectangleDelta.x == 0.0 && e->mConstraintRectangleDelta.y == 0.0) return;
	const auto windowDelta = (Position2D(getDreamCameraPositionX(getDreamMugenStageHandlerCameraCoordinateP()), getDreamCameraPositionY(getDreamMugenStageHandlerCameraCoordinateP())) * -1.0) * e->mConstraintRectangleDelta;
	const auto movedConstraintRectangle = e->mConstraintRectangle + windowDelta;
	setMugenAnimationConstraintRectangle(tSingleAnimation->mElement, movedConstraintRectangle);
}

static void updateSingleStaticStageElementTileVisibility(StaticStageHandlerElement* e, StageElementAnimationReference* tSingleAnimation) { 
		setMugenAnimationVisibility(tSingleAnimation->mElement, !(e->mInvisibleFlag || e->mIsInvisible || !e->mIsEnabled));
}

static void updateSingleStaticStageElementTile(StaticStageHandlerElement* e, StageElementAnimationReference* tSingleAnimation) {
	updateSingleStaticStageElementTileVisibility(e, tSingleAnimation);
	if (!e->mIsEnabled) return;

	Vector2D totalScale;
	updateSingleStaticStageElementTilePositionAndScale(e, tSingleAnimation, totalScale);
	updateSingleStaticStageElementTileTiling(e, tSingleAnimation, totalScale);
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
		e->mStart = e->mStart + e->mVelocity;
	}
}

static void updateSingleStaticStageElementBasePosition(StaticStageHandlerElement* e) {
	if (!e->mIsEnabled) return;

	const auto cameraOffset = Vector2D(getDreamCameraPositionX(getDreamMugenStageHandlerCameraCoordinateP()), getDreamCameraPositionY(getDreamMugenStageHandlerCameraCoordinateP())) * -1.0;
	Position2D positionDelta;
	if (e->mPositionLinkElement) {
		positionDelta = e->mPositionLinkElement->mTileBasePosition.xy() + Vector2D(-e->mCoordinates.x / 2, 0);
	}
	else {
		positionDelta = cameraOffset * e->mDelta;
	}
	const auto positionDeltaInElementSpace = positionDelta * e->mDrawScale * e->mParallaxScale;
	e->mTileBasePosition = (e->mStart + e->mSinOffset + e->mSinOffsetInternal + positionDeltaInElementSpace) - Vector2D(-e->mCoordinates.x / 2, 0);

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
	const auto delta = gMugenStageHandlerData.mCameraTargetPosition - gMugenStageHandlerData.mCameraPositionPreEffects;

	gMugenStageHandlerData.mCameraPositionPreEffects = gMugenStageHandlerData.mCameraPositionPreEffects + (delta * 0.95);
	gMugenStageHandlerData.mCameraPosition = (gMugenStageHandlerData.mCameraPositionPreEffects + gMugenStageHandlerData.mCameraShakeOffset).xyz(0.0);
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

Vector2DI getDreamMugenStageHandlerCameraCoordinates()
{
	return gMugenStageHandlerData.mCameraCoordinates;
}

int getDreamMugenStageHandlerCameraCoordinateP()
{
	return gMugenStageHandlerData.mCameraCoordinates.x;
}

void setDreamMugenStageHandlerCameraCoordinates(const Vector2DI& tCoordinates)
{
	gMugenStageHandlerData.mCameraCoordinates = tCoordinates;
}

void setDreamMugenStageHandlerCameraRange(const GeoRectangle2D& tRect)
{
	gMugenStageHandlerData.mCameraRange = tRect;
}

void setDreamMugenStageHandlerCameraPosition(const Position2D& p)
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

void setDreamMugenStageHandlerScreenShake(const Position2D& tScreenShake)
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
		staticElement.mSinOffset = Vector2D(0, 0);
		staticElement.mSinOffsetInternal = Vector2D(0, 0);
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

void clearDreamMugenStageHandler() {
	unloadMugenStageHandler();
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

static void addSingleMugenStageHandlerBackgroundElementTile(StaticStageHandlerElement* e, MugenSpriteFile* tSprites, BlendType tBlendType, const Vector2D& tAlpha, double tZoomDelta, const Vector3D& tOffset) {
	e->mAnimationReferences.push_back(StageElementAnimationReference());
	StageElementAnimationReference& newAnimation = e->mAnimationReferences.back();
	newAnimation.mElement = addMugenAnimation(e->mAnimation, tSprites, Vector3D(0, 0, 0));
	setMugenAnimationBasePosition(newAnimation.mElement, &newAnimation.mReferencePosition);
	newAnimation.mOffset = newAnimation.mStartPosition = tOffset;
	setMugenAnimationBlendType(newAnimation.mElement, tBlendType);
	setMugenAnimationTransparency(newAnimation.mElement, tAlpha.x);
	setMugenAnimationDestinationTransparency(newAnimation.mElement, tAlpha.y);
	setMugenAnimationConstraintRectangle(newAnimation.mElement, e->mConstraintRectangle);
	setMugenAnimationDrawScale(newAnimation.mElement, e->mDrawScale * e->mGlobalScale * Vector2D(1, e->mScaleStartY) * e->mParallaxScale);
	setMugenAnimationIsSpriteOffsetForcedToCenter(newAnimation.mElement, e->mIsParallax);
	setMugenAnimationCameraEffectPositionReference(newAnimation.mElement, getDreamMugenStageHandlerCameraEffectPositionReference());
	setMugenAnimationCameraScaleReference(newAnimation.mElement, getDreamMugenStageHandlerCameraZoomReference());
	setMugenAnimationCameraScaleFactor(newAnimation.mElement, tZoomDelta);
}

static void addMugenStageHandlerBackgroundElementTiles(StaticStageHandlerElement* e, MugenSpriteFile* tSprites, const Vector2DI& tTile, BlendType tBlendType, const Vector2D& tAlpha, double tZoomDelta) {
	int startX;
	int startY;
	int amountX;
	int amountY;
	int stepAmount = vector_size(&e->mAnimation->mSteps);
	Vector2DI size;
	if (stepAmount > 1) {
		size = Vector2DI(0, 0);
	}
	else {
		size = getAnimationFirstElementSpriteSize(e->mAnimation, tSprites);
	}

	const auto deltaX = !e->mDelta.x ? 1 : e->mDelta.x;
	const auto deltaY = !e->mDelta.y ? 1 : e->mDelta.y;
	handleSingleTile(tTile.x, &startX, &amountX, size.x, e->mTileSpacing.x, e->mCoordinates.x, deltaX, e->mInvertedMinimumWidthFactor);
	handleSingleTile(tTile.y, &startY, &amountY, size.y, e->mTileSpacing.y, e->mCoordinates.y, deltaY, 1.0);
	
	Vector3D offset = Vector3D(startX, startY, 0);
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

void addDreamMugenStageHandlerAnimatedBackgroundElement(const Position& tStart, MugenAnimation* tAnimation, int tOwnsAnimation, MugenSpriteFile* tSprites, const Position2D& tDelta, const Vector2DI& tTile, const Vector2DI& tTileSpacing, BlendType tBlendType, const Vector2D& tAlpha, const GeoRectangle2D& tConstraintRectangle, const Vector2D& tConstraintRectangleDelta, const Vector2D& tVelocity, const Vector3D& tSinX, const Vector3D& tSinY, double tScaleStartY, double tScaleDeltaY, const Vector2D& tScaleStart, const Vector2D& tScaleDelta, const Vector2D& tDrawScale, int tLayerNo, int tID, int tIsParallax, const Vector2DI& tWidth, const Vector2D& tXScale, double tZoomDelta, int tPositionLink, const Vector2DI& tCoordinates)
{
	gMugenStageHandlerData.mStaticElements.push_back(StaticStageHandlerElement());
	StaticStageHandlerElement* e = &gMugenStageHandlerData.mStaticElements.back();
	e->mStart = e->mStartResetValue = tStart;
	e->mSinOffset = Vector2D(0, 0);
	e->mSinOffsetInternal = Vector2D(0, 0);
	e->mDelta = tDelta;
	e->mCoordinates = tCoordinates;
	const auto sz = getScreenSize();
	const auto coordScale = sz.x / double(tCoordinates.x);
	e->mGlobalScale = Vector2D(coordScale, coordScale);
	e->mDrawScale = tDrawScale;

	e->mVelocity = e->mVelocityResetValue = tVelocity;

	e->mSprites = tSprites;
	e->mAnimation = tAnimation;
	e->mOwnsAnimation = tOwnsAnimation;
	e->mAnimationReferences.clear();
	e->mTile = tTile;
	const auto spriteSize = e->mAnimation ? getAnimationFirstElementSpriteSize(e->mAnimation, tSprites) : Vector2DI(1, 1);
	e->mTileSize = spriteSize;
	if (!e->mTileSize.x || !e->mTileSize.y) e->mTileSize = Vector2DI(1, 1);
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
		e->mWidth = Vector2D(spriteSize.x ? (tWidth.x / double(spriteSize.x)) : 1.0, spriteSize.y ? (tWidth.y / double(spriteSize.y)) : 1.0);
		e->mParallaxScale = Vector2D(e->mWidth.x, 1.0);
		e->mXScale.y = e->mXScale.y / e->mXScale.x;
		e->mWidth.y = e->mWidth.y / e->mWidth.x;
		e->mWidth.x = 1;
	}
	else {
		e->mWidth = Vector2D(1.0, 1.0);
		e->mParallaxScale = Vector2D(1.0, 1.0);
	}
	e->mInvertedMinimumWidthFactor = (1.0 / std::min(e->mParallaxScale.x, e->mWidth.y));

	e->mConstraintRectangle = tConstraintRectangle;
	e->mConstraintRectangleDelta = tConstraintRectangleDelta;
	e->mSinTime = 0;
	e->mSinX = tSinX;
	e->mSinY = tSinY;

	e->mTileBasePosition = Vector3D(0, 0, 0);
	e->mTileBaseScale = Vector2D(1.0, 1.0);
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

Position2D* getDreamMugenStageHandlerCameraEffectPositionReference()
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

Position2D* getDreamMugenStageHandlerCameraTargetPositionReference()
{
	return &gMugenStageHandlerData.mCameraTargetPosition;
}

Position* getDreamMugenStageHandlerCameraZoomReference()
{
	return &gMugenStageHandlerData.mCameraZoom;
}

void setDreamMugenStageHandlerCameraZoom(double tZoom)
{
	gMugenStageHandlerData.mCameraZoom = Vector3D(tZoom, tZoom, 1);
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
