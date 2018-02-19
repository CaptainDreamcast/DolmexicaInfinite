#include "characterselectscreen.h"

#include <assert.h>

#include <prism/mugendefreader.h>
#include <prism/mugenanimationhandler.h>
#include <prism/screeneffect.h>
#include <prism/input.h>
#include <prism/mugenanimationhandler.h>
#include <prism/mugensoundfilereader.h>
#include <prism/mugentexthandler.h>

#include "menubackground.h"
#include "titlescreen.h"
#include "storyscreen.h"
#include "playerdefinition.h"

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

} SelectScreenHeader;

typedef struct {
	int mIsCharacter;
	
	Vector3DI mCellPosition;
	int mBackgroundAnimationID;

	MugenSpriteFile mSprites;
	int mPortraitAnimationID;
	char mStageName[1024];
	char* mDisplayCharacterName;
	char mCharacterName[200];
} SelectCharacter;

typedef struct {
	int mIsActive;
	int mIsDone;

	int mSelectorAnimationID;
	int mBigPortraitAnimationID;
	int mNameTextID;

	Vector3DI mSelectedCharacter;
} Selector;

typedef struct {
	int mModeTextID;

} ModeText;

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
	Selector mSelectors[2];
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

	loadMenuPlayerHeader("p1", &gData.mHeader.mPlayers[0]);
	loadMenuPlayerHeader("p2", &gData.mHeader.mPlayers[1]);
}

typedef struct {
	int i;

} MenuCharacterLoadCaller;


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

static void loadSingleSelector(int i) {
	PlayerHeader* player = &gData.mHeader.mPlayers[i];

	Position p = getCellScreenPosition(player->mCursorStartCell);
	gData.mSelectors[i].mSelectedCharacter = player->mCursorStartCell;
	p.z = 60;
	gData.mSelectors[i].mSelectorAnimationID = addMugenAnimation(player->mActiveCursorAnimation, &gData.mSprites, p);
	
	SelectCharacter* character = getCellCharacter(gData.mSelectors[i].mSelectedCharacter);
	player->mBigPortraitOffset.z = 30;
	gData.mSelectors[i].mBigPortraitAnimationID = addMugenAnimation(player->mBigPortraitAnimation, &character->mSprites, player->mBigPortraitOffset);
	setMugenAnimationFaceDirection(gData.mSelectors[i].mBigPortraitAnimationID, player->mBigPortraitIsFacingRight);

	player->mNameOffset.z = 40;
	gData.mSelectors[i].mNameTextID = addMugenText(character->mDisplayCharacterName, player->mNameOffset, player->mNameFont.x);
	setMugenTextColor(gData.mSelectors[i].mNameTextID, getMugenTextColorFromMugenTextColorIndex(player->mNameFont.y));
	setMugenTextAlignment(gData.mSelectors[i].mNameTextID, getMugenTextAlignmentFromMugenAlignmentIndex(player->mNameFont.z));

	gData.mSelectors[i].mIsActive = 1;
	gData.mSelectors[i].mIsDone = 0;
}

static void loadSelectors() {
	int i;
	for (i = 0; i < 1; i++) {
		loadSingleSelector(i);
	}
}

static void loadModeText() {
	// TODO: read mode text
	Position p = gData.mHeader.mTitleOffset;
	p.z = 50;
	gData.mModeText.mModeTextID = addMugenText(gData.mModeName, p, gData.mHeader.mTitleFont.x); 
	setMugenTextColor(gData.mModeText.mModeTextID, getMugenTextColorFromMugenTextColorIndex(gData.mHeader.mTitleFont.y));
	setMugenTextAlignment(gData.mModeText.mModeTextID, getMugenTextAlignmentFromMugenAlignmentIndex(gData.mHeader.mTitleFont.z));
}

