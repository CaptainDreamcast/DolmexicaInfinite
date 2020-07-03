#pragma once

#include <prism/actorhandler.h>

#include "mugenstatereader.h"
#include "mugensound.h"

struct DreamPlayer;

typedef enum {
	MUGEN_ATTACK_CLASS_NORMAL,
	MUGEN_ATTACK_CLASS_SPECIAL,
	MUGEN_ATTACK_CLASS_HYPER,
} MugenAttackClass;

enum MugenAttackClassFlags : uint32_t {
	MUGEN_ATTACK_CLASS_NORMAL_FLAG = (1 << 0),
	MUGEN_ATTACK_CLASS_SPECIAL_FLAG = (1 << 1),
	MUGEN_ATTACK_CLASS_HYPER_FLAG = (1 << 2),
	MUGEN_ATTACK_CLASS_ALL_FLAG = (MUGEN_ATTACK_CLASS_NORMAL_FLAG | MUGEN_ATTACK_CLASS_SPECIAL_FLAG | MUGEN_ATTACK_CLASS_HYPER_FLAG),
};

typedef enum {
	MUGEN_ATTACK_TYPE_ATTACK,
	MUGEN_ATTACK_TYPE_THROW,
	MUGEN_ATTACK_TYPE_PROJECTILE,
} MugenAttackType;

typedef enum {
	MUGEN_ATTACK_HEIGHT_LOW,
	MUGEN_ATTACK_HEIGHT_HIGH,
	MUGEN_ATTACK_HEIGHT_TRIP,
	MUGEN_ATTACK_HEIGHT_HEAVY,
	MUGEN_ATTACK_HEIGHT_NONE,
} MugenAttackHeight;

typedef enum {
	MUGEN_HIT_ANIMATION_TYPE_LIGHT,
	MUGEN_HIT_ANIMATION_TYPE_MEDIUM,
	MUGEN_HIT_ANIMATION_TYPE_HARD,
	MUGEN_HIT_ANIMATION_TYPE_BACK,
	MUGEN_HIT_ANIMATION_TYPE_UP,
	MUGEN_HIT_ANIMATION_TYPE_DIAGONAL_UP,
	MUGEN_HIT_ANIMATION_TYPE_HEAVY,

} MugenHitAnimationType;

typedef enum {
	MUGEN_AFFECT_TEAM_BOTH,
	MUGEN_AFFECT_TEAM_ENEMY,
	MUGEN_AFFECT_TEAM_FRIENDLY,

} MugenAffectTeam;

typedef enum {
	MUGEN_HIT_PRIORITY_DODGE,
	MUGEN_HIT_PRIORITY_HIT,
	MUGEN_HIT_PRIORITY_MISS,
} MugenHitPriorityType;

typedef struct {
	int mIsActive;

	std::string mFlag1;
	std::vector<std::string> mFlag2;

	int mNow;
	int mTime;
	int mIsHitBy;
} DreamHitDefAttributeSlot;

typedef struct {
	int mIsActive;

	DreamHitDefAttributeSlot mReversalAttribute;

	int mPlayer1PauseTime;
	int mPlayer2PauseTime;
	
	int mIsSparkInPlayerFile;
	int mSparkNumber;
	Vector2DI mSparkXY;

	int mIsHitSoundInPlayerFile;
	DreamMugenSound mHitSound;

	int mHasPlayer1StateNumber;
	int mPlayer1StateNumber;
	int mHasPlayer2StateNumber;
	int mPlayer2StateNumber;
} ReversalDef;

