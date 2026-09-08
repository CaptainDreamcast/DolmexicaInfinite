#pragma once

#include <prism/datastructures.h>

#include <prism/mugenspritefilereader.h>
#include <prism/mugenanimationhandler.h>
#include <prism/mugensoundfilereader.h>
#include <prism/actorhandler.h>

#include "mugenstatereader.h"
#include "mugencommandreader.h"
#include "playerhitdata.h"
#include "afterimage.h"
#include "mugenstatehandler.h"

using namespace prism;

namespace prism {
	struct PhysicsHandlerElement;
}

typedef enum {
	PLAYER_BIND_POSITION_TYPE_AXIS,
	PLAYER_BIND_POSITION_TYPE_HEAD,
	PLAYER_BIND_POSITION_TYPE_MID,
} DreamPlayerBindPositionType;

typedef enum {
	VICTORY_TYPE_NORMAL,
	VICTORY_TYPE_SPECIAL,
	VICTORY_TYPE_HYPER,
	VICTORY_TYPE_THROW,
	VICTORY_TYPE_CHEESE,
	VICTORY_TYPE_TIMEOVER,
	VICTORY_TYPE_SUICIDE,
	VICTORY_TYPE_TEAMMATE, // never used because team modes not part of Dolmexica
} VictoryType;

#define PLAYER_Z 40
#define PLAYER_Z_PRIORITY_DELTA 0.1
#define PLAYER_Z_PLAYER_2_OFFSET 0.01
#define EXPLOD_SPRITE_Z_OFFSET 0.001

typedef struct {
	char mDefinitionPath[1024];
	int mHasPalettePath;
	char* mPalettePath;
	char* mSpritePath;
	DreamMugenCommands mCommands;
	MugenAnimations mAnimations;
	MugenSpriteFile mSprites;
	MugenSounds mSounds;
	DreamMugenConstants mConstants;
} DreamPlayerFiles;


typedef struct {
	char mName[100];
	char mDisplayName[100];
	char mVersion[100];
	char mMugenVersion[100];
	char mAuthor[100];

	char mPaletteDefaults[100];
	Vector3DI mLocalCoordinates;

} DreamPlayerHeaderConstants;

typedef struct {
	int mHasCustomDisplayName;
	char mDisplayName[100];
} DreamPlayerHeaderCustomOverrides;

typedef struct {
	DreamPlayerHeaderConstants mConstants;
	DreamPlayerFiles mFiles;
	DreamPlayerHeaderCustomOverrides mCustomOverrides;
} DreamPlayerHeader;

typedef struct {
	MugenAnimationHandlerElement* mAnimationElement;

	Position mShadowPosition;

} DreamPlayerShadow;

typedef struct {
	MugenAnimationHandlerElement* mAnimationElement;
	Position mPosition;

} DreamPlayerReflection;

typedef struct {
	int mLastDustTime;

} DreamPlayerDust;

typedef struct {
	int mCollisionTextID;
	Position mCollisionTextPosition;

} DreamPlayerDebugData;

struct DreamPlayer;
struct DreamPlayerTargetStableOrder {
	bool operator()(const std::pair<int, DreamPlayer*>& a, const std::pair<int, DreamPlayer*>& b) const;
};

struct DreamPlayer {
	DreamPlayerHeader* mHeader;
	DreamMugenConstantsSizeData mCustomSizeData;

	DreamPlayer* mRoot;
	DreamPlayer* mOtherPlayer;
	int mPreferredPalette;
	int mRootID;
	int mControllerID;
	int mID;
	int mIsHelper;
	int mIsProjectile;
	int mProjectileID;
	int mProjectileDataID;

	List mHelpers; // contains DreamPlayer
	std::list<PlayerHitData> mReceivedHitData;
	std::set<DreamPlayer*> mReceivedReversalDefPlayers;
	std::set<std::pair<int, DreamPlayer*>, DreamPlayerTargetStableOrder> mActiveTargets;
	DreamPlayer* mParent;
	int mHelperIDInParent;
	int mHelperIDInRoot;
	int mHelperIDInStore;

	IntMap mProjectiles; // contains DreamPlayer
	int mHasLastContactProjectile;
	int mLastContactProjectileID;
	int mLastContactProjectileTime;
	int mLastContactProjectileWasCanceled;
	int mLastContactProjectileWasGuarded;
	int mLastContactProjectileWasHit;

	int mAILevel;

	int mVars[100];
	int mSystemVars[100];
	float mFloatVars[100];
	float mSystemFloatVars[100];

	int mCommandID;
	RegisteredMugenStateMachine* mRegisteredStateMachine;
	MugenAnimations* mActiveAnimations;
	MugenAnimationHandlerElement* mAnimationElement;

	PhysicsHandlerElement* mPhysicsElement;
	int mHitDataID;
	PlayerHitData mPassiveHitData;
	PlayerHitData mActiveHitData;
	PlayerHitOverrides mHitOverrides;

	DreamMugenStateType mStateType;
	DreamMugenStateMoveType mMoveType;
	DreamMugenStatePhysics mStatePhysics;

	int mIsInControl;
	int mMoveContactCounter;
	int mMoveHit;
	int mMoveGuarded;
	int mLastHitGuarded;
	int mIsAlive;
	FaceDirection mFaceDirection;

