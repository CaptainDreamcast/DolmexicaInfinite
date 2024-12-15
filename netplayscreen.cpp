#include "netplayscreen.h"

#include <prism/wrapper.h>
#include <prism/mugendefreader.h>
#include <prism/mugenspritefilereader.h>
#include <prism/mugenanimationhandler.h>
#include <prism/mugensoundfilereader.h>
#include <prism/mugentexthandler.h>
#include <prism/screeneffect.h>
#include <prism/input.h>
#include <prism/log.h>
#include <prism/netplay.h>

#include "boxcursorhandler.h"
#include "config.h"
#include "scriptbackground.h"
#include "titlescreen.h"
#include "netplaylogic.h"
#include "stage.h"
#include "characterselectscreen.h"
#include "playerdefinition.h"
#include "gamelogic.h"
#include "fightscreen.h"

#define NETPLAY_POPUP_BG_Z 82
#define NETPLAY_POPUP_TEXT_Z 83

static Screen* getNetplayScreen();

typedef struct
{
	Vector3DI mCursorMoveSound;
	Vector3DI mCursorDoneSound;
	Vector3DI mCancelSound;
} NetplayHeader;

typedef enum {
	NETPLAY_SCREEN_GENERAL,
	NETPLAY_SCREEN_HOST,
	NETPLAY_SCREEN_JOIN,
} NetplayScreenType;

typedef enum {
	GENERAL_SETTING_HOST,
	GENERAL_SETTING_JOIN,
	GENERAL_SETTING_RETURN,
	GENERAL_SETTING_AMOUNT
} GeneralSettingType;

typedef struct {
	int mBoxCursorID;

	int mSelected;

	int mSelectableTextID[GENERAL_SETTING_AMOUNT];
	int mSettingTextID[GENERAL_SETTING_AMOUNT];
} NetplayGeneralScreen;

typedef struct {
	int mWaitingTextID;

	int mReturnTextID;
	int mReturnTextButtonID;
} NetplayHostScreen;

typedef struct {
	int mEnterIPTextID;
	int mIPTextID;
	bool mIsIPStringInitialized;
	std::string mIPString;

	int mPointerPosition;

	int mConfirmTextID;
	int mConfirmTextButtonID;

	int mReturnTextID;
	int mReturnTextButtonID;

	AnimationHandlerElement* mConsolePointerAnimationElement;
} NetplayJoinScreen;

static struct 
{
	int mTextFontID;

	int mHeaderTextID;
	AnimationHandlerElement* mBackgroundAnimationElement;

	MugenDefScript mScript;
	MugenSpriteFile mSprites;
	MugenAnimations mAnimations;
	MugenSounds mSounds;

	NetplayHeader mHeader;
	NetplayGeneralScreen mGeneral;
	NetplayHostScreen mHost;
	NetplayJoinScreen mJoin;

	NetplayScreenType mActiveScreen;

} gNetplayScreenData;

static struct {
	int mIsActive;
	std::string mMessage;

	AnimationHandlerElement* mBG;
	int mTextID;
	int mConfirmID;
} gNetplayPopupData;

static void loadNetplayPopup(void*) {
	gNetplayPopupData.mBG = playOneFrameAnimationLoop(Vector3D(60, 100, NETPLAY_POPUP_BG_Z), getEmptyWhiteTextureReference());
	setAnimationSize(gNetplayPopupData.mBG, Vector3D(200, 40, 1), Vector3D(0, 0, 0));
	setAnimationColor(gNetplayPopupData.mBG, 0, 0, 0.6);
	setAnimationTransparency(gNetplayPopupData.mBG, 0.7);
	gNetplayPopupData.mTextID = addMugenTextMugenStyle(gNetplayPopupData.mMessage.c_str(), Vector3D(160, 120, NETPLAY_POPUP_TEXT_Z), Vector3DI(-1, 0, 0));
	gNetplayPopupData.mConfirmID = addMugenTextMugenStyle("Press start to return to main menu.", Vector3D(160, 130, NETPLAY_POPUP_TEXT_Z), Vector3DI(-1, 0, 0));
	gNetplayPopupData.mIsActive = true;
}

static void updateNetplayPopup(void*) {
	if (hasPressedStartFlank())
	{
		setNewScreen(getDreamTitleScreen());
	}
}

