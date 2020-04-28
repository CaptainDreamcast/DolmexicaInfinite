#include "ai.h"

#include <assert.h>

#include <prism/datastructures.h>
#include <prism/math.h>
#include <prism/stlutil.h>

#include "mugencommandhandler.h"
#include "gamelogic.h"
#include "config.h"
#include "mugenstagehandler.h"

using  namespace std;

typedef struct {
	DreamPlayer* mPlayer;

	double mDifficultyFactor;
	int mRandomInputNow;
	int mRandomInputDuration;

	int mIsMoving;
	int mIsCrouching;
	int mIsJumping;

	int mIsGuardingLogicActive;
	int mWasGuardingSuccessful;

	vector<string> mCommandNames; 
} PlayerAI;

static struct {
	list<PlayerAI> mHandledPlayers; 

} gAI;

static void loadAIHandler(void* tData) {
	(void)tData;
	setProfilingSectionMarkerCurrentFunction();
	stl_new_list(gAI.mHandledPlayers);
}

static int unloadSingleHandledPlayer(void* tCaller, PlayerAI& tData) {
	(void)tCaller;
	PlayerAI* e = &tData;
	stl_delete_vector(e->mCommandNames);
	return 1;
}

static void unloadAIHandler(void* tData) {
	(void)tData;
	setProfilingSectionMarkerCurrentFunction();
	stl_list_remove_predicate(gAI.mHandledPlayers, unloadSingleHandledPlayer);
	stl_delete_list(gAI.mHandledPlayers);
}

static void setRandomPlayerCommandActive(PlayerAI* e) {
	int i = randfromInteger(0, e->mCommandNames.size() - 1);

	const string& name = e->mCommandNames[i];

	setDreamPlayerCommandActiveForAI(e->mPlayer->mCommandID, name.data(), 2);
}

static int setRandomPlayerCommandActiveIfTimePossible(PlayerAI* e) {
	int i = randfromInteger(0, e->mCommandNames.size() - 1);
	const string& name = e->mCommandNames[i];
	const auto duration = getDreamCommandMinimumDuration(e->mPlayer->mCommandID, name.data());
	if (duration < e->mRandomInputNow) {
		setDreamPlayerCommandActiveForAI(e->mPlayer->mCommandID, name.data(), 2);
		return 1;
	}
	return 0;
}

static void updateAIMovement(PlayerAI* e) {
	const auto otherPlayer = getPlayerOtherPlayer(e->mPlayer);
	const auto dist = getPlayerDistanceToFrontOfOtherPlayerX(e->mPlayer, getDreamMugenStageHandlerCameraCoordinateP());
	const auto playerStateType = getPlayerStateType(e->mPlayer);
	const auto otherPlayerStateType = getPlayerStateType(otherPlayer);
	const auto otherPlayerStateMoveType = getPlayerStateMoveType(otherPlayer);

	if (dist > 50) {
		e->mIsMoving = 1;
	}
	else if (dist < 30) {
		e->mIsMoving = 0;
	}

	const auto isOtherPlayerCloseAndCrouching = (dist < 30) && (otherPlayerStateType == MUGEN_STATE_TYPE_CROUCHING);
	const auto isOtherPlayerCrouchAttacking = (otherPlayerStateType == MUGEN_STATE_TYPE_CROUCHING) && (otherPlayerStateMoveType == MUGEN_STATE_MOVE_TYPE_ATTACK);
	e->mIsCrouching = isOtherPlayerCloseAndCrouching || isOtherPlayerCrouchAttacking;
	
	const auto isOtherPlayerJumpingClose = (dist < 30) && (playerStateType != MUGEN_STATE_TYPE_AIR) && (otherPlayerStateType == MUGEN_STATE_TYPE_AIR);
	const auto isOtherPlayerJumpingAttack = (dist < 30) && (playerStateType != MUGEN_STATE_TYPE_AIR) && (otherPlayerStateType == MUGEN_STATE_TYPE_AIR) && (otherPlayerStateMoveType == MUGEN_STATE_MOVE_TYPE_ATTACK);
	e->mIsJumping = isOtherPlayerJumpingClose || isOtherPlayerJumpingAttack;

	if (e->mIsMoving) {
		setDreamPlayerCommandActiveForAI(e->mPlayer->mCommandID, "holdfwd", 2);
	}
	if (e->mIsCrouching) {
		setDreamPlayerCommandActiveForAI(e->mPlayer->mCommandID, "holddown", 2);
	}
	if (e->mIsJumping) {
		setDreamPlayerCommandActiveForAI(e->mPlayer->mCommandID, "holdup", 2);
	}
}

