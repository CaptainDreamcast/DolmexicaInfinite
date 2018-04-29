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

#include <prism/log.h>

#include "menuscreen.h"
#include "menubackground.h"
#include "characterselectscreen.h"
#include "arcademode.h"
#include "versusmode.h"
#include "trainingmode.h"
#include "fightscreen.h"

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

static struct {
	MugenDefScript mScript;
	MugenSpriteFile mSprites;
	MugenAnimations mAnimations;
	MugenSounds mSounds;

	MenuHeader mHeader;
	Vector mMenus;

	Position mMenuBasePosition;
	Position mMenuTargetPosition;

	int mBoxCursorAnimationID;
	Position mBoxCursorBasePosition;

	TextureData mWhiteTexture;
	int mCreditBGAnimationID;
	int mLeftCreditTextID;
	int mRightCreditTextID;

	int mTopOption;
	int mSelected;
} gData;

static void gotoArcadeMode(void* tCaller) {
	(void)tCaller;
	startArcadeMode();
}

static void arcadeCB() {
	addFadeOut(gData.mHeader.mFadeOutTime, gotoArcadeMode, NULL);
}

static void gotoVersusMode(void* tCaller) {
	(void)tCaller;
	startVersusMode();
}

static void versusCB() {
	addFadeOut(gData.mHeader.mFadeOutTime, gotoVersusMode, NULL);
}

static void gotoTrainingMode(void* tCaller) {
	(void)tCaller;
	startTrainingMode();
}

static void trainingCB() {
	addFadeOut(gData.mHeader.mFadeOutTime, gotoTrainingMode, NULL);
}

static void exitCB() {
	abortScreenHandling();
}

static void loadMenuHeader() {
	gData.mHeader.mFadeInTime = getMugenDefIntegerOrDefault(&gData.mScript, "Title Info", "fadein.time", 30);
	gData.mHeader.mFadeOutTime = getMugenDefIntegerOrDefault(&gData.mScript, "Title Info", "fadeout.time", 30);

	gData.mHeader.mMenuPosition = getMugenDefVectorOrDefault(&gData.mScript, "Title Info", "menu.pos", makePosition(0, 0, 0));
	gData.mHeader.mItemFont = getMugenDefVectorIOrDefault(&gData.mScript, "Title Info", "menu.item.font", makeVector3DI(1, 0, 0));
	gData.mHeader.mItemActiveFont = getMugenDefVectorIOrDefault(&gData.mScript, "Title Info", "menu.item.active.font", makeVector3DI(1, 0, 0));
	gData.mHeader.mItemSpacing = getMugenDefVectorOrDefault(&gData.mScript, "Title Info", "menu.item.spacing", makePosition(0, 0, 0));

	gData.mHeader.mWindowMarginsY = getMugenDefVectorOrDefault(&gData.mScript, "Title Info", "menu.window.margins.y", makePosition(0, 0, 0));

	gData.mHeader.mVisibleItemAmount = getMugenDefIntegerOrDefault(&gData.mScript, "Title Info", "menu.window.visibleitems", 10);
	gData.mHeader.mIsBoxCursorVisible = getMugenDefIntegerOrDefault(&gData.mScript, "Title Info", "menu.boxcursor.visible", 1);

	gData.mHeader.mMenuRectangle = makeGeoRectangle(-INF / 2, gData.mHeader.mMenuPosition.y - gData.mHeader.mWindowMarginsY.x, INF, gData.mHeader.mWindowMarginsY.x + gData.mHeader.mWindowMarginsY.y + gData.mHeader.mVisibleItemAmount*gData.mHeader.mItemSpacing.y);

	MugenStringVector boxCoordinateVector = getMugenDefStringVectorVariable(&gData.mScript, "Title Info", "menu.boxcursor.coords");
	assert(boxCoordinateVector.mSize >= 4);
	gData.mHeader.mBoxCursorCoordinates.mTopLeft.x = atof(boxCoordinateVector.mElement[0]);
	gData.mHeader.mBoxCursorCoordinates.mTopLeft.y = atof(boxCoordinateVector.mElement[1]);
	gData.mHeader.mBoxCursorCoordinates.mBottomRight.x = atof(boxCoordinateVector.mElement[2]);
	gData.mHeader.mBoxCursorCoordinates.mBottomRight.y = atof(boxCoordinateVector.mElement[3]);

	gData.mHeader.mCursorMoveSound = getMugenDefVectorIOrDefault(&gData.mScript, "Title Info", "cursor.move.snd", makeVector3DI(0, 0, 0));
	gData.mHeader.mCursorDoneSound = getMugenDefVectorIOrDefault(&gData.mScript, "Title Info", "cursor.done.snd", makeVector3DI(0, 0, 0));
	gData.mHeader.mCancelSound = getMugenDefVectorIOrDefault(&gData.mScript, "Title Info", "cancel.snd", makeVector3DI(0, 0, 0));
}

