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
#include "menubackground.h"
#include "titlescreen.h"
#include "storyscreen.h"
#include "playerdefinition.h"
#include "gamelogic.h"
#include "stage.h"
#include "storymode.h"


using namespace std;

typedef struct {
	Vector3DI mCursorStartCell;
	MugenAnimation* mActiveCursorAnimation;
	MugenAnimation* mDoneCursorAnimation;

	MugenAnimation* mBigPortraitAnimation;
	Position mBigPortraitOffset;
	Vector3D mBigPortraitScale;
	int mBigPortraitIsFacingRight;

	Position mNameOffset;
	Vector3DI mNameFont;
	double mNameWidth;

	Vector3DI mCursorMoveSound;
	Vector3DI mCursorDoneSound;
	Vector3DI mRandomMoveSound;
} PlayerHeader;

typedef struct {
	Position mPosition;
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

	Position mPosition;
	int mIsShowingEmptyBoxes;
	int mCanMoveOverEmptyBoxes;

	Vector3D mCellSize;
	double mCellSpacing;
	
	MugenAnimation* mCellBackgroundAnimation;
	MugenAnimation* mRandomSelectionAnimation;
	int mRandomPortraitSwitchTime;

	PlayerHeader mPlayers[2];

	MugenAnimation* mSmallPortraitAnimation;
	Position mSmallPortraitOffset;
	Vector3D mSmallPortraitScale;

	Position mTitleOffset;
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
	
	Vector3DI mCellPosition;
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
	Vector3DI mSelectedCharacter;
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


	SelectScreenHeader mHeader;
	ModeText mModeText;

	TextureData mWhiteTexture;
	char mModeName[200];
	void(*mCB)();

	int mCharacterAmount;
	Vector mSelectCharacters; // vector of vectors of SelectCharacter
	Vector mSelectStages; // vector of SelectStage
	Vector mRealSelectCharacters; // vector of SelectCharacter of type character
	int mSelectorAmount;
	Selector mSelectors[2];

	StageSelectData mStageSelect;
	SelectCredits mCredits;

	CharacterSelectScreenType mSelectScreenType;

	int mCanSelectThisFrame;
	int mIsReturnDisabled;
	int mIsFadingOut;
} gCharacterSelectScreenData;

static MugenAnimation* getMugenDefMenuAnimationOrSprite(MugenDefScript* tScript, const char* tGroupName, const char* tVariableName) {
	char fullVariableName[200];
	sprintf(fullVariableName, "%s.anim", tVariableName);

	if (isMugenDefNumberVariable(tScript, tGroupName, fullVariableName)) {
		return getMugenAnimation(&gCharacterSelectScreenData.mAnimations, getMugenDefNumberVariable(tScript, tGroupName, fullVariableName));
	}

	sprintf(fullVariableName, "%s.spr", tVariableName);
	Vector3DI ret = getMugenDefVectorIOrDefault(tScript, tGroupName, fullVariableName, makeVector3DI(0, 0, 0));
	return createOneFrameMugenAnimationForSprite(ret.x, ret.y);
}

static void loadMenuPlayerHeader(const char* tPlayerName, PlayerHeader* tHeader) {
	char fullVariableName[200];

	sprintf(fullVariableName, "%s.cursor.startcell", tPlayerName);
	tHeader->mCursorStartCell = getMugenDefVectorIOrDefault(&gCharacterSelectScreenData.mScript, "Select Info", fullVariableName, makeVector3DI(0, 0, 0));

	sprintf(fullVariableName, "%s.cursor.active", tPlayerName);
	tHeader->mActiveCursorAnimation = getMugenDefMenuAnimationOrSprite(&gCharacterSelectScreenData.mScript, "Select Info", fullVariableName);

	sprintf(fullVariableName, "%s.cursor.done", tPlayerName);
	tHeader->mDoneCursorAnimation = getMugenDefMenuAnimationOrSprite(&gCharacterSelectScreenData.mScript, "Select Info", fullVariableName);

	sprintf(fullVariableName, "%s.face.spr", tPlayerName);
	Vector3DI bigPortraitSprite = getMugenDefVectorIOrDefault(&gCharacterSelectScreenData.mScript, "Select Info", fullVariableName, makeVector3DI(9000, 1, 0));
	tHeader->mBigPortraitAnimation = createOneFrameMugenAnimationForSprite(bigPortraitSprite.x, bigPortraitSprite.y);

	sprintf(fullVariableName, "%s.face.offset", tPlayerName);
	tHeader->mBigPortraitOffset = getMugenDefVectorOrDefault(&gCharacterSelectScreenData.mScript, "Select Info", fullVariableName, makePosition(0, 0, 0));

	sprintf(fullVariableName, "%s.face.scale", tPlayerName);
	tHeader->mBigPortraitScale = getMugenDefVectorOrDefault(&gCharacterSelectScreenData.mScript, "Select Info", fullVariableName, makePosition(0, 0, 0));

	sprintf(fullVariableName, "%s.face.facing", tPlayerName);
	int faceDirection = getMugenDefIntegerOrDefault(&gCharacterSelectScreenData.mScript, "Select Info", fullVariableName, 1);
	tHeader->mBigPortraitIsFacingRight = faceDirection == 1;

	sprintf(fullVariableName, "%s.name.offset", tPlayerName);
	tHeader->mNameOffset = getMugenDefVectorOrDefault(&gCharacterSelectScreenData.mScript, "Select Info", fullVariableName, makePosition(0, 0, 0));

	sprintf(fullVariableName, "%s.name.font", tPlayerName);
	tHeader->mNameFont = getMugenDefVectorIOrDefault(&gCharacterSelectScreenData.mScript, "Select Info", fullVariableName, makeVector3DI(1, 0, 0));

	sprintf(fullVariableName, "%s.name.width", tPlayerName);
	tHeader->mNameWidth = getMugenDefFloatOrDefault(&gCharacterSelectScreenData.mScript, "Select Info", fullVariableName, INF);

	sprintf(fullVariableName, "%s.cursor.move.snd", tPlayerName);
	tHeader->mCursorMoveSound = getMugenDefVectorIOrDefault(&gCharacterSelectScreenData.mScript, "Select Info", fullVariableName, makeVector3DI(0, 0, 0));

	sprintf(fullVariableName, "%s.cursor.done.snd", tPlayerName);
	tHeader->mCursorDoneSound = getMugenDefVectorIOrDefault(&gCharacterSelectScreenData.mScript, "Select Info", fullVariableName, makeVector3DI(0, 0, 0));

	sprintf(fullVariableName, "%s.random.move.snd", tPlayerName);
	tHeader->mRandomMoveSound = getMugenDefVectorIOrDefault(&gCharacterSelectScreenData.mScript, "Select Info", fullVariableName, makeVector3DI(0, 0, 0));
}

