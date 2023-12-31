#include "optionsscreen.h"

#include <algorithm>

#include <prism/wrapper.h>
#include <prism/input.h>
#include <prism/mugentexthandler.h>
#include <prism/mugenanimationhandler.h>
#include <prism/clipboardhandler.h>
#include <prism/screeneffect.h>
#include <prism/mugensoundfilereader.h>
#include <prism/memoryhandler.h>
#include <prism/mugendefreader.h>
#include <prism/animation.h>
#include <prism/stlutil.h>
#include <prism/system.h>
#include <prism/saveload.h>

#include "titlescreen.h"
#include "scriptbackground.h"
#include "boxcursorhandler.h"
#include "config.h"

using namespace std;

typedef struct {
	Vector3DI mCursorMoveSound;
	Vector3DI mCursorDoneSound;
	Vector3DI mCancelSound;
} OptionsHeader;

typedef enum {
	GENERAL_SETTING_DIFFICULTY,
	GENERAL_SETTING_LIFE,
	GENERAL_SETTING_TIME_LIMIT,
	GENERAL_SETTING_GAME_SPEED,
	GENERAL_SETTING_WAV_VOLUME,
	GENERAL_SETTING_MIDI_VOLUME,
	GENERAL_SETTING_INPUT_CONFIG,
	GENERAL_SETTING_TEAM_MODE_CONFIG,
	GENERAL_SETTING_LOAD_SAVE_OPTIONS,
	GENERAL_SETTING_LOAD_SAVE_VARS,
	GENERAL_SETTING_DEFAULT_VALUES,
	GENERAL_SETTING_RETURN,
	GENERAL_SETTING_AMOUNT
} GeneralSettingType;

typedef struct {
	int mBoxCursorID;

	int mSelected;
	
	int mSelectableTextID[GENERAL_SETTING_AMOUNT];
	int mSettingTextID[GENERAL_SETTING_AMOUNT];
} GeneralOptionsScreen;

typedef enum {
	INPUT_CONFIG_SETTING_KEY_CONFIG,
	INPUT_CONFIG_SETTING_JOYSTICK_TYPE,
	INPUT_CONFIG_SETTING_JOYSTICK_CONFIG,
	INPUT_CONFIG_SETTING_DEFAULT_VALUES,
	INPUT_CONFIG_SETTING_RETURN,

	INPUT_CONFIG_SETTING_AMOUNT
} InputConfigSettingType;

typedef struct {
	int mBoxCursorID[2];
	int mSelected[2];
	int mPlayerTextID[2];

	int mSelectableTextID[2][INPUT_CONFIG_SETTING_AMOUNT];
	int mSettingTextID[2][INPUT_CONFIG_SETTING_AMOUNT];
} InputConfigOptionsScreen;

typedef enum {
	KEY_CONFIG_SETTING_CONFIG_ALL,
	KEY_CONFIG_SETTING_UP,
	KEY_CONFIG_SETTING_DOWN,
	KEY_CONFIG_SETTING_LEFT,
	KEY_CONFIG_SETTING_RIGHT,
	KEY_CONFIG_SETTING_A,
	KEY_CONFIG_SETTING_B,
	KEY_CONFIG_SETTING_C,
	KEY_CONFIG_SETTING_X,
	KEY_CONFIG_SETTING_Y,
	KEY_CONFIG_SETTING_Z,
	KEY_CONFIG_SETTING_START,
	KEY_CONFIG_SETTING_DEFAULT,
	KEY_CONFIG_SETTING_EXIT,

	KEY_CONFIG_SETTING_AMOUNT
} KeyConfigSettingType;

typedef struct {
	int mBoxCursorID;
	Vector3DI mSelected;
	int mPlayerTextID[2];

	int mIsSettingInput;
	int mCurrentTargetButton;
	int mSetKeyBoxCursorID;
	int mSettingFlankDone;

	int mSelectableTextID[2][KEY_CONFIG_SETTING_AMOUNT];
	int mSettingTextID[2][KEY_CONFIG_SETTING_AMOUNT];
} KeyConfigOptionsScreen;

typedef enum {
	DREAMCAST_SAVE_SETTING_SELECT_VMU,
	DREAMCAST_SAVE_SETTING_SAVE,
	DREAMCAST_SAVE_SETTING_LOAD,
	DREAMCAST_SAVE_SETTING_DELETE,
	DREAMCAST_SAVE_SETTING_RETURN,
	DREAMCAST_SAVE_SETTING_AMOUNT,
} DreamcastSaveSettingType;

static const std::string gVMUNames[uint32_t(PrismSaveSlot::AMOUNT)] = {
	"A1",
	"A2",
	"B1",
	"B2",
	"C1",
	"C2",
	"D1",
	"D2",
};

#ifdef VITA
static const std::string gButtonShortCutStrings[6] = {
	"(X)",
	"(O)",
	"(Sqr)",
	"(Trngl)",
	"(L)",
	"(R)",
};
#else
static const std::string gButtonShortCutStrings[6] = {
	"(A)",
	"(B)",
	"(X)",
	"(Y)",
	"(L)",
	"(R)",
};
#endif

typedef struct {
	int mBoxCursorID;
	int mSelected;

	int mHasSave;
	int mFreeBlocksTextID;
	int mSaveSizeBlocksTextID;

	int mSelectableTextID[DREAMCAST_SAVE_SETTING_AMOUNT];
	int mSettingTextID[DREAMCAST_SAVE_SETTING_AMOUNT];

	GeneralSettingType mSaveLoadType;

	PrismSaveSlot mSelectedVMU;
} DreamcastSaveOptionsScreen;

typedef struct {
	int mIsActive;

	AnimationHandlerElement* mBackgroundAnimationElement;
	int mQuestionTextID;
	int mBoxCursorID;
	int mSelectionTextID[2];
	int mSelectedOption;
	std::function<void()> mFunc;
} OptionScreenConfirmationDialog;

typedef enum {
	OPTION_SCREEN_GENERAL,
	OPTION_SCREEN_INPUT_CONFIG,
	OPTION_SCREEN_KEY_CONFIG,
	OPTION_SCREEN_DREAMCAST_SAVE,
} OptionScreenType;

static struct {
	TextureData mWhiteTexture;
	int mTextFontID;

	MugenDefScript mScript;
	MugenSpriteFile mSprites;
	MugenAnimations mAnimations;
	MugenSounds mSounds;

	OptionsHeader mHeader;
	GeneralOptionsScreen mGeneral;
	InputConfigOptionsScreen mInputConfig;
	KeyConfigOptionsScreen mKeyConfig;
	DreamcastSaveOptionsScreen mDreamcastSave;
	OptionScreenConfirmationDialog mConfirmationDialog;
	int mHeaderTextID;
	AnimationHandlerElement* mBackgroundAnimationElement;

	OptionScreenType mActiveScreen;
} gOptionsScreenData;

static void loadOptionsHeader() {
	gOptionsScreenData.mHeader.mCursorMoveSound = getMugenDefVectorIOrDefault(&gOptionsScreenData.mScript, "option info", "cursor.move.snd", Vector3DI(1, 0, 0));
	gOptionsScreenData.mHeader.mCursorDoneSound = getMugenDefVectorIOrDefault(&gOptionsScreenData.mScript, "option info", "cursor.done.snd", Vector3DI(1, 0, 0));
	gOptionsScreenData.mHeader.mCancelSound = getMugenDefVectorIOrDefault(&gOptionsScreenData.mScript, "option info", "cancel.snd", Vector3DI(1, 0, 0));

}

static void setSelectedGeneralOptionInactive() {
	setMugenTextColor(gOptionsScreenData.mGeneral.mSelectableTextID[gOptionsScreenData.mGeneral.mSelected], getMugenTextColorFromMugenTextColorIndex(8));

	Vector3D selectionColor = getMugenTextColor(gOptionsScreenData.mGeneral.mSettingTextID[gOptionsScreenData.mGeneral.mSelected]);
	if (selectionColor.x != 0 && selectionColor.y != 0 && selectionColor.z != 0) {
		setMugenTextColor(gOptionsScreenData.mGeneral.mSettingTextID[gOptionsScreenData.mGeneral.mSelected], getMugenTextColorFromMugenTextColorIndex(8));
	}
}

static void setSelectedGeneralOptionActive() {
	setMugenTextColor(gOptionsScreenData.mGeneral.mSelectableTextID[gOptionsScreenData.mGeneral.mSelected], getMugenTextColorFromMugenTextColorIndex(0));

	Vector3D selectionColor = getMugenTextColor(gOptionsScreenData.mGeneral.mSettingTextID[gOptionsScreenData.mGeneral.mSelected]);
	if (selectionColor.x != 0 && selectionColor.y != 0 && selectionColor.z != 0) {
		setMugenTextColor(gOptionsScreenData.mGeneral.mSettingTextID[gOptionsScreenData.mGeneral.mSelected], getMugenTextColorFromMugenTextColorIndex(0));
	}

	Position pos = getMugenTextPosition(gOptionsScreenData.mGeneral.mSelectableTextID[gOptionsScreenData.mGeneral.mSelected]);
	pos.z = 0;
	setBoxCursorPosition(gOptionsScreenData.mGeneral.mBoxCursorID, pos);
}

static void updateSelectedGeneralOption(int tDelta) {
	setSelectedGeneralOptionInactive();
	gOptionsScreenData.mGeneral.mSelected += tDelta;
	gOptionsScreenData.mGeneral.mSelected = (gOptionsScreenData.mGeneral.mSelected + GENERAL_SETTING_AMOUNT) % GENERAL_SETTING_AMOUNT;
	tryPlayMugenSoundAdvanced(&gOptionsScreenData.mSounds, gOptionsScreenData.mHeader.mCursorMoveSound.x, gOptionsScreenData.mHeader.mCursorMoveSound.y, parseGameMidiVolumeToPrism(getGameMidiVolume()));
	setSelectedGeneralOptionActive();
}

static void setDifficultyOptionText();
static void setLifeOptionText();
static void setTimeLimitOptionText();
static void setGameSpeedOptionText();
static void setVolumeOptionText(GeneralSettingType tType, int(*tGet)());

