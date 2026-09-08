#pragma once

#include <prism/stlutil.h>

#include <prism/wrapper.h>
#include <prism/geometry.h>
#include <prism/mugenanimationhandler.h>
#include <prism/mugensoundfilereader.h>

using namespace prism;

#define DOLMEXICA_STORY_TEXT_BACKGROUND_DEFAULT_Z_DELTA		-2
#define DOLMEXICA_STORY_TEXT_FACE_DEFAULT_Z_DELTA			-1
#define DOLMEXICA_STORY_TEXT_CONTINUE_DEFAULT_Z_DELTA			-1

struct RegisteredMugenStateMachine;

typedef struct {
	int mID;
	MugenAnimationHandlerElement* mAnimationElement;

	int mIsBoundToStage;
	int mHasShadow;
	float mShadowBasePositionY;
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
	std::unordered_map<int, StoryAnimation> mStoryAnimations;
	std::unordered_map<int, StoryText> mStoryTexts;
	std::unordered_map<int, StoryCharacter> mStoryCharacters;

	std::unordered_map<int, int> mIntVars;
	std::unordered_map<int, float> mFloatVars;
	std::unordered_map<int, std::string> mStringVars;

	std::unordered_map<std::string, int> mTextNames;

	RegisteredMugenStateMachine* mRegisteredStateMachine;
	int mIsScheduledForDeletion;
	StoryInstance* mParent;
};

Screen* getDolmexicaStoryScreen();
ActorBlueprint getDolmexicaStoryActor();

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
void setDolmexicaStoryAnimationShadow(StoryInstance* tInstance, int tID, float tBasePositionY);
int getDolmexicaStoryAnimationAnimation(StoryInstance* tInstance, int tID);
void changeDolmexicaStoryAnimation(StoryInstance* tInstance, int tID, int tAnimation);
void setDolmexicaStoryAnimationPositionX(StoryInstance* tInstance, int tID, float tX);
void setDolmexicaStoryAnimationPositionY(StoryInstance* tInstance, int tID, float tY);
void addDolmexicaStoryAnimationPositionX(StoryInstance* tInstance, int tID, float tX);
void addDolmexicaStoryAnimationPositionY(StoryInstance* tInstance, int tID, float tY);
void setDolmexicaStoryAnimationStagePositionX(StoryInstance* tInstance, int tID, float tX);
void setDolmexicaStoryAnimationStagePositionY(StoryInstance* tInstance, int tID, float tY);
void setDolmexicaStoryAnimationScaleX(StoryInstance* tInstance, int tID, float tX);
void setDolmexicaStoryAnimationScaleY(StoryInstance* tInstance, int tID, float tY);
void setDolmexicaStoryAnimationIsFacingRight(StoryInstance* tInstance, int tID, int tIsFacingRight);
void setDolmexicaStoryAnimationAngle(StoryInstance* tInstance, int tID, float tAngle);
void addDolmexicaStoryAnimationAngle(StoryInstance* tInstance, int tID, float tAngle);
void setDolmexicaStoryAnimationColor(StoryInstance* tInstance, int tID, const Vector3D& tColor);
void setDolmexicaStoryAnimationOpacity(StoryInstance* tInstance, int tID, float tOpacity);

