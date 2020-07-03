#include "projectile.h"

#include <assert.h>
#include <algorithm>

#include <prism/datastructures.h>
#include <prism/math.h>
#include <prism/log.h>

#include "stage.h"
#include "mugenstagehandler.h"

using namespace std;

typedef struct {
	DreamPlayer* mPlayer;

	int mID;
	int mHitAnimation;
	int mRemoveAnimation;
	int mCancelAnimation;

	Vector2D mScale;
	int mRemoveAfterHit;
	int mRemoveTime;

	Vector3D mRemoveVelocity;
	Vector3D mAcceleration;
	Vector3D mVelocityMultipliers;

	int mHitAmountBeforeVanishing;
	int mMissTime;
	int mPriority;
	int mEdgeBound;
	int mStageBound;
	int mLowerBound;
	int mUpperBound;
	int mShadow;
	
	int mRemapPaletteGroup;
	int mRemapPaletteItem;

	int mAfterImageTime;
	int mAfterImageLength;
	int mAfterImage;

	int mNow;
	int mMissHitNow;
	int mHasChangedAnimationFinal;
	int mShouldBeRemoved;
} Projectile;

static struct {
	IntMap mProjectileList;
} gProjectileData;

static void loadProjectileHandler(void* tData) {
	(void)tData;
	setProfilingSectionMarkerCurrentFunction();
	gProjectileData.mProjectileList = new_int_map();
}

static void unloadProjectileHandler(void* tData) {
	(void)tData;
	setProfilingSectionMarkerCurrentFunction();
	delete_int_map(&gProjectileData.mProjectileList);
}

static void projectileRemoveAnimationFinishedCB(void* tCaller) {
	Projectile* e = (Projectile*)tCaller;
	e->mShouldBeRemoved = 1;
}

static int changeProjectileAnimation(Projectile* e, int tAnimationNumber, int tShouldBeRemoved = 1) {
	if (getPlayerAnimationNumber(e->mPlayer) != tAnimationNumber) {
		changePlayerAnimation(e->mPlayer, tAnimationNumber);
	}

	e->mHasChangedAnimationFinal |= tShouldBeRemoved;
	if (tShouldBeRemoved) {
		if (isPlayerAnimationTimeInfinite(e->mPlayer)) {
			removeProjectile(e->mPlayer);
			return 1;
		}
		else {
			setPlayerAnimationFinishedCallback(e->mPlayer, projectileRemoveAnimationFinishedCB, e);
			setProjectileVelocity(e->mPlayer, e->mRemoveVelocity.x, e->mRemoveVelocity.y, getDreamMugenStageHandlerCameraCoordinateP());
		}
	}
	return 0;
}

static int updateProjectileDurationAndReturnIfOver(Projectile* e) {
	if (e->mRemoveTime == -1) return 0;

	e->mNow++;
	if (!e->mHasChangedAnimationFinal && e->mNow >= e->mRemoveTime) {
		return changeProjectileAnimation(e, e->mRemoveAnimation);
	}
	return 0;
}

static int updateProjectileBoundsAndReturnIfOver(Projectile* e) {
	const auto x = getPlayerPositionX(e->mPlayer, getDreamMugenStageHandlerCameraCoordinateP());
	const auto y = getPlayerPositionY(e->mPlayer, getDreamMugenStageHandlerCameraCoordinateP());

	const auto left = getDreamStageLeftOfScreenBasedOnPlayer(getDreamMugenStageHandlerCameraCoordinateP());
	const auto right = getDreamStageRightOfScreenBasedOnPlayer(getDreamMugenStageHandlerCameraCoordinateP());
	const auto overShootLeft = left - x;
	const auto overShootRight = x - right;
	const auto maxiEdgeBound = max(overShootLeft, overShootRight);

	const auto stageLeft = getDreamStageBoundLeft(getDreamMugenStageHandlerCameraCoordinateP());
	const auto stageRight = getDreamStageBoundRight(getDreamMugenStageHandlerCameraCoordinateP());
	const auto leftOfStageOffset = stageLeft - x;
	const auto rightOfStageOffset = x - stageRight;
	const auto maxiStageBound = max(leftOfStageOffset, rightOfStageOffset);

	if (maxiEdgeBound > e->mEdgeBound || maxiStageBound > e->mStageBound || (y < e->mLowerBound || y > e->mUpperBound)) {
		removeProjectile(e->mPlayer);
		return 1;
	}
	return 0;
}

