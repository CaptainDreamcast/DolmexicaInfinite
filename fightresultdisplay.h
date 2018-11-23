#pragma once

#include <prism/actorhandler.h>
#include <prism/geometry.h>

extern ActorBlueprint getFightResultDisplay();

void setFightResultActive(int tIsActive);
void setFightResultData(int tIsEnabled, char* tMessage, Vector3DI tFont, Position tOffset, int tTextDisplayTime, int tLayerNo, int tFadeInTime, int tShowTime, int tFadeOutTime, int tIsShowingWinPose);
void setFightResultMessage(char* tMessage);
void setFightResultIsShowingWinPose(int tIsShowingWinPose);


int isShowingFightResults();
void showFightResults(void(*tNextScreenCB)(void*), void(*tTitleScreenCB)(void*));