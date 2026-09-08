#include "dolmexicadebug.h"

#include <algorithm>
#include <ctime>
#include <fstream>
#include <functional>
#include <set>
#include <sstream>
#include <prism/stlutil.h>
#include <prism/debug.h>
#include <prism/log.h>
#include <prism/input.h>
#include <prism/math.h>
#include <prism/mugentexthandler.h>

#include "gamelogic.h"
#include "playerdefinition.h"
#include "characterselectscreen.h"
#include "stage.h"
#include "fightscreen.h"
#include "mugenexplod.h"
#include "mugenstatehandler.h"
#include "fightdebug.h"
#include "mugencommandhandler.h"
#include "mugenassignmentevaluator.h"
#include "titlescreen.h"
#include "storymode.h"
#include "randomwatchmode.h"
#include "config.h"
#include "mugenstatehandler.h"
#include "dolmexicastoryscreen.h"

typedef struct {
	int mPreviousValue;
	int* mValuePointer;
} TrackedInteger;

typedef struct {
	std::unordered_map<std::string, TrackedInteger> mMap;
	int mIsOverridingTimeDilatation;
	float mOverridingTimeDilatationSpeed;

	std::unordered_map<std::string, std::set<int>> mStoryCharAnimations;
} DolmexicaDebugData;

static DolmexicaDebugData* gDolmexicaDebugData = nullptr;

static std::vector<std::string> splitCommandString(std::string s) {
	std::vector<std::string> ret;
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

static std::string fightCB(void* /*tCaller*/, const std::string& tCommand) {
	std::vector<std::string> words = splitCommandString(tCommand);
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
		else if (words[3] == "freeplay") {
			setGameModeFreePlay();
		}
		else {
			setGameModeTraining();
		}
	}
	else {
		setGameModeTraining();
	}

	if (words.size() >= 5) {
		setDreamStageMugenDefinition((getDolmexicaAssetFolder() + "stages/" + words[4]).c_str(), "");
	}
	else {
		setDreamStageMugenDefinition((getDolmexicaAssetFolder() + "stages/kfm.def").c_str(), "");
	}

	startFightScreen(mockFightFinishedCB);

	return "";
}

static std::string stageCB(void* tCaller, const std::string& tCommand) {
	(void)tCaller;
	std::vector<std::string> words = splitCommandString(tCommand);
	if (words.size() < 2) return "Too few arguments";

	setPlayerDefinitionPath(0, (getDolmexicaAssetFolder() + "chars/kfm/kfm.def").c_str());
	setPlayerDefinitionPath(1, (getDolmexicaAssetFolder() + "chars/kfm/kfm.def").c_str());
	setDreamStageMugenDefinition((getDolmexicaAssetFolder() + "stages/" + words[1]).c_str(), "");
	setGameModeTraining();
	startFightScreen(mockFightFinishedCB);

	return "";
}

static std::string fightdebugCB(void* tCaller, const std::string& tCommand) {
	(void)tCaller;
	(void)tCommand;
	switchFightDebugTextActivity();
	return "";
}

static std::string fightcollisionCB(void* tCaller, const std::string& tCommand) {
	(void)tCaller;
	(void)tCommand;
	switchFightCollisionDebugActivity();
	return "";
}

static std::string skipintroCB(void* tCaller, const std::string& tCommand) {
	(void)tCaller;
	(void)tCommand;
	skipFightIntroWithoutFading();
	return "";
}

static std::string rootposCB(void* tCaller, const std::string& tCommand) {
	(void)tCaller;
	std::vector<std::string> words = splitCommandString(tCommand);
	if (words.size() < 3) return "Too few arguments";

	int id = atoi(words[1].data());
	DreamPlayer* p = getRootPlayer(id);
	setPlayerPositionBasedOnScreenCenterX(p, (float)atof(words[2].data()), getPlayerCoordinateP(p));

	if (words.size() >= 4) {
		setPlayerPositionY(p, (float)atof(words[3].data()), getPlayerCoordinateP(p));
	}

	return "";
}

static std::string rootposcloseCB(void* tCaller, const std::string& tCommand) {
	(void)tCaller;
	std::vector<std::string> words = splitCommandString(tCommand);
	if (words.size() < 2) return "Too few arguments";

	float dist = (float)atof(words[1].data());
	setPlayerPositionBasedOnScreenCenterX(getRootPlayer(0), -dist / 2, getPlayerCoordinateP(getRootPlayer(0)));
	setPlayerPositionBasedOnScreenCenterX(getRootPlayer(1), dist / 2, getPlayerCoordinateP(getRootPlayer(1)));

	return "";
}

static std::string rootctrlCB(void* tCaller, const std::string& tCommand) {
	(void)tCaller;
	std::vector<std::string> words = splitCommandString(tCommand);
	if (words.size() < 3) return "Too few arguments";

	int id = atoi(words[1].data());
	DreamPlayer* p = getRootPlayer(id);
	setPlayerControl(p, atoi(words[2].data()));

	return "";
}

