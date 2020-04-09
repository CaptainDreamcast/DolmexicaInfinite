#pragma once

#include <string>

#include <prism/actorhandler.h>
#include <prism/geometry.h>

void loadMugenConfig();
void loadMugenSystemFonts();
void loadMugenFightFonts();

std::string getGameTitle();
const std::string& getDolmexicaAssetFolder();

const std::string& getMotifPath();

double getDreamDefaultAttackDamageDoneToPowerMultiplier();
double getDreamDefaultAttackDamageReceivedToPowerMultiplier();
double getDreamSuperTargetDefenseMultiplier();

int isMugenDebugActive();
int isMugenDebugAllowingDebugModeSwitch();
int isMugenDebugAllowingDebugKeysOutsideDebugMode();
const std::string& getMugenConfigStartStage();

int isUsingStaticAssignments();
double getConfigGameSpeedTimeFactor();
int isDrawingShadowsConfig();

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
double parseGameWavVolumeToPrism(int tWavVolume);
int getGameWavVolume();
int getUnscaledGameWavVolume();
void setUnscaledGameWavVolume(int tWavVolume);
double parseGameMidiVolumeToPrism(int tMidiVolume);
int getGameMidiVolume();
int getUnscaledGameMidiVolume();
void setUnscaledGameMidiVolume(int tMidiVolume);
int getSoundAreStereoEffectsActive();
int getArcadeAIRandomColor();
Vector3DI getArcadeAIRampStart();
Vector3DI getArcadeAIRampEnd();
Vector3DI getSurvivalAIRampStart();
Vector3DI getSurvivalAIRampEnd();
int calculateAIRampDifficulty(int tCurrentFightZeroIndexed, const Vector3DI& tAIRampStart, const Vector3DI& tAIRampEnd);

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