#pragma once

#include <prism/actorhandler.h>
#include <prism/animation.h>
#include <prism/mugenspritefilereader.h>
#include <prism/mugenanimationreader.h>

void setDreamMugenStageHandlerCameraCoordinates(Vector3DI tCoordinates);
void setDreamMugenStageHandlerCameraRange(GeoRectangle tRect);
void setDreamMugenStageHandlerCameraPosition(Position p);
void addDreamMugenStageHandlerCameraPositionX(double tX);
void setDreamMugenStageHandlerCameraPositionX(double tX);
void setDreamMugenStageHandlerCameraPositionY(double tY);
void resetDreamMugenStageHandlerCameraPosition();

void addDreamMugenStageHandlerAnimatedBackgroundElement(Position tStart, MugenAnimation* tAnimation, MugenSpriteFile * tSprites, Position tDelta, Vector3DI tTile, Vector3DI tTileSpacing, BlendType tBlendType, GeoRectangle tConstraintRectangle, Vector3D tVelocity, double tStartScaleY, double tScaleDeltaY, int tLayerNo, Vector3DI tCoordinates);
Position* getDreamMugenStageHandlerCameraPositionReference();

void setDreamStageInvisibleForOneFrame();
void setDreamStageLayer1InvisibleForOneFrame();


extern ActorBlueprint DreamMugenStageHandler;