typedef struct {
	int mIsActive;
	DreamPlayer* mPlayer;
	int mCoordinateP;

	DreamMugenStateType mType;
	MugenAttackClass mAttackClass;
	MugenAttackType mAttackType;

	char mHitFlag[10];
	char mGuardFlag[10];

	MugenAffectTeam mAffectTeam;
	MugenHitAnimationType mAnimationType;
	MugenHitAnimationType mAirAnimationType;
	MugenHitAnimationType mFallAnimationType;

	int mPriority;
	MugenHitPriorityType mPriorityType;

	int mDamage;
	int mGuardDamage;

	int mPlayer1PauseTime;
	int mPlayer2ShakeTime;
	int mGuardPlayer1PauseTime;
	int mGuardPlayer2ShakeTime;

	int mIsSparkInPlayerFile;
	int mSparkNumber;

	int mIsGuardSparkInPlayerFile;
	int mGuardSparkNumber;

	Position2D mSparkOffset;

	int mIsHitSoundInPlayerFile;
	DreamMugenSound mHitSound;
	int mIsGuardSoundInPlayerFile;
	DreamMugenSound mGuardSound;

	MugenAttackHeight mGroundType;
	MugenAttackHeight mAirType;

	int mGroundSlideTime;
	int mGuardSlideTime;
	int mGroundHitTime;
	int mGuardHitTime;
	int mAirHitTime;

	int mGuardControlTime;
	int mGuardDistance;

	double mVerticalAcceleration;
	Vector2D mGroundVelocity;
	double mGuardVelocity;
	Vector2D mAirVelocity;
	Vector2D mAirGuardVelocity;

	double mGroundCornerPushVelocityOffset;
	double mAirCornerPushVelocityOffset;
	double mDownCornerPushVelocityOffset;
	double mGuardCornerPushVelocityOffset;
	double mAirGuardCornerPushVelocityOffset;

	int mAirGuardControlTime;
	int mAirJugglePoints;

	int mHasMinimumDistance;
	Vector2DI mMinimumDistance;

	int mHasMaximumDistance;
	Vector2DI mMaximumDistance;

	int mHasSnap;
	Vector2DI mSnap;

	int mPlayer1DrawingPriority;
	int mPlayer2DrawingPriority;

	int mIsPlayer1TurningAround;
	int mPlayer1ChangeFaceDirectionRelativeToPlayer2;
	int mPlayer2ChangeFaceDirectionRelativeToPlayer1;

	int mPlayer1StateNumber;
	int mPlayer2StateNumber;

	int mCanPlayer2GetPlayer1State;
	int mIsForcingPlayer2ToStandingPosition;

	int mFall;
	Vector2D mFallVelocity;

	int mCanRecoverFall;
	int mFallRecoveryTime;
	int mFallDamage;

	int mAirFall;
	int mForcePlayer2OutOfFallState;

	Vector2D mDownVelocity;
	int mDownHitTime;
	int mDownDoesBounce;

	int mHitID;
	int mChainID;
	Vector2DI mNoChainIDs;

	int mDoesOnlyHitOneEnemy;

	int mCanKill;
	int mCanGuardKill;
	int mCanFallKill;

	int mNumberOfHitsForComboCounter;

	int mGetPlayer1Power;
	int mGetPlayer1GuardPower;

	int mGivePlayer2Power;
	int mGivePlayer2GuardPower;

	int mPaletteEffectTime;
	Vector3D mPaletteEffectMultiplication;
	Vector3D mPaletteEffectAddition;

	int mEnvironmentShakeTime;
	double mEnvironmentShakeFrequency;
	int mEnvironmentShakeAmplitude;
	double mEnvironmentShakePhase;

	int mFallEnvironmentShakeTime;
	double mFallEnvironmentShakeFrequency;
	int mFallEnvironmentShakeAmplitude;
	double mFallEnvironmentShakePhase;

	Vector2D mVelocity;

	int mIsFacingRight;

	ReversalDef mReversalDef;
} PlayerHitData;

typedef struct {
	int mIsActive;

	DreamMugenStateTypeFlags mStateTypeFlags;
	std::vector<std::pair<MugenAttackClassFlags, MugenAttackType>> mAttackClassTypePairs;
	int mStateNo;
	int mSlot;

	int mNow;
	int mDuration;

	int mDoesForceAir;
} HitOverride;

