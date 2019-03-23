#include "dolmexicadebug.h"

#include <prism/stlutil.h>
#include <prism/debug.h>

#include "gamelogic.h"
#include "playerdefinition.h"
#include "characterselectscreen.h"
#include "stage.h"
#include "fightscreen.h"

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
	vector<string> words = splitCommandString(tCommand);
	if (words.size() < 2) return "Too few arguments";

	setPlayerDefinitionPath(0, "assets/chars/kfm/kfm.def");
	setPlayerDefinitionPath(1, "assets/chars/kfm/kfm.def");
	setDreamStageMugenDefinition(("assets/stages/" + words[1]).data(), "");
	setGameModeTraining();
	startFightScreen();

	return "";
}

void initDolmexicaDebug()
{
	addPrismDebugConsoleCommand("fight", fightCB);
	addPrismDebugConsoleCommand("stage", stageCB);
}
