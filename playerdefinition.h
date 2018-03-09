#pragma once

#include <prism/datastructures.h>

#include <prism/mugenspritefilereader.h>
#include <prism/mugenanimationreader.h>
#include <prism/mugensoundfilereader.h>

#include "mugenstatereader.h"
#include "mugencommandreader.h"

typedef enum {
	PLAYER_BIND_POSITION_TYPE_AXIS,
	PLAYER_BIND_POSITION_TYPE_HEAD,
	PLAYER_BIND_POSITION_TYPE_MID,
} DreamPlayerBindPositionType;

#define MAXIMUM_HITSLOT_FLAG_2_AMOUNT 10

typedef struct {
	int mIsActive;

	char mFlag1[10];
	char mFlag2[MAXIMUM_HITSLOT_FLAG_2_AMOUNT][10];
	int mFlag2Amount;

	int mNow;
	int mTime;
	int mIsHitBy;
} DreamHitDefAttributeSlot;

typedef struct {
	char mName[100];
	char mDisplayName[100];
	char mVersion[100];
	char mMugenVersion[100];
	char mAuthor[100];

	char mPaletteDefaults[100];
	Vector3DI mLocalCoordinates;
} DreamPlayerHeader;

typedef struct Player_t{
	struct Player_t* mRoot;
	struct Player_t* mOtherPlayer;
	int mPreferredPalette;
	int mRootID;
	int mControllerID;
	int mID;
	int mIsHelper;
	int mIsProjectile;
	int mProjectileID;
	int mProjectileDataID;

	List mHelpers; // contains DreamPlayer
	struct Player_t* mParent;
	int mHelperIDInParent;

	IntMap mProjectiles; // contains DreamPlayer

	int mAILevel;

	char mDefinitionPath[1024];
	DreamPlayerHeader mHeader;
	DreamMugenCommands mCommands;
	DreamMugenConstants mConstants;
	MugenAnimations mAnimations;
	MugenSpriteFile mSprites;
	MugenSounds mSounds;

	int mVars[100];
	int mSystemVars[100];
	double mFloatVars[100];
	double mSystemFloatVars[100];

	int mCommandID;
	int mStateMachineID;
	int mAnimationID;
	int mPhysicsID;
	int mHitDataID;

	DreamMugenStateType mStateType;
	DreamMugenStateMoveType mMoveType;
	DreamMugenStatePhysics mStatePhysics;

	int mIsInControl;
	int mMoveContactCounter;
	int mMoveHit;
	int mMoveGuarded;
	int mIsAlive;
	FaceDirection mFaceDirection;

	int mNoWalkFlag;
	int mNoAutoTurnFlag;
	int mNoLandFlag;
	int mPushDisabledFlag;
	int mNoJuggleCheckFlag;
	int mIntroFlag;

	int mIsHitShakeOver;
	int mIsHitOver;
	int mIsFalling;
	int mCanRecoverFromFall;

	double mDefenseMultiplier;

	int mIsFrozen;
	Position mFreezePosition;

	int mIsLyingDown;
	Duration mLyingDownTime;

	int mIsHitPaused;
	Duration mHitPauseNow;
	int mHitPauseDuration;

	int mIsSuperPaused;

	int mLife;
	int mPower;

	int mHitCount;
	int mFallAmountInCombo;

	double mAttackMultiplier;

	int mHasMoveBeenReversed;

	int mIsBound;
	int mBoundNow;
	int mBoundDuration;
	int mBoundFaceSet;
	Position mBoundOffset;
	DreamPlayerBindPositionType mBoundPositionType;
	struct Player_t* mBoundTarget;
	int mBoundID;

	List mBoundHelpers;

	int mRoundsExisted;
	int mComboCounter;

	int mRoundsWon;

	int mIsBoundToScreen;

	DreamHitDefAttributeSlot mNotHitBy[2];
} DreamPlayer;

void loadPlayers();
void resetPlayers();
void resetPlayersEntirely();
void updatePlayers();

void playerHitCB(void* tData, void* tHitData);

void setPlayerDefinitionPath(int i, char* tDefinitionPath);
void getPlayerDefinitionPath(char* tDst, int i);
DreamPlayer* getRootPlayer(int i);
DreamPlayer* getPlayerRoot(DreamPlayer* p);
DreamPlayer* getPlayerParent(DreamPlayer* p);

