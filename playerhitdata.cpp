#include "playerhitdata.h"

#include <assert.h>

#include <prism/datastructures.h>
#include <prism/log.h>
#include <prism/system.h>
#include <prism/stlutil.h>

#include "mugensound.h"
#include "stage.h"
#include "playerdefinition.h"

static void updateSingleOverride(HitOverride* e) {
	if (!e->mIsActive) return;
	if (e->mDuration == -1) return;

	e->mNow++;
	if (e->mNow >= e->mDuration) {
		e->mIsActive = 0;
	}
}

static void updateSinglePlayerOverrides(PlayerHitOverrides* tData) {
	int i;
	for (i = 0; i < 8; i++) {
		updateSingleOverride(&tData->mHitOverrides[i]);
	}
}

void updatePlayerHitData(DreamPlayer * tPlayer)
{
	updateSinglePlayerOverrides(&tPlayer->mHitOverrides);
}

void initPlayerHitData(DreamPlayer* tPlayer)
{
	tPlayer->mPassiveHitData.mIsActive = 0;
	tPlayer->mPassiveHitData.mPlayer = tPlayer;

	tPlayer->mActiveHitData.mIsActive = 0;
	tPlayer->mActiveHitData.mPlayer = tPlayer;

	int i;
	for (i = 0; i < 8; i++) tPlayer->mHitOverrides.mHitOverrides[i].mIsActive = 0;
}

void copyHitDataToActive(DreamPlayer* tPlayer, void * tHitData)
{
	PlayerHitData* passive = (PlayerHitData*)tHitData;
	assert(passive->mIsActive);
	PlayerHitData* active = &tPlayer->mActiveHitData;

	*active = *passive;
}

int isReceivedHitDataActive(void* tHitData)
{
	PlayerHitData* passive = (PlayerHitData*)tHitData;
	return passive->mIsActive && isGeneralPlayer(passive->mPlayer);
}

int isHitDataActive(DreamPlayer* tPlayer)
{
	if (!isGeneralPlayer(tPlayer)) return 0;
	
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	return e->mIsActive;
}

int isActiveHitDataActive(DreamPlayer * tPlayer)
{
	if(!isGeneralPlayer(tPlayer)) return 0;

	PlayerHitData* e = &tPlayer->mActiveHitData;
	return e->mIsActive;
}

void setHitDataActive(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
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
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mIsActive = 0;
}

void setActiveHitDataInactive(DreamPlayer * tPlayer)
{
	if (!isGeneralPlayer(tPlayer)) return;
	
	PlayerHitData* e = &tPlayer->mActiveHitData;
	e->mIsActive = 0;
}

void * getPlayerHitDataReference(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	return e;
}

DreamPlayer * getReceivedHitDataPlayer(void * tHitData)
{
	PlayerHitData* passive = (PlayerHitData*)tHitData;

	return passive->mPlayer;
}

DreamMugenStateType getHitDataType(DreamPlayer * tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	return e->mType;
}

void setHitDataType(DreamPlayer* tPlayer, DreamMugenStateType tType)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mType = tType;
}

MugenAttackClass getHitDataAttackClass(DreamPlayer * tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	return e->mAttackClass;
}

MugenAttackClass getActiveHitDataAttackClass(DreamPlayer * tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return e->mAttackClass;
}

void setHitDataAttackClass(DreamPlayer* tPlayer, MugenAttackClass tClass)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mAttackClass = tClass;
}

MugenAttackType getHitDataAttackType(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	return e->mAttackType;
}

MugenAttackType getActiveHitDataAttackType(DreamPlayer * tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return e->mAttackType;
}

void setHitDataAttackType(DreamPlayer* tPlayer, MugenAttackType tType)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mAttackType = tType;
}

void setHitDataHitFlag(DreamPlayer* tPlayer, const char * tFlag)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	assert(strlen(tFlag) < sizeof e->mHitFlag);
	strcpy(e->mHitFlag, tFlag);
}

