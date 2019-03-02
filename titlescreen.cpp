#include "titlescreen.h"

#include <assert.h>

#include <prism/animation.h>
#include <prism/screeneffect.h>
#include <prism/input.h>
#include <prism/mugendefreader.h>
#include <prism/file.h>
#include <prism/mugenanimationhandler.h>
#include <prism/math.h>
#include <prism/mugensoundfilereader.h>
#include <prism/mugentexthandler.h>
#include <prism/tweening.h>
#include <prism/clipboardhandler.h>
#include <prism/screeneffect.h>
#include <prism/timer.h>

#include <prism/log.h>

#include "mugensound.h"
#include "menubackground.h"
#include "characterselectscreen.h"
#include "arcademode.h"
#include "versusmode.h"
#include "trainingmode.h"
#include "fightscreen.h"
#include "storymode.h"
#include "creditsmode.h"
#include "optionsscreen.h"
#include "boxcursorhandler.h"
#include "survivalmode.h"
#include "watchmode.h"
#include "superwatchmode.h"
#include "randomwatchmode.h"
#include "exhibitmode.h"
#include "freeplaymode.h"
#include "intro.h"

typedef struct {
	void(*mCB)();
	Position mOffset;

	int mTextID;
} MenuText;

typedef struct {
	int mFadeInTime;
	int mFadeOutTime;

	Position mMenuPosition;
	Vector3DI mItemFont;
	Vector3DI mItemActiveFont;

	Vector3D mItemSpacing;
	Vector3D mWindowMarginsY;
	GeoRectangle mMenuRectangle;

	int mIsBoxCursorVisible;
	GeoRectangle mBoxCursorCoordinates;

	int mVisibleItemAmount;

	Vector3DI mCursorMoveSound;
	Vector3DI mCursorDoneSound;
	Vector3DI mCancelSound;

} MenuHeader;

typedef struct {
	int mNow;
	int mWaitTime;
	int mIsEnabled;

} DemoHeader;

static struct {
	MugenDefScript mScript;
	MugenSpriteFile mSprites;
	MugenAnimations mAnimations;
	MugenSounds mSounds;

	MenuHeader mHeader;
	DemoHeader mDemo;
	Vector mMenus;

	Position mMenuBasePosition;
	Position mMenuTargetPosition;

	int mBoxCursorID;

	TextureData mWhiteTexture;
	int mCreditBGAnimationID;
	int mLeftCreditTextID;
	int mRightCreditTextID;

	int mTopOption;
	int mSelected;
} gTitleScreenData;

static void gotoArcadeMode(void* tCaller) {
	(void)tCaller;
	startArcadeMode();
}

static void arcadeCB() {
	addFadeOut(gTitleScreenData.mHeader.mFadeOutTime, gotoArcadeMode, NULL);
}

static void gotoVersusMode(void* tCaller) {
	(void)tCaller;
	startVersusMode();
}

static void versusCB() {
	addFadeOut(gTitleScreenData.mHeader.mFadeOutTime, gotoVersusMode, NULL);
}

static void gotoTrainingMode(void* tCaller) {
	(void)tCaller;
	startTrainingMode();
}

static void trainingCB() {
	addFadeOut(gTitleScreenData.mHeader.mFadeOutTime, gotoTrainingMode, NULL);
}

static void gotoStoryMode(void* tCaller) {
	(void)tCaller;
	startStoryMode();
}

static void storyCB() {
	addFadeOut(gTitleScreenData.mHeader.mFadeOutTime, gotoStoryMode, NULL);
}

static void gotoCreditsMode(void* tCaller) {
	(void)tCaller;
	startCreditsMode();
}

static void creditsCB() {
	addFadeOut(gTitleScreenData.mHeader.mFadeOutTime, gotoCreditsMode, NULL);
}

static void gotoOptionsScreen(void* tCaller) {
	(void)tCaller;
	startOptionsScreen();
}

static void optionsCB() {
	addFadeOut(gTitleScreenData.mHeader.mFadeOutTime, gotoOptionsScreen, NULL);
}

static void gotoSurvivalMode(void* tCaller) {
	(void)tCaller;
	startSurvivalMode();
}

static void survivalCB() {
	addFadeOut(gTitleScreenData.mHeader.mFadeOutTime, gotoSurvivalMode, NULL);
}

static void gotoWatchMode(void* tCaller) {
	(void)tCaller;
	startWatchMode();
}

static void watchCB() {
	addFadeOut(gTitleScreenData.mHeader.mFadeOutTime, gotoWatchMode, NULL);
}

