#include "fightui.h"

#include <assert.h>

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
	Round mRound;
	FightDisplay mFight;
	KO mKO;
	WinDisplay mWin;
	Continue mContinue;
	WinIcon mWinIcons[2];

	ControlCountdown mControl;

	EnvironmentColorEffect mEnvironmentEffects;
	EnvironmentShakeEffect mEnvironmentShake;

	List mHitSparks;
} gData;

static void loadFightDefFilesFromScript(MugenDefScript* tScript, char* tDefPath) {
	char directory[1024];
	getPathToFile(directory, tDefPath);

	char fileName[1024], fullPath[1024];
	getMugenDefStringOrDefault(fileName, tScript, "Files", "sff", "NO_FILE");
	sprintf(fullPath, "%s%s", directory, fileName);
	setMugenSpriteFileReaderToUsePalette(3); // TODO: check
	gData.mFightSprites = loadMugenSpriteFileWithoutPalette(fullPath);
	setMugenSpriteFileReaderToNotUsePalette();

	gData.mFightAnimations = loadMugenAnimationFile(tDefPath);

	getMugenDefStringOrDefault(fileName, tScript, "Files", "fightfx.sff", "NO_FILE");
	sprintf(fullPath, "%s%s", directory, fileName);
	gData.mFightFXSprites = loadMugenSpriteFileWithoutPalette(fullPath);

	getMugenDefStringOrDefault(fileName, tScript, "Files", "fightfx.air", "NO_FILE");
	sprintf(fullPath, "%s%s", directory, fileName);
	gData.mFightFXAnimations = loadMugenAnimationFile(fullPath);

	getMugenDefStringOrDefault(fileName, tScript, "Files", "snd", "NO_FILE");
	sprintf(fullPath, "%s%s", directory, fileName);
	gData.mFightSounds = loadMugenSoundFile(fullPath);

	getMugenDefStringOrDefault(fileName, tScript, "Files", "common.snd", "NO_FILE");
	sprintf(fullPath, "%s%s", directory, fileName);
	gData.mCommonSounds = loadMugenSoundFile(fullPath);

}

