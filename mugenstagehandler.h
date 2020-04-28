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
	Position mSinOffset;
	Position mSinOffsetInternal;
	Position mDelta;
	Vector3DI mCoordinates;
	Vector3D mVelocity;
	Vector3D mVelocityResetValue;
	Vector3D mGlobalScale;
	Vector3D mDrawScale;

	MugenSpriteFile* mSprites;
	MugenAnimation* mAnimation;
	int mOwnsAnimation;
	std::list<StageElementAnimationReference> mAnimationReferences;

	double mScaleStartY;
	double mScaleDeltaY;
	Vector3D mScaleStart;
	Vector3D mScaleDelta;

	Vector3DI mTile;
	Vector3DI mTileSize;
	Vector3DI mTileSpacing;
	int mLayerNo;

	int mIsEnabled;
	int mIsInvisible;
	int mInvisibleFlag;

	int mIsParallax;
	Vector3D mWidth;
	double mInvertedMinimumWidthFactor;
	Vector3D mXScale;

	GeoRectangle mConstraintRectangle;
	Vector3D mConstraintRectangleDelta;

	int mSinTime;
	Vector3D mSinX;
	Vector3D mSinY;

	Position mTileBasePosition;
	Position mTileBaseScale;
	struct StaticStageHandlerElement_t* mPositionLinkElement;
} StaticStageHandlerElement;

Vector3DI getDreamMugenStageHandlerCameraCoordinates();
int getDreamMugenStageHandlerCameraCoordinateP();
void setDreamMugenStageHandlerCameraCoordinates(const Vector3DI& tCoordinates);
void setDreamMugenStageHandlerCameraRange(const GeoRectangle& tRect);
void setDreamMugenStageHandlerCameraPosition(const Position& p);
void addDreamMugenStageHandlerCameraPositionX(double tX);
void setDreamMugenStageHandlerCameraPositionX(double tX);
void addDreamMugenStageHandlerCameraPositionY(double tY);
void setDreamMugenStageHandlerCameraPositionY(double tY);
void setDreamMugenStageHandlerScreenShake(const Position& tScreenShake);
void resetDreamMugenStageHandlerCameraPosition();
void resetDreamMugenStageHandler();

void addDreamMugenStageHandlerAnimatedBackgroundElement(const Position& tStart, MugenAnimation* tAnimation, int tOwnsAnimation, MugenSpriteFile * tSprites, const Position& tDelta, const Vector3DI& tTile, const Vector3DI& tTileSpacing, BlendType tBlendType, const Vector3D& tAlpha, const GeoRectangle& tConstraintRectangle, const Position& tConstraintRectangleDelta, const Vector3D& tVelocity, const Vector3D& tSinX, const Vector3D& tSinY, double tScaleStartY, double tScaleDeltaY, const Position& tScaleStart, const Position& tScaleDelta, const Position& tDrawScale, int tLayerNo, int tID, int tIsParallax, const Vector3DI& tWidth, const Vector3D& tXScale, double tZoomDelta, int tPositionLink, const Vector3DI& tCoordinates);
Position* getDreamMugenStageHandlerCameraPositionReference();
Position* getDreamMugenStageHandlerCameraEffectPositionReference();
void setDreamMugenStageHandlerCameraEffectPositionX(double tX);
void setDreamMugenStageHandlerCameraEffectPositionY(double tY);
Position* getDreamMugenStageHandlerCameraTargetPositionReference();
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
