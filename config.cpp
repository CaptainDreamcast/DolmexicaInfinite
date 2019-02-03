#include "config.h"

#include <prism/mugendefreader.h>
#include <prism/memoryhandler.h>
#include <prism/system.h>
#include <prism/sound.h>
#include <prism/soundeffect.h>

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
} gData;

static void loadRules(MugenDefScript* tScript) {
	gData.mDefaultAttackDamageDoneToPowerMultiplier = getMugenDefFloatOrDefault(tScript, "Rules", "Default.Attack.LifeToPowerMul", 0.7);
	gData.mDefaultAttackDamageReceivedToPowerMultiplier = getMugenDefFloatOrDefault(tScript, "Rules", "Default.GetHit.LifeToPowerMul", 0.6);
}

static void loadDebug(MugenDefScript* tScript) {
	gData.mDebug = getMugenDefIntegerOrDefault(tScript, "Debug", "debug", 1);
	gData.mAllowDebugMode = getMugenDefIntegerOrDefault(tScript, "Debug", "allowdebugmode", 1);
	gData.mAllowDebugKeys = getMugenDefIntegerOrDefault(tScript, "Debug", "allowdebugkeys", 0);
	gData.mSpeedup = getMugenDefIntegerOrDefault(tScript, "Debug", "speedup", 0);

	char* text = getAllocatedMugenDefStringOrDefault(tScript, "Debug", "startstage", "stages/stage0.def");
	strcpy(gData.mStartStage, text);
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
	return gData.mDefaultAttackDamageDoneToPowerMultiplier;
}

double getDreamDefaultAttackDamageReceivedToPowerMultiplier()
{
	return gData.mDefaultAttackDamageReceivedToPowerMultiplier;
}

int isMugenDebugActive()
{
	return gData.mDebug;
}

void setDefaultOptionVariables() {
	gData.mDifficulty = 4;
	gData.mLifeStartPercentageNumber = 100;
	gData.mIsTimerInfinite = 0;
	gData.mTimerDuration = 99;
	gData.mGameSpeed = 0;
	gData.mWavVolume = 50;
	gData.mMidiVolume = 50;
}

int getDifficulty()
{
	return gData.mDifficulty;
}

void setDifficulty(int tDifficulty)
{
	gData.mDifficulty = tDifficulty;
}

double getLifeStartPercentage()
{
	return gData.mLifeStartPercentageNumber / 100.0;
}

int getLifeStartPercentageNumber()
{
	return gData.mLifeStartPercentageNumber;
}

void setLifeStartPercentageNumber(int tLifeStartPercentageNumber)
{
	gData.mLifeStartPercentageNumber = tLifeStartPercentageNumber;
}

int isGlobalTimerInfinite()
{
	return gData.mIsTimerInfinite;
}

void setGlobalTimerInfinite()
{
	gData.mIsTimerInfinite = 1;
}

int getGlobalTimerDuration()
{
	return gData.mTimerDuration;
}

void setGlobalTimerDuration(int tDuration)
{
	gData.mTimerDuration = tDuration;
	gData.mIsTimerInfinite = 0;
}

int getGlobalGameSpeed()
{
	return gData.mGameSpeed;
}

void setGlobalGameSpeed(int tGameSpeed)
{
	gData.mGameSpeed = tGameSpeed;
}

int getGameWavVolume()
{
	return gData.mWavVolume;
}

void setGameWavVolume(int tWavVolume)
{
	gData.mWavVolume = tWavVolume;
	setVolume(gData.mWavVolume / 100.0);
}

int getGameMidiVolume()
{
	return gData.mMidiVolume;
}

void setGameMidiVolume(int tMidiVolume)
{
	gData.mMidiVolume = tMidiVolume;
	setSoundEffectVolume(gData.mMidiVolume / 100.0);
}

