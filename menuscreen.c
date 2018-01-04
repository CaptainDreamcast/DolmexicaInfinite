#include "menuscreen.h"

#include <tari/optionhandler.h>
#include <tari/screeneffect.h>
#include <tari/input.h>
#include <tari/mugenanimationhandler.h>

#include "fightscreen.h"
#include "titlescreen.h"
#include "twoplayerselectscreen.h"
#include "storyscreen.h"
#include "story.h"
#include "playerdefinition.h"
#include "gamelogic.h"

static struct {
	MugenSpriteFile mSprites;
	MugenAnimations mAnimations;

	int mBackgroundID;
	int mSelected;
} gData;

static void fadeOutFinished(void* tData) {
	Screen* screen = tData;
	setNewScreen(screen);
}

static void fadeOut(Screen* tScreen) {
	addFadeOut(30, fadeOutFinished, tScreen);
}

static void selectStoryOption() {
	resetDreamStory();

	setPlayerDefinitionPath(0, "assets/kfm/kfm.def");
	setPlayerDefinitionPath(1, "assets/kfm/kfm.def");
	setPlayerHuman(0);
	setPlayerArtificial(1);

	setDreamScreenAfterFightScreen(&DreamStoryScreen);

	fadeOut(&DreamFightScreen);
}

static void selectTwoPlayerOption() {
	fadeOut(&DreamTwoPlayerSelectScreen);
}

static void loadMenuScreen() {
	instantiateActor(getMugenAnimationHandlerActorBlueprint());

	gData.mSprites = loadMugenSpriteFileWithoutPalette("assets/menu/MENU.sff");
	gData.mAnimations = loadMugenAnimationFile("assets/menu/MENU.air");
	gData.mBackgroundID = addMugenAnimation(getMugenAnimation(&gData.mAnimations, 10+gData.mSelected), &gData.mSprites, makePosition(0, 0, 1));
	
	addFadeIn(30, NULL, NULL);
}

static void switchMode() {
	gData.mSelected ^= 1;

	changeMugenAnimation(gData.mBackgroundID, getMugenAnimation(&gData.mAnimations, 10+gData.mSelected));
}

static void selectMode() {
	if (gData.mSelected) {
		selectTwoPlayerOption();
	}
	else {
		selectStoryOption();
	}
}

static void updateMenuScreen() {
	if (hasPressedUpFlank() || hasPressedDownFlank() || hasPressedLeftFlank() || hasPressedRightFlank()) {
		switchMode();
	}

	if (hasPressedAFlank() || hasPressedStartFlank()) {
		selectMode();
	}
	
	if (hasPressedBFlank()) {
		fadeOut(&DreamTitleScreen);
	}

	if (hasPressedAbortFlank()) {
		setNewScreen(&DreamTitleScreen);
	}
}

Screen DreamMenuScreen = {
	.mLoad = loadMenuScreen,
	.mUpdate = updateMenuScreen,
};
