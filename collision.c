#include "collision.h"

#include <tari/collisionhandler.h>

static struct {
	int mPlayerAttackCollisionList[2];
	int mPlayerPassiveCollisionList[2];

} gData;

void setupDreamGameCollisions()
{
	int i;
	for (i = 0; i < 2; i++) {
		gData.mPlayerPassiveCollisionList[i] = addCollisionListToHandler();
		gData.mPlayerAttackCollisionList[i] = addCollisionListToHandler();
	}

	for (i = 0; i < 2; i++) {
		int other = i ^ 1;
		addCollisionHandlerCheck(gData.mPlayerAttackCollisionList[i], gData.mPlayerPassiveCollisionList[other]);
	}
}

int getDreamPlayerPassiveCollisionList(DreamPlayer* p)
{
	return gData.mPlayerPassiveCollisionList[p->mRootID];
}

int getDreamPlayerAttackCollisionList(DreamPlayer* p)
{
	return gData.mPlayerAttackCollisionList[p->mRootID];
}
