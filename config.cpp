#include "config.h"

#include <assert.h>

#include <algorithm>

#include <prism/mugendefreader.h>
#include <prism/mugendefwriter.h>
#include <prism/memoryhandler.h>
#include <prism/system.h>
#include <prism/sound.h>
#include <prism/soundeffect.h>
#include <prism/stlutil.h>
#include <prism/log.h>
#include <prism/mugentexthandler.h>
#include <prism/wrapper.h>
#include <prism/input.h>

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
	double mPanningWidthFactor;

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
	int mAiCheat;

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
	gConfigData.mSound.mPanningWidthFactor = getMugenDefIntegerOrDefault(tScript, "Sound Win", "panningwidth", 240) / 255.0;

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
	gConfigData.mArcade.mAiCheat = getMugenDefIntegerOrDefault(tScript, "Arcade", "ai.cheat", 1);
	
	gConfigData.mArcade.mArcadeAIRampStart = getMugenDefVectorIOrDefault(tScript, "Arcade", "arcade.airamp.start", makeVector3DI(2, 0, 0));
	gConfigData.mArcade.mArcadeAIRampEnd = getMugenDefVectorIOrDefault(tScript, "Arcade", "arcade.airamp.end", makeVector3DI(4, 3, 0));
	gConfigData.mArcade.mSurvivalAIRampStart = getMugenDefVectorIOrDefault(tScript, "Arcade", "survival.airamp.start", makeVector3DI(0, -3, 0));
	gConfigData.mArcade.mSurvivalAIRampEnd = getMugenDefVectorIOrDefault(tScript, "Arcade", "survival.airamp.end", makeVector3DI(16, 4, 0));
}

static void loadConfigKeysSinglePlayer(MugenDefScript* tScript, int tIndex) {
	std::stringstream ss;
	ss << "P" << (tIndex + 1) << " Keys Dolmexica";
	const auto groupName = ss.str();
	auto key = getMugenDefIntegerOrDefault(tScript, groupName.c_str(), "jump", int(getButtonForKeyboard(tIndex, CONTROLLER_UP_PRISM)));
	setButtonForKeyboard(tIndex, CONTROLLER_UP_PRISM, KeyboardKeyPrism(key));
	key = getMugenDefIntegerOrDefault(tScript, groupName.c_str(), "crouch", int(getButtonForKeyboard(tIndex, CONTROLLER_DOWN_PRISM)));
	setButtonForKeyboard(tIndex, CONTROLLER_DOWN_PRISM, KeyboardKeyPrism(key));
	key = getMugenDefIntegerOrDefault(tScript, groupName.c_str(), "left", int(getButtonForKeyboard(tIndex, CONTROLLER_LEFT_PRISM)));
	setButtonForKeyboard(tIndex, CONTROLLER_LEFT_PRISM, KeyboardKeyPrism(key));
	key = getMugenDefIntegerOrDefault(tScript, groupName.c_str(), "right", int(getButtonForKeyboard(tIndex, CONTROLLER_RIGHT_PRISM)));
	setButtonForKeyboard(tIndex, CONTROLLER_RIGHT_PRISM, KeyboardKeyPrism(key));

	key = getMugenDefIntegerOrDefault(tScript, groupName.c_str(), "a", int(getButtonForKeyboard(tIndex, CONTROLLER_A_PRISM)));
	setButtonForKeyboard(tIndex, CONTROLLER_A_PRISM, KeyboardKeyPrism(key));
	key = getMugenDefIntegerOrDefault(tScript, groupName.c_str(), "b", int(getButtonForKeyboard(tIndex, CONTROLLER_B_PRISM)));
	setButtonForKeyboard(tIndex, CONTROLLER_B_PRISM, KeyboardKeyPrism(key));
	key = getMugenDefIntegerOrDefault(tScript, groupName.c_str(), "c", int(getButtonForKeyboard(tIndex, CONTROLLER_R_PRISM)));
	setButtonForKeyboard(tIndex, CONTROLLER_R_PRISM, KeyboardKeyPrism(key));

	key = getMugenDefIntegerOrDefault(tScript, groupName.c_str(), "x", int(getButtonForKeyboard(tIndex, CONTROLLER_X_PRISM)));
	setButtonForKeyboard(tIndex, CONTROLLER_X_PRISM, KeyboardKeyPrism(key));
	key = getMugenDefIntegerOrDefault(tScript, groupName.c_str(), "y", int(getButtonForKeyboard(tIndex, CONTROLLER_Y_PRISM)));
	setButtonForKeyboard(tIndex, CONTROLLER_Y_PRISM, KeyboardKeyPrism(key));
	key = getMugenDefIntegerOrDefault(tScript, groupName.c_str(), "z", int(getButtonForKeyboard(tIndex, CONTROLLER_L_PRISM)));
	setButtonForKeyboard(tIndex, CONTROLLER_L_PRISM, KeyboardKeyPrism(key));

	key = getMugenDefIntegerOrDefault(tScript, groupName.c_str(), "start", int(getButtonForKeyboard(tIndex, CONTROLLER_START_PRISM)));
	setButtonForKeyboard(tIndex, CONTROLLER_START_PRISM, KeyboardKeyPrism(key));
}