static ActorBlueprint getNetplayPopup() {
	return makeActorBlueprint(loadNetplayPopup, NULL, updateNetplayPopup);
}

static void setNetplayPopupActive(const std::string& tMessage)
{
	if (gNetplayPopupData.mIsActive) return;

	gNetplayPopupData.mMessage = tMessage;
	const auto id = instantiateActor(getNetplayPopup());
	setActorUnpausable(id);
	pauseWrapper();
}

static void dolmexicaNetplayDisconnectCB(void*, const std::string& tReason) {
	if (tReason == "version")
	{
		setNetplayPopupActive("Incompatible Dolmexica versions.");
	}
	else
	{
		setNetplayPopupActive("Other player has disconnected.");
	}
}

static void dolmexicaNetplayDesyncCB(void*) {
	setNetplayPopupActive("Desync detected.");
}

static void resetNetplayPopup()
{
	gNetplayPopupData.mIsActive = false;
}

static void loadNetplayHeader() {
	gNetplayScreenData.mHeader.mCursorMoveSound = getMugenDefVectorIOrDefault(&gNetplayScreenData.mScript, "netplay info", "cursor.move.snd", Vector3DI(1, 0, 0));
	gNetplayScreenData.mHeader.mCursorDoneSound = getMugenDefVectorIOrDefault(&gNetplayScreenData.mScript, "netplay info", "cursor.done.snd", Vector3DI(1, 0, 0));
	gNetplayScreenData.mHeader.mCancelSound = getMugenDefVectorIOrDefault(&gNetplayScreenData.mScript, "netplay info", "cancel.snd", Vector3DI(1, 0, 0));
}

static void setSelectedGeneralOptionInactive() {
	setMugenTextColor(gNetplayScreenData.mGeneral.mSelectableTextID[gNetplayScreenData.mGeneral.mSelected], getMugenTextColorFromMugenTextColorIndex(8));
}

static void setSelectedGeneralOptionActive() {
	setMugenTextColor(gNetplayScreenData.mGeneral.mSelectableTextID[gNetplayScreenData.mGeneral.mSelected], getMugenTextColorFromMugenTextColorIndex(0));

	Position pos = getMugenTextPosition(gNetplayScreenData.mGeneral.mSelectableTextID[gNetplayScreenData.mGeneral.mSelected]);
	pos.z = 0;
	setBoxCursorPosition(gNetplayScreenData.mGeneral.mBoxCursorID, pos);
}

static void updateSelectedGeneralOption(int tDelta) {
	setSelectedGeneralOptionInactive();
	gNetplayScreenData.mGeneral.mSelected += tDelta;
	gNetplayScreenData.mGeneral.mSelected = (gNetplayScreenData.mGeneral.mSelected + GENERAL_SETTING_AMOUNT) % GENERAL_SETTING_AMOUNT;
	tryPlayMugenSoundAdvanced(&gNetplayScreenData.mSounds, gNetplayScreenData.mHeader.mCursorMoveSound.x, gNetplayScreenData.mHeader.mCursorMoveSound.y, parseGameMidiVolumeToPrism(getGameMidiVolume()));
	setSelectedGeneralOptionActive();
}

