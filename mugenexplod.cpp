#include "mugenexplod.h"

#include <prism/geometry.h>
#include <prism/physics.h>
#include <prism/datastructures.h>
#include <prism/memoryhandler.h>
#include <prism/mugenanimationhandler.h>
#include <prism/physicshandler.h>
#include <prism/log.h>
#include <prism/system.h>

#include "fightui.h"
#include "stage.h"
#include "mugenstagehandler.h"

typedef struct {
	int mInternalID;
	DreamPlayer* mPlayer;

	int mIsInFightDefFile;
	int mAnimationNumber;

	int mExternalID;
	Position mPosition;
	DreamExplodPositionType mPositionType;

	int mIsFlippedHorizontally;
	int mIsFlippedVertically;

	int mBindTime;

	Velocity mVelocity;
	Acceleration mAcceleration;

	Vector3DI mRandomOffset;
	int mRemoveTime;
	int mIsSuperMove;

	int mSuperMoveTime;
	int mPauseMoveTime;

	Vector3D mScale;
	int mSpritePriority;
	int mIsOnTop;

	int mIsUsingStageShadow;
	Vector3DI mShadow;

	int mUsesOwnPalette;
	int mIsRemovedOnGetHit;

	int mIgnoreHitPause;

	int mHasTransparencyType;
	DreamExplodTransparencyType mTransparencyType;

	int mIsFacingRight;
	int mPhysicsID;
	int mAnimationID;
	int mNow;

} Explod;


static struct {
	IntMap mExplods;
} gData;

static void loadExplods(void* tData) {
	(void)tData;
	gData.mExplods = new_int_map();
}

static void unloadExplods(void* tData) {
	(void)tData;
	delete_int_map(&gData.mExplods);
}

int addExplod(DreamPlayer* tPlayer)
{
	Explod* e = (Explod*)allocMemory(sizeof(Explod));
	e->mPlayer = tPlayer;
	e->mInternalID = int_map_push_back_owned(&gData.mExplods, e);
	return e->mInternalID;
}

void setExplodAnimation(int tID, int tIsInFightDefFile, int tAnimationNumber)
{
	Explod* e = (Explod*)int_map_get(&gData.mExplods, tID);
	e->mIsInFightDefFile = tIsInFightDefFile;
	e->mAnimationNumber = tAnimationNumber;
}

void setExplodID(int tID, int tExternalID)
{
	Explod* e = (Explod*)int_map_get(&gData.mExplods, tID);
	e->mExternalID = tExternalID;
}

void setExplodPosition(int tID, int tOffsetX, int tOffsetY)
{
	Explod* e = (Explod*)int_map_get(&gData.mExplods, tID);
	e->mPosition = makePosition(tOffsetX, tOffsetY, 0);
}

void setExplodPositionType(int tID, DreamExplodPositionType tType)
{
	Explod* e = (Explod*)int_map_get(&gData.mExplods, tID);
	e->mPositionType = tType;
}

void setExplodHorizontalFacing(int tID, int tFacing)
{
	Explod* e = (Explod*)int_map_get(&gData.mExplods, tID);
	int isPositionIndependentType = e->mPositionType == EXPLOD_POSITION_TYPE_RELATIVE_TO_RIGHT || e->mPositionType == EXPLOD_POSITION_TYPE_RELATIVE_TO_LEFT;
	if(isPositionIndependentType || e->mPosition.x >= 0) e->mIsFlippedHorizontally = tFacing == -1;
	else e->mIsFlippedHorizontally = tFacing == 1;
}

void setExplodVerticalFacing(int tID, int tFacing)
{
	Explod* e = (Explod*)int_map_get(&gData.mExplods, tID);
	e->mIsFlippedVertically = tFacing == -1;
}

// TODO: use bind time
void setExplodBindTime(int tID, int tBindTime)
{
	Explod* e = (Explod*)int_map_get(&gData.mExplods, tID);
	e->mBindTime = tBindTime;
}

void setExplodVelocity(int tID, double tX, double tY)
{
	Explod* e = (Explod*)int_map_get(&gData.mExplods, tID);
	e->mVelocity = makePosition(tX, tY, 0);
}