char* getActiveHitDataGuardFlag(DreamPlayer * tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return e->mGuardFlag;
}

void setHitDataGuardFlag(DreamPlayer* tPlayer, const char * tFlag)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	assert(strlen(tFlag) < sizeof e->mGuardFlag);
	strcpy(e->mGuardFlag, tFlag);
}

void setHitDataAffectTeam(DreamPlayer* tPlayer, MugenAffectTeam tAffectTeam)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mAffectTeam = tAffectTeam;
}

MugenHitAnimationType getActiveHitDataAnimationType(DreamPlayer * tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return e->mAnimationType;
}

MugenHitAnimationType getHitDataAnimationType(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	return e->mAnimationType;
}

void setHitDataAnimationType(DreamPlayer* tPlayer, MugenHitAnimationType tAnimationType)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mAnimationType = tAnimationType;
}

MugenHitAnimationType getHitDataAirAnimationType(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	return e->mAirAnimationType;
}

void setHitDataAirAnimationType(DreamPlayer* tPlayer, MugenHitAnimationType tAnimationType)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mAirAnimationType = tAnimationType;
}

void setHitDataFallAnimationType(DreamPlayer* tPlayer, MugenHitAnimationType tAnimationType)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mFallAnimationType = tAnimationType;
}

void setHitDataPriority(DreamPlayer* tPlayer, int tPriority, MugenHitPriorityType tPriorityType)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mPriority = tPriority;
	e->mPriorityType = tPriorityType;
}

int getActiveHitDataDamage(DreamPlayer * tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	if (!e->mIsActive) return 0;
	else return e->mDamage;
}

int getActiveHitDataGuardDamage(DreamPlayer * tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return e->mGuardDamage;
}

int getHitDataDamage(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	return e->mDamage;
}

void setHitDataDamage(DreamPlayer* tPlayer, int tDamage, int tGuardDamage)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mDamage = tDamage;
	e->mGuardDamage = tGuardDamage;
}

int getActiveHitDataPlayer1PauseTime(DreamPlayer * tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return e->mPlayer1PauseTime;
}

int getActiveHitDataPlayer1GuardPauseTime(DreamPlayer * tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return e->mGuardPlayer1PauseTime;
}

int getHitDataPlayer1PauseTime(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	return e->mPlayer1PauseTime;
}

int getActiveHitDataPlayer2PauseTime(DreamPlayer * tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return e->mPlayer2ShakeTime;
}

int getActiveHitDataPlayer2GuardPauseTime(DreamPlayer * tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return e->mGuardPlayer2ShakeTime;
}

int getHitDataPlayer2PauseTime(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	return e->mPlayer2ShakeTime;
}

void setHitDataPauseTime(DreamPlayer* tPlayer, int tPlayer1PauseTime, int tPlayer2PauseTime)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mPlayer1PauseTime = tPlayer1PauseTime;
	e->mPlayer2ShakeTime = tPlayer2PauseTime;
}

void setHitDataGuardPauseTime(DreamPlayer* tPlayer, int tPlayer1PauseTime, int tPlayer2PauseTime)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mGuardPlayer1PauseTime = tPlayer1PauseTime;
	e->mGuardPlayer2ShakeTime = tPlayer2PauseTime;
}

int isActiveHitDataSparkInPlayerFile(DreamPlayer * tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return e->mIsSparkInPlayerFile;
}

int isActiveHitDataGuardSparkInPlayerFile(DreamPlayer * tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return e->mIsGuardSparkInPlayerFile;
}

int isHitDataSparkInPlayerFile(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	return e->mIsSparkInPlayerFile;
}

int getActiveHitDataSparkNumber(DreamPlayer * tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return e->mSparkNumber;
}

int getActiveHitDataGuardSparkNumber(DreamPlayer * tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return e->mGuardSparkNumber;
}

int getHitDataGuardSparkNumber(DreamPlayer * tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	return e->mGuardSparkNumber;
}