static void updateAIGuarding(PlayerAI* e) {
	if (isPlayerBeingAttacked(e->mPlayer) && isPlayerInGuardDistance(e->mPlayer)) {
		if (!e->mIsGuardingLogicActive) {
			double rand = randfrom(0, 1);
			double guardPossibilityMin = 0.2;
			double guardPossibilityMax = 0.7;
			double guardPossibility = guardPossibilityMin + (guardPossibilityMax - guardPossibilityMin) * e->mDifficultyFactor;
			e->mWasGuardingSuccessful = (rand < guardPossibility);
			e->mIsGuardingLogicActive = 1;
		}
	}
	else {
		e->mIsGuardingLogicActive = 0;
	}

	if (e->mIsGuardingLogicActive && e->mWasGuardingSuccessful) {
		setDreamPlayerCommandActiveForAI(e->mPlayer->mCommandID, "holdback", 2);
	}
}

static void updateAICommands(PlayerAI* e) {
	if (getGameMode() == GAME_MODE_OSU) {
		return;
	}

	e->mRandomInputNow++;
	if (e->mRandomInputNow >= e->mRandomInputDuration) {
		int isSettingInput;
		if (getArcadeAICheat()) {
			setRandomPlayerCommandActive(e);
			isSettingInput = 1;
		}
		else {
			isSettingInput = setRandomPlayerCommandActiveIfTimePossible(e);
		}
		if (!isSettingInput) return;

		e->mRandomInputNow = 0;

		int lowerDurationMin = 30;
		int lowerDurationMax = 1;
		int upperDurationMin = 45;
		int upperDurationMax = 7;
		int lowerDuration = (int)(lowerDurationMin + (lowerDurationMax - lowerDurationMin) * e->mDifficultyFactor);
		int upperDuration = (int)(upperDurationMin + (upperDurationMax - upperDurationMin) * e->mDifficultyFactor);
		e->mRandomInputDuration = randfromInteger(lowerDuration, upperDuration);
	}
}

static void updateSingleAI(void* /*tCaller*/, PlayerAI& tData) {
	if (getDreamRoundStateNumber() != 2) return;
	PlayerAI* e = &tData;
	if (!getPlayerAILevel(e->mPlayer)) return;

	updateAIMovement(e);
	updateAIGuarding(e);
	updateAICommands(e);
}

static void updateAIHandler(void* tData) {
	(void)tData;
	setProfilingSectionMarkerCurrentFunction();
	stl_list_map(gAI.mHandledPlayers, updateSingleAI);
}

static void insertSingleCommandName(vector<string>* tCaller, const string& tKey, DreamMugenCommand& tData) {
	(void)tData;
	tCaller->push_back(tKey);
}

void setDreamAIActive(DreamPlayer * p)
{
	PlayerAI e;
	e.mPlayer = p;
	e.mRandomInputNow = 0;
	e.mRandomInputDuration = 20;
	e.mIsMoving = 0;
	e.mIsCrouching = 0;
	e.mIsJumping = 0;
	e.mIsGuardingLogicActive = 0;
	e.mCommandNames.clear();
	e.mDifficultyFactor = (getPlayerAILevel(p) - 1) / 7.0;

	DreamMugenCommands* commands = &p->mHeader->mFiles.mCommands;
	stl_string_map_map(commands->mCommands, insertSingleCommandName, &e.mCommandNames);

	gAI.mHandledPlayers.push_back(e);
}

typedef struct {
	int i;
	PlayerAI* mFound;
} FindAICaller;

static void findSameID(FindAICaller* tCaller, PlayerAI& tData) {
	PlayerAI* e = &tData;

	if (e->mPlayer->mRootID == tCaller->i) {
		tCaller->mFound = e;
	}
}

static PlayerAI* getAIFromPlayerID(int i) {
	FindAICaller caller;
	caller.i = i;
	caller.mFound = nullptr;

	stl_list_map(gAI.mHandledPlayers, findSameID, &caller);
	if (!caller.mFound) return NULL;
	else return caller.mFound;
}

void activateRandomAICommand(int i) {
	PlayerAI* e = getAIFromPlayerID(i);
	if (!e) return;
	setRandomPlayerCommandActive(e);
}

ActorBlueprint getDreamAIHandler() {
	return makeActorBlueprint(loadAIHandler, unloadAIHandler, updateAIHandler);
}
