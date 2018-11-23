#include "characterselectscreen.h"

#include <assert.h>

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
	int mBackgroundAnimationID;

	MugenSpriteFile mSprites;
	int mPortraitAnimationID;
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

	int mSelectorAnimationID;
	int mBigPortraitAnimationID;
	int mNameTextID;

	Vector3DI mSelectedCharacter;
	RandomSelector mRandom;
} Selector;

typedef struct {
	int mModeTextID;

} ModeText;

typedef struct {
	int mIsUsing;
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

	int mIsFadingOut;
} gData;

static MugenAnimation* getMugenDefMenuAnimationOrSprite(MugenDefScript* tScript, char* tGroupName, char* tVariableName) {
	char fullVariableName[200];
	sprintf(fullVariableName, "%s.anim", tVariableName);

	if (isMugenDefNumberVariable(tScript, tGroupName, fullVariableName)) {
		return getMugenAnimation(&gData.mAnimations, getMugenDefNumberVariable(tScript, tGroupName, fullVariableName));
	}

	sprintf(fullVariableName, "%s.spr", tVariableName);
	Vector3DI ret = getMugenDefVectorIOrDefault(tScript, tGroupName, fullVariableName, makeVector3DI(0, 0, 0));
	return createOneFrameMugenAnimationForSprite(ret.x, ret.y);
}

static void loadMenuPlayerHeader(char* tPlayerName, PlayerHeader* tHeader) {
	char fullVariableName[200];

	sprintf(fullVariableName, "%s.cursor.startcell", tPlayerName);
	tHeader->mCursorStartCell = getMugenDefVectorIOrDefault(&gData.mScript, "Select Info", fullVariableName, makeVector3DI(0, 0, 0));

	sprintf(fullVariableName, "%s.cursor.active", tPlayerName);
	tHeader->mActiveCursorAnimation = getMugenDefMenuAnimationOrSprite(&gData.mScript, "Select Info", fullVariableName);

	sprintf(fullVariableName, "%s.cursor.done", tPlayerName);
	tHeader->mDoneCursorAnimation = getMugenDefMenuAnimationOrSprite(&gData.mScript, "Select Info", fullVariableName);

	sprintf(fullVariableName, "%s.face.spr", tPlayerName);
	Vector3DI bigPortraitSprite = getMugenDefVectorIOrDefault(&gData.mScript, "Select Info", fullVariableName, makeVector3DI(9000, 1, 0));
	tHeader->mBigPortraitAnimation = createOneFrameMugenAnimationForSprite(bigPortraitSprite.x, bigPortraitSprite.y);

	sprintf(fullVariableName, "%s.face.offset", tPlayerName);
	tHeader->mBigPortraitOffset = getMugenDefVectorOrDefault(&gData.mScript, "Select Info", fullVariableName, makePosition(0, 0, 0));

	sprintf(fullVariableName, "%s.face.scale", tPlayerName);
	tHeader->mBigPortraitScale = getMugenDefVectorOrDefault(&gData.mScript, "Select Info", fullVariableName, makePosition(0, 0, 0));

	sprintf(fullVariableName, "%s.face.facing", tPlayerName);
	int faceDirection = getMugenDefIntegerOrDefault(&gData.mScript, "Select Info", fullVariableName, 1);
	tHeader->mBigPortraitIsFacingRight = faceDirection == 1;

	sprintf(fullVariableName, "%s.name.offset", tPlayerName);
	tHeader->mNameOffset = getMugenDefVectorOrDefault(&gData.mScript, "Select Info", fullVariableName, makePosition(0, 0, 0));

	sprintf(fullVariableName, "%s.name.font", tPlayerName);
	tHeader->mNameFont = getMugenDefVectorIOrDefault(&gData.mScript, "Select Info", fullVariableName, makeVector3DI(1, 0, 0));

	sprintf(fullVariableName, "%s.cursor.move.snd", tPlayerName);
	tHeader->mCursorMoveSound = getMugenDefVectorIOrDefault(&gData.mScript, "Select Info", fullVariableName, makeVector3DI(0, 0, 0));

	sprintf(fullVariableName, "%s.cursor.done.snd", tPlayerName);
	tHeader->mCursorDoneSound = getMugenDefVectorIOrDefault(&gData.mScript, "Select Info", fullVariableName, makeVector3DI(0, 0, 0));

	sprintf(fullVariableName, "%s.random.move.snd", tPlayerName);
	tHeader->mRandomMoveSound = getMugenDefVectorIOrDefault(&gData.mScript, "Select Info", fullVariableName, makeVector3DI(0, 0, 0));
}

static void loadStageSelectHeader() {
	gData.mHeader.mStageSelect.mPosition = getMugenDefVectorOrDefault(&gData.mScript, "Select Info", "stage.pos", makePosition(0, 0, 0));
	gData.mHeader.mStageSelect.mActiveFont1 = getMugenDefVectorIOrDefault(&gData.mScript, "Select Info", "stage.active.font", makeVector3DI(1, 0, 0));
	gData.mHeader.mStageSelect.mActiveFont2 = getMugenDefVectorIOrDefault(&gData.mScript, "Select Info", "stage.active2.font", gData.mHeader.mStageSelect.mActiveFont1);
	gData.mHeader.mStageSelect.mDoneFont = getMugenDefVectorIOrDefault(&gData.mScript, "Select Info", "stage.done.font", makeVector3DI(1, 0, 0));
}

static void loadStageSelect() {
	Position pos = gData.mHeader.mStageSelect.mPosition;
	pos.z = 40;
	gData.mStageSelect.mTextID = addMugenText(" ", pos, gData.mHeader.mStageSelect.mActiveFont1.x);

	gData.mStageSelect.mIsActive = 0;
}

static void loadSelectStageCredits(SelectStage* e, MugenDefScript* tScript) {
	if (gData.mSelectScreenType != CHARACTER_SELECT_SCREEN_TYPE_CREDITS) return;

	e->mCredits.mName = getAllocatedMugenDefStringOrDefault(tScript, "Info", "name", e->mName);
	e->mCredits.mAuthorName = getAllocatedMugenDefStringOrDefault(tScript, "Info", "author", "N/A");
	e->mCredits.mVersionDate = getAllocatedMugenDefStringOrDefault(tScript, "Info", "versiondate", "N/A");
}

static int isSelectStageLoadedAlready(char* tPath) {
	int i;
	for (i = 0; i < vector_size(&gData.mSelectStages); i++) {
		SelectStage* e = (SelectStage*)vector_get(&gData.mSelectStages, i);
		if (!strcmp(tPath, e->mPath)) return 1;
	}

	return 0;
}

