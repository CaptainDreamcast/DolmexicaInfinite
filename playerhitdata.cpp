#include "playerhitdata.h"

#include <assert.h>

#include <prism/datastructures.h>
#include <prism/log.h>
#include <prism/system.h>

#include "mugensound.h"
#include "stage.h"

typedef struct {
	int mIsActive;
	DreamPlayer* mPlayer;

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

	Position mSparkOffset;

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
	Velocity mGroundVelocity;
	double mGuardVelocity;
	Velocity mAirVelocity;
	Velocity mAirGuardVelocity;

	double mGroundCornerPushVelocityOffset;
	double mAirCornerPushVelocityOffset;
	double mDownCornerPushVelocityOffset;
	double mGuardCornerPushVelocityOffset;
	double mAirGuardCornerPushVelocityOffset;

	int mAirGuardControlTime;
	int mAirJugglePoints;

	int mHasMinimumDistance;
	Vector3DI mMinimumDistance;

	int mHasMaximumDistance;
	Vector3DI mMaximumDistance;

	int mHasSnap;
	Vector3DI mSnap;

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
	Velocity mFallVelocity;

	int mCanRecoverFall;
	int mFallRecoveryTime;
	int mFallDamage;

	int mAirFall;
	int mForcePlayer2OutOfFallState;

	Velocity mDownVelocity;
	int mDownHitTime;
	int mDownDoesBounce;

	int mHitID;
	int mChainID;
	Vector3DI mNoChainIDs;

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
	Vector3DI mPaletteEffectMultiplication;
	Vector3DI mPaletteEffectAddition;

	int mEnvironmentShakeTime;
	double mEnvironmentShakeFrequency;
	int mEnvironmentShakeAmplitude;
	double mEnvironmentShakePhase;

	int mFallEnvironmentShakeTime;
	double mFallEnvironmentShakeFrequency;
	int mFallEnvironmentShakeAmplitude;
	double mFallEnvironmentShakePhase;

	Velocity mVelocity;

	int mIsFacingRight;

	DreamHitDefAttributeSlot mReversalDef;
} PlayerHitData;

typedef struct {
	int mIsActive;

	DreamMugenStateType mStateType;
	MugenAttackClass mAttackClass;
	MugenAttackType mAttackType;
	int mStateNo;
	int mSlot;

	int mNow;
	int mDuration;

	int mDoesForceAir;
} HitOverride;

typedef struct {
	HitOverride mHitOverrides[8];
} PlayerHitOverrides;


static struct {
	IntMap mPassiveHitDataMap; // contains PlayerHitData
	IntMap mActiveHitDataMap; // contains PlayerHitData
	IntMap mHitOverrideMap; // contains PlayerHitOverrides
} gData;

static void loadHitDataHandler(void* tData) {
	(void)tData;
	gData.mPassiveHitDataMap = new_int_map();
	gData.mActiveHitDataMap = new_int_map();
	gData.mHitOverrideMap = new_int_map();
}

static void unloadHitDataHandler(void* tData) {
	(void)tData;
	delete_int_map(&gData.mPassiveHitDataMap);
	delete_int_map(&gData.mActiveHitDataMap);
	delete_int_map(&gData.mHitOverrideMap);
}

static void updateSingleOverride(HitOverride* e) {
	if (!e->mIsActive) return;
	if (e->mDuration == -1) return;

	e->mNow++;
	if (e->mNow >= e->mDuration) {
		e->mIsActive = 0;
	}
}

static void updateSinglePlayerOverrides(void* tCaller, void* tData) {
	(void)tCaller;
	PlayerHitOverrides* data = (PlayerHitOverrides*)tData;

	int i;
	for (i = 0; i < 8; i++) {
		updateSingleOverride(&data->mHitOverrides[i]);
	}
}

static void updateHitDataHandler(void* tData) {
	(void)tData;
	int_map_map(&gData.mHitOverrideMap, updateSinglePlayerOverrides, NULL);
}

ActorBlueprint getHitDataHandler() {
	return makeActorBlueprint(loadHitDataHandler, unloadHitDataHandler, updateHitDataHandler);
};

int initPlayerHitDataAndReturnID(DreamPlayer* tPlayer)
{
	PlayerHitData* passive = (PlayerHitData*)allocMemory(sizeof(PlayerHitData));
	passive->mIsActive = 0;
	passive->mPlayer = tPlayer;

	int id = int_map_push_back_owned(&gData.mPassiveHitDataMap, passive);

	PlayerHitData* active = (PlayerHitData*)allocMemory(sizeof(PlayerHitData));
	active->mIsActive = 0;
	active->mPlayer = tPlayer;

	int_map_push_owned(&gData.mActiveHitDataMap, id, active);

	PlayerHitOverrides* hitOverrides = (PlayerHitOverrides*)allocMemory(sizeof(PlayerHitOverrides));
	int i;
	for (i = 0; i < 8; i++) hitOverrides->mHitOverrides[i].mIsActive = 0;

	int_map_push_owned(&gData.mHitOverrideMap, id, hitOverrides);

	return id;
}

void removePlayerHitData(DreamPlayer* tPlayer)
{
	assert(int_map_contains(&gData.mActiveHitDataMap, tPlayer->mHitDataID));
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	assert(int_map_contains(&gData.mHitOverrideMap, tPlayer->mHitDataID));

	int_map_remove(&gData.mActiveHitDataMap, tPlayer->mHitDataID);
	int_map_remove(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	int_map_remove(&gData.mHitOverrideMap, tPlayer->mHitDataID);
}

void copyHitDataToActive(DreamPlayer* tPlayer, void * tHitData)
{
	PlayerHitData* passive = (PlayerHitData*)tHitData;
	assert(passive->mIsActive);
	assert(int_map_contains(&gData.mActiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* active = (PlayerHitData*)int_map_get(&gData.mActiveHitDataMap, tPlayer->mHitDataID);

	*active = *passive;
}

int isReceivedHitDataActive(void* tHitData)
{
	PlayerHitData* passive = (PlayerHitData*)tHitData;
	return passive->mIsActive;
}

int isHitDataActive(DreamPlayer* tPlayer)
{
	if (!int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID)) return 0;
	
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	return e->mIsActive;
}

int isActiveHitDataActive(DreamPlayer * tPlayer)
{
	if(!int_map_contains(&gData.mActiveHitDataMap, tPlayer->mHitDataID)) return 0;

	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mActiveHitDataMap, tPlayer->mHitDataID);
	return e->mIsActive;
}

void setHitDataActive(DreamPlayer* tPlayer)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mIsActive = 1;

	e->mReversalDef.mIsActive = 0;
}