static int loadSingleUIComponentWithFullComponentNameForStorageAndReturnIfLegit(MugenDefScript* tScript, MugenAnimations* tAnimations, Position tBasePosition, char* tGroupName, char* tComponentName, double tZ, MugenAnimation** oAnimation, int* oOwnsAnimation, Position* oPosition, int* oFaceDirection) {
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

static void loadSingleUIComponentWithFullComponentName(MugenDefScript* tScript, MugenSpriteFile* tSprites, MugenAnimations* tAnimations, Position tBasePosition, char* tGroupName, char* tComponentName, double tZ, MugenAnimation** oAnimation, int* oOwnsAnimation, int* oAnimationID, Position* oPosition, int tScaleCoordinateP) {
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

static void loadSingleUIComponent(int i, MugenDefScript* tScript, MugenSpriteFile* tSprites, MugenAnimations* tAnimations, Position tBasePosition, char* tGroupName, char* tComponentName, double tZ, MugenAnimation** oAnimation, int* oOwnsAnimation, int* oAnimationID, Position* oPosition, int tScaleCoordinateP) {
	char name[1024];

	sprintf(name, "p%d.%s", i + 1, tComponentName);
	loadSingleUIComponentWithFullComponentName(tScript, tSprites, tAnimations, tBasePosition, tGroupName, name, tZ, oAnimation, oOwnsAnimation, oAnimationID, oPosition, tScaleCoordinateP);
}

static void loadSingleUITextWithFullComponentNameForStorage(MugenDefScript* tScript, Position tBasePosition, char* tGroupName, char* tComponentName, double tZ, Position* oPosition, int tIsReadingText, char* tText, Vector3DI* oFontData) {
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


static void loadSingleUITextWithFullComponentName(MugenDefScript* tScript, Position tBasePosition, char* tGroupName, char* tComponentName, double tZ, int* oTextID, Position* oPosition, int tIsReadingText, char* tText, Vector3DI* oFontData) {
	loadSingleUITextWithFullComponentNameForStorage(tScript, tBasePosition, tGroupName, tComponentName, tZ, oPosition, tIsReadingText, tText, oFontData);

	*oTextID = addMugenText(tText, *oPosition, oFontData->x);
	setMugenTextColor(*oTextID, getMugenTextColorFromMugenTextColorIndex(oFontData->y));
	setMugenTextAlignment(*oTextID, getMugenTextAlignmentFromMugenAlignmentIndex(oFontData->z));
}


static void loadSingleUIText(int i, MugenDefScript* tScript, Position tBasePosition, char* tGroupName, char* tComponentName, double tZ, int* oTextID, Position* oPosition, int tIsReadingText, char* tText, Vector3DI* oFontData) {
	char name[1024];

	sprintf(name, "p%d.%s", i + 1, tComponentName);
	loadSingleUITextWithFullComponentName(tScript, tBasePosition, tGroupName, name, tZ, oTextID, oPosition, tIsReadingText, tText, oFontData);
}

static void loadSingleHealthBar(int i, MugenDefScript* tScript) {
	char name[1024];

	HealthBar* bar = &gData.mHealthBars[i];


	Position basePosition;
	sprintf(name, "p%d.pos", i + 1);
	basePosition = getMugenDefVectorOrDefault(tScript, "Lifebar", name, makePosition(0,0,0));
	basePosition.z = UI_BASE_Z;

	int coordP = COORD_P; // TODO
	loadSingleUIComponent(i, tScript, &gData.mFightSprites, &gData.mFightAnimations, basePosition, "Lifebar", "bg0", 1, &bar->mBG0Animation, &bar->mOwnsBG0Animation, &bar->mBG0AnimationID, &bar->mBG0Position, coordP);
	loadSingleUIComponent(i, tScript, &gData.mFightSprites, &gData.mFightAnimations, basePosition, "Lifebar", "bg1", 2, &bar->mBG1Animation, &bar->mOwnsBG1Animation, &bar->mBG1AnimationID, &bar->mBG1Position, coordP);
	loadSingleUIComponent(i, tScript, &gData.mFightSprites, &gData.mFightAnimations, basePosition, "Lifebar", "mid", 3, &bar->mMidAnimation, &bar->mOwnsMidAnimation,  &bar->mMidAnimationID, &bar->mMidPosition, coordP);
	loadSingleUIComponent(i, tScript, &gData.mFightSprites, &gData.mFightAnimations, basePosition, "Lifebar", "front", 4, &bar->mFrontAnimation, &bar->mOwnsFrontAnimation, &bar->mFrontAnimationID, &bar->mFrontPosition, coordP);

	sprintf(name, "p%d.range.x", i + 1);
	bar->mHealthRangeX = getMugenDefVectorOrDefault(tScript, "Lifebar", name, makePosition(0, 0, 0));

	bar->mPercentage = 1;
	bar->mDisplayedPercentage = 1;
	bar->mIsPaused = 0;
}

static void loadSinglePowerBar(int i, MugenDefScript* tScript) {
	char name[1024];

	PowerBar* bar = &gData.mPowerBars[i];

	Position basePosition;
	sprintf(name, "p%d.pos", i + 1);
	basePosition = getMugenDefVectorOrDefault(tScript, "Powerbar", name, makePosition(0, 0, 0));
	basePosition.z = UI_BASE_Z;

	int coordP = COORD_P; // TODO
	loadSingleUIComponent(i, tScript, &gData.mFightSprites, &gData.mFightAnimations, basePosition, "Powerbar", "bg0", 1, &bar->mBG0Animation, &bar->mOwnsBG0Animation, &bar->mBG0AnimationID, &bar->mBG0Position, coordP);
	loadSingleUIComponent(i, tScript, &gData.mFightSprites, &gData.mFightAnimations, basePosition, "Powerbar", "bg1", 2, &bar->mBG1Animation, &bar->mOwnsBG1Animation, &bar->mBG1AnimationID, &bar->mBG1Position, coordP);
	loadSingleUIComponent(i, tScript, &gData.mFightSprites, &gData.mFightAnimations, basePosition, "Powerbar", "mid", 3, &bar->mMidAnimation, &bar->mOwnsMidAnimation, &bar->mMidAnimationID, &bar->mMidPosition, coordP);
	loadSingleUIComponent(i, tScript, &gData.mFightSprites, &gData.mFightAnimations, basePosition, "Powerbar", "front", 4, &bar->mFrontAnimation, &bar->mOwnsFrontAnimation, &bar->mFrontAnimationID, &bar->mFrontPosition, coordP);
	
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

	Face* face = &gData.mFaces[i];

	Position basePosition;
	sprintf(name, "p%d.pos", i + 1);
	basePosition = getMugenDefVectorOrDefault(tScript, "Face", name, makePosition(0, 0, 0));
	basePosition.z = UI_BASE_Z;

	DreamPlayer* p = getRootPlayer(i);
	int coordP = COORD_P; // TODO
	loadSingleUIComponent(i, tScript, &gData.mFightSprites, &gData.mFightAnimations, basePosition, "Face", "bg", 1, &face->mBGAnimation, &face->mOwnsBGAnimation, &face->mBGAnimationID, &face->mBGPosition, coordP);
	loadSingleUIComponent(i, tScript, &gData.mFightSprites, &gData.mFightAnimations, basePosition, "Face", "bg0", 1, &face->mBG0Animation, &face->mOwnsBG0Animation, &face->mBG0AnimationID, &face->mBG0Position, coordP);
	loadSingleUIComponent(i, tScript, &gData.mFightSprites, &gData.mFightAnimations, basePosition, "Face", "bg1", 2, &face->mBG1Animation, &face->mOwnsBG1Animation, &face->mBG1AnimationID, &face->mBG1Position, coordP);
	loadSingleUIComponent(i, tScript, NULL, getPlayerAnimations(p), basePosition, "Face", "face", 3, &face->mFaceAnimation, &face->mOwnsFaceAnimation, &face->mFaceAnimationID, &face->mFacePosition, getPlayerCoordinateP(p));

}

static void loadSingleName(int i, MugenDefScript* tScript) {
	char name[1024];

	DisplayName* displayName = &gData.mDisplayName[i];

	Position basePosition;
	sprintf(name, "p%d.pos", i + 1);
	basePosition = getMugenDefVectorOrDefault(tScript, "Name", name, makePosition(0, 0, 0));
	basePosition.z = UI_BASE_Z;

	int coordP = COORD_P; // TODO
	loadSingleUIComponent(i, tScript, &gData.mFightSprites, &gData.mFightAnimations, basePosition, "Name", "bg", 0, &displayName->mBGAnimation, &displayName->mOwnsBGAnimation, &displayName->mBGAnimationID, &displayName->mBGPosition, coordP);

	char displayNameText[1024];
	sprintf(displayNameText, "%s", getPlayerDisplayName(getRootPlayer(i)));
	loadSingleUIText(i, tScript, basePosition, "Name", "name", 10, &displayName->mTextID, &displayName->mTextPosition, 0, displayNameText, &displayName->mFont);
}

static void loadSingleWinIcon(int i, MugenDefScript* tScript) {
	char name[1024];

	WinIcon* winIcon = &gData.mWinIcons[i];

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

	winIcon->mIconAnimationIDs = allocMemory(sizeof(int) * winIcon->mIconUpToAmount);
	winIcon->mIconAnimations = allocMemory(sizeof(MugenAnimation*) * winIcon->mIconUpToAmount);
	winIcon->mHasIsPerfectIcon = allocMemory(sizeof(int*) * winIcon->mIconUpToAmount);
	winIcon->mPerfectIconAnimations = allocMemory(sizeof(MugenAnimation*) * winIcon->mIconUpToAmount);
	winIcon->mPerfectIconAnimationIDs = allocMemory(sizeof(int*) * winIcon->mIconUpToAmount);
	winIcon->mIconAmount = 0;
}

static void loadPlayerUIs(MugenDefScript* tScript) {
	int i;
	for (i = 0; i < 2; i++) {
		loadSingleHealthBar(i, tScript);
		loadSinglePowerBar(i, tScript);
		loadSingleFace(i, tScript);
		loadSingleName(i, tScript);
		loadSingleWinIcon(i, tScript);
		setDreamLifeBarPercentage(getRootPlayer(i), 1);
		setDreamPowerBarPercentage(getRootPlayer(i), 0, 0);
	}
}

static void playDisplayText(int* oTextID, char* tText, Position tPosition, Vector3DI tFont);

static void loadTimer(MugenDefScript* tScript) {
	Position basePosition;
	basePosition = getMugenDefVectorOrDefault(tScript, "Time", "pos", makePosition(0, 0, 0));
	basePosition.z = UI_BASE_Z;

	int coordP = COORD_P; // TODO
	loadSingleUIComponentWithFullComponentName(tScript, &gData.mFightSprites, &gData.mFightAnimations, basePosition, "Time", "bg", 0, &gData.mTime.mBGAnimation, &gData.mTime.mOwnsBGAnimation, &gData.mTime.mBGAnimationID, &gData.mTime.mBGPosition, coordP);

	gData.mTime.mPosition = getMugenDefVectorOrDefault(tScript, "Time", "counter.offset", makePosition(0, 0, 0));
	gData.mTime.mPosition = vecAdd(gData.mTime.mPosition, basePosition);
	gData.mTime.mPosition.z+=10;

	gData.mTime.mFramesPerCount = getMugenDefIntegerOrDefault(tScript, "Time", "framespercount", 60);

	gData.mTime.mIsActive = 0;

	gData.mTime.mFont = getMugenDefVectorIOrDefault(tScript, "Time", "counter.font", makeVector3DI(1, 0, 0));



	playDisplayText(&gData.mTime.mTextID, "99", gData.mTime.mPosition, gData.mTime.mFont);
	
	gData.mTime.mIsFinished = 0;
	gData.mTime.mValue = 99;
	gData.mTime.mNow = 0;
	gData.mTime.mTimerFreezeFlag = 0;
	resetDreamTimer();
}

static void loadRound(MugenDefScript* tScript) {
	Position basePosition;
	basePosition = getMugenDefVectorOrDefault(tScript, "Round", "pos", makePosition(0,0,0));
	basePosition.z = UI_BASE_Z;

	gData.mRound.mRoundTime = getMugenDefIntegerOrDefault(tScript, "Round", "round.time", 0);
	gData.mRound.mDisplayTime = getMugenDefIntegerOrDefault(tScript, "Round", "round.default.displaytime", 0);
	gData.mRound.mSoundTime = getMugenDefIntegerOrDefault(tScript, "Round", "round.sndtime", 0);

	if (isMugenDefVariable(tScript, "Round", "round.default.anim")) {
		gData.mRound.mHasDefaultAnimation = 1;
		assert(loadSingleUIComponentWithFullComponentNameForStorageAndReturnIfLegit(tScript, &gData.mFightAnimations, basePosition, "Round", "round.default", 1, &gData.mRound.mDefaultAnimation, &gData.mRound.mOwnsDefaultAnimation, &gData.mRound.mPosition, &gData.mRound.mDefaultFaceDirection));
	}
	else {
		gData.mRound.mHasDefaultAnimation = 0;
		loadSingleUITextWithFullComponentNameForStorage(tScript, basePosition, "Round", "round.default", 1, &gData.mRound.mPosition, 1, gData.mRound.mText, &gData.mRound.mFont);
	}


	int i;
	for (i = 0; i < 9; i++) {
		char name[1024];
		sprintf(name, "round%d.anim", i + 1);
		if (!isMugenDefVariable(tScript, "Round", name)) {
			gData.mRound.mHasCustomRoundAnimation[i] = 0;
		}
		else {
			gData.mRound.mHasCustomRoundAnimation[i] = 1;
			sprintf(name, "round%d", i + 1);
			assert(loadSingleUIComponentWithFullComponentNameForStorageAndReturnIfLegit(tScript, &gData.mFightAnimations, basePosition, "Round", name, 1, &gData.mRound.mCustomRoundAnimations[i], &gData.mRound.mOwnsCustomRoundAnimations[i], &gData.mRound.mCustomRoundPositions[i], &gData.mRound.mCustomRoundFaceDirection[i]));
		}

		sprintf(name, "round%d.snd", i + 1);
		if (!isMugenDefVariable(tScript, "Round", name)) {
			gData.mRound.mHasRoundSound[i] = 0;
		}
		else {
			gData.mRound.mHasRoundSound[i] = 1;
			gData.mRound.mRoundSounds[i] = getMugenDefVectorIVariable(tScript, "Round", name);
		}
	}

	gData.mRound.mIsDisplayingRound = 0;
}

static void loadFight(MugenDefScript* tScript) {
	Position basePosition;
	basePosition = getMugenDefVectorOrDefault(tScript, "Round", "pos", makePosition(0, 0, 0));
	basePosition.z = UI_BASE_Z;

	assert(loadSingleUIComponentWithFullComponentNameForStorageAndReturnIfLegit(tScript, &gData.mFightAnimations, basePosition, "Round", "fight", 1, &gData.mFight.mAnimation, &gData.mFight.mOwnsAnimation, &gData.mFight.mPosition, &gData.mFight.mFaceDirection));
	gData.mFight.mSoundTime = getMugenDefIntegerOrDefault(tScript, "Round", "fight.sndtime", 0);
	gData.mFight.mSound = getMugenDefVectorIOrDefault(tScript, "Round", "fight.snd", makeVector3DI(1, 0, 0));

	gData.mFight.mIsDisplayingFight = 0;
}

static void loadKO(MugenDefScript* tScript) {
	Position basePosition;
	basePosition = getMugenDefVectorOrDefault(tScript, "Round", "pos", makePosition(0, 0, 0));
	basePosition.z = UI_BASE_Z;

	assert(loadSingleUIComponentWithFullComponentNameForStorageAndReturnIfLegit(tScript, &gData.mFightAnimations, basePosition, "Round", "ko", 1, &gData.mKO.mAnimation, &gData.mKO.mOwnsAnimation, &gData.mKO.mPosition, &gData.mKO.mFaceDirection));
	gData.mKO.mSoundTime = getMugenDefIntegerOrDefault(tScript, "Round", "ko.sndtime", 0);
	gData.mKO.mSound = getMugenDefVectorIOrDefault(tScript, "Round", "ko.snd", makeVector3DI(1, 0, 0));

	gData.mKO.mIsDisplaying = 0;
}

static void loadWinDisplay(MugenDefScript* tScript) {
	Position basePosition;
	basePosition = getMugenDefVectorOrDefault(tScript, "Round", "pos", makePosition(0, 0, 0));
	basePosition.z = UI_BASE_Z;

	loadSingleUITextWithFullComponentNameForStorage(tScript, basePosition, "Round", "win", 1, &gData.mWin.mPosition, 1, gData.mWin.mText, &gData.mWin.mFont);

	gData.mWin.mDisplayTime = getMugenDefIntegerOrDefault(tScript, "Round", "win.displaytime", 0);

	gData.mWin.mIsDisplaying = 0;
}

static void loadControl(MugenDefScript* tScript) {
	gData.mControl.mControlReturnTime = getMugenDefIntegerOrDefault(tScript, "Round", "ctrl.time", 0);

	gData.mControl.mIsCountingDownControl = 0;
}

static void loadHitSparks() {
	gData.mHitSparks = new_list();
}

static void loadContinue() {
	gData.mContinue.mIsActive = 0;
}

static void loadEnvironmentColorEffects() {
	gData.mEnvironmentEffects.mWhiteTexture = createWhiteTexture();
	gData.mEnvironmentEffects.mAnimationID = playOneFrameAnimationLoop(makePosition(0, 0, 0), &gData.mEnvironmentEffects.mWhiteTexture);
	setAnimationScale(gData.mEnvironmentEffects.mAnimationID, makePosition(0, 0, 0), makePosition(0, 0, 0));

	gData.mEnvironmentEffects.mIsActive = 0;
}

static void loadEnvironmentShakeEffects() {
	gData.mEnvironmentShake.mIsActive = 0;
}

static void loadFightUI(void* tData) {
	(void)tData;

	char defPath[1024];
	strcpy(defPath, "assets/data/fight.def"); // TODO
	MugenDefScript script = loadMugenDefScript(defPath);
	
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
	unloadMugenSpriteFile(&gData.mFightSprites);
	unloadMugenAnimationFile(&gData.mFightAnimations);
	unloadMugenSpriteFile(&gData.mFightFXSprites);
	unloadMugenAnimationFile(&gData.mFightFXAnimations);

	unloadMugenSoundFile(&gData.mFightSounds);
	unloadMugenSoundFile(&gData.mCommonSounds);
}



static void unloadSingleHealthBar(int i) {
	HealthBar* bar = &gData.mHealthBars[i];

	unloadSingleOptionalUIComponent(bar->mBG0Animation, bar->mOwnsBG0Animation, bar->mBG0AnimationID);
	unloadSingleOptionalUIComponent(bar->mBG1Animation, bar->mOwnsBG1Animation, bar->mBG1AnimationID);
	unloadSingleOptionalUIComponent(bar->mMidAnimation, bar->mOwnsMidAnimation, bar->mMidAnimationID);
	unloadSingleOptionalUIComponent(bar->mFrontAnimation, bar->mOwnsFrontAnimation, bar->mFrontAnimationID);
}

static void unloadSinglePowerBar(int i) {
	PowerBar* bar = &gData.mPowerBars[i];

	unloadSingleOptionalUIComponent(bar->mBG0Animation, bar->mOwnsBG0Animation, bar->mBG0AnimationID);
	unloadSingleOptionalUIComponent(bar->mBG1Animation, bar->mOwnsBG1Animation, bar->mBG1AnimationID);
	unloadSingleOptionalUIComponent(bar->mMidAnimation, bar->mOwnsMidAnimation, bar->mMidAnimationID);
	unloadSingleOptionalUIComponent(bar->mFrontAnimation, bar->mOwnsFrontAnimation, bar->mFrontAnimationID);
}

static void unloadSingleFace(int i) {
	Face* face = &gData.mFaces[i];

	unloadSingleOptionalUIComponent(face->mBGAnimation, face->mOwnsBGAnimation, face->mBGAnimationID);
	unloadSingleOptionalUIComponent(face->mBG0Animation, face->mOwnsBG0Animation, face->mBG0AnimationID);
	unloadSingleOptionalUIComponent(face->mBG1Animation, face->mOwnsBG1Animation, face->mBG1AnimationID);
	unloadSingleOptionalUIComponent(face->mFaceAnimation, face->mOwnsFaceAnimation, face->mFaceAnimationID);
}

static void unloadSingleName(int i) {
	DisplayName* displayName = &gData.mDisplayName[i];

	unloadSingleOptionalUIComponent(displayName->mBGAnimation, displayName->mOwnsBGAnimation, displayName->mBGAnimationID);
}


static void unloadSingleWinIcon(int i) {
	WinIcon* winIcon = &gData.mWinIcons[i];

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
	unloadSingleOptionalUIComponent(gData.mTime.mBGAnimation, gData.mTime.mOwnsBGAnimation, gData.mTime.mBGAnimationID);
}

static void unloadRound() {
	if (gData.mRound.mHasDefaultAnimation) {
		gData.mRound.mHasDefaultAnimation = 1;
		unloadSingleUIComponent(gData.mRound.mDefaultAnimation, gData.mRound.mOwnsDefaultAnimation);
	}

	int i;
	for (i = 0; i < 9; i++) {
		if (gData.mRound.mHasCustomRoundAnimation[i]) {
			unloadSingleUIComponent(gData.mRound.mCustomRoundAnimations[i], gData.mRound.mOwnsCustomRoundAnimations[i]);
		}
	}
}

static void unloadFight() {
	unloadSingleUIComponent(gData.mFight.mAnimation, gData.mFight.mOwnsAnimation);
}

static void unloadKO() {	
	unloadSingleUIComponent(gData.mKO.mAnimation, gData.mKO.mOwnsAnimation);
}

static void unloadHitSparks() {
	delete_list(&gData.mHitSparks);
}

static void unloadEnvironmentColorEffects() {
	unloadTexture(gData.mEnvironmentEffects.mWhiteTexture);
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

typedef struct {
	Position mPosition;
	int mAnimationID;
} HitSpark;

static int updateSingleHitSpark(void* tCaller, void* tData) {
	(void)tCaller;

	HitSpark* e = tData;

	if (!getMugenAnimationRemainingAnimationTime(e->mAnimationID)) {
		removeMugenAnimation(e->mAnimationID);
		return 1;
	}

	return 0;
}

static void updateHitSparks() {
	list_remove_predicate(&gData.mHitSparks, updateSingleHitSpark, NULL);
}

static void setBarToPercentage(int tAnimationID, Vector3D tRange, double tPercentage) {
	double fullSize = fabs(tRange.y - tRange.x);
	int newSize = (int)(fullSize * tPercentage);
	setMugenAnimationRectangleWidth(tAnimationID, newSize);
}

static void updateSingleHealthBar(int i) {
	HealthBar* bar = &gData.mHealthBars[i];

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

static void updateHealthBars() {
	int i;
	for (i = 0; i < 2; i++) {
		updateSingleHealthBar(i);
	}
}


static void playDisplayAnimation(int* oAnimationID, MugenAnimation* tAnimation, Position* tBasePosition, int tFaceDirection) {
	*oAnimationID = addMugenAnimation(tAnimation, &gData.mFightSprites, makePosition(0,0,0)); 
	setMugenAnimationBasePosition(*oAnimationID, tBasePosition);

	if (tFaceDirection == -1) {
		setMugenAnimationFaceDirection(*oAnimationID, 0);
	}
}

static void removeDisplayedAnimation(int tAnimationID) {
	removeMugenAnimation(tAnimationID);
}


static void playDisplayText(int* oTextID, char* tText, Position tPosition, Vector3DI tFont) {
	*oTextID = addMugenText(tText, tPosition, tFont.x);
	setMugenTextColor(*oTextID, getMugenTextColorFromMugenTextColorIndex(tFont.y));
	setMugenTextAlignment(*oTextID, getMugenTextAlignmentFromMugenAlignmentIndex(tFont.z));
}

static void removeDisplayedText(int tTextID) {
	removeMugenText(tTextID);
}

static void updateRoundSound() {
	int round = gData.mRound.mRoundIndex;
	if (gData.mRound.mDisplayNow >= gData.mRound.mSoundTime && gData.mRound.mHasRoundSound[round] && !gData.mRound.mHasPlayedSound) {
		tryPlayMugenSound(&gData.mFightSounds, gData.mRound.mRoundSounds[round].x, gData.mRound.mRoundSounds[round].y);
		gData.mRound.mHasPlayedSound = 1;
	}
}

static void updateRoundFinish() {
	if (gData.mRound.mDisplayNow >= gData.mRound.mDisplayTime) {

		if (gData.mRound.mHasActiveAnimation) {
			removeDisplayedAnimation(gData.mRound.mAnimationID);
		}
		else {
			removeDisplayedText(gData.mRound.mTextID);
		}

		gData.mRound.mCB();
		gData.mRound.mIsDisplayingRound = 0;
	}
}

static void updateRoundDisplay() {
	if (!gData.mRound.mIsDisplayingRound) return;

	gData.mRound.mDisplayNow++;

	updateRoundSound();
	updateRoundFinish();
}

static void startControlCountdown() {
	gData.mControl.mNow = 0;
	gData.mControl.mIsCountingDownControl = 1;
}

static void updateFightSound() {

	if (gData.mFight.mDisplayNow >= gData.mFight.mSoundTime && !gData.mFight.mHasPlayedSound) {
		tryPlayMugenSound(&gData.mFightSounds, gData.mFight.mSound.x, gData.mFight.mSound.y);
		gData.mFight.mHasPlayedSound = 1;
	}
}


static void updateFightFinish() {
	if (!getMugenAnimationRemainingAnimationTime(gData.mFight.mAnimationID)) {
		removeDisplayedAnimation(gData.mFight.mAnimationID);
		gData.mFight.mCB();
		startControlCountdown();
		gData.mFight.mIsDisplayingFight = 0;
	}
}

static void updateFightDisplay() {
	if (!gData.mFight.mIsDisplayingFight) return;

	gData.mFight.mDisplayNow++;
	updateFightSound();
	updateFightFinish();
}

static void updateKOSound() {

	if (gData.mKO.mDisplayNow >= gData.mKO.mSoundTime && !gData.mKO.mHasPlayedSound) {
		tryPlayMugenSound(&gData.mFightSounds, gData.mKO.mSound.x, gData.mKO.mSound.y);
		gData.mKO.mHasPlayedSound = 1;
	}
}

static void updateKOFinish() {
	if (!getMugenAnimationRemainingAnimationTime(gData.mKO.mAnimationID)) {
		removeDisplayedAnimation(gData.mKO.mAnimationID);
		gData.mKO.mCB();
		gData.mKO.mIsDisplaying = 0;
	}
}

static void updateKODisplay() {
	if (!gData.mKO.mIsDisplaying) return;

	gData.mKO.mDisplayNow++;
	updateKOSound();
	updateKOFinish();
}


static void updateWinDisplay() {
	if (!gData.mWin.mIsDisplaying) return;

	gData.mWin.mNow++;
	if (gData.mWin.mNow >= gData.mWin.mDisplayTime || hasPressedStartFlank()) {
		removeDisplayedText(gData.mWin.mTextID);
		gData.mWin.mCB();
		gData.mWin.mIsDisplaying = 0;
	}
}


static void updateControlCountdown() {
	if (!gData.mControl.mIsCountingDownControl) return;

	gData.mControl.mNow++;
	if (gData.mControl.mNow >= gData.mControl.mControlReturnTime) {
		setPlayerControl(getRootPlayer(0), 1);
		setPlayerControl(getRootPlayer(1), 1);
		gData.mControl.mIsCountingDownControl = 0;
	}
}

static void updateTimeDisplayText() {
	char text[10];
	if (gData.mTime.mIsInfinite) {
		sprintf(text, "o");
	}
	else {
		if (gData.mTime.mValue < 10) sprintf(text, "0%d", gData.mTime.mValue);
		else sprintf(text, "%d", gData.mTime.mValue);
	}

	changeMugenText(gData.mTime.mTextID, text);
}

static void setTimerFinishedInternal();

static void updateTimeDisplay() {
	if (!gData.mTime.mIsActive) return;
	if (gData.mTime.mIsInfinite) return;
	if (gData.mTime.mIsFinished) return;
	if (gData.mTime.mTimerFreezeFlag) {
		gData.mTime.mTimerFreezeFlag = 0;
		return;
	}

	gData.mTime.mNow++;
	if (gData.mTime.mNow >= gData.mTime.mFramesPerCount) {
		gData.mTime.mNow = 0;
		gData.mTime.mValue--;
		if (gData.mTime.mValue < 0) {
			setTimerFinishedInternal();
		}

		updateTimeDisplayText();
	}
}

static void setContinueInactive() {
	removeMugenText(gData.mContinue.mContinueTextID);
	removeMugenText(gData.mContinue.mValueTextID);
	gData.mContinue.mIsActive = 0;
}


static void decreaseContinueCounter() {
	gData.mContinue.mValue--;
	char text[10];
	sprintf(text, "%d", gData.mContinue.mValue);
	changeMugenText(gData.mContinue.mValueTextID, text);
}

static void updateContinueDisplay() {
	if (!gData.mContinue.mIsActive) return;

	if (hasPressedStartFlank()) {
		setContinueInactive();
		gData.mContinue.mPressedContinueCB();
		return;
	}
	
	gData.mContinue.mNow++;
	if (gData.mContinue.mNow >= gData.mContinue.mDuration || hasPressedAFlank()) {
		gData.mContinue.mNow = 0;
		if (!gData.mContinue.mValue) {
			setContinueInactive();
			gData.mContinue.mFinishedCB();
			return;
		}

		decreaseContinueCounter();
	}
}

static void updateEnvironmentColor() {
	if (!gData.mEnvironmentEffects.mIsActive) return;

	setDreamStageLayer1InvisibleForOneFrame();

	gData.mEnvironmentEffects.mNow++;
	if (gData.mEnvironmentEffects.mNow >= gData.mEnvironmentEffects.mDuration) {
		setAnimationScale(gData.mEnvironmentEffects.mAnimationID, makePosition(0, 0, 0), makePosition(0, 0, 0));
		gData.mEnvironmentEffects.mIsActive = 0;
	}
}

static void updateFightUI(void* tData) {
	(void)tData;
	updateHitSparks();
	updateHealthBars();
	updateRoundDisplay();
	updateFightDisplay();
	updateKODisplay();
	updateWinDisplay();
	updateControlCountdown();
	updateTimeDisplay();
	updateContinueDisplay();
	updateEnvironmentColor();
}




ActorBlueprint DreamFightUIBP = {
	.mLoad = loadFightUI,
	.mUnload = unloadFightUI,
	.mUpdate = updateFightUI,
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
		spriteFile = &gData.mFightFXSprites;
		if (!hasMugenAnimation(&gData.mFightFXAnimations, tNumber)) return;
		anim = getMugenAnimation(&gData.mFightFXAnimations, tNumber);
	}

	HitSpark* e = allocMemory(sizeof(HitSpark));
	
	e->mPosition = tPosition;
	e->mPosition.z = HITSPARK_BASE_Z;
	e->mAnimationID = addMugenAnimation(anim, spriteFile, getDreamStageCoordinateSystemOffset(tPositionCoordinateP));
	setMugenAnimationBasePosition(e->mAnimationID, &e->mPosition);
	setMugenAnimationCameraPositionReference(e->mAnimationID, getDreamMugenStageHandlerCameraPositionReference());
	if (!tIsFacingRight) {
		setMugenAnimationFaceDirection(e->mAnimationID, tIsFacingRight);
	}
	list_push_back_owned(&gData.mHitSparks, e);
}

void addDreamDustCloud(Position tPosition, int tIsFacingRight, int tCoordinateP)
{
	Position pos = vecAdd(tPosition, getDreamStageCoordinateSystemOffset(tCoordinateP));
	int id = addMugenAnimation(getMugenAnimation(&gData.mFightFXAnimations, 120), &gData.mFightFXSprites, pos);
	setMugenAnimationNoLoop(id);
	setMugenAnimationCameraPositionReference(id, getDreamMugenStageHandlerCameraPositionReference());
	if (!tIsFacingRight) {
		setMugenAnimationFaceDirection(id, tIsFacingRight);
	}
}

void setDreamLifeBarPercentage(DreamPlayer* tPlayer, double tPercentage)
{
	HealthBar* bar = &gData.mHealthBars[tPlayer->mRootID];

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
	PowerBar* bar = &gData.mPowerBars[tPlayer->mRootID];
	setBarToPercentage(bar->mFrontAnimationID, bar->mPowerRangeX, tPercentage);

	int newLevel = tValue / 1000;

	if (newLevel >= 1 && newLevel > bar->mLevel) {
		tryPlayMugenSound(&gData.mFightSounds, bar->mLevelSounds[newLevel - 1].x, bar->mLevelSounds[newLevel - 1].y);
	}
	sprintf(bar->mCounterText, "%d", newLevel);
	changeMugenText(bar->mCounterTextID, bar->mCounterText);

	bar->mLevel = newLevel;
}

void enableDreamTimer()
{
	gData.mTime.mIsActive = 1;
}

void disableDreamTimer()
{
	gData.mTime.mIsActive = 0;
}

void resetDreamTimer()
{
	gData.mTime.mNow = 0;
	gData.mTime.mValue = 99;
	gData.mTime.mIsFinished = 0;
	updateTimeDisplayText();
}

void setTimerFinished() {
	gData.mTime.mIsInfinite = 0;
	gData.mTime.mValue = 0;
	gData.mTime.mIsFinished = 0;
	updateTimeDisplayText();
}

static void setTimerFinishedInternal() {
	gData.mTime.mIsInfinite = 0;
	gData.mTime.mValue = 0;
	gData.mTime.mIsFinished = 1;
	updateTimeDisplayText();
	gData.mTime.mFinishedCB();
}


MugenAnimation * getDreamFightEffectAnimation(int tNumber)
{
	return getMugenAnimation(&gData.mFightFXAnimations, tNumber);
}

MugenSpriteFile * getDreamFightEffectSprites()
{
	return &gData.mFightFXSprites;
}

MugenSounds * getDreamCommonSounds()
{
	return &gData.mCommonSounds;
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
	if (gData.mRound.mHasCustomRoundAnimation[tRound]) {
		gData.mRound.mHasActiveAnimation = 1;
		playDisplayAnimation(&gData.mRound.mAnimationID, gData.mRound.mCustomRoundAnimations[tRound], &gData.mRound.mCustomRoundPositions[tRound], gData.mRound.mCustomRoundFaceDirection[tRound]);
	}
	else if (gData.mRound.mHasDefaultAnimation) {
		gData.mRound.mHasActiveAnimation = 1;
		playDisplayAnimation(&gData.mRound.mAnimationID, gData.mRound.mDefaultAnimation, &gData.mRound.mPosition, gData.mRound.mDefaultFaceDirection);
	}
	else {
		gData.mRound.mHasActiveAnimation = 0;
		parseRoundText(gData.mRound.mDisplayedText, gData.mRound.mText, tRound + 1);

		playDisplayText(&gData.mRound.mTextID, gData.mRound.mDisplayedText, gData.mRound.mPosition, gData.mRound.mFont);
	}

	gData.mRound.mCB = tFunc;
	gData.mRound.mDisplayNow = 0;
	gData.mRound.mHasPlayedSound = 0;
	gData.mRound.mRoundIndex = tRound;
	gData.mRound.mIsDisplayingRound = 1;
}

void playDreamFightAnimation(void(*tFunc)())
{
	playDisplayAnimation(&gData.mFight.mAnimationID, gData.mFight.mAnimation, &gData.mFight.mPosition, gData.mFight.mFaceDirection);

	gData.mFight.mCB = tFunc;
	gData.mFight.mDisplayNow = 0;
	gData.mFight.mHasPlayedSound = 0;
	gData.mFight.mIsDisplayingFight = 1;
}

void playDreamKOAnimation(void(*tFunc)())
{
	playDisplayAnimation(&gData.mKO.mAnimationID, gData.mKO.mAnimation, &gData.mKO.mPosition, gData.mKO.mFaceDirection);

	gData.mKO.mDisplayNow = 0;
	gData.mKO.mHasPlayedSound = 0;
	gData.mKO.mCB = tFunc;
	gData.mKO.mIsDisplaying = 1;
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
	parseWinText(gData.mWin.mDisplayedText, gData.mWin.mText, tName, &gData.mWin.mDisplayPosition, gData.mWin.mPosition);
	playDisplayText(&gData.mWin.mTextID, gData.mWin.mDisplayedText, gData.mWin.mDisplayPosition, gData.mWin.mFont);
	
	gData.mWin.mNow = 0;
	gData.mWin.mCB = tFunc;
	gData.mWin.mIsDisplaying = 1;
}

void playDreamContinueAnimation(void(*tAnimationFinishedFunc)(), void(*tContinuePressedFunc)())
{

	gData.mContinue.mFinishedCB = tAnimationFinishedFunc;
	gData.mContinue.mPressedContinueCB = tContinuePressedFunc;
	gData.mContinue.mValue = 10;
	gData.mContinue.mNow = 0;
	gData.mContinue.mDuration = 60;

	playDisplayText(&gData.mContinue.mContinueTextID, "Continue?", makePosition(160, 100, UI_BASE_Z), makeVector3DI(3, 0, 0));
	playDisplayText(&gData.mContinue.mValueTextID, "10", makePosition(160, 120, UI_BASE_Z), makeVector3DI(2, 0, 0));


	gData.mContinue.mIsActive = 1;
}

void setDreamTimeDisplayFinishedCB(void(*tTimeDisplayFinishedFunc)())
{
	gData.mTime.mFinishedCB = tTimeDisplayFinishedFunc;
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
		setSingleUIComponentInvisibleForOneFrame(gData.mHealthBars[i].mBG0AnimationID);
		setSingleUIComponentInvisibleForOneFrame(gData.mHealthBars[i].mBG1AnimationID);
		setSingleUIComponentInvisibleForOneFrame(gData.mHealthBars[i].mMidAnimationID);
		setSingleUIComponentInvisibleForOneFrame(gData.mHealthBars[i].mFrontAnimationID);

		setSingleUIComponentInvisibleForOneFrame(gData.mPowerBars[i].mBG0AnimationID);
		setSingleUIComponentInvisibleForOneFrame(gData.mPowerBars[i].mBG1AnimationID);
		setSingleUIComponentInvisibleForOneFrame(gData.mPowerBars[i].mMidAnimationID);
		setSingleUIComponentInvisibleForOneFrame(gData.mPowerBars[i].mFrontAnimationID);
		setSingleUITextInvisibleForOneFrame(gData.mPowerBars[i].mCounterTextID);

		setSingleUIComponentInvisibleForOneFrame(gData.mFaces[i].mBGAnimationID);
		setSingleUIComponentInvisibleForOneFrame(gData.mFaces[i].mBG0AnimationID);
		setSingleUIComponentInvisibleForOneFrame(gData.mFaces[i].mBG1AnimationID);
		setSingleUIComponentInvisibleForOneFrame(gData.mFaces[i].mFaceAnimationID);

		setSingleUIComponentInvisibleForOneFrame(gData.mDisplayName[i].mBGAnimationID);
		setSingleUITextInvisibleForOneFrame(gData.mDisplayName[i].mTextID);
	}

	setSingleUIComponentInvisibleForOneFrame(gData.mTime.mBGAnimationID);
	setSingleUITextInvisibleForOneFrame(gData.mTime.mTextID);
}

void setDreamNoMusicFlag()
{
	// TODO: BGM
}

void setTimerFreezeFlag()
{
	gData.mTime.mTimerFreezeFlag = 1;
}

void setTimerInfinite()
{
	gData.mTime.mIsInfinite = 1;
}

void setTimerFinite()
{
	gData.mTime.mIsInfinite = 0;
}

int isTimerFinished()
{
	return gData.mTime.mIsActive && !gData.mTime.mIsInfinite && gData.mTime.mIsFinished;
}

void setEnvironmentColor(Vector3DI tColors, int tTime, int tIsUnderCharacters)
{
	setAnimationPosition(gData.mEnvironmentEffects.mAnimationID, makePosition(0, 0, tIsUnderCharacters ? ENVIRONMENT_COLOR_LOWER_Z : ENVIRONMENT_COLOR_UPPER_Z));
	setAnimationSize(gData.mEnvironmentEffects.mAnimationID, makePosition(640, 480, 1), makePosition(0, 0, 0));
	setAnimationColor(gData.mEnvironmentEffects.mAnimationID, tColors.x / 255.0, tColors.y / 255.0, tColors.z / 255.0);

	gData.mEnvironmentEffects.mDuration = tTime;
	gData.mEnvironmentEffects.mNow = 0;
	gData.mEnvironmentEffects.mIsActive = 1;

}

// TODO: use environment shake
void setEnvironmentShake(int tDuration, double tFrequency, int tAmplitude, double tPhaseOffset)
{
	gData.mEnvironmentShake.mFrequency = tFrequency;
	gData.mEnvironmentShake.mAmplitude = tAmplitude;
	gData.mEnvironmentShake.mPhaseOffset = tPhaseOffset;

	gData.mEnvironmentShake.mNow = 0;
	gData.mEnvironmentShake.mDuration = tDuration;
	gData.mEnvironmentShake.mIsActive = 1;
}

static void addGeneralWinIcon(int tPlayer, Vector3DI tSprite, int tIsPerfect) {
	WinIcon* icon = &gData.mWinIcons[tPlayer];

	if (icon->mIconAmount >= icon->mIconUpToAmount) return;

	int id = icon->mIconAmount;
	Position pos = vecAdd2D(icon->mPosition, vecScale(icon->mOffset, id));	
	icon->mIconAnimations[id] = createOneFrameMugenAnimationForSprite(tSprite.x, tSprite.y);
	icon->mIconAnimationIDs[id] = addMugenAnimation(icon->mIconAnimations[id], &gData.mFightSprites, pos);

	icon->mHasIsPerfectIcon[id] = tIsPerfect;
	if (icon->mHasIsPerfectIcon[id]) {
		pos.z++;
		icon->mPerfectIconAnimations[id] = createOneFrameMugenAnimationForSprite(icon->mPerfectWin.x, icon->mPerfectWin.y);
		icon->mPerfectIconAnimationIDs[id] = addMugenAnimation(icon->mPerfectIconAnimations[id], &gData.mFightSprites, pos);
	}

	icon->mIconAmount++;
}

void addNormalWinIcon(int tPlayer, int tIsPerfect)
{
	WinIcon* icon = &gData.mWinIcons[tPlayer];
	addGeneralWinIcon(tPlayer, icon->mNormalWin, tIsPerfect);
}

void addSpecialWinIcon(int tPlayer, int tIsPerfect) {
	WinIcon* icon = &gData.mWinIcons[tPlayer];
	addGeneralWinIcon(tPlayer, icon->mSpecialWin, tIsPerfect);
}

void addHyperWinIcon(int tPlayer, int tIsPerfect) {
	WinIcon* icon = &gData.mWinIcons[tPlayer];
	addGeneralWinIcon(tPlayer, icon->mHyperWin, tIsPerfect);
}

void addThrowWinIcon(int tPlayer, int tIsPerfect) {
	WinIcon* icon = &gData.mWinIcons[tPlayer];
	addGeneralWinIcon(tPlayer, icon->mThrowWin, tIsPerfect);
}

void addCheeseWinIcon(int tPlayer, int tIsPerfect) {
	WinIcon* icon = &gData.mWinIcons[tPlayer];
	addGeneralWinIcon(tPlayer, icon->mCheeseWin, tIsPerfect);
}

void addTimeoverWinIcon(int tPlayer, int tIsPerfect) {
	WinIcon* icon = &gData.mWinIcons[tPlayer];
	addGeneralWinIcon(tPlayer, icon->mTimeOverWin, tIsPerfect);
}

void addSuicideWinIcon(int tPlayer, int tIsPerfect) {
	WinIcon* icon = &gData.mWinIcons[tPlayer];
	addGeneralWinIcon(tPlayer, icon->mSuicideWin, tIsPerfect);
}

void addTeammateWinIcon(int tPlayer, int tIsPerfect) {
	WinIcon* icon = &gData.mWinIcons[tPlayer];
	addGeneralWinIcon(tPlayer, icon->mTeammateWin, tIsPerfect);
}

static void removeSinglePlayerWinIcons(int tPlayer) {
	WinIcon* icon = &gData.mWinIcons[tPlayer];
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

void setUIFaces() {
	setMugenAnimationSprites(gData.mFaces[0].mFaceAnimationID, getPlayerSprites(getRootPlayer(0)));
	setMugenAnimationSprites(gData.mFaces[1].mFaceAnimationID, getPlayerSprites(getRootPlayer(1)));
}