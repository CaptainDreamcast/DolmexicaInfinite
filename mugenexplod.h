#pragma once

#include <prism/actorhandler.h>

#include "playerdefinition.h"

typedef enum {
	EXPLOD_POSITION_TYPE_RELATIVE_TO_P1,
	EXPLOD_POSITION_TYPE_RELATIVE_TO_P2,
	EXPLOD_POSITION_TYPE_RELATIVE_TO_FRONT,
	EXPLOD_POSITION_TYPE_RELATIVE_TO_BACK,
	EXPLOD_POSITION_TYPE_RELATIVE_TO_LEFT,
	EXPLOD_POSITION_TYPE_RELATIVE_TO_RIGHT,
	EXPLOD_POSITION_TYPE_NONE,
} DreamExplodPositionType;

typedef enum {
	EXPLOD_TRANSPARENCY_TYPE_ALPHA,
	EXPLOD_TRANSPARENCY_TYPE_ADD_ALPHA,
} DreamExplodTransparencyType;


int addExplod(DreamPlayer* tPlayer);
void setExplodAnimation(int tID, int tIsInFightDefFile, int tAnimationNumber);
void setExplodID(int tID, int tExternalID);
void setExplodPosition(int tID, int tOffsetX, int tOffsetY);
void setExplodPositionType(int tID, DreamExplodPositionType tType);
void setExplodHorizontalFacing(int tID, int tFacing);
void setExplodVerticalFacing(int tID, int tFacing);
void setExplodBindTime(int tID, int tBindTime);
void setExplodVelocity(int tID, double tX, double tY);
void setExplodAcceleration(int tID, double tX, double tY);
void setExplodRandomOffset(int tID, int tX, int tY);
void setExplodRemoveTime(int tID, int tRemoveTime);
void setExplodSuperMove(int tID, int tIsSuperMove);
void setExplodSuperMoveTime(int tID, int tSuperMoveTime);
void setExplodPauseMoveTime(int tID, int tPauseMoveTime);
void setExplodScale(int tID, double tX, double tY);
void setExplodSpritePriority(int tID, int tSpritePriority);
void setExplodOnTop(int tID, int tIsOnTop);
void setExplodShadow(int tID, int tR, int tG, int tB);
void setExplodOwnPalette(int tID, int tUsesOwnPalette);
void setExplodRemoveOnGetHit(int tID, int tIsRemovedOnGetHit);
void setExplodIgnoreHitPause(int tID, int tIgnoreHitPause);
void setExplodTransparencyType(int tID, int tHasTransparencyType, DreamExplodTransparencyType tTransparencyType);
void finalizeExplod(int tID);
void removeExplodsWithID(DreamPlayer* tPlayer, int tExplodID);
void removeAllExplodsForPlayer(DreamPlayer* tPlayer);
void removeAllExplods();
int getExplodIndexFromExplodID(DreamPlayer* tPlayer, int tExplodID);

int getExplodAmount(DreamPlayer* tPlayer);
int getExplodAmountWithID(DreamPlayer* tPlayer, int tID);


ActorBlueprint getDreamExplodHandler();

Position getFinalExplodPositionFromPositionType(DreamExplodPositionType tPositionType, Position mOffset, DreamPlayer* tPlayer);

void setExplodBindTimeForID(DreamPlayer* tPlayer, int tExplodID, int tBindTime);