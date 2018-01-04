#include "mugenanimationhandler.h"

#include <assert.h>

#include <tari/datastructures.h>
#include <tari/math.h>
#include <tari/drawing.h>
#include <tari/collisionhandler.h>
#include <tari/mugenspritefilereader.h>

#include "playerdefinition.h"
#include "stage.h"
#include "collision.h"
#include "playerhitdata.h"

typedef int MugenDuration;

typedef struct {
	int mList;
	int mID;
	Collider mCollider;
} MugenAnimationHandlerHitboxElement;

typedef struct {
	DreamPlayer* mPlayer;

	MugenAnimation* mAnimation;
	MugenSpriteFile* mSprites;
	int mStep;

	MugenDuration mOverallTime;
	MugenDuration mStepTime;
	MugenSpriteFileSprite* mSprite;
	int mHasSprite;

	Position* mBasePosition;
	Position mPlayerPositionInStageCoordinates;
	int mSpritePriority;

	int mIsFacingRight;

	int mIsUsingFixedZ;

	List mActiveHitboxes;

	int mPositionCoordinateP;
	int mScaleCoordinateP;

	int mIsPaused;

	int mIsAddition;

	int mHasRectangleWidth;
	int mRectangleWidth;

	int mHasCameraPositionInScreenSpace;
	Position* mCameraPositionInScreenSpaceReference;

	int mInvisibleFlag;

	int mHasOneFrameScale;
	Vector3D mOneFrameScale;

	int mHasOneFrameAngle;
	int mHasActiveFrameAngle;
	double mOneFrameAngle;

	int mHasStageDisabled;

} MugenAnimationHandlerElement;

static struct {
	IntMap mAnimations;
} gData;

static void loadMugenAnimationHandler(void* tData) {
	(void)tData;
	gData.mAnimations = new_int_map();
}

static MugenAnimationStep* getCurrentAnimationStep(MugenAnimationHandlerElement* e) {
	return vector_get(&e->mAnimation->mSteps, min(e->mStep, vector_size(&e->mAnimation->mSteps) - 1));
}


static void passiveAnimationHitCB(void* tCaller, void* tCollisionData) {
	MugenAnimationHandlerElement* e = tCaller;
	playerHitCB(e->mPlayer, tCollisionData);
}


static void attackAnimationHitCB(void* tCaller, void* tCollisionData) {
	(void)tCaller;
	(void)tCollisionData;
}

typedef struct {
	MugenAnimationHandlerElement* mElement;
	int mList;
	CollisionCallback mCB;
} HitboxAdditionCaller;

static void addSingleHitbox(void* tCaller, void* tData) {
	HitboxAdditionCaller* caller = tCaller;
	CollisionRect* rect = tData;

	MugenAnimationHandlerHitboxElement* e = allocMemory(sizeof(MugenAnimationHandlerHitboxElement));

	CollisionRect scaledRectangle = *rect;
	scaledRectangle = scaleGeoRectangleByFactor(scaledRectangle, getDreamScreenFactorFromCoordinateP(caller->mElement->mScaleCoordinateP)); 
	if (!caller->mElement->mIsFacingRight)	{
		double xBuffer = scaledRectangle.mTopLeft.x;
		scaledRectangle.mTopLeft.x = -scaledRectangle.mBottomRight.x;
		scaledRectangle.mBottomRight.x = -xBuffer;
	}

	e->mCollider = makeColliderFromRect(scaledRectangle); 
	
	e->mList = caller->mList;
	e->mID = addColliderToCollisionHandler(caller->mList, &caller->mElement->mPlayerPositionInStageCoordinates, e->mCollider, caller->mCB, caller->mElement, getPlayerHitDataReference(caller->mElement->mPlayer));
	
	list_push_back_owned(&caller->mElement->mActiveHitboxes, e);
}

static void addNewSingleHitboxType(MugenAnimationHandlerElement* e, List* tHitboxes, int tList, CollisionCallback tCB) {
	HitboxAdditionCaller caller;
	caller.mElement = e;
	caller.mList = tList;
	caller.mCB = tCB;

	list_map(tHitboxes, addSingleHitbox, &caller);
} 