static void loadConfigKeys(MugenDefScript* tScript) {
	for (int i = 0; i < 2; i++) {
		loadConfigKeysSinglePlayer(tScript, i);
	}
}

static const std::string getDolmexicaConfigPathAfterAssetFolderSet() {
	if (isFile(gConfigData.mAssetFolder + "data/dolmexica.cfg")) {
		return gConfigData.mAssetFolder + "data/dolmexica.cfg";
	}
	else {
		return gConfigData.mAssetFolder + "data/mugen.cfg";
	}
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

	MugenDefScript script;
	const auto scriptPath = getDolmexicaConfigPathAfterAssetFolderSet();
	loadMugenDefScript(&script, scriptPath);
	loadConfigOptions(&script);
	loadConfigRules(&script);
	loadConfigConfig(&script);
	loadConfigDebug(&script);
	loadConfigSound(&script);
	loadWindowTitle(&script);
	loadConfigArcade(&script);
	loadConfigKeys(&script);
	unloadMugenDefScript(script);

	setDefaultOptionVariables();

	if (isOnDreamcast()) {
		loadMugenConfigSave(PrismSaveSlot::AUTOMATIC);
	}
}

void loadMugenSystemFonts() {
	MugenDefScript script;
	loadMugenDefScript(&script, getDolmexicaAssetFolder() + getMotifPath());
	loadMugenFontsFromScript(&script, "Files");
	unloadMugenDefScript(script);
}

void loadMugenFightFonts()
{
	MugenDefScript script;
	loadMugenDefScript(&script, getDolmexicaAssetFolder() + "data/fight.def");
	loadMugenFontsFromScript(&script, "Files");
	unloadMugenDefScript(script);
}

