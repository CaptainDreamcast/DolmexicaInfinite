#include "projectile.h"

#include <assert.h>
#include <algorithm>

#include <prism/datastructures.h>
#include <prism/math.h>
#include <prism/log.h>

#include "stage.h"

using namespace std;

typedef struct {
	DreamPlayer* mPlayer;

	int mID;
	int mHitAnimation;
	int mRemoveAnimation;
	int mCancelAnimation;

	Vector3D mScale;
	int mRemoveAfterHit;
	int mRemoveTime;

	Vector3D mRemoveVelocity;
	Vector3D mAcceleration;
	Vector3D mVelocityMultipliers;

	int mHitAmountBeforeVanishing;
	int mMissTime;
	int mPriority;
	int mSpritePriority;
	int mEdgeBound;
	int mStageBound;
	int mLowerBound;
	int mUpperBound;
	int mShadow;
	int mSuperMoveTime;
	int mPauseMoveTime;
	int mHasOwnPalette;
	
	int mRemapPaletteGroup;
	int mRemapPaletteItem;

	int mAfterImageTime;
	int mAfterImageLength;
	int mAfterImage;

	int mNow;
} Projectile;

static struct {
	IntMap mProjectileList;
} gData;

static void loadProjectileHandler(void* tData) {
	(void)tData;
	gData.mProjectileList = new_int_map();
}

static void unloadProjectileHandler(void* tData) {
	(void)tData;
	delete_int_map(&gData.mProjectileList);
}

static int updateProjectileDurationAndReturnIfOver(Projectile* e) {
	if (e->mRemoveTime == -1) return 0;

	e->mNow++;
	if (e->mNow >= e->mRemoveTime) {
		removeProjectile(e->mPlayer);
		return 1;
	}

	return 0;
}

static int updateProjectileScreenBoundAndReturnIfOver(Projectile* e) {
	double left = getDreamStageLeftOfScreenBasedOnPlayer(getPlayerCoordinateP(e->mPlayer));
	double right = getDreamStageRightOfScreenBasedOnPlayer(getPlayerCoordinateP(e->mPlayer));

	double x = getPlayerPositionX(e->mPlayer, getPlayerCoordinateP(e->mPlayer));

	double overShootLeft = left - x;
	double overShootRight = x - right;
	double maxi = max(overShootLeft, overShootRight);
	
	if (maxi > e->mEdgeBound) {
		removeProjectile(e->mPlayer);
		return 1;
	}

	return 0;
}

static void updateProjectilePhysics(Projectile* e) {
	addPlayerVelocityX(e->mPlayer, e->mAcceleration.x, getPlayerCoordinateP(e->mPlayer));
	addPlayerVelocityY(e->mPlayer, e->mAcceleration.y, getPlayerCoordinateP(e->mPlayer));
}


static void updateSingleProjectile(void* tCaller, void* tData) {
	(void)tCaller;
	Projectile* e = (Projectile*)tData;

	updateProjectilePhysics(e);
	if (updateProjectileDurationAndReturnIfOver(e)) return;
	if (updateProjectileScreenBoundAndReturnIfOver(e)) return;
}

static void updateProjectileHandler(void* tData) {
	(void)tData;
	int_map_map(&gData.mProjectileList, updateSingleProjectile, NULL);
}

ActorBlueprint getProjectileHandler() {
	return makeActorBlueprint(loadProjectileHandler, unloadProjectileHandler, updateProjectileHandler);
};

void addAdditionalProjectileData(DreamPlayer* tProjectile) {
	Projectile* e = (Projectile*)allocMemory(sizeof(Projectile));
	e->mNow = 0;
	e->mPlayer = tProjectile;
	tProjectile->mProjectileDataID = int_map_push_back_owned(&gData.mProjectileList, e);
}

void removeAdditionalProjectileData(DreamPlayer* tProjectile) {
	if (!int_map_contains(&gData.mProjectileList, tProjectile->mProjectileDataID)) {
		logWarningFormat("Error trying to remove projectile data for player %d %d who has no projectile data.", tProjectile->mRootID, tProjectile->mID);
		return;
	}
	int_map_remove(&gData.mProjectileList, tProjectile->mProjectileDataID);
}

static void projectileHitAnimationFinishedCB(void* tCaller) {
	Projectile* e = (Projectile*)tCaller;
	e->mNow = 0;
	e->mRemoveTime = 0;
}


void handleProjectileHit(DreamPlayer * tProjectile)
{
	assert(int_map_contains(&gData.mProjectileList, tProjectile->mProjectileDataID));
	Projectile* e = (Projectile*)int_map_get(&gData.mProjectileList, tProjectile->mProjectileDataID);


	if (e->mHitAnimation != -1) {
		changePlayerAnimation(tProjectile, e->mHitAnimation);
		if (e->mRemoveAfterHit) {
			setPlayerAnimationFinishedCallback(tProjectile, projectileHitAnimationFinishedCB, e);
			setProjectileVelocity(tProjectile, 0, 0);
		}
	} else if (e->mRemoveAfterHit) {
		removeProjectile(tProjectile);
	}
}

