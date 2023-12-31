#include "victoryquotescreen.h"

#include <assert.h>

#include <prism/stlutil.h>
#include <prism/math.h>
#include <prism/mugendefreader.h>
#include <prism/screeneffect.h>
#include <prism/timer.h>
#include <prism/system.h>
#include <prism/mugentexthandler.h>
#include <prism/input.h>
#include <prism/geometry.h>

#include "config.h"
#include "playerdefinition.h"
#include "gamelogic.h"
#include "scriptbackground.h"

#define VICTORY_QUOTE_SCREEN_PLAYER_IMAGE_Z 50
#define VICTORY_QUOTE_SCREEN_PLAYER_TEXT_Z 51

#define VICTORY_QUOTE_SCREEN_QUOTE_TEXT_Z 80

typedef struct {
	Vector3D mOffset;
	Vector3DI mSprite;
	int mIsFacingRight;
	Vector2D mScale;
	GeoRectangle2D mWindow;

	Vector3D mNameOffset;
	Vector3DI mNameFont;
} VictoryQuoteScreenHeaderPlayerData;

typedef struct {
	std::string mText;
	Vector3D mOffset;
	Vector3DI mFont;
	GeoRectangle2D mWindow;
	int mIsTextWrapping;
} VictoryQuoteScreenHeaderWinQuote;

typedef struct {
	int mTime;
	int mFadeInTime;
	int mFadeOutTime;
	VictoryQuoteScreenHeaderPlayerData mPlayer;
	VictoryQuoteScreenHeaderWinQuote mWinQuote;
} VictoryQuoteScreenHeader;

typedef struct {
	MugenSpriteFile mSprites;
	MugenAnimation* mAnimation;

	MugenAnimationHandlerElement* mPortraitElement;
	int mNameTextID;
} VictoryQuoteScreenPlayer;

typedef struct {
	int mTextID;
} VictoryQuoteScreenWinQuote;

static struct {
	int mVictoryQuoteIndex;

	MugenSpriteFile mSprites;
	MugenAnimations mAnimations;
	VictoryQuoteScreenHeader mHeader;

	VictoryQuoteScreenPlayer mPlayer;
	VictoryQuoteScreenWinQuote mWinQuote;

	void(*mCB)();
} gVictoryQuoteScreenData;

static void loadVictoryQuoteScreenHeader(MugenDefScript* tSystemScript) {
	const auto sz = getScreenSize();

	gVictoryQuoteScreenData.mHeader.mTime = getMugenDefIntegerOrDefault(tSystemScript, "victory screen", "time", 300);
	gVictoryQuoteScreenData.mHeader.mFadeInTime = getMugenDefIntegerOrDefault(tSystemScript, "victory screen", "fadein.time", 8);
	gVictoryQuoteScreenData.mHeader.mFadeOutTime = getMugenDefIntegerOrDefault(tSystemScript, "victory screen", "fadeout.time", 15);

	gVictoryQuoteScreenData.mHeader.mPlayer.mOffset = getMugenDefVectorOrDefault(tSystemScript, "victory screen", "p1.offset", Vector3D(100, 20, 0));
	gVictoryQuoteScreenData.mHeader.mPlayer.mSprite = getMugenDefVectorIOrDefault(tSystemScript, "victory screen", "p1.spr", Vector3DI(9000, 2, 0));
	gVictoryQuoteScreenData.mHeader.mPlayer.mIsFacingRight = getMugenDefIntegerOrDefault(tSystemScript, "victory screen", "p1.facing", 1) == 1;
	gVictoryQuoteScreenData.mHeader.mPlayer.mScale = getMugenDefVector2DOrDefault(tSystemScript, "victory screen", "p1.scale", Vector2D(1, 1));
	gVictoryQuoteScreenData.mHeader.mPlayer.mWindow = getMugenDefGeoRectangle2DOrDefault(tSystemScript, "victory screen", "p1.window", GeoRectangle2D(0, 0, sz.x - 1, sz.y - 1));

	gVictoryQuoteScreenData.mHeader.mPlayer.mNameOffset = getMugenDefVectorOrDefault(tSystemScript, "victory screen", "p1.name.offset", Vector3D(20, 180, 0));
	gVictoryQuoteScreenData.mHeader.mPlayer.mNameFont = getMugenDefVectorIOrDefault(tSystemScript, "victory screen", "p1.name.font", Vector3DI(-1, 0, 1));

	gVictoryQuoteScreenData.mHeader.mWinQuote.mText = getSTLMugenDefStringOrDefault(tSystemScript, "victory screen", "winquote.text", "Winner!");
	gVictoryQuoteScreenData.mHeader.mWinQuote.mOffset = getMugenDefVectorOrDefault(tSystemScript, "victory screen", "winquote.offset", Vector3D(20, 192, 0));
	gVictoryQuoteScreenData.mHeader.mWinQuote.mFont = getMugenDefVectorIOrDefault(tSystemScript, "victory screen", "winquote.font", Vector3DI(-1, 0, 1));
	gVictoryQuoteScreenData.mHeader.mWinQuote.mWindow = getMugenDefGeoRectangle2DOrDefault(tSystemScript, "victory screen", "winquote.window", GeoRectangle2D(0, 0, sz.x - 1, sz.y - 1));
	const auto textWrapText = getSTLMugenDefStringOrDefault(tSystemScript, "victory screen", "winquote.textwrap", "w");
	gVictoryQuoteScreenData.mHeader.mWinQuote.mIsTextWrapping = textWrapText.find('w') != std::string::npos;
}

