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

void updatePlayerHitData(DreamPlayer* tPlayer)
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
	for (i = 0; i < 8; i++) {
		tPlayer->mHitOverrides.mHitOverrides[i].mAttackClassTypePairs.clear();
		tPlayer->mHitOverrides.mHitOverrides[i].mIsActive = 0;
	}
}

void setPlayerHitDataCoordinateP(DreamPlayer* tPlayer)
{
	tPlayer->mPassiveHitData.mCoordinateP = getPlayerCoordinateP(tPlayer);
	tPlayer->mActiveHitData.mCoordinateP = getPlayerCoordinateP(tPlayer);
}

void clearPlayerHitData(DreamPlayer* tPlayer)
{
	initPlayerHitData(tPlayer);
}

void copyHitDataToActive(DreamPlayer* tPlayer, void * tHitData)
{
	setProfilingSectionMarkerCurrentFunction();
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

int isActiveHitDataActive(DreamPlayer* tPlayer)
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
	e->mReversalDef.mIsActive = 0;
}

void setActiveHitDataInactive(DreamPlayer* tPlayer)
{
	if (!isGeneralPlayer(tPlayer)) return;
	
	PlayerHitData* e = &tPlayer->mActiveHitData;
	e->mIsActive = 0;
}

int getActiveHitDataCoordinateP(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return e->mCoordinateP;
}

void * getPlayerHitDataReference(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	return e;
}

DreamPlayer* getReceivedHitDataPlayer(void * tHitData)
{
	PlayerHitData* passive = (PlayerHitData*)tHitData;

	return passive->mPlayer;
}

DreamMugenStateType getHitDataType(DreamPlayer* tPlayer)
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

MugenAttackClass getHitDataAttackClass(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	return e->mAttackClass;
}

MugenAttackClass getActiveHitDataAttackClass(DreamPlayer* tPlayer)
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

MugenAttackType getActiveHitDataAttackType(DreamPlayer* tPlayer)
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

char* getActiveHitDataGuardFlag(DreamPlayer* tPlayer)
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

MugenAffectTeam getHitDataAffectTeam(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	return e->mAffectTeam;
}

void setHitDataAffectTeam(DreamPlayer* tPlayer, MugenAffectTeam tAffectTeam)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mAffectTeam = tAffectTeam;
}

MugenHitAnimationType getActiveHitDataAnimationType(DreamPlayer* tPlayer)
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

int getActiveHitDataDamage(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	if (!e->mIsActive) return 0;
	else return e->mDamage;
}

int getActiveHitDataGuardDamage(DreamPlayer* tPlayer)
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

int getActiveHitDataPlayer1PauseTime(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return e->mPlayer1PauseTime;
}

int getActiveHitDataPlayer1GuardPauseTime(DreamPlayer* tPlayer)
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

int getActiveHitDataPlayer2PauseTime(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return e->mPlayer2ShakeTime;
}

int getActiveHitDataPlayer2GuardPauseTime(DreamPlayer* tPlayer)
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

int isActiveHitDataSparkInPlayerFile(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return e->mIsSparkInPlayerFile;
}

int isActiveHitDataGuardSparkInPlayerFile(DreamPlayer* tPlayer)
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

int getActiveHitDataSparkNumber(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return e->mSparkNumber;
}

int getActiveHitDataGuardSparkNumber(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return e->mGuardSparkNumber;
}

int getHitDataGuardSparkNumber(DreamPlayer* tPlayer)
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

Position2D getActiveHitDataSparkXY(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return transformDreamCoordinatesVector2D(e->mSparkOffset, getActiveHitDataCoordinateP(tPlayer), getPlayerCoordinateP(tPlayer));
}

Position2D getHitDataSparkXY(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	return e->mSparkOffset;
}

void setHitDataSparkXY(DreamPlayer* tPlayer, int tX, int tY)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mSparkOffset = Vector2D(tX, tY);
}