void setReceivedHitDataInactive(void * tHitData)
{
	PlayerHitData* passive = (PlayerHitData*)tHitData;
	passive->mIsActive = 0;
}

void setHitDataInactive(DreamPlayer* tPlayer)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mIsActive = 0;
}

void setActiveHitDataInactive(DreamPlayer * tPlayer)
{
	if (!int_map_contains(&gData.mActiveHitDataMap, tPlayer->mHitDataID)) return; // TODO: fix
	
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mActiveHitDataMap, tPlayer->mHitDataID);
	e->mIsActive = 0;
}

void * getPlayerHitDataReference(DreamPlayer* tPlayer)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	return e;
}

DreamPlayer * getReceivedHitDataPlayer(void * tHitData)
{
	PlayerHitData* passive = (PlayerHitData*)tHitData;

	return passive->mPlayer;
}

DreamMugenStateType getHitDataType(DreamPlayer * tPlayer)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	return e->mType;
}

void setHitDataType(DreamPlayer* tPlayer, DreamMugenStateType tType)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mType = tType;
}

MugenAttackClass getHitDataAttackClass(DreamPlayer * tPlayer)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	return e->mAttackClass;
}

MugenAttackClass getActiveHitDataAttackClass(DreamPlayer * tPlayer)
{
	assert(int_map_contains(&gData.mActiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mActiveHitDataMap, tPlayer->mHitDataID);
	return e->mAttackClass;
}

void setHitDataAttackClass(DreamPlayer* tPlayer, MugenAttackClass tClass)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mAttackClass = tClass;
}

MugenAttackType getHitDataAttackType(DreamPlayer* tPlayer)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	return e->mAttackType;
}

MugenAttackType getActiveHitDataAttackType(DreamPlayer * tPlayer)
{
	assert(int_map_contains(&gData.mActiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mActiveHitDataMap, tPlayer->mHitDataID);
	return e->mAttackType;
}

void setHitDataAttackType(DreamPlayer* tPlayer, MugenAttackType tType)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mAttackType = tType;
}

void setHitDataHitFlag(DreamPlayer* tPlayer, char * tFlag)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	assert(strlen(tFlag) < sizeof e->mHitFlag);
	strcpy(e->mHitFlag, tFlag);
}

char* getActiveHitDataGuardFlag(DreamPlayer * tPlayer)
{
	assert(int_map_contains(&gData.mActiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mActiveHitDataMap, tPlayer->mHitDataID);
	return e->mGuardFlag;
}

void setHitDataGuardFlag(DreamPlayer* tPlayer, char * tFlag)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	assert(strlen(tFlag) < sizeof e->mGuardFlag);
	strcpy(e->mGuardFlag, tFlag);
}

void setHitDataAffectTeam(DreamPlayer* tPlayer, MugenAffectTeam tAffectTeam)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mAffectTeam = tAffectTeam;
}

MugenHitAnimationType getActiveHitDataAnimationType(DreamPlayer * tPlayer)
{
	assert(int_map_contains(&gData.mActiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mActiveHitDataMap, tPlayer->mHitDataID);
	return e->mAnimationType;
}

MugenHitAnimationType getHitDataAnimationType(DreamPlayer* tPlayer)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	return e->mAnimationType;
}

void setHitDataAnimationType(DreamPlayer* tPlayer, MugenHitAnimationType tAnimationType)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mAnimationType = tAnimationType;
}

MugenHitAnimationType getHitDataAirAnimationType(DreamPlayer* tPlayer)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	return e->mAirAnimationType;
}

void setHitDataAirAnimationType(DreamPlayer* tPlayer, MugenHitAnimationType tAnimationType)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mAirAnimationType = tAnimationType;
}

void setHitDataFallAnimationType(DreamPlayer* tPlayer, MugenHitAnimationType tAnimationType)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mFallAnimationType = tAnimationType;
}

void setHitDataPriority(DreamPlayer* tPlayer, int tPriority, MugenHitPriorityType tPriorityType)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mPriority = tPriority;
	e->mPriorityType = tPriorityType;
}

int getActiveHitDataDamage(DreamPlayer * tPlayer)
{
	assert(int_map_contains(&gData.mActiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mActiveHitDataMap, tPlayer->mHitDataID);
	return e->mDamage;
}

int getActiveHitDataGuardDamage(DreamPlayer * tPlayer)
{
	assert(int_map_contains(&gData.mActiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mActiveHitDataMap, tPlayer->mHitDataID);
	return e->mGuardDamage;
}

int getHitDataDamage(DreamPlayer* tPlayer)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	return e->mDamage;
}

void setHitDataDamage(DreamPlayer* tPlayer, int tDamage, int tGuardDamage)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mDamage = tDamage;
	e->mGuardDamage = tGuardDamage;
}

