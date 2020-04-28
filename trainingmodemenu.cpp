#include "trainingmodemenu.h"

#include <assert.h>

#include <prism/mugentexthandler.h>
#include <prism/screeneffect.h>
#include <prism/input.h>

#include "playerdefinition.h"
#include "config.h"
#include "mugencommandhandler.h"
#include "gamelogic.h"
#include "fightdebug.h"
#include "mugenstagehandler.h"

#define TRAINING_MODE_MENU_BG_Z 80
#define TRAINING_MODE_MENU_TEXT_Z 81

enum TrainingModeMode : int {
	TRAINING_MODE_MODE_MANUAL = 0,
	TRAINING_MODE_MODE_COOPERATIVE,
	TRAINING_MODE_MODE_AI,
	TRAINING_MODE_MODE_AMOUNT,
};

enum ManualModeOptions : int {
	MANUAL_MODE_OPTION_MODE = 0,
	MANUAL_MODE_OPTION_AMOUNT,
};

enum CooperativeModeOptions : int {
	COOPERATIVE_MODE_OPTION_MODE = 0,
	COOPERATIVE_MODE_OPTION_GUARD_MODE,
	COOPERATIVE_MODE_OPTION_DUMMY_MODE,
	COOPERATIVE_MODE_OPTION_DISTANCE,
	COOPERATIVE_MODE_OPTION_BUTTON_JAM,
	COOPERATIVE_MODE_OPTION_AMOUNT,
};

enum AIModeOptions : int {
	AI_MODE_OPTION_MODE = 0,
	AI_MODE_OPTION_AI_LEVEL,
	AI_MODE_OPTION_AMOUNT,
};

enum GuardModeOptions : int {
	GUARD_MODE_NONE = 0,
	GUARD_MODE_AUTO,
	GUARD_MODE_AMOUNT,
};

enum DummyModeOptions : int {
	DUMMY_MODE_STAND = 0,
	DUMMY_MODE_CROUCH,
	DUMMY_MODE_JUMP,
	DUMMY_MODE_WJUMP,
	DUMMY_MODE_AMOUNT,
};

enum DistanceOptions : int {
	DISTANCE_ANY = 0,
	DISTANCE_CLOSE,
	DISTANCE_MEDIUM,
	DISTANCE_FAR,
	DISTANCE_AMOUNT,
};

enum ButtonJamOptions : int {
	BUTTON_JAM_NONE = 0,
	BUTTON_JAM_A,
	BUTTON_JAM_B,
	BUTTON_JAM_C,
	BUTTON_JAM_X,
	BUTTON_JAM_Y,
	BUTTON_JAM_Z,
	BUTTON_JAM_START,
	BUTTON_JAM_AMOUNT,
};

typedef struct {
	GuardModeOptions mGuardMode;
	DummyModeOptions mDummyMode;
	DistanceOptions mDistance;
	ButtonJamOptions mButtonJam;
} CooperativeModeData;

typedef struct {
	int mAILevel;
} AIModeData;

static struct {
	int mIsActive;
	int mIsVisible;
	TrainingModeMode mMode;

	AIModeData mAIMode;
	CooperativeModeData mCooperativeMode;

	AnimationHandlerElement* mBackgroundAnimationElement;
	int mTitleText;
	int mOptionTexts[COOPERATIVE_MODE_OPTION_AMOUNT][2];
	int mSelectedOption;
} gTrainingModeMenuData;

static void	setSelectedOptionActive();

