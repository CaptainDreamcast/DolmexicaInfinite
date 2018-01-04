#pragma once

#include <tari/geometry.h>
#include <tari/datastructures.h>
#include <tari/actorhandler.h>

#include <tari/mugenspritefilereader.h>

// documentation at http://www.elecbyte.com/mugendocs-11b1/bgs.html


void setDreamStageMugenDefinition(char* tPath);
extern ActorBlueprint DreamStageBP;

double parseDreamCoordinatesToLocalCoordinateSystem(double tCoordinate, int tOtherCoordinateSystemAsP);

Position getDreamPlayerStartingPosition(int i, int tCoordinateP);
Position getDreamStageCoordinateSystemOffset(int tCoordinateP);
int doesDreamPlayerStartFacingLeft(int i);

double getDreamCameraPositionX(int tCoordinateP);
double getDreamCameraPositionY(int tCoordinateP);
double getDreamScreenFactorFromCoordinateP(int tCoordinateP);
int getDreamStageCoordinateP();

double transformDreamCoordinates(double tVal, int tSrcP, int tDstP);
Vector3D transformDreamCoordinatesVector(Vector3D tVal, int tSrcP, int tDstP);

double getDreamStageTopOfScreenBasedOnPlayer(int tCoordinateP);
double getDreamStageLeftOfScreenBasedOnPlayer(int tCoordinateP);
double getDreamStageRightOfScreenBasedOnPlayer(int tCoordinateP);
Position getDreamStageCenterOfScreenBasedOnPlayer(int tCoordinateP);

int getDreamGameWidth(int tCoordinateP);

int getDreamStageLeftEdgeMinimumPlayerDistance(int tCoordinateP);
int getDreamStageRightEdgeMinimumPlayerDistance(int tCoordinateP);