#include "fightui.h"

#include <assert.h>
#include <algorithm>

#include <prism/file.h>
#include <prism/math.h>
#include <prism/input.h>
#include <prism/mugendefreader.h>
#include <prism/mugenspritefilereader.h>
#include <prism/mugenanimationreader.h>
#include <prism/mugenanimationhandler.h>
#include <prism/mugentexthandler.h>

#include "stage.h"
#include "playerdefinition.h"
#include "mugenstagehandler.h"
#include "mugenanimationutilities.h"
#include "config.h"

using namespace std;

#define ENVIRONMENT_COLOR_LOWER_Z 29
#define HITSPARK_BASE_Z 51
#define ENVIRONMENT_COLOR_UPPER_Z 62
#define UI_BASE_Z 72

#define COORD_P 240

typedef struct {
	Position mBG0Position;
	int mOwnsBG0Animation;
	MugenAnimation* mBG0Animation;
	int mBG0AnimationID;

	Position mBG1Position;
	int mOwnsBG1Animation;
	MugenAnimation* mBG1Animation;
	int mBG1AnimationID;

	Position mMidPosition;
	int mOwnsMidAnimation;
	MugenAnimation* mMidAnimation;
	int mMidAnimationID;

	Position mFrontPosition;
	int mOwnsFrontAnimation;
	MugenAnimation* mFrontAnimation;
	int mFrontAnimationID;

	Vector3D mHealthRangeX;

	double mPercentage;
	double mDisplayedPercentage;

	int mIsPaused;
	Duration mPauseNow;

} HealthBar;

typedef struct {
	Position mBG0Position;
	int mOwnsBG0Animation;
	MugenAnimation* mBG0Animation;
	int mBG0AnimationID;

	Position mBG1Position;
	int mOwnsBG1Animation;
	MugenAnimation* mBG1Animation;
	int mBG1AnimationID;

	Position mMidPosition;
	int mOwnsMidAnimation;
	MugenAnimation* mMidAnimation;
	int mMidAnimationID;

	Position mFrontPosition;
	int mOwnsFrontAnimation;
	MugenAnimation* mFrontAnimation;
	int mFrontAnimationID;

	Vector3D mPowerRangeX;

	int mLevel;

	int mCounterTextID;
	Position mCounterPosition;
	Vector3DI mCounterFont;
	char mCounterText[10];

	Vector3DI mLevelSounds[4];
} PowerBar;

typedef struct {
	Position mBGPosition;
	int mOwnsBGAnimation;
	MugenAnimation* mBGAnimation;
	int mBGAnimationID;

	Position mBG0Position;
	int mOwnsBG0Animation;
	MugenAnimation* mBG0Animation;
	int mBG0AnimationID;

	Position mBG1Position;
	int mOwnsBG1Animation;
	MugenAnimation* mBG1Animation;
	int mBG1AnimationID;

	Position mFacePosition;
	int mOwnsFaceAnimation;
	MugenAnimation* mFaceAnimation;
	int mFaceAnimationID;
} Face;

typedef struct {
	Position mBGPosition;
	int mOwnsBGAnimation;
	MugenAnimation* mBGAnimation;
	int mBGAnimationID;

	Position mTextPosition;
	int mTextID;

	Vector3DI mFont;
} DisplayName;

typedef struct {
	int mIsActive;
	int mIsInfinite;
	int mIsFinished;

	int mValue;
	Position mPosition;

	Position mBGPosition;
	int mOwnsBGAnimation;
	MugenAnimation* mBGAnimation;
	int mBGAnimationID;

	Vector3DI mFont;

	int mTextID;
	int mFramesPerCount;
	int mNow;
	int mTimerFreezeFlag;

	void(*mFinishedCB)();
} TimeCounter;

typedef struct {
	int mIsActive;

	Position mPosition;
	double mStartX;

	Vector3DI mCounterFont;
	int mCounterShake;

	char mText[1024];
	Vector3DI mFont;
	Position mTextOffset;

	int mTextID;
	int mNumberTextID;

	double mCurrentX;

	int mDisplayNow;
	int mDisplayTime;
} Combo;

typedef struct {
	int mRoundTime;
	Position mPosition;
	int mOwnsDefaultAnimation;
	MugenAnimation* mDefaultAnimation;
	int mDefaultFaceDirection;
	int mHasDefaultAnimation;
	Vector3DI mFont;

	int mHasCustomRoundAnimation[10];
	int mOwnsCustomRoundAnimations[10];
	MugenAnimation* mCustomRoundAnimations[10];
	Position mCustomRoundPositions[10];
	int mCustomRoundFaceDirection[10];

	int mHasRoundSound[10];
	Vector3DI mRoundSounds[10];
	int mSoundTime;

	char mText[1024];
	char mDisplayedText[1024];
	int mDisplayTime;

	int mIsDisplayingRound;
	int mDisplayNow;
	int mHasPlayedSound;
	int mRoundIndex;

	int mHasActiveAnimation;
	int mAnimationID;
	int mTextID;


	void(*mCB)();
} Round;

// TODO: Double KO, etc.
typedef struct {
	Position mPosition;
	int mOwnsAnimation;
	MugenAnimation* mAnimation;
	int mFaceDirection;

	int mDisplayNow;
	int mIsDisplayingFight;

	int mAnimationID;

	int mHasPlayedSound;
	int mSoundTime;
	Vector3DI mSound;

	void(*mCB)();

} FightDisplay;

typedef struct {
	Position mPosition;
	int mOwnsAnimation;
	MugenAnimation* mAnimation;
	int mFaceDirection;

	int mDisplayNow;
	int mIsDisplaying;

	int mHasPlayedSound;
	int mSoundTime;
	Vector3DI mSound;

	int mAnimationID;

	void(*mCB)();

} KO;

typedef struct {
	int mTextID;
	
	Position mDisplayPosition;
	Position mPosition;
	char mText[1024];
	char mDisplayedText[1024];
	int mNow;
	int mDisplayTime;

	Vector3DI mFont;

	int mIsDisplaying;
	void(*mCB)();
} WinDisplay;

typedef struct {
	int mIsCountingDownControl;
	int mControlReturnTime;
	int mNow;
} ControlCountdown;

typedef struct {
	int mIsActive;

	int mContinueTextID;
	int mValueTextID;

	int mValue;
	int mNow;
	int mDuration;

	void(*mFinishedCB)();
	void(*mPressedContinueCB)();

} Continue;

typedef struct {
	Position mPosition;
	Position mOffset;

	Position mCounterOffset;
	Vector3DI mCounterFont;

	Vector3DI mNormalWin;
	Vector3DI mSpecialWin;
	Vector3DI mHyperWin;
	Vector3DI mThrowWin;
	Vector3DI mCheeseWin;
	Vector3DI mTimeOverWin;
	Vector3DI mSuicideWin;
	Vector3DI mTeammateWin;
	Vector3DI mPerfectWin;

	int mIconUpToAmount;

	int mIconAmount;
	MugenAnimation** mIconAnimations;
	int* mIconAnimationIDs;
	int* mHasIsPerfectIcon;
	MugenAnimation** mPerfectIconAnimations;
	int* mPerfectIconAnimationIDs;
} WinIcon;

typedef struct {
	TextureData mWhiteTexture;
	int mAnimationID;

	int mIsActive;
	int mNow;
	int mDuration;
} EnvironmentColorEffect;

typedef struct {
	double mFrequency;
	int mAmplitude;
	double mPhaseOffset;

	int mIsActive;
	int mNow;
	int mDuration;
} EnvironmentShakeEffect;

typedef struct {
	Position mPosition;
	int mAnimationID;
} HitSpark;

// TODO: "time" component for everything, which offsets when single element is shown after call
static struct {
	MugenSpriteFile mFightSprites;
	MugenAnimations mFightAnimations;
	MugenSounds mFightSounds;

	MugenSounds mCommonSounds;

	MugenSpriteFile mFightFXSprites;
	MugenAnimations mFightFXAnimations;

	HealthBar mHealthBars[2];
	PowerBar mPowerBars[2];
	Face mFaces[2];
	DisplayName mDisplayName[2];
	TimeCounter mTime;
	Combo mCombos[2];
	Round mRound;
	FightDisplay mFight;
	KO mKO;
	WinDisplay mWin;
	Continue mContinue;
	WinIcon mWinIcons[2];

	ControlCountdown mControl;

	EnvironmentColorEffect mEnvironmentEffects;
	EnvironmentShakeEffect mEnvironmentShake;

	list<HitSpark> mHitSparks;
} gFightUIData;

static void loadFightDefFilesFromScript(MugenDefScript* tScript, char* tDefPath) {
	char directory[1024];
	getPathToFile(directory, tDefPath);

	char fileName[1024], fullPath[1024];
	getMugenDefStringOrDefault(fileName, tScript, "Files", "sff", "NO_FILE");
	sprintf(fullPath, "%s%s", directory, fileName);
	setMugenSpriteFileReaderToUsePalette(3); // TODO: check
	gFightUIData.mFightSprites = loadMugenSpriteFileWithoutPalette(fullPath);
	setMugenSpriteFileReaderToNotUsePalette();

	gFightUIData.mFightAnimations = loadMugenAnimationFile(tDefPath);

	getMugenDefStringOrDefault(fileName, tScript, "Files", "fightfx.sff", "NO_FILE");
	sprintf(fullPath, "%s%s", directory, fileName);
	gFightUIData.mFightFXSprites = loadMugenSpriteFileWithoutPalette(fullPath);

	getMugenDefStringOrDefault(fileName, tScript, "Files", "fightfx.air", "NO_FILE");
	sprintf(fullPath, "%s%s", directory, fileName);
	gFightUIData.mFightFXAnimations = loadMugenAnimationFile(fullPath);

	getMugenDefStringOrDefault(fileName, tScript, "Files", "snd", "NO_FILE");
	sprintf(fullPath, "%s%s", directory, fileName);
	gFightUIData.mFightSounds = loadMugenSoundFile(fullPath);

	getMugenDefStringOrDefault(fileName, tScript, "Files", "common.snd", "NO_FILE");
	sprintf(fullPath, "%s%s", directory, fileName);
	gFightUIData.mCommonSounds = loadMugenSoundFile(fullPath);

}

