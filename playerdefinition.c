#include "playerdefinition.h"

#include <assert.h>
#include <string.h>

#include <prism/file.h>
#include <prism/physicshandler.h>
#include <prism/log.h>
#include <prism/system.h>
#include <prism/timer.h>
#include <prism/math.h>
#include <prism/mugendefreader.h>
#include <prism/mugenanimationreader.h>
#include <prism/mugenanimationhandler.h>

#include "mugencommandreader.h"
#include "mugenstatereader.h"
#include "mugencommandhandler.h"
#include "mugenstatehandler.h"
#include "playerhitdata.h"
#include "projectile.h"
#include "mugenlog.h"

#include "stage.h"
#include "fightui.h"
#include "mugenstagehandler.h"
#include "gamelogic.h"
#include "ai.h"
#include "collision.h"
#include "mugensound.h"
#include "mugenanimationutilities.h"

// TODO: move to proper place
#define PLAYER_Z 10

static struct {
	DreamPlayer mPlayers[2];
	int mUniqueIDCounter;
	int mIsInTrainingMode;
} gData;

static void loadPlayerHeaderFromScript(DreamPlayerHeader* tHeader, MugenDefScript* tScript) {
	getMugenDefStringOrDefault(tHeader->mName, tScript, "Info", "name", "Character");
	getMugenDefStringOrDefault(tHeader->mDisplayName, tScript, "Info", "displayname", tHeader->mName);
	getMugenDefStringOrDefault(tHeader->mVersion, tScript, "Info", "versiondate", "09,09,2017");
	getMugenDefStringOrDefault(tHeader->mMugenVersion, tScript, "Info", "mugenversion", "1.1");
	getMugenDefStringOrDefault(tHeader->mAuthor, tScript, "Info", "author", "John Doe");
	getMugenDefStringOrDefault(tHeader->mPaletteDefaults, tScript, "Info", "pal.defaults", "1");

	tHeader->mLocalCoordinates = getMugenDefVectorIOrDefault(tScript, "Info", "localcoord", makeVector3DI(320, 240, 0));
}

static void loadOptionalStateFiles(MugenDefScript* tScript, char* tPath, DreamPlayer* tPlayer) {
	char file[200];
	char scriptPath[1024];
	char name[100];

	int i;
	for (i = 1; i < 100; i++) {
		sprintf(name, "st%d", i);
		getMugenDefStringOrDefault(file, tScript, "Files", name, "");
		sprintf(scriptPath, "%s%s", tPath, file);
		if (!isFile(scriptPath)) continue;
		
		loadDreamMugenStateDefinitionsFromFile(&tPlayer->mConstants.mStates, scriptPath);
		malloc_stats();
	}

	
}

static void setPlayerFaceDirection(DreamPlayer* p, FaceDirection tDirection);

static void setPlayerExternalDependencies(DreamPlayer* tPlayer) {
	tPlayer->mPhysicsID = addToPhysicsHandler(getDreamPlayerStartingPosition(tPlayer->mRootID, tPlayer->mHeader.mLocalCoordinates.y));
	setPlayerPhysics(tPlayer, MUGEN_STATE_PHYSICS_STANDING);
	setPlayerStateMoveType(tPlayer, MUGEN_STATE_MOVE_TYPE_IDLE);
	setPlayerStateType(tPlayer, MUGEN_STATE_TYPE_STANDING);

	Position p = getDreamStageCoordinateSystemOffset(getPlayerCoordinateP(tPlayer));
	p.z = PLAYER_Z;
	tPlayer->mAnimationID = addMugenAnimation(getMugenAnimation(&tPlayer->mAnimations, 0), &tPlayer->mSprites, p);
	setMugenAnimationBasePosition(tPlayer->mAnimationID, getHandledPhysicsPositionReference(tPlayer->mPhysicsID));
	setMugenAnimationCameraPositionReference(tPlayer->mAnimationID, getDreamMugenStageHandlerCameraPositionReference());
	setMugenAnimationAttackCollisionActive(tPlayer->mAnimationID, getDreamPlayerAttackCollisionList(tPlayer), NULL, NULL, getPlayerHitDataReference(tPlayer));
	setMugenAnimationPassiveCollisionActive(tPlayer->mAnimationID, getDreamPlayerPassiveCollisionList(tPlayer), playerHitCB, tPlayer, getPlayerHitDataReference(tPlayer));
	tPlayer->mStateMachineID = registerDreamMugenStateMachine(&tPlayer->mConstants.mStates, tPlayer); 
}


static void loadPlayerFiles(char* tPath, DreamPlayer* tPlayer, MugenDefScript* tScript) {
	char file[200];
	char path[1024];
	char scriptPath[1024];
	char name[100];
	getPathToFile(path, tPath);


	getMugenDefStringOrDefault(file, tScript, "Files", "cns", "");
	assert(strcmp("", file));
	sprintf(scriptPath, "%s%s", path, file);
	tPlayer->mConstants = loadDreamMugenConstantsFile(scriptPath);
	malloc_stats();

	
	getMugenDefStringOrDefault(file, tScript, "Files", "stcommon", "");
	sprintf(scriptPath, "%s%s", path, file);
	if (isFile(scriptPath)) {
		loadDreamMugenStateDefinitionsFromFile(&tPlayer->mConstants.mStates, scriptPath);
	}
	else {
		sprintf(scriptPath, "assets/data/%s", file);
		if (isFile(scriptPath)) {
			loadDreamMugenStateDefinitionsFromFile(&tPlayer->mConstants.mStates, scriptPath);
		}
	}
	malloc_stats();

	getMugenDefStringOrDefault(file, tScript, "Files", "st", "");
	sprintf(scriptPath, "%s%s", path, file);
	if (isFile(scriptPath)) {
		loadDreamMugenStateDefinitionsFromFile(&tPlayer->mConstants.mStates, scriptPath);
	}
	malloc_stats();	

	loadOptionalStateFiles(tScript, path, tPlayer);

	getMugenDefStringOrDefault(file, tScript, "Files", "cmd", "");
	assert(strcmp("", file));
	sprintf(scriptPath, "%s%s", path, file);
	tPlayer->mCommands = loadDreamMugenCommandFile(scriptPath);
	loadDreamMugenStateDefinitionsFromFile(&tPlayer->mConstants.mStates, scriptPath);
	malloc_stats();


	getMugenDefStringOrDefault(file, tScript, "Files", "anim", "");
	assert(strcmp("", file));
	sprintf(scriptPath, "%s%s", path, file);
	tPlayer->mAnimations = loadMugenAnimationFile(scriptPath);
	malloc_stats();


	char palettePath[1024];
	int preferredPalette = tPlayer->mPreferredPalette;
	sprintf(name, "pal%d", preferredPalette);
	getMugenDefStringOrDefault(file, tScript, "Files", name, "");
	int hasPalettePath = strcmp("", file);
	sprintf(palettePath, "%s%s", path, file);
	getMugenDefStringOrDefault(file, tScript, "Files", "sprite", "");
	assert(strcmp("", file));
	sprintf(scriptPath, "%s%s", path, file);

	setMugenSpriteFileReaderToUsePalette(tPlayer->mRootID);
	tPlayer->mSprites = loadMugenSpriteFile(scriptPath, preferredPalette, hasPalettePath, palettePath);
	setMugenSpriteFileReaderToNotUsePalette();
	malloc_stats();

	getMugenDefStringOrDefault(file, tScript, "Files", "sound", "");
	if (strcmp("", file)) {
		sprintf(scriptPath, "%s%s", path, file);
		tPlayer->mSounds = loadMugenSoundFile(scriptPath);
	}
	else {
		tPlayer->mSounds = createEmptyMugenSoundFile();
	}
	malloc_stats();

	setPlayerExternalDependencies(tPlayer);
	tPlayer->mCommandID = registerDreamMugenCommands(tPlayer, &tPlayer->mCommands);
	malloc_stats();

	if (getPlayerAILevel(tPlayer)) {
		setDreamAIActive(tPlayer);
	}

	if (doesDreamPlayerStartFacingLeft(tPlayer->mRootID)) {
		setPlayerFaceDirection(tPlayer, FACE_DIRECTION_LEFT);
	}
	else {
		setPlayerFaceDirection(tPlayer, FACE_DIRECTION_RIGHT);
	}
}

static void initHitDefAttributeSlot(DreamHitDefAttributeSlot* tSlot) {
	tSlot->mIsActive = 0;
	tSlot->mNow = 0;
}

static void resetHelperState(DreamPlayer* p) {
	p->mHelpers = new_list();
	p->mProjectiles = new_int_map();

	p->mNoWalkFlag = 0;
	p->mNoAutoTurnFlag = 0;
	p->mNoLandFlag = 0;
	p->mPushDisabledFlag = 0;
	p->mNoAirGuardFlag = 0;
	p->mNoCrouchGuardFlag = 0;
	p->mNoStandGuardFlag = 0;
	p->mNoKOSoundFlag = 0;
	p->mNoKOSlowdownFlag = 0;
	p->mUnguardableFlag = 0;
	p->mTransparencyFlag = 0;

	p->mJumpFlank = 0;
	p->mAirJumpCounter = 0;

	p->mIsHitOver = 1;
	p->mIsHitShakeOver = 1;
	p->mIsFalling = 0;
	p->mCanRecoverFromFall = 0;

	p->mDefenseMultiplier = 1;

	p->mIsFrozen = 0;

	p->mIsLyingDown = 0;
	p->mIsHitPaused = 0;
	p->mIsSuperPaused = 0;
	p->mMoveContactCounter = 0;

	p->mHitCount = 0;
	p->mFallAmountInCombo = 0;

	p->mAttackMultiplier = 1;

	p->mHitDataID = initPlayerHitDataAndReturnID(p);

	p->mFaceDirection = FACE_DIRECTION_RIGHT;

	p->mHasMoveBeenReversed = 0;
	p->mMoveHit = 0;
	p->mMoveGuarded = 0;

	p->mRoundsExisted = 0;

	p->mIsBound = 0;
	p->mBoundHelpers = new_list();

	p->mComboCounter = 0;
	p->mIsDestroyed = 0;

	int i;
	for (i = 0; i < 2; i++) {
		initHitDefAttributeSlot(&p->mNotHitBy[i]);
		p->mDustClouds[i].mLastDustTime = 0;
	}
}

static void loadPlayerState(DreamPlayer* p) {
	memset(p->mVars, 0, sizeof p->mVars);
	memset(p->mSystemVars, 0, sizeof p->mSystemVars);
	memset(p->mFloatVars, 0, sizeof p->mFloatVars);
	memset(p->mSystemFloatVars, 0, sizeof p->mSystemFloatVars);
	
	p->mID = 0;

	p->mIsInControl = 1;
	p->mIsAlive = 1;
	p->mRoundsWon = 0;

	resetHelperState(p);
	p->mIsHelper = 0;
	p->mParent = NULL;
	p->mHelperIDInParent = -1;

	p->mIsProjectile = 0;
	p->mProjectileID = -1;
	p->mProjectileDataID = -1;

	p->mPower = 0; 
	
	p->mIsBoundToScreen = 1;
}

static void loadPlayerStateWithConstantsLoaded(DreamPlayer* p) {
	p->mLife = p->mConstants.mHeader.mLife;
	setPlayerDrawOffsetX(p, 0, getPlayerCoordinateP(p));
	setPlayerDrawOffsetY(p, 0, getPlayerCoordinateP(p));
}

static void loadPlayerShadow(DreamPlayer* p) {
	Position pos = getDreamStageCoordinateSystemOffset(getPlayerCoordinateP(p));
	pos.z = PLAYER_Z - 2;
	p->mShadow.mShadowPosition = *getHandledPhysicsPositionReference(p->mPhysicsID);
	p->mShadow.mAnimationID = addMugenAnimation(getMugenAnimation(&p->mAnimations, getMugenAnimationAnimationNumber(p->mAnimationID)), &p->mSprites, pos);
	setMugenAnimationBasePosition(p->mShadow.mAnimationID, &p->mShadow.mShadowPosition);
	setMugenAnimationCameraPositionReference(p->mShadow.mAnimationID, getDreamMugenStageHandlerCameraPositionReference());
	setMugenAnimationDrawScale(p->mShadow.mAnimationID, makePosition(1, -getDreamStageShadowScaleY(), 1));
	Vector3D color = getDreamStageShadowColor();
	(void)color; // TODO: proper shadow color
	setMugenAnimationColor(p->mShadow.mAnimationID, 0, 0, 0); // TODO: proper shadow color
	setMugenAnimationTransparency(p->mShadow.mAnimationID, getDreamStageShadowTransparency());
	setMugenAnimationFaceDirection(p->mShadow.mAnimationID, getMugenAnimationIsFacingRight(p->mAnimationID));
}

static void loadPlayerReflection(DreamPlayer* p) {
	Position pos = getDreamStageCoordinateSystemOffset(getPlayerCoordinateP(p));
	pos.z = PLAYER_Z - 1.9;
	p->mReflection.mPosition = *getHandledPhysicsPositionReference(p->mPhysicsID);
	p->mReflection.mAnimationID = addMugenAnimation(getMugenAnimation(&p->mAnimations, getMugenAnimationAnimationNumber(p->mAnimationID)), &p->mSprites, pos);

	setMugenAnimationBasePosition(p->mReflection.mAnimationID, &p->mReflection.mPosition);
	setMugenAnimationCameraPositionReference(p->mReflection.mAnimationID, getDreamMugenStageHandlerCameraPositionReference());
	setMugenAnimationDrawScale(p->mReflection.mAnimationID, makePosition(1, -1, 1));
	setMugenAnimationBlendType(p->mReflection.mAnimationID, BLEND_TYPE_ADDITION);
	setMugenAnimationTransparency(p->mReflection.mAnimationID, getDreamStageReflectionTransparency());
	setMugenAnimationFaceDirection(p->mReflection.mAnimationID, getMugenAnimationIsFacingRight(p->mAnimationID));
}

static void loadSinglePlayerFromMugenDefinition(DreamPlayer* p)
{
	MugenDefScript script = loadMugenDefScript(p->mDefinitionPath);

	loadPlayerState(p);
	loadPlayerHeaderFromScript(&p->mHeader, &script);
	loadPlayerFiles(p->mDefinitionPath, p, &script);
	loadPlayerStateWithConstantsLoaded(p);
	loadPlayerShadow(p);
	loadPlayerReflection(p);
	unloadMugenDefScript(script);
	

}

void loadPlayers() {
	int i = 0;
	for (i = 0; i < 2; i++) {
		gData.mPlayers[i].mRoot = &gData.mPlayers[i];
		gData.mPlayers[i].mOtherPlayer = &gData.mPlayers[i ^ 1];
		gData.mPlayers[i].mRootID = i;
		gData.mPlayers[i].mControllerID = i; // TODO: remove
		loadSinglePlayerFromMugenDefinition(&gData.mPlayers[i]);
	}
}

static int unloadSingleHelper(void* tCaller, void* tData) {
	(void)tCaller;
	DreamPlayer* p = tData;
	// TODO
	return 1;
}

static int unloadSingleProjectile(void* tCaller, void* tData) {
	(void)tCaller;
	DreamPlayer* p = tData;
	// TODO
	return 1;
}

// TODO: make sure child players are moved
static void unloadHelperStateWithoutFreeingOwnedHelpersAndProjectile(DreamPlayer* p) {
	delete_list(&p->mHelpers);
	delete_int_map(&p->mProjectiles);

	delete_list(&p->mBoundHelpers);
}

static void unloadHelperState(DreamPlayer* p) {
	list_remove_predicate(&p->mHelpers, unloadSingleHelper, NULL);
	delete_list(&p->mHelpers);
	int_map_remove_predicate(&p->mProjectiles, unloadSingleProjectile, NULL);
	delete_int_map(&p->mProjectiles);

	delete_list(&p->mBoundHelpers);
}

