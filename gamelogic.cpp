#include "gamelogic.h"

#include <assert.h>

#include <prism/screeneffect.h>
#include <prism/wrapper.h>
#include <prism/system.h>
#include <prism/input.h>
#include <prism/log.h>
#include <prism/math.h>

#include "playerdefinition.h"
#include "fightui.h"
#include "mugenstagehandler.h"
#include "titlescreen.h"
#include "fightscreen.h"
#include "fightdebug.h"
#include "fightresultdisplay.h"
#include "mugenexplod.h"
#include "config.h"
#include "osuhandler.h"
#include "arcademode.h"
#include "survivalmode.h"

typedef enum {
	ROUND_STATE_FADE_IN = 0,
	ROUND_STATE_INTRO = 1,
	ROUND_STATE_FIGHT = 2,
	ROUND_STATE_OVER = 3,
	ROUND_STATE_WIN_POSE = 4,

} RoundState;

typedef struct {
	int mEndTime;
	int mIsDisplayingBars;
	int mIsFadingOut;
} ExhibitLogic;

static struct {
	int mGameTime;
	int mRoundNumber;
	RoundState mRoundStateNumber;
	int mRoundsToWin;
	int mStartRound;

	int mIsDisplayingIntro;
	int mIsDisplayingWinPose;

	int mRoundNotOverFlag;

	DreamPlayer* mRoundWinner;
	int mMatchWinnerIndex;

	int mIsInSinglePlayerMode;
	int mIsContinueActive;

	ExhibitLogic mExhibit;
	

	GameMode mMode;
} gGameLogicData;

static void fightAnimationFinishedCB() {
	changePlayerStateIfDifferent(getRootPlayer(0), 0);
	changePlayerStateIfDifferent(getRootPlayer(1), 0);

	gGameLogicData.mRoundStateNumber = ROUND_STATE_FIGHT;
	enableDreamTimer();
}

static void roundAnimationFinishedCB() {
	playDreamFightAnimation(fightAnimationFinishedCB);
}

static void introFinished() {
	gGameLogicData.mIsDisplayingIntro = 0;
	if (getGameMode() == GAME_MODE_OSU && !shouldPlayOsuMusicInTheBeginning()) {
		startPlayingOsuSong();
	}
	playDreamRoundAnimation(gGameLogicData.mRoundNumber, roundAnimationFinishedCB);
}

static void startIntro() {
	gGameLogicData.mRoundStateNumber = ROUND_STATE_INTRO;
	gGameLogicData.mIsDisplayingIntro = 2;
}

static void fadeInFinished(void* tData) {
	(void)tData;

	startIntro();
}

static void startRound() {
	disableDreamTimer();
	gGameLogicData.mRoundStateNumber = ROUND_STATE_FADE_IN;
	gGameLogicData.mIsDisplayingIntro = 0;
	gGameLogicData.mIsDisplayingWinPose = 0;
	setPlayerControl(getRootPlayer(0), 0);
	setPlayerControl(getRootPlayer(1), 0);
	if (hasLoadedPlayerSprites()) {
		changePlayerState(getRootPlayer(0), 5900);
		changePlayerState(getRootPlayer(1), 5900);
	}
	addFadeIn(30, fadeInFinished, NULL);
}

static void gotoNextRound(void*);

static void setWinIcon() {
	VictoryType type = getPlayerVictoryType(gGameLogicData.mRoundWinner);
	int isPerfect = isPlayerAtFullLife(gGameLogicData.mRoundWinner);

	if (type == VICTORY_TYPE_NORMAL) {
		addNormalWinIcon(gGameLogicData.mRoundWinner->mRootID, isPerfect);
	}
	else if (type == VICTORY_TYPE_SPECIAL) {
		addSpecialWinIcon(gGameLogicData.mRoundWinner->mRootID, isPerfect);
	}
	else if (type == VICTORY_TYPE_HYPER) {
		addHyperWinIcon(gGameLogicData.mRoundWinner->mRootID, isPerfect);
	}
	else if (type == VICTORY_TYPE_THROW) {
		addThrowWinIcon(gGameLogicData.mRoundWinner->mRootID, isPerfect);
	}
	else if (type == VICTORY_TYPE_CHEESE) {
		addCheeseWinIcon(gGameLogicData.mRoundWinner->mRootID, isPerfect);
	}
	else if (type == VICTORY_TYPE_TIMEOVER) {
		addTimeoverWinIcon(gGameLogicData.mRoundWinner->mRootID, isPerfect);
	}
	else if (type == VICTORY_TYPE_SUICIDE) {
		addSuicideWinIcon(gGameLogicData.mRoundWinner->mRootID, isPerfect);
	}
	else if (type == VICTORY_TYPE_TEAMMATE) {
		addTeammateWinIcon(gGameLogicData.mRoundWinner->mRootID, isPerfect);
	}
	else {
		logWarningFormat("Unrecognized win icon type %d. Defaulting to normal.", type);
		addNormalWinIcon(gGameLogicData.mRoundWinner->mRootID, isPerfect);
	}
}