int getActiveHitDataPlayer1PauseTime(DreamPlayer * tPlayer)
{
	assert(int_map_contains(&gData.mActiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mActiveHitDataMap, tPlayer->mHitDataID);
	return e->mPlayer1PauseTime;
}

int getActiveHitDataPlayer1GuardPauseTime(DreamPlayer * tPlayer)
{
	assert(int_map_contains(&gData.mActiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mActiveHitDataMap, tPlayer->mHitDataID);
	return e->mGuardPlayer1PauseTime;
}

int getHitDataPlayer1PauseTime(DreamPlayer* tPlayer)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	return e->mPlayer1PauseTime;
}

int getActiveHitDataPlayer2PauseTime(DreamPlayer * tPlayer)
{
	assert(int_map_contains(&gData.mActiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mActiveHitDataMap, tPlayer->mHitDataID);
	return e->mPlayer2ShakeTime;
}

int getActiveHitDataPlayer2GuardPauseTime(DreamPlayer * tPlayer)
{
	assert(int_map_contains(&gData.mActiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mActiveHitDataMap, tPlayer->mHitDataID);
	return e->mGuardPlayer2ShakeTime;
}

int getHitDataPlayer2PauseTime(DreamPlayer* tPlayer)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	return e->mPlayer2ShakeTime;
}

void setHitDataPauseTime(DreamPlayer* tPlayer, int tPlayer1PauseTime, int tPlayer2PauseTime)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mPlayer1PauseTime = tPlayer1PauseTime;
	e->mPlayer2ShakeTime = tPlayer2PauseTime;
}

void setHitDataGuardPauseTime(DreamPlayer* tPlayer, int tPlayer1PauseTime, int tPlayer2PauseTime)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mGuardPlayer1PauseTime = tPlayer1PauseTime;
	e->mGuardPlayer2ShakeTime = tPlayer2PauseTime;
}

int isActiveHitDataSparkInPlayerFile(DreamPlayer * tPlayer)
{
	assert(int_map_contains(&gData.mActiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mActiveHitDataMap, tPlayer->mHitDataID);
	return e->mIsSparkInPlayerFile;
}

int isActiveHitDataGuardSparkInPlayerFile(DreamPlayer * tPlayer)
{
	assert(int_map_contains(&gData.mActiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mActiveHitDataMap, tPlayer->mHitDataID);
	return e->mIsGuardSparkInPlayerFile;
}

int isHitDataSparkInPlayerFile(DreamPlayer* tPlayer)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	return e->mIsSparkInPlayerFile;
}

int getActiveHitDataSparkNumber(DreamPlayer * tPlayer)
{
	assert(int_map_contains(&gData.mActiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mActiveHitDataMap, tPlayer->mHitDataID);
	return e->mSparkNumber;
}

int getActiveHitDataGuardSparkNumber(DreamPlayer * tPlayer)
{
	assert(int_map_contains(&gData.mActiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mActiveHitDataMap, tPlayer->mHitDataID);
	return e->mGuardSparkNumber;
}

int getHitDataGuardSparkNumber(DreamPlayer * tPlayer)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	return e->mGuardSparkNumber;
}

int getHitDataSparkNumber(DreamPlayer* tPlayer)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	return e->mSparkNumber;
}

void setHitDataSparkNumber(DreamPlayer* tPlayer, int tIsInPlayerFile, int tNumber)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mIsSparkInPlayerFile = tIsInPlayerFile;
	e->mSparkNumber = tNumber;
}

void setHitDataGuardSparkNumber(DreamPlayer* tPlayer, int tIsInPlayerFile, int tNumber)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mIsGuardSparkInPlayerFile = tIsInPlayerFile;
	e->mGuardSparkNumber = tNumber;
}

Position getActiveHitDataSparkXY(DreamPlayer * tPlayer)
{
	assert(int_map_contains(&gData.mActiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mActiveHitDataMap, tPlayer->mHitDataID);
	return e->mSparkOffset;
}

Position getHitDataSparkXY(DreamPlayer* tPlayer)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	return e->mSparkOffset;
}

void setHitDataSparkXY(DreamPlayer* tPlayer, int tX, int tY)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mSparkOffset = makePosition(tX, tY, 0);
}

void getActiveHitDataHitSound(DreamPlayer * tPlayer, int * oIsInPlayerFile, Vector3DI * oSound)
{
	assert(int_map_contains(&gData.mActiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mActiveHitDataMap, tPlayer->mHitDataID);
	*oIsInPlayerFile = e->mIsHitSoundInPlayerFile;
	*oSound = makeVector3DI(e->mHitSound.mGroup, e->mHitSound.mItem, 0);
}

void setHitDataHitSound(DreamPlayer* tPlayer, int tIsInPlayerFile, int tGroup, int tItem)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mIsHitSoundInPlayerFile = tIsInPlayerFile;
	e->mHitSound = makeDreamMugenSound(tGroup, tItem);
}

void getActiveHitDataGuardSound(DreamPlayer * tPlayer, int * oIsInPlayerFile, Vector3DI * oSound)
{
	assert(int_map_contains(&gData.mActiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mActiveHitDataMap, tPlayer->mHitDataID);
	*oIsInPlayerFile = e->mIsGuardSoundInPlayerFile;
	*oSound = makeVector3DI(e->mGuardSound.mGroup, e->mGuardSound.mItem, 0);
}

void setHitDataGuardSound(DreamPlayer* tPlayer, int tIsInPlayerFile, int tGroup, int tItem)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mIsGuardSoundInPlayerFile = tIsInPlayerFile;
	e->mGuardSound = makeDreamMugenSound(tGroup, tItem);
}

MugenAttackHeight getActiveHitDataGroundType(DreamPlayer * tPlayer)
{
	assert(int_map_contains(&gData.mActiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mActiveHitDataMap, tPlayer->mHitDataID);
	return e->mGroundType;
}

MugenAttackHeight getHitDataGroundType(DreamPlayer* tPlayer)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	return e->mGroundType;
}

void setHitDataGroundType(DreamPlayer* tPlayer, MugenAttackHeight tType)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mGroundType = tType;
}

MugenAttackHeight getActiveHitDataAirType(DreamPlayer * tPlayer)
{
	assert(int_map_contains(&gData.mActiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mActiveHitDataMap, tPlayer->mHitDataID);
	return e->mAirType;
}

MugenAttackHeight getHitDataAirType(DreamPlayer* tPlayer)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	return e->mAirType;
}

void setHitDataAirType(DreamPlayer* tPlayer, MugenAttackHeight tType)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mAirType = tType;
}