static void loadGeneralOptionsScreen() {
	setAnimationPosition(gOptionsScreenData.mBackgroundAnimationElement, Vector3D(52, 35, 40));
	setAnimationSize(gOptionsScreenData.mBackgroundAnimationElement, Vector3D(216, 198, 1), Vector3D(0, 0, 0));
	
	changeMugenText(gOptionsScreenData.mHeaderTextID, "OPTIONS");
	setMugenTextPosition(gOptionsScreenData.mHeaderTextID, Vector3D(160, 20, 45));
	gOptionsScreenData.mActiveScreen = OPTION_SCREEN_GENERAL;

	int offsetX = 70;
	int startY = 50;
	int offsetY = 13;
	gOptionsScreenData.mGeneral.mSelectableTextID[GENERAL_SETTING_DIFFICULTY] = addMugenTextMugenStyle("Difficulty", Vector3D(offsetX, startY, 45), Vector3DI(gOptionsScreenData.mTextFontID, 8, 1));
	gOptionsScreenData.mGeneral.mSettingTextID[GENERAL_SETTING_DIFFICULTY] = addMugenTextMugenStyle("Hard 8", Vector3D(320 - offsetX, startY, 45), Vector3DI(gOptionsScreenData.mTextFontID, 8, -1));
	setDifficultyOptionText();

	gOptionsScreenData.mGeneral.mSelectableTextID[GENERAL_SETTING_LIFE] = addMugenTextMugenStyle("Life", Vector3D(offsetX, startY + offsetY * 1, 45), Vector3DI(gOptionsScreenData.mTextFontID, 8, 1));
	gOptionsScreenData.mGeneral.mSettingTextID[GENERAL_SETTING_LIFE] = addMugenTextMugenStyle("100%", Vector3D(320 - offsetX, startY + offsetY * 1, 45), Vector3DI(gOptionsScreenData.mTextFontID, 8, -1));
	setLifeOptionText();

	gOptionsScreenData.mGeneral.mSelectableTextID[GENERAL_SETTING_TIME_LIMIT] = addMugenTextMugenStyle("Time Limit", Vector3D(offsetX, startY + offsetY * 2, 45), Vector3DI(gOptionsScreenData.mTextFontID, 8, 1));
	gOptionsScreenData.mGeneral.mSettingTextID[GENERAL_SETTING_TIME_LIMIT] = addMugenTextMugenStyle("99", Vector3D(320 - offsetX, startY + offsetY * 2, 45), Vector3DI(gOptionsScreenData.mTextFontID, 8, -1));
	setTimeLimitOptionText();

	gOptionsScreenData.mGeneral.mSelectableTextID[GENERAL_SETTING_GAME_SPEED] = addMugenTextMugenStyle("Game Speed", Vector3D(offsetX, startY + offsetY * 3, 45), Vector3DI(gOptionsScreenData.mTextFontID, 8, 1));
	gOptionsScreenData.mGeneral.mSettingTextID[GENERAL_SETTING_GAME_SPEED] = addMugenTextMugenStyle("Normal", Vector3D(320 - offsetX, startY + offsetY * 3, 45), Vector3DI(gOptionsScreenData.mTextFontID, 8, -1));
	setGameSpeedOptionText();

	gOptionsScreenData.mGeneral.mSelectableTextID[GENERAL_SETTING_WAV_VOLUME] = addMugenTextMugenStyle("Wav Volume", Vector3D(offsetX, startY + offsetY * 4, 45), Vector3DI(gOptionsScreenData.mTextFontID, 8, 1));
	gOptionsScreenData.mGeneral.mSettingTextID[GENERAL_SETTING_WAV_VOLUME] = addMugenTextMugenStyle("50", Vector3D(320 - offsetX, startY + offsetY * 4, 45), Vector3DI(gOptionsScreenData.mTextFontID, 8, -1));
	setVolumeOptionText(GENERAL_SETTING_WAV_VOLUME, getUnscaledGameWavVolume);

	gOptionsScreenData.mGeneral.mSelectableTextID[GENERAL_SETTING_MIDI_VOLUME] = addMugenTextMugenStyle("Midi Volume", Vector3D(offsetX, startY + offsetY * 5, 45), Vector3DI(gOptionsScreenData.mTextFontID, 8, 1));
	gOptionsScreenData.mGeneral.mSettingTextID[GENERAL_SETTING_MIDI_VOLUME] = addMugenTextMugenStyle("50", Vector3D(320 - offsetX, startY + offsetY * 5, 45), Vector3DI(gOptionsScreenData.mTextFontID, 8, -1));
	setVolumeOptionText(GENERAL_SETTING_MIDI_VOLUME, getUnscaledGameMidiVolume);

	gOptionsScreenData.mGeneral.mSelectableTextID[GENERAL_SETTING_INPUT_CONFIG] = addMugenTextMugenStyle("Input Config", Vector3D(offsetX, startY + offsetY * 6, 45), Vector3DI(gOptionsScreenData.mTextFontID, 8, 1));
	if (!isUsingController()) {
		gOptionsScreenData.mGeneral.mSettingTextID[GENERAL_SETTING_INPUT_CONFIG] = addMugenTextMugenStyle("(F1)", Vector3D(320 - offsetX, startY + offsetY * 6, 45), Vector3DI(gOptionsScreenData.mTextFontID, 5, -1));
	}
	else {
		gOptionsScreenData.mGeneral.mSettingTextID[GENERAL_SETTING_INPUT_CONFIG] = addMugenTextMugenStyle(gButtonShortCutStrings[2].c_str(), Vector3D(320 - offsetX, startY + offsetY * 6, 45), Vector3DI(gOptionsScreenData.mTextFontID, 5, -1));
	}
	gOptionsScreenData.mGeneral.mSelectableTextID[GENERAL_SETTING_TEAM_MODE_CONFIG] = addMugenTextMugenStyle("Team Mode Config", Vector3D(offsetX, startY + offsetY * 7, 45), Vector3DI(gOptionsScreenData.mTextFontID, 8, 1));
	if (!isUsingController()) {
		gOptionsScreenData.mGeneral.mSettingTextID[GENERAL_SETTING_TEAM_MODE_CONFIG] = addMugenTextMugenStyle("(F2)", Vector3D(320 - offsetX, startY + offsetY * 7, 45), Vector3DI(gOptionsScreenData.mTextFontID, 5, -1));
	}
	else {
		gOptionsScreenData.mGeneral.mSettingTextID[GENERAL_SETTING_TEAM_MODE_CONFIG] = addMugenTextMugenStyle(gButtonShortCutStrings[3].c_str(), Vector3D(320 - offsetX, startY + offsetY * 7, 45), Vector3DI(gOptionsScreenData.mTextFontID, 2, -1));
	}
	gOptionsScreenData.mGeneral.mSelectableTextID[GENERAL_SETTING_LOAD_SAVE_OPTIONS] = addMugenTextMugenStyle("Load/Save Options", Vector3D(offsetX, startY + offsetY * 9, 45), Vector3DI(gOptionsScreenData.mTextFontID, 8, 1));
	if (isOnDreamcast()) {
		gOptionsScreenData.mGeneral.mSettingTextID[GENERAL_SETTING_LOAD_SAVE_OPTIONS] = addMugenTextMugenStyle("", Vector3D(320 - offsetX, startY + offsetY * 9, 45), Vector3DI(gOptionsScreenData.mTextFontID, 8, -1));
	}
	else {
		gOptionsScreenData.mGeneral.mSettingTextID[GENERAL_SETTING_LOAD_SAVE_OPTIONS] = addMugenTextMugenStyle("Load", Vector3D(320 - offsetX, startY + offsetY * 9, 45), Vector3DI(gOptionsScreenData.mTextFontID, 8, -1));
	}

	gOptionsScreenData.mGeneral.mSelectableTextID[GENERAL_SETTING_LOAD_SAVE_VARS] = addMugenTextMugenStyle("Load/Save Global Vars", Vector3D(offsetX, startY + offsetY * 10, 45), Vector3DI(gOptionsScreenData.mTextFontID, 8, 1));
	if (isOnDreamcast()) {
		gOptionsScreenData.mGeneral.mSettingTextID[GENERAL_SETTING_LOAD_SAVE_VARS] = addMugenTextMugenStyle("", Vector3D(320 - offsetX, startY + offsetY * 10, 45), Vector3DI(gOptionsScreenData.mTextFontID, 8, -1));
	}
	else {
		gOptionsScreenData.mGeneral.mSettingTextID[GENERAL_SETTING_LOAD_SAVE_VARS] = addMugenTextMugenStyle("Load", Vector3D(320 - offsetX, startY + offsetY * 10, 45), Vector3DI(gOptionsScreenData.mTextFontID, 8, -1));
	}

	gOptionsScreenData.mGeneral.mSelectableTextID[GENERAL_SETTING_DEFAULT_VALUES] = addMugenTextMugenStyle("Default Values", Vector3D(offsetX, startY + offsetY * 11, 45), Vector3DI(gOptionsScreenData.mTextFontID, 8, 1));
	gOptionsScreenData.mGeneral.mSettingTextID[GENERAL_SETTING_DEFAULT_VALUES] = addMugenTextMugenStyle("", Vector3D(320 - offsetX, startY + offsetY * 11, 45), Vector3DI(gOptionsScreenData.mTextFontID, 8, -1));

	gOptionsScreenData.mGeneral.mSelectableTextID[GENERAL_SETTING_RETURN] = addMugenTextMugenStyle("Return to Main Menu", Vector3D(offsetX, startY + offsetY * 13, 45), Vector3DI(gOptionsScreenData.mTextFontID, 8, 1));
	if (!isUsingController()) {
		gOptionsScreenData.mGeneral.mSettingTextID[GENERAL_SETTING_RETURN] = addMugenTextMugenStyle("(Esc)", Vector3D(320 - offsetX, startY + offsetY * 13, 45), Vector3DI(gOptionsScreenData.mTextFontID, 5, -1));
	}
	else {
		gOptionsScreenData.mGeneral.mSettingTextID[GENERAL_SETTING_RETURN] = addMugenTextMugenStyle(gButtonShortCutStrings[1].c_str(), Vector3D(320 - offsetX, startY + offsetY * 13, 45), Vector3DI(gOptionsScreenData.mTextFontID, 3, -1));
	}
	gOptionsScreenData.mGeneral.mBoxCursorID = addBoxCursor(Vector3D(0, 0, 0), Vector3D(0, 0, 43), GeoRectangle2D(-6, -11, 320 - 2 * offsetX + 10, 14));

	setSelectedGeneralOptionActive();
}

static void unloadGeneralOptionsScreen() {
	for (int i = 0; i < GENERAL_SETTING_AMOUNT; i++) {
		removeMugenText(gOptionsScreenData.mGeneral.mSelectableTextID[i]);
		removeMugenText(gOptionsScreenData.mGeneral.mSettingTextID[i]);
	}

	removeBoxCursor(gOptionsScreenData.mGeneral.mBoxCursorID);
}

static void setSelectedInputConfigOptionActive(int i) {

	setMugenTextColor(gOptionsScreenData.mInputConfig.mSelectableTextID[i][gOptionsScreenData.mInputConfig.mSelected[i]], getMugenTextColorFromMugenTextColorIndex(0));

	Vector3D selectionColor = getMugenTextColor(gOptionsScreenData.mInputConfig.mSettingTextID[i][gOptionsScreenData.mInputConfig.mSelected[i]]);
	if (selectionColor.x != 0 && selectionColor.y != 0 && selectionColor.z != 0) {
		setMugenTextColor(gOptionsScreenData.mInputConfig.mSettingTextID[i][gOptionsScreenData.mInputConfig.mSelected[i]], getMugenTextColorFromMugenTextColorIndex(0));
	}

	Position pos = getMugenTextPosition(gOptionsScreenData.mInputConfig.mSelectableTextID[i][gOptionsScreenData.mInputConfig.mSelected[i]]);
	pos.z = 0;
	setBoxCursorPosition(gOptionsScreenData.mInputConfig.mBoxCursorID[i], pos);
}

static void setSelectedInputConfigOptionsActive() {
	for (int i = 0; i < 2; i++) {
		setSelectedInputConfigOptionActive(i);
	}
}

static void setSelectedInputConfigOptionInactive(int i) {
	setMugenTextColor(gOptionsScreenData.mInputConfig.mSelectableTextID[i][gOptionsScreenData.mInputConfig.mSelected[i]], getMugenTextColorFromMugenTextColorIndex(8));

	Vector3D selectionColor = getMugenTextColor(gOptionsScreenData.mInputConfig.mSettingTextID[i][gOptionsScreenData.mInputConfig.mSelected[i]]);
	if (selectionColor.x != 0 && selectionColor.y != 0 && selectionColor.z != 0) {
		setMugenTextColor(gOptionsScreenData.mInputConfig.mSettingTextID[i][gOptionsScreenData.mInputConfig.mSelected[i]], getMugenTextColorFromMugenTextColorIndex(8));
	}
}