void getActiveHitDataHitSound(DreamPlayer* tPlayer, int* oIsInPlayerFile, Vector2DI* oSound)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	*oIsInPlayerFile = e->mIsHitSoundInPlayerFile;
	*oSound = Vector2DI(e->mHitSound.mGroup, e->mHitSound.mItem);
}

void setHitDataHitSound(DreamPlayer* tPlayer, int tIsInPlayerFile, int tGroup, int tItem)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mIsHitSoundInPlayerFile = tIsInPlayerFile;
	e->mHitSound = makeDreamMugenSound(tGroup, tItem);
}

void getActiveHitDataGuardSound(DreamPlayer* tPlayer, int* oIsInPlayerFile, Vector2DI* oSound)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	*oIsInPlayerFile = e->mIsGuardSoundInPlayerFile;
	*oSound = Vector2DI(e->mGuardSound.mGroup, e->mGuardSound.mItem);
}

void setHitDataGuardSound(DreamPlayer* tPlayer, int tIsInPlayerFile, int tGroup, int tItem)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mIsGuardSoundInPlayerFile = tIsInPlayerFile;
	e->mGuardSound = makeDreamMugenSound(tGroup, tItem);
}

MugenAttackHeight getActiveHitDataGroundType(DreamPlayer* tPlayer)
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

MugenAttackHeight getActiveHitDataAirType(DreamPlayer* tPlayer)
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

int getActiveHitDataGroundHitTime(DreamPlayer* tPlayer)
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

int getActiveHitDataGroundSlideTime(DreamPlayer* tPlayer)
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

int getActiveHitDataGuardHitTime(DreamPlayer* tPlayer)
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

int getActiveHitDataGuardSlideTime(DreamPlayer* tPlayer)
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

int getActiveHitDataAirHitTime(DreamPlayer* tPlayer)
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

int getActiveHitDataGuardControlTime(DreamPlayer* tPlayer)
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

