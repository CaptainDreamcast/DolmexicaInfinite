#pragma once

#include <prism/stlutil.h>

#include <prism/wrapper.h>
#include <prism/geometry.h>
#include <prism/mugenanimationhandler.h>

typedef struct {
	int mID;
	int mAnimationID;

	int mIsBoundToStage;
	int mHasShadow;
	double mShadowBasePositionY;
	int mShadowAnimationID;

	int mIsDeleted;
} StoryAnimation;

typedef struct {
	int mID;
	Position mPosition;

	Position mTextOffset;
	int mTextID;

	int mHasBackground;
	Position mBackgroundOffset;
	MugenAnimation* mBackgroundAnimation;
	int mBackgroundAnimationID;

	int mHasFace;
	Position mFaceOffset;
	MugenAnimation* mFaceAnimation;
	int mFaceAnimationID;

	int mHasContinue;
	Position mContinueOffset; // TODO: sprites
	int mContinueAnimationID;

	int mHasName;
	Position mNameOffset;
	int mNameID;

	int mGoesToNextState;
	int mNextState;

	int mIsLockedOnToCharacter;
	Position mLockOffset;
	Position* mLockCharacterPositionReference;
	int mLockCharacterIsBoundToStage;

	int mHasFinished;
	int mIsDisabled;

} StoryText;

typedef struct {
	StoryAnimation mAnimation;

	MugenSpriteFile mSprites;
	MugenAnimations mAnimations;
} StoryCharacter;

typedef struct StoryInstance_t {
	std::map<int, StoryAnimation> mStoryAnimations;
	std::map<int, StoryText> mStoryTexts;
	std::map<int, StoryCharacter> mStoryCharacters;

	std::map<int, int> mIntVars;
	std::map<int, double> mFloatVars;
	std::map<int, std::string> mStringVars;

	std::map<std::string, int> mTextNames;


	int mStateMachineID;
	int mIsScheduledForDeletion;
	StoryInstance_t* mParent;
} StoryInstance;

Screen* getDolmexicaStoryScreen();

void setDolmexicaStoryScreenFile(char* tPath);

int isStoryCommandActive(const char* tCommand);

void addDolmexicaStoryAnimation(StoryInstance* tInstance, int tID, int tAnimation, Position tPosition);
void removeDolmexicaStoryAnimation(StoryInstance* tInstance, int tID);
void setDolmexicaStoryAnimationLooping(StoryInstance* tInstance, int tID, int tIsLooping);
void setDolmexicaStoryAnimationBoundToStage(StoryInstance* tInstance, int tID, int tIsBoundToStage);
void setDolmexicaStoryAnimationShadow(StoryInstance* tInstance, int tID, double tBasePositionY);
void changeDolmexicaStoryAnimation(StoryInstance* tInstance, int tID, int tAnimation);
void setDolmexicaStoryAnimationPositionX(StoryInstance* tInstance, int tID, double tX);
void setDolmexicaStoryAnimationPositionY(StoryInstance* tInstance, int tID, double tY);
void addDolmexicaStoryAnimationPositionX(StoryInstance* tInstance, int tID, double tX);
void addDolmexicaStoryAnimationPositionY(StoryInstance* tInstance, int tID, double tY);
void setDolmexicaStoryAnimationScaleX(StoryInstance* tInstance, int tID, double tX);
void setDolmexicaStoryAnimationScaleY(StoryInstance* tInstance, int tID, double tY);
void setDolmexicaStoryAnimationIsFacingRight(StoryInstance* tInstance, int tID, int tIsFacingRight);
void setDolmexicaStoryAnimationAngle(StoryInstance* tInstance, int tID, double tAngle);
void addDolmexicaStoryAnimationAngle(StoryInstance* tInstance, int tID, double tAngle);
void setDolmexicaStoryAnimationColor(StoryInstance* tInstance, int tID, Vector3D tColor);
void setDolmexicaStoryAnimationOpacity(StoryInstance* tInstance, int tID, double tOpacity);

void addDolmexicaStoryText(StoryInstance* tInstance, int tID, const char* tText, Vector3DI tFont, Position tBasePosition, Position tTextOffset, double tTextBoxWidth);
void removeDolmexicaStoryText(StoryInstance* tInstance, int tID);
void setDolmexicaStoryTextBackground(StoryInstance* tInstance, int tID, Vector3DI tSprite, Position tOffset);
void setDolmexicaStoryTextFace(StoryInstance* tInstance, int tID, Vector3DI tSprite, Position tOffset);
void setDolmexicaStoryTextName(StoryInstance* tInstance, int tID, const char* tText, Vector3DI tFont, Position tOffset);
void setDolmexicaStoryTextContinue(StoryInstance* tInstance, int tID, int tAnimation, Position tOffset);
double getDolmexicaStoryTextBasePositionX(StoryInstance* tInstance, int tID);
double getDolmexicaStoryTextBasePositionY(StoryInstance* tInstance, int tID);
void setDolmexicaStoryTextBasePosition(StoryInstance* tInstance, int tID, Position tPosition);
void setDolmexicaStoryTextText(StoryInstance* tInstance, int tID, const char* tText);
void setDolmexicaStoryTextTextOffset(StoryInstance* tInstance, int tID, Position tOffset);
void setDolmexicaStoryTextBackgroundSprite(StoryInstance* tInstance, int tID, Vector3DI tSprite);
void setDolmexicaStoryTextBackgroundOffset(StoryInstance* tInstance, int tID, Position tOffset);
void setDolmexicaStoryTextFaceSprite(StoryInstance* tInstance, int tID, Vector3DI tSprite);
void setDolmexicaStoryTextFaceOffset(StoryInstance* tInstance, int tID, Position tOffset);
void setDolmexicaStoryTextContinueAnimation(StoryInstance* tInstance, int tID, int tAnimation);
void setDolmexicaStoryTextContinueOffset(StoryInstance* tInstance, int tID, Position tOffset);
void setDolmexicaStoryTextNameText(StoryInstance* tInstance, int tID, const char* tText);
void setDolmexicaStoryTextNameOffset(StoryInstance* tInstance, int tID, Position tOffset);

