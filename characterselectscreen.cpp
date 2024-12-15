#include "characterselectscreen.h"

#include <assert.h>
#include <algorithm>
#include <string>

#include <prism/mugendefreader.h>
#include <prism/mugenanimationhandler.h>
#include <prism/screeneffect.h>
#include <prism/input.h>
#include <prism/mugenanimationhandler.h>
#include <prism/mugensoundfilereader.h>
#include <prism/mugentexthandler.h>
#include <prism/clipboardhandler.h>
#include <prism/log.h>
#include <prism/math.h>

#include "mugensound.h"
#include "scriptbackground.h"
#include "titlescreen.h"
#include "storyscreen.h"
#include "playerdefinition.h"
#include "gamelogic.h"
#include "stage.h"
#include "storymode.h"
#include "config.h"
#include "mugenassignmentevaluator.h"

#define SELECTOR_Z 49

using namespace std;

typedef struct {
	Vector2DI mCursorStartCell;
	int mIsActiveCursorAnimationOwned;
	MugenAnimation* mActiveCursorAnimation;
	Vector2D mActiveCursorScale;
	int mIsDoneCursorAnimationOwned;
	MugenAnimation* mDoneCursorAnimation;
	Vector2D mDoneCursorScale;

	MugenAnimation* mBigPortraitAnimation;
	Position2D mBigPortraitOffset;
	Vector2D mBigPortraitScale;
	int mBigPortraitIsFacingRight;

	Position2D mNameOffset;
	Vector3DI mNameFont;
	double mNameWidth;

	Vector2DI mCursorMoveSound;
	Vector2DI mCursorDoneSound;
	Vector2DI mRandomMoveSound;
} PlayerHeader;

typedef struct {
	Position2D mPosition;
	Vector3DI mActiveFont1;
	Vector3DI mActiveFont2;
	Vector3DI mDoneFont;
} StageSelectHeader;

typedef struct {
	int mFadeInTime;
	int mFadeOutTime;

	int mRows;
	int mColumns;
	int mIsWrapping;

	Position2D mPosition;
	int mIsShowingEmptyBoxes;
	int mCanMoveOverEmptyBoxes;
	int mIsShowingPortraits;
	int mIsSkippingCharScriptLoading;

	Vector2D mCellSize;
	double mCellSpacing;
	
	int mIsCellBackgroundAnimationOwned;
	MugenAnimation* mCellBackgroundAnimation;
	Vector2D mCellBackgroundScale;
	int mIsRandomSelectionAnimationOwned;
	MugenAnimation* mRandomSelectionAnimation;
	Vector2D mRandomSelectionScale;
	int mRandomPortraitSwitchTime;

	PlayerHeader mPlayers[2];

	MugenAnimation* mSmallPortraitAnimation;
	Position2D mSmallPortraitOffset;
	Vector2D mSmallPortraitScale;

	Position2D mTitleOffset;
	Vector3DI mTitleFont;

	StageSelectHeader mStageSelect;

} SelectScreenHeader;

typedef struct {
	char* mAuthorName;
	char* mName;
	char* mVersionDate;
} SelectCharacterCredits;

typedef enum {
	SELECT_CHARACTER_TYPE_EMPTY,
	SELECT_CHARACTER_TYPE_CHARACTER,
	SELECT_CHARACTER_TYPE_RANDOM

} SelectCharacterType;

typedef struct {
	SelectCharacterType mType;
	
	Vector2DI mCellPosition;
	MugenAnimationHandlerElement* mBackgroundAnimationElement;

	MugenSpriteFile mSprites;
	MugenAnimationHandlerElement* mPortraitAnimationElement;
	char mStageName[1024];
	char* mDisplayCharacterName;
	char mCharacterName[200];

	SelectCharacterCredits mCredits;
} SelectCharacter;

typedef struct {
	char* mAuthorName;
	char* mName;
	char* mVersionDate;
} SelectStageCredits;

typedef struct {
	char mPath[1024];
	char* mName;

	SelectStageCredits mCredits;
} SelectStage;

typedef struct {
	int mNow;
	int mCurrentCharacter;

} RandomSelector;

typedef struct {
	int mIsActive;
	int mIsDone;
	int mOwner;

	MugenAnimationHandlerElement* mSelectorAnimationElement;
	MugenAnimationHandlerElement* mBigPortraitAnimationElement;
	int mNameTextID;

	int mHasBeenLoadedBefore;
	Vector2DI mSelectedCharacter;
	RandomSelector mRandom;
} Selector;

typedef struct {
	int mModeTextID;

} ModeText;

typedef struct {
	int mIsUsing;
	int mIsInOsuMode;
	int mIsActive;
	int mIsDone;


	int mTextID;
	int mActiveFontDisplayed;
	int mSelectedStage;
} StageSelectData;

typedef enum {
	CHARACTER_SELECT_SCREEN_TYPE_ONE_PLAYER_MODE,
	CHARACTER_SELECT_SCREEN_TYPE_TWO_PLAYER_MODE,
	CHARACTER_SELECT_SCREEN_TYPE_ONE_PLAYER_SELECTS_EVERYTHING,
	CHARACTER_SELECT_SCREEN_TYPE_CREDITS,
	CHARACTER_SELECT_SCREEN_TYPE_STORY,

} CharacterSelectScreenType;

typedef struct {

	Position mNamePosition;
	Vector3DI mNameFont;
	int mNameID;

	Position mAuthorNamePosition;
	Vector3DI mAuthorNameFont;
	int mAuthorNameID;

	Position mVersionDatePosition;
	Vector3DI mVersionDateFont;
	int mVersionDateID;

} SelectCredits;

static struct {
	string mCustomSelectFilePath;
	MugenDefScript mScript;
	MugenDefScript mCharacterScript;
	MugenSpriteFile mSprites;
	MugenSounds mSounds;
	MugenAnimations mAnimations;

	Vector2DI mLocalCoord;

	SelectScreenHeader mHeader;
	ModeText mModeText;

	TextureData mWhiteTexture;
	char mModeName[200];
	void(*mCB)();

	int mCharacterAmount;
	std::list<std::list<SelectCharacter>> mSelectCharacters;
	Vector mSelectStages; // vector of SelectStage
	std::vector<SelectCharacter*> mRealSelectCharacters;
	int mSelectorAmount;
	Selector mSelectors[2];
	int mControllerUsed[2];

	StageSelectData mStageSelect;
	SelectCredits mCredits;

	CharacterSelectScreenType mSelectScreenType;

	int mCanSelectThisFrame;
	int mIsReturnDisabled;
	int mIsFadingOut;
} gCharacterSelectScreenData;

static MugenAnimation* getMugenDefMenuAnimationOrSprite(MugenDefScript* tScript, const char* tGroupName, const char* tVariableName, int& oIsOwned) {
	char fullVariableName[200];
	sprintf(fullVariableName, "%s.anim", tVariableName);

	if (isMugenDefNumberVariable(tScript, tGroupName, fullVariableName)) {
		oIsOwned = 0;
		return getMugenAnimation(&gCharacterSelectScreenData.mAnimations, getMugenDefNumberVariable(tScript, tGroupName, fullVariableName));
	}

	sprintf(fullVariableName, "%s.spr", tVariableName);
	const auto ret = getMugenDefVector2DIOrDefault(tScript, tGroupName, fullVariableName, Vector2DI(-1, -1));
	oIsOwned = 1;
	return createOneFrameMugenAnimationForSprite(ret.x, ret.y);
}

static void loadMenuPlayerHeader(const char* tPlayerName, PlayerHeader* tHeader) {
	char fullVariableName[200];

	sprintf(fullVariableName, "%s.cursor.startcell", tPlayerName);
	tHeader->mCursorStartCell = getMugenDefVector2DIOrDefault(&gCharacterSelectScreenData.mScript, "select info", fullVariableName, Vector2DI(0, 0));

	sprintf(fullVariableName, "%s.cursor.active", tPlayerName);
	tHeader->mActiveCursorAnimation = getMugenDefMenuAnimationOrSprite(&gCharacterSelectScreenData.mScript, "select info", fullVariableName, tHeader->mIsActiveCursorAnimationOwned);

	sprintf(fullVariableName, "%s.cursor.active.scale", tPlayerName);
	tHeader->mActiveCursorScale = getMugenDefVector2DOrDefault(&gCharacterSelectScreenData.mScript, "select info", fullVariableName, Vector2D(1.0, 1.0));
	tHeader->mActiveCursorScale = transformDreamCoordinatesVector2D(tHeader->mActiveCursorScale, gCharacterSelectScreenData.mLocalCoord.x, getScreenSize().x);

	sprintf(fullVariableName, "%s.cursor.done", tPlayerName);
	tHeader->mDoneCursorAnimation = getMugenDefMenuAnimationOrSprite(&gCharacterSelectScreenData.mScript, "select info", fullVariableName, tHeader->mIsDoneCursorAnimationOwned);

	sprintf(fullVariableName, "%s.cursor.done.scale", tPlayerName);
	tHeader->mDoneCursorScale = getMugenDefVector2DOrDefault(&gCharacterSelectScreenData.mScript, "select info", fullVariableName, Vector2D(1.0, 1.0));
	tHeader->mDoneCursorScale = transformDreamCoordinatesVector2D(tHeader->mDoneCursorScale, gCharacterSelectScreenData.mLocalCoord.x, getScreenSize().x);

	sprintf(fullVariableName, "%s.face.spr", tPlayerName);
	const auto bigPortraitSprite = getMugenDefVector2DIOrDefault(&gCharacterSelectScreenData.mScript, "select info", fullVariableName, Vector2DI(9000, 1));
	tHeader->mBigPortraitAnimation = createOneFrameMugenAnimationForSprite(bigPortraitSprite.x, bigPortraitSprite.y);

	sprintf(fullVariableName, "%s.face.offset", tPlayerName);
	tHeader->mBigPortraitOffset = getMugenDefVector2DOrDefault(&gCharacterSelectScreenData.mScript, "select info", fullVariableName, Vector2D(0, 0));
	tHeader->mBigPortraitOffset = transformDreamCoordinatesVector2D(tHeader->mBigPortraitOffset, gCharacterSelectScreenData.mLocalCoord.x, getScreenSize().x);

	sprintf(fullVariableName, "%s.face.scale", tPlayerName);
	tHeader->mBigPortraitScale = getMugenDefVector2DOrDefault(&gCharacterSelectScreenData.mScript, "select info", fullVariableName, Vector2D(0, 0));

	sprintf(fullVariableName, "%s.face.facing", tPlayerName);
	const auto faceDirection = getMugenDefIntegerOrDefault(&gCharacterSelectScreenData.mScript, "select info", fullVariableName, 1);
	tHeader->mBigPortraitIsFacingRight = faceDirection == 1;

	sprintf(fullVariableName, "%s.name.offset", tPlayerName);
	tHeader->mNameOffset = getMugenDefVector2DOrDefault(&gCharacterSelectScreenData.mScript, "select info", fullVariableName, Vector2D(0, 0));
	tHeader->mNameOffset = transformDreamCoordinatesVector2D(tHeader->mNameOffset, gCharacterSelectScreenData.mLocalCoord.x, getScreenSize().x);

	sprintf(fullVariableName, "%s.name.font", tPlayerName);
	tHeader->mNameFont = getMugenDefVectorIOrDefault(&gCharacterSelectScreenData.mScript, "select info", fullVariableName, Vector3DI(-1, 0, 0));

	sprintf(fullVariableName, "%s.name.width", tPlayerName);
	tHeader->mNameWidth = getMugenDefFloatOrDefault(&gCharacterSelectScreenData.mScript, "select info", fullVariableName, INF);
	tHeader->mNameWidth = transformDreamCoordinates(tHeader->mNameWidth, gCharacterSelectScreenData.mLocalCoord.x, getScreenSize().x);

	sprintf(fullVariableName, "%s.cursor.move.snd", tPlayerName);
	tHeader->mCursorMoveSound = getMugenDefVector2DIOrDefault(&gCharacterSelectScreenData.mScript, "select info", fullVariableName, Vector2DI(0, 0));

	sprintf(fullVariableName, "%s.cursor.done.snd", tPlayerName);
	tHeader->mCursorDoneSound = getMugenDefVector2DIOrDefault(&gCharacterSelectScreenData.mScript, "select info", fullVariableName, Vector2DI(0, 0));

	sprintf(fullVariableName, "%s.random.move.snd", tPlayerName);
	tHeader->mRandomMoveSound = getMugenDefVector2DIOrDefault(&gCharacterSelectScreenData.mScript, "select info", fullVariableName, Vector2DI(0, 0));
}

