#pragma once

#include <prism/actorhandler.h>
#include <prism/geometry.h>

#include <prism/mugenspritefilereader.h>
#include <prism/mugenanimationreader.h>

#include "playerdefinition.h"

void playDreamHitSpark(Position tPosition, DreamPlayer* tPlayer, int tIsInPlayerFile, int tNumber, int tIsFacingRight, int tPositionCoordinateP, int tScaleCoordinateP);
void addDreamDustCloud(Position tPosition, int tIsFacingRight, int tCoordinateP);
void setDreamLifeBarPercentage(DreamPlayer* tPlayer, double tPercentage);
void setDreamPowerBarPercentage(DreamPlayer* tPlayer, double tPercentage, int tValue);
void enableDreamTimer();
void disableDreamTimer();
void resetDreamTimer();

MugenAnimation* getDreamFightEffectAnimation(int tNumber);
MugenSpriteFile* getDreamFightEffectSprites();
MugenSounds* getDreamCommonSounds();

void playDreamRoundAnimation(int tRound, void(*tFunc)());
void playDreamFightAnimation(void(*tFunc)());
void playDreamKOAnimation(void(*tFunc)());
void playDreamWinAnimation(char* tName, void(*tFunc)());
void playDreamContinueAnimation(void(*tAnimationFinishedFunc)(), void(*tContinuePressedFunc)());
void setDreamTimeDisplayFinishedCB(void(*tTimeDisplayFinishedFunc)());

void setDreamBarInvisibleForOneFrame();

void setDreamNoMusicFlag();
void setTimerFreezeFlag();
void setTimerInfinite();
void setTimerFinite();

void setEnvironmentColor(Vector3DI tColors, int tTime, int tIsUnderCharacters);
void setEnvironmentShake(int tDuration, double tFrequency, int tAmplitude, double tPhaseOffset);

extern ActorBlueprint DreamFightUIBP;