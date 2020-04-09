#include "config.h"

#include <algorithm>

#include <prism/mugendefreader.h>
#include <prism/memoryhandler.h>
#include <prism/system.h>
#include <prism/sound.h>
#include <prism/soundeffect.h>
#include <prism/stlutil.h>
#include <prism/log.h>
#include <prism/mugentexthandler.h>
#include <prism/wrapper.h>

#include "fightdebug.h"

using namespace std;

typedef struct {
	int mDifficulty;
	int mLifeStartPercentageNumber;
	int mIsTimerInfinite;
	int mTimerDuration;
	int mGameSpeed;
	int mWavVolume;
	int mMidiVolume;
} ConfigOptionsResettableData;

typedef struct {
	ConfigOptionsResettableData mActive;
	ConfigOptionsResettableData mDefault;
	std::string mMotif;
} ConfigOptionsData;

typedef struct {
	double mDefaultAttackDamageDoneToPowerMultiplier;
	double mDefaultAttackDamageReceivedToPowerMultiplier;
	double mSuperTargetDefenseMultiplier;
} ConfigRulesData;

typedef struct {
	int mIsUsingStaticAssignments;
	double mGameSpeedFactor;
	int mIsDrawingShadows;
} ConfigConfigData;

typedef struct {
	int mDebug;
	int mAllowDebugMode;
	int mAllowDebugKeys;
	int mSpeedup;
	std::string mStartStage;
} ConfigDebugData;

typedef struct {
	int mIsPlayingSound;
	int mAreStereoEffectsActive;

	double mMasterWavVolumeFactor;

	double mWavVolumeFactor;
	double mMidiVolumeFactor;
	double mMP3VolumeFactor;
	double mModVolumeFactor;
	double mCDAVolumeFactor;

	int mIsPlayingMidi;
	int mIsPlayingMP3;
	int mIsPlayingMod;
	int mIsPlayingCDA;
} ConfigSoundData;

typedef struct {
	std::string mTitle;
} ConfigMiscData;

typedef struct {
	int mAIRandomColor;

	Vector3DI mArcadeAIRampStart;
	Vector3DI mArcadeAIRampEnd;
	Vector3DI mSurvivalAIRampStart;
	Vector3DI mSurvivalAIRampEnd;
} ConfigArcadeData;

static struct {
	ConfigOptionsData mOptions;
	ConfigRulesData mRules;
	ConfigConfigData mConfig;
	ConfigDebugData mDebug;
	ConfigSoundData mSound;
	ConfigMiscData mMisc;
	ConfigArcadeData mArcade;

	map<int, int> mGlobalVariables;
	map<int, double> mGlobalFVariables;
	map<int, std::string> mGlobalStringVariables;
	std::string mAssetFolder;
} gConfigData;

static void loadConfigOptions(MugenDefScript* tScript) {
	gConfigData.mOptions.mDefault.mDifficulty = getMugenDefIntegerOrDefault(tScript, "Options", "difficulty", 4);
	gConfigData.mOptions.mDefault.mLifeStartPercentageNumber = getMugenDefIntegerOrDefault(tScript, "Options", "life", 100);
	gConfigData.mOptions.mDefault.mIsTimerInfinite = false;
	gConfigData.mOptions.mDefault.mTimerDuration = getMugenDefIntegerOrDefault(tScript, "Options", "time", 99);
	gConfigData.mOptions.mDefault.mGameSpeed = getMugenDefIntegerOrDefault(tScript, "Options", "gamespeed", 0);
	gConfigData.mOptions.mDefault.mWavVolume = getMugenDefIntegerOrDefault(tScript, "Options", "wavvolume", 50);
	gConfigData.mOptions.mDefault.mMidiVolume = getMugenDefIntegerOrDefault(tScript, "Options", "midivolume", 50);
	
	gConfigData.mOptions.mMotif = getSTLMugenDefStringOrDefault(tScript, "Options", "motif", "data/system.def");
}

