#include "ai.h"

#include <assert.h>

#include <prism/datastructures.h>
#include <prism/math.h>

#include "mugencommandhandler.h"

typedef struct {
	DreamPlayer* mPlayer;

	int mRandomInputNow;
	int mRandomInputDuration;

	int mIsMoving;

	int mIsGuardingLogicActive;
	int mWasGuardingSuccessful;

	Vector mCommandNames; // contains char*
} PlayerAI;

static struct {
	List mHandledPlayers; // contains DreamPlayer AI

} gData;

static void loadAIHandler(void* tData) {
	(void)tData;
	gData.mHandledPlayers = new_list();
}

static int unloadSingleHandledPlayer(void* tCaller, void* tData) {
	(void)tCaller;
	PlayerAI* e = tData;
	delete_vector(&e->mCommandNames);
	return 1;
}

static void unloadAIHandler(void* tData) {
	(void)tData;
	list_remove_predicate(&gData.mHandledPlayers, unloadSingleHandledPlayer, NULL);
	delete_list(&gData.mHandledPlayers);
}

static void setRandomPlayerCommandActive(PlayerAI* e) {
	int i = randfromInteger(0, vector_size(&e->mCommandNames) - 1);

	char* name = vector_get(&e->mCommandNames, i);

	setDreamPlayerCommandActiveForAI(e->mPlayer->mCommandID, name, 2);
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
			e->mWasGuardingSuccessful = (rand < 0.2);
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
		e->mRandomInputDuration = randfromInteger(30, 45);

		setRandomPlayerCommandActive(e);
	}
}

static void updateSingleAI(void* tCaller, void* tData) {
	(void)tCaller;
	PlayerAI* e = tData;

	updateAIMovement(e);
	updateAIGuarding(e);
	updateAICommands(e);
}

static void updateAIHandler(void* tData) {
	(void)tData;

	list_map(&gData.mHandledPlayers, updateSingleAI, NULL);
}

static void insertSingleCommandName(void* tCaller, char* tKey, void* tData) {
	(void)tData;
	Vector* names = tCaller;
	vector_push_back(names, tKey);
}

void setDreamAIActive(DreamPlayer * p)
{
	assert(getPlayerAILevel(p));

	PlayerAI* e = allocMemory(sizeof(PlayerAI));
	e->mPlayer = p;
	e->mRandomInputNow = 0;
	e->mRandomInputDuration = 20;
	e->mIsMoving = 0;
	e->mIsGuardingLogicActive = 0;
	e->mCommandNames = new_vector();

	DreamMugenCommands* commands = &p->mCommands;
	string_map_map(&commands->mCommands, insertSingleCommandName, &e->mCommandNames);

	list_push_back_owned(&gData.mHandledPlayers, e);
}

ActorBlueprint DreamAIHandler = {
	.mLoad = loadAIHandler,
	.mUnload = unloadAIHandler,
	.mUpdate = updateAIHandler,
};