int getHitDataSparkNumber(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	return e->mSparkNumber;
}

void setHitDataSparkNumber(DreamPlayer* tPlayer, int tIsInPlayerFile, int tNumber)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mIsSparkInPlayerFile = tIsInPlayerFile;
	e->mSparkNumber = tNumber;
}

void setHitDataGuardSparkNumber(DreamPlayer* tPlayer, int tIsInPlayerFile, int tNumber)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mIsGuardSparkInPlayerFile = tIsInPlayerFile;
	e->mGuardSparkNumber = tNumber;
}

Position getActiveHitDataSparkXY(DreamPlayer * tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return e->mSparkOffset;
}

Position getHitDataSparkXY(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	return e->mSparkOffset;
}

void setHitDataSparkXY(DreamPlayer* tPlayer, int tX, int tY)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mSparkOffset = makePosition(tX, tY, 0);
}

void getActiveHitDataHitSound(DreamPlayer * tPlayer, int * oIsInPlayerFile, Vector3DI * oSound)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	*oIsInPlayerFile = e->mIsHitSoundInPlayerFile;
	*oSound = makeVector3DI(e->mHitSound.mGroup, e->mHitSound.mItem, 0);
}

void setHitDataHitSound(DreamPlayer* tPlayer, int tIsInPlayerFile, int tGroup, int tItem)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mIsHitSoundInPlayerFile = tIsInPlayerFile;
	e->mHitSound = makeDreamMugenSound(tGroup, tItem);
}

void getActiveHitDataGuardSound(DreamPlayer * tPlayer, int * oIsInPlayerFile, Vector3DI * oSound)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	*oIsInPlayerFile = e->mIsGuardSoundInPlayerFile;
	*oSound = makeVector3DI(e->mGuardSound.mGroup, e->mGuardSound.mItem, 0);
}

void setHitDataGuardSound(DreamPlayer* tPlayer, int tIsInPlayerFile, int tGroup, int tItem)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mIsGuardSoundInPlayerFile = tIsInPlayerFile;
	e->mGuardSound = makeDreamMugenSound(tGroup, tItem);
}

MugenAttackHeight getActiveHitDataGroundType(DreamPlayer * tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return e->mGroundType;
}

MugenAttackHeight getHitDataGroundType(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	return e->mGroundType;
}

void setHitDataGroundType(DreamPlayer* tPlayer, MugenAttackHeight tType)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mGroundType = tType;
}

MugenAttackHeight getActiveHitDataAirType(DreamPlayer * tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return e->mAirType;
}

MugenAttackHeight getHitDataAirType(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	return e->mAirType;
}

void setHitDataAirType(DreamPlayer* tPlayer, MugenAttackHeight tType)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mAirType = tType;
}

int getActiveHitDataGroundHitTime(DreamPlayer * tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return e->mGroundHitTime;
}

int getHitDataGroundHitTime(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	return e->mGroundHitTime;
}

void setHitDataGroundHitTime(DreamPlayer* tPlayer, int tHitTime)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mGroundHitTime = tHitTime;
}

int getActiveHitDataGroundSlideTime(DreamPlayer * tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return e->mGroundSlideTime;
}

int getHitDataGroundSlideTime(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	return e->mGroundSlideTime;
}

void setHitDataGroundSlideTime(DreamPlayer* tPlayer, int tSlideTime)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mGroundSlideTime = tSlideTime;
}

int getActiveHitDataGuardHitTime(DreamPlayer * tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return e->mGuardHitTime;
}

int getHitDataGuardHitTime(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	return e->mGuardHitTime;
}

void setHitDataGuardHitTime(DreamPlayer* tPlayer, int tHitTime)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mGuardHitTime = tHitTime;
}

int getActiveHitDataGuardSlideTime(DreamPlayer * tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return e->mGuardSlideTime;
}

int getHitDataGuardSlideTime(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	return e->mGuardSlideTime;
}