static void gotoSuperWatchMode(void* tCaller) {
	(void)tCaller;
	startSuperWatchMode();
}

static void superWatchCB() {
	addFadeOut(gTitleScreenData.mHeader.mFadeOutTime, gotoSuperWatchMode, NULL);
}

static void gotoRandomWatchMode(void* tCaller) {
	(void)tCaller;
	startRandomWatchMode();
}

static void randomWatchCB() {
	addFadeOut(gTitleScreenData.mHeader.mFadeOutTime, gotoRandomWatchMode, NULL);
}

static void gotoFreePlayMode(void* tCaller) {
	(void)tCaller;
	startFreePlayMode();
}

static void freePlayCB() {
	addFadeOut(gTitleScreenData.mHeader.mFadeOutTime, gotoFreePlayMode, NULL);
}

static void gotoExhibitMode(void* tCaller) {
	(void)tCaller;
	if (hasIntroStoryboard() && hasFinishedIntroWaitCycle()) {
		playIntroStoryboard();
	}
	else {
		increaseIntroWaitCycle();
		startExhibitMode();
	}	
}

static void exhibitCB(void* tCaller) {
	(void)tCaller;
	addFadeOut(gTitleScreenData.mHeader.mFadeOutTime, gotoExhibitMode, NULL);
}

static void exitCB() {
	abortScreenHandling();
}

static void loadMenuHeader() {
	gTitleScreenData.mHeader.mFadeInTime = getMugenDefIntegerOrDefault(&gTitleScreenData.mScript, "Title Info", "fadein.time", 30);
	gTitleScreenData.mHeader.mFadeOutTime = getMugenDefIntegerOrDefault(&gTitleScreenData.mScript, "Title Info", "fadeout.time", 30);

	gTitleScreenData.mHeader.mMenuPosition = getMugenDefVectorOrDefault(&gTitleScreenData.mScript, "Title Info", "menu.pos", makePosition(0, 0, 0));
	gTitleScreenData.mHeader.mItemFont = getMugenDefVectorIOrDefault(&gTitleScreenData.mScript, "Title Info", "menu.item.font", makeVector3DI(1, 0, 0));
	gTitleScreenData.mHeader.mItemActiveFont = getMugenDefVectorIOrDefault(&gTitleScreenData.mScript, "Title Info", "menu.item.active.font", makeVector3DI(1, 0, 0));
	gTitleScreenData.mHeader.mItemSpacing = getMugenDefVectorOrDefault(&gTitleScreenData.mScript, "Title Info", "menu.item.spacing", makePosition(0, 0, 0));

	gTitleScreenData.mHeader.mWindowMarginsY = getMugenDefVectorOrDefault(&gTitleScreenData.mScript, "Title Info", "menu.window.margins.y", makePosition(0, 0, 0));

	gTitleScreenData.mHeader.mVisibleItemAmount = getMugenDefIntegerOrDefault(&gTitleScreenData.mScript, "Title Info", "menu.window.visibleitems", 10);
	gTitleScreenData.mHeader.mIsBoxCursorVisible = getMugenDefIntegerOrDefault(&gTitleScreenData.mScript, "Title Info", "menu.boxcursor.visible", 1);

	gTitleScreenData.mHeader.mMenuRectangle = makeGeoRectangle(-INF / 2, gTitleScreenData.mHeader.mMenuPosition.y - gTitleScreenData.mHeader.mWindowMarginsY.x, INF, gTitleScreenData.mHeader.mWindowMarginsY.x + gTitleScreenData.mHeader.mWindowMarginsY.y + gTitleScreenData.mHeader.mVisibleItemAmount*gTitleScreenData.mHeader.mItemSpacing.y);

	MugenStringVector boxCoordinateVector = getMugenDefStringVectorVariable(&gTitleScreenData.mScript, "Title Info", "menu.boxcursor.coords");
	assert(boxCoordinateVector.mSize >= 4);
	gTitleScreenData.mHeader.mBoxCursorCoordinates.mTopLeft.x = atof(boxCoordinateVector.mElement[0]);
	gTitleScreenData.mHeader.mBoxCursorCoordinates.mTopLeft.y = atof(boxCoordinateVector.mElement[1]);
	gTitleScreenData.mHeader.mBoxCursorCoordinates.mBottomRight.x = atof(boxCoordinateVector.mElement[2]);
	gTitleScreenData.mHeader.mBoxCursorCoordinates.mBottomRight.y = atof(boxCoordinateVector.mElement[3]);

	gTitleScreenData.mHeader.mCursorMoveSound = getMugenDefVectorIOrDefault(&gTitleScreenData.mScript, "Title Info", "cursor.move.snd", makeVector3DI(0, 0, 0));
	gTitleScreenData.mHeader.mCursorDoneSound = getMugenDefVectorIOrDefault(&gTitleScreenData.mScript, "Title Info", "cursor.done.snd", makeVector3DI(0, 0, 0));
	gTitleScreenData.mHeader.mCancelSound = getMugenDefVectorIOrDefault(&gTitleScreenData.mScript, "Title Info", "cancel.snd", makeVector3DI(0, 0, 0));
}