static void setRoundWinner() {
	if (getPlayerLife(getRootPlayer(0)) >= getPlayerLife(getRootPlayer(1))) {
		gGameLogicData.mRoundWinner = getRootPlayer(0);
	}
	else {
		gGameLogicData.mRoundWinner = getRootPlayer(1);
	}

	setWinIcon();
	gGameLogicData.mRoundStateNumber = ROUND_STATE_OVER;
	setPlayerControl(getRootPlayer(0), 0);
	setPlayerControl(getRootPlayer(1), 0);
}

static void setMatchWinner() {
	gGameLogicData.mMatchWinnerIndex = gGameLogicData.mRoundWinner->mRootID;
}

static void startWinPose();

static void timerFinishedCB() {
	disableDreamTimer();
	setRoundWinner();
	startWinPose();
}

static void loadGameLogic(void* tData) {
	(void)tData;
	setDreamTimeDisplayFinishedCB(timerFinishedCB);

	gGameLogicData.mGameTime = 0;
	gGameLogicData.mRoundNumber = gGameLogicData.mStartRound;
	startRound();
}

static void updateIntro() {
	if (!gGameLogicData.mIsDisplayingIntro) return;
	
	if (gGameLogicData.mIsDisplayingIntro == 2) {
		gGameLogicData.mIsDisplayingIntro--;
		return;
	}
	
	int isAFinished = !isPlayerInIntro(getRootPlayer(0));
	int isBFinished = !isPlayerInIntro(getRootPlayer(1));

	if (isAFinished && isBFinished) {

		introFinished();
	}
}

static void goToNextScreen(void* tCaller) {
	(void)tCaller;

	stopFightScreenWin();
}

static void goToLoseScreen(void* tCaller) {
	(void)tCaller;
	stopFightScreenLose();
}

static void continueAnimationFinishedCB() {
	addFadeOut(30, goToLoseScreen, NULL);
}

static void resetGameLogic(void* tCaller) {
	(void)tCaller;

	gotoNextRound(NULL);
	removeAllWinIcons();
	resetPlayersEntirely();
	if (getGameMode() == GAME_MODE_OSU) {
		resetOsuHandler();
	}

	gGameLogicData.mGameTime = 0;
	gGameLogicData.mRoundNumber = gGameLogicData.mStartRound;
}

static void resetGameStart() {
	addFadeOut(30, resetGameLogic, NULL);
}

static void continuePressedCB() {
	resetGameStart();
}

static void startContinue() {
	playDreamContinueAnimation(continueAnimationFinishedCB, continuePressedCB);
}

static void winAnimationFinishedCB() {
	setMatchWinner();

	int isShowingResultsSurvival = gGameLogicData.mMode == GAME_MODE_SURVIVAL && !isPlayerHuman(gGameLogicData.mRoundWinner);
	if (gGameLogicData.mIsContinueActive && getPlayerAILevel(gGameLogicData.mRoundWinner)) {
		startContinue();
	}
	else if (isShowingFightResults() || isShowingResultsSurvival) {
		showFightResults(goToNextScreen, goToLoseScreen);
	} else {
		addFadeOut(30, goToNextScreen, NULL);
	}
}

static void startWinPose() {
	turnPlayerTowardsOtherPlayer(gGameLogicData.mRoundWinner);

	if (hasPlayerStateSelf(gGameLogicData.mRoundWinner, 180)) {
		changePlayerState(gGameLogicData.mRoundWinner, 180);
	}

	gGameLogicData.mRoundStateNumber = ROUND_STATE_WIN_POSE;
	gGameLogicData.mIsDisplayingWinPose = 1;
}

static void koAnimationFinishedCB() {
	startWinPose();
}

static void startKO() {
	disableDreamTimer();
	if (getGameMode() == GAME_MODE_OSU) {
		stopOsuHandler();
	}
	setRoundWinner();
	playDreamKOAnimation(koAnimationFinishedCB);
}