void setHitDataGuardSlideTime(DreamPlayer* tPlayer, int tSlideTime)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mGuardSlideTime = tSlideTime;
}

int getActiveHitDataAirHitTime(DreamPlayer * tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return e->mAirHitTime;
}

void setHitDataAirHitTime(DreamPlayer* tPlayer, int tHitTime)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mAirHitTime = tHitTime;
}

int getActiveHitDataGuardControlTime(DreamPlayer * tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return e->mGuardControlTime;
}

int getHitDataGuardControlTime(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	return e->mGuardControlTime;
}

void setHitDataGuardControlTime(DreamPlayer* tPlayer, int tControlTime)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mGuardControlTime = tControlTime;
}

int getHitDataGuardDistance(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	return e->mGuardDistance;
}

void setHitDataGuardDistance(DreamPlayer* tPlayer, int tDistance)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mGuardDistance = tDistance;
}

double getActiveHitDataYAccel(DreamPlayer * tPlayer)
{
	if (isActiveHitDataActive(tPlayer)) {
		assert(isGeneralPlayer(tPlayer));
		PlayerHitData* e = &tPlayer->mActiveHitData;
		return e->mVerticalAcceleration;
	}
	else {
		return transformDreamCoordinates(0.7, 480, getPlayerCoordinateP(tPlayer));
	}
}

double getHitDataYAccel(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	return e->mVerticalAcceleration;
}

void setHitDataYAccel(DreamPlayer* tPlayer, double YAccel)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mVerticalAcceleration = YAccel;
}

double getActiveHitDataGroundVelocityX(DreamPlayer * tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return e->mGroundVelocity.x;
}

double getHitDataGroundVelocityX(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	return e->mGroundVelocity.x;
}

double getActiveHitDataGroundVelocityY(DreamPlayer * tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return e->mGroundVelocity.y;
}

double getHitDataGroundVelocityY(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	return e->mGroundVelocity.y;
}

void setHitDataGroundVelocity(DreamPlayer* tPlayer, double tX, double tY)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mGroundVelocity = makePosition(tX, tY, 0);
}

double getActiveHitDataGuardVelocity(DreamPlayer * tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return e->mGuardVelocity;
}

double getHitDataGuardVelocity(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	return e->mGuardVelocity;
}

void setHitDataGuardVelocity(DreamPlayer* tPlayer, double tX)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mGuardVelocity = tX;
}

double getActiveHitDataAirVelocityX(DreamPlayer * tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return e->mAirVelocity.x;
}

double getHitDataAirVelocityX(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	return e->mAirVelocity.x;
}

double getActiveHitDataAirVelocityY(DreamPlayer * tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return e->mAirVelocity.y;
}

double getHitDataAirVelocityY(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	return e->mAirVelocity.y;
}

void setHitDataAirVelocity(DreamPlayer* tPlayer, double tX, double tY)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mAirVelocity = makePosition(tX, tY, 0);
}

void setHitDataAirGuardVelocity(DreamPlayer* tPlayer, double tX, double tY)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mAirGuardVelocity = makePosition(tX, tY, 0);
}

double getActiveGroundCornerPushVelocityOffset(DreamPlayer * tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return e->mGroundCornerPushVelocityOffset;
}

double getGroundCornerPushVelocityOffset(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	return e->mGroundCornerPushVelocityOffset;
}

void setGroundCornerPushVelocityOffset(DreamPlayer* tPlayer, double tX)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mGroundCornerPushVelocityOffset = tX;
}

double getActiveAirCornerPushVelocityOffset(DreamPlayer * tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return e->mAirCornerPushVelocityOffset;
}

void setAirCornerPushVelocityOffset(DreamPlayer* tPlayer, double tX)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mAirCornerPushVelocityOffset = tX;
}

double getActiveDownCornerPushVelocityOffset(DreamPlayer * tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return e->mDownCornerPushVelocityOffset;
}

void setDownCornerPushVelocityOffset(DreamPlayer* tPlayer, double tX)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mDownCornerPushVelocityOffset = tX;
}