static void loadDemoHeader() {
	gTitleScreenData.mDemo.mIsEnabled = getMugenDefIntegerOrDefault(&gTitleScreenData.mScript, "Demo Mode", "enabled", 1);
	gTitleScreenData.mDemo.mWaitTime = getMugenDefIntegerOrDefault(&gTitleScreenData.mScript, "Demo Mode", "title.waittime", 600);
	gTitleScreenData.mDemo.mNow = 0;
}

static void addMenuPoint(char* tVariableName, void(*tCB)()) {
	if (!isMugenDefStringVariable(&gTitleScreenData.mScript, "Title Info", tVariableName)) return;
	char* text = getAllocatedMugenDefStringVariable(&gTitleScreenData.mScript, "Title Info", tVariableName);
	
	if (!strcmp("", text)) {
		freeMemory(text);
		return;
	}

	int index = vector_size(&gTitleScreenData.mMenus);
	MenuText* e = (MenuText*)allocMemory(sizeof(MenuText));
	e->mCB = tCB;
	e->mOffset = makePosition(gTitleScreenData.mHeader.mItemSpacing.x*index, gTitleScreenData.mHeader.mItemSpacing.y*index, 50);
	Position position = vecAdd(gTitleScreenData.mHeader.mMenuPosition, e->mOffset);
	e->mTextID = addMugenText(text, position, gTitleScreenData.mHeader.mItemFont.x);
	setMugenTextColor(e->mTextID, getMugenTextColorFromMugenTextColorIndex(gTitleScreenData.mHeader.mItemFont.y));
	setMugenTextAlignment(e->mTextID, getMugenTextAlignmentFromMugenAlignmentIndex(gTitleScreenData.mHeader.mItemFont.z));
	setMugenTextRectangle(e->mTextID, gTitleScreenData.mHeader.mMenuRectangle);
	vector_push_back_owned(&gTitleScreenData.mMenus, e);

	freeMemory(text);
}

static void setLowerOptionAsBase() {
	gTitleScreenData.mTopOption = gTitleScreenData.mSelected - (gTitleScreenData.mHeader.mVisibleItemAmount - 1);
	gTitleScreenData.mMenuTargetPosition = makePosition(gTitleScreenData.mHeader.mMenuPosition.x - gTitleScreenData.mHeader.mItemSpacing.x*gTitleScreenData.mTopOption, gTitleScreenData.mHeader.mMenuPosition.y - gTitleScreenData.mHeader.mItemSpacing.y*gTitleScreenData.mTopOption, 0);
}

static void setUpperOptionAsBase() {
	gTitleScreenData.mTopOption = gTitleScreenData.mSelected;	
	gTitleScreenData.mMenuTargetPosition = makePosition(gTitleScreenData.mHeader.mMenuPosition.x - gTitleScreenData.mHeader.mItemSpacing.x*gTitleScreenData.mTopOption, gTitleScreenData.mHeader.mMenuPosition.y - gTitleScreenData.mHeader.mItemSpacing.y*gTitleScreenData.mTopOption, 0);
}

static void setSelectedMenuElementActive() {
	MenuText* e = (MenuText*)vector_get(&gTitleScreenData.mMenus, gTitleScreenData.mSelected);
	setMugenTextFont(e->mTextID, gTitleScreenData.mHeader.mItemActiveFont.x);
	setMugenTextColor(e->mTextID, getMugenTextColorFromMugenTextColorIndex(gTitleScreenData.mHeader.mItemActiveFont.y));
	setMugenTextAlignment(e->mTextID, getMugenTextAlignmentFromMugenAlignmentIndex(gTitleScreenData.mHeader.mItemActiveFont.z));

	if (gTitleScreenData.mSelected >= gTitleScreenData.mTopOption + gTitleScreenData.mHeader.mVisibleItemAmount) {
		setLowerOptionAsBase();
	}
	else if (gTitleScreenData.mSelected < gTitleScreenData.mTopOption) {
		setUpperOptionAsBase();
	}
}