static void loadStageSelectHeader() {
	gCharacterSelectScreenData.mHeader.mStageSelect.mPosition = getMugenDefVector2DOrDefault(&gCharacterSelectScreenData.mScript, "select info", "stage.pos", Vector2D(0, 0));
	gCharacterSelectScreenData.mHeader.mStageSelect.mPosition = transformDreamCoordinatesVector2D(gCharacterSelectScreenData.mHeader.mStageSelect.mPosition, gCharacterSelectScreenData.mLocalCoord.x, getScreenSize().x);
	gCharacterSelectScreenData.mHeader.mStageSelect.mActiveFont1 = getMugenDefVectorIOrDefault(&gCharacterSelectScreenData.mScript, "select info", "stage.active.font", Vector3DI(-1, 0, 0));
	gCharacterSelectScreenData.mHeader.mStageSelect.mActiveFont2 = getMugenDefVectorIOrDefault(&gCharacterSelectScreenData.mScript, "select info", "stage.active2.font", gCharacterSelectScreenData.mHeader.mStageSelect.mActiveFont1);
	gCharacterSelectScreenData.mHeader.mStageSelect.mDoneFont = getMugenDefVectorIOrDefault(&gCharacterSelectScreenData.mScript, "select info", "stage.done.font", Vector3DI(-1, 0, 0));
}

static void loadStageSelect() {
	const auto pos = gCharacterSelectScreenData.mHeader.mStageSelect.mPosition.xyz(40.0);
	gCharacterSelectScreenData.mStageSelect.mTextID = addMugenText(" ", pos, gCharacterSelectScreenData.mHeader.mStageSelect.mActiveFont1.x);

	gCharacterSelectScreenData.mStageSelect.mIsActive = 0;
}

static void loadSelectStageCredits(SelectStage* e, MugenDefScript* tScript) {
	if (gCharacterSelectScreenData.mSelectScreenType != CHARACTER_SELECT_SCREEN_TYPE_CREDITS) return;

	e->mCredits.mName = getAllocatedMugenDefStringOrDefault(tScript, "info", "name", e->mName);
	e->mCredits.mAuthorName = getAllocatedMugenDefStringOrDefault(tScript, "info", "author", "N/A");
	e->mCredits.mVersionDate = getAllocatedMugenDefStringOrDefault(tScript, "info", "versiondate", "N/A");
}

static int isSelectStageLoadedAlready(char* tPath) {
	int i;
	for (i = 0; i < vector_size(&gCharacterSelectScreenData.mSelectStages); i++) {
		SelectStage* e = (SelectStage*)vector_get(&gCharacterSelectScreenData.mSelectStages, i);
		if (!strcmp(tPath, e->mPath)) return 1;
	}

	return 0;
}

static void getStagePath(char* tDst, const char* tPath) {
	sprintf(tDst, "%s%s", getDolmexicaAssetFolder().c_str(), tPath);
}

static int isInOsuModeAndNotOsuStage(MugenDefScript* tScript) {
	if (!gCharacterSelectScreenData.mStageSelect.mIsInOsuMode) return 0;
	const std::string fileName = getSTLMugenDefStringOrDefault(tScript, "music", "bgmusic", "");
	if (!hasFileExtension(fileName.c_str())) return 1;
	const auto extension = getFileExtension(fileName.c_str());
	return !stringEqualCaseIndependent(extension, "osu");
}

static void addSingleSelectStage(char* tPath) {
	char path[1024];
	getStagePath(path, tPath);

	if (!isFile(path)) {
		logWarningFormat("Unable to find stage file %s. Ignoring.", path);
		return;
	}

	if (isSelectStageLoadedAlready(path)) return;
	MugenDefScript script;
	loadMugenDefScript(&script, path);
	const int isUsingStage = !isInOsuModeAndNotOsuStage(&script);
	if (isUsingStage) {
		SelectStage* e = (SelectStage*)allocMemory(sizeof(SelectStage));
		strcpy(e->mPath, path);
		e->mName = getAllocatedMugenDefStringVariable(&script, "info", "name");
		loadSelectStageCredits(e, &script);
		vector_push_back_owned(&gCharacterSelectScreenData.mSelectStages, e);
	}
	unloadMugenDefScript(&script);
}

static void loadExtraStages() {
	if (!gCharacterSelectScreenData.mStageSelect.mIsUsing) return;

	MugenDefScriptGroup* group = &gCharacterSelectScreenData.mCharacterScript.mGroups["extrastages"];
	ListIterator iterator = list_iterator_begin(&group->mOrderedElementList);
	int hasElements = 1;
	while (hasElements) {
		MugenDefScriptGroupElement* element = (MugenDefScriptGroupElement*)list_iterator_get(iterator);
		if (element->mType == MUGEN_DEF_SCRIPT_GROUP_STRING_ELEMENT) {
			MugenDefScriptStringElement* stringElement = (MugenDefScriptStringElement*)element->mData;
			addSingleSelectStage(stringElement->mString);
		}

		if (!list_has_next(iterator)) break;
		list_iterator_increase(&iterator);
	}
}

static void loadStageSelectRandomStage() {
	if (gCharacterSelectScreenData.mSelectScreenType == CHARACTER_SELECT_SCREEN_TYPE_CREDITS) return;

	SelectStage* e = (SelectStage*)allocMemory(sizeof(SelectStage));
	e->mName = (char*)allocMemory(sizeof("Random") + 2);
	strcpy(e->mName, "Random");
	vector_push_back_owned(&gCharacterSelectScreenData.mSelectStages, e);
}

static void loadStageSelectStages() {
	if (!gCharacterSelectScreenData.mStageSelect.mIsUsing) return;

	gCharacterSelectScreenData.mSelectStages = new_vector();

	loadStageSelectRandomStage();
}


static void loadCreditsHeader() {
	if (gCharacterSelectScreenData.mSelectScreenType != CHARACTER_SELECT_SCREEN_TYPE_CREDITS) return;

	auto basePosition = getMugenDefVector2DOrDefault(&gCharacterSelectScreenData.mScript, "select info", "credits.position", Vector2D(0, 0)).xyz(40.0);
	basePosition = transformDreamCoordinatesVectorXY(basePosition, gCharacterSelectScreenData.mLocalCoord.x, getScreenSize().x);

	auto offset = getMugenDefVector2DOrDefault(&gCharacterSelectScreenData.mScript, "select info", "credits.name.offset", Vector2D(0, 0));
	offset = transformDreamCoordinatesVector2D(offset, gCharacterSelectScreenData.mLocalCoord.x, getScreenSize().x);
	gCharacterSelectScreenData.mCredits.mNamePosition = basePosition + offset;
	gCharacterSelectScreenData.mCredits.mNameFont = getMugenDefVectorIOrDefault(&gCharacterSelectScreenData.mScript, "select info", "credits.name.font", Vector3DI(-1, 0, 0));

	offset = getMugenDefVector2DOrDefault(&gCharacterSelectScreenData.mScript, "select info", "credits.author.offset", Vector2D(0, 0));
	offset = transformDreamCoordinatesVector2D(offset, gCharacterSelectScreenData.mLocalCoord.x, getScreenSize().x);
	gCharacterSelectScreenData.mCredits.mAuthorNamePosition = basePosition + offset;
	gCharacterSelectScreenData.mCredits.mAuthorNameFont = getMugenDefVectorIOrDefault(&gCharacterSelectScreenData.mScript, "select info", "credits.author.font", Vector3DI(-1, 0, 0));

	offset = getMugenDefVector2DOrDefault(&gCharacterSelectScreenData.mScript, "select info", "credits.versiondate.offset", Vector2D(0, 0));
	offset = transformDreamCoordinatesVector2D(offset, gCharacterSelectScreenData.mLocalCoord.x, getScreenSize().x);
	gCharacterSelectScreenData.mCredits.mVersionDatePosition = basePosition + offset;
	gCharacterSelectScreenData.mCredits.mVersionDateFont = getMugenDefVectorIOrDefault(&gCharacterSelectScreenData.mScript, "select info", "credits.versiondate.font", Vector3DI(-1, 0, 0));
}

static void loadMenuHeader() {
	gCharacterSelectScreenData.mHeader.mFadeInTime = getMugenDefIntegerOrDefault(&gCharacterSelectScreenData.mScript, "select info", "fadein.time", 30);
	gCharacterSelectScreenData.mHeader.mFadeOutTime = getMugenDefIntegerOrDefault(&gCharacterSelectScreenData.mScript, "select info", "fadeout.time", 30);

	gCharacterSelectScreenData.mHeader.mRows = getMugenDefIntegerOrDefault(&gCharacterSelectScreenData.mScript, "select info", "rows", 1);
	gCharacterSelectScreenData.mHeader.mColumns = getMugenDefIntegerOrDefault(&gCharacterSelectScreenData.mScript, "select info", "columns", 1);
	gCharacterSelectScreenData.mHeader.mIsWrapping = getMugenDefIntegerOrDefault(&gCharacterSelectScreenData.mScript, "select info", "wrapping", 0);
	gCharacterSelectScreenData.mHeader.mPosition = getMugenDefVector2DOrDefault(&gCharacterSelectScreenData.mScript, "select info", "pos", Vector2D(0, 0));
	gCharacterSelectScreenData.mHeader.mPosition = transformDreamCoordinatesVector2D(gCharacterSelectScreenData.mHeader.mPosition, gCharacterSelectScreenData.mLocalCoord.x, getScreenSize().x);

	gCharacterSelectScreenData.mHeader.mIsShowingEmptyBoxes = getMugenDefIntegerOrDefault(&gCharacterSelectScreenData.mScript, "select info", "showemptyboxes", 1);
	gCharacterSelectScreenData.mHeader.mCanMoveOverEmptyBoxes = getMugenDefIntegerOrDefault(&gCharacterSelectScreenData.mScript, "select info", "moveoveremptyboxes", 1);
	gCharacterSelectScreenData.mHeader.mIsShowingPortraits = getMugenDefIntegerOrDefault(&gCharacterSelectScreenData.mScript, "select info", "portraits", 1);
	gCharacterSelectScreenData.mHeader.mIsSkippingCharScriptLoading = getMugenDefIntegerOrDefault(&gCharacterSelectScreenData.mScript, "select info", "skipcharscriptloading", 0);
	if(gCharacterSelectScreenData.mSelectScreenType == CHARACTER_SELECT_SCREEN_TYPE_CREDITS || gCharacterSelectScreenData.mHeader.mIsShowingPortraits) gCharacterSelectScreenData.mHeader.mIsSkippingCharScriptLoading = 0;

	gCharacterSelectScreenData.mHeader.mCellSize = getMugenDefVector2DOrDefault(&gCharacterSelectScreenData.mScript, "select info", "cell.size", Vector2D(1, 1));
	gCharacterSelectScreenData.mHeader.mCellSize = transformDreamCoordinatesVector2D(gCharacterSelectScreenData.mHeader.mCellSize, gCharacterSelectScreenData.mLocalCoord.x, getScreenSize().x);
	gCharacterSelectScreenData.mHeader.mCellSpacing = getMugenDefFloatOrDefault(&gCharacterSelectScreenData.mScript, "select info", "cell.spacing", 10);
	gCharacterSelectScreenData.mHeader.mCellSpacing = transformDreamCoordinates(gCharacterSelectScreenData.mHeader.mCellSpacing, gCharacterSelectScreenData.mLocalCoord.x, getScreenSize().x);

	gCharacterSelectScreenData.mHeader.mCellBackgroundAnimation = getMugenDefMenuAnimationOrSprite(&gCharacterSelectScreenData.mScript, "select info", "cell.bg", gCharacterSelectScreenData.mHeader.mIsCellBackgroundAnimationOwned);
	gCharacterSelectScreenData.mHeader.mCellBackgroundScale = getMugenDefVector2DOrDefault(&gCharacterSelectScreenData.mScript, "select info", "cell.bg.scale", Vector2D(1.0, 1.0));
	gCharacterSelectScreenData.mHeader.mCellBackgroundScale = transformDreamCoordinatesVector2D(gCharacterSelectScreenData.mHeader.mCellBackgroundScale, gCharacterSelectScreenData.mLocalCoord.x, getScreenSize().x);
	gCharacterSelectScreenData.mHeader.mRandomSelectionAnimation = getMugenDefMenuAnimationOrSprite(&gCharacterSelectScreenData.mScript, "select info", "cell.random", gCharacterSelectScreenData.mHeader.mIsRandomSelectionAnimationOwned);
	gCharacterSelectScreenData.mHeader.mRandomSelectionScale = getMugenDefVector2DOrDefault(&gCharacterSelectScreenData.mScript, "select info", "cell.random.scale", Vector2D(1.0, 1.0));
	gCharacterSelectScreenData.mHeader.mRandomSelectionScale = transformDreamCoordinatesVector2D(gCharacterSelectScreenData.mHeader.mRandomSelectionScale, gCharacterSelectScreenData.mLocalCoord.x, getScreenSize().x);
	gCharacterSelectScreenData.mHeader.mRandomPortraitSwitchTime = getMugenDefIntegerOrDefault(&gCharacterSelectScreenData.mScript, "select info", "cell.random.switchtime", 1);

	const auto sprite = getMugenDefVector2DIOrDefault(&gCharacterSelectScreenData.mScript, "select info", "portrait.spr", Vector2DI(9000, 0));
	gCharacterSelectScreenData.mHeader.mSmallPortraitAnimation = createOneFrameMugenAnimationForSprite(sprite.x, sprite.y);

	gCharacterSelectScreenData.mHeader.mSmallPortraitOffset = getMugenDefVector2DOrDefault(&gCharacterSelectScreenData.mScript, "select info", "portrait.offset", Vector2D(0, 0));
	gCharacterSelectScreenData.mHeader.mSmallPortraitScale = getMugenDefVector2DOrDefault(&gCharacterSelectScreenData.mScript, "select info", "portrait.scale", Vector2D(1, 1));

	gCharacterSelectScreenData.mHeader.mTitleOffset = getMugenDefVector2DOrDefault(&gCharacterSelectScreenData.mScript, "select info", "title.offset", Vector2D(0, 0));
	gCharacterSelectScreenData.mHeader.mTitleFont = getMugenDefVectorIOrDefault(&gCharacterSelectScreenData.mScript, "select info", "title.font", Vector3DI(-1, 0, 0));

	loadCreditsHeader();
	loadStageSelectHeader();

	loadMenuPlayerHeader("p1", &gCharacterSelectScreenData.mHeader.mPlayers[0]);
	loadMenuPlayerHeader("p2", &gCharacterSelectScreenData.mHeader.mPlayers[1]);
}