static void loadConfigRules(MugenDefScript* tScript) {
	gConfigData.mRules.mDefaultAttackDamageDoneToPowerMultiplier = getMugenDefFloatOrDefault(tScript, "Rules", "default.attack.lifetopowermul", 0.7);
	gConfigData.mRules.mDefaultAttackDamageReceivedToPowerMultiplier = getMugenDefFloatOrDefault(tScript, "Rules", "default.gethit.lifetopowermul", 0.6);
	gConfigData.mRules.mSuperTargetDefenseMultiplier = getMugenDefFloatOrDefault(tScript, "Rules", "super.targetdefencemul", 1.5);
}

static void loadConfigConfig(MugenDefScript* tScript) {
	gConfigData.mConfig.mIsUsingStaticAssignments = getMugenDefIntegerOrDefault(tScript, "Config", "staticassignments", 0);
	const auto gameSpeed = getMugenDefIntegerOrDefault(tScript, "Config", "gamespeed", 60);
	gConfigData.mConfig.mGameSpeedFactor = gameSpeed / 60.0;
	gConfigData.mConfig.mIsDrawingShadows = getMugenDefIntegerOrDefault(tScript, "Config", "drawshadows", 1);

	setWrapperTimeDilatation(gConfigData.mConfig.mGameSpeedFactor);
}

static void loadConfigDebug(MugenDefScript* tScript) {
	gConfigData.mDebug.mDebug = getMugenDefIntegerOrDefault(tScript, "Debug", "debug", 0);
	if (gConfigData.mDebug.mDebug) {
		setFightDebugToPlayerOneBeforeFight();
	}
	gConfigData.mDebug.mAllowDebugMode = getMugenDefIntegerOrDefault(tScript, "Debug", "allowdebugmode", 1);
	gConfigData.mDebug.mAllowDebugKeys = getMugenDefIntegerOrDefault(tScript, "Debug", "allowdebugkeys", 0);
	gConfigData.mDebug.mSpeedup = getMugenDefIntegerOrDefault(tScript, "Debug", "speedup", 0);
	if (gConfigData.mDebug.mSpeedup) {
		static const auto MAXIMUM_SPEEDUP_FACTOR = 900;
		setGlobalGameSpeed(MAXIMUM_SPEEDUP_FACTOR);
	}
	gConfigData.mDebug.mStartStage = getSTLMugenDefStringOrDefault(tScript, "Debug", "startstage", "stages/stage0.def");
}

static void loadConfigSound(MugenDefScript* tScript) {
	gConfigData.mSound.mIsPlayingSound = getMugenDefIntegerOrDefault(tScript, "Sound Win", "sound", 1);
	gConfigData.mSound.mAreStereoEffectsActive = getMugenDefIntegerOrDefault(tScript, "Sound Win", "stereoeffects", 1);

	gConfigData.mSound.mMasterWavVolumeFactor = getMugenDefIntegerOrDefault(tScript, "Sound Win", "masterwavvolume", 255) / 255.0;
	
	gConfigData.mSound.mWavVolumeFactor = getMugenDefIntegerOrDefault(tScript, "Sound Win", "wavvolume", 128) / 255.0;
	gConfigData.mSound.mMidiVolumeFactor = getMugenDefIntegerOrDefault(tScript, "Sound Win", "midivolume", 128) / 255.0;
	gConfigData.mSound.mMP3VolumeFactor = getMugenDefIntegerOrDefault(tScript, "Sound Win", "mp3volume", 128) / 255.0;
	gConfigData.mSound.mModVolumeFactor = getMugenDefIntegerOrDefault(tScript, "Sound Win", "modvolume", 128) / 255.0;
	const auto cdaValue = getMugenDefIntegerOrDefault(tScript, "Sound Win", "cdavolume", -1);
	gConfigData.mSound.mCDAVolumeFactor = (cdaValue == -1) ? 1.0 : (cdaValue / 255.0);

	gConfigData.mSound.mIsPlayingMidi = getMugenDefIntegerOrDefault(tScript, "Sound Win", "playmidi", 1);
	gConfigData.mSound.mIsPlayingMP3 = getMugenDefIntegerOrDefault(tScript, "Sound Win", "playmp3", 1);
	gConfigData.mSound.mIsPlayingMod = getMugenDefIntegerOrDefault(tScript, "Sound Win", "playmod", 1);
	gConfigData.mSound.mIsPlayingCDA = getMugenDefIntegerOrDefault(tScript, "Sound Win", "playcda", 1);
}

