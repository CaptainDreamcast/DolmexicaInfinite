#pragma once

#include <tari/actorhandler.h>
#include <tari/animation.h>
#include <tari/mugenspritefilereader.h>
#include <tari/mugenanimationreader.h>

void setDreamMugenStageHandlerCameraCoordinates(Vector3DI tCoordinates);
void setDreamMugenStageHandlerCameraRange(GeoRectangle tRect);
void setDreamMugenStageHandlerCameraPosition(Position p);
void addDreamMugenStageHandlerCameraPositionX(double tX);
void setDreamMugenStageHandlerCameraPositionX(double tX);
void resetDreamMugenStageHandlerCameraPosition();

void setDreamMugenStageHandlerScreenShake(int tTime, double tFrequency, int tAmplitude, double tPhase);

void addDreamMugenStageHandlerAnimatedBackgroundElement(Position tStart, int tAnimationID, MugenAnimations* tAnimations, MugenSpriteFile* tSprites, Position tDelta, Vector3DI tTile, Vector3DI tTileSpacing, Vector3DI tCoordinates);
void addDreamMugenStageHandlerStaticBackgroundElement(Position tStart, int tSpriteGroup, int tSpriteItem, MugenSpriteFile* tSprites, Position tDelta, Vector3DI tTile, Vector3DI tTileSpacing, Vector3DI tCoordinates);
Position* getDreamMugenStageHandlerCameraPositionReference();

extern ActorBlueprint DreamMugenStageHandler;
