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
#include <prism/system.h>

#include "stage.h"
#include "playerdefinition.h"
#include "mugenstagehandler.h"
#include "mugenanimationutilities.h"
#include "config.h"
#include "gamelogic.h"
#include "osuhandler.h"

using namespace std;

#define ENVIRONMENT_COLOR_LOWER_Z 34
#define HITSPARK_BASE_Z 51
#define ENVIRONMENT_COLOR_UPPER_Z 62
#define UI_BASE_Z 72

typedef struct {
	double mScale;
	int mCoordinateP;
} FightFX;

typedef struct {
	Position mBG0Position;
	int mOwnsBG0Animation;
	MugenAnimation* mBG0Animation;
	MugenAnimationHandlerElement* mBG0AnimationElement;

	Position mBG1Position;
	int mOwnsBG1Animation;
	MugenAnimation* mBG1Animation;
	MugenAnimationHandlerElement* mBG1AnimationElement;

	Position mMidPosition;
	int mOwnsMidAnimation;
	MugenAnimation* mMidAnimation;
	MugenAnimationHandlerElement* mMidAnimationElement;

	Position mFrontPosition;
	int mOwnsFrontAnimation;
	MugenAnimation* mFrontAnimation;
	MugenAnimationHandlerElement* mFrontAnimationElement;

	Vector2D mHealthRangeX;

	double mPercentage;
	double mDisplayedPercentage;

	int mIsPaused;
	Duration mPauseNow;

} HealthBar;

typedef struct {
	Position mBG0Position;
	int mOwnsBG0Animation;
	MugenAnimation* mBG0Animation;
	MugenAnimationHandlerElement* mBG0AnimationElement;

	Position mBG1Position;
	int mOwnsBG1Animation;
	MugenAnimation* mBG1Animation;
	MugenAnimationHandlerElement* mBG1AnimationElement;

	Position mMidPosition;
	int mOwnsMidAnimation;
	MugenAnimation* mMidAnimation;
	MugenAnimationHandlerElement* mMidAnimationElement;

	Position mFrontPosition;
	int mOwnsFrontAnimation;
	MugenAnimation* mFrontAnimation;
	MugenAnimationHandlerElement* mFrontAnimationElement;

	Vector2D mPowerRangeX;

	int mLevel;

	int mCounterTextID;
	Position mCounterPosition;
	Vector3DI mCounterFont;
	char mCounterText[10];

	int mHasLevelSound[4];
	Vector2DI mLevelSounds[4];
} PowerBar;

typedef struct {
	Position mBGPosition;
	int mOwnsBGAnimation;
	MugenAnimation* mBGAnimation;
	MugenAnimationHandlerElement* mBGAnimationElement;

	Position mBG0Position;
	int mOwnsBG0Animation;
	MugenAnimation* mBG0Animation;
	MugenAnimationHandlerElement* mBG0AnimationElement;

	Position mBG1Position;
	int mOwnsBG1Animation;
	MugenAnimation* mBG1Animation;
	MugenAnimationHandlerElement* mBG1AnimationElement;

	Position mFacePosition;
	int mOwnsFaceAnimation;
	MugenAnimation* mFaceAnimation;
	MugenAnimationHandlerElement* mFaceAnimationElement;
} Face;

typedef struct {
	Position mBGPosition;
	int mOwnsBGAnimation;
	MugenAnimation* mBGAnimation;
	MugenAnimationHandlerElement* mBGAnimationElement;

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
	MugenAnimationHandlerElement* mBGAnimationElement;

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
	double mCurrentDeltaY;

	int mDisplayNow;
	int mDisplayTime;

	int mShakeNow;
} Combo;

typedef struct {
	int mTime;

	Position mPosition;
	int mOwnsDefaultAnimation;
	MugenAnimation* mDefaultAnimation;
	int mDefaultFaceDirection;
	Vector2D mDefaultScale;
	int mHasDefaultAnimation;
	Vector3DI mFont;

	int mHasCustomRoundAnimation[10];
	int mOwnsCustomRoundAnimations[10];
	MugenAnimation* mCustomRoundAnimations[10];
	Position mCustomRoundPositions[10];
	int mCustomRoundFaceDirection[10];
	Vector2D mCustomRoundScale[10];

	int mHasRoundSound[10];
	Vector2DI mRoundSounds[10];
	int mSoundTime;

	char mText[1024];
	char mDisplayedText[1024];
	int mDisplayTime;

	int mIsDisplayingRound;
	int mDisplayNow;
	int mHasPlayedSound;
	int mRoundIndex;

	int mHasActiveAnimation;
	MugenAnimationHandlerElement* mAnimationElement;
	int mTextID;


	void(*mCB)();
} Round;

typedef struct {
	int mTime;

	Position mPosition;
	int mOwnsAnimation;
	MugenAnimation* mAnimation;
	int mFaceDirection;
	Vector2D mScale;

	int mDisplayNow;
	int mIsDisplayingFight;

	MugenAnimationHandlerElement* mAnimationElement;

	int mHasPlayedSound;
	int mSoundTime;
	int mHasSound;
	Vector2DI mSound;

	void(*mCB)();

} FightDisplay;

typedef struct {
	int mTime;

	Position mPosition;
	int mOwnsAnimation;
	MugenAnimation* mAnimation;
	int mFaceDirection;
	Vector2D mScale;

	int mDisplayNow;
	int mIsDisplaying;

	int mHasPlayedSound;
	int mSoundTime;
	int mHasSound;
	Vector2DI mSound;

	MugenAnimationHandlerElement* mAnimationElement;

	void(*mCB)();
} KO;

typedef struct {
	int mTextID;

	Position mPosition;
	char mText[1024];
	int mNow;
	int mDisplayTime;

	Vector3DI mFont;

	int mHasPlayedSound;
	int mHasSound;
	Vector2DI mSound;

	int mIsDisplaying;
	void(*mCB)();
} DKO;

typedef struct {
	int mTextID;

	Position mPosition;
	char mText[1024];
	int mNow;
	int mDisplayTime;

	Vector3DI mFont;

	int mHasPlayedSound;
	int mHasSound;
	Vector2DI mSound;

	int mIsDisplaying;
	void(*mCB)();
} TO;

typedef struct {
	int mTime;

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
	int mTextID;

	Position mPosition;
	char mText[1024];
	int mNow;
	int mDisplayTime;

	Vector3DI mFont;

	int mIsDisplaying;
	void(*mCB)();
} DrawDisplay;

typedef struct {
	int mIsCountingDownControl;
	int mControlReturnTime;
	int mNow;
} ControlCountdown;

typedef struct {
	int mIsEnabled;
	int mIsActive;

	Position2D mContinueTextPosition;
	std::string mContinueText;
	Vector3DI mContinueFont;

	Position2D mCounterTextPosition;
	Vector3DI mCounterFont;
	int mCounterStartValue;
	int mCounterDuration;

	int mContinueTextID;
	int mValueTextID;

	int mValue;
	int mNow;
	int mDuration;

	void(*mFinishedCB)();
	void(*mPressedContinueCB)();

} Continue;

typedef struct {
	Vector2DI mSprite;
	Vector2D mScale;
} WinIconType;

typedef struct {
	Position mPosition;
	Position mOffset;

	Position mCounterOffset;
	Vector3DI mCounterFont;

	WinIconType mNormalWin;
	WinIconType mSpecialWin;
	WinIconType mHyperWin;
	WinIconType mThrowWin;
	WinIconType mCheeseWin;
	WinIconType mTimeOverWin;
	WinIconType mSuicideWin;
	WinIconType mTeammateWin;
	WinIconType mPerfectWin;

	int mIconUpToAmount;

	int mIconAmount;
	MugenAnimation** mIconAnimations;
	MugenAnimationHandlerElement** mIconAnimationElements;
	int* mHasIsPerfectIcon;
	MugenAnimation** mPerfectIconAnimations;
	MugenAnimationHandlerElement** mPerfectIconAnimationElements;
} WinIcon;

typedef struct {
	TextureData mWhiteTexture;
	AnimationHandlerElement* mAnimationElement;

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
	MugenAnimationHandlerElement* mAnimationElement;
} HitSpark;

typedef struct {
	int mTime;
} Slowdown;

typedef struct {
	int mWaitTime;
} Start;

typedef struct {
	int mWaitTime;
	int mHitTime;
	int mWinTime;
	int mTime;
} Over;

static struct {
	MugenSpriteFile mFightSprites;
	MugenAnimations mFightAnimations;
	MugenSounds mFightSounds;

	MugenSounds mCommonSounds;

	MugenSpriteFile mFightFXSprites;
	MugenAnimations mFightFXAnimations;

	FightFX mFightFX;
	HealthBar mHealthBars[2];
	PowerBar mPowerBars[2];
	Face mFaces[2];
	DisplayName mDisplayName[2];
	TimeCounter mTime;
	Combo mCombos[2];
	Round mRound;
	FightDisplay mFight;
	KO mKO;
	DKO mDKO;
	TO mTO;
	WinDisplay mWin;
	DrawDisplay mDraw;
	Continue mContinue;
	WinIcon mWinIcons[2];

	ControlCountdown mControl;
	Slowdown mSlow;
	Start mStart;
	Over mOver;

	EnvironmentColorEffect mEnvironmentEffects;
	EnvironmentShakeEffect mEnvironmentShake;

	list<HitSpark> mHitSparks;

	std::string mCustomFightMotifPath;
} gFightUIData;

static void loadFightDefScript(MugenDefScript* tScript, char* tDefPath) {
	strcpy(tDefPath, (getDolmexicaAssetFolder() + "data/480p/fight.def").c_str());
	if (isFile(tDefPath)) {
		gFightUIData.mFightFX.mCoordinateP = 640;
	}
	else {
		const auto motifPath = getDolmexicaAssetFolder() + getMotifPath();
		std::string folder;
		getPathToFile(folder, motifPath.c_str());
		std::string fightPath;
		if (!gFightUIData.mCustomFightMotifPath.empty()) {
			fightPath = gFightUIData.mCustomFightMotifPath;
			fightPath = findMugenSystemOrFightFilePath(fightPath, folder);
			gFightUIData.mCustomFightMotifPath = "";
		}
		else
		{
			MugenDefScript script;
			loadMugenDefScript(&script, motifPath);
			fightPath = getSTLMugenDefStringVariable(&script, "files", "fight");
			fightPath = findMugenSystemOrFightFilePath(fightPath, folder);
			unloadMugenDefScript(&script);
		}

		strcpy(tDefPath, fightPath.c_str());
		gFightUIData.mFightFX.mCoordinateP = 320;
	}

	loadMugenDefScript(tScript, tDefPath);
}

static void loadFightDefFilesFromScript(MugenDefScript* tScript, char* tDefPath) {
	std::string folder;
	getPathToFile(folder, tDefPath);

	auto file = getSTLMugenDefStringOrDefault(tScript, "files", "sff", "NO_FILE");
	file = findMugenSystemOrFightFilePath(file, folder);
	setMugenSpriteFileReaderToUsePalette(3);
	gFightUIData.mFightSprites = loadMugenSpriteFileWithoutPalette(file);
	setMugenSpriteFileReaderToNotUsePalette();

	gFightUIData.mFightAnimations = loadMugenAnimationFile(tDefPath);

	file = getSTLMugenDefStringOrDefault(tScript, "files", "fightfx.sff", "NO_FILE");
	file = findMugenSystemOrFightFilePath(file, folder);
	gFightUIData.mFightFXSprites = loadMugenSpriteFileWithoutPalette(file);

	file = getSTLMugenDefStringOrDefault(tScript, "files", "fightfx.air", "NO_FILE");
	file = findMugenSystemOrFightFilePath(file, folder);
	gFightUIData.mFightFXAnimations = loadMugenAnimationFile(file);

	file = getSTLMugenDefStringOrDefault(tScript, "files", "snd", "NO_FILE");
	file = findMugenSystemOrFightFilePath(file, folder);
	gFightUIData.mFightSounds = loadMugenSoundFile(file.c_str());

	file = getSTLMugenDefStringOrDefault(tScript, "files", "common.snd", "NO_FILE");
	file = findMugenSystemOrFightFilePath(file, folder);
	gFightUIData.mCommonSounds = loadMugenSoundFile(file.c_str());
}