int getActiveHitDataGroundHitTime(DreamPlayer * tPlayer)
{
	assert(int_map_contains(&gData.mActiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mActiveHitDataMap, tPlayer->mHitDataID);
	return e->mGroundHitTime;
}

int getHitDataGroundHitTime(DreamPlayer* tPlayer)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	return e->mGroundHitTime;
}

void setHitDataGroundHitTime(DreamPlayer* tPlayer, int tHitTime)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mGroundHitTime = tHitTime;
}

int getActiveHitDataGroundSlideTime(DreamPlayer * tPlayer)
{
	assert(int_map_contains(&gData.mActiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mActiveHitDataMap, tPlayer->mHitDataID);
	return e->mGroundSlideTime;
}

int getHitDataGroundSlideTime(DreamPlayer* tPlayer)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	return e->mGroundSlideTime;
}

void setHitDataGroundSlideTime(DreamPlayer* tPlayer, int tSlideTime)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mGroundSlideTime = tSlideTime;
}

int getActiveHitDataGuardHitTime(DreamPlayer * tPlayer)
{
	assert(int_map_contains(&gData.mActiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mActiveHitDataMap, tPlayer->mHitDataID);
	return e->mGuardHitTime;
}

int getHitDataGuardHitTime(DreamPlayer* tPlayer)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	return e->mGuardHitTime;
}

void setHitDataGuardHitTime(DreamPlayer* tPlayer, int tHitTime)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mGuardHitTime = tHitTime;
}

int getHitDataGuardSlideTime(DreamPlayer* tPlayer)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	return e->mGuardSlideTime;
}

void setHitDataGuardSlideTime(DreamPlayer* tPlayer, int tSlideTime)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mGuardSlideTime = tSlideTime;
}

int getActiveHitDataAirHitTime(DreamPlayer * tPlayer)
{
	assert(int_map_contains(&gData.mActiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mActiveHitDataMap, tPlayer->mHitDataID);
	return e->mAirHitTime;
}

void setHitDataAirHitTime(DreamPlayer* tPlayer, int tHitTime)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mAirHitTime = tHitTime;
}

int getActiveHitDataGuardControlTime(DreamPlayer * tPlayer)
{
	assert(int_map_contains(&gData.mActiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mActiveHitDataMap, tPlayer->mHitDataID);
	return e->mGuardControlTime;
}

int getHitDataGuardControlTime(DreamPlayer* tPlayer)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	return e->mGuardControlTime;
}

void setHitDataGuardControlTime(DreamPlayer* tPlayer, int tControlTime)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mGuardControlTime = tControlTime;
}

int getHitDataGuardDistance(DreamPlayer* tPlayer)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	return e->mGuardDistance;
}

void setHitDataGuardDistance(DreamPlayer* tPlayer, int tDistance)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mGuardDistance = tDistance;
}

double getActiveHitDataYAccel(DreamPlayer * tPlayer)
{
	if (isActiveHitDataActive(tPlayer)) { // TODO: properly
		assert(int_map_contains(&gData.mActiveHitDataMap, tPlayer->mHitDataID));
		PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mActiveHitDataMap, tPlayer->mHitDataID);
		return e->mVerticalAcceleration;
	}
	else {
		return transformDreamCoordinates(0.7, 480, getPlayerCoordinateP(tPlayer));
	}
}

double getHitDataYAccel(DreamPlayer* tPlayer)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	return e->mVerticalAcceleration;
}

void setHitDataYAccel(DreamPlayer* tPlayer, double YAccel)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mVerticalAcceleration = YAccel;
}

double getActiveHitDataGroundVelocityX(DreamPlayer * tPlayer)
{
	assert(int_map_contains(&gData.mActiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mActiveHitDataMap, tPlayer->mHitDataID);
	return e->mGroundVelocity.x;
}

double getHitDataGroundVelocityX(DreamPlayer* tPlayer)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	return e->mGroundVelocity.x;
}

double getActiveHitDataGroundVelocityY(DreamPlayer * tPlayer)
{
	assert(int_map_contains(&gData.mActiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mActiveHitDataMap, tPlayer->mHitDataID);
	return e->mGroundVelocity.y;
}

double getHitDataGroundVelocityY(DreamPlayer* tPlayer)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	return e->mGroundVelocity.y;
}

void setHitDataGroundVelocity(DreamPlayer* tPlayer, double tX, double tY)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mGroundVelocity = makePosition(tX, tY, 0);
}

double getActiveHitDataGuardVelocity(DreamPlayer * tPlayer)
{
	assert(int_map_contains(&gData.mActiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mActiveHitDataMap, tPlayer->mHitDataID);
	return e->mGuardVelocity;
}

double getHitDataGuardVelocity(DreamPlayer* tPlayer)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	return e->mGuardVelocity;
}

void setHitDataGuardVelocity(DreamPlayer* tPlayer, double tX)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mGuardVelocity = tX;
}

double getActiveHitDataAirVelocityX(DreamPlayer * tPlayer)
{
	assert(int_map_contains(&gData.mActiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mActiveHitDataMap, tPlayer->mHitDataID);
	return e->mAirVelocity.x;
}

double getHitDataAirVelocityX(DreamPlayer* tPlayer)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	return e->mAirVelocity.x;
}

double getActiveHitDataAirVelocityY(DreamPlayer * tPlayer)
{
	assert(int_map_contains(&gData.mActiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mActiveHitDataMap, tPlayer->mHitDataID);
	return e->mAirVelocity.y;
}

double getHitDataAirVelocityY(DreamPlayer* tPlayer)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	return e->mAirVelocity.y;
}

void setHitDataAirVelocity(DreamPlayer* tPlayer, double tX, double tY)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mAirVelocity = makePosition(tX, tY, 0);
}

void setHitDataAirGuardVelocity(DreamPlayer* tPlayer, double tX, double tY)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mAirGuardVelocity = makePosition(tX, tY, 0);
}

double getActiveGroundCornerPushVelocityOffset(DreamPlayer * tPlayer)
{
	assert(int_map_contains(&gData.mActiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mActiveHitDataMap, tPlayer->mHitDataID);
	return e->mGroundCornerPushVelocityOffset;
}

double getGroundCornerPushVelocityOffset(DreamPlayer* tPlayer)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	return e->mGroundCornerPushVelocityOffset;
}

