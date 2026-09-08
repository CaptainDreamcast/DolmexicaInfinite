#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <vector>

#include <prism/wrapper.h>
#include <prism/file.h>
#include <prism/system.h>
#include <prism/texture.h>
#include <prism/debug.h>
#include <prism/log.h>
#include <prism/math.h>

#include "commontestfunctionality.h"
#include "censuscommon.h"

#include "config.h"
#include "playerdefinition.h"
#include "mugenstagehandler.h"
#include "stage.h"
#include "gamelogic.h"
#include "fightscreen.h"

/* Renders and screenshots every character and stage in the assets folder so a
 * release pass can review batched evidence instead of watching fights live.
 * Output layout, consumed by tools/releasereport/generate_report.py:
 *   debug/report/chars/<name>/NNNNN.png + manifest.json
 *   debug/report/stages/<path>/NNNNN.png + manifest.json
 * Each asset's manifest carries a "flags" array: engine warnings logged while
 * that asset was loaded and played, census anomalies, and fight-progress
 * checks. An anomaly also triggers a screenshot of the frame it fired on,
 * captioned with the anomaly text.
 * Configuration via environment:
 *   DOLMEXICA_CAPTURE_SEED=0     random seed for the captured fights
 *   DOLMEXICA_CAPTURE_FRAMES=... frames per character (default 3 minutes)
 *   DOLMEXICA_ASSET_FILTER=...   only assets whose name contains this */

#define MEDIA_CAPTURE_REPORT_FOLDER "debug/report"

static const auto MEDIA_CAPTURE_CHARACTER_ITERATIONS = 60 * 60 * 3;
static const auto MEDIA_CAPTURE_CHARACTER_INTERVAL_FRAMES = 60 * 5;
static const auto MEDIA_CAPTURE_EVENT_COOLDOWN_FRAMES = 30;
static const auto MEDIA_CAPTURE_LIFE_DROP_THRESHOLD = 60;
static const auto MEDIA_CAPTURE_ROUND_START_TIMEOUT_FRAMES = 60 * 10;
static const auto MEDIA_CAPTURE_STAGE_IDLE_AMOUNT = 3;
static const auto MEDIA_CAPTURE_STAGE_IDLE_SPACING_FRAMES = 60 * 2;
static const auto MEDIA_CAPTURE_STAGE_SETTLE_FRAMES = 10;
static const auto MEDIA_CAPTURE_MAXIMUM_WARNING_AMOUNT = 20;

class MediaCaptureTest : public ::testing::Test {
protected:
	void SetUp() override {
		setupTestForScreenTestInAssetsFolder();
		setPrismDebugSideDisplayVisibility(0);
	}

	void TearDown() override {
		tearDownTestForScreenTestInAssetsFolder();
	}
};

struct CaptureContext {
	int mFrame = 0;
	int mRound = 0;
	int mLife[2] = { 0, 0 };
	int mEntityAmount = 0;
};

static struct {
	std::string mOutputPath;
	std::string mKind;
	std::string mName;
	int mFrame;
	int mCaptureAmount;
	int mLastCaptureFrame;
	int mWarningAmount;
	int mErrorAmount;
	std::vector<std::string> mManifestEntries;
	std::vector<std::string> mFlags;
	std::vector<std::string> mWarnings;
	std::set<std::string> mSeenTexts;
	std::set<std::string> mPrintedErrors;
} gMediaCaptureData;

static int getMediaCaptureCharacterIterations() {
	const char* framesEnv = getenv("DOLMEXICA_CAPTURE_FRAMES");
	return framesEnv ? atoi(framesEnv) : MEDIA_CAPTURE_CHARACTER_ITERATIONS;
}

static unsigned int getMediaCaptureRandomSeed() {
	const char* seedEnv = getenv("DOLMEXICA_CAPTURE_SEED");
	return seedEnv ? (unsigned int)atoi(seedEnv) : 0u;
}