double getActiveGuardCornerPushVelocityOffset(DreamPlayer * tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return e->mGuardCornerPushVelocityOffset;
}

double getGuardCornerPushVelocityOffset(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	return e->mGuardCornerPushVelocityOffset;
}

void setGuardCornerPushVelocityOffset(DreamPlayer* tPlayer, double tX)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mGuardCornerPushVelocityOffset = tX;
}

double getActiveAirGuardCornerPushVelocityOffset(DreamPlayer * tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return e->mAirGuardCornerPushVelocityOffset;
}

void setAirGuardCornerPushVelocityOffset(DreamPlayer* tPlayer, double tX)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mAirGuardCornerPushVelocityOffset = tX;
}

int getActiveHitDataAirGuardControlTime(DreamPlayer * tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return e->mAirGuardControlTime;
}

void setHitDataAirGuardControlTime(DreamPlayer* tPlayer, int tControlTime)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mAirGuardControlTime = tControlTime;
}

void setHitDataAirJuggle(DreamPlayer* tPlayer, int tJuggle)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mAirJugglePoints = tJuggle;
}

void setHitDataMinimumDistanceInactive(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mHasMinimumDistance = 0;
}

void setHitDataMinimumDistance(DreamPlayer* tPlayer, int x, int y)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mHasMinimumDistance = 1;
	e->mMinimumDistance = makeVector3DI(x, y, 0);
}

void setHitDataMaximumDistanceInactive(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mHasMaximumDistance = 0;
}

void setHitDataMaximumDistance(DreamPlayer* tPlayer, int x, int y)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mHasMaximumDistance = 1;
	e->mMaximumDistance = makeVector3DI(x, y, 0);
}

void setHitDataSnapInactive(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mHasSnap = 0;
}

void setHitDataSnap(DreamPlayer* tPlayer, int x, int y)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mHasSnap = 1;
	e->mSnap = makeVector3DI(x, y, 0);
}

void setHitDataPlayer1SpritePriority(DreamPlayer* tPlayer, int tPriority)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mPlayer1DrawingPriority = tPriority;
}

void setHitDataPlayer2SpritePriority(DreamPlayer* tPlayer, int tPriority)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mPlayer2DrawingPriority = tPriority;
}

void setHitDataPlayer1FaceDirection(DreamPlayer* tPlayer, int tFaceDirection)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mIsPlayer1TurningAround = tFaceDirection == -1;
}

void setHitDataPlayer1ChangeFaceDirectionRelativeToPlayer2(DreamPlayer* tPlayer, int tFaceDirection)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mPlayer1ChangeFaceDirectionRelativeToPlayer2 = tFaceDirection;
}

int getActiveHitDataPlayer2ChangeFaceDirectionRelativeToPlayer1(DreamPlayer * tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return e->mPlayer2ChangeFaceDirectionRelativeToPlayer1;
}

void setHitDataPlayer2ChangeFaceDirectionRelativeToPlayer1(DreamPlayer* tPlayer, int tFaceDirection)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mPlayer2ChangeFaceDirectionRelativeToPlayer1 = tFaceDirection;
}

int getHitDataPlayer1StateNumber(DreamPlayer * tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	return e->mPlayer1StateNumber;
}

int getActiveHitDataPlayer1StateNumber(DreamPlayer * tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return e->mPlayer1StateNumber;
}

void setPlayer1StateNumber(DreamPlayer* tPlayer, int tStateNumber)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mPlayer1StateNumber = tStateNumber;
}

int getActiveHitDataPlayer2StateNumber(DreamPlayer * tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return e->mPlayer2StateNumber;
}

void setPlayer2StateNumber(DreamPlayer* tPlayer, int tStateNumber)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mPlayer2StateNumber = tStateNumber;
}

int getHitDataPlayer2CapableOfGettingPlayer1State(DreamPlayer * tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	return e->mCanPlayer2GetPlayer1State;
}

