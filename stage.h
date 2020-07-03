#pragma once

#include <prism/geometry.h>
#include <prism/datastructures.h>
#include <prism/actorhandler.h>
#include <prism/drawing.h>
#include <prism/mugenspritefilereader.h>
#include <prism/mugenanimationhandler.h>
#include <prism/mugendefreader.h>
#include <prism/physicshandler.h>

// documentation at http://www.elecbyte.com/mugendocs-11b1/bgs.html

void loadBackgroundElementGroup(MugenDefScriptGroup* tGroup, int i, MugenSpriteFile* tSprites, MugenAnimations* tAnimations, const Vector2DI& tLocalCoordinates, const Vector2D& tGlobalScale);
void setDreamStageMugenDefinition(const char* tPath, const char* tCustomMusicPath);
ActorBlueprint getDreamStageBP();

MugenAnimations* getStageAnimations();
void playDreamStageMusic();

double parseDreamCoordinatesToLocalCoordinateSystem(double tCoordinate, int tOtherCoordinateSystemAsP);

Position2D getDreamPlayerStartingPositionInCameraCoordinates(int i);
Position2D getDreamCameraStartPosition(int tCoordinateP);
Position2D getDreamStageCoordinateSystemOffset(int tCoordinateP);
int doesDreamPlayerStartFacingLeft(int i);

double getDreamCameraPositionX(int tCoordinateP);
double getDreamCameraPositionY(int tCoordinateP);
double getDreamCameraZoom();
void setDreamStageZoomOneFrame(double tScale, const Position2D& tStagePos);
double getDreamScreenFactorFromCoordinateP(int tCoordinateP);
int getDreamStageCoordinateP();

double getDreamStageLeftEdgeX(int tCoordinateP);
double getDreamStageRightEdgeX(int tCoordinateP);
double getDreamStageTopEdgeY(int tCoordinateP);
double getDreamStageBottomEdgeY(int tCoordinateP);

double getDreamStageBoundLeft(int tCoordinateP);
double getDreamStageBoundRight(int tCoordinateP);

double transformDreamCoordinates(double tVal, int tSrcP, int tDstP);
int transformDreamCoordinatesI(int tVal, int tSrcP, int tDstP);
Vector2D transformDreamCoordinatesVector2D(const Vector2D& tVal, int tSrcP, int tDstP);
Vector3D transformDreamCoordinatesVector(const Vector3D& tVal, int tSrcP, int tDstP);
Vector2DI transformDreamCoordinatesVector2DI(const Vector2DI& tVal, int tSrcP, int tDstP);
Vector3DI transformDreamCoordinatesVectorI(const Vector3DI& tVal, int tSrcP, int tDstP);

double getDreamStageTopOfScreenBasedOnPlayer(int tCoordinateP);
double getDreamStageTopOfScreenBasedOnPlayerInStageCoordinateOffset(int tCoordinateP);
double getDreamStageLeftOfScreenBasedOnPlayer(int tCoordinateP);
double getDreamStageRightOfScreenBasedOnPlayer(int tCoordinateP);
Position2D getDreamStageCenterOfScreenBasedOnPlayer(int tCoordinateP);

int getDreamGameWidth(int tCoordinateP);
int getDreamGameHeight(int tCoordinateP);
int getDreamScreenWidth(int tCoordinateP);
int getDreamScreenHeight(int tCoordinateP);

char* getDreamStageAuthor();
char* getDreamStageDisplayName();
char* getDreamStageName();

int getDreamStageLeftEdgeMinimumPlayerDistance(int tCoordinateP);
int getDreamStageRightEdgeMinimumPlayerDistance(int tCoordinateP);

void setDreamStageCoordinates(const Vector2DI& tCoordinates);

double getDreamStageShadowTransparency();
Vector3D getDreamStageShadowColor();
double getDreamStageShadowScaleY();
double getDreamStageReflectionTransparency();
double getDreamStageShadowFadeRangeFactor(double tPosY, int tCoordinateP);

void setDreamStageNoAutomaticCameraMovement();
void setDreamStageAutomaticCameraMovement();

BlendType getBackgroundBlendType(MugenDefScriptGroup* tGroup);
BlendType handleBackgroundMask(MugenDefScriptGroup* tGroup, BlendType tBlendType);
Vector2D getBackgroundAlphaVector(MugenDefScriptGroup* tGroup);

void resetStageIfNotResetForRound();
void resetStageForRound();