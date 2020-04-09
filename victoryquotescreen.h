#pragma once

#include <prism/wrapper.h>

Screen* getVictoryQuoteScreen();
void setVictoryQuoteScreenQuoteIndex(int tIndex);
void setVictoryQuoteScreenFinishedCB(void(*tCB)());