int getActiveHitDataPlayer2CapableOfGettingPlayer1State(DreamPlayer * tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return e->mCanPlayer2GetPlayer1State;
}

void setHitDataPlayer2CapableOfGettingPlayer1State(DreamPlayer* tPlayer, int tVal)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mCanPlayer2GetPlayer1State = tVal;
}

void setHitDataForceStanding(DreamPlayer* tPlayer, int tIsForcedToStand)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mIsForcingPlayer2ToStandingPosition = tIsForcedToStand;
}

int getActiveHitDataFall(DreamPlayer * tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return e->mFall;
}

int getHitDataFall(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	return e->mFall;
}

void setActiveHitDataFall(DreamPlayer * tPlayer, int tIsCausingPlayer2ToFall)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	e->mFall = tIsCausingPlayer2ToFall;
}

void setHitDataFall(DreamPlayer* tPlayer, int tIsCausingPlayer2ToFall)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mFall = tIsCausingPlayer2ToFall;
}

double getActiveHitDataFallXVelocity(DreamPlayer * tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return e->mFallVelocity.x;
}

double getHitDataFallXVelocity(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	return e->mFallVelocity.x;
}

void setActiveHitDataFallXVelocity(DreamPlayer * tPlayer, double tX)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	e->mFallVelocity.x = tX;
}

void setHitDataFallXVelocity(DreamPlayer* tPlayer, double tX)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mFallVelocity.x = tX;
}

double getActiveHitDataFallYVelocity(DreamPlayer * tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return e->mFallVelocity.y;
}

double getHitDataFallYVelocity(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	return e->mFallVelocity.y;
}

void setActiveHitDataFallYVelocity(DreamPlayer * tPlayer, double tY)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	e->mFallVelocity.y = tY;
}

void setHitDataFallYVelocity(DreamPlayer* tPlayer, double tY)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mFallVelocity.y = tY;
}

int getActiveHitDataFallRecovery(DreamPlayer * tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return e->mCanRecoverFall;
}

int getHitDataFallRecovery(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	return e->mCanRecoverFall;
}

void setHitDataFallRecovery(DreamPlayer* tPlayer, int tCanRecover)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mCanRecoverFall = tCanRecover;
}

int getActiveHitDataFallRecoveryTime(DreamPlayer * tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return e->mFallRecoveryTime;
}

void setHitDataFallRecoveryTime(DreamPlayer* tPlayer, int tRecoverTime)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mFallRecoveryTime = tRecoverTime;
}

int getActiveHitDataFallDamage(DreamPlayer * tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return e->mFallDamage;
}

void setHitDataFallDamage(DreamPlayer* tPlayer, int tDamage)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mFallDamage = tDamage;
}

int getActiveHitDataAirFall(DreamPlayer * tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return e->mAirFall;
}

void setActiveHitDataAirFall(DreamPlayer * tPlayer, int tIsCausingPlayer2ToFall)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	e->mAirFall = tIsCausingPlayer2ToFall;
}

void setHitDataAirFall(DreamPlayer* tPlayer, int tIsCausingPlayer2ToFall)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mAirFall = tIsCausingPlayer2ToFall;
}

void setHitDataForceNoFall(DreamPlayer* tPlayer, int tForcePlayer2NotToFall)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mForcePlayer2OutOfFallState = tForcePlayer2NotToFall;
}

void setHitDataDownVelocity(DreamPlayer* tPlayer, double tX, double tY)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mDownVelocity = makePosition(tX, tY, 0);
}

void setHitDataDownHitTime(DreamPlayer* tPlayer, int tHitTime)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mDownHitTime = tHitTime;
}

void setHitDataDownBounce(DreamPlayer* tPlayer, int tDoesBounce)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mDownDoesBounce = tDoesBounce;
}

int getActiveHitDataHitID(DreamPlayer * tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return e->mHitID;
}

void setHitDataHitID(DreamPlayer* tPlayer, int tID)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mHitID = tID;
}