typedef struct {
	HitOverride mHitOverrides[8];
} PlayerHitOverrides;

void updatePlayerHitData(DreamPlayer* tPlayer);

void initPlayerHitData(DreamPlayer* tPlayer);
void setPlayerHitDataCoordinateP(DreamPlayer* tPlayer);
void clearPlayerHitData(DreamPlayer* tPlayer);

void copyHitDataToActive(DreamPlayer* tPlayer, void* tHitData);

int isReceivedHitDataActive(void* tHitData);
int isHitDataActive(DreamPlayer* tPlayer);
int isActiveHitDataActive(DreamPlayer* tPlayer);
void setHitDataActive(DreamPlayer* tPlayer);
void setReceivedHitDataInactive(void* tHitData);
void setHitDataInactive(DreamPlayer* tPlayer);
void setActiveHitDataInactive(DreamPlayer* tPlayer);
int getActiveHitDataCoordinateP(DreamPlayer* tPlayer);

void* getPlayerHitDataReference(DreamPlayer* tPlayer);
DreamPlayer* getReceivedHitDataPlayer(void* tHitData);

DreamMugenStateType getHitDataType(DreamPlayer* tPlayer);
void setHitDataType(DreamPlayer* tPlayer, DreamMugenStateType tType);
MugenAttackClass getHitDataAttackClass(DreamPlayer* tPlayer);
MugenAttackClass getActiveHitDataAttackClass(DreamPlayer* tPlayer);
void setHitDataAttackClass(DreamPlayer* tPlayer, MugenAttackClass tClass);

MugenAttackType getHitDataAttackType(DreamPlayer* tPlayer);
MugenAttackType getActiveHitDataAttackType(DreamPlayer* tPlayer);
void setHitDataAttackType(DreamPlayer* tPlayer, MugenAttackType tType);

void setHitDataHitFlag(DreamPlayer* tPlayer, const char* tFlag);
char* getActiveHitDataGuardFlag(DreamPlayer* tPlayer);
void setHitDataGuardFlag(DreamPlayer* tPlayer, const char* tFlag);
MugenAffectTeam getHitDataAffectTeam(DreamPlayer* tPlayer);
void setHitDataAffectTeam(DreamPlayer* tPlayer, MugenAffectTeam tAffectTeam);

MugenHitAnimationType getActiveHitDataAnimationType(DreamPlayer* tPlayer);
MugenHitAnimationType getHitDataAnimationType(DreamPlayer* tPlayer);
void setHitDataAnimationType(DreamPlayer* tPlayer, MugenHitAnimationType tAnimationType);
MugenHitAnimationType getHitDataAirAnimationType(DreamPlayer* tPlayer);
void setHitDataAirAnimationType(DreamPlayer* tPlayer, MugenHitAnimationType tAnimationType);
void setHitDataFallAnimationType(DreamPlayer* tPlayer, MugenHitAnimationType tAnimationType);

void setHitDataPriority(DreamPlayer* tPlayer, int tPriority, MugenHitPriorityType tPriorityType);
int getActiveHitDataDamage(DreamPlayer* tPlayer);
int getActiveHitDataGuardDamage(DreamPlayer* tPlayer);
int getHitDataDamage(DreamPlayer* tPlayer);
void setHitDataDamage(DreamPlayer* tPlayer, int tDamage, int tGuardDamage);

int getActiveHitDataPlayer1PauseTime(DreamPlayer* tPlayer);
int getActiveHitDataPlayer1GuardPauseTime(DreamPlayer* tPlayer);
int getHitDataPlayer1PauseTime(DreamPlayer* tPlayer);
int getActiveHitDataPlayer2PauseTime(DreamPlayer* tPlayer);
int getActiveHitDataPlayer2GuardPauseTime(DreamPlayer* tPlayer);
int getHitDataPlayer2PauseTime(DreamPlayer* tPlayer);
void setHitDataPauseTime(DreamPlayer* tPlayer, int tPlayer1PauseTime, int tPlayer2PauseTime);

