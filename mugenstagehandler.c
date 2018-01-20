#include "mugenstagehandler.h"

#include <assert.h>

#include <tari/datastructures.h>
#include <tari/math.h>
#include <tari/mugenanimationhandler.h>

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

	MugenAnimation* mAnimation;
	List mAnimationReferences;
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

static int getCameraCoordP() {
	return gData.mCameraCoordinates.y;
}

typedef struct{
	StaticStageHandlerElement* e;
} updateStageTileCaller;

static void updateSingleStaticStageElementTile(StaticStageHandlerElement* e, StageElementAnimationReference* tSingleAnimation) {
	double scale = e->mCoordinates.y / (double)gData.mCameraCoordinates.y;

	Vector3D deltaInCameraSpace = makePosition(getDreamCameraPositionX(getCameraCoordP()), getDreamCameraPositionY(getCameraCoordP()), 0);
	deltaInCameraSpace = vecScale(deltaInCameraSpace, -1);
	deltaInCameraSpace = vecScale3D(deltaInCameraSpace, e->mDelta);
	deltaInCameraSpace.z = 0;


	Vector3D deltaInElementSpace = vecScale(deltaInCameraSpace, scale); // TODO: fix
	tSingleAnimation->mReferencePosition = vecAdd(e->mStart, deltaInElementSpace);
	tSingleAnimation->mReferencePosition = vecAdd(tSingleAnimation->mReferencePosition, tSingleAnimation->mOffset);
	tSingleAnimation->mReferencePosition = vecSub(tSingleAnimation->mReferencePosition, vecScale(getDreamStageCoordinateSystemOffset(getCameraCoordP()), 1)); // TODO: properly
	tSingleAnimation->mReferencePosition = vecSub(tSingleAnimation->mReferencePosition, makePosition(-e->mCoordinates.x / 2, 0, 0));
	tSingleAnimation->mReferencePosition.z++;
}

static void updateSingleStaticStageElementTileCB(void* tCaller, void* tData) {
	updateStageTileCaller* caller = tCaller;
	StageElementAnimationReference* singleAnimation = tData;

	updateSingleStaticStageElementTile(caller->e, singleAnimation);
}

static void updateSingleStaticStageElement(StaticStageHandlerElement* e) {
	updateStageTileCaller caller;
	caller.e = e;

	list_map(&e->mAnimationReferences, updateSingleStaticStageElementTileCB, &caller);
	
	
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
	gData.mCameraPosition.x = gData.mCameraTargetPosition.x;
}

void setDreamMugenStageHandlerCameraPositionX(double tX)
{
	gData.mCameraTargetPosition.x = tX;
	gData.mCameraPosition.x = gData.mCameraTargetPosition.x;
}

void resetDreamMugenStageHandlerCameraPosition()
{
	gData.mCameraPosition.x = gData.mCameraTargetPosition.x = 0; // TODO: properly
	gData.mCameraPosition.y = gData.mCameraTargetPosition.y = 0;
	
}

void setDreamMugenStageHandlerScreenShake(int tTime, double tFrequency, int tAmplitude, double tPhase)
{
	(void)tTime;
	(void)tFrequency;
	(void)tAmplitude;
	(void)tPhase;
	// TODO;
}

static void handleSingleTile(int tTile, int* tStart, int* tAmount, int tSize, double tMinCam, double tMaxCam, int tScreenSize) {
	if (!tTile) {
		*tStart = 0;
		*tAmount = 1;
	}
	else if (tTile == 1) {
		*tStart = (int)tMinCam - (tScreenSize / 2);
		int length = (int)(tMaxCam - tMinCam) + tScreenSize;
		*tAmount = length / tSize + 1;
	}
	else {
		*tStart = 0;
		*tAmount = tTile;
	}
}