static void addNewHitboxes(MugenAnimationHandlerElement* e, MugenAnimationStep* tStep) {
	if (!e->mPlayer) return;
	addNewSingleHitboxType(e, &tStep->mPassiveHitboxes, getDreamPlayerPassiveCollisionList(e->mPlayer), passiveAnimationHitCB);
	addNewSingleHitboxType(e, &tStep->mAttackHitboxes, getDreamPlayerAttackCollisionList(e->mPlayer), attackAnimationHitCB);
}

static int removeSingleHitbox(void* tCaller, void* tData) {
	(void)tCaller;
	MugenAnimationHandlerHitboxElement* hitbox = tData;

	removeFromCollisionHandler(hitbox->mList, hitbox->mID);
	destroyCollider(&hitbox->mCollider);
	return 1;
}

static void removeOldHitboxes(MugenAnimationHandlerElement* e) {
	list_remove_predicate(&e->mActiveHitboxes, removeSingleHitbox, NULL);
}

static void updateHitboxes(MugenAnimationHandlerElement* e) {
	removeOldHitboxes(e);
	addNewHitboxes(e, getCurrentAnimationStep(e));
}

static MugenDuration getTimeWhenStepStarts(MugenAnimationHandlerElement* e, int tStep) {
	MugenDuration sum = 0;
	int i;
	for (i = 0; i < tStep; i++) {
		MugenAnimationStep* step = vector_get(&e->mAnimation->mSteps, i);
		sum += step->mDuration;
	}
	return sum;

}

static void loadNextStep(MugenAnimationHandlerElement* e) {
	e->mStep++;
	if (e->mStep == vector_size(&e->mAnimation->mSteps)) {
		e->mStep = e->mAnimation->mLoopStart;
		e->mOverallTime = getTimeWhenStepStarts(e, e->mStep); // TODO: test
	}

	e->mStepTime = 0;

	MugenAnimationStep* step = getCurrentAnimationStep(e);
	e->mSprite = getMugenSpriteFileTextureReference(e->mSprites, step->mGroupNumber, step->mSpriteNumber);
	e->mHasSprite = e->mSprite != NULL;

	updateHitboxes(e);

	e->mIsAddition = step->mIsAddition; // TODO: properly
}

static void increaseMugenDuration(MugenDuration* tDuration) {
	(*tDuration)++;
}

static void startNewAnimationWithStartStep(MugenAnimationHandlerElement* e, int tStartStep) {
	e->mOverallTime = getTimeWhenStepStarts(e, tStartStep);
	e->mSpritePriority = 0;

	e->mStep = tStartStep -1;
	loadNextStep(e);
}

int addDreamRegisteredAnimation(DreamPlayer* p, MugenAnimation * tStartAnimation, MugenSpriteFile* tSprites, Position* tBasePosition, int tPositionCoordinateSystemInP, int tScaleCoordinateSystemInP)
{
	MugenAnimationHandlerElement* e = allocMemory(sizeof(MugenAnimationHandlerElement));
	e->mAnimation = tStartAnimation;
	e->mSprites = tSprites;
	e->mOverallTime = 0;
	e->mBasePosition = tBasePosition;
	e->mPositionCoordinateP = tPositionCoordinateSystemInP;
	e->mScaleCoordinateP = tScaleCoordinateSystemInP;
	e->mPlayerPositionInStageCoordinates = vecAdd(*e->mBasePosition, getDreamStageCoordinateSystemOffset(e->mPositionCoordinateP));
	e->mPlayer = p;
	e->mIsFacingRight = 1;
	e->mActiveHitboxes = new_list();
	e->mIsUsingFixedZ = 0;
	e->mIsPaused = 0;
	e->mHasRectangleWidth = 0;
	e->mHasCameraPositionInScreenSpace = 0;
	e->mInvisibleFlag = 0;
	e->mHasOneFrameScale = 0;
	e->mHasOneFrameAngle = 0;
	e->mHasActiveFrameAngle = 0;
	e->mHasStageDisabled = 0;

	startNewAnimationWithStartStep(e, 0); 
	
	return int_map_push_back_owned(&gData.mAnimations, e);
}

void removeDreamRegisteredAnimation(int tID)
{
	MugenAnimationHandlerElement* e = int_map_get(&gData.mAnimations, tID);
	removeOldHitboxes(e);
	delete_list(&e->mActiveHitboxes);

	int_map_remove(&gData.mAnimations, tID);
}

int getDreamRegisteredAnimationAnimationNumber(int tID)
{
	MugenAnimationHandlerElement* e = int_map_get(&gData.mAnimations, tID);
	return e->mAnimation->mID;
}