static int loadSingleUIComponentWithFullComponentNameForStorageAndReturnIfLegit(MugenDefScript* tScript, MugenAnimations* tAnimations, Position tBasePosition, const char* tGroupName, const char* tComponentName, double tZ, MugenAnimation** oAnimation, int* oOwnsAnimation, Position* oPosition, int* oFaceDirection) {
	char name[1024];

	int animation;
	sprintf(name, "%s.anim", tComponentName);
	animation = getMugenDefIntegerOrDefault(tScript, tGroupName, name, -1);

	if (animation == -1) {
		Vector3DI sprite;
		sprintf(name, "%s.spr", tComponentName);
		sprite = getMugenDefVectorIOrDefault(tScript, tGroupName, name, makeVector3DI(-1, -1, 0));
		if (sprite.x == -1 && sprite.y == -1) return 0;

		*oAnimation = createOneFrameMugenAnimationForSprite(sprite.x, sprite.y);
		*oOwnsAnimation = 1;
	}
	else {
		*oAnimation = getMugenAnimation(tAnimations, animation);
		*oOwnsAnimation = 0;
	}

	sprintf(name, "%s.offset", tComponentName);
	*oPosition = getMugenDefVectorOrDefault(tScript, tGroupName, name, makePosition(0, 0, 0));
	oPosition->z = tZ;
	*oPosition = vecAdd(*oPosition, tBasePosition);

	sprintf(name, "%s.facing", tComponentName);
	*oFaceDirection = getMugenDefIntegerOrDefault(tScript, tGroupName, name, 1);

	return 1;
}

static void loadSingleUIComponentWithFullComponentName(MugenDefScript* tScript, MugenSpriteFile* tSprites, MugenAnimations* tAnimations, Position tBasePosition, const char* tGroupName, const char* tComponentName, double tZ, MugenAnimation** oAnimation, int* oOwnsAnimation, int* oAnimationID, Position* oPosition, int tScaleCoordinateP) {
	int faceDirection;

	if (!loadSingleUIComponentWithFullComponentNameForStorageAndReturnIfLegit(tScript, tAnimations, tBasePosition, tGroupName, tComponentName, tZ, oAnimation, oOwnsAnimation, oPosition, &faceDirection)) {
		*oAnimationID = -1;
		return;
	}

	*oAnimationID = addMugenAnimation(*oAnimation, tSprites, makePosition(0,0,0)); // TODO: fix
	setMugenAnimationBasePosition(*oAnimationID, oPosition);

	if (faceDirection == -1) {
		setMugenAnimationFaceDirection(*oAnimationID, 0);
	}

}

static void loadSingleUIComponent(int i, MugenDefScript* tScript, MugenSpriteFile* tSprites, MugenAnimations* tAnimations, Position tBasePosition, const char* tGroupName, const char* tComponentName, double tZ, MugenAnimation** oAnimation, int* oOwnsAnimation, int* oAnimationID, Position* oPosition, int tScaleCoordinateP) {
	char name[1024];

	sprintf(name, "p%d.%s", i + 1, tComponentName);
	loadSingleUIComponentWithFullComponentName(tScript, tSprites, tAnimations, tBasePosition, tGroupName, name, tZ, oAnimation, oOwnsAnimation, oAnimationID, oPosition, tScaleCoordinateP);
}

static void loadSingleUITextWithFullComponentNameForStorage(MugenDefScript* tScript, Position tBasePosition, const char* tGroupName, const char* tComponentName, double tZ, Position* oPosition, int tIsReadingText, char* tText, Vector3DI* oFontData) {
	char name[1024];

	sprintf(name, "%s.offset", tComponentName);
	*oPosition = getMugenDefVectorOrDefault(tScript, tGroupName, name, makePosition(0, 0, 0));
	oPosition->z = tZ;
	*oPosition = vecAdd(*oPosition, tBasePosition);

	if (tIsReadingText) {
		sprintf(name, "%s.text", tComponentName);
		char* text = getAllocatedMugenDefStringVariable(tScript, tGroupName, name);
		strcpy(tText, text);
		freeMemory(text);
	}

	sprintf(name, "%s.font", tComponentName);
	*oFontData = getMugenDefVectorIOrDefault(tScript, tGroupName, name, makeVector3DI(1, 0, 0));
}


static void loadSingleUITextWithFullComponentName(MugenDefScript* tScript, Position tBasePosition, const char* tGroupName, const char* tComponentName, double tZ, int* oTextID, Position* oPosition, int tIsReadingText, char* tText, Vector3DI* oFontData) {
	loadSingleUITextWithFullComponentNameForStorage(tScript, tBasePosition, tGroupName, tComponentName, tZ, oPosition, tIsReadingText, tText, oFontData);

	*oTextID = addMugenText(tText, *oPosition, oFontData->x);
	setMugenTextColor(*oTextID, getMugenTextColorFromMugenTextColorIndex(oFontData->y));
	setMugenTextAlignment(*oTextID, getMugenTextAlignmentFromMugenAlignmentIndex(oFontData->z));
}


static void loadSingleUIText(int i, MugenDefScript* tScript, Position tBasePosition, const char* tGroupName, const char* tComponentName, double tZ, int* oTextID, Position* oPosition, int tIsReadingText, char* tText, Vector3DI* oFontData) {
	char name[1024];

	sprintf(name, "p%d.%s", i + 1, tComponentName);
	loadSingleUITextWithFullComponentName(tScript, tBasePosition, tGroupName, name, tZ, oTextID, oPosition, tIsReadingText, tText, oFontData);
}

static void loadSingleHealthBar(int i, MugenDefScript* tScript) {
	char name[1024];

	HealthBar* bar = &gFightUIData.mHealthBars[i];


	Position basePosition;
	sprintf(name, "p%d.pos", i + 1);
	basePosition = getMugenDefVectorOrDefault(tScript, "Lifebar", name, makePosition(0,0,0));
	basePosition.z = UI_BASE_Z;

	int coordP = COORD_P; // TODO
	loadSingleUIComponent(i, tScript, &gFightUIData.mFightSprites, &gFightUIData.mFightAnimations, basePosition, "Lifebar", "bg0", 1, &bar->mBG0Animation, &bar->mOwnsBG0Animation, &bar->mBG0AnimationID, &bar->mBG0Position, coordP);
	loadSingleUIComponent(i, tScript, &gFightUIData.mFightSprites, &gFightUIData.mFightAnimations, basePosition, "Lifebar", "bg1", 2, &bar->mBG1Animation, &bar->mOwnsBG1Animation, &bar->mBG1AnimationID, &bar->mBG1Position, coordP);
	loadSingleUIComponent(i, tScript, &gFightUIData.mFightSprites, &gFightUIData.mFightAnimations, basePosition, "Lifebar", "mid", 3, &bar->mMidAnimation, &bar->mOwnsMidAnimation,  &bar->mMidAnimationID, &bar->mMidPosition, coordP);
	loadSingleUIComponent(i, tScript, &gFightUIData.mFightSprites, &gFightUIData.mFightAnimations, basePosition, "Lifebar", "front", 4, &bar->mFrontAnimation, &bar->mOwnsFrontAnimation, &bar->mFrontAnimationID, &bar->mFrontPosition, coordP);

	sprintf(name, "p%d.range.x", i + 1);
	bar->mHealthRangeX = getMugenDefVectorOrDefault(tScript, "Lifebar", name, makePosition(0, 0, 0));

	bar->mPercentage = 1;
	bar->mDisplayedPercentage = 1;
	bar->mIsPaused = 0;
}

static void loadSinglePowerBar(int i, MugenDefScript* tScript) {
	char name[1024];

	PowerBar* bar = &gFightUIData.mPowerBars[i];

	Position basePosition;
	sprintf(name, "p%d.pos", i + 1);
	basePosition = getMugenDefVectorOrDefault(tScript, "Powerbar", name, makePosition(0, 0, 0));
	basePosition.z = UI_BASE_Z;

	int coordP = COORD_P; // TODO
	loadSingleUIComponent(i, tScript, &gFightUIData.mFightSprites, &gFightUIData.mFightAnimations, basePosition, "Powerbar", "bg0", 1, &bar->mBG0Animation, &bar->mOwnsBG0Animation, &bar->mBG0AnimationID, &bar->mBG0Position, coordP);
	loadSingleUIComponent(i, tScript, &gFightUIData.mFightSprites, &gFightUIData.mFightAnimations, basePosition, "Powerbar", "bg1", 2, &bar->mBG1Animation, &bar->mOwnsBG1Animation, &bar->mBG1AnimationID, &bar->mBG1Position, coordP);
	loadSingleUIComponent(i, tScript, &gFightUIData.mFightSprites, &gFightUIData.mFightAnimations, basePosition, "Powerbar", "mid", 3, &bar->mMidAnimation, &bar->mOwnsMidAnimation, &bar->mMidAnimationID, &bar->mMidPosition, coordP);
	loadSingleUIComponent(i, tScript, &gFightUIData.mFightSprites, &gFightUIData.mFightAnimations, basePosition, "Powerbar", "front", 4, &bar->mFrontAnimation, &bar->mOwnsFrontAnimation, &bar->mFrontAnimationID, &bar->mFrontPosition, coordP);
	
	strcpy(bar->mCounterText, "0");
	loadSingleUIText(i, tScript, basePosition, "Powerbar", "counter", 5, &bar->mCounterTextID, &bar->mCounterPosition, 0, bar->mCounterText, &bar->mCounterFont);

	sprintf(name, "p%d.range.x", i + 1);
	bar->mPowerRangeX = getMugenDefVectorOrDefault(tScript, "Powerbar", name, makePosition(0, 0, 0));

	int j; // TODO: move to general powerbar header
	for (j = 0; j < 3; j++) {
		sprintf(name, "level%d.snd", j + 1);
		bar->mLevelSounds[j] = getMugenDefVectorIOrDefault(tScript, "Powerbar", name, makeVector3DI(1, 0, 0));
	}

	bar->mLevel = 0;
}