static void loadGeneralNetplayScreen() {
	setAnimationPosition(gNetplayScreenData.mBackgroundAnimationElement, Vector3D(52, 35, 40));
	setAnimationSize(gNetplayScreenData.mBackgroundAnimationElement, Vector3D(216, 48, 1), Vector3D(0, 0, 0));

	changeMugenText(gNetplayScreenData.mHeaderTextID, "NETPLAY");
	setMugenTextPosition(gNetplayScreenData.mHeaderTextID, Vector3D(160, 20, 45));
	gNetplayScreenData.mActiveScreen = NETPLAY_SCREEN_GENERAL;

	int offsetX = 70;
	int startY = 50;
	int offsetY = 13;
	gNetplayScreenData.mGeneral.mSelectableTextID[GENERAL_SETTING_HOST] = addMugenTextMugenStyle("Host", Vector3D(offsetX, startY, 45), Vector3DI(gNetplayScreenData.mTextFontID, 8, 1));
	gNetplayScreenData.mGeneral.mSettingTextID[GENERAL_SETTING_HOST] = addMugenTextMugenStyle("", Vector3D(320 - offsetX, startY, 45), Vector3DI(gNetplayScreenData.mTextFontID, 8, -1));

	gNetplayScreenData.mGeneral.mSelectableTextID[GENERAL_SETTING_JOIN] = addMugenTextMugenStyle("Join", Vector3D(offsetX, startY + offsetY * 1, 45), Vector3DI(gNetplayScreenData.mTextFontID, 8, 1));
	gNetplayScreenData.mGeneral.mSettingTextID[GENERAL_SETTING_JOIN] = addMugenTextMugenStyle("", Vector3D(320 - offsetX, startY + offsetY * 1, 45), Vector3DI(gNetplayScreenData.mTextFontID, 8, -1));

	gNetplayScreenData.mGeneral.mSelectableTextID[GENERAL_SETTING_RETURN] = addMugenTextMugenStyle("Return to Main Menu", Vector3D(offsetX, startY + offsetY * 2, 45), Vector3DI(gNetplayScreenData.mTextFontID, 8, 1));
	gNetplayScreenData.mGeneral.mSettingTextID[GENERAL_SETTING_RETURN] = addMugenTextMugenStyle("(Esc)", Vector3D(320 - offsetX, startY + offsetY * 2, 45), Vector3DI(gNetplayScreenData.mTextFontID, 5, -1));
	
	gNetplayScreenData.mGeneral.mBoxCursorID = addBoxCursor(Vector3D(0, 0, 0), Vector3D(0, 0, 43), GeoRectangle2D(-6, -11, 320 - 2 * offsetX + 10, 14));

	setSelectedGeneralOptionActive();
}

static void unloadGeneralNetplayScreen() {
	for (int i = 0; i < GENERAL_SETTING_AMOUNT; i++) {
		removeMugenText(gNetplayScreenData.mGeneral.mSelectableTextID[i]);
		removeMugenText(gNetplayScreenData.mGeneral.mSettingTextID[i]);
	}

	removeBoxCursor(gNetplayScreenData.mGeneral.mBoxCursorID);
}

static void netplayFightFinishedCB()
{
	setNewScreen(getDreamTitleScreen());
}

static void netplayCharacterSelectFinishedCB() {
	setGameModeNetplay(isDolmexicaNetplayHost());
	startFightScreen(netplayFightFinishedCB);
}

static void netplayCharacterSelectScreen()
{
	setCharacterSelectScreenModeName("Netplay");
	setCharacterSelectNetplay(isDolmexicaNetplayHost());
	setCharacterSelectStageActive();
	resetCharacterSelectSelectors();
	setCharacterSelectFinishedCB(netplayCharacterSelectFinishedCB);
	setNewScreen(getCharacterSelectScreen());
}

static void peerJoinedCB(void*)
{
	setWrapperAbortEnabled(true);
	cancelWaitingForCharacterFromUserInput(0);
	cancelWaitingForButtonFromUserInput(0);
	logg("peer joined");
	netplayCharacterSelectScreen();
}

static void loadHostNetplayScreen()
{
	setAnimationPosition(gNetplayScreenData.mBackgroundAnimationElement, Vector3D(52, 35, 40));
	setAnimationSize(gNetplayScreenData.mBackgroundAnimationElement, Vector3D(216, 48, 1), Vector3D(0, 0, 0));

	changeMugenText(gNetplayScreenData.mHeaderTextID, "HOST");
	setMugenTextPosition(gNetplayScreenData.mHeaderTextID, Vector3D(160, 20, 45));
	gNetplayScreenData.mActiveScreen = NETPLAY_SCREEN_HOST;

	int offsetX = 70;
	int startY = 50;
	int offsetY = 13;
	gNetplayScreenData.mHost.mWaitingTextID = addMugenTextMugenStyle("Waiting for join...", Vector3D(offsetX, startY, 45), Vector3DI(gNetplayScreenData.mTextFontID, 0, 1));
	gNetplayScreenData.mHost.mReturnTextID = addMugenTextMugenStyle("Abort", Vector3D(offsetX, startY + offsetY * 2, 45), Vector3DI(gNetplayScreenData.mTextFontID, 0, 1));
	gNetplayScreenData.mHost.mReturnTextButtonID = addMugenTextMugenStyle("(Esc)", Vector3D(320 - offsetX, startY + offsetY * 2, 45), Vector3DI(gNetplayScreenData.mTextFontID, 5, -1));

	setWrapperAbortEnabled(false);
	startDolmexicaNetplayHost(peerJoinedCB);
}