typedef struct {
	int i;

} MenuCharacterLoadCaller;

static void loadMenuCharacterCredits(SelectCharacter& e, MugenDefScript* tScript) {
	if (gCharacterSelectScreenData.mSelectScreenType != CHARACTER_SELECT_SCREEN_TYPE_CREDITS) return;

	e.mCredits.mName = getAllocatedMugenDefStringOrDefault(tScript, "info", "name", e.mDisplayCharacterName);
	e.mCredits.mAuthorName = getAllocatedMugenDefStringOrDefault(tScript, "info", "author", "N/A");
	e.mCredits.mVersionDate = getAllocatedMugenDefStringOrDefault(tScript, "info", "versiondate", "N/A");
}

static int loadMenuCharacterSpritesAndNameAndReturnWhetherExists(SelectCharacter& e, const char* tCharacterName, const char* tOptionalDisplayName) {
	
	char file[200];
	char path[1024];
	char scriptPath[1024];
	char scriptPathPreloaded[1024];
	char name[100];
	char palettePath[1024];

	string sanitizedCharacterName = std::string(tCharacterName);
	std::replace(sanitizedCharacterName.begin(), sanitizedCharacterName.end(), '\\', '/');

	getCharacterSelectNamePath(sanitizedCharacterName.c_str(), scriptPath);
	if (!isFile(scriptPath)) {
		return 0;
	}

	e.mDisplayCharacterName = nullptr;

	if(!gCharacterSelectScreenData.mHeader.mIsSkippingCharScriptLoading)
	{
		MugenDefScript script;
		loadMugenDefScript(&script, scriptPath);

		if (gCharacterSelectScreenData.mHeader.mIsShowingPortraits) {
			getPathToFile(path, scriptPath);

			int preferredPalette = 0;
			sprintf(name, "pal%d", preferredPalette + 1);
			getMugenDefStringOrDefault(file, &script, "files", name, "");
			int hasPalettePath = strcmp("", file);
			sprintf(palettePath, "%s%s", path, file);

			getMugenDefStringOrDefault(file, &script, "files", "sprite", "");
			assert(strcmp("", file));
			sprintf(scriptPath, "%s%s", path, file);
			sprintf(scriptPathPreloaded, "%s.portraits.preloaded", scriptPath);
			if (isFile(scriptPathPreloaded)) {
				e.mSprites = loadMugenSpriteFilePortraits(scriptPathPreloaded, hasPalettePath, palettePath);
			}
			else {
				e.mSprites = loadMugenSpriteFilePortraits(scriptPath, hasPalettePath, palettePath);
			}
		}

		e.mDisplayCharacterName = getAllocatedMugenDefStringVariable(&script, "info", "displayname");

		loadMenuCharacterCredits(e, &script);
		
		unloadMugenDefScript(&script);
	}

	strcpy(e.mCharacterName, sanitizedCharacterName.c_str());
	e.mType = SELECT_CHARACTER_TYPE_CHARACTER;

	if(tOptionalDisplayName && tOptionalDisplayName[0] != '\0') {
		if(e.mDisplayCharacterName) freeMemory(e.mDisplayCharacterName);
		e.mDisplayCharacterName = (char*)allocMemory(strlen(tOptionalDisplayName) + 10u);
		strcpy(e.mDisplayCharacterName, tOptionalDisplayName);
	} else if (gCharacterSelectScreenData.mHeader.mIsSkippingCharScriptLoading) {
		e.mDisplayCharacterName = (char*)allocMemory(strlen(e.mCharacterName) + 10u);
		strcpy(e.mDisplayCharacterName, e.mCharacterName);
	}

	gCharacterSelectScreenData.mRealSelectCharacters.push_back(&e);
	return 1;
}

static Position2D getCellScreenPosition(const Vector2DI& tCellPosition) {
	double dx = tCellPosition.x * (gCharacterSelectScreenData.mHeader.mCellSpacing + gCharacterSelectScreenData.mHeader.mCellSize.x);
	double dy = tCellPosition.y * (gCharacterSelectScreenData.mHeader.mCellSpacing + gCharacterSelectScreenData.mHeader.mCellSize.y);
	return gCharacterSelectScreenData.mHeader.mPosition + Vector2D(dx, dy);
}

static void showMenuSelectableAnimations(MenuCharacterLoadCaller* /*tCaller*/, SelectCharacter& e) {
	auto pos = getCellScreenPosition(e.mCellPosition).xyz(40.0);
	if (gCharacterSelectScreenData.mHeader.mIsShowingPortraits) {
		e.mPortraitAnimationElement = addMugenAnimation(gCharacterSelectScreenData.mHeader.mSmallPortraitAnimation, &e.mSprites, pos + gCharacterSelectScreenData.mHeader.mSmallPortraitOffset);
		setMugenAnimationDrawScale(e.mPortraitAnimationElement, gCharacterSelectScreenData.mHeader.mSmallPortraitScale);
	}
	if (!gCharacterSelectScreenData.mHeader.mIsShowingEmptyBoxes) {
		pos.z = 30;
		e.mBackgroundAnimationElement = addMugenAnimation(gCharacterSelectScreenData.mHeader.mCellBackgroundAnimation, &gCharacterSelectScreenData.mSprites, pos);
		setMugenAnimationDrawScale(e.mBackgroundAnimationElement, gCharacterSelectScreenData.mHeader.mCellBackgroundScale);
	}
}

static void loadSingleRealMenuCharacter(MenuCharacterLoadCaller* tCaller, const MugenStringVector& tVector) {
	assert(tVector.mSize >= 1);
	char* characterName = tVector.mElement[0];

	int row = tCaller->i / gCharacterSelectScreenData.mHeader.mColumns;
	int column = tCaller->i % gCharacterSelectScreenData.mHeader.mColumns;

	
	auto& e = stl_list_at(stl_list_at(gCharacterSelectScreenData.mSelectCharacters, row), column);
	if (tVector.mSize >= 2)	{
		strcpy(e.mStageName, tVector.mElement[1]);
	}
	else {
		strcpy(e.mStageName, "random");
	}

	tCaller->i++;
	
	char displayName[100];
	displayName[0] = '\0';
	int doesIncludeStage = 1;
	int isValid = 1;
	parseOptionalCharacterSelectParameters(tVector, NULL, &doesIncludeStage, NULL, &isValid, displayName);
	if (!isValid) {
		return;
	}
	if (!loadMenuCharacterSpritesAndNameAndReturnWhetherExists(e, characterName, displayName)) {
		return;
	}
	if (gCharacterSelectScreenData.mStageSelect.mIsUsing && doesIncludeStage) {
		addSingleSelectStage(e.mStageName);
	}

	showMenuSelectableAnimations(tCaller, e);
}

static void loadSingleRandomMenuCharacter(MenuCharacterLoadCaller* tCaller) {
	int row = tCaller->i / gCharacterSelectScreenData.mHeader.mColumns;
	int column = tCaller->i % gCharacterSelectScreenData.mHeader.mColumns;

	auto& e = stl_list_at(stl_list_at(gCharacterSelectScreenData.mSelectCharacters, row), column);
	e.mType = SELECT_CHARACTER_TYPE_RANDOM;

	auto pos = getCellScreenPosition(e.mCellPosition).xyz(40.0);
	e.mPortraitAnimationElement = addMugenAnimation(gCharacterSelectScreenData.mHeader.mRandomSelectionAnimation, &gCharacterSelectScreenData.mSprites, pos + gCharacterSelectScreenData.mHeader.mSmallPortraitOffset);
	setMugenAnimationDrawScale(e.mPortraitAnimationElement, gCharacterSelectScreenData.mHeader.mRandomSelectionScale);
	if (!gCharacterSelectScreenData.mHeader.mIsShowingEmptyBoxes) {
		pos.z = 30;
		e.mBackgroundAnimationElement = addMugenAnimation(gCharacterSelectScreenData.mHeader.mCellBackgroundAnimation, &gCharacterSelectScreenData.mSprites, pos);
		setMugenAnimationDrawScale(e.mBackgroundAnimationElement, gCharacterSelectScreenData.mHeader.mCellBackgroundScale);
	}

	tCaller->i++;
}

static void loadSingleRealMenuCharacter(MenuCharacterLoadCaller* tCaller, const char* tString) {
	if (!strcmp("randomselect", tString)) {
		loadSingleRandomMenuCharacter(tCaller);
		return;
	}

	auto stringVector = createAllocatedMugenStringVectorFromString(tString);
	loadSingleRealMenuCharacter(tCaller, stringVector);
	destroyMugenStringVector(stringVector);
}

static void loadSingleMenuCharacter(void* tCaller, void* tData) {

	MenuCharacterLoadCaller* caller = (MenuCharacterLoadCaller*)tCaller;
	MugenDefScriptGroupElement* element = (MugenDefScriptGroupElement*)tData;
	if (element->mType == MUGEN_DEF_SCRIPT_GROUP_VECTOR_ELEMENT) {
		MugenDefScriptVectorElement* vectorElement = (MugenDefScriptVectorElement*)element->mData;
		loadSingleRealMenuCharacter(caller, vectorElement->mVector);
	} else if (element->mType == MUGEN_DEF_SCRIPT_GROUP_STRING_ELEMENT) {
		MugenDefScriptStringElement* stringElement = (MugenDefScriptStringElement*)element->mData;
		loadSingleRealMenuCharacter(caller, stringElement->mString);
	}
}

static void loadMenuCharacters() {
	MugenDefScriptGroup* e = &gCharacterSelectScreenData.mCharacterScript.mGroups["characters"];

	MenuCharacterLoadCaller caller;
	caller.i = 0;

	list_map(&e->mOrderedElementList, loadSingleMenuCharacter, &caller);
	gCharacterSelectScreenData.mCharacterAmount = caller.i;
}