static void loadInputConfigOptionsScreen() {
	setAnimationPosition(gOptionsScreenData.mBackgroundAnimationElement, Vector3D(13, 35, 40));
	setAnimationSize(gOptionsScreenData.mBackgroundAnimationElement, Vector3D(292, 121, 1), Vector3D(0, 0, 0));

	changeMugenText(gOptionsScreenData.mHeaderTextID, "INPUT CONFIG");
	setMugenTextPosition(gOptionsScreenData.mHeaderTextID, Vector3D(160, 20, 45));
	gOptionsScreenData.mActiveScreen = OPTION_SCREEN_INPUT_CONFIG;

	gOptionsScreenData.mInputConfig.mPlayerTextID[0] = addMugenTextMugenStyle("PLAYER 1", Vector3D(80, 48, 45), Vector3DI(gOptionsScreenData.mTextFontID, 4, 0));
	gOptionsScreenData.mInputConfig.mPlayerTextID[1] = addMugenTextMugenStyle("PLAYER 2", Vector3D(240, 48, 45), Vector3DI(gOptionsScreenData.mTextFontID, 1, 0));


	int left = 0;
	int right = 160;
	int offsetX = 22;
	int startY = 64;
	int offsetY = 13;
	for (int i = 0; i < 2; i++) {

		gOptionsScreenData.mInputConfig.mSelectableTextID[i][INPUT_CONFIG_SETTING_KEY_CONFIG] = addMugenTextMugenStyle("Key Config", Vector3D(left + offsetX, startY, 45), Vector3DI(gOptionsScreenData.mTextFontID, 8, 1));
		if (!i) {
			gOptionsScreenData.mInputConfig.mSettingTextID[i][INPUT_CONFIG_SETTING_KEY_CONFIG] = addMugenTextMugenStyle("", Vector3D(right - offsetX, startY, 45), Vector3DI(gOptionsScreenData.mTextFontID, 8, -1));
		}
		else {
			if (!isUsingController()) {
				gOptionsScreenData.mInputConfig.mSettingTextID[i][INPUT_CONFIG_SETTING_KEY_CONFIG] = addMugenTextMugenStyle("(F1)", Vector3D(right - offsetX, startY, 45), Vector3DI(gOptionsScreenData.mTextFontID, 5, -1));
			}
			else {
				gOptionsScreenData.mInputConfig.mSettingTextID[i][INPUT_CONFIG_SETTING_KEY_CONFIG] = addMugenTextMugenStyle(gButtonShortCutStrings[2].c_str(), Vector3D(right - offsetX, startY, 45), Vector3DI(gOptionsScreenData.mTextFontID, 5, -1));
			}
		}
		gOptionsScreenData.mInputConfig.mSelectableTextID[i][INPUT_CONFIG_SETTING_JOYSTICK_TYPE] = addMugenTextMugenStyle("Joystick Type", Vector3D(left + offsetX, startY + offsetY * 2, 45), Vector3DI(gOptionsScreenData.mTextFontID, 8, 1));
		gOptionsScreenData.mInputConfig.mSettingTextID[i][INPUT_CONFIG_SETTING_JOYSTICK_TYPE] = addMugenTextMugenStyle("Disabled", Vector3D(right - offsetX, startY + offsetY * 2, 45), Vector3DI(gOptionsScreenData.mTextFontID, 8, -1));

		gOptionsScreenData.mInputConfig.mSelectableTextID[i][INPUT_CONFIG_SETTING_JOYSTICK_CONFIG] = addMugenTextMugenStyle("Joystick Config", Vector3D(left + offsetX, startY + offsetY * 3, 45), Vector3DI(gOptionsScreenData.mTextFontID, 8, 1));
		if (!i) {
			gOptionsScreenData.mInputConfig.mSettingTextID[i][INPUT_CONFIG_SETTING_JOYSTICK_CONFIG] = addMugenTextMugenStyle("", Vector3D(right - offsetX, startY + offsetY * 3, 45), Vector3DI(gOptionsScreenData.mTextFontID, 8, -1));
		}
		else {
			if (!isUsingController()) {
				gOptionsScreenData.mInputConfig.mSettingTextID[i][INPUT_CONFIG_SETTING_JOYSTICK_CONFIG] = addMugenTextMugenStyle("(F2)", Vector3D(right - offsetX, startY + offsetY * 3, 45), Vector3DI(gOptionsScreenData.mTextFontID, 5, -1));
			}
			else {
				gOptionsScreenData.mInputConfig.mSettingTextID[i][INPUT_CONFIG_SETTING_JOYSTICK_CONFIG] = addMugenTextMugenStyle(gButtonShortCutStrings[3].c_str(), Vector3D(right - offsetX, startY + offsetY * 3, 45), Vector3DI(gOptionsScreenData.mTextFontID, 2, -1));
			}
		}
		gOptionsScreenData.mInputConfig.mSelectableTextID[i][INPUT_CONFIG_SETTING_DEFAULT_VALUES] = addMugenTextMugenStyle("Default Values", Vector3D(left + offsetX, startY + offsetY * 5, 45), Vector3DI(gOptionsScreenData.mTextFontID, 8, 1));
		gOptionsScreenData.mInputConfig.mSettingTextID[i][INPUT_CONFIG_SETTING_DEFAULT_VALUES] = addMugenTextMugenStyle("", Vector3D(right - offsetX, startY + offsetY * 5, 45), Vector3DI(gOptionsScreenData.mTextFontID, 8, -1));

		gOptionsScreenData.mInputConfig.mSelectableTextID[i][INPUT_CONFIG_SETTING_RETURN] = addMugenTextMugenStyle("Return to Options", Vector3D(left + offsetX, startY + offsetY * 6, 45), Vector3DI(gOptionsScreenData.mTextFontID, 8, 1));
		
		if (!i) {
			gOptionsScreenData.mInputConfig.mSettingTextID[i][INPUT_CONFIG_SETTING_RETURN] = addMugenTextMugenStyle("", Vector3D(right - offsetX, startY + offsetY * 6, 45), Vector3DI(gOptionsScreenData.mTextFontID, 8, -1));
		}
		else {
			if (!isUsingController()) {
				gOptionsScreenData.mInputConfig.mSettingTextID[i][INPUT_CONFIG_SETTING_RETURN] = addMugenTextMugenStyle("", Vector3D(right - offsetX, startY + offsetY * 6, 45), Vector3DI(gOptionsScreenData.mTextFontID, 5, -1));
			}
			else {
				gOptionsScreenData.mInputConfig.mSettingTextID[i][INPUT_CONFIG_SETTING_RETURN] = addMugenTextMugenStyle(gButtonShortCutStrings[1].c_str(), Vector3D(right - offsetX, startY + offsetY * 6, 45), Vector3DI(gOptionsScreenData.mTextFontID, 3, -1));
			}
		}
		gOptionsScreenData.mInputConfig.mBoxCursorID[i] = addBoxCursor(Vector3D(0, 0, 0), Vector3D(0, 0, 43), GeoRectangle2D(-6, -11, 160 - 2 * offsetX + 10, 14));

		left = 160;
		right = 320;
	}

	
	setSelectedInputConfigOptionsActive();
}

static void unloadInputConfigOptionsScreen() {
	for (int i = 0; i < 2; i++) {
		removeMugenText(gOptionsScreenData.mInputConfig.mPlayerTextID[i]);

		for (int j = 0; j < INPUT_CONFIG_SETTING_AMOUNT; j++) {
			removeMugenText(gOptionsScreenData.mInputConfig.mSelectableTextID[i][j]);
			removeMugenText(gOptionsScreenData.mInputConfig.mSettingTextID[i][j]);
		}
		removeBoxCursor(gOptionsScreenData.mInputConfig.mBoxCursorID[i]);
	}

}

static void setSelectedKeyConfigOptionActive() {

	int selectableID; 
	if(gOptionsScreenData.mKeyConfig.mSelected.y == KEY_CONFIG_SETTING_EXIT) selectableID = gOptionsScreenData.mKeyConfig.mSelectableTextID[0][gOptionsScreenData.mKeyConfig.mSelected.y];
	else selectableID = gOptionsScreenData.mKeyConfig.mSelectableTextID[gOptionsScreenData.mKeyConfig.mSelected.x][gOptionsScreenData.mKeyConfig.mSelected.y];

	setMugenTextColor(selectableID, getMugenTextColorFromMugenTextColorIndex(0));

	Vector3D selectionColor = getMugenTextColor(gOptionsScreenData.mKeyConfig.mSettingTextID[gOptionsScreenData.mKeyConfig.mSelected.x][gOptionsScreenData.mKeyConfig.mSelected.y]);
	if (selectionColor.x != 0 && selectionColor.y != 0 && selectionColor.z != 0) {
		setMugenTextColor(gOptionsScreenData.mKeyConfig.mSettingTextID[gOptionsScreenData.mKeyConfig.mSelected.x][gOptionsScreenData.mKeyConfig.mSelected.y], getMugenTextColorFromMugenTextColorIndex(0));
	}

	Position pos = getMugenTextPosition(selectableID);
	pos.z = 0;
	setBoxCursorPosition(gOptionsScreenData.mKeyConfig.mBoxCursorID, pos);
}

static void setSelectedKeyConfigOptionInactive() {

	int selectableID;
	if (gOptionsScreenData.mKeyConfig.mSelected.y == KEY_CONFIG_SETTING_EXIT) selectableID = gOptionsScreenData.mKeyConfig.mSelectableTextID[0][gOptionsScreenData.mKeyConfig.mSelected.y];
	else selectableID = gOptionsScreenData.mKeyConfig.mSelectableTextID[gOptionsScreenData.mKeyConfig.mSelected.x][gOptionsScreenData.mKeyConfig.mSelected.y];
	setMugenTextColor(selectableID, getMugenTextColorFromMugenTextColorIndex(8));

	Vector3D selectionColor = getMugenTextColor(gOptionsScreenData.mKeyConfig.mSettingTextID[gOptionsScreenData.mKeyConfig.mSelected.x][gOptionsScreenData.mKeyConfig.mSelected.y]);
	if (selectionColor.x != 0 && selectionColor.y != 0 && selectionColor.z != 0) {
		setMugenTextColor(gOptionsScreenData.mKeyConfig.mSettingTextID[gOptionsScreenData.mKeyConfig.mSelected.x][gOptionsScreenData.mKeyConfig.mSelected.y], getMugenTextColorFromMugenTextColorIndex(8));
	}
}

static string gKeyNames[KEYBOARD_AMOUNT_PRISM] = {
	"A",
	"B",
	"C",
	"D",
	"E",
	"F",
	"G",
	"H",
	"I",
	"J",
	"K",
	"L",
	"M",
	"N",
	"O",
	"P",
	"Q",
	"R",
	"S",
	"T",
	"U",
	"V",
	"W",
	"X",
	"Y",
	"Z",
	"0",
	"1",
	"2",
	"3",
	"4",
	"5",
	"6",
	"7",
	"8",
	"9",
	"Space",
	"Left",
	"Right",
	"Up",
	"Down",
	"F1",
	"F2",
	"F3",
	"F4",
	"F5",
	"F6",
	"F7",
	"F8",
	"F9",
	"F10",
	"F11",
	"F12",
	"ScrollLock",
	"Pause",
	"Caret",
	"LeftCtrl",
	"LeftAlt",
	"LeftShift",
	"Enter",
	"Backspace",
	"Delete",
	".",
	"/",
};

#ifdef VITA
static string gButtonNames[11] = {
	"X",
	"O",
	"Square",
	"Triangle",
	"L",
	"R",
	"Left",
	"Right",
	"Up",
	"Down",
	"Start",
};
#else
static string gButtonNames[11] = {
	"A",
	"B",
	"X",
	"Y",
	"L",
	"R",
	"Left",
	"Right",
	"Up",
	"Down",
	"Start",
};
#endif