static void loadWindowTitle(MugenDefScript* tScript) {
	if (isOnDreamcast()) return;
	if (!isMugenDefStringVariable(tScript, "Misc", "title")) return;

	gConfigData.mMisc.mTitle = getSTLMugenDefStringOrDefault(tScript, "Misc", "title", "DOLMEXICA INFINITE");
	updateGameName(gConfigData.mMisc.mTitle.c_str());

	string iconPath = getSTLMugenDefStringOrDefault(tScript, "Misc", "icon", "");
	if (isFile(getDolmexicaAssetFolder() + "data/" + iconPath)) {
		setIcon((getDolmexicaAssetFolder() + "data/" + iconPath).c_str());
	}
}

static void loadConfigArcade(MugenDefScript* tScript) {
	gConfigData.mArcade.mAIRandomColor = getMugenDefIntegerOrDefault(tScript, "Arcade", "ai.randomcolor", 1);
	
	gConfigData.mArcade.mArcadeAIRampStart = getMugenDefVectorIOrDefault(tScript, "Arcade", "arcade.airamp.start", makeVector3DI(2, 0, 0));
	gConfigData.mArcade.mArcadeAIRampEnd = getMugenDefVectorIOrDefault(tScript, "Arcade", "arcade.airamp.end", makeVector3DI(4, 3, 0));
	gConfigData.mArcade.mSurvivalAIRampStart = getMugenDefVectorIOrDefault(tScript, "Arcade", "survival.airamp.start", makeVector3DI(0, -3, 0));
	gConfigData.mArcade.mSurvivalAIRampEnd = getMugenDefVectorIOrDefault(tScript, "Arcade", "survival.airamp.end", makeVector3DI(16, 4, 0));
}

static void loadDolmexicaAssetFolder()
{
	if (isFile("assets/data/dolmexica.cfg") || isFile("assets/data/mugen.cfg")) {
		gConfigData.mAssetFolder = "assets/";
	}
	else {
		gConfigData.mAssetFolder = "";
	}
}

void loadMugenConfig() {
	loadDolmexicaAssetFolder();
	// TODO: load saved values (https://dev.azure.com/captdc/DogmaRnDA/_workitems/edit/201)

	MugenDefScript script;
	if (isFile(getDolmexicaAssetFolder() + "data/dolmexica.cfg")) {
		loadMugenDefScript(&script, getDolmexicaAssetFolder() + "data/dolmexica.cfg");
	}
	else {
		loadMugenDefScript(&script, getDolmexicaAssetFolder() + "data/mugen.cfg");
	}
	loadConfigOptions(&script);
	loadConfigRules(&script);
	loadConfigConfig(&script);
	loadConfigDebug(&script);
	loadConfigSound(&script);
	loadWindowTitle(&script);
	loadConfigArcade(&script);
	unloadMugenDefScript(script);

	setDefaultOptionVariables();
}

void loadMugenSystemFonts() {
	MugenDefScript script;
	loadMugenDefScript(&script, getDolmexicaAssetFolder() + getMotifPath());
	loadMugenFontsFromScript(&script);
	unloadMugenDefScript(script);
}

void loadMugenFightFonts()
{
	MugenDefScript script;
	loadMugenDefScript(&script, getDolmexicaAssetFolder() + "data/fight.def");
	loadMugenFontsFromScript(&script);
	unloadMugenDefScript(script);
}

std::string getGameTitle()
{
	return gConfigData.mMisc.mTitle;
}

