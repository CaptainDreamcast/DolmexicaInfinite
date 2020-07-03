#pragma once

#include "playerdefinition.h"

#include <prism/actorhandler.h>

ActorBlueprint getProjectileHandler();

void addAdditionalProjectileData(DreamPlayer* tProjectile);
void removeAdditionalProjectileData(DreamPlayer* tProjectile);
void handleProjectileHit(DreamPlayer* tProjectile, int tWasGuarded, int tWasCanceled);

void setProjectileID(DreamPlayer* p, int tID);
int getProjectileID(DreamPlayer* p);

void setProjectileAnimation(DreamPlayer* p, int tAnimation);
int getProjectileHitAnimation(DreamPlayer* p);
void setProjectileHitAnimation(DreamPlayer* p, int tAnimation);
int getProjectileRemoveAnimation(DreamPlayer* p);
void setProjectileRemoveAnimation(DreamPlayer* p, int tAnimation);
void setProjectileCancelAnimation(DreamPlayer* p, int tAnimation);
void setProjectileScale(DreamPlayer* p, double tX, double tY);
void setProjectileRemoveAfterHit(DreamPlayer* p, int tValue);
void setProjectileRemoveTime(DreamPlayer* p, int tTime);
void setProjectileVelocity(DreamPlayer* p, double tX, double tY, int tCoordinateP);
void setProjectileRemoveVelocity(DreamPlayer* p, double tX, double tY, int tCoordinateP);
void setProjectileAcceleration(DreamPlayer* p, double tX, double tY, int tCoordinateP);
void setProjectileVelocityMultipliers(DreamPlayer* p, double tX, double tY);

void setProjectileHitAmountBeforeVanishing(DreamPlayer* p, int tHitAmount);
void setProjectilMisstime(DreamPlayer* p, int tMissTime);
int getProjectilePriority(DreamPlayer* p);
void setProjectilePriority(DreamPlayer* p, int tPriority);
void reduceProjectilePriorityAndResetHitData(DreamPlayer* p);
void setProjectileSpritePriority(DreamPlayer* p, int tSpritePriority);

void setProjectileEdgeBound(DreamPlayer* p, int tEdgeBound, int tCoordinateP);
void setProjectileStageBound(DreamPlayer* p, int tStageBound, int tCoordinateP);
void setProjectileHeightBoundValues(DreamPlayer* p, int tLowerBound, int tUpperBound, int tCoordinateP);
void setProjectilePosition(DreamPlayer* p, const Position2D& tPosition, int tCoordinateP);

void setProjectileShadow(DreamPlayer* p, int tShadow);
void setProjectileSuperMoveTime(DreamPlayer* p, int tSuperMoveTime);
void setProjectilePauseMoveTime(DreamPlayer* p, int tPauseMoveTime);

void setProjectileHasOwnPalette(DreamPlayer* p, int tValue);
void setProjectileRemapPalette(DreamPlayer* p, int tGroup, int tItem);

int canProjectileHit(DreamPlayer* p);