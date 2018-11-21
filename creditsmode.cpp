#include "creditsmode.h"

#include <prism/log.h>

#include "characterselectscreen.h"
#include "titlescreen.h"

static void characterSelectFinishedCB() {
	setNewScreen(&DreamTitleScreen);
}

void startCreditsMode()
{
	setCharacterSelectScreenModeName("Credits");
	setCharacterSelectCredits();
	setCharacterSelectStageActive();
	setCharacterSelectFinishedCB(characterSelectFinishedCB);
	setNewScreen(&CharacterSelectScreen);
}