void setProjectileID(DreamPlayer * tProjectile, int tID)
{
	assert(int_map_contains(&gData.mProjectileList, tProjectile->mProjectileDataID));
	Projectile* e = (Projectile*)int_map_get(&gData.mProjectileList, tProjectile->mProjectileDataID);
	e->mID = tID;
}

void setProjectileAnimation(DreamPlayer * p, int tAnimation)
{
	changePlayerAnimation(p, tAnimation);
}

int getProjectileHitAnimation(DreamPlayer * p)
{
	assert(int_map_contains(&gData.mProjectileList, p->mProjectileDataID));
	Projectile* e = (Projectile*)int_map_get(&gData.mProjectileList, p->mProjectileDataID);
	return e->mHitAnimation;
}

void setProjectileHitAnimation(DreamPlayer * p, int tAnimation)
{
	assert(int_map_contains(&gData.mProjectileList, p->mProjectileDataID));
	Projectile* e = (Projectile*)int_map_get(&gData.mProjectileList, p->mProjectileDataID);
	e->mHitAnimation = tAnimation;
}

int getProjectileRemoveAnimation(DreamPlayer * p)
{
	assert(int_map_contains(&gData.mProjectileList, p->mProjectileDataID));
	Projectile* e = (Projectile*)int_map_get(&gData.mProjectileList, p->mProjectileDataID);
	return e->mRemoveAnimation;
}

void setProjectileRemoveAnimation(DreamPlayer * p, int tAnimation)
{
	assert(int_map_contains(&gData.mProjectileList, p->mProjectileDataID));
	Projectile* e = (Projectile*)int_map_get(&gData.mProjectileList, p->mProjectileDataID);
	e->mRemoveAnimation = tAnimation;
}

void setProjectileCancelAnimation(DreamPlayer * p, int tAnimation)
{
	assert(int_map_contains(&gData.mProjectileList, p->mProjectileDataID));
	Projectile* e = (Projectile*)int_map_get(&gData.mProjectileList, p->mProjectileDataID);
	e->mCancelAnimation = tAnimation;
}

void setProjectileScale(DreamPlayer * p, double tX, double tY)
{
	assert(int_map_contains(&gData.mProjectileList, p->mProjectileDataID));
	Projectile* e = (Projectile*)int_map_get(&gData.mProjectileList, p->mProjectileDataID);
	e->mScale = makePosition(tX, tY, 1);
}

void setProjectileRemoveAfterHit(DreamPlayer * p, int tValue)
{
	assert(int_map_contains(&gData.mProjectileList, p->mProjectileDataID));
	Projectile* e = (Projectile*)int_map_get(&gData.mProjectileList, p->mProjectileDataID);
	e->mRemoveAfterHit = tValue;
}

void setProjectileRemoveTime(DreamPlayer * p, int tTime)
{
	assert(int_map_contains(&gData.mProjectileList, p->mProjectileDataID));
	Projectile* e = (Projectile*)int_map_get(&gData.mProjectileList, p->mProjectileDataID);
	e->mRemoveTime = tTime;
}

void setProjectileVelocity(DreamPlayer * p, double tX, double tY)
{
	setPlayerVelocityX(p, tX, getPlayerCoordinateP(p));
	setPlayerVelocityY(p, tY, getPlayerCoordinateP(p));
}

void setProjectileRemoveVelocity(DreamPlayer * p, double tX, double tY)
{
	assert(int_map_contains(&gData.mProjectileList, p->mProjectileDataID));
	Projectile* e = (Projectile*)int_map_get(&gData.mProjectileList, p->mProjectileDataID);
	e->mRemoveVelocity = makePosition(tX, tY, 0);
}

void setProjectileAcceleration(DreamPlayer * p, double tX, double tY)
{
	assert(int_map_contains(&gData.mProjectileList, p->mProjectileDataID));
	Projectile* e = (Projectile*)int_map_get(&gData.mProjectileList, p->mProjectileDataID);
	e->mAcceleration = makePosition(tX, tY, 0);
}

void setProjectileVelocityMultipliers(DreamPlayer * p, double tX, double tY)
{
	assert(int_map_contains(&gData.mProjectileList, p->mProjectileDataID));
	Projectile* e = (Projectile*)int_map_get(&gData.mProjectileList, p->mProjectileDataID);
	e->mVelocityMultipliers = makePosition(tX, tY, 1);
}

void setProjectileHitAmountBeforeVanishing(DreamPlayer * p, int tHitAmount)
{
	assert(int_map_contains(&gData.mProjectileList, p->mProjectileDataID));
	Projectile* e = (Projectile*)int_map_get(&gData.mProjectileList, p->mProjectileDataID);
	e->mHitAmountBeforeVanishing = tHitAmount;
}

