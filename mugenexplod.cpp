#include "mugenexplod.h"

#include <prism/geometry.h>
#include <prism/physics.h>
#include <prism/datastructures.h>
#include <prism/memoryhandler.h>
#include <prism/mugenanimationhandler.h>
#include <prism/physicshandler.h>
#include <prism/log.h>
#include <prism/system.h>
#include <prism/stlutil.h>

#include "fightui.h"
#include "stage.h"
#include "mugenstagehandler.h"

using namespace std;

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
	map<int, Explod> mExplods;
} gMugenExplod;

static void loadExplods(void* tData) {
	(void)tData;
	gMugenExplod.mExplods.clear();
}

static void unloadExplods(void* tData) {
	(void)tData;
	gMugenExplod.mExplods.clear();
}

int addExplod(DreamPlayer* tPlayer)
{
	int id = stl_int_map_push_back(gMugenExplod.mExplods, Explod());
	Explod& e = gMugenExplod.mExplods[id];
	e.mPlayer = tPlayer;
	e.mInternalID = id;
	return e.mInternalID;
}

void setExplodAnimation(int tID, int tIsInFightDefFile, int tAnimationNumber)
{
	Explod* e = &gMugenExplod.mExplods[tID];
	e->mIsInFightDefFile = tIsInFightDefFile;
	e->mAnimationNumber = tAnimationNumber;
}

void setExplodID(int tID, int tExternalID)
{
	Explod* e = &gMugenExplod.mExplods[tID];
	e->mExternalID = tExternalID;
}

void setExplodPosition(int tID, int tOffsetX, int tOffsetY)
{
	Explod* e = &gMugenExplod.mExplods[tID];
	e->mPosition = makePosition(tOffsetX, tOffsetY, 0);
}

void setExplodPositionType(int tID, DreamExplodPositionType tType)
{
	Explod* e = &gMugenExplod.mExplods[tID];
	e->mPositionType = tType;
}

void setExplodHorizontalFacing(int tID, int tFacing)
{
	Explod* e = &gMugenExplod.mExplods[tID];
	int isPositionIndependentType = e->mPositionType == EXPLOD_POSITION_TYPE_RELATIVE_TO_RIGHT || e->mPositionType == EXPLOD_POSITION_TYPE_RELATIVE_TO_LEFT;
	if(isPositionIndependentType || e->mPosition.x >= 0) e->mIsFlippedHorizontally = tFacing == -1;
	else e->mIsFlippedHorizontally = tFacing == 1;
}

void setExplodVerticalFacing(int tID, int tFacing)
{
	Explod* e = &gMugenExplod.mExplods[tID];
	e->mIsFlippedVertically = tFacing == -1;
}

// TODO: use bind time
void setExplodBindTime(int tID, int tBindTime)
{
	Explod* e = &gMugenExplod.mExplods[tID];
	e->mBindTime = tBindTime;
}

void setExplodVelocity(int tID, double tX, double tY)
{
	Explod* e = &gMugenExplod.mExplods[tID];
	e->mVelocity = makePosition(tX, tY, 0);
}

void setExplodAcceleration(int tID, double tX, double tY)
{
	Explod* e = &gMugenExplod.mExplods[tID];
	e->mAcceleration = makePosition(tX, tY, 0);
}

void setExplodRandomOffset(int tID, int tX, int tY)
{
	Explod* e = &gMugenExplod.mExplods[tID];
	e->mRandomOffset = makeVector3DI(tX, tY, 0);
}

void setExplodRemoveTime(int tID, int tRemoveTime)
{
	Explod* e = &gMugenExplod.mExplods[tID];
	e->mRemoveTime = tRemoveTime;
}

void setExplodSuperMove(int tID, int tIsSuperMove)
{
	Explod* e = &gMugenExplod.mExplods[tID];
	e->mIsSuperMove = tIsSuperMove;
}

void setExplodSuperMoveTime(int tID, int tSuperMoveTime)
{
	Explod* e = &gMugenExplod.mExplods[tID];
	e->mSuperMoveTime = tSuperMoveTime;
}

void setExplodPauseMoveTime(int tID, int tPauseMoveTime)
{
	Explod* e = &gMugenExplod.mExplods[tID];
	e->mPauseMoveTime = tPauseMoveTime;
}

void setExplodScale(int tID, double tX, double tY)
{
	Explod* e = &gMugenExplod.mExplods[tID];
	e->mScale = makePosition(tX, tY, 0);
}

void setExplodSpritePriority(int tID, int tSpritePriority)
{
	Explod* e = &gMugenExplod.mExplods[tID];
	e->mSpritePriority = tSpritePriority;
}

void setExplodOnTop(int tID, int tIsOnTop)
{
	Explod* e = &gMugenExplod.mExplods[tID];
	e->mIsOnTop = tIsOnTop;
}

void setExplodShadow(int tID, int tR, int tG, int tB)
{
	Explod* e = &gMugenExplod.mExplods[tID];
	e->mShadow = makeVector3DI(tR, tG, tB);
}