static void addMenuPoint(char* tVariableName, void(*tCB)()) {
	if (!isMugenDefStringVariable(&gData.mScript, "Title Info", tVariableName)) return;
	char* text = getAllocatedMugenDefStringVariable(&gData.mScript, "Title Info", tVariableName);
	
	if (!strcmp("", text)) {
		freeMemory(text);
		return;
	}

	int index = vector_size(&gData.mMenus);
	MenuText* e = allocMemory(sizeof(MenuText));
	e->mCB = tCB;
	e->mOffset = makePosition(gData.mHeader.mItemSpacing.x*index, gData.mHeader.mItemSpacing.y*index, 50);
	Position position = vecAdd(gData.mHeader.mMenuPosition, e->mOffset);
	e->mTextID = addMugenText(text, position, gData.mHeader.mItemFont.x);
	setMugenTextColor(e->mTextID, getMugenTextColorFromMugenTextColorIndex(gData.mHeader.mItemFont.y));
	setMugenTextAlignment(e->mTextID, getMugenTextAlignmentFromMugenAlignmentIndex(gData.mHeader.mItemFont.z));
	setMugenTextRectangle(e->mTextID, gData.mHeader.mMenuRectangle);
	vector_push_back_owned(&gData.mMenus, e);
}

static void setLowerOptionAsBase() {
	gData.mTopOption = gData.mSelected - (gData.mHeader.mVisibleItemAmount - 1);
	gData.mMenuTargetPosition = makePosition(gData.mHeader.mMenuPosition.x - gData.mHeader.mItemSpacing.x*gData.mTopOption, gData.mHeader.mMenuPosition.y - gData.mHeader.mItemSpacing.y*gData.mTopOption, 0);
}

static void setUpperOptionAsBase() {
	gData.mTopOption = gData.mSelected;	
	gData.mMenuTargetPosition = makePosition(gData.mHeader.mMenuPosition.x - gData.mHeader.mItemSpacing.x*gData.mTopOption, gData.mHeader.mMenuPosition.y - gData.mHeader.mItemSpacing.y*gData.mTopOption, 0);
}

static void setSelectedMenuElementActive() {
	MenuText* e = vector_get(&gData.mMenus, gData.mSelected);
	setMugenTextFont(e->mTextID, gData.mHeader.mItemActiveFont.x);
	setMugenTextColor(e->mTextID, getMugenTextColorFromMugenTextColorIndex(gData.mHeader.mItemActiveFont.y));
	setMugenTextAlignment(e->mTextID, getMugenTextAlignmentFromMugenAlignmentIndex(gData.mHeader.mItemActiveFont.z));

	if (gData.mSelected >= gData.mTopOption + gData.mHeader.mVisibleItemAmount) {
		setLowerOptionAsBase();
	}
	else if (gData.mSelected < gData.mTopOption) {
		setUpperOptionAsBase();
	}
}