static void updateProjectilePhysics(Projectile* e) {
	if (e->mHasChangedAnimationFinal) return;

	multiplyPlayerVelocityX(e->mPlayer, e->mVelocityMultipliers.x);
	multiplyPlayerVelocityY(e->mPlayer, e->mVelocityMultipliers.y);
	addPlayerVelocityX(e->mPlayer, e->mAcceleration.x, getDreamMugenStageHandlerCameraCoordinateP());
	addPlayerVelocityY(e->mPlayer, e->mAcceleration.y, getDreamMugenStageHandlerCameraCoordinateP());
}

static void updateProjectileMissTime(Projectile* e) {
	if (e->mMissHitNow < e->mMissTime) {
		e->mMissHitNow++;
	}
}

static void updateProjectileShadow(Projectile* e) {
	if (!e->mShadow) {
		setPlayerNoShadow(e->mPlayer);
	}
}

static void updateSingleProjectile(void* tCaller, void* tData) {
	(void)tCaller;
	Projectile* e = (Projectile*)tData;
	if (e->mShouldBeRemoved) {
		removeProjectile(e->mPlayer);
		return;
	}
	const auto timeDilationUpdates = getPlayerTimeDilationUpdates(e->mPlayer);
	for (int currentUpdate = 0; currentUpdate < timeDilationUpdates; currentUpdate++) {
		updateProjectilePhysics(e);
		updateProjectileMissTime(e);
		updateProjectileShadow(e);
		const auto parent = getPlayerParent(e->mPlayer);
		if (isGeneralPlayer(parent) && getPlayerDoesScaleProjectiles(parent)) {
			setPlayerTempScaleActive(e->mPlayer, e->mScale);
		}
		if (updateProjectileDurationAndReturnIfOver(e)) return;
		if (updateProjectileBoundsAndReturnIfOver(e)) return;
	}
}

static void updateProjectileHandler(void* tData) {
	(void)tData;
	setProfilingSectionMarkerCurrentFunction();
	int_map_map(&gProjectileData.mProjectileList, updateSingleProjectile, NULL);
}

ActorBlueprint getProjectileHandler() {
	return makeActorBlueprint(loadProjectileHandler, unloadProjectileHandler, updateProjectileHandler);
};

void addAdditionalProjectileData(DreamPlayer* tProjectile) {
	Projectile* e = (Projectile*)allocMemory(sizeof(Projectile));
	e->mNow = 1;
	e->mHasChangedAnimationFinal = 0;
	e->mShouldBeRemoved = 0;
	e->mPlayer = tProjectile;
	tProjectile->mProjectileDataID = int_map_push_back_owned(&gProjectileData.mProjectileList, e);
}

void removeAdditionalProjectileData(DreamPlayer* tProjectile) {
	if (!int_map_contains(&gProjectileData.mProjectileList, tProjectile->mProjectileDataID)) {
		logWarningFormat("Error trying to remove projectile data for player %d %d who has no projectile data.", tProjectile->mRootID, tProjectile->mID);
		return;
	}
	int_map_remove(&gProjectileData.mProjectileList, tProjectile->mProjectileDataID);
}

