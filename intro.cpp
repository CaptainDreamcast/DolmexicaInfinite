#include "intro.h"

#include <prism/mugendefreader.h>
#include <prism/memoryhandler.h>

#include "storyscreen.h"
#include "titlescreen.h"

static struct {
	int mHasLogoStoryboard;

	int mHasIntroStoryboard;
	int mWaitCycleNow;
	int mWaitCycleAmount;
} gData;

static void loadLogoAndIntroStoryboards() {
	MugenDefScript script = loadMugenDefScript("assets/data/system.def");

	char* logoStoryboard = getAllocatedMugenDefStringOrDefault(&script, "Files", "logo.storyboard", " ");
	gData.mHasLogoStoryboard = isFile(logoStoryboard);
	freeMemory(logoStoryboard);

	char* introStoryboard = getAllocatedMugenDefStringOrDefault(&script, "Files", "intro.storyboard", " ");
	gData.mHasIntroStoryboard = isFile(introStoryboard);
	gData.mWaitCycleAmount = getMugenDefIntegerOrDefault(&script, "Demo Mode", "intro.waitcycles", 1);
	gData.mWaitCycleNow = 0;
	freeMemory(introStoryboard);

	unloadMugenDefScript(script);
}

void startIntroFirstTime()
{
	loadLogoAndIntroStoryboards();

	if (hasLogoStoryboard()) {
		playLogoStoryboard();
	}
	else if (hasIntroStoryboard()) {
		playIntroStoryboard();
	}
	else {
		setNewScreen(getDreamTitleScreen());
	}

}

int hasLogoStoryboard()
{
	return gData.mHasLogoStoryboard;
}

static void logoStoryboardFinishedCB() {
	if (hasIntroStoryboard()) {
		playIntroStoryboard();
	}
	else {
		setNewScreen(getDreamTitleScreen());
	}
}

void playLogoStoryboard()
{
	MugenDefScript script = loadMugenDefScript("assets/data/system.def");
	char* logoStoryboard = getAllocatedMugenDefStringOrDefault(&script, "Files", "logo.storyboard", " ");
	
	setStoryDefinitionFile(logoStoryboard);
	setStoryScreenFinishedCB(logoStoryboardFinishedCB);
	setNewScreen(getStoryScreen()); // TODO: Dolmexica storyboards
	
}

int hasIntroStoryboard()
{
	return gData.mHasIntroStoryboard;
}

int hasFinishedIntroWaitCycle()
{
	return gData.mWaitCycleNow == gData.mWaitCycleAmount;
}

void increaseIntroWaitCycle()
{
	gData.mWaitCycleNow++;
}

static void introStoryboardFinishedCB() {
	setNewScreen(getDreamTitleScreen());
}

void playIntroStoryboard()
{
	MugenDefScript script = loadMugenDefScript("assets/data/system.def");
	char* introStoryboard = getAllocatedMugenDefStringOrDefault(&script, "Files", "intro.storyboard", " ");

	setStoryDefinitionFile(introStoryboard);
	setStoryScreenFinishedCB(introStoryboardFinishedCB);

	gData.mWaitCycleNow = 0;
	setNewScreen(getStoryScreen()); // TODO: Dolmexica storyboards
}
