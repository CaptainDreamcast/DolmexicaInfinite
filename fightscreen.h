#pragma once

#include <stdio.h>
#include <prism/wrapper.h>

void startFightScreen(void(*tWinCB)(), void(*tLoseCB)() = NULL);
void reloadFightScreen();
void stopFightScreenWin();
void stopFightScreenLose();
void stopFightScreenToFixedScreen(Screen* tNextScreen);