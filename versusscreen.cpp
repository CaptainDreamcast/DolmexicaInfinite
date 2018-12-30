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
#include "menubackground.h"
#include "titlescreen.h"
#include "playerdefinition.h"

typedef struct {
	Position mPosition;
	int mIsFacingRight;
	Vector3D mScale;

	Position mNamePosition;
	Vector3DI mNameFont;

	MugenSpriteFile mSprites;
	char* mDisplayCharacterName;
	MugenAnimation* mAnimation;
	int mAnimationID;
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
	getMugenDefStringOrDefault(file, &script, "Files", name, "");
	int hasPalettePath = strcmp("", file);
	sprintf(palettePath, "%s%s", path, file);

	getMugenDefStringOrDefault(file, &script, "Files", "sprite", "");
	assert(strcmp("", file));
	sprintf(scriptPath, "%s%s", path, file);
	player->mSprites = loadMugenSpriteFilePortraits(scriptPath, preferredPalette, hasPalettePath, palettePath);
	player->mDisplayCharacterName = getAllocatedMugenDefStringVariable(&script, "Info", "displayname");

	unloadMugenDefScript(script);

	Position pos = player->mPosition;
	pos.z = 50;
	player->mAnimation = createOneFrameMugenAnimationForSprite(9000, 1);
	player->mAnimationID = addMugenAnimation(player->mAnimation, &player->mSprites, pos);
	setMugenAnimationFaceDirection(player->mAnimationID, player->mIsFacingRight);

	pos = player->mNamePosition;
	pos.z = 51;
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
	player->mPosition = getMugenDefVectorOrDefault(&gVersusScreenData.mScript, "VS Screen", fullVariableName, makePosition(0, 0, 0));

	sprintf(fullVariableName, "%s.facing", playerName);
	int faceDirection = getMugenDefIntegerOrDefault(&gVersusScreenData.mScript, "VS Screen", fullVariableName, 1);
	player->mIsFacingRight = faceDirection == 1;

	sprintf(fullVariableName, "%s.scale", playerName);
	player->mScale = getMugenDefVectorOrDefault(&gVersusScreenData.mScript, "VS Screen", fullVariableName, makePosition(1, 1, 1));

	sprintf(fullVariableName, "%s.name.pos", playerName);
	player->mNamePosition = getMugenDefVectorOrDefault(&gVersusScreenData.mScript, "VS Screen", fullVariableName, makePosition(0, 0, 0));

	sprintf(fullVariableName, "%s.name.font", playerName);
	player->mNameFont = getMugenDefVectorIOrDefault(&gVersusScreenData.mScript, "VS Screen", fullVariableName, makeVector3DI(1, 0, 0));


	loadPlayerAnimationsAndName(i);
}

static void loadVersusHeader() {
	gVersusScreenData.mHeader.mTime = getMugenDefIntegerOrDefault(&gVersusScreenData.mScript, "VS Screen", "time", 60);

	gVersusScreenData.mHeader.mFadeInTime = getMugenDefIntegerOrDefault(&gVersusScreenData.mScript, "VS Screen", "fadein.time", 10);
	gVersusScreenData.mHeader.mFadeOutTime = getMugenDefIntegerOrDefault(&gVersusScreenData.mScript, "VS Screen", "fadeout.time", 10);

	int i;
	for (i = 0; i < 2; i++) {
		loadVersusPlayer(i);
	}
}

static void loadVersusMusic() {
	char* path = getAllocatedMugenDefStringOrDefault(&gVersusScreenData.mScript, "Music", "vs.bgm", " ");
	int isLooping = getMugenDefIntegerOrDefault(&gVersusScreenData.mScript, "Music", "vs.bgm.loop", 1);

	if (isMugenBGMMusicPath(path)) {
		playMugenBGMMusicPath(path, isLooping);
	}

	freeMemory(path);
}

static void screenTimeFinishedCB(void* tCaller);

static void loadVersusScreen() {

	// TODO: properly
	char folder[1024];
	loadMugenDefScript(&gVersusScreenData.mScript, "assets/data/system.def");
	gVersusScreenData.mAnimations = loadMugenAnimationFile("assets/data/system.def");
	getPathToFile(folder, "assets/data/system.def");
	setWorkingDirectory(folder);

	char* text = getAllocatedMugenDefStringVariable(&gVersusScreenData.mScript, "Files", "spr");
	gVersusScreenData.mSprites = loadMugenSpriteFileWithoutPalette(text);
	freeMemory(text);

	setWorkingDirectory("/");

	loadVersusHeader();
	loadMenuBackground(&gVersusScreenData.mScript, &gVersusScreenData.mSprites, &gVersusScreenData.mAnimations, "VersusBGdef", "VersusBG");
	loadVersusMusic();

	addFadeIn(gVersusScreenData.mHeader.mFadeInTime, NULL, NULL);
	addTimerCB(gVersusScreenData.mHeader.mTime, screenTimeFinishedCB, NULL);
}

static void unloadVersusScreen() {
	unloadMugenDefScript(gVersusScreenData.mScript);
	unloadMugenSpriteFile(&gVersusScreenData.mSprites);
	unloadMugenAnimationFile(&gVersusScreenData.mAnimations);

	int i;
	for (i = 0; i < 2; i++) {
		destroyMugenAnimation(gVersusScreenData.mHeader.mPlayers[i].mAnimation);
		unloadMugenSpriteFile(&gVersusScreenData.mHeader.mPlayers[i].mSprites);
		freeMemory(gVersusScreenData.mHeader.mPlayers[i].mDisplayCharacterName);
	}
}

static void gotoNextScreenCB(void* tCaller) {
	gVersusScreenData.mCB();
}

static void screenTimeFinishedCB(void* tCaller) {
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
