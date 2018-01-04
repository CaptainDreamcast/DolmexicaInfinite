#include "twoplayerselectscreen.h"

#include <tari/geometry.h>
#include <tari/animation.h>
#include <tari/input.h>
#include <tari/screeneffect.h>
#include <tari/texthandler.h>
#include <tari/math.h>
#include <tari/mugenanimationhandler.h>

#include "titlescreen.h"
#include "fightscreen.h"
#include "playerdefinition.h"
#include "gamelogic.h"
#include "menuscreen.h"

typedef struct {
	Position mPosition;
	int mAnimationID;
	TextureData mTexture;

	char mName[1024];
	char mDefinitionPath[1024];

} SelectScreenCharacter;

typedef struct {
	int mSelected;
	Position mPosition;
	int mAnimationID;
	TextureData mTexture;

	int mHasFinishedSelection;
	char mSelectionText[1024];
	int mSelectionTextID;

	int mPortraitID;
} SelectScreenSelector;

static struct {
	MugenSpriteFile mSprites;
	MugenAnimations mAnimations;

	int mCharacterAmount;
	SelectScreenCharacter mCharacters[10];

	SelectScreenSelector mSelectors[2];

	TextureData mHalfSelector;
} gData;

static void addCharacter(char* tDefinitionPath, char* tPortraitPath, char* tName, Position tPosition) {
	int i = gData.mCharacterAmount;
	
	gData.mCharacters[i].mPosition = tPosition;
	gData.mCharacters[i].mTexture = loadTexture(tPortraitPath);
	gData.mCharacters[i].mAnimationID = playOneFrameAnimationLoop(tPosition, &gData.mCharacters[i].mTexture);
	strcpy(gData.mCharacters[i].mName, tName);
	strcpy(gData.mCharacters[i].mDefinitionPath, tDefinitionPath);

	gData.mCharacterAmount++;
}


static void updateSelectorDisplay(int i) {
	gData.mSelectors[i].mPosition = gData.mCharacters[gData.mSelectors[i].mSelected].mPosition;
	gData.mSelectors[i].mPosition.z += i + 1;

	sprintf(gData.mSelectors[i].mSelectionText, "Player %d selection: %s", i + 1, gData.mCharacters[gData.mSelectors[i].mSelected].mName);
	setHandledText(gData.mSelectors[i].mSelectionTextID, gData.mSelectors[i].mSelectionText);

	changeMugenAnimation(gData.mSelectors[i].mPortraitID, getMugenAnimation(&gData.mAnimations, 20+ gData.mSelectors[i].mSelected));

	if (gData.mSelectors[0].mHasFinishedSelection || gData.mSelectors[1].mHasFinishedSelection) return;

	if (gData.mSelectors[0].mSelected == gData.mSelectors[1].mSelected) {
		changeAnimation(gData.mSelectors[1].mAnimationID, &gData.mHalfSelector, createOneFrameAnimation(), makeRectangleFromTexture(gData.mHalfSelector));
	}
	else {
		changeAnimation(gData.mSelectors[1].mAnimationID, &gData.mSelectors[1].mTexture, createOneFrameAnimation(), makeRectangleFromTexture(gData.mSelectors[1].mTexture));
	}
}


static void loadSelector(int i, Position tTextPosition) {
	SelectScreenSelector* selector = &gData.mSelectors[i];

	double portraitX;
	int portraitIsFacingRight;
	if (i) {
		selector->mSelected = gData.mCharacterAmount - 1;
		portraitX = 640;
		portraitIsFacingRight = 0;
	}
	else {
		selector->mSelected = 0;
		portraitX = 0;
		portraitIsFacingRight = 1;
	}
	selector->mPosition = gData.mCharacters[i].mPosition;
	selector->mPosition.z+=i+1;
	
	char path[1024];
	sprintf(path, "assets/selectscreen/selector%d.pkg", i);
	selector->mTexture = loadTexture(path);

	selector->mAnimationID = playOneFrameAnimationLoop(makePosition(0,0,0), &selector->mTexture);
	setAnimationBasePositionReference(selector->mAnimationID, &selector->mPosition);

	selector->mSelectionTextID = addHandledText(tTextPosition, "", 0, COLOR_WHITE, makePosition(20, 20, 1), makePosition(-5, -5, 0), makePosition(INF, INF, 0), INF);

	selector->mPortraitID = addMugenAnimation(getMugenAnimation(&gData.mAnimations, 20+selector->mSelected), &gData.mSprites, makePosition(portraitX, 480, 2));
	setMugenAnimationFaceDirection(selector->mPortraitID, portraitIsFacingRight);

	selector->mHasFinishedSelection = 0;
}

