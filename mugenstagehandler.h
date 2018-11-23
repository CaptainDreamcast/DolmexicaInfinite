#pragma once

#include <prism/actorhandler.h>
#include <prism/animation.h>
#include <prism/mugenspritefilereader.h>
#include <prism/mugenanimationreader.h>
#include <prism/geometry.h>
#include <prism/datastructures.h>

#define BACKGROUND_UPPER_BASE_Z 52

typedef struct {

	Position mStart;
	Position mDelta;
	Vector3DI mCoordinates;
	Vector3D mVelocity;

	MugenSpriteFile* mSprites;
	MugenAnimation* mAnimation;
	int mOwnsAnimation;
	List mAnimationReferences;

	double mStartScaleY;
	double mScaleDeltaY;

	Vector3DI mTileSize;
	int mLayerNo;

	int mIsEnabled;
	int mIsInvisible;
	int mInvisibleFlag;
} StaticStageHandlerElement;

void setDreamMugenStageHandlerCameraCoordinates(Vector3DI tCoordinates);
void setDreamMugenStageHandlerCameraRange(GeoRectangle tRect);
void setDreamMugenStageHandlerCameraPosition(Position p);
void addDreamMugenStageHandlerCameraPositionX(double tX);
void setDreamMugenStageHandlerCameraPositionX(double tX);
void addDreamMugenStageHandlerCameraPositionY(double tY);
void setDreamMugenStageHandlerCameraPositionY(double tY);
void resetDreamMugenStageHandlerCameraPosition();

void addDreamMugenStageHandlerAnimatedBackgroundElement(Position tStart, MugenAnimation* tAnimation, int tOwnsAnimation, MugenSpriteFile * tSprites, Position tDelta, Vector3DI tTile, Vector3DI tTileSpacing, BlendType tBlendType, GeoRectangle tConstraintRectangle, Vector3D tVelocity, double tStartScaleY, double tScaleDeltaY, int tLayerNo, int tID, Vector3DI tCoordinates);
Position* getDreamMugenStageHandlerCameraPositionReference();

void setDreamStageInvisibleForOneFrame();
void setDreamStageLayer1InvisibleForOneFrame();

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
void setStageElementAnimation(StaticStageHandlerElement* tElement, int tAnimation);

Vector* getStageHandlerElementsWithID(int tID);

ActorBlueprint getDreamMugenStageHandler();
