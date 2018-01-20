#include "titlescreen.h"

#include <assert.h>

#include <tari/animation.h>
#include <tari/screeneffect.h>
#include <tari/input.h>
#include <tari/mugendefreader.h>
#include <tari/file.h>
#include <tari/mugenanimationhandler.h>
#include <tari/math.h>

#include "menuscreen.h"
#include "mugentexthandler.h"
#include "menubackground.h"

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

} MenuHeader;

static struct {
	MugenDefScript mScript;
	MugenSpriteFile mSprites;

	MenuHeader mHeader;
	Vector mMenus;

	Position mMenuBasePosition;
	Position mMenuTargetPosition;


	int mTopOption;
	int mSelected;


} gData;

static void arcadeCB() {

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

}

static void addMenuPoint(char* tVariableName, void(*tCB)()) {
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

static void loadTitleScreen() {
	instantiateActor(MugenTextHandler);
	instantiateActor(getMugenAnimationHandlerActorBlueprint());

	char folder[1024];
	gData.mScript = loadMugenDefScript("assets/data/system.def");
	getPathToFile(folder, "assets/data/system.def");
	setWorkingDirectory(folder);

	char* text = getAllocatedMugenDefStringVariable(&gData.mScript, "Files", "spr");
	gData.mSprites = loadMugenSpriteFileWithoutPalette(text);
	freeMemory(text);

	loadMenuHeader();
	loadMenuBackground(&gData.mScript, &gData.mSprites, NULL, "TitleBGdef", "TitleBG");

	gData.mMenus = new_vector();
	addMenuPoint("menu.itemname.arcade", arcadeCB);
	addMenuPoint("menu.itemname.versus", arcadeCB);
	addMenuPoint("menu.itemname.teamarcade", arcadeCB);
	addMenuPoint("menu.itemname.teamversus", arcadeCB);
	addMenuPoint("menu.itemname.teamcoop", arcadeCB);
	addMenuPoint("menu.itemname.survival", arcadeCB);
	addMenuPoint("menu.itemname.survivalcoop", arcadeCB);
	addMenuPoint("menu.itemname.training", arcadeCB);
	addMenuPoint("menu.itemname.watch", arcadeCB);
	addMenuPoint("menu.itemname.options", arcadeCB);
	addMenuPoint("menu.itemname.exit", arcadeCB);

	gData.mMenuBasePosition = gData.mHeader.mMenuPosition;
	gData.mMenuTargetPosition = gData.mMenuBasePosition;
	gData.mTopOption = 0;
	gData.mSelected = 0,
	setSelectedMenuElementActive();

	setWorkingDirectory("/");




	addFadeIn(gData.mHeader.mFadeInTime, NULL, NULL);
}

static void updateItemSelection() {
	
	if (hasPressedDownFlank()) {
		setSelectedMenuElementInactive();
		gData.mSelected = (gData.mSelected + 1) % vector_size(&gData.mMenus);
		setSelectedMenuElementActive();
	}
	else if (hasPressedUpFlank()) {
		setSelectedMenuElementInactive();
		gData.mSelected--;
		if (gData.mSelected < 0) gData.mSelected += vector_size(&gData.mMenus);
		setSelectedMenuElementActive();
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

static void updateTitleScreen() {
	updateItemSelection();
	updateMenuBasePosition();
	updateMenuElementPositions();

	if (hasPressedAbortFlank()) {
		abortScreenHandling();
	}
}

Screen DreamTitleScreen = {
	.mLoad = loadTitleScreen,
	.mUpdate = updateTitleScreen,
};