static void updateWinCondition() {
	if (gGameLogicData.mRoundStateNumber != 2) return;

	int i;
	for (i = 0; i < 2; i++) {

		if (!isPlayerAlive(getRootPlayer(i))) {
			startKO();
			break;
		}
	}
}

static void updateNoControl() {
	if (gGameLogicData.mRoundStateNumber < 3) return;
	setPlayerControl(getRootPlayer(0), 0);
	setPlayerControl(getRootPlayer(1), 0);
}

static void resetRoundData(void* /*tCaller*/) {
	enableDrawing();
	resetPlayers();
	resetDreamMugenStageHandlerCameraPosition();
	resetDreamTimer();
	changePlayerState(getRootPlayer(0), 0);
	changePlayerState(getRootPlayer(1), 0);
	startRound();
}

static void gotoNextRound(void* tCaller) {
	(void)tCaller;
	increasePlayerRoundsExisted();
	gGameLogicData.mRoundNumber++;
	resetRoundData(NULL);
}

static void restoreSurvivalHealth() {
	if (gGameLogicData.mMode != GAME_MODE_SURVIVAL) return;
	if (!isPlayerHuman(gGameLogicData.mRoundWinner)) return;

	addPlayerLife(gGameLogicData.mRoundWinner, gGameLogicData.mRoundWinner, getPlayerLifeMax(gGameLogicData.mRoundWinner) / 2);
}

static void updateWinPose() {
	if (!gGameLogicData.mIsDisplayingWinPose) return;

	int hasSkipped = hasPressedStartFlankSingle(0) || hasPressedStartFlankSingle(1);
	int isTimeOver = !getPlayerAnimationTimeDeltaUntilFinished(gGameLogicData.mRoundWinner) || hasPlayerAnimationLooped(gGameLogicData.mRoundWinner);
	int isStepInfinite = isMugenAnimationStepDurationInfinite(getPlayerAnimationStepDuration(gGameLogicData.mRoundWinner)); 
	int isOver = (isTimeOver || isStepInfinite) && !gGameLogicData.mRoundNotOverFlag;
	if (isOver || hasSkipped) {
		increasePlayerRoundsWon(gGameLogicData.mRoundWinner);
		if (hasPlayerWon(gGameLogicData.mRoundWinner)) {
			restoreSurvivalHealth();
			playDreamWinAnimation(getPlayerDisplayName(gGameLogicData.mRoundWinner), winAnimationFinishedCB);
		}
		else {
			addFadeOut(30, gotoNextRound, NULL);
		}
		gGameLogicData.mIsDisplayingWinPose = 0;
	}

}

static void updateRoundNotOverFlag() {
	gGameLogicData.mRoundNotOverFlag = 0;
}

static void updateExhibitMode() {
	if (gGameLogicData.mMode != GAME_MODE_EXHIBIT) return;

	if (!gGameLogicData.mExhibit.mIsDisplayingBars) {
		setDreamBarInvisibleForOneFrame();
	}

	int isOver = (hasPressedStartFlank() || gGameLogicData.mGameTime >= gGameLogicData.mExhibit.mEndTime);
	if (!gGameLogicData.mExhibit.mIsFadingOut && isOver) {
		addFadeOut(180, goToNextScreen, NULL);
		gGameLogicData.mExhibit.mIsFadingOut = 1;
	}
}


static void skipIntroFinishedCB(void* tCaller) {
	(void)tCaller;

	fightAnimationFinishedCB();
}

static void skipIntroCB(void* tCaller) {
	(void)tCaller;
	enableDrawing();
	stopFightAndRoundAnimation();
	if (getGameMode() == GAME_MODE_OSU && !shouldPlayOsuMusicInTheBeginning() && !isOsuHandlerActive()) {
		startPlayingOsuSong();
	}
	gGameLogicData.mIsDisplayingIntro = 0;
	changePlayerState(getRootPlayer(0), 0);
	changePlayerState(getRootPlayer(1), 0);
	resetPlayerPosition(getRootPlayer(0));
	resetPlayerPosition(getRootPlayer(1));
	removeAllExplods();
	addFadeIn(10, skipIntroFinishedCB, NULL);
}

static void updateIntroSkip() {
	if (gGameLogicData.mRoundStateNumber != ROUND_STATE_INTRO) return;

	if (hasPressedStartFlankSingle(0) || hasPressedStartFlankSingle(1)) {
		addFadeOut(10, skipIntroCB, NULL);
	}
}

static void updateGameLogic(void* tData) {
	(void)tData;
	gGameLogicData.mGameTime++;

	updateIntro();
	updateWinCondition();
	updateNoControl();
	updateWinPose();
	updateRoundNotOverFlag();
	updateExhibitMode();
	updateIntroSkip();
}

