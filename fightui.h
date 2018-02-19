#pragma once

#include <prism/actorhandler.h>
#include <prism/geometry.h>

#include <prism/mugenspritefilereader.h>
#include <prism/mugenanimationreader.h>

#include "playerdefinition.h"

void playDreamHitSpark(Position tPosition, DreamPlayer* tPlayer, int tIsInPlayerFile, int tNumber, int tIsFacingRight, int tPositionCoordinateP, int tScaleCoordinateP);
void setDreamLifeBarPercentage(DreamPlayer* tPlayer, double tPercentage);
void setDreamPowerBarPercentage(DreamPlayer* tPlayer, double tPercentage);
void enableDreamTimer();
void disableDreamTimer();
void resetDreamTimer();

MugenAnimation* getDreamFightEffectAnimation(int tNumber);
MugenSpriteFile* getDreamFightEffectSprites();

void playDreamRoundAnimation(int tRound, void(*tFunc)());
void playDreamFightAnimation(void(*tFunc)());
void playDreamKOAnimation(void(*tFunc)());
void playDreamWinAnimation(char* tName, void(*tFunc)());
void playDreamContinueAnimation(void(*tAnimationFinishedFunc)(), void(*tContinuePressedFunc)());
void setDreamTimeDisplayFinishedCB(void(*tTimeDisplayFinishedFunc)());

void setDreamBarInvisibleForOneFrame();

extern ActorBlueprint DreamFightUIBP;