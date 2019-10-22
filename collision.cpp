#include "collision.h"

#include <prism/collisionhandler.h>

static struct {
	CollisionListData* mPlayerAttackCollisionList[2];
	CollisionListData* mPlayerPassiveCollisionList[2];

} gDolmexicaCollisionData;

void setupDreamGameCollisions()
{
	int i;
	for (i = 0; i < 2; i++) {
		gDolmexicaCollisionData.mPlayerPassiveCollisionList[i] = addCollisionListToHandler();
		gDolmexicaCollisionData.mPlayerAttackCollisionList[i] = addCollisionListToHandler();
	}

	for (i = 0; i < 2; i++) {
		int other = i ^ 1;
		addCollisionHandlerCheck(gDolmexicaCollisionData.mPlayerAttackCollisionList[i], gDolmexicaCollisionData.mPlayerPassiveCollisionList[other]);
	}
}

CollisionListData* getDreamPlayerPassiveCollisionList(DreamPlayer* p)
{
	return gDolmexicaCollisionData.mPlayerPassiveCollisionList[p->mRootID];
}

CollisionListData* getDreamPlayerAttackCollisionList(DreamPlayer* p)
{
	return gDolmexicaCollisionData.mPlayerAttackCollisionList[p->mRootID];
}