int getActiveHitDataChainID(DreamPlayer * tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return e->mChainID;
}

void setHitDataChainID(DreamPlayer* tPlayer, int tID)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mChainID = tID;
}

void setHitDataNoChainID(DreamPlayer* tPlayer, int tID1, int tID2)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mNoChainIDs = makeVector3DI(tID1, tID2, -1);
}

void setHitDataHitOnce(DreamPlayer* tPlayer, int tIsOnlyAffectingOneOpponent)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mDoesOnlyHitOneEnemy = tIsOnlyAffectingOneOpponent;
}

void setHitDataKill(DreamPlayer* tPlayer, int tCanKill)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mCanKill = tCanKill;
}

void setHitDataGuardKill(DreamPlayer* tPlayer, int tCanKill)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mCanGuardKill = tCanKill;
}

int getActiveHitDataFallKill(DreamPlayer * tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return e->mCanFallKill;
}

void setHitDataFallKill(DreamPlayer* tPlayer, int tCanKill)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mCanFallKill = tCanKill;
}

void setHitDataNumberOfHits(DreamPlayer* tPlayer, int tNumberOfHits)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mNumberOfHitsForComboCounter = tNumberOfHits;
}

int getActiveHitDataPlayer1PowerAdded(DreamPlayer * tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return e->mGetPlayer1Power;
}

int getActiveHitDataPlayer1GuardPowerAdded(DreamPlayer * tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return e->mGetPlayer1GuardPower;
}

int getHitDataPlayer1PowerAdded(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	return e->mGetPlayer1Power;
}

void setHitDataGetPower(DreamPlayer* tPlayer, int tPlayer1PowerAdded, int tPlayer1PowerAddedWhenGuarded)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mGetPlayer1Power = tPlayer1PowerAdded;
	e->mGetPlayer1GuardPower = tPlayer1PowerAddedWhenGuarded;
}

int getActiveHitDataPlayer2PowerAdded(DreamPlayer * tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return e->mGivePlayer2Power;
}

int getActiveHitDataPlayer2GuardPowerAdded(DreamPlayer * tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return e->mGivePlayer2GuardPower;
}

int getHitDataPlayer2PowerAdded(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	return e->mGivePlayer2Power;
}

void setHitDataGivePower(DreamPlayer* tPlayer, int tPlayer2PowerAdded, int tPlayer2PowerAddedWhenGuarded)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mGivePlayer2Power = tPlayer2PowerAdded;
	e->mGivePlayer2GuardPower = tPlayer2PowerAddedWhenGuarded;
}

void setHitDataPaletteEffectTime(DreamPlayer* tPlayer, int tEffectTime)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mPaletteEffectTime = tEffectTime;
}

void setHitDataPaletteEffectMultiplication(DreamPlayer* tPlayer, int tR, int tG, int tB)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mPaletteEffectMultiplication = makeVector3DI(tR, tG, tB);
}

void setHitDataPaletteEffectAddition(DreamPlayer* tPlayer, int tR, int tG, int tB)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mPaletteEffectAddition = makeVector3DI(tR, tG, tB);
}

int getActiveHitDataEnvironmentShakeTime(DreamPlayer * tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return e->mEnvironmentShakeTime;
}

void setHitDataEnvironmentShakeTime(DreamPlayer* tPlayer, int tTime)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mEnvironmentShakeTime = tTime;
}

double getActiveHitDataEnvironmentShakeFrequency(DreamPlayer * tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return e->mEnvironmentShakeFrequency;
}

double getHitDataEnvironmentShakeFrequency(DreamPlayer * tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	return e->mEnvironmentShakeFrequency;
}

void setHitDataEnvironmentShakeFrequency(DreamPlayer* tPlayer, double tFrequency)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mEnvironmentShakeFrequency = tFrequency;
}

int getActiveHitDataEnvironmentShakeAmplitude(DreamPlayer * tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return e->mEnvironmentShakeAmplitude;
}