double getActiveHitDataYAccel(DreamPlayer* tPlayer)
{
	if (isActiveHitDataActive(tPlayer)) {
		assert(isGeneralPlayer(tPlayer));
		PlayerHitData* e = &tPlayer->mActiveHitData;
		return transformDreamCoordinates(e->mVerticalAcceleration, getActiveHitDataCoordinateP(tPlayer), getPlayerCoordinateP(tPlayer));
	}
	else {
		return transformDreamCoordinates(0.7, 640, getPlayerCoordinateP(tPlayer));
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

double getActiveHitDataGroundVelocityX(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return transformDreamCoordinates(e->mGroundVelocity.x, getActiveHitDataCoordinateP(tPlayer), getPlayerCoordinateP(tPlayer));
}

double getHitDataGroundVelocityX(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	return e->mGroundVelocity.x;
}

double getActiveHitDataGroundVelocityY(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return transformDreamCoordinates(e->mGroundVelocity.y, getActiveHitDataCoordinateP(tPlayer), getPlayerCoordinateP(tPlayer));
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
	e->mGroundVelocity = Vector2D(tX, tY);
}

double getActiveHitDataGuardVelocity(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return transformDreamCoordinates(e->mGuardVelocity, getActiveHitDataCoordinateP(tPlayer), getPlayerCoordinateP(tPlayer));
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

double getActiveHitDataAirVelocityX(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return transformDreamCoordinates(e->mAirVelocity.x, getActiveHitDataCoordinateP(tPlayer), getPlayerCoordinateP(tPlayer));
}

double getHitDataAirVelocityX(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	return e->mAirVelocity.x;
}

double getActiveHitDataAirVelocityY(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return transformDreamCoordinates(e->mAirVelocity.y, getActiveHitDataCoordinateP(tPlayer), getPlayerCoordinateP(tPlayer));
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
	e->mAirVelocity = Vector2D(tX, tY);
}

void setHitDataAirGuardVelocity(DreamPlayer* tPlayer, double tX, double tY)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mAirGuardVelocity = Vector2D(tX, tY);
}

double getActiveGroundCornerPushVelocityOffset(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return transformDreamCoordinates(e->mGroundCornerPushVelocityOffset, getActiveHitDataCoordinateP(tPlayer), getPlayerCoordinateP(tPlayer));
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

double getActiveAirCornerPushVelocityOffset(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return transformDreamCoordinates(e->mAirCornerPushVelocityOffset, getActiveHitDataCoordinateP(tPlayer), getPlayerCoordinateP(tPlayer));
}

void setAirCornerPushVelocityOffset(DreamPlayer* tPlayer, double tX)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mAirCornerPushVelocityOffset = tX;
}

double getActiveDownCornerPushVelocityOffset(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return transformDreamCoordinates(e->mDownCornerPushVelocityOffset, getActiveHitDataCoordinateP(tPlayer), getPlayerCoordinateP(tPlayer));
}

void setDownCornerPushVelocityOffset(DreamPlayer* tPlayer, double tX)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mDownCornerPushVelocityOffset = tX;
}

double getActiveGuardCornerPushVelocityOffset(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return transformDreamCoordinates(e->mGuardCornerPushVelocityOffset, getActiveHitDataCoordinateP(tPlayer), getPlayerCoordinateP(tPlayer));
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

double getActiveAirGuardCornerPushVelocityOffset(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return transformDreamCoordinates(e->mAirGuardCornerPushVelocityOffset, getActiveHitDataCoordinateP(tPlayer), getPlayerCoordinateP(tPlayer));
}

void setAirGuardCornerPushVelocityOffset(DreamPlayer* tPlayer, double tX)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mAirGuardCornerPushVelocityOffset = tX;
}

int getActiveHitDataAirGuardControlTime(DreamPlayer* tPlayer)
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

int getActiveHitDataHasMinimumDistance(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return e->mHasMinimumDistance;
}

Vector2DI getActiveHitDataMinimumDistance(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return transformDreamCoordinatesVector2DI(e->mMinimumDistance, getActiveHitDataCoordinateP(tPlayer), getPlayerCoordinateP(tPlayer));
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
	e->mMinimumDistance = Vector2DI(x, y);
}

int getActiveHitDataHasMaximumDistance(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return e->mHasMaximumDistance;
}

Vector2DI getActiveHitDataMaximumDistance(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return transformDreamCoordinatesVector2DI(e->mMaximumDistance, getActiveHitDataCoordinateP(tPlayer), getPlayerCoordinateP(tPlayer));
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
	e->mMaximumDistance = Vector2DI(x, y);
}

int getActiveHitDataHasSnap(DreamPlayer* tPlayer) {
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return e->mHasSnap;
}

Vector2DI getActiveHitDataSnap(DreamPlayer* tPlayer) {
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return transformDreamCoordinatesVector2DI(e->mSnap, getActiveHitDataCoordinateP(tPlayer), getPlayerCoordinateP(tPlayer));
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
	e->mSnap = Vector2DI(x, y);
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

int getActiveHitDataPlayer2ChangeFaceDirectionRelativeToPlayer1(DreamPlayer* tPlayer)
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

int getHitDataPlayer1StateNumber(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	return e->mPlayer1StateNumber;
}

int getActiveHitDataPlayer1StateNumber(DreamPlayer* tPlayer)
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

int getActiveHitDataPlayer2StateNumber(DreamPlayer* tPlayer)
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

int getHitDataPlayer2CapableOfGettingPlayer1State(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	return e->mCanPlayer2GetPlayer1State;
}

int getActiveHitDataPlayer2CapableOfGettingPlayer1State(DreamPlayer* tPlayer)
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

int getActiveHitDataForceStanding(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return e->mIsForcingPlayer2ToStandingPosition;
}

void setHitDataForceStanding(DreamPlayer* tPlayer, int tIsForcedToStand)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mIsForcingPlayer2ToStandingPosition = tIsForcedToStand;
}

int getActiveHitDataFall(DreamPlayer* tPlayer)
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

void setActiveHitDataFall(DreamPlayer* tPlayer, int tIsCausingPlayer2ToFall)
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

double getActiveHitDataFallXVelocity(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return transformDreamCoordinates(e->mFallVelocity.x, getActiveHitDataCoordinateP(tPlayer), getPlayerCoordinateP(tPlayer));
}

double getHitDataFallXVelocity(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	return transformDreamCoordinates(e->mFallVelocity.x, getActiveHitDataCoordinateP(tPlayer), getPlayerCoordinateP(tPlayer));
}

void setActiveHitDataFallXVelocity(DreamPlayer* tPlayer, double tX, int tCoordinateP)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	e->mFallVelocity.x = transformDreamCoordinates(tX, tCoordinateP, getActiveHitDataCoordinateP(tPlayer));
}