static void unloadPlayerState(DreamPlayer* p) {
	unloadHelperState(p);
}

static void unloadPlayerFiles(DreamPlayer* tPlayer) {
	unloadDreamMugenConstantsFile(&tPlayer->mConstants);
	unloadDreamMugenCommandFile(&tPlayer->mCommands);
	unloadMugenAnimationFile(&tPlayer->mAnimations);
	unloadMugenSpriteFile(&tPlayer->mSprites);
	unloadMugenSoundFile(&tPlayer->mSounds);

}

static void unloadSinglePlayer(DreamPlayer* p) {
	unloadPlayerState(p);
	unloadPlayerFiles(p);
}

void unloadPlayers() {
	int i;
	for (i = 0; i < 2; i++) {
		unloadSinglePlayer(&gData.mPlayers[i]);
	}
}


static void resetSinglePlayer(DreamPlayer* p) {
	p->mIsAlive = 1;

	p->mLife = p->mConstants.mHeader.mLife;
	setDreamLifeBarPercentage(p, 1);

	setPlayerPosition(p, getDreamPlayerStartingPosition(p->mRootID, getPlayerCoordinateP(p)), getPlayerCoordinateP(p));
	
	if (doesDreamPlayerStartFacingLeft(p->mRootID)) {
		setPlayerFaceDirection(p, FACE_DIRECTION_LEFT);
	}
	else {
		setPlayerFaceDirection(p, FACE_DIRECTION_RIGHT);
	}
}

void resetPlayers()
{
	resetSinglePlayer(&gData.mPlayers[0]);
	resetSinglePlayer(&gData.mPlayers[1]);
}

static void resetSinglePlayerEntirely(DreamPlayer* p) {
	p->mRoundsWon = 0;
	p->mRoundsExisted = 0;
	p->mPower = 0;
	setDreamPowerBarPercentage(p, 0, p->mPower);
}

void resetPlayersEntirely()
{
	resetPlayers();
	resetSinglePlayerEntirely(&gData.mPlayers[0]);
	resetSinglePlayerEntirely(&gData.mPlayers[1]);
}



static void updateWalking(DreamPlayer* p) {

	if (p->mNoWalkFlag) {
		p->mNoWalkFlag = 0;
		return;
	}

	if (!p->mIsInControl) return;

	if (getPlayerStateType(p) != MUGEN_STATE_TYPE_STANDING) return;

	if (isPlayerCommandActive(p, "holdfwd") || isPlayerCommandActive(p, "holdback")) {
		if (getPlayerState(p) != 20) {
			changePlayerState(p, 20);
		}
	}
	else if (getPlayerState(p) == 20) {
		changePlayerState(p, 0);
	}
}

static void updateAirJumping(DreamPlayer* p) {
	if (!p->mIsInControl) return;
	if (getPlayerStateType(p) != MUGEN_STATE_TYPE_AIR) return;

	int hasJumpLeft = p->mAirJumpCounter < p->mConstants.mMovementData.mAllowedAirJumpAmount;
	int hasMinimumHeight = -getPlayerPositionY(p, getPlayerCoordinateP(p)) >= p->mConstants.mMovementData.mAirJumpMinimumHeight;

	if (isPlayerCommandActive(p, "holdup") && p->mJumpFlank && hasJumpLeft && hasMinimumHeight) { // TODO: find something better than flank
		p->mJumpFlank = 0;
		p->mAirJumpCounter++;
		changePlayerState(p, 45);
	}
}

static void updateJumpFlank(DreamPlayer* p) {
	if (p->mJumpFlank) return;
	p->mJumpFlank |= !isPlayerCommandActive(p, "holdup");
}

static void updateJumping(DreamPlayer* p) {
	if (!p->mIsInControl) return;
	if (getPlayerStateType(p) != MUGEN_STATE_TYPE_STANDING) return;


	if (isPlayerCommandActive(p, "holdup")) {
		changePlayerState(p, 40);
	}
}

static void updateLanding(DreamPlayer* p) {
	if (p->mNoLandFlag) {
		p->mNoLandFlag = 0;
		return;
	}

	Position pos = *getHandledPhysicsPositionReference(p->mPhysicsID);
	if (getPlayerPhysics(p) != MUGEN_STATE_PHYSICS_AIR) return;
	if (isPlayerPaused(p)) return;


	Velocity vel = *getHandledPhysicsVelocityReference(p->mPhysicsID);
	if (pos.y >= 0 && vel.y >= 0) {
		setPlayerControl(p, 1);
		changePlayerState(p, 52);
	}
}

static void updateCrouchingDown(DreamPlayer* p) {
	if (!p->mIsInControl) return;
	if (getPlayerStateType(p) != MUGEN_STATE_TYPE_STANDING) return;


	if (isPlayerCommandActive(p, "holddown")) {
		changePlayerState(p, 10);
	}
}

static void updateStandingUp(DreamPlayer* p) {
	if (!p->mIsInControl) return;
	if (getPlayerStateType(p) != MUGEN_STATE_TYPE_CROUCHING) return;


	if (!isPlayerCommandActive(p, "holddown")) {
		changePlayerState(p, 12);
	}
}

static void setPlayerFaceDirection(DreamPlayer* p, FaceDirection tDirection) {
	if (p->mFaceDirection == tDirection) return;

	setMugenAnimationFaceDirection(p->mAnimationID, tDirection == FACE_DIRECTION_RIGHT);

	if (!p->mIsHelper) {
		setDreamMugenCommandFaceDirection(p->mCommandID, tDirection);
	}

	p->mFaceDirection = tDirection;
}

static void updateAutoTurn(DreamPlayer* p) {
	if (p->mNoAutoTurnFlag) {
		p->mNoAutoTurnFlag = 0;
		return;
	}

	if (!p->mIsInControl) return;

	if (getPlayerStateType(p) == MUGEN_STATE_TYPE_AIR) return;

	DreamPlayer* p2 = getPlayerOtherPlayer(p);

	double x1 = getHandledPhysicsPositionReference(p->mPhysicsID)->x;
	double x2 = getHandledPhysicsPositionReference(p2->mPhysicsID)->x;

	if (x1 > x2) setPlayerFaceDirection(p, FACE_DIRECTION_LEFT);
	else if (x1 < x2) setPlayerFaceDirection(p, FACE_DIRECTION_RIGHT);
}

static void updatePositionFreeze(DreamPlayer* p) {
	if (p->mIsFrozen) {
		Position* pos = getHandledPhysicsPositionReference(p->mPhysicsID);
		*pos = p->mFreezePosition;
		p->mIsFrozen = 0;
	}
}

static void updateGettingUp(DreamPlayer* p) {
	if (getPlayerState(p) != 5110 && !p->mIsLyingDown) return;

	if (!p->mIsLyingDown) {
		increasePlayerFallAmountInCombo(p);
		p->mLyingDownTime = 0;
		p->mIsLyingDown = 1;
		return;
	}

	if (p->mIsLyingDown && getPlayerState(p) != 5110) {
		p->mIsLyingDown = 0;
		return;
	}

	if (handleDurationAndCheckIfOver(&p->mLyingDownTime, p->mConstants.mHeader.mLiedownTime)) {
		p->mIsLyingDown = 0;
		changePlayerState(p, 5120);
	}

}

static void updateHitPause(DreamPlayer* p) {
	if (!p->mIsHitPaused) return;

	if (handleDurationAndCheckIfOver(&p->mHitPauseNow, p->mHitPauseDuration)) {
		setPlayerUnHitPaused(p);
	}
}

static void updateBindingPosition(DreamPlayer* p) {
	DreamPlayer* bindRoot = p->mBoundTarget;
	Position pos = getPlayerPosition(bindRoot, getPlayerCoordinateP(p));

	if (p->mBoundPositionType == PLAYER_BIND_POSITION_TYPE_HEAD) {
		pos = vecAdd(pos, getPlayerHeadPosition(bindRoot));
	}
	else if (p->mBoundPositionType == PLAYER_BIND_POSITION_TYPE_MID) {
		pos = vecAdd(pos, getPlayerMiddlePosition(bindRoot));
	}

	if (getPlayerIsFacingRight(bindRoot)) {
		pos = vecAdd(pos, p->mBoundOffset);
	}
	else {
		pos = vecAdd(pos, makePosition(-p->mBoundOffset.x, p->mBoundOffset.y, 0));
	}

	setPlayerPosition(p, pos, getPlayerCoordinateP(p));

	if (p->mBoundFaceSet) {
		if (p->mBoundFaceSet == 1) setPlayerIsFacingRight(p, getPlayerIsFacingRight(bindRoot));
		else setPlayerIsFacingRight(p, !getPlayerIsFacingRight(bindRoot));
	}
}

static void removePlayerBinding(DreamPlayer* tPlayer) {
	tPlayer->mIsBound = 0;

	DreamPlayer* boundTo = tPlayer->mBoundTarget;
	list_remove(&boundTo->mBoundHelpers, tPlayer->mBoundID);
}

static void updateBinding(DreamPlayer* p) {
	if (!p->mIsBound) return;
	if (isPlayerPaused(p)) return;

	p->mBoundNow++;
	if (p->mBoundNow >= p->mBoundDuration) {
		removePlayerBinding(p);
		return;
	}

	updateBindingPosition(p);
}

static int isPlayerGuarding(DreamPlayer* p) {
	(void)p;
	return getPlayerState(p) >= 120 && getPlayerState(p) <= 155; // TODO: properly
}


static int doesFlagPreventGuarding(DreamPlayer* p) {
	if (getPlayerStateType(p) == MUGEN_STATE_TYPE_STANDING && p->mNoStandGuardFlag) return 1;
	if (getPlayerStateType(p) == MUGEN_STATE_TYPE_CROUCHING && p->mNoCrouchGuardFlag) return 1;
	if (getPlayerStateType(p) == MUGEN_STATE_TYPE_AIR && p->mNoAirGuardFlag) return 1;

	return 0;
}

static void updateGuarding(DreamPlayer* p) {
	if (isPlayerPaused(p)) return;
	if (!p->mIsInControl) return;
	if (isPlayerGuarding(p)) {
		return;
	}
	if (doesFlagPreventGuarding(p)) return;

	if (isPlayerCommandActive(p, "holdback") && isPlayerBeingAttacked(p) && isPlayerInGuardDistance(p)) {
		changePlayerState(p, 120);
	}
}

static void setPlayerUnguarding(DreamPlayer* p) {
	if (getPlayerStateType(p) == MUGEN_STATE_TYPE_STANDING) {
		changePlayerState(p, 0);
	}
	else if (getPlayerStateType(p) == MUGEN_STATE_TYPE_CROUCHING) {
		changePlayerState(p, 11);
	}
	else if (getPlayerStateType(p) == MUGEN_STATE_TYPE_AIR) {
		changePlayerState(p, 51);
	}
	else {
		logWarningFormat("Unknown player state %d. Defaulting to state 0.", getPlayerStateType(p));
		changePlayerState(p, 0);
	}
}

static void updateGuardingOver(DreamPlayer* p) {
	if (isPlayerPaused(p)) return;
	if (!p->mIsInControl) return;
	if (getPlayerState(p) != 140) return;
	if (getRemainingPlayerAnimationTime(p)) return;


	setPlayerUnguarding(p);
}

static void updateGuardingFlags(DreamPlayer* p) {
	if (isPlayerGuarding(p) && doesFlagPreventGuarding(p)) {
		setPlayerUnguarding(p);
	}

	p->mNoAirGuardFlag = 0;
	p->mNoCrouchGuardFlag = 0;
	p->mNoStandGuardFlag = 0;
	p->mUnguardableFlag = 0;
}

static int updateSinglePlayer(DreamPlayer* p);

static int updateSinglePlayerCB(void* tCaller, void* tData) {
	(void)tCaller;
	DreamPlayer* p = tData;
	return updateSinglePlayer(p);
}

static void updateSingleHitAttributeSlot(DreamHitDefAttributeSlot* tSlot) {
	if (!tSlot->mIsActive) return;

	tSlot->mNow++;
	if (tSlot->mNow >= tSlot->mTime) {
		tSlot->mIsActive = 0;
	}
}

static void updateHitAttributeSlots(DreamPlayer* p) {
	if (isPlayerPaused(p)) return;

	int i;
	for (i = 0; i < 2; i++) {
		updateSingleHitAttributeSlot(&p->mNotHitBy[i]);
	}
}

static void updatePush(DreamPlayer* p) {
	if (isPlayerPaused(p)) return;
	if (isPlayerHelper(p)) return;
	if (!getPlayerControl(p)) return;
	if (p->mPushDisabledFlag) return;

	DreamPlayer* otherPlayer = getPlayerOtherPlayer(p);
	if (otherPlayer->mPushDisabledFlag) return;

	double frontX1 = getPlayerFrontX(p);
	double frontX2 = getPlayerFrontX(otherPlayer);

	double x1 = getPlayerPositionX(p, getPlayerCoordinateP(p));
	double x2 = getPlayerPositionX(otherPlayer, getPlayerCoordinateP(p));
	
	double distX = getPlayerDistanceToFrontOfOtherPlayerX(p);
	double distY = getPlayerAxisDistanceY(p);

	if (distY >= 0 && distY >= otherPlayer->mConstants.mSizeData.mHeight) return;
	if (distY <= 0 && distY <= -p->mConstants.mSizeData.mHeight) return;

	if (x1 < x2) {
		if (frontX1 > frontX2) {
			setPlayerPositionX(p, x1 - distX / 2, getPlayerCoordinateP(p));
			setPlayerPositionX(otherPlayer, x2 + distX / 2, getPlayerCoordinateP(p));
		}
	}
	else {
		if (frontX1 < frontX2) {
			setPlayerPositionX(p, x1 + distX / 2, getPlayerCoordinateP(p));
			setPlayerPositionX(otherPlayer, x2 - distX / 2, getPlayerCoordinateP(p));
		}
	}

}

static void updatePushFlags() {
	getRootPlayer(0)->mPushDisabledFlag = 0;
	getRootPlayer(1)->mPushDisabledFlag = 0;
}

static void updateStageBorder(DreamPlayer* p) {
	if (!p->mIsBoundToScreen) return;

	double left = getDreamStageLeftOfScreenBasedOnPlayer(getPlayerCoordinateP(p));
	double right = getDreamStageRightOfScreenBasedOnPlayer(getPlayerCoordinateP(p));
	// int lx = getDreamStageLeftEdgeMinimumPlayerDistance(getPlayerCoordinateP(p));
	// int rx = getDreamStageRightEdgeMinimumPlayerDistance(getPlayerCoordinateP(p));
	double x = getPlayerPositionX(p, getPlayerCoordinateP(p));

	if (x < left) {
		setPlayerPositionX(p, left, getPlayerCoordinateP(p));
	}
	else if (x > right) {
		setPlayerPositionX(p, right, getPlayerCoordinateP(p));
	}
}

static void updateKOFlags(DreamPlayer* p) {
	p->mNoKOSoundFlag = 0;
	p->mNoKOSlowdownFlag = 0;
}

static void updateTransparencyFlag(DreamPlayer* p) {
	if (!p->mTransparencyFlag) return;

	setMugenAnimationBlendType(p->mAnimationID, BLEND_TYPE_NORMAL);
	setMugenAnimationTransparency(p->mAnimationID, 1);

	p->mTransparencyFlag = 0;
}

