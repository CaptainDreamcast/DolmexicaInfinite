#include "optionsscreen.h"

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

#include "titlescreen.h"
#include "menubackground.h"
#include "boxcursorhandler.h"

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
	GENERAL_SETTING_LOAD_SAVE,
	GENERAL_SETTING_DEFAULT_VALUES,
	GENERAL_SETTING_RETURN,
	GENERAL_SETTING_AMOUNT
} GeneralSettingType;


typedef struct {
	int mBackgroundAnimationID;
	int mBoxCursorID;

	int mSelected;
	
	int mHeaderTextID;
	int mSelectableTextID[11];
	int mSettingTextID[11];
} GeneralOptionsScreen;

typedef enum {
	OPTION_SCREEN_GENERAL,
	OPTION_SCREEN_INPUT_CONFIG,
	OPTION_SCREEN_INPUT_SET,
} OptionScreenType;

static struct {
	TextureData mWhiteTexture;
	int mBackgroundBoxID;

	MugenDefScript mScript;
	MugenSpriteFile mSprites;
	MugenAnimations mAnimations;
	MugenSounds mSounds;

	OptionsHeader mHeader;
	GeneralOptionsScreen mGeneral;

	OptionScreenType mActiveScreen;
} gData;

static void loadOptionsHeader() {
	gData.mHeader.mCursorMoveSound = getMugenDefVectorIOrDefault(&gData.mScript, "Option Info", "cursor.move.snd", makeVector3DI(1, 0, 0));
	gData.mHeader.mCursorDoneSound = getMugenDefVectorIOrDefault(&gData.mScript, "Option Info", "cursor.done.snd", makeVector3DI(1, 0, 0));
	gData.mHeader.mCancelSound = getMugenDefVectorIOrDefault(&gData.mScript, "Option Info", "cancel.snd", makeVector3DI(1, 0, 0));

}

static void setSelectedGeneralOptionInactive() {
	setMugenTextColor(gData.mGeneral.mSelectableTextID[gData.mGeneral.mSelected], getMugenTextColorFromMugenTextColorIndex(8));

	Vector3D selectionColor = getMugenTextColor(gData.mGeneral.mSettingTextID[gData.mGeneral.mSelected]);
	if (selectionColor.z != 0) {
		setMugenTextColor(gData.mGeneral.mSettingTextID[gData.mGeneral.mSelected], getMugenTextColorFromMugenTextColorIndex(8));
	}
}

static void setSelectedGeneralOptionActive() {
	setMugenTextColor(gData.mGeneral.mSelectableTextID[gData.mGeneral.mSelected], getMugenTextColorFromMugenTextColorIndex(0));

	Vector3D selectionColor = getMugenTextColor(gData.mGeneral.mSettingTextID[gData.mGeneral.mSelected]);
	if (selectionColor.z != 0) {
		setMugenTextColor(gData.mGeneral.mSettingTextID[gData.mGeneral.mSelected], getMugenTextColorFromMugenTextColorIndex(0));
	}

	Position pos = getMugenTextPosition(gData.mGeneral.mSelectableTextID[gData.mGeneral.mSelected]);
	pos.z = 0;
	setBoxCursorPosition(gData.mGeneral.mBoxCursorID, pos);
}

static void updateSelectedGeneralOption(int tDelta) {
	setSelectedGeneralOptionInactive();
	gData.mGeneral.mSelected += tDelta;
	gData.mGeneral.mSelected = (gData.mGeneral.mSelected + GENERAL_SETTING_AMOUNT) % GENERAL_SETTING_AMOUNT;
	tryPlayMugenSound(&gData.mSounds, gData.mHeader.mCursorMoveSound.x, gData.mHeader.mCursorMoveSound.y);
	setSelectedGeneralOptionActive();
}