static void loadTrainingModeMenu(void*) {
	setProfilingSectionMarkerCurrentFunction();
	gTrainingModeMenuData.mBackgroundAnimationElement = playOneFrameAnimationLoop(makePosition(78, 28, TRAINING_MODE_MENU_BG_Z), getEmptyWhiteTextureReference());
	setAnimationSize(gTrainingModeMenuData.mBackgroundAnimationElement, makePosition(164, 124, 1), makePosition(0, 0, 0));
	setAnimationColor(gTrainingModeMenuData.mBackgroundAnimationElement, 0, 0, 0.6);
	setAnimationTransparency(gTrainingModeMenuData.mBackgroundAnimationElement, 0.7);
	setAnimationVisibility(gTrainingModeMenuData.mBackgroundAnimationElement, 0);

	gTrainingModeMenuData.mTitleText = addMugenTextMugenStyle("Training Mode", makePosition(160, 40, TRAINING_MODE_MENU_TEXT_Z), makeVector3DI(-1, 0, 0));
	setMugenTextVisibility(gTrainingModeMenuData.mTitleText, 0);

	int y = 60;
	for (int i = 0; i < COOPERATIVE_MODE_OPTION_AMOUNT; i++) {
		gTrainingModeMenuData.mOptionTexts[i][0] = addMugenTextMugenStyle("Text", makePosition(90, y, TRAINING_MODE_MENU_TEXT_Z), makeVector3DI(-1, 8, 1));
		setMugenTextVisibility(gTrainingModeMenuData.mOptionTexts[i][0], 0);
		gTrainingModeMenuData.mOptionTexts[i][1] = addMugenTextMugenStyle("Text", makePosition(180, y, TRAINING_MODE_MENU_TEXT_Z), makeVector3DI(-1, 5, 1));
		setMugenTextVisibility(gTrainingModeMenuData.mOptionTexts[i][1], 0);
		y += 20;
	}

	setSelectedOptionActive();
	gTrainingModeMenuData.mAIMode.mAILevel = getDifficulty();

	gTrainingModeMenuData.mIsActive = 1;
	gTrainingModeMenuData.mIsVisible = 0;
}

static void unloadTrainingModeMenu(void*) {
	setProfilingSectionMarkerCurrentFunction();
	gTrainingModeMenuData.mIsActive = 0;
}

static void updateSettingTrainingModeMenuActive() {
	if (hasPressedKeyboardKeyFlank(KEYBOARD_F6_PRISM)) {
		switchDebugTimeOff();
	}
}

static int getCurrentModeOptionAmount() {
	switch (gTrainingModeMenuData.mMode)
	{
	case TRAINING_MODE_MODE_MANUAL:
		return MANUAL_MODE_OPTION_AMOUNT;
	case TRAINING_MODE_MODE_COOPERATIVE:
		return COOPERATIVE_MODE_OPTION_AMOUNT;
	case TRAINING_MODE_MODE_AI:
		return AI_MODE_OPTION_AMOUNT;
	default:
		assert(false && "Unrecognized training menu mode");
		return 0;
	}
}

static void setCurrentModeInactive() {
	for (int i = 0; i < getCurrentModeOptionAmount(); i++) {
		setMugenTextVisibility(gTrainingModeMenuData.mOptionTexts[i][0], 0);
		setMugenTextVisibility(gTrainingModeMenuData.mOptionTexts[i][1], 0);
	}
}

static void setGeneralModeActive() {
	for (int i = 0; i < getCurrentModeOptionAmount(); i++) {
		setMugenTextVisibility(gTrainingModeMenuData.mOptionTexts[i][0], 1);
		setMugenTextVisibility(gTrainingModeMenuData.mOptionTexts[i][1], 1);
	}
}

static void setOptionTextValue(int tIndex, const char* tValue) {
	changeMugenText(gTrainingModeMenuData.mOptionTexts[tIndex][1], tValue);
}

static void setManualModeActive() {
	setPlayerHuman(1);

	setGeneralModeActive();
	changeMugenText(gTrainingModeMenuData.mOptionTexts[MANUAL_MODE_OPTION_MODE][0], "Dummy control");
	setOptionTextValue(MANUAL_MODE_OPTION_MODE, "Manual");
}

static const char* getGuardModeText() {
	switch (gTrainingModeMenuData.mCooperativeMode.mGuardMode)
	{
	case GUARD_MODE_NONE:
		return "None";
	case GUARD_MODE_AUTO:
		return "Auto";
	default:
		assert(false && "Unrecognized training menu guard mode");
		return "";
	}
}

static const char* getDummyModeText() {
	switch (gTrainingModeMenuData.mCooperativeMode.mDummyMode)
	{
	case DUMMY_MODE_STAND:
		return "Stand";
	case DUMMY_MODE_CROUCH:
		return "Crouch";
	case DUMMY_MODE_JUMP:
		return "Jump";
	case DUMMY_MODE_WJUMP:
		return "W Jump";
	default:
		assert(false && "Unrecognized training menu dummy mode");
		return "";
	}
}

static const char* getDistanceText() {
	switch (gTrainingModeMenuData.mCooperativeMode.mDistance)
	{
	case DISTANCE_ANY:
		return "Any";
	case DISTANCE_CLOSE:
		return "Close";
	case DISTANCE_MEDIUM:
		return "Medium";
	case DISTANCE_FAR:
		return "Far";
	default:
		assert(false && "Unrecognized training menu distancce");
		return "";
	}
}