void setHitDataFallXVelocity(DreamPlayer* tPlayer, double tX)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mFallVelocity.x = tX;
}

double getActiveHitDataFallYVelocity(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return transformDreamCoordinates(e->mFallVelocity.y, getActiveHitDataCoordinateP(tPlayer), getPlayerCoordinateP(tPlayer));
}

double getHitDataFallYVelocity(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	return e->mFallVelocity.y;
}

void setActiveHitDataFallYVelocity(DreamPlayer* tPlayer, double tY, int tCoordinateP)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	e->mFallVelocity.y = transformDreamCoordinates(tY, tCoordinateP, getActiveHitDataCoordinateP(tPlayer));
}

void setHitDataFallYVelocity(DreamPlayer* tPlayer, double tY)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mFallVelocity.y = tY;
}

int getActiveHitDataFallRecovery(DreamPlayer* tPlayer)
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

int getActiveHitDataFallRecoveryTime(DreamPlayer* tPlayer)
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

int getActiveHitDataFallDamage(DreamPlayer* tPlayer)
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

int getActiveHitDataAirFall(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return e->mAirFall;
}

void setActiveHitDataAirFall(DreamPlayer* tPlayer, int tIsCausingPlayer2ToFall)
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

int getActiveHitDataForceNoFall(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return e->mForcePlayer2OutOfFallState;
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
	e->mDownVelocity = Vector2D(tX, tY);
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

int getActiveHitDataHitID(DreamPlayer* tPlayer)
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

int getActiveHitDataChainID(DreamPlayer* tPlayer)
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
	e->mNoChainIDs = Vector2DI(tID1, tID2);
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

int getActiveHitDataFallKill(DreamPlayer* tPlayer)
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

int getActiveHitDataPlayer1PowerAdded(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return e->mGetPlayer1Power;
}

int getActiveHitDataPlayer1GuardPowerAdded(DreamPlayer* tPlayer)
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

int getActiveHitDataPlayer2PowerAdded(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return e->mGivePlayer2Power;
}

int getActiveHitDataPlayer2GuardPowerAdded(DreamPlayer* tPlayer)
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

int getActiveHitDataPaletteEffectTime(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return e->mPaletteEffectTime;
}

void setHitDataPaletteEffectTime(DreamPlayer* tPlayer, int tEffectTime)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mPaletteEffectTime = tEffectTime;
}

Vector3D getActiveHitDataPaletteEffectMultiplication(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return e->mPaletteEffectMultiplication;
}

void setHitDataPaletteEffectMultiplication(DreamPlayer* tPlayer, int tR, int tG, int tB)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mPaletteEffectMultiplication = Vector3DI(tR, tG, tB) / 256.0;
}

Vector3D getActiveHitDataPaletteEffectAddition(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return e->mPaletteEffectAddition;
}