static void updateShadow(DreamPlayer* p) {
	setMugenAnimationFaceDirection(p->mShadow.mAnimationID, getMugenAnimationIsFacingRight(p->mAnimationID));

	p->mShadow.mShadowPosition = *getHandledPhysicsPositionReference(p->mPhysicsID);
	setMugenAnimationVisibility(p->mShadow.mAnimationID, p->mShadow.mShadowPosition.y <= 0);
	p->mShadow.mShadowPosition.y *= -getDreamStageShadowScaleY();
	
	Vector3D fadeRange = getDreamStageShadowFadeRange(getPlayerCoordinateP(p));
	fadeRange = vecScale(fadeRange, 0.5); // TODO: fix
	double posY = -(getDreamScreenHeight(getPlayerCoordinateP(p))-getPlayerScreenPositionY(p, getPlayerCoordinateP(p)));

	if (posY <= fadeRange.x) {
		setMugenAnimationTransparency(p->mShadow.mAnimationID, 0);
	}
	else if (posY <= fadeRange.y) {
		double t = 1-(((-posY) - (-fadeRange.y)) / ((-fadeRange.x) - (-fadeRange.y)));
		setMugenAnimationTransparency(p->mShadow.mAnimationID, t*getDreamStageShadowTransparency());
	}
	else {
		setMugenAnimationTransparency(p->mShadow.mAnimationID, getDreamStageShadowTransparency());
	}
}

static void updateReflection(DreamPlayer* p) {
	setMugenAnimationFaceDirection(p->mReflection.mAnimationID, getMugenAnimationIsFacingRight(p->mAnimationID));

	p->mReflection.mPosition = *getHandledPhysicsPositionReference(p->mPhysicsID);
	setMugenAnimationVisibility(p->mReflection.mAnimationID, p->mReflection.mPosition.y <= 0);
	p->mReflection.mPosition.y *= -1;

}

static void updateSingleProjectile(DreamPlayer* p) {
	updateShadow(p);
	updateReflection(p);
}

static void updateSingleProjectileCB(void* tCaller, void* tData) {
	(void)tCaller;
	updateSingleProjectile((DreamPlayer*)tData);
}

static void updatePlayerTrainingMode(DreamPlayer* p) {
	if (!gData.mIsInTrainingMode) return;
	if (!getPlayerControl(p)) return;

	addPlayerLife(p, 3);
	addPlayerPower(p, 50);
}

static void updatePlayerDestruction(DreamPlayer* p) {
	freeMemory(p);
}

static int updateSinglePlayer(DreamPlayer* p) {
	if (p->mIsDestroyed) {
		updatePlayerDestruction(p);
		return 1;
	}

	updateWalking(p);
	updateAirJumping(p);
	updateJumpFlank(p);
	updateJumping(p);
	updateLanding(p);
	updateCrouchingDown(p);
	updateStandingUp(p);
	updateAutoTurn(p);
	updatePositionFreeze(p);
	updateGettingUp(p);
	updateHitPause(p);
	updateBinding(p);
	updateGuarding(p);
	updateGuardingOver(p);
	updateGuardingFlags(p);
	updateHitAttributeSlots(p);
	updatePush(p);
	updateStageBorder(p);
	updateKOFlags(p);
	updateTransparencyFlag(p);
	updateShadow(p);
	updateReflection(p);
	updatePlayerTrainingMode(p);

	list_remove_predicate(&p->mHelpers, updateSinglePlayerCB, NULL);
	int_map_map(&p->mProjectiles, updateSingleProjectileCB, NULL);
	return 0;
}

void updatePlayers()
{
	int i;
	for (i = 0; i < 2; i++) {
		updateSinglePlayer(&gData.mPlayers[i]);
	}

	updatePushFlags();
}

static void setPlayerHitOver(void* tCaller) {
	DreamPlayer* p = tCaller;

	setActiveHitDataInactive(p);
	p->mIsHitOver = 1;
}

static void setPlayerHitShakeOver(void* tCaller) {
	DreamPlayer* p = tCaller;

	p->mIsHitShakeOver = 1;

	Duration hitDuration;
	if (isPlayerGuarding(p)) {
		hitDuration = getActiveHitDataGuardHitTime(p);
	}
	else if(getPlayerStateType(p) == MUGEN_STATE_TYPE_AIR){
		hitDuration = getActiveHitDataAirHitTime(p);
	}
	else {
		hitDuration = getActiveHitDataGroundHitTime(p);
	}

	setPlayerVelocityX(p, getActiveHitDataVelocityX(p), getPlayerCoordinateP(p));
	setPlayerVelocityY(p, getActiveHitDataVelocityY(p), getPlayerCoordinateP(p));

	addTimerCB(hitDuration, setPlayerHitOver, p);
}

static void handlePlayerHitOverride(DreamPlayer* p, DreamPlayer* tOtherPlayer, int* tNextState) {
	int doesForceAir;
	getMatchingHitOverrideStateNoAndForceAir(p, tOtherPlayer, tNextState, &doesForceAir);
	if (doesForceAir) {
		setPlayerUnguarding(p);
		setPlayerStateType(p, MUGEN_STATE_TYPE_AIR);
		setActiveHitDataAirFall(p, 1);
		p->mIsFalling = getActiveHitDataFall(p);
	}
}

static void setPlayerHitStates(DreamPlayer* p, DreamPlayer* tOtherPlayer) {
	if (getActiveHitDataPlayer1StateNumber(p) != -1) {
		changePlayerState(tOtherPlayer, getActiveHitDataPlayer1StateNumber(p));
	}

	if (getActiveHitDataPlayer2StateNumber(p) == -1) {

		int nextState;

		if (hasMatchingHitOverride(p, tOtherPlayer)) {
			handlePlayerHitOverride(p, tOtherPlayer, &nextState);
		}
		else if (getPlayerStateType(p) == MUGEN_STATE_TYPE_STANDING) {
			if (isPlayerGuarding(p)) {
				nextState = 150;
			}
			else if(getActiveHitDataGroundType(p) == MUGEN_ATTACK_HEIGHT_TRIP){
				nextState = 5070;
			}
			else {
				nextState = 5000;
			}
		}
		else if (getPlayerStateType(p) == MUGEN_STATE_TYPE_CROUCHING) {
			if (isPlayerGuarding(p)) {
				nextState = 152;
			}
			else if (getActiveHitDataGroundType(p) == MUGEN_ATTACK_HEIGHT_TRIP) {
				nextState = 5070;
			}
			else {
				nextState = 5010;
			}
		}
		else if (getPlayerStateType(p) == MUGEN_STATE_TYPE_AIR) {
			if (isPlayerGuarding(p)) {
				nextState = 154;
			}
			else {
				nextState = 5020;
			}
		}
		else {
			logWarningFormat("Unrecognized player state type %d. Defaulting to state 5000.", getPlayerStateType(p));
			nextState = 5000;
		}

		changePlayerState(p, nextState);
	}
	else if (getActiveHitDataPlayer2CapableOfGettingPlayer1State(p)) {
		changePlayerStateToOtherPlayerStateMachine(p, tOtherPlayer, getActiveHitDataPlayer2StateNumber(p));
	}
	else {
		changePlayerState(p, getActiveHitDataPlayer2StateNumber(p));
	}
}

static int checkPlayerHitGuardFlagsAndReturnIfGuardable(DreamPlayer* tPlayer, char* tFlags) {
	DreamMugenStateType type = getPlayerStateType(tPlayer);
	char test[100];
	strcpy(test, tFlags);
	turnStringLowercase(test);

	if (type == MUGEN_STATE_TYPE_STANDING) {

		return strchr(test, 'h') != NULL || strchr(test, 'm') != NULL;
	} else  if (type == MUGEN_STATE_TYPE_CROUCHING) {
		return strchr(test, 'l') != NULL || strchr(test, 'm') != NULL;
	}
	else  if (type == MUGEN_STATE_TYPE_AIR) {
		return strchr(test, 'a') != NULL;
	}
	else {
		logWarningFormat("Unrecognized player type %d. Defaulting to unguardable.", type);
		return 0;
	}

}

static void setPlayerUnguarding(DreamPlayer* p);

static void setPlayerHit(DreamPlayer* p, DreamPlayer* tOtherPlayer, void* tHitData) {
	setPlayerControl(p, 0);

	copyHitDataToActive(p, tHitData);

	if (getPlayerUnguardableFlag(tOtherPlayer) || (isPlayerGuarding(p) && !checkPlayerHitGuardFlagsAndReturnIfGuardable(p, getActiveHitDataGuardFlag(p)))) {
		setPlayerUnguarding(p);
	}

	if (isPlayerGuarding(p)) {
		setActiveHitDataVelocityX(p, getActiveHitDataGuardVelocity(p));
		setActiveHitDataVelocityY(p, 0);
		p->mIsFalling = 0; 
	}
	else if (getPlayerStateType(p) == MUGEN_STATE_TYPE_AIR) {
		setActiveHitDataVelocityX(p, getActiveHitDataAirVelocityX(p));
	 	setActiveHitDataVelocityY(p, getActiveHitDataAirVelocityY(p));
		p->mIsFalling = getActiveHitDataAirFall(p);
	}
	else {
		setActiveHitDataVelocityX(p, getActiveHitDataGroundVelocityX(p));
		setActiveHitDataVelocityY(p, getActiveHitDataGroundVelocityY(p));
		p->mIsFalling = getActiveHitDataFall(p);
	}

	int hitShakeDuration, hitPauseDuration;
	int damage;
	int powerUp1;
	int powerUp2;
	if (isPlayerGuarding(p)) {
		damage = (int)(getActiveHitDataGuardDamage(p) * p->mAttackMultiplier);
		setPlayerMoveGuarded(tOtherPlayer);
		powerUp1 = getActiveHitDataPlayer2GuardPowerAdded(p);
		powerUp2 = getActiveHitDataPlayer1GuardPowerAdded(p);
		hitPauseDuration = getActiveHitDataPlayer1GuardPauseTime(p);
		hitShakeDuration = getActiveHitDataPlayer2GuardPauseTime(p);
	}
	else {
		damage = (int)(getActiveHitDataDamage(p) * p->mAttackMultiplier);
		setPlayerMoveHit(tOtherPlayer);
		powerUp1 = getActiveHitDataPlayer2PowerAdded(p);
		powerUp2 = getActiveHitDataPlayer1PowerAdded(p);
		hitPauseDuration = getActiveHitDataPlayer1PauseTime(p);
		hitShakeDuration = getActiveHitDataPlayer2PauseTime(p);
	}

	p->mIsHitShakeOver = 0;
	p->mIsHitOver = 0;
	
	p->mCanRecoverFromFall = getActiveHitDataFallRecovery(p); // TODO: add time

	addPlayerDamage(p, damage);
	addPlayerPower(p, powerUp1);
	addPlayerPower(tOtherPlayer, powerUp2);

	setPlayerHitStates(p, tOtherPlayer);
	if (hitShakeDuration) {
		setPlayerHitPaused(p, hitShakeDuration);
	}

	if (hitPauseDuration) {
		setPlayerHitPaused(tOtherPlayer, hitPauseDuration);
	}

	addTimerCB(hitShakeDuration, setPlayerHitShakeOver, p);
}

static void playPlayerHitSpark(DreamPlayer* p1, DreamPlayer* p2, int tIsInPlayerFile, int tNumber, Position tSparkOffset) {
	if (tNumber == -1) return;

	Position pos1 = *getHandledPhysicsPositionReference(p1->mPhysicsID);
	Position pos2 = *getHandledPhysicsPositionReference(p2->mPhysicsID);

	Position base;
	if (pos1.x < pos2.x) {
		int width = p1->mFaceDirection == FACE_DIRECTION_RIGHT ? p1->mConstants.mSizeData.mGroundFrontWidth : p1->mConstants.mSizeData.mGroundBackWidth;
		base = vecAdd(pos1, makePosition(width, 0, 0));
		base.y = pos2.y;
	}
	else {
		int width = p1->mFaceDirection == FACE_DIRECTION_LEFT ? p1->mConstants.mSizeData.mGroundFrontWidth : p1->mConstants.mSizeData.mGroundBackWidth;
		base = vecAdd(pos1, makePosition(-width, 0, 0));
		base.y = pos2.y;
	}

	if (p1->mFaceDirection == FACE_DIRECTION_LEFT) {
		tSparkOffset.x = -tSparkOffset.x;
	}

	tSparkOffset = vecAdd(tSparkOffset, base);

	playDreamHitSpark(tSparkOffset, p2, tIsInPlayerFile, tNumber, getActiveHitDataIsFacingRight(p1), getPlayerCoordinateP(p2), getPlayerCoordinateP(p2));
}

typedef struct {
	int mFound;
	MugenAttackType mType;
	MugenAttackClass mClass;
} HitDefAttributeFlag2Caller;

static void checkSingleFlag2InHitDefAttributeSlot(HitDefAttributeFlag2Caller* tCaller, char* tFlag) {

	if (tCaller->mClass == MUGEN_ATTACK_CLASS_NORMAL) {
		if (tFlag[0] != 'n') return;
	}
	else if (tCaller->mClass == MUGEN_ATTACK_CLASS_SPECIAL) {
		if (tFlag[0] != 's') return;
	}
	else if (tCaller->mClass == MUGEN_ATTACK_CLASS_HYPER) {
		if (tFlag[0] != 'h') return;
	}
	else {
		logWarningFormat("Unrecognized attack class %d. Defaulting to not found.", tCaller->mClass);
		return;
	}

	if (tCaller->mType == MUGEN_ATTACK_TYPE_ATTACK) {
		if (tFlag[1] != 'a') return;
	} 
	else if (tCaller->mType == MUGEN_ATTACK_TYPE_PROJECTILE) {
		if (tFlag[1] != 'p') return;
	}
	else if (tCaller->mType == MUGEN_ATTACK_TYPE_THROW) {
		if (tFlag[1] != 't') return;
	}
	else {
		logWarningFormat("Unrecognized attack type %d. Defaulting to not found.", tCaller->mType);
		return;
	}

	tCaller->mFound = 1;
}

static int checkSingleNoHitDefSlot(DreamHitDefAttributeSlot* tSlot, DreamPlayer* p2) {
	if (!tSlot->mIsActive) return 1;

	DreamMugenStateType type = getHitDataType(p2);

	if (type == MUGEN_STATE_TYPE_STANDING) {
		if (strchr(tSlot->mFlag1, 's') != NULL) return tSlot->mIsHitBy;
	} 
	else if (type == MUGEN_STATE_TYPE_CROUCHING) {
		if (strchr(tSlot->mFlag1, 'c') != NULL) return tSlot->mIsHitBy;
	}
	else if (type == MUGEN_STATE_TYPE_AIR) {
		if (strchr(tSlot->mFlag1, 'a') != NULL) return tSlot->mIsHitBy;
	}
	else {
		logWarningFormat("Invalid hitdef type %d. Defaulting to not not hit.", type);
		return 0;
	}

	HitDefAttributeFlag2Caller caller;
	caller.mFound = 0;
	caller.mType = getHitDataAttackType(p2);
	caller.mClass = getHitDataAttackClass(p2);

	int i;
	for (i = 0; i < tSlot->mFlag2Amount; i++) {
		checkSingleFlag2InHitDefAttributeSlot(&caller, tSlot->mFlag2[i]);
	}

	if (caller.mFound) return tSlot->mIsHitBy;
	else return !tSlot->mIsHitBy;
}

static int checkActiveHitDefAttributeSlots(DreamPlayer* p, DreamPlayer* p2) {
	int i;
	for (i = 0; i < 2; i++) {
		if (!checkSingleNoHitDefSlot(&p->mNotHitBy[i], p2)) return 0;
	}

	return 1;
}

static void playPlayerHitSound(DreamPlayer* p, void(*tFunc)(DreamPlayer*,int*,Vector3DI*)) {
	int isInPlayerFile;
	Vector3DI sound;
	tFunc(p, &isInPlayerFile, &sound);

	MugenSounds* soundFile;
	if (isInPlayerFile) {
		soundFile = getPlayerSounds(p);
	}
	else {
		soundFile = getDreamCommonSounds();
	}

	tryPlayMugenSound(soundFile, sound.x, sound.y);
}

