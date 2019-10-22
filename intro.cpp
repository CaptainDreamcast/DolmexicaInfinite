#include "intro.h"

#include <prism/mugendefreader.h>
#include <prism/memoryhandler.h>

#include "storyscreen.h"
#include "titlescreen.h"
#include "dolmexicastoryscreen.h"

static struct {
	int mHasLogoStoryboard;

	int mHasIntroStoryboard;
	int mWaitCycleNow;
	int mWaitCycleAmount;
} gIntroData;

static void loadLogoAndIntroStoryboards() {
	MugenDefScript script; 
	loadMugenDefScript(&script, "assets/data/system.def");

	char* logoStoryboard = getAllocatedMugenDefStringOrDefault(&script, "Files", "logo.storyboard", " ");
	gIntroData.mHasLogoStoryboard = isFile(logoStoryboard);
	freeMemory(logoStoryboard);

	char* introStoryboard = getAllocatedMugenDefStringOrDefault(&script, "Files", "intro.storyboard", " ");
	gIntroData.mHasIntroStoryboard = isFile(introStoryboard);
	gIntroData.mWaitCycleAmount = getMugenDefIntegerOrDefault(&script, "Demo Mode", "intro.waitcycles", 1);
	gIntroData.mWaitCycleNow = 0;
	freeMemory(introStoryboard);

	unloadMugenDefScript(script);
}

static void logoStoryboardFinishedCB() {
	if (hasIntroStoryboard()) {
		playIntroStoryboard();
	}
	else {
		setNewScreen(getDreamTitleScreen());
	}
}

static Screen* loadLogoStoryboardAndReturnScreen() {
	MugenDefScript script;
	loadMugenDefScript(&script, "assets/data/system.def");
	char* logoStoryboard = getAllocatedMugenDefStringOrDefault(&script, "Files", "logo.storyboard", " ");

	MugenDefScript storyboardScript;
	loadMugenDefScript(&storyboardScript, logoStoryboard);
	auto versionString = getSTLMugenDefStringOrDefault(&storyboardScript, "Info", "version", "");
	turnStringLowercase(versionString);

	if (versionString == "dolmexica") {
		setDolmexicaStoryScreenFile(logoStoryboard);
		return getDolmexicaStoryScreen();
	}
	else {
		setStoryDefinitionFile(logoStoryboard);
		setStoryScreenFinishedCB(logoStoryboardFinishedCB);
		return getStoryScreen();
	}
}

static void introStoryboardFinishedCB() {
	setNewScreen(getDreamTitleScreen());
}

static Screen* loadIntroStoryboardAndReturnScreen() {
	MugenDefScript script;
	loadMugenDefScript(&script, "assets/data/system.def");
	char* introStoryboard = getAllocatedMugenDefStringOrDefault(&script, "Files", "intro.storyboard", " ");

	MugenDefScript storyboardScript;
	loadMugenDefScript(&storyboardScript, introStoryboard);
	auto versionString = getSTLMugenDefStringOrDefault(&storyboardScript, "Info", "version", "");
	turnStringLowercase(versionString);

	gIntroData.mWaitCycleNow = 0;
	if (versionString == "dolmexica") {
		setDolmexicaStoryScreenFile(introStoryboard);
		return getDolmexicaStoryScreen();
	}
	else {
		setStoryDefinitionFile(introStoryboard);
		setStoryScreenFinishedCB(introStoryboardFinishedCB);
		return getStoryScreen();
	}
}

Screen* startIntroFirstTimeAndReturnScreen()
{
	loadLogoAndIntroStoryboards();

	if (hasLogoStoryboard()) {
		return loadLogoStoryboardAndReturnScreen();
	}
	else if (hasIntroStoryboard()) {
		return loadIntroStoryboardAndReturnScreen();
	}
	else {
		return getDreamTitleScreen();
	}

}

int hasLogoStoryboard()
{
	return gIntroData.mHasLogoStoryboard;
}

void playLogoStoryboard()
{
	if (hasLogoStoryboard()) {
		auto screen = loadLogoStoryboardAndReturnScreen();
		setNewScreen(screen);
	}
	else {
		setNewScreen(getDreamTitleScreen());
	}
}

int hasIntroStoryboard()
{
	return gIntroData.mHasIntroStoryboard;
}

int hasFinishedIntroWaitCycle()
{
	return gIntroData.mWaitCycleNow == gIntroData.mWaitCycleAmount;
}

void increaseIntroWaitCycle()
{
	gIntroData.mWaitCycleNow++;
}

void playIntroStoryboard()
{
	if (hasIntroStoryboard()) {
		auto screen = loadIntroStoryboardAndReturnScreen();
		setNewScreen(screen);
	}
	else {
		setNewScreen(getDreamTitleScreen());
	}
}
