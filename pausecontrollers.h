#pragma once

#include <prism/actorhandler.h>
#include <prism/geometry.h>

#include "playerdefinition.h"

extern ActorBlueprint DreamPauseHandler;

void setDreamSuperPausePlayer(DreamPlayer* tPlayer);
void setDreamSuperPauseTime(int tTime);
void setDreamSuperPauseBufferTimeForCommandsDuringPauseEnd(int tBufferTime);
void setDreamSuperPauseMoveTime(int tMoveTime);
void setDreamSuperPauseIsPausingBG(int tIsPausingBG);
void setDreamSuperPauseAnimation(int tIsInPlayerFile, int tAnimationNumber);
void setDreamSuperPauseSound(int tIsInPlayerFile, int tSoundGroup, int tSoundItem);
void setDreamSuperPausePosition(Position tPosition);
void setDreamSuperPauseDarkening(int tIsDarkening);
void setDreamSuperPausePlayer2DefenseMultiplier(double tMultiplier);
void setDreamSuperPausePowerToAdd(int tPowerToAdd);
void setDreamSuperPausePlayerUnhittability(int tIsUnhittable);
void setDreamSuperPauseActive();