static void getStagePath(char* tDst, char* tPath) {
	sprintf(tDst, "assets/%s", tPath);
}

static void addSingleSelectStage(char* tPath) {
	char path[1024];
	getStagePath(path, tPath);

	if (!isFile(path)) {
		logWarningFormat("Unable to find stage file %s. Ignoring.", path);
		return;
	}

	if (isSelectStageLoadedAlready(path)) return;

	SelectStage* e = (SelectStage*)allocMemory(sizeof(SelectStage));
	strcpy(e->mPath, path);
	
	MugenDefScript script = loadMugenDefScript(path);
	e->mName = getAllocatedMugenDefStringVariable(&script, "Info", "name");
	loadSelectStageCredits(e, &script);
	unloadMugenDefScript(script);
	
	vector_push_back_owned(&gData.mSelectStages, e);
}

static void loadExtraStages() {
	if (!gData.mStageSelect.mIsUsing) return;

	MugenDefScriptGroup* group = (MugenDefScriptGroup*)string_map_get(&gData.mCharacterScript.mGroups, "ExtraStages");
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
	if (!gData.mStageSelect.mIsUsing) return;

	gData.mSelectStages = new_vector();
}


static void loadCreditsHeader() {
	if (gData.mSelectScreenType != CHARACTER_SELECT_SCREEN_TYPE_CREDITS) return;

	Position basePosition = getMugenDefVectorOrDefault(&gData.mScript, "Select Info", "credits.position", makePosition(0, 0, 0));
	basePosition.z = 40;

	Position offset = makePosition(0, 0, 0);
	offset = getMugenDefVectorOrDefault(&gData.mScript, "Select Info", "credits.name.offset", makePosition(0, 0, 0));
	gData.mCredits.mNamePosition = vecAdd2D(basePosition, offset);
	gData.mCredits.mNameFont = getMugenDefVectorIOrDefault(&gData.mScript, "Select Info", "credits.name.font", makeVector3DI(1, 0, 0));

	offset = getMugenDefVectorOrDefault(&gData.mScript, "Select Info", "credits.author.offset", makePosition(0, 0, 0));
	gData.mCredits.mAuthorNamePosition = vecAdd2D(basePosition, offset);
	gData.mCredits.mAuthorNameFont = getMugenDefVectorIOrDefault(&gData.mScript, "Select Info", "credits.author.font", makeVector3DI(1, 0, 0));

	offset = getMugenDefVectorOrDefault(&gData.mScript, "Select Info", "credits.versiondate.offset", makePosition(0, 0, 0));
	gData.mCredits.mVersionDatePosition = vecAdd2D(basePosition, offset);
	gData.mCredits.mVersionDateFont = getMugenDefVectorIOrDefault(&gData.mScript, "Select Info", "credits.versiondate.font", makeVector3DI(1, 0, 0));

}

static void loadMenuHeader() {
	gData.mHeader.mFadeInTime = getMugenDefIntegerOrDefault(&gData.mScript, "Select Info", "fadein.time", 30);
	gData.mHeader.mFadeOutTime = getMugenDefIntegerOrDefault(&gData.mScript, "Select Info", "fadeout.time", 30);

	gData.mHeader.mRows = getMugenDefIntegerOrDefault(&gData.mScript, "Select Info", "rows", 1);
	gData.mHeader.mColumns = getMugenDefIntegerOrDefault(&gData.mScript, "Select Info", "columns", 1);
	gData.mHeader.mIsWrapping = getMugenDefIntegerOrDefault(&gData.mScript, "Select Info", "wrapping", 0);
	gData.mHeader.mPosition = getMugenDefVectorOrDefault(&gData.mScript, "Select Info", "pos", makePosition(0,0,0));

	gData.mHeader.mIsShowingEmptyBoxes = getMugenDefIntegerOrDefault(&gData.mScript, "Select Info", "showemptyboxes", 1);
	gData.mHeader.mCanMoveOverEmptyBoxes = getMugenDefIntegerOrDefault(&gData.mScript, "Select Info", "moveoveremptyboxes", 1);

	gData.mHeader.mCellSize = getMugenDefVectorOrDefault(&gData.mScript, "Select Info", "cell.size", makePosition(1, 1, 0));
	gData.mHeader.mCellSpacing = getMugenDefFloatOrDefault(&gData.mScript, "Select Info", "cell.spacing", 10);

	gData.mHeader.mCellBackgroundAnimation = getMugenDefMenuAnimationOrSprite(&gData.mScript, "Select Info", "cell.bg");
	gData.mHeader.mRandomSelectionAnimation = getMugenDefMenuAnimationOrSprite(&gData.mScript, "Select Info", "cell.random");
	gData.mHeader.mRandomPortraitSwitchTime = getMugenDefIntegerOrDefault(&gData.mScript, "Select Info", "cell.random.switchtime", 1);

	Vector3DI sprite = getMugenDefVectorIOrDefault(&gData.mScript, "Select Info", "portrait.spr", makeVector3DI(9000, 0, 0));
	gData.mHeader.mSmallPortraitAnimation = createOneFrameMugenAnimationForSprite(sprite.x, sprite.y);

	gData.mHeader.mSmallPortraitOffset = getMugenDefVectorOrDefault(&gData.mScript, "Select Info", "portrait.offset", makePosition(0, 0, 0));
	gData.mHeader.mSmallPortraitScale = getMugenDefVectorOrDefault(&gData.mScript, "Select Info", "portrait.scale", makePosition(1, 1, 0));

	gData.mHeader.mTitleOffset = getMugenDefVectorOrDefault(&gData.mScript, "Select Info", "title.offset", makePosition(0, 0, 0));
	gData.mHeader.mTitleFont = getMugenDefVectorIOrDefault(&gData.mScript, "Select Info", "title.font", makeVector3DI(-1, 0, 0));

	loadCreditsHeader();
	loadStageSelectHeader();

	loadMenuPlayerHeader("p1", &gData.mHeader.mPlayers[0]);
	loadMenuPlayerHeader("p2", &gData.mHeader.mPlayers[1]);
}

typedef struct {
	int i;

} MenuCharacterLoadCaller;

static void loadMenuCharacterCredits(SelectCharacter* e, MugenDefScript* tScript) {
	if (gData.mSelectScreenType != CHARACTER_SELECT_SCREEN_TYPE_CREDITS) return;

	e->mCredits.mName = getAllocatedMugenDefStringOrDefault(tScript, "Info", "name", e->mDisplayCharacterName);
	e->mCredits.mAuthorName = getAllocatedMugenDefStringOrDefault(tScript, "Info", "author", "N/A");
	e->mCredits.mVersionDate = getAllocatedMugenDefStringOrDefault(tScript, "Info", "versiondate", "N/A");
}

