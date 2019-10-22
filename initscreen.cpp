#include "initscreen.h"

#include "intro.h"

static void loadInitScreen() {
	setNewScreen(startIntroFirstTimeAndReturnScreen());
}

static Screen gInitScreen;

Screen* getInitScreen()
{
	gInitScreen = makeScreen(loadInitScreen);
	return &gInitScreen;
}
