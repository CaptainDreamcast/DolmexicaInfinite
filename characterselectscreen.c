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

#include "menubackground.h"
#include "titlescreen.h"
#include "storyscreen.h"
#include "playerdefinition.h"
#include "gamelogic.h"
#include "stage.h"

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

typedef struct {
	int mIsCharacter;
	
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
	int mIsActive;
	int mIsDone;
	int mOwner;

	int mSelectorAnimationID;
	int mBigPortraitAnimationID;
	int mNameTextID;

	Vector3DI mSelectedCharacter;
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

static void addSingleSelectStage(char* tPath) {
	char path[1024];
	sprintf(path, "assets/%s", tPath); // TODO: remove when assets are removed

	SelectStage* e = allocMemory(sizeof(SelectStage));
	strcpy(e->mPath, path);
	
	MugenDefScript script = loadMugenDefScript(path);
	e->mName = getAllocatedMugenDefStringVariable(&script, "Info", "name");
	loadSelectStageCredits(e, &script);
	unloadMugenDefScript(script);
	
	vector_push_back_owned(&gData.mSelectStages, e);
}

static void loadStageSelectStages() {
	if (!gData.mStageSelect.mIsUsing) return;

	gData.mSelectStages = new_vector();

	MugenDefScriptGroup* group = string_map_get(&gData.mCharacterScript.mGroups, "ExtraStages");
	ListIterator iterator = list_iterator_begin(&group->mOrderedElementList);
	int hasElements = 1;
	while (hasElements) {
		MugenDefScriptGroupElement* element = list_iterator_get(iterator);
		if (element->mType == MUGEN_DEF_SCRIPT_GROUP_STRING_ELEMENT) {
			MugenDefScriptStringElement* stringElement = element->mData;
			addSingleSelectStage(stringElement->mString);
		}

		if (!list_has_next(iterator)) break;
		list_iterator_increase(&iterator);
	}
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

static void loadMenuCharacterSpritesAndName(SelectCharacter* e, char* tCharacterName) {
	
	char file[200];
	char path[1024];
	char scriptPath[1024];
	char name[100];
	char palettePath[1024];

	sprintf(path, "assets/chars/%s/", tCharacterName);

	MugenDefScript script;
	sprintf(scriptPath, "%s%s.def", path, tCharacterName);
	script = loadMugenDefScript(scriptPath);

	int preferredPalette = 0;
	sprintf(name, "pal%d", preferredPalette + 1);
	getMugenDefStringOrDefault(file, &script, "Files", name, "");
	int hasPalettePath = strcmp("", file);
	sprintf(palettePath, "%s%s", path, file);

	getMugenDefStringOrDefault(file, &script, "Files", "sprite", "");
	assert(strcmp("", file));
	sprintf(scriptPath, "%s%s", path, file);
	e->mSprites = loadMugenSpriteFilePortraits(scriptPath, preferredPalette, hasPalettePath, palettePath);

	strcpy(e->mCharacterName, tCharacterName);
	e->mDisplayCharacterName = getAllocatedMugenDefStringVariable(&script, "Info", "displayname");

	e->mIsCharacter = 1;
	
	loadMenuCharacterCredits(e, &script);
	

	unloadMugenDefScript(script);
}

static Position getCellScreenPosition(Vector3DI tCellPosition) {
	double dx = tCellPosition.x * (gData.mHeader.mCellSpacing + gData.mHeader.mCellSize.x);
	double dy = tCellPosition.y * (gData.mHeader.mCellSpacing + gData.mHeader.mCellSize.y);
	Position pos = makePosition(dx, dy, 0);
	pos = vecAdd(gData.mHeader.mPosition, pos);
	return pos;
}

static void loadSingleMenuCharacter(void* tCaller, void* tData) {

	MenuCharacterLoadCaller* caller = tCaller;
	MugenDefScriptGroupElement* element = tData;	
	assert(element->mType == MUGEN_DEF_SCRIPT_GROUP_VECTOR_ELEMENT);
	MugenDefScriptVectorElement* vectorElement = element->mData;
	
	assert(vectorElement->mVector.mSize >= 2);
	char* characterName = vectorElement->mVector.mElement[0];
	char* stageName = vectorElement->mVector.mElement[1];

	int row = caller->i / gData.mHeader.mColumns;
	int column = caller->i % gData.mHeader.mColumns;
	
	SelectCharacter* e = vector_get(vector_get(&gData.mSelectCharacters, row), column);
	strcpy(e->mStageName, stageName);
	loadMenuCharacterSpritesAndName(e, characterName);

	Position pos = getCellScreenPosition(e->mCellPosition);
	pos.z = 40;
	e->mPortraitAnimationID = addMugenAnimation(gData.mHeader.mSmallPortraitAnimation, &e->mSprites, pos);
	if (!gData.mHeader.mIsShowingEmptyBoxes) {
		pos.z = 30;
		e->mBackgroundAnimationID = addMugenAnimation(gData.mHeader.mCellBackgroundAnimation, &gData.mSprites, pos);
	}

	caller->i++;
}

static void loadMenuCharacters() {
	MugenDefScriptGroup* e = string_map_get(&gData.mCharacterScript.mGroups, "Characters");

	MenuCharacterLoadCaller caller;
	caller.i = 0;
	
	list_map(&e->mOrderedElementList, loadSingleMenuCharacter, &caller);
	gData.mCharacterAmount = caller.i;
}

static void loadSingleMenuCell(Vector3DI tCellPosition) {
	SelectCharacter* e = allocMemory(sizeof(SelectCharacter));
	e->mIsCharacter = 0;
	e->mCellPosition = tCellPosition;

	if (gData.mHeader.mIsShowingEmptyBoxes) {
		Position pos = getCellScreenPosition(e->mCellPosition);
		pos.z = 30;
		e->mBackgroundAnimationID = addMugenAnimation(gData.mHeader.mCellBackgroundAnimation, &gData.mSprites, pos);
	}

	vector_push_back_owned(vector_get(&gData.mSelectCharacters, tCellPosition.y), e);
}

static void loadMenuCells() {
	gData.mSelectCharacters = new_vector();
	
	int y, x;
	for (y = 0; y < gData.mHeader.mRows; y++) {
		Vector* v = allocMemory(sizeof(Vector));
		*v = new_vector();
		vector_push_back_owned(&gData.mSelectCharacters, v);

		for (x = 0; x < gData.mHeader.mColumns; x++) {
			loadSingleMenuCell(makeVector3DI(x, y, 0));
		}
	}
}

static SelectCharacter* getCellCharacter(Vector3DI tCellPosition) {
	SelectCharacter* ret = vector_get(vector_get(&gData.mSelectCharacters, tCellPosition.y), tCellPosition.x);
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

static void loadCharacterSelectScreen() {
	instantiateActor(MugenTextHandler);
	instantiateActor(getMugenAnimationHandlerActorBlueprint());
	instantiateActor(ClipboardHandler);

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

	loadMenuCells();
	loadMenuCharacters();
	loadStageSelectStages();

	loadCredits();
	loadSelectors();
	loadModeText();
	loadStageSelect();

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
	SelectStage* e = tData;
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
	SelectCharacter* e = tData;
	if (e->mIsCharacter) {
		unloadMugenSpriteFile(&e->mSprites);
		freeMemory(e->mDisplayCharacterName);
		unloadMenuCharacterCredits(e);
	}
}

static void unloadSingleSelectCharacterRow(void* tCaller, void* tData) {
	(void)tCaller;
	Vector* e = tData;
	vector_map(e, unloadSingleSelectCharacter, NULL);
	delete_vector(e);
}


static void unloadSelectCharacters() {
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

	if (character->mIsCharacter) {
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

static void moveSelectionToTarget(int i, Vector3DI tTarget, int tDoesPlaySound) {
	PlayerHeader* player = &gData.mHeader.mPlayers[i];
	PlayerHeader* owner = &gData.mHeader.mPlayers[gData.mSelectors[i].mOwner];

	gData.mSelectors[i].mSelectedCharacter = tTarget;
	Position p = getCellScreenPosition(tTarget);
	p.z = 60;
	setMugenAnimationPosition(gData.mSelectors[i].mSelectorAnimationID, p);

	if (tDoesPlaySound) {
		tryPlayMugenSound(&gData.mSounds, owner->mCursorMoveSound.x, owner->mCursorMoveSound.y);
	}

	SelectCharacter* character = getCellCharacter(gData.mSelectors[i].mSelectedCharacter);
	if (character->mIsCharacter) {
		setMugenAnimationBaseDrawScale(gData.mSelectors[i].mBigPortraitAnimationID, 1);
		setMugenAnimationSprites(gData.mSelectors[i].mBigPortraitAnimationID, &character->mSprites);
		changeMugenAnimation(gData.mSelectors[i].mBigPortraitAnimationID, player->mBigPortraitAnimation);
		changeMugenText(gData.mSelectors[i].mNameTextID, character->mDisplayCharacterName);
	}
	else {
		setMugenAnimationBaseDrawScale(gData.mSelectors[i].mBigPortraitAnimationID, 0);
		changeMugenText(gData.mSelectors[i].mNameTextID, " ");
	}

	updateCharacterSelectionCredits(character);
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

static void updateSingleSelection(int i) {
	if (!gData.mSelectors[i].mIsActive) return;
	if (gData.mSelectors[i].mIsDone) return;

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
	setNewScreen(&DreamTitleScreen);
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
	SelectStage* stage = vector_get(&gData.mSelectStages, tNewStage);

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
		if (gData.mSelectScreenType == CHARACTER_SELECT_SCREEN_TYPE_ONE_PLAYER_SELECTS_EVERYTHING || !isOtherPlayerFinished) {
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
	if (!character->mIsCharacter) return;

	PlayerHeader* owner = &gData.mHeader.mPlayers[gData.mSelectors[i].mOwner];
	changeMugenAnimation(gData.mSelectors[i].mSelectorAnimationID, owner->mDoneCursorAnimation);
	tryPlayMugenSound(&gData.mSounds, owner->mCursorDoneSound.x, owner->mCursorDoneSound.y);

	char path[1024];
	sprintf(path, "assets/chars/%s/%s.def", character->mCharacterName, character->mCharacterName);
	setPlayerDefinitionPath(i, path);

	flashSelection(i);

	checkSetSecondPlayerActive(i);
	checkSetStageSelectActive(i);

	gData.mSelectors[i].mIsDone = 1;

	checkFadeToNextScreen();
}

static void setStageSelectionFinished(int i) {
	if (!gData.mStageSelect.mIsUsing || !gData.mStageSelect.mIsActive || gData.mStageSelect.mIsDone) return;

	tryPlayMugenSound(&gData.mSounds, gData.mHeader.mPlayers[i].mCursorDoneSound.x, gData.mHeader.mPlayers[i].mCursorDoneSound.y);

	SelectStage* stage = vector_get(&gData.mSelectStages, gData.mStageSelect.mSelectedStage);
	setDreamStageMugenDefinition(stage->mPath);

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

	if (hasPressedAbortFlank()) {
		setNewScreen(&DreamTitleScreen);
	}
}

Screen CharacterSelectScreen = {
	.mLoad = loadCharacterSelectScreen,
	.mUnload = unloadCharacterSelectScreen,
	.mUpdate = updateCharacterSelectScreen,
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