void playerHitCB(void* tData, void* tHitData)
{
	// TOOD: reversaldef
	DreamPlayer* p = tData;

	DreamPlayer* otherPlayer = getReceivedHitDataPlayer(tHitData);

	if (getPlayerStateType(p) == MUGEN_STATE_TYPE_LYING) return;
	if (!isReceivedHitDataActive(tHitData)) return;
	if (!checkActiveHitDefAttributeSlots(p, otherPlayer)) return;
	if (isIgnoredBecauseOfHitOverride(p, otherPlayer)) return;

	setPlayerHit(p, otherPlayer, tHitData);
	setReceivedHitDataInactive(tHitData);
	
	if (isPlayerGuarding(p)) {
		playPlayerHitSound(p, getActiveHitDataGuardSound);
		playPlayerHitSpark(p, otherPlayer, isActiveHitDataGuardSparkInPlayerFile(p), getActiveHitDataGuardSparkNumber(p), getActiveHitDataSparkXY(p));
	}
	else {
		increasePlayerHitCount(otherPlayer);
		playPlayerHitSound(p, getActiveHitDataHitSound);
		playPlayerHitSpark(p, otherPlayer, isActiveHitDataSparkInPlayerFile(p), getActiveHitDataSparkNumber(p), getActiveHitDataSparkXY(p));
	}
	
	setPlayerMoveContactCounterActive(otherPlayer);

	if (isPlayerProjectile(otherPlayer)) {
		handleProjectileHit(otherPlayer);
	}
}

void setPlayerDefinitionPath(int i, char * tDefinitionPath)
{
	strcpy(gData.mPlayers[i].mDefinitionPath, tDefinitionPath);
}

void getPlayerDefinitionPath(char* tDst, int i)
{
	strcpy(tDst, gData.mPlayers[i].mDefinitionPath);
}

void setPlayerPreferredPalette(int i, int tPalette)
{
	gData.mPlayers[i].mPreferredPalette = tPalette;
}

DreamPlayer * getRootPlayer(int i)
{
	return &gData.mPlayers[i];
}

DreamPlayer * getPlayerRoot(DreamPlayer * p)
{
	return p->mRoot;
}

DreamPlayer * getPlayerParent(DreamPlayer * p)
{
	return p->mParent;
}

int getPlayerState(DreamPlayer* p)
{
	return getDreamRegisteredStateState(p->mStateMachineID);
}

int getPlayerPreviousState(DreamPlayer* p)
{
	return getDreamRegisteredStatePreviousState(p->mStateMachineID);
}

DreamMugenStateType getPlayerStateType(DreamPlayer* p)
{
	return p->mStateType;
}

void setPlayerStateType(DreamPlayer* p, DreamMugenStateType tType)
{
	if (tType == MUGEN_STATE_TYPE_UNCHANGED) return;

	p->mStateType = tType;
}

DreamMugenStateMoveType getPlayerStateMoveType(DreamPlayer* p)
{
	return p->mMoveType;
}

void setPlayerStateMoveType(DreamPlayer* p, DreamMugenStateMoveType tType)
{
	if (tType == MUGEN_STATE_MOVE_TYPE_UNCHANGED) return;

	p->mMoveType = tType;
}

int getPlayerControl(DreamPlayer* p)
{
	return p->mIsInControl;
}

void setPlayerControl(DreamPlayer* p, int tNewControl)
{
	p->mIsInControl = tNewControl;
}


DreamMugenStatePhysics getPlayerPhysics(DreamPlayer* p) {
	return p->mStatePhysics;
}

void setPlayerPhysics(DreamPlayer* p, DreamMugenStatePhysics tNewPhysics)
{

	if (tNewPhysics == MUGEN_STATE_PHYSICS_UNCHANGED) {
		return;
	}
	else if (tNewPhysics == MUGEN_STATE_PHYSICS_STANDING) {
		setHandledPhysicsDragCoefficient(p->mPhysicsID, makePosition(p->mConstants.mMovementData.mStandFiction, 0, 0));
		setHandledPhysicsGravity(p->mPhysicsID, makePosition(0, 0, 0));
		Position* pos = getHandledPhysicsPositionReference(p->mPhysicsID);
		Velocity* vel = getHandledPhysicsVelocityReference(p->mPhysicsID);
		Acceleration* acc = getHandledPhysicsAccelerationReference(p->mPhysicsID);
		
		vel->y = 0;
		acc->y = 0;
	}
	else if (tNewPhysics == MUGEN_STATE_PHYSICS_CROUCHING) {
		setHandledPhysicsDragCoefficient(p->mPhysicsID, makePosition(p->mConstants.mMovementData.mCrouchFriction, 0, 0));
		setHandledPhysicsGravity(p->mPhysicsID, makePosition(0, 0, 0));
		Position* pos = getHandledPhysicsPositionReference(p->mPhysicsID);
		Velocity* vel = getHandledPhysicsVelocityReference(p->mPhysicsID);
		Acceleration* acc = getHandledPhysicsAccelerationReference(p->mPhysicsID);

		pos->y = 0;
		vel->y = 0;
		acc->y = 0;
	}
	else if (tNewPhysics == MUGEN_STATE_PHYSICS_NONE) {
		setHandledPhysicsDragCoefficient(p->mPhysicsID, makePosition(0, 0, 0));
		setHandledPhysicsGravity(p->mPhysicsID, makePosition(0, 0, 0));


		if (getPlayerStateType(p) == MUGEN_STATE_TYPE_LYING) {
			Acceleration* acc = getHandledPhysicsAccelerationReference(p->mPhysicsID);
			acc->y = 0;
		}
	}
	else if (tNewPhysics == MUGEN_STATE_PHYSICS_AIR) {
		setHandledPhysicsDragCoefficient(p->mPhysicsID, makePosition(0, 0, 0));
		setHandledPhysicsGravity(p->mPhysicsID, makePosition(0, p->mConstants.mMovementData.mVerticalAcceleration, 0));

		if (tNewPhysics != p->mStatePhysics) {
			setPlayerNoLandFlag(p);			
		}

		if (p->mStatePhysics == MUGEN_STATE_PHYSICS_STANDING || p->mStatePhysics == MUGEN_STATE_PHYSICS_CROUCHING) {
			p->mAirJumpCounter = 0;
			p->mJumpFlank = 0;
		}
	}
	else {
		logWarningFormat("Unrecognized physics state %d. Defaulting to unchanged.", tNewPhysics);
		return;
	}

	p->mStatePhysics = tNewPhysics;
}

int getPlayerMoveContactCounter(DreamPlayer* p)
{
	return p->mMoveContactCounter;
}

static void increaseComboCounter(DreamPlayer* p) {

	p->mComboCounter++;
}

static void resetComboCounter(DreamPlayer* p) {
	if (!getPlayerControl(p)) return;

	p->mComboCounter = 0;
}

void resetPlayerMoveContactCounter(DreamPlayer* p)
{
	p->mMoveContactCounter = 0;
	resetComboCounter(p);
}

void setPlayerMoveContactCounterActive(DreamPlayer* p) {
	p->mMoveContactCounter = 1;
	increaseComboCounter(p);
}

int getPlayerVariable(DreamPlayer* p, int tIndex)
{
	// assert(tIndex < 100); // TODO: figure out
	return p->mVars[tIndex];
}

void setPlayerVariable(DreamPlayer* p, int tIndex, int tValue)
{
	// assert(tIndex < 100); // TODO: figure out
	p->mVars[tIndex] = tValue;
}

void addPlayerVariable(DreamPlayer * p, int tIndex, int tValue)
{
	int cur = getPlayerVariable(p, tIndex);
	cur += tValue;
	setPlayerVariable(p, tIndex, cur);
}

int getPlayerSystemVariable(DreamPlayer* p, int tIndex)
{
	// assert(tIndex < 100); // TODO: figure out
	return p->mSystemVars[tIndex];
}

void setPlayerSystemVariable(DreamPlayer* p, int tIndex, int tValue)
{
	// assert(tIndex < 100); // TODO: figure out
	p->mSystemVars[tIndex] = tValue;
}

void addPlayerSystemVariable(DreamPlayer * p, int tIndex, int tValue)
{
	int cur = getPlayerSystemVariable(p, tIndex);
	cur += tValue;
	setPlayerSystemVariable(p, tIndex, cur);
}

double getPlayerFloatVariable(DreamPlayer* p, int tIndex)
{
	// assert(tIndex < 100); // TODO: figure out
	return p->mFloatVars[tIndex];
}

void setPlayerFloatVariable(DreamPlayer* p, int tIndex, double tValue)
{
	// assert(tIndex < 100); // TODO: figure out
	p->mFloatVars[tIndex] = tValue;
}

void addPlayerFloatVariable(DreamPlayer * p, int tIndex, double tValue)
{
	double cur = getPlayerFloatVariable(p, tIndex);
	cur += tValue;
	setPlayerFloatVariable(p, tIndex, cur);
}

double getPlayerSystemFloatVariable(DreamPlayer* p, int tIndex)
{
	// assert(tIndex < 100); // TODO: figure out
	return p->mSystemFloatVars[tIndex];
}

void setPlayerSystemFloatVariable(DreamPlayer* p, int tIndex, double tValue)
{
	// assert(tIndex < 100); // TODO: figure out
	p->mSystemFloatVars[tIndex] = tValue;
}

void addPlayerSystemFloatVariable(DreamPlayer * p, int tIndex, double tValue)
{
	double cur = getPlayerSystemFloatVariable(p, tIndex);
	cur += tValue;
	setPlayerSystemFloatVariable(p, tIndex, cur);
}

int getPlayerTimeInState(DreamPlayer* p)
{
	return getDreamRegisteredStateTimeInState(p->mStateMachineID);
}

int getPlayerAnimationNumber(DreamPlayer* p)
{
	return getMugenAnimationAnimationNumber(p->mAnimationID);
}

int getRemainingPlayerAnimationTime(DreamPlayer* p)
{
	return getMugenAnimationRemainingAnimationTime(p->mAnimationID);
}

Vector3D getPlayerPosition(DreamPlayer* p, int tCoordinateP)
{
	return makePosition(getPlayerPositionX(p, tCoordinateP), getPlayerPositionY(p, tCoordinateP), 0);
}

double getPlayerPositionBasedOnScreenCenterX(DreamPlayer * p, int tCoordinateP)
{
	Position pos = getDreamStageCenterOfScreenBasedOnPlayer(tCoordinateP);
	Position ret = vecSub(getPlayerPosition(p, tCoordinateP), pos);
	return ret.x;
}

double getPlayerScreenPositionX(DreamPlayer * p, int tCoordinateP)
{
	double scale = tCoordinateP / getPlayerCoordinateP(p);
	double physicsPosition = getHandledPhysicsPositionReference(p->mPhysicsID)->x;
	double cameraPosition = getDreamMugenStageHandlerCameraPositionReference()->x;
	double animationPosition = getMugenAnimationPosition(p->mAnimationID).x;
	double unscaledPosition = physicsPosition + animationPosition - cameraPosition;
	return unscaledPosition * scale;
}

double getPlayerPositionX(DreamPlayer* p, int tCoordinateP)
{
	double scale = tCoordinateP / getPlayerCoordinateP(p);
	return getHandledPhysicsPositionReference(p->mPhysicsID)->x * scale;
}

double getPlayerPositionBasedOnStageFloorY(DreamPlayer * p, int tCoordinateP)
{
	Position pos = getDreamStageCenterOfScreenBasedOnPlayer(tCoordinateP);
	Position ret = vecSub(getPlayerPosition(p, tCoordinateP), pos);
	return ret.y;
}

double getPlayerScreenPositionY(DreamPlayer * p, int tCoordinateP)
{
	double scale = tCoordinateP / getPlayerCoordinateP(p);
	double physicsPosition = getHandledPhysicsPositionReference(p->mPhysicsID)->y;
	double cameraPosition = getDreamMugenStageHandlerCameraPositionReference()->y;
	double animationPosition = getMugenAnimationPosition(p->mAnimationID).y;
	double unscaledPosition = physicsPosition + animationPosition - cameraPosition;
	return unscaledPosition * scale;
}

double getPlayerPositionY(DreamPlayer* p, int tCoordinateP)
{
	double scale = tCoordinateP / getPlayerCoordinateP(p);
	return getHandledPhysicsPositionReference(p->mPhysicsID)->y * scale;
}

double getPlayerVelocityX(DreamPlayer* p, int tCoordinateP)
{
	double scale = tCoordinateP / getPlayerCoordinateP(p);
	double x = getHandledPhysicsVelocityReference(p->mPhysicsID)->x * scale;
	if (p->mFaceDirection == FACE_DIRECTION_LEFT) x *= -1;

	return  x;
}

double getPlayerVelocityY(DreamPlayer* p, int tCoordinateP)
{
	double scale = tCoordinateP / getPlayerCoordinateP(p);
	return getHandledPhysicsVelocityReference(p->mPhysicsID)->y * scale;
}

int getPlayerDataLife(DreamPlayer * p)
{
	return p->mConstants.mHeader.mLife;
}

int getPlayerDataAttack(DreamPlayer * p)
{
	return p->mConstants.mHeader.mAttack;
}

int getPlayerDataDefense(DreamPlayer * p)
{
	return p->mConstants.mHeader.mDefense;
}

int getPlayerDataLiedownTime(DreamPlayer * p)
{
	return (int)p->mConstants.mHeader.mLiedownTime; // TODO: fix
}

int getPlayerDataAirjuggle(DreamPlayer * p)
{
	return p->mConstants.mHeader.mAirJugglePoints;
}

int getPlayerDataSparkNo(DreamPlayer * p)
{
	return p->mConstants.mHeader.mSparkNo;
}

int getPlayerDataGuardSparkNo(DreamPlayer * p)
{
	return p->mConstants.mHeader.mGuardSparkNo;
}

int getPlayerDataKOEcho(DreamPlayer * p)
{
	return p->mConstants.mHeader.mKOEcho;
}

int getPlayerDataIntPersistIndex(DreamPlayer * p)
{
	return p->mConstants.mHeader.mIntPersistIndex;
}

int getPlayerDataFloatPersistIndex(DreamPlayer * p)
{
	return p->mConstants.mHeader.mFloatPersistIndex;
}

int getPlayerSizeAirBack(DreamPlayer * p)
{
	return p->mConstants.mSizeData.mAirBackWidth;
}

int getPlayerSizeAirFront(DreamPlayer * p)
{
	return p->mConstants.mSizeData.mAirFrontWidth;
}

int getPlayerSizeAttackDist(DreamPlayer * p)
{
	return p->mConstants.mSizeData.mAttackDistance;
}

int getPlayerSizeProjectileAttackDist(DreamPlayer * p)
{
	return p->mConstants.mSizeData.mProjectileAttackDistance;
}

int getPlayerSizeProjectilesDoScale(DreamPlayer * p)
{
	return p->mConstants.mSizeData.mDoesScaleProjectiles;
}

int getPlayerSizeShadowOffset(DreamPlayer * p)
{
	return p->mConstants.mSizeData.mShadowOffset;
}

int getPlayerSizeDrawOffsetX(DreamPlayer * p)
{
	return p->mConstants.mSizeData.mDrawOffset.x;
}

int getPlayerSizeDrawOffsetY(DreamPlayer * p)
{
	return p->mConstants.mSizeData.mDrawOffset.y;
}

double getPlayerVelocityAirGetHitGroundRecoverX(DreamPlayer * p)
{
	return p->mConstants.mVelocityData.mAirGetHitGroundRecovery.x;
}

double getPlayerVelocityAirGetHitGroundRecoverY(DreamPlayer * p)
{
	return p->mConstants.mVelocityData.mAirGetHitGroundRecovery.y;
}