static void loadFightFX(MugenDefScript* tScript) {
	gFightUIData.mFightFX.mScale = getMugenDefFloatOrDefault(tScript, "fightfx", "scale", 1.0);
}

static int loadSingleUIComponentWithFullComponentNameForStorageAndReturnIfLegit(MugenDefScript* tScript, MugenAnimations* tAnimations, const Position& tBasePosition, const char* tGroupName, const char* tComponentName, double tZ, MugenAnimation** oAnimation, int* oOwnsAnimation, Position* oPosition, int* oFaceDirection, Vector2D* oScale, double tCoordinateScale) {
	char name[1024];

	int animation;
	sprintf(name, "%s.anim", tComponentName);
	animation = getMugenDefIntegerOrDefault(tScript, tGroupName, name, -1);

	if (animation == -1) {
		Vector2DI sprite;
		sprintf(name, "%s.spr", tComponentName);
		sprite = getMugenDefVector2DIOrDefault(tScript, tGroupName, name, Vector2DI(-1, -1));

		*oAnimation = createOneFrameMugenAnimationForSprite(sprite.x, sprite.y);
		*oOwnsAnimation = 1;
	}
	else {
		*oAnimation = getMugenAnimation(tAnimations, animation);
		*oOwnsAnimation = 0;
	}

	sprintf(name, "%s.offset", tComponentName);
	*oPosition = getMugenDefVectorOrDefault(tScript, tGroupName, name, Vector3D(0, 0, 0));
	*oPosition *= tCoordinateScale;
	oPosition->z = tZ;
	*oPosition = vecAdd(*oPosition, tBasePosition);

	sprintf(name, "%s.facing", tComponentName);
	*oFaceDirection = getMugenDefIntegerOrDefault(tScript, tGroupName, name, 1);

	sprintf(name, "%s.scale", tComponentName);
	*oScale = getMugenDefVector2DOrDefault(tScript, tGroupName, name, Vector2D(1.0, 1.0));

	return 1;
}

static void loadSingleUIComponentWithFullComponentName(MugenDefScript* tScript, MugenSpriteFile* tSprites, MugenAnimations* tAnimations, const Position& tBasePosition, const char* tGroupName, const char* tComponentName, double tZ, MugenAnimation** oAnimation, int* oOwnsAnimation, MugenAnimationHandlerElement** oAnimationElement, Position* oPosition, double tCoordinateScale, double tAdditionalDrawScale) {
	int faceDirection;
	Vector2D scale;
	if (!loadSingleUIComponentWithFullComponentNameForStorageAndReturnIfLegit(tScript, tAnimations, tBasePosition, tGroupName, tComponentName, tZ, oAnimation, oOwnsAnimation, oPosition, &faceDirection, &scale, tCoordinateScale)) {
		*oAnimationElement = NULL;
		return;
	}

	*oAnimationElement = addMugenAnimation(*oAnimation, tSprites, Vector3D(0,0,0));
	setMugenAnimationBasePosition(*oAnimationElement, oPosition);
	setMugenAnimationBaseDrawScale(*oAnimationElement, tCoordinateScale * gFightUIData.mFightFX.mScale * tAdditionalDrawScale);
	setMugenAnimationDrawScale(*oAnimationElement, scale);

	if (faceDirection == -1) {
		setMugenAnimationFaceDirection(*oAnimationElement, 0);
	}

}

static void loadSingleUIComponent(int i, MugenDefScript* tScript, MugenSpriteFile* tSprites, MugenAnimations* tAnimations, const Position& tBasePosition, const char* tGroupName, const char* tComponentName, double tZ, MugenAnimation** oAnimation, int* oOwnsAnimation, MugenAnimationHandlerElement** oAnimationElement, Position* oPosition, double tCoordinateScale, double tAdditionalDrawScale) {
	char name[1024];

	sprintf(name, "p%d.%s", i + 1, tComponentName);
	loadSingleUIComponentWithFullComponentName(tScript, tSprites, tAnimations, tBasePosition, tGroupName, name, tZ, oAnimation, oOwnsAnimation, oAnimationElement, oPosition, tCoordinateScale, tAdditionalDrawScale);
}

static void loadSingleUITextWithFullComponentNameForStorage(MugenDefScript* tScript, const Position& tBasePosition, const char* tGroupName, const char* tComponentName, double tZ, Position* oPosition, int tIsReadingText, char* tText, Vector3DI* oFontData, double tCoordinateScale) {
	char name[1024];

	sprintf(name, "%s.offset", tComponentName);
	*oPosition = getMugenDefVectorOrDefault(tScript, tGroupName, name, Vector3D(0, 0, 0));
	*oPosition *= tCoordinateScale;
	oPosition->z = tZ;
	*oPosition = vecAdd(*oPosition, tBasePosition);

	if (tIsReadingText) {
		sprintf(name, "%s.text", tComponentName);
		if (isMugenDefStringVariable(tScript, tGroupName, name)) {
			char* text = getAllocatedMugenDefStringVariable(tScript, tGroupName, name);
			strcpy(tText, text);
			freeMemory(text);
		}
		else {
			tText[0] = '\0';
		}
	}

	sprintf(name, "%s.font", tComponentName);
	*oFontData = getMugenDefVectorIOrDefault(tScript, tGroupName, name, Vector3DI(-1, 0, 0));
}


static void loadSingleUITextWithFullComponentName(MugenDefScript* tScript, const Position& tBasePosition, const char* tGroupName, const char* tComponentName, double tZ, int* oTextID, Position* oPosition, int tIsReadingText, char* tText, Vector3DI* oFontData, double tCoordinateScale) {
	loadSingleUITextWithFullComponentNameForStorage(tScript, tBasePosition, tGroupName, tComponentName, tZ, oPosition, tIsReadingText, tText, oFontData, tCoordinateScale);

	*oTextID = addMugenText(tText, *oPosition, oFontData->x);
	setMugenTextColor(*oTextID, getMugenTextColorFromMugenTextColorIndex(oFontData->y));
	setMugenTextAlignment(*oTextID, getMugenTextAlignmentFromMugenAlignmentIndex(oFontData->z));
	setMugenTextScale(*oTextID, tCoordinateScale * gFightUIData.mFightFX.mScale);
}


static void loadSingleUIText(int i, MugenDefScript* tScript, const Position& tBasePosition, const char* tGroupName, const char* tComponentName, double tZ, int* oTextID, Position* oPosition, int tIsReadingText, char* tText, Vector3DI* oFontData, double tCoordinateScale) {
	char name[1024];

	sprintf(name, "p%d.%s", i + 1, tComponentName);
	loadSingleUITextWithFullComponentName(tScript, tBasePosition, tGroupName, name, tZ, oTextID, oPosition, tIsReadingText, tText, oFontData, tCoordinateScale);
}

static void loadSingleHealthBar(int i, MugenDefScript* tScript) {
	char name[1024];

	HealthBar* bar = &gFightUIData.mHealthBars[i];

	const auto coordinateScale = getScreenSize().x / double(getDreamUICoordinateP());
	Position basePosition;
	sprintf(name, "p%d.pos", i + 1);
	basePosition = getMugenDefVectorOrDefault(tScript, "lifebar", name, Vector3D(0,0,0));
	basePosition *= coordinateScale;
	basePosition.z = UI_BASE_Z;

	loadSingleUIComponent(i, tScript, &gFightUIData.mFightSprites, &gFightUIData.mFightAnimations, basePosition, "lifebar", "bg0", 1, &bar->mBG0Animation, &bar->mOwnsBG0Animation, &bar->mBG0AnimationElement, &bar->mBG0Position, coordinateScale, 1.0);
	loadSingleUIComponent(i, tScript, &gFightUIData.mFightSprites, &gFightUIData.mFightAnimations, basePosition, "lifebar", "bg1", 2, &bar->mBG1Animation, &bar->mOwnsBG1Animation, &bar->mBG1AnimationElement, &bar->mBG1Position, coordinateScale, 1.0);
	loadSingleUIComponent(i, tScript, &gFightUIData.mFightSprites, &gFightUIData.mFightAnimations, basePosition, "lifebar", "mid", 3, &bar->mMidAnimation, &bar->mOwnsMidAnimation,  &bar->mMidAnimationElement, &bar->mMidPosition, coordinateScale, 1.0);
	loadSingleUIComponent(i, tScript, &gFightUIData.mFightSprites, &gFightUIData.mFightAnimations, basePosition, "lifebar", "front", 4, &bar->mFrontAnimation, &bar->mOwnsFrontAnimation, &bar->mFrontAnimationElement, &bar->mFrontPosition, coordinateScale, 1.0);

	sprintf(name, "p%d.range.x", i + 1);
	bar->mHealthRangeX = getMugenDefVector2DOrDefault(tScript, "lifebar", name, Vector2D(0, 0));

	bar->mPercentage = 1;
	bar->mDisplayedPercentage = 1;
	bar->mIsPaused = 0;
}

static void loadSinglePowerBar(int i, MugenDefScript* tScript) {
	char name[1024];

	PowerBar* bar = &gFightUIData.mPowerBars[i];

	const auto coordinateScale = getScreenSize().x / double(getDreamUICoordinateP());
	Position basePosition;
	sprintf(name, "p%d.pos", i + 1);
	basePosition = getMugenDefVectorOrDefault(tScript, "powerbar", name, Vector3D(0, 0, 0));
	basePosition *= coordinateScale;
	basePosition.z = UI_BASE_Z;

	loadSingleUIComponent(i, tScript, &gFightUIData.mFightSprites, &gFightUIData.mFightAnimations, basePosition, "powerbar", "bg0", 1, &bar->mBG0Animation, &bar->mOwnsBG0Animation, &bar->mBG0AnimationElement, &bar->mBG0Position, coordinateScale, 1.0);
	loadSingleUIComponent(i, tScript, &gFightUIData.mFightSprites, &gFightUIData.mFightAnimations, basePosition, "powerbar", "bg1", 2, &bar->mBG1Animation, &bar->mOwnsBG1Animation, &bar->mBG1AnimationElement, &bar->mBG1Position, coordinateScale, 1.0);
	loadSingleUIComponent(i, tScript, &gFightUIData.mFightSprites, &gFightUIData.mFightAnimations, basePosition, "powerbar", "mid", 3, &bar->mMidAnimation, &bar->mOwnsMidAnimation, &bar->mMidAnimationElement, &bar->mMidPosition, coordinateScale, 1.0);
	loadSingleUIComponent(i, tScript, &gFightUIData.mFightSprites, &gFightUIData.mFightAnimations, basePosition, "powerbar", "front", 4, &bar->mFrontAnimation, &bar->mOwnsFrontAnimation, &bar->mFrontAnimationElement, &bar->mFrontPosition, coordinateScale, 1.0);
	
	strcpy(bar->mCounterText, "0");
	loadSingleUIText(i, tScript, basePosition, "powerbar", "counter", 5, &bar->mCounterTextID, &bar->mCounterPosition, 0, bar->mCounterText, &bar->mCounterFont, coordinateScale);

	sprintf(name, "p%d.range.x", i + 1);
	bar->mPowerRangeX = getMugenDefVector2DOrDefault(tScript, "powerbar", name, Vector2D(0, 0));

	int j;
	for (j = 0; j < 3; j++) {
		sprintf(name, "level%d.snd", j + 1);
		bar->mHasLevelSound[j] = isMugenDefVector2DIVariable(tScript, "powerbar", name);
		if (bar->mHasLevelSound[j]) {
			bar->mLevelSounds[j] = getMugenDefVector2DIOrDefault(tScript, "powerbar", name, Vector2DI(1, 0));
		}
	}

	bar->mLevel = 0;
}