void setHitDataPaletteEffectAddition(DreamPlayer* tPlayer, int tR, int tG, int tB)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mPaletteEffectAddition = Vector3DI(tR, tG, tB) / 256.0;
}

int getActiveHitDataEnvironmentShakeTime(DreamPlayer* tPlayer)
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

double getActiveHitDataEnvironmentShakeFrequency(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return e->mEnvironmentShakeFrequency;
}

double getHitDataEnvironmentShakeFrequency(DreamPlayer* tPlayer)
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

int getActiveHitDataEnvironmentShakeAmplitude(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return transformDreamCoordinatesI(e->mEnvironmentShakeAmplitude, getActiveHitDataCoordinateP(tPlayer), getPlayerCoordinateP(tPlayer));
}

int getHitDataEnvironmentShakeAmplitude(DreamPlayer* tPlayer)
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

double getActiveHitDataEnvironmentShakePhase(DreamPlayer* tPlayer)
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

int getActiveHitDataFallEnvironmentShakeTime(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return e->mFallEnvironmentShakeTime;
}

void setActiveHitDataFallEnvironmentShakeTime(DreamPlayer* tPlayer, int tTime)
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

double getActiveHitDataFallEnvironmentShakeFrequency(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return e->mFallEnvironmentShakeFrequency;
}

double getHitDataFallEnvironmentShakeFrequency(DreamPlayer* tPlayer)
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

int getActiveHitDataFallEnvironmentShakeAmplitude(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return transformDreamCoordinatesI(e->mFallEnvironmentShakeAmplitude, getActiveHitDataCoordinateP(tPlayer), getPlayerCoordinateP(tPlayer));
}

void setHitDataFallEnvironmentShakeAmplitude(DreamPlayer* tPlayer, int tAmplitude)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mFallEnvironmentShakeAmplitude = tAmplitude;
}

double getActiveHitDataFallEnvironmentShakePhase(DreamPlayer* tPlayer)
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

double getActiveHitDataVelocityX(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return transformDreamCoordinates(e->mVelocity.x, getActiveHitDataCoordinateP(tPlayer), getPlayerCoordinateP(tPlayer));
}

double getHitDataVelocityX(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	return e->mVelocity.x;
}

void setActiveHitDataVelocityX(DreamPlayer* tPlayer, double x, int tCoordinateP)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	e->mVelocity.x = transformDreamCoordinates(x, tCoordinateP, getActiveHitDataCoordinateP(tPlayer));
}

void setHitDataVelocityX(DreamPlayer* tPlayer, double x)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mVelocity.x = x;
}

double getActiveHitDataVelocityY(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return transformDreamCoordinates(e->mVelocity.y, getActiveHitDataCoordinateP(tPlayer), getPlayerCoordinateP(tPlayer));
}

double getHitDataVelocityY(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	return e->mVelocity.y;
}

void setActiveHitDataVelocityY(DreamPlayer* tPlayer, double y, int tCoordinateP)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	e->mVelocity.y = transformDreamCoordinates(y, tCoordinateP, getActiveHitDataCoordinateP(tPlayer));
}

void setHitDataVelocityY(DreamPlayer* tPlayer, double y)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mVelocity.y = y;
}

int getActiveHitDataIsFacingRight(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mActiveHitData;
	return e->mIsFacingRight;
}

void setHitDataIsFacingRight(DreamPlayer* tPlayer, int tIsFacingRight)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mIsFacingRight = tIsFacingRight;
}

int isHitDataReversalDefActive(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	return e->mReversalDef.mIsActive;
}

void setHitDataReversalDefActive(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mReversalDef.mReversalAttribute.mFlag2.clear();
	e->mReversalDef.mReversalAttribute.mIsActive = 1;
	e->mReversalDef.mReversalAttribute.mIsHitBy = 1;
	e->mReversalDef.mIsActive = 1;
}

DreamHitDefAttributeSlot * getHitDataReversalDefReversalAttribute(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	return &e->mReversalDef.mReversalAttribute;
}