double getPlayerVelocityAirGetHitAirRecoverMulX(DreamPlayer * p)
{
	return p->mConstants.mVelocityData.mAirGetHitAirRecoveryMultiplier.x;
}

double getPlayerVelocityAirGetHitAirRecoverMulY(DreamPlayer * p)
{
	return p->mConstants.mVelocityData.mAirGetHitAirRecoveryMultiplier.y;
}

double getPlayerVelocityAirGetHitAirRecoverAddX(DreamPlayer * p)
{
	return p->mConstants.mVelocityData.mAirGetHitAirRecoveryOffset.x;
}

double getPlayerVelocityAirGetHitAirRecoverAddY(DreamPlayer * p)
{
	return p->mConstants.mVelocityData.mAirGetHitAirRecoveryOffset.y;
}

double getPlayerVelocityAirGetHitAirRecoverBack(DreamPlayer * p)
{
	return p->mConstants.mVelocityData.mAirGetHitExtraXWhenHoldingBackward;
}

double getPlayerVelocityAirGetHitAirRecoverFwd(DreamPlayer * p)
{
	return p->mConstants.mVelocityData.mAirGetHitExtraXWhenHoldingForward;
}

double getPlayerVelocityAirGetHitAirRecoverUp(DreamPlayer * p)
{
	return p->mConstants.mVelocityData.mAirGetHitExtraYWhenHoldingUp;
}

double getPlayerVelocityAirGetHitAirRecoverDown(DreamPlayer * p)
{
	return p->mConstants.mVelocityData.mAirGetHitExtraYWhenHoldingDown;
}

int getPlayerMovementAirJumpNum(DreamPlayer * p)
{
	return p->mConstants.mMovementData.mAllowedAirJumpAmount;
}

int getPlayerMovementAirJumpHeight(DreamPlayer * p)
{
	return p->mConstants.mMovementData.mAirJumpMinimumHeight;
}

double getPlayerMovementJumpChangeAnimThreshold(DreamPlayer * p)
{
	return p->mConstants.mMovementData.mJumpChangeAnimThreshold;
}

double getPlayerMovementAirGetHitAirRecoverYAccel(DreamPlayer * p)
{
	return p->mConstants.mMovementData.mAirGetHitAirRecoveryVerticalAcceleration;
}

double getPlayerStandFriction(DreamPlayer * p)
{
	return p->mConstants.mMovementData.mStandFiction;
}

double getPlayerStandFrictionThreshold(DreamPlayer* p)
{
	return p->mConstants.mMovementData.mStandFrictionThreshold;
}

double getPlayerCrouchFriction(DreamPlayer * p)
{
	return p->mConstants.mMovementData.mCrouchFriction;
}

double getPlayerCrouchFrictionThreshold(DreamPlayer* p)
{
	return p->mConstants.mMovementData.mCrouchFrictionThreshold;
}

double getPlayerAirGetHitGroundLevelY(DreamPlayer* p)
{
	return p->mConstants.mMovementData.mAirGetHitGroundLevelY;
}

double getPlayerAirGetHitGroundRecoveryGroundLevelY(DreamPlayer * p)
{
	return p->mConstants.mMovementData.mAirGetHitGroundRecoveryGroundGoundLevelY;
}

double getPlayerAirGetHitGroundRecoveryGroundYTheshold(DreamPlayer* p)
{
	return p->mConstants.mMovementData.mAirGetHitGroundRecoveryGroundYTheshold;
}

double getPlayerAirGetHitAirRecoveryVelocityYThreshold(DreamPlayer* p)
{
	return p->mConstants.mMovementData.mAirGetHitAirRecoveryVelocityYThreshold;
}

double getPlayerAirGetHitTripGroundLevelY(DreamPlayer* p)
{
	return p->mConstants.mMovementData.mAirGetHitTripGroundLevelY;
}

double getPlayerDownBounceOffsetX(DreamPlayer* p, int tCoordinateP)
{
	return p->mConstants.mMovementData.mBounceOffset.x;
}

double getPlayerDownBounceOffsetY(DreamPlayer* p, int tCoordinateP)
{
	return p->mConstants.mMovementData.mBounceOffset.y;
}

double getPlayerDownVerticalBounceAcceleration(DreamPlayer* p)
{
	return p->mConstants.mMovementData.mVerticalBounceAcceleration;
}

double getPlayerDownBounceGroundLevel(DreamPlayer* p, int tCoordinateP)
{
	return p->mConstants.mMovementData.mBounceGroundLevel;
}

double getPlayerLyingDownFrictionThreshold(DreamPlayer* p)
{
	return p->mConstants.mMovementData.mLyingDownFrictionThreshold;
}

double getPlayerVerticalAcceleration(DreamPlayer* p)
{
	return p->mConstants.mMovementData.mVerticalAcceleration;
}

double getPlayerForwardWalkVelocityX(DreamPlayer* p)
{
	return p->mConstants.mVelocityData.mWalkForward.x;
}

double getPlayerBackwardWalkVelocityX(DreamPlayer* p)
{
	return p->mConstants.mVelocityData.mWalkBackward.x;
}

double getPlayerForwardRunVelocityX(DreamPlayer* p)
{
	return p->mConstants.mVelocityData.mRunForward.x;
}

double getPlayerForwardRunVelocityY(DreamPlayer* p)
{
	return p->mConstants.mVelocityData.mRunForward.y;
}

double getPlayerBackwardRunVelocityX(DreamPlayer* p)
{
	return p->mConstants.mVelocityData.mRunBackward.x;
}

double getPlayerBackwardRunVelocityY(DreamPlayer* p)
{
	return p->mConstants.mVelocityData.mRunBackward.y;
}

double getPlayerBackwardRunJumpVelocityX(DreamPlayer * p)
{
	return p->mConstants.mVelocityData.mRunJumpBackward.x;
}

double getPlayerForwardRunJumpVelocityX(DreamPlayer* p)
{
	return p->mConstants.mVelocityData.mRunJumpForward.x;
}

double getPlayerNeutralJumpVelocityX(DreamPlayer* p)
{
	return p->mConstants.mVelocityData.mJumpNeutral.x;
}

double getPlayerForwardJumpVelocityX(DreamPlayer* p)
{
	return p->mConstants.mVelocityData.mJumpForward.x;
}

double getPlayerBackwardJumpVelocityX(DreamPlayer* p)
{
	return p->mConstants.mVelocityData.mJumpBackward.x;
}

double getPlayerJumpVelocityY(DreamPlayer* p)
{
	return p->mConstants.mVelocityData.mJumpNeutral.y;
}

double getPlayerNeutralAirJumpVelocityX(DreamPlayer* p)
{
	return p->mConstants.mVelocityData.mAirJumpNeutral.x;
}

double getPlayerForwardAirJumpVelocityX(DreamPlayer* p)
{
	return p->mConstants.mVelocityData.mAirJumpForward.x;
}

double getPlayerBackwardAirJumpVelocityX(DreamPlayer* p)
{
	return p->mConstants.mVelocityData.mAirJumpBackward.x;
}

double getPlayerAirJumpVelocityY(DreamPlayer* p)
{
	return p->mConstants.mVelocityData.mAirJumpNeutral.y;
}


int isPlayerAlive(DreamPlayer* p)
{
	return p->mIsAlive;
}

int isPlayerDestroyed(DreamPlayer * p)
{
	return p->mIsDestroyed;
}

void setPlayerVelocityX(DreamPlayer* p, double x, int tCoordinateP)
{
	double scale = getPlayerCoordinateP(p) / tCoordinateP;
	Velocity* vel = getHandledPhysicsVelocityReference(p->mPhysicsID);
	double fx = x*scale;
	if (p->mFaceDirection == FACE_DIRECTION_LEFT) fx *= -1;
	vel->x = fx;
}

void setPlayerVelocityY(DreamPlayer* p, double y, int tCoordinateP)
{
	double scale = getPlayerCoordinateP(p) / tCoordinateP;
	Velocity* vel = getHandledPhysicsVelocityReference(p->mPhysicsID);
	vel->y = y*scale;
}

void multiplyPlayerVelocityX(DreamPlayer* p, double x, int tCoordinateP)
{
	double scale = getPlayerCoordinateP(p) / tCoordinateP;
	Velocity* vel = getHandledPhysicsVelocityReference(p->mPhysicsID);
	double fx = x * scale;
	vel->x *= fx;
}

void multiplyPlayerVelocityY(DreamPlayer* p, double y, int tCoordinateP)
{
	double scale = getPlayerCoordinateP(p) / tCoordinateP;
	Velocity* vel = getHandledPhysicsVelocityReference(p->mPhysicsID);
	vel->y *= y*scale;
}

void addPlayerVelocityX(DreamPlayer* p, double x, int tCoordinateP)
{
	if (isPlayerPaused(p)) return;

	double scale = getPlayerCoordinateP(p) / tCoordinateP;
	Velocity* vel = getHandledPhysicsVelocityReference(p->mPhysicsID);
	double fx = x*scale;
	if (p->mFaceDirection == FACE_DIRECTION_LEFT) fx *= -1;
	vel->x += fx;
}

void addPlayerVelocityY(DreamPlayer* p, double y, int tCoordinateP)
{
	if (isPlayerPaused(p)) return;

	double scale = getPlayerCoordinateP(p) / tCoordinateP;
	Velocity* vel = getHandledPhysicsVelocityReference(p->mPhysicsID);
	vel->y += y*scale;
}

void setPlayerPosition(DreamPlayer * p, Position tPosition, int tCoordinateP)
{
	setPlayerPositionX(p, tPosition.x, tCoordinateP);
	setPlayerPositionY(p, tPosition.y, tCoordinateP);
}

void setPlayerPositionX(DreamPlayer* p, double x, int tCoordinateP)
{
	double scale = getPlayerCoordinateP(p) / tCoordinateP;
	Position* pos = getHandledPhysicsPositionReference(p->mPhysicsID);
	pos->x = x*scale;
}

void setPlayerPositionY(DreamPlayer* p, double y, int tCoordinateP)
{
	double scale = getPlayerCoordinateP(p) / tCoordinateP;
	Position* pos = getHandledPhysicsPositionReference(p->mPhysicsID);
	pos->y = y*scale;
}

void addPlayerPositionX(DreamPlayer* p, double x, int tCoordinateP)
{
	if (isPlayerPaused(p)) return;

	double scale = getPlayerCoordinateP(p) / tCoordinateP;
	Position* pos = getHandledPhysicsPositionReference(p->mPhysicsID);
	double fx = x*scale;
	if (p->mFaceDirection == FACE_DIRECTION_LEFT) fx *= -1;
	pos->x += fx;
}

void addPlayerPositionY(DreamPlayer* p, double y, int tCoordinateP)
{
	if (isPlayerPaused(p)) return;

	Position* pos = getHandledPhysicsPositionReference(p->mPhysicsID);

	double scale = getPlayerCoordinateP(p) / tCoordinateP;
	pos->y += y*scale;
}

int isPlayerCommandActive(DreamPlayer* p, char * tCommandName)
{
	return isDreamCommandActive(p->mCommandID, tCommandName);
}



int hasPlayerState(DreamPlayer * p, int mNewState)
{
	return hasDreamHandledStateMachineState(p->mStateMachineID, mNewState);
}

int hasPlayerStateSelf(DreamPlayer * p, int mNewState)
{
	return hasDreamHandledStateMachineStateSelf(p->mStateMachineID, mNewState);
}

void changePlayerState(DreamPlayer* p, int mNewState)
{
	if (!hasPlayerState(p, mNewState)) {
		logWarning("Trying to change into state that does not exist");
		logWarningInteger(mNewState);
		return;
	}
	changeDreamHandledStateMachineState(p->mStateMachineID, mNewState);
	setDreamRegisteredStateTimeInState(p->mStateMachineID, -1);
	updateDreamSingleStateMachineByID(p->mStateMachineID); // TODO: think
}

void changePlayerStateToOtherPlayerStateMachine(DreamPlayer * p, DreamPlayer * tOtherPlayer, int mNewState)
{
	changeDreamHandledStateMachineStateToOtherPlayerStateMachine(p->mStateMachineID, tOtherPlayer->mStateMachineID, mNewState);
	setDreamRegisteredStateTimeInState(p->mStateMachineID, -1);
	updateDreamSingleStateMachineByID(p->mStateMachineID); // TODO: think
}

void changePlayerStateBeforeImmediatelyEvaluatingIt(DreamPlayer * p, int mNewState)
{
	if (!hasPlayerState(p, mNewState)) {
		logWarning("Trying to change into state that does not exist");
		logWarningInteger(mNewState);
		return;
	}
	changeDreamHandledStateMachineState(p->mStateMachineID, mNewState);

}

void changePlayerStateToSelfBeforeImmediatelyEvaluatingIt(DreamPlayer * p, int tNewState)
{
	changeDreamHandledStateMachineStateToOwnStateMachine(p->mStateMachineID, tNewState);
}

void changePlayerAnimation(DreamPlayer* p, int tNewAnimation)
{
	changePlayerAnimationWithStartStep(p, tNewAnimation, 0);
}

void changePlayerAnimationWithStartStep(DreamPlayer* p, int tNewAnimation, int tStartStep)
{
	MugenAnimation* newAnimation = getMugenAnimation(&p->mAnimations, tNewAnimation);
	changeMugenAnimationWithStartStep(p->mAnimationID, newAnimation, tStartStep);
	changeMugenAnimationWithStartStep(p->mShadow.mAnimationID, newAnimation, tStartStep);
	changeMugenAnimationWithStartStep(p->mReflection.mAnimationID, newAnimation, tStartStep);
}

void changePlayerAnimationToPlayer2AnimationWithStartStep(DreamPlayer * p, int tNewAnimation, int tStartStep)
{
	DreamPlayer* otherPlayer = getPlayerOtherPlayer(p);
	MugenAnimation* newAnimation = getMugenAnimation(&otherPlayer->mAnimations, tNewAnimation);
	changeMugenAnimationWithStartStep(p->mAnimationID, newAnimation, tStartStep);
	changeMugenAnimationWithStartStep(p->mShadow.mAnimationID, newAnimation, tStartStep);
	changeMugenAnimationWithStartStep(p->mReflection.mAnimationID, newAnimation, tStartStep);
}

int isPlayerStartingAnimationElementWithID(DreamPlayer* p, int tStepID)
{
	return isStartingMugenAnimationElementWithID(p->mAnimationID, tStepID);
}

int getPlayerTimeFromAnimationElement(DreamPlayer* p, int tStep)
{
	return getTimeFromMugenAnimationElement(p->mAnimationID, tStep);
}

int getPlayerAnimationElementFromTimeOffset(DreamPlayer* p, int tTime)
{
	return getMugenAnimationElementFromTimeOffset(p->mAnimationID, tTime);
}

void setPlayerSpritePriority(DreamPlayer* p, int tPriority)
{
	Position pos = getMugenAnimationPosition(p->mAnimationID);
	pos.z = PLAYER_Z + tPriority * 0.1 + p->mRootID * 0.01; // TODO: properly
	setMugenAnimationPosition(p->mAnimationID, pos);
}

void setPlayerNoWalkFlag(DreamPlayer* p)
{
	p->mNoWalkFlag = 1;
}

void setPlayerNoAutoTurnFlag(DreamPlayer* p)
{
	p->mNoAutoTurnFlag = 1;
}

void setPlayerInvisibleFlag(DreamPlayer * p)
{
	setMugenAnimationInvisibleForOneFrame(p->mAnimationID); 
	setMugenAnimationInvisibleForOneFrame(p->mShadow.mAnimationID);
	setMugenAnimationInvisibleForOneFrame(p->mReflection.mAnimationID);
}

void setPlayerNoLandFlag(DreamPlayer* p)
{
	p->mNoLandFlag = 1;
}