static int loadSingleStoryFileAndReturnWhetherItExists(SelectCharacter& e, char* tPath) {
	char file[200];
	char path[1024];
	char scriptPath[1024];

	sprintf(scriptPath, "%s%s", getDolmexicaAssetFolder().c_str(), tPath);
	if (!isFile(scriptPath)) {
		return 0;
	}
	MugenDefScript script;
	loadMugenDefScript(&script, scriptPath);

	getPathToFile(path, scriptPath);

	getMugenDefStringOrDefault(file, &script, "info", "select.spr", "");
	assert(strcmp("", file));
	sprintf(scriptPath, "%s%s", path, file);
	e.mSprites = loadMugenSpriteFileWithoutPalette(scriptPath);

	strcpy(e.mCharacterName, tPath);
	e.mDisplayCharacterName = getAllocatedMugenDefStringVariable(&script, "info", "name");

	e.mType = SELECT_CHARACTER_TYPE_CHARACTER;

	unloadMugenDefScript(&script);

	gCharacterSelectScreenData.mRealSelectCharacters.push_back(&e);
	return 1;
}

static void loadSingleStory(MenuCharacterLoadCaller* tCaller, char* tPath) {
	if (!strcmp("randomselect", tPath)) {
		loadSingleRandomMenuCharacter(tCaller);
		return;
	}

	int row = tCaller->i / gCharacterSelectScreenData.mHeader.mColumns;
	int column = tCaller->i % gCharacterSelectScreenData.mHeader.mColumns;
	tCaller->i++;
	auto& e = stl_list_at(stl_list_at(gCharacterSelectScreenData.mSelectCharacters, row), column);
	strcpy(e.mStageName, "");
	if (!loadSingleStoryFileAndReturnWhetherItExists(e, tPath)) {
		return;
	}

	showMenuSelectableAnimations(tCaller, e);
}

static void loadSingleMenuStory(void* tCaller, void* tData) {
	
	MenuCharacterLoadCaller* caller = (MenuCharacterLoadCaller*)tCaller;
	MugenDefScriptGroupElement* element = (MugenDefScriptGroupElement*)tData;

	if (element->mType == MUGEN_DEF_SCRIPT_GROUP_STRING_ELEMENT) {
		MugenDefScriptStringElement* stringElement = (MugenDefScriptStringElement*)element->mData;
		loadSingleStory(caller, stringElement->mString);
	}


}

static void loadMenuStories() {
	MugenDefScriptGroup* e = &gCharacterSelectScreenData.mCharacterScript.mGroups["stories"];

	MenuCharacterLoadCaller caller;
	caller.i = 0;

	list_map(&e->mOrderedElementList, loadSingleMenuStory, &caller);
	gCharacterSelectScreenData.mCharacterAmount = caller.i;
}


static void loadMenuSelectables() {
	if (gCharacterSelectScreenData.mSelectScreenType != CHARACTER_SELECT_SCREEN_TYPE_STORY) {
		loadMenuCharacters();
	}
	else {
		loadMenuStories();
	}
}

static void loadSingleMenuCell(const Vector2DI& tCellPosition) {
	SelectCharacter e;
	e.mType = SELECT_CHARACTER_TYPE_EMPTY;
	e.mCellPosition = tCellPosition;

	if (gCharacterSelectScreenData.mHeader.mIsShowingEmptyBoxes) {
		const auto pos = getCellScreenPosition(e.mCellPosition).xyz(30.0);
		e.mBackgroundAnimationElement = addMugenAnimation(gCharacterSelectScreenData.mHeader.mCellBackgroundAnimation, &gCharacterSelectScreenData.mSprites, pos);
		setMugenAnimationDrawScale(e.mBackgroundAnimationElement, gCharacterSelectScreenData.mHeader.mCellBackgroundScale);
	}

	stl_list_at(gCharacterSelectScreenData.mSelectCharacters, tCellPosition.y).push_back(e);
}

static void loadMenuCells() {
	gCharacterSelectScreenData.mSelectCharacters.clear();
	gCharacterSelectScreenData.mRealSelectCharacters.clear();
	
	int y, x;
	for (y = 0; y < gCharacterSelectScreenData.mHeader.mRows; y++) {
		gCharacterSelectScreenData.mSelectCharacters.push_back(std::list<SelectCharacter>());

		for (x = 0; x < gCharacterSelectScreenData.mHeader.mColumns; x++) {
			loadSingleMenuCell(Vector2DI(x, y));
		}
	}
}

static SelectCharacter* getCellCharacter(const Vector2DI& tCellPosition) {
	SelectCharacter* ret = &stl_list_at(stl_list_at(gCharacterSelectScreenData.mSelectCharacters, tCellPosition.y), tCellPosition.x);
	return ret;
}

static void moveSelectionToTarget(int i, const Vector2DI& tTarget, int tDoesPlaySound);

static Vector2DI increaseCellPositionWithDirection(const Vector2DI& pos, int tDelta) {
	auto ret = pos;
	if (tDelta < 0)
	{
		if (pos.x == 0) {
			ret.x = gCharacterSelectScreenData.mHeader.mColumns - 1;
			ret.y = pos.y == 0 ? (gCharacterSelectScreenData.mHeader.mColumns - 1) : (pos.y - 1);
		}
		else {
			ret.x--;
		}
	}
	else {
		if (pos.x == gCharacterSelectScreenData.mHeader.mColumns - 1) {
			ret.x = 0;
			ret.y = (pos.y == gCharacterSelectScreenData.mHeader.mRows - 1) ? 0 : (pos.y + 1);
		}
		else {
			ret.x++;
		}
	}

	return ret;
}

static Vector2DI findStartCellPosition(const Vector2DI& startPos, int delta, int forceValid = 0) {
	auto pos = startPos;

	int maxLength = gCharacterSelectScreenData.mHeader.mRows * gCharacterSelectScreenData.mHeader.mColumns;
	for (int i = 0; i < maxLength; i++) {
		SelectCharacter* selectChar = getCellCharacter(pos);
		if ((gCharacterSelectScreenData.mHeader.mCanMoveOverEmptyBoxes && !forceValid) || selectChar->mType == SELECT_CHARACTER_TYPE_CHARACTER) return pos;

		pos = increaseCellPositionWithDirection(pos, delta);
	}

	return startPos;
}


static void loadSingleSelector(int i, int tOwner) {
	PlayerHeader* player = &gCharacterSelectScreenData.mHeader.mPlayers[i];
	PlayerHeader* owner = &gCharacterSelectScreenData.mHeader.mPlayers[tOwner];

	if (!gCharacterSelectScreenData.mSelectors[i].mHasBeenLoadedBefore) {
		gCharacterSelectScreenData.mSelectors[i].mSelectedCharacter = Vector2DI(player->mCursorStartCell.y, player->mCursorStartCell.x);
	}
	gCharacterSelectScreenData.mSelectors[i].mSelectedCharacter = findStartCellPosition(gCharacterSelectScreenData.mSelectors[i].mSelectedCharacter, i == 0 ? 1 : -1);

	const auto p = getCellScreenPosition(Vector2DI(gCharacterSelectScreenData.mSelectors[i].mSelectedCharacter.y, gCharacterSelectScreenData.mSelectors[i].mSelectedCharacter.x)).xyz(SELECTOR_Z);
	gCharacterSelectScreenData.mSelectors[i].mSelectorAnimationElement = addMugenAnimation(owner->mActiveCursorAnimation, &gCharacterSelectScreenData.mSprites, p);
	setMugenAnimationDrawScale(gCharacterSelectScreenData.mSelectors[i].mSelectorAnimationElement, owner->mActiveCursorScale);
	
	SelectCharacter* character = getCellCharacter(findStartCellPosition(gCharacterSelectScreenData.mSelectors[i].mSelectedCharacter, 1, 1));
	if (gCharacterSelectScreenData.mHeader.mIsShowingPortraits) {
		gCharacterSelectScreenData.mSelectors[i].mBigPortraitAnimationElement = addMugenAnimation(player->mBigPortraitAnimation, &character->mSprites, player->mBigPortraitOffset.xyz(30.0));
		setMugenAnimationFaceDirection(gCharacterSelectScreenData.mSelectors[i].mBigPortraitAnimationElement, player->mBigPortraitIsFacingRight);
		setMugenAnimationDrawScale(gCharacterSelectScreenData.mSelectors[i].mBigPortraitAnimationElement, Vector2D(player->mBigPortraitScale.x, player->mBigPortraitScale.y));
	}

	gCharacterSelectScreenData.mSelectors[i].mNameTextID = addMugenText(character->mDisplayCharacterName, player->mNameOffset.xyz(40.0), player->mNameFont.x);
	setMugenTextColor(gCharacterSelectScreenData.mSelectors[i].mNameTextID, getMugenTextColorFromMugenTextColorIndex(player->mNameFont.y));
	setMugenTextAlignment(gCharacterSelectScreenData.mSelectors[i].mNameTextID, getMugenTextAlignmentFromMugenAlignmentIndex(player->mNameFont.z));
	setMugenTextTextBoxWidth(gCharacterSelectScreenData.mSelectors[i].mNameTextID, player->mNameWidth);

	moveSelectionToTarget(i, gCharacterSelectScreenData.mSelectors[i].mSelectedCharacter, 0);

	gCharacterSelectScreenData.mSelectors[i].mOwner = tOwner;
	gCharacterSelectScreenData.mSelectors[i].mIsActive = 1;
	gCharacterSelectScreenData.mSelectors[i].mIsDone = 0;

	gCharacterSelectScreenData.mSelectors[i].mHasBeenLoadedBefore = 1;
}

static void unloadSingleSelector(int i) {
	removeMugenAnimation(gCharacterSelectScreenData.mSelectors[i].mSelectorAnimationElement);
	if (gCharacterSelectScreenData.mHeader.mIsShowingPortraits) {
		removeMugenAnimation(gCharacterSelectScreenData.mSelectors[i].mBigPortraitAnimationElement);
	}
	removeMugenText(gCharacterSelectScreenData.mSelectors[i].mNameTextID);

	gCharacterSelectScreenData.mSelectors[i].mIsActive = 0;
}

static void loadSelectors() {

	int i;
	for (i = 0; i < 2; i++) {
		gCharacterSelectScreenData.mSelectors[i].mIsActive = 0;
	}

	gCharacterSelectScreenData.mSelectorAmount = gCharacterSelectScreenData.mSelectScreenType == CHARACTER_SELECT_SCREEN_TYPE_TWO_PLAYER_MODE ? 2 : 1;
	for (i = 0; i < gCharacterSelectScreenData.mSelectorAmount; i++) {
		loadSingleSelector(i, gCharacterSelectScreenData.mControllerUsed[i]);
	}
}

static void loadModeText() {
	const auto p = gCharacterSelectScreenData.mHeader.mTitleOffset.xyz(50.0);
	gCharacterSelectScreenData.mModeText.mModeTextID = addMugenText(gCharacterSelectScreenData.mModeName, p, gCharacterSelectScreenData.mHeader.mTitleFont.x); 
	setMugenTextColor(gCharacterSelectScreenData.mModeText.mModeTextID, getMugenTextColorFromMugenTextColorIndex(gCharacterSelectScreenData.mHeader.mTitleFont.y));
	setMugenTextAlignment(gCharacterSelectScreenData.mModeText.mModeTextID, getMugenTextAlignmentFromMugenAlignmentIndex(gCharacterSelectScreenData.mHeader.mTitleFont.z));
}

static void loadCredits() {
	if (gCharacterSelectScreenData.mSelectScreenType != CHARACTER_SELECT_SCREEN_TYPE_CREDITS) return;

	gCharacterSelectScreenData.mCredits.mNameID = addMugenText("", gCharacterSelectScreenData.mCredits.mNamePosition, gCharacterSelectScreenData.mCredits.mNameFont.x);
	setMugenTextColor(gCharacterSelectScreenData.mCredits.mNameID, getMugenTextColorFromMugenTextColorIndex(gCharacterSelectScreenData.mCredits.mNameFont.y));
	setMugenTextAlignment(gCharacterSelectScreenData.mCredits.mNameID, getMugenTextAlignmentFromMugenAlignmentIndex(gCharacterSelectScreenData.mCredits.mNameFont.z));

	gCharacterSelectScreenData.mCredits.mAuthorNameID = addMugenText("", gCharacterSelectScreenData.mCredits.mAuthorNamePosition, gCharacterSelectScreenData.mCredits.mAuthorNameFont.x);
	setMugenTextColor(gCharacterSelectScreenData.mCredits.mAuthorNameID, getMugenTextColorFromMugenTextColorIndex(gCharacterSelectScreenData.mCredits.mAuthorNameFont.y));
	setMugenTextAlignment(gCharacterSelectScreenData.mCredits.mAuthorNameID, getMugenTextAlignmentFromMugenAlignmentIndex(gCharacterSelectScreenData.mCredits.mAuthorNameFont.z));

	gCharacterSelectScreenData.mCredits.mVersionDateID = addMugenText("", gCharacterSelectScreenData.mCredits.mVersionDatePosition, gCharacterSelectScreenData.mCredits.mVersionDateFont.x);
	setMugenTextColor(gCharacterSelectScreenData.mCredits.mVersionDateID, getMugenTextColorFromMugenTextColorIndex(gCharacterSelectScreenData.mCredits.mVersionDateFont.y));
	setMugenTextAlignment(gCharacterSelectScreenData.mCredits.mVersionDateID, getMugenTextAlignmentFromMugenAlignmentIndex(gCharacterSelectScreenData.mCredits.mVersionDateFont.z));
}

