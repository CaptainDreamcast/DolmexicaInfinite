#pragma once

#include <prism/actorhandler.h>
#include <prism/animation.h>
#include <prism/mugenspritefilereader.h>
#include <prism/mugenanimationreader.h>
#include <prism/geometry.h>
#include <prism/datastructures.h>
#include <prism/stlutil.h>

#define BACKGROUND_UPPER_BASE_Z 52

struct MugenAnimationHandlerElement;

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

	double mScaleStartY;
	double mScaleDeltaY;
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
	double mInvertedMinimumWidthFactor;
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
void setDreamMugenStageHandlerCameraPosition(const Position2D& p);
void addDreamMugenStageHandlerCameraPositionX(double tX);
void setDreamMugenStageHandlerCameraPositionX(double tX);
void addDreamMugenStageHandlerCameraPositionY(double tY);
void setDreamMugenStageHandlerCameraPositionY(double tY);
void setDreamMugenStageHandlerScreenShake(const Position2D& tScreenShake);
void resetDreamMugenStageHandlerCameraPosition();
void resetDreamMugenStageHandler();
void clearDreamMugenStageHandler();

void addDreamMugenStageHandlerAnimatedBackgroundElement(const Position& tStart, MugenAnimation* tAnimation, int tOwnsAnimation, MugenSpriteFile * tSprites, const Position2D& tDelta, const Vector2DI& tTile, const Vector2DI& tTileSpacing, BlendType tBlendType, const Vector2D& tAlpha, const GeoRectangle2D& tConstraintRectangle, const Vector2D& tConstraintRectangleDelta, const Vector2D& tVelocity, const Vector3D& tSinX, const Vector3D& tSinY, double tScaleStartY, double tScaleDeltaY, const Vector2D& tScaleStart, const Vector2D& tScaleDelta, const Vector2D& tDrawScale, int tLayerNo, int tID, int tIsParallax, const Vector2DI& tWidth, const Vector2D& tXScale, double tZoomDelta, int tPositionLink, const Vector2DI& tCoordinates);
Position* getDreamMugenStageHandlerCameraPositionReference();
Position2D* getDreamMugenStageHandlerCameraEffectPositionReference();
void setDreamMugenStageHandlerCameraEffectPositionX(double tX);
void setDreamMugenStageHandlerCameraEffectPositionY(double tY);
Position2D* getDreamMugenStageHandlerCameraTargetPositionReference();
Position* getDreamMugenStageHandlerCameraZoomReference();
void setDreamMugenStageHandlerCameraZoom(double tZoom);

void setDreamMugenStageHandlerSpeed(double tSpeed);

void setDreamStageInvisibleForOneFrame();
void setDreamStageLayer1InvisibleForOneFrame();
void setDreamStagePaletteEffects(int tDuration, const Vector3D& tAddition, const Vector3D& tMultiplier, const Vector3D& tSineAmplitude, int tSinePeriod, int tInvertAll, double tColorFactor);

void setStageElementInvisible(StaticStageHandlerElement* tElement, int tIsInvisible);
void setStageElementEnabled(StaticStageHandlerElement* tElement, int tIsEnabled);
void setStageElementVelocityX(StaticStageHandlerElement* tElement, double tVelocityX);
void setStageElementVelocityY(StaticStageHandlerElement* tElement, double tVelocityY);
void addStageElementVelocityX(StaticStageHandlerElement* tElement, double tVelocityX);
void addStageElementVelocityY(StaticStageHandlerElement* tElement, double tVelocityY);
void setStageElementPositionX(StaticStageHandlerElement* tElement, double tPositionX);
void setStageElementPositionY(StaticStageHandlerElement* tElement, double tPositionY);
void addStageElementPositionX(StaticStageHandlerElement* tElement, double tPositionX);
void addStageElementPositionY(StaticStageHandlerElement* tElement, double tPositionY);
void setStageElementSinOffsetX(StaticStageHandlerElement* tElement, double tOffsetX);
void setStageElementSinOffsetY(StaticStageHandlerElement* tElement, double tOffsetY);
void setStageElementAnimation(StaticStageHandlerElement* tElement, int tAnimation);

double calculateStageElementSinOffset(int tTick, double tAmplitude, double tPeriod, double tPhase);

std::vector<StaticStageHandlerElement*>& getStageHandlerElementsWithID(int tID);

ActorBlueprint getDreamMugenStageHandler();