static void loadSingleFace(int i, MugenDefScript* tScript) {
	char name[1024];

	Face* face = &gFightUIData.mFaces[i];

	Position basePosition;
	sprintf(name, "p%d.pos", i + 1);
	basePosition = getMugenDefVectorOrDefault(tScript, "Face", name, makePosition(0, 0, 0));
	basePosition.z = UI_BASE_Z;

	DreamPlayer* p = getRootPlayer(i);
	int coordP = COORD_P; // TODO
	loadSingleUIComponent(i, tScript, &gFightUIData.mFightSprites, &gFightUIData.mFightAnimations, basePosition, "Face", "bg", 1, &face->mBGAnimation, &face->mOwnsBGAnimation, &face->mBGAnimationID, &face->mBGPosition, coordP);
	loadSingleUIComponent(i, tScript, &gFightUIData.mFightSprites, &gFightUIData.mFightAnimations, basePosition, "Face", "bg0", 1, &face->mBG0Animation, &face->mOwnsBG0Animation, &face->mBG0AnimationID, &face->mBG0Position, coordP);
	loadSingleUIComponent(i, tScript, &gFightUIData.mFightSprites, &gFightUIData.mFightAnimations, basePosition, "Face", "bg1", 2, &face->mBG1Animation, &face->mOwnsBG1Animation, &face->mBG1AnimationID, &face->mBG1Position, coordP);
	loadSingleUIComponent(i, tScript, NULL, getPlayerAnimations(p), basePosition, "Face", "face", 3, &face->mFaceAnimation, &face->mOwnsFaceAnimation, &face->mFaceAnimationID, &face->mFacePosition, getPlayerCoordinateP(p));

}

static void loadSingleName(int i, MugenDefScript* tScript) {
	char name[1024];

	DisplayName* displayName = &gFightUIData.mDisplayName[i];

	Position basePosition;
	sprintf(name, "p%d.pos", i + 1);
	basePosition = getMugenDefVectorOrDefault(tScript, "Name", name, makePosition(0, 0, 0));
	basePosition.z = UI_BASE_Z;

	int coordP = COORD_P; // TODO
	loadSingleUIComponent(i, tScript, &gFightUIData.mFightSprites, &gFightUIData.mFightAnimations, basePosition, "Name", "bg", 0, &displayName->mBGAnimation, &displayName->mOwnsBGAnimation, &displayName->mBGAnimationID, &displayName->mBGPosition, coordP);

	char displayNameText[1024];
	sprintf(displayNameText, "%s", getPlayerDisplayName(getRootPlayer(i)));
	loadSingleUIText(i, tScript, basePosition, "Name", "name", 10, &displayName->mTextID, &displayName->mTextPosition, 0, displayNameText, &displayName->mFont);
}

static void loadSingleWinIcon(int i, MugenDefScript* tScript) {
	char name[1024];

	WinIcon* winIcon = &gFightUIData.mWinIcons[i];

	sprintf(name, "p%d.pos", i + 1);
	winIcon->mPosition = getMugenDefVectorOrDefault(tScript, "WinIcon", name, makePosition(0, 0, 0));
	winIcon->mPosition.z = UI_BASE_Z;

	sprintf(name, "p%d.iconoffset", i + 1);
	winIcon->mOffset = getMugenDefVectorOrDefault(tScript, "WinIcon", name, makePosition(0, 0, 0));

	sprintf(name, "p%d.counter.offset", i + 1);
	winIcon->mCounterOffset = getMugenDefVectorOrDefault(tScript, "WinIcon", name, makePosition(0, 0, 0));

	sprintf(name, "p%d.counter.font", i + 1);
	winIcon->mCounterFont = getMugenDefVectorIOrDefault(tScript, "WinIcon", name, makeVector3DI(1, 0, 0));

	sprintf(name, "p%d.n.spr", i + 1);
	winIcon->mNormalWin = getMugenDefVectorIOrDefault(tScript, "WinIcon", name, makeVector3DI(0, 0, 0));
	sprintf(name, "p%d.s.spr", i + 1);
	winIcon->mSpecialWin = getMugenDefVectorIOrDefault(tScript, "WinIcon", name, makeVector3DI(0, 0, 0));
	sprintf(name, "p%d.h.spr", i + 1);
	winIcon->mHyperWin = getMugenDefVectorIOrDefault(tScript, "WinIcon", name, makeVector3DI(0, 0, 0));
	sprintf(name, "p%d.throw.spr", i + 1);
	winIcon->mThrowWin = getMugenDefVectorIOrDefault(tScript, "WinIcon", name, makeVector3DI(0, 0, 0));
	sprintf(name, "p%d.c.spr", i + 1);
	winIcon->mCheeseWin = getMugenDefVectorIOrDefault(tScript, "WinIcon", name, makeVector3DI(0, 0, 0));
	sprintf(name, "p%d.t.spr", i + 1);
	winIcon->mTimeOverWin = getMugenDefVectorIOrDefault(tScript, "WinIcon", name, makeVector3DI(0, 0, 0));
	sprintf(name, "p%d.suicide.spr", i + 1);
	winIcon->mSuicideWin = getMugenDefVectorIOrDefault(tScript, "WinIcon", name, makeVector3DI(0, 0, 0));
	sprintf(name, "p%d.teammate.spr", i + 1);
	winIcon->mTeammateWin = getMugenDefVectorIOrDefault(tScript, "WinIcon", name, makeVector3DI(0, 0, 0));
	sprintf(name, "p%d.perfect.spr", i + 1);
	winIcon->mPerfectWin = getMugenDefVectorIOrDefault(tScript, "WinIcon", name, makeVector3DI(0, 0, 0));
	
	winIcon->mIconUpToAmount = getMugenDefIntegerOrDefault(tScript, "WinIcon", "useiconupto", 4);

	winIcon->mIconAnimationIDs = (int*)allocMemory(sizeof(int) * winIcon->mIconUpToAmount);
	winIcon->mIconAnimations = (MugenAnimation**)allocMemory(sizeof(MugenAnimation*) * winIcon->mIconUpToAmount);
	winIcon->mHasIsPerfectIcon = (int*)allocMemory(sizeof(int*) * winIcon->mIconUpToAmount);
	winIcon->mPerfectIconAnimations = (MugenAnimation**)allocMemory(sizeof(MugenAnimation*) * winIcon->mIconUpToAmount);
	winIcon->mPerfectIconAnimationIDs = (int*)allocMemory(sizeof(int*) * winIcon->mIconUpToAmount);
	winIcon->mIconAmount = 0;
}

static void setComboPosition(int i) {
	Combo* combo = &gFightUIData.mCombos[i];

	if (i == 0) {
		Position startPosition = makePosition(combo->mCurrentX, combo->mPosition.y, combo->mPosition.z);
		setMugenTextPosition(combo->mNumberTextID, startPosition);
		startPosition = vecAdd(startPosition, makePosition(getMugenTextSizeX(combo->mNumberTextID), 0, 0));
		startPosition = vecAdd(startPosition, combo->mTextOffset);
		setMugenTextPosition(combo->mTextID, startPosition);
	}
	else {
		Position startPosition = makePosition(combo->mCurrentX, combo->mPosition.y, combo->mPosition.z);
		setMugenTextPosition(combo->mTextID, startPosition);
		startPosition = vecSub(startPosition, makePosition(getMugenTextSizeX(combo->mTextID), 0, 0));
		startPosition = vecSub(startPosition, combo->mTextOffset);
		setMugenTextPosition(combo->mNumberTextID, startPosition);
	}
}

static void loadSingleCombo(int i, MugenDefScript* tScript) {
	char name[1024];

	Combo* combo = &gFightUIData.mCombos[i];


	sprintf(name, "team%d.pos", i + 1);
	combo->mPosition = getMugenDefVectorOrDefault(tScript, "Combo", name, makePosition(0, 0, 0));
	combo->mPosition.z = UI_BASE_Z;

	int coordP = COORD_P; 
	(void)coordP; // TODO
	sprintf(name, "team%d.start.x", i + 1);
	combo->mStartX = getMugenDefFloatOrDefault(tScript, "Combo", name, 0);

	sprintf(name, "team%d.counter.font", i + 1);
	combo->mCounterFont = getMugenDefVectorIOrDefault(tScript, "Combo", name, makeVector3DI(1, 0, 1));
	sprintf(name, "team%d.counter.shake", i + 1);
	combo->mCounterShake = getMugenDefIntegerOrDefault(tScript, "Combo", name, 0);

	sprintf(name, "team%d.text.text", i + 1);
	getMugenDefStringOrDefault(combo->mText, tScript, "Combo", name, "COMBO");
	sprintf(name, "team%d.text.font", i + 1);
	combo->mFont = getMugenDefVectorIOrDefault(tScript, "Combo", name, makeVector3DI(1, 0, 1));
	sprintf(name, "team%d.text.offset", i + 1);
	combo->mTextOffset = getMugenDefVectorOrDefault(tScript, "Combo", name, makePosition(0, 0, 0));
	combo->mTextOffset.z = 0;

	sprintf(name, "team%d.displaytime", i + 1);
	combo->mDisplayTime = getMugenDefIntegerOrDefault(tScript, "Combo", name, 0);

	Position startPosition = makePosition(combo->mPosition.x, combo->mPosition.y, combo->mPosition.z);
	combo->mNumberTextID = addMugenTextMugenStyle("111", startPosition, combo->mCounterFont);
	combo->mTextID = addMugenTextMugenStyle(combo->mText, startPosition, combo->mFont);
	
	if (i == 0) {
		combo->mCurrentX = combo->mStartX - getMugenTextSizeX(combo->mNumberTextID);
	}
	else {
		combo->mCurrentX = combo->mStartX + getMugenTextSizeX(combo->mTextID);
	}
	setComboPosition(i);

	combo->mIsActive = 0;
}