void setHitDataGuardPauseTime(DreamPlayer* tPlayer, int tPlayer1PauseTime, int tPlayer2PauseTime);

int isActiveHitDataSparkInPlayerFile(DreamPlayer* tPlayer);
int isActiveHitDataGuardSparkInPlayerFile(DreamPlayer* tPlayer);
int isHitDataSparkInPlayerFile(DreamPlayer* tPlayer);
int getActiveHitDataSparkNumber(DreamPlayer* tPlayer);
int getActiveHitDataGuardSparkNumber(DreamPlayer* tPlayer);
int getHitDataGuardSparkNumber(DreamPlayer * tPlayer);
int getHitDataSparkNumber(DreamPlayer* tPlayer);
void setHitDataSparkNumber(DreamPlayer* tPlayer, int tIsInPlayerFile, int tNumber);
void setHitDataGuardSparkNumber(DreamPlayer* tPlayer, int tIsInPlayerFile, int tNumber);
Position2D getActiveHitDataSparkXY(DreamPlayer* tPlayer);
Position2D getHitDataSparkXY(DreamPlayer* tPlayer);
void setHitDataSparkXY(DreamPlayer* tPlayer, int tX, int tY);

void getActiveHitDataHitSound(DreamPlayer* tPlayer, int* oIsInPlayerFile, Vector2DI* oSound);
void setHitDataHitSound(DreamPlayer* tPlayer, int tIsInPlayerFile, int tGroup, int tItem);
void getActiveHitDataGuardSound(DreamPlayer* tPlayer, int* oIsInPlayerFile, Vector2DI* oSound);
void setHitDataGuardSound(DreamPlayer* tPlayer, int tIsInPlayerFile, int tGroup, int tItem);

MugenAttackHeight getActiveHitDataGroundType(DreamPlayer* tPlayer);
MugenAttackHeight getHitDataGroundType(DreamPlayer* tPlayer);
void setHitDataGroundType(DreamPlayer* tPlayer, MugenAttackHeight tType);
MugenAttackHeight getActiveHitDataAirType(DreamPlayer* tPlayer);
MugenAttackHeight getHitDataAirType(DreamPlayer* tPlayer);
void setHitDataAirType(DreamPlayer* tPlayer, MugenAttackHeight tType);

int getActiveHitDataGroundHitTime(DreamPlayer* tPlayer);
int getHitDataGroundHitTime(DreamPlayer* tPlayer);
void setHitDataGroundHitTime(DreamPlayer* tPlayer, int tHitTime);
int getActiveHitDataGroundSlideTime(DreamPlayer* tPlayer);
int getHitDataGroundSlideTime(DreamPlayer* tPlayer);
void setHitDataGroundSlideTime(DreamPlayer* tPlayer, int tSlideTime);

int getActiveHitDataGuardHitTime(DreamPlayer* tPlayer);
int getHitDataGuardHitTime(DreamPlayer* tPlayer);
void setHitDataGuardHitTime(DreamPlayer* tPlayer, int tHitTime);
int getActiveHitDataGuardSlideTime(DreamPlayer* tPlayer);
int getHitDataGuardSlideTime(DreamPlayer* tPlayer);
void setHitDataGuardSlideTime(DreamPlayer* tPlayer, int tSlideTime);

int getActiveHitDataAirHitTime(DreamPlayer* tPlayer);
void setHitDataAirHitTime(DreamPlayer* tPlayer, int tHitTime);
int getActiveHitDataGuardControlTime(DreamPlayer* tPlayer);
int getHitDataGuardControlTime(DreamPlayer* tPlayer);
void setHitDataGuardControlTime(DreamPlayer* tPlayer, int tControlTime);
int getHitDataGuardDistance(DreamPlayer* tPlayer);
void setHitDataGuardDistance(DreamPlayer* tPlayer, int tDistance);

double getActiveHitDataYAccel(DreamPlayer* tPlayer);
double getHitDataYAccel(DreamPlayer* tPlayer);
void setHitDataYAccel(DreamPlayer* tPlayer, double YAccel);