	int mNoWalkFlag;
	int mNoAutoTurnFlag;
	int mNoLandFlag;
	int mPushDisabledFlag;
	int mNoJuggleCheckFlag;
	int mIntroFlag;
	int mNoAirGuardFlag;
	int mNoCrouchGuardFlag;
	int mNoStandGuardFlag;
	int mNoKOSoundFlag;
	int mNoKOSlowdownFlag;
	int mUnguardableFlag;
	int mTransparencyFlag;

	int mWidthFlag;
	int mInvisibilityFlag;
	Vector2DI mOneTickStageWidth;
	Vector2DI mOneTickPlayerWidth;
	Vector3D mDrawOffset;

	int mJumpFlank;
	int mAirJumpCounter;

	int mIsHitOver;
	int mIsHitOverridden;
	int mIsFalling;
	int mCanRecoverFromFall;
	int mRecoverTimeSinceHitPause;
	int mRecoverTime;

	float mDefenseMultiplier;
	float mSuperDefenseMultiplier;

	int mIsFrozen;
	Position mFreezePosition;

	int mIsLyingDown;
	int mLyingDownTime;

	int mIsHitPaused;
	int mHitPauseNow;
	int mHitPauseDuration;

	int mSuperMoveTime;
	int mPauseMoveTime;

	int mIsHitShakeActive;
	int mHitShakeNow;
	int mHitShakeDuration;

	int mIsHitOverWaitActive;
	int mHitOverNow;
	int mHitOverDuration;

	int mIsAngleActive;
	float mAngle;

	Vector2D mTempScale;

	int mLife;
	int mPower;

	int mCheeseWinFlag;
	int mSuicideWinFlag;

	int mHitCount;
	int mFallAmountInCombo;

	float mAttackMultiplier;

	int mMoveReversed;

	int mIsBound;
	int mBoundNow;
	int mBoundDuration;
	int mBoundFaceSet;
	Position2D mBoundOffsetCameraSpace;
	DreamPlayerBindPositionType mBoundPositionType;
	DreamPlayer* mBoundTarget;
	int mBoundID;

	List mBoundHelpers;

	int mRoundsExisted;
	int mComboCounter;
	int mDisplayedComboCounter;

	int mRoundsWon;

	int mIsBoundToScreenForever;
	int mIsBoundToScreenForTick;
	Vector3DI mIsCameraFollowing;

	float mStartLifePercentage;

	int mIsGuardingInternally;
	int mIsBeingJuggled;
	int mAirJugglePoints;

	float mTimeDilatationNow;
	int mTimeDilatationUpdates;
	float mTimeDilatation;

	int mHasOwnPalette;

	DreamPlayerDust mDustClouds[2];

	DreamHitDefAttributeSlot mNotHitBy[2];

	DreamPlayerShadow mShadow;
	DreamPlayerReflection mReflection;
	DreamPlayerAfterImage mAfterImage;
	DreamPlayerDebugData mDebug;

	int mIsDestroyed;
};

void loadPlayers(MemoryStack* tMemoryStack);
void loadPlayerSprites();
void unloadPlayers();
void resetPlayers();
void resetPlayersEntirely();
void resetPlayerPosition(DreamPlayer* p);
void updatePlayers();
void drawPlayers();

ActorBlueprint getPreStateMachinePlayersBlueprint();
ActorBlueprint getPostStateMachinePlayersBlueprint();

int hasLoadedPlayerSprites();

void playerHitCB(void* tData, void* tHitData, int tOtherCollisionList);
void playerReversalHitCB(void* tData, void* tHitData, int tOtherCollisionList);

void setPlayerDefinitionPath(int i, const char* tDefinitionPath);
void getPlayerDefinitionPath(char* tDst, int i);
void setPlayerPreferredPalette(int i, int tPalette);
void setPlayerPreferredPaletteRandom(int i);

DreamPlayer* getRootPlayer(int i);
DreamPlayer* getPlayerRoot(DreamPlayer* p);
DreamPlayer* getPlayerParent(DreamPlayer* p);

int getPlayerState(DreamPlayer* p); 
int getPlayerPreviousState(DreamPlayer* p);
int getPlayerStateJugglePoints(DreamPlayer* p);

DreamMugenStateType getPlayerStateType(DreamPlayer* p);
void setPlayerStateType(DreamPlayer* p, DreamMugenStateType tType);
DreamMugenStateMoveType getPlayerStateMoveType(DreamPlayer* p);
void setPlayerStateMoveType(DreamPlayer* p, DreamMugenStateMoveType tType);

int getPlayerControl(DreamPlayer* p);
void setPlayerControl(DreamPlayer* p, int tNewControl);

DreamMugenStatePhysics getPlayerPhysics(DreamPlayer* p);
void setPlayerPhysics(DreamPlayer* p, DreamMugenStatePhysics tNewPhysics);

int getPlayerMoveContactCounter(DreamPlayer* p);
void resetPlayerMoveContactCounter(DreamPlayer* p);
void setPlayerMoveContactCounterActive(DreamPlayer* p);