void addDolmexicaStoryText(StoryInstance* tInstance, int tID, const char* tText, const Vector3DI& tFont, float tScale, const Position2D& tBasePosition, const Position2D& tTextOffset, float tTextBoxWidth);
void removeDolmexicaStoryText(StoryInstance* tInstance, int tID);
const char* getDolmexicaStoryTextText(StoryInstance* tInstance, int tID);
const char* getDolmexicaStoryTextDisplayedText(StoryInstance* tInstance, int tID);
const char* getDolmexicaStoryTextNameText(StoryInstance* tInstance, int tID);
int isDolmexicaStoryTextVisible(StoryInstance* tInstance, int tID);
void setDolmexicaStoryTextBackground(StoryInstance* tInstance, int tID, const Vector2DI& tSprite, const Position& tOffset, const Vector2D& tScale);
void setDolmexicaStoryTextBackground(StoryInstance* tInstance, int tID, int tAnimation, const Position& tOffset, const Vector2D& tScale);
void setDolmexicaStoryTextFace(StoryInstance* tInstance, int tID, const Vector2DI& tSprite, const Position& tOffset, const Vector2D& tScale);
void setDolmexicaStoryTextFace(StoryInstance* tInstance, int tID, int tAnimation, const Position& tOffset, const Vector2D& tScale);
void setDolmexicaStoryTextName(StoryInstance* tInstance, int tID, const char* tText, const Vector3DI& tFont, const Position2D& tOffset, float tScale);
void setDolmexicaStoryTextContinue(StoryInstance* tInstance, int tID, const Vector2DI& tSprite, const Position& tOffset, const Vector2D& tScale);
void setDolmexicaStoryTextContinue(StoryInstance* tInstance, int tID, int tAnimation, const Position& tOffset, const Vector2D& tScale);
float getDolmexicaStoryTextBasePositionX(StoryInstance* tInstance, int tID);
float getDolmexicaStoryTextBasePositionY(StoryInstance* tInstance, int tID);
void setDolmexicaStoryTextBasePosition(StoryInstance* tInstance, int tID, const Position2D& tPosition);
void setDolmexicaStoryTextText(StoryInstance* tInstance, int tID, const char* tText);
void setDolmexicaStoryTextFont(StoryInstance* tInstance, int tID, const Vector3DI& tFont);
void setDolmexicaStoryTextScale(StoryInstance* tInstance, int tID, float tScale);
void setDolmexicaStoryTextSound(StoryInstance* tInstance, int tID, const Vector2DI& tSound);
void setDolmexicaStoryTextSoundFrequency(StoryInstance* tInstance, int tID, int tSoundFrequency);
void setDolmexicaStoryTextTextOffset(StoryInstance* tInstance, int tID, const Position2D& tOffset);
void setDolmexicaStoryTextBackgroundSprite(StoryInstance* tInstance, int tID, const Vector2DI& tSprite);
void setDolmexicaStoryTextBackgroundOffset(StoryInstance* tInstance, int tID, const Position& tOffset);
void setDolmexicaStoryTextBackgroundScale(StoryInstance* tInstance, int tID, const Vector2D& tScale);
void setDolmexicaStoryTextFaceSprite(StoryInstance* tInstance, int tID, const Vector2DI& tSprite);
void setDolmexicaStoryTextFaceOffset(StoryInstance* tInstance, int tID, const Position& tOffset);
void setDolmexicaStoryTextFaceScale(StoryInstance* tInstance, int tID, const Vector2D& tScale);
void setDolmexicaStoryTextContinueAnimation(StoryInstance* tInstance, int tID, int tAnimation);
void setDolmexicaStoryTextContinueSound(StoryInstance* tInstance, int tID, const Vector2DI& tSound);
void setDolmexicaStoryTextContinueOffset(StoryInstance* tInstance, int tID, const Position& tOffset);
void setDolmexicaStoryTextContinueScale(StoryInstance* tInstance, int tID, const Vector2D& tScale);
void setDolmexicaStoryTextNameText(StoryInstance* tInstance, int tID, const char* tText);
void setDolmexicaStoryTextNameFont(StoryInstance* tInstance, int tID, const Vector3DI& tFont);
void setDolmexicaStoryTextNameOffset(StoryInstance* tInstance, int tID, const Position2D& tOffset);
void setDolmexicaStoryTextNameScale(StoryInstance* tInstance, int tID, float tScale);

int getDolmexicaStoryTextNextState(StoryInstance* tInstance, int tID);
void setDolmexicaStoryTextNextState(StoryInstance* tInstance, int tID, int tNextState);
void setDolmexicaStoryTextLockToCharacter(StoryInstance* tInstance, int tID, int tCharacterID, const Position2D& tOffset);
void setDolmexicaStoryTextLockToCharacter(StoryInstance* tInstance, int tID, int tCharacterID, const Position2D& tOffset, int tHelperID);
void setDolmexicaStoryTextInactive(StoryInstance* tInstance, int tID);
int isDolmexicaStoryTextBuiltUp(StoryInstance* tInstance, int tID);
void setDolmexicaStoryTextBuiltUp(StoryInstance* tInstance, int tID);
void setDolmexicaStoryTextPositionX(StoryInstance* tInstance, int tID, float tX);
void setDolmexicaStoryTextPositionY(StoryInstance* tInstance, int tID, float tY);
void addDolmexicaStoryTextPositionX(StoryInstance* tInstance, int tID, float tX);
void addDolmexicaStoryTextPositionY(StoryInstance* tInstance, int tID, float tY);

void setDolmexicaStoryIDName(StoryInstance* tInstance, int tID, const std::string& tName);
int getDolmexicaStoryTextIDFromName(StoryInstance* tInstance, const std::string& tName);

void changeDolmexicaStoryState(StoryInstance* tInstance, int tNextState);
void changeDolmexicaStoryStateOutsideStateHandler(StoryInstance* tInstance, int tNextState);
void endDolmexicaStoryboard(StoryInstance* tInstance, int tNextStoryState);

int getDolmexicaStoryTimeInState(StoryInstance* tInstance);
int getDolmexicaStoryStateNumber(StoryInstance* tInstance);

int getDolmexicaStoryAnimationTimeLeft(StoryInstance* tInstance, int tID);
float getDolmexicaStoryAnimationPositionX(StoryInstance* tInstance, int tID);
float getDolmexicaStoryAnimationPositionY(StoryInstance* tInstance, int tID);
float getDolmexicaStoryAnimationScreenPositionX(StoryInstance* tInstance, int tID);
float getDolmexicaStoryAnimationScreenPositionY(StoryInstance* tInstance, int tID);
float getDolmexicaStoryAnimationStagePositionX(StoryInstance* tInstance, int tID);
float getDolmexicaStoryAnimationStagePositionY(StoryInstance* tInstance, int tID);