void setExplodAcceleration(int tID, double tX, double tY)
{
	Explod* e = (Explod*)int_map_get(&gData.mExplods, tID);
	e->mAcceleration = makePosition(tX, tY, 0);
}

void setExplodRandomOffset(int tID, int tX, int tY)
{
	Explod* e = (Explod*)int_map_get(&gData.mExplods, tID);
	e->mRandomOffset = makeVector3DI(tX, tY, 0);
}

void setExplodRemoveTime(int tID, int tRemoveTime)
{
	Explod* e = (Explod*)int_map_get(&gData.mExplods, tID);
	e->mRemoveTime = tRemoveTime;
}

void setExplodSuperMove(int tID, int tIsSuperMove)
{
	Explod* e = (Explod*)int_map_get(&gData.mExplods, tID);
	e->mIsSuperMove = tIsSuperMove;
}

void setExplodSuperMoveTime(int tID, int tSuperMoveTime)
{
	Explod* e = (Explod*)int_map_get(&gData.mExplods, tID);
	e->mSuperMoveTime = tSuperMoveTime;
}

void setExplodPauseMoveTime(int tID, int tPauseMoveTime)
{
	Explod* e = (Explod*)int_map_get(&gData.mExplods, tID);
	e->mPauseMoveTime = tPauseMoveTime;
}

void setExplodScale(int tID, double tX, double tY)
{
	Explod* e = (Explod*)int_map_get(&gData.mExplods, tID);
	e->mScale = makePosition(tX, tY, 0);
}

void setExplodSpritePriority(int tID, int tSpritePriority)
{
	Explod* e = (Explod*)int_map_get(&gData.mExplods, tID);
	e->mSpritePriority = tSpritePriority;
}

void setExplodOnTop(int tID, int tIsOnTop)
{
	Explod* e = (Explod*)int_map_get(&gData.mExplods, tID);
	e->mIsOnTop = tIsOnTop;
}

void setExplodShadow(int tID, int tR, int tG, int tB)
{
	Explod* e = (Explod*)int_map_get(&gData.mExplods, tID);
	e->mShadow = makeVector3DI(tR, tG, tB);
}

void setExplodOwnPalette(int tID, int tUsesOwnPalette)
{
	Explod* e = (Explod*)int_map_get(&gData.mExplods, tID);
	e->mUsesOwnPalette = tUsesOwnPalette;
}

void setExplodRemoveOnGetHit(int tID, int tIsRemovedOnGetHit)
{
	Explod* e = (Explod*)int_map_get(&gData.mExplods, tID);
	e->mIsRemovedOnGetHit = tIsRemovedOnGetHit;
}

void setExplodIgnoreHitPause(int tID, int tIgnoreHitPause)
{
	Explod* e = (Explod*)int_map_get(&gData.mExplods, tID);
	e->mIgnoreHitPause = tIgnoreHitPause;
}

void setExplodTransparencyType(int tID, int tHasTransparencyType, DreamExplodTransparencyType tTransparencyType)
{
	Explod* e = (Explod*)int_map_get(&gData.mExplods, tID);
	e->mHasTransparencyType = tHasTransparencyType;
	e->mTransparencyType = tTransparencyType;
}

static void explodAnimationFinishedCB(void* tCaller);