static int loadMenuCharacterSpritesAndNameAndReturnWhetherExists(SelectCharacter* e, char* tCharacterName) {
	
	char file[200];
	char path[1024];
	char scriptPath[1024];
	char name[100];
	char palettePath[1024];


	MugenDefScript script;
	getCharacterSelectNamePath(tCharacterName, scriptPath);
	if (!isFile(scriptPath)) {
		logWarningFormat("Unable to find file %s. Cannot load character %s. Ignoring.", scriptPath, tCharacterName);
		return 0;
	}
	script = loadMugenDefScript(scriptPath);

	getPathToFile(path, scriptPath);

	int preferredPalette = 0;
	sprintf(name, "pal%d", preferredPalette + 1);
	getMugenDefStringOrDefault(file, &script, "Files", name, "");
	int hasPalettePath = strcmp("", file);
	sprintf(palettePath, "%s%s", path, file);
	printf("%s\n", palettePath);

	getMugenDefStringOrDefault(file, &script, "Files", "sprite", "");
	assert(strcmp("", file));
	sprintf(scriptPath, "%s%s", path, file);
	e->mSprites = loadMugenSpriteFilePortraits(scriptPath, preferredPalette, hasPalettePath, palettePath);

	strcpy(e->mCharacterName, tCharacterName);
	e->mDisplayCharacterName = getAllocatedMugenDefStringVariable(&script, "Info", "displayname");

	e->mType = SELECT_CHARACTER_TYPE_CHARACTER;
	
	loadMenuCharacterCredits(e, &script);
	
	unloadMugenDefScript(script);
	
	vector_push_back(&gData.mRealSelectCharacters, e);
	return 1;
}

static Position getCellScreenPosition(Vector3DI tCellPosition) {
	double dx = tCellPosition.x * (gData.mHeader.mCellSpacing + gData.mHeader.mCellSize.x);
	double dy = tCellPosition.y * (gData.mHeader.mCellSpacing + gData.mHeader.mCellSize.y);
	Position pos = makePosition(dx, dy, 0);
	pos = vecAdd(gData.mHeader.mPosition, pos);
	return pos;
}

static void showMenuSelectableAnimations(MenuCharacterLoadCaller* tCaller, SelectCharacter* e) {
	Position pos = getCellScreenPosition(e->mCellPosition);
	pos.z = 40;
	e->mPortraitAnimationID = addMugenAnimation(gData.mHeader.mSmallPortraitAnimation, &e->mSprites, pos);
	if (!gData.mHeader.mIsShowingEmptyBoxes) {
		pos.z = 30;
		e->mBackgroundAnimationID = addMugenAnimation(gData.mHeader.mCellBackgroundAnimation, &gData.mSprites, pos);
	}

	tCaller->i++;
}

static void loadSingleRealMenuCharacter(MenuCharacterLoadCaller* tCaller, MugenDefScriptVectorElement* tVectorElement) {
	assert(tVectorElement->mVector.mSize >= 2);
	char* characterName = tVectorElement->mVector.mElement[0];
	char* stageName = tVectorElement->mVector.mElement[1];

	int row = tCaller->i / gData.mHeader.mColumns;
	int column = tCaller->i % gData.mHeader.mColumns;

	SelectCharacter* e = (SelectCharacter*)vector_get((Vector*)vector_get(&gData.mSelectCharacters, row), column);
	strcpy(e->mStageName, stageName);
	if (!loadMenuCharacterSpritesAndNameAndReturnWhetherExists(e, characterName)) {
		return;
	}

	if (gData.mStageSelect.mIsUsing) {
		int doesIncludeStage = 1;
		parseOptionalCharacterSelectParameters(tVectorElement->mVector, NULL, &doesIncludeStage, NULL);

		if (doesIncludeStage) {
			addSingleSelectStage(stageName);
		}
	}

	showMenuSelectableAnimations(tCaller, e);
}