void addDolmexicaStoryCharacter(StoryInstance* tInstance, int tID, const char* tName, int tPreferredPalette, int tAnimation, const Position2D& tPosition);
void removeDolmexicaStoryCharacter(StoryInstance* tInstance, int tID);
int getDolmexicaStoryCharacterIsBoundToStage(StoryInstance* tInstance, int tID);
void setDolmexicaStoryCharacterBoundToStage(StoryInstance* tInstance, int tID, int tIsBoundToStage);
int getDolmexicaStoryCharacterHasShadow(StoryInstance* tInstance, int tID);
void setDolmexicaStoryCharacterShadow(StoryInstance* tInstance, int tID, float tBasePositionY);
int getDolmexicaStoryCharacterAnimation(StoryInstance* tInstance, int tID);
void changeDolmexicaStoryCharacterAnimation(StoryInstance* tInstance, int tID, int tAnimation);
float getDolmexicaStoryCharacterPositionX(StoryInstance* tInstance, int tID);
float getDolmexicaStoryCharacterPositionY(StoryInstance* tInstance, int tID);
float getDolmexicaStoryCharacterScreenPositionX(StoryInstance* tInstance, int tID);
float getDolmexicaStoryCharacterScreenPositionY(StoryInstance* tInstance, int tID);
float getDolmexicaStoryCharacterStagePositionX(StoryInstance* tInstance, int tID);
float getDolmexicaStoryCharacterStagePositionY(StoryInstance* tInstance, int tID);
void setDolmexicaStoryCharacterPositionX(StoryInstance* tInstance, int tID, float tX);
void setDolmexicaStoryCharacterPositionY(StoryInstance* tInstance, int tID, float tY);
void setDolmexicaStoryCharacterPositionZ(StoryInstance* tInstance, int tID, float tZ);
void addDolmexicaStoryCharacterPositionX(StoryInstance* tInstance, int tID, float tX);
void addDolmexicaStoryCharacterPositionY(StoryInstance* tInstance, int tID, float tY);
void setDolmexicaStoryCharacterStagePositionX(StoryInstance* tInstance, int tID, float tX);
void setDolmexicaStoryCharacterStagePositionY(StoryInstance* tInstance, int tID, float tY);
void setDolmexicaStoryCharacterScaleX(StoryInstance* tInstance, int tID, float tX);
void setDolmexicaStoryCharacterScaleY(StoryInstance* tInstance, int tID, float tY);
void setDolmexicaStoryCharacterIsFacingRight(StoryInstance* tInstance, int tID, int tIsFacingRight);
void setDolmexicaStoryCharacterColor(StoryInstance* tInstance, int tID, const Vector3D& tColor);
void setDolmexicaStoryCharacterOpacity(StoryInstance* tInstance, int tID, float tOpacity);
void setDolmexicaStoryCharacterAngle(StoryInstance* tInstance, int tID, float tAngle);
void addDolmexicaStoryCharacterAngle(StoryInstance* tInstance, int tID, float tAngle);

int getDolmexicaStoryCharacterTimeLeft(StoryInstance* tInstance, int tID);

int getDolmexicaStoryIntegerVariable(StoryInstance* tInstance, int tID);
void setDolmexicaStoryIntegerVariable(StoryInstance* tInstance, int tID, int tValue);
void addDolmexicaStoryIntegerVariable(StoryInstance* tInstance, int tID, int tValue);

float getDolmexicaStoryFloatVariable(StoryInstance* tInstance, int tID);
void setDolmexicaStoryFloatVariable(StoryInstance* tInstance, int tID, float tValue);
void addDolmexicaStoryFloatVariable(StoryInstance* tInstance, int tID, float tValue);

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

void setDolmexicaStoryCameraFocusX(float x);
void setDolmexicaStoryCameraFocusY(float y);
void setDolmexicaStoryCameraZoom(float tScale);

int getDolmexicaStoryCoordinateP();

void setDolmexicaStoryDebugStartState(int tFromState, int tDebugStartState);

#ifdef _WIN32
enum class StoryEditorObjectKind {
	Character = 0,
	Animation = 1,
	Text = 2,
};
enum class StoryEditorStampKind {
	Creation = 0,
	ChangeText = 1,
	LockText = 2,
	ChangeAnim = 3, // ChangeAnim / CharChangeAnim
	SetFacing = 4, // AnimSetFacing / CharSetFacing
	SetScale = 5, // AnimScaleSet / CharScaleSet
};
void storyEditorStampStoryObjectController(StoryInstance* tInstance, StoryEditorObjectKind tObjectKind, int tID, StoryEditorStampKind tStampKind, const void* tController);
int storyEditorFilterStorySoundPlay(int tGroup, int tItem, float tVolume, int tChannel, float tFrequencyMultiplier, int tIsLooping, float tPanning, const char* tSource);
#endif