double getActiveHitDataGroundVelocityX(DreamPlayer* tPlayer);
double getHitDataGroundVelocityX(DreamPlayer* tPlayer);
double getActiveHitDataGroundVelocityY(DreamPlayer* tPlayer);
double getHitDataGroundVelocityY(DreamPlayer* tPlayer);
void setHitDataGroundVelocity(DreamPlayer* tPlayer, double tX, double tY);
double getActiveHitDataGuardVelocity(DreamPlayer* tPlayer);
double getHitDataGuardVelocity(DreamPlayer* tPlayer);
void setHitDataGuardVelocity(DreamPlayer* tPlayer, double tX);
double getActiveHitDataAirVelocityX(DreamPlayer* tPlayer);
double getHitDataAirVelocityX(DreamPlayer* tPlayer);
double getActiveHitDataAirVelocityY(DreamPlayer* tPlayer);
double getHitDataAirVelocityY(DreamPlayer* tPlayer);
void setHitDataAirVelocity(DreamPlayer* tPlayer, double tX, double tY);
void setHitDataAirGuardVelocity(DreamPlayer* tPlayer, double tX, double tY);

double getActiveGroundCornerPushVelocityOffset(DreamPlayer* tPlayer);
double getGroundCornerPushVelocityOffset(DreamPlayer* tPlayer);
void setGroundCornerPushVelocityOffset(DreamPlayer* tPlayer, double tX);
double getActiveAirCornerPushVelocityOffset(DreamPlayer* tPlayer);
void setAirCornerPushVelocityOffset(DreamPlayer* tPlayer, double tX);
double getActiveDownCornerPushVelocityOffset(DreamPlayer* tPlayer);
void setDownCornerPushVelocityOffset(DreamPlayer* tPlayer, double tX);
double getActiveGuardCornerPushVelocityOffset(DreamPlayer* tPlayer);
double getGuardCornerPushVelocityOffset(DreamPlayer* tPlayer);
void setGuardCornerPushVelocityOffset(DreamPlayer* tPlayer, double tX);
double getActiveAirGuardCornerPushVelocityOffset(DreamPlayer* tPlayer);
void setAirGuardCornerPushVelocityOffset(DreamPlayer* tPlayer, double tX);

int getActiveHitDataAirGuardControlTime(DreamPlayer* tPlayer);
void setHitDataAirGuardControlTime(DreamPlayer* tPlayer, int tControlTime);
void setHitDataAirJuggle(DreamPlayer* tPlayer, int tJuggle);

int getActiveHitDataHasMinimumDistance(DreamPlayer* tPlayer);
Vector2DI getActiveHitDataMinimumDistance(DreamPlayer* tPlayer);
void setHitDataMinimumDistanceInactive(DreamPlayer* tPlayer);
void setHitDataMinimumDistance(DreamPlayer* tPlayer, int x, int y);

int getActiveHitDataHasMaximumDistance(DreamPlayer* tPlayer);
Vector2DI getActiveHitDataMaximumDistance(DreamPlayer* tPlayer);
void setHitDataMaximumDistanceInactive(DreamPlayer* tPlayer);
void setHitDataMaximumDistance(DreamPlayer* tPlayer, int x, int y);

int getActiveHitDataHasSnap(DreamPlayer* tPlayer);
Vector2DI getActiveHitDataSnap(DreamPlayer* tPlayer);
void setHitDataSnapInactive(DreamPlayer* tPlayer);
void setHitDataSnap(DreamPlayer* tPlayer, int x, int y);