// Mugen assets carry Shift-JIS and Latin-1 bytes in their names and file paths, and a warning quoting one would leave the manifest invalid UTF-8 that the report generator cannot read. Escaped per byte, it stays valid JSON and still recoverable.
static void appendEscapedJsonCharacter(std::string& oText, char tCharacter) {
	const auto value = (unsigned char)tCharacter;
	if (value == '"' || value == '\\') {
		oText += '\\';
		oText += char(value);
		return;
	}
	if (value >= 0x20 && value < 0x80) {
		oText += char(value);
		return;
	}
	char escape[8];
	snprintf(escape, sizeof(escape), "\\u%04x", (unsigned int)value);
	oText += escape;
}

static std::string escapeJsonText(const std::string& tText) {
	std::string ret;
	for (const auto character : tText) {
		if (character == '\n' || character == '\r' || character == '\t') {
			ret += ' ';
			continue;
		}
		appendEscapedJsonCharacter(ret, character);
	}
	return ret;
}

static int addAssetTextOnce(std::vector<std::string>& oTexts, const std::string& tText) {
	if (gMediaCaptureData.mSeenTexts.count(tText)) return 0;
	gMediaCaptureData.mSeenTexts.insert(tText);
	oTexts.push_back(tText);
	return 1;
}

static int addAssetFlag(const std::string& tFlag) {
	return addAssetTextOnce(gMediaCaptureData.mFlags, tFlag);
}

static int addAssetWarning(const std::string& tWarning) {
	return addAssetTextOnce(gMediaCaptureData.mWarnings, tWarning);
}

static int isFlaggableLogType(LogType tType) {
	return tType == LOG_TYPE_WARNING || tType == LOG_TYPE_ERROR;
}

static std::string makeAssetLogText(const LogEntry& tEntry) {
	std::string text = tEntry.mText;
	while (!text.empty() && (text.back() == '\n' || text.back() == '\r' || text.back() == ' ')) text.pop_back();
	return (tEntry.mType == LOG_TYPE_ERROR ? "error: " : "warning: ") + text;
}

// An engine error usually aborts the process before finishAssetCapture writes the manifest, so a flag that only exists in memory dies with the run. Errors go to the console as they happen, past the LOG_TYPE_NONE silence, or an asset that kills the run leaves no evidence of why.
static void printAssetErrorFlagOnce(const std::string& tFlag) {
	if (gMediaCaptureData.mPrintedErrors.count(tFlag)) return;
	gMediaCaptureData.mPrintedErrors.insert(tFlag);
	printf("[%s] %s\n", gMediaCaptureData.mName.c_str(), tFlag.c_str());
	fflush(stdout);
}

static int isWithinAssetWarningLimit() {
	return gMediaCaptureData.mWarningAmount < MEDIA_CAPTURE_MAXIMUM_WARNING_AMOUNT;
}

static int isWithinAssetErrorLimit() {
	return gMediaCaptureData.mErrorAmount < MEDIA_CAPTURE_MAXIMUM_WARNING_AMOUNT;
}

// Warnings are noise on nearly every Mugen asset, so they are recorded for the report to show but never flag the asset. Errors and census anomalies are what a release pass has to look at.
static void collectAssetLogEntry(const LogEntry& tEntry) {
	const auto text = makeAssetLogText(tEntry);
	if (tEntry.mType == LOG_TYPE_ERROR) {
		printAssetErrorFlagOnce(text);
		if (!isWithinAssetErrorLimit()) return;
		gMediaCaptureData.mErrorAmount += addAssetFlag(text);
		return;
	}
	if (!isWithinAssetWarningLimit()) return;
	gMediaCaptureData.mWarningAmount += addAssetWarning(text);
}

static void collectAssetWarningCB(void* /*tCaller*/, const LogEntry& tEntry) {
	if (!isFlaggableLogType(tEntry.mType)) return;
	collectAssetLogEntry(tEntry);
}