int getPlayerVariable(DreamPlayer* p, int tIndex);
int* getPlayerVariableReference(DreamPlayer* p, int tIndex);
void setPlayerVariable(DreamPlayer* p, int tIndex, int tValue);
void addPlayerVariable(DreamPlayer* p, int tIndex, int tValue);
int getPlayerSystemVariable(DreamPlayer* p, int tIndex);
void setPlayerSystemVariable(DreamPlayer* p, int tIndex, int tValue);
void addPlayerSystemVariable(DreamPlayer* p, int tIndex, int tValue);
float getPlayerFloatVariable(DreamPlayer* p, int tIndex);
void setPlayerFloatVariable(DreamPlayer* p, int tIndex, float tValue);
void addPlayerFloatVariable(DreamPlayer* p, int tIndex, float tValue);
float getPlayerSystemFloatVariable(DreamPlayer* p, int tIndex);
void setPlayerSystemFloatVariable(DreamPlayer* p, int tIndex, float tValue);
void addPlayerSystemFloatVariable(DreamPlayer* p, int tIndex, float tValue);

int getPlayerTimeInState(DreamPlayer* p);
int getPlayerAnimationNumber(DreamPlayer* p);
int getPlayerAnimationStep(DreamPlayer* p);
int getPlayerAnimationStepAmount(DreamPlayer* p);
int getPlayerAnimationStepDuration(DreamPlayer* p);
int getPlayerAnimationTimeDeltaUntilFinished(DreamPlayer* p);
int isPlayerAnimationTimeInfinite(DreamPlayer* p);
int hasPlayerAnimationLooped(DreamPlayer* p);
int getPlayerAnimationDuration(DreamPlayer* p);
int getPlayerAnimationTime(DreamPlayer* p);
int getPlayerSpriteGroup(DreamPlayer* p);
int getPlayerSpriteElement(DreamPlayer* p);

Vector2D getPlayerPosition(DreamPlayer* p, int tCoordinateP);
float getPlayerPositionBasedOnScreenCenterX(DreamPlayer* p, int tCoordinateP);
float getPlayerScreenPositionX(DreamPlayer* p, int tCoordinateP);
float getPlayerPositionX(DreamPlayer* p, int tCoordinateP);
float getPlayerPositionBasedOnStageFloorY(DreamPlayer* p, int tCoordinateP);
float getPlayerScreenPositionY(DreamPlayer* p, int tCoordinateP);
float getPlayerPositionY(DreamPlayer* p, int tCoordinateP);
float getPlayerVelocityX(DreamPlayer* p, int tCoordinateP);
float getPlayerVelocityY(DreamPlayer* p, int tCoordinateP);

int getPlayerDataLife(DreamPlayer* p);
int getPlayerDataAttack(DreamPlayer* p);
float getPlayerDataAttackFactor(DreamPlayer* p);
int getPlayerDataDefense(DreamPlayer* p); 
float getPlayerDataDefenseFactor(DreamPlayer* p); 
int getPlayerDataLiedownTime(DreamPlayer* p);
int getPlayerDataAirjuggle(DreamPlayer* p);
int getPlayerDataSparkNo(DreamPlayer* p);
int getPlayerDataGuardSparkNo(DreamPlayer* p);
int getPlayerDataKOEcho(DreamPlayer* p);
int getPlayerDataIntPersistIndex(DreamPlayer* p);
int getPlayerDataFloatPersistIndex(DreamPlayer* p);

int getPlayerSizeAirBack(DreamPlayer* p, int tCoordinateP);
int getPlayerSizeAirFront(DreamPlayer* p, int tCoordinateP);
int getPlayerSizeAttackDist(DreamPlayer* p, int tCoordinateP);
int getPlayerSizeProjectileAttackDist(DreamPlayer* p, int tCoordinateP);
int getPlayerSizeProjectilesDoScale(DreamPlayer* p);
int getPlayerSizeShadowOffset(DreamPlayer* p, int tCoordinateP);
int getPlayerSizeDrawOffsetX(DreamPlayer* p, int tCoordinateP);
int getPlayerSizeDrawOffsetY(DreamPlayer* p, int tCoordinateP);

float getPlayerVelocityAirGetHitGroundRecoverX(DreamPlayer* p, int tCoordinateP);
float getPlayerVelocityAirGetHitGroundRecoverY(DreamPlayer* p, int tCoordinateP);
float getPlayerVelocityAirGetHitAirRecoverMulX(DreamPlayer* p);
float getPlayerVelocityAirGetHitAirRecoverMulY(DreamPlayer* p);
float getPlayerVelocityAirGetHitAirRecoverAddX(DreamPlayer* p, int tCoordinateP);
float getPlayerVelocityAirGetHitAirRecoverAddY(DreamPlayer* p, int tCoordinateP);
float getPlayerVelocityAirGetHitAirRecoverBack(DreamPlayer* p, int tCoordinateP);
float getPlayerVelocityAirGetHitAirRecoverFwd(DreamPlayer* p, int tCoordinateP);
float getPlayerVelocityAirGetHitAirRecoverUp(DreamPlayer* p, int tCoordinateP);
float getPlayerVelocityAirGetHitAirRecoverDown(DreamPlayer* p, int tCoordinateP);

int getPlayerMovementAirJumpNum(DreamPlayer* p);
void setPlayerMovementAirJumpNum(DreamPlayer* p, int tAmount); // for testing only
int getPlayerMovementAirJumpHeight(DreamPlayer* p, int tCoordinateP);
float getPlayerMovementJumpChangeAnimThreshold(DreamPlayer* p, int tCoordinateP);
float getPlayerMovementAirGetHitAirRecoverYAccel(DreamPlayer* p, int tCoordinateP);