void setHitDataPlayer1SpritePriority(DreamPlayer* tPlayer, int tPriority);
void setHitDataPlayer2SpritePriority(DreamPlayer* tPlayer, int tPriority);
void setHitDataPlayer1FaceDirection(DreamPlayer* tPlayer, int tFaceDirection);
void setHitDataPlayer1ChangeFaceDirectionRelativeToPlayer2(DreamPlayer* tPlayer, int tFaceDirection);
int getActiveHitDataPlayer2ChangeFaceDirectionRelativeToPlayer1(DreamPlayer* tPlayer);
void setHitDataPlayer2ChangeFaceDirectionRelativeToPlayer1(DreamPlayer* tPlayer, int tFaceDirection);

int getHitDataPlayer1StateNumber(DreamPlayer* tPlayer);
int getActiveHitDataPlayer1StateNumber(DreamPlayer* tPlayer);
void setPlayer1StateNumber(DreamPlayer* tPlayer, int tStateNumber);
int getActiveHitDataPlayer2StateNumber(DreamPlayer* tPlayer);
void setPlayer2StateNumber(DreamPlayer* tPlayer, int tStateNumber);
int getHitDataPlayer2CapableOfGettingPlayer1State(DreamPlayer* tPlayer);
int getActiveHitDataPlayer2CapableOfGettingPlayer1State(DreamPlayer* tPlayer);
void setHitDataPlayer2CapableOfGettingPlayer1State(DreamPlayer* tPlayer, int tVal);
int getActiveHitDataForceStanding(DreamPlayer* tPlayer);
void setHitDataForceStanding(DreamPlayer* tPlayer, int tIsForcedToStand);

int getActiveHitDataFall(DreamPlayer* tPlayer);
int getHitDataFall(DreamPlayer* tPlayer);
void setActiveHitDataFall(DreamPlayer* tPlayer, int tIsCausingPlayer2ToFall);
void setHitDataFall(DreamPlayer* tPlayer, int tIsCausingPlayer2ToFall);
double getActiveHitDataFallXVelocity(DreamPlayer* tPlayer);
double getHitDataFallXVelocity(DreamPlayer* tPlayer);
void setActiveHitDataFallXVelocity(DreamPlayer* tPlayer, double tX, int tCoordinateP);
void setHitDataFallXVelocity(DreamPlayer* tPlayer, double tX);
double getActiveHitDataFallYVelocity(DreamPlayer* tPlayer);
double getHitDataFallYVelocity(DreamPlayer* tPlayer);
void setActiveHitDataFallYVelocity(DreamPlayer* tPlayer, double tY, int tCoordinateP);
void setHitDataFallYVelocity(DreamPlayer* tPlayer, double tY);
int getActiveHitDataFallRecovery(DreamPlayer* tPlayer);
int getHitDataFallRecovery(DreamPlayer* tPlayer);
void setHitDataFallRecovery(DreamPlayer* tPlayer, int tCanRecover);
int getActiveHitDataFallRecoveryTime(DreamPlayer* tPlayer);
void setHitDataFallRecoveryTime(DreamPlayer* tPlayer, int tRecoverTime);
int getActiveHitDataFallDamage(DreamPlayer* tPlayer);
void setHitDataFallDamage(DreamPlayer* tPlayer, int tDamage);
int getActiveHitDataAirFall(DreamPlayer* tPlayer);
void setActiveHitDataAirFall(DreamPlayer* tPlayer, int tIsCausingPlayer2ToFall);
void setHitDataAirFall(DreamPlayer* tPlayer, int tIsCausingPlayer2ToFall);
int getActiveHitDataForceNoFall(DreamPlayer* tPlayer);
void setHitDataForceNoFall(DreamPlayer* tPlayer, int tForcePlayer2NotToFall);

void setHitDataDownVelocity(DreamPlayer* tPlayer, double tX, double tY);
void setHitDataDownHitTime(DreamPlayer* tPlayer, int tHitTime);
void setHitDataDownBounce(DreamPlayer* tPlayer, int tDoesBounce);

int getActiveHitDataHitID(DreamPlayer* tPlayer);
void setHitDataHitID(DreamPlayer* tPlayer, int tID);
int getActiveHitDataChainID(DreamPlayer* tPlayer);
void setHitDataChainID(DreamPlayer* tPlayer, int tID);
void setHitDataNoChainID(DreamPlayer* tPlayer, int tID1, int tID2);