void setGroundCornerPushVelocityOffset(DreamPlayer* tPlayer, double tX)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mGroundCornerPushVelocityOffset = tX;
}

double getActiveAirCornerPushVelocityOffset(DreamPlayer * tPlayer)
{
	assert(int_map_contains(&gData.mActiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mActiveHitDataMap, tPlayer->mHitDataID);
	return e->mAirCornerPushVelocityOffset;
}

void setAirCornerPushVelocityOffset(DreamPlayer* tPlayer, double tX)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mAirCornerPushVelocityOffset = tX;
}

double getActiveDownCornerPushVelocityOffset(DreamPlayer * tPlayer)
{
	assert(int_map_contains(&gData.mActiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mActiveHitDataMap, tPlayer->mHitDataID);
	return e->mDownCornerPushVelocityOffset;
}

void setDownCornerPushVelocityOffset(DreamPlayer* tPlayer, double tX)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mDownCornerPushVelocityOffset = tX;
}

double getActiveGuardCornerPushVelocityOffset(DreamPlayer * tPlayer)
{
	assert(int_map_contains(&gData.mActiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mActiveHitDataMap, tPlayer->mHitDataID);
	return e->mGuardCornerPushVelocityOffset;
}

double getGuardCornerPushVelocityOffset(DreamPlayer* tPlayer)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	return e->mGuardCornerPushVelocityOffset;
}

void setGuardCornerPushVelocityOffset(DreamPlayer* tPlayer, double tX)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mGuardCornerPushVelocityOffset = tX;
}

double getActiveAirGuardCornerPushVelocityOffset(DreamPlayer * tPlayer)
{
	assert(int_map_contains(&gData.mActiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mActiveHitDataMap, tPlayer->mHitDataID);
	return e->mAirGuardCornerPushVelocityOffset;
}

void setAirGuardCornerPushVelocityOffset(DreamPlayer* tPlayer, double tX)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mAirGuardCornerPushVelocityOffset = tX;
}

int getActiveHitDataAirGuardControlTime(DreamPlayer * tPlayer)
{
	assert(int_map_contains(&gData.mActiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mActiveHitDataMap, tPlayer->mHitDataID);
	return e->mAirGuardControlTime;
}

void setHitDataAirGuardControlTime(DreamPlayer* tPlayer, int tControlTime)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mAirGuardControlTime = tControlTime;
}

void setHitDataAirJuggle(DreamPlayer* tPlayer, int tJuggle)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mAirJugglePoints = tJuggle;
}

void setHitDataMinimumDistanceInactive(DreamPlayer* tPlayer)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mHasMinimumDistance = 0;
}

void setHitDataMinimumDistance(DreamPlayer* tPlayer, int x, int y)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mHasMinimumDistance = 1;
	e->mMinimumDistance = makeVector3DI(x, y, 0);
}

void setHitDataMaximumDistanceInactive(DreamPlayer* tPlayer)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mHasMaximumDistance = 0;
}

void setHitDataMaximumDistance(DreamPlayer* tPlayer, int x, int y)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mHasMaximumDistance = 1;
	e->mMaximumDistance = makeVector3DI(x, y, 0);
}

void setHitDataSnapInactive(DreamPlayer* tPlayer)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mHasSnap = 0;
}

void setHitDataSnap(DreamPlayer* tPlayer, int x, int y)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mHasSnap = 1;
	e->mSnap = makeVector3DI(x, y, 0);
}

void setHitDataPlayer1SpritePriority(DreamPlayer* tPlayer, int tPriority)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mPlayer1DrawingPriority = tPriority;
}

void setHitDataPlayer2SpritePriority(DreamPlayer* tPlayer, int tPriority)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mPlayer2DrawingPriority = tPriority;
}

void setHitDataPlayer1FaceDirection(DreamPlayer* tPlayer, int tFaceDirection)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mIsPlayer1TurningAround = tFaceDirection == -1;
}

void setHitDataPlayer1ChangeFaceDirectionRelativeToPlayer2(DreamPlayer* tPlayer, int tFaceDirection)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mPlayer1ChangeFaceDirectionRelativeToPlayer2 = tFaceDirection;
}

void setHitDataPlayer2ChangeFaceDirectionRelativeToPlayer1(DreamPlayer* tPlayer, int tFaceDirection)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mPlayer2ChangeFaceDirectionRelativeToPlayer1 = tFaceDirection;
}

int getHitDataPlayer1StateNumber(DreamPlayer * tPlayer)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	return e->mPlayer1StateNumber;
}

int getActiveHitDataPlayer1StateNumber(DreamPlayer * tPlayer)
{
	assert(int_map_contains(&gData.mActiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mActiveHitDataMap, tPlayer->mHitDataID);
	return e->mPlayer1StateNumber;
}

void setPlayer1StateNumber(DreamPlayer* tPlayer, int tStateNumber)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mPlayer1StateNumber = tStateNumber;
}

int getActiveHitDataPlayer2StateNumber(DreamPlayer * tPlayer)
{
	assert(int_map_contains(&gData.mActiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mActiveHitDataMap, tPlayer->mHitDataID);
	return e->mPlayer2StateNumber;
}

void setPlayer2StateNumber(DreamPlayer* tPlayer, int tStateNumber)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mPlayer2StateNumber = tStateNumber;
}

int getHitDataPlayer2CapableOfGettingPlayer1State(DreamPlayer * tPlayer)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	return e->mCanPlayer2GetPlayer1State;
}

int getActiveHitDataPlayer2CapableOfGettingPlayer1State(DreamPlayer * tPlayer)
{
	assert(int_map_contains(&gData.mActiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mActiveHitDataMap, tPlayer->mHitDataID);
	return e->mCanPlayer2GetPlayer1State;
}

void setHitDataPlayer2CapableOfGettingPlayer1State(DreamPlayer* tPlayer, int tVal)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mCanPlayer2GetPlayer1State = tVal;
}

