#pragma once

#include "playerdefinition.h"

struct CollisionListData;

void setupDreamGameCollisions();
CollisionListData* getDreamPlayerPassiveCollisionList(DreamPlayer* p);
CollisionListData* getDreamPlayerAttackCollisionList(DreamPlayer* p);