void setHitDataReversalDefFlag1(DreamPlayer* tPlayer, const char* tFlag)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mReversalDef.mReversalAttribute.mFlag1 = copyOverCleanHitDefAttributeFlag(tFlag);
}

void addHitDataReversalDefFlag2(DreamPlayer* tPlayer, const char* tFlag)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;

	std::string nFlag = copyOverCleanHitDefAttributeFlag(tFlag);
	if (nFlag.size() != 2) {
		logWarningFormat("Unparseable reversal definition flag: %s. Ignore.", nFlag.c_str());
		return;
	}
	turnStringLowercase(nFlag);

	e->mReversalDef.mReversalAttribute.mFlag2.push_back(nFlag);
}

int getReversalDefPlayer1PauseTime(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	return e->mReversalDef.mPlayer1PauseTime;
}

int getReversalDefPlayer2PauseTime(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	return e->mReversalDef.mPlayer2PauseTime;
}

void setReversalDefPauseTime(DreamPlayer* tPlayer, int tPlayer1PauseTime, int tPlayer2PauseTime)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mReversalDef.mPlayer1PauseTime = tPlayer1PauseTime;
	e->mReversalDef.mPlayer2PauseTime = tPlayer2PauseTime;
}

int isReversalDefSparkInPlayerFile(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	return e->mReversalDef.mIsSparkInPlayerFile;
}

int getReversalDefSparkNumber(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	return e->mReversalDef.mSparkNumber;
}

void setReversalDefSparkNumber(DreamPlayer* tPlayer, int tIsInPlayerFile, int tNumber)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mReversalDef.mIsSparkInPlayerFile = tIsInPlayerFile;
	e->mReversalDef.mSparkNumber = tNumber;
}

Vector2DI getReversalDefSparkXY(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	return e->mReversalDef.mSparkXY;
}

void setReversalDefSparkXY(DreamPlayer* tPlayer, int tX, int tY)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mReversalDef.mSparkXY = Vector2DI(tX, tY);
}

void getReversalDefHitSound(DreamPlayer* tPlayer, int* oIsInPlayerFile, Vector2DI* oHitSound)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	*oIsInPlayerFile = e->mReversalDef.mIsHitSoundInPlayerFile;
	*oHitSound = Vector2DI(e->mReversalDef.mHitSound.mGroup, e->mReversalDef.mHitSound.mItem);
}

void setReversalDefHitSound(DreamPlayer* tPlayer, int tIsInPlayerFile, int tGroup, int tItem)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mReversalDef.mIsHitSoundInPlayerFile = tIsInPlayerFile;
	e->mReversalDef.mHitSound.mGroup = tGroup;
	e->mReversalDef.mHitSound.mItem = tItem;
}

int hasReversalDefP1StateNo(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	return e->mReversalDef.mHasPlayer1StateNumber;
}

int getReversalDefP1StateNo(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	return e->mReversalDef.mPlayer1StateNumber;
}

void setReversalDefP1StateNo(DreamPlayer* tPlayer, int hasP1StateNo, int p1StateNo)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mReversalDef.mHasPlayer1StateNumber = hasP1StateNo;
	e->mReversalDef.mPlayer1StateNumber = p1StateNo;
}

int hasReversalDefP2StateNo(DreamPlayer* tPlayer)
{
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	return e->mReversalDef.mHasPlayer2StateNumber;
}

int getReversalDefP2StateNo(DreamPlayer* tPlayer)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	return e->mReversalDef.mPlayer2StateNumber;
}

void setReversalDefP2StateNo(DreamPlayer* tPlayer, int hasP2StateNo, int p2StateNo)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitData* e = &tPlayer->mPassiveHitData;
	e->mReversalDef.mHasPlayer2StateNumber = hasP2StateNo;
	e->mReversalDef.mPlayer2StateNumber = p2StateNo;
}

