#include "gamelogic.h"

#include <assert.h>

#include <prism/screeneffect.h>
#include <prism/wrapper.h>
#include <prism/framerate.h>
#include <prism/wrapper.h>
#include <prism/input.h>
#include <prism/log.h>
#include <prism/math.h>

#include "playerdefinition.h"
#include "fightui.h"
#include "mugenstagehandler.h"
#include "titlescreen.h"
#include "fightscreen.h"
#include "fightresultdisplay.h"
#include "mugenexplod.h"
#include "config.h"

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
} gData;

static void fightAnimationFinishedCB() {
	gData.mRoundStateNumber = ROUND_STATE_FIGHT;
	enableDreamTimer();
}

static void roundAnimationFinishedCB() {
	playDreamFightAnimation(fightAnimationFinishedCB);
}

static void introFinished() {
	gData.mIsDisplayingIntro = 0;
	playDreamRoundAnimation(gData.mRoundNumber, roundAnimationFinishedCB);
}

static void startIntro() {
	gData.mRoundStateNumber = ROUND_STATE_INTRO;
	gData.mIsDisplayingIntro = 2;
	changePlayerState(getRootPlayer(0), 5900);
	changePlayerState(getRootPlayer(1), 5900);
}

static void fadeInFinished(void* tData) {
	(void)tData;

	startIntro();
}

static void startRound() {
	disableDreamTimer();
	gData.mRoundStateNumber = ROUND_STATE_FADE_IN;
	gData.mIsDisplayingIntro = 0;
	gData.mIsDisplayingWinPose = 0;
	setPlayerControl(getRootPlayer(0), 0);
	setPlayerControl(getRootPlayer(1), 0);
	addFadeIn(30, fadeInFinished, NULL);
}

static void gotoNextRound(void*);

static void setWinIcon() {
	VictoryType type = getPlayerVictoryType(gData.mRoundWinner);
	int isPerfect = isPlayerAtFullLife(gData.mRoundWinner);

	if (type == VICTORY_TYPE_NORMAL) {
		addNormalWinIcon(gData.mRoundWinner->mRootID, isPerfect);
	}
	else if (type == VICTORY_TYPE_SPECIAL) {
		addSpecialWinIcon(gData.mRoundWinner->mRootID, isPerfect);
	}
	else if (type == VICTORY_TYPE_HYPER) {
		addHyperWinIcon(gData.mRoundWinner->mRootID, isPerfect);
	}
	else if (type == VICTORY_TYPE_THROW) {
		addThrowWinIcon(gData.mRoundWinner->mRootID, isPerfect);
	}
	else if (type == VICTORY_TYPE_CHEESE) {
		addCheeseWinIcon(gData.mRoundWinner->mRootID, isPerfect);
	}
	else if (type == VICTORY_TYPE_TIMEOVER) {
		addTimeoverWinIcon(gData.mRoundWinner->mRootID, isPerfect);
	}
	else if (type == VICTORY_TYPE_SUICIDE) {
		addSuicideWinIcon(gData.mRoundWinner->mRootID, isPerfect);
	}
	else if (type == VICTORY_TYPE_TEAMMATE) {
		addTeammateWinIcon(gData.mRoundWinner->mRootID, isPerfect);
	}
	else {
		logWarningFormat("Unrecognized win icon type %d. Defaulting to normal.", type);
		addNormalWinIcon(gData.mRoundWinner->mRootID, isPerfect);
	}
}

static void setRoundWinner() {
	if (getPlayerLife(getRootPlayer(0)) >= getPlayerLife(getRootPlayer(1))) {
		gData.mRoundWinner = getRootPlayer(0);
	}
	else {
		gData.mRoundWinner = getRootPlayer(1);
	}

	setWinIcon();
	gData.mRoundStateNumber = ROUND_STATE_OVER;
	setPlayerControl(getRootPlayer(0), 0);
	setPlayerControl(getRootPlayer(1), 0);
}

static void setMatchWinner() {
	gData.mMatchWinnerIndex = gData.mRoundWinner->mRootID;
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

	gData.mGameTime = 0;
	gData.mRoundNumber = gData.mStartRound;
	startRound();
}