static int loadVictoryQuoteWithIndexAndReturnIfSuccessful(MugenDefScript* tScript, int tIndex, std::string& oQuote) {
	const auto s = std::string("victory").append(std::to_string(tIndex));
	if (!isMugenDefStringVariable(tScript, "quotes", s.c_str())) {
		return 0;
	}
	oQuote = getSTLMugenDefStringVariable(tScript, "quotes", s.c_str());
	return 1;
}

static int loadRandomQuoteAndReturnIfSuccessful(MugenDefScript* tScript, std::string& oQuote) {
	std::vector<std::string> quotes;
	for (int i = 0; i <= 99; i++) {
		const auto s = std::string("victory").append(std::to_string(i));
		if (!isMugenDefStringVariable(tScript, "quotes", s.c_str())) {
			if (i >= 1) break;
			else continue;
		}
		quotes.push_back(getSTLMugenDefStringVariable(tScript, "quotes", s.c_str()));
	}
	if (!quotes.size()) {
		return 0;
	}

	oQuote = quotes[randfromInteger(0, int(quotes.size()) - 1)];
	return 1;
}

static std::string loadVictoryQuote(MugenDefScript* tScript) {
	std::string quote;
	if (gVictoryQuoteScreenData.mVictoryQuoteIndex == -1 || !loadVictoryQuoteWithIndexAndReturnIfSuccessful(tScript, gVictoryQuoteScreenData.mVictoryQuoteIndex, quote)) {
		const auto result = loadRandomQuoteAndReturnIfSuccessful(tScript, quote);
		if (!result) {
			quote = gVictoryQuoteScreenData.mHeader.mWinQuote.mText;
		}
	}
	return quote;
}

static void gotoNextScreenCB(void* /*tCaller*/) {
	if (gVictoryQuoteScreenData.mCB) {
		gVictoryQuoteScreenData.mCB();
	}
}

static void victoryQuoteScreenTimeFinishedCB(void* /*tCaller*/) {
	addFadeOut(gVictoryQuoteScreenData.mHeader.mFadeOutTime, gotoNextScreenCB, NULL);
}

static void loadPlayerAnimationsAndName(MugenDefScript* tPlayerScript, const char* tPath) {
	char file[1024];
	char scriptPath[1024];
	char name[1024];
	char palettePath[1024];

	int preferredPalette = 0;
	sprintf(name, "pal%d", preferredPalette + 1);
	getMugenDefStringOrDefault(file, tPlayerScript, "files", name, "");
	int hasPalettePath = strcmp("", file);
	sprintf(palettePath, "%s%s", tPath, file);

	getMugenDefStringOrDefault(file, tPlayerScript, "files", "sprite", "");
	assert(strcmp("", file));
	sprintf(scriptPath, "%s%s", tPath, file);
	gVictoryQuoteScreenData.mPlayer.mSprites = loadMugenSpriteFilePortraits(scriptPath, hasPalettePath, palettePath);
	const auto playerName = getSTLMugenDefStringVariable(tPlayerScript, "info", "displayname");

	auto pos = gVictoryQuoteScreenData.mHeader.mPlayer.mOffset;
	pos.z = VICTORY_QUOTE_SCREEN_PLAYER_IMAGE_Z;
	if (hasMugenSprite(&gVictoryQuoteScreenData.mPlayer.mSprites, gVictoryQuoteScreenData.mHeader.mPlayer.mSprite.x, gVictoryQuoteScreenData.mHeader.mPlayer.mSprite.y)) {
		gVictoryQuoteScreenData.mPlayer.mAnimation = createOneFrameMugenAnimationForSprite(gVictoryQuoteScreenData.mHeader.mPlayer.mSprite.x, gVictoryQuoteScreenData.mHeader.mPlayer.mSprite.y);
	}
	else {
		gVictoryQuoteScreenData.mPlayer.mAnimation = createOneFrameMugenAnimationForSprite(9000, 1);
	}
	gVictoryQuoteScreenData.mPlayer.mPortraitElement = addMugenAnimation(gVictoryQuoteScreenData.mPlayer.mAnimation, &gVictoryQuoteScreenData.mPlayer.mSprites, pos);
	setMugenAnimationFaceDirection(gVictoryQuoteScreenData.mPlayer.mPortraitElement, gVictoryQuoteScreenData.mHeader.mPlayer.mIsFacingRight);
	setMugenAnimationDrawScale(gVictoryQuoteScreenData.mPlayer.mPortraitElement, gVictoryQuoteScreenData.mHeader.mPlayer.mScale);
	setMugenAnimationConstraintRectangle(gVictoryQuoteScreenData.mPlayer.mPortraitElement, gVictoryQuoteScreenData.mHeader.mPlayer.mWindow);

	pos = gVictoryQuoteScreenData.mHeader.mPlayer.mNameOffset;
	pos.z = VICTORY_QUOTE_SCREEN_PLAYER_TEXT_Z;
	gVictoryQuoteScreenData.mPlayer.mNameTextID = addMugenTextMugenStyle(playerName.c_str(), pos, gVictoryQuoteScreenData.mHeader.mPlayer.mNameFont);
}

