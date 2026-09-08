#pragma once

#include <prism/actorhandler.h>
#include <prism/animation.h>
#include <prism/mugenspritefilereader.h>
#include <prism/mugenanimationreader.h>
#include <prism/geometry.h>
#include <prism/datastructures.h>
#include <prism/stlutil.h>

using namespace prism;

#define BACKGROUND_UPPER_BASE_Z 52

namespace prism {
	struct MugenAnimationHandlerElement;
}

typedef struct {
	MugenAnimationHandlerElement* mElement;
	Position mReferencePosition;
	Position mOffset;
	Position mStartPosition;
} StageElementAnimationReference;

typedef struct StaticStageHandlerElement_t {
	Position mStart;
	Position mStartResetValue;
	Position2D mSinOffset;
	Position2D mSinOffsetInternal;
	Position2D mDelta;
	Vector2DI mCoordinates;
	Vector2D mVelocity;
	Vector2D mVelocityResetValue;
	Vector2D mGlobalScale;
	Vector2D mDrawScale;
	Vector2D mParallaxScale;

	MugenSpriteFile* mSprites;
	MugenAnimation* mAnimation;
	int mOwnsAnimation;
	std::list<StageElementAnimationReference> mAnimationReferences;

	float mScaleStartY;
	float mScaleDeltaY;
	Vector2D mScaleStart;
	Vector2D mScaleDelta;

	Vector2DI mTile;
	Vector2DI mTileSize;
	Vector2DI mTileSpacing;
	int mLayerNo;

	int mIsEnabled;
	int mIsInvisible;
	int mInvisibleFlag;

	int mIsParallax;
	Vector2D mWidth;
	float mInvertedMinimumWidthFactor;
	Vector2D mXScale;

	GeoRectangle2D mConstraintRectangle;
	Vector2D mConstraintRectangleDelta;

	int mSinTime;
	Vector3D mSinX;
	Vector3D mSinY;

	Position mTileBasePosition;
	Vector2D mTileBaseScale;
	struct StaticStageHandlerElement_t* mPositionLinkElement;
} StaticStageHandlerElement;

Vector2DI getDreamMugenStageHandlerCameraCoordinates();
int getDreamMugenStageHandlerCameraCoordinateP();
void setDreamMugenStageHandlerCameraCoordinates(const Vector2DI& tCoordinates);
void setDreamMugenStageHandlerCameraRange(const GeoRectangle2D& tRect);
GeoRectangle2D getDreamMugenStageHandlerCameraRange();
void setDreamMugenStageHandlerCameraPosition(const Position2D& p);
void addDreamMugenStageHandlerCameraPositionX(float tX);
void setDreamMugenStageHandlerCameraPositionX(float tX);
void addDreamMugenStageHandlerCameraPositionY(float tY);
void setDreamMugenStageHandlerCameraPositionY(float tY);
void setDreamMugenStageHandlerScreenShake(const Position2D& tScreenShake);
void resetDreamMugenStageHandlerCameraPosition();
void resetDreamMugenStageHandler();
void clearDreamMugenStageHandler();

void addDreamMugenStageHandlerAnimatedBackgroundElement(const Position& tStart, MugenAnimation* tAnimation, int tOwnsAnimation, MugenSpriteFile * tSprites, const Position2D& tDelta, const Vector2DI& tTile, const Vector2DI& tTileSpacing, BlendType tBlendType, const Vector2D& tAlpha, const GeoRectangle2D& tConstraintRectangle, const Vector2D& tConstraintRectangleDelta, const Vector2D& tVelocity, const Vector3D& tSinX, const Vector3D& tSinY, float tScaleStartY, float tScaleDeltaY, const Vector2D& tScaleStart, const Vector2D& tScaleDelta, const Vector2D& tDrawScale, int tLayerNo, int tID, int tIsParallax, const Vector2DI& tWidth, const Vector2D& tXScale, float tZoomDelta, int tPositionLink, const Vector2DI& tCoordinates);
Position* getDreamMugenStageHandlerCameraPositionReference();
Position2D* getDreamMugenStageHandlerCameraEffectPositionReference();
void setDreamMugenStageHandlerCameraEffectPositionX(float tX);
void setDreamMugenStageHandlerCameraEffectPositionY(float tY);
Position2D* getDreamMugenStageHandlerCameraTargetPositionReference();
Position* getDreamMugenStageHandlerCameraZoomReference();
void setDreamMugenStageHandlerCameraZoom(float tZoom);

void setDreamMugenStageHandlerSpeed(float tSpeed);

void setDreamStageInvisibleForOneFrame();
void setDreamStageLayer1InvisibleForOneFrame();
void setDreamStagePaletteEffects(int tDuration, const Vector3D& tAddition, const Vector3D& tMultiplier, const Vector3D& tSineAmplitude, int tSinePeriod, int tInvertAll, float tColorFactor);

void setStageElementInvisible(StaticStageHandlerElement* tElement, int tIsInvisible);
void setStageElementEnabled(StaticStageHandlerElement* tElement, int tIsEnabled);
void setStageElementVelocityX(StaticStageHandlerElement* tElement, float tVelocityX);
void setStageElementVelocityY(StaticStageHandlerElement* tElement, float tVelocityY);
void addStageElementVelocityX(StaticStageHandlerElement* tElement, float tVelocityX);
void addStageElementVelocityY(StaticStageHandlerElement* tElement, float tVelocityY);
void setStageElementPositionX(StaticStageHandlerElement* tElement, float tPositionX);
void setStageElementPositionY(StaticStageHandlerElement* tElement, float tPositionY);
void addStageElementPositionX(StaticStageHandlerElement* tElement, float tPositionX);
void addStageElementPositionY(StaticStageHandlerElement* tElement, float tPositionY);
void setStageElementSinOffsetX(StaticStageHandlerElement* tElement, float tOffsetX);
void setStageElementSinOffsetY(StaticStageHandlerElement* tElement, float tOffsetY);
void setStageElementAnimation(StaticStageHandlerElement* tElement, int tAnimation);

float calculateStageElementSinOffset(int tTick, float tAmplitude, float tPeriod, float tPhase);

std::vector<StaticStageHandlerElement*>& getStageHandlerElementsWithID(int tID);

ActorBlueprint getDreamMugenStageHandler();
