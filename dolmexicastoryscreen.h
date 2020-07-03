#pragma once

#include <prism/stlutil.h>

#include <prism/wrapper.h>
#include <prism/geometry.h>
#include <prism/mugenanimationhandler.h>
#include <prism/mugensoundfilereader.h>

struct RegisteredMugenStateMachine;

typedef struct {
	int mID;
	MugenAnimationHandlerElement* mAnimationElement;

	int mIsBoundToStage;
	int mHasShadow;
	double mShadowBasePositionY;
	MugenAnimationHandlerElement* mShadowAnimationElement;

	int mIsDeleted;
} StoryAnimation;

typedef struct {
	int mID;
	Position mPosition;

	int mHasTextSound;
	Vector2DI mTextSound;
	int mTextSoundFrequency;

	Position2D mTextOffset;
	int mTextID;

	int mHasBackground;
	Position mBackgroundOffset;
	int mIsBackgroundAnimationOwned;
	MugenAnimation* mBackgroundAnimation;
	MugenAnimationHandlerElement* mBackgroundAnimationElement;

	int mHasFace;
	Position mFaceOffset;
	int mIsFaceAnimationOwned;
	MugenAnimation* mFaceAnimation;
	MugenAnimationHandlerElement* mFaceAnimationElement;

	int mHasContinue;
	Position mContinueOffset;
	int mIsContinueAnimationOwned;
	MugenAnimation* mContinueAnimation;
	MugenAnimationHandlerElement* mContinueAnimationElement;

	int mHasContinueSound;
	Vector2DI mContinueSound;

	int mHasName;
	Position2D mNameOffset;
	int mNameID;

	int mGoesToNextState;
	int mNextState;

	int mIsLockedOnToCharacter;
	Position2D mLockOffset;
	Position* mLockCharacterPositionReference;
	int mLockCharacterIsBoundToStage;

	int mHasFinished;
	int mIsDisabled;

} StoryText;

typedef struct {
	StoryAnimation mAnimation;

	MugenSpriteFile mSprites;
	MugenAnimations mAnimations;

	std::string mName;
} StoryCharacter;

struct StoryInstance {
	std::map<int, StoryAnimation> mStoryAnimations;
	std::map<int, StoryText> mStoryTexts;
	std::map<int, StoryCharacter> mStoryCharacters;

	std::map<int, int> mIntVars;
	std::map<int, double> mFloatVars;
	std::map<int, std::string> mStringVars;

	std::map<std::string, int> mTextNames;

	RegisteredMugenStateMachine* mRegisteredStateMachine;
	int mIsScheduledForDeletion;
	StoryInstance* mParent;
};

Screen* getDolmexicaStoryScreen();

void setDolmexicaStoryScreenFileAndPrepareScreen(const char* tPath);

MugenSounds* getDolmexicaStorySounds();
int isStoryCommandActive(const char* tCommand);

void addDolmexicaStoryAnimation(StoryInstance* tInstance, int tID, int tAnimation, const Position2D& tPosition);
void removeDolmexicaStoryAnimation(StoryInstance* tInstance, int tID);
int getDolmexicaStoryAnimationIsLooping(StoryInstance* tInstance, int tID);
void setDolmexicaStoryAnimationLooping(StoryInstance* tInstance, int tID, int tIsLooping);
int getDolmexicaStoryAnimationIsBoundToStage(StoryInstance* tInstance, int tID);
void setDolmexicaStoryAnimationBoundToStage(StoryInstance* tInstance, int tID, int tIsBoundToStage);
int getDolmexicaStoryAnimationHasShadow(StoryInstance* tInstance, int tID);
void setDolmexicaStoryAnimationShadow(StoryInstance* tInstance, int tID, double tBasePositionY);
int getDolmexicaStoryAnimationAnimation(StoryInstance* tInstance, int tID);
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
void setDolmexicaStoryAnimationColor(StoryInstance* tInstance, int tID, const Vector3D& tColor);
void setDolmexicaStoryAnimationOpacity(StoryInstance* tInstance, int tID, double tOpacity);