static void loadCharacterSelectScreen() {
	instantiateActor(MugenTextHandler);
	instantiateActor(getMugenAnimationHandlerActorBlueprint());

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
	loadMenuBackground(&gData.mScript, &gData.mSprites, NULL, "SelectBGdef", "SelectBG");

	setWorkingDirectory("/");

	loadMenuCells();
	loadMenuCharacters();

	loadSelectors();
	loadModeText();

	gData.mWhiteTexture = createWhiteTexture();

	addFadeIn(gData.mHeader.mFadeInTime, NULL, NULL);
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

static void moveSelectionToTarget(int i, Vector3DI tTarget) {
	PlayerHeader* player = &gData.mHeader.mPlayers[i];

	gData.mSelectors[i].mSelectedCharacter = tTarget;
	Position p = getCellScreenPosition(tTarget);
	p.z = 60;
	setMugenAnimationPosition(gData.mSelectors[i].mSelectorAnimationID, p);

	playMugenSound(&gData.mSounds, player->mCursorMoveSound.x, player->mCursorMoveSound.y);

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
}

static void updateSingleSelection(int i) {
	if (!gData.mSelectors[i].mIsActive) return;
	if (gData.mSelectors[i].mIsDone) return;

	Vector3DI target = gData.mSelectors[i].mSelectedCharacter;
	if (hasPressedRightFlankSingle(i)) {
		target = vecAddI(target, makeVector3DI(1, 0, 0));
	}
	else if (hasPressedLeftFlankSingle(i)) {
		target = vecAddI(target, makeVector3DI(-1, 0, 0));
	}

	if (hasPressedDownFlankSingle(i)) {
		target = vecAddI(target, makeVector3DI(0, 1, 0));
	}
	else if (hasPressedUpFlankSingle(i)) {
		target = vecAddI(target, makeVector3DI(0, -1, 0));
	}

	target = sanitizeCellPosition(i, target);
	if (vecEqualsI(gData.mSelectors[i].mSelectedCharacter, target)) return;

	moveSelectionToTarget(i, target);
}

static void updateSelections() {
	int i;
	for (i = 0; i < 2; i++) {
		updateSingleSelection(i);
	}
}

static void gotoNextScreen(void* tCaller) {
	(void)tCaller;

	gData.mCB();
}

static void fadeToStory() {
	addFadeOut(gData.mHeader.mFadeOutTime, gotoNextScreen, NULL);
}

static void flashSelection(int i) {
	Position pos = getCellScreenPosition(gData.mSelectors[i].mSelectedCharacter);
	pos.z = 41;
	Animation anim = createAnimation(1, 2);
	int id = playAnimation(pos, &gData.mWhiteTexture, anim, makeRectangleFromTexture(gData.mWhiteTexture), NULL, NULL);
	setAnimationSize(id, makePosition(25, 25, 1), makePosition(0, 0, 0));
}

static void setSelectionFinished(int i) {
	SelectCharacter* character = getCellCharacter(gData.mSelectors[i].mSelectedCharacter);
	if (!character->mIsCharacter) return;

	PlayerHeader* player = &gData.mHeader.mPlayers[i];
	changeMugenAnimation(gData.mSelectors[i].mSelectorAnimationID, player->mDoneCursorAnimation);
	playMugenSound(&gData.mSounds, player->mCursorDoneSound.x, player->mCursorDoneSound.y);

	char path[1024];
	sprintf(path, "assets/chars/%s/%s.def", character->mCharacterName, character->mCharacterName);
	setPlayerDefinitionPath(i, path);

	gData.mSelectors[i].mIsDone = 1;

	flashSelection(i);
	// TODO: wait for other selection
	fadeToStory();
}

static void updateSingleSelectionConfirmation(int i) {
	if (!gData.mSelectors[i].mIsActive) return;
	if (gData.mSelectors[i].mIsDone) return;

	if (hasPressedAFlankSingle(i) || hasPressedStartFlankSingle(i)) {
		setSelectionFinished(i);
	}
}

static void updateSelectionConfirmations() {
	int i;
	for (i = 0; i < 2; i++) {
		updateSingleSelectionConfirmation(i);
	}
}

static void updateCharacterSelectScreen() {
	updateSelections();
	updateSelectionConfirmations();

	if (hasPressedAbortFlank()) {
		setNewScreen(&DreamTitleScreen);
	}
}

Screen CharacterSelectScreen = {
	.mLoad = loadCharacterSelectScreen,
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