static void setBarToPercentage(int tAnimationID, Vector3D tRange, double tPercentage);

static void setDreamLifeBarPercentageStart(DreamPlayer* tPlayer, double tPercentage) {
	int i = tPlayer->mRootID;
	setDreamLifeBarPercentage(tPlayer, tPercentage);
	gFightUIData.mHealthBars[i].mDisplayedPercentage = getPlayerLifePercentage(getRootPlayer(i));
	setBarToPercentage(gFightUIData.mHealthBars[i].mMidAnimationID, gFightUIData.mHealthBars[i].mHealthRangeX, gFightUIData.mHealthBars[i].mDisplayedPercentage);
}


static void loadPlayerUIs(MugenDefScript* tScript) {
	int i;
	for (i = 0; i < 2; i++) {
		loadSingleCombo(i, tScript);
		loadSingleHealthBar(i, tScript);
		loadSinglePowerBar(i, tScript);
		loadSingleFace(i, tScript);
		loadSingleName(i, tScript);
		loadSingleWinIcon(i, tScript);
		setDreamLifeBarPercentageStart(getRootPlayer(i), getPlayerLifePercentage(getRootPlayer(i)));
		setDreamPowerBarPercentage(getRootPlayer(i), 0, 0);
	}
}

static void playDisplayText(int* oTextID, const char* tText, Position tPosition, Vector3DI tFont);

static void loadTimer(MugenDefScript* tScript) {
	Position basePosition;
	basePosition = getMugenDefVectorOrDefault(tScript, "Time", "pos", makePosition(0, 0, 0));
	basePosition.z = UI_BASE_Z;

	int coordP = COORD_P; 
	(void)coordP; // TODO
	loadSingleUIComponentWithFullComponentName(tScript, &gFightUIData.mFightSprites, &gFightUIData.mFightAnimations, basePosition, "Time", "bg", 0, &gFightUIData.mTime.mBGAnimation, &gFightUIData.mTime.mOwnsBGAnimation, &gFightUIData.mTime.mBGAnimationID, &gFightUIData.mTime.mBGPosition, coordP);

	gFightUIData.mTime.mPosition = getMugenDefVectorOrDefault(tScript, "Time", "counter.offset", makePosition(0, 0, 0));
	gFightUIData.mTime.mPosition = vecAdd(gFightUIData.mTime.mPosition, basePosition);
	gFightUIData.mTime.mPosition.z+=10;

	gFightUIData.mTime.mFramesPerCount = getMugenDefIntegerOrDefault(tScript, "Time", "framespercount", 60);

	gFightUIData.mTime.mIsActive = 0;

	gFightUIData.mTime.mFont = getMugenDefVectorIOrDefault(tScript, "Time", "counter.font", makeVector3DI(1, 0, 0));

	playDisplayText(&gFightUIData.mTime.mTextID, "99", gFightUIData.mTime.mPosition, gFightUIData.mTime.mFont);
	
	gFightUIData.mTime.mTimerFreezeFlag = 0;
	resetDreamTimer();
}

static void loadRound(MugenDefScript* tScript) {
	Position basePosition;
	basePosition = getMugenDefVectorOrDefault(tScript, "Round", "pos", makePosition(0,0,0));
	basePosition.z = UI_BASE_Z;

	gFightUIData.mRound.mRoundTime = getMugenDefIntegerOrDefault(tScript, "Round", "round.time", 0);
	gFightUIData.mRound.mDisplayTime = getMugenDefIntegerOrDefault(tScript, "Round", "round.default.displaytime", 0);
	gFightUIData.mRound.mSoundTime = getMugenDefIntegerOrDefault(tScript, "Round", "round.sndtime", 0);

	if (isMugenDefVariable(tScript, "Round", "round.default.anim")) {
		gFightUIData.mRound.mHasDefaultAnimation = 1;
		loadSingleUIComponentWithFullComponentNameForStorageAndReturnIfLegit(tScript, &gFightUIData.mFightAnimations, basePosition, "Round", "round.default", 1, &gFightUIData.mRound.mDefaultAnimation, &gFightUIData.mRound.mOwnsDefaultAnimation, &gFightUIData.mRound.mPosition, &gFightUIData.mRound.mDefaultFaceDirection);
	}
	else {
		gFightUIData.mRound.mHasDefaultAnimation = 0;
		loadSingleUITextWithFullComponentNameForStorage(tScript, basePosition, "Round", "round.default", 1, &gFightUIData.mRound.mPosition, 1, gFightUIData.mRound.mText, &gFightUIData.mRound.mFont);
	}


	int i;
	for (i = 0; i < 9; i++) {
		char name[1024];
		sprintf(name, "round%d.anim", i + 1);
		if (!isMugenDefVariable(tScript, "Round", name)) {
			gFightUIData.mRound.mHasCustomRoundAnimation[i] = 0;
		}
		else {
			gFightUIData.mRound.mHasCustomRoundAnimation[i] = 1;
			sprintf(name, "round%d", i + 1);
			loadSingleUIComponentWithFullComponentNameForStorageAndReturnIfLegit(tScript, &gFightUIData.mFightAnimations, basePosition, "Round", name, 1, &gFightUIData.mRound.mCustomRoundAnimations[i], &gFightUIData.mRound.mOwnsCustomRoundAnimations[i], &gFightUIData.mRound.mCustomRoundPositions[i], &gFightUIData.mRound.mCustomRoundFaceDirection[i]);
		}

		sprintf(name, "round%d.snd", i + 1);
		if (!isMugenDefVariable(tScript, "Round", name)) {
			gFightUIData.mRound.mHasRoundSound[i] = 0;
		}
		else {
			gFightUIData.mRound.mHasRoundSound[i] = 1;
			gFightUIData.mRound.mRoundSounds[i] = getMugenDefVectorIVariable(tScript, "Round", name);
		}
	}

	gFightUIData.mRound.mIsDisplayingRound = 0;
}

static void loadFight(MugenDefScript* tScript) {
	Position basePosition;
	basePosition = getMugenDefVectorOrDefault(tScript, "Round", "pos", makePosition(0, 0, 0));
	basePosition.z = UI_BASE_Z;

	loadSingleUIComponentWithFullComponentNameForStorageAndReturnIfLegit(tScript, &gFightUIData.mFightAnimations, basePosition, "Round", "fight", 1, &gFightUIData.mFight.mAnimation, &gFightUIData.mFight.mOwnsAnimation, &gFightUIData.mFight.mPosition, &gFightUIData.mFight.mFaceDirection);
	gFightUIData.mFight.mSoundTime = getMugenDefIntegerOrDefault(tScript, "Round", "fight.sndtime", 0);
	gFightUIData.mFight.mSound = getMugenDefVectorIOrDefault(tScript, "Round", "fight.snd", makeVector3DI(1, 0, 0));

	gFightUIData.mFight.mIsDisplayingFight = 0;
}

static void loadKO(MugenDefScript* tScript) {
	Position basePosition;
	basePosition = getMugenDefVectorOrDefault(tScript, "Round", "pos", makePosition(0, 0, 0));
	basePosition.z = UI_BASE_Z;

	loadSingleUIComponentWithFullComponentNameForStorageAndReturnIfLegit(tScript, &gFightUIData.mFightAnimations, basePosition, "Round", "ko", 1, &gFightUIData.mKO.mAnimation, &gFightUIData.mKO.mOwnsAnimation, &gFightUIData.mKO.mPosition, &gFightUIData.mKO.mFaceDirection);
	gFightUIData.mKO.mSoundTime = getMugenDefIntegerOrDefault(tScript, "Round", "ko.sndtime", 0);
	gFightUIData.mKO.mSound = getMugenDefVectorIOrDefault(tScript, "Round", "ko.snd", makeVector3DI(1, 0, 0));

	gFightUIData.mKO.mIsDisplaying = 0;
}

static void loadWinDisplay(MugenDefScript* tScript) {
	Position basePosition;
	basePosition = getMugenDefVectorOrDefault(tScript, "Round", "pos", makePosition(0, 0, 0));
	basePosition.z = UI_BASE_Z;

	loadSingleUITextWithFullComponentNameForStorage(tScript, basePosition, "Round", "win", 1, &gFightUIData.mWin.mPosition, 1, gFightUIData.mWin.mText, &gFightUIData.mWin.mFont);

	gFightUIData.mWin.mDisplayTime = getMugenDefIntegerOrDefault(tScript, "Round", "win.displaytime", 0);

	gFightUIData.mWin.mIsDisplaying = 0;
}

static void loadControl(MugenDefScript* tScript) {
	gFightUIData.mControl.mControlReturnTime = getMugenDefIntegerOrDefault(tScript, "Round", "ctrl.time", 0);

	gFightUIData.mControl.mIsCountingDownControl = 0;
}

static void loadHitSparks() {
	gFightUIData.mHitSparks.clear();
}

static void loadContinue() {
	gFightUIData.mContinue.mIsActive = 0;
}