static void loadSelectScreen() {
	instantiateActor(getMugenAnimationHandlerActorBlueprint());

	gData.mSprites = loadMugenSpriteFileWithoutPalette("assets/selectscreen/SELECT.sff");
	gData.mAnimations = loadMugenAnimationFile("assets/selectscreen/SELECT.air");

	addMugenAnimation(getMugenAnimation(&gData.mAnimations, 10), &gData.mSprites, makePosition(0, 0, 1));
	addMugenAnimation(getMugenAnimation(&gData.mAnimations, 11), &gData.mSprites, makePosition(35, 329, 3));

	gData.mCharacterAmount = 0;
	addCharacter("assets/kfm/kfm.def", "assets/portraits/kfm.pkg", "Kung-fu Man", makePosition(80, 150, 5));
	addCharacter("assets/Beat/Beat.def", "assets/portraits/beat.pkg", "Beat", makePosition(240, 150, 5));
	addCharacter("assets/Sonicth/Sonicth.def", "assets/portraits/sonic.pkg", "Sonic", makePosition(400, 150, 5));

	gData.mHalfSelector = loadTexture("assets/selectscreen/selectorhalf.pkg");
	
	loadSelector(0, makePosition(50, 350, 5));
	loadSelector(1, makePosition(50, 390, 5));

	int i;
	for (i = 0; i < 2; i++) {
		updateSelectorDisplay(i);
	}

	addFadeIn(30, NULL, NULL);
}

static void goToFight(void* tCaller) {
	(void)tCaller;
	
	int i;
	for (i = 0; i < 2; i++) {
		setPlayerDefinitionPath(i, gData.mCharacters[gData.mSelectors[i].mSelected].mDefinitionPath);
	}
	setDreamScreenAfterFightScreen(&DreamTwoPlayerSelectScreen);
	setPlayerHuman(0);
	setPlayerHuman(1);

	setNewScreen(&DreamFightScreen);
}

static void goToMenu(void* tCaller) {
	(void)tCaller;

	setNewScreen(&DreamMenuScreen);
}

static void finalizeSelection(int i) {
	if (!gData.mSelectors[1].mHasFinishedSelection && gData.mSelectors[0].mSelected == gData.mSelectors[1].mSelected) {
		changeAnimation(gData.mSelectors[1].mAnimationID, &gData.mSelectors[1].mTexture, createOneFrameAnimation(), makeRectangleFromTexture(gData.mSelectors[1].mTexture));
	}
	setAnimationScale(gData.mSelectors[i].mAnimationID, makePosition(0, 0, 0), makePosition(0, 0, 0));

	sprintf(gData.mSelectors[i].mSelectionText, "Player %d selection: %s!", i + 1, gData.mCharacters[gData.mSelectors[i].mSelected].mName);
	setHandledText(gData.mSelectors[i].mSelectionTextID, gData.mSelectors[i].mSelectionText);

	gData.mSelectors[i].mHasFinishedSelection = 1;

	if (gData.mSelectors[0].mHasFinishedSelection && gData.mSelectors[1].mHasFinishedSelection) {
		addFadeOut(30, goToFight, NULL);
	}
}

static void updateSinglePlayer(int i) {
	if (gData.mSelectors[i].mHasFinishedSelection) return;

	if (hasPressedLeftFlankSingle(i)) {
		gData.mSelectors[i].mSelected--;
		if (gData.mSelectors[i].mSelected < 0) gData.mSelectors[i].mSelected += gData.mCharacterAmount;
		updateSelectorDisplay(i);
	}

	if (hasPressedRightFlankSingle(i)) {
		gData.mSelectors[i].mSelected = (gData.mSelectors[i].mSelected + 1) % gData.mCharacterAmount;
		updateSelectorDisplay(i);
	}

	if (hasPressedBFlankSingle(i)) {
		addFadeOut(30, goToMenu, NULL);
	}

	if (hasPressedAFlankSingle(i)) {
		finalizeSelection(i);
	}
}

static void updateSelectScreen() {
	int i;
	for (i = 0; i < 2; i++) {
		updateSinglePlayer(i);
	}

	if (hasPressedAbortFlank()) {
		setNewScreen(&DreamTitleScreen);
	}	
}

Screen DreamTwoPlayerSelectScreen = {
	.mLoad = loadSelectScreen,
	.mUpdate = updateSelectScreen,
};