#pragma once

#include <prism/actorhandler.h>
#include <prism/geometry.h>

#include "playerdefinition.h"

ActorBlueprint getPauseControllerHandler();

void setDreamSuperPauseTime(DreamPlayer* tPlayer, int tTime);
void setDreamSuperPauseBufferTimeForCommandsDuringPauseEnd(DreamPlayer* tPlayer, int tBufferTime);
int getDreamSuperPauseMoveTime();
void setDreamSuperPauseMoveTime(DreamPlayer* tPlayer, int tMoveTime);
void setDreamSuperPauseIsPausingBG(DreamPlayer* tPlayer, int tIsPausingBG);
void setDreamSuperPauseAnimation(DreamPlayer* tPlayer, int tIsInPlayerFile, int tAnimationNumber);
void setDreamSuperPauseSound(DreamPlayer* tPlayer, int tIsInPlayerFile, int tSoundGroup, int tSoundItem);
void setDreamSuperPausePosition(DreamPlayer* tPlayer, double tX, double tY);
void setDreamSuperPauseDarkening(DreamPlayer* tPlayer, int tIsDarkening);
void setDreamSuperPausePlayer2DefenseMultiplier(DreamPlayer* tPlayer, double tMultiplier);
void setDreamSuperPausePowerToAdd(DreamPlayer* tPlayer, int tPowerToAdd);
void setDreamSuperPausePlayerUnhittability(DreamPlayer* tPlayer, int tIsUnhittable);
int setDreamSuperPauseActiveAndReturnIfWorked(DreamPlayer* tPlayer);
int isDreamSuperPauseActive();
int getDreamSuperPauseTimeSinceStart();
DreamPlayer* getDreamSuperPauseOwner();

void setDreamPauseTime(DreamPlayer* tPlayer, int tTime);
void setDreamPauseBufferTimeForCommandsDuringPauseEnd(DreamPlayer* tPlayer, int tBufferTime);
int getDreamPauseMoveTime();
void setDreamPauseMoveTime(DreamPlayer* tPlayer, int tMoveTime);
void setDreamPauseIsPausingBG(DreamPlayer* tPlayer, int tIsPausingBG);
int setDreamPauseActiveAndReturnIfWorked(DreamPlayer* tPlayer);
int isDreamPauseActive();
int getDreamPauseTimeSinceStart();
DreamPlayer* getDreamPauseOwner();

int isDreamAnyPauseActive();