static void loadWinQuote(const std::string& tQuote) {
	auto pos = gVictoryQuoteScreenData.mHeader.mWinQuote.mOffset;
	pos.z = VICTORY_QUOTE_SCREEN_QUOTE_TEXT_Z;
	gVictoryQuoteScreenData.mWinQuote.mTextID = addMugenTextMugenStyle(tQuote.c_str(), pos, gVictoryQuoteScreenData.mHeader.mWinQuote.mFont);
	setMugenTextRectangle(gVictoryQuoteScreenData.mWinQuote.mTextID, gVictoryQuoteScreenData.mHeader.mWinQuote.mWindow);
	if (gVictoryQuoteScreenData.mHeader.mWinQuote.mIsTextWrapping) {
		auto width = gVictoryQuoteScreenData.mHeader.mWinQuote.mWindow.mBottomRight.x - gVictoryQuoteScreenData.mHeader.mWinQuote.mOffset.x;
		if (width <= 0) {
			width = INF;
		}
		setMugenTextTextBoxWidth(gVictoryQuoteScreenData.mWinQuote.mTextID, width);
	}
}

static void loadVictoryQuoteScreen() {
	char scriptPath[1024], path[1024], file[1024];
	const auto motifPath = getDolmexicaAssetFolder() + getMotifPath();
	MugenDefScript script;
	loadMugenDefScript(&script, motifPath);
	gVictoryQuoteScreenData.mAnimations = loadMugenAnimationFile(motifPath);
	getPathToFile(path, motifPath.c_str());

	auto text = getSTLMugenDefStringVariable(&script, "files", "spr");
	text = findMugenSystemOrFightFilePath(text, path);
	gVictoryQuoteScreenData.mSprites = loadMugenSpriteFileWithoutPalette(text);

	loadVictoryQuoteScreenHeader(&script);
	loadScriptBackground(&script, &gVictoryQuoteScreenData.mSprites, &gVictoryQuoteScreenData.mAnimations, "victorybgdef", "victorybg");

	unloadMugenDefScript(&script);

	getPlayerDefinitionPath(scriptPath, getDreamMatchWinnerIndex());
	getPathToFile(path, scriptPath);
	loadMugenDefScript(&script, scriptPath);
	loadPlayerAnimationsAndName(&script, path);

	getMugenDefStringOrDefault(file, &script, "files", "cns", "");
	assert(strcmp("", file));
	sprintf(scriptPath, "%s%s", path, file);
	unloadMugenDefScript(&script);

	loadMugenDefScript(&script, scriptPath);
	const auto quote = loadVictoryQuote(&script);
	unloadMugenDefScript(&script);
	loadWinQuote(quote);

	addFadeIn(gVictoryQuoteScreenData.mHeader.mFadeInTime, NULL, NULL);
	addTimerCB(gVictoryQuoteScreenData.mHeader.mTime, victoryQuoteScreenTimeFinishedCB, NULL);
}

static void updateVictoryQuoteScreen() {
	if (hasPressedAFlank() || hasPressedStartFlank()) {
		victoryQuoteScreenTimeFinishedCB(NULL);
	}
}

Screen gVictoryQuoteScreenScreen;

Screen* getVictoryQuoteScreen()
{
	gVictoryQuoteScreenScreen = makeScreen(loadVictoryQuoteScreen, updateVictoryQuoteScreen);
	return &gVictoryQuoteScreenScreen;
}

void setVictoryQuoteScreenQuoteIndex(int tIndex)
{
	gVictoryQuoteScreenData.mVictoryQuoteIndex = tIndex;
}

void setVictoryQuoteScreenFinishedCB(void(*tCB)())
{
	gVictoryQuoteScreenData.mCB = tCB;
}