static void loadSingleRandomMenuCharacter(MenuCharacterLoadCaller* tCaller) {
	int row = tCaller->i / gData.mHeader.mColumns;
	int column = tCaller->i % gData.mHeader.mColumns;

	SelectCharacter* e = (SelectCharacter*)vector_get((Vector*)vector_get(&gData.mSelectCharacters, row), column);

	e->mType = SELECT_CHARACTER_TYPE_RANDOM;

	Position pos = getCellScreenPosition(e->mCellPosition);
	pos.z = 40;
	e->mPortraitAnimationID = addMugenAnimation(gData.mHeader.mRandomSelectionAnimation, &gData.mSprites, pos);
	if (!gData.mHeader.mIsShowingEmptyBoxes) {
		pos.z = 30;
		e->mBackgroundAnimationID = addMugenAnimation(gData.mHeader.mCellBackgroundAnimation, &gData.mSprites, pos);
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
	MugenDefScriptGroup* e = (MugenDefScriptGroup*)string_map_get(&gData.mCharacterScript.mGroups, "Characters");

	MenuCharacterLoadCaller caller;
	caller.i = 0;

	list_map(&e->mOrderedElementList, loadSingleMenuCharacter, &caller);
	gData.mCharacterAmount = caller.i;
}

static int loadSingleStoryFileAndReturnWhetherItExists(SelectCharacter* e, char* tPath) {
	char file[200];
	char path[1024];
	char scriptPath[1024];

	sprintf(scriptPath, "assets/%s", tPath);
	MugenDefScript script;
	if (!isFile(scriptPath)) {
		logWarningFormat("Unable to find file %s. Cannot load story %s. Ignoring.", scriptPath, tPath);
		return 0;
	}
	script = loadMugenDefScript(scriptPath);

	getPathToFile(path, scriptPath);

	getMugenDefStringOrDefault(file, &script, "Info", "select.spr", "");
	assert(strcmp("", file));
	sprintf(scriptPath, "%s%s", path, file);
	e->mSprites = loadMugenSpriteFileWithoutPalette(scriptPath);

	strcpy(e->mCharacterName, tPath);
	e->mDisplayCharacterName = getAllocatedMugenDefStringVariable(&script, "Info", "name");

	e->mType = SELECT_CHARACTER_TYPE_CHARACTER;

	unloadMugenDefScript(script);

	vector_push_back(&gData.mRealSelectCharacters, e);
	return 1;
}

static void loadSingleStory(MenuCharacterLoadCaller* tCaller, char* tPath) {
	if (!strcmp("randomselect", tPath)) {
		loadSingleRandomMenuCharacter(tCaller);
		return;
	}

	int row = tCaller->i / gData.mHeader.mColumns;
	int column = tCaller->i % gData.mHeader.mColumns;
	SelectCharacter* e = (SelectCharacter*)vector_get((Vector*)vector_get(&gData.mSelectCharacters, row), column);
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
	MugenDefScriptGroup* e = (MugenDefScriptGroup*)string_map_get(&gData.mCharacterScript.mGroups, "Stories");

	MenuCharacterLoadCaller caller;
	caller.i = 0;

	list_map(&e->mOrderedElementList, loadSingleMenuStory, &caller);
	gData.mCharacterAmount = caller.i;
}


static void loadMenuSelectables() {
	if (gData.mSelectScreenType != CHARACTER_SELECT_SCREEN_TYPE_STORY) {
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

	if (gData.mHeader.mIsShowingEmptyBoxes) {
		Position pos = getCellScreenPosition(e->mCellPosition);
		pos.z = 30;
		e->mBackgroundAnimationID = addMugenAnimation(gData.mHeader.mCellBackgroundAnimation, &gData.mSprites, pos);
	}

	vector_push_back_owned((Vector*)vector_get(&gData.mSelectCharacters, tCellPosition.y), e);
}

static void loadMenuCells() {
	gData.mSelectCharacters = new_vector();
	gData.mRealSelectCharacters = new_vector();
	
	int y, x;
	for (y = 0; y < gData.mHeader.mRows; y++) {
		Vector* v = (Vector*)allocMemory(sizeof(Vector));
		*v = new_vector();
		vector_push_back_owned(&gData.mSelectCharacters, v);

		for (x = 0; x < gData.mHeader.mColumns; x++) {
			loadSingleMenuCell(makeVector3DI(x, y, 0));
		}
	}
}

static SelectCharacter* getCellCharacter(Vector3DI tCellPosition) {
	SelectCharacter* ret = (SelectCharacter*)vector_get((Vector*)vector_get(&gData.mSelectCharacters, tCellPosition.y), tCellPosition.x);
	return ret;
}

static void moveSelectionToTarget(int i, Vector3DI tTarget, int tDoesPlaySound);

static void loadSingleSelector(int i, int tOwner) {
	PlayerHeader* player = &gData.mHeader.mPlayers[i];
	PlayerHeader* owner = &gData.mHeader.mPlayers[tOwner];

	Position p = getCellScreenPosition(player->mCursorStartCell);
	gData.mSelectors[i].mSelectedCharacter = makeVector3DI(player->mCursorStartCell.y, player->mCursorStartCell.x, 0);
	p.z = 60;
	gData.mSelectors[i].mSelectorAnimationID = addMugenAnimation(owner->mActiveCursorAnimation, &gData.mSprites, p);
	
	SelectCharacter* character = getCellCharacter(makeVector3DI(0, 0, 0));
	player->mBigPortraitOffset.z = 30;
	gData.mSelectors[i].mBigPortraitAnimationID = addMugenAnimation(player->mBigPortraitAnimation, &character->mSprites, player->mBigPortraitOffset);
	setMugenAnimationFaceDirection(gData.mSelectors[i].mBigPortraitAnimationID, player->mBigPortraitIsFacingRight);

	player->mNameOffset.z = 40;
	gData.mSelectors[i].mNameTextID = addMugenText(character->mDisplayCharacterName, player->mNameOffset, player->mNameFont.x);
	setMugenTextColor(gData.mSelectors[i].mNameTextID, getMugenTextColorFromMugenTextColorIndex(player->mNameFont.y));
	setMugenTextAlignment(gData.mSelectors[i].mNameTextID, getMugenTextAlignmentFromMugenAlignmentIndex(player->mNameFont.z));

	moveSelectionToTarget(i, gData.mSelectors[i].mSelectedCharacter, 0);

	gData.mSelectors[i].mOwner = tOwner;
	gData.mSelectors[i].mIsActive = 1;
	gData.mSelectors[i].mIsDone = 0;
}

static void unloadSingleSelector(int i) {
	removeMugenAnimation(gData.mSelectors[i].mSelectorAnimationID);
	removeMugenAnimation(gData.mSelectors[i].mBigPortraitAnimationID);
	removeMugenText(gData.mSelectors[i].mNameTextID);

	gData.mSelectors[i].mIsActive = 0;
}

static void loadSelectors() {

	int i;
	for (i = 0; i < 2; i++) {
		gData.mSelectors[i].mIsActive = 0;
	}

	gData.mSelectorAmount = gData.mSelectScreenType == CHARACTER_SELECT_SCREEN_TYPE_TWO_PLAYER_MODE ? 2 : 1;
	for (i = 0; i < gData.mSelectorAmount; i++) {
		loadSingleSelector(i, i);
	}
}

static void loadModeText() {
	Position p = gData.mHeader.mTitleOffset;
	p.z = 50;
	gData.mModeText.mModeTextID = addMugenText(gData.mModeName, p, gData.mHeader.mTitleFont.x); 
	setMugenTextColor(gData.mModeText.mModeTextID, getMugenTextColorFromMugenTextColorIndex(gData.mHeader.mTitleFont.y));
	setMugenTextAlignment(gData.mModeText.mModeTextID, getMugenTextAlignmentFromMugenAlignmentIndex(gData.mHeader.mTitleFont.z));
}

static void loadCredits() {
	if (gData.mSelectScreenType != CHARACTER_SELECT_SCREEN_TYPE_CREDITS) return;

	gData.mCredits.mNameID = addMugenText("", gData.mCredits.mNamePosition, gData.mCredits.mNameFont.x);
	setMugenTextColor(gData.mCredits.mNameID, getMugenTextColorFromMugenTextColorIndex(gData.mCredits.mNameFont.y));
	setMugenTextAlignment(gData.mCredits.mNameID, getMugenTextAlignmentFromMugenAlignmentIndex(gData.mCredits.mNameFont.z));

	gData.mCredits.mAuthorNameID = addMugenText("", gData.mCredits.mAuthorNamePosition, gData.mCredits.mAuthorNameFont.x);
	setMugenTextColor(gData.mCredits.mAuthorNameID, getMugenTextColorFromMugenTextColorIndex(gData.mCredits.mAuthorNameFont.y));
	setMugenTextAlignment(gData.mCredits.mAuthorNameID, getMugenTextAlignmentFromMugenAlignmentIndex(gData.mCredits.mAuthorNameFont.z));

	gData.mCredits.mVersionDateID = addMugenText("", gData.mCredits.mVersionDatePosition, gData.mCredits.mVersionDateFont.x);
	setMugenTextColor(gData.mCredits.mVersionDateID, getMugenTextColorFromMugenTextColorIndex(gData.mCredits.mVersionDateFont.y));
	setMugenTextAlignment(gData.mCredits.mVersionDateID, getMugenTextAlignmentFromMugenAlignmentIndex(gData.mCredits.mVersionDateFont.z));
}

static void loadSelectMusic() {
	char* path = getAllocatedMugenDefStringOrDefault(&gData.mScript, "Music", "select.bgm", " ");
	int isLooping = getMugenDefIntegerOrDefault(&gData.mScript, "Music", "select.bgm.loop", 1);

	if (isMugenBGMMusicPath(path)) {
		playMugenBGMMusicPath(path, isLooping);
	}

	freeMemory(path);
}

static void loadCharacterSelectScreen() {

	gData.mCharacterScript = loadMugenDefScript("assets/data/select.def");

	char folder[1024];
	gData.mScript = loadMugenDefScript("assets/data/system.def");
	gData.mAnimations = loadMugenAnimationFile("assets/data/system.def");
	getPathToFile(folder, "assets/data/system.def");
	setWorkingDirectory(folder);

	char* text = getAllocatedMugenDefStringVariable(&gData.mScript, "Files", "spr");
	gData.mSprites = loadMugenSpriteFileWithoutPalette(text);
	freeMemory(text);

	text = getAllocatedMugenDefStringVariable(&gData.mScript, "Files", "snd");
	gData.mSounds = loadMugenSoundFile(text);
	freeMemory(text);

	loadMenuHeader();
	loadMenuBackground(&gData.mScript, &gData.mSprites, &gData.mAnimations, "SelectBGdef", "SelectBG");

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

	gData.mWhiteTexture = createWhiteTexture();

	gData.mIsFadingOut = 0;
	addFadeIn(gData.mHeader.mFadeInTime, NULL, NULL);
}

static void unloadSelectStageCredits(SelectStage* e) {
	if (gData.mSelectScreenType != CHARACTER_SELECT_SCREEN_TYPE_CREDITS) return;

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
	if (!gData.mStageSelect.mIsUsing) return;

	vector_map(&gData.mSelectStages, unloadSingleSelectStage, NULL);
	delete_vector(&gData.mSelectStages);
}

static void unloadMenuCharacterCredits(SelectCharacter* e) {
	if (gData.mSelectScreenType != CHARACTER_SELECT_SCREEN_TYPE_CREDITS) return;

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
	delete_vector(&gData.mRealSelectCharacters);
	vector_map(&gData.mSelectCharacters, unloadSingleSelectCharacterRow, NULL);
	delete_vector(&gData.mSelectCharacters);
}


static void unloadCharacterSelectScreen() {
	unloadMugenDefScript(gData.mScript);
	unloadMugenDefScript(gData.mCharacterScript);

	unloadMugenSpriteFile(&gData.mSprites);
	unloadMugenAnimationFile(&gData.mAnimations);
	unloadMugenSoundFile(&gData.mSounds);

	destroyMugenAnimation(gData.mHeader.mCellBackgroundAnimation);
	destroyMugenAnimation(gData.mHeader.mRandomSelectionAnimation);

	int i;
	for (i = 0; i < 2; i++) {
		destroyMugenAnimation(gData.mHeader.mPlayers[i].mActiveCursorAnimation);
		destroyMugenAnimation(gData.mHeader.mPlayers[i].mDoneCursorAnimation);
		destroyMugenAnimation(gData.mHeader.mPlayers[i].mBigPortraitAnimation);
	}

	destroyMugenAnimation(gData.mHeader.mSmallPortraitAnimation);

	unloadTexture(gData.mWhiteTexture);

	unloadSelectCharacters();
	unloadSelectStages();

}

static void handleSingleWrapping(int* tPosition, int tSize) {
	if (*tPosition < 0) {
		if (gData.mHeader.mIsWrapping) {
			*tPosition += tSize;
		}
		else {
			*tPosition = 0;
		}
	}

	if (*tPosition > tSize - 1) {
		if (gData.mHeader.mIsWrapping) {
			*tPosition -= tSize;
		}
		else {
			*tPosition = tSize - 1;
		}
	}
}

static Vector3DI sanitizeCellPosition(int i, Vector3DI tCellPosition) {
	
	handleSingleWrapping(&tCellPosition.x, gData.mHeader.mColumns);
	handleSingleWrapping(&tCellPosition.y, gData.mHeader.mRows);

	int index = tCellPosition.y * gData.mHeader.mColumns + tCellPosition.x;
	if (index >= gData.mCharacterAmount && !gData.mHeader.mCanMoveOverEmptyBoxes) {
		return gData.mSelectors[i].mSelectedCharacter;
	}

	return tCellPosition;	
}

static void updateCharacterSelectionCredits(SelectCharacter* character) {
	if (gData.mSelectScreenType != CHARACTER_SELECT_SCREEN_TYPE_CREDITS) return;

	if (character->mType == SELECT_CHARACTER_TYPE_CHARACTER) {
		char text[1024];
		sprintf(text, "Name: %s", character->mCredits.mName);
		changeMugenText(gData.mCredits.mNameID, text);
		sprintf(text, "Author: %s", character->mCredits.mAuthorName);
		changeMugenText(gData.mCredits.mAuthorNameID, text);
		sprintf(text, "Date: %s", character->mCredits.mVersionDate);
		changeMugenText(gData.mCredits.mVersionDateID, text);
	}
	else {
		changeMugenText(gData.mCredits.mNameID, "Name:");
		changeMugenText(gData.mCredits.mAuthorNameID, "Author:");
		changeMugenText(gData.mCredits.mVersionDateID, "Date:");
	}
}

static void showSelectCharacterForSelector(int i, SelectCharacter* tCharacter) {
	PlayerHeader* player = &gData.mHeader.mPlayers[i];

	if (tCharacter->mType == SELECT_CHARACTER_TYPE_CHARACTER) {
		setMugenAnimationBaseDrawScale(gData.mSelectors[i].mBigPortraitAnimationID, 1);
		setMugenAnimationSprites(gData.mSelectors[i].mBigPortraitAnimationID, &tCharacter->mSprites);
		changeMugenAnimation(gData.mSelectors[i].mBigPortraitAnimationID, player->mBigPortraitAnimation);
		changeMugenText(gData.mSelectors[i].mNameTextID, tCharacter->mDisplayCharacterName);
	}
	else {
		setMugenAnimationBaseDrawScale(gData.mSelectors[i].mBigPortraitAnimationID, 0);
		changeMugenText(gData.mSelectors[i].mNameTextID, " ");
	}

	updateCharacterSelectionCredits(tCharacter);
}

static void showNewRandomSelectCharacter(int i);

static void moveSelectionToTarget(int i, Vector3DI tTarget, int tDoesPlaySound) {
	PlayerHeader* owner = &gData.mHeader.mPlayers[gData.mSelectors[i].mOwner];

	gData.mSelectors[i].mSelectedCharacter = tTarget;
	Position p = getCellScreenPosition(tTarget);
	p.z = 60;
	setMugenAnimationPosition(gData.mSelectors[i].mSelectorAnimationID, p);

	if (tDoesPlaySound) {
		tryPlayMugenSound(&gData.mSounds, owner->mCursorMoveSound.x, owner->mCursorMoveSound.y);
	}

	SelectCharacter* character = getCellCharacter(gData.mSelectors[i].mSelectedCharacter);
	if (character->mType != SELECT_CHARACTER_TYPE_RANDOM) {
		showSelectCharacterForSelector(i, character);
	}
	else {
		showNewRandomSelectCharacter(i);
	}
}

static int hasCreditSelectionMovedToStage(Vector3DI tTarget) {
	if (gData.mSelectScreenType != CHARACTER_SELECT_SCREEN_TYPE_CREDITS) return 0;

	return (tTarget.y == -1 || tTarget.y == gData.mHeader.mRows);
}

static void setStageSelectActive(int i);

static void moveCreditSelectionToStage(int i) {
	setStageSelectActive(i);
	unloadSingleSelector(i);
}

static void updateSingleSelectionInput(int i) {
	Vector3DI target = gData.mSelectors[i].mSelectedCharacter;
	if (hasPressedRightFlankSingle(gData.mSelectors[i].mOwner)) {
		target = vecAddI(target, makeVector3DI(1, 0, 0));
	}
	else if (hasPressedLeftFlankSingle(gData.mSelectors[i].mOwner)) {
		target = vecAddI(target, makeVector3DI(-1, 0, 0));
	}

	if (hasPressedDownFlankSingle(gData.mSelectors[i].mOwner)) {
		target = vecAddI(target, makeVector3DI(0, 1, 0));
	}
	else if (hasPressedUpFlankSingle(gData.mSelectors[i].mOwner)) {
		target = vecAddI(target, makeVector3DI(0, -1, 0));
	}

	if (hasCreditSelectionMovedToStage(target)) {
		moveCreditSelectionToStage(i);
		return;
	}

	target = sanitizeCellPosition(i, target);
	if (vecEqualsI(gData.mSelectors[i].mSelectedCharacter, target)) return;

	moveSelectionToTarget(i, target, 1);
}

static void showNewRandomSelectCharacter(int i) {
	int amount = vector_size(&gData.mRealSelectCharacters);
	int newIndex = gData.mSelectors[i].mRandom.mCurrentCharacter;
	if (amount > 1) {
		while (newIndex == gData.mSelectors[i].mRandom.mCurrentCharacter) {
			newIndex = randfromInteger(0, amount - 1);
		}
	}
	else {
		newIndex = 0;
	}

	gData.mSelectors[i].mRandom.mCurrentCharacter = newIndex;
	SelectCharacter* e = (SelectCharacter*)vector_get(&gData.mRealSelectCharacters, gData.mSelectors[i].mRandom.mCurrentCharacter);
	showSelectCharacterForSelector(i, e);
	gData.mSelectors[i].mRandom.mNow = 0;
}

static void updateSingleSelectionRandom(int i) {
	SelectCharacter* character = getCellCharacter(gData.mSelectors[i].mSelectedCharacter);
	if (character->mType != SELECT_CHARACTER_TYPE_RANDOM) return;

	gData.mSelectors[i].mRandom.mNow++;
	if (gData.mSelectors[i].mRandom.mNow >= gData.mHeader.mRandomPortraitSwitchTime) {
		showNewRandomSelectCharacter(i);
		tryPlayMugenSound(&gData.mSounds, gData.mHeader.mPlayers[i].mRandomMoveSound.x, gData.mHeader.mPlayers[i].mRandomMoveSound.y);
	}

}

static void updateSingleSelection(int i) {
	if (!gData.mSelectors[i].mIsActive) return;
	if (gData.mSelectors[i].mIsDone) return;


	updateSingleSelectionInput(i);
	updateSingleSelectionRandom(i);
}

static void updateStageSelection(int i, int tNewStage, int tDoesPlaySound);
static void setStageSelectInactive(int i);

static int updateCreditStageSelectionAndReturnIfStageSelectionOver(int i) {
	if (gData.mSelectScreenType != CHARACTER_SELECT_SCREEN_TYPE_CREDITS) return 0;

	if (hasPressedUpFlankSingle(i) || hasPressedDownFlankSingle(i)) {
		setStageSelectInactive(i);
		loadSingleSelector(i, i);
		return 1;
	}

	return 0;
}

static void updateSingleStageSelection(int i) {
	if (!gData.mSelectors[i].mIsActive && gData.mSelectScreenType != CHARACTER_SELECT_SCREEN_TYPE_CREDITS) return;
	if (!gData.mSelectors[i].mIsDone && gData.mSelectScreenType != CHARACTER_SELECT_SCREEN_TYPE_CREDITS) return;
	if (gData.mSelectScreenType == CHARACTER_SELECT_SCREEN_TYPE_ONE_PLAYER_SELECTS_EVERYTHING && i == 1) return;
	if (!gData.mStageSelect.mIsUsing) return;
	if (!gData.mStageSelect.mIsActive) return;
	if (gData.mStageSelect.mIsDone) return;

	int stageAmount = vector_size(&gData.mSelectStages);
	int target = gData.mStageSelect.mSelectedStage;
	if (hasPressedRightFlankSingle(gData.mSelectors[i].mOwner)) {
		target = (target + 1) % stageAmount;	
	}
	else if (hasPressedLeftFlankSingle(gData.mSelectors[i].mOwner)) {
		target--;
		if (target < 0) target += stageAmount;
	}

	if (updateCreditStageSelectionAndReturnIfStageSelectionOver(i)) return;

	if (target == gData.mStageSelect.mSelectedStage) return;
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
	gData.mIsFadingOut = 1;
	addFadeOut(gData.mHeader.mFadeOutTime, gotoTitleScreenCB, NULL);
}

static void gotoNextScreen(void* tCaller) {
	(void)tCaller;

	gData.mCB();
}

static void fadeToNextScreen() {
	gData.mIsFadingOut = 1;
	addFadeOut(gData.mHeader.mFadeOutTime, gotoNextScreen, NULL);
}

static void checkFadeToNextScreen() {
	int hasPlayer1Selected = !gData.mSelectors[0].mIsActive || gData.mSelectors[0].mIsDone;
	int hasPlayer2Selected = !gData.mSelectors[1].mIsActive || gData.mSelectors[1].mIsDone;
	int hasStageSelected = !gData.mStageSelect.mIsUsing || gData.mStageSelect.mIsDone;

	if (hasPlayer1Selected && hasPlayer2Selected && hasStageSelected) {
		fadeToNextScreen();
	}
}

static void flashSelection(int i) {
	Position pos = getCellScreenPosition(gData.mSelectors[i].mSelectedCharacter);
	pos.z = 41;
	Animation anim = createAnimation(1, 2);
	int id = playAnimation(pos, &gData.mWhiteTexture, anim, makeRectangleFromTexture(gData.mWhiteTexture), NULL, NULL);
	setAnimationSize(id, makePosition(25, 25, 1), makePosition(0, 0, 0));
}

static void updateMugenTextBasedOnVector3DI(int tID, Vector3DI tFontData) {
	setMugenTextFont(tID, tFontData.x);
	setMugenTextColor(tID, getMugenTextColorFromMugenTextColorIndex(tFontData.y));
	setMugenTextAlignment(tID, getMugenTextAlignmentFromMugenAlignmentIndex(tFontData.z));
}

static void updateStageCredit(SelectStage* tStage) {
	if (gData.mSelectScreenType != CHARACTER_SELECT_SCREEN_TYPE_CREDITS) return;

	char text[1024];
	sprintf(text, "Name: %s", tStage->mCredits.mName);
	changeMugenText(gData.mCredits.mNameID, text);
	sprintf(text, "Author: %s", tStage->mCredits.mAuthorName);
	changeMugenText(gData.mCredits.mAuthorNameID, text);
	sprintf(text, "Date: %s", tStage->mCredits.mVersionDate);
	changeMugenText(gData.mCredits.mVersionDateID, text);
}

static void updateStageSelection(int i, int tNewStage, int tDoesPlaySound) {
	SelectStage* stage = (SelectStage*)vector_get(&gData.mSelectStages, tNewStage);

	char newText[200];
	sprintf(newText, "Stage %d: %s", tNewStage + 1, stage->mName);
	changeMugenText(gData.mStageSelect.mTextID, newText);
	updateStageCredit(stage);

	if (tDoesPlaySound) {
		tryPlayMugenSound(&gData.mSounds, gData.mHeader.mPlayers[i].mCursorMoveSound.x, gData.mHeader.mPlayers[i].mCursorMoveSound.y);
	}

	gData.mStageSelect.mSelectedStage = tNewStage;
}

static void setStageSelectActive(int i) {
	updateStageSelection(i, gData.mStageSelect.mSelectedStage, 0);
	updateMugenTextBasedOnVector3DI(gData.mStageSelect.mTextID, gData.mHeader.mStageSelect.mActiveFont1);
	
	gData.mStageSelect.mActiveFontDisplayed = 0;
	gData.mStageSelect.mIsActive = 1;
	gData.mStageSelect.mIsDone = 0;
}

static void checkSetStageSelectActive(int i) {
	if (!gData.mStageSelect.mIsUsing) return;
	if (gData.mStageSelect.mIsActive) return;
	if (gData.mSelectScreenType == CHARACTER_SELECT_SCREEN_TYPE_ONE_PLAYER_SELECTS_EVERYTHING && i == 0) return;

	setStageSelectActive(i);
}

static void setStageSelectInactive(int i) {
	changeMugenText(gData.mStageSelect.mTextID, " ");
	gData.mStageSelect.mIsActive = 0;
}

static int checkSetStageSelectInactive(int i) {
	if (!gData.mStageSelect.mIsUsing) return 0;
	if (!gData.mStageSelect.mIsActive) return 0;
	if (gData.mSelectScreenType == CHARACTER_SELECT_SCREEN_TYPE_ONE_PLAYER_SELECTS_EVERYTHING && i == 0) return 0;

	int otherPlayer = i ^ 1;
	int isOtherPlayerFinished = !gData.mSelectors[otherPlayer].mIsActive || gData.mSelectors[otherPlayer].mIsDone;
	if (!gData.mStageSelect.mIsDone) {
		if (gData.mSelectScreenType == CHARACTER_SELECT_SCREEN_TYPE_ONE_PLAYER_SELECTS_EVERYTHING || gData.mSelectScreenType == CHARACTER_SELECT_SCREEN_TYPE_ONE_PLAYER_MODE || !isOtherPlayerFinished) {
			setStageSelectInactive(i);
		}
		return 0;
	}
	else {
		PlayerHeader* owner = &gData.mHeader.mPlayers[gData.mSelectors[i].mOwner];
		tryPlayMugenSound(&gData.mSounds, owner->mCursorDoneSound.x, owner->mCursorDoneSound.y);
		setStageSelectActive(i);
	}

	return 1;
}

static void checkSetSecondPlayerActive(int i) {
	if (i == 0 && gData.mSelectScreenType == CHARACTER_SELECT_SCREEN_TYPE_ONE_PLAYER_SELECTS_EVERYTHING) {
		loadSingleSelector(1, 0);
	}
}

static void deselectSelection(int i, int tDoesPlaySound);

static void checkSetSecondPlayerInactive(int i) {
	if (i == 1 && gData.mSelectScreenType == CHARACTER_SELECT_SCREEN_TYPE_ONE_PLAYER_SELECTS_EVERYTHING && !gData.mSelectors[i].mIsDone) {
		unloadSingleSelector(1);
		deselectSelection(0, 0);
	}
}

static void setCharacterSelectionFinished(int i) {
	SelectCharacter* character = getCellCharacter(gData.mSelectors[i].mSelectedCharacter);
	if (character->mType == SELECT_CHARACTER_TYPE_EMPTY) return;

	PlayerHeader* owner = &gData.mHeader.mPlayers[gData.mSelectors[i].mOwner];
	changeMugenAnimation(gData.mSelectors[i].mSelectorAnimationID, owner->mDoneCursorAnimation);
	tryPlayMugenSound(&gData.mSounds, owner->mCursorDoneSound.x, owner->mCursorDoneSound.y);

	if (character->mType == SELECT_CHARACTER_TYPE_RANDOM) {
		character = (SelectCharacter*)vector_get(&gData.mRealSelectCharacters, gData.mSelectors[i].mRandom.mCurrentCharacter);
	}

	if (gData.mSelectScreenType != CHARACTER_SELECT_SCREEN_TYPE_STORY) {
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

	gData.mSelectors[i].mIsDone = 1;

	checkFadeToNextScreen();
}

static void setStageSelectionFinished(int i) {
	if (!gData.mStageSelect.mIsUsing || !gData.mStageSelect.mIsActive || gData.mStageSelect.mIsDone) return;

	tryPlayMugenSound(&gData.mSounds, gData.mHeader.mPlayers[i].mCursorDoneSound.x, gData.mHeader.mPlayers[i].mCursorDoneSound.y);

	SelectStage* stage = (SelectStage*)vector_get(&gData.mSelectStages, gData.mStageSelect.mSelectedStage);
	char dummyMusicPath[2];
	*dummyMusicPath = '\0';
	setDreamStageMugenDefinition(stage->mPath, dummyMusicPath);

	updateMugenTextBasedOnVector3DI(gData.mStageSelect.mTextID, gData.mHeader.mStageSelect.mDoneFont);
	gData.mStageSelect.mIsDone = 1;
	checkFadeToNextScreen();
}

static void setSelectionFinished(int i) {
	if (gData.mSelectors[i].mIsDone) {
		setStageSelectionFinished(i);
	}
	else {
		setCharacterSelectionFinished(i);
	}
}

static void deselectSelection(int i, int tDoesPlaySound) {
	if (checkSetStageSelectInactive(i)) return;

	PlayerHeader* owner = &gData.mHeader.mPlayers[gData.mSelectors[i].mOwner];
	changeMugenAnimation(gData.mSelectors[i].mSelectorAnimationID, owner->mActiveCursorAnimation);
	if(tDoesPlaySound) tryPlayMugenSound(&gData.mSounds, owner->mCursorDoneSound.x, owner->mCursorDoneSound.y);

	checkSetSecondPlayerInactive(i);

	gData.mSelectors[i].mIsDone = 0;
}

static void updateSingleSelectionConfirmation(int i) {
	if (!gData.mSelectors[i].mIsActive) return;
	if (gData.mSelectScreenType == CHARACTER_SELECT_SCREEN_TYPE_CREDITS) return;
	if (gData.mSelectScreenType == CHARACTER_SELECT_SCREEN_TYPE_ONE_PLAYER_SELECTS_EVERYTHING && i == 0 && gData.mSelectors[1].mIsActive) return;

	if (hasPressedAFlankSingle(gData.mSelectors[i].mOwner) || hasPressedStartFlankSingle(gData.mSelectors[i].mOwner)) {
		setSelectionFinished(i);
	}
}

static void updateSingleSelectionDeselect(int i) {
	if (gData.mIsFadingOut) return;
	if (!gData.mSelectors[i].mIsActive && gData.mSelectScreenType != CHARACTER_SELECT_SCREEN_TYPE_CREDITS) return;
	if (gData.mSelectScreenType == CHARACTER_SELECT_SCREEN_TYPE_ONE_PLAYER_SELECTS_EVERYTHING && i == 0 && gData.mSelectors[i].mIsDone) return;

	if (hasPressedBFlankSingle(gData.mSelectors[i].mOwner)) {
		int isSelectorDone = gData.mSelectors[i].mIsDone && gData.mSelectScreenType != CHARACTER_SELECT_SCREEN_TYPE_CREDITS;
		int canGoBackToFirstPlayer = i == 1 && gData.mSelectScreenType == CHARACTER_SELECT_SCREEN_TYPE_ONE_PLAYER_SELECTS_EVERYTHING && !gData.mSelectors[i].mIsDone;
		if (isSelectorDone || canGoBackToFirstPlayer) {
			deselectSelection(i, 1);
		}
		else {
			fadeToTitleScreen();
		}
	}
}

static void updateSelectionInputs() {

	int i;
	for (i = 0; i < 2; i++) {
		updateSingleSelectionConfirmation(i);
		updateSingleSelectionDeselect(i);
	}
}

static void updateStageSelect() {
	if (!gData.mStageSelect.mIsUsing || !gData.mStageSelect.mIsActive || gData.mStageSelect.mIsDone) return;

	gData.mStageSelect.mActiveFontDisplayed = (gData.mStageSelect.mActiveFontDisplayed + 1) % 4;
	Vector3DI fontData = gData.mStageSelect.mActiveFontDisplayed < 2 ? gData.mHeader.mStageSelect.mActiveFont2 : gData.mHeader.mStageSelect.mActiveFont1;
	updateMugenTextBasedOnVector3DI(gData.mStageSelect.mTextID, fontData);
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

void setCharacterSelectScreenModeName(char * tModeName)
{
	strcpy(gData.mModeName, tModeName);
}

void setCharacterSelectFinishedCB(void(*tCB)())
{
	gData.mCB = tCB;
}

void setCharacterSelectStageActive()
{
	gData.mStageSelect.mIsUsing = 1;
}

void setCharacterSelectStageInactive()
{
	gData.mStageSelect.mIsUsing = 0;
}

void setCharacterSelectOnePlayer()
{
	gData.mSelectScreenType = CHARACTER_SELECT_SCREEN_TYPE_ONE_PLAYER_MODE;
}

void setCharacterSelectOnePlayerSelectAll()
{
	gData.mSelectScreenType = CHARACTER_SELECT_SCREEN_TYPE_ONE_PLAYER_SELECTS_EVERYTHING;
}

void setCharacterSelectTwoPlayers()
{
	gData.mSelectScreenType = CHARACTER_SELECT_SCREEN_TYPE_TWO_PLAYER_MODE;

}

void setCharacterSelectCredits()
{
	gData.mSelectScreenType = CHARACTER_SELECT_SCREEN_TYPE_CREDITS;
}

void setCharacterSelectStory()
{
	gData.mSelectScreenType = CHARACTER_SELECT_SCREEN_TYPE_STORY;
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

void getCharacterSelectNamePath(char* tName, char* oDst) {
	if (strchr(tName, '.')) {
		if (strcmp("zip", getFileExtension(tName))) {
			logWarningFormat("No support for zipped characters. Error loading %s.", tName);
			*oDst = '\0'; // TODO: zip format
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
	MugenDefScriptGroup* e = (MugenDefScriptGroup*)string_map_get(&tScript->mGroups, "Characters");

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

	MugenDefScriptGroup* e = (MugenDefScriptGroup*)string_map_get(&tScript->mGroups, "Characters");
	list_map(&e->mOrderedElementList, loadSingleRandomCharacterStage, &caller);

	e = (MugenDefScriptGroup*)string_map_get(&tScript->mGroups, "ExtraStages");
	list_map(&e->mOrderedElementList, loadSingleRandomStageStage, &caller);


	int index = randfromInteger(0, vector_size(&caller.mElements) - 1);
	PossibleRandomStageElement* newStage = (PossibleRandomStageElement*)vector_get(&caller.mElements, index);
	char dummyMusicPath[2];
	*dummyMusicPath = '\0'; // TODO
	setDreamStageMugenDefinition(newStage->mPath, dummyMusicPath);

	delete_vector(&caller.mElements);
	delete_string_map(&caller.mAllElements);
}
