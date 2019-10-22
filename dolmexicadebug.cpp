#include "dolmexicadebug.h"

#include <sstream>
#include <prism/stlutil.h>
#include <prism/debug.h>
#include <prism/log.h>
#include <prism/input.h>

#include "gamelogic.h"
#include "playerdefinition.h"
#include "characterselectscreen.h"
#include "stage.h"
#include "fightscreen.h"
#include "fightdebug.h"
#include "mugencommandhandler.h"
#include "mugenassignmentevaluator.h"
#include "titlescreen.h"
#include "storymode.h"
#include "randomwatchmode.h"

using namespace std;

typedef struct {
	int mPreviousValue;
	int* mValuePointer;
} TrackedInteger;

typedef struct {
	map<std::string, TrackedInteger> mMap;
	int mIsOverridingTimeDilatation = 0;
	double mOverridingTimeDilatationSpeed = 1.0;
} DolmexicaDebugData;

static DolmexicaDebugData* gDolmexicaDebugData = nullptr;

static vector<string> splitCommandString(string s) {
	vector<string> ret;
	int n = 0;
	while (n < (int)s.size()) {
		int next = (int)s.find(' ', n);
		if (next == -1) {
			ret.push_back(s.substr(n));
			break;
		}
		else {
			ret.push_back(s.substr(n, next - n));
			n = next + 1;
		}
	}
	return ret;
}

static void mockFightFinishedCB() {
	setNewScreen(getDreamTitleScreen());
}

static string fightCB(void* /*tCaller*/, string tCommand) {
	vector<string> words = splitCommandString(tCommand);
	if (words.size() < 3) return "Too few arguments";
	
	char path[1024];
	getCharacterSelectNamePath(words[1].data(), path);
	setPlayerDefinitionPath(0, path);
	getCharacterSelectNamePath(words[2].data(), path);
	setPlayerDefinitionPath(1, path);

	if (words.size() >= 4) {
		if (words[3] == "watch") {
			setGameModeWatch();
		} 
		else if (words[3] == "superwatch") {
			setGameModeSuperWatch();
		}
		else if (words[3] == "osu") {
			setGameModeOsu();
		}
		else if (words[3] == "versus") {
			setGameModeVersus();
		}
		else {
			setGameModeTraining();
		}
	}
	else {
		setGameModeTraining();
	}

	if (words.size() >= 5) {
		setDreamStageMugenDefinition(("assets/stages/" + words[4]).data(), "");
	}
	else {
		setDreamStageMugenDefinition("assets/stages/kfm.def", "");
	}

	startFightScreen(mockFightFinishedCB);

	return "";
}

static string stageCB(void* tCaller, string tCommand) {
	(void)tCaller;
	vector<string> words = splitCommandString(tCommand);
	if (words.size() < 2) return "Too few arguments";

	setPlayerDefinitionPath(0, "assets/chars/kfm/kfm.def");
	setPlayerDefinitionPath(1, "assets/chars/kfm/kfm.def");
	setDreamStageMugenDefinition(("assets/stages/" + words[1]).data(), "");
	setGameModeTraining();
	startFightScreen(mockFightFinishedCB);

	return "";
}

static string fightdebugCB(void* tCaller, string tCommand) {
	(void)tCaller;
	(void)tCommand;
	switchFightDebugTextActivity();
	return "";
}

static string fightcollisionCB(void* tCaller, string tCommand) {
	(void)tCaller;
	(void)tCommand;
	switchFightCollisionDebugActivity();
	return "";
}

static string skipintroCB(void* tCaller, string tCommand) {
	(void)tCaller;
	(void)tCommand;
	skipFightIntroWithoutFading();
	return "";
}

static string rootposCB(void* tCaller, string tCommand) {
	(void)tCaller;
	vector<string> words = splitCommandString(tCommand);
	if (words.size() < 3) return "Too few arguments";

	int id = atoi(words[1].data());
	DreamPlayer* p = getRootPlayer(id);
	setPlayerPositionBasedOnScreenCenterX(p, atof(words[2].data()), getPlayerCoordinateP(p));

	if (words.size() >= 4) {
		setPlayerPositionY(p, atof(words[3].data()), getPlayerCoordinateP(p));
	}

	return "";
}

static string rootposcloseCB(void* tCaller, string tCommand) {
	(void)tCaller;
	vector<string> words = splitCommandString(tCommand);
	if (words.size() < 2) return "Too few arguments";

	double dist = atof(words[1].data());
	setPlayerPositionBasedOnScreenCenterX(getRootPlayer(0), -dist / 2, getPlayerCoordinateP(getRootPlayer(0)));
	setPlayerPositionBasedOnScreenCenterX(getRootPlayer(1), dist / 2, getPlayerCoordinateP(getRootPlayer(1)));

	return "";
}

