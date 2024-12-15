#include "fightnetplay.h"

#include <assert.h>

#include <prism/netplay.h>

#include "gamelogic.h"
#include "playerdefinition.h"
#include "mugencommandhandler.h"
#include "netplaylogic.h"

static struct {
	bool mHasReceivedNetplayData;
	FightNetplayData mCachedReceivedNetplayData;
} gFightNetplayData;

struct FightSyncCheckData {
	int mLife1;
	int mLife2;
};

static Buffer gatherNetplaySyncCheckData(void*) {
	Buffer b = makeBufferEmptyOwned();

	FightSyncCheckData syncCheckData;
	syncCheckData.mLife1 = getPlayerLife(getRootPlayer(0));
	syncCheckData.mLife2 = getPlayerLife(getRootPlayer(1));
	appendBufferBuffer(&b, makeBuffer(&syncCheckData, sizeof(FightSyncCheckData)));
	return b;
}

static int checkNetplaySyncCheckData(void*, const Buffer& b1, const Buffer& b2) {
	const auto checkData1 = (FightSyncCheckData*)b1.mData;
	const auto checkData2 = (FightSyncCheckData*)b2.mData;
	return checkData1->mLife1 == checkData2->mLife1 && checkData1->mLife2 == checkData2->mLife2;
}

static void initFightNetplay(void*) {
	gFightNetplayData.mHasReceivedNetplayData = false;
	setNetplaySyncCBs(gatherNetplaySyncCheckData, NULL, checkNetplaySyncCheckData, NULL);
}

static void shutdownFightNetplay(void*) {
	setNetplaySyncCBs(NULL, NULL, NULL, NULL);
}

int hasNewFightNetplayReceivedData() {
	return gFightNetplayData.mHasReceivedNetplayData;
}

FightNetplayData popFightNetplayReceivedData() {
	assert(gFightNetplayData.mHasReceivedNetplayData);
	gFightNetplayData.mHasReceivedNetplayData = false;
	return gFightNetplayData.mCachedReceivedNetplayData;
}

void sendFightNetplayData(const Buffer& tData) {
	sendNetplayData(tData);
}

ActorBlueprint getFightNetplayBlueprint() {
	return makeActorBlueprint(initFightNetplay, shutdownFightNetplay);
}