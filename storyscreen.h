#pragma once

#include <prism/wrapper.h>

using namespace prism;

Screen* getStoryScreen();

void setStoryDefinitionFileAndPrepareScreen(const char* tPath);
void setStoryScreenFinishedCB(void(*tCB)());