void setDolmexicaStoryTextNextState(StoryInstance* tInstance, int tID, int tNextState);
void setDolmexicaStoryTextLockToCharacter(StoryInstance* tInstance, int tID, int tCharacterID, Position tOffset);
void setDolmexicaStoryTextLockToCharacter(StoryInstance* tInstance, int tID, int tCharacterID, Position tOffset, int tHelperID);
void setDolmexicaStoryTextInactive(StoryInstance* tInstance, int tID);
void setDolmexicaStoryTextBuiltUp(StoryInstance* tInstance, int tID);
void addDolmexicaStoryTextPositionX(StoryInstance* tInstance, int tID, double tX);
void addDolmexicaStoryTextPositionY(StoryInstance* tInstance, int tID, double tY);

void setDolmexicaStoryIDName(StoryInstance* tInstance, int tID, std::string tName);
int getDolmexicaStoryTextIDFromName(StoryInstance* tInstance, std::string tName);

void changeDolmexicaStoryState(StoryInstance* tInstance, int tNextState);
void endDolmexicaStoryboard(StoryInstance* tInstance, int tNextStoryState);


int getDolmexicaStoryTimeInState(StoryInstance* tInstance);

int getDolmexicaStoryAnimationTimeLeft(StoryInstance* tInstance, int tID);
double getDolmexicaStoryAnimationPositionX(StoryInstance* tInstance, int tID);

void addDolmexicaStoryCharacter(StoryInstance* tInstance, int tID, const char* tName, int tAnimation, Position tPosition);
void removeDolmexicaStoryCharacter(StoryInstance* tInstance, int tID);
void setDolmexicaStoryCharacterBoundToStage(StoryInstance* tInstance, int tID, int tIsBoundToStage);
void setDolmexicaStoryCharacterShadow(StoryInstance* tInstance, int tID, double tBasePositionY);
int getDolmexicaStoryCharacterAnimation(StoryInstance* tInstance, int tID);
void changeDolmexicaStoryCharacterAnimation(StoryInstance* tInstance, int tID, int tAnimation);
double getDolmexicaStoryCharacterPositionX(StoryInstance* tInstance, int tID);
void setDolmexicaStoryCharacterPositionX(StoryInstance* tInstance, int tID, double tX);
void setDolmexicaStoryCharacterPositionY(StoryInstance* tInstance, int tID, double tY);
void addDolmexicaStoryCharacterPositionX(StoryInstance* tInstance, int tID, double tX);
void addDolmexicaStoryCharacterPositionY(StoryInstance* tInstance, int tID, double tY);
void setDolmexicaStoryCharacterScaleX(StoryInstance* tInstance, int tID, double tX);
void setDolmexicaStoryCharacterScaleY(StoryInstance* tInstance, int tID, double tY);
void setDolmexicaStoryCharacterIsFacingRight(StoryInstance* tInstance, int tID, int tIsFacingRight);
void setDolmexicaStoryCharacterColor(StoryInstance* tInstance, int tID, Vector3D tColor);
void setDolmexicaStoryCharacterOpacity(StoryInstance* tInstance, int tID, double tOpacity);
void setDolmexicaStoryCharacterAngle(StoryInstance* tInstance, int tID, double tAngle);
void addDolmexicaStoryCharacterAngle(StoryInstance* tInstance, int tID, double tAngle);

int getDolmexicaStoryCharacterTimeLeft(StoryInstance* tInstance, int tID);

int getDolmexicaStoryIntegerVariable(StoryInstance* tInstance, int tID);
void setDolmexicaStoryIntegerVariable(StoryInstance* tInstance, int tID, int tValue);
void addDolmexicaStoryIntegerVariable(StoryInstance* tInstance, int tID, int tValue);

double getDolmexicaStoryFloatVariable(StoryInstance* tInstance, int tID);
void setDolmexicaStoryFloatVariable(StoryInstance* tInstance, int tID, double tValue);
void addDolmexicaStoryFloatVariable(StoryInstance* tInstance, int tID, double tValue);

std::string getDolmexicaStoryStringVariable(StoryInstance* tInstance, int tID);
void setDolmexicaStoryStringVariable(StoryInstance* tInstance, int tID, std::string tValue);
void addDolmexicaStoryStringVariable(StoryInstance* tInstance, int tID, std::string tValue);
void addDolmexicaStoryStringVariable(StoryInstance* tInstance, int tID, int tValue);


StoryInstance* getDolmexicaStoryRootInstance();
StoryInstance* getDolmexicaStoryInstanceParent(StoryInstance* tInstance);
StoryInstance* getDolmexicaStoryHelperInstance(int tID);
void addDolmexicaStoryHelper(int tID, int tState, StoryInstance* tParent);
int getDolmexicaStoryGetHelperAmount(int tID);
void removeDolmexicaStoryHelper(int tID);
void destroyDolmexicaStoryHelper(StoryInstance* tInstance);
int getDolmexicaStoryIDFromString(const char* tString, StoryInstance* tInstance);

void playDolmexicaStoryMusic(const std::string& tPath);
void stopDolmexicaStoryMusic();
void pauseDolmexicaStoryMusic();
void resumeDolmexicaStoryMusic();