static string rootctrlCB(void* tCaller, string tCommand) {
	(void)tCaller;
	vector<string> words = splitCommandString(tCommand);
	if (words.size() < 3) return "Too few arguments";

	int id = atoi(words[1].data());
	DreamPlayer* p = getRootPlayer(id);
	setPlayerControl(p, atoi(words[2].data()));

	return "";
}

static string fullpowerCB(void* tCaller, string tCommand) {
	(void)tCaller;
	vector<string> words = splitCommandString(tCommand);
	if (words.size() < 2) return "Too few arguments";

	int id = atoi(words[1].data());
	DreamPlayer* p = getRootPlayer(id);
	setPlayerPower(p, getPlayerPowerMax(p));

	return "";
}

static string commandCB(void* tCaller, string tCommand) {
	(void)tCaller;
	vector<string> words = splitCommandString(tCommand);
	if (words.size() < 3) return "Too few arguments";
	
	int id = atoi(words[1].data());
	DreamPlayer* p = getRootPlayer(id);
	int bufferTime = (words.size() >= 4) ? atoi(words[3].data()) : 2;
	setDreamPlayerCommandActiveForAI(p->mCommandID, words[2].data(), bufferTime);
	return "";
}

static string testcommandNumberCB(void* tCaller, string tCommand) {
	(void)tCaller;
	vector<string> words = splitCommandString(tCommand);
	if (words.size() < 2) return "Too few arguments";

	int commandNumber = atoi(words[1].data());
	double dist = words.size() >= 3 ? atof(words[2].data()) : 20;

	const int placementDirection = (words.size() >= 4) ? atoi(words[3].data()) : 1;
	setPlayerPositionBasedOnScreenCenterX(getRootPlayer(0), -placementDirection * dist / 2, getPlayerCoordinateP(getRootPlayer(0)));
	setPlayerPositionBasedOnScreenCenterX(getRootPlayer(1), placementDirection * dist / 2, getPlayerCoordinateP(getRootPlayer(1)));

	DreamPlayer* p = getRootPlayer(0);
	const auto result = setDreamPlayerCommandNumberActiveForDebug(p->mCommandID, commandNumber);
	return result ? "" : "Over command amount";
}

static string evalCB(void* tCaller, string tCommand) {
	(void)tCaller;
	vector<string> words = splitCommandString(tCommand);
	if (words.size() < 3) return "Too few arguments";
	int id = atoi(words[1].data());
	DreamPlayer* p = getRootPlayer(id);

	int n = tCommand.find(' ', tCommand.find(' ') + 1) + 1;
	string assignmentString = tCommand.substr(n);
	char buffer[1024];
	strcpy(buffer, assignmentString.c_str());

	auto assignment = parseDreamMugenAssignmentFromString(buffer);
	string result;
	evaluateDreamAssignmentAndReturnAsString(result, &assignment, p);
	destroyDreamMugenAssignment(assignment);
	string ret = result;
	return ret;
}

static string evalhelperCB(void* tCaller, string tCommand) {
	(void)tCaller;
	vector<string> words = splitCommandString(tCommand);
	if (words.size() < 4) return "Too few arguments";
	int id = atoi(words[1].data());
	int helper = atoi(words[2].data());
	DreamPlayer* p = getRootPlayer(id);
	p = getPlayerHelperOrNullIfNonexistant(p, helper);
	if (!p) {
		return "No helper with that ID.";
	}

	int n = tCommand.find(' ', tCommand.find(' ', tCommand.find(' ') + 1) + 1) + 1;
	string assignmentString = tCommand.substr(n);
	char buffer[1024];
	strcpy(buffer, assignmentString.c_str());

	auto assignment = parseDreamMugenAssignmentFromString(buffer);
	string result;
	evaluateDreamAssignmentAndReturnAsString(result, &assignment, p);
	destroyDreamMugenAssignment(assignment);
	string ret = result;
	return ret;
}

static string trackvarCB(void* tCaller, string tCommand) {
	(void)tCaller;
	vector<string> words = splitCommandString(tCommand);
	if (words.size() < 3) return "Too few arguments";
	int id = atoi(words[1].data());
	int varNumber = atoi(words[2].data());

	DreamPlayer* p = getRootPlayer(id);

	TrackedInteger e;
	e.mValuePointer = getPlayerVariableReference(p, varNumber);
	e.mPreviousValue = *e.mValuePointer;
	ostringstream ss;
	ss << "player " << id << "; var " << varNumber;
	gDolmexicaDebugData->mMap[ss.str()] = e;
	
	ss.clear();
	ss << " value: " << *e.mValuePointer;
	return ss.str();
}

static string untrackvarCB(void* tCaller, string tCommand) {
	(void)tCaller;
	vector<string> words = splitCommandString(tCommand);
	if (words.size() < 3) return "Too few arguments";
	int id = atoi(words[1].data());
	int varNumber = atoi(words[2].data());

	ostringstream ss;
	ss << "player " << id << "; var " << varNumber;
	gDolmexicaDebugData->mMap.erase(ss.str());
	return "";
}