void setHitDataHitOnce(DreamPlayer* tPlayer, int tIsOnlyAffectingOneOpponent);
void setHitDataKill(DreamPlayer* tPlayer, int tCanKill);
void setHitDataGuardKill(DreamPlayer* tPlayer, int tCanKill);
int getActiveHitDataFallKill(DreamPlayer* tPlayer);
void setHitDataFallKill(DreamPlayer* tPlayer, int tCanKill);
void setHitDataNumberOfHits(DreamPlayer* tPlayer, int tNumberOfHits);
int getActiveHitDataPlayer1PowerAdded(DreamPlayer* tPlayer);
int getActiveHitDataPlayer1GuardPowerAdded(DreamPlayer* tPlayer);
int getHitDataPlayer1PowerAdded(DreamPlayer* tPlayer);
void setHitDataGetPower(DreamPlayer* tPlayer, int tPlayer1PowerAdded, int tPlayer1PowerAddedWhenGuarded);
int getActiveHitDataPlayer2PowerAdded(DreamPlayer* tPlayer);
int getActiveHitDataPlayer2GuardPowerAdded(DreamPlayer* tPlayer);
int getHitDataPlayer2PowerAdded(DreamPlayer* tPlayer);
void setHitDataGivePower(DreamPlayer* tPlayer, int tPlayer2PowerAdded, int tPlayer2PowerAddedWhenGuarded);

int getActiveHitDataPaletteEffectTime(DreamPlayer* tPlayer);
void setHitDataPaletteEffectTime(DreamPlayer* tPlayer, int tEffectTime);
Vector3D getActiveHitDataPaletteEffectMultiplication(DreamPlayer* tPlayer);
void setHitDataPaletteEffectMultiplication(DreamPlayer* tPlayer, int tR, int tG, int tB);
Vector3D getActiveHitDataPaletteEffectAddition(DreamPlayer* tPlayer);
void setHitDataPaletteEffectAddition(DreamPlayer* tPlayer, int tR, int tG, int tB);

int getActiveHitDataEnvironmentShakeTime(DreamPlayer* tPlayer);
void setHitDataEnvironmentShakeTime(DreamPlayer* tPlayer, int tTime);
double getActiveHitDataEnvironmentShakeFrequency(DreamPlayer* tPlayer);
double getHitDataEnvironmentShakeFrequency(DreamPlayer* tPlayer);
void setHitDataEnvironmentShakeFrequency(DreamPlayer* tPlayer, double tFrequency);
int getActiveHitDataEnvironmentShakeAmplitude(DreamPlayer* tPlayer);
int getHitDataEnvironmentShakeAmplitude(DreamPlayer* tPlayer);
void setHitDataEnvironmentShakeAmplitude(DreamPlayer* tPlayer, int tAmplitude);
double getActiveHitDataEnvironmentShakePhase(DreamPlayer* tPlayer);
void setHitDataEnvironmentShakePhase(DreamPlayer* tPlayer, double tPhase);

int getActiveHitDataFallEnvironmentShakeTime(DreamPlayer* tPlayer);
void setActiveHitDataFallEnvironmentShakeTime(DreamPlayer* tPlayer, int tTime);
void setHitDataFallEnvironmentShakeTime(DreamPlayer* tPlayer, int tTime);
double getActiveHitDataFallEnvironmentShakeFrequency(DreamPlayer* tPlayer);
double getHitDataFallEnvironmentShakeFrequency(DreamPlayer* tPlayer);
void setHitDataFallEnvironmentShakeFrequency(DreamPlayer* tPlayer, double tFrequency);
int getActiveHitDataFallEnvironmentShakeAmplitude(DreamPlayer* tPlayer);
void setHitDataFallEnvironmentShakeAmplitude(DreamPlayer* tPlayer, int tAmplitude);
double getActiveHitDataFallEnvironmentShakePhase(DreamPlayer* tPlayer);
void setHitDataFallEnvironmentShakePhase(DreamPlayer* tPlayer, double tPhase);

