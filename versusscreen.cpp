#include "versusscreen.h"

#include <assert.h>

#include <prism/mugenanimationhandler.h>
#include <prism/mugendefreader.h>
#include <prism/screeneffect.h>
#include <prism/input.h>
#include <prism/timer.h>
#include <prism/mugentexthandler.h>
#include <prism/clipboardhandler.h>

#include "mugensound.h"
#include "scriptbackground.h"
#include "titlescreen.h"
#include "playerdefinition.h"
#include "config.h"

#define VERSUS_SCREEN_PLAYER_IMAGE_Z 50
#define VERSUS_SCREEN_PLAYER_TEXT_Z 51

#define VERSUS_SCREEN_MATCH_TEXT_Z 80

typedef struct {
	Position mPosition;
	int mIsFacingRight;
	Vector3D mScale;

	Position mNamePosition;
	Vector3DI mNameFont;

	MugenSpriteFile mSprites;
	char* mDisplayCharacterName;
	MugenAnimation* mAnimation;
	MugenAnimationHandlerElement* mAnimationElement;
	int mTextID;
} VersusPlayer;

typedef struct {
	int mTime;

	int mFadeInTime;
	int mFadeOutTime;

	VersusPlayer mPlayers[2];
} VersusHeader;

static struct {
	VersusHeader mHeader;

	MugenDefScript mScript;
	MugenSpriteFile mSprites;
	MugenAnimations mAnimations;

	int mHasMatchNumber;
	int mMatchNumber;
	int mMatchTextID;

	void(*mCB)();
} gVersusScreenData;

static void loadPlayerAnimationsAndName(int i) {
	VersusPlayer* player = &gVersusScreenData.mHeader.mPlayers[i];

	char file[200];
	char path[1024];
	char scriptPath[1024];
	char name[100];
	char palettePath[1024];

	getPlayerDefinitionPath(scriptPath, i);
	getPathToFile(path, scriptPath);

	MugenDefScript script;
	loadMugenDefScript(&script, scriptPath);

	int preferredPalette = 0;
	sprintf(name, "pal%d", preferredPalette + 1);
	getMugenDefStringOrDefault(file, &script, "files", name, "");
	int hasPalettePath = strcmp("", file);
	sprintf(palettePath, "%s%s", path, file);

	getMugenDefStringOrDefault(file, &script, "files", "sprite", "");
	assert(strcmp("", file));
	sprintf(scriptPath, "%s%s", path, file);
	player->mSprites = loadMugenSpriteFilePortraits(scriptPath, hasPalettePath, palettePath);
	player->mDisplayCharacterName = getAllocatedMugenDefStringVariable(&script, "info", "displayname");

	unloadMugenDefScript(&script);

	Position pos = player->mPosition;
	pos.z = VERSUS_SCREEN_PLAYER_IMAGE_Z;
	player->mAnimation = createOneFrameMugenAnimationForSprite(9000, 1);
	player->mAnimationElement = addMugenAnimation(player->mAnimation, &player->mSprites, pos);
	setMugenAnimationFaceDirection(player->mAnimationElement, player->mIsFacingRight);

	pos = player->mNamePosition;
	pos.z = VERSUS_SCREEN_PLAYER_TEXT_Z;
	player->mTextID = addMugenText(player->mDisplayCharacterName, pos, player->mNameFont.x);
	setMugenTextColor(player->mTextID, getMugenTextColorFromMugenTextColorIndex(player->mNameFont.y));
	setMugenTextAlignment(player->mTextID, getMugenTextAlignmentFromMugenAlignmentIndex(player->mNameFont.z));
}

static void loadVersusPlayer(int i) {
	VersusPlayer* player = &gVersusScreenData.mHeader.mPlayers[i];

	char playerName[100];
	sprintf(playerName, "p%d", i + 1);

	char fullVariableName[200];

	sprintf(fullVariableName, "%s.pos", playerName);
	player->mPosition = getMugenDefVectorOrDefault(&gVersusScreenData.mScript, "vs screen", fullVariableName, Vector3D(0, 0, 0));

	sprintf(fullVariableName, "%s.facing", playerName);
	int faceDirection = getMugenDefIntegerOrDefault(&gVersusScreenData.mScript, "vs screen", fullVariableName, 1);
	player->mIsFacingRight = faceDirection == 1;

	sprintf(fullVariableName, "%s.scale", playerName);
	player->mScale = getMugenDefVectorOrDefault(&gVersusScreenData.mScript, "vs screen", fullVariableName, Vector3D(1, 1, 1));

	sprintf(fullVariableName, "%s.name.pos", playerName);
	player->mNamePosition = getMugenDefVectorOrDefault(&gVersusScreenData.mScript, "vs screen", fullVariableName, Vector3D(0, 0, 0));

	sprintf(fullVariableName, "%s.name.font", playerName);
	player->mNameFont = getMugenDefVectorIOrDefault(&gVersusScreenData.mScript, "vs screen", fullVariableName, Vector3DI(-1, 0, 0));


	loadPlayerAnimationsAndName(i);
}