static Position getFinalExplodPositionFromPositionType(DreamExplodPositionType tPositionType, Position mOffset, DreamPlayer* tPlayer) {
	if (tPositionType == EXPLOD_POSITION_TYPE_RELATIVE_TO_P1) {
		DreamPlayer* target = tPlayer;
		Position p = getPlayerPosition(target, getPlayerCoordinateP(tPlayer));
		int isReversed = !getPlayerIsFacingRight(target);
		if (isReversed) mOffset.x *= -1;
		return vecAdd(p, mOffset);
	}
	else if (tPositionType == EXPLOD_POSITION_TYPE_RELATIVE_TO_P2) {
		DreamPlayer* target = getPlayerOtherPlayer(tPlayer);
		Position p = getPlayerPosition(target, getPlayerCoordinateP(tPlayer));
		int isReversed = !getPlayerIsFacingRight(target);
		if (isReversed) mOffset.x *= -1;
		return vecAdd(p, mOffset);
	}
	else if (tPositionType == EXPLOD_POSITION_TYPE_RELATIVE_TO_FRONT) {
		DreamPlayer* target = tPlayer;
		Position p = makePosition(getPlayerScreenEdgeInFrontX(target), getDreamStageTopOfScreenBasedOnPlayerInStageCoordinateOffset(getPlayerCoordinateP(target)), 0);
		int isReversed = !getPlayerIsFacingRight(target);
		if (isReversed) mOffset.x *= -1;
		return vecAdd(p, mOffset);
	}
	else if (tPositionType == EXPLOD_POSITION_TYPE_RELATIVE_TO_BACK) {
		DreamPlayer* target = tPlayer;
		Position p = makePosition(getPlayerScreenEdgeInBackX(target), getDreamStageTopOfScreenBasedOnPlayerInStageCoordinateOffset(getPlayerCoordinateP(target)), 0);
		int isReversed = getPlayerIsFacingRight(target);
		if (isReversed) mOffset.x *= -1;
		return vecAdd(p, mOffset);
	}
	else if (tPositionType == EXPLOD_POSITION_TYPE_RELATIVE_TO_LEFT) {
		Position p = makePosition(getDreamStageLeftOfScreenBasedOnPlayer(getPlayerCoordinateP(tPlayer)), getDreamStageTopOfScreenBasedOnPlayerInStageCoordinateOffset(getPlayerCoordinateP(tPlayer)), 0);
		return vecAdd(p, mOffset);
	}
	else if (tPositionType == EXPLOD_POSITION_TYPE_RELATIVE_TO_RIGHT) {
		Position p = makePosition(getDreamStageRightOfScreenBasedOnPlayer(getPlayerCoordinateP(tPlayer)), getDreamStageTopOfScreenBasedOnPlayerInStageCoordinateOffset(getPlayerCoordinateP(tPlayer)), 0);
		return vecAdd(p, mOffset);
	}
	else if (tPositionType == EXPLOD_POSITION_TYPE_NONE) {
		Position p = makePosition(getDreamGameWidth(getPlayerCoordinateP(tPlayer)) / 2, 0, 0);
		return vecAdd(p, mOffset);
	}
	else {
		logWarningFormat("Unrecognized position type %d. Defaulting to EXPLOD_POSITION_TYPE_RELATIVE_TO_P1.", tPositionType);
		DreamPlayer* target = tPlayer;
		Position p = getPlayerPosition(target, getPlayerCoordinateP(tPlayer));
		int isReversed = !getPlayerIsFacingRight(target);
		if (isReversed) mOffset.x *= -1;
		return vecAdd(p, mOffset);
	}

}

void finalizeExplod(int tID)
{
	Explod* e = (Explod*)int_map_get(&gData.mExplods, tID);

	MugenSpriteFile* sprites;
	MugenAnimation* animation;
	if (e->mIsInFightDefFile) {
		sprites = getDreamFightEffectSprites();
		animation = getDreamFightEffectAnimation(e->mAnimationNumber);
	}
	else {
		sprites = getPlayerSprites(e->mPlayer);
		animation = getPlayerAnimation(e->mPlayer, e->mAnimationNumber);
	}

	if (e->mPositionType == EXPLOD_POSITION_TYPE_RELATIVE_TO_P1) {
		e->mIsFacingRight = getPlayerIsFacingRight(e->mPlayer);
	}
	else if (e->mPositionType == EXPLOD_POSITION_TYPE_RELATIVE_TO_P2) {
		e->mIsFacingRight = getPlayerIsFacingRight(getPlayerOtherPlayer(e->mPlayer));
	}
	else {
		e->mIsFacingRight = 1;
	}

	e->mPhysicsID = addToPhysicsHandler(makePosition(0, 0, 0));

	if (!e->mIsFacingRight) {
		e->mVelocity.x = -e->mVelocity.x;
		e->mAcceleration.x = -e->mAcceleration.x;
		e->mIsFlippedHorizontally = !e->mIsFlippedHorizontally;
	}
	addAccelerationToHandledPhysics(e->mPhysicsID, e->mVelocity);

	Position p = getDreamStageCoordinateSystemOffset(getPlayerCoordinateP(e->mPlayer)) + getFinalExplodPositionFromPositionType(e->mPositionType, e->mPosition, e->mPlayer);
	p.z = PLAYER_Z + 1 * e->mSpritePriority;
	e->mAnimationID = addMugenAnimation(animation, sprites, p);
	setMugenAnimationBasePosition(e->mAnimationID, getHandledPhysicsPositionReference(e->mPhysicsID));
	setMugenAnimationCameraPositionReference(e->mAnimationID, getDreamMugenStageHandlerCameraPositionReference());
	setMugenAnimationCallback(e->mAnimationID, explodAnimationFinishedCB, e);
	setMugenAnimationFaceDirection(e->mAnimationID, !e->mIsFlippedHorizontally);
	setMugenAnimationVerticalFaceDirection(e->mAnimationID, !e->mIsFlippedVertically);
	setMugenAnimationDrawScale(e->mAnimationID, e->mScale);

	e->mNow = 0;
}