const std::string& getDolmexicaAssetFolder()
{
	return gConfigData.mAssetFolder;
}

const std::string& getMotifPath()
{
	return gConfigData.mOptions.mMotif;
}

double getDreamDefaultAttackDamageDoneToPowerMultiplier()
{
	return gConfigData.mRules.mDefaultAttackDamageDoneToPowerMultiplier;
}

double getDreamDefaultAttackDamageReceivedToPowerMultiplier()
{
	return gConfigData.mRules.mDefaultAttackDamageReceivedToPowerMultiplier;
}

double getDreamSuperTargetDefenseMultiplier()
{
	return gConfigData.mRules.mSuperTargetDefenseMultiplier;
}

int isMugenDebugActive()
{
	return gConfigData.mDebug.mDebug || gConfigData.mDebug.mAllowDebugMode || gConfigData.mDebug.mAllowDebugKeys;
}

int isMugenDebugAllowingDebugModeSwitch()
{
	return gConfigData.mDebug.mDebug || gConfigData.mDebug.mAllowDebugMode;
}

int isMugenDebugAllowingDebugKeysOutsideDebugMode()
{
	return gConfigData.mDebug.mAllowDebugKeys;
}

const std::string& getMugenConfigStartStage()
{
	return gConfigData.mDebug.mStartStage;
}

int isUsingStaticAssignments()
{
	return gConfigData.mConfig.mIsUsingStaticAssignments;
}

double getConfigGameSpeedTimeFactor()
{
	return gConfigData.mConfig.mGameSpeedFactor;
}

int isDrawingShadowsConfig()
{
	return gConfigData.mConfig.mIsDrawingShadows;
}

void setDefaultOptionVariables() {
	gConfigData.mOptions.mActive = gConfigData.mOptions.mDefault;

	setVolume(parseGameWavVolumeToPrism(getGameWavVolume()));
	setSoundEffectVolume(parseGameMidiVolumeToPrism(getGameMidiVolume()));
}

int getDifficulty()
{
	return gConfigData.mOptions.mActive.mDifficulty;
}

void setDifficulty(int tDifficulty)
{
	gConfigData.mOptions.mActive.mDifficulty = tDifficulty;
}

double getLifeStartPercentage()
{
	return gConfigData.mOptions.mActive.mLifeStartPercentageNumber / 100.0;
}

int getLifeStartPercentageNumber()
{
	return gConfigData.mOptions.mActive.mLifeStartPercentageNumber;
}

void setLifeStartPercentageNumber(int tLifeStartPercentageNumber)
{
	gConfigData.mOptions.mActive.mLifeStartPercentageNumber = tLifeStartPercentageNumber;
}

int isGlobalTimerInfinite()
{
	return gConfigData.mOptions.mActive.mIsTimerInfinite;
}

void setGlobalTimerInfinite()
{
	gConfigData.mOptions.mActive.mIsTimerInfinite = 1;
}

int getGlobalTimerDuration()
{
	return gConfigData.mOptions.mActive.mTimerDuration;
}

void setGlobalTimerDuration(int tDuration)
{
	gConfigData.mOptions.mActive.mTimerDuration = tDuration;
	gConfigData.mOptions.mActive.mIsTimerInfinite = 0;
}

int getGlobalGameSpeed()
{
	return gConfigData.mOptions.mActive.mGameSpeed;
}

void setGlobalGameSpeed(int tGameSpeed)
{
	gConfigData.mOptions.mActive.mGameSpeed = tGameSpeed;
}

double parseGameWavVolumeToPrism(int tWavVolume)
{
	return tWavVolume / 100.0;
}

int getGameWavVolume()
{
	return int(gConfigData.mOptions.mActive.mWavVolume * gConfigData.mSound.mWavVolumeFactor * gConfigData.mSound.mMasterWavVolumeFactor * gConfigData.mSound.mIsPlayingSound);
}

int getUnscaledGameWavVolume()
{
	return gConfigData.mOptions.mActive.mWavVolume;
}

