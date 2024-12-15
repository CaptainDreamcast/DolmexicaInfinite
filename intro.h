#pragma once

#include <prism/wrapper.h>

using namespace prism;

Screen* startIntroFirstTimeAndReturnScreen();

int hasLogoStoryboard();
void playLogoStoryboard();

int hasIntroStoryboard();
int hasFinishedIntroWaitCycle();
void increaseIntroWaitCycle();
void playIntroStoryboard();