static void loadSingleFace(int i, MugenDefScript* tScript) {
	char name[1024];

	Face* face = &gFightUIData.mFaces[i];

	const auto coordinateScale = getScreenSize().x / double(getDreamUICoordinateP());
	Position basePosition;
	sprintf(name, "p%d.pos", i + 1);
	basePosition = getMugenDefVectorOrDefault(tScript, "face", name, Vector3D(0, 0, 0));
	basePosition *= coordinateScale;
	basePosition.z = UI_BASE_Z;

	const auto playerScale = getDreamUICoordinateP() / double(getPlayerCoordinateP(getRootPlayer(i)));
	DreamPlayer* p = getRootPlayer(i);
	loadSingleUIComponent(i, tScript, &gFightUIData.mFightSprites, &gFightUIData.mFightAnimations, basePosition, "face", "bg", 1, &face->mBGAnimation, &face->mOwnsBGAnimation, &face->mBGAnimationElement, &face->mBGPosition, coordinateScale, 1.0);
	loadSingleUIComponent(i, tScript, &gFightUIData.mFightSprites, &gFightUIData.mFightAnimations, basePosition, "face", "bg0", 1, &face->mBG0Animation, &face->mOwnsBG0Animation, &face->mBG0AnimationElement, &face->mBG0Position, coordinateScale, 1.0);
	loadSingleUIComponent(i, tScript, &gFightUIData.mFightSprites, &gFightUIData.mFightAnimations, basePosition, "face", "bg1", 2, &face->mBG1Animation, &face->mOwnsBG1Animation, &face->mBG1AnimationElement, &face->mBG1Position, coordinateScale, 1.0);
	loadSingleUIComponent(i, tScript, NULL, getPlayerAnimations(p), basePosition, "face", "face", 3, &face->mFaceAnimation, &face->mOwnsFaceAnimation, &face->mFaceAnimationElement, &face->mFacePosition, coordinateScale, playerScale);

}

static void loadSingleName(int i, MugenDefScript* tScript) {
	char name[1024];

	DisplayName* displayName = &gFightUIData.mDisplayName[i];

	const auto coordinateScale = getScreenSize().x / double(getDreamUICoordinateP());
	Position basePosition;
	sprintf(name, "p%d.pos", i + 1);
	basePosition = getMugenDefVectorOrDefault(tScript, "name", name, Vector3D(0, 0, 0));
	basePosition *= coordinateScale;
	basePosition.z = UI_BASE_Z;

	loadSingleUIComponent(i, tScript, &gFightUIData.mFightSprites, &gFightUIData.mFightAnimations, basePosition, "name", "bg", 0, &displayName->mBGAnimation, &displayName->mOwnsBGAnimation, &displayName->mBGAnimationElement, &displayName->mBGPosition, coordinateScale, 1.0);

	char displayNameText[1024];
	sprintf(displayNameText, "%s", getPlayerDisplayName(getRootPlayer(i)));
	loadSingleUIText(i, tScript, basePosition, "name", "name", 10, &displayName->mTextID, &displayName->mTextPosition, 0, displayNameText, &displayName->mFont, coordinateScale);
}

static void loadSingleWinIconType(WinIconType* oType, int i, MugenDefScript* tScript, const char* tName, char* tBuffer) {
	sprintf(tBuffer, "p%d.%s.spr", i + 1, tName);
	oType->mSprite = getMugenDefVector2DIOrDefault(tScript, "winicon", tBuffer, Vector2DI(-1, -1));
	sprintf(tBuffer, "p%d.%s.scale", i + 1, tName);
	oType->mScale = getMugenDefVector2DOrDefault(tScript, "winicon", tBuffer, Vector2D(1.0, 1.0));
}

static void loadSingleWinIcon(int i, MugenDefScript* tScript) {
	char name[1024];

	WinIcon* winIcon = &gFightUIData.mWinIcons[i];

	sprintf(name, "p%d.pos", i + 1);
	winIcon->mPosition = getMugenDefVectorOrDefault(tScript, "winicon", name, Vector3D(0, 0, 0));
	winIcon->mPosition.z = UI_BASE_Z;

	sprintf(name, "p%d.iconoffset", i + 1);
	winIcon->mOffset = getMugenDefVectorOrDefault(tScript, "winicon", name, Vector3D(0, 0, 0));

	sprintf(name, "p%d.counter.offset", i + 1);
	winIcon->mCounterOffset = getMugenDefVectorOrDefault(tScript, "winicon", name, Vector3D(0, 0, 0));

	sprintf(name, "p%d.counter.font", i + 1);
	winIcon->mCounterFont = getMugenDefVectorIOrDefault(tScript, "winicon", name, Vector3DI(-1, 0, 0));

	loadSingleWinIconType(&winIcon->mNormalWin, i, tScript, "n", name);
	loadSingleWinIconType(&winIcon->mSpecialWin, i, tScript, "s", name);
	loadSingleWinIconType(&winIcon->mHyperWin, i, tScript, "h", name);
	loadSingleWinIconType(&winIcon->mThrowWin, i, tScript, "throw", name);
	loadSingleWinIconType(&winIcon->mCheeseWin, i, tScript, "c", name);
	loadSingleWinIconType(&winIcon->mTimeOverWin, i, tScript, "t", name);
	loadSingleWinIconType(&winIcon->mSuicideWin, i, tScript, "suicide", name);
	loadSingleWinIconType(&winIcon->mTeammateWin, i, tScript, "teammate", name);
	loadSingleWinIconType(&winIcon->mPerfectWin, i, tScript, "perfect", name);

	winIcon->mIconUpToAmount = getMugenDefIntegerOrDefault(tScript, "winicon", "useiconupto", 4);

	winIcon->mIconAnimationElements = (MugenAnimationHandlerElement**)allocMemory(sizeof(MugenAnimationHandlerElement*) * winIcon->mIconUpToAmount);
	winIcon->mIconAnimations = (MugenAnimation**)allocMemory(sizeof(MugenAnimation*) * winIcon->mIconUpToAmount);
	winIcon->mHasIsPerfectIcon = (int*)allocMemory(sizeof(int) * winIcon->mIconUpToAmount);
	winIcon->mPerfectIconAnimations = (MugenAnimation**)allocMemory(sizeof(MugenAnimation*) * winIcon->mIconUpToAmount);
	winIcon->mPerfectIconAnimationElements = (MugenAnimationHandlerElement**)allocMemory(sizeof(MugenAnimationHandlerElement*) * winIcon->mIconUpToAmount);
	winIcon->mIconAmount = 0;
}

static void setComboPosition(int i) {
	Combo* combo = &gFightUIData.mCombos[i];

	if (i == 0) {
		Position startPosition = Vector3D(combo->mCurrentX, combo->mPosition.y + combo->mCurrentDeltaY, combo->mPosition.z);
		setMugenTextPosition(combo->mNumberTextID, startPosition);
		startPosition.x += getMugenTextSizeX(combo->mNumberTextID);
		startPosition = vecAdd(startPosition, combo->mTextOffset);
		setMugenTextPosition(combo->mTextID, startPosition);
	}
	else {
		Position startPosition = Vector3D(combo->mCurrentX, combo->mPosition.y + combo->mCurrentDeltaY, combo->mPosition.z);
		setMugenTextPosition(combo->mTextID, startPosition);
		startPosition.x -= getMugenTextSizeX(combo->mTextID);
		startPosition = vecSub(startPosition, combo->mTextOffset);
		setMugenTextPosition(combo->mNumberTextID, startPosition);
	}
}

static void loadSingleCombo(int i, MugenDefScript* tScript) {
	char name[1024];

	Combo* combo = &gFightUIData.mCombos[i];

	const auto coordinateScale = getScreenSize().x / double(getDreamUICoordinateP());
	sprintf(name, "team%d.pos", i + 1);
	combo->mPosition = getMugenDefVectorOrDefault(tScript, "combo", name, Vector3D(0, 0, 0));
	combo->mPosition *= coordinateScale;
	combo->mPosition.z = UI_BASE_Z;

	sprintf(name, "team%d.start.x", i + 1);
	combo->mStartX = getMugenDefFloatOrDefault(tScript, "combo", name, 0);

	sprintf(name, "team%d.counter.font", i + 1);
	combo->mCounterFont = getMugenDefVectorIOrDefault(tScript, "combo", name, Vector3DI(-1, 0, 1));
	sprintf(name, "team%d.counter.shake", i + 1);
	combo->mCounterShake = getMugenDefIntegerOrDefault(tScript, "combo", name, 0);

	sprintf(name, "team%d.text.text", i + 1);
	getMugenDefStringOrDefault(combo->mText, tScript, "combo", name, "combo");
	sprintf(name, "team%d.text.font", i + 1);
	combo->mFont = getMugenDefVectorIOrDefault(tScript, "combo", name, Vector3DI(-1, 0, 1));
	sprintf(name, "team%d.text.offset", i + 1);
	combo->mTextOffset = getMugenDefVectorOrDefault(tScript, "combo", name, Vector3D(0, 0, 0));
	combo->mTextOffset.z = 0;

	sprintf(name, "team%d.displaytime", i + 1);
	combo->mDisplayTime = getMugenDefIntegerOrDefault(tScript, "combo", name, 0);

	const auto startPosition = Vector3D(combo->mPosition.x, combo->mPosition.y, combo->mPosition.z);
	combo->mNumberTextID = addMugenTextMugenStyle("111", startPosition, combo->mCounterFont);
	combo->mTextID = addMugenTextMugenStyle(combo->mText, startPosition, combo->mFont);
	
	if (i == 0) {
		combo->mCurrentX = combo->mStartX - getMugenTextSizeX(combo->mNumberTextID);
	}
	else {
		combo->mCurrentX = combo->mStartX + getMugenTextSizeX(combo->mTextID);
	}
	combo->mCurrentDeltaY = 0;
	setComboPosition(i);

	combo->mIsActive = 0;
}

static void setBarToPercentage(MugenAnimationHandlerElement* tAnimationElement, const Vector2D& tRange, double tPercentage);