static void loadSelectMusic() {
	char* path = getAllocatedMugenDefStringOrDefault(&gCharacterSelectScreenData.mScript, "music", "select.bgm", " ");
	int isLooping = getMugenDefIntegerOrDefault(&gCharacterSelectScreenData.mScript, "music", "select.bgm.loop", 1);

	if (isMugenBGMMusicPath(path)) {
		playMugenBGMMusicPath(path, isLooping);
	}

	freeMemory(path);
}

static int isValidSelectPath(const std::string& path, std::string& oFinalSelectPath) {
	if (path.empty()) return 0;

	if (isFile(getDolmexicaAssetFolder() + "data/" + path)) {
		oFinalSelectPath = getDolmexicaAssetFolder() + "data/" + path;
		return 1;
	}
	if (isFile(getDolmexicaAssetFolder() + path)) {
		oFinalSelectPath = getDolmexicaAssetFolder() + path;
		return 1;
	}
	if (isFile(path)) {
		oFinalSelectPath = path;
		return 1;
	}

	return 0;
}

static int checkIfCharactersSane() {
	return int(gCharacterSelectScreenData.mRealSelectCharacters.size());
}

static int checkIfStagesSane() {
	if (!gCharacterSelectScreenData.mStageSelect.mIsUsing) return 1;
	else if (gCharacterSelectScreenData.mSelectScreenType == CHARACTER_SELECT_SCREEN_TYPE_CREDITS) return vector_size(&gCharacterSelectScreenData.mSelectStages);
	else return vector_size(&gCharacterSelectScreenData.mSelectStages) >= 2;
}

static void setStageSelectorSane() {
	if (!gCharacterSelectScreenData.mStageSelect.mIsUsing) return;
	gCharacterSelectScreenData.mStageSelect.mSelectedStage = min(gCharacterSelectScreenData.mStageSelect.mSelectedStage, vector_size(&gCharacterSelectScreenData.mSelectStages) - 1);
}

static int sanityCheck() {
	if (!checkIfCharactersSane() || !checkIfStagesSane()) {
		setNewScreen(getDreamTitleScreen());
		return 0;
	}
	else {
		setStageSelectorSane();
		return 1;
	}
}

static void loadCharacterSelectScreen() {
	setupDreamGlobalAssignmentEvaluator();

	std::string folder;
	const auto motifPath = getDolmexicaAssetFolder() + getMotifPath();
	loadMugenDefScript(&gCharacterSelectScreenData.mScript, motifPath);
	getPathToFile(folder, motifPath.c_str());

	std::string selectPath;
	if (!isValidSelectPath(gCharacterSelectScreenData.mCustomSelectFilePath, selectPath)) {
		selectPath = getSTLMugenDefStringVariable(&gCharacterSelectScreenData.mScript, "files", "select");
		selectPath = findMugenSystemOrFightFilePath(selectPath, folder);
	}
	else gCharacterSelectScreenData.mCustomSelectFilePath = "";

	loadMugenDefScript(&gCharacterSelectScreenData.mCharacterScript, selectPath);

	gCharacterSelectScreenData.mAnimations = loadMugenAnimationFile(motifPath);

	auto text = getSTLMugenDefStringVariable(&gCharacterSelectScreenData.mScript, "files", "spr");
	text = findMugenSystemOrFightFilePath(text, folder);
	gCharacterSelectScreenData.mSprites = loadMugenSpriteFileWithoutPalette(text);

	text = getSTLMugenDefStringVariable(&gCharacterSelectScreenData.mScript, "files", "snd");
	text = findMugenSystemOrFightFilePath(text, folder);
	gCharacterSelectScreenData.mSounds = loadMugenSoundFile(text.c_str());

	gCharacterSelectScreenData.mLocalCoord = getMugenDefVectorIOrDefault(&gCharacterSelectScreenData.mScript, "info", "localcoord", Vector3DI(320, 240, 0)).xy();

	loadMenuHeader();
	loadScriptBackground(&gCharacterSelectScreenData.mScript, &gCharacterSelectScreenData.mSprites, &gCharacterSelectScreenData.mAnimations, "selectbgdef", "selectbg", gCharacterSelectScreenData.mLocalCoord);

	loadStageSelectStages();
	loadMenuCells();
	loadMenuSelectables();
	loadExtraStages();

	if (!sanityCheck()) {
		return;
	}
	loadCredits();
	loadSelectors();
	loadModeText();
	loadStageSelect();
	loadSelectMusic();

	gCharacterSelectScreenData.mWhiteTexture = createWhiteTexture();

	gCharacterSelectScreenData.mIsFadingOut = 0;
	addFadeIn(gCharacterSelectScreenData.mHeader.mFadeInTime, NULL, NULL);
}

static void unloadSelectStageCredits(SelectStage* e) {
	if (gCharacterSelectScreenData.mSelectScreenType != CHARACTER_SELECT_SCREEN_TYPE_CREDITS) return;

	freeMemory(e->mCredits.mName);
	freeMemory(e->mCredits.mAuthorName);
	freeMemory(e->mCredits.mVersionDate);
}

static void unloadSingleSelectStage(void* tCaller, void* tData) {
	(void)tCaller;
	SelectStage* e = (SelectStage*)tData;
	freeMemory(e->mName);
	unloadSelectStageCredits(e);
}

static void unloadSelectStages() {
	if (!gCharacterSelectScreenData.mStageSelect.mIsUsing) return;

	vector_map(&gCharacterSelectScreenData.mSelectStages, unloadSingleSelectStage, NULL);
	delete_vector(&gCharacterSelectScreenData.mSelectStages);
}

static void unloadMenuCharacterCredits(SelectCharacter& e) {
	if (gCharacterSelectScreenData.mSelectScreenType != CHARACTER_SELECT_SCREEN_TYPE_CREDITS) return;

	freeMemory(e.mCredits.mName); 
	freeMemory(e.mCredits.mAuthorName);
	freeMemory(e.mCredits.mVersionDate);
}

static void unloadSingleSelectCharacter(SelectCharacter& e) {
	if (e.mType == SELECT_CHARACTER_TYPE_CHARACTER) {
		if (gCharacterSelectScreenData.mHeader.mIsShowingPortraits) {
			unloadMugenSpriteFile(&e.mSprites);
		}
		freeMemory(e.mDisplayCharacterName);
		unloadMenuCharacterCredits(e);
	}
}

static void unloadSelectCharacters() {
	gCharacterSelectScreenData.mRealSelectCharacters.clear();

	for (auto& row : gCharacterSelectScreenData.mSelectCharacters)
	{
		for (auto& character : row)
		{
			unloadSingleSelectCharacter(character);
		}
	}
	gCharacterSelectScreenData.mSelectCharacters.clear();
}

static void resetSelectState() {
	gCharacterSelectScreenData.mIsReturnDisabled = 0;
}

static void unloadCharacterSelectScreen() {
	unloadMugenDefScript(&gCharacterSelectScreenData.mScript);
	unloadMugenDefScript(&gCharacterSelectScreenData.mCharacterScript);

	unloadMugenSpriteFile(&gCharacterSelectScreenData.mSprites);
	unloadMugenAnimationFile(&gCharacterSelectScreenData.mAnimations);
	unloadMugenSoundFile(&gCharacterSelectScreenData.mSounds);

	if (gCharacterSelectScreenData.mHeader.mIsCellBackgroundAnimationOwned) {
		destroyMugenAnimation(gCharacterSelectScreenData.mHeader.mCellBackgroundAnimation);
	}
	if (gCharacterSelectScreenData.mHeader.mIsRandomSelectionAnimationOwned) {
		destroyMugenAnimation(gCharacterSelectScreenData.mHeader.mRandomSelectionAnimation);
	}

	int i;
	for (i = 0; i < 2; i++) {
		if (gCharacterSelectScreenData.mHeader.mPlayers[i].mIsActiveCursorAnimationOwned) {
			destroyMugenAnimation(gCharacterSelectScreenData.mHeader.mPlayers[i].mActiveCursorAnimation);
		}
		if (gCharacterSelectScreenData.mHeader.mPlayers[i].mIsDoneCursorAnimationOwned) {
			destroyMugenAnimation(gCharacterSelectScreenData.mHeader.mPlayers[i].mDoneCursorAnimation);
		}
		destroyMugenAnimation(gCharacterSelectScreenData.mHeader.mPlayers[i].mBigPortraitAnimation);
	}

	destroyMugenAnimation(gCharacterSelectScreenData.mHeader.mSmallPortraitAnimation);

	unloadTexture(gCharacterSelectScreenData.mWhiteTexture);

	unloadSelectCharacters();
	unloadSelectStages();

	resetSelectState();

	shutdownDreamAssignmentEvaluator();
}

static void handleSingleWrapping(int* tPosition, int tSize) {
	if (*tPosition < 0) {
		if (gCharacterSelectScreenData.mHeader.mIsWrapping) {
			*tPosition += tSize;
		}
		else {
			*tPosition = 0;
		}
	}

	if (*tPosition > tSize - 1) {
		if (gCharacterSelectScreenData.mHeader.mIsWrapping) {
			*tPosition -= tSize;
		}
		else {
			*tPosition = tSize - 1;
		}
	}
}

static Vector2DI findTargetCellPosition(int i, const Vector2DI& tDelta) {
	
	const auto startPos = gCharacterSelectScreenData.mSelectors[i].mSelectedCharacter;
	auto pos = startPos + tDelta;
	auto prevPos = startPos;

	int maxLength = max(gCharacterSelectScreenData.mHeader.mRows, gCharacterSelectScreenData.mHeader.mColumns);
	for (int j = 0; j < maxLength; j++) {
		handleSingleWrapping(&pos.x, gCharacterSelectScreenData.mHeader.mColumns);
		handleSingleWrapping(&pos.y, gCharacterSelectScreenData.mHeader.mRows);
		SelectCharacter* selectChar = getCellCharacter(pos);
		if (gCharacterSelectScreenData.mHeader.mCanMoveOverEmptyBoxes || selectChar->mType != SELECT_CHARACTER_TYPE_EMPTY) return pos;

		if ((pos == prevPos) || (pos == startPos)) return startPos;
		prevPos = pos;
		pos = pos + tDelta;
	}

	return startPos;	
}

static void updateCharacterSelectionCredits(SelectCharacter* character) {
	if (gCharacterSelectScreenData.mSelectScreenType != CHARACTER_SELECT_SCREEN_TYPE_CREDITS) return;

	if (character->mType == SELECT_CHARACTER_TYPE_CHARACTER) {
		char text[1024];
		sprintf(text, "Name: %s", character->mCredits.mName);
		changeMugenText(gCharacterSelectScreenData.mCredits.mNameID, text);
		sprintf(text, "Author: %s", character->mCredits.mAuthorName);
		changeMugenText(gCharacterSelectScreenData.mCredits.mAuthorNameID, text);
		sprintf(text, "Date: %s", character->mCredits.mVersionDate);
		changeMugenText(gCharacterSelectScreenData.mCredits.mVersionDateID, text);
	}
	else {
		changeMugenText(gCharacterSelectScreenData.mCredits.mNameID, "Name:");
		changeMugenText(gCharacterSelectScreenData.mCredits.mAuthorNameID, "Author:");
		changeMugenText(gCharacterSelectScreenData.mCredits.mVersionDateID, "Date:");
	}
}

