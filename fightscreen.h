#pragma once

#include <prism/wrapper.h>

void setFightScreenFinishedCBs(void(*tWinCB)(), void(*tLoseCB)());
void startFightScreen();
void stopFightScreenWin();
void stopFightScreenLose();
void stopFightScreenToFixedScreen(Screen* tNextScreen);