void setHitDataForceStanding(DreamPlayer* tPlayer, int tIsForcedToStand)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mIsForcingPlayer2ToStandingPosition = tIsForcedToStand;
}

int getActiveHitDataFall(DreamPlayer * tPlayer)
{
	assert(int_map_contains(&gData.mActiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mActiveHitDataMap, tPlayer->mHitDataID);
	return e->mFall;
}

int getHitDataFall(DreamPlayer* tPlayer)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	return e->mFall;
}

void setActiveHitDataFall(DreamPlayer * tPlayer, int tIsCausingPlayer2ToFall)
{
	assert(int_map_contains(&gData.mActiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mActiveHitDataMap, tPlayer->mHitDataID);
	e->mFall = tIsCausingPlayer2ToFall;
}

void setHitDataFall(DreamPlayer* tPlayer, int tIsCausingPlayer2ToFall)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mFall = tIsCausingPlayer2ToFall;
}

double getActiveHitDataFallXVelocity(DreamPlayer * tPlayer)
{
	assert(int_map_contains(&gData.mActiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mActiveHitDataMap, tPlayer->mHitDataID);
	return e->mFallVelocity.x;
}

double getHitDataFallXVelocity(DreamPlayer* tPlayer)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	return e->mFallVelocity.x;
}

void setActiveHitDataFallXVelocity(DreamPlayer * tPlayer, double tX)
{
	assert(int_map_contains(&gData.mActiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mActiveHitDataMap, tPlayer->mHitDataID);
	e->mFallVelocity.x = tX;
}

void setHitDataFallXVelocity(DreamPlayer* tPlayer, double tX)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mFallVelocity.x = tX;
}

double getActiveHitDataFallYVelocity(DreamPlayer * tPlayer)
{
	assert(int_map_contains(&gData.mActiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mActiveHitDataMap, tPlayer->mHitDataID);
	return e->mFallVelocity.y;
}

double getHitDataFallYVelocity(DreamPlayer* tPlayer)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	return e->mFallVelocity.y;
}

void setActiveHitDataFallYVelocity(DreamPlayer * tPlayer, double tY)
{
	assert(int_map_contains(&gData.mActiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mActiveHitDataMap, tPlayer->mHitDataID);
	e->mFallVelocity.y = tY;
}

void setHitDataFallYVelocity(DreamPlayer* tPlayer, double tY)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mFallVelocity.y = tY;
}

int getActiveHitDataFallRecovery(DreamPlayer * tPlayer)
{
	assert(int_map_contains(&gData.mActiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mActiveHitDataMap, tPlayer->mHitDataID);
	return e->mCanRecoverFall;
}

int getHitDataFallRecovery(DreamPlayer* tPlayer)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	return e->mCanRecoverFall;
}

void setHitDataFallRecovery(DreamPlayer* tPlayer, int tCanRecover)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mCanRecoverFall = tCanRecover;
}

void setHitDataFallRecoveryTime(DreamPlayer* tPlayer, int tRecoverTime)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mFallRecoveryTime = tRecoverTime;
}

int getActiveHitDataFallDamage(DreamPlayer * tPlayer)
{
	assert(int_map_contains(&gData.mActiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mActiveHitDataMap, tPlayer->mHitDataID);
	return e->mFallDamage;
}

void setHitDataFallDamage(DreamPlayer* tPlayer, int tDamage)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mFallDamage = tDamage;
}

int getActiveHitDataAirFall(DreamPlayer * tPlayer)
{
	assert(int_map_contains(&gData.mActiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mActiveHitDataMap, tPlayer->mHitDataID);
	return e->mAirFall;
}

void setActiveHitDataAirFall(DreamPlayer * tPlayer, int tIsCausingPlayer2ToFall)
{
	assert(int_map_contains(&gData.mActiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mActiveHitDataMap, tPlayer->mHitDataID);
	e->mAirFall = tIsCausingPlayer2ToFall;
}

void setHitDataAirFall(DreamPlayer* tPlayer, int tIsCausingPlayer2ToFall)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mAirFall = tIsCausingPlayer2ToFall;
}

void setHitDataForceNoFall(DreamPlayer* tPlayer, int tForcePlayer2NotToFall)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mForcePlayer2OutOfFallState = tForcePlayer2NotToFall;
}

void setHitDataDownVelocity(DreamPlayer* tPlayer, double tX, double tY)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mDownVelocity = makePosition(tX, tY, 0);
}

void setHitDataDownHitTime(DreamPlayer* tPlayer, int tHitTime)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mDownHitTime = tHitTime;
}

void setHitDataDownBounce(DreamPlayer* tPlayer, int tDoesBounce)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mDownDoesBounce = tDoesBounce;
}

void setHitDataHitID(DreamPlayer* tPlayer, int tID)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mHitID = tID;
}

void setHitDataChainID(DreamPlayer* tPlayer, int tID)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mChainID = tID;
}

void setHitDataNoChainID(DreamPlayer* tPlayer, int tID1, int tID2)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mNoChainIDs = makeVector3DI(tID1, tID2, -1);
}

void setHitDataHitOnce(DreamPlayer* tPlayer, int tIsOnlyAffectingOneOpponent)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mDoesOnlyHitOneEnemy = tIsOnlyAffectingOneOpponent;
}

void setHitDataKill(DreamPlayer* tPlayer, int tCanKill)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mCanKill = tCanKill;
}

void setHitDataGuardKill(DreamPlayer* tPlayer, int tCanKill)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mCanGuardKill = tCanKill;
}

void setHitDataFallKill(DreamPlayer* tPlayer, int tCanKill)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mCanFallKill = tCanKill;
}

void setHitDataNumberOfHits(DreamPlayer* tPlayer, int tNumberOfHits)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mNumberOfHitsForComboCounter = tNumberOfHits;
}

