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

typedef enum {
	EXPLOD_SPACE_NONE,
	EXPLOD_SPACE_SCREEN,
	EXPLOD_SPACE_STAGE,
} DreamExplodSpace;

int addExplod(DreamPlayer* tPlayer);
void setExplodAnimation(int tID, int tIsInFightDefFile, int tAnimationNumber);
void setExplodID(int tID, int tExternalID);
void setExplodSpace(int tID, DreamExplodSpace tSpace);
void setExplodPosition(int tID, int tOffsetX, int tOffsetY);
void setExplodPositionType(int tID, DreamExplodPositionType tType);
void setExplodHorizontalFacing(int tID, int tFacing);
void setExplodVerticalFacing(int tID, int tFacing);
void setExplodBindTime(int tID, int tBindTime);
void setExplodVelocity(int tID, double tX, double tY);
void setExplodAcceleration(int tID, double tX, double tY);
void setExplodRandomOffset(int tID, int tX, int tY);
void setExplodRemoveTime(int tID, int tRemoveTime);
void setExplodSuperMove(int tID, int tCanMoveDuringSuperMove);
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

void updateExplodAnimation(DreamPlayer* tPlayer, int tID, int tIsInFightDefFile, int tAnimationNumber);
void updateExplodSpace(DreamPlayer* tPlayer, int tID, DreamExplodSpace tSpace);
void updateExplodPosition(DreamPlayer* tPlayer, int tID, int tOffsetX, int tOffsetY);
void updateExplodPositionType(DreamPlayer* tPlayer, int tID, DreamExplodPositionType tType);
void updateExplodHorizontalFacing(DreamPlayer* tPlayer, int tID, int tFacing);
void updateExplodVerticalFacing(DreamPlayer* tPlayer, int tID, int tFacing);
void updateExplodBindTime(DreamPlayer* tPlayer, int tID, int tBindTime);
void updateExplodVelocity(DreamPlayer* tPlayer, int tID, double tX, double tY);
void updateExplodAcceleration(DreamPlayer* tPlayer, int tID, double tX, double tY);
void updateExplodRandomOffset(DreamPlayer* tPlayer, int tID, int tX, int tY);
void updateExplodRemoveTime(DreamPlayer* tPlayer, int tID, int tRemoveTime);
void updateExplodSuperMove(DreamPlayer* tPlayer, int tID, int tIsSuperMove);
void updateExplodSuperMoveTime(DreamPlayer* tPlayer, int tID, int tSuperMoveTime);
void updateExplodPauseMoveTime(DreamPlayer* tPlayer, int tID, int tPauseMoveTime);
void updateExplodScale(DreamPlayer* tPlayer, int tID, double tX, double tY);
void updateExplodSpritePriority(DreamPlayer* tPlayer, int tID, int tSpritePriority);
void updateExplodOnTop(DreamPlayer* tPlayer, int tID, int tIsOnTop);
void updateExplodShadow(DreamPlayer* tPlayer, int tID, int tR, int tG, int tB);
void updateExplodOwnPalette(DreamPlayer* tPlayer, int tID, int tUsesOwnPalette);
void updateExplodRemoveOnGetHit(DreamPlayer* tPlayer, int tID, int tIsRemovedOnGetHit);
void updateExplodIgnoreHitPause(DreamPlayer* tPlayer, int tID, int tIgnoreHitPause);
void updateExplodTransparencyType(DreamPlayer* tPlayer, int tID, DreamExplodTransparencyType tTransparencyType);

void removeExplodsWithID(DreamPlayer* tPlayer, int tExplodID);
void removeAllExplodsForPlayer(DreamPlayer* tPlayer);
void removeExplodsForPlayerAfterHit(DreamPlayer* tPlayer);
void removeAllExplods();
int getExplodIndexFromExplodID(DreamPlayer* tPlayer, int tExplodID);

void setPlayerExplodPaletteEffects(DreamPlayer* tPlayer, int tDuration, const Vector3D& tAddition, const Vector3D& tMultiplier, const Vector3D& tSineAmplitude, int tSinePeriod, int tInvertAll, double tColorFactor, int tIgnoreOwnPal);

int getExplodAmount(DreamPlayer* tPlayer);
int getExplodAmountWithID(DreamPlayer* tPlayer, int tID);

void setExplodsSpeed(double tSpeed);
void setAllExplodsNoShadow();

ActorBlueprint getDreamExplodHandler();