void handleProjectileHit(DreamPlayer* tProjectile, int tWasGuarded, int tWasCanceled)
{
	assert(int_map_contains(&gProjectileData.mProjectileList, tProjectile->mProjectileDataID));
	Projectile* e = (Projectile*)int_map_get(&gProjectileData.mProjectileList, tProjectile->mProjectileDataID);
	e->mMissHitNow = 0;

	DreamPlayer* owner = tProjectile->mParent;
	owner->mHasLastContactProjectile = 1;
	owner->mLastContactProjectileTime = 0;
	owner->mLastContactProjectileID = e->mID;
	owner->mLastContactProjectileWasCanceled = tWasCanceled;
	owner->mLastContactProjectileWasGuarded = !tWasCanceled && tWasGuarded;
	owner->mLastContactProjectileWasHit = !tWasCanceled && !tWasGuarded;

	e->mHitAmountBeforeVanishing--;
	if (e->mHitAmountBeforeVanishing > 0) {
		setHitDataActive(tProjectile);
	}

	const auto shouldProjectileBeRemoved = e->mRemoveAfterHit && (e->mHitAmountBeforeVanishing <= 0);
	const auto nextAnimation = tWasCanceled ? e->mCancelAnimation : e->mHitAnimation;
	changeProjectileAnimation(e, nextAnimation, shouldProjectileBeRemoved);
}

void setProjectileID(DreamPlayer * tProjectile, int tID)
{
	assert(int_map_contains(&gProjectileData.mProjectileList, tProjectile->mProjectileDataID));
	Projectile* e = (Projectile*)int_map_get(&gProjectileData.mProjectileList, tProjectile->mProjectileDataID);
	e->mID = tID;
}

int getProjectileID(DreamPlayer * tProjectile)
{
	assert(int_map_contains(&gProjectileData.mProjectileList, tProjectile->mProjectileDataID));
	Projectile* e = (Projectile*)int_map_get(&gProjectileData.mProjectileList, tProjectile->mProjectileDataID);
	return e->mID;
}

void setProjectileAnimation(DreamPlayer* p, int tAnimation)
{
	changePlayerAnimation(p, tAnimation);
}

int getProjectileHitAnimation(DreamPlayer* p)
{
	assert(int_map_contains(&gProjectileData.mProjectileList, p->mProjectileDataID));
	Projectile* e = (Projectile*)int_map_get(&gProjectileData.mProjectileList, p->mProjectileDataID);
	return e->mHitAnimation;
}

void setProjectileHitAnimation(DreamPlayer* p, int tAnimation)
{
	assert(int_map_contains(&gProjectileData.mProjectileList, p->mProjectileDataID));
	Projectile* e = (Projectile*)int_map_get(&gProjectileData.mProjectileList, p->mProjectileDataID);
	e->mHitAnimation = tAnimation;
}

int getProjectileRemoveAnimation(DreamPlayer* p)
{
	assert(int_map_contains(&gProjectileData.mProjectileList, p->mProjectileDataID));
	Projectile* e = (Projectile*)int_map_get(&gProjectileData.mProjectileList, p->mProjectileDataID);
	return e->mRemoveAnimation;
}

void setProjectileRemoveAnimation(DreamPlayer* p, int tAnimation)
{
	assert(int_map_contains(&gProjectileData.mProjectileList, p->mProjectileDataID));
	Projectile* e = (Projectile*)int_map_get(&gProjectileData.mProjectileList, p->mProjectileDataID);
	e->mRemoveAnimation = tAnimation;
}

void setProjectileCancelAnimation(DreamPlayer* p, int tAnimation)
{
	assert(int_map_contains(&gProjectileData.mProjectileList, p->mProjectileDataID));
	Projectile* e = (Projectile*)int_map_get(&gProjectileData.mProjectileList, p->mProjectileDataID);
	e->mCancelAnimation = tAnimation;
}

void setProjectileScale(DreamPlayer* p, double tX, double tY)
{
	assert(int_map_contains(&gProjectileData.mProjectileList, p->mProjectileDataID));
	Projectile* e = (Projectile*)int_map_get(&gProjectileData.mProjectileList, p->mProjectileDataID);
	e->mScale = Vector2D(tX, tY);
}

void setProjectileRemoveAfterHit(DreamPlayer* p, int tValue)
{
	assert(int_map_contains(&gProjectileData.mProjectileList, p->mProjectileDataID));
	Projectile* e = (Projectile*)int_map_get(&gProjectileData.mProjectileList, p->mProjectileDataID);
	e->mRemoveAfterHit = tValue;
}