static void loadVersusHeader() {
	gVersusScreenData.mHeader.mTime = getMugenDefIntegerOrDefault(&gVersusScreenData.mScript, "vs screen", "time", 60);

	gVersusScreenData.mHeader.mFadeInTime = getMugenDefIntegerOrDefault(&gVersusScreenData.mScript, "vs screen", "fadein.time", 10);
	gVersusScreenData.mHeader.mFadeOutTime = getMugenDefIntegerOrDefault(&gVersusScreenData.mScript, "vs screen", "fadeout.time", 10);

	int i;
	for (i = 0; i < 2; i++) {
		loadVersusPlayer(i);
	}
}

static std::string parseMatchTextString(const std::string& tText) {
	std::stringstream ss;
	for (size_t i = 0; i < tText.size(); i++) {
		if (tText[i] == '%' && i != tText.size() - 1 && tText[i + 1] == 'i') {
			ss << gVersusScreenData.mMatchNumber;
			i++;
		}
		else {
			ss << tText[i];
		}
	}
	return ss.str();
}

static void loadMatchText() {
	if (!gVersusScreenData.mHasMatchNumber) return;

	const auto text = getSTLMugenDefStringOrDefault(&gVersusScreenData.mScript, "vs screen", "match.text", "");
	auto offset = getMugenDefVectorOrDefault(&gVersusScreenData.mScript, "vs screen", "match.offset", Vector3D(0, 0, 0));
	offset.z = VERSUS_SCREEN_MATCH_TEXT_Z;
	const auto font = getMugenDefVectorIOrDefault(&gVersusScreenData.mScript, "vs screen", "match.font", Vector3DI(-1, 0, 0));
	const auto parsedText = parseMatchTextString(text);

	gVersusScreenData.mMatchTextID = addMugenTextMugenStyle(parsedText.c_str(), offset, font);
}

static void loadVersusMusic() {
	char* path = getAllocatedMugenDefStringOrDefault(&gVersusScreenData.mScript, "music", "vs.bgm", " ");
	int isLooping = getMugenDefIntegerOrDefault(&gVersusScreenData.mScript, "music", "vs.bgm.loop", 1);

	if (isMugenBGMMusicPath(path)) {
		playMugenBGMMusicPath(path, isLooping);
	}

	freeMemory(path);
}

static void screenTimeFinishedCB(void* tCaller);

static void loadVersusScreen() {
	std::string folder;
	const auto motifPath = getDolmexicaAssetFolder() + getMotifPath();
	loadMugenDefScript(&gVersusScreenData.mScript, motifPath);
	gVersusScreenData.mAnimations = loadMugenAnimationFile(motifPath);
	getPathToFile(folder, motifPath.c_str());

	auto text = getSTLMugenDefStringVariable(&gVersusScreenData.mScript, "files", "spr");
	text = findMugenSystemOrFightFilePath(text, folder);
	gVersusScreenData.mSprites = loadMugenSpriteFileWithoutPalette(text);

	loadVersusHeader();
	loadScriptBackground(&gVersusScreenData.mScript, &gVersusScreenData.mSprites, &gVersusScreenData.mAnimations, "versusbgdef", "versusbg");
	loadMatchText();
	loadVersusMusic();

	addFadeIn(gVersusScreenData.mHeader.mFadeInTime, NULL, NULL);
	addTimerCB(gVersusScreenData.mHeader.mTime, screenTimeFinishedCB, NULL);
}

static void unloadVersusScreen() {
	unloadMugenDefScript(&gVersusScreenData.mScript);
	unloadMugenSpriteFile(&gVersusScreenData.mSprites);
	unloadMugenAnimationFile(&gVersusScreenData.mAnimations);

	int i;
	for (i = 0; i < 2; i++) {
		destroyMugenAnimation(gVersusScreenData.mHeader.mPlayers[i].mAnimation);
		unloadMugenSpriteFile(&gVersusScreenData.mHeader.mPlayers[i].mSprites);
		freeMemory(gVersusScreenData.mHeader.mPlayers[i].mDisplayCharacterName);
	}

	if (gVersusScreenData.mHasMatchNumber) {
		removeMugenText(gVersusScreenData.mMatchTextID);
	}
}

static void gotoNextScreenCB(void* /*tCaller*/) {
	gVersusScreenData.mCB();
}

static void screenTimeFinishedCB(void* /*tCaller*/) {
	addFadeOut(gVersusScreenData.mHeader.mFadeOutTime, gotoNextScreenCB, NULL);
}

static void updateVersusScreen() {
	if (hasPressedAFlank() || hasPressedStartFlank()) {
		screenTimeFinishedCB(NULL);
	}
}

static Screen gVersusScreen;

Screen* getVersusScreen() {
	gVersusScreen = makeScreen(loadVersusScreen, updateVersusScreen, NULL, unloadVersusScreen);
	return &gVersusScreen;
}

void setVersusScreenFinishedCB(void(*tCB)())
{
	gVersusScreenData.mCB = tCB;
}

void setVersusScreenMatchNumber(int tRound)
{
	gVersusScreenData.mMatchNumber = tRound;
	gVersusScreenData.mHasMatchNumber = 1;
}

void setVersusScreenNoMatchNumber()
{
	gVersusScreenData.mHasMatchNumber = 0;
}