static void setDreamLifeBarPercentageStart(DreamPlayer* tPlayer, double tPercentage) {
	int i = tPlayer->mRootID;
	setDreamLifeBarPercentage(tPlayer, tPercentage);
	gFightUIData.mHealthBars[i].mDisplayedPercentage = getPlayerLifePercentage(getRootPlayer(i));
	if (!gFightUIData.mHealthBars[i].mMidAnimationElement) return;
	setBarToPercentage(gFightUIData.mHealthBars[i].mMidAnimationElement, gFightUIData.mHealthBars[i].mHealthRangeX, gFightUIData.mHealthBars[i].mDisplayedPercentage);
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

static void playDisplayText(int* oTextID, const char* tText, const Position& tPosition, const Vector3DI& tFont, int tTime);

static void loadTimer(MugenDefScript* tScript) {
	const auto coordinateScale = getScreenSize().x / double(getDreamUICoordinateP());
	Position basePosition;
	basePosition = getMugenDefVectorOrDefault(tScript, "time", "pos", Vector3D(0, 0, 0));
	basePosition *= coordinateScale;
	basePosition.z = UI_BASE_Z;

	loadSingleUIComponentWithFullComponentName(tScript, &gFightUIData.mFightSprites, &gFightUIData.mFightAnimations, basePosition, "time", "bg", 0, &gFightUIData.mTime.mBGAnimation, &gFightUIData.mTime.mOwnsBGAnimation, &gFightUIData.mTime.mBGAnimationElement, &gFightUIData.mTime.mBGPosition, coordinateScale, 1.0);

	gFightUIData.mTime.mPosition = getMugenDefVectorOrDefault(tScript, "time", "counter.offset", Vector3D(0, 0, 0));
	gFightUIData.mTime.mPosition = vecAdd(gFightUIData.mTime.mPosition, basePosition);
	gFightUIData.mTime.mPosition.z+=10;

	gFightUIData.mTime.mFramesPerCount = getMugenDefIntegerOrDefault(tScript, "time", "framespercount", 60);

	gFightUIData.mTime.mIsActive = 0;

	gFightUIData.mTime.mFont = getMugenDefVectorIOrDefault(tScript, "time", "counter.font", Vector3DI(-1, 0, 0));

	playDisplayText(&gFightUIData.mTime.mTextID, "99", gFightUIData.mTime.mPosition, gFightUIData.mTime.mFont, 0);
	
	gFightUIData.mTime.mTimerFreezeFlag = 0;
	resetDreamTimer();
}

static void loadRound(MugenDefScript* tScript) {
	const auto coordinateScale = getScreenSize().x / double(getDreamUICoordinateP());
	Position basePosition;
	basePosition = getMugenDefVectorOrDefault(tScript, "round", "pos", Vector3D(0,0,0));
	basePosition *= coordinateScale;
	basePosition.z = UI_BASE_Z;

	gFightUIData.mRound.mTime = getMugenDefIntegerOrDefault(tScript, "round", "round.time", 0);
	gFightUIData.mRound.mDisplayTime = getMugenDefIntegerOrDefault(tScript, "round", "round.default.displaytime", 0);
	gFightUIData.mRound.mSoundTime = getMugenDefIntegerOrDefault(tScript, "round", "round.sndtime", 0);

	if (isMugenDefVariable(tScript, "round", "round.default.anim")) {
		gFightUIData.mRound.mHasDefaultAnimation = 1;
		loadSingleUIComponentWithFullComponentNameForStorageAndReturnIfLegit(tScript, &gFightUIData.mFightAnimations, basePosition, "round", "round.default", 1, &gFightUIData.mRound.mDefaultAnimation, &gFightUIData.mRound.mOwnsDefaultAnimation, &gFightUIData.mRound.mPosition, &gFightUIData.mRound.mDefaultFaceDirection, &gFightUIData.mRound.mDefaultScale, coordinateScale);
	}
	else {
		gFightUIData.mRound.mHasDefaultAnimation = 0;
		loadSingleUITextWithFullComponentNameForStorage(tScript, basePosition, "round", "round.default", 1, &gFightUIData.mRound.mPosition, 1, gFightUIData.mRound.mText, &gFightUIData.mRound.mFont, coordinateScale);
	}


	int i;
	for (i = 0; i < 9; i++) {
		char name[1024];
		sprintf(name, "round%d.anim", i + 1);
		if (!isMugenDefVariable(tScript, "round", name)) {
			gFightUIData.mRound.mHasCustomRoundAnimation[i] = 0;
		}
		else {
			gFightUIData.mRound.mHasCustomRoundAnimation[i] = 1;
			sprintf(name, "round%d", i + 1);
			loadSingleUIComponentWithFullComponentNameForStorageAndReturnIfLegit(tScript, &gFightUIData.mFightAnimations, basePosition, "round", name, 1, &gFightUIData.mRound.mCustomRoundAnimations[i], &gFightUIData.mRound.mOwnsCustomRoundAnimations[i], &gFightUIData.mRound.mCustomRoundPositions[i], &gFightUIData.mRound.mCustomRoundFaceDirection[i], &gFightUIData.mRound.mCustomRoundScale[i], coordinateScale);
		}

		sprintf(name, "round%d.snd", i + 1);
		if (!isMugenDefVariable(tScript, "round", name)) {
			gFightUIData.mRound.mHasRoundSound[i] = 0;
		}
		else {
			gFightUIData.mRound.mHasRoundSound[i] = 1;
			gFightUIData.mRound.mRoundSounds[i] = getMugenDefVector2DIVariable(tScript, "round", name);
		}
	}

	gFightUIData.mRound.mIsDisplayingRound = 0;
}

static void loadFight(MugenDefScript* tScript) {
	const auto coordinateScale = getScreenSize().x / double(getDreamUICoordinateP());
	Position basePosition;
	basePosition = getMugenDefVectorOrDefault(tScript, "round", "pos", Vector3D(0, 0, 0));
	basePosition *= coordinateScale;
	basePosition.z = UI_BASE_Z;

	loadSingleUIComponentWithFullComponentNameForStorageAndReturnIfLegit(tScript, &gFightUIData.mFightAnimations, basePosition, "round", "fight", 1, &gFightUIData.mFight.mAnimation, &gFightUIData.mFight.mOwnsAnimation, &gFightUIData.mFight.mPosition, &gFightUIData.mFight.mFaceDirection, &gFightUIData.mFight.mScale, coordinateScale);
	gFightUIData.mFight.mTime = getMugenDefIntegerOrDefault(tScript, "round", "fight.time", 0);
	gFightUIData.mFight.mSoundTime = getMugenDefIntegerOrDefault(tScript, "round", "fight.sndtime", 0);
	gFightUIData.mFight.mHasSound = isMugenDefVector2DIVariable(tScript, "round", "fight.snd");
	if (gFightUIData.mFight.mHasSound) {
		gFightUIData.mFight.mSound = getMugenDefVector2DIOrDefault(tScript, "round", "fight.snd", Vector2DI(1, 0));
	}

	gFightUIData.mFight.mIsDisplayingFight = 0;
}

static void loadKO(MugenDefScript* tScript) {
	const auto coordinateScale = getScreenSize().x / double(getDreamUICoordinateP());
	Position basePosition;
	basePosition = getMugenDefVectorOrDefault(tScript, "round", "pos", Vector3D(0, 0, 0));
	basePosition *= coordinateScale;
	basePosition.z = UI_BASE_Z;

	loadSingleUIComponentWithFullComponentNameForStorageAndReturnIfLegit(tScript, &gFightUIData.mFightAnimations, basePosition, "round", "ko", 1, &gFightUIData.mKO.mAnimation, &gFightUIData.mKO.mOwnsAnimation, &gFightUIData.mKO.mPosition, &gFightUIData.mKO.mFaceDirection, &gFightUIData.mKO.mScale, coordinateScale);
	gFightUIData.mKO.mTime = getMugenDefIntegerOrDefault(tScript, "round", "ko.time", 0);
	gFightUIData.mKO.mSoundTime = getMugenDefIntegerOrDefault(tScript, "round", "ko.sndtime", 0);
	gFightUIData.mKO.mHasSound = isMugenDefVector2DIVariable(tScript, "round", "ko.snd");
	if (gFightUIData.mKO.mHasSound) {
		gFightUIData.mKO.mSound = getMugenDefVector2DIOrDefault(tScript, "round", "ko.snd", Vector2DI(-1, 0));
	}
	gFightUIData.mKO.mIsDisplaying = 0;
}

static void loadDKO(MugenDefScript* tScript) {
	const auto coordinateScale = getScreenSize().x / double(getDreamUICoordinateP());
	Position basePosition;
	basePosition = getMugenDefVectorOrDefault(tScript, "round", "pos", Vector3D(0, 0, 0));
	basePosition *= coordinateScale;
	basePosition.z = UI_BASE_Z;

	loadSingleUITextWithFullComponentNameForStorage(tScript, basePosition, "round", "dko", 1, &gFightUIData.mDKO.mPosition, 1, gFightUIData.mDKO.mText, &gFightUIData.mDKO.mFont, coordinateScale);

	gFightUIData.mDKO.mDisplayTime = getMugenDefIntegerOrDefault(tScript, "round", "dko.displaytime", 0);

	gFightUIData.mDKO.mHasSound = isMugenDefVector2DIVariable(tScript, "round", "dko.snd");
	if (gFightUIData.mDKO.mHasSound) {
		gFightUIData.mDKO.mSound = getMugenDefVector2DIOrDefault(tScript, "round", "dko.snd", Vector2DI(1, 0));
	}

	gFightUIData.mDKO.mIsDisplaying = 0;
}

static void loadTO(MugenDefScript* tScript) {
	const auto coordinateScale = getScreenSize().x / double(getDreamUICoordinateP());
	Position basePosition;
	basePosition = getMugenDefVectorOrDefault(tScript, "round", "pos", Vector3D(0, 0, 0));
	basePosition *= coordinateScale;
	basePosition.z = UI_BASE_Z;

	loadSingleUITextWithFullComponentNameForStorage(tScript, basePosition, "round", "to", 1, &gFightUIData.mTO.mPosition, 1, gFightUIData.mTO.mText, &gFightUIData.mTO.mFont, coordinateScale);

	gFightUIData.mTO.mDisplayTime = getMugenDefIntegerOrDefault(tScript, "round", "to.displaytime", 0);

	gFightUIData.mTO.mHasSound = isMugenDefVector2DIVariable(tScript, "round", "to.snd");
	if (gFightUIData.mTO.mHasSound) {
		gFightUIData.mTO.mSound = getMugenDefVector2DIOrDefault(tScript, "round", "to.snd", Vector2DI(1, 0));
	}

	gFightUIData.mTO.mIsDisplaying = 0;
}

static void loadWinDisplay(MugenDefScript* tScript) {
	const auto coordinateScale = getScreenSize().x / double(getDreamUICoordinateP());
	Position basePosition;
	basePosition = getMugenDefVectorOrDefault(tScript, "round", "pos", Vector3D(0, 0, 0));
	basePosition *= coordinateScale;
	basePosition.z = UI_BASE_Z;

	loadSingleUITextWithFullComponentNameForStorage(tScript, basePosition, "round", "win", 1, &gFightUIData.mWin.mPosition, 1, gFightUIData.mWin.mText, &gFightUIData.mWin.mFont, coordinateScale);
	gFightUIData.mWin.mTime = getMugenDefIntegerOrDefault(tScript, "round", "win.time", 0);
	gFightUIData.mWin.mDisplayTime = getMugenDefIntegerOrDefault(tScript, "round", "win.displaytime", 0);

	gFightUIData.mWin.mIsDisplaying = 0;
}

static void loadDrawDisplay(MugenDefScript* tScript) {
	const auto coordinateScale = getScreenSize().x / double(getDreamUICoordinateP());
	Position basePosition;
	basePosition = getMugenDefVectorOrDefault(tScript, "round", "pos", Vector3D(0, 0, 0));
	basePosition *= coordinateScale;
	basePosition.z = UI_BASE_Z;

	loadSingleUITextWithFullComponentNameForStorage(tScript, basePosition, "round", "draw", 1, &gFightUIData.mDraw.mPosition, 1, gFightUIData.mDraw.mText, &gFightUIData.mDraw.mFont, coordinateScale);

	gFightUIData.mDraw.mDisplayTime = getMugenDefIntegerOrDefault(tScript, "round", "draw.displaytime", 0);

	gFightUIData.mDraw.mIsDisplaying = 0;
}

static void loadControl(MugenDefScript* tScript) {
	gFightUIData.mControl.mControlReturnTime = getMugenDefIntegerOrDefault(tScript, "round", "ctrl.time", 0);

	gFightUIData.mControl.mIsCountingDownControl = 0;
}

static void loadSlow(MugenDefScript* tScript) {
	gFightUIData.mSlow.mTime = getMugenDefIntegerOrDefault(tScript, "round", "slow.time", 0);
}

static void loadStart(MugenDefScript* tScript) {
	gFightUIData.mStart.mWaitTime = getMugenDefIntegerOrDefault(tScript, "round", "start.waittime", 0);
}

static void loadOver(MugenDefScript* tScript) {
	gFightUIData.mOver.mWaitTime = getMugenDefIntegerOrDefault(tScript, "round", "over.waittime", 0);
	gFightUIData.mOver.mHitTime = getMugenDefIntegerOrDefault(tScript, "round", "over.hittime", 0);
	gFightUIData.mOver.mWinTime = getMugenDefIntegerOrDefault(tScript, "round", "over.wintime", 0);
	gFightUIData.mOver.mTime = getMugenDefIntegerOrDefault(tScript, "round", "over.time", 0);
}

static void loadMatch(MugenDefScript* tScript) {
	const auto wins = getMugenDefIntegerOrDefault(tScript, "round", "match.wins", 2);
	if (!hasCustomRoundsToWinAmount()) {
		setRoundsToWin(wins);
	}
	// const auto maxDrawGames = getMugenDefIntegerOrDefault(tScript, "round", "match.maxdrawgames", 1); // not used in Dolmexica
}

static void loadHitSparks() {
	gFightUIData.mHitSparks.clear();
}

static void loadContinue(MugenDefScript* tScript) {
	gFightUIData.mContinue.mIsEnabled = getMugenDefIntegerOrDefault(tScript, "continue", "enabled", 1);
	gFightUIData.mContinue.mContinueTextPosition = getMugenDefVector2DOrDefault(tScript, "continue", "continue.pos", Vector2D(160, 100));
	gFightUIData.mContinue.mContinueText = getSTLMugenDefStringOrDefault(tScript, "continue", "continue.text", "Continue?");
	gFightUIData.mContinue.mContinueFont = getMugenDefVectorIOrDefault(tScript, "continue", "continue.font", Vector3DI(-1, 0, 0));
	gFightUIData.mContinue.mCounterTextPosition = getMugenDefVector2DOrDefault(tScript, "continue", "counter.pos", Vector2D(160, 120));
	gFightUIData.mContinue.mCounterFont = getMugenDefVectorIOrDefault(tScript, "continue", "counter.font", Vector3DI(-1, 0, 0));
	gFightUIData.mContinue.mCounterStartValue = getMugenDefIntegerOrDefault(tScript, "continue", "counter.value", 10);
	gFightUIData.mContinue.mCounterDuration = getMugenDefIntegerOrDefault(tScript, "continue", "counter.duration", 60);

	gFightUIData.mContinue.mIsActive = 0;
}

static void loadEnvironmentColorEffects() {
	gFightUIData.mEnvironmentEffects.mWhiteTexture = createWhiteTexture();
	gFightUIData.mEnvironmentEffects.mAnimationElement = playOneFrameAnimationLoop(Vector3D(0, 0, 0), &gFightUIData.mEnvironmentEffects.mWhiteTexture);
	setAnimationScale(gFightUIData.mEnvironmentEffects.mAnimationElement, Vector3D(0, 0, 0), Vector3D(0, 0, 0));

	gFightUIData.mEnvironmentEffects.mIsActive = 0;
}

static void loadEnvironmentShakeEffects() {
	gFightUIData.mEnvironmentShake.mIsActive = 0;
}

static void loadFightUI(void* tData) {
	(void)tData;
	setProfilingSectionMarkerCurrentFunction();

	char defPath[1024];
	MugenDefScript script; 
	loadFightDefScript(&script, defPath);
	loadFightDefFilesFromScript(&script, defPath);
	loadFightFX(&script);
	loadPlayerUIs(&script);
	loadTimer(&script);
	loadRound(&script);
	loadFight(&script);
	loadKO(&script);
	loadDKO(&script);
	loadTO(&script);
	loadWinDisplay(&script);
	loadDrawDisplay(&script);
	loadControl(&script);
	loadSlow(&script);
	loadStart(&script);
	loadOver(&script);
	loadMatch(&script);
	loadContinue(&script);
	if (getGameMode() == GAME_MODE_OSU) {
		loadOsuParametersFromScript(&script, defPath);
	}
	unloadMugenDefScript(&script);

	loadHitSparks();
	loadEnvironmentColorEffects();
	loadEnvironmentShakeEffects();
}

static void unloadSingleUIComponent(MugenAnimation* tAnimation, int tOwnsAnimation) {
	if (!tOwnsAnimation) return;
	destroyMugenAnimation(tAnimation);
}

static void unloadSingleOptionalUIComponent(MugenAnimation* tAnimation, int tOwnsAnimation, MugenAnimationHandlerElement* tAnimationElement) {
	if (tAnimationElement == NULL) return;
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

	unloadSingleOptionalUIComponent(bar->mBG0Animation, bar->mOwnsBG0Animation, bar->mBG0AnimationElement);
	unloadSingleOptionalUIComponent(bar->mBG1Animation, bar->mOwnsBG1Animation, bar->mBG1AnimationElement);
	unloadSingleOptionalUIComponent(bar->mMidAnimation, bar->mOwnsMidAnimation, bar->mMidAnimationElement);
	unloadSingleOptionalUIComponent(bar->mFrontAnimation, bar->mOwnsFrontAnimation, bar->mFrontAnimationElement);
}

static void unloadSinglePowerBar(int i) {
	PowerBar* bar = &gFightUIData.mPowerBars[i];

	unloadSingleOptionalUIComponent(bar->mBG0Animation, bar->mOwnsBG0Animation, bar->mBG0AnimationElement);
	unloadSingleOptionalUIComponent(bar->mBG1Animation, bar->mOwnsBG1Animation, bar->mBG1AnimationElement);
	unloadSingleOptionalUIComponent(bar->mMidAnimation, bar->mOwnsMidAnimation, bar->mMidAnimationElement);
	unloadSingleOptionalUIComponent(bar->mFrontAnimation, bar->mOwnsFrontAnimation, bar->mFrontAnimationElement);
}

static void unloadSingleFace(int i) {
	Face* face = &gFightUIData.mFaces[i];

	unloadSingleOptionalUIComponent(face->mBGAnimation, face->mOwnsBGAnimation, face->mBGAnimationElement);
	unloadSingleOptionalUIComponent(face->mBG0Animation, face->mOwnsBG0Animation, face->mBG0AnimationElement);
	unloadSingleOptionalUIComponent(face->mBG1Animation, face->mOwnsBG1Animation, face->mBG1AnimationElement);
	unloadSingleOptionalUIComponent(face->mFaceAnimation, face->mOwnsFaceAnimation, face->mFaceAnimationElement);
}

static void unloadSingleName(int i) {
	DisplayName* displayName = &gFightUIData.mDisplayName[i];

	unloadSingleOptionalUIComponent(displayName->mBGAnimation, displayName->mOwnsBGAnimation, displayName->mBGAnimationElement);
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

	freeMemory(winIcon->mIconAnimationElements);
	freeMemory(winIcon->mIconAnimations);
	freeMemory(winIcon->mHasIsPerfectIcon);
	freeMemory(winIcon->mPerfectIconAnimations);
	freeMemory(winIcon->mPerfectIconAnimationElements);
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
	unloadSingleOptionalUIComponent(gFightUIData.mTime.mBGAnimation, gFightUIData.mTime.mOwnsBGAnimation, gFightUIData.mTime.mBGAnimationElement);
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
	setProfilingSectionMarkerCurrentFunction();

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

	if (!getMugenAnimationRemainingAnimationTime(e->mAnimationElement)) {
		removeMugenAnimation(e->mAnimationElement);
		return 1;
	}

	return 0;
}

static void updateHitSparks() {
	stl_list_remove_predicate(gFightUIData.mHitSparks, updateSingleHitSpark);
}

static void setBarToPercentage(MugenAnimationHandlerElement* tAnimationElement, const Vector2D& tRange, double tPercentage) {
	double fullSize = fabs(tRange.y - tRange.x);
	int newSize = (int)(fullSize * tPercentage);
	const auto coordinateScale = getScreenSize().x / double(getDreamUICoordinateP());
	const auto scaleFactor = 1.0 / getMugenAnimationDrawScale(tAnimationElement).x;
	setMugenAnimationRectangleWidth(tAnimationElement, int(newSize * coordinateScale * scaleFactor));
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
	setBarToPercentage(bar->mMidAnimationElement, bar->mHealthRangeX, bar->mDisplayedPercentage);
}

static double calculateShake(int t, double tPhaseOffset, double tFrequency, int tAmplitude) {
	return sin(tPhaseOffset + t * (tFrequency / 360.0) * 2.0 * M_PI) * tAmplitude;
}

static void updateSingleComboShake(Combo* tCombo) {
	if (!tCombo->mCounterShake) return;

	static const auto COMBO_SHAKE_DURATION = 10;
	if (tCombo->mShakeNow < COMBO_SHAKE_DURATION) {
		static const auto COMBO_SHAKE_FREQUENCY = 60.0;
		static const auto COMBO_SHAKE_AMPLITUDE = -4;
		static const auto COMBO_SHAKE_PHASE_OFFSET = 0.0;
		tCombo->mCurrentDeltaY = calculateShake(tCombo->mShakeNow, COMBO_SHAKE_PHASE_OFFSET, COMBO_SHAKE_FREQUENCY, COMBO_SHAKE_AMPLITUDE);
		tCombo->mShakeNow++;
	}
	else {
		tCombo->mCurrentDeltaY = 0;
	}
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
	
	updateSingleComboShake(combo);
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

static void playDisplayAnimation(MugenAnimationHandlerElement** oAnimationElement, MugenAnimation* tAnimation, Position* tBasePosition, int tFaceDirection, const Vector2D& tScale, int tStartTime) {
	*oAnimationElement = addMugenAnimation(tAnimation, &gFightUIData.mFightSprites, Vector3D(0,0,0));
	setMugenAnimationBasePosition(*oAnimationElement, tBasePosition);

	if (tFaceDirection == -1) {
		setMugenAnimationFaceDirection(*oAnimationElement, 0);
	}
	setMugenAnimationDrawScale(*oAnimationElement, tScale);

	if (tStartTime > 0) {
		pauseMugenAnimation(*oAnimationElement);
		setMugenAnimationVisibility(*oAnimationElement, 0);
	}
}

static void removeDisplayedAnimation(MugenAnimationHandlerElement* tAnimationElement) {
	removeMugenAnimation(tAnimationElement);
}

static void playDisplayText(int* oTextID, const char* tText, const Position& tPosition, const Vector3DI& tFont, int tStartTime) {
	*oTextID = addMugenText(tText, tPosition, tFont.x);
	setMugenTextColor(*oTextID, getMugenTextColorFromMugenTextColorIndex(tFont.y));
	setMugenTextAlignment(*oTextID, getMugenTextAlignmentFromMugenAlignmentIndex(tFont.z));

	if (tStartTime > 0) {
		setMugenTextVisibility(*oTextID, 0);
	}
}

static void removeDisplayedText(int tTextID) {
	removeMugenText(tTextID);
}

static void updateRoundSound() {
	int round = gFightUIData.mRound.mRoundIndex;
	if (gFightUIData.mRound.mDisplayNow >= (gFightUIData.mRound.mTime + gFightUIData.mRound.mSoundTime) && gFightUIData.mRound.mHasRoundSound[round] && !gFightUIData.mRound.mHasPlayedSound) {
		tryPlayMugenSoundAdvanced(&gFightUIData.mFightSounds, gFightUIData.mRound.mRoundSounds[round].x, gFightUIData.mRound.mRoundSounds[round].y, parseGameMidiVolumeToPrism(getGameMidiVolume()));
		gFightUIData.mRound.mHasPlayedSound = 1;
	}
}

static void removeRoundDisplay() {
	if (gFightUIData.mRound.mHasActiveAnimation) {
		removeDisplayedAnimation(gFightUIData.mRound.mAnimationElement);
	}
	else {
		removeDisplayedText(gFightUIData.mRound.mTextID);
	}
}

static void updateAnimationStart(MugenAnimationHandlerElement* e, int tNow, int tTime) {
	if (tNow < tTime) return;
	setMugenAnimationVisibility(e, 1);
	unpauseMugenAnimation(e);
}

static void updateTextStart(int tID, int tNow, int tTime) {
	if (tNow < tTime) return;
	setMugenTextVisibility(tID, 1);
}

static void updateRoundStart() {
	if (gFightUIData.mRound.mHasActiveAnimation) {
		updateAnimationStart(gFightUIData.mRound.mAnimationElement, gFightUIData.mRound.mDisplayNow, gFightUIData.mRound.mTime);
	}
	else {
		updateTextStart(gFightUIData.mRound.mTextID, gFightUIData.mRound.mDisplayNow, gFightUIData.mRound.mTime);
	}
}

static void updateRoundFinish() {
	if (gFightUIData.mRound.mDisplayNow >= gFightUIData.mRound.mTime + gFightUIData.mRound.mDisplayTime) {

		removeRoundDisplay();
		gFightUIData.mRound.mCB();
		gFightUIData.mRound.mIsDisplayingRound = 0;
	}
}

static void updateRoundDisplay() {
	if (!gFightUIData.mRound.mIsDisplayingRound) return;

	gFightUIData.mRound.mDisplayNow++;

	updateRoundSound();
	updateRoundStart();
	updateRoundFinish();
}

static void startControlCountdown() {
	gFightUIData.mControl.mNow = 0;
	gFightUIData.mControl.mIsCountingDownControl = 1;
}

static void updateFightSound() {

	if (gFightUIData.mFight.mDisplayNow >= (gFightUIData.mFight.mTime + gFightUIData.mFight.mSoundTime) && !gFightUIData.mFight.mHasPlayedSound) {
		if (gFightUIData.mFight.mHasSound) {
			tryPlayMugenSoundAdvanced(&gFightUIData.mFightSounds, gFightUIData.mFight.mSound.x, gFightUIData.mFight.mSound.y, parseGameMidiVolumeToPrism(getGameMidiVolume()));
		}
		gFightUIData.mFight.mHasPlayedSound = 1;
	}
}


static void updateFightFinish() {
	if (!getMugenAnimationRemainingAnimationTime(gFightUIData.mFight.mAnimationElement) || isMugenAnimationStepDurationInfinite(getMugenAnimationAnimationStepDuration(gFightUIData.mFight.mAnimationElement))) {
		removeDisplayedAnimation(gFightUIData.mFight.mAnimationElement);
		gFightUIData.mFight.mCB();
		gFightUIData.mFight.mIsDisplayingFight = 0;
	}
}

static void updateFightDisplay() {
	if (!gFightUIData.mFight.mIsDisplayingFight) return;

	gFightUIData.mFight.mDisplayNow++;
	updateFightSound();
	updateAnimationStart(gFightUIData.mFight.mAnimationElement, gFightUIData.mFight.mDisplayNow, gFightUIData.mFight.mTime);
	updateFightFinish();
}

static void updateKOSound() {

	if (gFightUIData.mKO.mDisplayNow >= (gFightUIData.mKO.mTime + gFightUIData.mKO.mSoundTime) && !gFightUIData.mKO.mHasPlayedSound) {
		if (gFightUIData.mKO.mHasSound) {
			tryPlayMugenSoundAdvanced(&gFightUIData.mFightSounds, gFightUIData.mKO.mSound.x, gFightUIData.mKO.mSound.y, parseGameMidiVolumeToPrism(getGameMidiVolume()));
		}
		gFightUIData.mKO.mHasPlayedSound = 1;
	}
}

static void updateKOFinish() {
	if (!getMugenAnimationRemainingAnimationTime(gFightUIData.mKO.mAnimationElement) || isMugenAnimationStepDurationInfinite(getMugenAnimationAnimationStepDuration(gFightUIData.mKO.mAnimationElement))) {
		removeDisplayedAnimation(gFightUIData.mKO.mAnimationElement);
		gFightUIData.mKO.mCB();
		gFightUIData.mKO.mIsDisplaying = 0;
	}
}

static void updateKODisplay() {
	if (!gFightUIData.mKO.mIsDisplaying) return;

	gFightUIData.mKO.mDisplayNow++;
	updateKOSound();
	updateAnimationStart(gFightUIData.mKO.mAnimationElement, gFightUIData.mKO.mDisplayNow, gFightUIData.mKO.mTime);
	updateKOFinish();
}

static void updateDKOSound() {

	if (gFightUIData.mDKO.mNow >= (gFightUIData.mKO.mTime + gFightUIData.mKO.mSoundTime) && !gFightUIData.mDKO.mHasPlayedSound) { // DKO does not have own soundtime
		if (gFightUIData.mDKO.mHasSound) {
			tryPlayMugenSoundAdvanced(&gFightUIData.mFightSounds, gFightUIData.mDKO.mSound.x, gFightUIData.mDKO.mSound.y, parseGameMidiVolumeToPrism(getGameMidiVolume()));
		}
		gFightUIData.mDKO.mHasPlayedSound = 1;
	}
}

static void updateDKOFinish() {
	if (gFightUIData.mDKO.mNow >= gFightUIData.mKO.mTime + gFightUIData.mDKO.mDisplayTime) {
		removeMugenText(gFightUIData.mDKO.mTextID);
		gFightUIData.mDKO.mCB();
		gFightUIData.mDKO.mIsDisplaying = 0;
	}
}

static void updateDKODisplay() {
	if (!gFightUIData.mDKO.mIsDisplaying) return;

	gFightUIData.mDKO.mNow++;
	updateDKOSound();
	updateTextStart(gFightUIData.mDKO.mTextID, gFightUIData.mDKO.mNow, gFightUIData.mKO.mTime);
	updateDKOFinish();
}

static void updateTOSound() {

	if (gFightUIData.mTO.mNow >= (gFightUIData.mKO.mTime + gFightUIData.mKO.mSoundTime) && !gFightUIData.mTO.mHasPlayedSound) { // TO does not have own soundtime
		if (gFightUIData.mTO.mHasSound) {
			tryPlayMugenSoundAdvanced(&gFightUIData.mFightSounds, gFightUIData.mTO.mSound.x, gFightUIData.mTO.mSound.y, parseGameMidiVolumeToPrism(getGameMidiVolume()));
		}
		gFightUIData.mTO.mHasPlayedSound = 1;
	}
}

static void updateTOFinish() {
	if (gFightUIData.mTO.mNow >= gFightUIData.mKO.mTime + gFightUIData.mTO.mDisplayTime) {
		removeMugenText(gFightUIData.mTO.mTextID);
		gFightUIData.mTO.mCB();
		gFightUIData.mTO.mIsDisplaying = 0;
	}
}

static void updateTODisplay() {
	if (!gFightUIData.mTO.mIsDisplaying) return;

	gFightUIData.mTO.mNow++;
	updateTOSound();
	updateTextStart(gFightUIData.mTO.mTextID, gFightUIData.mTO.mNow, gFightUIData.mKO.mTime);
	updateTOFinish();
}

static int hasFightUIPressedStartFlankAny() {
	return hasPressedStartFlankSingle(0) || hasPressedStartFlankSingle(1);
}

static int hasFightUIPressedAFlankAny() {
	return hasPressedAFlankSingle(0) || hasPressedAFlankSingle(1);
}

static void updateWinDisplayFinish() {
	if (gFightUIData.mWin.mNow >= (gFightUIData.mWin.mTime + gFightUIData.mWin.mDisplayTime) || hasFightUIPressedStartFlankAny()) {
		removeDisplayedText(gFightUIData.mWin.mTextID);
		gFightUIData.mWin.mCB();
		gFightUIData.mWin.mIsDisplaying = 0;
	}
}

static void updateWinDisplay() {
	if (!gFightUIData.mWin.mIsDisplaying) return;

	gFightUIData.mWin.mNow++;
	updateTextStart(gFightUIData.mWin.mTextID, gFightUIData.mWin.mNow, gFightUIData.mWin.mTime);
	updateWinDisplayFinish();
}

static void updateDrawDisplayFinish() {
	if (gFightUIData.mDraw.mNow >= (gFightUIData.mWin.mTime + gFightUIData.mDraw.mDisplayTime) || hasFightUIPressedStartFlankAny()) {
		removeDisplayedText(gFightUIData.mDraw.mTextID);
		gFightUIData.mDraw.mCB();
		gFightUIData.mDraw.mIsDisplaying = 0;
	}
}

static void updateDrawDisplay() {
	if (!gFightUIData.mDraw.mIsDisplaying) return;

	gFightUIData.mDraw.mNow++;
	updateTextStart(gFightUIData.mDraw.mTextID, gFightUIData.mDraw.mNow, gFightUIData.mWin.mTime);
	updateDrawDisplayFinish();
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

	const auto isNotFirstFrameOfCheck = (gFightUIData.mContinue.mNow > 1 || gFightUIData.mContinue.mValue != gFightUIData.mContinue.mCounterStartValue);
	if (hasFightUIPressedStartFlankAny() && isNotFirstFrameOfCheck) {
		setContinueInactive();
		gFightUIData.mContinue.mPressedContinueCB();
		return;
	}
	
	gFightUIData.mContinue.mNow++;
	if (gFightUIData.mContinue.mNow >= gFightUIData.mContinue.mDuration || hasFightUIPressedAFlankAny()) {
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

	if (gFightUIData.mEnvironmentEffects.mNow >= gFightUIData.mEnvironmentEffects.mDuration) {
		setAnimationScale(gFightUIData.mEnvironmentEffects.mAnimationElement, Vector3D(0, 0, 0), Vector3D(0, 0, 0));
		gFightUIData.mEnvironmentEffects.mIsActive = 0;
	}
	gFightUIData.mEnvironmentEffects.mNow++;
}

static double calculateEnvironmentShake(int t) {
	return calculateShake(t, gFightUIData.mEnvironmentShake.mPhaseOffset, gFightUIData.mEnvironmentShake.mFrequency, gFightUIData.mEnvironmentShake.mAmplitude);
}

static void updateEnvironmentShake() {
	if (!gFightUIData.mEnvironmentShake.mIsActive) return;

	setDreamMugenStageHandlerScreenShake(Vector2D(0.f, calculateEnvironmentShake(gFightUIData.mEnvironmentShake.mNow)));

	if (gFightUIData.mEnvironmentShake.mNow >= gFightUIData.mEnvironmentShake.mDuration) {
		setDreamMugenStageHandlerScreenShake(Vector2D(0.f, 0.f));
		gFightUIData.mEnvironmentShake.mIsActive = 0;
	}
	gFightUIData.mEnvironmentShake.mNow++;
}

static void updateFightUI(void* tData) {
	(void)tData;
	setProfilingSectionMarkerCurrentFunction();

	updateHitSparks();
	updateUISide();
	updateRoundDisplay();
	updateFightDisplay();
	updateKODisplay();
	updateDKODisplay();
	updateTODisplay();
	updateWinDisplay();
	updateDrawDisplay();
	updateControlCountdown();
	updateTimeDisplay();
	updateContinueDisplay();
	updateEnvironmentColor();
	updateEnvironmentShake();
}

ActorBlueprint getDreamFightUIBP() {
	return makeActorBlueprint(loadFightUI, unloadFightUI, updateFightUI);
};

void setCustomFightMotif(const std::string& tPath)
{
	gFightUIData.mCustomFightMotifPath = tPath;
}

int hasCustomFightMotif()
{
	return gFightUIData.mCustomFightMotifPath != "";
}

const std::string& getCustomFightMotif()
{
	return gFightUIData.mCustomFightMotifPath;
}

void playDreamHitSpark(const Position2D& tPosition, DreamPlayer* tPlayer, int tIsInPlayerFile, int tNumber, int tIsFacingRight, int tPositionCoordinateP)
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
	
	e.mPosition = tPosition.xyz(HITSPARK_BASE_Z);
	e.mAnimationElement = addMugenAnimation(anim, spriteFile, getDreamStageCoordinateSystemOffset(tPositionCoordinateP).xyz(0.0));
	setMugenAnimationBasePosition(e.mAnimationElement, &e.mPosition);
	setMugenAnimationCameraPositionReference(e.mAnimationElement, getDreamMugenStageHandlerCameraPositionReference());
	setMugenAnimationBaseDrawScale(e.mAnimationElement, (getScreenSize().x / double(getDreamUICoordinateP())) * getDreamUIFightFXScale());
	setMugenAnimationCameraEffectPositionReference(e.mAnimationElement, getDreamMugenStageHandlerCameraEffectPositionReference());
	setMugenAnimationCameraScaleReference(e.mAnimationElement, getDreamMugenStageHandlerCameraZoomReference());
	if (!tIsFacingRight) {
		setMugenAnimationFaceDirection(e.mAnimationElement, tIsFacingRight);
	}
	
}

void addDreamDustCloud(const Position& tPositionCameraSpace, int tIsFacingRight)
{
	const auto pos = vecAdd(tPositionCameraSpace, getDreamStageCoordinateSystemOffset(getDreamMugenStageHandlerCameraCoordinateP()).xyz(0.0));
	auto element = addMugenAnimation(getMugenAnimation(&gFightUIData.mFightFXAnimations, 120), &gFightUIData.mFightFXSprites, pos);
	setMugenAnimationNoLoop(element);
	setMugenAnimationCameraPositionReference(element, getDreamMugenStageHandlerCameraPositionReference());
	setMugenAnimationBaseDrawScale(element, (getScreenSize().x / double(getDreamUICoordinateP())) * getDreamUIFightFXScale());
	setMugenAnimationCameraEffectPositionReference(element, getDreamMugenStageHandlerCameraEffectPositionReference());
	setMugenAnimationCameraScaleReference(element, getDreamMugenStageHandlerCameraZoomReference());
	if (!tIsFacingRight) {
		setMugenAnimationFaceDirection(element, tIsFacingRight);
	}
}

void setDreamLifeBarPercentage(DreamPlayer* tPlayer, double tPercentage)
{
	HealthBar* bar = &gFightUIData.mHealthBars[tPlayer->mRootID];

	bar->mPercentage = tPercentage;
	if (!bar->mFrontAnimationElement) return;
	setBarToPercentage(bar->mFrontAnimationElement, bar->mHealthRangeX, bar->mPercentage);

	if (bar->mPercentage > bar->mDisplayedPercentage) {
		bar->mDisplayedPercentage = bar->mPercentage;
		setBarToPercentage(bar->mMidAnimationElement, bar->mHealthRangeX, bar->mDisplayedPercentage);
	}
	bar->mIsPaused = 1;
	bar->mPauseNow = 0;
}

void setDreamPowerBarPercentage(DreamPlayer* tPlayer, double tPercentage, int tValue)
{
	PowerBar* bar = &gFightUIData.mPowerBars[tPlayer->mRootID];
	if (!bar->mFrontAnimationElement) return;
	setBarToPercentage(bar->mFrontAnimationElement, bar->mPowerRangeX, tPercentage);

	int newLevel = tValue / 1000;

	if (newLevel >= 1 && newLevel > bar->mLevel && bar->mHasLevelSound[newLevel - 1]) {
		tryPlayMugenSoundAdvanced(&gFightUIData.mFightSounds, bar->mLevelSounds[newLevel - 1].x, bar->mLevelSounds[newLevel - 1].y, parseGameMidiVolumeToPrism(getGameMidiVolume()));
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
	if (hasCustomTimerDuration()) {
		gFightUIData.mTime.mValue = getCustomTimerDuration();
	}
	else {
		gFightUIData.mTime.mValue = getGlobalTimerDuration();
	}
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

MugenAnimation* getDreamFightEffectAnimation(int tNumber)
{
	return getMugenAnimation(&gFightUIData.mFightFXAnimations, tNumber);
}

MugenSpriteFile* getDreamFightEffectSprites()
{
	return &gFightUIData.mFightFXSprites;
}

MugenSounds* getDreamCommonSounds()
{
	return &gFightUIData.mCommonSounds;
}

int getDreamUICoordinateP()
{
	return gFightUIData.mFightFX.mCoordinateP;
}

double getDreamUIFightFXScale()
{
	return gFightUIData.mFightFX.mScale;
}

static void parseRoundText(char* tDst, char* tSrc, int tRoundNumber) {

	int i, o = 0;
	int n = int(strlen(tSrc));
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
		playDisplayAnimation(&gFightUIData.mRound.mAnimationElement, gFightUIData.mRound.mCustomRoundAnimations[tRound], &gFightUIData.mRound.mCustomRoundPositions[tRound], gFightUIData.mRound.mCustomRoundFaceDirection[tRound], gFightUIData.mRound.mCustomRoundScale[tRound], gFightUIData.mRound.mTime);
	}
	else if (gFightUIData.mRound.mHasDefaultAnimation) {
		gFightUIData.mRound.mHasActiveAnimation = 1;
		playDisplayAnimation(&gFightUIData.mRound.mAnimationElement, gFightUIData.mRound.mDefaultAnimation, &gFightUIData.mRound.mPosition, gFightUIData.mRound.mDefaultFaceDirection, gFightUIData.mRound.mDefaultScale, gFightUIData.mRound.mTime);
	}
	else {
		gFightUIData.mRound.mHasActiveAnimation = 0;
		parseRoundText(gFightUIData.mRound.mDisplayedText, gFightUIData.mRound.mText, tRound + 1);

		playDisplayText(&gFightUIData.mRound.mTextID, gFightUIData.mRound.mDisplayedText, gFightUIData.mRound.mPosition, gFightUIData.mRound.mFont, gFightUIData.mRound.mTime);
	}

	gFightUIData.mRound.mCB = tFunc;
	gFightUIData.mRound.mDisplayNow = 0;
	gFightUIData.mRound.mHasPlayedSound = 0;
	gFightUIData.mRound.mRoundIndex = tRound;
	gFightUIData.mRound.mIsDisplayingRound = 1;
}

void playDreamFightAnimation(void(*tFunc)())
{
	playDisplayAnimation(&gFightUIData.mFight.mAnimationElement, gFightUIData.mFight.mAnimation, &gFightUIData.mFight.mPosition, gFightUIData.mFight.mFaceDirection, gFightUIData.mFight.mScale, gFightUIData.mFight.mTime);

	gFightUIData.mFight.mCB = tFunc;
	gFightUIData.mFight.mDisplayNow = 0;
	gFightUIData.mFight.mHasPlayedSound = 0;
	gFightUIData.mFight.mIsDisplayingFight = 1;
	startControlCountdown();
}

void playDreamKOAnimation(void(*tFunc)())
{
	playDisplayAnimation(&gFightUIData.mKO.mAnimationElement, gFightUIData.mKO.mAnimation, &gFightUIData.mKO.mPosition, gFightUIData.mKO.mFaceDirection, gFightUIData.mKO.mScale, gFightUIData.mKO.mTime);

	gFightUIData.mKO.mDisplayNow = 0;
	gFightUIData.mKO.mHasPlayedSound = 0;
	gFightUIData.mKO.mCB = tFunc;
	gFightUIData.mKO.mIsDisplaying = 1;
}

void playDreamDKOAnimation(void(*tFunc)())
{
	playDisplayText(&gFightUIData.mDKO.mTextID, gFightUIData.mDKO.mText, gFightUIData.mDKO.mPosition, gFightUIData.mDKO.mFont, gFightUIData.mKO.mTime);

	gFightUIData.mDKO.mNow = 0;
	gFightUIData.mDKO.mHasPlayedSound = 0;
	gFightUIData.mDKO.mCB = tFunc;
	gFightUIData.mDKO.mIsDisplaying = 1;
}

void playDreamTOAnimation(void(*tFunc)())
{
	playDisplayText(&gFightUIData.mTO.mTextID, gFightUIData.mTO.mText, gFightUIData.mTO.mPosition, gFightUIData.mTO.mFont, gFightUIData.mKO.mTime);

	gFightUIData.mTO.mNow = 0;
	gFightUIData.mTO.mHasPlayedSound = 0;
	gFightUIData.mTO.mCB = tFunc;
	gFightUIData.mTO.mIsDisplaying = 1;
}

static void parseWinText(char* tDst, char* tSrc, char* tName, Position* oDisplayPosition, const Position& tPosition) {
	int i, o = 0;
	int bonus = 0;
	int n = int(strlen(tSrc));
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
	playDisplayText(&gFightUIData.mWin.mTextID, gFightUIData.mWin.mDisplayedText, gFightUIData.mWin.mDisplayPosition, gFightUIData.mWin.mFont, gFightUIData.mWin.mTime);
	
	gFightUIData.mWin.mNow = 0;
	gFightUIData.mWin.mCB = tFunc;
	gFightUIData.mWin.mIsDisplaying = 1;
}

void playDreamDrawAnimation(void(*tFunc)())
{
	playDisplayText(&gFightUIData.mDraw.mTextID, gFightUIData.mDraw.mText, gFightUIData.mDraw.mPosition, gFightUIData.mDraw.mFont, gFightUIData.mWin.mTime);

	gFightUIData.mDraw.mNow = 0;
	gFightUIData.mDraw.mCB = tFunc;
	gFightUIData.mDraw.mIsDisplaying = 1;
}

void playDreamContinueAnimation(void(*tAnimationFinishedFunc)(), void(*tContinuePressedFunc)())
{
	if (!gFightUIData.mContinue.mIsEnabled) {
		tAnimationFinishedFunc();
		return;
	}
	
	gFightUIData.mContinue.mFinishedCB = tAnimationFinishedFunc;
	gFightUIData.mContinue.mPressedContinueCB = tContinuePressedFunc;
	gFightUIData.mContinue.mValue = gFightUIData.mContinue.mCounterStartValue;
	gFightUIData.mContinue.mNow = 0;
	gFightUIData.mContinue.mDuration = gFightUIData.mContinue.mCounterDuration;

	std::string integerBeginning;
	convertIntegerToStringFast(integerBeginning, gFightUIData.mContinue.mValue);
	playDisplayText(&gFightUIData.mContinue.mContinueTextID, gFightUIData.mContinue.mContinueText.c_str(), gFightUIData.mContinue.mContinueTextPosition.xyz(UI_BASE_Z), gFightUIData.mContinue.mContinueFont, 0);
	playDisplayText(&gFightUIData.mContinue.mValueTextID, integerBeginning.c_str(), gFightUIData.mContinue.mCounterTextPosition.xyz(UI_BASE_Z), gFightUIData.mContinue.mCounterFont, 0);

	gFightUIData.mContinue.mIsActive = 1;
}

void setDreamTimeDisplayFinishedCB(void(*tTimeDisplayFinishedFunc)())
{
	gFightUIData.mTime.mFinishedCB = tTimeDisplayFinishedFunc;
}

static void setSingleUIComponentInvisibleForOneFrame(MugenAnimationHandlerElement* tAnimationElement) {
	if(tAnimationElement != NULL) setMugenAnimationInvisibleForOneFrame(tAnimationElement);
}

static void setSingleUITextInvisibleForOneFrame(int tTextID) {
	if (tTextID != -1) setMugenTextInvisibleForOneFrame(tTextID);
}

void setDreamBarInvisibleForOneFrame()
{
	int i;
	for (i = 0; i < 2; i++) {
		setSingleUIComponentInvisibleForOneFrame(gFightUIData.mHealthBars[i].mBG0AnimationElement);
		setSingleUIComponentInvisibleForOneFrame(gFightUIData.mHealthBars[i].mBG1AnimationElement);
		setSingleUIComponentInvisibleForOneFrame(gFightUIData.mHealthBars[i].mMidAnimationElement);
		setSingleUIComponentInvisibleForOneFrame(gFightUIData.mHealthBars[i].mFrontAnimationElement);

		setSingleUIComponentInvisibleForOneFrame(gFightUIData.mPowerBars[i].mBG0AnimationElement);
		setSingleUIComponentInvisibleForOneFrame(gFightUIData.mPowerBars[i].mBG1AnimationElement);
		setSingleUIComponentInvisibleForOneFrame(gFightUIData.mPowerBars[i].mMidAnimationElement);
		setSingleUIComponentInvisibleForOneFrame(gFightUIData.mPowerBars[i].mFrontAnimationElement);
		setSingleUITextInvisibleForOneFrame(gFightUIData.mPowerBars[i].mCounterTextID);

		setSingleUIComponentInvisibleForOneFrame(gFightUIData.mFaces[i].mBGAnimationElement);
		setSingleUIComponentInvisibleForOneFrame(gFightUIData.mFaces[i].mBG0AnimationElement);
		setSingleUIComponentInvisibleForOneFrame(gFightUIData.mFaces[i].mBG1AnimationElement);
		setSingleUIComponentInvisibleForOneFrame(gFightUIData.mFaces[i].mFaceAnimationElement);

		setSingleUIComponentInvisibleForOneFrame(gFightUIData.mDisplayName[i].mBGAnimationElement);

		for (int j = 0; j < gFightUIData.mWinIcons[i].mIconAmount; j++) {
			setSingleUIComponentInvisibleForOneFrame(gFightUIData.mWinIcons[i].mIconAnimationElements[j]);
			if (gFightUIData.mWinIcons[i].mHasIsPerfectIcon[j]) {
				setSingleUIComponentInvisibleForOneFrame(gFightUIData.mWinIcons[i].mPerfectIconAnimationElements[j]);
			}
		}
		setSingleUITextInvisibleForOneFrame(gFightUIData.mDisplayName[i].mTextID);
	}

	setSingleUIComponentInvisibleForOneFrame(gFightUIData.mTime.mBGAnimationElement);
	setSingleUITextInvisibleForOneFrame(gFightUIData.mTime.mTextID);
}

static void setSingleUIComponentPaletteEffects(MugenAnimationHandlerElement* tAnimationElement, int tDuration, const Vector3D& tAddition, const Vector3D& tMultiplier, const Vector3D& tSineAmplitude, int tSinePeriod, int tInvertAll, double tColorFactor) {
	if (tAnimationElement != NULL) {
		setMugenAnimationPaletteEffectForDuration(tAnimationElement, tDuration, tAddition, tMultiplier, tSineAmplitude, tSinePeriod, tInvertAll, tColorFactor);
	}
}

void setDreamBarPaletteEffects(int tDuration, const Vector3D& tAddition, const Vector3D& tMultiplier, const Vector3D& tSineAmplitude, int tSinePeriod, int tInvertAll, double tColorFactor)
{
	int i;
	for (i = 0; i < 2; i++) {
		setSingleUIComponentPaletteEffects(gFightUIData.mHealthBars[i].mBG0AnimationElement, tDuration, tAddition, tMultiplier, tSineAmplitude, tSinePeriod, tInvertAll, tColorFactor);
		setSingleUIComponentPaletteEffects(gFightUIData.mHealthBars[i].mBG1AnimationElement, tDuration, tAddition, tMultiplier, tSineAmplitude, tSinePeriod, tInvertAll, tColorFactor);
		setSingleUIComponentPaletteEffects(gFightUIData.mHealthBars[i].mMidAnimationElement, tDuration, tAddition, tMultiplier, tSineAmplitude, tSinePeriod, tInvertAll, tColorFactor);
		setSingleUIComponentPaletteEffects(gFightUIData.mHealthBars[i].mFrontAnimationElement, tDuration, tAddition, tMultiplier, tSineAmplitude, tSinePeriod, tInvertAll, tColorFactor);

		setSingleUIComponentPaletteEffects(gFightUIData.mPowerBars[i].mBG0AnimationElement, tDuration, tAddition, tMultiplier, tSineAmplitude, tSinePeriod, tInvertAll, tColorFactor);
		setSingleUIComponentPaletteEffects(gFightUIData.mPowerBars[i].mBG1AnimationElement, tDuration, tAddition, tMultiplier, tSineAmplitude, tSinePeriod, tInvertAll, tColorFactor);
		setSingleUIComponentPaletteEffects(gFightUIData.mPowerBars[i].mMidAnimationElement, tDuration, tAddition, tMultiplier, tSineAmplitude, tSinePeriod, tInvertAll, tColorFactor);
		setSingleUIComponentPaletteEffects(gFightUIData.mPowerBars[i].mFrontAnimationElement, tDuration, tAddition, tMultiplier, tSineAmplitude, tSinePeriod, tInvertAll, tColorFactor);

		setSingleUIComponentPaletteEffects(gFightUIData.mFaces[i].mBGAnimationElement, tDuration, tAddition, tMultiplier, tSineAmplitude, tSinePeriod, tInvertAll, tColorFactor);
		setSingleUIComponentPaletteEffects(gFightUIData.mFaces[i].mBG0AnimationElement, tDuration, tAddition, tMultiplier, tSineAmplitude, tSinePeriod, tInvertAll, tColorFactor);
		setSingleUIComponentPaletteEffects(gFightUIData.mFaces[i].mBG1AnimationElement, tDuration, tAddition, tMultiplier, tSineAmplitude, tSinePeriod, tInvertAll, tColorFactor);
		setSingleUIComponentPaletteEffects(gFightUIData.mFaces[i].mFaceAnimationElement, tDuration, tAddition, tMultiplier, tSineAmplitude, tSinePeriod, tInvertAll, tColorFactor);

		setSingleUIComponentPaletteEffects(gFightUIData.mDisplayName[i].mBGAnimationElement, tDuration, tAddition, tMultiplier, tSineAmplitude, tSinePeriod, tInvertAll, tColorFactor);

		for (int j = 0; j < gFightUIData.mWinIcons[i].mIconAmount; j++) {
			setSingleUIComponentPaletteEffects(gFightUIData.mWinIcons[i].mIconAnimationElements[j], tDuration, tAddition, tMultiplier, tSineAmplitude, tSinePeriod, tInvertAll, tColorFactor);
			if (gFightUIData.mWinIcons[i].mHasIsPerfectIcon[j]) {
				setSingleUIComponentPaletteEffects(gFightUIData.mWinIcons[i].mPerfectIconAnimationElements[j], tDuration, tAddition, tMultiplier, tSineAmplitude, tSinePeriod, tInvertAll, tColorFactor);
			}
		}
	}

	setSingleUIComponentPaletteEffects(gFightUIData.mTime.mBGAnimationElement, tDuration, tAddition, tMultiplier, tSineAmplitude, tSinePeriod, tInvertAll, tColorFactor);
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
	return !gFightUIData.mTime.mIsInfinite && gFightUIData.mTime.mIsFinished;
}

void setEnvironmentColor(const Vector3DI& tColors, int tTime, int tIsUnderCharacters)
{
	setAnimationPosition(gFightUIData.mEnvironmentEffects.mAnimationElement, Vector3D(0, 0, tIsUnderCharacters ? ENVIRONMENT_COLOR_LOWER_Z : ENVIRONMENT_COLOR_UPPER_Z));
	setAnimationSize(gFightUIData.mEnvironmentEffects.mAnimationElement, Vector3D(640, 480, 1), Vector3D(0, 0, 0));
	setAnimationColor(gFightUIData.mEnvironmentEffects.mAnimationElement, tColors.x / 255.0, tColors.y / 255.0, tColors.z / 255.0);

	gFightUIData.mEnvironmentEffects.mDuration = tTime;
	gFightUIData.mEnvironmentEffects.mNow = 0;
	gFightUIData.mEnvironmentEffects.mIsActive = 1;

}

void setEnvironmentShake(int tDuration, double tFrequency, int tAmplitude, double tPhaseOffset, int tCoordinateP)
{
	gFightUIData.mEnvironmentShake.mFrequency = tFrequency;
	gFightUIData.mEnvironmentShake.mAmplitude = transformDreamCoordinatesI(tAmplitude, tCoordinateP, getDreamMugenStageHandlerCameraCoordinateP());
	gFightUIData.mEnvironmentShake.mPhaseOffset = tPhaseOffset;

	gFightUIData.mEnvironmentShake.mNow = 0;
	gFightUIData.mEnvironmentShake.mDuration = tDuration;
	gFightUIData.mEnvironmentShake.mIsActive = 1;
}

static void addGeneralWinIcon(int tPlayer, const WinIconType& tWinIconType, int tIsPerfect) {
	WinIcon* icon = &gFightUIData.mWinIcons[tPlayer];

	if (icon->mIconAmount >= icon->mIconUpToAmount) return;

	int id = icon->mIconAmount;
	Position pos = vecAdd2D(icon->mPosition, vecScale(icon->mOffset, id));	
	icon->mIconAnimations[id] = createOneFrameMugenAnimationForSprite(tWinIconType.mSprite.x, tWinIconType.mSprite.y);
	icon->mIconAnimationElements[id] = addMugenAnimation(icon->mIconAnimations[id], &gFightUIData.mFightSprites, pos);
	setMugenAnimationDrawScale(icon->mIconAnimationElements[id], tWinIconType.mScale);

	icon->mHasIsPerfectIcon[id] = tIsPerfect;
	if (icon->mHasIsPerfectIcon[id]) {
		pos.z++;
		icon->mPerfectIconAnimations[id] = createOneFrameMugenAnimationForSprite(icon->mPerfectWin.mSprite.x, icon->mPerfectWin.mSprite.y);
		icon->mPerfectIconAnimationElements[id] = addMugenAnimation(icon->mPerfectIconAnimations[id], &gFightUIData.mFightSprites, pos);
		setMugenAnimationDrawScale(icon->mPerfectIconAnimationElements[id], icon->mPerfectWin.mScale);
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
		removeMugenAnimation(icon->mIconAnimationElements[i]);
		destroyMugenAnimation(icon->mIconAnimations[i]);

		if (icon->mHasIsPerfectIcon[i]) {
			removeMugenAnimation(icon->mPerfectIconAnimationElements[i]);
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
		removeDisplayedAnimation(gFightUIData.mFight.mAnimationElement);
		gFightUIData.mFight.mIsDisplayingFight = 0;
	}

	startControlCountdown();	
}

void setUIFaces() {
	setMugenAnimationSprites(gFightUIData.mFaces[0].mFaceAnimationElement, getPlayerSprites(getRootPlayer(0)));
	setMugenAnimationSprites(gFightUIData.mFaces[1].mFaceAnimationElement, getPlayerSprites(getRootPlayer(1)));
}

void setComboUIDisplay(int i, int tAmount)
{
	if (tAmount < 2) return;

	Combo* combo = &gFightUIData.mCombos[i];
	char number[20];
	sprintf(number, "%d", tAmount);

	changeMugenText(combo->mNumberTextID, number);
	combo->mShakeNow = 0;
	combo->mDisplayNow = 0;
	combo->mIsActive = 1;
}

int getSlowTime()
{
	return gFightUIData.mSlow.mTime;
}

int getStartWaitTime()
{
	return gFightUIData.mStart.mWaitTime;
}

int getOverWaitTime()
{
	return gFightUIData.mOver.mWaitTime;
}

int getOverHitTime()
{
	return gFightUIData.mOver.mHitTime;
}

int getOverWinTime()
{
	return gFightUIData.mOver.mWinTime;
}

int getOverTime()
{
	return gFightUIData.mOver.mTime;
}