static void loadKeyConfigOptionsScreen() {
	setAnimationPosition(gOptionsScreenData.mBackgroundAnimationElement, Vector3D(33, 15, 40));
	setAnimationSize(gOptionsScreenData.mBackgroundAnimationElement, Vector3D(252, 221, 1), Vector3D(0, 0, 0));

	changeMugenText(gOptionsScreenData.mHeaderTextID, "KEY CONFIG");
	setMugenTextPosition(gOptionsScreenData.mHeaderTextID, Vector3D(161, 11, 45));
	gOptionsScreenData.mActiveScreen = OPTION_SCREEN_KEY_CONFIG;

	gOptionsScreenData.mKeyConfig.mPlayerTextID[0] = addMugenTextMugenStyle("PLAYER 1", Vector3D(91, 28, 45), Vector3DI(gOptionsScreenData.mTextFontID, 4, 0));
	gOptionsScreenData.mKeyConfig.mPlayerTextID[1] = addMugenTextMugenStyle("PLAYER 2", Vector3D(230, 28, 45), Vector3DI(gOptionsScreenData.mTextFontID, 1, 0));


	int left = 38;
	int right = 142;
	int offsetX = 11;
	int startY = 44;
	int offsetY = 13;
	for (int i = 0; i < 2; i++) {

		int usingController = isUsingController();

		gOptionsScreenData.mKeyConfig.mSelectableTextID[i][KEY_CONFIG_SETTING_CONFIG_ALL] = addMugenTextMugenStyle("Config all", Vector3D(left + offsetX, startY, 45), Vector3DI(gOptionsScreenData.mTextFontID, 8, 1));
		if (!usingController) {
			string ftext = i ? "(F2)" : "(F1)";
			gOptionsScreenData.mKeyConfig.mSettingTextID[i][KEY_CONFIG_SETTING_CONFIG_ALL] = addMugenTextMugenStyle(ftext.data(), Vector3D(right - offsetX, startY, 45), Vector3DI(gOptionsScreenData.mTextFontID, 5, -1));
		}
		else if (!i) {
			gOptionsScreenData.mKeyConfig.mSettingTextID[i][KEY_CONFIG_SETTING_CONFIG_ALL] = addMugenTextMugenStyle(gButtonShortCutStrings[2].c_str(), Vector3D(right - offsetX, startY, 45), Vector3DI(gOptionsScreenData.mTextFontID, 5, -1));
		}
		else {
			gOptionsScreenData.mKeyConfig.mSettingTextID[i][KEY_CONFIG_SETTING_CONFIG_ALL] = addMugenTextMugenStyle(gButtonShortCutStrings[3].c_str(), Vector3D(right - offsetX, startY, 45), Vector3DI(gOptionsScreenData.mTextFontID, 2, -1));
		}
		ControllerButtonPrism button = CONTROLLER_UP_PRISM;
		string valueText = usingController ? gButtonNames[getButtonForController(i, button)] : gKeyNames[getButtonForKeyboard(i, button)];
		gOptionsScreenData.mKeyConfig.mSelectableTextID[i][KEY_CONFIG_SETTING_UP] = addMugenTextMugenStyle("Up", Vector3D(left + offsetX, startY + offsetY * 1, 45), Vector3DI(gOptionsScreenData.mTextFontID, 8, 1));
		gOptionsScreenData.mKeyConfig.mSettingTextID[i][KEY_CONFIG_SETTING_UP] = addMugenTextMugenStyle(valueText.data(), Vector3D(right - offsetX, startY + offsetY * 1, 45), Vector3DI(gOptionsScreenData.mTextFontID, 8, -1));

		button = CONTROLLER_DOWN_PRISM;
		valueText = usingController ? gButtonNames[getButtonForController(i, button)] : gKeyNames[getButtonForKeyboard(i, button)];
		gOptionsScreenData.mKeyConfig.mSelectableTextID[i][KEY_CONFIG_SETTING_DOWN] = addMugenTextMugenStyle("Down", Vector3D(left + offsetX, startY + offsetY * 2, 45), Vector3DI(gOptionsScreenData.mTextFontID, 8, 1));
		gOptionsScreenData.mKeyConfig.mSettingTextID[i][KEY_CONFIG_SETTING_DOWN] = addMugenTextMugenStyle(valueText.data(), Vector3D(right - offsetX, startY + offsetY * 2, 45), Vector3DI(gOptionsScreenData.mTextFontID, 8, -1));

		button = CONTROLLER_LEFT_PRISM;
		valueText = usingController ? gButtonNames[getButtonForController(i, button)] : gKeyNames[getButtonForKeyboard(i, button)];
		gOptionsScreenData.mKeyConfig.mSelectableTextID[i][KEY_CONFIG_SETTING_LEFT] = addMugenTextMugenStyle("Left", Vector3D(left + offsetX, startY + offsetY * 3, 45), Vector3DI(gOptionsScreenData.mTextFontID, 8, 1));
		gOptionsScreenData.mKeyConfig.mSettingTextID[i][KEY_CONFIG_SETTING_LEFT] = addMugenTextMugenStyle(valueText.data(), Vector3D(right - offsetX, startY + offsetY * 3, 45), Vector3DI(gOptionsScreenData.mTextFontID, 8, -1));

		button = CONTROLLER_RIGHT_PRISM;
		valueText = usingController ? gButtonNames[getButtonForController(i, button)] : gKeyNames[getButtonForKeyboard(i, button)];
		gOptionsScreenData.mKeyConfig.mSelectableTextID[i][KEY_CONFIG_SETTING_RIGHT] = addMugenTextMugenStyle("Right", Vector3D(left + offsetX, startY + offsetY * 4, 45), Vector3DI(gOptionsScreenData.mTextFontID, 8, 1));
		gOptionsScreenData.mKeyConfig.mSettingTextID[i][KEY_CONFIG_SETTING_RIGHT] = addMugenTextMugenStyle(valueText.data(), Vector3D(right - offsetX, startY + offsetY * 4, 45), Vector3DI(gOptionsScreenData.mTextFontID, 8, -1));

		button = CONTROLLER_A_PRISM;
		valueText = usingController ? gButtonNames[getButtonForController(i, button)] : gKeyNames[getButtonForKeyboard(i, button)];
		gOptionsScreenData.mKeyConfig.mSelectableTextID[i][KEY_CONFIG_SETTING_A] = addMugenTextMugenStyle("A", Vector3D(left + offsetX, startY + offsetY * 5, 45), Vector3DI(gOptionsScreenData.mTextFontID, 8, 1));
		gOptionsScreenData.mKeyConfig.mSettingTextID[i][KEY_CONFIG_SETTING_A] = addMugenTextMugenStyle(valueText.data(), Vector3D(right - offsetX, startY + offsetY * 5, 45), Vector3DI(gOptionsScreenData.mTextFontID, 8, -1));

		button = CONTROLLER_B_PRISM;
		valueText = usingController ? gButtonNames[getButtonForController(i, button)] : gKeyNames[getButtonForKeyboard(i, button)];
		gOptionsScreenData.mKeyConfig.mSelectableTextID[i][KEY_CONFIG_SETTING_B] = addMugenTextMugenStyle("B", Vector3D(left + offsetX, startY + offsetY * 6, 45), Vector3DI(gOptionsScreenData.mTextFontID, 8, 1));
		gOptionsScreenData.mKeyConfig.mSettingTextID[i][KEY_CONFIG_SETTING_B] = addMugenTextMugenStyle(valueText.data(), Vector3D(right - offsetX, startY + offsetY * 6, 45), Vector3DI(gOptionsScreenData.mTextFontID, 8, -1));

		button = CONTROLLER_R_PRISM;
		valueText = usingController ? gButtonNames[getButtonForController(i, button)] : gKeyNames[getButtonForKeyboard(i, button)];
		gOptionsScreenData.mKeyConfig.mSelectableTextID[i][KEY_CONFIG_SETTING_C] = addMugenTextMugenStyle("C", Vector3D(left + offsetX, startY + offsetY * 7, 45), Vector3DI(gOptionsScreenData.mTextFontID, 8, 1));
		gOptionsScreenData.mKeyConfig.mSettingTextID[i][KEY_CONFIG_SETTING_C] = addMugenTextMugenStyle(valueText.data(), Vector3D(right - offsetX, startY + offsetY * 7, 45), Vector3DI(gOptionsScreenData.mTextFontID, 8, -1));

		button = CONTROLLER_X_PRISM;
		valueText = usingController ? gButtonNames[getButtonForController(i, button)] : gKeyNames[getButtonForKeyboard(i, button)];
		gOptionsScreenData.mKeyConfig.mSelectableTextID[i][KEY_CONFIG_SETTING_X] = addMugenTextMugenStyle("X", Vector3D(left + offsetX, startY + offsetY * 8, 45), Vector3DI(gOptionsScreenData.mTextFontID, 8, 1));
		gOptionsScreenData.mKeyConfig.mSettingTextID[i][KEY_CONFIG_SETTING_X] = addMugenTextMugenStyle(valueText.data(), Vector3D(right - offsetX, startY + offsetY * 8, 45), Vector3DI(gOptionsScreenData.mTextFontID, 8, -1));

		button = CONTROLLER_Y_PRISM;
		valueText = usingController ? gButtonNames[getButtonForController(i, button)] : gKeyNames[getButtonForKeyboard(i, button)];
		gOptionsScreenData.mKeyConfig.mSelectableTextID[i][KEY_CONFIG_SETTING_Y] = addMugenTextMugenStyle("Y", Vector3D(left + offsetX, startY + offsetY * 9, 45), Vector3DI(gOptionsScreenData.mTextFontID, 8, 1));
		gOptionsScreenData.mKeyConfig.mSettingTextID[i][KEY_CONFIG_SETTING_Y] = addMugenTextMugenStyle(valueText.data(), Vector3D(right - offsetX, startY + offsetY * 9, 45), Vector3DI(gOptionsScreenData.mTextFontID, 8, -1));

		button = CONTROLLER_L_PRISM;
		valueText = usingController ? gButtonNames[getButtonForController(i, button)] : gKeyNames[getButtonForKeyboard(i, button)];
		gOptionsScreenData.mKeyConfig.mSelectableTextID[i][KEY_CONFIG_SETTING_Z] = addMugenTextMugenStyle("Z", Vector3D(left + offsetX, startY + offsetY * 10, 45), Vector3DI(gOptionsScreenData.mTextFontID, 8, 1));
		gOptionsScreenData.mKeyConfig.mSettingTextID[i][KEY_CONFIG_SETTING_Z] = addMugenTextMugenStyle(valueText.data(), Vector3D(right - offsetX, startY + offsetY * 10, 45), Vector3DI(gOptionsScreenData.mTextFontID, 8, -1));

		button = CONTROLLER_START_PRISM;
		valueText = usingController ? gButtonNames[getButtonForController(i, button)] : gKeyNames[getButtonForKeyboard(i, button)];
		gOptionsScreenData.mKeyConfig.mSelectableTextID[i][KEY_CONFIG_SETTING_START] = addMugenTextMugenStyle("Start", Vector3D(left + offsetX, startY + offsetY * 11, 45), Vector3DI(gOptionsScreenData.mTextFontID, 8, 1));
		gOptionsScreenData.mKeyConfig.mSettingTextID[i][KEY_CONFIG_SETTING_START] = addMugenTextMugenStyle(valueText.data(), Vector3D(right - offsetX, startY + offsetY * 11, 45), Vector3DI(gOptionsScreenData.mTextFontID, 8, -1));

		gOptionsScreenData.mKeyConfig.mSelectableTextID[i][KEY_CONFIG_SETTING_DEFAULT] = addMugenTextMugenStyle("Default", Vector3D(left + offsetX, startY + offsetY * 13, 45), Vector3DI(gOptionsScreenData.mTextFontID, 8, 1));

		if (!usingController) {
			string ftext = i ? "(F4)" : "(F3)";
			gOptionsScreenData.mKeyConfig.mSettingTextID[i][KEY_CONFIG_SETTING_DEFAULT] = addMugenTextMugenStyle(ftext.data(), Vector3D(right - offsetX, startY + offsetY * 13, 45), Vector3DI(gOptionsScreenData.mTextFontID, 5, -1));
		}
		else {
			string ftext = i ? gButtonShortCutStrings[4] : gButtonShortCutStrings[5];
			gOptionsScreenData.mKeyConfig.mSettingTextID[i][KEY_CONFIG_SETTING_DEFAULT] = addMugenTextMugenStyle(ftext.data(), Vector3D(right - offsetX, startY + offsetY * 13, 45), Vector3DI(gOptionsScreenData.mTextFontID, 0, -1));
		}

		if (i == 0) {
			gOptionsScreenData.mKeyConfig.mSelectableTextID[i][KEY_CONFIG_SETTING_EXIT] = addMugenTextMugenStyle("Exit", Vector3D(left + offsetX, startY + offsetY * 14, 45), Vector3DI(gOptionsScreenData.mTextFontID, 8, 1));
			if (!usingController) {
				gOptionsScreenData.mKeyConfig.mSettingTextID[i][KEY_CONFIG_SETTING_EXIT] = addMugenTextMugenStyle("", Vector3D(right - offsetX, startY + offsetY * 14, 45), Vector3DI(gOptionsScreenData.mTextFontID, 5, -1));
			}
			else {
				gOptionsScreenData.mKeyConfig.mSettingTextID[i][KEY_CONFIG_SETTING_EXIT] = addMugenTextMugenStyle(gButtonShortCutStrings[1].c_str(), Vector3D(right - offsetX, startY + offsetY * 14, 45), Vector3DI(gOptionsScreenData.mTextFontID, 3, -1));
			}
		}
		else {
			left = 38;
			right = 142;
			gOptionsScreenData.mKeyConfig.mSelectableTextID[i][KEY_CONFIG_SETTING_EXIT] = addMugenTextMugenStyle("", Vector3D(left + offsetX, startY + offsetY * 14, 45), Vector3DI(gOptionsScreenData.mTextFontID, 8, 1));
			gOptionsScreenData.mKeyConfig.mSettingTextID[i][KEY_CONFIG_SETTING_EXIT] = addMugenTextMugenStyle("", Vector3D(right - offsetX, startY + offsetY * 14, 45), Vector3DI(gOptionsScreenData.mTextFontID, 5, -1));
		}
		left = 178;
		right = 282;
	}

	gOptionsScreenData.mKeyConfig.mBoxCursorID = addBoxCursor(Vector3D(0, 0, 0), Vector3D(0, 0, 43), GeoRectangle2D(-5, -11, 92, 14));
	gOptionsScreenData.mKeyConfig.mIsSettingInput = 0;
	gOptionsScreenData.mKeyConfig.mSettingFlankDone = 1;

	setSelectedKeyConfigOptionActive();
}

static void unloadKeyConfigOptionsScreen() {
	for (int i = 0; i < 2; i++) {
		removeMugenText(gOptionsScreenData.mKeyConfig.mPlayerTextID[i]);

		for (int j = 0; j < KEY_CONFIG_SETTING_AMOUNT; j++) {
			removeMugenText(gOptionsScreenData.mKeyConfig.mSelectableTextID[i][j]);
			removeMugenText(gOptionsScreenData.mKeyConfig.mSettingTextID[i][j]);
		}
	}

	removeBoxCursor(gOptionsScreenData.mKeyConfig.mBoxCursorID);
}

static void setDreamcastSaveSelectedVMUText() {
	changeMugenText(gOptionsScreenData.mDreamcastSave.mSettingTextID[DREAMCAST_SAVE_SETTING_SELECT_VMU], gVMUNames[uint32_t(gOptionsScreenData.mDreamcastSave.mSelectedVMU)].c_str());
}

static void setDreamcastFreeBlocksText() {
	if (!isPrismSaveSlotActive(gOptionsScreenData.mDreamcastSave.mSelectedVMU)) {
		changeMugenText(gOptionsScreenData.mDreamcastSave.mFreeBlocksTextID, "No VMU detected in slot");
		return;
	}

	if (gOptionsScreenData.mDreamcastSave.mSaveLoadType == GENERAL_SETTING_LOAD_SAVE_OPTIONS) {
		gOptionsScreenData.mDreamcastSave.mHasSave = hasMugenConfigSave(gOptionsScreenData.mDreamcastSave.mSelectedVMU);
	}
	else {
		gOptionsScreenData.mDreamcastSave.mHasSave = hasGlobalVariablesSave(gOptionsScreenData.mDreamcastSave.mSelectedVMU);
	}

	const auto freeSize = getAvailableSizeForSaveSlot(gOptionsScreenData.mDreamcastSave.mSelectedVMU);
	const auto blockAmount = freeSize / 512;
	auto s = std::string("Free blocks: ").append(std::to_string(blockAmount));
	if (gOptionsScreenData.mDreamcastSave.mHasSave) {
		s = s.append(" (save exists)");
	}
	changeMugenText(gOptionsScreenData.mDreamcastSave.mFreeBlocksTextID, s.c_str());
}