static void unloadExplod(Explod* e) {
	removeMugenAnimation(e->mAnimationID);
	removeFromPhysicsHandler(e->mPhysicsID);
}

typedef struct {
	DreamPlayer* mPlayer;
	int mExplodID;

} RemoveExplodsCaller;


static int removeSingleExplodWithID(void* tCaller, void* tData) {
	RemoveExplodsCaller* caller = (RemoveExplodsCaller*)tCaller;
	Explod* e = (Explod*)tData;

	if (e->mPlayer == caller->mPlayer && e->mExternalID == caller->mExplodID) {
		unloadExplod(e);
		return 1;
	}

	return 0;
}

void removeExplodsWithID(DreamPlayer * tPlayer, int tExplodID)
{
	RemoveExplodsCaller caller;
	caller.mPlayer = tPlayer;
	caller.mExplodID = tExplodID;
	int_map_remove_predicate(&gData.mExplods, removeSingleExplodWithID, &caller);
}

static int removeSingleExplod(void* tCaller, void* tData) {
	RemoveExplodsCaller* caller = (RemoveExplodsCaller*)tCaller;
	Explod* e = (Explod*)tData;

	if (e->mPlayer == caller->mPlayer) {
		unloadExplod(e);
		return 1;
	}

	return 0;
}

void removeAllExplodsForPlayer(DreamPlayer * tPlayer)
{
	RemoveExplodsCaller caller;
	caller.mPlayer = tPlayer;
	int_map_remove_predicate(&gData.mExplods, removeSingleExplod, &caller);
}

static int removeSingleExplodAlways(void* tCaller, void* tData) {
	Explod* e = (Explod*)tData;

	unloadExplod(e);
	return 1;
}

void removeAllExplods()
{
	int_map_remove_predicate(&gData.mExplods, removeSingleExplodAlways, NULL);
}

typedef struct {
	DreamPlayer* mPlayer;
	int mExplodID;
	int mReturnValue;
} FindExplodCaller;

void compareSingleExplodIDToSearchID(void* tCaller, void* tData) {
	FindExplodCaller* caller = (FindExplodCaller*)tCaller;
	Explod* e = (Explod*)tData;

	if (e->mPlayer == caller->mPlayer && e->mExternalID == caller->mExplodID) {
		caller->mReturnValue = e->mInternalID;
	}
}

int getExplodIndexFromExplodID(DreamPlayer* tPlayer, int tExplodID)
{
	FindExplodCaller caller;
	caller.mPlayer = tPlayer;
	caller.mExplodID = tExplodID;
	caller.mReturnValue = -1;

	int_map_map(&gData.mExplods, compareSingleExplodIDToSearchID, &caller);

	return caller.mReturnValue;
}


void compareSingleAmountSearchPlayer(void* tCaller, void* tData) {
	FindExplodCaller* caller = (FindExplodCaller*)tCaller;
	Explod* e = (Explod*)tData;

	if (e->mPlayer == caller->mPlayer) {
		caller->mReturnValue++;
	}
}

int getExplodAmount(DreamPlayer * tPlayer)
{
	FindExplodCaller caller;
	caller.mPlayer = tPlayer;
	caller.mReturnValue = 0;

	int_map_map(&gData.mExplods, compareSingleAmountSearchPlayer, &caller);

	return caller.mReturnValue;
}