static void unloadHostNetplayScreen() {
	removeMugenText(gNetplayScreenData.mHost.mWaitingTextID);
	removeMugenText(gNetplayScreenData.mHost.mReturnTextID);
	removeMugenText(gNetplayScreenData.mHost.mReturnTextButtonID);

	setWrapperAbortEnabled(true);
	stopDolmexicaNetplayHost();
}

static void updateIPText()
{
	double offset;
	if (!gNetplayScreenData.mJoin.mPointerPosition) {
		offset = 0;
	}
	else {
		std::string prefix = gNetplayScreenData.mJoin.mIPString.substr(0, gNetplayScreenData.mJoin.mPointerPosition);
		changeMugenText(gNetplayScreenData.mJoin.mIPTextID, prefix.data());
		offset = getMugenTextSizeX(gNetplayScreenData.mJoin.mIPTextID) + 1;
	}
	changeMugenText(gNetplayScreenData.mJoin.mIPTextID, gNetplayScreenData.mJoin.mIPString.data());
	setAnimationPosition(gNetplayScreenData.mJoin.mConsolePointerAnimationElement, Vector3D(70 + offset, 63, 46));
}

static void tryJoinIP()
{
	if (isNetplayActive()) return;

	if (tryDolmexicaNetplayJoin(gNetplayScreenData.mJoin.mIPString, peerJoinedCB))
	{
		logFormat("Join successful to %s", gNetplayScreenData.mJoin.mIPString.c_str());
	}
}

static void joinCharacterInputReceived(void* /*tCaller*/, const std::string& tText) {
	gNetplayScreenData.mJoin.mIPString.insert(gNetplayScreenData.mJoin.mPointerPosition, tText);
	gNetplayScreenData.mJoin.mPointerPosition += int(tText.size());
	updateIPText();
}

static void joinKeyboardInputReceived(void* tCaller, KeyboardKeyPrism tKey) {
	(void)tCaller;

	if (tKey == KEYBOARD_BACKSPACE_PRISM) {
		if (gNetplayScreenData.mJoin.mPointerPosition > 0) {
			gNetplayScreenData.mJoin.mIPString.erase(gNetplayScreenData.mJoin.mPointerPosition - 1, 1);
			gNetplayScreenData.mJoin.mPointerPosition--;
			updateIPText();
		}
	}
	else if (tKey == KEYBOARD_DELETE_PRISM) {
		if (gNetplayScreenData.mJoin.mPointerPosition < (int)gNetplayScreenData.mJoin.mIPString.size()) {
			gNetplayScreenData.mJoin.mIPString.erase(gNetplayScreenData.mJoin.mPointerPosition, 1);
			updateIPText();
		}
	}
	else if (tKey == KEYBOARD_LEFT_PRISM) {
		if (gNetplayScreenData.mJoin.mPointerPosition > 0) {
			gNetplayScreenData.mJoin.mPointerPosition--;
			updateIPText();
		}
	}
	else if (tKey == KEYBOARD_RIGHT_PRISM) {
		if (gNetplayScreenData.mJoin.mPointerPosition < (int)gNetplayScreenData.mJoin.mIPString.size()) {
			gNetplayScreenData.mJoin.mPointerPosition++;
			updateIPText();
		}
	}
	else if (tKey == KEYBOARD_RETURN_PRISM) {
		tryJoinIP();
	}

	waitForButtonFromUserInputForKeyboard(0, joinKeyboardInputReceived);
}