static const char* getButtonJamText() {
	switch (gTrainingModeMenuData.mCooperativeMode.mButtonJam)
	{
	case BUTTON_JAM_NONE:
		return "None";
	case BUTTON_JAM_A:
		return "A";
	case BUTTON_JAM_B:
		return "B";
	case BUTTON_JAM_C:
		return "C";
	case BUTTON_JAM_X:
		return "X";
	case BUTTON_JAM_Y:
		return "Y";
	case BUTTON_JAM_Z:
		return "Z";
	case BUTTON_JAM_START:
		return "Start";
	default:
		assert(false && "Unrecognized training menu button jam");
		return "";
	}
}

static void setCooperativeModeActive() {
	setPlayerHuman(1);

	setGeneralModeActive();
	changeMugenText(gTrainingModeMenuData.mOptionTexts[COOPERATIVE_MODE_OPTION_MODE][0], "Dummy control");
	changeMugenText(gTrainingModeMenuData.mOptionTexts[COOPERATIVE_MODE_OPTION_GUARD_MODE][0], "Guard mode");
	changeMugenText(gTrainingModeMenuData.mOptionTexts[COOPERATIVE_MODE_OPTION_DUMMY_MODE][0], "Dummy mode");
	changeMugenText(gTrainingModeMenuData.mOptionTexts[COOPERATIVE_MODE_OPTION_DISTANCE][0], "Distance");
	changeMugenText(gTrainingModeMenuData.mOptionTexts[COOPERATIVE_MODE_OPTION_BUTTON_JAM][0], "Button jam");

	setOptionTextValue(COOPERATIVE_MODE_OPTION_MODE, "Cooperative");
	setOptionTextValue(COOPERATIVE_MODE_OPTION_GUARD_MODE, getGuardModeText());
	setOptionTextValue(COOPERATIVE_MODE_OPTION_DUMMY_MODE, getDummyModeText());
	setOptionTextValue(COOPERATIVE_MODE_OPTION_DISTANCE, getDistanceText());
	setOptionTextValue(COOPERATIVE_MODE_OPTION_BUTTON_JAM, getButtonJamText());
}

static std::string getAILevelText() {
	std::stringstream ss;
	ss << getPlayerAILevel(getRootPlayer(1));
	return ss.str();
}

static void setAIModeActive() {
	setPlayerArtificial(1, gTrainingModeMenuData.mAIMode.mAILevel);
	const auto aiLevelText = getAILevelText();

	setGeneralModeActive();
	changeMugenText(gTrainingModeMenuData.mOptionTexts[AI_MODE_OPTION_MODE][0], "Dummy control");
	changeMugenText(gTrainingModeMenuData.mOptionTexts[AI_MODE_OPTION_AI_LEVEL][0], "AI Level");

	setOptionTextValue(AI_MODE_OPTION_MODE, "AI");
	setOptionTextValue(AI_MODE_OPTION_AI_LEVEL, aiLevelText.c_str());
}

static void setCurrentModeActive() {
	switch (gTrainingModeMenuData.mMode)
	{
	case TRAINING_MODE_MODE_MANUAL:
		setManualModeActive();
		break;
	case TRAINING_MODE_MODE_COOPERATIVE:
		setCooperativeModeActive();
		break;
	case TRAINING_MODE_MODE_AI:
		setAIModeActive();
		break;
	default:
		assert(false && "Unrecognized training menu mode");
	}
}

static void changeTrainingModeMenuMode(int tDelta) {
	setCurrentModeInactive();
	gTrainingModeMenuData.mMode = TrainingModeMode((gTrainingModeMenuData.mMode + TRAINING_MODE_MODE_AMOUNT + tDelta) % TRAINING_MODE_MODE_AMOUNT);
	setCurrentModeActive();
}

static void changeManualModeMenuOption(int tDelta) {
	switch (gTrainingModeMenuData.mSelectedOption)
	{
	case MANUAL_MODE_OPTION_MODE:
		changeTrainingModeMenuMode(tDelta);
		break;
	default:
		assert(false && "Unrecognized manual mode option");
	}
}