void compareSingleAmountSearchExplodIDToSearchID(void* tCaller, void* tData) {
	FindExplodCaller* caller = (FindExplodCaller*)tCaller;
	Explod* e = (Explod*)tData;

	if (e->mPlayer == caller->mPlayer && e->mExternalID == caller->mExplodID) {
		caller->mReturnValue++;
	}
}

int getExplodAmountWithID(DreamPlayer * tPlayer, int tID)
{
	FindExplodCaller caller;
	caller.mPlayer = tPlayer;
	caller.mExplodID = tID;
	caller.mReturnValue = 0;

	int_map_map(&gData.mExplods, compareSingleAmountSearchExplodIDToSearchID, &caller);

	return caller.mReturnValue;
}

static void explodAnimationFinishedCB(void* tCaller) {
	Explod* e = (Explod*)tCaller;
	if (e->mRemoveTime != -2) return;

	e->mRemoveTime = 0; // TODO
}

static void updateExplodBindTime(Explod* e) {
	if (e->mBindTime <= 0) return;

	if (!isPlayer(e->mPlayer)) {
		return;
	}

	auto pos = getDreamStageCoordinateSystemOffset(getPlayerCoordinateP(e->mPlayer)) + getFinalExplodPositionFromPositionType(e->mPositionType, e->mPosition, e->mPlayer);
	pos.z = PLAYER_Z + 1 * e->mSpritePriority;
	setMugenAnimationPosition(e->mAnimationID, pos);

	e->mBindTime--;
}


static int updateExplodRemoveTime(Explod* e) {
	if (e->mRemoveTime < 0) return 0;

	e->mNow++;
	return e->mNow >= e->mRemoveTime;
}

static void updateExplodPhysics(Explod* e) {
	addAccelerationToHandledPhysics(e->mPhysicsID, e->mAcceleration);
}

static void updateStaticExplodPosition(Explod* e) {
	if (e->mPositionType == EXPLOD_POSITION_TYPE_RELATIVE_TO_RIGHT || e->mPositionType == EXPLOD_POSITION_TYPE_RELATIVE_TO_LEFT) {
		Position p = getDreamStageCoordinateSystemOffset(getPlayerCoordinateP(e->mPlayer)) + getFinalExplodPositionFromPositionType(e->mPositionType, e->mPosition, e->mPlayer);
		p.z = PLAYER_Z + 1 * e->mSpritePriority;
		setMugenAnimationPosition(e->mAnimationID, p);
	}
}

static int updateSingleExplod(void* tCaller, void* tData) {
	(void)tCaller;
	Explod* e = (Explod*)tData;
	updateStaticExplodPosition(e);
	if (isPlayerPaused(e->mPlayer)) return 0;

	updateExplodBindTime(e);
	if (updateExplodRemoveTime(e)) {
		unloadExplod(e);
		return 1;
	}

	updateExplodPhysics(e);

	return 0;
}

static void updateExplods(void* tData) {
	int_map_remove_predicate(&gData.mExplods, updateSingleExplod, NULL);
}

ActorBlueprint getDreamExplodHandler() {
	return makeActorBlueprint(loadExplods, unloadExplods, updateExplods);
}


// TODO: use bind time
typedef struct {
	DreamPlayer* mPlayer;
	int mID;
	int mBindTime;
} BindTimeSetterForIDCaller;

static void setExplodBindTimeForSingleExplod(void* tCaller, void* tData) {
	BindTimeSetterForIDCaller* caller = (BindTimeSetterForIDCaller*)tCaller;
	Explod* e = (Explod*)tData;

	if (e->mPlayer != caller->mPlayer) return;
	if (caller->mID != -1 && e->mExternalID != caller->mID) return;

	e->mBindTime = caller->mBindTime;
}

void setExplodBindTimeForID(DreamPlayer * tPlayer, int tExplodID, int tBindTime)
{
	BindTimeSetterForIDCaller caller;
	caller.mPlayer = tPlayer;
	caller.mID = tExplodID;
	caller.mBindTime = tBindTime;
	int_map_map(&gData.mExplods, setExplodBindTimeForSingleExplod, &caller);
}