int getPlayerState(DreamPlayer* p); 
int getPlayerPreviousState(DreamPlayer* p);


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
void setPlayerVariable(DreamPlayer* p, int tIndex, int tValue);
void addPlayerVariable(DreamPlayer* p, int tIndex, int tValue);
int getPlayerSystemVariable(DreamPlayer* p, int tIndex);
void setPlayerSystemVariable(DreamPlayer* p, int tIndex, int tValue);
void addPlayerSystemVariable(DreamPlayer* p, int tIndex, int tValue);
double getPlayerFloatVariable(DreamPlayer* p, int tIndex);
void setPlayerFloatVariable(DreamPlayer* p, int tIndex, double tValue);
void addPlayerFloatVariable(DreamPlayer* p, int tIndex, double tValue);
double getPlayerSystemFloatVariable(DreamPlayer* p, int tIndex);
void setPlayerSystemFloatVariable(DreamPlayer* p, int tIndex, double tValue);
void addPlayerSystemFloatVariable(DreamPlayer* p, int tIndex, double tValue);

int getPlayerTimeInState(DreamPlayer* p);
int getPlayerAnimationNumber(DreamPlayer* p);
int getRemainingPlayerAnimationTime(DreamPlayer* p);

Vector3D getPlayerPosition(DreamPlayer* p, int tCoordinateP);
double getPlayerPositionBasedOnScreenCenterX(DreamPlayer* p, int tCoordinateP);
double getPlayerScreenPositionX(DreamPlayer* p, int tCoordinateP);
double getPlayerPositionX(DreamPlayer* p, int tCoordinateP);
double getPlayerPositionBasedOnStageFloorY(DreamPlayer* p, int tCoordinateP);
double getPlayerScreenPositionY(DreamPlayer* p, int tCoordinateP);
double getPlayerPositionY(DreamPlayer* p, int tCoordinateP);
double getPlayerVelocityX(DreamPlayer* p, int tCoordinateP);
double getPlayerVelocityY(DreamPlayer* p, int tCoordinateP);

double getPlayerStandFrictionThreshold(DreamPlayer* p);
double getPlayerCrouchFrictionThreshold(DreamPlayer* p);
double getPlayerAirGetHitGroundLevelY(DreamPlayer* p);
double getPlayerAirGetHitGroundRecoveryGroundLevelY(DreamPlayer* p);
double getPlayerAirGetHitGroundRecoveryGroundYTheshold(DreamPlayer* p);
double getPlayerAirGetHitAirRecoveryVelocityYThreshold(DreamPlayer* p);
double getPlayerAirGetHitTripGroundLevelY(DreamPlayer* p);
double getPlayerDownBounceOffsetX(DreamPlayer* p);
double getPlayerDownBounceOffsetY(DreamPlayer* p);
double getPlayerDownVerticalBounceAcceleration(DreamPlayer* p);
double getPlayerDownBounceGroundLevel(DreamPlayer* p);
double getPlayerLyingDownFrictionThreshold(DreamPlayer* p);
double getPlayerVerticalAcceleration(DreamPlayer* p);

double getPlayerForwardWalkVelocityX(DreamPlayer* p);
double getPlayerBackwardWalkVelocityX(DreamPlayer* p);
double getPlayerForwardRunVelocityX(DreamPlayer* p);
double getPlayerForwardRunVelocityY(DreamPlayer* p);
double getPlayerBackwardRunVelocityX(DreamPlayer* p);
double getPlayerBackwardRunVelocityY(DreamPlayer* p);
double getPlayerForwardRunJumpVelocityX(DreamPlayer* p);
double getPlayerNeutralJumpVelocityX(DreamPlayer* p);
double getPlayerForwardJumpVelocityX(DreamPlayer* p);
double getPlayerBackwardJumpVelocityX(DreamPlayer* p);
double getPlayerJumpVelocityY(DreamPlayer* p);
double getPlayerNeutralAirJumpVelocityX(DreamPlayer* p);
double getPlayerForwardAirJumpVelocityX(DreamPlayer* p);
double getPlayerBackwardAirJumpVelocityX(DreamPlayer* p);
double getPlayerAirJumpVelocityY(DreamPlayer* p);


int isPlayerAlive(DreamPlayer* p);

void setPlayerVelocityX(DreamPlayer* p, double x, int tCoordinateP);
void setPlayerVelocityY(DreamPlayer* p, double y, int tCoordinateP);
void multiplyPlayerVelocityX(DreamPlayer* p, double x, int tCoordinateP);
void multiplyPlayerVelocityY(DreamPlayer* p, double y, int tCoordinateP);
void addPlayerVelocityX(DreamPlayer* p, double x, int tCoordinateP);
void addPlayerVelocityY(DreamPlayer* p, double y, int tCoordinateP);