static void showSelectCharacterForSelector(int i, SelectCharacter* tCharacter) {
	PlayerHeader* player = &gCharacterSelectScreenData.mHeader.mPlayers[i];

	if (tCharacter->mType == SELECT_CHARACTER_TYPE_CHARACTER) {
		if (gCharacterSelectScreenData.mHeader.mIsShowingPortraits) {
			setMugenAnimationBaseDrawScale(gCharacterSelectScreenData.mSelectors[i].mBigPortraitAnimationElement, 1);
			setMugenAnimationSprites(gCharacterSelectScreenData.mSelectors[i].mBigPortraitAnimationElement, &tCharacter->mSprites);
			changeMugenAnimation(gCharacterSelectScreenData.mSelectors[i].mBigPortraitAnimationElement, player->mBigPortraitAnimation);
		}
		changeMugenText(gCharacterSelectScreenData.mSelectors[i].mNameTextID, tCharacter->mDisplayCharacterName);
	}
	else {
		if (gCharacterSelectScreenData.mHeader.mIsShowingPortraits) {
			setMugenAnimationBaseDrawScale(gCharacterSelectScreenData.mSelectors[i].mBigPortraitAnimationElement, 0);
		}
		changeMugenText(gCharacterSelectScreenData.mSelectors[i].mNameTextID, " ");
	}

	updateCharacterSelectionCredits(tCharacter);
}

static void showNewRandomSelectCharacter(int i);

static void moveSelectionToTarget(int i, const Vector2DI& tTarget, int tDoesPlaySound) {
	PlayerHeader* owner = &gCharacterSelectScreenData.mHeader.mPlayers[gCharacterSelectScreenData.mSelectors[i].mOwner];

	gCharacterSelectScreenData.mSelectors[i].mSelectedCharacter = tTarget;
	const auto p = getCellScreenPosition(tTarget).xyz(SELECTOR_Z);
	setMugenAnimationPosition(gCharacterSelectScreenData.mSelectors[i].mSelectorAnimationElement, p);

	if (tDoesPlaySound) {
		tryPlayMugenSoundAdvanced(&gCharacterSelectScreenData.mSounds, owner->mCursorMoveSound.x, owner->mCursorMoveSound.y, parseGameMidiVolumeToPrism(getGameMidiVolume()));
	}

	SelectCharacter* character = getCellCharacter(gCharacterSelectScreenData.mSelectors[i].mSelectedCharacter);
	if (character->mType != SELECT_CHARACTER_TYPE_RANDOM) {
		showSelectCharacterForSelector(i, character);
	}
	else {
		showNewRandomSelectCharacter(i);
	}
}

static int hasCreditSelectionMovedToStage(int i, const Vector2DI& tDelta) {
	if (gCharacterSelectScreenData.mSelectScreenType != CHARACTER_SELECT_SCREEN_TYPE_CREDITS) return 0;

	const auto target = gCharacterSelectScreenData.mSelectors[i].mSelectedCharacter + tDelta;
	return (target.y == -1 || target.y == gCharacterSelectScreenData.mHeader.mRows);
}

static void setStageSelectActive(int i);

static void moveCreditSelectionToStage(int i) {
	setStageSelectActive(i);
	unloadSingleSelector(i);
}

static void updateSingleSelectionInput(int i) {
	auto delta = Vector2DI(0, 0);
	if (hasPressedRightFlankSingle(gCharacterSelectScreenData.mSelectors[i].mOwner)) {
		delta = Vector2DI(1, 0);
	}
	else if (hasPressedLeftFlankSingle(gCharacterSelectScreenData.mSelectors[i].mOwner)) {
		delta = Vector2DI(-1, 0);
	}

	if (hasPressedDownFlankSingle(gCharacterSelectScreenData.mSelectors[i].mOwner)) {
		delta = Vector2DI(0, 1);
	}
	else if (hasPressedUpFlankSingle(gCharacterSelectScreenData.mSelectors[i].mOwner)) {
		delta = Vector2DI(0, -1);
	}

	if (hasCreditSelectionMovedToStage(i, delta)) {
		moveCreditSelectionToStage(i);
		return;
	}

	const auto target = findTargetCellPosition(i, delta);
	if (gCharacterSelectScreenData.mSelectors[i].mSelectedCharacter == target) return;

	moveSelectionToTarget(i, target, 1);
}

static void showNewRandomSelectCharacter(int i) {
	int amount = int(gCharacterSelectScreenData.mRealSelectCharacters.size());
	int newIndex = gCharacterSelectScreenData.mSelectors[i].mRandom.mCurrentCharacter;
	if (amount > 1) {
		while (newIndex == gCharacterSelectScreenData.mSelectors[i].mRandom.mCurrentCharacter) {
			newIndex = randfromInteger(0, amount - 1);
		}
	}
	else {
		newIndex = 0;
	}

	gCharacterSelectScreenData.mSelectors[i].mRandom.mCurrentCharacter = newIndex;
	auto e = gCharacterSelectScreenData.mRealSelectCharacters[gCharacterSelectScreenData.mSelectors[i].mRandom.mCurrentCharacter];
	showSelectCharacterForSelector(i, e);
	gCharacterSelectScreenData.mSelectors[i].mRandom.mNow = 0;
}

static void updateSingleSelectionRandom(int i) {
	SelectCharacter* character = getCellCharacter(gCharacterSelectScreenData.mSelectors[i].mSelectedCharacter);
	if (character->mType != SELECT_CHARACTER_TYPE_RANDOM) return;

	gCharacterSelectScreenData.mSelectors[i].mRandom.mNow++;
	if (gCharacterSelectScreenData.mSelectors[i].mRandom.mNow >= gCharacterSelectScreenData.mHeader.mRandomPortraitSwitchTime) {
		showNewRandomSelectCharacter(i);
		tryPlayMugenSoundAdvanced(&gCharacterSelectScreenData.mSounds, gCharacterSelectScreenData.mHeader.mPlayers[i].mRandomMoveSound.x, gCharacterSelectScreenData.mHeader.mPlayers[i].mRandomMoveSound.y, parseGameMidiVolumeToPrism(getGameMidiVolume()));
	}

}

static void updateSingleSelection(int i) {
	if (!gCharacterSelectScreenData.mSelectors[i].mIsActive) return;
	if (gCharacterSelectScreenData.mSelectors[i].mIsDone) return;


	updateSingleSelectionInput(i);
	updateSingleSelectionRandom(i);
}

static void updateStageSelection(int i, int tNewStage, int tDoesPlaySound);
static void setStageSelectInactive();

static int updateCreditStageSelectionAndReturnIfStageSelectionOver(int i) {
	if (gCharacterSelectScreenData.mSelectScreenType != CHARACTER_SELECT_SCREEN_TYPE_CREDITS) return 0;

	if (hasPressedUpFlankSingle(i) || hasPressedDownFlankSingle(i)) {
		setStageSelectInactive();
		loadSingleSelector(i, i);
		return 1;
	}

	return 0;
}

static void updateSingleStageSelection(int i) {
	if (!gCharacterSelectScreenData.mSelectors[i].mIsActive && gCharacterSelectScreenData.mSelectScreenType != CHARACTER_SELECT_SCREEN_TYPE_CREDITS) return;
	if (!gCharacterSelectScreenData.mSelectors[i].mIsDone && gCharacterSelectScreenData.mSelectScreenType != CHARACTER_SELECT_SCREEN_TYPE_CREDITS) return;
	if ((gCharacterSelectScreenData.mSelectScreenType == CHARACTER_SELECT_SCREEN_TYPE_ONE_PLAYER_SELECTS_EVERYTHING || gCharacterSelectScreenData.mSelectScreenType == CHARACTER_SELECT_SCREEN_TYPE_CREDITS) && i == 1) return;
	if (!gCharacterSelectScreenData.mStageSelect.mIsUsing) return;
	if (!gCharacterSelectScreenData.mStageSelect.mIsActive) return;
	if (gCharacterSelectScreenData.mStageSelect.mIsDone) return;

	int stageAmount = vector_size(&gCharacterSelectScreenData.mSelectStages);
	int target = gCharacterSelectScreenData.mStageSelect.mSelectedStage;
	if (hasPressedRightFlankSingle(gCharacterSelectScreenData.mSelectors[i].mOwner)) {
		target = (target + 1) % stageAmount;	
	}
	else if (hasPressedLeftFlankSingle(gCharacterSelectScreenData.mSelectors[i].mOwner)) {
		target--;
		if (target < 0) target += stageAmount;
	}

	if (updateCreditStageSelectionAndReturnIfStageSelectionOver(i)) return;

	if (target == gCharacterSelectScreenData.mStageSelect.mSelectedStage) return;
	updateStageSelection(i, target, 1);
}

static void updateSelections() {
	int i;
	for (i = 0; i < 2; i++) {
		updateSingleStageSelection(i); // doing this after character selection update breaks due to input flanks
		updateSingleSelection(i);
	}
}

static void gotoTitleScreenCB(void* tCaller) {
	(void)tCaller;
	setNewScreen(getDreamTitleScreen());
}

static void fadeToTitleScreen() {
	gCharacterSelectScreenData.mIsFadingOut = 1;
	addFadeOut(gCharacterSelectScreenData.mHeader.mFadeOutTime, gotoTitleScreenCB, NULL);
}

static void gotoNextScreen(void* tCaller) {
	(void)tCaller;

	gCharacterSelectScreenData.mCB();
}

static void fadeToNextScreen() {
	gCharacterSelectScreenData.mIsFadingOut = 1;
	addFadeOut(gCharacterSelectScreenData.mHeader.mFadeOutTime, gotoNextScreen, NULL);
}

static void checkFadeToNextScreen() {
	int hasPlayer1Selected = !gCharacterSelectScreenData.mSelectors[0].mIsActive || gCharacterSelectScreenData.mSelectors[0].mIsDone;
	int hasPlayer2Selected = !gCharacterSelectScreenData.mSelectors[1].mIsActive || gCharacterSelectScreenData.mSelectors[1].mIsDone;
	int hasStageSelected = !gCharacterSelectScreenData.mStageSelect.mIsUsing || gCharacterSelectScreenData.mStageSelect.mIsDone;

	if (hasPlayer1Selected && hasPlayer2Selected && hasStageSelected) {
		fadeToNextScreen();
	}
}

static void flashSelection(int i) {
	const auto pos = getCellScreenPosition(gCharacterSelectScreenData.mSelectors[i].mSelectedCharacter).xyz(41.0);
	Animation anim = createAnimation(1, 2);
	AnimationHandlerElement* element = playAnimation(pos + gCharacterSelectScreenData.mHeader.mSmallPortraitOffset, &gCharacterSelectScreenData.mWhiteTexture, anim, makeRectangleFromTexture(gCharacterSelectScreenData.mWhiteTexture), NULL, NULL);
	setAnimationSize(element, Vector3D(25, 25, 1) * gCharacterSelectScreenData.mHeader.mSmallPortraitScale, Vector3D(0, 0, 0));
}

static void updateMugenTextBasedOnVector3DI(int tID, const Vector3DI& tFontData) {
	setMugenTextFont(tID, tFontData.x);
	setMugenTextColor(tID, getMugenTextColorFromMugenTextColorIndex(tFontData.y));
	setMugenTextAlignment(tID, getMugenTextAlignmentFromMugenAlignmentIndex(tFontData.z));
}

static void updateStageCredit(SelectStage* tStage) {
	if (gCharacterSelectScreenData.mSelectScreenType != CHARACTER_SELECT_SCREEN_TYPE_CREDITS) return;

	char text[1024];
	sprintf(text, "Name: %s", tStage->mCredits.mName);
	changeMugenText(gCharacterSelectScreenData.mCredits.mNameID, text);
	sprintf(text, "Author: %s", tStage->mCredits.mAuthorName);
	changeMugenText(gCharacterSelectScreenData.mCredits.mAuthorNameID, text);
	sprintf(text, "Date: %s", tStage->mCredits.mVersionDate);
	changeMugenText(gCharacterSelectScreenData.mCredits.mVersionDateID, text);
}

static bool isSelectedStageRandomStage(int tIndex) {
	return gCharacterSelectScreenData.mSelectScreenType != CHARACTER_SELECT_SCREEN_TYPE_CREDITS && tIndex == 0;
}

static void updateStageSelection(int i, int tNewStage, int tDoesPlaySound) {
	SelectStage* stage = (SelectStage*)vector_get(&gCharacterSelectScreenData.mSelectStages, tNewStage);

	char newText[200];
	if (isSelectedStageRandomStage(tNewStage))
	{
		sprintf(newText, "Stage: %s", stage->mName);
	}
	else
	{
		if (gCharacterSelectScreenData.mSelectScreenType != CHARACTER_SELECT_SCREEN_TYPE_CREDITS) {
			sprintf(newText, "Stage %d: %s", tNewStage, stage->mName);
		}
		else
		{
			sprintf(newText, "Stage %d: %s", tNewStage + 1, stage->mName);
		}
	}
	changeMugenText(gCharacterSelectScreenData.mStageSelect.mTextID, newText);
	updateStageCredit(stage);

	if (tDoesPlaySound) {
		tryPlayMugenSoundAdvanced(&gCharacterSelectScreenData.mSounds, gCharacterSelectScreenData.mHeader.mPlayers[i].mCursorMoveSound.x, gCharacterSelectScreenData.mHeader.mPlayers[i].mCursorMoveSound.y, parseGameMidiVolumeToPrism(getGameMidiVolume()));
	}

	gCharacterSelectScreenData.mStageSelect.mSelectedStage = tNewStage;
}

