#pragma once

#include <string>
#include <vector>

#include <prism/file.h>
#include <prism/actorhandler.h>

using namespace prism;

struct FightNetplaySingleCommandSent{
	std::string mName;
	int mIsActive;
	int mNow;
	int mBufferTime;
};

struct FightNetplayData
{
	std::vector<FightNetplaySingleCommandSent> mCommandStatus;
};

int hasNewFightNetplayReceivedData();
FightNetplayData popFightNetplayReceivedData();

void sendFightNetplayData(const Buffer& tData);

ActorBlueprint getFightNetplayBlueprint();