static void changeCooperativeModeGuardMode(int tDelta) {
	gTrainingModeMenuData.mCooperativeMode.mGuardMode = GuardModeOptions((gTrainingModeMenuData.mCooperativeMode.mGuardMode + GUARD_MODE_AMOUNT + tDelta) % GUARD_MODE_AMOUNT);
	setOptionTextValue(COOPERATIVE_MODE_OPTION_GUARD_MODE, getGuardModeText());
}

static void changeCooperativeModeDummyMode(int tDelta) {
	gTrainingModeMenuData.mCooperativeMode.mDummyMode = DummyModeOptions((gTrainingModeMenuData.mCooperativeMode.mDummyMode + DUMMY_MODE_AMOUNT + tDelta) % DUMMY_MODE_AMOUNT);
	setOptionTextValue(COOPERATIVE_MODE_OPTION_DUMMY_MODE, getDummyModeText());
}

static void changeCooperativeModeDistance(int tDelta) {
	gTrainingModeMenuData.mCooperativeMode.mDistance = DistanceOptions((gTrainingModeMenuData.mCooperativeMode.mDistance + DISTANCE_AMOUNT + tDelta) % DISTANCE_AMOUNT);
	setOptionTextValue(COOPERATIVE_MODE_OPTION_DISTANCE, getDistanceText());
}

static void changeCooperativeModeButtonJam(int tDelta) {
	gTrainingModeMenuData.mCooperativeMode.mButtonJam = ButtonJamOptions((gTrainingModeMenuData.mCooperativeMode.mButtonJam + BUTTON_JAM_AMOUNT + tDelta) % BUTTON_JAM_AMOUNT);
	setOptionTextValue(COOPERATIVE_MODE_OPTION_BUTTON_JAM, getButtonJamText());
}

static void changeCooperativeModeMenuOption(int tDelta) {
	switch (gTrainingModeMenuData.mSelectedOption)
	{
	case COOPERATIVE_MODE_OPTION_MODE:
		changeTrainingModeMenuMode(tDelta);
		break;
	case COOPERATIVE_MODE_OPTION_GUARD_MODE:
		changeCooperativeModeGuardMode(tDelta);
		break;
	case COOPERATIVE_MODE_OPTION_DUMMY_MODE:
		changeCooperativeModeDummyMode(tDelta);
		break;
	case COOPERATIVE_MODE_OPTION_DISTANCE:
		changeCooperativeModeDistance(tDelta);
		break;
	case COOPERATIVE_MODE_OPTION_BUTTON_JAM:
		changeCooperativeModeButtonJam(tDelta);
		break;
	default:
		assert(false && "Unrecognized cooperative mode option");
	}
}

static void changeAIModeAILevel(int tDelta) {
	auto currentLevel = getPlayerAILevel(getRootPlayer(1)) - 1;
	currentLevel = (currentLevel + 8 + tDelta) % 8;
	setPlayerArtificial(1, currentLevel + 1);

	const auto aiLevelText = getAILevelText();
	setOptionTextValue(AI_MODE_OPTION_AI_LEVEL, aiLevelText.c_str());
}

static void changeAIModeMenuOption(int tDelta) {
	switch (gTrainingModeMenuData.mSelectedOption)
	{
	case AI_MODE_OPTION_MODE:
		changeTrainingModeMenuMode(tDelta);
		break;
	case AI_MODE_OPTION_AI_LEVEL:
		changeAIModeAILevel(tDelta);
		break;
	default:
		assert(false && "Unrecognized AI mode option");
	}
}

static void changeTrainingModeMenuOptionValue(int tDelta) {
	switch (gTrainingModeMenuData.mMode)
	{
	case TRAINING_MODE_MODE_MANUAL:
		changeManualModeMenuOption(tDelta);
		break;
	case TRAINING_MODE_MODE_COOPERATIVE:
		changeCooperativeModeMenuOption(tDelta);
		break;
	case TRAINING_MODE_MODE_AI:
		changeAIModeMenuOption(tDelta);
		break;
	default:
		assert(false && "Unrecognized training menu mode");
	}
}

static void setSelectedOptionInactive() {
	setMugenTextColor(gTrainingModeMenuData.mOptionTexts[gTrainingModeMenuData.mSelectedOption][0], getMugenTextColorFromMugenTextColorIndex(8));
	setMugenTextColor(gTrainingModeMenuData.mOptionTexts[gTrainingModeMenuData.mSelectedOption][1], getMugenTextColorFromMugenTextColorIndex(5));
}