static void loadJoinNetplayScreen()
{
	setAnimationPosition(gNetplayScreenData.mBackgroundAnimationElement, Vector3D(52, 35, 40));
	setAnimationSize(gNetplayScreenData.mBackgroundAnimationElement, Vector3D(216, 74, 1), Vector3D(0, 0, 0));

	changeMugenText(gNetplayScreenData.mHeaderTextID, "JOIN");
	setMugenTextPosition(gNetplayScreenData.mHeaderTextID, Vector3D(160, 20, 45));
	gNetplayScreenData.mActiveScreen = NETPLAY_SCREEN_JOIN;

	int offsetX = 70;
	int startY = 50;
	int offsetY = 13;
	gNetplayScreenData.mJoin.mEnterIPTextID = addMugenTextMugenStyle("Enter Host IP:", Vector3D(offsetX, startY, 45), Vector3DI(gNetplayScreenData.mTextFontID, 0, 1));

	gNetplayScreenData.mJoin.mIPTextID = addMugenTextMugenStyle("", Vector3D(offsetX, startY + offsetY * 1, 45), Vector3DI(gNetplayScreenData.mTextFontID, 0, 1));

	gNetplayScreenData.mJoin.mConfirmTextID = addMugenTextMugenStyle("Confirm", Vector3D(offsetX, startY + offsetY * 3, 45), Vector3DI(gNetplayScreenData.mTextFontID, 0, 1));
	gNetplayScreenData.mJoin.mConfirmTextButtonID = addMugenTextMugenStyle("(Enter)", Vector3D(320 - offsetX, startY + offsetY * 3, 45), Vector3DI(gNetplayScreenData.mTextFontID, 5, -1));

	gNetplayScreenData.mJoin.mReturnTextID = addMugenTextMugenStyle("Abort", Vector3D(offsetX, startY + offsetY * 4, 45), Vector3DI(gNetplayScreenData.mTextFontID, 0, 1));
	gNetplayScreenData.mJoin.mReturnTextButtonID = addMugenTextMugenStyle("(Esc)", Vector3D(320 - offsetX, startY + offsetY * 4, 45), Vector3DI(gNetplayScreenData.mTextFontID, 5, -1));

	if (!gNetplayScreenData.mJoin.mIsIPStringInitialized)
	{
		gNetplayScreenData.mJoin.mIPString = "192.168.178.46"; // DEBUG
		gNetplayScreenData.mJoin.mIsIPStringInitialized = true;
	}
	gNetplayScreenData.mJoin.mPointerPosition = int(gNetplayScreenData.mJoin.mIPString.size());

	gNetplayScreenData.mJoin.mConsolePointerAnimationElement = playOneFrameAnimationLoop(Vector3D(20, 90, 45), getEmptyWhiteTextureReference());
	setAnimationSize(gNetplayScreenData.mJoin.mConsolePointerAnimationElement, Vector3D(6, 1, 1), Vector3D(0, 0, 0));
	setAnimationColor(gNetplayScreenData.mJoin.mConsolePointerAnimationElement, 1, 1, 1);
	updateIPText();

	waitForCharacterFromUserInput(0, joinCharacterInputReceived);
	waitForButtonFromUserInputForKeyboard(0, joinKeyboardInputReceived);
	setWrapperAbortEnabled(false);
}

static void unloadJoinNetplayScreen() {
	removeMugenText(gNetplayScreenData.mJoin.mEnterIPTextID);
	removeMugenText(gNetplayScreenData.mJoin.mIPTextID);
	removeMugenText(gNetplayScreenData.mJoin.mConfirmTextID);
	removeMugenText(gNetplayScreenData.mJoin.mConfirmTextButtonID);
	removeMugenText(gNetplayScreenData.mJoin.mReturnTextID);
	removeMugenText(gNetplayScreenData.mJoin.mReturnTextButtonID);
	removeHandledAnimation(gNetplayScreenData.mJoin.mConsolePointerAnimationElement);

	cancelWaitingForCharacterFromUserInput(0);
	cancelWaitingForButtonFromUserInput(0);
	setWrapperAbortEnabled(true);
}