float getPlayerStandFriction(DreamPlayer* p);
float getPlayerStandFrictionThreshold(DreamPlayer* p, int tCoordinateP);
float getPlayerCrouchFriction(DreamPlayer* p);
float getPlayerCrouchFrictionThreshold(DreamPlayer* p, int tCoordinateP);
float getPlayerAirGetHitGroundLevelY(DreamPlayer* p, int tCoordinateP);
float getPlayerAirGetHitGroundRecoveryGroundLevelY(DreamPlayer* p, int tCoordinateP);
float getPlayerAirGetHitGroundRecoveryGroundYTheshold(DreamPlayer* p, int tCoordinateP);
float getPlayerAirGetHitAirRecoveryVelocityYThreshold(DreamPlayer* p, int tCoordinateP);
float getPlayerAirGetHitTripGroundLevelY(DreamPlayer* p, int tCoordinateP);
float getPlayerDownBounceOffsetX(DreamPlayer* p, int tCoordinateP);
float getPlayerDownBounceOffsetY(DreamPlayer* p, int tCoordinateP);
float getPlayerDownVerticalBounceAcceleration(DreamPlayer* p, int tCoordinateP);
float getPlayerDownBounceGroundLevel(DreamPlayer* p, int tCoordinateP);
float getPlayerLyingDownFrictionThreshold(DreamPlayer* p, int tCoordinateP);
float getPlayerVerticalAcceleration(DreamPlayer* p, int tCoordinateP);

float getPlayerForwardWalkVelocityX(DreamPlayer* p, int tCoordinateP);
float getPlayerBackwardWalkVelocityX(DreamPlayer* p, int tCoordinateP);
float getPlayerForwardRunVelocityX(DreamPlayer* p, int tCoordinateP);
float getPlayerForwardRunVelocityY(DreamPlayer* p, int tCoordinateP);
float getPlayerBackwardRunVelocityX(DreamPlayer* p, int tCoordinateP);
float getPlayerBackwardRunVelocityY(DreamPlayer* p, int tCoordinateP);
float getPlayerBackwardRunJumpVelocityX(DreamPlayer* p, int tCoordinateP);
float getPlayerForwardRunJumpVelocityX(DreamPlayer* p, int tCoordinateP);
float getPlayerNeutralJumpVelocityX(DreamPlayer* p, int tCoordinateP);
float getPlayerForwardJumpVelocityX(DreamPlayer* p, int tCoordinateP);
float getPlayerBackwardJumpVelocityX(DreamPlayer* p, int tCoordinateP);
float getPlayerJumpVelocityY(DreamPlayer* p, int tCoordinateP);
float getPlayerNeutralAirJumpVelocityX(DreamPlayer* p, int tCoordinateP);
float getPlayerForwardAirJumpVelocityX(DreamPlayer* p, int tCoordinateP);
float getPlayerBackwardAirJumpVelocityX(DreamPlayer* p, int tCoordinateP);
float getPlayerAirJumpVelocityY(DreamPlayer* p, int tCoordinateP);

int isPlayerAlive(DreamPlayer* p);
int isPlayerDestroyed(DreamPlayer* p);

void setPlayerVelocityX(DreamPlayer* p, float x, int tCoordinateP);
void setPlayerVelocityY(DreamPlayer* p, float y, int tCoordinateP);
void multiplyPlayerVelocityX(DreamPlayer* p, float x);
void multiplyPlayerVelocityY(DreamPlayer* p, float y);
void addPlayerVelocityX(DreamPlayer* p, float x, int tCoordinateP);
void addPlayerVelocityY(DreamPlayer* p, float y, int tCoordinateP);

void setPlayerPosition(DreamPlayer* p, const Position2D& tPosition, int tCoordinateP);
void setPlayerPositionX(DreamPlayer* p, float x, int tCoordinateP);
void setPlayerPositionY(DreamPlayer* p, float y, int tCoordinateP);
void addPlayerPositionX(DreamPlayer* p, float x, int tCoordinateP);
void addPlayerPositionY(DreamPlayer* p, float y, int tCoordinateP);
void setPlayerPositionBasedOnScreenCenterX(DreamPlayer* p, float x, int tCoordinateP);

int isPlayerCommandActive(DreamPlayer* p, const char* tCommandName);
int isPlayerCommandActiveWithLookup(DreamPlayer* p, int tCommandLookupIndex);

int hasPlayerState(DreamPlayer* p, int mNewState);
int hasPlayerStateSelf(DreamPlayer* p, int mNewState);
void changePlayerState(DreamPlayer* p, int mNewState);
void changePlayerStateToSelf(DreamPlayer* p, int mNewState);
void changePlayerStateToOtherPlayerStateMachine(DreamPlayer* p, DreamPlayer* tOtherPlayer, int mNewState);
void changePlayerStateToOtherPlayerStateMachineBeforeImmediatelyEvaluatingIt(DreamPlayer* p, DreamPlayer* tOtherPlayer, int mNewState);
void changePlayerStateBeforeImmediatelyEvaluatingIt(DreamPlayer* p, int mNewState);
void changePlayerStateToSelfBeforeImmediatelyEvaluatingIt(DreamPlayer* p, int tNewState);
void changePlayerStateIfDifferent(DreamPlayer* p, int tNewState);
void setPlayerStatemachineToUpdateAgain(DreamPlayer* p);