int getHitDataEnvironmentShakeAmplitude(DreamPlayer * tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	return e->mEnvironmentShakeAmplitude;
}

void setHitDataEnvironmentShakeAmplitude(DreamPlayer* tPlayer, int tAmplitude)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mEnvironmentShakeAmplitude = tAmplitude;
}

double getActiveHitDataEnvironmentShakePhase(DreamPlayer * tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return e->mEnvironmentShakePhase;
}

void setHitDataEnvironmentShakePhase(DreamPlayer* tPlayer, double tPhase)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mEnvironmentShakePhase = tPhase;
}

int getActiveHitDataFallEnvironmentShakeTime(DreamPlayer * tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return e->mFallEnvironmentShakeTime;
}

void setActiveHitDataFallEnvironmentShakeTime(DreamPlayer * tPlayer, int tTime)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	e->mFallEnvironmentShakeTime = tTime;
}

void setHitDataFallEnvironmentShakeTime(DreamPlayer* tPlayer, int tTime)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mFallEnvironmentShakeTime = tTime;
}

double getActiveHitDataFallEnvironmentShakeFrequency(DreamPlayer * tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return e->mFallEnvironmentShakeFrequency;
}

double getHitDataFallEnvironmentShakeFrequency(DreamPlayer * tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	return e->mFallEnvironmentShakeFrequency;
}

void setHitDataFallEnvironmentShakeFrequency(DreamPlayer* tPlayer, double tFrequency)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mFallEnvironmentShakeFrequency = tFrequency;
}

int getActiveHitDataFallEnvironmentShakeAmplitude(DreamPlayer * tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return e->mFallEnvironmentShakeAmplitude;
}

void setHitDataFallEnvironmentShakeAmplitude(DreamPlayer* tPlayer, int tAmplitude)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mFallEnvironmentShakeAmplitude = tAmplitude;
}

double getActiveHitDataFallEnvironmentShakePhase(DreamPlayer * tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return e->mFallEnvironmentShakePhase;
}

void setHitDataFallEnvironmentShakePhase(DreamPlayer* tPlayer, double tPhase)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mFallEnvironmentShakePhase = tPhase;
}

double getActiveHitDataVelocityX(DreamPlayer * tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return e->mVelocity.x;
}

double getHitDataVelocityX(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	return e->mVelocity.x;
}

void setActiveHitDataVelocityX(DreamPlayer * tPlayer, double x)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	e->mVelocity.x = x;
}

void setHitDataVelocityX(DreamPlayer* tPlayer, double x)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mVelocity.x = x;
}

double getActiveHitDataVelocityY(DreamPlayer * tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return e->mVelocity.y;
}

double getHitDataVelocityY(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	return e->mVelocity.y;
}

void setActiveHitDataVelocityY(DreamPlayer * tPlayer, double y)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	e->mVelocity.y = y;
}

void setHitDataVelocityY(DreamPlayer* tPlayer, double y)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mVelocity.y = y;
}

int getActiveHitDataIsFacingRight(DreamPlayer * tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return e->mIsFacingRight;
}

void setHitDataIsFacingRight(DreamPlayer * tPlayer, int tIsFacingRight)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mIsFacingRight = tIsFacingRight;
}

void resetHitDataReversalDef(DreamPlayer * tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mReversalDef.mIsActive = 1;
	e->mReversalDef.mFlag2Amount = 0;
}

void setHitDataReversalDefFlag1(DreamPlayer * tPlayer, char * tFlag)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
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
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;

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
	assert(isGeneralPlayer(tPlayer));
	PlayerHitOverrides* overrides = &tPlayer->mHitOverrides;

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
	assert(isGeneralPlayer(tPlayer));
	PlayerHitOverrides* overrides = &tPlayer->mHitOverrides;

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
	assert(isGeneralPlayer(tPlayer));
	PlayerHitOverrides* overrides = &tPlayer->mHitOverrides;

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


