#pragma once

#include <prism/wrapper.h>

Screen* getVersusScreen();

void setVersusScreenFinishedCB(void(*tCB)());
void setVersusScreenMatchNumber(int tRound);
void setVersusScreenNoMatchNumber();