void setPlayerNoShadow(DreamPlayer * p)
{
	(void)p;
	// TODO: shadow
}

void setAllPlayersNoShadow()
{
	// TODO: shadows
}

void setPlayerPushDisabledFlag(DreamPlayer * p, int tIsDisabled)
{
	p->mPushDisabledFlag = tIsDisabled;
}

void setPlayerNoJuggleCheckFlag(DreamPlayer * p)
{
	p->mNoJuggleCheckFlag = 1;
}

void setPlayerIntroFlag(DreamPlayer * p)
{
	p->mIntroFlag = 1; // TODO: use
}

void setPlayerNoAirGuardFlag(DreamPlayer * p)
{
	p->mNoAirGuardFlag = 1;
}

void setPlayerNoCrouchGuardFlag(DreamPlayer * p)
{
	p->mNoCrouchGuardFlag = 1;

}

void setPlayerNoStandGuardFlag(DreamPlayer * p)
{
	p->mNoStandGuardFlag = 1;
}

void setPlayerNoKOSoundFlag(DreamPlayer * p)
{
	p->mNoKOSoundFlag = 1;
}

void setPlayerNoKOSlowdownFlag(DreamPlayer * p)
{
	p->mNoKOSlowdownFlag = 1;
}

void setPlayerUnguardableFlag(DreamPlayer * p)
{
	p->mUnguardableFlag = 1;
}

int getPlayerNoKOSlowdownFlag(DreamPlayer * p)
{
	return p->mNoKOSlowdownFlag;
}

int getPlayerUnguardableFlag(DreamPlayer * p)
{
	return p->mUnguardableFlag;
}

int isPlayerInIntro(DreamPlayer * p)
{
	int ret = p->mIntroFlag;
	p->mIntroFlag = 0;
	return ret;
}

int doesPlayerHaveAnimation(DreamPlayer * p, int tAnimation)
{
	return hasMugenAnimation(&p->mAnimations, tAnimation); // TODO: check active animations; implement animation setting
}

int doesPlayerHaveAnimationHimself(DreamPlayer* p, int tAnimation)
{
	return hasMugenAnimation(&p->mAnimations, tAnimation);
}

int isPlayerHitShakeOver(DreamPlayer* p)
{
	return p->mIsHitShakeOver;
}

int isPlayerHitOver(DreamPlayer* p)
{
	return p->mIsHitOver;
}

double getPlayerHitVelocityX(DreamPlayer * p, int tCoordinateP)
{
	if (isPlayerHitOver(p)) return 0.0;
	return -getPlayerVelocityX(p, tCoordinateP);	
}

double getPlayerHitVelocityY(DreamPlayer * p, int tCoordinateP)
{
	if (isPlayerHitOver(p)) return 0.0;
	return -getPlayerVelocityY(p, tCoordinateP);
}

int isPlayerFalling(DreamPlayer* p) {
	return p->mIsFalling;
}

int canPlayerRecoverFromFalling(DreamPlayer* p)
{
	return isPlayerFalling(p) && p->mCanRecoverFromFall; // TODO: add time
}

int getPlayerSlideTime(DreamPlayer* p)
{
	return getActiveHitDataGroundSlideTime(p); // TODO: for guarding
}
 
void setPlayerDefenseMultiplier(DreamPlayer* p, double tValue)
{
	p->mDefenseMultiplier = tValue;
}

void setPlayerPositionFrozen(DreamPlayer* p)
{
	p->mIsFrozen = 1;
	p->mFreezePosition = *getHandledPhysicsPositionReference(p->mPhysicsID);
}

void setPlayerPositionUnfrozen(DreamPlayer* p)
{
	p->mIsFrozen = 0;
}

MugenSpriteFile * getPlayerSprites(DreamPlayer* p)
{
	return &p->mSprites;
}

MugenAnimations * getPlayerAnimations(DreamPlayer* p)
{
	return &p->mAnimations;
}

MugenAnimation * getPlayerAnimation(DreamPlayer* p, int tNumber)
{
	return getMugenAnimation(&p->mAnimations, tNumber);
}

MugenSounds * getPlayerSounds(DreamPlayer * p)
{
	return &p->mSounds;
}

int getPlayerCoordinateP(DreamPlayer* p)
{
	return p->mHeader.mLocalCoordinates.y;
}

char * getPlayerDisplayName(DreamPlayer* p)
{
	return p->mHeader.mDisplayName;
}

char * getPlayerName(DreamPlayer * p)
{
	return p->mHeader.mName;
}

char * getPlayerAuthorName(DreamPlayer * p)
{
	return p->mHeader.mAuthor;
}

int isPlayerPaused(DreamPlayer* p)
{
	return p->mIsHitPaused || p->mIsSuperPaused;
}

static void pausePlayer(DreamPlayer* p) {
	if (isPlayerPaused(p)) return;

	pausePhysics(p->mPhysicsID);
	pauseMugenAnimation(p->mAnimationID);
	pauseMugenAnimation(p->mShadow.mAnimationID);
	pauseMugenAnimation(p->mReflection.mAnimationID);
	pauseDreamRegisteredStateMachine(p->mStateMachineID);
}

static void unpausePlayer(DreamPlayer* p) {
	if (isPlayerPaused(p)) return;

	resumePhysics(p->mPhysicsID);
	unpauseMugenAnimation(p->mAnimationID);
	unpauseMugenAnimation(p->mShadow.mAnimationID);
	unpauseMugenAnimation(p->mReflection.mAnimationID);
	unpauseDreamRegisteredStateMachine(p->mStateMachineID);
}

void setPlayerHitPaused(DreamPlayer* p, int tDuration)
{
	pausePlayer(p);

	p->mIsHitPaused = 1;
	p->mHitPauseNow = 0;
	p->mHitPauseDuration = tDuration;
}

void setPlayerUnHitPaused(DreamPlayer* p)
{
	p->mIsHitPaused = 0;

	unpausePlayer(p);
}

void setPlayerSuperPaused(DreamPlayer* p)
{
	pausePlayer(p);

	p->mIsSuperPaused = 1;
}

void setPlayerUnSuperPaused(DreamPlayer* p)
{
	p->mIsSuperPaused = 0;

	unpausePlayer(p);

	if (!isPlayerPaused(p)) {
		advanceMugenAnimationOneTick(p->mAnimationID); // TODO: fix somehow
		advanceMugenAnimationOneTick(p->mShadow.mAnimationID); // TODO: fix somehow
		advanceMugenAnimationOneTick(p->mReflection.mAnimationID); // TODO: fix somehow
	}
}

static void setPlayerDead(DreamPlayer* p) {
	if (gData.mIsInTrainingMode) return;

	p->mIsAlive = 0;
	
	if (!p->mNoKOSoundFlag) {
		tryPlayMugenSound(&p->mSounds, 11, 0);
	}
}

void addPlayerDamage(DreamPlayer* p, int tDamage)
{
	p->mLife -= tDamage;
	if (p->mLife <= 0) {
		setPlayerDead(p);
		p->mLife = 0;
	}
	p->mLife = min(p->mLife, getPlayerLifeMax(p));

	double perc = p->mLife / (double)p->mConstants.mHeader.mLife;
	setDreamLifeBarPercentage(p, perc);

}

int getPlayerTargetAmount(DreamPlayer* p)
{
	(void)p;
	return 0; // TODO
}

int getPlayerTargetAmountWithID(DreamPlayer* p, int tID)
{
	(void)p;
	(void)tID;
	return 0; // TODO
}

int getPlayerHelperAmount(DreamPlayer* p)
{
	return list_size(&p->mHelpers);
}

typedef struct {
	int mSearchID;
	int mFoundAmount;
} PlayerHelperCountCaller;

static void countHelperByIDCB(void* tCaller, void* tData) {
	PlayerHelperCountCaller* caller = tCaller;
	DreamPlayer* helper = tData;

	if (helper->mID == caller->mSearchID) {
		caller->mFoundAmount++;
	}
}

int getPlayerHelperAmountWithID(DreamPlayer* p, int tID)
{
	PlayerHelperCountCaller caller;
	caller.mFoundAmount = 0;
	caller.mSearchID = tID;
	list_map(&p->mHelpers, countHelperByIDCB, &caller);

	return caller.mFoundAmount;
}

typedef struct {
	DreamPlayer* mFoundHelper;
	int mSearchID;

} PlayerHelperFindCaller;

static void findHelperByIDCB(void* tCaller, void* tData) {
	PlayerHelperFindCaller* caller = tCaller;
	DreamPlayer* helper = tData;

	if (helper->mID == caller->mSearchID) {
		caller->mFoundHelper = helper;
	}
}

DreamPlayer * getPlayerHelperOrNullIfNonexistant(DreamPlayer * p, int tID)
{
	PlayerHelperFindCaller caller;
	caller.mFoundHelper = NULL;
	caller.mSearchID = tID;
	list_map(&p->mHelpers, findHelperByIDCB, &caller);

	return caller.mFoundHelper;
}

int getPlayerProjectileAmount(DreamPlayer* p)
{
	return int_map_size(&p->mProjectiles);
}

int getPlayerProjectileAmountWithID(DreamPlayer * p, int tID)
{
	(void)p;
	(void)tID;
	return 0; // TODO
}

int getPlayerProjectileTimeSinceCancel(DreamPlayer * p, int tID)
{
	(void)p;
	(void)tID;
	return 0; // TODO
}

int getPlayerProjectileTimeSinceContact(DreamPlayer * p, int tID)
{
	(void)p;
	(void)tID;
	return 0; // TODO
}

int getPlayerProjectileTimeSinceGuarded(DreamPlayer * p, int tID)
{
	(void)p;
	(void)tID;
	return 0; // TODO
}

int getPlayerProjectileTimeSinceHit(DreamPlayer * p, int tID)
{
	(void)p;
	(void)tID;
	return 0; // TODO
}

int getPlayerTimeLeftInHitPause(DreamPlayer* p)
{
	if (!p->mIsHitPaused) return 0;

	return (int)(p->mHitPauseDuration - p->mHitPauseNow);
}

double getPlayerFrontAxisDistanceToScreen(DreamPlayer* p)
{
	double x = getPlayerPositionX(p, getPlayerCoordinateP(p));
	double screenX = getPlayerScreenEdgeInFrontX(p);

	return screenX - x;
}

double getPlayerBackAxisDistanceToScreen(DreamPlayer* p)
{
	double x = getPlayerPositionX(p, getPlayerCoordinateP(p));
	double screenX = getDreamCameraPositionX(getPlayerCoordinateP(p));

	return x - screenX;
}

double getPlayerFrontBodyDistanceToScreen(DreamPlayer* p)
{
	double x = getPlayerFrontX(p);
	double screenX = getPlayerScreenEdgeInBackX(p);

	return fabs(screenX - x);
}

double getPlayerBackBodyDistanceToScreen(DreamPlayer* p)
{
	double x = getPlayerBackX(p);
	double screenX = getPlayerScreenEdgeInBackX(p);

	return fabs(screenX - x);
}

double getPlayerFrontX(DreamPlayer* p)
{
	double x = getPlayerPositionX(p, getPlayerCoordinateP(p));
	if (p->mFaceDirection == FACE_DIRECTION_RIGHT) return x + p->mConstants.mSizeData.mGroundFrontWidth; // TODO: air
	else return x - p->mConstants.mSizeData.mGroundFrontWidth; // TODO: air
}

double getPlayerBackX(DreamPlayer* p)
{
	double x = getPlayerPositionX(p, getPlayerCoordinateP(p));
	if (p->mFaceDirection == FACE_DIRECTION_RIGHT) return x - p->mConstants.mSizeData.mGroundBackWidth; // TODO: air
	else return x + p->mConstants.mSizeData.mGroundBackWidth; // TODO: air
}

double getPlayerScreenEdgeInFrontX(DreamPlayer* p)
{
	double x = getDreamCameraPositionX(getPlayerCoordinateP(p));

	if (p->mFaceDirection == FACE_DIRECTION_RIGHT) return x + p->mHeader.mLocalCoordinates.x;
	else return  x;
}

double getPlayerScreenEdgeInBackX(DreamPlayer* p)
{
	double x = getDreamCameraPositionX(getPlayerCoordinateP(p));

	if (p->mFaceDirection == FACE_DIRECTION_RIGHT) return x;
	else return  x + p->mHeader.mLocalCoordinates.x;
}

double getPlayerDistanceToFrontOfOtherPlayerX(DreamPlayer* p)
{
	DreamPlayer* otherPlayer = getPlayerOtherPlayer(p);
	double x1 = getPlayerFrontX(p);
	double x2 = getPlayerFrontX(otherPlayer);

	return fabs(x2-x1);
}

static double getPlayerAxisDistanceForTwoReferencesX(DreamPlayer* p1, DreamPlayer* p2) {

	double x1 = getPlayerPositionX(p1, getPlayerCoordinateP(p1));
	double x2 = getPlayerPositionX(p2, getPlayerCoordinateP(p1));

	return x2 - x1;
}

double getPlayerAxisDistanceForTwoReferencesY(DreamPlayer* p1, DreamPlayer* p2)
{
	double y1 = getPlayerPositionY(p1, getPlayerCoordinateP(p1));
	double y2 = getPlayerPositionY(p2, getPlayerCoordinateP(p1));

	return y2 - y1;
}

double getPlayerAxisDistanceX(DreamPlayer* p)
{
	DreamPlayer* otherPlayer = getPlayerOtherPlayer(p);
	return getPlayerAxisDistanceForTwoReferencesX(p, otherPlayer);
}

double getPlayerAxisDistanceY(DreamPlayer* p)
{
	DreamPlayer* otherPlayer = getPlayerOtherPlayer(p);
	return getPlayerAxisDistanceForTwoReferencesY(p, otherPlayer);
}

double getPlayerDistanceToRootX(DreamPlayer * p)
{
	DreamPlayer* otherPlayer = p->mRoot;
	return getPlayerAxisDistanceForTwoReferencesX(p, otherPlayer);
}

double getPlayerDistanceToRootY(DreamPlayer * p)
{
	DreamPlayer* otherPlayer = p->mRoot;
	return getPlayerAxisDistanceForTwoReferencesY(p, otherPlayer);
}

double getPlayerDistanceToParentX(DreamPlayer * p)
{
	if (!p->mParent) return 0;
	DreamPlayer* otherPlayer = p->mParent;
	return getPlayerAxisDistanceForTwoReferencesX(p, otherPlayer);
}

double getPlayerDistanceToParentY(DreamPlayer * p)
{
	if (!p->mParent) return 0;
	DreamPlayer* otherPlayer = p->mParent;
	return getPlayerAxisDistanceForTwoReferencesY(p, otherPlayer);
}

int getPlayerGroundSizeFront(DreamPlayer* p)
{
	return p->mConstants.mSizeData.mGroundFrontWidth;
}

void setPlayerGroundSizeFront(DreamPlayer * p, int tGroundSizeFront)
{
	p->mConstants.mSizeData.mGroundFrontWidth = tGroundSizeFront;
}

int getPlayerGroundSizeBack(DreamPlayer* p)
{
	return p->mConstants.mSizeData.mGroundBackWidth;
}

void setPlayerGroundSizeBack(DreamPlayer * p, int tGroundSizeBack)
{
	p->mConstants.mSizeData.mGroundBackWidth = tGroundSizeBack;
}

int getPlayerAirSizeFront(DreamPlayer * p)
{
	return p->mConstants.mSizeData.mAirFrontWidth;
}

void setPlayerAirSizeFront(DreamPlayer * p, int tAirSizeFront)
{
	p->mConstants.mSizeData.mAirFrontWidth = tAirSizeFront;
}

int getPlayerAirSizeBack(DreamPlayer * p)
{
	return p->mConstants.mSizeData.mAirBackWidth;
}