static void setDreamcastSaveSaveSizeText() {
	size_t saveSize;
	if (gOptionsScreenData.mDreamcastSave.mSaveLoadType == GENERAL_SETTING_LOAD_SAVE_OPTIONS) {
		saveSize = getMugenConfigSaveSize();
	}
	else {
		saveSize = getGlobalVariablesSaveSize();
	}

	const auto blockAmount = saveSize / 512;
	const auto s = std::string("Save blocks: ").append(std::to_string(blockAmount));
	changeMugenText(gOptionsScreenData.mDreamcastSave.mSaveSizeBlocksTextID, s.c_str());
}

static void setSelectedDreamcastSaveOptionInactive() {
	setMugenTextColor(gOptionsScreenData.mDreamcastSave.mSelectableTextID[gOptionsScreenData.mDreamcastSave.mSelected], getMugenTextColorFromMugenTextColorIndex(8));

	const auto selectionColor = getMugenTextColor(gOptionsScreenData.mDreamcastSave.mSettingTextID[gOptionsScreenData.mDreamcastSave.mSelected]);
	if (selectionColor.x != 0 && selectionColor.y != 0 && selectionColor.z != 0) {
		setMugenTextColor(gOptionsScreenData.mDreamcastSave.mSettingTextID[gOptionsScreenData.mDreamcastSave.mSelected], getMugenTextColorFromMugenTextColorIndex(8));
	}
}

static void setSelectedDreamcastSaveOptionActive() {
	setMugenTextColor(gOptionsScreenData.mDreamcastSave.mSelectableTextID[gOptionsScreenData.mDreamcastSave.mSelected], getMugenTextColorFromMugenTextColorIndex(0));

	const auto selectionColor = getMugenTextColor(gOptionsScreenData.mDreamcastSave.mSettingTextID[gOptionsScreenData.mDreamcastSave.mSelected]);
	if (selectionColor.x != 0 && selectionColor.y != 0 && selectionColor.z != 0) {
		setMugenTextColor(gOptionsScreenData.mDreamcastSave.mSettingTextID[gOptionsScreenData.mDreamcastSave.mSelected], getMugenTextColorFromMugenTextColorIndex(0));
	}

	auto pos = getMugenTextPosition(gOptionsScreenData.mDreamcastSave.mSelectableTextID[gOptionsScreenData.mDreamcastSave.mSelected]);
	pos.z = 0;
	setBoxCursorPosition(gOptionsScreenData.mDreamcastSave.mBoxCursorID, pos);
}

static void updateSelectedDreamcastSaveOption(int tDelta) {
	setSelectedDreamcastSaveOptionInactive();
	gOptionsScreenData.mDreamcastSave.mSelected += tDelta;
	gOptionsScreenData.mDreamcastSave.mSelected = (gOptionsScreenData.mDreamcastSave.mSelected + DREAMCAST_SAVE_SETTING_AMOUNT) % DREAMCAST_SAVE_SETTING_AMOUNT;
	tryPlayMugenSoundAdvanced(&gOptionsScreenData.mSounds, gOptionsScreenData.mHeader.mCursorMoveSound.x, gOptionsScreenData.mHeader.mCursorMoveSound.y, parseGameMidiVolumeToPrism(getGameMidiVolume()));
	setSelectedDreamcastSaveOptionActive();
}


static void loadDreamcastSaveOptionsScreen() {
	setAnimationPosition(gOptionsScreenData.mBackgroundAnimationElement, Vector3D(52, 35, 40));
	setAnimationSize(gOptionsScreenData.mBackgroundAnimationElement, Vector3D(216, 133, 1), Vector3D(0, 0, 0));

	if (gOptionsScreenData.mDreamcastSave.mSaveLoadType == GENERAL_SETTING_LOAD_SAVE_OPTIONS) {
		changeMugenText(gOptionsScreenData.mHeaderTextID, "SAVE/LOAD OPTIONS");
	}
	else {
		changeMugenText(gOptionsScreenData.mHeaderTextID, "SAVE/LOAD VARS");
	}
	setMugenTextPosition(gOptionsScreenData.mHeaderTextID, Vector3D(160, 20, 45));
	gOptionsScreenData.mActiveScreen = OPTION_SCREEN_DREAMCAST_SAVE;

	int offsetX = 70;
	int startY = 50;
	int offsetY = 13;
	gOptionsScreenData.mDreamcastSave.mSelectableTextID[DREAMCAST_SAVE_SETTING_SELECT_VMU] = addMugenTextMugenStyle("VMU", Vector3D(offsetX, startY, 45), Vector3DI(gOptionsScreenData.mTextFontID, 8, 1));
	gOptionsScreenData.mDreamcastSave.mSettingTextID[DREAMCAST_SAVE_SETTING_SELECT_VMU] = addMugenTextMugenStyle("A1", Vector3D(320 - offsetX, startY, 45), Vector3DI(gOptionsScreenData.mTextFontID, 8, -1));
	setDreamcastSaveSelectedVMUText();

	gOptionsScreenData.mDreamcastSave.mFreeBlocksTextID = addMugenTextMugenStyle("Free blocks: 0", Vector3D(offsetX, startY + offsetY * 1, 45), Vector3DI(gOptionsScreenData.mTextFontID, 8, 1));
	setDreamcastFreeBlocksText();

	gOptionsScreenData.mDreamcastSave.mSaveSizeBlocksTextID = addMugenTextMugenStyle("Save size: 0", Vector3D(offsetX, startY + offsetY * 2, 45), Vector3DI(gOptionsScreenData.mTextFontID, 8, 1));
	setDreamcastSaveSaveSizeText();

	gOptionsScreenData.mDreamcastSave.mSelectableTextID[DREAMCAST_SAVE_SETTING_SAVE] = addMugenTextMugenStyle("Save", Vector3D(offsetX, startY + offsetY * 4, 45), Vector3DI(gOptionsScreenData.mTextFontID, 8, 1));
	gOptionsScreenData.mDreamcastSave.mSettingTextID[DREAMCAST_SAVE_SETTING_SAVE] = addMugenTextMugenStyle("Confirm", Vector3D(320 - offsetX, startY + offsetY * 4, 45), Vector3DI(gOptionsScreenData.mTextFontID, 8, -1));

	gOptionsScreenData.mDreamcastSave.mSelectableTextID[DREAMCAST_SAVE_SETTING_LOAD] = addMugenTextMugenStyle("Load", Vector3D(offsetX, startY + offsetY * 5, 45), Vector3DI(gOptionsScreenData.mTextFontID, 8, 1));
	gOptionsScreenData.mDreamcastSave.mSettingTextID[DREAMCAST_SAVE_SETTING_LOAD] = addMugenTextMugenStyle(gButtonShortCutStrings[3].c_str(), Vector3D(320 - offsetX, startY + offsetY * 5, 45), Vector3DI(gOptionsScreenData.mTextFontID, 2, -1));

	gOptionsScreenData.mDreamcastSave.mSelectableTextID[DREAMCAST_SAVE_SETTING_DELETE] = addMugenTextMugenStyle("Delete", Vector3D(offsetX, startY + offsetY * 6, 45), Vector3DI(gOptionsScreenData.mTextFontID, 8, 1));
	gOptionsScreenData.mDreamcastSave.mSettingTextID[DREAMCAST_SAVE_SETTING_DELETE] = addMugenTextMugenStyle(gButtonShortCutStrings[2].c_str(), Vector3D(320 - offsetX, startY + offsetY * 6, 45), Vector3DI(gOptionsScreenData.mTextFontID, 5, -1));

	gOptionsScreenData.mDreamcastSave.mSelectableTextID[DREAMCAST_SAVE_SETTING_RETURN] = addMugenTextMugenStyle("Return to Options", Vector3D(offsetX, startY + offsetY * 8, 45), Vector3DI(gOptionsScreenData.mTextFontID, 8, 1));
	gOptionsScreenData.mDreamcastSave.mSettingTextID[DREAMCAST_SAVE_SETTING_RETURN] = addMugenTextMugenStyle(gButtonShortCutStrings[1].c_str(), Vector3D(320 - offsetX, startY + offsetY * 8, 45), Vector3DI(gOptionsScreenData.mTextFontID, 3, -1));

	gOptionsScreenData.mDreamcastSave.mBoxCursorID = addBoxCursor(Vector3D(0, 0, 0), Vector3D(0, 0, 43), GeoRectangle2D(-6, -11, 320 - 2 * offsetX + 10, 14));

	setSelectedDreamcastSaveOptionActive();
}

static void unloadDreamcastSaveOptionsScreen() {
	removeMugenText(gOptionsScreenData.mDreamcastSave.mFreeBlocksTextID);
	removeMugenText(gOptionsScreenData.mDreamcastSave.mSaveSizeBlocksTextID);

	for (int i = 0; i < DREAMCAST_SAVE_SETTING_AMOUNT; i++) {
		removeMugenText(gOptionsScreenData.mDreamcastSave.mSelectableTextID[i]);
		removeMugenText(gOptionsScreenData.mDreamcastSave.mSettingTextID[i]);
	}

	removeBoxCursor(gOptionsScreenData.mDreamcastSave.mBoxCursorID);
}

static void sanitizeOptionsValues() {
	setGlobalGameSpeed(std::min(getGlobalGameSpeed(), 9));
}

static void loadOptionsScreen() {
	gOptionsScreenData.mTextFontID = hasMugenFont(2) ? 2 : -1;

	sanitizeOptionsValues();

	instantiateActor(getBoxCursorHandler());
	gOptionsScreenData.mWhiteTexture = getEmptyWhiteTexture();

	const auto motifPath = getDolmexicaAssetFolder() + getMotifPath();
	loadMugenDefScript(&gOptionsScreenData.mScript, motifPath);
	std::string folder;
	getPathToFile(folder, motifPath.c_str());

	auto text = getSTLMugenDefStringVariable(&gOptionsScreenData.mScript, "files", "spr");
	text = findMugenSystemOrFightFilePath(text, folder);
	gOptionsScreenData.mSprites = loadMugenSpriteFileWithoutPalette(text);
	gOptionsScreenData.mAnimations = loadMugenAnimationFile(motifPath.c_str());

	text = getSTLMugenDefStringVariable(&gOptionsScreenData.mScript, "files", "snd");
	text = findMugenSystemOrFightFilePath(text, folder);
	gOptionsScreenData.mSounds = loadMugenSoundFile(text.c_str());

	loadOptionsHeader();
	loadScriptBackground(&gOptionsScreenData.mScript, &gOptionsScreenData.mSprites, &gOptionsScreenData.mAnimations, "optionbgdef", "optionbg");

	gOptionsScreenData.mHeaderTextID = addMugenTextMugenStyle("OPTIONS", Vector3D(160, 20, 45), Vector3DI(gOptionsScreenData.mTextFontID, 0, 0));
	gOptionsScreenData.mBackgroundAnimationElement = playOneFrameAnimationLoop(Vector3D(52, 35, 40), &gOptionsScreenData.mWhiteTexture);
	setAnimationColor(gOptionsScreenData.mBackgroundAnimationElement, 0, 0, 0.6);
	setAnimationTransparency(gOptionsScreenData.mBackgroundAnimationElement, 0.7);

	gOptionsScreenData.mConfirmationDialog.mIsActive = 0;
	loadGeneralOptionsScreen();
}

static void inputConfigToGeneralOptions() {
	if (gOptionsScreenData.mActiveScreen != OPTION_SCREEN_INPUT_CONFIG) return;

	unloadInputConfigOptionsScreen();
	loadGeneralOptionsScreen();
}

static void generalToInputConfigOptions() {
	if (gOptionsScreenData.mActiveScreen != OPTION_SCREEN_GENERAL) return;

	unloadGeneralOptionsScreen();
	loadInputConfigOptionsScreen();
}

static void keyConfigToInputConfigOptions() {
	if (gOptionsScreenData.mActiveScreen != OPTION_SCREEN_KEY_CONFIG) return;

	unloadKeyConfigOptionsScreen();
	loadInputConfigOptionsScreen();
}

static void inputConfigToKeyConfigOptions() {
	if (gOptionsScreenData.mActiveScreen != OPTION_SCREEN_INPUT_CONFIG) return;

	unloadInputConfigOptionsScreen();
	loadKeyConfigOptionsScreen();
}

static void dreamcastSaveToGeneralOptions() {
	if (gOptionsScreenData.mActiveScreen != OPTION_SCREEN_DREAMCAST_SAVE) return;

	unloadDreamcastSaveOptionsScreen();
	loadGeneralOptionsScreen();
}

static void generalToDreamcastSaveOptions() {
	if (gOptionsScreenData.mActiveScreen != OPTION_SCREEN_GENERAL) return;

	unloadGeneralOptionsScreen();
	loadDreamcastSaveOptionsScreen();
}

