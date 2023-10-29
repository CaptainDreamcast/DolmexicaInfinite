#pragma once

#include <prism/wrapper.h>
#include <prism/mugendefreader.h>

Screen* getCharacterSelectScreen();

void setCharacterSelectCustomSelectFile(const std::string& tFileName);
void setCharacterSelectScreenModeName(const char* tModeName);
void setCharacterSelectFinishedCB(void(*tCB)());
void setCharacterSelectStageActive(int tUseOnlyOsuStages = 0);
void setCharacterSelectStageInactive();
void setCharacterSelectOnePlayer();
void setCharacterSelectOnePlayerSelectAll();
void setCharacterSelectTwoPlayers();
void setCharacterSelectNetplay(int isHost);
void setCharacterSelectCredits();
void setCharacterSelectStory();
void setCharacterSelectDisableReturnOneTime();
void resetCharacterSelectSelectors();

void parseOptionalCharacterSelectParameters(const MugenStringVector& tVector, int* oOrder, int* oDoesIncludeStage, char* oMusicPath, int* oIsValid);
void getCharacterSelectNamePath(const char* tName, char* oDst);

int setCharacterRandomAndReturnIfSuccessful(MugenDefScript* tScript, int i);
void setStageRandom(MugenDefScript* tScript);