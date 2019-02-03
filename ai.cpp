#include "ai.h"

#include <assert.h>

#include <prism/datastructures.h>
#include <prism/math.h>
#include <prism/stlutil.h>

#include "mugencommandhandler.h"

using  namespace std;

typedef struct {
	DreamPlayer* mPlayer;

	double mDifficultyFactor;
	int mRandomInputNow;
	int mRandomInputDuration;

	int mIsMoving;

	int mIsGuardingLogicActive;
	int mWasGuardingSuccessful;

	vector<string> mCommandNames; 
} PlayerAI;

static struct {
	list<PlayerAI> mHandledPlayers; 

} gAI;

static void loadAIHandler(void* tData) {
	(void)tData;
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
	stl_list_remove_predicate(gAI.mHandledPlayers, unloadSingleHandledPlayer);
	stl_delete_list(gAI.mHandledPlayers);
}

static void setRandomPlayerCommandActive(PlayerAI* e) {
	int i = randfromInteger(0, e->mCommandNames.size() - 1);

	const string& name = e->mCommandNames[i];

	setDreamPlayerCommandActiveForAI(e->mPlayer->mCommandID, name.data(), 2);
}

static void updateAIMovement(PlayerAI* e) {
	double dist = getPlayerDistanceToFrontOfOtherPlayerX(e->mPlayer);

	if (e->mIsMoving) {
		setDreamPlayerCommandActiveForAI(e->mPlayer->mCommandID, "holdfwd", 2);
	}

	if (dist > 50) {
		e->mIsMoving = 1;
	}
	else if (dist < 30) {
		e->mIsMoving = 0;
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
	e->mRandomInputNow++;
	if (e->mRandomInputNow >= e->mRandomInputDuration) {
		e->mRandomInputNow = 0;

		int lowerDurationMin = 30;
		int lowerDurationMax = 1;
		int upperDurationMin = 45;
		int upperDurationMax = 7;
		int lowerDuration = (int)(lowerDurationMin + (lowerDurationMax - lowerDurationMin) * e->mDifficultyFactor);
		int upperDuration = (int)(upperDurationMin + (upperDurationMax - upperDurationMin) * e->mDifficultyFactor);
		e->mRandomInputDuration = randfromInteger(lowerDuration, upperDuration);

		setRandomPlayerCommandActive(e);
	}
}

static void updateSingleAI(void* tCaller, PlayerAI& tData) {
	(void)tCaller;
	PlayerAI* e = &tData;

	updateAIMovement(e);
	updateAIGuarding(e);
	updateAICommands(e);
}

static void updateAIHandler(void* tData) {
	(void)tData;

	stl_list_map(gAI.mHandledPlayers, updateSingleAI);
}

static void insertSingleCommandName(vector<string>* tCaller, const string& tKey, DreamMugenCommand& tData) {
	(void)tData;
	tCaller->push_back(tKey);
}

void setDreamAIActive(DreamPlayer * p)
{
	assert(getPlayerAILevel(p));

	PlayerAI e;
	e.mPlayer = p;
	e.mRandomInputNow = 0;
	e.mRandomInputDuration = 20;
	e.mIsMoving = 0;
	e.mIsGuardingLogicActive = 0;
	e.mCommandNames.clear();
	e.mDifficultyFactor = (getPlayerAILevel(p) - 1) / 7.0;

	DreamMugenCommands* commands = &p->mHeader->mFiles.mCommands;
	stl_string_map_map(commands->mCommands, insertSingleCommandName, &e.mCommandNames);

	gAI.mHandledPlayers.push_back(e);
}

ActorBlueprint getDreamAIHandler() {
	return makeActorBlueprint(loadAIHandler, unloadAIHandler, updateAIHandler);
}