static void loadEnvironmentColorEffects() {
	gFightUIData.mEnvironmentEffects.mWhiteTexture = createWhiteTexture();
	gFightUIData.mEnvironmentEffects.mAnimationID = playOneFrameAnimationLoop(makePosition(0, 0, 0), &gFightUIData.mEnvironmentEffects.mWhiteTexture);
	setAnimationScale(gFightUIData.mEnvironmentEffects.mAnimationID, makePosition(0, 0, 0), makePosition(0, 0, 0));

	gFightUIData.mEnvironmentEffects.mIsActive = 0;
}

static void loadEnvironmentShakeEffects() {
	gFightUIData.mEnvironmentShake.mIsActive = 0;
}

static void loadFightUI(void* tData) {
	(void)tData;

	char defPath[1024];
	strcpy(defPath, "assets/data/fight.def"); // TODO
	MugenDefScript script; 
	loadMugenDefScript(&script, defPath);
	
	loadFightDefFilesFromScript(&script, defPath);
	loadPlayerUIs(&script);
	loadTimer(&script);
	loadRound(&script);
	loadFight(&script);
	loadKO(&script);
	loadWinDisplay(&script);
	loadControl(&script);
	loadContinue();
	unloadMugenDefScript(script);

	loadHitSparks();
	loadEnvironmentColorEffects();
	loadEnvironmentShakeEffects();
}

static void unloadSingleUIComponent(MugenAnimation* tAnimation, int tOwnsAnimation) {
	if (!tOwnsAnimation) return;
	destroyMugenAnimation(tAnimation);
}

static void unloadSingleOptionalUIComponent(MugenAnimation* tAnimation, int tOwnsAnimation, int tAnimationID) {
	if (tAnimationID == -1) return;
	unloadSingleUIComponent(tAnimation, tOwnsAnimation);
}

static void unloadFightDefFiles() {
	unloadMugenSpriteFile(&gFightUIData.mFightSprites);
	unloadMugenAnimationFile(&gFightUIData.mFightAnimations);
	unloadMugenSpriteFile(&gFightUIData.mFightFXSprites);
	unloadMugenAnimationFile(&gFightUIData.mFightFXAnimations);

	unloadMugenSoundFile(&gFightUIData.mFightSounds);
	unloadMugenSoundFile(&gFightUIData.mCommonSounds);
}



static void unloadSingleHealthBar(int i) {
	HealthBar* bar = &gFightUIData.mHealthBars[i];

	unloadSingleOptionalUIComponent(bar->mBG0Animation, bar->mOwnsBG0Animation, bar->mBG0AnimationID);
	unloadSingleOptionalUIComponent(bar->mBG1Animation, bar->mOwnsBG1Animation, bar->mBG1AnimationID);
	unloadSingleOptionalUIComponent(bar->mMidAnimation, bar->mOwnsMidAnimation, bar->mMidAnimationID);
	unloadSingleOptionalUIComponent(bar->mFrontAnimation, bar->mOwnsFrontAnimation, bar->mFrontAnimationID);
}

static void unloadSinglePowerBar(int i) {
	PowerBar* bar = &gFightUIData.mPowerBars[i];

	unloadSingleOptionalUIComponent(bar->mBG0Animation, bar->mOwnsBG0Animation, bar->mBG0AnimationID);
	unloadSingleOptionalUIComponent(bar->mBG1Animation, bar->mOwnsBG1Animation, bar->mBG1AnimationID);
	unloadSingleOptionalUIComponent(bar->mMidAnimation, bar->mOwnsMidAnimation, bar->mMidAnimationID);
	unloadSingleOptionalUIComponent(bar->mFrontAnimation, bar->mOwnsFrontAnimation, bar->mFrontAnimationID);
}

static void unloadSingleFace(int i) {
	Face* face = &gFightUIData.mFaces[i];

	unloadSingleOptionalUIComponent(face->mBGAnimation, face->mOwnsBGAnimation, face->mBGAnimationID);
	unloadSingleOptionalUIComponent(face->mBG0Animation, face->mOwnsBG0Animation, face->mBG0AnimationID);
	unloadSingleOptionalUIComponent(face->mBG1Animation, face->mOwnsBG1Animation, face->mBG1AnimationID);
	unloadSingleOptionalUIComponent(face->mFaceAnimation, face->mOwnsFaceAnimation, face->mFaceAnimationID);
}

static void unloadSingleName(int i) {
	DisplayName* displayName = &gFightUIData.mDisplayName[i];

	unloadSingleOptionalUIComponent(displayName->mBGAnimation, displayName->mOwnsBGAnimation, displayName->mBGAnimationID);
}


static void unloadSingleWinIcon(int i) {
	WinIcon* winIcon = &gFightUIData.mWinIcons[i];

	int j;
	for (j = 0; j < winIcon->mIconAmount; j++) {
		destroyMugenAnimation(winIcon->mIconAnimations[j]);

		if (winIcon->mHasIsPerfectIcon[j]) {
			destroyMugenAnimation(winIcon->mPerfectIconAnimations[j]);
		}
	}

	freeMemory(winIcon->mIconAnimationIDs);
	freeMemory(winIcon->mIconAnimations);
	freeMemory(winIcon->mHasIsPerfectIcon);
	freeMemory(winIcon->mPerfectIconAnimations);
	freeMemory(winIcon->mPerfectIconAnimationIDs);
}

static void unloadPlayerUIs() {
	int i;
	for (i = 0; i < 2; i++) {
		unloadSingleHealthBar(i);
		unloadSinglePowerBar(i);
		unloadSingleFace(i);
		unloadSingleName(i);
		unloadSingleWinIcon(i);
	}
}

static void unloadTimer() {
	unloadSingleOptionalUIComponent(gFightUIData.mTime.mBGAnimation, gFightUIData.mTime.mOwnsBGAnimation, gFightUIData.mTime.mBGAnimationID);
}

static void unloadRound() {
	if (gFightUIData.mRound.mHasDefaultAnimation) {
		gFightUIData.mRound.mHasDefaultAnimation = 1;
		unloadSingleUIComponent(gFightUIData.mRound.mDefaultAnimation, gFightUIData.mRound.mOwnsDefaultAnimation);
	}

	int i;
	for (i = 0; i < 9; i++) {
		if (gFightUIData.mRound.mHasCustomRoundAnimation[i]) {
			unloadSingleUIComponent(gFightUIData.mRound.mCustomRoundAnimations[i], gFightUIData.mRound.mOwnsCustomRoundAnimations[i]);
		}
	}
}

static void unloadFight() {
	unloadSingleUIComponent(gFightUIData.mFight.mAnimation, gFightUIData.mFight.mOwnsAnimation);
}

static void unloadKO() {	
	unloadSingleUIComponent(gFightUIData.mKO.mAnimation, gFightUIData.mKO.mOwnsAnimation);
}

static void unloadHitSparks() {
	gFightUIData.mHitSparks.clear();
}

static void unloadEnvironmentColorEffects() {
	unloadTexture(gFightUIData.mEnvironmentEffects.mWhiteTexture);
}


static void unloadFightUI(void* tData) {
	(void)tData;

	unloadFightDefFiles();
	unloadPlayerUIs();
	unloadTimer();
	unloadRound();
	unloadFight();
	unloadKO();

	unloadHitSparks();
	unloadEnvironmentColorEffects();
}

static int updateSingleHitSpark(void* tCaller, HitSpark& tData) {
	(void)tCaller;

	HitSpark* e = &tData;

	if (!getMugenAnimationRemainingAnimationTime(e->mAnimationID)) {
		removeMugenAnimation(e->mAnimationID);
		return 1;
	}

	return 0;
}

static void updateHitSparks() {
	stl_list_remove_predicate(gFightUIData.mHitSparks, updateSingleHitSpark);
}

static void setBarToPercentage(int tAnimationID, Vector3D tRange, double tPercentage) {
	double fullSize = fabs(tRange.y - tRange.x);
	int newSize = (int)(fullSize * tPercentage);
	setMugenAnimationRectangleWidth(tAnimationID, newSize);
}

static void updateSingleHealthBar(int i) {
	HealthBar* bar = &gFightUIData.mHealthBars[i];

	double dx = bar->mPercentage - bar->mDisplayedPercentage;

	if (fabs(dx) < 1e-6) return;

	if (bar->mIsPaused) {
		if (handleDurationAndCheckIfOver(&bar->mPauseNow, 60)) {
			bar->mIsPaused = 0;
		}
		return;
	}

	bar->mDisplayedPercentage += dx * 0.1;
	setBarToPercentage(bar->mMidAnimationID, bar->mHealthRangeX, bar->mDisplayedPercentage);
}

#define COMBO_MOVEMENT_SPEED 4

static void updateSingleComboMovement(int i) {
	Combo* combo = &gFightUIData.mCombos[i];

	double left, right;
	if (i == 0) {
		left = combo->mStartX - getMugenTextSizeX(combo->mNumberTextID);
		right = combo->mPosition.x;
	}
	else {
		left = combo->mPosition.x;
		right = combo->mStartX + getMugenTextSizeX(combo->mTextID);
	}

	int isMovingRight = (i == 0 && combo->mIsActive) || (i == 1 && !combo->mIsActive);
	if (isMovingRight) {
		combo->mCurrentX += COMBO_MOVEMENT_SPEED;
		combo->mCurrentX = min(combo->mCurrentX, right);
	}
	else {
		combo->mCurrentX -= COMBO_MOVEMENT_SPEED;
		combo->mCurrentX = max(combo->mCurrentX, left);
	}
	setComboPosition(i);
}

static void updateSingleComboActivity(int i) {
	Combo* combo = &gFightUIData.mCombos[i];
	if (!combo->mIsActive) return;

	combo->mDisplayNow++;
	if (combo->mDisplayNow >= combo->mDisplayTime) {
		combo->mIsActive = 0;
	}
}

static void updateSingleCombo(int i) {
	updateSingleComboMovement(i);
	updateSingleComboActivity(i);
}