static void loadStageSelectHeader() {
	gCharacterSelectScreenData.mHeader.mStageSelect.mPosition = getMugenDefVectorOrDefault(&gCharacterSelectScreenData.mScript, "Select Info", "stage.pos", makePosition(0, 0, 0));
	gCharacterSelectScreenData.mHeader.mStageSelect.mActiveFont1 = getMugenDefVectorIOrDefault(&gCharacterSelectScreenData.mScript, "Select Info", "stage.active.font", makeVector3DI(1, 0, 0));
	gCharacterSelectScreenData.mHeader.mStageSelect.mActiveFont2 = getMugenDefVectorIOrDefault(&gCharacterSelectScreenData.mScript, "Select Info", "stage.active2.font", gCharacterSelectScreenData.mHeader.mStageSelect.mActiveFont1);
	gCharacterSelectScreenData.mHeader.mStageSelect.mDoneFont = getMugenDefVectorIOrDefault(&gCharacterSelectScreenData.mScript, "Select Info", "stage.done.font", makeVector3DI(1, 0, 0));
}

static void loadStageSelect() {
	Position pos = gCharacterSelectScreenData.mHeader.mStageSelect.mPosition;
	pos.z = 40;
	gCharacterSelectScreenData.mStageSelect.mTextID = addMugenText(" ", pos, gCharacterSelectScreenData.mHeader.mStageSelect.mActiveFont1.x);

	gCharacterSelectScreenData.mStageSelect.mIsActive = 0;
}

static void loadSelectStageCredits(SelectStage* e, MugenDefScript* tScript) {
	if (gCharacterSelectScreenData.mSelectScreenType != CHARACTER_SELECT_SCREEN_TYPE_CREDITS) return;

	e->mCredits.mName = getAllocatedMugenDefStringOrDefault(tScript, "Info", "name", e->mName);
	e->mCredits.mAuthorName = getAllocatedMugenDefStringOrDefault(tScript, "Info", "author", "N/A");
	e->mCredits.mVersionDate = getAllocatedMugenDefStringOrDefault(tScript, "Info", "versiondate", "N/A");
}

static int isSelectStageLoadedAlready(char* tPath) {
	int i;
	for (i = 0; i < vector_size(&gCharacterSelectScreenData.mSelectStages); i++) {
		SelectStage* e = (SelectStage*)vector_get(&gCharacterSelectScreenData.mSelectStages, i);
		if (!strcmp(tPath, e->mPath)) return 1;
	}

	return 0;
}

static void getStagePath(char* tDst, char* tPath) {
	sprintf(tDst, "assets/%s", tPath);
}

static int isInOsuModeAndNotOsuStage(MugenDefScript* tScript) {
	if (!gCharacterSelectScreenData.mStageSelect.mIsInOsuMode) return 0;
	const std::string fileName = getSTLMugenDefStringOrDefault(tScript, "Music", "bgmusic", "");
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
		e->mName = getAllocatedMugenDefStringVariable(&script, "Info", "name");
		loadSelectStageCredits(e, &script);
		vector_push_back_owned(&gCharacterSelectScreenData.mSelectStages, e);
	}
	unloadMugenDefScript(script);
}