static void loadNetplayScreen() {
	gNetplayScreenData.mTextFontID = hasMugenFont(2) ? 2 : -1;

	instantiateActor(getBoxCursorHandler());

	const auto motifPath = getDolmexicaAssetFolder() + getMotifPath();
	loadMugenDefScript(&gNetplayScreenData.mScript, motifPath);
	std::string folder;
	getPathToFile(folder, motifPath.c_str());

	auto text = getSTLMugenDefStringVariable(&gNetplayScreenData.mScript, "files", "spr");
	text = findMugenSystemOrFightFilePath(text, folder);
	gNetplayScreenData.mSprites = loadMugenSpriteFileWithoutPalette(text);
	gNetplayScreenData.mAnimations = loadMugenAnimationFile(motifPath.c_str());

	text = getSTLMugenDefStringVariable(&gNetplayScreenData.mScript, "files", "snd");
	text = findMugenSystemOrFightFilePath(text, folder);
	gNetplayScreenData.mSounds = loadMugenSoundFile(text.c_str());

	loadNetplayHeader();
	loadScriptBackground(&gNetplayScreenData.mScript, &gNetplayScreenData.mSprites, &gNetplayScreenData.mAnimations, "netplaybgdef", "netplaybg");

	gNetplayScreenData.mHeaderTextID = addMugenTextMugenStyle("OPTIONS", Vector3D(160, 20, 45), Vector3DI(gNetplayScreenData.mTextFontID, 0, 0));
	gNetplayScreenData.mBackgroundAnimationElement = playOneFrameAnimationLoop(Vector3D(52, 35, 40), getEmptyWhiteTextureReference());
	setAnimationColor(gNetplayScreenData.mBackgroundAnimationElement, 0, 0, 0.6);
	setAnimationTransparency(gNetplayScreenData.mBackgroundAnimationElement, 0.7);

	resetNetplayPopup();
	initDolmexicaNetplay();
	setNetplayDesyncCB(dolmexicaNetplayDesyncCB, NULL);
	setNetplayDisconnectCB(dolmexicaNetplayDisconnectCB, NULL);

	loadGeneralNetplayScreen();

	setDrawingFrameSkippingEnabled(true);
}

static void generalToHost() {
	if (gNetplayScreenData.mActiveScreen != NETPLAY_SCREEN_GENERAL) return;

	unloadGeneralNetplayScreen();
	loadHostNetplayScreen();
}

static void generalToJoin() {
	if (gNetplayScreenData.mActiveScreen != NETPLAY_SCREEN_GENERAL) return;

	unloadGeneralNetplayScreen();
	loadJoinNetplayScreen();
}

static void updateNetplayScreenGeneralSelect() {
	if (gNetplayScreenData.mGeneral.mSelected == GENERAL_SETTING_HOST) {
		generalToHost();
	}
	else if (gNetplayScreenData.mGeneral.mSelected == GENERAL_SETTING_JOIN) {
		generalToJoin();
	}
	else if (gNetplayScreenData.mGeneral.mSelected == GENERAL_SETTING_RETURN) {
		tryPlayMugenSoundAdvanced(&gNetplayScreenData.mSounds, gNetplayScreenData.mHeader.mCancelSound.x, gNetplayScreenData.mHeader.mCancelSound.y, parseGameMidiVolumeToPrism(getGameMidiVolume()));
		setNewScreen(getDreamTitleScreen());
	}
}

static void updateNetplayScreenGeneral() {
	if (hasPressedUpFlank()) {
		updateSelectedGeneralOption(-1);
	}
	else if (hasPressedDownFlank()) {
		updateSelectedGeneralOption(1);
	}

	if (hasPressedAFlank() || hasPressedStartFlank()) {
		updateNetplayScreenGeneralSelect();
	}

	if (hasPressedBFlank()) {
		setNewScreen(getDreamTitleScreen());
	}
}

static void updateNetplayScreenHost() {
	if (hasPressedBFlank() || hasPressedAbortFlank()) {
		consumeAbortFlank();
		unloadHostNetplayScreen();
		loadGeneralNetplayScreen();
	}
}

static void updateNetplayScreenJoin() {
	if (hasPressedAbortFlank()) {
		consumeAbortFlank();
		unloadJoinNetplayScreen();
		loadGeneralNetplayScreen();
	}
}

static void updateNetplayScreenSelection() {
	if (gNetplayScreenData.mActiveScreen == NETPLAY_SCREEN_GENERAL) {
		updateNetplayScreenGeneral();
	}
	else if (gNetplayScreenData.mActiveScreen == NETPLAY_SCREEN_HOST) {
		updateNetplayScreenHost();
	}
	else if (gNetplayScreenData.mActiveScreen == NETPLAY_SCREEN_JOIN) {
		updateNetplayScreenJoin();
	}
}

static void updateNetplayScreen() {
	updateNetplayScreenSelection();
	updateDolmexicaNetplay();
}

static Screen gNetplayScreen;

static Screen* getNetplayScreen() {
	gNetplayScreen = makeScreen(loadNetplayScreen, updateNetplayScreen);
	return &gNetplayScreen;
};

void startNetplayScreen()
{
	setNewScreen(getNetplayScreen());
}