void setPlayerHitOverride(DreamPlayer* tPlayer, DreamMugenStateTypeFlags tStateTypeFlags, const std::vector<std::pair<MugenAttackClassFlags, MugenAttackType>>& tAttackClassTypePairs, int tStateNo, int tSlot, int tDuration, int tDoesForceAir)
{
	assert(isGeneralPlayer(tPlayer));
	PlayerHitOverrides* overrides = &tPlayer->mHitOverrides;

	HitOverride* e = &overrides->mHitOverrides[tSlot];
	e->mStateTypeFlags = tStateTypeFlags;
	e->mAttackClassTypePairs = tAttackClassTypePairs;
	e->mStateNo = tStateNo;
	e->mSlot = tSlot;
	e->mDoesForceAir = tDoesForceAir;

	e->mNow = 0;
	e->mDuration = tDuration;

	e->mIsActive = 1;
}

static int hasMatchingHitOverrideAttackClassTypePair(const std::vector<std::pair<MugenAttackClassFlags, MugenAttackType>>& tAttackClassTypePairs, MugenAttackClass tAttackClass, MugenAttackType tAttackType) {
	for (const auto& attackClassTypePair : tAttackClassTypePairs) {
		if (hasPrismFlag(attackClassTypePair.first, convertMugenAttackClassToFlag(tAttackClass)) && attackClassTypePair.second == tAttackType) return 1;
	}
	return 0;
}

int overrideEqualsPlayerHitDef(HitOverride* e, DreamPlayer* tPlayer) {
	if (!e->mIsActive) return 0;

	DreamMugenStateType stateType = getHitDataType(tPlayer);
	MugenAttackClass attackClass = getHitDataAttackClass(tPlayer);
	MugenAttackType attackType = getHitDataAttackType(tPlayer);

	if (!hasPrismFlag(e->mStateTypeFlags, convertDreamMugenStateTypeToFlag(stateType))) return 0;
	if (!hasMatchingHitOverrideAttackClassTypePair(e->mAttackClassTypePairs, attackClass, attackType)) return 0;

	return 1;
}

int hasMatchingHitOverride(DreamPlayer* tPlayer, DreamPlayer* tOtherPlayer) {
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

int isIgnoredBecauseOfHitOverride(DreamPlayer* tPlayer, DreamPlayer* tOtherPlayer)
{
	if (getHitDataPlayer1StateNumber(tOtherPlayer) == -1) return 0;
	if (getHitDataPlayer2CapableOfGettingPlayer1State(tOtherPlayer) != 1) return 0;

	return hasMatchingHitOverride(tPlayer, tOtherPlayer);
}

void getMatchingHitOverrideStateNoAndForceAir(DreamPlayer* tPlayer, DreamPlayer* tOtherPlayer, int* oStateNo, int* oDoesForceAir)
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

std::string copyOverCleanHitDefAttributeFlag(const char* tSrc) {
	int n = strlen(tSrc);

	std::string ret;
	int i;
	for (i = 0; i < n; i++) {
		if (tSrc[i] == ' ') continue;
		ret.push_back(tSrc[i]);
	}
	return ret;
}

MugenAttackClassFlags convertMugenAttackClassToFlag(MugenAttackClass tAttackClass)
{
	switch (tAttackClass) {
	case MUGEN_ATTACK_CLASS_NORMAL:
		return MUGEN_ATTACK_CLASS_NORMAL_FLAG;
	case MUGEN_ATTACK_CLASS_SPECIAL:
		return MUGEN_ATTACK_CLASS_SPECIAL_FLAG;
	case MUGEN_ATTACK_CLASS_HYPER:
		return MUGEN_ATTACK_CLASS_HYPER_FLAG;
	default:
		logWarningFormat("Unrecognized attack class %d. Defaulting to normal attack flag.", int(tAttackClass));
		return MUGEN_ATTACK_CLASS_NORMAL_FLAG;
	}
}
