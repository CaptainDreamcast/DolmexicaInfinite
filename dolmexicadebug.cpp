#include "dolmexicadebug.h"

#include <prism/stlutil.h>
#include <prism/debug.h>

#include "gamelogic.h"
#include "playerdefinition.h"
#include "characterselectscreen.h"
#include "stage.h"
#include "fightscreen.h"
#include "fightdebug.h"
#include "mugencommandhandler.h"
#include "mugenassignmentevaluator.h"

using namespace std;

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

static string fightCB(void* tCaller, string tCommand) {
	vector<string> words = splitCommandString(tCommand);
	if (words.size() < 3) return "Too few arguments";
	
	char path[1024];
	getCharacterSelectNamePath(words[1].data(), path);
	setPlayerDefinitionPath(0, path);
	getCharacterSelectNamePath(words[2].data(), path);
	setPlayerDefinitionPath(1, path);
	setDreamStageMugenDefinition("assets/stages/kfm.def", "");

	if (words.size() >= 4) {
		if (words[3] == "watch") {
			setGameModeWatch();
		} else 	if (words[3] == "superwatch") {
			setGameModeSuperWatch();
		}
		else {
			setGameModeTraining();
		}
	}
	else {
		setGameModeTraining();
	}

	startFightScreen();

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
	startFightScreen();

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

	int id = stoi(words[1]);
	DreamPlayer* p = getRootPlayer(id);
	setPlayerPositionBasedOnScreenCenterX(p, stof(words[2]), getPlayerCoordinateP(p));

	if (words.size() >= 4) {
		setPlayerPositionY(p, stof(words[3]), getPlayerCoordinateP(p));
	}

	return "";
}

static string rootposcloseCB(void* tCaller, string tCommand) {
	(void)tCaller;
	vector<string> words = splitCommandString(tCommand);
	if (words.size() < 2) return "Too few arguments";

	double dist = stof(words[1]);
	setPlayerPositionBasedOnScreenCenterX(getRootPlayer(0), -dist / 2, getPlayerCoordinateP(getRootPlayer(0)));
	setPlayerPositionBasedOnScreenCenterX(getRootPlayer(1), dist / 2, getPlayerCoordinateP(getRootPlayer(1)));

	return "";
}

static string rootctrlCB(void* tCaller, string tCommand) {
	(void)tCaller;
	vector<string> words = splitCommandString(tCommand);
	if (words.size() < 3) return "Too few arguments";

	int id = stoi(words[1]);
	DreamPlayer* p = getRootPlayer(id);
	setPlayerControl(p, stoi(words[2]));

	return "";
}

static string fullpowerCB(void* tCaller, string tCommand) {
	(void)tCaller;
	vector<string> words = splitCommandString(tCommand);
	if (words.size() < 2) return "Too few arguments";

	int id = stoi(words[1]);
	DreamPlayer* p = getRootPlayer(id);
	setPlayerPower(p, getPlayerPowerMax(p));

	return "";
}

static string commandCB(void* tCaller, string tCommand) {
	(void)tCaller;
	vector<string> words = splitCommandString(tCommand);
	if (words.size() < 3) return "Too few arguments";
	
	int id = stoi(words[1]);
	DreamPlayer* p = getRootPlayer(id);
	int bufferTime = (words.size() >= 4) ? stoi(words[3]) : 2;
	setDreamPlayerCommandActiveForAI(p->mCommandID, words[2].data(), bufferTime);
	return "";
}

static string evalCB(void* tCaller, string tCommand) {
	(void)tCaller;
	vector<string> words = splitCommandString(tCommand);
	if (words.size() < 3) return "Too few arguments";
	int id = stoi(words[1]);
	DreamPlayer* p = getRootPlayer(id);

	int n = tCommand.find(' ', tCommand.find(' ') + 1) + 1;
	string assignmentString = tCommand.substr(n);
	char buffer[1024];
	strcpy(buffer, assignmentString.c_str());

	auto assignment = parseDreamMugenAssignmentFromString(buffer);
	char* result = evaluateDreamAssignmentAndReturnAsAllocatedString(&assignment, p);
	// destroyDreamMugenAssignment(assignment); // TODO: fix leak, only debug anyway though
	string ret = result;
	freeMemory(result);
	return ret;
}

void initDolmexicaDebug()
{
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
	addPrismDebugConsoleCommand("eval", evalCB);
}