static void setSelectedOptionActive() {
	setMugenTextColor(gTrainingModeMenuData.mOptionTexts[gTrainingModeMenuData.mSelectedOption][0], getMugenTextColorFromMugenTextColorIndex(0));
	setMugenTextColorRGB(gTrainingModeMenuData.mOptionTexts[gTrainingModeMenuData.mSelectedOption][1], 1.0, 1.0, 0.75);
}

static void changeSelectedOption(int tDelta) {
	const auto optionAmount = getCurrentModeOptionAmount();

	setSelectedOptionInactive();
	gTrainingModeMenuData.mSelectedOption = (gTrainingModeMenuData.mSelectedOption + optionAmount + tDelta) % optionAmount;
	setSelectedOptionActive();
}

static void updateTrainingModeMenuInput() {
	if (hasPressedLeftFlank()) {
		changeTrainingModeMenuOptionValue(-1);
	}
	else if (hasPressedRightFlank()) {
		changeTrainingModeMenuOptionValue(1);
	}

	if (hasPressedUpFlank()) {
		changeSelectedOption(-1);
	}
	else if(hasPressedDownFlank()) {
		changeSelectedOption(1);
	}
}

static void updateTrainingModeGuardMode(DreamPlayer* tPlayer, int tIsBeingAttacked) {
	if (gTrainingModeMenuData.mCooperativeMode.mGuardMode == GUARD_MODE_NONE) return;
	if (!tIsBeingAttacked) return;
	
	const auto otherPlayer = getRootPlayer(0);
	const auto otherPlayerState = getPlayerStateType(otherPlayer);
	if (otherPlayerState == MUGEN_STATE_TYPE_CROUCHING) {
		setDreamPlayerCommandActiveForAI(tPlayer->mCommandID, "holddown", 2);
	}
	setDreamPlayerCommandActiveForAI(tPlayer->mCommandID, "holdback", 2);
}

static void updateTrainingModeCrouch(DreamPlayer* tPlayer, int tIsBeingAttacked) {
	if (tIsBeingAttacked) return;
	setDreamPlayerCommandActiveForAI(tPlayer->mCommandID, "holddown", 2);
}

static void updateTrainingModeJump(DreamPlayer* tPlayer, int tIsBeingAttacked) {
	if (tIsBeingAttacked) return;
	
	const auto playerState = getPlayerStateType(tPlayer);
	if (playerState == MUGEN_STATE_TYPE_STANDING) {
		setDreamPlayerCommandActiveForAI(tPlayer->mCommandID, "holdup", 2);
	}
}

static void updateTrainingModeWJump(DreamPlayer* tPlayer, int tIsBeingAttacked) {
	if (tIsBeingAttacked) return;

	const auto playerState = getPlayerStateType(tPlayer);
	const auto playerVelocityY = getPlayerVelocityY(tPlayer, getPlayerCoordinateP(tPlayer));
	if (playerState == MUGEN_STATE_TYPE_STANDING || playerVelocityY > 0) {
		setDreamPlayerCommandActiveForAI(tPlayer->mCommandID, "holdup", 2);
	}
}

static void updateTrainingModeDummyMode(DreamPlayer* tPlayer, int tIsBeingAttacked) {
	switch (gTrainingModeMenuData.mCooperativeMode.mDummyMode)
	{
	case DUMMY_MODE_STAND:
		break;
	case DUMMY_MODE_CROUCH:
		updateTrainingModeCrouch(tPlayer, tIsBeingAttacked);
		break;
	case DUMMY_MODE_JUMP:
		updateTrainingModeJump(tPlayer, tIsBeingAttacked);
		break;
	case DUMMY_MODE_WJUMP:
		updateTrainingModeWJump(tPlayer, tIsBeingAttacked);
		break;
	default:
		assert(false && "Unrecognized training menu dummy mode");
	}
}

static void updateTrainingModeDistanceClose(DreamPlayer* tPlayer) {
	const auto dist = getPlayerDistanceToFrontOfOtherPlayerX(tPlayer, getDreamMugenStageHandlerCameraCoordinateP());
	if (dist > 30) {
		setDreamPlayerCommandActiveForAI(tPlayer->mCommandID, "holdfwd", 2);
	}
}