static void beginAssetCapture(const std::string& tKind, const std::string& tName, const std::string& tOutputPath) {
	gMediaCaptureData.mOutputPath = tOutputPath;
	gMediaCaptureData.mKind = tKind;
	gMediaCaptureData.mName = tName;
	gMediaCaptureData.mFrame = 0;
	gMediaCaptureData.mCaptureAmount = 0;
	gMediaCaptureData.mLastCaptureFrame = -MEDIA_CAPTURE_EVENT_COOLDOWN_FRAMES;
	gMediaCaptureData.mWarningAmount = 0;
	gMediaCaptureData.mErrorAmount = 0;
	gMediaCaptureData.mManifestEntries.clear();
	gMediaCaptureData.mFlags.clear();
	gMediaCaptureData.mWarnings.clear();
	gMediaCaptureData.mSeenTexts.clear();
	gMediaCaptureData.mPrintedErrors.clear();

	std::error_code ec;
	std::filesystem::create_directories(tOutputPath, ec);
	setLogCallback(collectAssetWarningCB, NULL);
}

static std::string makeCaptureFileName(int tCaptureIndex) {
	auto ret = std::to_string(tCaptureIndex);
	while (ret.size() < 5) ret = "0" + ret;
	return ret + ".png";
}

static void addCaptureManifestEntry(const std::string& tFileName, const char* tReason, const CaptureContext& tContext, const std::string& tNote) {
	std::string entry = "{\"file\": \"" + escapeJsonText(tFileName) + "\"";
	entry += ", \"frame\": " + std::to_string(tContext.mFrame);
	entry += ", \"round\": " + std::to_string(tContext.mRound);
	entry += ", \"life\": [" + std::to_string(tContext.mLife[0]) + ", " + std::to_string(tContext.mLife[1]) + "]";
	entry += ", \"entities\": " + std::to_string(tContext.mEntityAmount);
	entry += ", \"reason\": \"" + escapeJsonText(tReason) + "\"";
	if (!tNote.empty()) entry += ", \"note\": \"" + escapeJsonText(tNote) + "\"";
	entry += "}";
	gMediaCaptureData.mManifestEntries.push_back(entry);
}

static CaptureContext readCaptureContext() {
	CaptureContext ret;
	ret.mFrame = gMediaCaptureData.mFrame;
	ret.mRound = getDreamRoundNumber();
	for (int side = 0; side < 2; side++) {
		DreamPlayer* root = getRootPlayer(side);
		ret.mLife[side] = root ? getPlayerLife(root) : 0;
	}
	ret.mEntityAmount = getTotalPlayerAmount();
	return ret;
}

static void captureAssetFrameWithNote(const char* tReason, const CaptureContext& tContext, const std::string& tNote) {
	renderPrismWrapperScreenForDebugOnce();

	const auto fileName = makeCaptureFileName(gMediaCaptureData.mCaptureAmount);
	saveScreenShot((gMediaCaptureData.mOutputPath + "/" + fileName).c_str());
	addCaptureManifestEntry(fileName, tReason, tContext, tNote);

	gMediaCaptureData.mCaptureAmount++;
	gMediaCaptureData.mLastCaptureFrame = tContext.mFrame;
}

static void captureAssetFrame(const char* tReason, const CaptureContext& tContext) {
	captureAssetFrameWithNote(tReason, tContext, "");
}

static void captureCurrentAssetFrame(const char* tReason) {
	captureAssetFrame(tReason, readCaptureContext());
}

static void writeCaptureManifestHeader(std::ofstream& tFile) {
	const auto screenSize = getDisplayedScreenSize();
	tFile << "{\n";
	tFile << "\t\"kind\": \"" << escapeJsonText(gMediaCaptureData.mKind) << "\",\n";
	tFile << "\t\"name\": \"" << escapeJsonText(gMediaCaptureData.mName) << "\",\n";
	tFile << "\t\"seed\": " << getMediaCaptureRandomSeed() << ",\n";
	tFile << "\t\"screenWidth\": " << screenSize.x << ",\n";
	tFile << "\t\"screenHeight\": " << screenSize.y << ",\n";
}