void loadMugenStoryFonts(const char* tPath)
{
	MugenDefScript script;
	loadMugenDefScript(&script, tPath);
	loadMugenFontsFromScript(&script, "Info");
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

double getSoundPanningWidthFactor()
{
	return gConfigData.mSound.mPanningWidthFactor;
}

int getArcadeAIRandomColor()
{
	return gConfigData.mArcade.mAIRandomColor;
}

int getArcadeAICheat()
{
	return gConfigData.mArcade.mAiCheat;
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

void setGlobalStringVariable(int tID, const std::string& tValue)
{
	gConfigData.mGlobalStringVariables[tID] = tValue;
}

void addGlobalStringVariable(int tID, const std::string& tValue)
{
	gConfigData.mGlobalStringVariables[tID] = gConfigData.mGlobalStringVariables[tID] + tValue;
}

void addGlobalStringVariable(int tID, int tValue)
{
	gConfigData.mGlobalStringVariables[tID][0] += (char)tValue;
}

const std::string& getGlobalStringVariable(int tID)
{
	return gConfigData.mGlobalStringVariables[tID];
}

#ifdef DREAMCAST
static constexpr auto OPTIONS_SAVE_FILENAME = "DOLINF_OPTIONS";

struct MugenOptionsDreamcastSaveData {
	uint8_t mVersion;
	uint8_t mDifficulty;
	uint16_t mLifeStartPercentageNumber;

	uint8_t mTimerDuration;
	int8_t mGameSpeed;
	uint8_t mWavVolume;
	uint8_t mMidiVolume;

	uint8_t mKeys[2][CONTROLLER_BUTTON_AMOUNT_PRISM];
};

static void loadMugenConfigDreamcast(PrismSaveSlot tSaveSlot) {
	if (!hasPrismGameSave(tSaveSlot, OPTIONS_SAVE_FILENAME)) {
		return;
	}

	auto b = loadPrismGameSave(tSaveSlot, OPTIONS_SAVE_FILENAME);
	MugenOptionsDreamcastSaveData saveData = *((MugenOptionsDreamcastSaveData*)b.mData);

	gConfigData.mOptions.mDefault.mDifficulty = saveData.mDifficulty;
	gConfigData.mOptions.mDefault.mLifeStartPercentageNumber = saveData.mLifeStartPercentageNumber;
	gConfigData.mOptions.mDefault.mIsTimerInfinite = false;
	gConfigData.mOptions.mDefault.mTimerDuration = saveData.mTimerDuration;
	gConfigData.mOptions.mDefault.mGameSpeed = saveData.mGameSpeed;
	gConfigData.mOptions.mDefault.mWavVolume = saveData.mWavVolume;
	gConfigData.mOptions.mDefault.mMidiVolume = saveData.mMidiVolume;

	for (size_t i = 0; i < 2; i++) {
		for (size_t j = 0; j < size_t(CONTROLLER_BUTTON_AMOUNT_PRISM); j++) {
			setButtonForController(i, ControllerButtonPrism(j), ControllerButtonPrism(saveData.mKeys[i][j]));
		}
	}

	freeBuffer(b);

	setDefaultOptionVariables();
}
#endif

void loadMugenConfigSave(PrismSaveSlot tSaveSlot)
{
#ifdef _WIN32
	(void)tSaveSlot;
	loadMugenConfig();
#elif defined(DREAMCAST)
	loadMugenConfigDreamcast(tSaveSlot);
#endif
}

#ifdef _WIN32
static void saveConfigOptionsWindows(ModifiableMugenDefScript* tScript) {
	saveMugenDefInteger(tScript, "Options", "difficulty", gConfigData.mOptions.mActive.mDifficulty);
	saveMugenDefInteger(tScript, "Options", "life", gConfigData.mOptions.mActive.mLifeStartPercentageNumber);
	saveMugenDefInteger(tScript, "Options", "time", gConfigData.mOptions.mActive.mTimerDuration);
	saveMugenDefInteger(tScript, "Options", "gamespeed", gConfigData.mOptions.mActive.mGameSpeed);
	saveMugenDefInteger(tScript, "Options", "wavvolume", gConfigData.mOptions.mActive.mWavVolume);
	saveMugenDefInteger(tScript, "Options", "midivolume", gConfigData.mOptions.mActive.mMidiVolume);
}

static void saveConfigKeysWindowSinglePlayer(ModifiableMugenDefScript* tScript, int tIndex) {
	std::stringstream ss;
	ss << "P" << (tIndex + 1) << " Keys Dolmexica";
	const auto groupName = ss.str();
	saveMugenDefInteger(tScript, groupName.c_str(), "jump", int(getButtonForKeyboard(tIndex, CONTROLLER_UP_PRISM)));
	saveMugenDefInteger(tScript, groupName.c_str(), "crouch", int(getButtonForKeyboard(tIndex, CONTROLLER_DOWN_PRISM)));
	saveMugenDefInteger(tScript, groupName.c_str(), "left", int(getButtonForKeyboard(tIndex, CONTROLLER_LEFT_PRISM)));
	saveMugenDefInteger(tScript, groupName.c_str(), "right", int(getButtonForKeyboard(tIndex, CONTROLLER_RIGHT_PRISM)));
	
	saveMugenDefInteger(tScript, groupName.c_str(), "a", int(getButtonForKeyboard(tIndex, CONTROLLER_A_PRISM)));
	saveMugenDefInteger(tScript, groupName.c_str(), "b", int(getButtonForKeyboard(tIndex, CONTROLLER_B_PRISM)));
	saveMugenDefInteger(tScript, groupName.c_str(), "c", int(getButtonForKeyboard(tIndex, CONTROLLER_R_PRISM)));
	
	saveMugenDefInteger(tScript, groupName.c_str(), "x", int(getButtonForKeyboard(tIndex, CONTROLLER_X_PRISM)));
	saveMugenDefInteger(tScript, groupName.c_str(), "y", int(getButtonForKeyboard(tIndex, CONTROLLER_Y_PRISM)));
	saveMugenDefInteger(tScript, groupName.c_str(), "z", int(getButtonForKeyboard(tIndex, CONTROLLER_L_PRISM)));
	
	saveMugenDefInteger(tScript, groupName.c_str(), "start", int(getButtonForKeyboard(tIndex, CONTROLLER_START_PRISM)));
}

static void saveConfigKeysWindows(ModifiableMugenDefScript* tScript) {
	for (int i = 0; i < 2; i++) {
		saveConfigKeysWindowSinglePlayer(tScript, i);
	}
}

static void saveMugenConfigWindows()
{
	const auto path = getDolmexicaConfigPathAfterAssetFolderSet();
	auto modifiableScript = openModifiableMugenDefScript(path);
	saveConfigOptionsWindows(&modifiableScript);
	saveConfigKeysWindows(&modifiableScript);
	saveModifiableMugenDefScript(&modifiableScript, path);
	closeModifiableMugenDefScript(&modifiableScript);
}

static void deleteMugenConfigWindows() {
	logWarning("Deleting config save irrelevant under Windows");
}

static int hasMugenConfigSaveWindows() {
	logWarning("Config save existence irrelevant under Windows");
	return 0;
}

static size_t getMugenConfigSaveSizeWindows() {
	logWarning("Config save size irrelevant under Windows");
	return 0;
}
#elif defined(DREAMCAST)
static MugenOptionsDreamcastSaveData createMugenConfigSaveSaveData() {
	MugenOptionsDreamcastSaveData saveData;
	saveData.mVersion = 1;
	saveData.mDifficulty = uint8_t(gConfigData.mOptions.mActive.mDifficulty);
	saveData.mLifeStartPercentageNumber = uint16_t(gConfigData.mOptions.mActive.mLifeStartPercentageNumber);
	saveData.mTimerDuration = uint8_t(gConfigData.mOptions.mActive.mTimerDuration);
	saveData.mGameSpeed = int8_t(gConfigData.mOptions.mActive.mGameSpeed);
	saveData.mWavVolume = uint8_t(gConfigData.mOptions.mActive.mWavVolume);
	saveData.mMidiVolume = uint8_t(gConfigData.mOptions.mActive.mMidiVolume);

	for (size_t i = 0; i < 2; i++) {
		for (size_t j = 0; j < size_t(CONTROLLER_BUTTON_AMOUNT_PRISM); j++) {
			saveData.mKeys[i][j] = uint8_t(getButtonForController(i, ControllerButtonPrism(j)));
		}
	}
	return saveData;
}

static uint16_t gIconPalette[DREAMCAST_SAVE_ICON_PALETTE_SIZE / 2] = {
0x0000, 0xF2D0, 0xF060, 0xFFFF, 0x0000, 0x0000, 0x0000, 0x0000,
0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
};

static uint8_t gOptionsIcon[DREAMCAST_SAVE_ICON_BUFFER_SIZE] = {
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x01, 0x11, 0x11, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x01, 0x11, 0x11, 0x11, 0x11, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x11, 0x11, 0x11, 0x00, 0x01, 0x11, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x11, 0x11, 0x10, 0x00, 0x00, 0x01, 0x11, 0x10, 0x00, 0x11, 0x11, 0x10, 0x00, 0x00, 0x00,
0x00, 0x11, 0x11, 0x10, 0x00, 0x00, 0x00, 0x01, 0x11, 0x11, 0x10, 0x00, 0x11, 0x00, 0x00, 0x00,
0x00, 0x11, 0x11, 0x10, 0x00, 0x00, 0x00, 0x00, 0x11, 0x11, 0x00, 0x00, 0x01, 0x10, 0x00, 0x00,
0x00, 0x22, 0x22, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00,
0x00, 0x02, 0x22, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x22, 0x20, 0x00, 0x00, 0x01, 0x10, 0x00,
0x00, 0x00, 0x22, 0x22, 0x00, 0x00, 0x00, 0x00, 0x22, 0x22, 0x22, 0x00, 0x00, 0x02, 0x20, 0x00,
0x00, 0x00, 0x02, 0x22, 0x20, 0x00, 0x00, 0x02, 0x22, 0x00, 0x02, 0x20, 0x00, 0x02, 0x20, 0x00,
0x00, 0x00, 0x00, 0x22, 0x22, 0x22, 0x22, 0x22, 0x20, 0x00, 0x00, 0x22, 0x20, 0x02, 0x20, 0x00,
0x00, 0x00, 0x00, 0x00, 0x22, 0x22, 0x22, 0x22, 0x00, 0x00, 0x00, 0x00, 0x22, 0x22, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x33, 0x33, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x30,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x30,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x30,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x30,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x30,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x30,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x33, 0x33, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static void saveMugenConfigDreamcast(PrismSaveSlot tSaveSlot) {
	auto saveData = createMugenConfigSaveSaveData();
	const auto freeSize = getAvailableSizeForSaveSlot(tSaveSlot);
	const auto saveSize = getPrismGameSaveSize(makeBuffer(&saveData, sizeof(MugenOptionsDreamcastSaveData)), "DOLMEXICA INFINITE", "DolInf Options", "DolInf Options", makeBuffer(gOptionsIcon, DREAMCAST_SAVE_ICON_BUFFER_SIZE), makeBuffer(gIconPalette, DREAMCAST_SAVE_ICON_PALETTE_SIZE));
	if (saveSize > freeSize) {
		logWarningFormat("Unable to save options. Save size: %d, available size: %d", saveSize, freeSize);
		return;
	}

	savePrismGameSave(tSaveSlot, OPTIONS_SAVE_FILENAME, makeBuffer(&saveData, sizeof(MugenOptionsDreamcastSaveData)), "DOLMEXICA INFINITE", "DolInf Options", "DolInf Options", makeBuffer(gOptionsIcon, DREAMCAST_SAVE_ICON_BUFFER_SIZE), makeBuffer(gIconPalette, DREAMCAST_SAVE_ICON_PALETTE_SIZE));
}

static void deleteMugenConfigDreamcast(PrismSaveSlot tSaveSlot) {
	if (!hasPrismGameSave(tSaveSlot, OPTIONS_SAVE_FILENAME)) {
		return;
	}
	deletePrismGameSave(tSaveSlot, OPTIONS_SAVE_FILENAME);
}

static int hasMugenConfigSaveDreamcast(PrismSaveSlot tSaveSlot) {
	return hasPrismGameSave(tSaveSlot, OPTIONS_SAVE_FILENAME);
}

static size_t getMugenConfigSaveSizeDreamcast() {
	auto saveData = createMugenConfigSaveSaveData();
	return getPrismGameSaveSize(makeBuffer(&saveData, sizeof(MugenOptionsDreamcastSaveData)), "DOLMEXICA INFINITE", "DolInf Options", "DolInf Options", makeBuffer(gOptionsIcon, DREAMCAST_SAVE_ICON_BUFFER_SIZE), makeBuffer(gIconPalette, DREAMCAST_SAVE_ICON_PALETTE_SIZE));
}
#endif

void saveMugenConfigSave(PrismSaveSlot tSaveSlot)
{
#ifdef _WIN32
	(void)tSaveSlot;
	saveMugenConfigWindows();
#elif defined(DREAMCAST)
	saveMugenConfigDreamcast(tSaveSlot);
#endif
}

void deleteMugenConfigSave(PrismSaveSlot tSaveSlot)
{
#ifdef _WIN32
	(void)tSaveSlot;
	deleteMugenConfigWindows();
#elif defined(DREAMCAST)
	deleteMugenConfigDreamcast(tSaveSlot);
#endif
}

int hasMugenConfigSave(PrismSaveSlot tSaveSlot)
{
#ifdef _WIN32
	(void)tSaveSlot;
	return hasMugenConfigSaveWindows();
#elif defined(DREAMCAST)
	return hasMugenConfigSaveDreamcast(tSaveSlot);
#else
	return 0;
#endif
}

size_t getMugenConfigSaveSize()
{
#ifdef _WIN32
	return getMugenConfigSaveSizeWindows();
#elif defined(DREAMCAST)
	return getMugenConfigSaveSizeDreamcast();
#else
	return 0;
#endif
}

#ifdef _WIN32
static std::string getGlobalVarSavePathAfterTitleSetWindows() {
	auto gameTitle = getGameTitle();
	if (gameTitle.empty()) gameTitle = "DolmexicaInfinite";
	return getDolmexicaAssetFolder() + "data/" + sanitizeFileNameWithInvalidCharacters(gameTitle) + "_save.def";
}

static void loadGlobalIntegerVariablesWindows(MugenDefScript* tScript) {
	if (!hasMugenDefScriptGroup(tScript, "Vars")) return;
	auto group = getMugenDefScriptGroup(tScript, "Vars");

	for (auto& keyValuePair : group->mElements) {
		const auto key = atoi(keyValuePair.first.c_str());
		const auto value = getMugenDefNumberVariableAsElement(&keyValuePair.second);
		gConfigData.mGlobalVariables[key] = value;
	}
}

static void loadGlobalFloatVariablesWindows(MugenDefScript* tScript) {
	if (!hasMugenDefScriptGroup(tScript, "FVars")) return;
	auto group = getMugenDefScriptGroup(tScript, "FVars");

	for (auto& keyValuePair : group->mElements) {
		const auto key = atoi(keyValuePair.first.c_str());
		const auto value = getMugenDefFloatVariableAsElement(&keyValuePair.second);
		gConfigData.mGlobalFVariables[key] = value;
	}
}

static void loadGlobalStringVariablesWindows(MugenDefScript* tScript) {
	if (!hasMugenDefScriptGroup(tScript, "SVars")) return;
	auto group = getMugenDefScriptGroup(tScript, "SVars");

	for (auto& keyValuePair : group->mElements) {
		const auto key = atoi(keyValuePair.first.c_str());
		const auto value = getSTLMugenDefStringVariableAsElement(&keyValuePair.second);
		gConfigData.mGlobalStringVariables[key] = value;
	}
}

static void loadGlobalVariablesWindows() {
	const auto path = getGlobalVarSavePathAfterTitleSetWindows();
	if (!isFile(path)) return;

	MugenDefScript script;
	loadMugenDefScript(&script, path.c_str());
	loadGlobalIntegerVariablesWindows(&script);
	loadGlobalFloatVariablesWindows(&script);
	loadGlobalStringVariablesWindows(&script);
	unloadMugenDefScript(script);
}
#elif defined(DREAMCAST)
static constexpr auto VARS_SAVE_FILENAME_PRE = "DI_";

static std::string getGlobalVarSaveFileNameAfterTitleSetDreamcast() {
	auto gameTitle = getGameTitle();
	if (gameTitle.empty()) gameTitle = "DolmexicaInfinite";
	return sanitizeFileNameWithInvalidCharacters(gameTitle);
}

static void loadGlobalVariablesDreamcast(PrismSaveSlot tSaveSlot) {
	auto fileName = getGlobalVarSaveFileNameAfterTitleSetDreamcast();
	turnStringUppercase(fileName);
	if (!hasPrismGameSave(tSaveSlot, (VARS_SAVE_FILENAME_PRE + fileName).c_str())) {
		return;
	}

	auto b = loadPrismGameSave(tSaveSlot, (VARS_SAVE_FILENAME_PRE + fileName).c_str());
	uint32_t version;
	auto p = getBufferPointer(b);
	readFromBufferPointer(&version, &p, sizeof(uint32_t));
	assert(version == 1);
	if (version != 1) {
		logWarningFormat("Global var save has invalid version: %d. Aborting load.", version);
		freeBuffer(b);
		return;
	}

	uint32_t varAmount;
	readFromBufferPointer(&varAmount, &p, sizeof(uint32_t));
	for (size_t i = 0; i < varAmount; i++) {
		int32_t key, val;
		readFromBufferPointer(&key, &p, sizeof(int32_t));
		readFromBufferPointer(&val, &p, sizeof(int32_t));
		gConfigData.mGlobalVariables[key] = val;
	}

	readFromBufferPointer(&varAmount, &p, sizeof(uint32_t));
	for (size_t i = 0; i < varAmount; i++) {
		int32_t key;
		float val;
		readFromBufferPointer(&key, &p, sizeof(int32_t));
		readFromBufferPointer(&val, &p, sizeof(float));
		gConfigData.mGlobalFVariables[key] = double(val);
	}

	readFromBufferPointer(&varAmount, &p, sizeof(uint32_t));
	for (size_t i = 0; i < varAmount; i++) {
		int32_t key;
		uint32_t len;
		std::string val;
		readFromBufferPointer(&key, &p, sizeof(int32_t));
		readFromBufferPointer(&len, &p, sizeof(uint32_t));
		std::vector<char> readBuffer(len + 2, '\0');
		readFromBufferPointer(readBuffer.data(), &p, len);
		gConfigData.mGlobalStringVariables[key] = std::string(readBuffer.data());
	}

	freeBuffer(b);
}
#endif

void loadGlobalVariables(PrismSaveSlot tSaveSlot)
{
#ifdef _WIN32
	(void)tSaveSlot;
	loadGlobalVariablesWindows();
#elif defined(DREAMCAST)
	loadGlobalVariablesDreamcast(tSaveSlot);
#endif
}

#ifdef _WIN32
static void saveGlobalIntegerVariablesWindows(ModifiableMugenDefScript* tScript) {
	for (const auto& keyValuePair : gConfigData.mGlobalVariables) {
		stringstream ss;
		ss << keyValuePair.first;
		saveMugenDefInteger(tScript, "Vars", ss.str().c_str(), keyValuePair.second);
	}
}

static void saveGlobalFloatVariablesWindows(ModifiableMugenDefScript* tScript) {
	for (const auto& keyValuePair : gConfigData.mGlobalFVariables) {
		stringstream ss;
		ss << keyValuePair.first;
		saveMugenDefFloat(tScript, "FVars", ss.str().c_str(), keyValuePair.second);
	}
}

static void saveGlobalStringVariablesWindows(ModifiableMugenDefScript* tScript) {
	for (const auto& keyValuePair : gConfigData.mGlobalStringVariables) {
		stringstream ss;
		ss << keyValuePair.first;
		saveMugenDefString(tScript, "SVars", ss.str().c_str(), "\"" + keyValuePair.second + "\"");
	}
}

static void saveGlobalVariablesWindows() {
	const auto path = getGlobalVarSavePathAfterTitleSetWindows();
	auto modifiableScript = openModifiableMugenDefScript(path);
	clearModifiableMugenDefScript(&modifiableScript);
	saveGlobalIntegerVariablesWindows(&modifiableScript);
	saveGlobalFloatVariablesWindows(&modifiableScript);
	saveGlobalStringVariablesWindows(&modifiableScript);
	saveModifiableMugenDefScript(&modifiableScript, path);
	closeModifiableMugenDefScript(&modifiableScript);
}

static void deleteGlobalVariablesWindows()
{
	logWarning("Deleting variable save irrelevant under Windows");
}

static int hasGlobalVariablesSaveWindows() {
	logWarning("Variable save existence irrelevant under Windows");
	return 0;
}

static size_t getGlobalVariablesSaveSizeWindows() {
	logWarning("Variable save size irrelevant under Windows");
	return 0;
}
#elif defined(DREAMCAST)
static Buffer buildGlobalVariableSaveData() {
	Buffer ret = makeBufferEmptyOwned();
	static const auto SAVE_VERSION = 1u;
	appendBufferUint32(&ret, SAVE_VERSION);

	appendBufferUint32(&ret, gConfigData.mGlobalVariables.size());
	for (auto& keyValuePair : gConfigData.mGlobalVariables) {
		appendBufferInt32(&ret, keyValuePair.first);
		appendBufferInt32(&ret, keyValuePair.second);
	}

	appendBufferUint32(&ret, gConfigData.mGlobalFVariables.size());
	for (auto& keyValuePair : gConfigData.mGlobalFVariables) {
		appendBufferInt32(&ret, keyValuePair.first);
		appendBufferFloat(&ret, float(keyValuePair.second));
	}

	appendBufferUint32(&ret, gConfigData.mGlobalStringVariables.size());
	for (auto& keyValuePair : gConfigData.mGlobalStringVariables) {
		const auto value = keyValuePair.second;
		std::vector<char> writeBuffer(value.begin(), value.end());
		writeBuffer.push_back('\0');
		while (writeBuffer.size() % 4) writeBuffer.push_back('\0');

		appendBufferInt32(&ret, keyValuePair.first);
		appendBufferUint32(&ret, writeBuffer.size());
		appendBufferBuffer(&ret, makeBuffer(writeBuffer.data(), writeBuffer.size()));
	}

	return ret;
}

static uint8_t gVarsIcon[DREAMCAST_SAVE_ICON_BUFFER_SIZE] = {
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x01, 0x11, 0x11, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x01, 0x11, 0x11, 0x11, 0x11, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x11, 0x11, 0x11, 0x00, 0x01, 0x11, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x11, 0x11, 0x10, 0x00, 0x00, 0x01, 0x11, 0x10, 0x00, 0x11, 0x11, 0x10, 0x00, 0x00, 0x00,
0x00, 0x11, 0x11, 0x10, 0x00, 0x00, 0x00, 0x01, 0x11, 0x11, 0x10, 0x00, 0x11, 0x00, 0x00, 0x00,
0x00, 0x11, 0x11, 0x10, 0x00, 0x00, 0x00, 0x00, 0x11, 0x11, 0x00, 0x00, 0x01, 0x10, 0x00, 0x00,
0x00, 0x22, 0x22, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00,
0x00, 0x02, 0x22, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x22, 0x20, 0x00, 0x00, 0x01, 0x10, 0x00,
0x00, 0x00, 0x22, 0x22, 0x00, 0x00, 0x00, 0x00, 0x22, 0x22, 0x22, 0x00, 0x00, 0x02, 0x20, 0x00,
0x00, 0x00, 0x02, 0x22, 0x20, 0x00, 0x00, 0x02, 0x22, 0x00, 0x02, 0x20, 0x00, 0x02, 0x20, 0x00,
0x00, 0x00, 0x00, 0x22, 0x22, 0x22, 0x22, 0x22, 0x20, 0x00, 0x00, 0x22, 0x20, 0x02, 0x20, 0x00,
0x00, 0x00, 0x00, 0x00, 0x22, 0x22, 0x22, 0x22, 0x00, 0x00, 0x00, 0x00, 0x22, 0x22, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x30,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x30,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x30,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x30,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x03, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x03, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x30, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x30, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static void saveGlobalVariablesDreamcast(PrismSaveSlot tSaveSlot) {
	auto b = buildGlobalVariableSaveData();
	auto fileName = getGlobalVarSaveFileNameAfterTitleSetDreamcast();
	const auto desc = fileName;
	turnStringUppercase(fileName);
	const auto freeSize = getAvailableSizeForSaveSlot(tSaveSlot);
	const auto saveSize = getPrismGameSaveSize(b, "DOLMEXICA INFINITE", desc.c_str(), desc.c_str(), makeBuffer(gVarsIcon, DREAMCAST_SAVE_ICON_BUFFER_SIZE), makeBuffer(gIconPalette, DREAMCAST_SAVE_ICON_PALETTE_SIZE));
	if (saveSize > freeSize) {
		logWarningFormat("Unable to save vars. Save size: %d, available size: %d", saveSize, freeSize);
		return;
	}
	
	savePrismGameSave(tSaveSlot, (VARS_SAVE_FILENAME_PRE + fileName).c_str(), b, "DOLMEXICA INFINITE", desc.c_str() , desc.c_str(), makeBuffer(gVarsIcon, DREAMCAST_SAVE_ICON_BUFFER_SIZE), makeBuffer(gIconPalette, DREAMCAST_SAVE_ICON_PALETTE_SIZE));
	freeBuffer(b);
}

static void deleteGlobalVariablesDreamcast(PrismSaveSlot tSaveSlot) {
	auto fileName = getGlobalVarSaveFileNameAfterTitleSetDreamcast();
	turnStringUppercase(fileName);
	if (!hasPrismGameSave(tSaveSlot, (VARS_SAVE_FILENAME_PRE + fileName).c_str())) {
		return;
	}
	deletePrismGameSave(tSaveSlot, (VARS_SAVE_FILENAME_PRE + fileName).c_str());
}

static int hasGlobalVariablesSaveDreamcast(PrismSaveSlot tSaveSlot) {
	auto fileName = getGlobalVarSaveFileNameAfterTitleSetDreamcast();
	turnStringUppercase(fileName);
	return hasPrismGameSave(tSaveSlot, (VARS_SAVE_FILENAME_PRE + fileName).c_str());
}

static size_t getGlobalVariablesSaveSizeDreamcast() {
	auto b = buildGlobalVariableSaveData();
	auto fileName = getGlobalVarSaveFileNameAfterTitleSetDreamcast();
	const auto desc = fileName;
	turnStringUppercase(fileName);
	return getPrismGameSaveSize(b, "DOLMEXICA INFINITE", desc.c_str(), desc.c_str(), makeBuffer(gVarsIcon, DREAMCAST_SAVE_ICON_BUFFER_SIZE), makeBuffer(gIconPalette, DREAMCAST_SAVE_ICON_PALETTE_SIZE));
}
#endif

void saveGlobalVariables(PrismSaveSlot tSaveSlot)
{
#ifdef _WIN32
	(void)tSaveSlot;
	saveGlobalVariablesWindows();
#elif defined(DREAMCAST)
	saveGlobalVariablesDreamcast(tSaveSlot);
#endif
}

void deleteGlobalVariables(PrismSaveSlot tSaveSlot)
{
#ifdef _WIN32
	(void)tSaveSlot;
	deleteGlobalVariablesWindows();
#elif defined(DREAMCAST)
	deleteGlobalVariablesDreamcast(tSaveSlot);
#endif
}

int hasGlobalVariablesSave(PrismSaveSlot tSaveSlot)
{
#ifdef _WIN32
	(void)tSaveSlot;
	return hasGlobalVariablesSaveWindows();
#elif defined(DREAMCAST)
	return hasGlobalVariablesSaveDreamcast(tSaveSlot);
#else
	return 0;
#endif
}

size_t getGlobalVariablesSaveSize() {
#ifdef _WIN32
	return getGlobalVariablesSaveSizeWindows();
#elif defined(DREAMCAST)
	return getGlobalVariablesSaveSizeDreamcast();
#else
	return 0;
#endif
}