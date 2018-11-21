#include "config.h"

#include <prism/mugendefreader.h>
#include <prism/memoryhandler.h>

static struct {
	double mDefaultAttackDamageDoneToPowerMultiplier;
	double mDefaultAttackDamageReceivedToPowerMultiplier;

	int mDebug;
	int mAllowDebugMode; // TODO
	int mAllowDebugKeys; // TODO
	int mSpeedup; // TODO
	char mStartStage[200]; // TODO

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

void loadMugenConfig() {
	MugenDefScript script = loadMugenDefScript("assets/data/mugen.cfg"); 
	loadRules(&script);
	loadDebug(&script);
	unloadMugenDefScript(script);
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