void changePlayerAnimation(DreamPlayer* p, int tNewAnimation);
void changePlayerAnimationWithStartStep(DreamPlayer* p, int tNewAnimation, int tStartStep);
void changePlayerAnimationToPlayer2AnimationWithStartStep(DreamPlayer* p, int tNewAnimation, int tStartStep);
void setPlayerAnimationFinishedCallback(DreamPlayer* p, void(*tFunc)(void*), void* tCaller);

int isPlayerStartingAnimationElementWithID(DreamPlayer* p, int tStepID);
int getPlayerTimeFromAnimationElement(DreamPlayer* p, int tStep);
int getPlayerAnimationElementFromTimeOffset(DreamPlayer* p, int tTime);
int isPlayerAnimationTimeOffsetInAnimation(DreamPlayer* p, int tTime);
int getPlayerAnimationTimeWhenStepStarts(DreamPlayer* p, int tStep);

float calculateSpriteZFromSpritePriority(int tPriority, int tRootID, int tIsExplod);
void setPlayerSpritePriority(DreamPlayer* p, int tPriority);

void setPlayerNoWalkFlag(DreamPlayer* p);
void setPlayerNoAutoTurnFlag(DreamPlayer* p);
void setPlayerInvisibleFlag(DreamPlayer* p);
void setPlayerNoLandFlag(DreamPlayer* p);
void setPlayerNoShadow(DreamPlayer* p);
void setAllPlayersNoShadow();
void setPlayerPushDisabledFlag(DreamPlayer* p, int tIsDisabled);
void setPlayerNoJuggleCheckFlag(DreamPlayer* p);
void setPlayerIntroFlag(DreamPlayer* p);
void setPlayerNoAirGuardFlag(DreamPlayer* p); 
void setPlayerNoCrouchGuardFlag(DreamPlayer* p);
void setPlayerNoStandGuardFlag(DreamPlayer* p);
void setPlayerNoKOSoundFlag(DreamPlayer* p);
void setPlayerNoKOSlowdownFlag(DreamPlayer* p);
void setPlayerUnguardableFlag(DreamPlayer* p);

int getPlayerNoKOSlowdownFlag(DreamPlayer* p);
int getPlayerUnguardableFlag(DreamPlayer* p);

int isPlayerInIntro(DreamPlayer* p);

int doesPlayerHaveAnimation(DreamPlayer* p, int tAnimation);
int doesPlayerHaveAnimationHimself(DreamPlayer* p, int tAnimation);

int isPlayerFalling(DreamPlayer* p);
int canPlayerRecoverFromFalling(DreamPlayer* p);
int isPlayerHitShakeOver(DreamPlayer* p);
void setPlayerHitShakeOver(DreamPlayer* p);
int isPlayerHitOver(DreamPlayer* p);
void setPlayerHitOver(DreamPlayer* p);
int getPlayerHitTime(DreamPlayer* p);
float getPlayerHitVelocityX(DreamPlayer* p, int tCoordinateP);
float getPlayerHitVelocityY(DreamPlayer* p, int tCoordinateP);

int getPlayerSlideTime(DreamPlayer* p);

float getPlayerDefenseMultiplier(DreamPlayer* p);
float getInvertedPlayerDefenseMultiplier(DreamPlayer* p);
void setPlayerDefenseMultiplier(DreamPlayer* p, float tValue);
void setPlayerSuperDefenseMultiplier(DreamPlayer* p, float tValue);
void setPlayerPositionFrozen(DreamPlayer* p);
void setPlayerPositionUnfrozen(DreamPlayer* p);

MugenSpriteFile* getPlayerSprites(DreamPlayer* p);
MugenAnimations* getPlayerAnimations(DreamPlayer* p);
MugenAnimation* getPlayerAnimation(DreamPlayer* p, int tNumber);
MugenSounds* getPlayerSounds(DreamPlayer* p);

void setCustomPlayerDisplayName(int i, const std::string& tName);
int getPlayerCoordinateP(DreamPlayer* p);
char* getPlayerDisplayName(DreamPlayer* p);
char* getPlayerName(DreamPlayer* p);
char* getPlayerAuthorName(DreamPlayer* p);

int isPlayerPaused(DreamPlayer* p);
int isPlayerHitPaused(DreamPlayer* p);
void setPlayerHitPaused(DreamPlayer* p, int tDuration);
void setPlayerUnHitPaused(DreamPlayer* p);

float getPlayerDeathVelAddY(DreamPlayer* p, int tCoordinateP);
void addPlayerDamage(DreamPlayer* p, DreamPlayer* tDamagingPlayer, int tDamage);

int getPlayerTargetAmount(DreamPlayer* p);
int getPlayerTargetAmountWithID(DreamPlayer* p, int tID);
DreamPlayer* getPlayerTargetWithID(DreamPlayer* p, int tID);
void dropPlayerTargets(DreamPlayer* p, int tExcludeID, int tIsKeepingOneAtMost);

DreamPlayer* getPlayerByIndex(int i);
int getTotalPlayerAmount();

