#pragma once

#include <string>

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

void setGlobalVariable(int tIndex, int tValue);
void addGlobalVariable(int tIndex, int tValue);
int getGlobalVariable(int tIndex);
void setGlobalFloatVariable(int tIndex, double tValue);
void addGlobalFloatVariable(int tIndex, double tValue);
double getGlobalFloatVariable(int tIndex);
void setGlobalStringVariable(int tID, std::string tValue);
void addGlobalStringVariable(int tID, std::string tValue);
void addGlobalStringVariable(int tID, int tValue);
std::string getGlobalStringVariable(int tID);