static void updateUISide() {
	int i;
	for (i = 0; i < 2; i++) {
		updateSingleHealthBar(i);
		updateSingleCombo(i);
	}
}


static void playDisplayAnimation(int* oAnimationID, MugenAnimation* tAnimation, Position* tBasePosition, int tFaceDirection) {
	*oAnimationID = addMugenAnimation(tAnimation, &gFightUIData.mFightSprites, makePosition(0,0,0)); 
	setMugenAnimationBasePosition(*oAnimationID, tBasePosition);

	if (tFaceDirection == -1) {
		setMugenAnimationFaceDirection(*oAnimationID, 0);
	}
}

static void removeDisplayedAnimation(int tAnimationID) {
	removeMugenAnimation(tAnimationID);
}


static void playDisplayText(int* oTextID, const char* tText, Position tPosition, Vector3DI tFont) {
	*oTextID = addMugenText(tText, tPosition, tFont.x);
	setMugenTextColor(*oTextID, getMugenTextColorFromMugenTextColorIndex(tFont.y));
	setMugenTextAlignment(*oTextID, getMugenTextAlignmentFromMugenAlignmentIndex(tFont.z));
}

static void removeDisplayedText(int tTextID) {
	removeMugenText(tTextID);
}

static void updateRoundSound() {
	int round = gFightUIData.mRound.mRoundIndex;
	if (gFightUIData.mRound.mDisplayNow >= gFightUIData.mRound.mSoundTime && gFightUIData.mRound.mHasRoundSound[round] && !gFightUIData.mRound.mHasPlayedSound) {
		tryPlayMugenSound(&gFightUIData.mFightSounds, gFightUIData.mRound.mRoundSounds[round].x, gFightUIData.mRound.mRoundSounds[round].y);
		gFightUIData.mRound.mHasPlayedSound = 1;
	}
}

static void removeRoundDisplay() {
	if (gFightUIData.mRound.mHasActiveAnimation) {
		removeDisplayedAnimation(gFightUIData.mRound.mAnimationID);
	}
	else {
		removeDisplayedText(gFightUIData.mRound.mTextID);
	}
}

static void updateRoundFinish() {
	if (gFightUIData.mRound.mDisplayNow >= gFightUIData.mRound.mDisplayTime) {

		removeRoundDisplay();
		gFightUIData.mRound.mCB();
		gFightUIData.mRound.mIsDisplayingRound = 0;
	}
}

static void updateRoundDisplay() {
	if (!gFightUIData.mRound.mIsDisplayingRound) return;

	gFightUIData.mRound.mDisplayNow++;

	updateRoundSound();
	updateRoundFinish();
}

static void startControlCountdown() {
	gFightUIData.mControl.mNow = 0;
	gFightUIData.mControl.mIsCountingDownControl = 1;
}

static void updateFightSound() {

	if (gFightUIData.mFight.mDisplayNow >= gFightUIData.mFight.mSoundTime && !gFightUIData.mFight.mHasPlayedSound) {
		tryPlayMugenSound(&gFightUIData.mFightSounds, gFightUIData.mFight.mSound.x, gFightUIData.mFight.mSound.y);
		gFightUIData.mFight.mHasPlayedSound = 1;
	}
}


static void updateFightFinish() {
	if (!getMugenAnimationRemainingAnimationTime(gFightUIData.mFight.mAnimationID)) {
		removeDisplayedAnimation(gFightUIData.mFight.mAnimationID);
		gFightUIData.mFight.mCB();
		startControlCountdown();
		gFightUIData.mFight.mIsDisplayingFight = 0;
	}
}

static void updateFightDisplay() {
	if (!gFightUIData.mFight.mIsDisplayingFight) return;

	gFightUIData.mFight.mDisplayNow++;
	updateFightSound();
	updateFightFinish();
}

static void updateKOSound() {

	if (gFightUIData.mKO.mDisplayNow >= gFightUIData.mKO.mSoundTime && !gFightUIData.mKO.mHasPlayedSound) {
		tryPlayMugenSound(&gFightUIData.mFightSounds, gFightUIData.mKO.mSound.x, gFightUIData.mKO.mSound.y);
		gFightUIData.mKO.mHasPlayedSound = 1;
	}
}

static void updateKOFinish() {
	if (!getMugenAnimationRemainingAnimationTime(gFightUIData.mKO.mAnimationID)) {
		removeDisplayedAnimation(gFightUIData.mKO.mAnimationID);
		gFightUIData.mKO.mCB();
		gFightUIData.mKO.mIsDisplaying = 0;
	}
}

static void updateKODisplay() {
	if (!gFightUIData.mKO.mIsDisplaying) return;

	gFightUIData.mKO.mDisplayNow++;
	updateKOSound();
	updateKOFinish();
}


static void updateWinDisplay() {
	if (!gFightUIData.mWin.mIsDisplaying) return;

	gFightUIData.mWin.mNow++;
	if (gFightUIData.mWin.mNow >= gFightUIData.mWin.mDisplayTime || hasPressedStartFlank()) {
		removeDisplayedText(gFightUIData.mWin.mTextID);
		gFightUIData.mWin.mCB();
		gFightUIData.mWin.mIsDisplaying = 0;
	}
}


static void updateControlCountdown() {
	if (!gFightUIData.mControl.mIsCountingDownControl) return;

	gFightUIData.mControl.mNow++;
	if (gFightUIData.mControl.mNow >= gFightUIData.mControl.mControlReturnTime) {
		setPlayerControl(getRootPlayer(0), 1);
		setPlayerControl(getRootPlayer(1), 1);
		gFightUIData.mControl.mIsCountingDownControl = 0;
	}
}

static void updateTimeDisplayText() {
	char text[10];
	if (gFightUIData.mTime.mIsInfinite) {
		sprintf(text, "o");
	}
	else {
		if (gFightUIData.mTime.mValue < 10) sprintf(text, "0%d", gFightUIData.mTime.mValue);
		else sprintf(text, "%d", gFightUIData.mTime.mValue);
	}

	changeMugenText(gFightUIData.mTime.mTextID, text);
}

static void setTimerFinishedInternal();

static void updateTimeDisplay() {
	if (!gFightUIData.mTime.mIsActive) return;
	if (gFightUIData.mTime.mIsInfinite) return;
	if (gFightUIData.mTime.mIsFinished) return;
	if (gFightUIData.mTime.mTimerFreezeFlag) {
		gFightUIData.mTime.mTimerFreezeFlag = 0;
		return;
	}

	gFightUIData.mTime.mNow++;
	if (gFightUIData.mTime.mNow >= gFightUIData.mTime.mFramesPerCount) {
		gFightUIData.mTime.mNow = 0;
		gFightUIData.mTime.mValue--;
		if (gFightUIData.mTime.mValue < 0) {
			setTimerFinishedInternal();
		}

		updateTimeDisplayText();
	}
}

static void setContinueInactive() {
	removeMugenText(gFightUIData.mContinue.mContinueTextID);
	removeMugenText(gFightUIData.mContinue.mValueTextID);
	gFightUIData.mContinue.mIsActive = 0;
}


static void decreaseContinueCounter() {
	gFightUIData.mContinue.mValue--;
	char text[10];
	sprintf(text, "%d", gFightUIData.mContinue.mValue);
	changeMugenText(gFightUIData.mContinue.mValueTextID, text);
}

static void updateContinueDisplay() {
	if (!gFightUIData.mContinue.mIsActive) return;

	if (hasPressedStartFlank()) {
		setContinueInactive();
		gFightUIData.mContinue.mPressedContinueCB();
		return;
	}
	
	gFightUIData.mContinue.mNow++;
	if (gFightUIData.mContinue.mNow >= gFightUIData.mContinue.mDuration || hasPressedAFlank()) {
		gFightUIData.mContinue.mNow = 0;
		if (!gFightUIData.mContinue.mValue) {
			setContinueInactive();
			gFightUIData.mContinue.mFinishedCB();
			return;
		}

		decreaseContinueCounter();
	}
}

static void updateEnvironmentColor() {
	if (!gFightUIData.mEnvironmentEffects.mIsActive) return;

	setDreamStageLayer1InvisibleForOneFrame();

	gFightUIData.mEnvironmentEffects.mNow++;
	if (gFightUIData.mEnvironmentEffects.mNow >= gFightUIData.mEnvironmentEffects.mDuration) {
		setAnimationScale(gFightUIData.mEnvironmentEffects.mAnimationID, makePosition(0, 0, 0), makePosition(0, 0, 0));
		gFightUIData.mEnvironmentEffects.mIsActive = 0;
	}
}

static void updateFightUI(void* tData) {
	(void)tData;
	updateHitSparks();
	updateUISide();
	updateRoundDisplay();
	updateFightDisplay();
	updateKODisplay();
	updateWinDisplay();
	updateControlCountdown();
	updateTimeDisplay();
	updateContinueDisplay();
	updateEnvironmentColor();
}




ActorBlueprint getDreamFightUIBP() {
	return makeActorBlueprint(loadFightUI, unloadFightUI, updateFightUI);
};