int getPlayerHelperAmount(DreamPlayer* p);
int getPlayerHelperAmountWithID(DreamPlayer* p, int tID);
DreamPlayer* getPlayerHelperOrNullIfNonexistant(DreamPlayer* p, int tID);

int getPlayerProjectileAmount(DreamPlayer* p);
int getPlayerProjectileAmountWithID(DreamPlayer* p, int tID);
int getPlayerProjectileTimeSinceCancel(DreamPlayer* p, int tID);
int getPlayerProjectileTimeSinceContact(DreamPlayer* p, int tID);
int getPlayerProjectileTimeSinceGuarded(DreamPlayer* p, int tID);
int getPlayerProjectileTimeSinceHit(DreamPlayer* p, int tID);
int getPlayerProjectileHit(DreamPlayer* p, int tID);
int getPlayerProjectileContact(DreamPlayer* p, int tID);
int getPlayerProjectileGuarded(DreamPlayer* p, int tID);

void setPlayerHasOwnPalette(DreamPlayer* p, int tHasOwnPalette);
void setPlayerPaletteEffect(DreamPlayer* p, int tDuration, const Vector3D& tAddition, const Vector3D& tMultiplier, const Vector3D& tSineAmplitude, int tSinePeriod, int tInvertAll, float tColorFactor, int tIgnoreOwnPal);
void remapPlayerPalette(DreamPlayer* p, const Vector2DI& tSource, const Vector2DI& tDestination);

int getPlayerTimeLeftInHitPause(DreamPlayer* p);
void setPlayerPauseMoveTime(DreamPlayer* p, int tPauseMoveTime);
void setPlayerSuperMoveTime(DreamPlayer* p, int tSuperMoveTime);

float getPlayerFrontAxisDistanceToScreen(DreamPlayer* p, int tCoordinateP);
float getPlayerBackAxisDistanceToScreen(DreamPlayer* p, int tCoordinateP);

float getPlayerFrontBodyDistanceToScreen(DreamPlayer* p, int tCoordinateP);
float getPlayerBackBodyDistanceToScreen(DreamPlayer* p, int tCoordinateP);

float getPlayerFrontWidth(DreamPlayer* p, int tCoordinateP);
float getPlayerFrontWidthPlayer(DreamPlayer* p, int tCoordinateP);
float getPlayerFrontWidthStage(DreamPlayer* p, int tCoordinateP);
float getPlayerBackWidth(DreamPlayer* p, int tCoordinateP);
float getPlayerBackWidthPlayer(DreamPlayer* p, int tCoordinateP);
float getPlayerBackWidthStage(DreamPlayer* p, int tCoordinateP);
float getPlayerFrontX(DreamPlayer* p, int tCoordinateP);
float getPlayerFrontXPlayer(DreamPlayer* p, int tCoordinateP);
float getPlayerFrontXStage(DreamPlayer* p, int tCoordinateP);
float getPlayerBackX(DreamPlayer* p, int tCoordinateP);
float getPlayerBackXPlayer(DreamPlayer* p, int tCoordinateP);
float getPlayerBackXStage(DreamPlayer* p, int tCoordinateP);
int isPlayerInCorner(DreamPlayer* p);

float getPlayerScreenEdgeInFrontX(DreamPlayer* p, int tCoordinateP);
float getPlayerScreenEdgeInBackX(DreamPlayer* p, int tCoordinateP);

float getPlayerDistanceToFrontOfOtherPlayerX(DreamPlayer* p, int tCoordinateP);
float getPlayerAxisDistanceX(DreamPlayer* p, int tCoordinateP);
float getPlayerAxisDistanceY(DreamPlayer* p, int tCoordinateP);
float getPlayerDistanceToRootX(DreamPlayer* p, int tCoordinateP);
float getPlayerDistanceToRootY(DreamPlayer* p, int tCoordinateP);
float getPlayerDistanceToParentX(DreamPlayer* p, int tCoordinateP);
float getPlayerDistanceToParentY(DreamPlayer* p, int tCoordinateP);

int getPlayerGroundSizeFront(DreamPlayer* p, int tCoordinateP);
void setPlayerGroundSizeFront(DreamPlayer* p, int tGroundSizeFront, int tCoordinateP);
int getPlayerGroundSizeBack(DreamPlayer* p, int tCoordinateP);
void setPlayerGroundSizeBack(DreamPlayer* p, int tGroundSizeBack, int tCoordinateP);
int getPlayerAirSizeFront(DreamPlayer* p, int tCoordinateP);
void setPlayerAirSizeFront(DreamPlayer* p, int tAirSizeFront, int tCoordinateP);
int getPlayerAirSizeBack(DreamPlayer* p, int tCoordinateP);
void setPlayerAirSizeBack(DreamPlayer* p, int tAirSizeBack, int tCoordinateP);
int getPlayerHeight(DreamPlayer* p, int tCoordinateP);
void setPlayerHeight(DreamPlayer* p, int tHeight, int tCoordinateP);

void increasePlayerRoundsExisted();
void increasePlayerRoundsWon(DreamPlayer* p);
int hasPlayerWonByKO(DreamPlayer* p);
int hasPlayerLostByKO(DreamPlayer* p);
int hasPlayerWonPerfectly(DreamPlayer* p);
int hasPlayerWonByTime(DreamPlayer* p);
int hasPlayerWon(DreamPlayer* p);
int hasPlayerLost(DreamPlayer* p);
int hasPlayerDrawn(DreamPlayer* p);