static void updateIntro() {
	if (!gData.mIsDisplayingIntro) return;
	
	if (gData.mIsDisplayingIntro == 2) { // TODO: fix call stuff
		gData.mIsDisplayingIntro--;
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

	gData.mGameTime = 0;
	gData.mRoundNumber = gData.mStartRound;
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

	int isShowingResultsSurvival = gData.mMode == GAME_MODE_SURVIVAL && !isPlayerHuman(gData.mRoundWinner);
	if (gData.mIsContinueActive && getPlayerAILevel(gData.mRoundWinner)) {
		startContinue();
	}
	else if (isShowingFightResults() || isShowingResultsSurvival) {
		showFightResults(goToNextScreen, goToLoseScreen);
	} else {
		addFadeOut(30, goToNextScreen, NULL);
	}
}

static void startWinPose() {
	turnPlayerTowardsOtherPlayer(gData.mRoundWinner);

	if (hasPlayerStateSelf(gData.mRoundWinner, 180)) {
		changePlayerState(gData.mRoundWinner, 180);

	}

	gData.mRoundStateNumber = ROUND_STATE_WIN_POSE;
	gData.mIsDisplayingWinPose = 1;
}

static void koAnimationFinishedCB() {
	startWinPose();
}

static void startKO() {
	disableDreamTimer();
	setRoundWinner();
	playDreamKOAnimation(koAnimationFinishedCB);
}

static void updateWinCondition() {
	if (gData.mRoundStateNumber != 2) return;

	int i;
	for (i = 0; i < 2; i++) {

		if (!isPlayerAlive(getRootPlayer(i))) {
			startKO();
			break;
		}
	}
}

static void updateNoControl() {
	if (gData.mRoundStateNumber < 3) return;
	setPlayerControl(getRootPlayer(0), 0);
	setPlayerControl(getRootPlayer(1), 0);
}

static void resetRoundData(void* tCaller) {
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
	gData.mRoundNumber++;
	resetRoundData(NULL);
}

static void restoreSurvivalHealth() {
	if (gData.mMode != GAME_MODE_SURVIVAL) return;
	if (!isPlayerHuman(gData.mRoundWinner)) return;

	addPlayerLife(gData.mRoundWinner, getPlayerLifeMax(gData.mRoundWinner) / 2);
}

static void updateWinPose() {
	if (!gData.mIsDisplayingWinPose) return;

	int hasSkipped = hasPressedStartFlankSingle(0) || hasPressedStartFlankSingle(1);
	int isTimeOver = !getRemainingPlayerAnimationTime(gData.mRoundWinner);
	int isStepInfinite = isMugenAnimationStepDurationInfinite(getPlayerAnimationStepDuration(gData.mRoundWinner)); 
	int isOver = isTimeOver || isStepInfinite;
	if (isOver || hasSkipped) {
		increasePlayerRoundsWon(gData.mRoundWinner);
		if (hasPlayerWon(gData.mRoundWinner)) {
			restoreSurvivalHealth();
			playDreamWinAnimation(getPlayerDisplayName(gData.mRoundWinner), winAnimationFinishedCB);
		}
		else {
			addFadeOut(30, gotoNextRound, NULL);
		}
		gData.mIsDisplayingWinPose = 0;
	}

}

static void updateExhibitMode() {
	if (gData.mMode != GAME_MODE_EXHIBIT) return;

	if (!gData.mExhibit.mIsDisplayingBars) {
		setDreamBarInvisibleForOneFrame();
	}

	int isOver = (hasPressedStartFlank() || gData.mGameTime >= gData.mExhibit.mEndTime);
	if (!gData.mExhibit.mIsFadingOut && isOver) {
		addFadeOut(180, goToNextScreen, NULL);
		gData.mExhibit.mIsFadingOut = 1;
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
	gData.mIsDisplayingIntro = 0;
	changePlayerState(getRootPlayer(0), 0);
	changePlayerState(getRootPlayer(1), 0);
	removeAllExplods();
	addFadeIn(10, skipIntroFinishedCB, NULL);
}

static void updateIntroSkip() {
	if (gData.mRoundStateNumber != ROUND_STATE_INTRO) return;

	if (hasPressedStartFlankSingle(0) || hasPressedStartFlankSingle(1)) {
		addFadeOut(10, skipIntroCB, NULL);
	}
}

static void updateGameLogic(void* tData) {
	(void)tData;
	gData.mGameTime++;

	updateIntro();
	updateWinCondition();
	updateNoControl();
	updateWinPose();
	updateExhibitMode();
	updateIntroSkip();
}

ActorBlueprint getDreamGameLogic() {
	return makeActorBlueprint(loadGameLogic, NULL, updateGameLogic);
}


int getDreamGameTime()
{
	return gData.mGameTime;
}

int getDreamRoundNumber()
{
	return gData.mRoundNumber; 
}

int getRoundsToWin()
{
	return gData.mRoundsToWin;
}

int getDreamRoundStateNumber()
{
	return gData.mRoundStateNumber;
}

int getDreamMatchNumber()
{
	return 1; // TODO
}

int isDreamMatchOver()
{
	return 0; // TODO
}

void setDreamRoundNotOverFlag()
{
	gData.mRoundNotOverFlag = 1; // TODO: use
}

int isDreamGameModeTwoPlayer()
{
	return !gData.mIsInSinglePlayerMode;
}

void setDreamGameModeSinglePlayer()
{
	gData.mIsInSinglePlayerMode = 1;
}

void setDreamGameModeTwoPlayer()
{
	gData.mIsInSinglePlayerMode = 0;
}

int getDreamTicksPerSecond()
{
	return 60; // TODO: variable framerate
}

int getDreamMatchWinnerIndex()
{
	return gData.mMatchWinnerIndex;
}

void resetRound() {
	addFadeOut(30,  resetRoundData, NULL);
}

void reloadFight()
{
	startFightScreen();
}

void setFightContinueActive()
{
	gData.mIsContinueActive = 1;
}

void setFightContinueInactive()
{
	gData.mIsContinueActive = 0;
}

void setGameModeArcade() {
	gData.mRoundsToWin = 2;
	gData.mStartRound = 1;

	setFightContinueActive();
	setTimerFinite();
	setPlayersToRealFightMode();
	setPlayerHuman(0);
	setPlayerArtificial(1, getDifficulty());
	setPlayerPreferredPalette(0, 1);
	setPlayerPreferredPalette(1, 2);
	setPlayerStartLifePercentage(0, 1);
	setPlayerStartLifePercentage(1, 1);

	gData.mMode = GAME_MODE_ARCADE;
}

void setGameModeFreePlay()
{
	gData.mRoundsToWin = 2;
	gData.mStartRound = 1;

	setFightContinueActive();
	setTimerFinite();
	setPlayersToRealFightMode();
	setPlayerHuman(0);
	setPlayerArtificial(1, getDifficulty());
	setPlayerPreferredPalette(0, 1);
	setPlayerPreferredPalette(1, 2);
	setPlayerStartLifePercentage(0, 1);
	setPlayerStartLifePercentage(1, 1);

	gData.mMode = GAME_MODE_FREE_PLAY;
}

void setGameModeVersus() {
	gData.mRoundsToWin = 2;
	gData.mStartRound = 1;

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

	gData.mMode = GAME_MODE_VERSUS;
}

void setGameModeSurvival(double tLifePercentage, int tRound) {
	gData.mRoundsToWin = 1;
	gData.mStartRound = tRound;

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

	gData.mMode = GAME_MODE_SURVIVAL;
}

void setGameModeTraining() {
	gData.mRoundsToWin = 2;
	gData.mStartRound = 1;

	setFightResultActive(0);
	setFightContinueActive();
	setTimerInfinite();
	setPlayersToTrainingMode();
	setPlayerHuman(0);
	setPlayerHuman(1);
	setPlayerPreferredPalette(0, 1);
	setPlayerPreferredPalette(1, 2);
	setPlayerStartLifePercentage(0, 1);
	setPlayerStartLifePercentage(1, 1);

	gData.mMode = GAME_MODE_TRAINING;
}

void setGameModeWatch()
{
	gData.mRoundsToWin = 2;
	gData.mStartRound = 1;

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

	gData.mMode = GAME_MODE_WATCH;
}

void setGameModeSuperWatch()
{
	gData.mRoundsToWin = 2;
	gData.mStartRound = 1;

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

	gData.mMode = GAME_MODE_SUPER_WATCH;
}

void setGameModeExhibit(int tEndTime, int tIsDisplayingBars)
{
	gData.mRoundsToWin = 1;
	gData.mStartRound = 1;

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

	gData.mExhibit.mEndTime = tEndTime;
	gData.mExhibit.mIsDisplayingBars = tIsDisplayingBars;
	gData.mExhibit.mIsFadingOut = 0;

	gData.mMode = GAME_MODE_EXHIBIT;
}

void setGameModeStory() {
	gData.mRoundsToWin = 2;
	gData.mStartRound = 1;

	setFightResultActive(0);
	setTimerFinite();
	setPlayersToRealFightMode();
	setPlayerHuman(0);
	setPlayerArtificial(1, getDifficulty());
	setPlayerStartLifePercentage(0, 1);
	setPlayerStartLifePercentage(1, 1);

	gData.mMode = GAME_MODE_STORY;
}

GameMode getGameMode()
{
	return gData.mMode;
}
