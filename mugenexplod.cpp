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
#include <prism/math.h>

#include "fightui.h"
#include "stage.h"
#include "mugenstagehandler.h"
#include "pausecontrollers.h"
#include "config.h"
#include "mugenanimationutilities.h"
#include "mugenstatehandler.h"

#define EXPLOD_SHADOW_Z 32

using namespace std;

typedef struct {
	int mInternalID;
	DreamPlayer* mPlayer;

	int mIsInFightDefFile;
	int mAnimationNumber;

	int mExternalID;
	DreamExplodSpace mSpace;
	Position2D mPosition;
	DreamExplodPositionType mPositionType;

	int mIsFlippedHorizontallyAtStart;
	int mIsFlippedHorizontally;
	int mIsFlippedVertically;

	int mBindNow;
	int mBindTime;

	Velocity mVelocity;
	Acceleration mAcceleration;

	Vector2DI mRandomOffset;
	int mRemoveTime = -2;

	int mSuperMoveTime;
	int mPauseMoveTime;

	Vector2D mScale;
	int mSpritePriority;
	int mIsOnTop;

	Vector3D mShadowColor;

	int mUsesOwnPalette;
	int mIsRemovedOnGetHit;

	int mIgnoreHitPause;

	int mHasTransparencyType;
	DreamExplodTransparencyType mTransparencyType;

	PhysicsHandlerElement* mPhysicsElement;
	MugenAnimationHandlerElement* mAnimationElement;
	MugenAnimationHandlerElement* mShadowAnimationElement;

	double mTimeDilatationNow;
	double mTimeDilatation;
	int mNow;
} Explod;


static struct {
	map<int, Explod> mExplods;
} gMugenExplod;

static void loadExplods(void* tData) {
	(void)tData;
	setProfilingSectionMarkerCurrentFunction();
	gMugenExplod.mExplods.clear();
}

