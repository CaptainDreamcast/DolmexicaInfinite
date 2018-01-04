#include "mugenexplod.h"

#include <tari/geometry.h>
#include <tari/physics.h>
#include <tari/datastructures.h>
#include <tari/memoryhandler.h>

typedef struct {
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
} Explod;


static struct {
	List mExplods;
} gData;

static void loadExplods(void* tData) {
	(void)tData;
	gData.mExplods = new_list();
}

int addExplod(DreamPlayer* tPlayer)
{
	Explod* e = allocMemory(sizeof(Explod));
	e->mPlayer = tPlayer;

	return list_push_back_owned(&gData.mExplods, e);
}

void setExplodAnimation(int tID, int tIsInFightDefFile, int tAnimationNumber)
{
	Explod* e = list_get(&gData.mExplods, tID);
	e->mIsInFightDefFile = tIsInFightDefFile;
	e->mAnimationNumber = tAnimationNumber;
}

void setExplodID(int tID, int tExternalID)
{
	Explod* e = list_get(&gData.mExplods, tID);
	e->mExternalID = tExternalID;
}

void setExplodPosition(int tID, int tOffsetX, int tOffsetY)
{
	Explod* e = list_get(&gData.mExplods, tID);
	e->mPosition = makePosition(tOffsetX, tOffsetY, 0);
}

void setExplodPositionType(int tID, DreamExplodPositionType tType)
{
	Explod* e = list_get(&gData.mExplods, tID);
	e->mPositionType = tType;
}

void setExplodHorizontalFacing(int tID, int tFacing)
{
	Explod* e = list_get(&gData.mExplods, tID);
	if(e->mPosition.x >= 0) e->mIsFlippedHorizontally = tFacing == -1;
	else e->mIsFlippedHorizontally = tFacing == 1;
}

void setExplodVerticalFacing(int tID, int tFacing)
{
	Explod* e = list_get(&gData.mExplods, tID);
	e->mIsFlippedVertically = tFacing == -1;
}

void setExplodBindTime(int tID, int tBindTime)
{
	Explod* e = list_get(&gData.mExplods, tID);
	e->mBindTime = tBindTime;
}

void setExplodVelocity(int tID, double tX, double tY)
{
	Explod* e = list_get(&gData.mExplods, tID);
	e->mVelocity = makePosition(tX, tY, 0);
}

void setExplodAcceleration(int tID, double tX, double tY)
{
	Explod* e = list_get(&gData.mExplods, tID);
	e->mAcceleration = makePosition(tX, tY, 0);
}

void setExplodRandomOffset(int tID, int tX, int tY)
{
	Explod* e = list_get(&gData.mExplods, tID);
	e->mRandomOffset = makeVector3DI(tX, tY, 0);
}

void setExplodRemoveTime(int tID, int tRemoveTime)
{
	Explod* e = list_get(&gData.mExplods, tID);
	e->mRemoveTime = tRemoveTime;
}

void setExplodSuperMove(int tID, int tIsSuperMove)
{
	Explod* e = list_get(&gData.mExplods, tID);
	e->mIsSuperMove = tIsSuperMove;
}

void setExplodSuperMoveTime(int tID, int tSuperMoveTime)
{
	Explod* e = list_get(&gData.mExplods, tID);
	e->mSuperMoveTime = tSuperMoveTime;
}

void setExplodPauseMoveTime(int tID, int tPauseMoveTime)
{
	Explod* e = list_get(&gData.mExplods, tID);
	e->mPauseMoveTime = tPauseMoveTime;
}

void setExplodScale(int tID, double tX, double tY)
{
	Explod* e = list_get(&gData.mExplods, tID);
	e->mScale = makePosition(tX, tY, 0);
}

void setExplodSpritePriority(int tID, int tSpritePriority)
{
	Explod* e = list_get(&gData.mExplods, tID);
	e->mSpritePriority = tSpritePriority;
}

void setExplodOnTop(int tID, int tIsOnTop)
{
	Explod* e = list_get(&gData.mExplods, tID);
	e->mIsOnTop = tIsOnTop;
}

void setExplodShadow(int tID, int tR, int tG, int tB)
{
	Explod* e = list_get(&gData.mExplods, tID);
	e->mShadow = makeVector3DI(tR, tG, tB);
}

void setExplodOwnPalette(int tID, int tUsesOwnPalette)
{
	Explod* e = list_get(&gData.mExplods, tID);
	e->mUsesOwnPalette = tUsesOwnPalette;
}

void setExplodRemoveOnGetHit(int tID, int tIsRemovedOnGetHit)
{
	Explod* e = list_get(&gData.mExplods, tID);
	e->mIsRemovedOnGetHit = tIsRemovedOnGetHit;
}

void setExplodIgnoreHitPause(int tID, int tIgnoreHitPause)
{
	Explod* e = list_get(&gData.mExplods, tID);
	e->mIgnoreHitPause = tIgnoreHitPause;
}

void setExplodTransparencyType(int tID, int tHasTransparencyType, DreamExplodTransparencyType tTransparencyType)
{
	Explod* e = list_get(&gData.mExplods, tID);
	e->mHasTransparencyType = tHasTransparencyType;
	e->mTransparencyType = tTransparencyType;
}

int getExplodAmount(DreamPlayer * tPlayer)
{
	(void)tPlayer;
	return 0; // TODO
}

int getExplodAmountWithID(DreamPlayer * tPlayer, int tID)
{
	(void)tPlayer;
	(void)tID;
	return 0; // TODO
}


ActorBlueprint DreamExplodHandler = {
	.mLoad = loadExplods,
};