static void writeCaptureManifestTextArray(std::ofstream& tFile, const char* tName, const std::vector<std::string>& tTexts) {
	tFile << "\t\"" << tName << "\": [";
	for (size_t i = 0; i < tTexts.size(); i++) {
		if (i) tFile << ", ";
		tFile << "\"" << escapeJsonText(tTexts[i]) << "\"";
	}
	tFile << "],\n";
}

static void writeCaptureManifestFlags(std::ofstream& tFile) {
	writeCaptureManifestTextArray(tFile, "flags", gMediaCaptureData.mFlags);
	writeCaptureManifestTextArray(tFile, "warnings", gMediaCaptureData.mWarnings);
}

static void writeCaptureManifestEntries(std::ofstream& tFile) {
	tFile << "\t\"captures\": [\n";
	for (size_t i = 0; i < gMediaCaptureData.mManifestEntries.size(); i++) {
		tFile << "\t\t" << gMediaCaptureData.mManifestEntries[i];
		if (i + 1 < gMediaCaptureData.mManifestEntries.size()) tFile << ",";
		tFile << "\n";
	}
	tFile << "\t]\n";
	tFile << "}\n";
}

static void finishAssetCapture() {
	resetLogCallback();

	std::ofstream file(gMediaCaptureData.mOutputPath + "/manifest.json");
	if (!file.is_open()) return;
	writeCaptureManifestHeader(file);
	writeCaptureManifestFlags(file);
	writeCaptureManifestEntries(file);
}

static void advanceCaptureFightByOneFrame() {
	updatePrismWrapperScreenForDebugWithIterations(1);
	gMediaCaptureData.mFrame++;
}

static void advanceCaptureFightByFrames(int tFrames) {
	while (tFrames--) {
		advanceCaptureFightByOneFrame();
	}
}

static void advanceCaptureFightUntilRoundStarted() {
	for (int frame = 0; frame < MEDIA_CAPTURE_ROUND_START_TIMEOUT_FRAMES; frame++) {
		if (getDreamRoundStateNumber() >= 2) return;
		advanceCaptureFightByOneFrame();
	}
}

static void logCurrentlyCapturedAsset(const std::string& tAssetName) {
	setMinimumLogType(LOG_TYPE_NORMAL);
	logFormat("Capturing %s", tAssetName.c_str());
	setMinimumLogType(LOG_TYPE_NONE);
}

// ---------------------------------------------------------------- characters

struct CharacterCaptureTracker {
	int mRound = 0;
	int mLife[2] = { 0, 0 };
	int mEntityHighWaterMark = 0;
	int mHasLifeChanged = 0;
	int mHasReadAnyFrame = 0;
};

static int isCharacterCaptureIntervalFrame(const CaptureContext& tContext) {
	return (tContext.mFrame % MEDIA_CAPTURE_CHARACTER_INTERVAL_FRAMES) == 0;
}

static int isWithinEventCaptureCooldown(const CaptureContext& tContext) {
	return (tContext.mFrame - gMediaCaptureData.mLastCaptureFrame) < MEDIA_CAPTURE_EVENT_COOLDOWN_FRAMES;
}

static int hasRoundChanged(const CharacterCaptureTracker& tTracker, const CaptureContext& tContext) {
	return tContext.mRound != tTracker.mRound;
}

static int hasLifeDroppedSharply(const CharacterCaptureTracker& tTracker, const CaptureContext& tContext) {
	for (int side = 0; side < 2; side++) {
		if (tTracker.mLife[side] - tContext.mLife[side] >= MEDIA_CAPTURE_LIFE_DROP_THRESHOLD) return 1;
	}
	return 0;
}

static int hasLifeChangedSincePreviousFrame(const CharacterCaptureTracker& tTracker, const CaptureContext& tContext) {
	for (int side = 0; side < 2; side++) {
		if (tTracker.mLife[side] != tContext.mLife[side]) return 1;
	}
	return 0;
}