void setPlayerPosition(DreamPlayer* p, Position tPosition, int tCoordinateP);
void setPlayerPositionX(DreamPlayer* p, double x, int tCoordinateP);
void setPlayerPositionY(DreamPlayer* p, double y, int tCoordinateP);
void addPlayerPositionX(DreamPlayer* p, double x, int tCoordinateP);
void addPlayerPositionY(DreamPlayer* p, double y, int tCoordinateP);

int isPlayerCommandActive(DreamPlayer* p, char* tCommandName);

int hasPlayerState(DreamPlayer* p, int mNewState);
int hasPlayerStateSelf(DreamPlayer* p, int mNewState);
void changePlayerState(DreamPlayer* p, int mNewState);
void changePlayerStateToOtherPlayerStateMachine(DreamPlayer* p, DreamPlayer* tOtherPlayer, int mNewState);
void changePlayerStateBeforeImmediatelyEvaluatingIt(DreamPlayer* p, int mNewState);
void changePlayerStateToSelfBeforeImmediatelyEvaluatingIt(DreamPlayer* p, int tNewState);

void changePlayerAnimation(DreamPlayer* p, int tNewAnimation);
void changePlayerAnimationWithStartStep(DreamPlayer* p, int tNewAnimation, int tStartStep);
void changePlayerAnimationToPlayer2AnimationWithStartStep(DreamPlayer* p, int tNewAnimation, int tStartStep);

int isPlayerStartingAnimationElementWithID(DreamPlayer* p, int tStepID);
int getPlayerTimeFromAnimationElement(DreamPlayer* p, int tStep);
int getPlayerAnimationElementFromTimeOffset(DreamPlayer* p, int tTime);

void setPlayerSpritePriority(DreamPlayer* p, int tPriority);

void setPlayerNoWalkFlag(DreamPlayer* p);
void setPlayerNoAutoTurnFlag(DreamPlayer* p);
void setPlayerInvisibleFlag(DreamPlayer* p);
void setPlayerNoLandFlag(DreamPlayer* p);
void setPlayerNoShadow(DreamPlayer* p);
void setPlayerPushDisabledFlag(DreamPlayer* p, int tIsDisabled);
void setPlayerNoJuggleCheckFlag(DreamPlayer* p);
void setPlayerIntroFlag(DreamPlayer* p);
void setPlayerNoAirGuardFlag(DreamPlayer* p);

int isPlayerInIntro(DreamPlayer* p);

int doesPlayerHaveAnimation(DreamPlayer* p, int tAnimation);
int doesPlayerHaveAnimationHimself(DreamPlayer* p, int tAnimation);

int isPlayerFalling(DreamPlayer* p);
int canPlayerRecoverFromFalling(DreamPlayer* p);
int isPlayerHitShakeOver(DreamPlayer* p);
int isPlayerHitOver(DreamPlayer* p);
double getPlayerHitVelocityX(DreamPlayer* p, int tCoordinateP);
double getPlayerHitVelocityY(DreamPlayer* p, int tCoordinateP);

int getPlayerSlideTime(DreamPlayer* p);

void setPlayerDefenseMultiplier(DreamPlayer* p, double tValue);
void setPlayerPositionFrozen(DreamPlayer* p);

MugenSpriteFile* getPlayerSprites(DreamPlayer* p);
MugenAnimations* getPlayerAnimations(DreamPlayer* p);
MugenAnimation* getPlayerAnimation(DreamPlayer* p, int tNumber);
MugenSounds* getPlayerSounds(DreamPlayer* p);

int getPlayerCoordinateP(DreamPlayer* p);
char* getPlayerDisplayName(DreamPlayer* p);
char* getPlayerName(DreamPlayer* p);
char* getPlayerAuthorName(DreamPlayer* p);

int isPlayerPaused(DreamPlayer* p);
void setPlayerHitPaused(DreamPlayer* p, int tDuration);
void setPlayerUnHitPaused(DreamPlayer* p);
void setPlayerSuperPaused(DreamPlayer* p);
void setPlayerUnSuperPaused(DreamPlayer* p);

void addPlayerDamage(DreamPlayer* p, int tDamage);

int getPlayerTargetAmount(DreamPlayer* p);
int getPlayerTargetAmountWithID(DreamPlayer* p, int tID);

int getPlayerHelperAmount(DreamPlayer* p);
int getPlayerHelperAmountWithID(DreamPlayer* p, int tID);
DreamPlayer* getPlayerHelperOrNullIfNonexistant(DreamPlayer* p, int tID);