int getDreamRegisteredAnimationRemainingAnimationTime(int tID)
{
	MugenAnimationHandlerElement* e = int_map_get(&gData.mAnimations, tID);
	int remainingTime = (e->mAnimation->mTotalDuration - e->mOverallTime) - 1;
	
	remainingTime = max(0, remainingTime); // TODO: fix when reading out

	return remainingTime;
}

void setDreamRegisteredAnimationFaceDirection(int tID, FaceDirection tDirection)
{
	MugenAnimationHandlerElement* e = int_map_get(&gData.mAnimations, tID);
	e->mIsFacingRight = tDirection == FACE_DIRECTION_RIGHT;
}

void setDreamRegisteredAnimationRectangleWidth(int tID, int tWidth)
{
	MugenAnimationHandlerElement* e = int_map_get(&gData.mAnimations, tID);
	e->mHasRectangleWidth = 1;
	e->mRectangleWidth = tWidth;
}

void setDreamRegisteredAnimationCameraPositionReference(int tID, Position * tCameraPosition)
{
	MugenAnimationHandlerElement* e = int_map_get(&gData.mAnimations, tID);
	e->mHasCameraPositionInScreenSpace = 1;
	e->mCameraPositionInScreenSpaceReference = tCameraPosition;
}

void setDreamRegisteredAnimationInvisibleFlag(int tID)
{
	MugenAnimationHandlerElement* e = int_map_get(&gData.mAnimations, tID);
	e->mInvisibleFlag = 1;
}

void setDreamRegisteredAnimationOneFrameDrawScale(int tID, Vector3D tScale)
{
	MugenAnimationHandlerElement* e = int_map_get(&gData.mAnimations, tID);
	e->mHasOneFrameScale = 1;
	e->mOneFrameScale = tScale;
}

void setDreamRegisteredAnimationOneFrameDrawAngle(int tID, double tAngle)
{
	MugenAnimationHandlerElement* e = int_map_get(&gData.mAnimations, tID);
	e->mHasOneFrameAngle = 1;
	if (e->mHasActiveFrameAngle) return;

	e->mHasActiveFrameAngle = 1;
	e->mOneFrameAngle = tAngle;
}

void addDreamRegisteredAnimationOneFrameDrawAngle(int tID, double tAngle)
{
	MugenAnimationHandlerElement* e = int_map_get(&gData.mAnimations, tID);

	e->mOneFrameAngle += tAngle;
}

void setDreamRegisteredAnimationOneFrameFixedDrawAngle(int tID, double tAngle)
{
	MugenAnimationHandlerElement* e = int_map_get(&gData.mAnimations, tID);
	e->mOneFrameAngle = tAngle;
}

void setDreamRegisteredAnimationToNotUseStage(int tID)
{
	MugenAnimationHandlerElement* e = int_map_get(&gData.mAnimations, tID);
	e->mHasStageDisabled = 1;
}

void setDreamRegisteredAnimationBasePosition(int tID, Position * tBasePosition)
{
	MugenAnimationHandlerElement* e = int_map_get(&gData.mAnimations, tID);
	e->mBasePosition = tBasePosition;
}

void changeDreamGameMugenAnimationWithStartStep(int tID, MugenAnimation * tNewAnimation, int tStartStep)
{
	MugenAnimationHandlerElement* e = int_map_get(&gData.mAnimations, tID);
	e->mAnimation = tNewAnimation;
	startNewAnimationWithStartStep(e, tStartStep);
}

int isDreamStartingHandledAnimationElementWithID(int tID, int tStepID)
{
	MugenAnimationHandlerElement* e = int_map_get(&gData.mAnimations, tID);
	if (e->mIsPaused) return 0;

	int currentStep = e->mStep + 1;
	return currentStep == tStepID && e->mStepTime == 0;
}



int getDreamTimeFromHandledAnimationElement(int tID, int tStep)
{
	tStep--;

	MugenAnimationHandlerElement* e = int_map_get(&gData.mAnimations, tID);
	if (tStep >= vector_size(&e->mAnimation->mSteps)) {
		tStep = vector_size(&e->mAnimation->mSteps) - 1; // TODO: think about this
	}

	MugenDuration sum = getTimeWhenStepStarts(e, tStep);

	int offsetFromStep = e->mOverallTime - sum;

	return offsetFromStep;
}