static void setOptionScreenConfirmationDialogActive(const char* tQuestionText, const std::function<void()>& tFunc) {
	gOptionsScreenData.mConfirmationDialog.mBackgroundAnimationElement = playOneFrameAnimationLoop(Vector3D(58, 60, 49), &gOptionsScreenData.mWhiteTexture);
	setAnimationColor(gOptionsScreenData.mConfirmationDialog.mBackgroundAnimationElement, 0, 0, 0.6);
	setAnimationTransparency(gOptionsScreenData.mConfirmationDialog.mBackgroundAnimationElement, 0.7);
	setAnimationSize(gOptionsScreenData.mConfirmationDialog.mBackgroundAnimationElement, Vector3D(202, 50, 1), Vector3D(0, 0, 0));

	gOptionsScreenData.mConfirmationDialog.mQuestionTextID = addMugenTextMugenStyle(tQuestionText, Vector3D(160, 75, 51), Vector3DI(gOptionsScreenData.mTextFontID, 0, 0));
	gOptionsScreenData.mConfirmationDialog.mSelectionTextID[0] = addMugenTextMugenStyle("Yes", Vector3D(130, 98, 51), Vector3DI(gOptionsScreenData.mTextFontID, 8, 1));
	gOptionsScreenData.mConfirmationDialog.mSelectionTextID[1] = addMugenTextMugenStyle("No", Vector3D(180, 98, 51), Vector3DI(gOptionsScreenData.mTextFontID, 0, 1));
	gOptionsScreenData.mConfirmationDialog.mBoxCursorID = addBoxCursor(Vector3D(0, 0, 0), Vector3D(0, 0, 50), GeoRectangle2D(-5, -11, 27, 14));

	gOptionsScreenData.mConfirmationDialog.mSelectedOption = 1;
	auto textPos = getMugenTextPosition(gOptionsScreenData.mConfirmationDialog.mSelectionTextID[gOptionsScreenData.mConfirmationDialog.mSelectedOption]);
	textPos.z = 0.0;
	setBoxCursorPosition(gOptionsScreenData.mConfirmationDialog.mBoxCursorID, textPos);

	gOptionsScreenData.mConfirmationDialog.mFunc = tFunc;
	gOptionsScreenData.mConfirmationDialog.mIsActive = 1;
}

static void setOptionScreenConfirmationDialogInactive() {
	removeHandledAnimation(gOptionsScreenData.mConfirmationDialog.mBackgroundAnimationElement);
	removeMugenText(gOptionsScreenData.mConfirmationDialog.mQuestionTextID);
	removeMugenText(gOptionsScreenData.mConfirmationDialog.mSelectionTextID[0]);
	removeMugenText(gOptionsScreenData.mConfirmationDialog.mSelectionTextID[1]);
	removeBoxCursor(gOptionsScreenData.mConfirmationDialog.mBoxCursorID);

	gOptionsScreenData.mConfirmationDialog.mIsActive = 0;
}

static void handleOptionScreenGeneralSelectLoadSaveOptionsWindows(GeneralSettingType tType, void(*tLoad)(PrismSaveSlot), void(*tSave)(PrismSaveSlot)) {
	if (!strcmp(getMugenTextText(gOptionsScreenData.mGeneral.mSettingTextID[tType]), "Load")) {
		const auto func = [tLoad]() {
			tLoad(PrismSaveSlot::AUTOMATIC);
			generalToInputConfigOptions();
			inputConfigToGeneralOptions();
		};
		setOptionScreenConfirmationDialogActive("Do you really want to load?", func);
	}
	else {
		const auto func = [tSave]() {
			tSave(PrismSaveSlot::AUTOMATIC);
		};
		setOptionScreenConfirmationDialogActive("Do you really want to save?", func);
	}
}

static void handleOptionScreenGeneralSelectLoadSaveOptionsDreamcast(GeneralSettingType tType) {
	gOptionsScreenData.mDreamcastSave.mSaveLoadType = tType;
	generalToDreamcastSaveOptions();
}

static void handleOptionScreenGeneralSelectLoadSaveOptions(GeneralSettingType tType, void(*tLoad)(PrismSaveSlot), void(*tSave)(PrismSaveSlot)) {
	if (isOnDreamcast()) {
		handleOptionScreenGeneralSelectLoadSaveOptionsDreamcast(tType);
		tryPlayMugenSoundAdvanced(&gOptionsScreenData.mSounds, gOptionsScreenData.mHeader.mCursorDoneSound.x, gOptionsScreenData.mHeader.mCursorDoneSound.y, parseGameMidiVolumeToPrism(getGameMidiVolume()));
	}
	else if (isOnWindows()) {
		handleOptionScreenGeneralSelectLoadSaveOptionsWindows(tType, tLoad, tSave);
		tryPlayMugenSoundAdvanced(&gOptionsScreenData.mSounds, gOptionsScreenData.mHeader.mCursorDoneSound.x, gOptionsScreenData.mHeader.mCursorDoneSound.y, parseGameMidiVolumeToPrism(getGameMidiVolume()));
	}
}

static void updateOptionScreenGeneralSelect() {
	if (gOptionsScreenData.mGeneral.mSelected == GENERAL_SETTING_INPUT_CONFIG) {
		generalToInputConfigOptions();
	}
	else if (gOptionsScreenData.mGeneral.mSelected == GENERAL_SETTING_RETURN) {
		tryPlayMugenSoundAdvanced(&gOptionsScreenData.mSounds, gOptionsScreenData.mHeader.mCancelSound.x, gOptionsScreenData.mHeader.mCancelSound.y, parseGameMidiVolumeToPrism(getGameMidiVolume()));
		setNewScreen(getDreamTitleScreen());
	} 
	else if (gOptionsScreenData.mGeneral.mSelected == GENERAL_SETTING_LOAD_SAVE_OPTIONS) {
		handleOptionScreenGeneralSelectLoadSaveOptions(GENERAL_SETTING_LOAD_SAVE_OPTIONS, loadMugenConfigSave, saveMugenConfigSave);
	}
	else if (gOptionsScreenData.mGeneral.mSelected == GENERAL_SETTING_LOAD_SAVE_VARS) {
		handleOptionScreenGeneralSelectLoadSaveOptions(GENERAL_SETTING_LOAD_SAVE_VARS, loadGlobalVariables, saveGlobalVariables);
	}
	else if (gOptionsScreenData.mGeneral.mSelected == GENERAL_SETTING_DEFAULT_VALUES) {
		setDefaultOptionVariables();
		generalToInputConfigOptions();
		inputConfigToGeneralOptions();
	}
}

static string gDifficultyNames[] = {
	"Invalid",
	"Easy 1",
	"Easy 2",
	"Medium 3",
	"Medium 4",
	"Medium 5",
	"Hard 6",
	"Hard 7",
	"Hard 8",
};

static void setDifficultyOptionText() {
	changeMugenText(gOptionsScreenData.mGeneral.mSettingTextID[GENERAL_SETTING_DIFFICULTY], gDifficultyNames[getDifficulty()].data());
}

static void changeDifficultyOption(int tDelta) {
	int currentDifficulty = getDifficulty();
	if (tDelta == -1 && currentDifficulty > 1) currentDifficulty--;
	if (tDelta == 1 && currentDifficulty < 8) currentDifficulty++;
	setDifficulty(currentDifficulty);
	setDifficultyOptionText();
}

static void setLifeOptionText() {
	char lifeText[30];
	sprintf(lifeText, "%d%%", getLifeStartPercentageNumber());
	changeMugenText(gOptionsScreenData.mGeneral.mSettingTextID[GENERAL_SETTING_LIFE], lifeText);
}

static void changeLifeOption(int tDelta) {
	int lifeNumber = getLifeStartPercentageNumber();
	if (tDelta == -1 && lifeNumber > 30) lifeNumber -=10;
	if (tDelta == 1 && lifeNumber < 300) lifeNumber += 10;
	setLifeStartPercentageNumber(lifeNumber);
	setLifeOptionText();
}

static void setTimeLimitOptionText() {
	if (isGlobalTimerInfinite()) {
		changeMugenText(gOptionsScreenData.mGeneral.mSettingTextID[GENERAL_SETTING_TIME_LIMIT], "None");
	}
	else {
		char timeLimitText[30];
		sprintf(timeLimitText, "%d", getGlobalTimerDuration());
		changeMugenText(gOptionsScreenData.mGeneral.mSettingTextID[GENERAL_SETTING_TIME_LIMIT], timeLimitText);
	}
}


static void changeTimeLimitOption(int tDelta) {
	int timeLimit = getGlobalTimerDuration();
	int isInfinite = isGlobalTimerInfinite();
	if (tDelta == -1) {
		if (isInfinite) {
			timeLimit = 99;
			isInfinite = 0;
		}
		else if (timeLimit == 99) timeLimit = 80;
		else if(timeLimit > 20) timeLimit -= 20;
	}
	if (tDelta == 1 && !isInfinite) {
		if(timeLimit == 80) timeLimit = 99;
		else if (timeLimit == 99) {
			isInfinite = 1;
		}
		else {
			timeLimit += 20;
		}
	}

	if (isInfinite) {
		setGlobalTimerInfinite();
	}
	else {
		setGlobalTimerDuration(timeLimit);
	}
	setTimeLimitOptionText();
}

static void setGameSpeedOptionText() {
	int currentSpeed = getGlobalGameSpeed();
	char gameSpeedText[30];
	if (!currentSpeed) {
		changeMugenText(gOptionsScreenData.mGeneral.mSettingTextID[GENERAL_SETTING_GAME_SPEED], "Normal");
	}
	else if(currentSpeed > 0){
		sprintf(gameSpeedText, "Fast %d", currentSpeed);
		changeMugenText(gOptionsScreenData.mGeneral.mSettingTextID[GENERAL_SETTING_GAME_SPEED], gameSpeedText);
	}
	else {
		sprintf(gameSpeedText, "Slow %d", -currentSpeed);
		changeMugenText(gOptionsScreenData.mGeneral.mSettingTextID[GENERAL_SETTING_GAME_SPEED], gameSpeedText);
	}
}

static void changeGameSpeedOption(int tDelta) {
	int currentSpeed = getGlobalGameSpeed();
	if (tDelta == -1 && currentSpeed > -9) currentSpeed--;
	if (tDelta == 1 && currentSpeed < 9) currentSpeed++;
	setGlobalGameSpeed(currentSpeed);
	setGameSpeedOptionText();
}

static void setVolumeOptionText(GeneralSettingType tType, int(*tGet)()) {
	int currentVolume = tGet();
	if (currentVolume == 0) {
		changeMugenText(gOptionsScreenData.mGeneral.mSettingTextID[tType], "Off");
	}
	else if (currentVolume == 100) {
		changeMugenText(gOptionsScreenData.mGeneral.mSettingTextID[tType], "Max");
	}  
	else {
		char volumeText[30];
		sprintf(volumeText, "%d", currentVolume);
		changeMugenText(gOptionsScreenData.mGeneral.mSettingTextID[tType], volumeText);
	}
}

static void changeVolumeOption(int tDelta, GeneralSettingType tType, int(*tGet)(), void(*tSet)(int)) {
	int currentVolume = tGet();
	if (tDelta == -1 && currentVolume > 0) currentVolume--;
	if (tDelta == 1 && currentVolume < 100) currentVolume++;
	tSet(currentVolume);
	setVolumeOptionText(tType, tGet);
}

static void toggleLoadSaveOption(GeneralSettingType tType) {
	if (isOnDreamcast()) return;
	if (!strcmp(getMugenTextText(gOptionsScreenData.mGeneral.mSettingTextID[tType]), "Load")) {
		changeMugenText(gOptionsScreenData.mGeneral.mSettingTextID[tType], "Save");
	}
	else {
		changeMugenText(gOptionsScreenData.mGeneral.mSettingTextID[tType], "Load");
	}
}

static void changeSelectedGeneralOption(int tDelta) {
	if (gOptionsScreenData.mGeneral.mSelected == GENERAL_SETTING_DIFFICULTY) {
		changeDifficultyOption(tDelta);
	}
	else if (gOptionsScreenData.mGeneral.mSelected == GENERAL_SETTING_LIFE) {
		changeLifeOption(tDelta);
	}
	else if (gOptionsScreenData.mGeneral.mSelected == GENERAL_SETTING_TIME_LIMIT) {
		changeTimeLimitOption(tDelta);
	}
	else if (gOptionsScreenData.mGeneral.mSelected == GENERAL_SETTING_GAME_SPEED) {
		changeGameSpeedOption(tDelta);
	}
	else if (gOptionsScreenData.mGeneral.mSelected == GENERAL_SETTING_WAV_VOLUME) {
		changeVolumeOption(tDelta, GENERAL_SETTING_WAV_VOLUME, getUnscaledGameWavVolume, setUnscaledGameWavVolume);
	}
	else if (gOptionsScreenData.mGeneral.mSelected == GENERAL_SETTING_MIDI_VOLUME) {
		changeVolumeOption(tDelta, GENERAL_SETTING_MIDI_VOLUME, getUnscaledGameMidiVolume, setUnscaledGameMidiVolume);
	}
	else if (gOptionsScreenData.mGeneral.mSelected == GENERAL_SETTING_LOAD_SAVE_OPTIONS) {
		toggleLoadSaveOption(GENERAL_SETTING_LOAD_SAVE_OPTIONS);
	}
	else if (gOptionsScreenData.mGeneral.mSelected == GENERAL_SETTING_LOAD_SAVE_VARS) {
		toggleLoadSaveOption(GENERAL_SETTING_LOAD_SAVE_VARS);
	}
}

