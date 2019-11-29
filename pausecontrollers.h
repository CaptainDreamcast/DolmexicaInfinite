#pragma once

#include <prism/actorhandler.h>
#include <prism/geometry.h>

#include "playerdefinition.h"

ActorBlueprint getPauseControllerHandler();

void setDreamSuperPauseTime(DreamPlayer* tPlayer, int tTime);
void setDreamSuperPauseBufferTimeForCommandsDuringPauseEnd(DreamPlayer* tPlayer, int tBufferTime);
void setDreamSuperPauseMoveTime(DreamPlayer* tPlayer, int tMoveTime);
void setDreamSuperPauseIsPausingBG(DreamPlayer* tPlayer, int tIsPausingBG);
void setDreamSuperPauseAnimation(DreamPlayer* tPlayer, int tIsInPlayerFile, int tAnimationNumber);
void setDreamSuperPauseSound(DreamPlayer* tPlayer, int tIsInPlayerFile, int tSoundGroup, int tSoundItem);
void setDreamSuperPausePosition(DreamPlayer* tPlayer, double tX, double tY);
void setDreamSuperPauseDarkening(DreamPlayer* tPlayer, int tIsDarkening);
void setDreamSuperPausePlayer2DefenseMultiplier(DreamPlayer* tPlayer, double tMultiplier);
void setDreamSuperPausePowerToAdd(DreamPlayer* tPlayer, int tPowerToAdd);
void setDreamSuperPausePlayerUnhittability(DreamPlayer* tPlayer, int tIsUnhittable);
void setDreamSuperPauseActive(DreamPlayer* tPlayer);
int isDreamSuperPauseActive();

void setDreamPauseTime(DreamPlayer* tPlayer, int tTime);
void setDreamPauseBufferTimeForCommandsDuringPauseEnd(DreamPlayer* tPlayer, int tBufferTime);
void setDreamPauseMoveTime(DreamPlayer* tPlayer, int tMoveTime);
void setDreamPauseIsPausingBG(DreamPlayer* tPlayer, int tIsPausingBG);
void setDreamPauseActive(DreamPlayer* tPlayer);
int isDreamPauseActive();

int isDreamAnyPauseActive();