int getPlayerProjectileAmount(DreamPlayer* p);
int getPlayerProjectileAmountWithID(DreamPlayer* p, int tID);
int getPlayerProjectileTimeSinceCancel(DreamPlayer* p, int tID);
int getPlayerProjectileTimeSinceContact(DreamPlayer* p, int tID);
int getPlayerProjectileTimeSinceGuarded(DreamPlayer* p, int tID);
int getPlayerProjectileTimeSinceHit(DreamPlayer* p, int tID);

int getPlayerTimeLeftInHitPause(DreamPlayer* p);

double getPlayerFrontAxisDistanceToScreen(DreamPlayer* p);
double getPlayerBackAxisDistanceToScreen(DreamPlayer* p);

double getPlayerFrontBodyDistanceToScreen(DreamPlayer* p);
double getPlayerBackBodyDistanceToScreen(DreamPlayer* p);

double getPlayerFrontX(DreamPlayer* p);
double getPlayerBackX(DreamPlayer* p);

double getPlayerScreenEdgeInFrontX(DreamPlayer* p);
double getPlayerScreenEdgeInBackX(DreamPlayer* p);

double getPlayerDistanceToFrontOfOtherPlayerX(DreamPlayer* p);
double getPlayerAxisDistanceX(DreamPlayer* p);
double getPlayerAxisDistanceY(DreamPlayer* p);
double getPlayerDistanceToRootX(DreamPlayer* p);
double getPlayerDistanceToRootY(DreamPlayer* p);
double getPlayerDistanceToParentX(DreamPlayer* p);
double getPlayerDistanceToParentY(DreamPlayer* p);

int getPlayerGroundSizeFront(DreamPlayer* p);
void setPlayerGroundSizeFront(DreamPlayer* p, int tGroundSizeFront);
int getPlayerGroundSizeBack(DreamPlayer* p);
void setPlayerGroundSizeBack(DreamPlayer* p, int tGroundSizeBack);
int getPlayerAirSizeFront(DreamPlayer* p);
void setPlayerAirSizeFront(DreamPlayer* p, int tAirSizeFront);
int getPlayerAirSizeBack(DreamPlayer* p);
void setPlayerAirSizeBack(DreamPlayer* p, int tAirSizeBack);
int getPlayerHeight(DreamPlayer* p);
void setPlayerHeight(DreamPlayer* p, int tHeight);

void increasePlayerRoundsExisted();
void increasePlayerRoundsWon(DreamPlayer* p);
int hasPlayerWonByKO(DreamPlayer* p);
int hasPlayerLostByKO(DreamPlayer* p);
int hasPlayerWonPerfectly(DreamPlayer* p);
int hasPlayerWon(DreamPlayer* p);
int hasPlayerLost(DreamPlayer* p);
int hasPlayerDrawn(DreamPlayer* p);

int hasPlayerMoveHitOtherPlayer(DreamPlayer* p);
int isPlayerHit(DreamPlayer* p); // TODO: this or getPlayerMoveHit
int hasPlayerMoveBeenReversedByOtherPlayer(DreamPlayer* p);
int getPlayerMoveHit(DreamPlayer* p); // TODO: this or isPlayerHit
void setPlayerMoveHit(DreamPlayer* p);
void setPlayerMoveHitReset(DreamPlayer* p);
int getPlayerMoveGuarded(DreamPlayer* p);
void setPlayerMoveGuarded(DreamPlayer* p);


int getPlayerFallAmountInCombo(DreamPlayer* p);
void increasePlayerFallAmountInCombo(DreamPlayer* p);
void resetPlayerFallAmountInCombo(DreamPlayer* p);

int getPlayerHitCount(DreamPlayer* p);
int getPlayerUniqueHitCount(DreamPlayer* p);
void increasePlayerHitCount(DreamPlayer* p);
void resetPlayerHitCount(DreamPlayer* p);

void setPlayerAttackMultiplier(DreamPlayer* p, double tValue);

double getPlayerFallDefenseMultiplier(DreamPlayer* p);

void setPlayerHuman(int i);
void setPlayerArtificial(int i);
int getPlayerAILevel(DreamPlayer* p);

int getPlayerLife(DreamPlayer* p);
int getPlayerLifeMax(DreamPlayer* p);
int getPlayerPower(DreamPlayer* p);
int getPlayerPowerMax(DreamPlayer* p);
void addPlayerPower(DreamPlayer* p, int tPower);

