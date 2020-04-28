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

void setDreamStageMugenDefinition(const char* tPath, const char* tCustomMusicPath);
ActorBlueprint getDreamStageBP();

MugenAnimations* getStageAnimations();
void playDreamStageMusic();

double parseDreamCoordinatesToLocalCoordinateSystem(double tCoordinate, int tOtherCoordinateSystemAsP);

Position getDreamPlayerStartingPositionInCameraCoordinates(int i);
Position getDreamCameraStartPosition(int tCoordinateP);
Position getDreamStageCoordinateSystemOffset(int tCoordinateP);
int doesDreamPlayerStartFacingLeft(int i);

double getDreamCameraPositionX(int tCoordinateP);
double getDreamCameraPositionY(int tCoordinateP);
double getDreamCameraZoom();
void setDreamStageZoomOneFrame(double tScale, const Position& tStagePos);
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
Vector3D transformDreamCoordinatesVector(const Vector3D& tVal, int tSrcP, int tDstP);
Vector3DI transformDreamCoordinatesVectorI(const Vector3DI& tVal, int tSrcP, int tDstP);

double getDreamStageTopOfScreenBasedOnPlayer(int tCoordinateP);
double getDreamStageTopOfScreenBasedOnPlayerInStageCoordinateOffset(int tCoordinateP);
double getDreamStageLeftOfScreenBasedOnPlayer(int tCoordinateP);
double getDreamStageRightOfScreenBasedOnPlayer(int tCoordinateP);
Position getDreamStageCenterOfScreenBasedOnPlayer(int tCoordinateP);

int getDreamGameWidth(int tCoordinateP);
int getDreamGameHeight(int tCoordinateP);
int getDreamScreenWidth(int tCoordinateP);
int getDreamScreenHeight(int tCoordinateP);

char* getDreamStageAuthor();
char* getDreamStageDisplayName();
char* getDreamStageName();

int getDreamStageLeftEdgeMinimumPlayerDistance(int tCoordinateP);
int getDreamStageRightEdgeMinimumPlayerDistance(int tCoordinateP);

void setDreamStageCoordinates(const Vector3DI& tCoordinates);

double getDreamStageShadowTransparency();
Vector3D getDreamStageShadowColor();
double getDreamStageShadowScaleY();
double getDreamStageReflectionTransparency();
double getDreamStageShadowFadeRangeFactor(double tPosY, int tCoordinateP);

void setDreamStageNoAutomaticCameraMovement();
void setDreamStageAutomaticCameraMovement();

BlendType getBackgroundBlendType(MugenDefScriptGroup* tGroup);
BlendType handleBackgroundMask(MugenDefScriptGroup* tGroup, BlendType tBlendType);
Vector3D getBackgroundAlphaVector(MugenDefScriptGroup* tGroup);

void resetStageIfNotResetForRound();
void resetStageForRound();