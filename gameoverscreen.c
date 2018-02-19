#include "gameoverscreen.h"

#include <prism/screeneffect.h>
#include <prism/animation.h>
#include <prism/input.h>

#include "titlescreen.h"

static struct {
	TextureData mTexture;
	int mAnimationID;

} gData;

static void loadGameOverScreen() {
	gData.mTexture = loadTexture("assets/main/fight/gameover/BG.pkg");
	gData.mAnimationID = playOneFrameAnimationLoop(makePosition(0,0,1), &gData.mTexture);
	addFadeIn(30, NULL, NULL);
}

static void goToTitleScreen(void* tCaller) {
	(void)tCaller;
	setNewScreen(&DreamTitleScreen);
}

static void updateGameOverScreen() {
	if (hasPressedStartFlank()) {
		addFadeOut(30, goToTitleScreen, NULL);
	}

	if (hasPressedAbortFlank()) {
		setNewScreen(&DreamTitleScreen);
	}
}

Screen DreamGameOverScreen = {
	.mLoad = loadGameOverScreen,
	.mUpdate = updateGameOverScreen,
};