static int getHandledAnimationElementFromTimeOffsetLoop(MugenAnimationHandlerElement* e, int tTime, int tCurrentStep, int dx) {
	int n = vector_size(&e->mAnimation->mSteps);

	int isRunning = 1;
	while (isRunning) {
		MugenAnimationStep* step = vector_get(&e->mAnimation->mSteps, tCurrentStep);
		if (tTime < step->mDuration) return tCurrentStep;
		tTime -= step->mDuration;

		tCurrentStep += dx;
		if (tCurrentStep == n) tCurrentStep = e->mAnimation->mLoopStart;
		if (tCurrentStep < e->mAnimation->mLoopStart) tCurrentStep = n - 1;
	}

	return -1;
}

int getDreamHandledAnimationElementFromTimeOffset(int tID, int tTime)
{
	MugenAnimationHandlerElement* e = int_map_get(&gData.mAnimations, tID);
	
	int ret;
	if (tTime > 0) {
		tTime += e->mStepTime;
		ret = getHandledAnimationElementFromTimeOffsetLoop(e, tTime, e->mStep, 1);
	}
	else {
		MugenAnimationStep* step = vector_get(&e->mAnimation->mSteps, e->mStep);
		tTime *= -1;
		tTime += (step->mDuration - 1) - e->mStepTime;
		ret = getHandledAnimationElementFromTimeOffsetLoop(e, tTime, e->mStep, -1);
	}

	return ret + 1;
}

void setDreamRegisteredAnimationSpritePriority(int tID, int tPriority)
{
	MugenAnimationHandlerElement* e = int_map_get(&gData.mAnimations, tID);
	e->mSpritePriority = tPriority;
}

void setDreamRegisteredAnimationToUseFixedZ(int tID)
{
	MugenAnimationHandlerElement* e = int_map_get(&gData.mAnimations, tID);
	e->mIsUsingFixedZ = 1;
}

static void updateSingleMugenAnimation(void* tCaller, void* tData) {
	(void)tCaller;
	MugenAnimationHandlerElement* e = tData;

	if (e->mIsPaused) return;

	MugenAnimationStep* step = getCurrentAnimationStep(e);
	increaseMugenDuration(&e->mOverallTime);
	increaseMugenDuration(&e->mStepTime);
	if (step->mDuration == -1) return;
	if (e->mStepTime >= step->mDuration) {
		loadNextStep(e);
	}
}

void pauseDreamRegisteredAnimation(int tID)
{
	MugenAnimationHandlerElement* e = int_map_get(&gData.mAnimations, tID);
	e->mIsPaused = 1;
}

void unpauseDreamRegisteredAnimation(int tID)
{
	MugenAnimationHandlerElement* e = int_map_get(&gData.mAnimations, tID);
	e->mIsPaused = 0;
}

void advanceDreamRegisteredAnimationOneTick(int tID)
{
	MugenAnimationHandlerElement* e = int_map_get(&gData.mAnimations, tID);
	updateSingleMugenAnimation(NULL, e);
}


static void updateMugenAnimationHandler(void* tData) {
	(void)tData;

	int_map_map(&gData.mAnimations, updateSingleMugenAnimation, NULL);
}

typedef struct {
	MugenAnimationHandlerElement* e;
	MugenAnimationStep* mStep;
	Position mScalePosition;
	Vector3D mScale;

	Position mBasePosition;

} DrawSingleMugenAnimationSpriteCaller;

