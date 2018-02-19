#pragma once

#include <prism/wrapper.h>

extern Screen StoryScreen;

void setStoryDefinitionFile(char* tPath);
void setStoryScreenFinishedCB(void(*tCB)());