int getActiveHitDataPlayer1PowerAdded(DreamPlayer * tPlayer)
{
	assert(int_map_contains(&gData.mActiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mActiveHitDataMap, tPlayer->mHitDataID);
	return e->mGetPlayer1Power;
}

int getActiveHitDataPlayer1GuardPowerAdded(DreamPlayer * tPlayer)
{
	assert(int_map_contains(&gData.mActiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mActiveHitDataMap, tPlayer->mHitDataID);
	return e->mGetPlayer1GuardPower;
}

int getHitDataPlayer1PowerAdded(DreamPlayer* tPlayer)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	return e->mGetPlayer1Power;
}

void setHitDataGetPower(DreamPlayer* tPlayer, int tPlayer1PowerAdded, int tPlayer1PowerAddedWhenGuarded)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mGetPlayer1Power = tPlayer1PowerAdded;
	e->mGetPlayer1GuardPower = tPlayer1PowerAddedWhenGuarded;
}

int getActiveHitDataPlayer2PowerAdded(DreamPlayer * tPlayer)
{
	assert(int_map_contains(&gData.mActiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mActiveHitDataMap, tPlayer->mHitDataID);
	return e->mGivePlayer2Power;
}

int getActiveHitDataPlayer2GuardPowerAdded(DreamPlayer * tPlayer)
{
	assert(int_map_contains(&gData.mActiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mActiveHitDataMap, tPlayer->mHitDataID);
	return e->mGivePlayer2GuardPower;
}

int getHitDataPlayer2PowerAdded(DreamPlayer* tPlayer)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	return e->mGivePlayer2Power;
}

void setHitDataGivePower(DreamPlayer* tPlayer, int tPlayer2PowerAdded, int tPlayer2PowerAddedWhenGuarded)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mGivePlayer2Power = tPlayer2PowerAdded;
	e->mGivePlayer2GuardPower = tPlayer2PowerAddedWhenGuarded;
}

void setHitDataPaletteEffectTime(DreamPlayer* tPlayer, int tEffectTime)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mPaletteEffectTime = tEffectTime;
}

void setHitDataPaletteEffectMultiplication(DreamPlayer* tPlayer, int tR, int tG, int tB)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mPaletteEffectMultiplication = makeVector3DI(tR, tG, tB);
}

void setHitDataPaletteEffectAddition(DreamPlayer* tPlayer, int tR, int tG, int tB)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mPaletteEffectAddition = makeVector3DI(tR, tG, tB);
}

void setHitDataEnvironmentShakeTime(DreamPlayer* tPlayer, int tTime)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mEnvironmentShakeTime = tTime;
}

void setHitDataEnvironmentShakeFrequency(DreamPlayer* tPlayer, double tFrequency)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mEnvironmentShakeFrequency = tFrequency;
}

void setHitDataEnvironmentShakeAmplitude(DreamPlayer* tPlayer, int tAmplitude)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mEnvironmentShakeAmplitude = tAmplitude;
}

void setHitDataEnvironmentShakePhase(DreamPlayer* tPlayer, double tPhase)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mEnvironmentShakePhase = tPhase;
}

int getActiveHitDataFallEnvironmentShakeTime(DreamPlayer * tPlayer)
{
	assert(int_map_contains(&gData.mActiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mActiveHitDataMap, tPlayer->mHitDataID);
	return e->mFallEnvironmentShakeTime;
}

void setActiveHitDataFallEnvironmentShakeTime(DreamPlayer * tPlayer, int tTime)
{
	assert(int_map_contains(&gData.mActiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mActiveHitDataMap, tPlayer->mHitDataID);
	e->mFallEnvironmentShakeTime = tTime;
}

void setHitDataFallEnvironmentShakeTime(DreamPlayer* tPlayer, int tTime)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mFallEnvironmentShakeTime = tTime;
}

double getActiveHitDataFallEnvironmentShakeFrequency(DreamPlayer * tPlayer)
{
	assert(int_map_contains(&gData.mActiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mActiveHitDataMap, tPlayer->mHitDataID);
	return e->mFallEnvironmentShakeFrequency;
}

void setHitDataFallEnvironmentShakeFrequency(DreamPlayer* tPlayer, double tFrequency)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mFallEnvironmentShakeFrequency = tFrequency;
}

int getActiveHitDataFallEnvironmentShakeAmplitude(DreamPlayer * tPlayer)
{
	assert(int_map_contains(&gData.mActiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mActiveHitDataMap, tPlayer->mHitDataID);
	return e->mFallEnvironmentShakeAmplitude;
}

void setHitDataFallEnvironmentShakeAmplitude(DreamPlayer* tPlayer, int tAmplitude)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mFallEnvironmentShakeAmplitude = tAmplitude;
}

double getActiveHitDataFallEnvironmentShakePhase(DreamPlayer * tPlayer)
{
	assert(int_map_contains(&gData.mActiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mActiveHitDataMap, tPlayer->mHitDataID);
	return e->mFallEnvironmentShakePhase;
}

void setHitDataFallEnvironmentShakePhase(DreamPlayer* tPlayer, double tPhase)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mFallEnvironmentShakePhase = tPhase;
}

double getActiveHitDataVelocityX(DreamPlayer * tPlayer)
{
	assert(int_map_contains(&gData.mActiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mActiveHitDataMap, tPlayer->mHitDataID);
	return e->mVelocity.x;
}

double getHitDataVelocityX(DreamPlayer* tPlayer)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	return e->mVelocity.x;
}

void setActiveHitDataVelocityX(DreamPlayer * tPlayer, double x)
{
	assert(int_map_contains(&gData.mActiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mActiveHitDataMap, tPlayer->mHitDataID);
	e->mVelocity.x = x;
}

void setHitDataVelocityX(DreamPlayer* tPlayer, double x)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mVelocity.x = x;
}

double getActiveHitDataVelocityY(DreamPlayer * tPlayer)
{
	assert(int_map_contains(&gData.mActiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mActiveHitDataMap, tPlayer->mHitDataID);
	return e->mVelocity.y;
}

double getHitDataVelocityY(DreamPlayer* tPlayer)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	return e->mVelocity.y;
}

void setActiveHitDataVelocityY(DreamPlayer * tPlayer, double y)
{
	assert(int_map_contains(&gData.mActiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mActiveHitDataMap, tPlayer->mHitDataID);
	e->mVelocity.y = y;
}

void setHitDataVelocityY(DreamPlayer* tPlayer, double y)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mVelocity.y = y;
}

int getActiveHitDataIsFacingRight(DreamPlayer * tPlayer)
{
	assert(int_map_contains(&gData.mActiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mActiveHitDataMap, tPlayer->mHitDataID);
	return e->mIsFacingRight;
}

void setHitDataIsFacingRight(DreamPlayer * tPlayer, int tIsFacingRight)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mIsFacingRight = tIsFacingRight;
}

void resetHitDataReversalDef(DreamPlayer * tPlayer)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	e->mReversalDef.mIsActive = 1;
	e->mReversalDef.mFlag2Amount = 0;
}