static void setSelectedMenuElementInactive() {
	MenuText* e = vector_get(&gData.mMenus, gData.mSelected);
	setMugenTextFont(e->mTextID, gData.mHeader.mItemFont.x);
	setMugenTextColor(e->mTextID, getMugenTextColorFromMugenTextColorIndex(gData.mHeader.mItemFont.y));
	setMugenTextAlignment(e->mTextID, getMugenTextAlignmentFromMugenAlignmentIndex(gData.mHeader.mItemFont.z));
}

static void loadCredits() {
	gData.mCreditBGAnimationID = playOneFrameAnimationLoop(makePosition(0, 230, 50), &gData.mWhiteTexture);
	setAnimationSize(gData.mCreditBGAnimationID, makePosition(320, 20, 1), makePosition(0, 0, 0));
	setAnimationColor(gData.mCreditBGAnimationID, 0, 0, 0.5);

	gData.mLeftCreditTextID = addMugenText("Dolmexica Infinite Demo 2", makePosition(0, 240, 51), 1);

	gData.mRightCreditTextID = addMugenText("05/04/18 Presented by Dogma", makePosition(320, 240, 51), 1);
	setMugenTextAlignment(gData.mRightCreditTextID, MUGEN_TEXT_ALIGNMENT_RIGHT);
}

static void boxCursorCB1(void* tCaller);

static void loadBoxCursor() {
	if (gData.mHeader.mIsBoxCursorVisible) {
		gData.mBoxCursorBasePosition = makePosition(0, 0, 0);
		gData.mBoxCursorAnimationID = playOneFrameAnimationLoop(makePosition(gData.mHeader.mBoxCursorCoordinates.mTopLeft.x, gData.mHeader.mBoxCursorCoordinates.mTopLeft.y + 7.5, 49), &gData.mWhiteTexture); // TODO: fix the "+ 7"
		double w = gData.mHeader.mBoxCursorCoordinates.mBottomRight.x - gData.mHeader.mBoxCursorCoordinates.mTopLeft.x;
		double h = gData.mHeader.mBoxCursorCoordinates.mBottomRight.y - gData.mHeader.mBoxCursorCoordinates.mTopLeft.y;
		setAnimationSize(gData.mBoxCursorAnimationID, makePosition(w, h, 1), makePosition(0, 0, 0));
		setAnimationBasePositionReference(gData.mBoxCursorAnimationID, &gData.mBoxCursorBasePosition);
		setAnimationColor(gData.mBoxCursorAnimationID, 0, 1, 1);
		setAnimationTransparency(gData.mBoxCursorAnimationID, 1);
		boxCursorCB1(NULL);
	}
}

static void loadTitleScreen() {
	instantiateActor(MugenTextHandler);
	instantiateActor(getMugenAnimationHandlerActorBlueprint());
	instantiateActor(ClipboardHandler);
	
	gData.mWhiteTexture = createWhiteTexture();

	char folder[1024];
	gData.mScript = loadMugenDefScript("assets/data/system.def");
	getPathToFile(folder, "assets/data/system.def");
	setWorkingDirectory(folder);

	char* text = getAllocatedMugenDefStringVariable(&gData.mScript, "Files", "spr");
	gData.mSprites = loadMugenSpriteFileWithoutPalette(text);
	gData.mAnimations = loadMugenAnimationFile("system.def");
	freeMemory(text);

	text = getAllocatedMugenDefStringVariable(&gData.mScript, "Files", "snd");
	gData.mSounds = loadMugenSoundFile(text);
	freeMemory(text);

	loadMenuHeader();
	loadMenuBackground(&gData.mScript, &gData.mSprites, &gData.mAnimations, "TitleBGdef", "TitleBG");

	gData.mMenus = new_vector();
	addMenuPoint("menu.itemname.story", arcadeCB);
	addMenuPoint("menu.itemname.arcade", arcadeCB);
	addMenuPoint("menu.itemname.versus", versusCB);
	addMenuPoint("menu.itemname.teamarcade", arcadeCB);
	addMenuPoint("menu.itemname.teamversus", arcadeCB);
	addMenuPoint("menu.itemname.teamcoop", arcadeCB);
	addMenuPoint("menu.itemname.survival", arcadeCB);
	addMenuPoint("menu.itemname.survivalcoop", arcadeCB);
	addMenuPoint("menu.itemname.training", trainingCB);
	addMenuPoint("menu.itemname.watch", arcadeCB);
	addMenuPoint("menu.itemname.options", arcadeCB);
	addMenuPoint("menu.itemname.exit", exitCB);

	loadBoxCursor();

	gData.mMenuBasePosition = gData.mHeader.mMenuPosition;
	gData.mMenuTargetPosition = gData.mMenuBasePosition;
	gData.mTopOption = 0;
	setSelectedMenuElementActive();

	loadCredits();

	setWorkingDirectory("/");

	addFadeIn(gData.mHeader.mFadeInTime, NULL, NULL);

	logTextureMemoryState();
	logMemoryState();
}