static void loadGeneralOptionsScreen() {
	gData.mGeneral.mBackgroundAnimationID = playOneFrameAnimationLoop(makePosition(52, 35, 40), &gData.mWhiteTexture);
	setAnimationSize(gData.mGeneral.mBackgroundAnimationID, makePosition(216, 185, 1), makePosition(0, 0, 0));
	setAnimationColor(gData.mGeneral.mBackgroundAnimationID, 0, 0, 0.6);
	setAnimationTransparency(gData.mGeneral.mBackgroundAnimationID, 0.7);
	

	gData.mGeneral.mHeaderTextID = addMugenTextMugenStyle("OPTIONS", makePosition(160, 20, 60), makeVector3DI(2, 0, 0));

	int offsetX = 70;
	int startY = 50;
	int offsetY = 13;
	gData.mGeneral.mSelectableTextID[GENERAL_SETTING_DIFFICULTY] = addMugenTextMugenStyle("Difficulty", makePosition(offsetX, startY, 60), makeVector3DI(2, 8, 1));
	gData.mGeneral.mSettingTextID[GENERAL_SETTING_DIFFICULTY] = addMugenTextMugenStyle("Hard 8", makePosition(320 - offsetX, startY, 60), makeVector3DI(2, 8, -1));

	gData.mGeneral.mSelectableTextID[GENERAL_SETTING_LIFE] = addMugenTextMugenStyle("Life", makePosition(offsetX, startY + offsetY * 1, 60), makeVector3DI(2, 8, 1));
	gData.mGeneral.mSettingTextID[GENERAL_SETTING_LIFE] = addMugenTextMugenStyle("100%", makePosition(320 - offsetX, startY + offsetY * 1, 60), makeVector3DI(2, 8, -1));

	gData.mGeneral.mSelectableTextID[GENERAL_SETTING_TIME_LIMIT] = addMugenTextMugenStyle("Time Limit", makePosition(offsetX, startY + offsetY * 2, 60), makeVector3DI(2, 8, 1));
	gData.mGeneral.mSettingTextID[GENERAL_SETTING_TIME_LIMIT] = addMugenTextMugenStyle("99", makePosition(320 - offsetX, startY + offsetY * 2, 60), makeVector3DI(2, 8, -1));

	gData.mGeneral.mSelectableTextID[GENERAL_SETTING_GAME_SPEED] = addMugenTextMugenStyle("Game Speed", makePosition(offsetX, startY + offsetY * 3, 60), makeVector3DI(2, 8, 1));
	gData.mGeneral.mSettingTextID[GENERAL_SETTING_GAME_SPEED] = addMugenTextMugenStyle("Normal", makePosition(320 - offsetX, startY + offsetY * 3, 60), makeVector3DI(2, 8, -1));

	gData.mGeneral.mSelectableTextID[GENERAL_SETTING_WAV_VOLUME] = addMugenTextMugenStyle("Wav Volume", makePosition(offsetX, startY + offsetY * 4, 60), makeVector3DI(2, 8, 1));
	gData.mGeneral.mSettingTextID[GENERAL_SETTING_WAV_VOLUME] = addMugenTextMugenStyle("50", makePosition(320 - offsetX, startY + offsetY * 4, 60), makeVector3DI(2, 8, -1));

	gData.mGeneral.mSelectableTextID[GENERAL_SETTING_MIDI_VOLUME] = addMugenTextMugenStyle("Midi Volume", makePosition(offsetX, startY + offsetY * 5, 60), makeVector3DI(2, 8, 1));
	gData.mGeneral.mSettingTextID[GENERAL_SETTING_MIDI_VOLUME] = addMugenTextMugenStyle("50", makePosition(320 - offsetX, startY + offsetY * 5, 60), makeVector3DI(2, 8, -1));

	gData.mGeneral.mSelectableTextID[GENERAL_SETTING_INPUT_CONFIG] = addMugenTextMugenStyle("Input Config", makePosition(offsetX, startY + offsetY * 6, 60), makeVector3DI(2, 8, 1));
	gData.mGeneral.mSettingTextID[GENERAL_SETTING_INPUT_CONFIG] = addMugenTextMugenStyle("(F1)", makePosition(320 - offsetX, startY + offsetY * 6, 60), makeVector3DI(2, 5, -1));

	gData.mGeneral.mSelectableTextID[GENERAL_SETTING_TEAM_MODE_CONFIG] = addMugenTextMugenStyle("Team Mode Config", makePosition(offsetX, startY + offsetY * 7, 60), makeVector3DI(2, 8, 1));
	gData.mGeneral.mSettingTextID[GENERAL_SETTING_TEAM_MODE_CONFIG] = addMugenTextMugenStyle("(F2)", makePosition(320 - offsetX, startY + offsetY * 7, 60), makeVector3DI(2, 5, -1));

	gData.mGeneral.mSelectableTextID[GENERAL_SETTING_LOAD_SAVE] = addMugenTextMugenStyle("Load/Save", makePosition(offsetX, startY + offsetY * 9, 60), makeVector3DI(2, 8, 1));
	gData.mGeneral.mSettingTextID[GENERAL_SETTING_LOAD_SAVE] = addMugenTextMugenStyle("Load", makePosition(320 - offsetX, startY + offsetY * 9, 60), makeVector3DI(2, 8, -1));

	gData.mGeneral.mSelectableTextID[GENERAL_SETTING_DEFAULT_VALUES] = addMugenTextMugenStyle("Default Values", makePosition(offsetX, startY + offsetY * 10, 60), makeVector3DI(2, 8, 1));
	gData.mGeneral.mSettingTextID[GENERAL_SETTING_DEFAULT_VALUES] = addMugenTextMugenStyle("", makePosition(320 - offsetX, startY + offsetY * 10, 60), makeVector3DI(2, 8, -1));

	gData.mGeneral.mSelectableTextID[GENERAL_SETTING_RETURN] = addMugenTextMugenStyle("Return to Main Menu", makePosition(offsetX, startY + offsetY * 12, 60), makeVector3DI(2, 8, 1));
	gData.mGeneral.mSettingTextID[GENERAL_SETTING_RETURN] = addMugenTextMugenStyle("(Esc)", makePosition(320 - offsetX, startY + offsetY * 12, 60), makeVector3DI(2, 5, -1));

	gData.mGeneral.mBoxCursorID = addBoxCursor(makePosition(0, 0, 0), makePosition(0, 0, 50), makeGeoRectangle(-5, -3.5, 320 - 2 * offsetX + 10, 15));

	setSelectedGeneralOptionActive();

	gData.mActiveScreen = OPTION_SCREEN_GENERAL;
}