int isPlayerBeingAttacked(DreamPlayer* p);
int isPlayerInGuardDistance(DreamPlayer* p);
int getDefaultPlayerAttackDistance(DreamPlayer* p);

Position getPlayerHeadPosition(DreamPlayer* p);
double getPlayerHeadPositionX(DreamPlayer* p);
double getPlayerHeadPositionY(DreamPlayer* p);
void setPlayerHeadPosition(DreamPlayer* p, double tX, double tY);

Position getPlayerMiddlePosition(DreamPlayer* p);
double getPlayerMiddlePositionX(DreamPlayer* p);
double getPlayerMiddlePositionY(DreamPlayer* p);
void setPlayerMiddlePosition(DreamPlayer* p, double tX, double tY);

int getPlayerShadowOffset(DreamPlayer* p);
void setPlayerShadowOffset(DreamPlayer* p, int tOffset);

int isPlayerHelper(DreamPlayer* p);

void setPlayerIsFacingRight(DreamPlayer* p, int tIsFacingRight);
int getPlayerIsFacingRight(DreamPlayer* p);
void turnPlayerAround(DreamPlayer* p);

DreamPlayer* getPlayerOtherPlayer(DreamPlayer* p);

double getPlayerScaleX(DreamPlayer* p);
void setPlayerScaleX(DreamPlayer* p, double tScaleX);
double getPlayerScaleY(DreamPlayer* p);
void setPlayerScaleY(DreamPlayer* p, double tScaleY);

DreamPlayer* clonePlayerAsHelper(DreamPlayer* p);
void destroyPlayer(DreamPlayer* tPlayer);
void addHelperToPlayer(DreamPlayer* p, DreamPlayer* tHelper);
int getPlayerID(DreamPlayer* p);
void setPlayerID(DreamPlayer* p, int tID);
void setPlayerHelperControl(DreamPlayer* p, int tCanControl);

DreamPlayer* createNewProjectileFromPlayer(DreamPlayer* p);
void removeProjectile(DreamPlayer* p);

int getPlayerControlTime(DreamPlayer* p);

void setPlayerDrawScale(DreamPlayer* p, Vector3D tScale);
void setPlayerDrawAngle(DreamPlayer* p, double tAngle);
void addPlayerDrawAngle(DreamPlayer* p, double tAngle);
void setPlayerFixedDrawAngle(DreamPlayer* p, double tAngle);

void bindPlayerToRoot(DreamPlayer* p, int tTime, int tFacing, Vector3D tOffset);
void bindPlayerToParent(DreamPlayer* p, int tTime, int tFacing, Vector3D tOffset);
void bindPlayerTargets(DreamPlayer* p, int tTime, Vector3D tOffset, int tID);
int isPlayerBound(DreamPlayer* p);

void addPlayerTargetLife(DreamPlayer* p, int tID, int tLife, int tCanKill, int tIsAbsolute);
void setPlayerTargetControl(DreamPlayer* p, int tID, int tControl);
void setPlayerTargetFacing(DreamPlayer* p, int tID, int tFacing);
void changePlayerTargetState(DreamPlayer* p, int tID, int tNewState);

int doesPlayerIDExist(DreamPlayer* p, int tID);
DreamPlayer* getPlayerByIDOrNullIfNonexistant(DreamPlayer* p, int tID);

int getPlayerRoundsExisted(DreamPlayer* p);

int getPlayerPaletteNumber(DreamPlayer* p);

void setPlayerScreenBound(DreamPlayer* p, int tIsBoundToScreen, int tIsCameraFollowingX, int tIsCameraFollowingY);

void resetPlayerHitBy(DreamPlayer* p, int tSlot);
void resetPlayerNotHitBy(DreamPlayer* p, int tSlot);
void setPlayerNotHitByFlag1(DreamPlayer* p, int tSlot, char* tFlag);
void addPlayerNotHitByFlag2(DreamPlayer* p, int tSlot, char* tFlag);
void setPlayerNotHitByTime(DreamPlayer* p, int tSlot, int tTime);

int getDefaultPlayerSparkNumberIsInPlayerFile(DreamPlayer* p);
int getDefaultPlayerSparkNumber(DreamPlayer* p);
int getDefaultPlayerGuardSparkNumberIsInPlayerFile(DreamPlayer* p);
int getDefaultPlayerGuardSparkNumber(DreamPlayer* p);

int isPlayerProjectile(DreamPlayer* p);
int isPlayerHomeTeam(DreamPlayer* p);