void setExplodOwnPalette(int tID, int tUsesOwnPalette)
{
	Explod* e = &gMugenExplod.mExplods[tID];
	e->mUsesOwnPalette = tUsesOwnPalette;
}

void setExplodRemoveOnGetHit(int tID, int tIsRemovedOnGetHit)
{
	Explod* e = &gMugenExplod.mExplods[tID];
	e->mIsRemovedOnGetHit = tIsRemovedOnGetHit;
}

void setExplodIgnoreHitPause(int tID, int tIgnoreHitPause)
{
	Explod* e = &gMugenExplod.mExplods[tID];
	e->mIgnoreHitPause = tIgnoreHitPause;
}

void setExplodTransparencyType(int tID, int tHasTransparencyType, DreamExplodTransparencyType tTransparencyType)
{
	Explod* e = &gMugenExplod.mExplods[tID];
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
	Explod* e = &gMugenExplod.mExplods[tID];

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


static int removeSingleExplodWithID(RemoveExplodsCaller* tCaller, Explod& tData) {
	Explod* e = &tData;

	if (e->mPlayer == tCaller->mPlayer && e->mExternalID == tCaller->mExplodID) {
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
	stl_int_map_remove_predicate(gMugenExplod.mExplods, removeSingleExplodWithID, &caller);
}

static int removeSingleExplod(RemoveExplodsCaller* tCaller, Explod& tData) {
	Explod* e = &tData;

	if (e->mPlayer == tCaller->mPlayer) {
		unloadExplod(e);
		return 1;
	}

	return 0;
}

void removeAllExplodsForPlayer(DreamPlayer * tPlayer)
{
	RemoveExplodsCaller caller;
	caller.mPlayer = tPlayer;
	stl_int_map_remove_predicate(gMugenExplod.mExplods, removeSingleExplod, &caller);
}

static int removeSingleExplodAlways(void* tCaller, Explod& tData) {
	Explod* e = &tData;

	unloadExplod(e);
	return 1;
}

void removeAllExplods()
{
	stl_int_map_remove_predicate(gMugenExplod.mExplods, removeSingleExplodAlways);
}

typedef struct {
	DreamPlayer* mPlayer;
	int mExplodID;
	int mReturnValue;
} FindExplodCaller;

void compareSingleExplodIDToSearchID(FindExplodCaller* tCaller, Explod& tData) {
	Explod* e = &tData;

	if (e->mPlayer == tCaller->mPlayer && e->mExternalID == tCaller->mExplodID) {
		tCaller->mReturnValue = e->mInternalID;
	}
}

int getExplodIndexFromExplodID(DreamPlayer* tPlayer, int tExplodID)
{
	FindExplodCaller caller;
	caller.mPlayer = tPlayer;
	caller.mExplodID = tExplodID;
	caller.mReturnValue = -1;

	stl_int_map_map(gMugenExplod.mExplods, compareSingleExplodIDToSearchID, &caller);

	return caller.mReturnValue;
}


void compareSingleAmountSearchPlayer(FindExplodCaller* tCaller, Explod& tData) {
	Explod* e = &tData;

	if (e->mPlayer == tCaller->mPlayer) {
		tCaller->mReturnValue++;
	}
}

int getExplodAmount(DreamPlayer * tPlayer)
{
	FindExplodCaller caller;
	caller.mPlayer = tPlayer;
	caller.mReturnValue = 0;

	stl_int_map_map(gMugenExplod.mExplods, compareSingleAmountSearchPlayer, &caller);

	return caller.mReturnValue;
}

void compareSingleAmountSearchExplodIDToSearchID(FindExplodCaller* tCaller, Explod& tData) {
	Explod* e = &tData;

	if (e->mPlayer == tCaller->mPlayer && e->mExternalID == tCaller->mExplodID) {
		tCaller->mReturnValue++;
	}
}

int getExplodAmountWithID(DreamPlayer * tPlayer, int tID)
{
	FindExplodCaller caller;
	caller.mPlayer = tPlayer;
	caller.mExplodID = tID;
	caller.mReturnValue = 0;

	stl_int_map_map(gMugenExplod.mExplods, compareSingleAmountSearchExplodIDToSearchID, &caller);

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

static int updateSingleExplod(void* tCaller, Explod& tData) {
	(void)tCaller;
	Explod* e = &tData;
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
	stl_int_map_remove_predicate(gMugenExplod.mExplods, updateSingleExplod);
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

static void setExplodBindTimeForSingleExplod(BindTimeSetterForIDCaller* tCaller, Explod& tData) {
	Explod* e = &tData;

	if (e->mPlayer != tCaller->mPlayer) return;
	if (tCaller->mID != -1 && e->mExternalID != tCaller->mID) return;

	e->mBindTime = tCaller->mBindTime;
}

void setExplodBindTimeForID(DreamPlayer * tPlayer, int tExplodID, int tBindTime)
{
	BindTimeSetterForIDCaller caller;
	caller.mPlayer = tPlayer;
	caller.mID = tExplodID;
	caller.mBindTime = tBindTime;
	stl_int_map_map(gMugenExplod.mExplods, setExplodBindTimeForSingleExplod, &caller);
}
