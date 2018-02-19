#pragma once

#include <prism/wrapper.h>

extern Screen CharacterSelectScreen;

void setCharacterSelectScreenModeName(char* tModeName);
void setCharacterSelectFinishedCB(void(*tCB)());