int hasPlayerMoveHitOtherPlayer(DreamPlayer* p);
int isPlayerHit(DreamPlayer* p);
int getPlayerMoveReversed(DreamPlayer* p);
int getPlayerMoveHit(DreamPlayer* p);
void setPlayerMoveHit(DreamPlayer* p);
void setPlayerMoveHitReset(DreamPlayer* p);
int getPlayerMoveGuarded(DreamPlayer* p);
void setPlayerMoveGuarded(DreamPlayer* p);
int getLastPlayerHitGuarded(DreamPlayer* p);

int getPlayerFallAmountInCombo(DreamPlayer* p);
void increasePlayerFallAmountInCombo(DreamPlayer* p);
void resetPlayerFallAmountInCombo(DreamPlayer* p);

int getPlayerHitCount(DreamPlayer* p);
int getPlayerUniqueHitCount(DreamPlayer* p);
void increasePlayerHitCount(DreamPlayer* p);
void resetPlayerHitCount(DreamPlayer* p);
void increasePlayerComboCounter(DreamPlayer* p, int tValue);

float getPlayerAttackMultiplier(DreamPlayer* p);
void setPlayerAttackMultiplier(DreamPlayer* p, float tValue);

float getPlayerFallDefenseMultiplier(DreamPlayer* p);

void setPlayerHuman(int i, int tCustomControllerUsed = -1);
void setPlayerArtificial(int i, int tValue);
int isPlayerHuman(DreamPlayer* p);
int getPlayerAILevel(DreamPlayer* p);

void setPlayerStartLifePercentage(int tIndex, float tPercentage);
float getPlayerLifePercentage(DreamPlayer* p);
void setPlayerLife(DreamPlayer* p, DreamPlayer* tLifeGivingPlayer, int tLife);
void addPlayerLife(DreamPlayer* p, DreamPlayer* tLifeGivingPlayer, int tLife);
int getPlayerLife(DreamPlayer* p);
int getPlayerLifeMax(DreamPlayer* p);
int getPlayerPower(DreamPlayer* p);
int getPlayerPowerMax(DreamPlayer* p);
void setPlayerPower(DreamPlayer* p, int tPower);
void addPlayerPower(DreamPlayer* p, int tPower);

int isPlayerBeingAttacked(DreamPlayer* p);
int isPlayerInGuardDistance(DreamPlayer* p);
int getDefaultPlayerAttackDistance(DreamPlayer* p, int tCoordinateP);

Position2D getPlayerHeadPosition(DreamPlayer* p, int tCoordinateP);
float getPlayerHeadPositionX(DreamPlayer* p, int tCoordinateP);
float getPlayerHeadPositionY(DreamPlayer* p, int tCoordinateP);
void setPlayerHeadPosition(DreamPlayer* p, float tX, float tY, int tCoordinateP);

Position2D getPlayerMiddlePosition(DreamPlayer* p, int tCoordinateP);
float getPlayerMiddlePositionX(DreamPlayer* p, int tCoordinateP);
float getPlayerMiddlePositionY(DreamPlayer* p, int tCoordinateP);
void setPlayerMiddlePosition(DreamPlayer* p, float tX, float tY, int tCoordinateP);

int getPlayerShadowOffset(DreamPlayer* p, int tCoordinateP);
void setPlayerShadowOffset(DreamPlayer* p, int tOffset, int tCoordinateP);

int isPlayerHelper(DreamPlayer* p);

void setPlayerIsFacingRight(DreamPlayer* p, int tIsFacingRight);
int getPlayerIsFacingRight(DreamPlayer* p);
void turnPlayerAround(DreamPlayer* p);

DreamPlayer* getPlayerOtherPlayer(DreamPlayer* p);

float getPlayerScaleX(DreamPlayer* p);
void setPlayerScaleX(DreamPlayer* p, float tScaleX);
float getPlayerScaleY(DreamPlayer* p);
void setPlayerScaleY(DreamPlayer* p, float tScaleY);
float getPlayerToCameraScale(DreamPlayer* p);

int getPlayerDoesScaleProjectiles(DreamPlayer* p);
void setPlayerDoesScaleProjectiles(DreamPlayer* p, int tDoesScaleProjectiles);

DreamPlayer* clonePlayerAsHelper(DreamPlayer* p);
int destroyPlayer(DreamPlayer* tPlayer);
int getPlayerID(DreamPlayer* p);
void setPlayerID(DreamPlayer* p, int tID);
void setPlayerHelperControl(DreamPlayer* p, int tCanControl);

DreamPlayer* createNewProjectileFromPlayer(DreamPlayer* p);
void removeProjectile(DreamPlayer* p);

int getPlayerControlTime(DreamPlayer* p);
int getPlayerRecoverTime(DreamPlayer* p);

void setPlayerTempScaleActive(DreamPlayer* p, const Vector2D& tScale);
void setPlayerDrawAngleActive(DreamPlayer* p);
void addPlayerDrawAngle(DreamPlayer* p, float tAngle);
void multiplyPlayerDrawAngle(DreamPlayer* p, float tFactor);
void setPlayerDrawAngleValue(DreamPlayer* p, float tAngle);

