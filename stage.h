#pragma once

#include <prism/geometry.h>
#include <prism/datastructures.h>
#include <prism/actorhandler.h>

#include <prism/mugenspritefilereader.h>

// documentation at http://www.elecbyte.com/mugendocs-11b1/bgs.html


void setDreamStageMugenDefinition(char* tPath, char* tCustomMusicPath);
extern ActorBlueprint DreamStageBP;

void playDreamStageMusic();

double parseDreamCoordinatesToLocalCoordinateSystem(double tCoordinate, int tOtherCoordinateSystemAsP);

Position getDreamPlayerStartingPosition(int i, int tCoordinateP);
Position getDreamStageCoordinateSystemOffset(int tCoordinateP);
int doesDreamPlayerStartFacingLeft(int i);

double getDreamCameraPositionX(int tCoordinateP);
double getDreamCameraPositionY(int tCoordinateP);
double getDreamCameraZoom(int tCoordinateP);
double getDreamScreenFactorFromCoordinateP(int tCoordinateP);
int getDreamStageCoordinateP();

double getDreamStageLeftEdgeX(int tCoordinateP);
double getDreamStageRightEdgeX(int tCoordinateP);
double getDreamStageTopEdgeY(int tCoordinateP);
double getDreamStageBottomEdgeY(int tCoordinateP);


double transformDreamCoordinates(double tVal, int tSrcP, int tDstP);
Vector3D transformDreamCoordinatesVector(Vector3D tVal, int tSrcP, int tDstP);

double getDreamStageTopOfScreenBasedOnPlayer(int tCoordinateP);
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

void setDreamStageCoordinates(Vector3DI tCoordinates);

double getDreamStageShadowTransparency();
Vector3D getDreamStageShadowColor();
double getDreamStageShadowScaleY();
Vector3D getDreamStageShadowFadeRange(int tCoordinateP);
double getDreamStageReflectionTransparency();

void setDreamStageNoAutomaticCameraMovement();
void setDreamStageAutomaticCameraMovement();