static void updateOptionScreenGeneral() {
	if (hasPressedUpFlank()) {
		updateSelectedGeneralOption(-1);
	} else if (hasPressedDownFlank()) {
		updateSelectedGeneralOption(1);
	}

	if (hasPressedLeftFlank()) {
		changeSelectedGeneralOption(-1);
	}
	else if (hasPressedRightFlank()) {
		changeSelectedGeneralOption(1);
	}

	int f1Input = isUsingController() ? hasPressedXFlank() : hasPressedKeyboardKeyFlank(KEYBOARD_F1_PRISM);
	if (hasPressedAFlank() || hasPressedStartFlank()) {
		updateOptionScreenGeneralSelect();
	}
	else if (f1Input) {
		generalToInputConfigOptions();
	}

	if (hasPressedBFlank()) {
		setNewScreen(getDreamTitleScreen());
	}
}

static void updateSelectedInputConfigOption(int tPlayerIndex, int tDelta) {
	setSelectedInputConfigOptionInactive(tPlayerIndex);
	gOptionsScreenData.mInputConfig.mSelected[tPlayerIndex] += tDelta;
	gOptionsScreenData.mInputConfig.mSelected[tPlayerIndex] = (gOptionsScreenData.mInputConfig.mSelected[tPlayerIndex] + INPUT_CONFIG_SETTING_AMOUNT) % INPUT_CONFIG_SETTING_AMOUNT;
	tryPlayMugenSoundAdvanced(&gOptionsScreenData.mSounds, gOptionsScreenData.mHeader.mCursorMoveSound.x, gOptionsScreenData.mHeader.mCursorMoveSound.y, parseGameMidiVolumeToPrism(getGameMidiVolume()));
	setSelectedInputConfigOptionActive(tPlayerIndex);
}

static void updateOptionScreenInputConfigSelect(int i) {
	if (gOptionsScreenData.mInputConfig.mSelected[i] == INPUT_CONFIG_SETTING_KEY_CONFIG) {
		inputConfigToKeyConfigOptions();
	}
	else if (gOptionsScreenData.mInputConfig.mSelected[i] == INPUT_CONFIG_SETTING_RETURN) {
		inputConfigToGeneralOptions();
	}
}

static void updateOptionScreenInputConfig() {

	for (int i = 0; i < 2; i++) {
		if (hasPressedUpFlankSingle(i)) {
			updateSelectedInputConfigOption(i, -1);
		}
		else if (hasPressedDownFlankSingle(i)) {
			updateSelectedInputConfigOption(i, 1);
		}

		int f1Input = isUsingControllerSingle(i) ? hasPressedXFlankSingle(i) : (!i && hasPressedKeyboardKeyFlank(KEYBOARD_F1_PRISM));
		if (hasPressedAFlankSingle(i) || hasPressedStartFlankSingle(i)) {
			updateOptionScreenInputConfigSelect(i);
		}
		else if (f1Input) {
			inputConfigToKeyConfigOptions();
		}
		else if (hasPressedBFlankSingle(i)) {
			inputConfigToGeneralOptions();
		}
	}

	if (hasPressedBFlank()) {
		inputConfigToGeneralOptions();
	}
}

static void updateSelectedKeyConfigOption(int tDeltaX, int tDeltaY) {
	setSelectedKeyConfigOptionInactive();
	gOptionsScreenData.mKeyConfig.mSelected.x += tDeltaX;
	gOptionsScreenData.mKeyConfig.mSelected.x = (gOptionsScreenData.mKeyConfig.mSelected.x + 2) % 2;

	if (tDeltaY == 1) {
		if (gOptionsScreenData.mKeyConfig.mSelected.y == KEY_CONFIG_SETTING_CONFIG_ALL) gOptionsScreenData.mKeyConfig.mSelected.y = KEY_CONFIG_SETTING_DEFAULT;
		else gOptionsScreenData.mKeyConfig.mSelected.y += tDeltaY;
	}
	else if (tDeltaY == -1) {
		if (gOptionsScreenData.mKeyConfig.mSelected.y == KEY_CONFIG_SETTING_DEFAULT) gOptionsScreenData.mKeyConfig.mSelected.y = KEY_CONFIG_SETTING_CONFIG_ALL;
		else gOptionsScreenData.mKeyConfig.mSelected.y += tDeltaY;
	}
	gOptionsScreenData.mKeyConfig.mSelected.y = (gOptionsScreenData.mKeyConfig.mSelected.y + KEY_CONFIG_SETTING_AMOUNT) % KEY_CONFIG_SETTING_AMOUNT;
	tryPlayMugenSoundAdvanced(&gOptionsScreenData.mSounds, gOptionsScreenData.mHeader.mCursorMoveSound.x, gOptionsScreenData.mHeader.mCursorMoveSound.y, parseGameMidiVolumeToPrism(getGameMidiVolume()));
	setSelectedKeyConfigOptionActive();
}

static ControllerButtonPrism gButtonOrder[] = {
	CONTROLLER_UP_PRISM,
	CONTROLLER_DOWN_PRISM,
	CONTROLLER_LEFT_PRISM,
	CONTROLLER_RIGHT_PRISM,
	CONTROLLER_A_PRISM,
	CONTROLLER_B_PRISM,
	CONTROLLER_R_PRISM,
	CONTROLLER_X_PRISM,
	CONTROLLER_Y_PRISM,
	CONTROLLER_L_PRISM,
	CONTROLLER_START_PRISM,
};

static void endSettingKeyConfigInput() {
	removeBoxCursor(gOptionsScreenData.mKeyConfig.mSetKeyBoxCursorID);
	resumeBoxCursor(gOptionsScreenData.mKeyConfig.mBoxCursorID);
	gOptionsScreenData.mKeyConfig.mIsSettingInput = 0;
	gOptionsScreenData.mKeyConfig.mSettingFlankDone = 0;
}

static void setCurrentKeyConfigSetActive();

static void setCurrentKeyConfigSetInactive() {
	int settingID = gOptionsScreenData.mKeyConfig.mSettingTextID[gOptionsScreenData.mKeyConfig.mSelected.x][KEY_CONFIG_SETTING_UP + gOptionsScreenData.mKeyConfig.mCurrentTargetButton];
	int selectableID = gOptionsScreenData.mKeyConfig.mSelectableTextID[gOptionsScreenData.mKeyConfig.mSelected.x][KEY_CONFIG_SETTING_UP + gOptionsScreenData.mKeyConfig.mCurrentTargetButton];

	setMugenTextColor(settingID, getMugenTextColorFromMugenTextColorIndex(8));
	setMugenTextColor(selectableID, getMugenTextColorFromMugenTextColorIndex(8));
}

static void currentKeyConfigOverCB(void* tCaller) {
	(void)tCaller;
	int settingID = gOptionsScreenData.mKeyConfig.mSettingTextID[gOptionsScreenData.mKeyConfig.mSelected.x][KEY_CONFIG_SETTING_UP + gOptionsScreenData.mKeyConfig.mCurrentTargetButton];
	if (isUsingControllerSingle(gOptionsScreenData.mKeyConfig.mSelected.x)) {
		ControllerButtonPrism button = getButtonForController(gOptionsScreenData.mKeyConfig.mSelected.x, gButtonOrder[gOptionsScreenData.mKeyConfig.mCurrentTargetButton]);
		changeMugenText(settingID, gButtonNames[button].data());
	}
	else {
		KeyboardKeyPrism key = getButtonForKeyboard(gOptionsScreenData.mKeyConfig.mSelected.x, gButtonOrder[gOptionsScreenData.mKeyConfig.mCurrentTargetButton]);
		changeMugenText(settingID, gKeyNames[key].data());
	}
	
	setCurrentKeyConfigSetInactive();
	if (gButtonOrder[gOptionsScreenData.mKeyConfig.mCurrentTargetButton] == CONTROLLER_START_PRISM) {
		endSettingKeyConfigInput();
		return;
	}
	
	gOptionsScreenData.mKeyConfig.mCurrentTargetButton++;
	setCurrentKeyConfigSetActive();
}

static void setCurrentKeyConfigSetActive() {
	int settingID = gOptionsScreenData.mKeyConfig.mSettingTextID[gOptionsScreenData.mKeyConfig.mSelected.x][KEY_CONFIG_SETTING_UP + gOptionsScreenData.mKeyConfig.mCurrentTargetButton];
	int selectableID = gOptionsScreenData.mKeyConfig.mSelectableTextID[gOptionsScreenData.mKeyConfig.mSelected.x][KEY_CONFIG_SETTING_UP + gOptionsScreenData.mKeyConfig.mCurrentTargetButton];
	Position pos = getMugenTextPosition(selectableID);
	pos.z = 0;
	setBoxCursorPosition(gOptionsScreenData.mKeyConfig.mSetKeyBoxCursorID, pos);

	setMugenTextColor(settingID, getMugenTextColorFromMugenTextColorIndex(0));
	setMugenTextColor(selectableID, getMugenTextColorFromMugenTextColorIndex(0));

	if (isUsingControllerSingle(gOptionsScreenData.mKeyConfig.mSelected.x)) {	
		setButtonFromUserInputForController(gOptionsScreenData.mKeyConfig.mSelected.x, gButtonOrder[gOptionsScreenData.mKeyConfig.mCurrentTargetButton], currentKeyConfigOverCB);
	}
	else {
		setButtonFromUserInputForKeyboard(gOptionsScreenData.mKeyConfig.mSelected.x, gButtonOrder[gOptionsScreenData.mKeyConfig.mCurrentTargetButton], currentKeyConfigOverCB);
	}
}

static KeyboardKeyPrism gDefaultKeys[MAXIMUM_CONTROLLER_AMOUNT][KEYBOARD_AMOUNT_PRISM] = {
	{
		KEYBOARD_A_PRISM,
		KEYBOARD_S_PRISM,
		KEYBOARD_Q_PRISM,
		KEYBOARD_W_PRISM,
		KEYBOARD_E_PRISM,
		KEYBOARD_D_PRISM,
		KEYBOARD_LEFT_PRISM,
		KEYBOARD_RIGHT_PRISM,
		KEYBOARD_UP_PRISM,
		KEYBOARD_DOWN_PRISM,
		KEYBOARD_RETURN_PRISM,
	},
	{
		KEYBOARD_H_PRISM,
		KEYBOARD_J_PRISM,
		KEYBOARD_Y_PRISM,
		KEYBOARD_U_PRISM,
		KEYBOARD_I_PRISM,
		KEYBOARD_K_PRISM,
		KEYBOARD_4_PRISM,
		KEYBOARD_6_PRISM,
		KEYBOARD_8_PRISM,
		KEYBOARD_2_PRISM,
		KEYBOARD_3_PRISM,
	},
};

static void setKeyConfigDefault() {
		for (int i = 0; i < 11; i++) {
			int settingID = gOptionsScreenData.mKeyConfig.mSettingTextID[gOptionsScreenData.mKeyConfig.mSelected.x][KEY_CONFIG_SETTING_UP + i];
			ControllerButtonPrism button = gButtonOrder[i];
			if (isUsingControllerSingle(gOptionsScreenData.mKeyConfig.mSelected.x)) {
				setButtonForController(gOptionsScreenData.mKeyConfig.mSelected.x, button, button);
				changeMugenText(settingID, gButtonNames[button].data());
			}
			else {
				setButtonForKeyboard(gOptionsScreenData.mKeyConfig.mSelected.x, button, gDefaultKeys[gOptionsScreenData.mKeyConfig.mSelected.x][button]);
				changeMugenText(settingID, gKeyNames[gDefaultKeys[gOptionsScreenData.mKeyConfig.mSelected.x][button]].data());
			}
		}
}

static void startSettingKeyConfigInput() {
	gOptionsScreenData.mKeyConfig.mCurrentTargetButton = 0;
	gOptionsScreenData.mKeyConfig.mIsSettingInput = 1;
	gOptionsScreenData.mKeyConfig.mSetKeyBoxCursorID = addBoxCursor(Vector3D(0, 0, 0), Vector3D(0, 0, 43), GeoRectangle2D(-5, -10, 92, 12));
	pauseBoxCursor(gOptionsScreenData.mKeyConfig.mBoxCursorID);
	setCurrentKeyConfigSetActive();
}

