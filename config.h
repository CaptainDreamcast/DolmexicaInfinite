#pragma once

#include <string>

#include <prism/actorhandler.h>
#include <prism/geometry.h>
#include <prism/saveload.h>

using namespace prism;

void loadMugenConfig();
void loadMugenSystemFonts();
void loadMugenFightFonts();
void loadMugenStoryFonts(const char* tPath, const char* tGroupName);

std::string getGameTitle();
const std::string& getDolmexicaAssetFolder();

const std::string& getMotifPath();
std::string findMugenSystemOrFightFilePath(const std::string& tFile, const std::string& tFolder);

float getDreamDefaultAttackDamageDoneToPowerMultiplier();
float getDreamDefaultAttackDamageReceivedToPowerMultiplier();
float getDreamSuperTargetDefenseMultiplier();

int isMugenDebugActive();
int isMugenDebugAllowingDebugModeSwitch();
int isMugenDebugAllowingDebugKeysOutsideDebugMode();
const std::string& getMugenConfigStartStage();

int isUsingStaticAssignments();
float getConfigGameSpeedTimeFactor();
int isDrawingShadowsConfig();

void setDefaultOptionVariables();
int getDifficulty();
void setDifficulty(int tDifficulty);
float getLifeStartPercentage();
int getLifeStartPercentageNumber();
void setLifeStartPercentageNumber(int tLifeStartPercentageNumber);
int isGlobalTimerInfinite();
void setGlobalTimerInfinite();
int getGlobalTimerDuration();
void setGlobalTimerDuration(int tDuration);
int getGlobalGameSpeed();
void setGlobalGameSpeed(int tGameSpeed);
float parseGameWavVolumeToPrism(int tWavVolume);
int getGameWavVolume();
int getUnscaledGameWavVolume();
void setUnscaledGameWavVolume(int tWavVolume);
float parseGameMidiVolumeToPrism(int tMidiVolume);
int getGameMidiVolume();
int getUnscaledGameMidiVolume();
void setUnscaledGameMidiVolume(int tMidiVolume);
int getSoundAreStereoEffectsActive();
float getSoundPanningWidthFactor();
int getArcadeAIRandomColor();
int getArcadeAICheat();
Vector3DI getArcadeAIRampStart();
Vector3DI getArcadeAIRampEnd();
Vector3DI getSurvivalAIRampStart();
Vector3DI getSurvivalAIRampEnd();
int calculateAIRampDifficulty(int tCurrentFightZeroIndexed, const Vector3DI& tAIRampStart, const Vector3DI& tAIRampEnd);

void setGlobalVariable(int tIndex, int tValue);
void addGlobalVariable(int tIndex, int tValue);
int getGlobalVariable(int tIndex);
void setGlobalFloatVariable(int tIndex, float tValue);
void addGlobalFloatVariable(int tIndex, float tValue);
float getGlobalFloatVariable(int tIndex);
void setGlobalStringVariable(int tID, const std::string& tValue);
void addGlobalStringVariable(int tID, const std::string& tValue);
void addGlobalStringVariable(int tID, int tValue);
const std::string& getGlobalStringVariable(int tID);

void loadMugenConfigSave(PrismSaveSlot tSaveSlot);
void saveMugenConfigSave(PrismSaveSlot tSaveSlot);
void deleteMugenConfigSave(PrismSaveSlot tSaveSlot);
int hasMugenConfigSave(PrismSaveSlot tSaveSlot);
size_t getMugenConfigSaveSize();

void loadGlobalVariables(PrismSaveSlot tSaveSlot);
void saveGlobalVariables(PrismSaveSlot tSaveSlot);
void deleteGlobalVariables(PrismSaveSlot tSaveSlot);
int hasGlobalVariablesSave(PrismSaveSlot tSaveSlot);
size_t getGlobalVariablesSaveSize();
