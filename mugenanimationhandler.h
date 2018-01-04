#pragma once

#include <tari/actorhandler.h>
#include <tari/geometry.h>
#include <tari/mugenspritefilereader.h>
#include <tari/mugenanimationreader.h>

#include "playerdefinition.h"

int addDreamRegisteredAnimation(DreamPlayer* p, MugenAnimation * tStartAnimation, MugenSpriteFile* tSprites, Position* tBasePosition, int tPositionCoordinateSystemInP, int tScaleCoordinateSystemInP);
void removeDreamRegisteredAnimation(int tID);

int getDreamRegisteredAnimationAnimationNumber(int tID);
int getDreamRegisteredAnimationRemainingAnimationTime(int tID);
void setDreamRegisteredAnimationFaceDirection(int tID, FaceDirection tDirection);
void setDreamRegisteredAnimationRectangleWidth(int tID, int tWidth);
void setDreamRegisteredAnimationCameraPositionReference(int tID, Position* tCameraPosition);
void setDreamRegisteredAnimationInvisibleFlag(int tID);
void setDreamRegisteredAnimationOneFrameDrawScale(int tID, Vector3D tScale);
void setDreamRegisteredAnimationOneFrameDrawAngle(int tID, double tAngle);
void addDreamRegisteredAnimationOneFrameDrawAngle(int tID, double tAngle);
void setDreamRegisteredAnimationOneFrameFixedDrawAngle(int tID, double tAngle);
void setDreamRegisteredAnimationToNotUseStage(int tID);
void setDreamRegisteredAnimationBasePosition(int tID, Position* tBasePosition);

void changeDreamGameMugenAnimationWithStartStep(int tID, MugenAnimation* tNewAnimation, int tStartStep);

int isDreamStartingHandledAnimationElementWithID(int tID, int tStepID);
int getDreamTimeFromHandledAnimationElement(int tID, int tStep);
int getDreamHandledAnimationElementFromTimeOffset(int tID, int tTime);

void setDreamRegisteredAnimationSpritePriority(int tID, int tPriority);
void setDreamRegisteredAnimationToUseFixedZ(int tID);
void pauseDreamRegisteredAnimation(int tID);
void unpauseDreamRegisteredAnimation(int tID);

void advanceDreamRegisteredAnimationOneTick(int tID);

extern ActorBlueprint DreamMugenGameAnimationHandler;