static string stateCB(void* tCaller, string tCommand) {
	(void)tCaller;
	vector<string> words = splitCommandString(tCommand);
	if (words.size() < 3) return "Too few arguments";
	int id = atoi(words[1].data());
	int stateNumber = atoi(words[2].data());
	DreamPlayer* p = getRootPlayer(id);

	changePlayerState(p, stateNumber);
	return "";
}

static string storyCB(void* /*tCaller*/, string tCommand) {
	const auto words = splitCommandString(tCommand);
	if (words.size() < 2) return "Too few arguments";
	const auto& name = words[1];

	int step = 0;
	if (words.size() >= 3) {
		step = atoi(words[2].c_str());
	}

	startStoryModeWithForcedStartState(name.c_str(), step);
	return "";
}

static string randomwatchCB(void* /*tCaller*/, string /*tCommand*/) {
	startRandomWatchMode();
	return "";
}

static string speedCB(void* /*tCaller*/, string tCommand) {
	const auto words = splitCommandString(tCommand);
	if (words.size() < 2) return "Too few arguments";
	const auto speed = atof(words[1].c_str());
	setWrapperTimeDilatation(speed);
	gDolmexicaDebugData->mIsOverridingTimeDilatation = 1;
	gDolmexicaDebugData->mOverridingTimeDilatationSpeed = speed;
	return "";
}

static string roundamountCB(void* /*tCaller*/, string tCommand) {
	const auto words = splitCommandString(tCommand);
	if (words.size() < 2) return "Too few arguments";
	const auto rounds = atoi(words[1].c_str());
	setRoundsToWin(rounds);
	return "";
}

void initDolmexicaDebug()
{
	gDolmexicaDebugData = new DolmexicaDebugData();

	addPrismDebugConsoleCommand("fight", fightCB);
	addPrismDebugConsoleCommand("stage", stageCB);
	addPrismDebugConsoleCommand("fightdebug", fightdebugCB);
	addPrismDebugConsoleCommand("fightcollision", fightcollisionCB);
	addPrismDebugConsoleCommand("skipintro", skipintroCB);
	addPrismDebugConsoleCommand("rootpos", rootposCB);
	addPrismDebugConsoleCommand("rootposclose", rootposcloseCB);
	addPrismDebugConsoleCommand("rootctrl", rootctrlCB);
	addPrismDebugConsoleCommand("fullpower", fullpowerCB);
	addPrismDebugConsoleCommand("command", commandCB);
	addPrismDebugConsoleCommand("testcommandn", testcommandNumberCB);
	addPrismDebugConsoleCommand("eval", evalCB);
	addPrismDebugConsoleCommand("evalhelper", evalhelperCB);
	addPrismDebugConsoleCommand("trackvar", trackvarCB);
	addPrismDebugConsoleCommand("untrackvar", untrackvarCB);
	addPrismDebugConsoleCommand("state", stateCB);
	addPrismDebugConsoleCommand("story", storyCB);
	addPrismDebugConsoleCommand("randomwatch", randomwatchCB);
	addPrismDebugConsoleCommand("speed", speedCB);
	addPrismDebugConsoleCommand("roundamount", roundamountCB);
}

static void loadDolmexicaDebugHandler(void* tData) {
	(void)tData;

	gDolmexicaDebugData->mMap.clear();
}

static void updateSingleTrackedInteger(void* tCaller, const std::string& tKey, TrackedInteger& e) {
	(void)tCaller;

	if (*e.mValuePointer != e.mPreviousValue) {
		ostringstream ss;
		ss << tKey << " changed to " << *e.mValuePointer;
		submitToPrismDebugConsole(ss.str());
	}

	e.mPreviousValue = *e.mValuePointer;

}

static void updateSpeedOverrideToggle() {
	if (hasPressedKeyboardKeyFlank(KEYBOARD_F9_PRISM)) {
		gDolmexicaDebugData->mIsOverridingTimeDilatation ^= 1;
		if (gDolmexicaDebugData->mIsOverridingTimeDilatation) setWrapperTimeDilatation(gDolmexicaDebugData->mOverridingTimeDilatationSpeed);
		else setWrapperTimeDilatation(1.0);
	}
}

static void updateDolmexicaDebugHandler(void* tData) {
	(void)tData;
	updateSpeedOverrideToggle();
	stl_string_map_map(gDolmexicaDebugData->mMap, updateSingleTrackedInteger);
}

ActorBlueprint getDolmexicaDebug() {
	return makeActorBlueprint(loadDolmexicaDebugHandler, NULL, updateDolmexicaDebugHandler);
}

int isDebugOverridingTimeDilatation()
{
	if (!gDolmexicaDebugData) return 0;
	return gDolmexicaDebugData->mIsOverridingTimeDilatation;
}
