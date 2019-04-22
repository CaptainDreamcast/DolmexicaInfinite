#include "config.h"

#include <prism/mugendefreader.h>
#include <prism/memoryhandler.h>
#include <prism/system.h>
#include <prism/sound.h>
#include <prism/soundeffect.h>
#include <prism/stlutil.h>

using namespace std;

static struct {
	double mDefaultAttackDamageDoneToPowerMultiplier;
	double mDefaultAttackDamageReceivedToPowerMultiplier;

	int mDebug;
	int mAllowDebugMode; // TODO
	int mAllowDebugKeys; // TODO
	int mSpeedup; // TODO
	char mStartStage[200]; // TODO
	int mDifficulty;
	int mLifeStartPercentageNumber;
	int mIsTimerInfinite;
	int mTimerDuration;
	int mGameSpeed;
	int mWavVolume;
	int mMidiVolume;

	map<int, int> mGlobalVariables;
	map<int, double> mGlobalFVariables;
	map<int, std::string> mGlobalStringVariables;


} gConfigData;

static void loadRules(MugenDefScript* tScript) {
	gConfigData.mDefaultAttackDamageDoneToPowerMultiplier = getMugenDefFloatOrDefault(tScript, "Rules", "Default.Attack.LifeToPowerMul", 0.7);
	gConfigData.mDefaultAttackDamageReceivedToPowerMultiplier = getMugenDefFloatOrDefault(tScript, "Rules", "Default.GetHit.LifeToPowerMul", 0.6);
}

static void loadDebug(MugenDefScript* tScript) {
	gConfigData.mDebug = getMugenDefIntegerOrDefault(tScript, "Debug", "debug", 1);
	gConfigData.mAllowDebugMode = getMugenDefIntegerOrDefault(tScript, "Debug", "allowdebugmode", 1);
	gConfigData.mAllowDebugKeys = getMugenDefIntegerOrDefault(tScript, "Debug", "allowdebugkeys", 0);
	gConfigData.mSpeedup = getMugenDefIntegerOrDefault(tScript, "Debug", "speedup", 0);

	char* text = getAllocatedMugenDefStringOrDefault(tScript, "Debug", "startstage", "stages/stage0.def");
	strcpy(gConfigData.mStartStage, text);
	freeMemory(text);
}

static void loadWindowTitle(MugenDefScript* tScript) {
	if (isOnDreamcast()) return;
	if (!isMugenDefStringVariable(tScript, "Misc", "title")) return;

	char* text = getAllocatedMugenDefStringOrDefault(tScript, "Misc", "title", "DOLMEXICA INFINITE");
	updateGameName(text);
	freeMemory(text);
}

void loadMugenConfig() {
	MugenDefScript script; 
	loadMugenDefScript(&script, "assets/data/mugen.cfg");
	loadRules(&script);
	loadDebug(&script);
	loadWindowTitle(&script);
	unloadMugenDefScript(script);

	setDefaultOptionVariables(); // TODO: load saved values
}

double getDreamDefaultAttackDamageDoneToPowerMultiplier()
{
	return gConfigData.mDefaultAttackDamageDoneToPowerMultiplier;
}

double getDreamDefaultAttackDamageReceivedToPowerMultiplier()
{
	return gConfigData.mDefaultAttackDamageReceivedToPowerMultiplier;
}

int isMugenDebugActive()
{
	return gConfigData.mDebug;
}

void setDefaultOptionVariables() {
	gConfigData.mDifficulty = 4;
	gConfigData.mLifeStartPercentageNumber = 100;
	gConfigData.mIsTimerInfinite = 0;
	gConfigData.mTimerDuration = 99;
	gConfigData.mGameSpeed = 0;
    if(isOnDreamcast()) {
	    gConfigData.mWavVolume = 100;
	    gConfigData.mMidiVolume = 100;
    } else {
        gConfigData.mWavVolume = 10;
        gConfigData.mMidiVolume = 20;
    }

	setVolume(gConfigData.mWavVolume / 100.0);
	setSoundEffectVolume(gConfigData.mMidiVolume / 100.0);
}

int getDifficulty()
{
	return gConfigData.mDifficulty;
}

void setDifficulty(int tDifficulty)
{
	gConfigData.mDifficulty = tDifficulty;
}

double getLifeStartPercentage()
{
	return gConfigData.mLifeStartPercentageNumber / 100.0;
}

int getLifeStartPercentageNumber()
{
	return gConfigData.mLifeStartPercentageNumber;
}

void setLifeStartPercentageNumber(int tLifeStartPercentageNumber)
{
	gConfigData.mLifeStartPercentageNumber = tLifeStartPercentageNumber;
}

int isGlobalTimerInfinite()
{
	return gConfigData.mIsTimerInfinite;
}

void setGlobalTimerInfinite()
{
	gConfigData.mIsTimerInfinite = 1;
}

int getGlobalTimerDuration()
{
	return gConfigData.mTimerDuration;
}

void setGlobalTimerDuration(int tDuration)
{
	gConfigData.mTimerDuration = tDuration;
	gConfigData.mIsTimerInfinite = 0;
}

int getGlobalGameSpeed()
{
	return gConfigData.mGameSpeed;
}

void setGlobalGameSpeed(int tGameSpeed)
{
	gConfigData.mGameSpeed = tGameSpeed;
}

int getGameWavVolume()
{
	return gConfigData.mWavVolume;
}

void setGameWavVolume(int tWavVolume)
{
	gConfigData.mWavVolume = tWavVolume;
	setVolume(gConfigData.mWavVolume / 100.0);
}

int getGameMidiVolume()
{
	return gConfigData.mMidiVolume;
}

void setGameMidiVolume(int tMidiVolume)
{
	gConfigData.mMidiVolume = tMidiVolume;
	setSoundEffectVolume(gConfigData.mMidiVolume / 100.0);
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
	gConfigData.mGlobalStringVariables[tID][0] += tValue;
}

std::string getGlobalStringVariable(int tID)
{
	return gConfigData.mGlobalStringVariables[tID];
}
