#include "intro.h"

#include <prism/mugendefreader.h>
#include <prism/memoryhandler.h>

#include "storyscreen.h"
#include "titlescreen.h"
#include "dolmexicastoryscreen.h"
#include "config.h"

static struct {
	int mHasLogoStoryboard;

	int mHasIntroStoryboard;
	int mWaitCycleNow;
	int mWaitCycleAmount;
} gIntroData;

static void loadLogoAndIntroStoryboards() {
	const auto motifPath = getDolmexicaAssetFolder() + getMotifPath();
	MugenDefScript script; 
	loadMugenDefScript(&script, motifPath);
	std::string folder;
	getPathToFile(folder, motifPath.c_str());

	auto logoStoryboard = getSTLMugenDefStringOrDefault(&script, "files", "logo.storyboard", " ");
	logoStoryboard = findMugenSystemOrFightFilePath(logoStoryboard, folder);
	gIntroData.mHasLogoStoryboard = isFile(logoStoryboard);

	auto introStoryboard = getSTLMugenDefStringOrDefault(&script, "files", "intro.storyboard", " ");
	introStoryboard = findMugenSystemOrFightFilePath(introStoryboard, folder);
	gIntroData.mHasIntroStoryboard = isFile(introStoryboard);
	gIntroData.mWaitCycleAmount = getMugenDefIntegerOrDefault(&script, "demo mode", "intro.waitcycles", 1);
	gIntroData.mWaitCycleNow = 0;

	unloadMugenDefScript(&script);
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
	const auto motifPath = getDolmexicaAssetFolder() + getMotifPath();
	MugenDefScript script;
	loadMugenDefScript(&script, motifPath);
	std::string folder;
	getPathToFile(folder, motifPath.c_str());
	auto logoStoryboard = getSTLMugenDefStringOrDefault(&script, "files", "logo.storyboard", " ");
	logoStoryboard = findMugenSystemOrFightFilePath(logoStoryboard, folder);

	MugenDefScript storyboardScript;
	loadMugenDefScript(&storyboardScript, logoStoryboard);
	auto versionString = getSTLMugenDefStringOrDefault(&storyboardScript, "info", "version", "");
	turnStringLowercase(versionString);

	if (versionString == "dolmexica") {
		setDolmexicaStoryScreenFileAndPrepareScreen(logoStoryboard.c_str());
		return getDolmexicaStoryScreen();
	}
	else {
		setStoryDefinitionFileAndPrepareScreen(logoStoryboard.c_str());
		setStoryScreenFinishedCB(logoStoryboardFinishedCB);
		return getStoryScreen();
	}
}

static void introStoryboardFinishedCB() {
	setNewScreen(getDreamTitleScreen());
}

static Screen* loadIntroStoryboardAndReturnScreen() {
	const auto motifPath = getDolmexicaAssetFolder() + getMotifPath();
	MugenDefScript script;
	loadMugenDefScript(&script, motifPath);
	std::string folder;
	getPathToFile(folder, motifPath.c_str());
	auto introStoryboard = getSTLMugenDefStringOrDefault(&script, "files", "intro.storyboard", " ");
	introStoryboard = findMugenSystemOrFightFilePath(introStoryboard, folder);

	MugenDefScript storyboardScript;
	loadMugenDefScript(&storyboardScript, introStoryboard);
	auto versionString = getSTLMugenDefStringOrDefault(&storyboardScript, "info", "version", "");
	turnStringLowercase(versionString);

	gIntroData.mWaitCycleNow = 0;
	if (versionString == "dolmexica") {
		setDolmexicaStoryScreenFileAndPrepareScreen(introStoryboard.c_str());
		return getDolmexicaStoryScreen();
	}
	else {
		setStoryDefinitionFileAndPrepareScreen(introStoryboard.c_str());
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
