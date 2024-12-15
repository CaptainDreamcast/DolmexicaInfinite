#pragma once

#include "playerdefinition.h"

using namespace prism;

namespace prism {
	struct CollisionListData;
}

void setupDreamGameCollisions();
CollisionListData* getDreamPlayerPassiveCollisionList(DreamPlayer* p);
CollisionListData* getDreamPlayerAttackCollisionList(DreamPlayer* p);