static void addSingleMugenStageHandlerBackgroundElementTile(StaticStageHandlerElement* e, MugenSpriteFile* tSprites, BlendType tBlendType, Vector3D tOffset) {
	StageElementAnimationReference* newAnimation = allocMemory(sizeof(StageElementAnimationReference));
	newAnimation->mID = addMugenAnimation(e->mAnimation, tSprites, makePosition(0, 0, 0));
	setMugenAnimationBasePosition(newAnimation->mID, &newAnimation->mReferencePosition);
	newAnimation->mOffset = tOffset;
	setMugenAnimationBlendType(newAnimation->mID, tBlendType);

	list_push_back_owned(&e->mAnimationReferences, newAnimation);
}

static void addMugenStageHandlerBackgroundElementTiles(StaticStageHandlerElement* e, MugenSpriteFile* tSprites, Vector3DI tTile, Vector3DI tTileSpacing, BlendType tBlendType) {
	int startX;
	int startY;
	int amountX;
	int amountY;
	Vector3DI size = getAnimationFirstElementSpriteSize(e->mAnimation, tSprites);

	assert(!tTileSpacing.x); // TODO
	assert(!tTileSpacing.y); // TODO

	double cameraToElementScale = e->mCoordinates.y / getCameraCoordP();
	handleSingleTile(tTile.x, &startX, &amountX, size.x, gData.mCameraRange.mTopLeft.x*cameraToElementScale, gData.mCameraRange.mBottomRight.x*cameraToElementScale, e->mCoordinates.x);
	handleSingleTile(tTile.y, &startY, &amountY, size.y, gData.mCameraRange.mTopLeft.y*cameraToElementScale, gData.mCameraRange.mBottomRight.y*cameraToElementScale, e->mCoordinates.y);

	Vector3D offset = makePosition(0, startY, 0);
	int j;
	for (j = 0; j < amountY; j++) {
		int i;
		offset.x = startX;
		for (i = 0; i < amountX; i++) {
			addSingleMugenStageHandlerBackgroundElementTile(e, tSprites, tBlendType, offset);
			offset.x += size.x;
		}
		offset.y += size.y;
	}

}

static void addMugenStageHandlerBackgroundElement(Position tStart, MugenAnimation* tAnimation, MugenSpriteFile * tSprites, Position tDelta, Vector3DI tTile, Vector3DI tTileSpacing, BlendType tBlendType, Vector3DI tCoordinates) {
	StaticStageHandlerElement* e = allocMemory(sizeof(StaticStageHandlerElement));
	e->mStart = tStart;
	e->mDelta = tDelta;
	e->mCoordinates = tCoordinates;

	e->mAnimation = tAnimation;
	e->mAnimationReferences = new_list();
	
	addMugenStageHandlerBackgroundElementTiles(e, tSprites, tTile, tTileSpacing, tBlendType);
	updateSingleStaticStageElement(e);

	vector_push_back_owned(&gData.mStaticElements, e);
}

void addDreamMugenStageHandlerAnimatedBackgroundElement(Position tStart, int tAnimationID, MugenAnimations* tAnimations, MugenSpriteFile * tSprites, Position tDelta, Vector3DI tTile, Vector3DI tTileSpacing, BlendType tBlendType, Vector3DI tCoordinates)
{
	addMugenStageHandlerBackgroundElement(tStart, getMugenAnimation(tAnimations, tAnimationID), tSprites, tDelta, tTile, tTileSpacing, tBlendType, tCoordinates);
}

void addDreamMugenStageHandlerStaticBackgroundElement(Position tStart, int tSpriteGroup, int tSpriteItem, MugenSpriteFile* tSprites, Position tDelta, Vector3DI tTile, Vector3DI tTileSpacing, BlendType tBlendType, Vector3DI tCoordinates)
{
	addMugenStageHandlerBackgroundElement(tStart, createOneFrameMugenAnimationForSprite(tSpriteGroup, tSpriteItem), tSprites, tDelta, tTile, tTileSpacing, tBlendType, tCoordinates);
}

Position * getDreamMugenStageHandlerCameraPositionReference()
{
	return &gData.mCameraPosition;
}