static void setStageSelectActive(int i) {
	updateStageSelection(i, gCharacterSelectScreenData.mStageSelect.mSelectedStage, 0);
	updateMugenTextBasedOnVector3DI(gCharacterSelectScreenData.mStageSelect.mTextID, gCharacterSelectScreenData.mHeader.mStageSelect.mActiveFont1);
	
	gCharacterSelectScreenData.mStageSelect.mActiveFontDisplayed = 0;
	gCharacterSelectScreenData.mStageSelect.mIsActive = 1;
	gCharacterSelectScreenData.mStageSelect.mIsDone = 0;
}

static void checkSetStageSelectActive(int i) {
	if (!gCharacterSelectScreenData.mStageSelect.mIsUsing) return;
	if (gCharacterSelectScreenData.mStageSelect.mIsActive) return;
	if (gCharacterSelectScreenData.mSelectScreenType == CHARACTER_SELECT_SCREEN_TYPE_ONE_PLAYER_SELECTS_EVERYTHING && i == 0) return;

	setStageSelectActive(i);
}

static void setStageSelectInactive() {
	changeMugenText(gCharacterSelectScreenData.mStageSelect.mTextID, " ");
	gCharacterSelectScreenData.mStageSelect.mIsActive = 0;
}

static int checkSetStageSelectInactive(int i) {
	if (!gCharacterSelectScreenData.mStageSelect.mIsUsing) return 0;
	if (!gCharacterSelectScreenData.mStageSelect.mIsActive) return 0;
	if (gCharacterSelectScreenData.mSelectScreenType == CHARACTER_SELECT_SCREEN_TYPE_ONE_PLAYER_SELECTS_EVERYTHING && i == 0) return 0;

	int otherPlayer = i ^ 1;
	int isOtherPlayerFinished = !gCharacterSelectScreenData.mSelectors[otherPlayer].mIsActive || gCharacterSelectScreenData.mSelectors[otherPlayer].mIsDone;
	if (!gCharacterSelectScreenData.mStageSelect.mIsDone) {
		if (gCharacterSelectScreenData.mSelectScreenType == CHARACTER_SELECT_SCREEN_TYPE_ONE_PLAYER_SELECTS_EVERYTHING || gCharacterSelectScreenData.mSelectScreenType == CHARACTER_SELECT_SCREEN_TYPE_ONE_PLAYER_MODE || !isOtherPlayerFinished) {
			setStageSelectInactive();
		}
		return 0;
	}
	else {
		PlayerHeader* owner = &gCharacterSelectScreenData.mHeader.mPlayers[gCharacterSelectScreenData.mSelectors[i].mOwner];
		tryPlayMugenSoundAdvanced(&gCharacterSelectScreenData.mSounds, owner->mCursorDoneSound.x, owner->mCursorDoneSound.y, parseGameMidiVolumeToPrism(getGameMidiVolume()));
		setStageSelectActive(i);
	}

	return 1;
}

static void checkSetSecondPlayerActive(int i) {
	if (i == 0 && gCharacterSelectScreenData.mSelectScreenType == CHARACTER_SELECT_SCREEN_TYPE_ONE_PLAYER_SELECTS_EVERYTHING) {
		loadSingleSelector(1, 0);
	}
}

static void deselectSelection(int i, int tDoesPlaySound);

static void checkSetSecondPlayerInactive(int i) {
	if (i == 1 && gCharacterSelectScreenData.mSelectScreenType == CHARACTER_SELECT_SCREEN_TYPE_ONE_PLAYER_SELECTS_EVERYTHING && !gCharacterSelectScreenData.mSelectors[i].mIsDone) {
		unloadSingleSelector(1);
		deselectSelection(0, 0);
	}
}

static void setCharacterSelectionFinished(int i) {
	SelectCharacter* character = getCellCharacter(gCharacterSelectScreenData.mSelectors[i].mSelectedCharacter);
	if (character->mType == SELECT_CHARACTER_TYPE_EMPTY) return;

	PlayerHeader* owner = &gCharacterSelectScreenData.mHeader.mPlayers[gCharacterSelectScreenData.mSelectors[i].mOwner];
	changeMugenAnimation(gCharacterSelectScreenData.mSelectors[i].mSelectorAnimationElement, owner->mDoneCursorAnimation);
	setMugenAnimationDrawScale(gCharacterSelectScreenData.mSelectors[i].mSelectorAnimationElement, owner->mDoneCursorScale);
	tryPlayMugenSoundAdvanced(&gCharacterSelectScreenData.mSounds, owner->mCursorDoneSound.x, owner->mCursorDoneSound.y, parseGameMidiVolumeToPrism(getGameMidiVolume()));

	if (character->mType == SELECT_CHARACTER_TYPE_RANDOM) {
		character = gCharacterSelectScreenData.mRealSelectCharacters[gCharacterSelectScreenData.mSelectors[i].mRandom.mCurrentCharacter];
	}

	if (gCharacterSelectScreenData.mSelectScreenType != CHARACTER_SELECT_SCREEN_TYPE_STORY) {
		char path[1024];
		getCharacterSelectNamePath(character->mCharacterName, path);
		setPlayerDefinitionPath(i, path);
	}
	else {
		setStoryModeStoryPath(character->mCharacterName);
	}

	flashSelection(i);

	checkSetSecondPlayerActive(i);
	checkSetStageSelectActive(i);

	gCharacterSelectScreenData.mSelectors[i].mIsDone = 1;

	checkFadeToNextScreen();
}

static void setStageSelectionFinished(int i) {
	if (!gCharacterSelectScreenData.mStageSelect.mIsUsing || !gCharacterSelectScreenData.mStageSelect.mIsActive || gCharacterSelectScreenData.mStageSelect.mIsDone) return;

	tryPlayMugenSoundAdvanced(&gCharacterSelectScreenData.mSounds, gCharacterSelectScreenData.mHeader.mPlayers[i].mCursorDoneSound.x, gCharacterSelectScreenData.mHeader.mPlayers[i].mCursorDoneSound.y, parseGameMidiVolumeToPrism(getGameMidiVolume()));

	SelectStage* stage;
	if (isSelectedStageRandomStage(gCharacterSelectScreenData.mStageSelect.mSelectedStage)) {
		const auto stageAmount = vector_size(&gCharacterSelectScreenData.mSelectStages);
		stage = (SelectStage*)vector_get(&gCharacterSelectScreenData.mSelectStages, randfromInteger(1, stageAmount - 1));
	}
	else {
		stage = (SelectStage*)vector_get(&gCharacterSelectScreenData.mSelectStages, gCharacterSelectScreenData.mStageSelect.mSelectedStage);
	}
	char dummyMusicPath[2];
	*dummyMusicPath = '\0';
	setDreamStageMugenDefinition(stage->mPath, dummyMusicPath);

	updateMugenTextBasedOnVector3DI(gCharacterSelectScreenData.mStageSelect.mTextID, gCharacterSelectScreenData.mHeader.mStageSelect.mDoneFont);
	gCharacterSelectScreenData.mStageSelect.mIsDone = 1;
	checkFadeToNextScreen();
}

static void setSelectionFinished(int i) {
	if (gCharacterSelectScreenData.mSelectors[i].mIsDone) {
		setStageSelectionFinished(i);
	}
	else {
		setCharacterSelectionFinished(i);
	}
}

static void deselectSelection(int i, int tDoesPlaySound) {
	if (checkSetStageSelectInactive(i)) return;

	PlayerHeader* owner = &gCharacterSelectScreenData.mHeader.mPlayers[gCharacterSelectScreenData.mSelectors[i].mOwner];
	changeMugenAnimation(gCharacterSelectScreenData.mSelectors[i].mSelectorAnimationElement, owner->mActiveCursorAnimation);
	setMugenAnimationDrawScale(gCharacterSelectScreenData.mSelectors[i].mSelectorAnimationElement, owner->mActiveCursorScale);
	if(tDoesPlaySound) tryPlayMugenSoundAdvanced(&gCharacterSelectScreenData.mSounds, owner->mCursorDoneSound.x, owner->mCursorDoneSound.y, parseGameMidiVolumeToPrism(getGameMidiVolume()));

	checkSetSecondPlayerInactive(i);

	gCharacterSelectScreenData.mSelectors[i].mIsDone = 0;
}

static void updateSingleSelectionConfirmation(int i) {
	if (!gCharacterSelectScreenData.mCanSelectThisFrame) return;
	if (!gCharacterSelectScreenData.mSelectors[i].mIsActive) return;
	if (gCharacterSelectScreenData.mSelectScreenType == CHARACTER_SELECT_SCREEN_TYPE_CREDITS) return;
	if (gCharacterSelectScreenData.mSelectScreenType == CHARACTER_SELECT_SCREEN_TYPE_ONE_PLAYER_SELECTS_EVERYTHING && i == 0 && gCharacterSelectScreenData.mSelectors[1].mIsActive) return;

	if (hasPressedAFlankSingle(gCharacterSelectScreenData.mSelectors[i].mOwner) || hasPressedStartFlankSingle(gCharacterSelectScreenData.mSelectors[i].mOwner)) {
		setSelectionFinished(i);
		if (gCharacterSelectScreenData.mSelectScreenType == CHARACTER_SELECT_SCREEN_TYPE_ONE_PLAYER_SELECTS_EVERYTHING) {
			gCharacterSelectScreenData.mCanSelectThisFrame = 0;
		}
	}
}

static void updateSingleSelectionDeselect(int i) {
	if (!gCharacterSelectScreenData.mCanSelectThisFrame) return;
	if (gCharacterSelectScreenData.mIsFadingOut) return;
	if (!gCharacterSelectScreenData.mSelectors[i].mIsActive && gCharacterSelectScreenData.mSelectScreenType != CHARACTER_SELECT_SCREEN_TYPE_CREDITS) return;
	if (gCharacterSelectScreenData.mSelectScreenType == CHARACTER_SELECT_SCREEN_TYPE_ONE_PLAYER_SELECTS_EVERYTHING && i == 0 && gCharacterSelectScreenData.mSelectors[i].mIsDone) return;

	if (hasPressedBFlankSingle(gCharacterSelectScreenData.mSelectors[i].mOwner)) {
		int isSelectorDone = gCharacterSelectScreenData.mSelectors[i].mIsDone && gCharacterSelectScreenData.mSelectScreenType != CHARACTER_SELECT_SCREEN_TYPE_CREDITS;
		int canGoBackToFirstPlayer = i == 1 && gCharacterSelectScreenData.mSelectScreenType == CHARACTER_SELECT_SCREEN_TYPE_ONE_PLAYER_SELECTS_EVERYTHING && !gCharacterSelectScreenData.mSelectors[i].mIsDone;
		if (isSelectorDone || canGoBackToFirstPlayer) {
			deselectSelection(i, 1);
			if (gCharacterSelectScreenData.mSelectScreenType == CHARACTER_SELECT_SCREEN_TYPE_ONE_PLAYER_SELECTS_EVERYTHING) {
				gCharacterSelectScreenData.mCanSelectThisFrame = 0;
			}
		}
		else if(!gCharacterSelectScreenData.mIsReturnDisabled){
			fadeToTitleScreen();
		}
	}
}

static void updateSelectionInputs() {
	gCharacterSelectScreenData.mCanSelectThisFrame = 1;
	int i;
	for (i = 0; i < 2; i++) {
		updateSingleSelectionConfirmation(i);
		updateSingleSelectionDeselect(i);
	}
}

static void updateStageSelect() {
	if (!gCharacterSelectScreenData.mStageSelect.mIsUsing || !gCharacterSelectScreenData.mStageSelect.mIsActive || gCharacterSelectScreenData.mStageSelect.mIsDone) return;

	gCharacterSelectScreenData.mStageSelect.mActiveFontDisplayed = (gCharacterSelectScreenData.mStageSelect.mActiveFontDisplayed + 1) % 4;
	Vector3DI fontData = gCharacterSelectScreenData.mStageSelect.mActiveFontDisplayed < 2 ? gCharacterSelectScreenData.mHeader.mStageSelect.mActiveFont2 : gCharacterSelectScreenData.mHeader.mStageSelect.mActiveFont1;
	updateMugenTextBasedOnVector3DI(gCharacterSelectScreenData.mStageSelect.mTextID, fontData);
}