static void setSelectedMenuElementInactive() {
	MenuText* e = (MenuText*)vector_get(&gTitleScreenData.mMenus, gTitleScreenData.mSelected);
	setMugenTextFont(e->mTextID, gTitleScreenData.mHeader.mItemFont.x);
	setMugenTextColor(e->mTextID, getMugenTextColorFromMugenTextColorIndex(gTitleScreenData.mHeader.mItemFont.y));
	setMugenTextAlignment(e->mTextID, getMugenTextAlignmentFromMugenAlignmentIndex(gTitleScreenData.mHeader.mItemFont.z));
}

static void loadCredits() {
	gTitleScreenData.mCreditBGAnimationID = playOneFrameAnimationLoop(makePosition(0, 230, 50), &gTitleScreenData.mWhiteTexture);
	setAnimationSize(gTitleScreenData.mCreditBGAnimationID, makePosition(320, 20, 1), makePosition(0, 0, 0));
	setAnimationColor(gTitleScreenData.mCreditBGAnimationID, 0, 0, 0.5);

	gTitleScreenData.mLeftCreditTextID = addMugenText("Dolmexica Infinite Demo 7", makePosition(0, 240, 51), 1);
	
	gTitleScreenData.mRightCreditTextID = addMugenText("03/01/19 Presented by Dogma", makePosition(320, 240, 51), 1);
	setMugenTextAlignment(gTitleScreenData.mRightCreditTextID, MUGEN_TEXT_ALIGNMENT_RIGHT);
}


static void loadBoxCursor() {
	if (gTitleScreenData.mHeader.mIsBoxCursorVisible) {
		gTitleScreenData.mBoxCursorID = addBoxCursor(makePosition(0, 0, 0), makePosition(0, 0, 49), gTitleScreenData.mHeader.mBoxCursorCoordinates); 
	}
}

static void loadTitleMusic() {
	char* path = getAllocatedMugenDefStringOrDefault(&gTitleScreenData.mScript, "Music", "title.bgm", " ");
	int isLooping = getMugenDefIntegerOrDefault(&gTitleScreenData.mScript, "Music", "title.bgm.loop", 1);

	if (isMugenBGMMusicPath(path)) {
		playMugenBGMMusicPath(path, isLooping);
	}

	freeMemory(path);
}

static void loadTitleScreen() {
	setWrapperTitleScreen(getDreamTitleScreen());
	
	instantiateActor(getBoxCursorHandler());

	gTitleScreenData.mWhiteTexture = getEmptyWhiteTexture();

	char folder[1024];
	loadMugenDefScript(&gTitleScreenData.mScript, "assets/data/system.def");
	getPathToFile(folder, "assets/data/system.def");
	setWorkingDirectory(folder);

	char* text = getAllocatedMugenDefStringVariable(&gTitleScreenData.mScript, "Files", "spr");
	gTitleScreenData.mSprites = loadMugenSpriteFileWithoutPalette(text);
	gTitleScreenData.mAnimations = loadMugenAnimationFile("system.def");
	freeMemory(text);

	text = getAllocatedMugenDefStringVariable(&gTitleScreenData.mScript, "Files", "snd");
	gTitleScreenData.mSounds = loadMugenSoundFile(text);
	freeMemory(text);
	
	loadMenuHeader();
	loadDemoHeader();
	loadMenuBackground(&gTitleScreenData.mScript, &gTitleScreenData.mSprites, &gTitleScreenData.mAnimations, "TitleBGdef", "TitleBG");

	gTitleScreenData.mMenus = new_vector();
	addMenuPoint("menu.itemname.story", storyCB);
	addMenuPoint("menu.itemname.arcade", arcadeCB);
	addMenuPoint("menu.itemname.freeplay", freePlayCB);
	addMenuPoint("menu.itemname.versus", versusCB);
	addMenuPoint("menu.itemname.teamarcade", arcadeCB);
	addMenuPoint("menu.itemname.teamversus", arcadeCB);
	addMenuPoint("menu.itemname.teamcoop", arcadeCB);
	addMenuPoint("menu.itemname.survival", survivalCB);
	addMenuPoint("menu.itemname.survivalcoop", arcadeCB);
	addMenuPoint("menu.itemname.training", trainingCB);
	addMenuPoint("menu.itemname.watch", watchCB);
	addMenuPoint("menu.itemname.superwatch", superWatchCB);
	addMenuPoint("menu.itemname.randomwatch", randomWatchCB);
	addMenuPoint("menu.itemname.options", optionsCB);
	addMenuPoint("menu.itemname.credits", creditsCB);
	addMenuPoint("menu.itemname.exit", exitCB);

	loadBoxCursor();

	gTitleScreenData.mMenuBasePosition = gTitleScreenData.mHeader.mMenuPosition;
	gTitleScreenData.mMenuTargetPosition = gTitleScreenData.mMenuBasePosition;
	gTitleScreenData.mTopOption = 0;
	setSelectedMenuElementActive();

	loadCredits();

	setWorkingDirectory("/");
	loadTitleMusic();

	addFadeIn(gTitleScreenData.mHeader.mFadeInTime, NULL, NULL);

	logTextureMemoryState();
	logMemoryState();
}

