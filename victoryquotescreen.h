#pragma once

#include <prism/wrapper.h>

using namespace prism;

Screen* getVictoryQuoteScreen();
void setVictoryQuoteScreenQuoteIndex(int tIndex);
void setVictoryQuoteScreenFinishedCB(void(*tCB)());