static int hasEntityAmountSpiked(const CharacterCaptureTracker& tTracker, const CaptureContext& tContext) {
	return tContext.mEntityAmount > tTracker.mEntityHighWaterMark;
}

static const char* getCharacterCaptureReason(const CharacterCaptureTracker& tTracker, const CaptureContext& tContext) {
	if (isCharacterCaptureIntervalFrame(tContext)) return "interval";
	if (!tTracker.mHasReadAnyFrame) return nullptr;
	if (isWithinEventCaptureCooldown(tContext)) return nullptr;
	if (hasRoundChanged(tTracker, tContext)) return "round";
	if (hasLifeDroppedSharply(tTracker, tContext)) return "hit";
	if (hasEntityAmountSpiked(tTracker, tContext)) return "entities";
	return nullptr;
}

static void updateCharacterCaptureTracker(CharacterCaptureTracker& tTracker, const CaptureContext& tContext) {
	if (tTracker.mHasReadAnyFrame && hasLifeChangedSincePreviousFrame(tTracker, tContext)) tTracker.mHasLifeChanged = 1;
	tTracker.mHasReadAnyFrame = 1;
	tTracker.mRound = tContext.mRound;
	tTracker.mLife[0] = tContext.mLife[0];
	tTracker.mLife[1] = tContext.mLife[1];
	tTracker.mEntityHighWaterMark = std::max(tTracker.mEntityHighWaterMark, tContext.mEntityAmount);
}

static int captureCharacterAnomaliesForFrame(const CaptureContext& tContext) {
	const auto addedAnomalies = updateCensusWithCurrentFrame(tContext.mFrame, tContext.mRound);
	if (!addedAnomalies) return 0;

	const auto& anomalies = getCensusAnomalies();
	for (size_t i = anomalies.size() - addedAnomalies; i < anomalies.size(); i++) {
		addAssetFlag(anomalies[i].mText);
	}
	captureAssetFrameWithNote("anomaly", tContext, anomalies.back().mText);
	return 1;
}

static void addCharacterFightProgressFlags(const CharacterCaptureTracker& tTracker) {
	if (!tTracker.mHasLifeChanged) addAssetFlag("neither side lost life in the captured fight");
	for (int side = 0; side < 2; side++) {
		if (getCensusRootGetHitEntryAmount(side)) continue;
		addAssetFlag("player " + std::to_string(side + 1) + " never entered a gethit state");
	}
}

static void runCharacterCaptureLoop() {
	CharacterCaptureTracker tracker;
	resetCensus();
	advanceCaptureFightUntilRoundStarted();
	const auto iterations = getMediaCaptureCharacterIterations();
	while (gMediaCaptureData.mFrame < iterations) {
		advanceCaptureFightByOneFrame();

		const auto context = readCaptureContext();
		if (!captureCharacterAnomaliesForFrame(context)) {
			const auto reason = getCharacterCaptureReason(tracker, context);
			if (reason) captureAssetFrame(reason, context);
		}
		updateCharacterCaptureTracker(tracker, context);
	}
	addCharacterFightProgressFlags(tracker);
}

static std::string getCharacterDefinitionPath(const std::string& tCharacterName) {
	return getDolmexicaAssetFolder() + "chars/" + tCharacterName + "/" + tCharacterName + ".def";
}

static void startCharacterCaptureFight(const std::string& tCharacterName) {
	updateGameName(("Capturing " + tCharacterName).c_str());
	initForAutomatedFightScreenTest();
	setPlayerDefinitionPath(0, getCharacterDefinitionPath(tCharacterName).c_str());
	setPlayerDefinitionPath(1, getCharacterDefinitionPath(tCharacterName).c_str());
	setDreamStageMugenDefinition((getDolmexicaAssetFolder() + "stages/kfm.def").c_str(), "");
	setGameModeSuperWatch();
	initPrismWrapperScreenForDebug(getDreamFightScreenForTesting());
	setRandomSeed(getMediaCaptureRandomSeed());
}