ActorBlueprint getDreamGameLogic() {
	return makeActorBlueprint(loadGameLogic, NULL, updateGameLogic);
}


int getDreamGameTime()
{
	return gGameLogicData.mGameTime;
}

int getDreamRoundNumber()
{
	return gGameLogicData.mRoundNumber; 
}

int getRoundsToWin()
{
	return gGameLogicData.mRoundsToWin;
}

void setRoundsToWin(int tRoundsToWin)
{
	gGameLogicData.mRoundsToWin = tRoundsToWin;
}

int getDreamRoundStateNumber()
{
	return gGameLogicData.mRoundStateNumber;
}

int getDreamMatchNumber()
{
	switch (getGameMode()) {
	case GAME_MODE_ARCADE:
		return getArcadeModeMatchNumber();
	case GAME_MODE_SURVIVAL:
		return getSurvivalModeMatchNumber();
	default:
		return 1;
	}
}

int isDreamMatchOver()
{
	return gGameLogicData.mRoundStateNumber >= ROUND_STATE_WIN_POSE;
}

void setDreamRoundNotOverFlag()
{
	gGameLogicData.mRoundNotOverFlag = 1;
}

int isDreamGameModeTwoPlayer()
{
	return !gGameLogicData.mIsInSinglePlayerMode;
}

void setDreamGameModeSinglePlayer()
{
	gGameLogicData.mIsInSinglePlayerMode = 1;
}

void setDreamGameModeTwoPlayer()
{
	gGameLogicData.mIsInSinglePlayerMode = 0;
}

int getDreamTicksPerSecond()
{
	return 60;
}

int getDreamMatchWinnerIndex()
{
	return gGameLogicData.mMatchWinnerIndex;
}

void resetRound() {
	addFadeOut(30,  resetRoundData, NULL);
}

void reloadFight()
{
	reloadFightScreen();
}

void skipFightIntroWithoutFading()
{
	skipFadeIn();
	skipIntroCB(NULL);
	skipFadeIn();
}

void setFightContinueActive()
{
	gGameLogicData.mIsContinueActive = 1;
}

void setFightContinueInactive()
{
	gGameLogicData.mIsContinueActive = 0;
}

void setGameModeArcade() {
	gGameLogicData.mRoundsToWin = 2;
	gGameLogicData.mStartRound = 1;

	setFightContinueActive();
	setTimerFinite();
	setPlayersToRealFightMode();
	setPlayerHuman(0);
	setPlayerArtificial(1, getDifficulty());
	setPlayerPreferredPalette(0, 1);
	setPlayerPreferredPalette(1, 2);
	setPlayerStartLifePercentage(0, 1);
	setPlayerStartLifePercentage(1, 1);

	gGameLogicData.mMode = GAME_MODE_ARCADE;
}

void setGameModeFreePlay()
{
	gGameLogicData.mRoundsToWin = 2;
	gGameLogicData.mStartRound = 1;

	setFightContinueActive();
	setTimerFinite();
	setPlayersToRealFightMode();
	setPlayerHuman(0);
	setPlayerArtificial(1, getDifficulty());
	setPlayerPreferredPalette(0, 1);
	setPlayerPreferredPalette(1, 2);
	setPlayerStartLifePercentage(0, 1);
	setPlayerStartLifePercentage(1, 1);

	gGameLogicData.mMode = GAME_MODE_FREE_PLAY;
}

void setGameModeVersus() {
	gGameLogicData.mRoundsToWin = 2;
	gGameLogicData.mStartRound = 1;

	setFightResultActive(0);
	setFightContinueInactive();
	setTimerFinite();
	setPlayersToRealFightMode();
	setPlayerHuman(0);
	setPlayerHuman(1);
	setPlayerPreferredPalette(0, 1);
	setPlayerPreferredPalette(1, 2);
	setPlayerStartLifePercentage(0, 1);
	setPlayerStartLifePercentage(1, 1);

	gGameLogicData.mMode = GAME_MODE_VERSUS;
}

void setGameModeSurvival(double tLifePercentage, int tRound) {
	gGameLogicData.mRoundsToWin = 1;
	gGameLogicData.mStartRound = tRound;

	setFightResultActive(0);
	setFightContinueInactive();
	setTimerFinite();
	setPlayersToRealFightMode();
	setPlayerHuman(0);
	setPlayerArtificial(1, getDifficulty());
	setPlayerPreferredPalette(0, 1);
	setPlayerPreferredPalette(1, 2);
	setPlayerStartLifePercentage(0, tLifePercentage);
	setPlayerStartLifePercentage(1, 1);

	gGameLogicData.mMode = GAME_MODE_SURVIVAL;
}