double getActiveHitDataVelocityX(DreamPlayer* tPlayer);
double getHitDataVelocityX(DreamPlayer* tPlayer);
void setActiveHitDataVelocityX(DreamPlayer* tPlayer, double x, int tCoordinateP);
void setHitDataVelocityX(DreamPlayer* tPlayer, double x);
double getActiveHitDataVelocityY(DreamPlayer* tPlayer);
double getHitDataVelocityY(DreamPlayer* tPlayer);
void setActiveHitDataVelocityY(DreamPlayer* tPlayer, double y, int tCoordinateP);
void setHitDataVelocityY(DreamPlayer* tPlayer, double y);

int getActiveHitDataIsFacingRight(DreamPlayer* tPlayer);
void setHitDataIsFacingRight(DreamPlayer* tPlayer, int tIsFacingRight);

int isHitDataReversalDefActive(DreamPlayer* tPlayer);
void setHitDataReversalDefActive(DreamPlayer* tPlayer);
DreamHitDefAttributeSlot* getHitDataReversalDefReversalAttribute(DreamPlayer* tPlayer);
void setHitDataReversalDefFlag1(DreamPlayer* tPlayer, const char* tFlag);
void addHitDataReversalDefFlag2(DreamPlayer* tPlayer, const char* tFlag);
int getReversalDefPlayer1PauseTime(DreamPlayer* tPlayer);
int getReversalDefPlayer2PauseTime(DreamPlayer* tPlayer);
void setReversalDefPauseTime(DreamPlayer* tPlayer, int tPlayer1PauseTime, int tPlayer2PauseTime);
int isReversalDefSparkInPlayerFile(DreamPlayer* tPlayer);
int getReversalDefSparkNumber(DreamPlayer* tPlayer);
void setReversalDefSparkNumber(DreamPlayer* tPlayer, int tIsInPlayerFile, int tNumber);
Vector2DI getReversalDefSparkXY(DreamPlayer* tPlayer);
void setReversalDefSparkXY(DreamPlayer* tPlayer, int tX, int tY);
void getReversalDefHitSound(DreamPlayer* tPlayer, int* oIsInPlayerFile, Vector2DI* oHitSound);
void setReversalDefHitSound(DreamPlayer* tPlayer, int tIsInPlayerFile, int tGroup, int tItem);
int hasReversalDefP1StateNo(DreamPlayer* tPlayer);
int getReversalDefP1StateNo(DreamPlayer* tPlayer);
void setReversalDefP1StateNo(DreamPlayer* tPlayer, int hasP1StateNo, int p1StateNo = 0);
int hasReversalDefP2StateNo(DreamPlayer* tPlayer);
int getReversalDefP2StateNo(DreamPlayer* tPlayer);
void setReversalDefP2StateNo(DreamPlayer* tPlayer, int hasP2StateNo, int p2StateNo = 0);

void setPlayerHitOverride(DreamPlayer* tPlayer, DreamMugenStateTypeFlags tStateTypeFlags, const std::vector<std::pair<MugenAttackClassFlags, MugenAttackType>>& tAttackClassTypePairs, int tStateNo, int tSlot, int tDuration, int tDoesForceAir);

int hasMatchingHitOverride(DreamPlayer* tPlayer, DreamPlayer * tOtherPlayer);
int isIgnoredBecauseOfHitOverride(DreamPlayer* tPlayer, DreamPlayer* tOtherPlayer);
void getMatchingHitOverrideStateNoAndForceAir(DreamPlayer* tPlayer, DreamPlayer * tOtherPlayer, int* oStateNo, int* oDoesForceAir);

std::string copyOverCleanHitDefAttributeFlag(const char* tSrc);
MugenAttackClassFlags convertMugenAttackClassToFlag(MugenAttackClass tAttackClass);