static void updateOptionScreenKeyConfigSelect() {
	if (gOptionsScreenData.mKeyConfig.mSelected.y == KEY_CONFIG_SETTING_CONFIG_ALL) {
		startSettingKeyConfigInput();
	} 
	else if (gOptionsScreenData.mKeyConfig.mSelected.y == KEY_CONFIG_SETTING_DEFAULT) {
		setKeyConfigDefault();
	}
	else if (gOptionsScreenData.mKeyConfig.mSelected.y == KEY_CONFIG_SETTING_EXIT) {
		keyConfigToInputConfigOptions();
	}
}

static void updateOptionScreenKeyConfig() {
	if (gOptionsScreenData.mKeyConfig.mIsSettingInput) return;

	if (!gOptionsScreenData.mKeyConfig.mSettingFlankDone) {
		gOptionsScreenData.mKeyConfig.mSettingFlankDone = 1;
		return;
	}

	if (hasPressedUpFlank()) {
		updateSelectedKeyConfigOption(0, -1);
	}
	else if (hasPressedDownFlank()) {
		updateSelectedKeyConfigOption(0, 1);
	}

	if (hasPressedLeftFlank()) {
		updateSelectedKeyConfigOption(-1, 0);
	}
	else if (hasPressedRightFlank()) {
		updateSelectedKeyConfigOption(1, 0);
	}

	int f1Input = isUsingController() ? hasPressedXFlank() : hasPressedKeyboardKeyFlank(KEYBOARD_F1_PRISM);
	int f2Input = isUsingController() ? hasPressedYFlank() : hasPressedKeyboardKeyFlank(KEYBOARD_F2_PRISM);
	int f3Input = isUsingController() ? hasPressedLFlank() : hasPressedKeyboardKeyFlank(KEYBOARD_F3_PRISM);
	int f4Input = isUsingController() ? hasPressedRFlank() : hasPressedKeyboardKeyFlank(KEYBOARD_F4_PRISM);
	if (hasPressedAFlank() || hasPressedStartFlank()) {
		updateOptionScreenKeyConfigSelect();
	}
	else if (hasPressedBFlank()) {
		keyConfigToInputConfigOptions();
	}
	else if (f1Input) {
		gOptionsScreenData.mKeyConfig.mSelected.x = 0;
		gOptionsScreenData.mKeyConfig.mSelected.y = KEY_CONFIG_SETTING_CONFIG_ALL;
		startSettingKeyConfigInput();
	}
	else if (f2Input) {
		gOptionsScreenData.mKeyConfig.mSelected.x = 1;
		gOptionsScreenData.mKeyConfig.mSelected.y = KEY_CONFIG_SETTING_CONFIG_ALL;
		startSettingKeyConfigInput();
	}
	else if (f3Input) {
		gOptionsScreenData.mKeyConfig.mSelected.x = 0;
		gOptionsScreenData.mKeyConfig.mSelected.y = KEY_CONFIG_SETTING_DEFAULT;
		setKeyConfigDefault();
	}
	else if (f4Input) {
		gOptionsScreenData.mKeyConfig.mSelected.x = 1;
		gOptionsScreenData.mKeyConfig.mSelected.y = KEY_CONFIG_SETTING_DEFAULT;
		setKeyConfigDefault();
	}
}

static void changeDreamcastSaveSelectVMUOption(int tDelta) {
	auto index = int(gOptionsScreenData.mDreamcastSave.mSelectedVMU) + tDelta;
	index = (index + int(PrismSaveSlot::AMOUNT)) % int(PrismSaveSlot::AMOUNT);
	gOptionsScreenData.mDreamcastSave.mSelectedVMU = PrismSaveSlot(index);
	setDreamcastSaveSelectedVMUText();
	setDreamcastFreeBlocksText();
}

static void changeSelectedDreamcastSaveOption(int tDelta) {
	if (gOptionsScreenData.mDreamcastSave.mSelected == DREAMCAST_SAVE_SETTING_SELECT_VMU) {
		changeDreamcastSaveSelectVMUOption(tDelta);
	}
}

static void updateOptionScreenDreamcastSaveSelectSave() {
	std::function<void()> func;
	if (gOptionsScreenData.mDreamcastSave.mSaveLoadType == GENERAL_SETTING_LOAD_SAVE_OPTIONS) {
		func = []() {
			saveMugenConfigSave(gOptionsScreenData.mDreamcastSave.mSelectedVMU);
			setDreamcastFreeBlocksText();
		};
	}
	else {
		func = []() {
			saveGlobalVariables(gOptionsScreenData.mDreamcastSave.mSelectedVMU);
			setDreamcastFreeBlocksText();
		};
	}
	setOptionScreenConfirmationDialogActive("Do you really want to save?", func);
}

static void updateOptionScreenDreamcastSaveSelectLoad() {
	std::function<void()> func;
	if (gOptionsScreenData.mDreamcastSave.mSaveLoadType == GENERAL_SETTING_LOAD_SAVE_OPTIONS) {
		func = []() {
			loadMugenConfigSave(gOptionsScreenData.mDreamcastSave.mSelectedVMU);
		};
	}
	else {
		func = []() {
			loadGlobalVariables(gOptionsScreenData.mDreamcastSave.mSelectedVMU);
		};
	}
	setOptionScreenConfirmationDialogActive("Do you really want to load?", func);
}

static void updateOptionScreenDreamcastSaveSelectDelete() {
	std::function<void()> func;
	if (gOptionsScreenData.mDreamcastSave.mSaveLoadType == GENERAL_SETTING_LOAD_SAVE_OPTIONS) {
		func = []() {
			deleteMugenConfigSave(gOptionsScreenData.mDreamcastSave.mSelectedVMU);
			setDreamcastFreeBlocksText();
		};
	}
	else {
		func = []() {
			deleteGlobalVariables(gOptionsScreenData.mDreamcastSave.mSelectedVMU);
			setDreamcastFreeBlocksText();
		};
	}
	setOptionScreenConfirmationDialogActive("Do you really want to delete?", func);
}

static void updateOptionScreenDreamcastSaveSelect() {
	if (gOptionsScreenData.mDreamcastSave.mSelected == DREAMCAST_SAVE_SETTING_SAVE && isPrismSaveSlotActive(gOptionsScreenData.mDreamcastSave.mSelectedVMU)) {
		updateOptionScreenDreamcastSaveSelectSave();
		tryPlayMugenSoundAdvanced(&gOptionsScreenData.mSounds, gOptionsScreenData.mHeader.mCursorDoneSound.x, gOptionsScreenData.mHeader.mCursorDoneSound.y, parseGameMidiVolumeToPrism(getGameMidiVolume()));
	}
	else if (gOptionsScreenData.mDreamcastSave.mSelected == DREAMCAST_SAVE_SETTING_LOAD && isPrismSaveSlotActive(gOptionsScreenData.mDreamcastSave.mSelectedVMU) && gOptionsScreenData.mDreamcastSave.mHasSave) {
		updateOptionScreenDreamcastSaveSelectLoad();
		tryPlayMugenSoundAdvanced(&gOptionsScreenData.mSounds, gOptionsScreenData.mHeader.mCursorDoneSound.x, gOptionsScreenData.mHeader.mCursorDoneSound.y, parseGameMidiVolumeToPrism(getGameMidiVolume()));
	}
	else if (gOptionsScreenData.mDreamcastSave.mSelected == DREAMCAST_SAVE_SETTING_DELETE && isPrismSaveSlotActive(gOptionsScreenData.mDreamcastSave.mSelectedVMU) && gOptionsScreenData.mDreamcastSave.mHasSave) {
		updateOptionScreenDreamcastSaveSelectDelete();
		tryPlayMugenSoundAdvanced(&gOptionsScreenData.mSounds, gOptionsScreenData.mHeader.mCursorDoneSound.x, gOptionsScreenData.mHeader.mCursorDoneSound.y, parseGameMidiVolumeToPrism(getGameMidiVolume()));
	}
	else if (gOptionsScreenData.mDreamcastSave.mSelected == DREAMCAST_SAVE_SETTING_RETURN) {
		tryPlayMugenSoundAdvanced(&gOptionsScreenData.mSounds, gOptionsScreenData.mHeader.mCancelSound.x, gOptionsScreenData.mHeader.mCancelSound.y, parseGameMidiVolumeToPrism(getGameMidiVolume()));
		dreamcastSaveToGeneralOptions();
	}
}

static void updateOptionScreenDreamcastSave() {
	if (hasPressedUpFlank()) {
		updateSelectedDreamcastSaveOption(-1);
	}
	else if (hasPressedDownFlank()) {
		updateSelectedDreamcastSaveOption(1);
	}

	if (hasPressedLeftFlank()) {
		changeSelectedDreamcastSaveOption(-1);
	}
	else if (hasPressedRightFlank()) {
		changeSelectedDreamcastSaveOption(1);
	}

	if (hasPressedAFlank() || hasPressedStartFlank()) {
		updateOptionScreenDreamcastSaveSelect();
	}
	else if (hasPressedXFlank() && isPrismSaveSlotActive(gOptionsScreenData.mDreamcastSave.mSelectedVMU) && gOptionsScreenData.mDreamcastSave.mHasSave) {
		updateOptionScreenDreamcastSaveSelectDelete();
		tryPlayMugenSoundAdvanced(&gOptionsScreenData.mSounds, gOptionsScreenData.mHeader.mCursorDoneSound.x, gOptionsScreenData.mHeader.mCursorDoneSound.y, parseGameMidiVolumeToPrism(getGameMidiVolume()));
	}
	else if (hasPressedYFlank() && isPrismSaveSlotActive(gOptionsScreenData.mDreamcastSave.mSelectedVMU) && gOptionsScreenData.mDreamcastSave.mHasSave) {
		updateOptionScreenDreamcastSaveSelectLoad();
		tryPlayMugenSoundAdvanced(&gOptionsScreenData.mSounds, gOptionsScreenData.mHeader.mCursorDoneSound.x, gOptionsScreenData.mHeader.mCursorDoneSound.y, parseGameMidiVolumeToPrism(getGameMidiVolume()));
	}
	else if (hasPressedBFlank()) {
		tryPlayMugenSoundAdvanced(&gOptionsScreenData.mSounds, gOptionsScreenData.mHeader.mCancelSound.x, gOptionsScreenData.mHeader.mCancelSound.y, parseGameMidiVolumeToPrism(getGameMidiVolume()));
		dreamcastSaveToGeneralOptions();
	}
}

static void updateOptionScreenConfirmationDialogSelection() {
	setMugenTextColor(gOptionsScreenData.mConfirmationDialog.mSelectionTextID[gOptionsScreenData.mConfirmationDialog.mSelectedOption], getMugenTextColorFromMugenTextColorIndex(8));

	gOptionsScreenData.mConfirmationDialog.mSelectedOption ^= 1;
	auto textPos = getMugenTextPosition(gOptionsScreenData.mConfirmationDialog.mSelectionTextID[gOptionsScreenData.mConfirmationDialog.mSelectedOption]);
	textPos.z = 0.0;
	setBoxCursorPosition(gOptionsScreenData.mConfirmationDialog.mBoxCursorID, textPos);
	setMugenTextColor(gOptionsScreenData.mConfirmationDialog.mSelectionTextID[gOptionsScreenData.mConfirmationDialog.mSelectedOption], getMugenTextColorFromMugenTextColorIndex(0));
}

static void selectOptionScreenConfirmationDialogSelection() {
	if (gOptionsScreenData.mConfirmationDialog.mSelectedOption == 0) {
		gOptionsScreenData.mConfirmationDialog.mFunc();
	}
	setOptionScreenConfirmationDialogInactive();
}

static void updateOptionScreenConfirmationDialog() {
	if (hasPressedLeftFlank() || hasPressedRightFlank()) {
		updateOptionScreenConfirmationDialogSelection();
	}

	if (hasPressedAFlank() || hasPressedStartFlank()) {
		selectOptionScreenConfirmationDialogSelection();
	}
	else if (hasPressedBFlank()) {
		setOptionScreenConfirmationDialogInactive();
	}
}

static void updateOptionScreenSelection() {
	if (gOptionsScreenData.mConfirmationDialog.mIsActive) {
		updateOptionScreenConfirmationDialog();
		return;
	}

	if (gOptionsScreenData.mActiveScreen == OPTION_SCREEN_GENERAL) {
		updateOptionScreenGeneral();
	} else if (gOptionsScreenData.mActiveScreen == OPTION_SCREEN_INPUT_CONFIG) {
		updateOptionScreenInputConfig();
	} else if (gOptionsScreenData.mActiveScreen == OPTION_SCREEN_KEY_CONFIG) {
		updateOptionScreenKeyConfig();
	} else if (gOptionsScreenData.mActiveScreen == OPTION_SCREEN_DREAMCAST_SAVE) {
		updateOptionScreenDreamcastSave();
	}
}

static void updateOptionsScreen() {
	updateOptionScreenSelection();
}

static Screen gOptionsScreen;

static Screen* getOptionsScreen() {
	gOptionsScreen = makeScreen(loadOptionsScreen, updateOptionsScreen);
	return &gOptionsScreen;
};

void startOptionsScreen()
{
	setNewScreen(getOptionsScreen());
}
