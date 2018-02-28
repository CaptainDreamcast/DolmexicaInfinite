#pragma once

#include <prism/wrapper.h>

void setFightScreenFinishedCB(void(*tCB)());
void startFightScreen();
void stopFightScreen();
void stopFightScreenToFixedScreen(Screen* tNextScreen);