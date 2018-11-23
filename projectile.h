#pragma once

#include "playerdefinition.h"

#include <prism/actorhandler.h>

ActorBlueprint getProjectileHandler();

void addAdditionalProjectileData(DreamPlayer* tProjectile);
void removeAdditionalProjectileData(DreamPlayer* tProjectile);
void handleProjectileHit(DreamPlayer* tProjectile);

void setProjectileID(DreamPlayer* p, int tID);

void setProjectileAnimation(DreamPlayer* p, int tAnimation);
int getProjectileHitAnimation(DreamPlayer* p);
void setProjectileHitAnimation(DreamPlayer* p, int tAnimation);
int getProjectileRemoveAnimation(DreamPlayer* p);
void setProjectileRemoveAnimation(DreamPlayer* p, int tAnimation);
void setProjectileCancelAnimation(DreamPlayer* p, int tAnimation);
void setProjectileScale(DreamPlayer* p, double tX, double tY);
void setProjectileRemoveAfterHit(DreamPlayer* p, int tValue);
void setProjectileRemoveTime(DreamPlayer* p, int tTime);
void setProjectileVelocity(DreamPlayer* p, double tX, double tY);
void setProjectileRemoveVelocity(DreamPlayer* p, double tX, double tY);
void setProjectileAcceleration(DreamPlayer* p, double tX, double tY);
void setProjectileVelocityMultipliers(DreamPlayer* p, double tX, double tY);


void setProjectileHitAmountBeforeVanishing(DreamPlayer* p, int tHitAmount);
void setProjectilMisstime(DreamPlayer* p, int tMissTime);
void setProjectilePriority(DreamPlayer* p, int tPriority);
void setProjectileSpritePriority(DreamPlayer* p, int tSpritePriority);

void setProjectileEdgeBound(DreamPlayer* p, int tEdgeBound);
void setProjectileStageBound(DreamPlayer* p, int tStageBound);
void setProjectileHeightBoundValues(DreamPlayer* p, int tLowerBound, int tUpperBound);
void setProjectilePosition(DreamPlayer* p, Position tPosition);

void setProjectileShadow(DreamPlayer* p, int tShadow);
void setProjectileSuperMoveTime(DreamPlayer* p, int tSuperMoveTime);
void setProjectilePauseMoveTime(DreamPlayer* p, int tPauseMoveTime);

void setProjectileHasOwnPalette(DreamPlayer* p, int tValue);
void setProjectileRemapPalette(DreamPlayer* p, int tGroup, int tItem); // TODO: rename
void setProjectileAfterImageTime(DreamPlayer* p, int tAfterImageTime);
void setProjectileAfterImageLength(DreamPlayer* p, int tAfterImageLength);
void setProjectileAfterImage(DreamPlayer* p, int tAfterImage);