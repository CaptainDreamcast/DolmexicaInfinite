#pragma once

#include <prism/actorhandler.h>

void loadMugenConfig();

double getDreamDefaultAttackDamageDoneToPowerMultiplier();
double getDreamDefaultAttackDamageReceivedToPowerMultiplier();

int isMugenDebugActive();

void setDefaultOptionVariables();
int getDifficulty();
void setDifficulty(int tDifficulty);
double getLifeStartPercentage();
int getLifeStartPercentageNumber();
void setLifeStartPercentageNumber(int tLifeStartPercentageNumber);
int isGlobalTimerInfinite();
void setGlobalTimerInfinite();
int getGlobalTimerDuration();
void setGlobalTimerDuration(int tDuration);
int getGlobalGameSpeed();
void setGlobalGameSpeed(int tGameSpeed);
int getGameWavVolume();
void setGameWavVolume(int tWavVolume);
int getGameMidiVolume();
void setGameMidiVolume(int tMidiVolume);