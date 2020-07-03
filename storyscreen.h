#pragma once

#include <prism/wrapper.h>

Screen* getStoryScreen();

void setStoryDefinitionFileAndPrepareScreen(const char* tPath);
void setStoryScreenFinishedCB(void(*tCB)());