static void unloadExplods(void* tData) {
	(void)tData;
	setProfilingSectionMarkerCurrentFunction();
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

void setExplodSpace(int tID, DreamExplodSpace tSpace)
{
	Explod* e = &gMugenExplod.mExplods[tID];
	e->mSpace = tSpace;
}

void setExplodPosition(int tID, int tOffsetX, int tOffsetY)
{
	Explod* e = &gMugenExplod.mExplods[tID];
	e->mPosition = Vector2D(tOffsetX, tOffsetY);
	e->mPosition = transformDreamCoordinatesVector2D(e->mPosition, getActiveStateMachineCoordinateP(), getDreamMugenStageHandlerCameraCoordinateP());
}

void setExplodPositionType(int tID, DreamExplodPositionType tType)
{
	Explod* e = &gMugenExplod.mExplods[tID];
	e->mPositionType = tType;
}

void setExplodHorizontalFacing(int tID, int tFacing)
{
	Explod* e = &gMugenExplod.mExplods[tID];
	e->mIsFlippedHorizontallyAtStart = e->mIsFlippedHorizontally = tFacing == -1;
}

void setExplodVerticalFacing(int tID, int tFacing)
{
	Explod* e = &gMugenExplod.mExplods[tID];
	e->mIsFlippedVertically = tFacing == -1;
}

void setExplodBindTime(int tID, int tBindTime)
{
	Explod* e = &gMugenExplod.mExplods[tID];
	e->mBindNow = 0;
	e->mBindTime = tBindTime;
}

void setExplodVelocity(int tID, double tX, double tY)
{
	Explod* e = &gMugenExplod.mExplods[tID];
	e->mVelocity = Vector3D(tX, tY, 0);
	e->mVelocity = transformDreamCoordinatesVector(e->mVelocity, getActiveStateMachineCoordinateP(), getDreamMugenStageHandlerCameraCoordinateP());
}

void setExplodAcceleration(int tID, double tX, double tY)
{
	Explod* e = &gMugenExplod.mExplods[tID];
	e->mAcceleration = Vector3D(tX, tY, 0);
	e->mAcceleration = transformDreamCoordinatesVector(e->mAcceleration, getActiveStateMachineCoordinateP(), getDreamMugenStageHandlerCameraCoordinateP());
}

static int getExplodRandomRangeMin(int tX) {
	return -tX / 2;
}

static int getExplodRandomRangeMax(int tX) {
	return (tX / 2) - (tX % 2);
}

void setExplodRandomOffset(int tID, int tX, int tY)
{
	Explod* e = &gMugenExplod.mExplods[tID];
	e->mRandomOffset = Vector2DI(randfromInteger(getExplodRandomRangeMin(tX), getExplodRandomRangeMax(tX)), randfromInteger(getExplodRandomRangeMin(tY), getExplodRandomRangeMax(tY)));
	e->mRandomOffset = transformDreamCoordinatesVector2DI(e->mRandomOffset, getActiveStateMachineCoordinateP(), getDreamMugenStageHandlerCameraCoordinateP());
}

void setExplodRemoveTime(int tID, int tRemoveTime)
{
	Explod* e = &gMugenExplod.mExplods[tID];
	e->mRemoveTime = tRemoveTime;
}

void setExplodSuperMove(int tID, int tCanMoveDuringSuperMove)
{
	Explod* e = &gMugenExplod.mExplods[tID];
	e->mSuperMoveTime = INF * tCanMoveDuringSuperMove;
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
	e->mScale = Vector2D(tX, tY);
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

static void parseShadowStatus(Explod* e, int tR, int tG, int tB) {
	if (tR == -1 || (tR == 1 && !tG && !tB)) {
		e->mShadowColor = getDreamStageShadowColor();

	}
	else {
		e->mShadowColor = Vector3D(tR / 255.0, tG / 255.0, tB / 255.0);
	}
}

void setExplodShadow(int tID, int tR, int tG, int tB)
{
	Explod* e = &gMugenExplod.mExplods[tID];
	parseShadowStatus(e, tR, tG, tB);
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

static Position2D getFinalExplodPositionFromPositionType(DreamExplodPositionType tPositionType, Position2D mOffset, DreamPlayer* tPlayer) {
	if (tPositionType == EXPLOD_POSITION_TYPE_RELATIVE_TO_P1) {
		DreamPlayer* target = tPlayer;
		const auto p = getPlayerPosition(target, getDreamMugenStageHandlerCameraCoordinateP());
		int isReversed = !getPlayerIsFacingRight(target);
		if (isReversed) mOffset.x *= -1;
		return p + mOffset;
	}
	else if (tPositionType == EXPLOD_POSITION_TYPE_RELATIVE_TO_P2) {
		DreamPlayer* target = getPlayerOtherPlayer(tPlayer);
		const auto p = getPlayerPosition(target, getDreamMugenStageHandlerCameraCoordinateP());
		int isReversed = !getPlayerIsFacingRight(target);
		if (isReversed) mOffset.x *= -1;
		return p + mOffset;
	}
	else if (tPositionType == EXPLOD_POSITION_TYPE_RELATIVE_TO_FRONT) {
		DreamPlayer* target = tPlayer;
		const auto p = Vector2D(getPlayerScreenEdgeInFrontX(target, getDreamMugenStageHandlerCameraCoordinateP()), getDreamStageTopOfScreenBasedOnPlayerInStageCoordinateOffset(getDreamMugenStageHandlerCameraCoordinateP()));
		int isReversed = !getPlayerIsFacingRight(target);
		if (isReversed) mOffset.x *= -1;
		return p + mOffset;
	}
	else if (tPositionType == EXPLOD_POSITION_TYPE_RELATIVE_TO_BACK) {
		DreamPlayer* target = tPlayer;
		const auto p = Vector2D(getPlayerScreenEdgeInBackX(target, getDreamMugenStageHandlerCameraCoordinateP()), getDreamStageTopOfScreenBasedOnPlayerInStageCoordinateOffset(getDreamMugenStageHandlerCameraCoordinateP()));
		int isReversed = getPlayerIsFacingRight(target);
		if (isReversed) mOffset.x *= -1;
		return p + mOffset;
	}
	else if (tPositionType == EXPLOD_POSITION_TYPE_RELATIVE_TO_LEFT) {
		const auto p = Vector2D(getDreamStageLeftOfScreenBasedOnPlayer(getDreamMugenStageHandlerCameraCoordinateP()), getDreamStageTopOfScreenBasedOnPlayerInStageCoordinateOffset(getDreamMugenStageHandlerCameraCoordinateP()));
		return p + mOffset;
	}
	else if (tPositionType == EXPLOD_POSITION_TYPE_RELATIVE_TO_RIGHT) {
		const auto p = Vector2D(getDreamStageRightOfScreenBasedOnPlayer(getDreamMugenStageHandlerCameraCoordinateP()), getDreamStageTopOfScreenBasedOnPlayerInStageCoordinateOffset(getDreamMugenStageHandlerCameraCoordinateP()));
		return p + mOffset;
	}
	else if (tPositionType == EXPLOD_POSITION_TYPE_NONE) {
		const auto p = Vector2D(getDreamGameWidth(getDreamMugenStageHandlerCameraCoordinateP()) / 2, 0);
		return p + mOffset;
	}
	else {
		logWarningFormat("Unrecognized position type %d. Defaulting to EXPLOD_POSITION_TYPE_RELATIVE_TO_P1.", tPositionType);
		DreamPlayer* target = tPlayer;
		const auto p = getPlayerPosition(target, getDreamMugenStageHandlerCameraCoordinateP());
		int isReversed = !getPlayerIsFacingRight(target);
		if (isReversed) mOffset.x *= -1;
		return p + mOffset;
	}

}

static Position getExplodPosition(Explod* e) {
	Position2D p;
	switch (e->mSpace) {
	case EXPLOD_SPACE_NONE:
		p = getDreamStageCoordinateSystemOffset(getDreamMugenStageHandlerCameraCoordinateP()) + getFinalExplodPositionFromPositionType(e->mPositionType, e->mPosition, e->mPlayer);
		break;
	case EXPLOD_SPACE_STAGE:
		p = getDreamStageCoordinateSystemOffset(getDreamMugenStageHandlerCameraCoordinateP()) + getFinalExplodPositionFromPositionType(EXPLOD_POSITION_TYPE_NONE, e->mPosition, e->mPlayer);
		break;
	case EXPLOD_SPACE_SCREEN:
		p = e->mPosition;
		break;
	default:
		p = Vector2D(0.0, 0.0);
		logWarningFormat("Unrecognized explod space %d", int(e->mSpace));
		break;
	}
	p = p + e->mRandomOffset;
	return p.xyz(calculateSpriteZFromSpritePriority(e->mSpritePriority, e->mPlayer->mRootID, 1));
}

static void getExplodSpritesAnimationsScale(Explod* e, MugenSpriteFile** tSprites, MugenAnimation** tAnimation, double* tBaseScale) {
	if (e->mIsInFightDefFile) {
		*tSprites = getDreamFightEffectSprites();
		*tAnimation = getDreamFightEffectAnimation(e->mAnimationNumber);
		*tBaseScale = (getScreenSize().y / double(getDreamUICoordinateP())) * getDreamUIFightFXScale();
	}
	else {
		*tSprites = getPlayerSprites(e->mPlayer);
		*tAnimation = getPlayerAnimation(e->mPlayer, e->mAnimationNumber);
		*tBaseScale = getPlayerToCameraScale(e->mPlayer);
	}
}

static void updateExplodSpaceFinalization(Explod* e) {
	if (e->mSpace == EXPLOD_SPACE_NONE) {
		int isFacingRight;
		if (e->mPositionType == EXPLOD_POSITION_TYPE_RELATIVE_TO_P1) {
			isFacingRight = getPlayerIsFacingRight(e->mPlayer);
		}
		else if (e->mPositionType == EXPLOD_POSITION_TYPE_RELATIVE_TO_P2) {
			isFacingRight = getPlayerIsFacingRight(getPlayerOtherPlayer(e->mPlayer));
		}
		else {
			isFacingRight = 1;
		}
		if (!isFacingRight) {
			e->mVelocity.x = -e->mVelocity.x;
			e->mAcceleration.x = -e->mAcceleration.x;
			e->mIsFlippedHorizontally = !e->mIsFlippedHorizontallyAtStart;
		}
		else {
			e->mIsFlippedHorizontally = e->mIsFlippedHorizontallyAtStart;
		}
	}
	else {
		e->mIsFlippedHorizontally = e->mIsFlippedHorizontallyAtStart;
	}
}

static void updateExplodSpaceCamera(Explod* e) {
	if (e->mSpace != EXPLOD_SPACE_SCREEN) {
		setMugenAnimationCameraPositionReference(e->mAnimationElement, getDreamMugenStageHandlerCameraPositionReference());
		setMugenAnimationCameraEffectPositionReference(e->mAnimationElement, getDreamMugenStageHandlerCameraEffectPositionReference());
		setMugenAnimationCameraScaleReference(e->mAnimationElement, getDreamMugenStageHandlerCameraZoomReference());

		setMugenAnimationCameraPositionReference(e->mShadowAnimationElement, getDreamMugenStageHandlerCameraPositionReference());
		setMugenAnimationCameraEffectPositionReference(e->mShadowAnimationElement, getDreamMugenStageHandlerCameraEffectPositionReference());
		setMugenAnimationCameraScaleReference(e->mShadowAnimationElement, getDreamMugenStageHandlerCameraZoomReference());
	}
	else {
		removeMugenAnimationCameraPositionReference(e->mAnimationElement);
		removeMugenAnimationCameraEffectPositionReference(e->mAnimationElement);
		removeMugenAnimationCameraScaleReference(e->mAnimationElement);

		removeMugenAnimationCameraPositionReference(e->mShadowAnimationElement);
		removeMugenAnimationCameraEffectPositionReference(e->mShadowAnimationElement);
		removeMugenAnimationCameraScaleReference(e->mShadowAnimationElement);
	}
}

static void updateActiveExplodShadow(Explod* e) {
	if (!getMugenAnimationVisibility(e->mShadowAnimationElement)) return;
	if (!isDrawingShadowsConfig()) return;

	const auto stageOffset = getDreamStageCoordinateSystemOffset(getDreamMugenStageHandlerCameraCoordinateP());
	const auto physicsPosition = getHandledPhysicsPositionReference(e->mPhysicsElement)->y;
	const auto explodAnimationPosition = getMugenAnimationPosition(e->mAnimationElement);
	const auto noCameraPosY = physicsPosition + explodAnimationPosition.y;
	const auto screenPositionY = noCameraPosY - getDreamMugenStageHandlerCameraPositionReference()->y;
	auto deltaY = noCameraPosY - stageOffset.y;
	deltaY *= -getDreamStageShadowScaleY();
	deltaY += getPlayerShadowOffset(e->mPlayer, getDreamMugenStageHandlerCameraCoordinateP());
	const auto newShadowAnimationPosition = Vector3D(explodAnimationPosition.x, stageOffset.y + deltaY - physicsPosition, EXPLOD_SHADOW_Z);
	setMugenAnimationPosition(e->mShadowAnimationElement, newShadowAnimationPosition);
	const auto posY = -(getDreamScreenHeight(getDreamMugenStageHandlerCameraCoordinateP()) - screenPositionY);
	const auto t = getDreamStageShadowFadeRangeFactor(posY, getDreamMugenStageHandlerCameraCoordinateP());
	setMugenAnimationTransparency(e->mShadowAnimationElement, t*getDreamStageShadowTransparency());
}

static void updateExplodShadowColorAndVisibility(Explod* e) {
	if (!e->mShadowColor.x && !e->mShadowColor.y && !e->mShadowColor.z) {
		setMugenAnimationVisibility(e->mShadowAnimationElement, 0);
	}
	else {
		setMugenAnimationVisibility(e->mShadowAnimationElement, 1);
		if (isOnDreamcast()) {
			setMugenAnimationColor(e->mShadowAnimationElement, 0, 0, 0);
		}
		else {
			setMugenAnimationColorSolid(e->mShadowAnimationElement, e->mShadowColor.x, e->mShadowColor.x, e->mShadowColor.x);
		}
	}
}

void finalizeExplod(int tID)
{
	Explod* e = &gMugenExplod.mExplods[tID];

	MugenSpriteFile* sprites;
	MugenAnimation* animation;
	double baseScale;
	getExplodSpritesAnimationsScale(e, &sprites, &animation, &baseScale);

	updateExplodSpaceFinalization(e);

	e->mPhysicsElement = addToPhysicsHandler(Vector3D(0, 0, 0));
	addAccelerationToHandledPhysics(e->mPhysicsElement, e->mVelocity);

	const auto p = getExplodPosition(e);
	e->mAnimationElement = addMugenAnimation(animation, sprites, p);
	e->mShadowAnimationElement = addMugenAnimation(animation, sprites, Vector3D(0, 0, 0));
	setMugenAnimationBasePosition(e->mAnimationElement, getHandledPhysicsPositionReference(e->mPhysicsElement));
	setMugenAnimationBasePosition(e->mShadowAnimationElement, getHandledPhysicsPositionReference(e->mPhysicsElement));
	setMugenAnimationCallback(e->mAnimationElement, explodAnimationFinishedCB, e);
	setMugenAnimationFaceDirection(e->mAnimationElement, !e->mIsFlippedHorizontally);
	setMugenAnimationFaceDirection(e->mShadowAnimationElement, !e->mIsFlippedHorizontally);
	setMugenAnimationVerticalFaceDirection(e->mAnimationElement, !e->mIsFlippedVertically);
	setMugenAnimationVerticalFaceDirection(e->mShadowAnimationElement, !e->mIsFlippedVertically);
	setMugenAnimationDrawScale(e->mAnimationElement, e->mScale);
	setMugenAnimationDrawScale(e->mShadowAnimationElement, Vector2D(1, -getDreamStageShadowScaleY()) * e->mScale);
	setMugenAnimationBaseDrawScale(e->mAnimationElement, baseScale);
	setMugenAnimationBaseDrawScale(e->mShadowAnimationElement, baseScale);
	if (e->mHasTransparencyType) {
		setMugenAnimationBlendType(e->mAnimationElement, e->mTransparencyType == EXPLOD_TRANSPARENCY_TYPE_ADD_ALPHA ? BLEND_TYPE_ADDITION : BLEND_TYPE_NORMAL);
	}

	updateExplodShadowColorAndVisibility(e);
	setMugenAnimationTransparency(e->mShadowAnimationElement, getDreamStageShadowTransparency() * isDrawingShadowsConfig());

	updateExplodSpaceCamera(e);
	updateActiveExplodShadow(e);

	e->mTimeDilatationNow = 0.0;
	e->mTimeDilatation = 1.0;
	e->mNow = 0;
}

typedef struct {
	DreamPlayer* mPlayer;
	int mID;
	Vector3DI mValue;
} IntegerSetterForIDCaller;

typedef struct {
	DreamPlayer* mPlayer;
	int mID;
	Vector2D mValue;
} FloatSetterForIDCaller;

static void updateExplodAnimationForSingleExplod(IntegerSetterForIDCaller* tCaller, Explod& tData) {
	Explod* e = &tData;

	if (e->mPlayer != tCaller->mPlayer) return;
	if (tCaller->mID != -1 && e->mExternalID != tCaller->mID) return;

	e->mIsInFightDefFile = tCaller->mValue.x;
	e->mAnimationNumber = tCaller->mValue.y;

	MugenSpriteFile* sprites;
	MugenAnimation* animation;
	double baseScale;
	getExplodSpritesAnimationsScale(e, &sprites, &animation, &baseScale);

	setMugenAnimationSprites(e->mAnimationElement, sprites);
	changeMugenAnimation(e->mAnimationElement, animation);
	setMugenAnimationBaseDrawScale(e->mAnimationElement, baseScale);

	setMugenAnimationSprites(e->mShadowAnimationElement, sprites);
	changeMugenAnimation(e->mShadowAnimationElement, animation);
	setMugenAnimationBaseDrawScale(e->mShadowAnimationElement, baseScale);
}

void updateExplodAnimation(DreamPlayer* tPlayer, int tID, int tIsInFightDefFile, int tAnimationNumber)
{
	IntegerSetterForIDCaller caller;
	caller.mPlayer = tPlayer;
	caller.mID = tID;
	caller.mValue.x = tIsInFightDefFile;
	caller.mValue.y = tAnimationNumber;
	stl_int_map_map(gMugenExplod.mExplods, updateExplodAnimationForSingleExplod, &caller);
}

static void updateExplodPositionAfterUpdate(Explod* e) {
	const auto pos = getExplodPosition(e);
	setMugenAnimationPosition(e->mAnimationElement, pos);
	updateActiveExplodShadow(e);
}

static void flipExplodPhysicsHorizontal(Explod* e) {
	auto physicsObjectReference = getPhysicsFromHandler(e->mPhysicsElement);
	physicsObjectReference->mVelocity.x *= -1;
	physicsObjectReference->mAcceleration.x *= -1;
}

static void updateExplodSpaceForSingleExplod(IntegerSetterForIDCaller* tCaller, Explod& tData) {
	Explod* e = &tData;

	if (e->mPlayer != tCaller->mPlayer) return;
	if (tCaller->mID != -1 && e->mExternalID != tCaller->mID) return;
	const auto previousIsFlippedHorizontally = e->mIsFlippedHorizontally;
	e->mSpace = DreamExplodSpace(tCaller->mValue.x);
	updateExplodSpaceFinalization(e);
	updateExplodSpaceCamera(e);
	setMugenAnimationFaceDirection(e->mAnimationElement, !e->mIsFlippedHorizontally);
	setMugenAnimationFaceDirection(e->mShadowAnimationElement, !e->mIsFlippedHorizontally);
	if (previousIsFlippedHorizontally != e->mIsFlippedHorizontally) {
		flipExplodPhysicsHorizontal(e);
	}
	updateExplodPositionAfterUpdate(e);
}

void updateExplodSpace(DreamPlayer* tPlayer, int tID, DreamExplodSpace tSpace)
{
	IntegerSetterForIDCaller caller;
	caller.mPlayer = tPlayer;
	caller.mID = tID;
	caller.mValue.x = int(tSpace);
	stl_int_map_map(gMugenExplod.mExplods, updateExplodSpaceForSingleExplod, &caller);
}

static void updateExplodPositionForSingleExplod(IntegerSetterForIDCaller* tCaller, Explod& tData) {
	Explod* e = &tData;

	if (e->mPlayer != tCaller->mPlayer) return;
	if (tCaller->mID != -1 && e->mExternalID != tCaller->mID) return;

	e->mPosition = Vector2D(tCaller->mValue.x, tCaller->mValue.y);
	e->mPosition = transformDreamCoordinatesVector2D(e->mPosition, getActiveStateMachineCoordinateP(), getDreamMugenStageHandlerCameraCoordinateP());
	updateExplodPositionAfterUpdate(e);
}

void updateExplodPosition(DreamPlayer* tPlayer, int tID, int tOffsetX, int tOffsetY)
{
	IntegerSetterForIDCaller caller;
	caller.mPlayer = tPlayer;
	caller.mID = tID;
	caller.mValue.x = tOffsetX;
	caller.mValue.y = tOffsetY;
	stl_int_map_map(gMugenExplod.mExplods, updateExplodPositionForSingleExplod, &caller);
}

static void updateExplodPositionTypeForSingleExplod(IntegerSetterForIDCaller* tCaller, Explod& tData) {
	Explod* e = &tData;

	if (e->mPlayer != tCaller->mPlayer) return;
	if (tCaller->mID != -1 && e->mExternalID != tCaller->mID) return;

	e->mPositionType = DreamExplodPositionType(tCaller->mValue.x);
	updateExplodPositionAfterUpdate(e);
}

void updateExplodPositionType(DreamPlayer* tPlayer, int tID, DreamExplodPositionType tType)
{
	IntegerSetterForIDCaller caller;
	caller.mPlayer = tPlayer;
	caller.mID = tID;
	caller.mValue.x = int(tType);
	stl_int_map_map(gMugenExplod.mExplods, updateExplodPositionTypeForSingleExplod, &caller);
}

static void updateExplodHorizontalFacingForSingleExplod(IntegerSetterForIDCaller* tCaller, Explod& tData) {
	Explod* e = &tData;

	if (e->mPlayer != tCaller->mPlayer) return;
	if (tCaller->mID != -1 && e->mExternalID != tCaller->mID) return;

	e->mIsFlippedHorizontallyAtStart = e->mIsFlippedHorizontally = tCaller->mValue.x == -1;
	updateExplodSpaceFinalization(e);
	setMugenAnimationFaceDirection(e->mAnimationElement, !e->mIsFlippedHorizontally);
	setMugenAnimationFaceDirection(e->mShadowAnimationElement, !e->mIsFlippedHorizontally);
}

void updateExplodHorizontalFacing(DreamPlayer* tPlayer, int tID, int tFacing)
{
	IntegerSetterForIDCaller caller;
	caller.mPlayer = tPlayer;
	caller.mID = tID;
	caller.mValue.x = tFacing;
	stl_int_map_map(gMugenExplod.mExplods, updateExplodHorizontalFacingForSingleExplod, &caller);
}

static void updateExplodVerticalFacingForSingleExplod(IntegerSetterForIDCaller* tCaller, Explod& tData) {
	Explod* e = &tData;

	if (e->mPlayer != tCaller->mPlayer) return;
	if (tCaller->mID != -1 && e->mExternalID != tCaller->mID) return;

	e->mIsFlippedVertically = tCaller->mValue.x == -1;
	setMugenAnimationVerticalFaceDirection(e->mAnimationElement, !e->mIsFlippedVertically);
	setMugenAnimationVerticalFaceDirection(e->mShadowAnimationElement, !e->mIsFlippedVertically);
}

void updateExplodVerticalFacing(DreamPlayer* tPlayer, int tID, int tFacing)
{
	IntegerSetterForIDCaller caller;
	caller.mPlayer = tPlayer;
	caller.mID = tID;
	caller.mValue.x = tFacing;
	stl_int_map_map(gMugenExplod.mExplods, updateExplodVerticalFacingForSingleExplod, &caller);
}

static void updateExplodBindTimeForSingleExplod(IntegerSetterForIDCaller* tCaller, Explod& tData) {
	Explod* e = &tData;

	if (e->mPlayer != tCaller->mPlayer) return;
	if (tCaller->mID != -1 && e->mExternalID != tCaller->mID) return;

	e->mBindNow = 0;
	e->mBindTime = tCaller->mValue.x;
}

void updateExplodBindTime(DreamPlayer* tPlayer, int tID, int tBindTime)
{
	IntegerSetterForIDCaller caller;
	caller.mPlayer = tPlayer;
	caller.mID = tID;
	caller.mValue.x = tBindTime;
	stl_int_map_map(gMugenExplod.mExplods, updateExplodBindTimeForSingleExplod, &caller);
}

static void updateExplodVelocityForSingleExplod(FloatSetterForIDCaller* tCaller, Explod& tData) {
	Explod* e = &tData;

	if (e->mPlayer != tCaller->mPlayer) return;
	if (tCaller->mID != -1 && e->mExternalID != tCaller->mID) return;

	e->mVelocity.x = tCaller->mValue.x;
	e->mVelocity.y = tCaller->mValue.y;
	auto vel = getHandledPhysicsVelocityReference(e->mPhysicsElement);
	vel->x = e->mVelocity.x;
	vel->y = e->mVelocity.y;
	*vel = transformDreamCoordinatesVector(*vel, getActiveStateMachineCoordinateP(), getDreamMugenStageHandlerCameraCoordinateP());
}

void updateExplodVelocity(DreamPlayer* tPlayer, int tID, double tX, double tY)
{
	FloatSetterForIDCaller caller;
	caller.mPlayer = tPlayer;
	caller.mID = tID;
	caller.mValue.x = tX;
	caller.mValue.y = tY;
	stl_int_map_map(gMugenExplod.mExplods, updateExplodVelocityForSingleExplod, &caller);
}

static void updateExplodAccelerationForSingleExplod(FloatSetterForIDCaller* tCaller, Explod& tData) {
	Explod* e = &tData;

	if (e->mPlayer != tCaller->mPlayer) return;
	if (tCaller->mID != -1 && e->mExternalID != tCaller->mID) return;

	e->mAcceleration.x = tCaller->mValue.x;
	e->mAcceleration.y = tCaller->mValue.y;
	auto acceleration = getHandledPhysicsAccelerationReference(e->mPhysicsElement);
	acceleration->x = e->mAcceleration.x;
	acceleration->y = e->mAcceleration.y;
	*acceleration = transformDreamCoordinatesVector(*acceleration, getActiveStateMachineCoordinateP(), getDreamMugenStageHandlerCameraCoordinateP());
}

void updateExplodAcceleration(DreamPlayer* tPlayer, int tID, double tX, double tY)
{
	FloatSetterForIDCaller caller;
	caller.mPlayer = tPlayer;
	caller.mID = tID;
	caller.mValue.x = tX;
	caller.mValue.y = tY;
	stl_int_map_map(gMugenExplod.mExplods, updateExplodAccelerationForSingleExplod, &caller);
}

static void updateExplodRandomOffsetForSingleExplod(IntegerSetterForIDCaller* tCaller, Explod& tData) {
	Explod* e = &tData;

	if (e->mPlayer != tCaller->mPlayer) return;
	if (tCaller->mID != -1 && e->mExternalID != tCaller->mID) return;

	e->mRandomOffset = Vector2DI(randfromInteger(getExplodRandomRangeMin(tCaller->mValue.x), getExplodRandomRangeMax(tCaller->mValue.x)), randfromInteger(getExplodRandomRangeMin(tCaller->mValue.y), getExplodRandomRangeMax(tCaller->mValue.y)));
	e->mRandomOffset = transformDreamCoordinatesVector2DI(e->mRandomOffset, getActiveStateMachineCoordinateP(), getDreamMugenStageHandlerCameraCoordinateP());
	updateExplodPositionAfterUpdate(e);
}

void updateExplodRandomOffset(DreamPlayer* tPlayer, int tID, int tX, int tY)
{
	IntegerSetterForIDCaller caller;
	caller.mPlayer = tPlayer;
	caller.mID = tID;
	caller.mValue.x = tX;
	caller.mValue.y = tY;
	stl_int_map_map(gMugenExplod.mExplods, updateExplodRandomOffsetForSingleExplod, &caller);
}

static void updateExplodRemoveTimeForSingleExplod(IntegerSetterForIDCaller* tCaller, Explod& tData) {
	Explod* e = &tData;

	if (e->mPlayer != tCaller->mPlayer) return;
	if (tCaller->mID != -1 && e->mExternalID != tCaller->mID) return;

	e->mRemoveTime = tCaller->mValue.x;
}

void updateExplodRemoveTime(DreamPlayer* tPlayer, int tID, int tRemoveTime)
{
	IntegerSetterForIDCaller caller;
	caller.mPlayer = tPlayer;
	caller.mID = tID;
	caller.mValue.x = tRemoveTime;
	stl_int_map_map(gMugenExplod.mExplods, updateExplodRemoveTimeForSingleExplod, &caller);
}

static void updateExplodSuperMoveForSingleExplod(IntegerSetterForIDCaller* tCaller, Explod& tData) {
	Explod* e = &tData;

	if (e->mPlayer != tCaller->mPlayer) return;
	if (tCaller->mID != -1 && e->mExternalID != tCaller->mID) return;

	e->mSuperMoveTime = INF * tCaller->mValue.x;
}

void updateExplodSuperMove(DreamPlayer* tPlayer, int tID, int tIsSuperMove)
{
	IntegerSetterForIDCaller caller;
	caller.mPlayer = tPlayer;
	caller.mID = tID;
	caller.mValue.x = tIsSuperMove;
	stl_int_map_map(gMugenExplod.mExplods, updateExplodSuperMoveForSingleExplod, &caller);
}

static void updateExplodSuperMoveTimeForSingleExplod(IntegerSetterForIDCaller* tCaller, Explod& tData) {
	Explod* e = &tData;

	if (e->mPlayer != tCaller->mPlayer) return;
	if (tCaller->mID != -1 && e->mExternalID != tCaller->mID) return;

	e->mSuperMoveTime = tCaller->mValue.x;
}

void updateExplodSuperMoveTime(DreamPlayer* tPlayer, int tID, int tSuperMoveTime)
{
	IntegerSetterForIDCaller caller;
	caller.mPlayer = tPlayer;
	caller.mID = tID;
	caller.mValue.x = tSuperMoveTime;
	stl_int_map_map(gMugenExplod.mExplods, updateExplodSuperMoveTimeForSingleExplod, &caller);
}

static void updateExplodPauseMoveTimeForSingleExplod(IntegerSetterForIDCaller* tCaller, Explod& tData) {
	Explod* e = &tData;

	if (e->mPlayer != tCaller->mPlayer) return;
	if (tCaller->mID != -1 && e->mExternalID != tCaller->mID) return;

	e->mPauseMoveTime = tCaller->mValue.x;
}

void updateExplodPauseMoveTime(DreamPlayer* tPlayer, int tID, int tPauseMoveTime)
{
	IntegerSetterForIDCaller caller;
	caller.mPlayer = tPlayer;
	caller.mID = tID;
	caller.mValue.x = tPauseMoveTime;
	stl_int_map_map(gMugenExplod.mExplods, updateExplodPauseMoveTimeForSingleExplod, &caller);
}

static void updateExplodScaleForSingleExplod(FloatSetterForIDCaller* tCaller, Explod& tData) {
	Explod* e = &tData;

	if (e->mPlayer != tCaller->mPlayer) return;
	if (tCaller->mID != -1 && e->mExternalID != tCaller->mID) return;

	e->mScale.x = tCaller->mValue.x;
	e->mScale.y = tCaller->mValue.y;
	setMugenAnimationDrawScale(e->mAnimationElement, e->mScale);
	setMugenAnimationDrawScale(e->mShadowAnimationElement, Vector2D(1, -getDreamStageShadowScaleY()) * e->mScale);
}

void updateExplodScale(DreamPlayer* tPlayer, int tID, double tX, double tY)
{
	FloatSetterForIDCaller caller;
	caller.mPlayer = tPlayer;
	caller.mID = tID;
	caller.mValue.x = tX;
	caller.mValue.y = tY;
	stl_int_map_map(gMugenExplod.mExplods, updateExplodScaleForSingleExplod, &caller);
}

static void updateExplodSpritePriorityForSingleExplod(IntegerSetterForIDCaller* tCaller, Explod& tData) {
	Explod* e = &tData;

	if (e->mPlayer != tCaller->mPlayer) return;
	if (tCaller->mID != -1 && e->mExternalID != tCaller->mID) return;

	e->mSpritePriority = tCaller->mValue.x;
	updateExplodPositionAfterUpdate(e);
}

void updateExplodSpritePriority(DreamPlayer* tPlayer, int tID, int tSpritePriority)
{
	IntegerSetterForIDCaller caller;
	caller.mPlayer = tPlayer;
	caller.mID = tID;
	caller.mValue.x = tSpritePriority;
	stl_int_map_map(gMugenExplod.mExplods, updateExplodSpritePriorityForSingleExplod, &caller);
}

static void updateExplodOnTopForSingleExplod(IntegerSetterForIDCaller* tCaller, Explod& tData) {
	Explod* e = &tData;

	if (e->mPlayer != tCaller->mPlayer) return;
	if (tCaller->mID != -1 && e->mExternalID != tCaller->mID) return;

	e->mIsOnTop = tCaller->mValue.x;
	updateExplodPositionAfterUpdate(e);
}

void updateExplodOnTop(DreamPlayer* tPlayer, int tID, int tIsOnTop)
{
	IntegerSetterForIDCaller caller;
	caller.mPlayer = tPlayer;
	caller.mID = tID;
	caller.mValue.x = tIsOnTop;
	stl_int_map_map(gMugenExplod.mExplods, updateExplodOnTopForSingleExplod, &caller);
}

static void updateExplodShadowForSingleExplod(IntegerSetterForIDCaller* tCaller, Explod& tData) {
	Explod* e = &tData;

	if (e->mPlayer != tCaller->mPlayer) return;
	if (tCaller->mID != -1 && e->mExternalID != tCaller->mID) return;

	parseShadowStatus(e, tCaller->mValue.x, tCaller->mValue.y, tCaller->mValue.z);
	updateExplodShadowColorAndVisibility(e);
	updateActiveExplodShadow(e);
}

void updateExplodShadow(DreamPlayer* tPlayer, int tID, int tR, int tG, int tB)
{
	IntegerSetterForIDCaller caller;
	caller.mPlayer = tPlayer;
	caller.mID = tID;
	caller.mValue = Vector3DI(tR, tG, tB);
	stl_int_map_map(gMugenExplod.mExplods, updateExplodShadowForSingleExplod, &caller);
}

static void updateExplodOwnPaletteForSingleExplod(IntegerSetterForIDCaller* tCaller, Explod& tData) {
	Explod* e = &tData;

	if (e->mPlayer != tCaller->mPlayer) return;
	if (tCaller->mID != -1 && e->mExternalID != tCaller->mID) return;

	e->mUsesOwnPalette = tCaller->mValue.x;
}

void updateExplodOwnPalette(DreamPlayer* tPlayer, int tID, int tUsesOwnPalette)
{
	IntegerSetterForIDCaller caller;
	caller.mPlayer = tPlayer;
	caller.mID = tID;
	caller.mValue.x = tUsesOwnPalette;
	stl_int_map_map(gMugenExplod.mExplods, updateExplodOwnPaletteForSingleExplod, &caller);
}

static void updateExplodRemoveOnGetHitForSingleExplod(IntegerSetterForIDCaller* tCaller, Explod& tData) {
	Explod* e = &tData;

	if (e->mPlayer != tCaller->mPlayer) return;
	if (tCaller->mID != -1 && e->mExternalID != tCaller->mID) return;

	e->mIsRemovedOnGetHit = tCaller->mValue.x;
}

void updateExplodRemoveOnGetHit(DreamPlayer* tPlayer, int tID, int tIsRemovedOnGetHit)
{
	IntegerSetterForIDCaller caller;
	caller.mPlayer = tPlayer;
	caller.mID = tID;
	caller.mValue.x = tIsRemovedOnGetHit;
	stl_int_map_map(gMugenExplod.mExplods, updateExplodRemoveOnGetHitForSingleExplod, &caller);
}

static void updateExplodIgnoreHitPauseForSingleExplod(IntegerSetterForIDCaller* tCaller, Explod& tData) {
	Explod* e = &tData;

	if (e->mPlayer != tCaller->mPlayer) return;
	if (tCaller->mID != -1 && e->mExternalID != tCaller->mID) return;

	e->mIgnoreHitPause = tCaller->mValue.x;
}

void updateExplodIgnoreHitPause(DreamPlayer* tPlayer, int tID, int tIgnoreHitPause)
{
	IntegerSetterForIDCaller caller;
	caller.mPlayer = tPlayer;
	caller.mID = tID;
	caller.mValue.x = tIgnoreHitPause;
	stl_int_map_map(gMugenExplod.mExplods, updateExplodIgnoreHitPauseForSingleExplod, &caller);
}

static void updateExplodTransparencyForSingleExplod(IntegerSetterForIDCaller* tCaller, Explod& tData) {
	Explod* e = &tData;

	if (e->mPlayer != tCaller->mPlayer) return;
	if (tCaller->mID != -1 && e->mExternalID != tCaller->mID) return;

	e->mHasTransparencyType = 1;
	e->mTransparencyType = DreamExplodTransparencyType(tCaller->mValue.x);
	setMugenAnimationBlendType(e->mAnimationElement, e->mTransparencyType == EXPLOD_TRANSPARENCY_TYPE_ADD_ALPHA ? BLEND_TYPE_ADDITION : BLEND_TYPE_NORMAL);
}

void updateExplodTransparencyType(DreamPlayer* tPlayer, int tID, DreamExplodTransparencyType tTransparencyType)
{
	IntegerSetterForIDCaller caller;
	caller.mPlayer = tPlayer;
	caller.mID = tID;
	caller.mValue.x = int(tTransparencyType);
	stl_int_map_map(gMugenExplod.mExplods, updateExplodTransparencyForSingleExplod, &caller);
}

static void unloadExplod(Explod* e) {
	removeMugenAnimation(e->mAnimationElement);
	removeMugenAnimation(e->mShadowAnimationElement);
	removeFromPhysicsHandler(e->mPhysicsElement);
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

void removeExplodsWithID(DreamPlayer* tPlayer, int tExplodID)
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

void removeAllExplodsForPlayer(DreamPlayer* tPlayer)
{
	RemoveExplodsCaller caller;
	caller.mPlayer = tPlayer;
	stl_int_map_remove_predicate(gMugenExplod.mExplods, removeSingleExplod, &caller);
}

static int removeSingleExplodAfterHit(RemoveExplodsCaller* tCaller, Explod& tData) {
	setProfilingSectionMarkerCurrentFunction();
	Explod* e = &tData;

	if (e->mPlayer == tCaller->mPlayer && e->mIsRemovedOnGetHit) {
		unloadExplod(e);
		return 1;
	}

	return 0;
}

void removeExplodsForPlayerAfterHit(DreamPlayer* tPlayer)
{
	setProfilingSectionMarkerCurrentFunction();
	RemoveExplodsCaller caller;
	caller.mPlayer = tPlayer;
	stl_int_map_remove_predicate(gMugenExplod.mExplods, removeSingleExplodAfterHit, &caller);
}

static int removeSingleExplodAlways(void* /*tCaller*/, Explod& tData) {
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

void setPlayerExplodPaletteEffects(DreamPlayer* tPlayer, int tDuration, const Vector3D& tAddition, const Vector3D& tMultiplier, const Vector3D& tSineAmplitude, int tSinePeriod, int tInvertAll, double tColorFactor, int tIgnoreOwnPal)
{
	for (auto& explodKeyValuePair : gMugenExplod.mExplods) {
		auto& explod = explodKeyValuePair.second;
		if (explod.mPlayer != tPlayer) continue;
		if (!tIgnoreOwnPal && explod.mUsesOwnPalette) continue;

		setMugenAnimationPaletteEffectForDuration(explod.mAnimationElement, tDuration, tAddition, tMultiplier, tSineAmplitude, tSinePeriod, tInvertAll, tColorFactor);
	}
}

void compareSingleAmountSearchPlayer(FindExplodCaller* tCaller, Explod& tData) {
	Explod* e = &tData;

	if (e->mPlayer == tCaller->mPlayer) {
		tCaller->mReturnValue++;
	}
}

int getExplodAmount(DreamPlayer* tPlayer)
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

int getExplodAmountWithID(DreamPlayer* tPlayer, int tID)
{
	FindExplodCaller caller;
	caller.mPlayer = tPlayer;
	caller.mExplodID = tID;
	caller.mReturnValue = 0;

	stl_int_map_map(gMugenExplod.mExplods, compareSingleAmountSearchExplodIDToSearchID, &caller);

	return caller.mReturnValue;
}

typedef struct {
	double mSpeed;
} SetExplodsSpeedCaller;

static void setSingleExplodSpeed(Explod* e, double tSpeed) {
	e->mTimeDilatation = tSpeed;
	setMugenAnimationSpeed(e->mAnimationElement, tSpeed);
	setMugenAnimationSpeed(e->mShadowAnimationElement, tSpeed);
	setHandledPhysicsSpeed(e->mPhysicsElement, tSpeed);
}

static void setSingleExplodSpeedCB(SetExplodsSpeedCaller* tSpeedSetCaller, Explod& e) {
	setSingleExplodSpeed(&e, tSpeedSetCaller->mSpeed);
}

void setExplodsSpeed(double tSpeed) {
	SetExplodsSpeedCaller caller;
	caller.mSpeed = tSpeed;
	stl_int_map_map(gMugenExplod.mExplods, setSingleExplodSpeedCB, &caller);
}

void setAllExplodsNoShadow()
{
	for (auto& explodPair : gMugenExplod.mExplods) {
		auto& e = explodPair.second;
		setMugenAnimationInvisibleForOneFrame(e.mShadowAnimationElement);
	}
}

static void explodAnimationFinishedCB(void* tCaller) {
	Explod* e = (Explod*)tCaller;
	if (e->mRemoveTime != -2) return;

	e->mRemoveTime = 0;
}

static void updateActiveExplodBindTime(Explod* e) {
	if (e->mBindTime == -1 || e->mBindNow < e->mBindTime) {
		e->mBindNow++;
		pauseHandledPhysics(e->mPhysicsElement);
		if (!isPlayer(e->mPlayer)) {
			return;
		}
		const auto pos = getExplodPosition(e);
		setMugenAnimationPosition(e->mAnimationElement, pos);
	}
	else {
		resumeHandledPhysics(e->mPhysicsElement);
	}
}


static int updateActiveExplodRemoveTime(Explod* e) {
	if (e->mRemoveTime < 0) return 0;

	e->mNow++;
	return e->mNow >= e->mRemoveTime;
}

static void updateActiveExplodPhysics(Explod* e) {
	addAccelerationToHandledPhysics(e->mPhysicsElement, e->mAcceleration);
}

static void updateStaticExplodPosition(Explod* e) {
	if (e->mSpace == EXPLOD_SPACE_NONE && (e->mPositionType == EXPLOD_POSITION_TYPE_RELATIVE_TO_RIGHT || e->mPositionType == EXPLOD_POSITION_TYPE_RELATIVE_TO_LEFT)) {
		const auto p = getExplodPosition(e);
		setMugenAnimationPosition(e->mAnimationElement, p);
	}
}

static void setSingleExplodSpeed(Explod* e, double tSpeed);

static int updateActiveExplodSuperPauseStopAndReturnIfStopped(Explod* e) {
	if (!isDreamSuperPauseActive()) return 0;

	const auto timeSinceSuperPauseStarted = getDreamSuperPauseTimeSinceStart();
	const auto moveTime = e->mSuperMoveTime;
	if (moveTime <= timeSinceSuperPauseStarted) {
		setSingleExplodSpeed(e, 0);
		return 1;
	}

	return 0;
}

static int updateActiveExplodPauseStopAndReturnIfStopped(Explod* e) {
	if (!isDreamPauseActive() || isDreamSuperPauseActive()) return 0;

	const auto timeSincePauseStarted = getDreamPauseTimeSinceStart();
	const auto moveTime = e->mPauseMoveTime;
	if (moveTime <= timeSincePauseStarted) {
		setSingleExplodSpeed(e, 0);
		return 1;
	}

	return 0;
}

static int updateSingleExplod(void* tCaller, Explod& tData) {
	(void)tCaller;
	Explod* e = &tData;

	e->mTimeDilatationNow += e->mTimeDilatation;
	int updateAmount = (int)e->mTimeDilatationNow;
	e->mTimeDilatationNow -= updateAmount;
	while (updateAmount--) {
		updateStaticExplodPosition(e);
		if (isPlayerHitPaused(e->mPlayer)) return 0;

		if (updateActiveExplodSuperPauseStopAndReturnIfStopped(e)) return 0;
		if (updateActiveExplodPauseStopAndReturnIfStopped(e)) return 0;
		updateActiveExplodBindTime(e);
		if (updateActiveExplodRemoveTime(e)) {
			unloadExplod(e);
			return 1;
		}

		updateActiveExplodPhysics(e);
		updateActiveExplodShadow(e);
	}
	return 0;
}

static void updateExplods(void* /*tData*/) {
	setProfilingSectionMarkerCurrentFunction();
	stl_int_map_remove_predicate(gMugenExplod.mExplods, updateSingleExplod);
}

ActorBlueprint getDreamExplodHandler() {
	return makeActorBlueprint(loadExplods, unloadExplods, updateExplods);
}