void addDolmexicaStoryText(StoryInstance* tInstance, int tID, const char* tText, const Vector3DI& tFont, const Position2D& tBasePosition, const Position2D& tTextOffset, double tTextBoxWidth);
void removeDolmexicaStoryText(StoryInstance* tInstance, int tID);
const char* getDolmexicaStoryTextText(StoryInstance* tInstance, int tID);
const char* getDolmexicaStoryTextDisplayedText(StoryInstance* tInstance, int tID);
const char* getDolmexicaStoryTextNameText(StoryInstance* tInstance, int tID);
int isDolmexicaStoryTextVisible(StoryInstance* tInstance, int tID);
void setDolmexicaStoryTextBackground(StoryInstance* tInstance, int tID, const Vector2DI& tSprite, const Position2D& tOffset);
void setDolmexicaStoryTextBackground(StoryInstance* tInstance, int tID, int tAnimation, const Position2D& tOffset);
void setDolmexicaStoryTextFace(StoryInstance* tInstance, int tID, const Vector2DI& tSprite, const Position2D& tOffset);
void setDolmexicaStoryTextFace(StoryInstance* tInstance, int tID, int tAnimation, const Position2D& tOffset);
void setDolmexicaStoryTextName(StoryInstance* tInstance, int tID, const char* tText, const Vector3DI& tFont, const Position2D& tOffset);
void setDolmexicaStoryTextContinue(StoryInstance* tInstance, int tID, const Vector2DI& tSprite, const Position2D& tOffset);
void setDolmexicaStoryTextContinue(StoryInstance* tInstance, int tID, int tAnimation, const Position2D& tOffset);
double getDolmexicaStoryTextBasePositionX(StoryInstance* tInstance, int tID);
double getDolmexicaStoryTextBasePositionY(StoryInstance* tInstance, int tID);
void setDolmexicaStoryTextBasePosition(StoryInstance* tInstance, int tID, const Position2D& tPosition);
void setDolmexicaStoryTextText(StoryInstance* tInstance, int tID, const char* tText);
void setDolmexicaStoryTextSound(StoryInstance* tInstance, int tID, const Vector2DI& tSound);
void setDolmexicaStoryTextSoundFrequency(StoryInstance* tInstance, int tID, int tSoundFrequency);
void setDolmexicaStoryTextTextOffset(StoryInstance* tInstance, int tID, const Position2D& tOffset);
void setDolmexicaStoryTextBackgroundSprite(StoryInstance* tInstance, int tID, const Vector2DI& tSprite);
void setDolmexicaStoryTextBackgroundOffset(StoryInstance* tInstance, int tID, const Position2D& tOffset);
void setDolmexicaStoryTextFaceSprite(StoryInstance* tInstance, int tID, const Vector2DI& tSprite);
void setDolmexicaStoryTextFaceOffset(StoryInstance* tInstance, int tID, const Position2D& tOffset);
void setDolmexicaStoryTextContinueAnimation(StoryInstance* tInstance, int tID, int tAnimation);
void setDolmexicaStoryTextContinueSound(StoryInstance* tInstance, int tID, const Vector2DI& tSound);
void setDolmexicaStoryTextContinueOffset(StoryInstance* tInstance, int tID, const Position2D& tOffset);
void setDolmexicaStoryTextNameText(StoryInstance* tInstance, int tID, const char* tText);
void setDolmexicaStoryTextNameFont(StoryInstance* tInstance, int tID, const Vector3DI& tFont);
void setDolmexicaStoryTextNameOffset(StoryInstance* tInstance, int tID, const Position2D& tOffset);