static void captureCharacter(const std::string& tCharacterName) {
	logCurrentlyCapturedAsset(tCharacterName);
	beginAssetCapture("character", tCharacterName, std::string(MEDIA_CAPTURE_REPORT_FOLDER) + "/chars/" + tCharacterName);
	startCharacterCaptureFight(tCharacterName);
	runCharacterCaptureLoop();
	finishAssetCapture();
	unloadPrismWrapperScreenForDebug();
}

static void captureAllCharacters() {
	std::error_code ec;
	for (const auto& entry : std::filesystem::directory_iterator(getDolmexicaAssetFolder() + "chars", ec)) {
		if (!entry.is_directory()) continue;
		const auto characterName = entry.path().filename().string();
		if (!isAssetIncludedInTestRun(characterName)) continue;
		captureCharacter(characterName);
	}
}

TEST_F(MediaCaptureTest, CharacterMediaCapture) {
	captureAllCharacters();
}

// -------------------------------------------------------------------- stages

static float getStageCapturePlayerSeparation() {
	static const auto STAGE_CAPTURE_PLAYER_SEPARATION_SCREEN_FRACTION = 0.1f;
	return getDreamGameWidth(getDreamStageCoordinateP()) * STAGE_CAPTURE_PLAYER_SEPARATION_SCREEN_FRACTION;
}

static void pinPlayersToScreenCenterOffset(float tOffsetFromScreenCenterX) {
	const auto coordinateP = getDreamStageCoordinateP();
	const auto separation = getStageCapturePlayerSeparation();
	for (int side = 0; side < 2; side++) {
		DreamPlayer* root = getRootPlayer(side);
		if (!root) continue;
		const auto sideOffset = side ? (separation / 2) : (-separation / 2);
		setPlayerVelocityX(root, 0.0f, coordinateP);
		setPlayerVelocityY(root, 0.0f, coordinateP);
		setPlayerPositionBasedOnScreenCenterX(root, tOffsetFromScreenCenterX + sideOffset, coordinateP);
	}
}

static int hasCameraPositionChanged(float tPreviousPosition, float tCurrentPosition) {
	static const auto CAMERA_MOVEMENT_EPSILON = 0.01f;
	return std::fabs(tCurrentPosition - tPreviousPosition) > CAMERA_MOVEMENT_EPSILON;
}

// Walking the players into the tension zone only moves the camera as fast as the stage lets it, and on a wide stage it never arrives. The camera range is the stage's own bound, so the camera is placed on it directly.
static void putCameraOnStageEdge(int tIsRightEdge) {
	const auto cameraRange = getDreamMugenStageHandlerCameraRange();
	setDreamStageNoAutomaticCameraMovement();
	setDreamMugenStageHandlerCameraPositionX(tIsRightEdge ? cameraRange.mBottomRight.x : cameraRange.mTopLeft.x);
}

static void settlePlayersOnCurrentCameraPosition() {
	for (int frame = 0; frame < MEDIA_CAPTURE_STAGE_SETTLE_FRAMES; frame++) {
		pinPlayersToScreenCenterOffset(0.0f);
		advanceCaptureFightByOneFrame();
	}
	pinPlayersToScreenCenterOffset(0.0f);
}

static void captureStageAtHorizontalEdge(const char* tReason, int tIsRightEdge) {
	putCameraOnStageEdge(tIsRightEdge);
	settlePlayersOnCurrentCameraPosition();
	captureCurrentAssetFrame(tReason);
}

static void captureStageHorizontalEdges() {
	captureStageAtHorizontalEdge("stage_left", 0);
	captureStageAtHorizontalEdge("stage_right", 1);
	setDreamStageAutomaticCameraMovement();
}

static float getStageCaptureHighPositionY() {
	return (float)-getDreamGameHeight(getDreamStageCoordinateP());
}