static void unloadTitleScreen() {
	unloadMugenDefScript(gTitleScreenData.mScript);
	unloadMugenSpriteFile(&gTitleScreenData.mSprites);
	unloadMugenAnimationFile(&gTitleScreenData.mAnimations);
	unloadMugenSoundFile(&gTitleScreenData.mSounds);

	delete_vector(&gTitleScreenData.mMenus);
}

static void updateItemSelection() {
	
	if (hasPressedDownFlank()) {
		setSelectedMenuElementInactive();
		gTitleScreenData.mSelected = (gTitleScreenData.mSelected + 1) % vector_size(&gTitleScreenData.mMenus);
		setSelectedMenuElementActive();
		tryPlayMugenSound(&gTitleScreenData.mSounds, gTitleScreenData.mHeader.mCursorMoveSound.x, gTitleScreenData.mHeader.mCursorMoveSound.y);
	}
	else if (hasPressedUpFlank()) {
		setSelectedMenuElementInactive();
		gTitleScreenData.mSelected--;
		if (gTitleScreenData.mSelected < 0) gTitleScreenData.mSelected += vector_size(&gTitleScreenData.mMenus);
		setSelectedMenuElementActive();
		tryPlayMugenSound(&gTitleScreenData.mSounds, gTitleScreenData.mHeader.mCursorMoveSound.x, gTitleScreenData.mHeader.mCursorMoveSound.y);
	}

}

static void updateMenuBasePosition() {
	gTitleScreenData.mMenuBasePosition = vecAdd(gTitleScreenData.mMenuBasePosition, vecScale(vecSub(gTitleScreenData.mMenuTargetPosition, gTitleScreenData.mMenuBasePosition), 0.1));
}

static void updateMenuElementPositions() {
	int i;
	int l = vector_size(&gTitleScreenData.mMenus);
	for (i = 0; i < l; i++) {
		MenuText* e = (MenuText*)vector_get(&gTitleScreenData.mMenus, i);
		setMugenTextPosition(e->mTextID, vecAdd(gTitleScreenData.mMenuBasePosition, e->mOffset));
	}
}

static void updateItemSelectionConfirmation() {
	if (hasPressedAFlank() || hasPressedStartFlank()) {
		MenuText* e = (MenuText*)vector_get(&gTitleScreenData.mMenus, gTitleScreenData.mSelected);
		tryPlayMugenSound(&gTitleScreenData.mSounds, gTitleScreenData.mHeader.mCursorDoneSound.x, gTitleScreenData.mHeader.mCursorDoneSound.y);
		e->mCB();
	}
}

static void updateSelectionBoxPosition() {
	MenuText* e = (MenuText*)vector_get(&gTitleScreenData.mMenus, gTitleScreenData.mSelected);

	Position pos = getMugenTextPosition(e->mTextID);
	pos.z = 0;
	setBoxCursorPosition(gTitleScreenData.mBoxCursorID, pos);
}

static void updateSelectionBox() {
	if (!gTitleScreenData.mHeader.mIsBoxCursorVisible) return;

	updateSelectionBoxPosition();
}

static void updateDemoMode() {
	if (!gTitleScreenData.mDemo.mIsEnabled) return;

	if (hasPressedAnyButton()) {
		gTitleScreenData.mDemo.mNow = 0;
	}

	if (gTitleScreenData.mDemo.mNow >= gTitleScreenData.mDemo.mWaitTime) {
		exhibitCB(NULL);
		gTitleScreenData.mDemo.mNow = 0;
	}

	gTitleScreenData.mDemo.mNow++;
}

static void updateTitleScreen() {
	// startFightScreen(); // TODO
	
	updateItemSelection();
	updateMenuBasePosition();
	updateMenuElementPositions();
	updateSelectionBox();
	updateItemSelectionConfirmation();
	updateDemoMode();
}

static Screen gDreamTitleScreen;

Screen* getDreamTitleScreen() {
	gDreamTitleScreen = makeScreen(loadTitleScreen, updateTitleScreen, NULL, unloadTitleScreen);
	return &gDreamTitleScreen;
}
