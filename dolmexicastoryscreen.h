#pragma once

#include <prism/wrapper.h>
#include <prism/geometry.h>

extern Screen DolmexicaStoryScreen;

void setDolmexicaStoryScreenFile(char* tPath);

void addDolmexicaStoryAnimation(int tID, int tAnimation, Position tPosition);
void removeDolmexicaStoryAnimation(int tID);
void setDolmexicaStoryAnimationLooping(int tID, int tIsLooping);
void changeDolmexicaStoryAnimation(int tID, int tAnimation);
void setDolmexicaStoryAnimationPositionX(int tID, double tX);
void setDolmexicaStoryAnimationPositionY(int tID, double tY);
void addDolmexicaStoryAnimationPositionX(int tID, double tX);
void addDolmexicaStoryAnimationPositionY(int tID, double tY);
void setDolmexicaStoryAnimationIsFacingRight(int tID, int tIsFacingRight);

void addDolmexicaStoryText(int tID, char* tText, Vector3DI tFont, Position tPosition, double tTextBoxWidth);
void removeDolmexicaStoryText(int tID);
void setDolmexicaStoryTextBackground(int tID, Vector3DI tSprite, Position tOffset);
void setDolmexicaStoryTextFace(int tID, Vector3DI tSprite, Position tOffset);
void setDolmexicaStoryTextPosition(int tID, Position tPosition);
void setDolmexicaStoryTextText(int tID, char* tText);
void setDolmexicaStoryTextBackgroundSprite(int tID, Vector3DI tSprite);
void setDolmexicaStoryTextBackgroundOffset(int tID, Position tOffset);
void setDolmexicaStoryTextFaceSprite(int tID, Vector3DI tSprite);
void setDolmexicaStoryTextFaceOffset(int tID, Position tOffset);
void setDolmexicaStoryTextNextState(int tID, int tNextState);

void changeDolmexicaStoryState(int tNextState);

int getDolmexicaStoryTimeInState();

int getDolmexicaStoryAnimationTimeLeft(int tID);