static std::string fullpowerCB(void* tCaller, const std::string& tCommand) {
	(void)tCaller;
	std::vector<std::string> words = splitCommandString(tCommand);
	if (words.size() < 2) return "Too few arguments";

	int id = atoi(words[1].data());
	DreamPlayer* p = getRootPlayer(id);
	setPlayerPower(p, getPlayerPowerMax(p));

	return "";
}

static std::string commandCB(void* tCaller, const std::string& tCommand) {
	(void)tCaller;
	std::vector<std::string> words = splitCommandString(tCommand);
	if (words.size() < 3) return "Too few arguments";
	
	int id = atoi(words[1].data());
	DreamPlayer* p = getRootPlayer(id);
	int bufferTime = (words.size() >= 4) ? atoi(words[3].data()) : 2;
	setDreamPlayerCommandActiveForAI(p->mCommandID, words[2].data(), bufferTime);
	return "";
}

static std::string testcommandNumberCB(void* tCaller, const std::string& tCommand) {
	(void)tCaller;
	std::vector<std::string> words = splitCommandString(tCommand);
	if (words.size() < 2) return "Too few arguments";

	int commandNumber = atoi(words[1].data());
	float dist = words.size() >= 3 ? (float)atof(words[2].data()) : 20;

	const int placementDirection = (words.size() >= 4) ? atoi(words[3].data()) : 1;
	setPlayerPositionBasedOnScreenCenterX(getRootPlayer(0), -placementDirection * dist / 2, getPlayerCoordinateP(getRootPlayer(0)));
	setPlayerPositionBasedOnScreenCenterX(getRootPlayer(1), placementDirection * dist / 2, getPlayerCoordinateP(getRootPlayer(1)));

	DreamPlayer* p = getRootPlayer(0);
	const auto result = setDreamPlayerCommandNumberActiveForDebug(p->mCommandID, commandNumber);
	return result ? "" : "Over command amount";
}

static std::string evalCB(void* tCaller, const std::string& tCommand) {
	(void)tCaller;
	std::vector<std::string> words = splitCommandString(tCommand);
	if (words.size() < 3) return "Too few arguments";
	int id = atoi(words[1].data());
	DreamPlayer* p = getRootPlayer(id);

	auto n = tCommand.find(' ', tCommand.find(' ') + 1) + 1;
	std::string assignmentString = tCommand.substr(n);
	char buffer[1024];
	strcpy(buffer, assignmentString.c_str());

	setActiveStateMachineCoordinateP(getPlayerCoordinateP(p));

	auto assignment = parseDreamMugenAssignmentFromString(buffer);
	std::string result;
	evaluateDreamAssignmentAndReturnAsString(result, &assignment, p);
	destroyDreamMugenAssignment(assignment);
	std::string ret = result;
	return ret;
}

static std::string evalhelperCB(void* tCaller, const std::string& tCommand) {
	(void)tCaller;
	std::vector<std::string> words = splitCommandString(tCommand);
	if (words.size() < 4) return "Too few arguments";
	int id = atoi(words[1].data());
	int helper = atoi(words[2].data());
	DreamPlayer* p = getRootPlayer(id);
	p = getPlayerHelperOrNullIfNonexistant(p, helper);
	if (!p) {
		return "No helper with that ID.";
	}

	auto n = tCommand.find(' ', tCommand.find(' ', tCommand.find(' ') + 1) + 1) + 1;
	std::string assignmentString = tCommand.substr(n);
	char buffer[1024];
	strcpy(buffer, assignmentString.c_str());

	setActiveStateMachineCoordinateP(getPlayerCoordinateP(p));

	auto assignment = parseDreamMugenAssignmentFromString(buffer);
	std::string result;
	evaluateDreamAssignmentAndReturnAsString(result, &assignment, p);
	destroyDreamMugenAssignment(assignment);
	std::string ret = result;
	return ret;
}

static std::string trackvarCB(void* tCaller, const std::string& tCommand) {
	(void)tCaller;
	std::vector<std::string> words = splitCommandString(tCommand);
	if (words.size() < 3) return "Too few arguments";
	int id = atoi(words[1].data());
	int varNumber = atoi(words[2].data());

	DreamPlayer* p = getRootPlayer(id);

	TrackedInteger e;
	e.mValuePointer = getPlayerVariableReference(p, varNumber);
	e.mPreviousValue = *e.mValuePointer;
	const auto s = std::string("player ").append(std::to_string(id)).append("; var ").append(std::to_string(varNumber));
	gDolmexicaDebugData->mMap[s] = e;
	
	const auto ret = std::string(" value: ").append(std::to_string(*e.mValuePointer));
	return ret;
}