void setProjectileRemoveTime(DreamPlayer* p, int tTime)
{
	assert(int_map_contains(&gProjectileData.mProjectileList, p->mProjectileDataID));
	Projectile* e = (Projectile*)int_map_get(&gProjectileData.mProjectileList, p->mProjectileDataID);
	e->mRemoveTime = tTime;
}

void setProjectileVelocity(DreamPlayer* p, double tX, double tY, int tCoordinateP)
{
	setPlayerVelocityX(p, tX, tCoordinateP);
	setPlayerVelocityY(p, tY, tCoordinateP);
}

void setProjectileRemoveVelocity(DreamPlayer* p, double tX, double tY, int tCoordinateP)
{
	assert(int_map_contains(&gProjectileData.mProjectileList, p->mProjectileDataID));
	Projectile* e = (Projectile*)int_map_get(&gProjectileData.mProjectileList, p->mProjectileDataID);
	e->mRemoveVelocity = transformDreamCoordinatesVector(Vector3D(tX, tY, 0), tCoordinateP, getDreamMugenStageHandlerCameraCoordinateP());
}

void setProjectileAcceleration(DreamPlayer* p, double tX, double tY, int tCoordinateP)
{
	assert(int_map_contains(&gProjectileData.mProjectileList, p->mProjectileDataID));
	Projectile* e = (Projectile*)int_map_get(&gProjectileData.mProjectileList, p->mProjectileDataID);
	e->mAcceleration = transformDreamCoordinatesVector(Vector3D(tX, tY, 0), tCoordinateP, getDreamMugenStageHandlerCameraCoordinateP());
}

void setProjectileVelocityMultipliers(DreamPlayer* p, double tX, double tY)
{
	assert(int_map_contains(&gProjectileData.mProjectileList, p->mProjectileDataID));
	Projectile* e = (Projectile*)int_map_get(&gProjectileData.mProjectileList, p->mProjectileDataID);
	e->mVelocityMultipliers = Vector3D(tX, tY, 1);
}

void setProjectileHitAmountBeforeVanishing(DreamPlayer* p, int tHitAmount)
{
	assert(int_map_contains(&gProjectileData.mProjectileList, p->mProjectileDataID));
	Projectile* e = (Projectile*)int_map_get(&gProjectileData.mProjectileList, p->mProjectileDataID);
	e->mHitAmountBeforeVanishing = tHitAmount;
}

void setProjectilMisstime(DreamPlayer* p, int tMissTime)
{
	assert(int_map_contains(&gProjectileData.mProjectileList, p->mProjectileDataID));
	Projectile* e = (Projectile*)int_map_get(&gProjectileData.mProjectileList, p->mProjectileDataID);
	e->mMissTime = tMissTime;
	e->mMissHitNow = e->mMissTime + 1;
}

int getProjectilePriority(DreamPlayer* p)
{
	assert(int_map_contains(&gProjectileData.mProjectileList, p->mProjectileDataID));
	Projectile* e = (Projectile*)int_map_get(&gProjectileData.mProjectileList, p->mProjectileDataID);
	return e->mPriority;
}

void setProjectilePriority(DreamPlayer* p, int tPriority)
{
	assert(int_map_contains(&gProjectileData.mProjectileList, p->mProjectileDataID));
	Projectile* e = (Projectile*)int_map_get(&gProjectileData.mProjectileList, p->mProjectileDataID);
	e->mPriority = tPriority;
}

void reduceProjectilePriorityAndResetHitData(DreamPlayer* p)
{
	assert(int_map_contains(&gProjectileData.mProjectileList, p->mProjectileDataID));
	Projectile* e = (Projectile*)int_map_get(&gProjectileData.mProjectileList, p->mProjectileDataID);
	e->mPriority--;
	setHitDataActive(p);
}

void setProjectileSpritePriority(DreamPlayer* p, int tSpritePriority)
{
	setPlayerSpritePriority(p, tSpritePriority);
}