static void pinPlayersToHighPosition() {
	const auto coordinateP = getDreamStageCoordinateP();
	for (int side = 0; side < 2; side++) {
		DreamPlayer* root = getRootPlayer(side);
		if (!root) continue;
		setPlayerVelocityY(root, 0.0f, coordinateP);
		setPlayerPositionY(root, getStageCaptureHighPositionY(), coordinateP);
	}
}

static void captureStageHighPositionIfCameraFollowsUpwards() {
	const auto coordinateP = getDreamStageCoordinateP();
	const auto groundCameraY = getDreamCameraPositionY(coordinateP);
	for (int frame = 0; frame < MEDIA_CAPTURE_STAGE_SETTLE_FRAMES; frame++) {
		pinPlayersToHighPosition();
		advanceCaptureFightByOneFrame();
	}
	pinPlayersToHighPosition();
	if (!hasCameraPositionChanged(groundCameraY, getDreamCameraPositionY(coordinateP))) return;

	captureCurrentAssetFrame("stage_high");
}

static void captureStageIdleFrames() {
	for (int idle = 0; idle < MEDIA_CAPTURE_STAGE_IDLE_AMOUNT; idle++) {
		advanceCaptureFightByFrames(MEDIA_CAPTURE_STAGE_IDLE_SPACING_FRAMES);
		captureCurrentAssetFrame("stage_idle");
	}
}

static void runStageCaptureSequence() {
	advanceCaptureFightUntilRoundStarted();
	captureCurrentAssetFrame("stage_start");
	captureStageIdleFrames();
	captureStageHorizontalEdges();
	captureStageHighPositionIfCameraFollowsUpwards();
}

static void startStageCaptureFight(const std::string& tStageDefPath) {
	updateGameName(("Capturing " + tStageDefPath).c_str());
	initForAutomatedFightScreenTest();
	setPlayerDefinitionPath(0, (getDolmexicaAssetFolder() + "chars/kfm/kfm.def").c_str());
	setPlayerDefinitionPath(1, (getDolmexicaAssetFolder() + "chars/kfm/kfm.def").c_str());
	setDreamStageMugenDefinition(tStageDefPath.c_str(), "");
	setGameModeTraining();
	initPrismWrapperScreenForDebug(getDreamFightScreenForTesting());
	setRandomSeed(getMediaCaptureRandomSeed());
}

static std::string makeStageCaptureOutputPath(const std::string& tStageDefPath) {
	auto relativePath = tStageDefPath;
	const auto stagesPrefix = getDolmexicaAssetFolder() + "stages/";
	if (relativePath.rfind(stagesPrefix, 0) == 0) relativePath = relativePath.substr(stagesPrefix.size());
	const auto extensionStart = relativePath.rfind('.');
	if (extensionStart != std::string::npos) relativePath = relativePath.substr(0, extensionStart);
	return std::string(MEDIA_CAPTURE_REPORT_FOLDER) + "/stages/" + relativePath;
}

static void captureStage(const std::string& tStageDefPath) {
	logCurrentlyCapturedAsset(tStageDefPath);
	beginAssetCapture("stage", tStageDefPath, makeStageCaptureOutputPath(tStageDefPath));
	startStageCaptureFight(tStageDefPath);
	runStageCaptureSequence();
	finishAssetCapture();
	unloadPrismWrapperScreenForDebug();
}

static void captureAllStages() {
	std::error_code ec;
	for (const auto& entry : std::filesystem::recursive_directory_iterator(getDolmexicaAssetFolder() + "stages/", ec)) {
		if (!entry.is_regular_file()) continue;
		if (entry.path().extension() != ".def") continue;
		auto stageDefPath = entry.path().generic_string();
		cleanPathSlashes(stageDefPath);
		if (!isAssetIncludedInTestRun(stageDefPath)) continue;
		captureStage(stageDefPath);
	}
}

TEST_F(MediaCaptureTest, StageMediaCapture) {
	captureAllStages();
}