int getDolmexicaStoryTextNextState(StoryInstance* tInstance, int tID);
void setDolmexicaStoryTextNextState(StoryInstance* tInstance, int tID, int tNextState);
void setDolmexicaStoryTextLockToCharacter(StoryInstance* tInstance, int tID, int tCharacterID, const Position2D& tOffset);
void setDolmexicaStoryTextLockToCharacter(StoryInstance* tInstance, int tID, int tCharacterID, const Position2D& tOffset, int tHelperID);
void setDolmexicaStoryTextInactive(StoryInstance* tInstance, int tID);
int isDolmexicaStoryTextBuiltUp(StoryInstance* tInstance, int tID);
void setDolmexicaStoryTextBuiltUp(StoryInstance* tInstance, int tID);
void setDolmexicaStoryTextPositionX(StoryInstance* tInstance, int tID, double tX);
void setDolmexicaStoryTextPositionY(StoryInstance* tInstance, int tID, double tY);
void addDolmexicaStoryTextPositionX(StoryInstance* tInstance, int tID, double tX);
void addDolmexicaStoryTextPositionY(StoryInstance* tInstance, int tID, double tY);

void setDolmexicaStoryIDName(StoryInstance* tInstance, int tID, const std::string& tName);
int getDolmexicaStoryTextIDFromName(StoryInstance* tInstance, const std::string& tName);

void changeDolmexicaStoryState(StoryInstance* tInstance, int tNextState);
void changeDolmexicaStoryStateOutsideStateHandler(StoryInstance* tInstance, int tNextState);
void endDolmexicaStoryboard(StoryInstance* tInstance, int tNextStoryState);

int getDolmexicaStoryTimeInState(StoryInstance* tInstance);
int getDolmexicaStoryStateNumber(StoryInstance* tInstance);

int getDolmexicaStoryAnimationTimeLeft(StoryInstance* tInstance, int tID);
double getDolmexicaStoryAnimationPositionX(StoryInstance* tInstance, int tID);
double getDolmexicaStoryAnimationPositionY(StoryInstance* tInstance, int tID);

void addDolmexicaStoryCharacter(StoryInstance* tInstance, int tID, const char* tName, int tPreferredPalette, int tAnimation, const Position2D& tPosition);
void removeDolmexicaStoryCharacter(StoryInstance* tInstance, int tID);
int getDolmexicaStoryCharacterIsBoundToStage(StoryInstance* tInstance, int tID);
void setDolmexicaStoryCharacterBoundToStage(StoryInstance* tInstance, int tID, int tIsBoundToStage);
int getDolmexicaStoryCharacterHasShadow(StoryInstance* tInstance, int tID);
void setDolmexicaStoryCharacterShadow(StoryInstance* tInstance, int tID, double tBasePositionY);
int getDolmexicaStoryCharacterAnimation(StoryInstance* tInstance, int tID);
void changeDolmexicaStoryCharacterAnimation(StoryInstance* tInstance, int tID, int tAnimation);
double getDolmexicaStoryCharacterPositionX(StoryInstance* tInstance, int tID);
double getDolmexicaStoryCharacterPositionY(StoryInstance* tInstance, int tID);
void setDolmexicaStoryCharacterPositionX(StoryInstance* tInstance, int tID, double tX);
void setDolmexicaStoryCharacterPositionY(StoryInstance* tInstance, int tID, double tY);
void addDolmexicaStoryCharacterPositionX(StoryInstance* tInstance, int tID, double tX);
void addDolmexicaStoryCharacterPositionY(StoryInstance* tInstance, int tID, double tY);
void setDolmexicaStoryCharacterScaleX(StoryInstance* tInstance, int tID, double tX);
void setDolmexicaStoryCharacterScaleY(StoryInstance* tInstance, int tID, double tY);
void setDolmexicaStoryCharacterIsFacingRight(StoryInstance* tInstance, int tID, int tIsFacingRight);
void setDolmexicaStoryCharacterColor(StoryInstance* tInstance, int tID, const Vector3D& tColor);
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
void setDolmexicaStoryStringVariable(StoryInstance* tInstance, int tID, const std::string& tValue);
void addDolmexicaStoryStringVariable(StoryInstance* tInstance, int tID, const std::string& tValue);
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

void setDolmexicaStoryCameraFocusX(double x);
void setDolmexicaStoryCameraFocusY(double y);
void setDolmexicaStoryCameraZoom(double tScale);

int getDolmexicaStoryCoordinateP();