void bindPlayerToRoot(DreamPlayer* p, int tTime, int tFacing, const Vector2D& tOffset, int tCoordinateP);
void bindPlayerToParent(DreamPlayer* p, int tTime, int tFacing, const Vector2D& tOffset, int tCoordinateP);
void bindPlayerToTarget(DreamPlayer* p, int tTime, const Vector2D& tOffset, DreamPlayerBindPositionType tBindPositionType, int tID, int tCoordinateP);
int isPlayerBound(DreamPlayer* p);

void bindPlayerTargetToPlayer(DreamPlayer* p, int tTime, const Vector2D& tOffset, int tID, int tCoordinateP);
void addPlayerTargetLife(DreamPlayer* p, DreamPlayer* tLifeGivingPlayer, int tID, int tLife, int tCanKill, int tIsAbsolute);
void addPlayerTargetPower(DreamPlayer* p, int tID, int tPower);
void addPlayerTargetVelocityX(DreamPlayer* p, int tID, float tValue, int tCoordinateP);
void addPlayerTargetVelocityY(DreamPlayer* p, int tID, float tValue, int tCoordinateP);
void setPlayerTargetVelocityX(DreamPlayer* p, int tID, float tValue, int tCoordinateP);
void setPlayerTargetVelocityY(DreamPlayer* p, int tID, float tValue, int tCoordinateP);
void setPlayerTargetControl(DreamPlayer* p, int tID, int tControl);
void setPlayerTargetHitOver(DreamPlayer* p, int tID);
void setPlayerTargetFacing(DreamPlayer* p, int tID, int tFacing);
void changePlayerTargetState(DreamPlayer* p, int tID, int tNewState);

int doesPlayerIDExist(DreamPlayer* p, int tID);
DreamPlayer* getPlayerByIDOrNullIfNonexistant(DreamPlayer* p, int tID);

int getPlayerRoundsExisted(DreamPlayer* p);

int getPlayerPaletteNumber(DreamPlayer* p);

void setPlayerScreenBoundForTick(DreamPlayer* p, int tIsBoundToScreen, int tIsCameraFollowingX, int tIsCameraFollowingY);
void setPlayerScreenBoundForever(DreamPlayer* p, int tIsBoundToScreen);

void resetPlayerHitBy(DreamPlayer* p, int tSlot);
void resetPlayerNotHitBy(DreamPlayer* p, int tSlot);
void setPlayerNotHitByFlag1(DreamPlayer* p, int tSlot, const char* tFlag);
void addPlayerNotHitByFlag2(DreamPlayer* p, int tSlot, const char* tFlag);
void setPlayerNotHitByTime(DreamPlayer* p, int tSlot, int tTime);

int getDefaultPlayerSparkNumberIsInPlayerFile(DreamPlayer* p);
int getDefaultPlayerSparkNumber(DreamPlayer* p);
int getDefaultPlayerGuardSparkNumberIsInPlayerFile(DreamPlayer* p);
int getDefaultPlayerGuardSparkNumber(DreamPlayer* p);

int isPlayerProjectile(DreamPlayer* p);
int isPlayerHomeTeam(DreamPlayer* p);

void setPlayerDrawOffsetX(DreamPlayer* p, float tValue, int tCoordinateP);
void setPlayerDrawOffsetY(DreamPlayer* p, float tValue, int tCoordinateP);

void setPlayerOneFrameTransparency(DreamPlayer* p, BlendType tType, int tAlphaSource, int tAlphaDest);
void setPlayerWidthOneFrame(DreamPlayer* p, const Vector2DI& tEdgeWidth, const Vector2DI& tPlayerWidth, int tCoordinateP);

void addPlayerDust(DreamPlayer* p, int tDustIndex, const Position2D& tPos, int tSpacing, int tCoordinateP);
VictoryType getPlayerVictoryType(DreamPlayer* p);
int isPlayerAtFullLife(DreamPlayer* p);

int getPlayerVictoryQuoteIndex(DreamPlayer* p);
void setPlayerVictoryQuoteIndex(DreamPlayer* p, int tIndex);

void setPlayersToTrainingMode();
void setPlayersToRealFightMode();

int isPlayer(DreamPlayer* p);
int isValidPlayerOrProjectile(DreamPlayer* p);
int isGeneralPlayer(DreamPlayer* p);
int isPlayerTargetValid(DreamPlayer* p);

int isPlayerCollisionDebugActive();
void setPlayerCollisionDebug(int tIsActive);

void turnPlayerTowardsOtherPlayer(DreamPlayer* p);

int isPlayerInputAllowed(DreamPlayer* p);

int getPlayerTimeDilationUpdates(DreamPlayer* p);
float getPlayerSpeed(DreamPlayer* p);
void setPlayersSpeed(float tSpeed);
void setPlayerTargetsSuperDefenseMultiplier(DreamPlayer* tPlayer, float tMultiplier);

Vector3DI getIsCameraFollowingPlayer(DreamPlayer* p);

int parsePlayerSoundEffectChannel(int tChannel, DreamPlayer* tPlayer);
float getPlayerVolumeModifier(DreamPlayer* tPlayer);
float getPlayerMidiVolumeForPrism(DreamPlayer* tPlayer);