static void updateCharacterSelectScreen() {
	updateSelections();
	updateSelectionInputs();
	updateStageSelect();
}

static Screen gCharacterSelectScreen;

Screen* getCharacterSelectScreen() {
	gCharacterSelectScreen = makeScreen(loadCharacterSelectScreen, updateCharacterSelectScreen, NULL, unloadCharacterSelectScreen);
	return &gCharacterSelectScreen;
};

void setCharacterSelectCustomSelectFile(const std::string& tFileName)
{
	gCharacterSelectScreenData.mCustomSelectFilePath = tFileName;
}

void setCharacterSelectScreenModeName(const char * tModeName)
{
	strcpy(gCharacterSelectScreenData.mModeName, tModeName);
}

void setCharacterSelectFinishedCB(void(*tCB)())
{
	gCharacterSelectScreenData.mCB = tCB;
}

void setCharacterSelectStageActive(int tOnlyOsuStages)
{
	gCharacterSelectScreenData.mStageSelect.mIsInOsuMode = tOnlyOsuStages;
	gCharacterSelectScreenData.mStageSelect.mIsUsing = 1;
}

void setCharacterSelectStageInactive()
{
	gCharacterSelectScreenData.mStageSelect.mIsUsing = 0;
}

void setCharacterSelectOnePlayer()
{
	for (int i = 0; i < 2; i++) {
		gCharacterSelectScreenData.mControllerUsed[i] = i;
	}
	gCharacterSelectScreenData.mSelectScreenType = CHARACTER_SELECT_SCREEN_TYPE_ONE_PLAYER_MODE;
}

void setCharacterSelectOnePlayerSelectAll()
{
	for (int i = 0; i < 2; i++) {
		gCharacterSelectScreenData.mControllerUsed[i] = i;
	}
	gCharacterSelectScreenData.mSelectScreenType = CHARACTER_SELECT_SCREEN_TYPE_ONE_PLAYER_SELECTS_EVERYTHING;
}

void setCharacterSelectTwoPlayers()
{
	for (int i = 0; i < 2; i++) {
		gCharacterSelectScreenData.mControllerUsed[i] = i;
	}
	gCharacterSelectScreenData.mSelectScreenType = CHARACTER_SELECT_SCREEN_TYPE_TWO_PLAYER_MODE;

}

void setCharacterSelectNetplay(int isHost)
{
	gCharacterSelectScreenData.mControllerUsed[0] = isHost ? 0 : 1;
	gCharacterSelectScreenData.mControllerUsed[1] = isHost ? 1 : 0;
	gCharacterSelectScreenData.mSelectScreenType = CHARACTER_SELECT_SCREEN_TYPE_TWO_PLAYER_MODE;
}

void setCharacterSelectCredits()
{
	for (int i = 0; i < 2; i++) {
		gCharacterSelectScreenData.mControllerUsed[i] = i;
	}
	gCharacterSelectScreenData.mSelectScreenType = CHARACTER_SELECT_SCREEN_TYPE_CREDITS;
}

void setCharacterSelectStory()
{
	for (int i = 0; i < 2; i++) {
		gCharacterSelectScreenData.mControllerUsed[i] = i;
	}
	gCharacterSelectScreenData.mSelectScreenType = CHARACTER_SELECT_SCREEN_TYPE_STORY;
}

void setCharacterSelectDisableReturnOneTime()
{
	gCharacterSelectScreenData.mIsReturnDisabled = 1;
}

void resetCharacterSelectSelectors()
{
	for (int i = 0; i < 2; i++) {
		gCharacterSelectScreenData.mSelectors[i].mHasBeenLoadedBefore = 0;
	}
}

static char* removeEmptyParameterSpace(char* p, int delta, char* startChar) {
	while (*p == ' ' && p > startChar) p += delta;

	return p;
}

static int parseSingleOptionalParameterPart(char* tStart, char* tEnd, char* tParameter, char* oDst) {
	tStart = removeEmptyParameterSpace(tStart, 1, tParameter);
	tEnd = removeEmptyParameterSpace(tEnd, -1, tParameter);

	auto length = tEnd - tStart + 1;
	if (length <= 0) return 0;

	strcpy(oDst, tStart);
	oDst[length] = '\0';

	return 1;
}

static void parseOptionalParameterPaths(char* tParameter, char* oName, char* oValue) {
	*oName = '\0';
	*oValue = '\0';

	char* equalSign = strchr(tParameter, '=');
	if (!equalSign) return;

	if (!parseSingleOptionalParameterPart(tParameter, equalSign - 1, tParameter, oName)) return;
	if (!parseSingleOptionalParameterPart(equalSign + 1, tParameter + strlen(tParameter) - 1, tParameter, oValue)) return;
}

static void parseSingleOptionalCharacterParameter(char* tParameter, int* oOrder, int* oDoesIncludeStage, char* oMusicPath, int* oIsValid, char* oDisplayName) {
	char name[100];
	char value[1000];
	parseOptionalParameterPaths(tParameter, name, value);
	turnStringLowercase(name);

	if (!strcmp("order", name)) {
		if (oOrder) *oOrder = atoi(value);
	}
	else if (!strcmp("music", name)) {
		if (oMusicPath) strcpy(oMusicPath, value);
	}
	else if (!strcmp("includestage", name)) {
		if (oDoesIncludeStage) *oDoesIncludeStage = atoi(value);
	}
	else if (!strcmp("dependency", name)) {
		if (oIsValid) {
			auto assignment = parseDreamMugenAssignmentFromString(value);
			*oIsValid = evaluateDreamAssignment(&assignment, NULL);
			destroyDreamMugenAssignment(assignment);
		}
	}
	else if (!strcmp("displayname", name)) {
		if (oDisplayName) strcpy(oDisplayName, value);
	}
}

void parseOptionalCharacterSelectParameters(const MugenStringVector& tVector, int* oOrder, int* oDoesIncludeStage, char* oMusicPath, int* oIsValid, char* oDisplayName) {
	int i;
	for (i = 2; i < tVector.mSize; i++) {
		parseSingleOptionalCharacterParameter(tVector.mElement[i], oOrder, oDoesIncludeStage, oMusicPath, oIsValid, oDisplayName);
	}
}

void getCharacterSelectNamePath(const char* tName, char* oDst) {
	if (strchr(tName, '.')) {
		if (!strcmp("zip", getFileExtension(tName))) {
			logWarningFormat("No support for zipped characters. Error loading %s.", tName);
			*oDst = '\0';
		}
		else sprintf(oDst, "%schars/%s", getDolmexicaAssetFolder().c_str(), tName);
	}
	else sprintf(oDst, "%schars/%s/%s.def", getDolmexicaAssetFolder().c_str(), tName, tName);
}

typedef struct {
	char mPath[1024];

} PossibleRandomCharacterElement;

typedef struct {
	Vector mElements; // contains SurvivalCharacter

} RandomCharacterCaller;

static void loadRandomCharacter(RandomCharacterCaller* tCaller, const char* tName) {
	PossibleRandomCharacterElement* e = (PossibleRandomCharacterElement*)allocMemory(sizeof(PossibleRandomCharacterElement));
	getCharacterSelectNamePath(tName, e->mPath);
	if (!isFile(e->mPath)) {
		freeMemory(e);
		return;
	}

	vector_push_back_owned(&tCaller->mElements, e);
}

static void loadSingleRandomCharacter(void* tCaller, void* tData) {

	RandomCharacterCaller* caller = (RandomCharacterCaller*)tCaller;
	MugenDefScriptGroupElement* element = (MugenDefScriptGroupElement*)tData;
	if (element->mType == MUGEN_DEF_SCRIPT_GROUP_VECTOR_ELEMENT) {
		MugenDefScriptVectorElement* vectorElement = (MugenDefScriptVectorElement*)element->mData;
		loadRandomCharacter(caller, vectorElement->mVector.mElement[0]);
	}
	else if (element->mType == MUGEN_DEF_SCRIPT_GROUP_STRING_ELEMENT) {
		MugenDefScriptStringElement* stringElement = (MugenDefScriptStringElement*)element->mData;
		if (strcmp("randomselect", stringElement->mString)) {
			loadRandomCharacter(caller, stringElement->mString);
		}
	}
}

int setCharacterRandomAndReturnIfSuccessful(MugenDefScript* tScript, int i)
{
	MugenDefScriptGroup* e = &tScript->mGroups["characters"];

	RandomCharacterCaller caller;
	caller.mElements = new_vector();

	list_map(&e->mOrderedElementList, loadSingleRandomCharacter, &caller);

	if (!vector_size(&caller.mElements)) {
		delete_vector(&caller.mElements);
		return 0;
	}

	int index = randfromInteger(0, vector_size(&caller.mElements) - 1);
	PossibleRandomCharacterElement* newChar = (PossibleRandomCharacterElement*)vector_get(&caller.mElements, index);
	setPlayerDefinitionPath(i, newChar->mPath);

	delete_vector(&caller.mElements);
	return 1;
}

typedef struct {
	char mPath[1024];

} PossibleRandomStageElement;

typedef struct {
	Vector mElements; // contains SurvivalCharacter
	StringMap mAllElements; // contains NULL
} RandomStageCaller;

static void addPossibleRandomStage(RandomStageCaller* tCaller, char* tPath) {
	if (!strcmp("random", tPath)) return;
	if (string_map_contains(&tCaller->mAllElements, tPath)) return;

	PossibleRandomStageElement* e = (PossibleRandomStageElement*)allocMemory(sizeof(PossibleRandomStageElement));
	getStagePath(e->mPath, tPath);

	string_map_push(&tCaller->mAllElements, tPath, NULL);
	vector_push_back_owned(&tCaller->mElements, e);
}

static void loadRandomCharacterStage(RandomStageCaller* tCaller, MugenDefScriptVectorElement* tVectorElement) {
	if (tVectorElement->mVector.mSize < 2) return;

	addPossibleRandomStage(tCaller, tVectorElement->mVector.mElement[1]);
}

static void loadSingleRandomCharacterStage(void* tCaller, void* tData) {

	RandomStageCaller* caller = (RandomStageCaller*)tCaller;
	MugenDefScriptGroupElement* element = (MugenDefScriptGroupElement*)tData;
	if (element->mType == MUGEN_DEF_SCRIPT_GROUP_VECTOR_ELEMENT) {
		MugenDefScriptVectorElement* vectorElement = (MugenDefScriptVectorElement*)element->mData;
		loadRandomCharacterStage(caller, vectorElement);
	}
}

static void loadSingleRandomStageStage(void* tCaller, void* tData) {

	RandomStageCaller* caller = (RandomStageCaller*)tCaller;
	MugenDefScriptGroupElement* element = (MugenDefScriptGroupElement*)tData;
	if (element->mType == MUGEN_DEF_SCRIPT_GROUP_STRING_ELEMENT) {
		MugenDefScriptStringElement* stringElement = (MugenDefScriptStringElement*)element->mData;
		addPossibleRandomStage(caller, stringElement->mString);
	}
}

void setStageRandom(MugenDefScript * tScript)
{
	RandomStageCaller caller;
	caller.mElements = new_vector();
	caller.mAllElements = new_string_map();

	MugenDefScriptGroup* e = &tScript->mGroups["characters"];
	list_map(&e->mOrderedElementList, loadSingleRandomCharacterStage, &caller);

	e = &tScript->mGroups["extrastages"];
	list_map(&e->mOrderedElementList, loadSingleRandomStageStage, &caller);

	if (!vector_size(&caller.mElements)) {
		PossibleRandomStageElement* defaultStageElement = (PossibleRandomStageElement*)allocMemory(sizeof(PossibleRandomStageElement));
		getStagePath(defaultStageElement->mPath, getMugenConfigStartStage().c_str());
		vector_push_back_owned(&caller.mElements, defaultStageElement);
	}

	int index = randfromInteger(0, vector_size(&caller.mElements) - 1);
	PossibleRandomStageElement* newStage = (PossibleRandomStageElement*)vector_get(&caller.mElements, index);
	char dummyMusicPath[2];
	*dummyMusicPath = '\0';
	setDreamStageMugenDefinition(newStage->mPath, dummyMusicPath);

	delete_vector(&caller.mElements);
	delete_string_map(&caller.mAllElements);
}