static void updateTrainingModeDistanceMedium(DreamPlayer* tPlayer) {
	const auto dist = getPlayerDistanceToFrontOfOtherPlayerX(tPlayer, getDreamMugenStageHandlerCameraCoordinateP());
	if (dist < 140 && !isPlayerInCorner(tPlayer)) {
		setDreamPlayerCommandActiveForAI(tPlayer->mCommandID, "holdback", 2);
	}
	if (dist > 180) {
		setDreamPlayerCommandActiveForAI(tPlayer->mCommandID, "holdfwd", 2);
	}
}

static void updateTrainingModeDistanceFar(DreamPlayer* tPlayer) {
	if (!isPlayerInCorner(tPlayer)) {
		setDreamPlayerCommandActiveForAI(tPlayer->mCommandID, "holdback", 2);
	}
}

static void updateTrainingModeDistance(DreamPlayer* tPlayer, int tIsBeingAttacked) {
	if (tIsBeingAttacked) return;

	switch (gTrainingModeMenuData.mCooperativeMode.mDistance)
	{
	case DISTANCE_ANY:
		break;
	case DISTANCE_CLOSE:
		updateTrainingModeDistanceClose(tPlayer);
		break;
	case DISTANCE_MEDIUM:
		updateTrainingModeDistanceMedium(tPlayer);
		break;
	case DISTANCE_FAR:
		updateTrainingModeDistanceFar(tPlayer);
		break;
	default:
		assert(false && "Unrecognized training menu distancce");
	}
}

static void updateTrainingModeButtonJam() {
	int isMashingButton = (getDreamGameTime() % 10) == 0;
	if (!isMashingButton) return;

	switch (gTrainingModeMenuData.mCooperativeMode.mButtonJam)
	{
	case BUTTON_JAM_NONE:
		break;
	case BUTTON_JAM_A:
		setDreamButtonAActiveForPlayer(1);
		break;
	case BUTTON_JAM_B:
		setDreamButtonBActiveForPlayer(1);
		break;
	case BUTTON_JAM_C:
		setDreamButtonCActiveForPlayer(1);
		break;
	case BUTTON_JAM_X:
		setDreamButtonXActiveForPlayer(1);
		break;
	case BUTTON_JAM_Y:
		setDreamButtonYActiveForPlayer(1);
		break;
	case BUTTON_JAM_Z:
		setDreamButtonZActiveForPlayer(1);
		break;
	case BUTTON_JAM_START:
		setDreamButtonStartActiveForPlayer(1);
		break;
	default:
		assert(false && "Unrecognized training menu button jam");
	}
}

static void updateTrainingModeEffect() {
	if (gTrainingModeMenuData.mMode != TRAINING_MODE_MODE_COOPERATIVE) return;
	
	const auto player = getRootPlayer(1);
	int isBeingAttacked = isPlayerBeingAttacked(player) && isPlayerInGuardDistance(player);
	updateTrainingModeGuardMode(player, isBeingAttacked);
	updateTrainingModeDummyMode(player, isBeingAttacked);
	updateTrainingModeDistance(player, isBeingAttacked);
	updateTrainingModeButtonJam();
}

static void updateTrainingModeMenu(void*) {
	setProfilingSectionMarkerCurrentFunction();

	updateSettingTrainingModeMenuActive();
	if (gTrainingModeMenuData.mIsVisible) {
		updateTrainingModeMenuInput();
	}
	updateTrainingModeEffect();
}

ActorBlueprint getTrainingModeMenu()
{
	return makeActorBlueprint(loadTrainingModeMenu, unloadTrainingModeMenu, updateTrainingModeMenu);
}

static void setTrainingModeMenuVisible() {
	setAnimationVisibility(gTrainingModeMenuData.mBackgroundAnimationElement, 1);
	setMugenTextVisibility(gTrainingModeMenuData.mTitleText, 1);

	setCurrentModeActive();
	gTrainingModeMenuData.mIsVisible = 1;
}

static void setTrainingModeMenuInvisible() {
	setAnimationVisibility(gTrainingModeMenuData.mBackgroundAnimationElement, 0);
	setMugenTextVisibility(gTrainingModeMenuData.mTitleText, 0);

	setCurrentModeInactive();
	gTrainingModeMenuData.mIsVisible = 0;
}

void setTrainingModeMenuVisibility(int tIsVisible)
{
	if (tIsVisible) {
		setTrainingModeMenuVisible();
	}
	else {
		setTrainingModeMenuInvisible();
	}
}