void setPlayerAirSizeBack(DreamPlayer * p, int tAirSizeBack)
{
	p->mConstants.mSizeData.mAirBackWidth = tAirSizeBack;
}

int getPlayerHeight(DreamPlayer* p)
{
	return p->mConstants.mSizeData.mHeight;
}

void setPlayerHeight(DreamPlayer * p, int tHeight)
{
	p->mConstants.mSizeData.mHeight = tHeight;
}

static void increaseSinglePlayerRoundsExisted(DreamPlayer * p);
static void increasePlayerRoundsExistedCB(void* tCaller, void* tData) {
	(void)tCaller;
	DreamPlayer* p = tData;
	increaseSinglePlayerRoundsExisted(p);
}

static void increaseSinglePlayerRoundsExisted(DreamPlayer * p)
{
	p->mRoundsExisted++;
	list_map(&p->mHelpers, increasePlayerRoundsExistedCB, NULL);
}

void increasePlayerRoundsExisted()
{
	increaseSinglePlayerRoundsExisted(&gData.mPlayers[0]);
	increaseSinglePlayerRoundsExisted(&gData.mPlayers[1]);
}


void increasePlayerRoundsWon(DreamPlayer * p)
{
	p->mRoundsWon++;
}

int hasPlayerWonByKO(DreamPlayer* p)
{
	(void)p;
	return 0; // TODO
}

int hasPlayerLostByKO(DreamPlayer * p)
{
	(void)p;
	return 0; // TODO
}

int hasPlayerWonPerfectly(DreamPlayer* p)
{
	return hasPlayerWon(p) && p->mLife == p->mConstants.mHeader.mLife;
}

int hasPlayerWon(DreamPlayer* p)
{
	return p->mRoundsWon == 2; // TODO: getDreamRoundNumber
}

int hasPlayerLost(DreamPlayer* p)
{
	DreamPlayer* otherPlayer = getPlayerOtherPlayer(p);
	return hasPlayerWon(otherPlayer); 
}

int hasPlayerDrawn(DreamPlayer * p)
{
	return 0; // TODO
}

int hasPlayerMoveHitOtherPlayer(DreamPlayer* p)
{
	DreamMugenStateMoveType type = getPlayerStateMoveType(p);
	int isInAttackState = type == MUGEN_STATE_MOVE_TYPE_ATTACK;
	int isOtherPlayerHit = isPlayerHit(getPlayerOtherPlayer(p));
	int wasItCurrentAttack = !isHitDataActive(p); // TODO: properly

	return isInAttackState && isOtherPlayerHit && wasItCurrentAttack;
}

int isPlayerHit(DreamPlayer* p)
{
	DreamMugenStateMoveType moveType = getPlayerStateMoveType(p);
	return moveType == MUGEN_STATE_MOVE_TYPE_BEING_HIT;  // TODO: properly
}

int hasPlayerMoveBeenReversedByOtherPlayer(DreamPlayer * p)
{
	return p->mHasMoveBeenReversed;
}

int getPlayerMoveHit(DreamPlayer * p)
{
	return p->mMoveHit;
}

void setPlayerMoveHit(DreamPlayer * p)
{
	p->mMoveHit = 1;
}

void setPlayerMoveHitReset(DreamPlayer * p)
{
	p->mMoveContactCounter = 0;
	p->mMoveHit = 0;
	p->mMoveGuarded = 0;
	p->mHasMoveBeenReversed = 0;
}

int getPlayerMoveGuarded(DreamPlayer * p)
{
	return p->mMoveGuarded;
}



void setPlayerMoveGuarded(DreamPlayer * p)
{
	p->mMoveGuarded = 1;
}

int getPlayerFallAmountInCombo(DreamPlayer* p)
{
	return p->mFallAmountInCombo;
}

void increasePlayerFallAmountInCombo(DreamPlayer* p)
{
	p->mFallAmountInCombo++;
}

void resetPlayerFallAmountInCombo(DreamPlayer* p)
{
	p->mFallAmountInCombo = 0;
}

int getPlayerHitCount(DreamPlayer* p)
{
	return p->mHitCount;
}

int getPlayerUniqueHitCount(DreamPlayer * p)
{
	return getPlayerHitCount(p); // TODO: fix when teams are implemented
}

void increasePlayerHitCount(DreamPlayer* p)
{
	p->mHitCount++;
}

void resetPlayerHitCount(DreamPlayer* p)
{
	p->mHitCount = 0;
	resetPlayerFallAmountInCombo(p);
}

void setPlayerAttackMultiplier(DreamPlayer* p, double tValue)
{
	p->mAttackMultiplier = tValue;
}

double getPlayerFallDefenseMultiplier(DreamPlayer* p)
{
	int f = p->mConstants.mHeader.mFallDefenseUp;
	return 100.0 / (f+100);
}

void setPlayerHuman(int i)
{
	DreamPlayer* p = getRootPlayer(i);
	p->mAILevel = 0;
}

void setPlayerArtificial(int i)
{
	DreamPlayer* p = getRootPlayer(i);
	p->mAILevel = 8; // TODO: properly
}

int getPlayerAILevel(DreamPlayer* p)
{
	return p->mAILevel;
}

void setPlayerLife(DreamPlayer * p, int tLife)
{
	p->mLife = tLife;
}

void addPlayerLife(DreamPlayer * p, int tLife)
{
	addPlayerDamage(p, -tLife);
}

int getPlayerLife(DreamPlayer* p)
{
	return p->mLife;
}

int getPlayerLifeMax(DreamPlayer* p)
{
	return p->mConstants.mHeader.mLife;
}

int getPlayerPower(DreamPlayer* p)
{
	return p->mPower;
}

int getPlayerPowerMax(DreamPlayer* p)
{
	return p->mConstants.mHeader.mPower;
}

void setPlayerPower(DreamPlayer * p, int tPower)
{
	p->mPower = max(0, min(getPlayerPowerMax(p), tPower));

	double perc = p->mPower / (double)p->mConstants.mHeader.mPower;
	setDreamPowerBarPercentage(p, perc, p->mPower);
}

void addPlayerPower(DreamPlayer* p, int tPower)
{
	setPlayerPower(p, p->mPower + tPower);
}

int isPlayerBeingAttacked(DreamPlayer* p) {
	DreamPlayer* otherPlayer = getPlayerOtherPlayer(p);

	return isHitDataActive(otherPlayer);
}

int isPlayerInGuardDistance(DreamPlayer* p)
{
	DreamPlayer* otherPlayer = getPlayerOtherPlayer(p);

	int isBeingAttacked = isPlayerBeingAttacked(p);

	double dist = fabs(getPlayerAxisDistanceX(p));
	int isInDistance = dist < getHitDataGuardDistance(otherPlayer); // TODO: check helpers

	return isBeingAttacked && isInDistance;
}

int getDefaultPlayerAttackDistance(DreamPlayer * p)
{
	return p->mConstants.mSizeData.mAttackDistance;
}

Position getPlayerHeadPosition(DreamPlayer * p)
{
	return makePosition(getPlayerHeadPositionX(p), getPlayerHeadPositionY(p), 0);
}

double getPlayerHeadPositionX(DreamPlayer* p)
{
	return p->mConstants.mSizeData.mHeadPosition.x;
}

double getPlayerHeadPositionY(DreamPlayer* p)
{
	return p->mConstants.mSizeData.mHeadPosition.y;
}

void setPlayerHeadPosition(DreamPlayer * p, double tX, double tY)
{
	p->mConstants.mSizeData.mHeadPosition = makePosition(tX, tY, 0);
}

Position getPlayerMiddlePosition(DreamPlayer * p)
{
	return makePosition(getPlayerMiddlePositionX(p), getPlayerMiddlePositionY(p), 0);
}

double getPlayerMiddlePositionX(DreamPlayer * p)
{
	return p->mConstants.mSizeData.mMidPosition.x;
}

double getPlayerMiddlePositionY(DreamPlayer * p)
{
	return p->mConstants.mSizeData.mMidPosition.y;
}

void setPlayerMiddlePosition(DreamPlayer* p, double tX, double tY)
{
	p->mConstants.mSizeData.mMidPosition = makePosition(tX, tY, 0);
}

int getPlayerShadowOffset(DreamPlayer * p)
{
	return p->mConstants.mSizeData.mShadowOffset;
}

void setPlayerShadowOffset(DreamPlayer * p, int tOffset)
{
	p->mConstants.mSizeData.mShadowOffset = tOffset;
}

int isPlayerHelper(DreamPlayer* p)
{
	return p->mIsHelper;
}

void setPlayerIsFacingRight(DreamPlayer * p, int tIsFacingRight)
{
	setPlayerFaceDirection(p, tIsFacingRight ? FACE_DIRECTION_RIGHT : FACE_DIRECTION_LEFT);
}

int getPlayerIsFacingRight(DreamPlayer* p)
{
	return p->mFaceDirection == FACE_DIRECTION_RIGHT;
}

void turnPlayerAround(DreamPlayer * p)
{
	setPlayerIsFacingRight(p, !getPlayerIsFacingRight(p));
}

DreamPlayer* getPlayerOtherPlayer(DreamPlayer* p) {
	return p->mOtherPlayer;
}

double getPlayerScaleX(DreamPlayer * p)
{
	return p->mConstants.mSizeData.mScale.x;
}

void setPlayerScaleX(DreamPlayer * p, double tScaleX)
{
	p->mConstants.mSizeData.mScale.x = tScaleX;
}

double getPlayerScaleY(DreamPlayer * p)
{
	return p->mConstants.mSizeData.mScale.y;
}

void setPlayerScaleY(DreamPlayer * p, double tScaleY)
{
	p->mConstants.mSizeData.mScale.y = tScaleY;
}

DreamPlayer * clonePlayerAsHelper(DreamPlayer * p)
{
	DreamPlayer* helper = allocMemory(sizeof(DreamPlayer));
	*helper = *p;

	resetHelperState(helper);
	setPlayerExternalDependencies(helper);
	loadPlayerShadow(helper);
	loadPlayerReflection(helper);
	setDreamRegisteredStateToHelperMode(helper->mStateMachineID);

	helper->mParent = p;
	helper->mIsHelper = 1;
	helper->mHelperIDInParent = list_push_back(&p->mHelpers, helper);

	return helper;
}

typedef struct {
	DreamPlayer* mParent;

} MovePlayerHelperCaller;

static int moveSinglePlayerHelper(void* tCaller, void* tData) {
	MovePlayerHelperCaller* caller = tCaller;
	DreamPlayer* helper = tData;
	DreamPlayer* parent = caller->mParent;
	DreamPlayer* parentParent = parent->mParent;
	
	helper->mParent = parentParent;
	helper->mHelperIDInParent = list_push_back(&parentParent->mHelpers, helper);

	return 1;
}

static void movePlayerHelpersToParent(DreamPlayer* p) {
	MovePlayerHelperCaller caller;
	caller.mParent = p;
	list_remove_predicate(&p->mHelpers, moveSinglePlayerHelper, &caller);
	delete_list(&p->mHelpers);
}

static void removePlayerBindingCB(void* tCaller, void* tData) {
	(void)tCaller;
	DreamPlayer* p = tData;
	removePlayerBinding(p);
}

static void removePlayerBoundHelpers(DreamPlayer* p) {
	list_map(&p->mBoundHelpers, removePlayerBindingCB, NULL);
	delete_list(&p->mBoundHelpers);
}

static void destroyGeneralPlayer(DreamPlayer* p) {
	removeMugenAnimation(p->mAnimationID);
	removeMugenAnimation(p->mShadow.mAnimationID);
	removeMugenAnimation(p->mReflection.mAnimationID);
	removeFromPhysicsHandler(p->mPhysicsID);
	removePlayerHitData(p);
	p->mIsDestroyed = 1;
}

void destroyPlayer(DreamPlayer * p) // TODO: rename
{
	assert(p->mIsHelper);
	assert(p->mParent);
	assert(p->mHelperIDInParent != -1);

	removePlayerBoundHelpers(p);
	movePlayerHelpersToParent(p);
	destroyGeneralPlayer(p);
}

int getPlayerID(DreamPlayer * p)
{
	return p->mID;
}

void setPlayerID(DreamPlayer * p, int tID)
{
	p->mID = tID;
}

void setPlayerHelperControl(DreamPlayer * p, int tCanControl)
{
	if (!tCanControl) {
		setDreamRegisteredStateDisableCommandState(p->mStateMachineID);
	}
}

static void addProjectileToRoot(DreamPlayer* tPlayer, DreamPlayer* tProjectile) {
	DreamPlayer* root = tPlayer->mRoot;

	tProjectile->mProjectileID = int_map_push_back(&root->mProjectiles, tProjectile);
}

DreamPlayer * createNewProjectileFromPlayer(DreamPlayer * p)
{
	DreamPlayer* helper = allocMemory(sizeof(DreamPlayer));
	*helper = *p;

	resetHelperState(helper);
	setPlayerExternalDependencies(helper);
	loadPlayerShadow(helper);
	loadPlayerReflection(helper);
	disableDreamRegisteredStateMachine(helper->mStateMachineID);
	addProjectileToRoot(p, helper);
	addAdditionalProjectileData(helper);
	setPlayerPhysics(helper, MUGEN_STATE_PHYSICS_NONE);
	setPlayerIsFacingRight(helper, getPlayerIsFacingRight(p));
	helper->mIsProjectile = 1;

	return helper;
}

static void removeProjectileFromPlayer(DreamPlayer* p) {
	DreamPlayer* root = getPlayerRoot(p);
	int_map_remove(&root->mProjectiles, p->mProjectileID);
}

void removeProjectile(DreamPlayer* p) {
	assert(p->mIsProjectile);
	assert(p->mProjectileID != -1);
	removeAdditionalProjectileData(p);
	removeProjectileFromPlayer(p);
	removeDreamRegisteredStateMachine(p->mStateMachineID);
	unloadHelperStateWithoutFreeingOwnedHelpersAndProjectile(p);
	destroyGeneralPlayer(p);
	freeMemory(p);
}

int getPlayerControlTime(DreamPlayer * p)
{
	if (getPlayerStateType(p) == MUGEN_STATE_TYPE_AIR) {
		return getActiveHitDataAirGuardControlTime(p);
	}
	else {
		return getActiveHitDataGuardControlTime(p);
	}
}

void setPlayerDrawScale(DreamPlayer * p, Vector3D tScale)
{
	setMugenAnimationDrawScale(p->mAnimationID, tScale); // TODO: one frame only
}

void setPlayerDrawAngle(DreamPlayer * p, double tAngle)
{
	setMugenAnimationDrawAngle(p->mAnimationID, tAngle); // TODO: one frame only and non-fixed
}

void addPlayerDrawAngle(DreamPlayer * p, double tAngle)
{
	double angle = getMugenAnimationDrawAngle(p->mAnimationID);
	angle += tAngle;
	setMugenAnimationDrawAngle(p->mAnimationID, angle); // TODO: one frame only
}

void multiplyPlayerDrawAngle(DreamPlayer * p, double tFactor)
{
	double angle = getMugenAnimationDrawAngle(p->mAnimationID);
	angle *= tFactor;
	setMugenAnimationDrawAngle(p->mAnimationID, angle); // TODO: one frame only
}

void setPlayerFixedDrawAngle(DreamPlayer * p, double tAngle)
{
	setMugenAnimationDrawAngle(p->mAnimationID, tAngle); // TODO: one frame only and fixed
}

static void bindHelperToPlayer(DreamPlayer* tHelper, DreamPlayer* tBind, int tTime, int tFacing, Vector3D tOffset, DreamPlayerBindPositionType tType) {
	tHelper->mIsBound = 1;
	tHelper->mBoundNow = 0;
	tHelper->mBoundDuration = tTime;
	tHelper->mBoundFaceSet = tFacing;
	tHelper->mBoundOffset = tOffset;
	tHelper->mBoundPositionType = tType;
	tHelper->mBoundTarget = tBind;

	updateBindingPosition(tHelper);

	tHelper->mBoundID = list_push_back(&tBind->mBoundHelpers, tHelper);
}