void setGameModeTraining() {
	gGameLogicData.mRoundsToWin = 2;
	gGameLogicData.mStartRound = 1;

	setFightResultActive(0);
	setFightContinueActive();
	setTimerInfinite();
	setPlayersToTrainingMode();
	setPlayerHuman(0);
	setPlayerHuman(1);
	setPlayerPreferredPalette(0, 1);
	setPlayerPreferredPalette(1, 1);
	setPlayerStartLifePercentage(0, 1);
	setPlayerStartLifePercentage(1, 1);

	gGameLogicData.mMode = GAME_MODE_TRAINING;
}

void setGameModeWatch()
{
	gGameLogicData.mRoundsToWin = 2;
	gGameLogicData.mStartRound = 1;

	setFightResultActive(0);
	setFightContinueInactive();
	setTimerFinite();
	setPlayersToRealFightMode();
	setPlayerArtificial(0, getDifficulty());
	setPlayerArtificial(1, getDifficulty());
	setPlayerPreferredPalette(0, 1);
	setPlayerPreferredPalette(1, 2);
	setPlayerStartLifePercentage(0, 1);
	setPlayerStartLifePercentage(1, 1);

	gGameLogicData.mMode = GAME_MODE_WATCH;
}

void setGameModeSuperWatch()
{
	gGameLogicData.mRoundsToWin = 2;
	gGameLogicData.mStartRound = 1;

	setFightResultActive(0);
	setFightContinueInactive();
	setTimerInfinite();
	setPlayersToTrainingMode();
	setPlayerArtificial(0, getDifficulty());
	setPlayerArtificial(1, getDifficulty());
	setPlayerPreferredPalette(0, 1);
	setPlayerPreferredPalette(1, 2);
	setPlayerStartLifePercentage(0, 1);
	setPlayerStartLifePercentage(1, 1);

	gGameLogicData.mMode = GAME_MODE_SUPER_WATCH;
}

void setGameModeExhibit(int tEndTime, int tIsDisplayingBars, int tIsDisplayingDebug)
{
	gGameLogicData.mRoundsToWin = 1;
	gGameLogicData.mStartRound = 1;

	setFightResultActive(0);
	setFightContinueInactive();
	setTimerInfinite();
	setPlayersToTrainingMode();
	setPlayerArtificial(0, getDifficulty());
	setPlayerArtificial(1, getDifficulty());
	setPlayerPreferredPalette(0, 1);
	setPlayerPreferredPalette(1, 2);
	setPlayerStartLifePercentage(0, 1);
	setPlayerStartLifePercentage(1, 1);
	if (isMugenDebugActive() && tIsDisplayingDebug) {
		setFightDebugToPlayerOneBeforeFight();
	}
	gGameLogicData.mExhibit.mEndTime = tEndTime;
	gGameLogicData.mExhibit.mIsDisplayingBars = tIsDisplayingBars;
	gGameLogicData.mExhibit.mIsFadingOut = 0;

	gGameLogicData.mMode = GAME_MODE_EXHIBIT;
}

void setGameModeStory() {
	gGameLogicData.mRoundsToWin = 2;
	gGameLogicData.mStartRound = 1;

	setFightResultActive(0);
	setTimerFinite();
	setPlayersToRealFightMode();
	setPlayerHuman(0);
	setPlayerArtificial(1, getDifficulty());
	setPlayerStartLifePercentage(0, 1);
	setPlayerStartLifePercentage(1, 1);

	gGameLogicData.mMode = GAME_MODE_STORY;
}

void setGameModeOsu()
{
	gGameLogicData.mRoundsToWin = 1;
	gGameLogicData.mStartRound = 1;

	setFightResultActive(0);
	setTimerInfinite();
	setPlayersToRealFightMode();
	setPlayerHuman(0);
	setPlayerArtificial(1, getDifficulty());
	setPlayerPreferredPalette(0, 1);
	setPlayerPreferredPalette(1, 2);
	setPlayerStartLifePercentage(0, 2);
	setPlayerStartLifePercentage(1, 2);

	gGameLogicData.mMode = GAME_MODE_OSU;
}

void resetGameMode()
{
	if (getGameMode() == GAME_MODE_OSU)
	{
		setGameModeFreePlay();
	}
}

GameMode getGameMode()
{
	return gGameLogicData.mMode;
}