void playDreamHitSpark(Position tPosition, DreamPlayer* tPlayer, int tIsInPlayerFile, int tNumber, int tIsFacingRight, int tPositionCoordinateP, int tScaleCoordinateP)
{
	MugenAnimation* anim;
	MugenSpriteFile* spriteFile;

	if (tIsInPlayerFile) {
		spriteFile = getPlayerSprites(tPlayer);
		if(!hasMugenAnimation(getPlayerAnimations(tPlayer), tNumber)) return;
		anim = getMugenAnimation(getPlayerAnimations(tPlayer), tNumber);
	}
	else {
		spriteFile = &gFightUIData.mFightFXSprites;
		if (!hasMugenAnimation(&gFightUIData.mFightFXAnimations, tNumber)) return;
		anim = getMugenAnimation(&gFightUIData.mFightFXAnimations, tNumber);
	}

	gFightUIData.mHitSparks.push_back(HitSpark());
	HitSpark& e = gFightUIData.mHitSparks.back();
	
	e.mPosition = tPosition;
	e.mPosition.z = HITSPARK_BASE_Z;
	e.mAnimationID = addMugenAnimation(anim, spriteFile, getDreamStageCoordinateSystemOffset(tPositionCoordinateP));
	setMugenAnimationBasePosition(e.mAnimationID, &e.mPosition);
	setMugenAnimationCameraPositionReference(e.mAnimationID, getDreamMugenStageHandlerCameraPositionReference());
	if (!tIsFacingRight) {
		setMugenAnimationFaceDirection(e.mAnimationID, tIsFacingRight);
	}
	
}

void addDreamDustCloud(Position tPosition, int tIsFacingRight, int tCoordinateP)
{
	Position pos = vecAdd(tPosition, getDreamStageCoordinateSystemOffset(tCoordinateP));
	int id = addMugenAnimation(getMugenAnimation(&gFightUIData.mFightFXAnimations, 120), &gFightUIData.mFightFXSprites, pos);
	setMugenAnimationNoLoop(id);
	setMugenAnimationCameraPositionReference(id, getDreamMugenStageHandlerCameraPositionReference());
	if (!tIsFacingRight) {
		setMugenAnimationFaceDirection(id, tIsFacingRight);
	}
}

void setDreamLifeBarPercentage(DreamPlayer* tPlayer, double tPercentage)
{
	HealthBar* bar = &gFightUIData.mHealthBars[tPlayer->mRootID];

	bar->mPercentage = tPercentage;
	setBarToPercentage(bar->mFrontAnimationID, bar->mHealthRangeX, bar->mPercentage);

	if (bar->mPercentage > bar->mDisplayedPercentage) {
		bar->mDisplayedPercentage = bar->mPercentage;
		setBarToPercentage(bar->mMidAnimationID, bar->mHealthRangeX, bar->mDisplayedPercentage);
	}
	bar->mIsPaused = 1;
	bar->mPauseNow = 0;
}

void setDreamPowerBarPercentage(DreamPlayer* tPlayer, double tPercentage, int tValue)
{
	PowerBar* bar = &gFightUIData.mPowerBars[tPlayer->mRootID];
	setBarToPercentage(bar->mFrontAnimationID, bar->mPowerRangeX, tPercentage);

	int newLevel = tValue / 1000;

	if (newLevel >= 1 && newLevel > bar->mLevel) {
		tryPlayMugenSound(&gFightUIData.mFightSounds, bar->mLevelSounds[newLevel - 1].x, bar->mLevelSounds[newLevel - 1].y);
	}
	sprintf(bar->mCounterText, "%d", newLevel);
	changeMugenText(bar->mCounterTextID, bar->mCounterText);

	bar->mLevel = newLevel;
}

void enableDreamTimer()
{
	gFightUIData.mTime.mIsActive = 1;
}

void disableDreamTimer()
{
	gFightUIData.mTime.mIsActive = 0;
}

void resetDreamTimer()
{
	gFightUIData.mTime.mNow = 0;
	gFightUIData.mTime.mValue = getGlobalTimerDuration();
	gFightUIData.mTime.mIsFinished = 0;
	updateTimeDisplayText();
}

void setTimerFinished() {
	gFightUIData.mTime.mIsInfinite = 0;
	gFightUIData.mTime.mValue = 0;
	gFightUIData.mTime.mIsFinished = 0;
	updateTimeDisplayText();
}

static void setTimerFinishedInternal() {
	gFightUIData.mTime.mIsInfinite = 0;
	gFightUIData.mTime.mValue = 0;
	gFightUIData.mTime.mIsFinished = 1;
	updateTimeDisplayText();
	gFightUIData.mTime.mFinishedCB();
}


MugenAnimation * getDreamFightEffectAnimation(int tNumber)
{
	return getMugenAnimation(&gFightUIData.mFightFXAnimations, tNumber);
}

MugenSpriteFile * getDreamFightEffectSprites()
{
	return &gFightUIData.mFightFXSprites;
}

MugenSounds * getDreamCommonSounds()
{
	return &gFightUIData.mCommonSounds;
}

static void parseRoundText(char* tDst, char* tSrc, int tRoundNumber) {

	int i, o = 0;
	int n = strlen(tSrc);
	for (i = 0; i < n; i++) {
		if (tSrc[i] == '%' && i < n - 1 && tSrc[i + 1] == 'i') {
			int len = sprintf(&tDst[o], "%d", tRoundNumber);
			o += len;
			i++;
		}
		else {
			tDst[o] = tSrc[i];
			o++;
		}
	}
	tDst[o] = '\0';
}

void playDreamRoundAnimation(int tRound, void(*tFunc)())
{
	
	tRound--;
	if (gFightUIData.mRound.mHasCustomRoundAnimation[tRound]) {
		gFightUIData.mRound.mHasActiveAnimation = 1;
		playDisplayAnimation(&gFightUIData.mRound.mAnimationID, gFightUIData.mRound.mCustomRoundAnimations[tRound], &gFightUIData.mRound.mCustomRoundPositions[tRound], gFightUIData.mRound.mCustomRoundFaceDirection[tRound]);
	}
	else if (gFightUIData.mRound.mHasDefaultAnimation) {
		gFightUIData.mRound.mHasActiveAnimation = 1;
		playDisplayAnimation(&gFightUIData.mRound.mAnimationID, gFightUIData.mRound.mDefaultAnimation, &gFightUIData.mRound.mPosition, gFightUIData.mRound.mDefaultFaceDirection);
	}
	else {
		gFightUIData.mRound.mHasActiveAnimation = 0;
		parseRoundText(gFightUIData.mRound.mDisplayedText, gFightUIData.mRound.mText, tRound + 1);

		playDisplayText(&gFightUIData.mRound.mTextID, gFightUIData.mRound.mDisplayedText, gFightUIData.mRound.mPosition, gFightUIData.mRound.mFont);
	}

	gFightUIData.mRound.mCB = tFunc;
	gFightUIData.mRound.mDisplayNow = 0;
	gFightUIData.mRound.mHasPlayedSound = 0;
	gFightUIData.mRound.mRoundIndex = tRound;
	gFightUIData.mRound.mIsDisplayingRound = 1;
}

void playDreamFightAnimation(void(*tFunc)())
{
	playDisplayAnimation(&gFightUIData.mFight.mAnimationID, gFightUIData.mFight.mAnimation, &gFightUIData.mFight.mPosition, gFightUIData.mFight.mFaceDirection);

	gFightUIData.mFight.mCB = tFunc;
	gFightUIData.mFight.mDisplayNow = 0;
	gFightUIData.mFight.mHasPlayedSound = 0;
	gFightUIData.mFight.mIsDisplayingFight = 1;
}

void playDreamKOAnimation(void(*tFunc)())
{
	playDisplayAnimation(&gFightUIData.mKO.mAnimationID, gFightUIData.mKO.mAnimation, &gFightUIData.mKO.mPosition, gFightUIData.mKO.mFaceDirection);

	gFightUIData.mKO.mDisplayNow = 0;
	gFightUIData.mKO.mHasPlayedSound = 0;
	gFightUIData.mKO.mCB = tFunc;
	gFightUIData.mKO.mIsDisplaying = 1;
}

static void parseWinText(char* tDst, char* tSrc, char* tName, Position* oDisplayPosition, Position tPosition) {
	int i, o = 0;
	int bonus = 0;
	int n = strlen(tSrc);
	for (i = 0; i < n; i++) {
		if (tSrc[i] == '%' && i < n - 1 && tSrc[i + 1] == 's') {
			int len = sprintf(&tDst[o], "%s", tName);
			bonus += len - 1;
			o += len;
			i++;
		}
		else {
			tDst[o] = tSrc[i];
			o++;
		}
	}
	tDst[o] = '\0';

	*oDisplayPosition = tPosition;
}

void playDreamWinAnimation(char * tName, void(*tFunc)())
{
	parseWinText(gFightUIData.mWin.mDisplayedText, gFightUIData.mWin.mText, tName, &gFightUIData.mWin.mDisplayPosition, gFightUIData.mWin.mPosition);
	playDisplayText(&gFightUIData.mWin.mTextID, gFightUIData.mWin.mDisplayedText, gFightUIData.mWin.mDisplayPosition, gFightUIData.mWin.mFont);
	
	gFightUIData.mWin.mNow = 0;
	gFightUIData.mWin.mCB = tFunc;
	gFightUIData.mWin.mIsDisplaying = 1;
}

void playDreamContinueAnimation(void(*tAnimationFinishedFunc)(), void(*tContinuePressedFunc)())
{

	gFightUIData.mContinue.mFinishedCB = tAnimationFinishedFunc;
	gFightUIData.mContinue.mPressedContinueCB = tContinuePressedFunc;
	gFightUIData.mContinue.mValue = 10;
	gFightUIData.mContinue.mNow = 0;
	gFightUIData.mContinue.mDuration = 60;

	playDisplayText(&gFightUIData.mContinue.mContinueTextID, "Continue?", makePosition(160, 100, UI_BASE_Z), makeVector3DI(3, 0, 0));
	playDisplayText(&gFightUIData.mContinue.mValueTextID, "10", makePosition(160, 120, UI_BASE_Z), makeVector3DI(2, 0, 0));


	gFightUIData.mContinue.mIsActive = 1;
}

void setDreamTimeDisplayFinishedCB(void(*tTimeDisplayFinishedFunc)())
{
	gFightUIData.mTime.mFinishedCB = tTimeDisplayFinishedFunc;
}