void bindPlayerToRoot(DreamPlayer * p, int tTime, int tFacing, Vector3D tOffset)
{
	bindHelperToPlayer(p, p->mRoot, tTime, tFacing, tOffset, PLAYER_BIND_POSITION_TYPE_AXIS);
}

void bindPlayerToParent(DreamPlayer * p, int tTime, int tFacing, Vector3D tOffset)
{
	bindHelperToPlayer(p, p->mParent, tTime, tFacing, tOffset, PLAYER_BIND_POSITION_TYPE_AXIS);
}

// TODO: correct target finding
typedef struct {
	DreamPlayer* mBindRoot;
	int mID;
	Position mOffset;
	int mTime;
	DreamPlayerBindPositionType mBindPositionType; 
} BindPlayerTargetsCaller;

static void bindSinglePlayerToTarget(DreamPlayer* tBindRoot, DreamPlayer* tTarget, BindPlayerTargetsCaller* tCaller);

static void bindPlayerTargetCB(void* tCaller, void* tData) {
	BindPlayerTargetsCaller* caller = tCaller;
	DreamPlayer* helper = tData;

	bindSinglePlayerToTarget(caller->mBindRoot, helper, caller);
}

static void bindSinglePlayerToTarget(DreamPlayer* tBindRoot, DreamPlayer* tTarget, BindPlayerTargetsCaller* tCaller) {

	if (tCaller->mID == -1 || tCaller->mID == getPlayerID(tTarget)) {
		bindHelperToPlayer(tBindRoot, tTarget, tCaller->mTime, 0, tCaller->mOffset, tCaller->mBindPositionType);
	}

	list_map(&tTarget->mHelpers, bindPlayerTargetCB, tCaller);
}

void bindPlayerToTarget(DreamPlayer * p, int tTime, Vector3D tOffset, DreamPlayerBindPositionType tBindPositionType, int tID)
{
	BindPlayerTargetsCaller caller;
	caller.mBindRoot = p;
	caller.mID = tID;
	caller.mOffset = tOffset;
	caller.mTime = tTime + 1; // TODO: fix
	caller.mBindPositionType = tBindPositionType;
	bindSinglePlayerToTarget(p, getPlayerOtherPlayer(p)->mRoot, &caller);
}

int isPlayerBound(DreamPlayer * p)
{
	return p->mIsBound;
}


static void bindSingleTargetToPlayer(DreamPlayer* tBindRoot, DreamPlayer* tTarget, BindPlayerTargetsCaller* tCaller);

static void bindTargetToPlayerCB(void* tCaller, void* tData) {
	BindPlayerTargetsCaller* caller = tCaller;
	DreamPlayer* helper = tData;

	bindSingleTargetToPlayer(caller->mBindRoot, helper, caller);
}

static void bindSingleTargetToPlayer(DreamPlayer* tBindRoot, DreamPlayer* tTarget, BindPlayerTargetsCaller* tCaller) {

	if (tCaller->mID == -1 || tCaller->mID == getPlayerID(tTarget)) {
		bindHelperToPlayer(tTarget, tBindRoot, tCaller->mTime, 0, tCaller->mOffset, tCaller->mBindPositionType);
	}

	list_map(&tTarget->mHelpers, bindTargetToPlayerCB, tCaller);
}

void bindPlayerTargetToPlayer(DreamPlayer * p, int tTime, Vector3D tOffset, int tID)
{
	BindPlayerTargetsCaller caller;
	caller.mBindRoot = p;
	caller.mID = tID;
	caller.mOffset = tOffset;
	caller.mTime = tTime + 1; // TODO: fix
	caller.mBindPositionType = PLAYER_BIND_POSITION_TYPE_AXIS;
	bindSingleTargetToPlayer(p, getPlayerOtherPlayer(p)->mRoot, &caller);
}

// TODO: proper targeting
void addPlayerTargetLife(DreamPlayer * p, int tID, int tLife, int tCanKill, int tIsAbsolute)
{
	DreamPlayer* otherPlayer = getPlayerOtherPlayer(p)->mRoot;
	if (tID != -1 && getPlayerID(otherPlayer) != tID) return;

	(void)tIsAbsolute; // TODO


	int playerLife = getPlayerLife(otherPlayer);
	if (!tCanKill && playerLife + tLife <= 0) {
		tLife = -(playerLife - 1);
	}

	addPlayerDamage(otherPlayer, -tLife);

}

// TODO: proper targeting
void addPlayerTargetPower(DreamPlayer * p, int tID, int tPower)
{
	DreamPlayer* otherPlayer = getPlayerOtherPlayer(p)->mRoot;
	if (tID != -1 && getPlayerID(otherPlayer) != tID) return;

	addPlayerPower(otherPlayer, tPower);
}

// TODO: proper targeting
void addPlayerTargetVelocityX(DreamPlayer * p, int tID, double tValue, int tCoordinateP)
{
	DreamPlayer* otherPlayer = getPlayerOtherPlayer(p)->mRoot;
	if (tID != -1 && getPlayerID(otherPlayer) != tID) return;
	
	addPlayerVelocityX(otherPlayer, tValue, tCoordinateP);
}

// TODO: proper targeting
void addPlayerTargetVelocityY(DreamPlayer * p, int tID, double tValue, int tCoordinateP)
{
	DreamPlayer* otherPlayer = getPlayerOtherPlayer(p)->mRoot;
	if (tID != -1 && getPlayerID(otherPlayer) != tID) return;

	addPlayerVelocityY(otherPlayer, tValue, tCoordinateP);
}

// TODO: proper targeting
void setPlayerTargetVelocityX(DreamPlayer * p, int tID, double tValue, int tCoordinateP)
{
	DreamPlayer* otherPlayer = getPlayerOtherPlayer(p)->mRoot;
	if (tID != -1 && getPlayerID(otherPlayer) != tID) return;

	setPlayerVelocityX(otherPlayer, tValue, tCoordinateP);
}

// TODO: proper targeting
void setPlayerTargetVelocityY(DreamPlayer * p, int tID, double tValue, int tCoordinateP)
{
	DreamPlayer* otherPlayer = getPlayerOtherPlayer(p)->mRoot;
	if (tID != -1 && getPlayerID(otherPlayer) != tID) return;

	setPlayerVelocityY(otherPlayer, tValue, tCoordinateP);
}

void setPlayerTargetControl(DreamPlayer * p, int tID, int tControl)
{
	DreamPlayer* otherPlayer = getPlayerOtherPlayer(p)->mRoot;
	if (tID != -1 && getPlayerID(otherPlayer) != tID) return;

	setPlayerControl(otherPlayer, tControl);
}

void setPlayerTargetFacing(DreamPlayer * p, int tID, int tFacing)
{
	DreamPlayer* otherPlayer = getPlayerOtherPlayer(p)->mRoot;
	if (tID != -1 && getPlayerID(otherPlayer) != tID) return;

	if (tFacing == 1) {
		setPlayerIsFacingRight(otherPlayer, getPlayerIsFacingRight(p));
	}
	else {
		setPlayerIsFacingRight(otherPlayer, !getPlayerIsFacingRight(p));
	}
}

void changePlayerTargetState(DreamPlayer * p, int tID, int tNewState)
{
	DreamPlayer* otherPlayer = getPlayerOtherPlayer(p)->mRoot;
	if (tID != -1 && getPlayerID(otherPlayer) != tID) return;

	changePlayerState(otherPlayer, tNewState);
}

typedef struct {
	int mSearchID;
	DreamPlayer* mPlayer;

} SearchPlayerIDCaller;

static DreamPlayer* searchSinglePlayerForID(DreamPlayer* p, int tID);

static void searchPlayerForIDCB(void* tCaller, void* tData) {
	SearchPlayerIDCaller* caller = tCaller;
	DreamPlayer* p = tData;

	DreamPlayer* ret = searchSinglePlayerForID(p, caller->mSearchID);
	if (ret != NULL) {
		caller->mPlayer = ret;
	}
}

static DreamPlayer* searchSinglePlayerForID(DreamPlayer* p, int tID) {
	if (getPlayerID(p) == tID) return p;

	SearchPlayerIDCaller caller;
	caller.mSearchID = tID;
	caller.mPlayer = NULL;
	list_map(&p->mHelpers, searchPlayerForIDCB, &caller);

	return caller.mPlayer;
}

int doesPlayerIDExist(DreamPlayer * p, int tID)
{
	return searchSinglePlayerForID(p->mRoot, tID) != NULL;
}

DreamPlayer * getPlayerByIDOrNullIfNonexistant(DreamPlayer * p, int tID)
{
	return searchSinglePlayerForID(p->mRoot, tID);
}

int getPlayerRoundsExisted(DreamPlayer * p)
{
	return p->mRoundsExisted;
}

int getPlayerPaletteNumber(DreamPlayer * p)
{
	return p->mPreferredPalette;
}

void setPlayerScreenBound(DreamPlayer * p, int tIsBoundToScreen, int tIsCameraFollowingX, int tIsCameraFollowingY)
{
	p->mIsBoundToScreen = tIsBoundToScreen;
	(void)tIsCameraFollowingX;
	(void)tIsCameraFollowingY;
	// TODO

}

static void resetPlayerHitBySlotGeneral(DreamPlayer * p, int tSlot) {
	p->mNotHitBy[tSlot].mFlag2Amount = 0;
	p->mNotHitBy[tSlot].mNow = 0;
	p->mNotHitBy[tSlot].mIsActive = 1;
}

void resetPlayerHitBy(DreamPlayer * p, int tSlot)
{
	resetPlayerHitBySlotGeneral(p, tSlot);
	p->mNotHitBy[tSlot].mIsHitBy = 1;
}

void resetPlayerNotHitBy(DreamPlayer * p, int tSlot)
{
	resetPlayerHitBySlotGeneral(p, tSlot);
	p->mNotHitBy[tSlot].mIsHitBy = 0;
}

void setPlayerNotHitByFlag1(DreamPlayer * p, int tSlot, char * tFlag)
{
	strcpy(p->mNotHitBy[tSlot].mFlag1, tFlag);
	turnStringLowercase(p->mNotHitBy[tSlot].mFlag1);
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

void addPlayerNotHitByFlag2(DreamPlayer * p, int tSlot, char * tFlag)
{

	char* nFlag = allocMemory(strlen(tFlag) + 5);
	copyOverCleanFlag2(nFlag, tFlag);
	assert(strlen(nFlag) == 2);
	turnStringLowercase(nFlag);

	assert(p->mNotHitBy[tSlot].mFlag2Amount < MAXIMUM_HITSLOT_FLAG_2_AMOUNT);

	strcpy(p->mNotHitBy[tSlot].mFlag2[p->mNotHitBy[tSlot].mFlag2Amount], nFlag);
	p->mNotHitBy[tSlot].mFlag2Amount++;

	freeMemory(nFlag);
}

void setPlayerNotHitByTime(DreamPlayer * p, int tSlot, int tTime)
{
	p->mNotHitBy[tSlot].mTime = tTime;
}

int getDefaultPlayerSparkNumberIsInPlayerFile(DreamPlayer * p)
{
	return p->mConstants.mHeader.mIsSparkNoInPlayerFile;
}

int getDefaultPlayerSparkNumber(DreamPlayer * p)
{
	return p->mConstants.mHeader.mSparkNo;
}

int getDefaultPlayerGuardSparkNumberIsInPlayerFile(DreamPlayer * p)
{
	return p->mConstants.mHeader.mIsGuardSparkNoInPlayerFile;
}

int getDefaultPlayerGuardSparkNumber(DreamPlayer * p)
{
	return p->mConstants.mHeader.mGuardSparkNo;
}

int isPlayerProjectile(DreamPlayer * p)
{
	return p->mIsProjectile;
}

int isPlayerHomeTeam(DreamPlayer * p)
{
	return p->mRootID; // TODO: properly after teams are implemented
}

void setPlayerDrawOffsetX(DreamPlayer* p, double tValue, int tCoordinateP) {
	tValue = transformDreamCoordinates(tValue, tCoordinateP, getPlayerCoordinateP(p));
	Position pos = getMugenAnimationPosition(p->mAnimationID);
	Position basePos = getDreamStageCoordinateSystemOffset(getPlayerCoordinateP(p));
	Position newPos = vecAdd(basePos, makePosition(p->mConstants.mSizeData.mDrawOffset.x, p->mConstants.mSizeData.mDrawOffset.y, 0));
	newPos.x += tValue;
	newPos.y = pos.y;
	newPos.z = pos.z;

	setMugenAnimationPosition(p->mAnimationID, newPos);
}

void setPlayerDrawOffsetY(DreamPlayer* p, double tValue, int tCoordinateP) {
	tValue = transformDreamCoordinates(tValue, tCoordinateP, getPlayerCoordinateP(p));
	Position pos = getMugenAnimationPosition(p->mAnimationID);
	Position basePos = getDreamStageCoordinateSystemOffset(getPlayerCoordinateP(p));
	Position newPos = vecAdd(basePos, makePosition(p->mConstants.mSizeData.mDrawOffset.x, p->mConstants.mSizeData.mDrawOffset.y, 0));
	newPos.x = pos.x;
	newPos.y += tValue;
	newPos.z = pos.z;

	setMugenAnimationPosition(p->mAnimationID, newPos);
}

void setPlayerOneFrameTransparency(DreamPlayer * p, BlendType tType, int tAlphaSource, int tAlphaDest)
{
	setMugenAnimationBlendType(p->mAnimationID, tType);
	setMugenAnimationTransparency(p->mAnimationID, tAlphaSource / 256.0);
	(void)tAlphaDest; // TODO: use
	p->mTransparencyFlag = 1;
}

void addPlayerDust(DreamPlayer * p, int tDustIndex, Position tPos, int tSpacing)
{
	int time = getDreamGameTime();
	int since = time - p->mDustClouds[tDustIndex].mLastDustTime;
	if (since < tSpacing) return;

	p->mDustClouds[tDustIndex].mLastDustTime = time;
	Position playerPosition = getPlayerPosition(p, getPlayerCoordinateP(p));
	Position pos = vecAdd(tPos, playerPosition); 
	pos.z = PLAYER_Z + 1; // TODO: fix z
	addDreamDustCloud(pos, getPlayerIsFacingRight(p), getPlayerCoordinateP(p));
}

VictoryType getPlayerVictoryType(DreamPlayer * p)
{
	DreamPlayer* otherPlayer = getPlayerOtherPlayer(p);

	// TODO: suicide, teammate
	if (isTimerFinished()) {
		return VICTORY_TYPE_TIMEOVER;
	}
	else if (isPlayerGuarding(otherPlayer)) {
		return VICTORY_TYPE_CHEESE;
	}
	else if (getActiveHitDataAttackType(otherPlayer) == MUGEN_ATTACK_TYPE_THROW) {
		return VICTORY_TYPE_THROW;
	}
	else if (getActiveHitDataAttackClass(otherPlayer) == MUGEN_ATTACK_CLASS_HYPER) {
		return VICTORY_TYPE_HYPER;
	}
	else if (getActiveHitDataAttackClass(otherPlayer) == MUGEN_ATTACK_CLASS_SPECIAL) {
		return VICTORY_TYPE_SPECIAL;
	}
	else {
		return VICTORY_TYPE_NORMAL;
	}
}

int isPlayerAtFullLife(DreamPlayer * p)
{
	return getPlayerLife(p) == getPlayerLifeMax(p);
}

void setPlayersToTrainingMode()
{
	gData.mIsInTrainingMode = 1;
}

void setPlayersToRealFightMode()
{
	gData.mIsInTrainingMode = 0;
}