void setProjectileEdgeBound(DreamPlayer* p, int tEdgeBound, int tCoordinateP)
{
	assert(int_map_contains(&gProjectileData.mProjectileList, p->mProjectileDataID));
	Projectile* e = (Projectile*)int_map_get(&gProjectileData.mProjectileList, p->mProjectileDataID);
	e->mEdgeBound = transformDreamCoordinatesI(tEdgeBound, tCoordinateP, getDreamMugenStageHandlerCameraCoordinateP());
}

void setProjectileStageBound(DreamPlayer* p, int tStageBound, int tCoordinateP)
{
	assert(int_map_contains(&gProjectileData.mProjectileList, p->mProjectileDataID));
	Projectile* e = (Projectile*)int_map_get(&gProjectileData.mProjectileList, p->mProjectileDataID);
	e->mStageBound = transformDreamCoordinatesI(tStageBound, tCoordinateP, getDreamMugenStageHandlerCameraCoordinateP());
}

void setProjectileHeightBoundValues(DreamPlayer* p, int tLowerBound, int tUpperBound, int tCoordinateP)
{
	assert(int_map_contains(&gProjectileData.mProjectileList, p->mProjectileDataID));
	Projectile* e = (Projectile*)int_map_get(&gProjectileData.mProjectileList, p->mProjectileDataID);
	e->mLowerBound = transformDreamCoordinatesI(tLowerBound, tCoordinateP, getDreamMugenStageHandlerCameraCoordinateP());
	e->mUpperBound = transformDreamCoordinatesI(tUpperBound, tCoordinateP, getDreamMugenStageHandlerCameraCoordinateP());
}

void setProjectilePosition(DreamPlayer* p, const Position2D& tPosition, int tCoordinateP)
{
	setPlayerPosition(p, tPosition, tCoordinateP);
}

void setProjectileShadow(DreamPlayer* p, int tShadow)
{
	assert(int_map_contains(&gProjectileData.mProjectileList, p->mProjectileDataID));
	Projectile* e = (Projectile*)int_map_get(&gProjectileData.mProjectileList, p->mProjectileDataID);
	e->mShadow = tShadow;
	updateProjectileShadow(e);
}

void setProjectileSuperMoveTime(DreamPlayer* p, int tSuperMoveTime)
{
	assert(int_map_contains(&gProjectileData.mProjectileList, p->mProjectileDataID));
	Projectile* e = (Projectile*)int_map_get(&gProjectileData.mProjectileList, p->mProjectileDataID);
	setPlayerSuperMoveTime(e->mPlayer, tSuperMoveTime);
}

void setProjectilePauseMoveTime(DreamPlayer* p, int tPauseMoveTime)
{
	assert(int_map_contains(&gProjectileData.mProjectileList, p->mProjectileDataID));
	Projectile* e = (Projectile*)int_map_get(&gProjectileData.mProjectileList, p->mProjectileDataID);
	setPlayerPauseMoveTime(e->mPlayer, tPauseMoveTime);
}

void setProjectileHasOwnPalette(DreamPlayer* p, int tValue)
{
	assert(int_map_contains(&gProjectileData.mProjectileList, p->mProjectileDataID));
	setPlayerHasOwnPalette(p, tValue);
}

void setProjectileRemapPalette(DreamPlayer* p, int tGroup, int tItem)
{
	assert(int_map_contains(&gProjectileData.mProjectileList, p->mProjectileDataID));
	Projectile* e = (Projectile*)int_map_get(&gProjectileData.mProjectileList, p->mProjectileDataID);
	e->mRemapPaletteGroup = tGroup; // not used in Dolmexica Infinite
	e->mRemapPaletteItem = tItem;
}

int canProjectileHit(DreamPlayer* p)
{
	assert(int_map_contains(&gProjectileData.mProjectileList, p->mProjectileDataID));
	Projectile* e = (Projectile*)int_map_get(&gProjectileData.mProjectileList, p->mProjectileDataID);
	return e->mMissHitNow >= e->mMissTime;
}