static void loadExtraStages() {
	if (!gCharacterSelectScreenData.mStageSelect.mIsUsing) return;

	MugenDefScriptGroup* group = &gCharacterSelectScreenData.mCharacterScript.mGroups["ExtraStages"];
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

static void loadStageSelectStages() {
	if (!gCharacterSelectScreenData.mStageSelect.mIsUsing) return;

	gCharacterSelectScreenData.mSelectStages = new_vector();
}


static void loadCreditsHeader() {
	if (gCharacterSelectScreenData.mSelectScreenType != CHARACTER_SELECT_SCREEN_TYPE_CREDITS) return;

	Position basePosition = getMugenDefVectorOrDefault(&gCharacterSelectScreenData.mScript, "Select Info", "credits.position", makePosition(0, 0, 0));
	basePosition.z = 40;

	Position offset = makePosition(0, 0, 0);
	offset = getMugenDefVectorOrDefault(&gCharacterSelectScreenData.mScript, "Select Info", "credits.name.offset", makePosition(0, 0, 0));
	gCharacterSelectScreenData.mCredits.mNamePosition = vecAdd2D(basePosition, offset);
	gCharacterSelectScreenData.mCredits.mNameFont = getMugenDefVectorIOrDefault(&gCharacterSelectScreenData.mScript, "Select Info", "credits.name.font", makeVector3DI(1, 0, 0));

	offset = getMugenDefVectorOrDefault(&gCharacterSelectScreenData.mScript, "Select Info", "credits.author.offset", makePosition(0, 0, 0));
	gCharacterSelectScreenData.mCredits.mAuthorNamePosition = vecAdd2D(basePosition, offset);
	gCharacterSelectScreenData.mCredits.mAuthorNameFont = getMugenDefVectorIOrDefault(&gCharacterSelectScreenData.mScript, "Select Info", "credits.author.font", makeVector3DI(1, 0, 0));

	offset = getMugenDefVectorOrDefault(&gCharacterSelectScreenData.mScript, "Select Info", "credits.versiondate.offset", makePosition(0, 0, 0));
	gCharacterSelectScreenData.mCredits.mVersionDatePosition = vecAdd2D(basePosition, offset);
	gCharacterSelectScreenData.mCredits.mVersionDateFont = getMugenDefVectorIOrDefault(&gCharacterSelectScreenData.mScript, "Select Info", "credits.versiondate.font", makeVector3DI(1, 0, 0));

}

static void loadMenuHeader() {
	gCharacterSelectScreenData.mHeader.mFadeInTime = getMugenDefIntegerOrDefault(&gCharacterSelectScreenData.mScript, "Select Info", "fadein.time", 30);
	gCharacterSelectScreenData.mHeader.mFadeOutTime = getMugenDefIntegerOrDefault(&gCharacterSelectScreenData.mScript, "Select Info", "fadeout.time", 30);

	gCharacterSelectScreenData.mHeader.mRows = getMugenDefIntegerOrDefault(&gCharacterSelectScreenData.mScript, "Select Info", "rows", 1);
	gCharacterSelectScreenData.mHeader.mColumns = getMugenDefIntegerOrDefault(&gCharacterSelectScreenData.mScript, "Select Info", "columns", 1);
	gCharacterSelectScreenData.mHeader.mIsWrapping = getMugenDefIntegerOrDefault(&gCharacterSelectScreenData.mScript, "Select Info", "wrapping", 0);
	gCharacterSelectScreenData.mHeader.mPosition = getMugenDefVectorOrDefault(&gCharacterSelectScreenData.mScript, "Select Info", "pos", makePosition(0,0,0));

	gCharacterSelectScreenData.mHeader.mIsShowingEmptyBoxes = getMugenDefIntegerOrDefault(&gCharacterSelectScreenData.mScript, "Select Info", "showemptyboxes", 1);
	gCharacterSelectScreenData.mHeader.mCanMoveOverEmptyBoxes = getMugenDefIntegerOrDefault(&gCharacterSelectScreenData.mScript, "Select Info", "moveoveremptyboxes", 1);

	gCharacterSelectScreenData.mHeader.mCellSize = getMugenDefVectorOrDefault(&gCharacterSelectScreenData.mScript, "Select Info", "cell.size", makePosition(1, 1, 0));
	gCharacterSelectScreenData.mHeader.mCellSpacing = getMugenDefFloatOrDefault(&gCharacterSelectScreenData.mScript, "Select Info", "cell.spacing", 10);

	gCharacterSelectScreenData.mHeader.mCellBackgroundAnimation = getMugenDefMenuAnimationOrSprite(&gCharacterSelectScreenData.mScript, "Select Info", "cell.bg");
	gCharacterSelectScreenData.mHeader.mRandomSelectionAnimation = getMugenDefMenuAnimationOrSprite(&gCharacterSelectScreenData.mScript, "Select Info", "cell.random");
	gCharacterSelectScreenData.mHeader.mRandomPortraitSwitchTime = getMugenDefIntegerOrDefault(&gCharacterSelectScreenData.mScript, "Select Info", "cell.random.switchtime", 1);

	Vector3DI sprite = getMugenDefVectorIOrDefault(&gCharacterSelectScreenData.mScript, "Select Info", "portrait.spr", makeVector3DI(9000, 0, 0));
	gCharacterSelectScreenData.mHeader.mSmallPortraitAnimation = createOneFrameMugenAnimationForSprite(sprite.x, sprite.y);

	gCharacterSelectScreenData.mHeader.mSmallPortraitOffset = getMugenDefVectorOrDefault(&gCharacterSelectScreenData.mScript, "Select Info", "portrait.offset", makePosition(0, 0, 0));
	gCharacterSelectScreenData.mHeader.mSmallPortraitScale = getMugenDefVectorOrDefault(&gCharacterSelectScreenData.mScript, "Select Info", "portrait.scale", makePosition(1, 1, 0));

	gCharacterSelectScreenData.mHeader.mTitleOffset = getMugenDefVectorOrDefault(&gCharacterSelectScreenData.mScript, "Select Info", "title.offset", makePosition(0, 0, 0));
	gCharacterSelectScreenData.mHeader.mTitleFont = getMugenDefVectorIOrDefault(&gCharacterSelectScreenData.mScript, "Select Info", "title.font", makeVector3DI(-1, 0, 0));

	loadCreditsHeader();
	loadStageSelectHeader();

	loadMenuPlayerHeader("p1", &gCharacterSelectScreenData.mHeader.mPlayers[0]);
	loadMenuPlayerHeader("p2", &gCharacterSelectScreenData.mHeader.mPlayers[1]);
}

typedef struct {
	int i;

} MenuCharacterLoadCaller;

static void loadMenuCharacterCredits(SelectCharacter* e, MugenDefScript* tScript) {
	if (gCharacterSelectScreenData.mSelectScreenType != CHARACTER_SELECT_SCREEN_TYPE_CREDITS) return;

	e->mCredits.mName = getAllocatedMugenDefStringOrDefault(tScript, "Info", "name", e->mDisplayCharacterName);
	e->mCredits.mAuthorName = getAllocatedMugenDefStringOrDefault(tScript, "Info", "author", "N/A");
	e->mCredits.mVersionDate = getAllocatedMugenDefStringOrDefault(tScript, "Info", "versiondate", "N/A");
}

static int loadMenuCharacterSpritesAndNameAndReturnWhetherExists(SelectCharacter* e, char* tCharacterName) {
	
	char file[200];
	char path[1024];
	char scriptPath[1024];
	char scriptPathPreloaded[1024];
	char name[100];
	char palettePath[1024];


	getCharacterSelectNamePath(tCharacterName, scriptPath);
	if (!isFile(scriptPath)) {
		return 0;
	}
	MugenDefScript script;
	loadMugenDefScript(&script, scriptPath);

	getPathToFile(path, scriptPath);

	int preferredPalette = 0;
	sprintf(name, "pal%d", preferredPalette + 1);
	getMugenDefStringOrDefault(file, &script, "Files", name, "");
	int hasPalettePath = strcmp("", file);
	sprintf(palettePath, "%s%s", path, file);

	getMugenDefStringOrDefault(file, &script, "Files", "sprite", "");
	assert(strcmp("", file));
	sprintf(scriptPath, "%s%s", path, file);
	sprintf(scriptPathPreloaded, "%s.portraits.preloaded", scriptPath);
	if (isFile(scriptPathPreloaded)) {
		e->mSprites = loadMugenSpriteFilePortraits(scriptPathPreloaded, hasPalettePath, palettePath);
	}
	else {
		e->mSprites = loadMugenSpriteFilePortraits(scriptPath, hasPalettePath, palettePath);
	}

	strcpy(e->mCharacterName, tCharacterName);
	e->mDisplayCharacterName = getAllocatedMugenDefStringVariable(&script, "Info", "displayname");

	e->mType = SELECT_CHARACTER_TYPE_CHARACTER;
	
	loadMenuCharacterCredits(e, &script);
	
	unloadMugenDefScript(script);
	
	vector_push_back(&gCharacterSelectScreenData.mRealSelectCharacters, e);
	return 1;
}

static Position getCellScreenPosition(Vector3DI tCellPosition) {
	double dx = tCellPosition.x * (gCharacterSelectScreenData.mHeader.mCellSpacing + gCharacterSelectScreenData.mHeader.mCellSize.x);
	double dy = tCellPosition.y * (gCharacterSelectScreenData.mHeader.mCellSpacing + gCharacterSelectScreenData.mHeader.mCellSize.y);
	Position pos = makePosition(dx, dy, 0);
	pos = vecAdd(gCharacterSelectScreenData.mHeader.mPosition, pos);
	return pos;
}

static void showMenuSelectableAnimations(MenuCharacterLoadCaller* /*tCaller*/, SelectCharacter* e) {
	Position pos = getCellScreenPosition(e->mCellPosition);
	pos.z = 40;
	e->mPortraitAnimationElement = addMugenAnimation(gCharacterSelectScreenData.mHeader.mSmallPortraitAnimation, &e->mSprites, pos);
	if (!gCharacterSelectScreenData.mHeader.mIsShowingEmptyBoxes) {
		pos.z = 30;
		e->mBackgroundAnimationElement = addMugenAnimation(gCharacterSelectScreenData.mHeader.mCellBackgroundAnimation, &gCharacterSelectScreenData.mSprites, pos);
	}
}

static void loadSingleRealMenuCharacter(MenuCharacterLoadCaller* tCaller, MugenDefScriptVectorElement* tVectorElement) {
	assert(tVectorElement->mVector.mSize >= 2);
	char* characterName = tVectorElement->mVector.mElement[0];
	char* stageName = tVectorElement->mVector.mElement[1];

	int row = tCaller->i / gCharacterSelectScreenData.mHeader.mColumns;
	int column = tCaller->i % gCharacterSelectScreenData.mHeader.mColumns;

	SelectCharacter* e = (SelectCharacter*)vector_get((Vector*)vector_get(&gCharacterSelectScreenData.mSelectCharacters, row), column);
	strcpy(e->mStageName, stageName);

	tCaller->i++;
	if (!loadMenuCharacterSpritesAndNameAndReturnWhetherExists(e, characterName)) {
		return;
	}

	if (gCharacterSelectScreenData.mStageSelect.mIsUsing) {
		int doesIncludeStage = 1;
		parseOptionalCharacterSelectParameters(tVectorElement->mVector, NULL, &doesIncludeStage, NULL);

		if (doesIncludeStage) {
			addSingleSelectStage(stageName);
		}
	}

	showMenuSelectableAnimations(tCaller, e);
}

static void loadSingleRandomMenuCharacter(MenuCharacterLoadCaller* tCaller) {
	int row = tCaller->i / gCharacterSelectScreenData.mHeader.mColumns;
	int column = tCaller->i % gCharacterSelectScreenData.mHeader.mColumns;

	SelectCharacter* e = (SelectCharacter*)vector_get((Vector*)vector_get(&gCharacterSelectScreenData.mSelectCharacters, row), column);

	e->mType = SELECT_CHARACTER_TYPE_RANDOM;

	Position pos = getCellScreenPosition(e->mCellPosition);
	pos.z = 40;
	e->mPortraitAnimationElement = addMugenAnimation(gCharacterSelectScreenData.mHeader.mRandomSelectionAnimation, &gCharacterSelectScreenData.mSprites, pos);
	if (!gCharacterSelectScreenData.mHeader.mIsShowingEmptyBoxes) {
		pos.z = 30;
		e->mBackgroundAnimationElement = addMugenAnimation(gCharacterSelectScreenData.mHeader.mCellBackgroundAnimation, &gCharacterSelectScreenData.mSprites, pos);
	}

	tCaller->i++;
}

static void loadSingleSpecialMenuCharacter(MenuCharacterLoadCaller* tCaller, MugenDefScriptStringElement* tStringElement) {
	if (!strcmp("randomselect", tStringElement->mString)) {
		loadSingleRandomMenuCharacter(tCaller);
	}
}

static void loadSingleMenuCharacter(void* tCaller, void* tData) {

	MenuCharacterLoadCaller* caller = (MenuCharacterLoadCaller*)tCaller;
	MugenDefScriptGroupElement* element = (MugenDefScriptGroupElement*)tData;
	if (element->mType == MUGEN_DEF_SCRIPT_GROUP_VECTOR_ELEMENT) {
		MugenDefScriptVectorElement* vectorElement = (MugenDefScriptVectorElement*)element->mData;
		loadSingleRealMenuCharacter(caller, vectorElement);
	} else if (element->mType == MUGEN_DEF_SCRIPT_GROUP_STRING_ELEMENT) {
		MugenDefScriptStringElement* stringElement = (MugenDefScriptStringElement*)element->mData;
		loadSingleSpecialMenuCharacter(caller, stringElement);
	}
	
	
}

static void loadMenuCharacters() {
	MugenDefScriptGroup* e = &gCharacterSelectScreenData.mCharacterScript.mGroups["Characters"];

	MenuCharacterLoadCaller caller;
	caller.i = 0;

	list_map(&e->mOrderedElementList, loadSingleMenuCharacter, &caller);
	gCharacterSelectScreenData.mCharacterAmount = caller.i;
}

static int loadSingleStoryFileAndReturnWhetherItExists(SelectCharacter* e, char* tPath) {
	char file[200];
	char path[1024];
	char scriptPath[1024];

	sprintf(scriptPath, "assets/%s", tPath);
	if (!isFile(scriptPath)) {
		return 0;
	}
	MugenDefScript script;
	loadMugenDefScript(&script, scriptPath);

	getPathToFile(path, scriptPath);

	getMugenDefStringOrDefault(file, &script, "Info", "select.spr", "");
	assert(strcmp("", file));
	sprintf(scriptPath, "%s%s", path, file);
	e->mSprites = loadMugenSpriteFileWithoutPalette(scriptPath);

	strcpy(e->mCharacterName, tPath);
	e->mDisplayCharacterName = getAllocatedMugenDefStringVariable(&script, "Info", "name");

	e->mType = SELECT_CHARACTER_TYPE_CHARACTER;

	unloadMugenDefScript(script);

	vector_push_back(&gCharacterSelectScreenData.mRealSelectCharacters, e);
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
	SelectCharacter* e = (SelectCharacter*)vector_get((Vector*)vector_get(&gCharacterSelectScreenData.mSelectCharacters, row), column);
	strcpy(e->mStageName, "");
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
	MugenDefScriptGroup* e = &gCharacterSelectScreenData.mCharacterScript.mGroups["Stories"];

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

static void loadSingleMenuCell(Vector3DI tCellPosition) {
	SelectCharacter* e = (SelectCharacter*)allocMemory(sizeof(SelectCharacter));
	e->mType = SELECT_CHARACTER_TYPE_EMPTY;
	e->mCellPosition = tCellPosition;

	if (gCharacterSelectScreenData.mHeader.mIsShowingEmptyBoxes) {
		Position pos = getCellScreenPosition(e->mCellPosition);
		pos.z = 30;
		e->mBackgroundAnimationElement = addMugenAnimation(gCharacterSelectScreenData.mHeader.mCellBackgroundAnimation, &gCharacterSelectScreenData.mSprites, pos);
	}

	vector_push_back_owned((Vector*)vector_get(&gCharacterSelectScreenData.mSelectCharacters, tCellPosition.y), e);
}

static void loadMenuCells() {
	gCharacterSelectScreenData.mSelectCharacters = new_vector();
	gCharacterSelectScreenData.mRealSelectCharacters = new_vector();
	
	int y, x;
	for (y = 0; y < gCharacterSelectScreenData.mHeader.mRows; y++) {
		Vector* v = (Vector*)allocMemory(sizeof(Vector));
		*v = new_vector();
		vector_push_back_owned(&gCharacterSelectScreenData.mSelectCharacters, v);

		for (x = 0; x < gCharacterSelectScreenData.mHeader.mColumns; x++) {
			loadSingleMenuCell(makeVector3DI(x, y, 0));
		}
	}
}

static SelectCharacter* getCellCharacter(Vector3DI tCellPosition) {
	SelectCharacter* ret = (SelectCharacter*)vector_get((Vector*)vector_get(&gCharacterSelectScreenData.mSelectCharacters, tCellPosition.y), tCellPosition.x);
	return ret;
}

static void moveSelectionToTarget(int i, Vector3DI tTarget, int tDoesPlaySound);

static Vector3DI increaseCellPositionWithDirection(const Vector3DI& pos, int tDelta) {
	Vector3DI ret = pos;
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

static Vector3DI findStartCellPosition(const Vector3DI startPos, int delta, int forceValid = 0) {
	Vector3DI pos = startPos;

	int maxLength = gCharacterSelectScreenData.mHeader.mRows * gCharacterSelectScreenData.mHeader.mColumns;
	for (int i = 0; i < maxLength; i++) {
		SelectCharacter* selectChar = getCellCharacter(pos);
		if ((gCharacterSelectScreenData.mHeader.mCanMoveOverEmptyBoxes && !forceValid) || selectChar->mType != SELECT_CHARACTER_TYPE_EMPTY) return pos;

		pos = increaseCellPositionWithDirection(pos, delta);
	}

	return startPos;
}


static void loadSingleSelector(int i, int tOwner) {
	PlayerHeader* player = &gCharacterSelectScreenData.mHeader.mPlayers[i];
	PlayerHeader* owner = &gCharacterSelectScreenData.mHeader.mPlayers[tOwner];

	if (!gCharacterSelectScreenData.mSelectors[i].mHasBeenLoadedBefore) {
		gCharacterSelectScreenData.mSelectors[i].mSelectedCharacter = makeVector3DI(player->mCursorStartCell.y, player->mCursorStartCell.x, 0);
	}
	gCharacterSelectScreenData.mSelectors[i].mSelectedCharacter = findStartCellPosition(gCharacterSelectScreenData.mSelectors[i].mSelectedCharacter, i == 0 ? 1 : -1);

	Position p = getCellScreenPosition(makeVector3DI(gCharacterSelectScreenData.mSelectors[i].mSelectedCharacter.y, gCharacterSelectScreenData.mSelectors[i].mSelectedCharacter.x, 0));
	p.z = 60;
	gCharacterSelectScreenData.mSelectors[i].mSelectorAnimationElement = addMugenAnimation(owner->mActiveCursorAnimation, &gCharacterSelectScreenData.mSprites, p);
	
	SelectCharacter* character = getCellCharacter(findStartCellPosition(gCharacterSelectScreenData.mSelectors[i].mSelectedCharacter, 1, 1));
	player->mBigPortraitOffset.z = 30;
	gCharacterSelectScreenData.mSelectors[i].mBigPortraitAnimationElement = addMugenAnimation(player->mBigPortraitAnimation, &character->mSprites, player->mBigPortraitOffset);
	setMugenAnimationFaceDirection(gCharacterSelectScreenData.mSelectors[i].mBigPortraitAnimationElement, player->mBigPortraitIsFacingRight);
	setMugenAnimationDrawScale(gCharacterSelectScreenData.mSelectors[i].mBigPortraitAnimationElement, makePosition(player->mBigPortraitScale.x, player->mBigPortraitScale.y, 1));

	player->mNameOffset.z = 40;
	gCharacterSelectScreenData.mSelectors[i].mNameTextID = addMugenText(character->mDisplayCharacterName, player->mNameOffset, player->mNameFont.x);
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
	removeMugenAnimation(gCharacterSelectScreenData.mSelectors[i].mBigPortraitAnimationElement);
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
		loadSingleSelector(i, i);
	}
}

static void loadModeText() {
	Position p = gCharacterSelectScreenData.mHeader.mTitleOffset;
	p.z = 50;
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
	char* path = getAllocatedMugenDefStringOrDefault(&gCharacterSelectScreenData.mScript, "Music", "select.bgm", " ");
	int isLooping = getMugenDefIntegerOrDefault(&gCharacterSelectScreenData.mScript, "Music", "select.bgm.loop", 1);

	if (isMugenBGMMusicPath(path)) {
		playMugenBGMMusicPath(path, isLooping);
	}

	freeMemory(path);
}

static int isValidSelectPath(const std::string& path, std::string& oFinalSelectPath) {
	if (path.empty()) return 0;

	if (isFile("assets/data/" + path)) {
		oFinalSelectPath = "assets/data/" + path;
		return 1;
	}
	if (isFile(path)) {
		oFinalSelectPath = path;
		return 1;
	}

	return 0;
}

static int checkIfCharactersSane() {
	return vector_size(&gCharacterSelectScreenData.mRealSelectCharacters);
}

static int checkIfStagesSane() {
	if (!gCharacterSelectScreenData.mStageSelect.mIsUsing) return 1;
	return vector_size(&gCharacterSelectScreenData.mSelectStages);
}

static void setStageSelectorSane() {
	if (!gCharacterSelectScreenData.mStageSelect.mIsUsing) return;
	gCharacterSelectScreenData.mStageSelect.mSelectedStage = min(gCharacterSelectScreenData.mStageSelect.mSelectedStage, vector_size(&gCharacterSelectScreenData.mSelectStages) - 1);
}

static void sanityCheck() {
	if (!checkIfCharactersSane() || !checkIfStagesSane()) {
		setNewScreen(getDreamTitleScreen());
	}
	else {
		setStageSelectorSane();
	}
}

static void loadCharacterSelectScreen() {

	string selectPath;
	if (!isValidSelectPath(gCharacterSelectScreenData.mCustomSelectFilePath, selectPath)) {
		selectPath = "assets/data/select.def";
	}
	else gCharacterSelectScreenData.mCustomSelectFilePath = "";

	loadMugenDefScript(&gCharacterSelectScreenData.mCharacterScript, selectPath);

	char folder[1024];
	loadMugenDefScript(&gCharacterSelectScreenData.mScript, "assets/data/system.def");
	gCharacterSelectScreenData.mAnimations = loadMugenAnimationFile("assets/data/system.def");
	getPathToFile(folder, "assets/data/system.def");
	setWorkingDirectory(folder);

	char* text = getAllocatedMugenDefStringVariable(&gCharacterSelectScreenData.mScript, "Files", "spr");
	gCharacterSelectScreenData.mSprites = loadMugenSpriteFileWithoutPalette(text);
	freeMemory(text);

	text = getAllocatedMugenDefStringVariable(&gCharacterSelectScreenData.mScript, "Files", "snd");
	gCharacterSelectScreenData.mSounds = loadMugenSoundFile(text);
	freeMemory(text);

	loadMenuHeader();
	loadMenuBackground(&gCharacterSelectScreenData.mScript, &gCharacterSelectScreenData.mSprites, &gCharacterSelectScreenData.mAnimations, "SelectBGdef", "SelectBG");

	setWorkingDirectory("/");

	loadStageSelectStages();
	loadMenuCells();
	loadMenuSelectables();
	loadExtraStages();

	loadCredits();
	loadSelectors();
	loadModeText();
	loadStageSelect();
	loadSelectMusic();

	gCharacterSelectScreenData.mWhiteTexture = createWhiteTexture();

	gCharacterSelectScreenData.mIsFadingOut = 0;
	addFadeIn(gCharacterSelectScreenData.mHeader.mFadeInTime, NULL, NULL);
	sanityCheck();
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

static void unloadMenuCharacterCredits(SelectCharacter* e) {
	if (gCharacterSelectScreenData.mSelectScreenType != CHARACTER_SELECT_SCREEN_TYPE_CREDITS) return;

	freeMemory(e->mCredits.mName); 
	freeMemory(e->mCredits.mAuthorName);
	freeMemory(e->mCredits.mVersionDate);
}

static void unloadSingleSelectCharacter(void* tCaller, void* tData) {
	(void)tCaller;
	SelectCharacter* e = (SelectCharacter*)tData;
	if (e->mType == SELECT_CHARACTER_TYPE_CHARACTER) {
		unloadMugenSpriteFile(&e->mSprites);
		freeMemory(e->mDisplayCharacterName);
		unloadMenuCharacterCredits(e);
	}
}

static void unloadSingleSelectCharacterRow(void* tCaller, void* tData) {
	(void)tCaller;
	Vector* e = (Vector*)tData;
	vector_map(e, unloadSingleSelectCharacter, NULL);
	delete_vector(e);
}


static void unloadSelectCharacters() {
	delete_vector(&gCharacterSelectScreenData.mRealSelectCharacters);
	vector_map(&gCharacterSelectScreenData.mSelectCharacters, unloadSingleSelectCharacterRow, NULL);
	delete_vector(&gCharacterSelectScreenData.mSelectCharacters);
}

static void resetSelectState() {
	gCharacterSelectScreenData.mIsReturnDisabled = 0;
}

static void unloadCharacterSelectScreen() {
	unloadMugenDefScript(gCharacterSelectScreenData.mScript);
	unloadMugenDefScript(gCharacterSelectScreenData.mCharacterScript);

	unloadMugenSpriteFile(&gCharacterSelectScreenData.mSprites);
	unloadMugenAnimationFile(&gCharacterSelectScreenData.mAnimations);
	unloadMugenSoundFile(&gCharacterSelectScreenData.mSounds);

	destroyMugenAnimation(gCharacterSelectScreenData.mHeader.mCellBackgroundAnimation);
	destroyMugenAnimation(gCharacterSelectScreenData.mHeader.mRandomSelectionAnimation);

	int i;
	for (i = 0; i < 2; i++) {
		destroyMugenAnimation(gCharacterSelectScreenData.mHeader.mPlayers[i].mActiveCursorAnimation);
		destroyMugenAnimation(gCharacterSelectScreenData.mHeader.mPlayers[i].mDoneCursorAnimation);
		destroyMugenAnimation(gCharacterSelectScreenData.mHeader.mPlayers[i].mBigPortraitAnimation);
	}

	destroyMugenAnimation(gCharacterSelectScreenData.mHeader.mSmallPortraitAnimation);

	unloadTexture(gCharacterSelectScreenData.mWhiteTexture);

	unloadSelectCharacters();
	unloadSelectStages();

	resetSelectState();
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

static Vector3DI findTargetCellPosition(int i, Vector3DI tDelta) {
	
	Vector3DI startPos = gCharacterSelectScreenData.mSelectors[i].mSelectedCharacter;
	Vector3DI pos = vecAddI(startPos, tDelta);
	Vector3DI prevPos = startPos;


	int maxLength = max(gCharacterSelectScreenData.mHeader.mRows, gCharacterSelectScreenData.mHeader.mColumns);
	for (int j = 0; j < maxLength; j++) {
		handleSingleWrapping(&pos.x, gCharacterSelectScreenData.mHeader.mColumns);
		handleSingleWrapping(&pos.y, gCharacterSelectScreenData.mHeader.mRows);
		SelectCharacter* selectChar = getCellCharacter(pos);
		if (gCharacterSelectScreenData.mHeader.mCanMoveOverEmptyBoxes || selectChar->mType != SELECT_CHARACTER_TYPE_EMPTY) return pos;

		if (vecEqualsI(pos, prevPos) || vecEqualsI(pos, startPos)) return startPos;
		prevPos = pos;
		pos = vecAddI(pos, tDelta);
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
		setMugenAnimationBaseDrawScale(gCharacterSelectScreenData.mSelectors[i].mBigPortraitAnimationElement, 1);
		setMugenAnimationSprites(gCharacterSelectScreenData.mSelectors[i].mBigPortraitAnimationElement, &tCharacter->mSprites);
		changeMugenAnimation(gCharacterSelectScreenData.mSelectors[i].mBigPortraitAnimationElement, player->mBigPortraitAnimation);
		changeMugenText(gCharacterSelectScreenData.mSelectors[i].mNameTextID, tCharacter->mDisplayCharacterName);
	}
	else {
		setMugenAnimationBaseDrawScale(gCharacterSelectScreenData.mSelectors[i].mBigPortraitAnimationElement, 0);
		changeMugenText(gCharacterSelectScreenData.mSelectors[i].mNameTextID, " ");
	}

	updateCharacterSelectionCredits(tCharacter);
}

static void showNewRandomSelectCharacter(int i);

static void moveSelectionToTarget(int i, Vector3DI tTarget, int tDoesPlaySound) {
	PlayerHeader* owner = &gCharacterSelectScreenData.mHeader.mPlayers[gCharacterSelectScreenData.mSelectors[i].mOwner];

	gCharacterSelectScreenData.mSelectors[i].mSelectedCharacter = tTarget;
	Position p = getCellScreenPosition(tTarget);
	p.z = 60;
	setMugenAnimationPosition(gCharacterSelectScreenData.mSelectors[i].mSelectorAnimationElement, p);

	if (tDoesPlaySound) {
		tryPlayMugenSound(&gCharacterSelectScreenData.mSounds, owner->mCursorMoveSound.x, owner->mCursorMoveSound.y);
	}

	SelectCharacter* character = getCellCharacter(gCharacterSelectScreenData.mSelectors[i].mSelectedCharacter);
	if (character->mType != SELECT_CHARACTER_TYPE_RANDOM) {
		showSelectCharacterForSelector(i, character);
	}
	else {
		showNewRandomSelectCharacter(i);
	}
}

static int hasCreditSelectionMovedToStage(int i, Vector3DI tDelta) {
	if (gCharacterSelectScreenData.mSelectScreenType != CHARACTER_SELECT_SCREEN_TYPE_CREDITS) return 0;

	Vector3DI target = vecAddI(gCharacterSelectScreenData.mSelectors[i].mSelectedCharacter, tDelta);
	return (target.y == -1 || target.y == gCharacterSelectScreenData.mHeader.mRows);
}

static void setStageSelectActive(int i);

static void moveCreditSelectionToStage(int i) {
	setStageSelectActive(i);
	unloadSingleSelector(i);
}

static void updateSingleSelectionInput(int i) {
	Vector3DI delta = makeVector3DI(0, 0, 0);
	if (hasPressedRightFlankSingle(gCharacterSelectScreenData.mSelectors[i].mOwner)) {
		delta = makeVector3DI(1, 0, 0);
	}
	else if (hasPressedLeftFlankSingle(gCharacterSelectScreenData.mSelectors[i].mOwner)) {
		delta = makeVector3DI(-1, 0, 0);
	}

	if (hasPressedDownFlankSingle(gCharacterSelectScreenData.mSelectors[i].mOwner)) {
		delta = makeVector3DI(0, 1, 0);
	}
	else if (hasPressedUpFlankSingle(gCharacterSelectScreenData.mSelectors[i].mOwner)) {
		delta = makeVector3DI(0, -1, 0);
	}

	if (hasCreditSelectionMovedToStage(i, delta)) {
		moveCreditSelectionToStage(i);
		return;
	}

	Vector3DI target = findTargetCellPosition(i, delta);
	

	
	if (vecEqualsI(gCharacterSelectScreenData.mSelectors[i].mSelectedCharacter, target)) return;

	moveSelectionToTarget(i, target, 1);
}

static void showNewRandomSelectCharacter(int i) {
	int amount = vector_size(&gCharacterSelectScreenData.mRealSelectCharacters);
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
	SelectCharacter* e = (SelectCharacter*)vector_get(&gCharacterSelectScreenData.mRealSelectCharacters, gCharacterSelectScreenData.mSelectors[i].mRandom.mCurrentCharacter);
	showSelectCharacterForSelector(i, e);
	gCharacterSelectScreenData.mSelectors[i].mRandom.mNow = 0;
}

static void updateSingleSelectionRandom(int i) {
	SelectCharacter* character = getCellCharacter(gCharacterSelectScreenData.mSelectors[i].mSelectedCharacter);
	if (character->mType != SELECT_CHARACTER_TYPE_RANDOM) return;

	gCharacterSelectScreenData.mSelectors[i].mRandom.mNow++;
	if (gCharacterSelectScreenData.mSelectors[i].mRandom.mNow >= gCharacterSelectScreenData.mHeader.mRandomPortraitSwitchTime) {
		showNewRandomSelectCharacter(i);
		tryPlayMugenSound(&gCharacterSelectScreenData.mSounds, gCharacterSelectScreenData.mHeader.mPlayers[i].mRandomMoveSound.x, gCharacterSelectScreenData.mHeader.mPlayers[i].mRandomMoveSound.y);
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
	if (gCharacterSelectScreenData.mSelectScreenType == CHARACTER_SELECT_SCREEN_TYPE_ONE_PLAYER_SELECTS_EVERYTHING && i == 1) return;
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
		updateSingleSelection(i);
		updateSingleStageSelection(i);
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
	Position pos = getCellScreenPosition(gCharacterSelectScreenData.mSelectors[i].mSelectedCharacter);
	pos.z = 41;
	Animation anim = createAnimation(1, 2);
	AnimationHandlerElement* element = playAnimation(pos, &gCharacterSelectScreenData.mWhiteTexture, anim, makeRectangleFromTexture(gCharacterSelectScreenData.mWhiteTexture), NULL, NULL);
	setAnimationSize(element, makePosition(25, 25, 1), makePosition(0, 0, 0));
}

static void updateMugenTextBasedOnVector3DI(int tID, Vector3DI tFontData) {
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

static void updateStageSelection(int i, int tNewStage, int tDoesPlaySound) {
	SelectStage* stage = (SelectStage*)vector_get(&gCharacterSelectScreenData.mSelectStages, tNewStage);

	char newText[200];
	sprintf(newText, "Stage %d: %s", tNewStage + 1, stage->mName);
	changeMugenText(gCharacterSelectScreenData.mStageSelect.mTextID, newText);
	updateStageCredit(stage);

	if (tDoesPlaySound) {
		tryPlayMugenSound(&gCharacterSelectScreenData.mSounds, gCharacterSelectScreenData.mHeader.mPlayers[i].mCursorMoveSound.x, gCharacterSelectScreenData.mHeader.mPlayers[i].mCursorMoveSound.y);
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
		tryPlayMugenSound(&gCharacterSelectScreenData.mSounds, owner->mCursorDoneSound.x, owner->mCursorDoneSound.y);
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
	tryPlayMugenSound(&gCharacterSelectScreenData.mSounds, owner->mCursorDoneSound.x, owner->mCursorDoneSound.y);

	if (character->mType == SELECT_CHARACTER_TYPE_RANDOM) {
		character = (SelectCharacter*)vector_get(&gCharacterSelectScreenData.mRealSelectCharacters, gCharacterSelectScreenData.mSelectors[i].mRandom.mCurrentCharacter);
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

	tryPlayMugenSound(&gCharacterSelectScreenData.mSounds, gCharacterSelectScreenData.mHeader.mPlayers[i].mCursorDoneSound.x, gCharacterSelectScreenData.mHeader.mPlayers[i].mCursorDoneSound.y);

	SelectStage* stage = (SelectStage*)vector_get(&gCharacterSelectScreenData.mSelectStages, gCharacterSelectScreenData.mStageSelect.mSelectedStage);
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
	if(tDoesPlaySound) tryPlayMugenSound(&gCharacterSelectScreenData.mSounds, owner->mCursorDoneSound.x, owner->mCursorDoneSound.y);

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
	gCharacterSelectScreenData.mSelectScreenType = CHARACTER_SELECT_SCREEN_TYPE_ONE_PLAYER_MODE;
}

void setCharacterSelectOnePlayerSelectAll()
{
	gCharacterSelectScreenData.mSelectScreenType = CHARACTER_SELECT_SCREEN_TYPE_ONE_PLAYER_SELECTS_EVERYTHING;
}

void setCharacterSelectTwoPlayers()
{
	gCharacterSelectScreenData.mSelectScreenType = CHARACTER_SELECT_SCREEN_TYPE_TWO_PLAYER_MODE;

}

void setCharacterSelectCredits()
{
	gCharacterSelectScreenData.mSelectScreenType = CHARACTER_SELECT_SCREEN_TYPE_CREDITS;
}

void setCharacterSelectStory()
{
	gCharacterSelectScreenData.mSelectScreenType = CHARACTER_SELECT_SCREEN_TYPE_STORY;
}

void setCharacterSelectDisableReturnOneTime()
{
	gCharacterSelectScreenData.mIsReturnDisabled = 1;
}

static char* removeEmptyParameterSpace(char* p, int delta, char* startChar) {
	while (*p == ' ' && p > startChar) p += delta;

	return p;
}

static int parseSingleOptionalParameterPart(char* tStart, char* tEnd, char* tParameter, char* oDst) {
	tStart = removeEmptyParameterSpace(tStart, 1, tParameter);
	tEnd = removeEmptyParameterSpace(tEnd, -1, tParameter);

	int length = tEnd - tStart + 1;
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

static void parseSingleOptionalCharacterParameter(char* tParameter, int* oOrder, int* oDoesIncludeStage, char* oMusicPath) {
	char name[100];
	char value[100];
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
}

void parseOptionalCharacterSelectParameters(MugenStringVector tVector, int* oOrder, int* oDoesIncludeStage, char* oMusicPath) {
	int i;
	for (i = 2; i < tVector.mSize; i++) {
		parseSingleOptionalCharacterParameter(tVector.mElement[i], oOrder, oDoesIncludeStage, oMusicPath);
	}
}

void getCharacterSelectNamePath(const char* tName, char* oDst) {
	if (strchr(tName, '.')) {
		if (strcmp("zip", getFileExtension(tName))) {
			logWarningFormat("No support for zipped characters. Error loading %s.", tName);
			*oDst = '\0';
		}
		else sprintf(oDst, "assets/chars/%s", tName);
	}
	else sprintf(oDst, "assets/chars/%s/%s.def", tName, tName);
}

typedef struct {
	char mPath[1024];

} PossibleRandomCharacterElement;

typedef struct {
	Vector mElements; // contains SurvivalCharacter

} RandomCharacterCaller;

static void loadRandomCharacter(RandomCharacterCaller* tCaller, MugenDefScriptVectorElement* tVectorElement) {
	PossibleRandomCharacterElement* e = (PossibleRandomCharacterElement*)allocMemory(sizeof(PossibleRandomCharacterElement));
	getCharacterSelectNamePath(tVectorElement->mVector.mElement[0], e->mPath);
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
		loadRandomCharacter(caller, vectorElement);
	}
}

void setCharacterRandom(MugenDefScript * tScript, int i)
{
	MugenDefScriptGroup* e = &tScript->mGroups["Characters"];

	RandomCharacterCaller caller;
	caller.mElements = new_vector();

	list_map(&e->mOrderedElementList, loadSingleRandomCharacter, &caller);

	int index = randfromInteger(0, vector_size(&caller.mElements) - 1);
	PossibleRandomCharacterElement* newChar = (PossibleRandomCharacterElement*)vector_get(&caller.mElements, index);
	setPlayerDefinitionPath(i, newChar->mPath);

	delete_vector(&caller.mElements);
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

	MugenDefScriptGroup* e = &tScript->mGroups["Characters"];
	list_map(&e->mOrderedElementList, loadSingleRandomCharacterStage, &caller);

	e = &tScript->mGroups["ExtraStages"];
	list_map(&e->mOrderedElementList, loadSingleRandomStageStage, &caller);


	int index = randfromInteger(0, vector_size(&caller.mElements) - 1);
	PossibleRandomStageElement* newStage = (PossibleRandomStageElement*)vector_get(&caller.mElements, index);
	char dummyMusicPath[2];
	*dummyMusicPath = '\0';
	setDreamStageMugenDefinition(newStage->mPath, dummyMusicPath);

	delete_vector(&caller.mElements);
	delete_string_map(&caller.mAllElements);
}
