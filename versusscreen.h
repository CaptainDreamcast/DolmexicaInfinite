#pragma once

#include <prism/wrapper.h>

using namespace prism;

Screen* getVersusScreen();

void setVersusScreenFinishedCB(void(*tCB)());
void setVersusScreenMatchNumber(int tRound);
void setVersusScreenNoMatchNumber();