void setUnscaledGameWavVolume(int tWavVolume)
{
	gConfigData.mOptions.mActive.mWavVolume = tWavVolume;
	setVolume(parseGameWavVolumeToPrism(getGameWavVolume()));
}

double parseGameMidiVolumeToPrism(int tMidiVolume)
{
	return tMidiVolume / 100.0;
}

int getGameMidiVolume()
{
	return int(gConfigData.mOptions.mActive.mMidiVolume * gConfigData.mSound.mMidiVolumeFactor * gConfigData.mSound.mIsPlayingMidi * gConfigData.mSound.mIsPlayingSound);
}

int getUnscaledGameMidiVolume()
{
	return gConfigData.mOptions.mActive.mMidiVolume;
}

void setUnscaledGameMidiVolume(int tMidiVolume)
{
	gConfigData.mOptions.mActive.mMidiVolume = tMidiVolume;
	setSoundEffectVolume(parseGameMidiVolumeToPrism(getGameMidiVolume()));
}

int getSoundAreStereoEffectsActive() {
	return gConfigData.mSound.mAreStereoEffectsActive;
}

int getArcadeAIRandomColor()
{
	return gConfigData.mArcade.mAIRandomColor;
}

Vector3DI getArcadeAIRampStart()
{
	return gConfigData.mArcade.mArcadeAIRampStart;
}

Vector3DI getArcadeAIRampEnd()
{
	return gConfigData.mArcade.mArcadeAIRampEnd;
}

Vector3DI getSurvivalAIRampStart()
{
	return gConfigData.mArcade.mSurvivalAIRampStart;
}

Vector3DI getSurvivalAIRampEnd()
{
	return gConfigData.mArcade.mSurvivalAIRampEnd;
}

int calculateAIRampDifficulty(int tCurrentFightZeroIndexed, const Vector3DI& tAIRampStart, const Vector3DI& tAIRampEnd)
{
	const auto baseDifficulty = getDifficulty();
	int difficulty;
	if (tCurrentFightZeroIndexed <= tAIRampStart.x) {
		difficulty = baseDifficulty + tAIRampStart.y;
	}
	else if (tCurrentFightZeroIndexed >= tAIRampEnd.x) {
		difficulty = baseDifficulty + tAIRampEnd.y;
	}
	else {
		const auto t = (tCurrentFightZeroIndexed - tAIRampStart.x) / double(tAIRampEnd.x - tAIRampStart.x);
		const auto offset = int(tAIRampStart.y + t * (tAIRampEnd.y - tAIRampStart.y));
		difficulty = baseDifficulty + offset;
	}

	return std::min(std::max(1, difficulty), 8);
}

void setGlobalVariable(int tIndex, int tValue)
{
	gConfigData.mGlobalVariables[tIndex] = tValue;
}

void addGlobalVariable(int tIndex, int tValue)
{
	gConfigData.mGlobalVariables[tIndex] += tValue;
}

int getGlobalVariable(int tIndex)
{
	return gConfigData.mGlobalVariables[tIndex];
}

void setGlobalFloatVariable(int tIndex, double tValue)
{
	gConfigData.mGlobalFVariables[tIndex] = tValue;
}

void addGlobalFloatVariable(int tIndex, double tValue)
{
	gConfigData.mGlobalFVariables[tIndex] += tValue;
}

double getGlobalFloatVariable(int tIndex)
{
	return gConfigData.mGlobalFVariables[tIndex];
}

void setGlobalStringVariable(int tID, std::string tValue)
{
	gConfigData.mGlobalStringVariables[tID] = tValue;
}

void addGlobalStringVariable(int tID, std::string tValue)
{
	gConfigData.mGlobalStringVariables[tID] = gConfigData.mGlobalStringVariables[tID] + tValue;
}

void addGlobalStringVariable(int tID, int tValue)
{
	gConfigData.mGlobalStringVariables[tID][0] += (char)tValue;
}

std::string getGlobalStringVariable(int tID)
{
	return gConfigData.mGlobalStringVariables[tID];
}