void setHitDataReversalDefFlag1(DreamPlayer * tPlayer, char * tFlag)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);
	strcpy(e->mReversalDef.mFlag1, tFlag);
}

static void copyOverCleanFlag2(char* tDst, char* tSrc) {
	int n = strlen(tSrc);

	int o = 0;
	int i;
	for (i = 0; i < n; i++) {
		if (tSrc[i] == ' ') continue;
		tDst[o++] = tSrc[i];
	}
	tDst[o] = '\0';
}

void addHitDataReversalDefFlag2(DreamPlayer * tPlayer, char * tFlag)
{
	assert(int_map_contains(&gData.mPassiveHitDataMap, tPlayer->mHitDataID));
	PlayerHitData* e = (PlayerHitData*)int_map_get(&gData.mPassiveHitDataMap, tPlayer->mHitDataID);

	char* nFlag = (char*)allocMemory(strlen(tFlag) + 5);
	copyOverCleanFlag2(nFlag, tFlag);
	if (strlen(nFlag) != 2) {
		logWarningFormat("Unparseable reversal definition flag: %s. Ignore.", nFlag);
		freeMemory(nFlag);
		return;
	}
	turnStringLowercase(nFlag);

	if (e->mReversalDef.mFlag2Amount >= MAXIMUM_HITSLOT_FLAG_2_AMOUNT) {
		logWarningFormat("Too many reversal definition flags. Ignoring current flag %s.", nFlag);
		freeMemory(nFlag);
		return;
	}

	strcpy(e->mReversalDef.mFlag2[e->mReversalDef.mFlag2Amount], nFlag);
	e->mReversalDef.mFlag2Amount++;

	freeMemory(nFlag);
}

void setPlayerHitOverride(DreamPlayer * tPlayer, DreamMugenStateType tStateType, MugenAttackClass tAttackClass, MugenAttackType tAttackType, int tStateNo, int tSlot, int tDuration, int tDoesForceAir)
{
	assert(int_map_contains(&gData.mHitOverrideMap, tPlayer->mHitDataID));
	PlayerHitOverrides* overrides = (PlayerHitOverrides*)int_map_get(&gData.mHitOverrideMap, tPlayer->mHitDataID);

	HitOverride* e = &overrides->mHitOverrides[tSlot];
	e->mStateType = tStateType;
	e->mAttackClass = tAttackClass;
	e->mAttackType = tAttackType;
	e->mStateNo = tStateNo;
	e->mSlot = tSlot;
	e->mDoesForceAir = tDoesForceAir;

	e->mNow = 0;
	e->mDuration = tDuration;

	e->mIsActive = 1;
}

int overrideEqualsPlayerHitDef(HitOverride* e, DreamPlayer* tPlayer) {
	if (!e->mIsActive) return 0;

	DreamMugenStateType stateType = getHitDataType(tPlayer);
	MugenAttackClass attackClass = getHitDataAttackClass(tPlayer);
	MugenAttackType attackType = getHitDataAttackType(tPlayer);

	if (e->mStateType != stateType) return 0;
	if (e->mAttackClass != attackClass) return 0;
	if (e->mAttackType != attackType) return 0;

	return 1;
}

int hasMatchingHitOverride(DreamPlayer* tPlayer, DreamPlayer * tOtherPlayer) {
	assert(int_map_contains(&gData.mHitOverrideMap, tPlayer->mHitDataID));
	PlayerHitOverrides* overrides = (PlayerHitOverrides*)int_map_get(&gData.mHitOverrideMap, tPlayer->mHitDataID);

	int i;
	for (i = 0; i < 8; i++) {
		if (overrideEqualsPlayerHitDef(&overrides->mHitOverrides[i], tOtherPlayer)) {
			return 1;
		}
	}

	return 0;
}

int isIgnoredBecauseOfHitOverride(DreamPlayer* tPlayer, DreamPlayer * tOtherPlayer)
{
	if (getHitDataPlayer1StateNumber(tOtherPlayer) == -1) return 0;
	if (getHitDataPlayer2CapableOfGettingPlayer1State(tOtherPlayer) != 1) return 0;

	return hasMatchingHitOverride(tPlayer, tOtherPlayer);
}

void getMatchingHitOverrideStateNoAndForceAir(DreamPlayer * tPlayer, DreamPlayer * tOtherPlayer, int * oStateNo, int * oDoesForceAir)
{
	assert(int_map_contains(&gData.mHitOverrideMap, tPlayer->mHitDataID));
	PlayerHitOverrides* overrides = (PlayerHitOverrides*)int_map_get(&gData.mHitOverrideMap, tPlayer->mHitDataID);

	int i;
	for (i = 0; i < 8; i++) {
		if (overrideEqualsPlayerHitDef(&overrides->mHitOverrides[i], tOtherPlayer)) {
			*oStateNo = overrides->mHitOverrides[i].mStateNo;
			*oDoesForceAir = overrides->mHitOverrides[i].mDoesForceAir;
			return;
		}
	}

	logWarningFormat("Unable to find matching hit override for player %d and otherPlayer %d. Defaulting to state 0 and no air forcing.", tPlayer->mRootID, tOtherPlayer->mRootID);
	*oStateNo = 0;
	*oDoesForceAir = 0;
}