static std::string untrackvarCB(void* tCaller, const std::string& tCommand) {
	(void)tCaller;
	std::vector<std::string> words = splitCommandString(tCommand);
	if (words.size() < 3) return "Too few arguments";
	int id = atoi(words[1].data());
	int varNumber = atoi(words[2].data());

	const auto s = std::string("player ").append(std::to_string(id)).append("; var ").append(std::to_string(varNumber));
	gDolmexicaDebugData->mMap.erase(s);
	return "";
}

static std::string stateCB(void* tCaller, const std::string& tCommand) {
	(void)tCaller;
	std::vector<std::string> words = splitCommandString(tCommand);
	if (words.size() < 3) return "Too few arguments";
	int id = atoi(words[1].data());
	int stateNumber = atoi(words[2].data());
	DreamPlayer* p = getRootPlayer(id);

	changePlayerState(p, stateNumber);
	return "";
}

static std::string storyCB(void* /*tCaller*/, const std::string& tCommand) {
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

static std::string randomwatchCB(void* /*tCaller*/, const std::string& /*tCommand*/) {
	startRandomWatchMode();
	return "";
}

static void setDebugTimeDilatation(float tSpeed) {
	setWrapperTimeDilatation(tSpeed);
	gDolmexicaDebugData->mIsOverridingTimeDilatation = (tSpeed != 1.0f) ? 1 : 0;
	gDolmexicaDebugData->mOverridingTimeDilatationSpeed = tSpeed;
}

static std::string speedCB(void* /*tCaller*/, const std::string& tCommand) {
	const auto words = splitCommandString(tCommand);
	if (words.size() < 2) return "Too few arguments";
	setDebugTimeDilatation((float)atof(words[1].c_str()));
	return "";
}

static std::string roundamountCB(void* /*tCaller*/, const std::string& tCommand) {
	const auto words = splitCommandString(tCommand);
	if (words.size() < 2) return "Too few arguments";
	const auto rounds = atoi(words[1].c_str());
	setRoundsToWin(rounds);
	return "";
}

static std::string readStoryAnimsCB(void* /*tCaller*/, const std::string& /*tCommand*/) {
	auto b = fileToBuffer("debug/anims.txt");
	auto p = getBufferPointer(b);

	while (hasStringFromTextStreamBufferPointer(p)) {
		const auto characterName = readStringFromTextStreamBufferPointer(&p);
	
		while (true) {
			if (!hasStringFromTextStreamBufferPointer(p)) break;
			const auto nextString = readStringFromTextStreamBufferPointer(&p);
			if (nextString == "------------------") break;

			const auto animValue = atoi(nextString.c_str());
			gDolmexicaDebugData->mStoryCharAnimations[characterName].insert(animValue);
		}
	}
	freeBuffer(b);

	return "";
}

static std::string writeStoryAnimsCB(void* /*tCaller*/, const std::string& /*tCommand*/) {
	std::stringstream ss;
	
	if (isFile("debug/anims.txt")) {
		auto b = fileToBuffer("debug/anims.txt");
		bufferToFile("debug/anims_old.txt", b);
		freeBuffer(b);
	}

	for (const auto& it : gDolmexicaDebugData->mStoryCharAnimations) {
		ss << it.first << std::endl << std::endl;
		for (const auto val : it.second) {
			ss << val << std::endl;
		}
		ss << "------------------" << std::endl;

	}

	bufferToFile("debug/anims.txt", makeBuffer((void*)ss.str().c_str(), uint32_t(ss.str().size())));

	return "";
}

static std::string difficultyCB(void* /*tCaller*/, const std::string& tCommand) {
	const auto words = splitCommandString(tCommand);
	if (words.size() < 2) return "Too few arguments";
	const auto difficulty = atoi(words[1].c_str());
	setDifficulty(difficulty);
	return "";
}

#if defined (_WIN32) || defined(VITA)
#include <filesystem>

static bool caseIndifferentManualTestNameSort (const std::string& lhs, const std::string& rhs) {
	return std::lexicographical_compare(lhs.begin(), lhs.end(),
		rhs.begin(), rhs.end(),
		[](char a, char b) {
		return tolower(a) < tolower(b);
	});
}
#define MANUAL_TEST_RESULTS_PATH "debug/manualtest_results.txt"
#define MANUAL_TEST_LIST_PATH "debug/manualtest_list.txt"

static const float MANUAL_TEST_SPEEDS[] = { 1.0f, 2.0f, 4.0f };
static const int MANUAL_TEST_SPEED_AMOUNT = sizeof(MANUAL_TEST_SPEEDS) / sizeof(MANUAL_TEST_SPEEDS[0]);

static struct {
	std::string mKind;
	std::string mAssetName;
	int mAssetIndex;
	int mAssetAmount;

	int mIsActive = 0;
	int mOverlayTextID = -1;
	int mSpeedIndex = 0;
} gManualTestData;

static std::string makeManualTestOverlayText() {
	std::ostringstream ss;
	ss << gManualTestData.mAssetName << "  " << gManualTestData.mAssetIndex << "/" << gManualTestData.mAssetAmount;
	if (gManualTestData.mSpeedIndex) ss << "  " << MANUAL_TEST_SPEEDS[gManualTestData.mSpeedIndex] << "x";
	return ss.str();
}

static void refreshManualTestOverlayText() {
	if (gManualTestData.mOverlayTextID == -1) return;
	changeMugenText(gManualTestData.mOverlayTextID, makeManualTestOverlayText().c_str());
}

static void loadManualTestOverlay() {
	if (!gManualTestData.mIsActive) return;

	gManualTestData.mOverlayTextID = addMugenText(makeManualTestOverlayText().c_str(), Vector3D(315, 236, 79), -1);
	setMugenTextAlignment(gManualTestData.mOverlayTextID, MUGEN_TEXT_ALIGNMENT_RIGHT);
}

static void finishManualTest() {
	gManualTestData.mIsActive = 0;
	gManualTestData.mOverlayTextID = -1;
}

static void startManualTestAsset(const std::string& tKind, const std::string& tAssetName, int tAssetIndex, int tAssetAmount) {
	gManualTestData.mIsActive = 1;
	gManualTestData.mKind = tKind;
	gManualTestData.mAssetName = tAssetName;
	gManualTestData.mAssetIndex = tAssetIndex;
	gManualTestData.mAssetAmount = tAssetAmount;
	gManualTestData.mOverlayTextID = -1;
}

static std::string makeManualTestTimestamp() {
	const auto now = std::time(nullptr);
	char text[32];
	std::strftime(text, sizeof(text), "%Y-%m-%dT%H:%M:%S", std::localtime(&now));
	return text;
}

static void recordManualTestVerdict(const char* tVerdict) {
	std::filesystem::create_directories("debug");
	std::ofstream file(MANUAL_TEST_RESULTS_PATH, std::ios::app);
	if (!file.is_open()) return;
	file << makeManualTestTimestamp() << "\t" << gManualTestData.mKind << "\t"
		<< gManualTestData.mAssetName << "\t" << tVerdict << "\t" << std::endl;
}

static std::vector<std::string> readManualTestResultLines() {
	std::vector<std::string> ret;
	std::ifstream file(MANUAL_TEST_RESULTS_PATH);
	if (!file.is_open()) return ret;
	std::string line;
	while (std::getline(file, line)) {
		if (!line.empty()) ret.push_back(line);
	}
	return ret;
}

static std::vector<std::string> splitManualTestResultLine(const std::string& tLine) {
	std::vector<std::string> ret;
	std::istringstream ss(tLine);
	std::string field;
	while (std::getline(ss, field, '\t')) {
		ret.push_back(field);
	}
	return ret;
}

static std::string turnManualTestNameLowercase(const std::string& tName) {
	std::string ret = tName;
	std::transform(ret.begin(), ret.end(), ret.begin(), [](unsigned char c) { return (char)tolower(c); });
	return ret;
}

static void addManualTestListLine(std::set<std::string>& oList, const std::string& tKind, const std::string& tLine) {
	auto line = tLine;
	while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t')) line.pop_back();
	if (line.empty() || line[0] == '#') return;

	const auto fields = splitManualTestResultLine(line);
	if (fields.size() >= 2) {
		if (fields[0] != tKind) return;
		oList.insert(turnManualTestNameLowercase(fields[1]));
		return;
	}
	oList.insert(turnManualTestNameLowercase(fields[0]));
}

static std::set<std::string> readManualTestListOfKind(const std::string& tKind) {
	std::set<std::string> ret;
	std::ifstream file(MANUAL_TEST_LIST_PATH);
	if (!file.is_open()) return ret;
	std::string line;
	while (std::getline(file, line)) {
		addManualTestListLine(ret, tKind, line);
	}
	return ret;
}

static int isAssetInManualTestList(const std::set<std::string>& tList, const std::string& tAssetName) {
	return tList.count(turnManualTestNameLowercase(tAssetName)) != 0;
}

static std::string getLastManualTestAssetOfKind(const std::string& tKind) {
	const auto lines = readManualTestResultLines();
	for (auto it = lines.rbegin(); it != lines.rend(); it++) {
		const auto fields = splitManualTestResultLine(*it);
		if (fields.size() < 3 || fields[1] != tKind) continue;
		return fields[2];
	}
	return "";
}

static std::string appendNoteToLastManualTestResult(const std::string& tNote) {
	auto lines = readManualTestResultLines();
	if (lines.empty()) return "No manual test result to annotate";

	lines.back() += tNote;
	std::ofstream file(MANUAL_TEST_RESULTS_PATH, std::ios::trunc);
	if (!file.is_open()) return "Unable to write " MANUAL_TEST_RESULTS_PATH;
	for (const auto& line : lines) {
		file << line << std::endl;
	}
	return "";
}

static std::string noteCB(void* /*tCaller*/, const std::string& tCommand) {
	const auto separator = tCommand.find(' ');
	if (separator == std::string::npos) return "Too few arguments";
	return appendNoteToLastManualTestResult(tCommand.substr(separator + 1));
}

static const char* getManualTestVerdictFromKeys() {
	if (hasPressedKeyboardKeyFlank(KEYBOARD_F1_PRISM)) return "pass";
	if (hasPressedKeyboardKeyFlank(KEYBOARD_F7_PRISM)) return "fail";
	if (hasPressedKeyboardKeyFlank(KEYBOARD_F8_PRISM)) return "flag";
	return nullptr;
}

static void updateManualTestSpeedCycle() {
	if (!hasPressedKeyboardKeyFlank(KEYBOARD_F9_PRISM)) return;

	gManualTestData.mSpeedIndex = (gManualTestData.mSpeedIndex + 1) % MANUAL_TEST_SPEED_AMOUNT;
	setDebugTimeDilatation(MANUAL_TEST_SPEEDS[gManualTestData.mSpeedIndex]);
	refreshManualTestOverlayText();
}

static size_t getManualTestVarietySelectionIndex(const std::string& tAssetName, int tSalt, size_t tAmount) {
	if (!tAmount) return 0;
	return (std::hash<std::string>{}(tAssetName) + size_t(tSalt)) % tAmount;
}

static struct {
	int mIsActive;
	int mIsAutoMode;
	int mIsVarietyMode;
	std::set<std::string, bool(*)(const std::string& lhs, const std::string& rhs)> mCharacters = std::set<std::string, bool(*)(const std::string& lhs, const std::string& rhs)>(caseIndifferentManualTestNameSort);
	std::vector<std::string> mAllCharacters;
	std::vector<std::string> mAllStages;
	int mCharacterAmount;
	int mCurrentCharacterIndex;
} gFullCharacterTestData;

static void collectFullCharacterTestCharacters() {
	std::filesystem::path folderMask = getDolmexicaAssetFolder() + "chars";
	for (auto const& dir_entry : std::filesystem::directory_iterator{ folderMask })
	{
		gFullCharacterTestData.mCharacters.insert(dir_entry.path().stem().string());
	}
	gFullCharacterTestData.mAllCharacters.assign(gFullCharacterTestData.mCharacters.begin(), gFullCharacterTestData.mCharacters.end());
	gFullCharacterTestData.mCharacterAmount = int(gFullCharacterTestData.mCharacters.size());
}

static void pruneFullCharacterTestCharacters(const std::string& tStartCharacter, int tIsExcludingStartCharacter) {
	if (tStartCharacter.empty()) return;

	auto it = gFullCharacterTestData.mCharacters.begin();
	while (it != gFullCharacterTestData.mCharacters.end()) {
		auto current = it;
		it++;
		const auto isStartCharacter = (*current == tStartCharacter);
		if (isStartCharacter && !tIsExcludingStartCharacter) break;
		gFullCharacterTestData.mCharacters.erase(current);
		if (isStartCharacter) break;
	}
}

static void pruneFullCharacterTestCharactersToPickedList() {
	const auto pickedList = readManualTestListOfKind("character");
	auto it = gFullCharacterTestData.mCharacters.begin();
	while (it != gFullCharacterTestData.mCharacters.end()) {
		auto current = it;
		it++;
		if (isAssetInManualTestList(pickedList, *current)) continue;
		gFullCharacterTestData.mCharacters.erase(current);
	}
	gFullCharacterTestData.mCharacterAmount = int(gFullCharacterTestData.mCharacters.size());
}

static void processFullCharacterTestCharacters(const std::string& tStartCharacter, int tIsExcludingStartCharacter, int tIsPickedListMode)
{
	gFullCharacterTestData.mCharacters.clear();
	gFullCharacterTestData.mAllCharacters.clear();
	collectFullCharacterTestCharacters();
	if (tIsPickedListMode) pruneFullCharacterTestCharactersToPickedList();
	pruneFullCharacterTestCharacters(tStartCharacter, tIsExcludingStartCharacter);
	gFullCharacterTestData.mCurrentCharacterIndex = gFullCharacterTestData.mCharacterAmount - int(gFullCharacterTestData.mCharacters.size());
}

static void fullManualTestFightFinishedCB() {}

static void collectFullCharacterTestVarietyStages() {
	if (!gFullCharacterTestData.mAllStages.empty()) return;
	std::filesystem::path folderMask = getDolmexicaAssetFolder() + "stages/";
	for (auto const& dir_entry : std::filesystem::recursive_directory_iterator{ folderMask })
	{
		if (!dir_entry.is_regular_file()) continue;
		if (dir_entry.path().extension().string() != ".def") continue;
		auto stagePath = dir_entry.path().generic_string();
		cleanPathSlashes(stagePath);
		gFullCharacterTestData.mAllStages.push_back(stagePath);
	}
}

static void setFullCharacterTestVarietyOpponentAndStage(const std::string& tCharacterName) {
	collectFullCharacterTestVarietyStages();

	const auto& opponents = gFullCharacterTestData.mAllCharacters;
	if (!opponents.empty()) {
		const auto opponentIndex = getManualTestVarietySelectionIndex(tCharacterName, 0, opponents.size());
		char path[1024];
		getCharacterSelectNamePath(opponents[opponentIndex].c_str(), path);
		setPlayerDefinitionPath(1, path);
	}

	const auto& stages = gFullCharacterTestData.mAllStages;
	if (stages.empty()) return;
	const auto stageIndex = getManualTestVarietySelectionIndex(tCharacterName, 1, stages.size());
	setDreamStageMugenDefinition(stages[stageIndex].c_str(), "");
}

static void startNextFullCharacterTestFight() {
	if (gFullCharacterTestData.mCharacters.empty()) {
		finishManualTest();
		abortScreenHandling();
		return;
	}
	const auto characterName = *gFullCharacterTestData.mCharacters.begin();
	gFullCharacterTestData.mCharacters.erase(gFullCharacterTestData.mCharacters.begin());
	gFullCharacterTestData.mCurrentCharacterIndex++;

	char path[1024];
	getCharacterSelectNamePath(characterName.c_str(), path);
	setPlayerDefinitionPath(0, path);
	setPlayerDefinitionPath(1, path);
	setDreamStageMugenDefinition((getDolmexicaAssetFolder() + "stages/kfm.def").c_str(), "");
	if (gFullCharacterTestData.mIsVarietyMode) setFullCharacterTestVarietyOpponentAndStage(characterName);
	setGameModeSuperWatch();
	setRandomSeed(0);
	startManualTestAsset("character", characterName, gFullCharacterTestData.mCurrentCharacterIndex, gFullCharacterTestData.mCharacterAmount);
	startFightScreen(fullManualTestFightFinishedCB);
}

static std::string fullCharacterTestCB(void* /*tCaller*/, const std::string& tCommand) {
	const auto words = splitCommandString(tCommand);
	std::string startCharacter;
	int isResuming = 0;
	int isPickedListMode = 0;
	gFullCharacterTestData.mIsAutoMode = 0;
	gFullCharacterTestData.mIsVarietyMode = 0;
	for (size_t i = 1; i < words.size(); i++) {
		if (words[i] == "auto") gFullCharacterTestData.mIsAutoMode = 1;
		else if (words[i] == "variety") gFullCharacterTestData.mIsVarietyMode = 1;
		else if (words[i] == "resume") isResuming = 1;
		else if (words[i] == "list") isPickedListMode = 1;
		else startCharacter = words[i];
	}
	if (isResuming) startCharacter = getLastManualTestAssetOfKind("character");

	processFullCharacterTestCharacters(startCharacter, isResuming, isPickedListMode);
	if (gFullCharacterTestData.mCharacters.empty()) return isPickedListMode ? "No characters picked in " MANUAL_TEST_LIST_PATH : "No characters left to test";
	gFullCharacterTestData.mIsActive = 1;
	startNextFullCharacterTestFight();
	return "";
}

static int isFullCharacterTestAutomaticallyOver() {
	static const auto TEST_SECONDS = 120;
	if (!gFullCharacterTestData.mIsAutoMode) return 0;
	return getDreamGameTime() > TEST_SECONDS * 60;
}

static void updateFullCharacterTest() {
	if (!gFullCharacterTestData.mIsActive) return;

	updateManualTestSpeedCycle();
	const auto verdict = getManualTestVerdictFromKeys();
	if (!verdict && !isFullCharacterTestAutomaticallyOver()) return;

	recordManualTestVerdict(verdict ? verdict : "auto");
	startNextFullCharacterTestFight();
}

static struct {
	int mIsActive;
	std::set<std::string, bool(*)(const std::string& lhs, const std::string& rhs)> mStages = std::set<std::string, bool(*)(const std::string& lhs, const std::string& rhs)>(caseIndifferentManualTestNameSort);
	int mStageAmount;
	int mCurrentStageIndex;
} gFullStageTestData;

static void collectFullStageTestStagesRecursive(const std::string& tSearchPath) {
	std::filesystem::path folderMask = tSearchPath;
	for (auto const& dir_entry : std::filesystem::directory_iterator{ folderMask })
	{
		if (dir_entry.is_directory())
		{
			collectFullStageTestStagesRecursive(tSearchPath + dir_entry.path().stem().string() + "/");
		}
		else {
			const auto extension = dir_entry.path().extension().string();
			if (extension != ".def") continue;
			auto filePath = tSearchPath + dir_entry.path().filename().string();
			cleanPathSlashes(filePath);
			gFullStageTestData.mStages.insert(filePath);

		}
	}
}

static void pruneFullStageTestStages(const std::string& tStartStage, int tIsExcludingStartStage) {
	if (tStartStage.empty()) return;

	auto it = gFullStageTestData.mStages.begin();
	while (it != gFullStageTestData.mStages.end()) {
		auto current = it;
		it++;
		const auto isStartStage = (*current == tStartStage);
		if (isStartStage && !tIsExcludingStartStage) break;
		gFullStageTestData.mStages.erase(current);
		if (isStartStage) break;
	}
}

static void pruneFullStageTestStagesToPickedList() {
	const auto pickedList = readManualTestListOfKind("stage");
	auto it = gFullStageTestData.mStages.begin();
	while (it != gFullStageTestData.mStages.end()) {
		auto current = it;
		it++;
		if (isAssetInManualTestList(pickedList, *current)) continue;
		gFullStageTestData.mStages.erase(current);
	}
}

static void processFullStageTestStages(const std::string& tStartStage, int tIsExcludingStartStage, int tIsPickedListMode)
{
	gFullStageTestData.mStages.clear();
	collectFullStageTestStagesRecursive(getDolmexicaAssetFolder() + "stages/");
	if (tIsPickedListMode) pruneFullStageTestStagesToPickedList();
	gFullStageTestData.mStageAmount = int(gFullStageTestData.mStages.size());
	pruneFullStageTestStages(tStartStage, tIsExcludingStartStage);
	gFullStageTestData.mCurrentStageIndex = gFullStageTestData.mStageAmount - int(gFullStageTestData.mStages.size());
}

static void startNextFullStageTestFight() {
	if (gFullStageTestData.mStages.empty()) {
		finishManualTest();
		abortScreenHandling();
		return;
	}
	const auto stageName = *gFullStageTestData.mStages.begin();
	gFullStageTestData.mStages.erase(gFullStageTestData.mStages.begin());
	gFullStageTestData.mCurrentStageIndex++;

	setPlayerDefinitionPath(0, (getDolmexicaAssetFolder() + "chars/kfm/kfm.def").c_str());
	setPlayerDefinitionPath(1, (getDolmexicaAssetFolder() + "chars/kfm/kfm.def").c_str());
	setDreamStageMugenDefinition(stageName.c_str(), "");
	setGameModeTraining();
	setRandomSeed(0);
	startManualTestAsset("stage", stageName, gFullStageTestData.mCurrentStageIndex, gFullStageTestData.mStageAmount);
	startFightScreen(fullManualTestFightFinishedCB);
}

static std::string fullStageTestCB(void* /*tCaller*/, const std::string& tCommand) {
	const auto words = splitCommandString(tCommand);
	std::string startStage;
	int isResuming = 0;
	int isPickedListMode = 0;
	for (size_t i = 1; i < words.size(); i++) {
		if (words[i] == "resume") isResuming = 1;
		else if (words[i] == "list") isPickedListMode = 1;
		else startStage = words[i];
	}
	if (isResuming) startStage = getLastManualTestAssetOfKind("stage");

	processFullStageTestStages(startStage, isResuming, isPickedListMode);
	if (gFullStageTestData.mStages.empty()) return isPickedListMode ? "No stages picked in " MANUAL_TEST_LIST_PATH : "No stages left to test";
	gFullStageTestData.mIsActive = 1;
	startNextFullStageTestFight();
	return "";
}

static void updateFullStageTest() {
	if (!gFullStageTestData.mIsActive) return;

	updateManualTestSpeedCycle();
	const auto verdict = getManualTestVerdictFromKeys();
	if (!verdict) return;

	recordManualTestVerdict(verdict);
	startNextFullStageTestFight();
}
#else
static std::string fullCharacterTestCB(void*, const std::string&) {
	return "";
}

static void updateFullCharacterTest() {}

static std::string fullStageTestCB(void*, const std::string&) {
	return "";
}

static void updateFullStageTest() {}

static std::string noteCB(void*, const std::string&) {
	return "";
}

static void loadManualTestOverlay() {}
#endif

static std::string randomSeedCB(void* /*tCaller*/, const std::string& tCommand) {
	const auto words = splitCommandString(tCommand);
	if (words.size() < 2) return "Too few arguments";
	const auto seed = atoi(words[1].c_str());
	setRandomSeed(unsigned(seed));
	return "";
}

static std::string airJumpCB(void* /*tCaller*/, const std::string& tCommand) {
	const auto words = splitCommandString(tCommand);
	if (words.size() < 2) return "Too few arguments";
	const auto airJumpAmount = atoi(words[1].c_str());
	setPlayerMovementAirJumpNum(getRootPlayer(0), airJumpAmount);
	return "";
}

static std::string setDebugStateCB(void* /*tCaller*/, const std::string& tCommand) {
	const auto words = splitCommandString(tCommand);
	int debugStartState = 0;
	int fromState = 0; 
	if(words.size() >= 3)
	{
		fromState = atoi(words[2].c_str());
	}
	if (words.size() < 2) 
	{
		debugStartState = getDolmexicaStoryStateNumber(getDolmexicaStoryRootInstance());
	}
	else
	{
		debugStartState = atoi(words[1].c_str());
	}
	setDolmexicaStoryDebugStartState(fromState, debugStartState);
	return "";
}

void initDolmexicaDebug()
{
	gDolmexicaDebugData = new DolmexicaDebugData();
	gDolmexicaDebugData->mIsOverridingTimeDilatation = 0;
	gDolmexicaDebugData->mOverridingTimeDilatationSpeed = 1.0;

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
	addPrismDebugConsoleCommand("readstoryanims", readStoryAnimsCB);
	addPrismDebugConsoleCommand("writestoryanims", writeStoryAnimsCB);
	addPrismDebugConsoleCommand("difficulty", difficultyCB);
	addPrismDebugConsoleCommand("fullcharactertest", fullCharacterTestCB);
	addPrismDebugConsoleCommand("fullstagetest", fullStageTestCB);
	addPrismDebugConsoleCommand("note", noteCB);
	addPrismDebugConsoleCommand("randomseed", randomSeedCB);
	addPrismDebugConsoleCommand("airjump", airJumpCB);
	addPrismDebugConsoleCommand("setdebugstate", setDebugStateCB);
}

static void loadDolmexicaDebugHandler(void* tData) {
	(void)tData;
	setProfilingSectionMarkerCurrentFunction();
	gDolmexicaDebugData->mMap.clear();
	loadManualTestOverlay();
}

static void updateSingleTrackedInteger(void* tCaller, const std::string& tKey, TrackedInteger& e) {
	(void)tCaller;

	if (*e.mValuePointer != e.mPreviousValue) {
		std::ostringstream ss;
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


static void dumpEntityCensus() {
	logg("=== ENTITY CENSUS (F10) ===");
	const int total = getTotalPlayerAmount();
	for (int i = 0; i < total; i++) {
		DreamPlayer* p = getPlayerByIndex(i);
		if (!p) continue;
		logFormat("entity %d: root=%d id=%d helper=%d proj=%d state=%d prevstate=%d anim=%d pos=%.1f/%.1f alive=%d hitpaused=%d ctrl=%d movetype=%d statetype=%d",
			i, p->mRootID, p->mID, isPlayerHelper(p), isPlayerProjectile(p),
			getPlayerState(p), getPlayerPreviousState(p), getPlayerAnimationNumber(p),
			(double)getPlayerPositionX(p, 320), (double)getPlayerPositionY(p, 320),
			isPlayerAlive(p), isPlayerHitPaused(p), getPlayerControl(p),
			(int)getPlayerStateMoveType(p), (int)getPlayerStateType(p));
		logFormat("  explods=%d helpers=%d targets=%d usingTempStates=%d",
			getExplodAmount(p), getPlayerHelperAmount(p), getPlayerTargetAmount(p),
			!isInOwnStateMachine(p->mRegisteredStateMachine));
	}
	logg("=== CENSUS END ===");
}

static void updateEntityCensusDump() {
	if (hasPressedKeyboardKeyFlank(KEYBOARD_F10_PRISM)) {
		dumpEntityCensus();
	}
}

static void updateDolmexicaDebugHandler(void* tData) {
	(void)tData;
	setProfilingSectionMarkerCurrentFunction();
	updateSpeedOverrideToggle();
	updateEntityCensusDump();
	updateFullCharacterTest();
	updateFullStageTest();
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

void addDebugDolmexicaStoryCharacterAnimation(const char * tCharacter, int tAnimation)
{
	if (!gDolmexicaDebugData) return;
	std::string name = tCharacter;
	turnStringLowercase(name);
	gDolmexicaDebugData->mStoryCharAnimations[name].insert(tAnimation);
}

const std::set<int>& getDebugDolmexicaStoryCharacterAnimations(const char* tCharacter)
{
	static const std::set<int> emptyAnimationSet;
	if (!gDolmexicaDebugData) return emptyAnimationSet;
	std::string name = tCharacter;
	turnStringLowercase(name);
	const auto it = gDolmexicaDebugData->mStoryCharAnimations.find(name);
	if (it == gDolmexicaDebugData->mStoryCharAnimations.end()) return emptyAnimationSet;
	return it->second;
}