void setProjectilMisstime(DreamPlayer * p, int tMissTime)
{
	assert(int_map_contains(&gData.mProjectileList, p->mProjectileDataID));
	Projectile* e = (Projectile*)int_map_get(&gData.mProjectileList, p->mProjectileDataID);
	e->mMissTime = tMissTime;
}

void setProjectilePriority(DreamPlayer * p, int tPriority)
{
	assert(int_map_contains(&gData.mProjectileList, p->mProjectileDataID));
	Projectile* e = (Projectile*)int_map_get(&gData.mProjectileList, p->mProjectileDataID);
	e->mPriority = tPriority;
}

void setProjectileSpritePriority(DreamPlayer * p, int tSpritePriority)
{
	assert(int_map_contains(&gData.mProjectileList, p->mProjectileDataID));
	Projectile* e = (Projectile*)int_map_get(&gData.mProjectileList, p->mProjectileDataID);
	e->mSpritePriority = tSpritePriority;
}

void setProjectileEdgeBound(DreamPlayer * p, int tEdgeBound)
{
	assert(int_map_contains(&gData.mProjectileList, p->mProjectileDataID));
	Projectile* e = (Projectile*)int_map_get(&gData.mProjectileList, p->mProjectileDataID);
	e->mEdgeBound = tEdgeBound;
}

void setProjectileStageBound(DreamPlayer * p, int tStageBound)
{
	assert(int_map_contains(&gData.mProjectileList, p->mProjectileDataID));
	Projectile* e = (Projectile*)int_map_get(&gData.mProjectileList, p->mProjectileDataID);
	e->mStageBound = tStageBound;
}

void setProjectileHeightBoundValues(DreamPlayer * p, int tLowerBound, int tUpperBound)
{
	assert(int_map_contains(&gData.mProjectileList, p->mProjectileDataID));
	Projectile* e = (Projectile*)int_map_get(&gData.mProjectileList, p->mProjectileDataID);
	e->mLowerBound = tLowerBound;
	e->mUpperBound = tUpperBound;
}

void setProjectilePosition(DreamPlayer * p, Position tPosition)
{
	setPlayerPosition(p, tPosition, getPlayerCoordinateP(p));
}

void setProjectileShadow(DreamPlayer * p, int tShadow)
{
	assert(int_map_contains(&gData.mProjectileList, p->mProjectileDataID));
	Projectile* e = (Projectile*)int_map_get(&gData.mProjectileList, p->mProjectileDataID);
	e->mShadow = tShadow;
}

void setProjectileSuperMoveTime(DreamPlayer * p, int tSuperMoveTime)
{
	assert(int_map_contains(&gData.mProjectileList, p->mProjectileDataID));
	Projectile* e = (Projectile*)int_map_get(&gData.mProjectileList, p->mProjectileDataID);
	e->mSuperMoveTime = tSuperMoveTime;
}

void setProjectilePauseMoveTime(DreamPlayer * p, int tPauseMoveTime)
{
	assert(int_map_contains(&gData.mProjectileList, p->mProjectileDataID));
	Projectile* e = (Projectile*)int_map_get(&gData.mProjectileList, p->mProjectileDataID);
	e->mPauseMoveTime = tPauseMoveTime;
}

void setProjectileHasOwnPalette(DreamPlayer * p, int tValue)
{
	assert(int_map_contains(&gData.mProjectileList, p->mProjectileDataID));
	Projectile* e = (Projectile*)int_map_get(&gData.mProjectileList, p->mProjectileDataID);
	e->mHasOwnPalette = tValue;
}

void setProjectileRemapPalette(DreamPlayer * p, int tGroup, int tItem)
{
	assert(int_map_contains(&gData.mProjectileList, p->mProjectileDataID));
	Projectile* e = (Projectile*)int_map_get(&gData.mProjectileList, p->mProjectileDataID);
	e->mRemapPaletteGroup = tGroup;
	e->mRemapPaletteItem = tItem;
}

void setProjectileAfterImageTime(DreamPlayer * p, int tAfterImageTime)
{
	assert(int_map_contains(&gData.mProjectileList, p->mProjectileDataID));
	Projectile* e = (Projectile*)int_map_get(&gData.mProjectileList, p->mProjectileDataID);
	e->mAfterImageTime = tAfterImageTime;
}

void setProjectileAfterImageLength(DreamPlayer * p, int tAfterImageLength)
{
	assert(int_map_contains(&gData.mProjectileList, p->mProjectileDataID));
	Projectile* e = (Projectile*)int_map_get(&gData.mProjectileList, p->mProjectileDataID);
	e->mAfterImageLength = tAfterImageLength;
}

void setProjectileAfterImage(DreamPlayer * p, int tAfterImage)
{
	assert(int_map_contains(&gData.mProjectileList, p->mProjectileDataID));
	Projectile* e = (Projectile*)int_map_get(&gData.mProjectileList, p->mProjectileDataID);
	e->mAfterImage = tAfterImage;
}