static void boxCursorCB2(void* tCaller) {
	tweenDouble(getAnimationTransparencyReference(gData.mBoxCursorAnimationID), 0.2, 0.1, linearTweeningFunction, 20, boxCursorCB1, NULL);
}

static void boxCursorCB1(void* tCaller) {
	tweenDouble(getAnimationTransparencyReference(gData.mBoxCursorAnimationID), 0.1, 0.2, linearTweeningFunction, 20, boxCursorCB2, NULL);
}

static void updateItemSelection() {
	
	if (hasPressedDownFlank()) {
		setSelectedMenuElementInactive();
		gData.mSelected = (gData.mSelected + 1) % vector_size(&gData.mMenus);
		setSelectedMenuElementActive();
		tryPlayMugenSound(&gData.mSounds, gData.mHeader.mCursorMoveSound.x, gData.mHeader.mCursorMoveSound.y);
	}
	else if (hasPressedUpFlank()) {
		setSelectedMenuElementInactive();
		gData.mSelected--;
		if (gData.mSelected < 0) gData.mSelected += vector_size(&gData.mMenus);
		setSelectedMenuElementActive();
		tryPlayMugenSound(&gData.mSounds, gData.mHeader.mCursorMoveSound.x, gData.mHeader.mCursorMoveSound.y);
	}

}

static void updateMenuBasePosition() {
	gData.mMenuBasePosition = vecAdd(gData.mMenuBasePosition, vecScale(vecSub(gData.mMenuTargetPosition, gData.mMenuBasePosition), 0.1));
}

static void updateMenuElementPositions() {
	int i;
	int l = vector_size(&gData.mMenus);
	for (i = 0; i < l; i++) {
		MenuText* e = vector_get(&gData.mMenus, i);
		setMugenTextPosition(e->mTextID, vecAdd(gData.mMenuBasePosition, e->mOffset));
	}
}

static void updateItemSelectionConfirmation() {
	if (hasPressedAFlank() || hasPressedStartFlank()) {
		MenuText* e = vector_get(&gData.mMenus, gData.mSelected);
		tryPlayMugenSound(&gData.mSounds, gData.mHeader.mCursorDoneSound.x, gData.mHeader.mCursorDoneSound.y);
		e->mCB();
	}
}

static void updateSelectionBoxPosition() {
	MenuText* e = vector_get(&gData.mMenus, gData.mSelected);

	gData.mBoxCursorBasePosition = getMugenTextPosition(e->mTextID);
	gData.mBoxCursorBasePosition.z = 0;
}

static void updateSelectionBox() {
	if (!gData.mHeader.mIsBoxCursorVisible) return;

	updateSelectionBoxPosition();
}

static void updateTitleScreen() {
	// startFightScreen(); // TODO
	
	updateItemSelection();
	updateMenuBasePosition();
	updateMenuElementPositions();
	updateSelectionBox();
	updateItemSelectionConfirmation();

	if (hasPressedAbortFlank()) {
		abortScreenHandling();
	}

	if (hasPressedXFlank()) {
		addControllerRumbleBasic(20);
	}
}

Screen DreamTitleScreen = {
	.mLoad = loadTitleScreen,
	.mUpdate = updateTitleScreen,
};