static void loadOptionsScreen() {
	instantiateActor(MugenTextHandler);
	instantiateActor(getMugenAnimationHandlerActorBlueprint());
	instantiateActor(ClipboardHandler);
	instantiateActor(BoxCursorHandler);

	gData.mWhiteTexture = getEmptyWhiteTexture();

	char folder[1024];
	gData.mScript = loadMugenDefScript("assets/data/system.def");
	getPathToFile(folder, "assets/data/system.def");
	setWorkingDirectory(folder);

	char* text = getAllocatedMugenDefStringVariable(&gData.mScript, "Files", "spr");
	gData.mSprites = loadMugenSpriteFileWithoutPalette(text);
	gData.mAnimations = loadMugenAnimationFile("system.def");
	freeMemory(text);

	text = getAllocatedMugenDefStringVariable(&gData.mScript, "Files", "snd");
	gData.mSounds = loadMugenSoundFile(text);
	freeMemory(text);

	loadOptionsHeader();
	loadMenuBackground(&gData.mScript, &gData.mSprites, &gData.mAnimations, "OptionBGdef", "OptionBG");

	loadGeneralOptionsScreen();

	setWorkingDirectory("/");
}

static void updateOptionScreenSelect() {
	if (gData.mGeneral.mSelected == GENERAL_SETTING_RETURN) {
		tryPlayMugenSound(&gData.mSounds, gData.mHeader.mCancelSound.x, gData.mHeader.mCancelSound.y);
		setNewScreen(&DreamTitleScreen);
	}
}

static void updateOptionScreenGeneral() {
	if (hasPressedUpFlank()) {
		updateSelectedGeneralOption(-1);
	} else if (hasPressedDownFlank()) {
		updateSelectedGeneralOption(1);
	}

	if (hasPressedAFlank() || hasPressedStartFlank()) {
		updateOptionScreenSelect();
	}
}

static void updateOptionScreenSelection() {
	if (gData.mActiveScreen == OPTION_SCREEN_GENERAL) {
		updateOptionScreenGeneral();
	}
}

static void updateOptionsScreen() {
	updateOptionScreenSelection();

	if(hasPressedBFlank()) {
		setNewScreen(&DreamTitleScreen);
	}
}

static Screen OptionsScreen = {
	.mLoad = loadOptionsScreen,
	.mUpdate = updateOptionsScreen,
};

void startOptionsScreen()
{
	setNewScreen(&OptionsScreen);
}