static void setSingleUIComponentInvisibleForOneFrame(int tAnimationID) {
	if(tAnimationID != -1) setMugenAnimationInvisibleForOneFrame(tAnimationID);
}

static void setSingleUITextInvisibleForOneFrame(int tTextID) {
	if (tTextID != -1) setMugenTextInvisibleForOneFrame(tTextID);
}

void setDreamBarInvisibleForOneFrame()
{
	int i;
	for (i = 0; i < 2; i++) {
		setSingleUIComponentInvisibleForOneFrame(gFightUIData.mHealthBars[i].mBG0AnimationID);
		setSingleUIComponentInvisibleForOneFrame(gFightUIData.mHealthBars[i].mBG1AnimationID);
		setSingleUIComponentInvisibleForOneFrame(gFightUIData.mHealthBars[i].mMidAnimationID);
		setSingleUIComponentInvisibleForOneFrame(gFightUIData.mHealthBars[i].mFrontAnimationID);

		setSingleUIComponentInvisibleForOneFrame(gFightUIData.mPowerBars[i].mBG0AnimationID);
		setSingleUIComponentInvisibleForOneFrame(gFightUIData.mPowerBars[i].mBG1AnimationID);
		setSingleUIComponentInvisibleForOneFrame(gFightUIData.mPowerBars[i].mMidAnimationID);
		setSingleUIComponentInvisibleForOneFrame(gFightUIData.mPowerBars[i].mFrontAnimationID);
		setSingleUITextInvisibleForOneFrame(gFightUIData.mPowerBars[i].mCounterTextID);

		setSingleUIComponentInvisibleForOneFrame(gFightUIData.mFaces[i].mBGAnimationID);
		setSingleUIComponentInvisibleForOneFrame(gFightUIData.mFaces[i].mBG0AnimationID);
		setSingleUIComponentInvisibleForOneFrame(gFightUIData.mFaces[i].mBG1AnimationID);
		setSingleUIComponentInvisibleForOneFrame(gFightUIData.mFaces[i].mFaceAnimationID);

		setSingleUIComponentInvisibleForOneFrame(gFightUIData.mDisplayName[i].mBGAnimationID);

		for (int j = 0; j < gFightUIData.mWinIcons[i].mIconAmount; j++) {
			setSingleUIComponentInvisibleForOneFrame(gFightUIData.mWinIcons[i].mIconAnimationIDs[j]);
		}
		setSingleUITextInvisibleForOneFrame(gFightUIData.mDisplayName[i].mTextID);
	}

	setSingleUIComponentInvisibleForOneFrame(gFightUIData.mTime.mBGAnimationID);
	setSingleUITextInvisibleForOneFrame(gFightUIData.mTime.mTextID);
}

void setDreamNoMusicFlag()
{
	// TODO: BGM
}

void setTimerFreezeFlag()
{
	gFightUIData.mTime.mTimerFreezeFlag = 1;
}

void setTimerInfinite()
{
	gFightUIData.mTime.mIsInfinite = 1;
}

void setTimerFinite()
{
	if (isGlobalTimerInfinite()) setTimerInfinite();
	else gFightUIData.mTime.mIsInfinite = 0;
}

int isTimerFinished()
{
	return gFightUIData.mTime.mIsActive && !gFightUIData.mTime.mIsInfinite && gFightUIData.mTime.mIsFinished;
}

void setEnvironmentColor(Vector3DI tColors, int tTime, int tIsUnderCharacters)
{
	setAnimationPosition(gFightUIData.mEnvironmentEffects.mAnimationID, makePosition(0, 0, tIsUnderCharacters ? ENVIRONMENT_COLOR_LOWER_Z : ENVIRONMENT_COLOR_UPPER_Z));
	setAnimationSize(gFightUIData.mEnvironmentEffects.mAnimationID, makePosition(640, 480, 1), makePosition(0, 0, 0));
	setAnimationColor(gFightUIData.mEnvironmentEffects.mAnimationID, tColors.x / 255.0, tColors.y / 255.0, tColors.z / 255.0);

	gFightUIData.mEnvironmentEffects.mDuration = tTime;
	gFightUIData.mEnvironmentEffects.mNow = 0;
	gFightUIData.mEnvironmentEffects.mIsActive = 1;

}

// TODO: use environment shake
void setEnvironmentShake(int tDuration, double tFrequency, int tAmplitude, double tPhaseOffset)
{
	gFightUIData.mEnvironmentShake.mFrequency = tFrequency;
	gFightUIData.mEnvironmentShake.mAmplitude = tAmplitude;
	gFightUIData.mEnvironmentShake.mPhaseOffset = tPhaseOffset;

	gFightUIData.mEnvironmentShake.mNow = 0;
	gFightUIData.mEnvironmentShake.mDuration = tDuration;
	gFightUIData.mEnvironmentShake.mIsActive = 1;
}

static void addGeneralWinIcon(int tPlayer, Vector3DI tSprite, int tIsPerfect) {
	WinIcon* icon = &gFightUIData.mWinIcons[tPlayer];

	if (icon->mIconAmount >= icon->mIconUpToAmount) return;

	int id = icon->mIconAmount;
	Position pos = vecAdd2D(icon->mPosition, vecScale(icon->mOffset, id));	
	icon->mIconAnimations[id] = createOneFrameMugenAnimationForSprite(tSprite.x, tSprite.y);
	icon->mIconAnimationIDs[id] = addMugenAnimation(icon->mIconAnimations[id], &gFightUIData.mFightSprites, pos);

	icon->mHasIsPerfectIcon[id] = tIsPerfect;
	if (icon->mHasIsPerfectIcon[id]) {
		pos.z++;
		icon->mPerfectIconAnimations[id] = createOneFrameMugenAnimationForSprite(icon->mPerfectWin.x, icon->mPerfectWin.y);
		icon->mPerfectIconAnimationIDs[id] = addMugenAnimation(icon->mPerfectIconAnimations[id], &gFightUIData.mFightSprites, pos);
	}

	icon->mIconAmount++;
}

void addNormalWinIcon(int tPlayer, int tIsPerfect)
{
	WinIcon* icon = &gFightUIData.mWinIcons[tPlayer];
	addGeneralWinIcon(tPlayer, icon->mNormalWin, tIsPerfect);
}

void addSpecialWinIcon(int tPlayer, int tIsPerfect) {
	WinIcon* icon = &gFightUIData.mWinIcons[tPlayer];
	addGeneralWinIcon(tPlayer, icon->mSpecialWin, tIsPerfect);
}

void addHyperWinIcon(int tPlayer, int tIsPerfect) {
	WinIcon* icon = &gFightUIData.mWinIcons[tPlayer];
	addGeneralWinIcon(tPlayer, icon->mHyperWin, tIsPerfect);
}

void addThrowWinIcon(int tPlayer, int tIsPerfect) {
	WinIcon* icon = &gFightUIData.mWinIcons[tPlayer];
	addGeneralWinIcon(tPlayer, icon->mThrowWin, tIsPerfect);
}

void addCheeseWinIcon(int tPlayer, int tIsPerfect) {
	WinIcon* icon = &gFightUIData.mWinIcons[tPlayer];
	addGeneralWinIcon(tPlayer, icon->mCheeseWin, tIsPerfect);
}

void addTimeoverWinIcon(int tPlayer, int tIsPerfect) {
	WinIcon* icon = &gFightUIData.mWinIcons[tPlayer];
	addGeneralWinIcon(tPlayer, icon->mTimeOverWin, tIsPerfect);
}

void addSuicideWinIcon(int tPlayer, int tIsPerfect) {
	WinIcon* icon = &gFightUIData.mWinIcons[tPlayer];
	addGeneralWinIcon(tPlayer, icon->mSuicideWin, tIsPerfect);
}

void addTeammateWinIcon(int tPlayer, int tIsPerfect) {
	WinIcon* icon = &gFightUIData.mWinIcons[tPlayer];
	addGeneralWinIcon(tPlayer, icon->mTeammateWin, tIsPerfect);
}

static void removeSinglePlayerWinIcons(int tPlayer) {
	WinIcon* icon = &gFightUIData.mWinIcons[tPlayer];
	int i;
	for (i = 0; i < icon->mIconAmount; i++) {
		removeMugenAnimation(icon->mIconAnimationIDs[i]);
		destroyMugenAnimation(icon->mIconAnimations[i]);

		if (icon->mHasIsPerfectIcon[i]) {
			removeMugenAnimation(icon->mPerfectIconAnimationIDs[i]);
			destroyMugenAnimation(icon->mPerfectIconAnimations[i]);
		}
	}

	icon->mIconAmount = 0;
}

void removeAllWinIcons()
{
	int i;
	for (i = 0; i < 2; i++) {
		removeSinglePlayerWinIcons(i);
	}

}

void stopFightAndRoundAnimation()
{
	if (gFightUIData.mRound.mIsDisplayingRound) {
		removeRoundDisplay();
		gFightUIData.mRound.mIsDisplayingRound = 0;
	}

	if (gFightUIData.mFight.mIsDisplayingFight) {
		removeDisplayedAnimation(gFightUIData.mFight.mAnimationID);
		gFightUIData.mFight.mIsDisplayingFight = 0;
	}

	startControlCountdown();	
}

void setUIFaces() {
	setMugenAnimationSprites(gFightUIData.mFaces[0].mFaceAnimationID, getPlayerSprites(getRootPlayer(0)));
	setMugenAnimationSprites(gFightUIData.mFaces[1].mFaceAnimationID, getPlayerSprites(getRootPlayer(1)));
}

void setComboUIDisplay(int i, int tAmount)
{
	if (tAmount < 2) return;

	Combo* combo = &gFightUIData.mCombos[i];
	char number[20];
	sprintf(number, "%d", tAmount);

	changeMugenText(combo->mNumberTextID, number);
	combo->mDisplayNow = 0;
	combo->mIsActive = 1;
}