static void drawSingleMugenAnimationSpriteCB(void* tCaller, void* tData) {
	DrawSingleMugenAnimationSpriteCaller* caller = tCaller;
	MugenSpriteFileSubSprite* sprite = tData;

	MugenAnimationHandlerElement* e = caller->e;
	MugenAnimationStep* step = caller->mStep;
	Position p = caller->mBasePosition;
	p = vecAdd(p, makePosition(sprite->mOffset.x, sprite->mOffset.y, 0));

	Rectangle texturePos = makeRectangleFromTexture(sprite->mTexture);
	if (e->mHasRectangleWidth) {
		int newWidth = e->mRectangleWidth - sprite->mOffset.x;
		if (newWidth <= 0) return;
		newWidth = min(newWidth, sprite->mTexture.mTextureSize.x);
		texturePos.bottomRight.x = texturePos.topLeft.x + newWidth;
	}

	int isFacingRight = e->mIsFacingRight;
	if (step->mIsFlippingHorizontally) isFacingRight ^= 1;
	
	if (!isFacingRight) {
		Rectangle originalTexturePos = texturePos;
		Position center = e->mPlayerPositionInStageCoordinates;
		double deltaX = center.x - p.x;
		double nRightX = center.x + deltaX;
		double nLeftX = nRightX - abs(originalTexturePos.bottomRight.x - originalTexturePos.topLeft.x);
		p.x = nLeftX;
		texturePos.topLeft.x = originalTexturePos.bottomRight.x;
		texturePos.bottomRight.x = originalTexturePos.topLeft.x;
	}

	if (e->mHasCameraPositionInScreenSpace) {
		p = vecSub(p, *e->mCameraPositionInScreenSpaceReference);
	}

	if (e->mIsAddition) {
		setDrawingBlendType(BLEND_TYPE_ADDITION);
	}

	scaleDrawing3D(caller->mScale, caller->mScalePosition);
	drawSprite(sprite->mTexture, p, texturePos);
	setDrawingParametersToIdentity();
}

static void drawSingleMugenAnimation(void* tCaller, void* tData) {
	(void)tCaller;
	MugenAnimationHandlerElement* e = tData;

	double drawAngle;
	Vector3D drawScale;

	if (e->mHasOneFrameAngle) {
		drawAngle = e->mOneFrameAngle;
		e->mHasOneFrameAngle = 0;
		e->mHasActiveFrameAngle = 1;
	}
	else {
		drawAngle = 0;
		e->mHasActiveFrameAngle = 0;
	}

	(void)drawAngle; // TODO: use

	if (e->mHasOneFrameScale) {
		drawScale = e->mOneFrameScale;
		e->mHasOneFrameScale = 0;
	}
	else {
		drawScale = makePosition(1, 1, 1);
	}

	if (e->mInvisibleFlag) {
		e->mInvisibleFlag = 0;
		return;
	}

	if (!e->mHasSprite) return;

	MugenAnimationStep* step = getCurrentAnimationStep(e);

	if (e->mHasStageDisabled) {
		e->mPlayerPositionInStageCoordinates = *e->mBasePosition;
	} else {
		e->mPlayerPositionInStageCoordinates = vecAdd(*e->mBasePosition, getDreamStageCoordinateSystemOffset(e->mPositionCoordinateP));
		e->mPlayerPositionInStageCoordinates = vecAdd(e->mPlayerPositionInStageCoordinates, step->mDelta);
		e->mPlayerPositionInStageCoordinates = vecScale(e->mPlayerPositionInStageCoordinates, getDreamScreenFactorFromCoordinateP(e->mPositionCoordinateP));
	}
	Position p = e->mPlayerPositionInStageCoordinates;


	drawScale = vecScale(drawScale, getDreamScreenFactorFromCoordinateP(e->mScaleCoordinateP));
	drawScale.z = 1;

	if (e->mHasCameraPositionInScreenSpace) {
		p = vecSub(p, *e->mCameraPositionInScreenSpaceReference);
	}

	Position scalePosition = p;

	p = vecSub(p, e->mSprite->mAxisOffset);

	if (e->mHasCameraPositionInScreenSpace) {
		p = vecAdd(p, *e->mCameraPositionInScreenSpaceReference); // TODO: more elegantly
	}

	double z;
	if(e->mIsUsingFixedZ) {
		z = e->mBasePosition->z;
	}
	else {
		z = 10 + 0.1*e->mSpritePriority; // TODO
	}
	p.z = z;

	DrawSingleMugenAnimationSpriteCaller caller;
	caller.e = e;
	caller.mStep = step;
	caller.mScale = drawScale;
	caller.mScalePosition = scalePosition;
	caller.mBasePosition = p;

	list_map(&e->mSprite->mTextures, drawSingleMugenAnimationSpriteCB, &caller);
}

static void drawMugenAnimationHandler(void* tData) {
	(void)tData;
	int_map_map(&gData.mAnimations, drawSingleMugenAnimation, NULL);
}


ActorBlueprint DreamMugenGameAnimationHandler = {
	.mLoad = loadMugenAnimationHandler,
	.mUpdate = updateMugenAnimationHandler,
	.mDraw = drawMugenAnimationHandler,
};
