#pragma once

#include <prism/wrapper.h>

Screen* getStoryScreen();

void setStoryDefinitionFile(char* tPath);
void setStoryScreenFinishedCB(void(*tCB)());