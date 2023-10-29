#include "playerdefinition.h"

#include <assert.h>
#include <string.h>
#include <algorithm>

#include <prism/file.h>
#include <prism/physicshandler.h>
#include <prism/log.h>
#include <prism/system.h>
#include <prism/timer.h>
#include <prism/math.h>
#include <prism/mugendefreader.h>
#include <prism/mugenanimationreader.h>
#include <prism/mugenanimationhandler.h>
#include <prism/mugentexthandler.h>
#include <prism/screeneffect.h>
#include <prism/input.h>
#include <prism/soundeffect.h>
#include <prism/collisionhandler.h>

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
#include "mugenexplod.h"
#include "pausecontrollers.h"
#include "config.h"
#include "mugenassignmentevaluator.h"

using namespace std;

#define SHADOW_Z 32
#define REFLECTION_Z 33
#define DUST_Z 47
#define WIDTH_LINE_Z 48
#define CENTER_POINT_Z 49
#define PLAYER_DEBUG_TEXT_Z 79

static struct {
	DreamPlayerHeader mPlayerHeader[2];
	DreamPlayer mPlayers[2];
	int mUniqueIDCounter;
	int mIsInTrainingMode;
	int mIsCollisionDebugActive;
	MemoryStack* mMemoryStack;
	int mIsLoading;
	int mHasLoadedSprites;

	List mAllPlayers; // contains DreamPlayer
	std::set<DreamPlayer*> mAllProjectiles;
	std::map<int, DreamPlayer> mHelperStore;
} gPlayerDefinition;

static void loadPlayerHeaderFromScript(DreamPlayerHeader* tHeader, MugenDefScript* tScript) {
	getMugenDefStringOrDefault(tHeader->mConstants.mName, tScript, "info", "name", "Character");
	getMugenDefStringOrDefault(tHeader->mConstants.mDisplayName, tScript, "info", "displayname", tHeader->mConstants.mName);
	getMugenDefStringOrDefault(tHeader->mConstants.mVersion, tScript, "info", "versiondate", "09,09,2017");
	getMugenDefStringOrDefault(tHeader->mConstants.mMugenVersion, tScript, "info", "mugenversion", "1.1");
	getMugenDefStringOrDefault(tHeader->mConstants.mAuthor, tScript, "info", "author", "John Doe");
	getMugenDefStringOrDefault(tHeader->mConstants.mPaletteDefaults, tScript, "info", "pal.defaults", "1");

	tHeader->mConstants.mLocalCoordinates = getMugenDefVectorIOrDefault(tScript, "info", "localcoord", Vector3DI(320, 240, 0));
	if (!tHeader->mConstants.mLocalCoordinates.x) tHeader->mConstants.mLocalCoordinates.x = 320;
	if (!tHeader->mConstants.mLocalCoordinates.y) tHeader->mConstants.mLocalCoordinates.y = 240;
}

static void loadOptionalStateFiles(MugenDefScript* tScript, char* tPath, DreamPlayer* tPlayer) {
	char file[200];
	char scriptPath[1024];
	char name[100];

	int i;
	for (i = 0; i < 100; i++) {
		sprintf(name, "st%d", i);
		getMugenDefStringOrDefault(file, tScript, "files", name, "");
		sprintf(scriptPath, "%s%s", tPath, file);
		if (!isFile(scriptPath)) continue;
		
		loadDreamMugenStateDefinitionsFromFile(&tPlayer->mHeader->mFiles.mConstants.mStates, scriptPath);
		logMemoryState();
	}
}

static void setPlayerFaceDirection(DreamPlayer* p, FaceDirection tDirection);

static void setPlayerExternalDependencies(DreamPlayer* tPlayer) {
	tPlayer->mPhysicsElement = addToPhysicsHandler(getDreamPlayerStartingPositionInCameraCoordinates(tPlayer->mRootID).xyz(0.0));
	setPlayerPhysics(tPlayer, MUGEN_STATE_PHYSICS_STANDING);
	setPlayerStateMoveType(tPlayer, MUGEN_STATE_MOVE_TYPE_IDLE);
	setPlayerStateType(tPlayer, MUGEN_STATE_TYPE_STANDING);

	const auto p = getDreamStageCoordinateSystemOffset(getDreamMugenStageHandlerCameraCoordinateP()).xyz(calculateSpriteZFromSpritePriority(0, tPlayer->mRootID, 0));
	tPlayer->mActiveAnimations = &tPlayer->mHeader->mFiles.mAnimations;
	tPlayer->mAnimationElement = addMugenAnimation(getMugenAnimation(&tPlayer->mHeader->mFiles.mAnimations, 0), gPlayerDefinition.mIsLoading ? NULL : &tPlayer->mHeader->mFiles.mSprites, p);
	setMugenAnimationDrawScale(tPlayer->mAnimationElement, tPlayer->mHeader->mFiles.mConstants.mSizeData.mScale * getPlayerToCameraScale(tPlayer));
	setMugenAnimationCameraEffectPositionReference(tPlayer->mAnimationElement, getDreamMugenStageHandlerCameraEffectPositionReference());
	setMugenAnimationCameraScaleReference(tPlayer->mAnimationElement, getDreamMugenStageHandlerCameraZoomReference());
	setMugenAnimationBasePosition(tPlayer->mAnimationElement, getHandledPhysicsPositionReference(tPlayer->mPhysicsElement));
	setMugenAnimationCameraPositionReference(tPlayer->mAnimationElement, getDreamMugenStageHandlerCameraPositionReference());
	setMugenAnimationAttackCollisionActive(tPlayer->mAnimationElement, getDreamPlayerAttackCollisionList(tPlayer), playerReversalHitCB, tPlayer, getPlayerHitDataReference(tPlayer));
	setMugenAnimationPassiveCollisionActive(tPlayer->mAnimationElement, getDreamPlayerPassiveCollisionList(tPlayer), playerHitCB, tPlayer, getPlayerHitDataReference(tPlayer));
	tPlayer->mRegisteredStateMachine = registerDreamMugenStateMachine(&tPlayer->mHeader->mFiles.mConstants.mStates, tPlayer);
}

static int getPlayerRandomPaletteIndex(MugenDefScript* tScript) {
	std::vector<int> possibleValues;
	static const auto MAXIMUM_PALETTE_AMOUNT = 16;
	for (int i = 0; i <= MAXIMUM_PALETTE_AMOUNT; i++) {
		std::stringstream ss;
		ss << "pal" << i;
		if (isMugenDefStringVariable(tScript, "files", ss.str().c_str())) {
			possibleValues.push_back(i);
		}
	}
	if (possibleValues.empty()) return 0;
	else return possibleValues[randfromInteger(0, int(possibleValues.size()) - 1)];
}

static int parsePlayerPreferredPalette(int tPalette, MugenDefScript* tScript) {
	if (tPalette == -1) {
		return getPlayerRandomPaletteIndex(tScript);
	}
	else {
		return tPalette;
	}
}

static void loadPlayerFiles(char* tPath, DreamPlayer* tPlayer, MugenDefScript* tScript) {
	char file[200];
	char path[1024];
	char scriptPath[1024];
	char name[100];
	getPathToFile(path, tPath);

	getMugenDefStringOrDefault(file, tScript, "files", "cmd", "");
	assert(strcmp("", file));
	sprintf(scriptPath, "%s%s", path, file);
	tPlayer->mHeader->mFiles.mCommands = loadDreamMugenCommandFile(scriptPath);
	logMemoryState();
	tPlayer->mCommandID = registerDreamMugenCommands(tPlayer->mControllerID, &tPlayer->mHeader->mFiles.mCommands);
	logMemoryState();

	setDreamAssignmentCommandLookupID(tPlayer->mCommandID);
	getMugenDefStringOrDefault(file, tScript, "files", "cns", "");
	assert(strcmp("", file));
	sprintf(scriptPath, "%s%s", path, file);
	tPlayer->mHeader->mFiles.mConstants = loadDreamMugenConstantsFile(scriptPath);
	tPlayer->mCustomSizeData = tPlayer->mHeader->mFiles.mConstants.mSizeData;
	logMemoryState();
	
	getMugenDefStringOrDefault(file, tScript, "files", "stcommon", "");
	sprintf(scriptPath, "%s%s", path, file);
	if (isFile(scriptPath)) {
		loadDreamMugenStateDefinitionsFromFile(&tPlayer->mHeader->mFiles.mConstants.mStates, scriptPath, 1);
	}
	else {
		sprintf(scriptPath, "%sdata/%s", getDolmexicaAssetFolder().c_str(), file);
		if (isFile(scriptPath)) {
			loadDreamMugenStateDefinitionsFromFile(&tPlayer->mHeader->mFiles.mConstants.mStates, scriptPath, 1);
		}
	}
	logMemoryState();

	getMugenDefStringOrDefault(file, tScript, "files", "st", "");
	sprintf(scriptPath, "%s%s", path, file);
	if (isFile(scriptPath)) {
		loadDreamMugenStateDefinitionsFromFile(&tPlayer->mHeader->mFiles.mConstants.mStates, scriptPath);
	}
	logMemoryState();

	loadOptionalStateFiles(tScript, path, tPlayer);

	getMugenDefStringOrDefault(file, tScript, "files", "cmd", "");
	assert(strcmp("", file));
	sprintf(scriptPath, "%s%s", path, file);
	loadDreamMugenStateDefinitionsFromFile(&tPlayer->mHeader->mFiles.mConstants.mStates, scriptPath);

	resetDreamAssignmentCommandLookupID();

	getMugenDefStringOrDefault(file, tScript, "files", "anim", "");
	assert(strcmp("", file));
	sprintf(scriptPath, "%s%s", path, file);
	tPlayer->mHeader->mFiles.mAnimations = loadMugenAnimationFile(scriptPath);
	logMemoryState();

	char palettePath[1024];
	const auto preferredPalette = parsePlayerPreferredPalette(tPlayer->mPreferredPalette, tScript);
	sprintf(name, "pal%d", preferredPalette);
	getMugenDefStringOrDefault(file, tScript, "files", name, "");
	int hasPalettePath = strcmp("", file);
	sprintf(palettePath, "%s%s", path, file);
	getMugenDefStringOrDefault(file, tScript, "files", "sprite", "");
	assert(strcmp("", file));
	sprintf(scriptPath, "%s%s", path, file);

	tPlayer->mHeader->mFiles.mHasPalettePath = hasPalettePath;
	tPlayer->mHeader->mFiles.mPalettePath = copyToAllocatedString(palettePath);
	tPlayer->mHeader->mFiles.mSpritePath = copyToAllocatedString(scriptPath);

	getMugenDefStringOrDefault(file, tScript, "files", "sound", "");
	sprintf(scriptPath, "%s%s", path, file);
	if (isFile(scriptPath)) {
		setSoundEffectCompression(1);
		tPlayer->mHeader->mFiles.mSounds = loadMugenSoundFile(scriptPath);
		setSoundEffectCompression(0);
	}
	else {
		tPlayer->mHeader->mFiles.mSounds = createEmptyMugenSoundFile();
	}
	logMemoryState();

	setPlayerExternalDependencies(tPlayer);

	if (getPlayerAILevel(tPlayer) || (getGameMode() == GAME_MODE_TRAINING && tPlayer->mRootID == 1)) {
		setDreamAIActive(tPlayer);
	}
}

static void initHitDefAttributeSlot(DreamHitDefAttributeSlot* tSlot) {
	tSlot->mIsActive = 0;
	tSlot->mNow = 0;
}

static void resetHelperState(DreamPlayer* p) {
	p->mHelpers = new_list();
	p->mProjectiles = new_int_map();

	p->mReceivedHitData.clear();
	p->mReceivedReversalDefPlayers.clear();
	p->mActiveTargets.clear();

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
	p->mWidthFlag = 0;
	p->mNoJuggleCheckFlag = 0;
	p->mDrawOffset = Vector3D(0, 0, 0);
	p->mJumpFlank = 0;
	p->mAirJumpCounter = 0;

	p->mIsHitOver = 1;
	p->mIsFalling = 0;
	p->mCanRecoverFromFall = 0;

	p->mIsHitShakeActive = 0;
	p->mHitShakeNow = 0;
	p->mHitShakeDuration = 0;

	p->mIsHitOverWaitActive = 0;
	p->mHitOverNow = 0;
	p->mHitOverDuration = 0;

	p->mIsAngleActive = 0;
	p->mAngle = 0;
	p->mTempScale = Vector2D(1, 1);

	p->mCheeseWinFlag = 0;
	p->mSuicideWinFlag = 0;

	p->mDefenseMultiplier = 1;
	p->mSuperDefenseMultiplier = 1;

	p->mIsFrozen = 0;

	p->mIsLyingDown = 0;
	p->mIsHitPaused = 0;
	p->mMoveContactCounter = 0;

	p->mSuperMoveTime = 0;
	p->mPauseMoveTime = 0;

	p->mHitCount = 0;
	p->mFallAmountInCombo = 0;

	p->mAttackMultiplier = 1;

	initPlayerHitData(p);
	initPlayerAfterImage(p);

	p->mFaceDirection = FACE_DIRECTION_RIGHT;

	p->mMoveReversed = 0;
	p->mMoveHit = 0;
	p->mMoveGuarded = 0;
	p->mLastHitGuarded = 0;

	p->mHasLastContactProjectile = 0;

	p->mRoundsExisted = 0;

	p->mIsBound = 0;
	p->mBoundHelpers = new_list();

	p->mIsGuardingInternally = 0;

	p->mIsBeingJuggled = 0;
	p->mComboCounter = 0;
	p->mDisplayedComboCounter = 0;
	p->mIsDestroyed = 0;

	p->mHasOwnPalette = 1;

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
	p->mParent = getPlayerRoot(p);
	p->mHelperIDInParent = -1;
	p->mHelperIDInRoot = -1;
	p->mHelperIDInStore = -1;

	p->mIsProjectile = 0;
	p->mProjectileID = -1;
	p->mProjectileDataID = -1;

	p->mPower = 0; 
	
	p->mIsBoundToScreenForever = 1;
	p->mIsBoundToScreenForTick = 1;
	p->mIsCameraFollowing = Vector3DI(1, 1, 1);

	p->mTimeDilatationNow = 0.0;
	p->mTimeDilatation = 1.0;
	p->mTimeDilatationUpdates = 1;
}

static void loadPlayerStateWithConstantsLoaded(DreamPlayer* p) {
	p->mHeader->mFiles.mConstants.mHeader.mLife = (int)(p->mHeader->mFiles.mConstants.mHeader.mLife * getLifeStartPercentage());
	p->mLife = (int)(p->mHeader->mFiles.mConstants.mHeader.mLife * p->mStartLifePercentage);
	setPlayerDrawOffsetX(p, 0, getPlayerCoordinateP(p));
	setPlayerDrawOffsetY(p, 0, getPlayerCoordinateP(p));
	setPlayerHitDataCoordinateP(p);

	if (doesDreamPlayerStartFacingLeft(p->mRootID)) {
		setPlayerFaceDirection(p, FACE_DIRECTION_LEFT);
	}
	else {
		setPlayerFaceDirection(p, FACE_DIRECTION_RIGHT);
	}
}

static void loadPlayerShadow(DreamPlayer* p) {
	const auto pos = getDreamStageCoordinateSystemOffset(getDreamMugenStageHandlerCameraCoordinateP()).xyz(SHADOW_Z);
	p->mShadow.mShadowPosition = *getHandledPhysicsPositionReference(p->mPhysicsElement);
	p->mShadow.mShadowPosition.y += getPlayerShadowOffset(p, getDreamMugenStageHandlerCameraCoordinateP());
	p->mShadow.mAnimationElement = addMugenAnimation(getMugenAnimation(&p->mHeader->mFiles.mAnimations, getMugenAnimationAnimationNumber(p->mAnimationElement)), gPlayerDefinition.mIsLoading ? NULL : &p->mHeader->mFiles.mSprites, pos);
	setMugenAnimationBasePosition(p->mShadow.mAnimationElement, &p->mShadow.mShadowPosition);
	setMugenAnimationCameraPositionReference(p->mShadow.mAnimationElement, getDreamMugenStageHandlerCameraPositionReference());
	setMugenAnimationDrawScale(p->mShadow.mAnimationElement, Vector2D(1, -getDreamStageShadowScaleY()) * p->mHeader->mFiles.mConstants.mSizeData.mScale * getPlayerToCameraScale(p));
	setMugenAnimationCameraEffectPositionReference(p->mShadow.mAnimationElement, getDreamMugenStageHandlerCameraEffectPositionReference());
	setMugenAnimationCameraScaleReference(p->mShadow.mAnimationElement, getDreamMugenStageHandlerCameraZoomReference());
	Vector3D color = getDreamStageShadowColor();
	if (isOnDreamcast()) {
		setMugenAnimationColor(p->mShadow.mAnimationElement, 0, 0, 0);
	}
	else {
		setMugenAnimationColorSolid(p->mShadow.mAnimationElement, color.x, color.y, color.z);
	}
	setMugenAnimationTransparency(p->mShadow.mAnimationElement, getDreamStageShadowTransparency() * isDrawingShadowsConfig());
	setMugenAnimationFaceDirection(p->mShadow.mAnimationElement, getMugenAnimationIsFacingRight(p->mAnimationElement));
}

static void loadPlayerReflection(DreamPlayer* p) {
	const auto pos = getDreamStageCoordinateSystemOffset(getDreamMugenStageHandlerCameraCoordinateP()).xyz(REFLECTION_Z);
	p->mReflection.mPosition = *getHandledPhysicsPositionReference(p->mPhysicsElement);
	p->mReflection.mAnimationElement = addMugenAnimation(getMugenAnimation(&p->mHeader->mFiles.mAnimations, getMugenAnimationAnimationNumber(p->mAnimationElement)), gPlayerDefinition.mIsLoading ? NULL : &p->mHeader->mFiles.mSprites, pos);

	setMugenAnimationBasePosition(p->mReflection.mAnimationElement, &p->mReflection.mPosition);
	setMugenAnimationCameraPositionReference(p->mReflection.mAnimationElement, getDreamMugenStageHandlerCameraPositionReference());
	setMugenAnimationDrawScale(p->mReflection.mAnimationElement, Vector2D(1, -1) * p->mHeader->mFiles.mConstants.mSizeData.mScale * getPlayerToCameraScale(p));
	setMugenAnimationCameraEffectPositionReference(p->mReflection.mAnimationElement, getDreamMugenStageHandlerCameraEffectPositionReference());
	setMugenAnimationCameraScaleReference(p->mReflection.mAnimationElement, getDreamMugenStageHandlerCameraZoomReference());
	setMugenAnimationBlendType(p->mReflection.mAnimationElement, BLEND_TYPE_ADDITION);
	setMugenAnimationTransparency(p->mReflection.mAnimationElement, getDreamStageReflectionTransparency());
	setMugenAnimationFaceDirection(p->mReflection.mAnimationElement, getMugenAnimationIsFacingRight(p->mAnimationElement));
}

static void loadPlayerDebug(DreamPlayer* p) {
	setMugenAnimationCollisionDebug(p->mAnimationElement, gPlayerDefinition.mIsCollisionDebugActive);

	char text[2];
	text[0] = '\0';
	p->mDebug.mCollisionTextID = addMugenText(text, Vector3D(0, 0, 1), -1);
	setMugenTextAlignment(p->mDebug.mCollisionTextID, MUGEN_TEXT_ALIGNMENT_CENTER);
}

static void loadSinglePlayerFromMugenDefinition(DreamPlayer* p)
{
	MugenDefScript script; 
	loadMugenDefScript(&script, p->mHeader->mFiles.mDefinitionPath);

	loadPlayerState(p);
	loadPlayerHeaderFromScript(p->mHeader, &script);
	loadPlayerFiles(p->mHeader->mFiles.mDefinitionPath, p, &script);
	loadPlayerShadow(p);
	loadPlayerReflection(p);
	loadPlayerStateWithConstantsLoaded(p);
	loadPlayerDebug(p);
	unloadMugenDefScript(&script);
}

void loadPlayers(MemoryStack* tMemoryStack) {
	setProfilingSectionMarkerCurrentFunction();

	gPlayerDefinition.mIsLoading = 1;

	gPlayerDefinition.mHelperStore.clear();
	gPlayerDefinition.mAllPlayers = new_list();
	gPlayerDefinition.mAllProjectiles.clear();
	list_push_back(&gPlayerDefinition.mAllPlayers, &gPlayerDefinition.mPlayers[0]);
	list_push_back(&gPlayerDefinition.mAllPlayers, &gPlayerDefinition.mPlayers[1]);

	gPlayerDefinition.mMemoryStack = tMemoryStack;
	int i = 0;
	for (i = 0; i < 2; i++) {
		gPlayerDefinition.mPlayers[i].mHeader = &gPlayerDefinition.mPlayerHeader[i];
		gPlayerDefinition.mPlayers[i].mRoot = &gPlayerDefinition.mPlayers[i];
		gPlayerDefinition.mPlayers[i].mOtherPlayer = &gPlayerDefinition.mPlayers[i ^ 1];
		gPlayerDefinition.mPlayers[i].mRootID = i;
		gPlayerDefinition.mPlayers[i].mControllerID = i;
		loadSinglePlayerFromMugenDefinition(&gPlayerDefinition.mPlayers[i]);
	}

	gPlayerDefinition.mIsLoading = 0;
	gPlayerDefinition.mHasLoadedSprites = 0;
}

static void loadSinglePlayerSprites(DreamPlayer* tPlayer) {
	setMugenSpriteFileReaderToUsePalette(tPlayer->mRootID);
	tPlayer->mHeader->mFiles.mSprites = loadMugenSpriteFile(tPlayer->mHeader->mFiles.mSpritePath, tPlayer->mHeader->mFiles.mHasPalettePath, tPlayer->mHeader->mFiles.mPalettePath);
	setMugenSpriteFileReaderToNotUsePalette();
	logMemoryState();

	setMugenAnimationSprites(tPlayer->mAnimationElement, &tPlayer->mHeader->mFiles.mSprites);
	setMugenAnimationSprites(tPlayer->mShadow.mAnimationElement, &tPlayer->mHeader->mFiles.mSprites);
	setMugenAnimationSprites(tPlayer->mReflection.mAnimationElement, &tPlayer->mHeader->mFiles.mSprites);
}

void loadPlayerSprites() {
	int i;
	for (i = 0; i < 2; i++) {
		loadSinglePlayerSprites(&gPlayerDefinition.mPlayers[i]);
	}

	gPlayerDefinition.mHasLoadedSprites = 1;
}

static void unloadHelperStateWithoutFreeingOwnedHelpersAndProjectile(DreamPlayer* p) {
	// projectiles shouldn't have helpers, so no need to move them
	delete_list(&p->mHelpers);
	delete_int_map(&p->mProjectiles);
	p->mReceivedHitData.clear();
	p->mReceivedReversalDefPlayers.clear();
	p->mActiveTargets.clear();

	delete_list(&p->mBoundHelpers);
}

static void unloadPlayerState(DreamPlayer* p) {
	p->mReceivedHitData.clear();
	p->mReceivedReversalDefPlayers.clear();
	p->mActiveTargets.clear();
	clearPlayerHitData(p);
	removePlayerAfterImage(p);
}

static void unloadPlayerFiles(DreamPlayerHeader* tHeader) {
	unloadDreamMugenConstantsFile(&tHeader->mFiles.mConstants);
	unloadDreamMugenCommandFile(&tHeader->mFiles.mCommands);
	unloadMugenAnimationFile(&tHeader->mFiles.mAnimations);
	unloadMugenSpriteFile(&tHeader->mFiles.mSprites);
	unloadMugenSoundFile(&tHeader->mFiles.mSounds);
}

static void unloadSinglePlayer(DreamPlayer* p, DreamPlayerHeader* tHeader) {
	unloadPlayerState(p);
	unloadPlayerFiles(tHeader);
}

static void unloadPlayerHeader(int i) {
	gPlayerDefinition.mPlayerHeader[i].mFiles.mConstants.mStates.mStates = std::map<int, DreamMugenState>();

	gPlayerDefinition.mPlayerHeader[i].mCustomOverrides.mHasCustomDisplayName = 0;
}

void unloadPlayers() {
	setProfilingSectionMarkerCurrentFunction();

	int i;
	for (i = 0; i < 2; i++) {
		unloadPlayerHeader(i);
		unloadSinglePlayer(&gPlayerDefinition.mPlayers[i], &gPlayerDefinition.mPlayerHeader[i]);
	}

	gPlayerDefinition.mHelperStore.clear();
	gPlayerDefinition.mAllProjectiles.clear();
}

static void removeSingleHelperCB(void* /*tCaller*/, void* tData) {
	DreamPlayer* p = (DreamPlayer*)tData;
	list_map(&p->mHelpers, removeSingleHelperCB, NULL);
	destroyPlayer(p);
}

static void removePlayerHelpers(DreamPlayer* p) {
	list_map(&p->mHelpers, removeSingleHelperCB, NULL);
}

static void resetPlayerVariables(DreamPlayer* p) {
	for (int i = 0; i < getPlayerDataIntPersistIndex(p); i++) {
		setPlayerVariable(p, i, 0);
	}

	for (int i = 0; i < getPlayerDataFloatPersistIndex(p); i++) {
		setPlayerFloatVariable(p, i, 0);
	}
}

static void forceUnpausePlayer(DreamPlayer* p);

static void resetSinglePlayer(DreamPlayer* p) {
	p->mIsAlive = 1;

	p->mLife = (int)(p->mHeader->mFiles.mConstants.mHeader.mLife * p->mStartLifePercentage);
	setDreamLifeBarPercentage(p, p->mStartLifePercentage);

	forceUnpausePlayer(p);
	resetPlayerPosition(p);

	if (doesDreamPlayerStartFacingLeft(p->mRootID)) {
		setPlayerFaceDirection(p, FACE_DIRECTION_LEFT);
	}
	else {
		setPlayerFaceDirection(p, FACE_DIRECTION_RIGHT);
	}

	removePlayerHelpers(p);
	resetPlayerVariables(p);
}

void resetPlayers()
{
	resetSinglePlayer(&gPlayerDefinition.mPlayers[0]);
	resetSinglePlayer(&gPlayerDefinition.mPlayers[1]);
	removeAllExplods();
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
	resetSinglePlayerEntirely(&gPlayerDefinition.mPlayers[0]);
	resetSinglePlayerEntirely(&gPlayerDefinition.mPlayers[1]);
}

void resetPlayerPosition(DreamPlayer* p)
{
	setPlayerPosition(p, getDreamPlayerStartingPositionInCameraCoordinates(p->mRootID), getDreamMugenStageHandlerCameraCoordinateP());
}

static int isPlayerGuarding(DreamPlayer* p);

static void updateWalking(DreamPlayer* p) {
	if (p->mIsHelper) return;
	if (p->mNoWalkFlag) {
		p->mNoWalkFlag = 0;
		return;
	}

	if (!p->mIsInControl) return;
	if (isPlayerGuarding(p)) return;

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
	if (p->mIsHelper) return;
	if (!p->mIsInControl) return;
	if (getPlayerStateType(p) != MUGEN_STATE_TYPE_AIR) return;

	int hasJumpLeft = p->mAirJumpCounter < p->mHeader->mFiles.mConstants.mMovementData.mAllowedAirJumpAmount;
	int hasMinimumHeight = -getPlayerPositionY(p, getPlayerCoordinateP(p)) >= p->mHeader->mFiles.mConstants.mMovementData.mAirJumpMinimumHeight;

	if (isPlayerCommandActive(p, "holdup") && p->mJumpFlank && hasJumpLeft && hasMinimumHeight) {
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
	if (p->mIsHelper) return;
	if (!p->mIsInControl) return;
	if (getPlayerStateType(p) != MUGEN_STATE_TYPE_STANDING) return;


	if (isPlayerCommandActive(p, "holdup")) {
		changePlayerState(p, 40);
	}
}

static void updateLanding(DreamPlayer* p) {
	if (p->mIsHelper) return;
	if (p->mNoLandFlag) {
		p->mNoLandFlag = 0;
		return;
	}

	Position pos = *getHandledPhysicsPositionReference(p->mPhysicsElement);
	if (getPlayerPhysics(p) != MUGEN_STATE_PHYSICS_AIR) return;
	if (isPlayerPaused(p)) return;


	Velocity vel = *getHandledPhysicsVelocityReference(p->mPhysicsElement);
	if (pos.y >= 0 && vel.y >= 0) {
		setPlayerControl(p, 1);
		changePlayerState(p, 52);
	}
}

static void updateCrouchingDown(DreamPlayer* p) {
	if (p->mIsHelper) return;
	if (!p->mIsInControl) return;
	if (getPlayerStateType(p) != MUGEN_STATE_TYPE_STANDING) return;


	if (isPlayerCommandActive(p, "holddown")) {
		changePlayerState(p, 10);
	}
}

static void updateStandingUp(DreamPlayer* p) {
	if (p->mIsHelper) return;
	if (!p->mIsInControl) return;
	if (getPlayerStateType(p) != MUGEN_STATE_TYPE_CROUCHING) return;


	if (!isPlayerCommandActive(p, "holddown")) {
		changePlayerState(p, 12);
	}
}

static void setPlayerFaceDirection(DreamPlayer* p, FaceDirection tDirection) {
	if (p->mFaceDirection == tDirection) return;

	setMugenAnimationFaceDirection(p->mAnimationElement, tDirection == FACE_DIRECTION_RIGHT);
	setMugenAnimationFaceDirection(p->mShadow.mAnimationElement, getMugenAnimationIsFacingRight(p->mAnimationElement));
	setMugenAnimationFaceDirection(p->mReflection.mAnimationElement, getMugenAnimationIsFacingRight(p->mAnimationElement));

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

	turnPlayerTowardsOtherPlayer(p);
}

static void updatePositionFreeze(DreamPlayer* p) {
	if (p->mIsFrozen) {
		Position* pos = getHandledPhysicsPositionReference(p->mPhysicsElement);
		*pos = p->mFreezePosition;
		p->mIsFrozen = 0;
	}
}

static void updateGettingUp(DreamPlayer* p) {
	if (p->mIsHelper) return;
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

	p->mLyingDownTime++;
	if (hasPressedAnyButtonFlankSingle(p->mRootID)) p->mLyingDownTime++;
	if (p->mLyingDownTime >= p->mHeader->mFiles.mConstants.mHeader.mLiedownTime) {
		p->mIsLyingDown = 0;
		changePlayerState(p, 5120);
	}

}

static void updateFall(DreamPlayer* p) {
	if (p->mIsHelper) return;
	if (!p->mIsFalling) return;

	if (!p->mIsHitPaused) {
		p->mRecoverTimeSinceHitPause++;
	}
}

static void updateHitPause(DreamPlayer* p) {
	if (!p->mIsHitPaused) {
		if (p->mMoveHit) p->mMoveHit++;
		if (p->mMoveGuarded) p->mMoveGuarded++;
		if (p->mMoveReversed) p->mMoveReversed++;
		if (p->mMoveContactCounter) p->mMoveContactCounter++;
		return;
	}
	
	p->mHitPauseNow++;
	if (p->mHitPauseNow >= p->mHitPauseDuration) {
		setPlayerUnHitPaused(p);
	}
}

static void updateBindingPosition(DreamPlayer* p) {
	DreamPlayer* bindRoot = p->mBoundTarget;
	auto pos = getPlayerPosition(bindRoot, getDreamMugenStageHandlerCameraCoordinateP());

	if (p->mBoundPositionType == PLAYER_BIND_POSITION_TYPE_HEAD) {
		pos = pos + getPlayerHeadPosition(bindRoot, getDreamMugenStageHandlerCameraCoordinateP());
	}
	else if (p->mBoundPositionType == PLAYER_BIND_POSITION_TYPE_MID) {
		pos = pos + getPlayerMiddlePosition(bindRoot, getDreamMugenStageHandlerCameraCoordinateP());
	}

	if (getPlayerIsFacingRight(bindRoot)) {
		pos = pos + p->mBoundOffsetCameraSpace;
	}
	else {
		pos = pos + Vector2D(-p->mBoundOffsetCameraSpace.x, p->mBoundOffsetCameraSpace.y);
	}

	setPlayerPosition(p, pos, getDreamMugenStageHandlerCameraCoordinateP());

	if (p->mBoundFaceSet) {
		if (p->mBoundFaceSet == 1) setPlayerIsFacingRight(p, getPlayerIsFacingRight(bindRoot));
		else setPlayerIsFacingRight(p, !getPlayerIsFacingRight(bindRoot));
	}
}

static void removePlayerBindingInternal(DreamPlayer* tPlayer) {
	if (!tPlayer->mIsBound) return;
	tPlayer->mIsBound = 0;

	DreamPlayer* boundTo = tPlayer->mBoundTarget;
	if (!isPlayer(boundTo)) return;
	list_remove(&boundTo->mBoundHelpers, tPlayer->mBoundID);
}

static void updateBinding(DreamPlayer* p) {
	if (!p->mIsBound) return;
	if (isPlayerPaused(p)) return;

	p->mBoundNow++;
	if (!isPlayer(p->mBoundTarget) || p->mBoundNow >= p->mBoundDuration) {
		removePlayerBindingInternal(p);
		return;
	}

	updateBindingPosition(p);
}

static int isPlayerGuarding(DreamPlayer* p) {
	(void)p;
	return getPlayerState(p) >= 120 && getPlayerState(p) <= 155;
}


static int doesFlagPreventGuarding(DreamPlayer* p) {
	if (getPlayerStateType(p) == MUGEN_STATE_TYPE_STANDING && p->mNoStandGuardFlag) return 1;
	if (getPlayerStateType(p) == MUGEN_STATE_TYPE_CROUCHING && p->mNoCrouchGuardFlag) return 1;
	if (getPlayerStateType(p) == MUGEN_STATE_TYPE_AIR && p->mNoAirGuardFlag) return 1;

	return 0;
}

static void setPlayerGuarding(DreamPlayer* p) {
	p->mIsGuardingInternally = 1;
	changePlayerState(p, 120);
}

static void setPlayerUnguarding(DreamPlayer* p) {
	p->mIsGuardingInternally = 0;

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

static void updateGuarding(DreamPlayer* p) {
	if (p->mIsHelper) return;
	if (isPlayerPaused(p)) return;
	if (!p->mIsInControl) return;
	if (isPlayerGuarding(p)) {
		return;
	}
	if (doesFlagPreventGuarding(p)) return;

	if (isPlayerCommandActive(p, "holdback") && isPlayerBeingAttacked(p) && isPlayerInGuardDistance(p)) {
		setPlayerGuarding(p);
	}
}

static void updateGuardingOver(DreamPlayer* p) {
	if (p->mIsHelper) return;
	if (isPlayerPaused(p)) return;
	if (!p->mIsInControl) return;
	if (getPlayerState(p) != 140) return;
	if (getPlayerAnimationTimeDeltaUntilFinished(p)) return;

	setPlayerUnguarding(p);
}

static int isPlayerGuardingInternally(DreamPlayer* p) {
	return p->mIsGuardingInternally;
}

static void updateGuardingFlags(DreamPlayer* p) {
	if (isPlayerGuardingInternally(p) && doesFlagPreventGuarding(p)) {
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
	DreamPlayer* p = (DreamPlayer*)tData;
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

static void updateProjectileTimeSinceContact(DreamPlayer* p) {
	if (!p->mHasLastContactProjectile) return;

	p->mLastContactProjectileTime++;
}

static void updatePlayerAirJuggle(DreamPlayer* p) {
	if (!p->mIsBeingJuggled) return;
	if (!getPlayerControl(p)) return;

	p->mIsBeingJuggled = 0;
	p->mAirJugglePoints = p->mHeader->mFiles.mConstants.mHeader.mAirJugglePoints;
}

static void updatePush(DreamPlayer* p) {
	if (isPlayerPaused(p)) return;
	if (isPlayerHelper(p)) return;
	if (p->mPushDisabledFlag) return;
	if (p->mIsBound) return;
	if (list_size(&p->mBoundHelpers)) return;

	DreamPlayer* otherPlayer = getPlayerOtherPlayer(p);
	if (otherPlayer->mPushDisabledFlag) return;

	double frontX1 = getPlayerFrontXPlayer(p, getPlayerCoordinateP(p));
	double frontX2 = getPlayerFrontXPlayer(otherPlayer, getPlayerCoordinateP(p));

	double x1 = getPlayerPositionX(p, getPlayerCoordinateP(p));
	double x2 = getPlayerPositionX(otherPlayer, getPlayerCoordinateP(p));
	
	double distX = getPlayerDistanceToFrontOfOtherPlayerX(p, getPlayerCoordinateP(p));
	double distY = getPlayerAxisDistanceY(p, getPlayerCoordinateP(p));

	if (distY >= 0 && distY >= getPlayerHeight(p, getPlayerCoordinateP(p))) return;
	if (distY <= 0 && distY <= -getPlayerHeight(p, getPlayerCoordinateP(p))) return;

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

static void updateAngle(DreamPlayer* p) {
	if (p->mIsAngleActive) {
		setMugenAnimationDrawAngle(p->mAnimationElement, degreesToRadians(p->mAngle));
		p->mIsAngleActive = 0;
	}
	else {
		setMugenAnimationDrawAngle(p->mAnimationElement, 0);
	}
}

static void updateScale(DreamPlayer* p) {
	const auto drawScale = Vector2D(getPlayerScaleX(p), getPlayerScaleY(p)) * p->mTempScale * getPlayerToCameraScale(p);
	setMugenAnimationDrawScale(p->mAnimationElement, drawScale);
	setMugenAnimationDrawScale(p->mShadow.mAnimationElement, Vector2D(1, -getDreamStageShadowScaleY()) * drawScale);
	setMugenAnimationDrawScale(p->mReflection.mAnimationElement, Vector2D(1, -1) * drawScale);
	p->mTempScale = Vector2D(1, 1);
}

static void updatePushFlags() {
	getRootPlayer(0)->mPushDisabledFlag = 0;
	getRootPlayer(1)->mPushDisabledFlag = 0;
}

#define CORNER_CHECK_EPSILON 1

static int isPlayerInCorner(DreamPlayer* p, int tIsCheckingRightCorner) {
	double back = getPlayerBackXStage(p, getPlayerCoordinateP(p));
	double front = getPlayerFrontXStage(p, getPlayerCoordinateP(p));

	if (tIsCheckingRightCorner) {
		double right = getDreamStageRightOfScreenBasedOnPlayer(getPlayerCoordinateP(p));
		double maxX = max(back, front);
		return (maxX > right - CORNER_CHECK_EPSILON);
	}
	else {
		double left = getDreamStageLeftOfScreenBasedOnPlayer(getPlayerCoordinateP(p));
		double minX = min(back, front);
		return (minX < left + CORNER_CHECK_EPSILON);
	}
}

static void updateStageBorderPost(DreamPlayer* p) {
	if (!p->mIsBoundToScreenForTick || !p->mIsBoundToScreenForever) return;

	double left = getDreamStageLeftOfScreenBasedOnPlayer(getPlayerCoordinateP(p));
	double right = getDreamStageRightOfScreenBasedOnPlayer(getPlayerCoordinateP(p));

	double back = getPlayerBackXStage(p, getPlayerCoordinateP(p));
	double front = getPlayerFrontXStage(p, getPlayerCoordinateP(p));
	double minX = min(back, front);
	double maxX = max(back, front);

	double x = getPlayerPositionX(p, getPlayerCoordinateP(p));

	if (minX < left) {
		x += left - minX;
		setPlayerPositionX(p, x, getPlayerCoordinateP(p));
	}
	else if (maxX > right) {
		x -= maxX - right;
		setPlayerPositionX(p, x, getPlayerCoordinateP(p));
	}
}

static void updatePlayerReceivedHits(DreamPlayer* p);

static void updateStageBorderPre(DreamPlayer* p) {
	if (!p->mIsBoundToScreenForTick || !p->mIsBoundToScreenForever) {
		p->mIsBoundToScreenForTick = 1;
		return;
	}

	updateStageBorderPost(p);
}

static void updateKOFlags(DreamPlayer* p) {
	p->mNoKOSoundFlag = 0;
	p->mNoKOSlowdownFlag = 0;
}

static void updateTransparencyFlag(DreamPlayer* p) {
	if (!p->mTransparencyFlag) return;

	setMugenAnimationBlendType(p->mAnimationElement, BLEND_TYPE_NORMAL);
	setMugenAnimationTransparency(p->mAnimationElement, 1);
	setMugenAnimationDestinationTransparency(p->mAnimationElement, 1);

	p->mTransparencyFlag = 0;
}

static void updateShadow(DreamPlayer* p) {
	if (!isDrawingShadowsConfig()) return;

	p->mShadow.mShadowPosition = *getHandledPhysicsPositionReference(p->mPhysicsElement);
	p->mShadow.mShadowPosition.y *= -getDreamStageShadowScaleY();
	p->mShadow.mShadowPosition.y += getPlayerShadowOffset(p, getDreamMugenStageHandlerCameraCoordinateP());
	
	const auto posY = -std::abs(getDreamScreenHeight(getPlayerCoordinateP(p))-getPlayerScreenPositionY(p, getPlayerCoordinateP(p)));
	const auto t = getDreamStageShadowFadeRangeFactor(posY, getPlayerCoordinateP(p));
	setMugenAnimationTransparency(p->mShadow.mAnimationElement, t*getDreamStageShadowTransparency());
}

static void updateReflection(DreamPlayer* p) {
	p->mReflection.mPosition = *getHandledPhysicsPositionReference(p->mPhysicsElement);
	if (!p->mInvisibilityFlag) {
		setMugenAnimationVisibility(p->mReflection.mAnimationElement, p->mReflection.mPosition.y <= 0);
	}
	p->mReflection.mPosition.y *= -1;

}

static void updatePlayerPhysicsClamp(DreamPlayer* p) {
	auto vel = getHandledPhysicsVelocityReference(p->mPhysicsElement);
	if (!vel->x && !vel->y) return;
	const double PHYSICS_EPSILON = 1e-6;
	if (fabs(vel->x) < PHYSICS_EPSILON) vel->x = 0;
	if (fabs(vel->y) < PHYSICS_EPSILON) vel->y = 0;
}

static void clearPlayerReceivedHits(DreamPlayer* p);

static void updateSingleProjectilePreStateMachine(DreamPlayer* p) {
	p->mTimeDilatationNow += p->mTimeDilatation;
	p->mTimeDilatationUpdates = (int)p->mTimeDilatationNow;
	p->mTimeDilatationNow -= p->mTimeDilatationUpdates;
	for (int currentUpdate = 0; currentUpdate < p->mTimeDilatationUpdates; currentUpdate++) {
		if (!isValidPlayerOrProjectile(p)) break;
		updatePlayerReceivedHits(p);
	}
}

static void updateSingleProjectilePreStateMachineCB(void* /*tCaller*/, void* tData) {
	updateSingleProjectilePreStateMachine((DreamPlayer*)tData);
}

static void updateSingleProjectile(DreamPlayer* p) {
	for (int currentUpdate = 0; currentUpdate < p->mTimeDilatationUpdates; currentUpdate++) {
		updateShadow(p);
		updateReflection(p);
		updateScale(p);
		updateAfterImage(p);
		updatePlayerPhysicsClamp(p);
	}
	clearPlayerReceivedHits(p);
}

static void updateSingleProjectileCB(void* tCaller, void* tData) {
	(void)tCaller;
	updateSingleProjectile((DreamPlayer*)tData);
}

static void updatePlayerTrainingMode(DreamPlayer* p) {
	if (!gPlayerDefinition.mIsInTrainingMode) return;
	if (!getPlayerControl(p)) return;

	addPlayerLife(p, p, 3);
	addPlayerPower(p, 50);
}

static void updatePlayerDebug(DreamPlayer* p) {
	if (!gPlayerDefinition.mIsCollisionDebugActive) return;

	double sx = getPlayerScreenPositionX(p, getDreamMugenStageHandlerCameraCoordinateP());
	double sy = getPlayerScreenPositionY(p, getDreamMugenStageHandlerCameraCoordinateP());

	Position pos = Vector3D(sx, sy+5, PLAYER_DEBUG_TEXT_Z);
	setMugenTextPosition(p->mDebug.mCollisionTextID, pos);

	char text[100];
	if (getPlayerStateMoveType(p) == MUGEN_STATE_MOVE_TYPE_ATTACK) {
		sprintf(text, "%s %d H", getPlayerDisplayName(p), getPlayerID(p));
	}
	else {
		sprintf(text, "%s %d", getPlayerDisplayName(p), getPlayerID(p));
	}

	changeMugenText(p->mDebug.mCollisionTextID, text);
}

static void updatePlayerDisplayedCombo(DreamPlayer* p) {
	DreamPlayer* otherPlayer = getPlayerOtherPlayer(p);

	if (getPlayerControl(otherPlayer)) {
		p->mDisplayedComboCounter = 0;
	}
}

static void updateActiveTargets(DreamPlayer* p) {
	if (getPlayerStateMoveType(p) != MUGEN_STATE_MOVE_TYPE_ATTACK && getPlayerControl(p)) {
		p->mActiveTargets.clear();
	}

	auto it = p->mActiveTargets.begin();
	while (it != p->mActiveTargets.end()) {
		auto current = it;
		it++;
		auto otherPlayer = current->second;
		if (!isPlayer(otherPlayer) || getPlayerStateMoveType(otherPlayer) != MUGEN_STATE_MOVE_TYPE_BEING_HIT || getPlayerControl(otherPlayer)) {
			p->mActiveTargets.erase(current);
		}
	}
}

static void updatePlayerHitOver(DreamPlayer* p) {
	if (!p->mIsHitOverWaitActive) return;

	p->mHitOverNow++;
	if (p->mHitOverNow >= p->mHitOverDuration) {
		setPlayerHitOver(p);
	}
}

static void updatePlayerHitShake(DreamPlayer* p) {
	if (!p->mIsHitShakeActive) return;

	p->mHitShakeNow++;
	if (p->mHitShakeNow > p->mHitShakeDuration) {
		setPlayerHitShakeOver(p);
	}
}

static void setSinglePlayerSpeed(DreamPlayer* p, double tSpeed);

static int updatePlayerSuperPauseStopAndReturnIfStopped(DreamPlayer* p) {
	if(!isDreamSuperPauseActive()) return 0;

	const auto timeSinceSuperPauseStarted = getDreamSuperPauseTimeSinceStart();
	const auto owningPlayerMoveTime = (p == getDreamSuperPauseOwner()) ? getDreamSuperPauseMoveTime() : 0;
	const auto playerMoveTime = std::max(p->mSuperMoveTime, owningPlayerMoveTime);
	if (playerMoveTime <= timeSinceSuperPauseStarted) {
		setSinglePlayerSpeed(p, 0);
		return 1;
	}

	return 0;
}

static int updatePlayerPauseStopAndReturnIfStopped(DreamPlayer* p) {
	if (!isDreamPauseActive() || isDreamSuperPauseActive()) return 0;

	const auto timeSincePauseStarted = getDreamPauseTimeSinceStart();
	const auto owningPlayerMoveTime = (p == getDreamPauseOwner()) ? getDreamPauseMoveTime() : 0;
	const auto playerMoveTime = std::max(p->mPauseMoveTime, owningPlayerMoveTime);
	if (playerMoveTime <= timeSincePauseStarted) {
		setSinglePlayerSpeed(p, 0);
		return 1;
	}

	return 0;
}

static void updatePlayerDestruction(DreamPlayer* p) {
	if (gPlayerDefinition.mHelperStore.find(p->mHelperIDInStore) == gPlayerDefinition.mHelperStore.end()) {
		logErrorFormat("Unable to delete helper %d %d, unable to find id %d in store. Ignoring.", p->mRootID, p->mID, p->mHelperIDInStore);
		return;
	}
	removeDreamRegisteredStateMachine(p->mRegisteredStateMachine);
	gPlayerDefinition.mHelperStore.erase(p->mHelperIDInStore);
}

static int updateSinglePlayer(DreamPlayer* p) {
	for (int currentUpdate = 0; currentUpdate < p->mTimeDilatationUpdates; currentUpdate++) {
		if (p->mIsDestroyed) {
			updatePlayerDestruction(p);

			return 1;
		}

		if (updatePlayerSuperPauseStopAndReturnIfStopped(p)) break;
		if (updatePlayerPauseStopAndReturnIfStopped(p)) break;
		updateLanding(p);
		updateHitAttributeSlots(p);
		updateProjectileTimeSinceContact(p);
		updateGuarding(p);
		updateGuardingOver(p);
		updateGuardingFlags(p);
		updatePlayerAirJuggle(p);
		updatePlayerDisplayedCombo(p);
		updatePush(p);
		updateStageBorderPost(p);
		updateKOFlags(p);
		updateTransparencyFlag(p);
		updateShadow(p);
		updateReflection(p);
		updateAfterImage(p);
		updateAngle(p);
		updateScale(p);
		updatePlayerPhysicsClamp(p);
		updateActiveTargets(p);
		updatePlayerTrainingMode(p);
		updatePlayerDebug(p);
		updateAutoTurn(p);
	}
	clearPlayerReceivedHits(p);

	list_remove_predicate(&p->mHelpers, updateSinglePlayerCB, NULL);
	int_map_map(&p->mProjectiles, updateSingleProjectileCB, NULL);
	return 0;
}

void updatePlayers()
{
	setProfilingSectionMarkerCurrentFunction();
	int i;
	for (i = 0; i < 2; i++) {
		updateSinglePlayer(&gPlayerDefinition.mPlayers[i]);
	}
}

static void updatePlayersWithCaller(void* /*tCaller*/) {
	setProfilingSectionMarkerCurrentFunction();

	updatePlayers();
}

static int updateSinglePlayerPreStateMachine(DreamPlayer* p);
static int updateSinglePlayerPreStateMachineCB(void* tCaller, void* tData) {
	(void)tCaller;
	DreamPlayer* p = (DreamPlayer*)tData;
	return updateSinglePlayerPreStateMachine(p);
}

static void updateWidthFlag(DreamPlayer* tPlayer) {
	tPlayer->mWidthFlag = 0;
}

static void updateInvisibilityFlag(DreamPlayer* tPlayer) {
	tPlayer->mInvisibilityFlag = 0;
}

static void updateNoJuggleCheckFlag(DreamPlayer* tPlayer) {
	tPlayer->mNoJuggleCheckFlag = 0;
}

static void updatePlayerDrawOffset(DreamPlayer* tPlayer) {
	tPlayer->mDrawOffset.x = 0;
	tPlayer->mDrawOffset.y = 0;

	const auto pos = getMugenAnimationPosition(tPlayer->mAnimationElement);
	const auto basePos = getDreamStageCoordinateSystemOffset(getDreamMugenStageHandlerCameraCoordinateP());
	const auto newPos = basePos + transformDreamCoordinatesVector2D(tPlayer->mCustomSizeData.mDrawOffset.f(), getPlayerCoordinateP(tPlayer), getDreamMugenStageHandlerCameraCoordinateP());
	setMugenAnimationPosition(tPlayer->mAnimationElement, newPos.xyz(pos.z));
}

static void updateCameraFollowingFlags(DreamPlayer* p) {
	p->mIsCameraFollowing.x = p->mIsCameraFollowing.y = 1;
}

static int updateSinglePlayerPreStateMachine(DreamPlayer* p) {
	setProfilingSectionMarkerCurrentFunction();

	p->mTimeDilatationNow += p->mTimeDilatation;
	p->mTimeDilatationUpdates = (int)p->mTimeDilatationNow;
	p->mTimeDilatationNow -= p->mTimeDilatationUpdates;
	for (int currentUpdate = 0; currentUpdate < p->mTimeDilatationUpdates; currentUpdate++) {
		if (p->mIsDestroyed) {
			updatePlayerDestruction(p);
			return 1;
		}
		updatePlayerHitData(p);
		updatePlayerHitOver(p);
		updatePlayerHitShake(p);
		updateHitPause(p);
		updatePlayerReceivedHits(p);
		updateCameraFollowingFlags(p);
		updateStageBorderPre(p);
		updatePush(p);
		updateWidthFlag(p);
		updateInvisibilityFlag(p);
		updateNoJuggleCheckFlag(p);
		updatePlayerDrawOffset(p);

		updateWalking(p);
		updateAirJumping(p);
		updateJumpFlank(p);
		updateJumping(p);
		updateCrouchingDown(p);
		updateStandingUp(p);
		updatePositionFreeze(p);
		updateGettingUp(p);
		updateFall(p);
		updateBinding(p);
	}

	list_remove_predicate(&p->mHelpers, updateSinglePlayerPreStateMachineCB, NULL);
	int_map_map(&p->mProjectiles, updateSingleProjectilePreStateMachineCB, NULL);
	return 0;
}

static void updatePlayersPreStateMachine(void* tData) {
	(void)tData;
	setProfilingSectionMarkerCurrentFunction();

	int i;
	for (i = 0; i < 2; i++) {
		updateSinglePlayerPreStateMachine(&gPlayerDefinition.mPlayers[i]);
	}

	updatePushFlags();
}

static void drawSinglePlayer(DreamPlayer* p);

static void drawSinglePlayerCB(void* tCaller, void* tData) {
	(void)tCaller;
	DreamPlayer* p = (DreamPlayer*)tData;
	drawSinglePlayer(p);
}

static void drawPlayerWidthAndCenter(DreamPlayer* p) {
	double sx = getPlayerScreenPositionX(p, getDreamMugenStageHandlerCameraCoordinateP());
	double sy = getPlayerScreenPositionY(p, getDreamMugenStageHandlerCameraCoordinateP());
	
	double x = getPlayerPositionX(p, getDreamMugenStageHandlerCameraCoordinateP());
	double fx = getPlayerFrontX(p, getDreamMugenStageHandlerCameraCoordinateP());
	double bx = getPlayerBackX(p, getDreamMugenStageHandlerCameraCoordinateP());

	double leftX, rightX;
	if (getPlayerIsFacingRight(p)) {
		rightX = sx + (fx - x);
		leftX = sx - (x - bx);
	}
	else {
		rightX = sx + (bx - x);
		leftX = sx - (x - fx);
	}

	Position pa = Vector3D(leftX, sy, WIDTH_LINE_Z);
	Position pb = Vector3D(rightX, sy, WIDTH_LINE_Z);
	drawColoredHorizontalLine(pa, pb, COLOR_DARK_GREEN);
	drawColoredPoint(Vector3D(sx, sy, CENTER_POINT_Z), COLOR_YELLOW);
}

static void drawSinglePlayer(DreamPlayer* p) {
	if (p->mIsDestroyed) return;

	drawPlayerWidthAndCenter(p);


	list_map(&p->mHelpers, drawSinglePlayerCB, NULL);
	//int_map_map(&p->mProjectiles, drawSinglePlayerCB, NULL);
}

void drawPlayers() {
	setProfilingSectionMarkerCurrentFunction();
	if (!gPlayerDefinition.mIsCollisionDebugActive) return;

	int i;
	for (i = 0; i < 2; i++) {
		drawSinglePlayer(&gPlayerDefinition.mPlayers[i]);
	}
}

ActorBlueprint getPreStateMachinePlayersBlueprint() {
	return makeActorBlueprint(NULL, NULL, updatePlayersPreStateMachine);
}

ActorBlueprint getPostStateMachinePlayersBlueprint()
{
	return makeActorBlueprint(NULL, NULL, updatePlayersWithCaller);
}

int hasLoadedPlayerSprites()
{
	return gPlayerDefinition.mHasLoadedSprites;
}

static void handlePlayerHitOverride(DreamPlayer* p, DreamPlayer* tOtherPlayer, int* tNextState) {
	int doesForceAir;
	getMatchingHitOverrideStateNoAndForceAir(p, tOtherPlayer, tNextState, &doesForceAir);
	if (doesForceAir) {
		setPlayerUnguarding(p);
		setPlayerStateType(p, MUGEN_STATE_TYPE_AIR);
		setActiveHitDataAirFall(p, 1);
		p->mIsFalling = getActiveHitDataFall(p) && !getActiveHitDataForceNoFall(p);
	}
}

static void setPlayerHitStatesPlayer(DreamPlayer* p, DreamPlayer* tOtherPlayer, int tHasMatchingHitOverride) {
	if (getActiveHitDataPlayer2StateNumber(p) == -1) {

		int nextState;

		if (tHasMatchingHitOverride) {
			handlePlayerHitOverride(p, tOtherPlayer, &nextState);
		}
		else if (getPlayerStateType(p) == MUGEN_STATE_TYPE_STANDING) {
			if (isPlayerGuarding(p)) {
				nextState = 150;
			}
			else if (getActiveHitDataGroundType(p) == MUGEN_ATTACK_HEIGHT_TRIP) {
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
		else if (getPlayerStateType(p) == MUGEN_STATE_TYPE_LYING) {
			if (getActiveHitDataGroundType(p) == MUGEN_ATTACK_HEIGHT_TRIP) {
				nextState = 5070;
			}
			else {
				nextState = 5080;
			}
		}
		else {
			logWarningFormat("Unrecognized player state type %d. Defaulting to state 5000.", getPlayerStateType(p));
			nextState = 5000;
		}

		changePlayerStateToSelf(p, nextState);
	}
	else if (getActiveHitDataPlayer2CapableOfGettingPlayer1State(p)) {
		changePlayerStateToOtherPlayerStateMachine(p, tOtherPlayer, getActiveHitDataPlayer2StateNumber(p));
	}
	else {
		changePlayerState(p, getActiveHitDataPlayer2StateNumber(p));
	}
}

static void setPlayerHitStatesNonPlayer(DreamPlayer* p, DreamPlayer* tOtherPlayer, int tHasMatchingHitOverride) {
	if (isPlayerHelper(p) && tHasMatchingHitOverride) {
		int nextState;
		handlePlayerHitOverride(p, tOtherPlayer, &nextState);
		changePlayerStateToSelf(p, nextState);
	}
}

static void setPlayerHitStates(DreamPlayer* p, DreamPlayer* tOtherPlayer, int tHasMatchingHitOverride) {
	setProfilingSectionMarkerCurrentFunction();
	if (isPlayerHelper(p) || isPlayerProjectile(p)) {
		setPlayerHitStatesNonPlayer(p, tOtherPlayer, tHasMatchingHitOverride);
	}
	else {
		setPlayerHitStatesPlayer(p, tOtherPlayer, tHasMatchingHitOverride);
	}

	int enemyStateBefore = getPlayerState(p);
	if (getActiveHitDataPlayer1StateNumber(p) != -1) {
		changePlayerState(tOtherPlayer, getActiveHitDataPlayer1StateNumber(p));
	}
	int enemyStateAfter = getPlayerState(p);

	if (enemyStateBefore == enemyStateAfter) {
		const auto hasPlayerMatchingHitOverride = hasMatchingHitOverride(p, tOtherPlayer);
		if (isPlayerHelper(p) || isPlayerProjectile(p)) {
			setPlayerHitStatesNonPlayer(p, tOtherPlayer, hasPlayerMatchingHitOverride);
		}
		else {
			setPlayerHitStatesPlayer(p, tOtherPlayer, hasPlayerMatchingHitOverride);
		}
	}
}

static int checkPlayerHitGuardFlagsAndReturnIfGuardable(DreamPlayer* tPlayer, char* tFlags) {
	setProfilingSectionMarkerCurrentFunction();
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

static int isPlayerInCorner(DreamPlayer* p, int tIsCheckingRightCorner);

static void handlePlayerCornerPush(DreamPlayer* p, DreamPlayer* tOtherPlayer) {
	if (isPlayerHelper(tOtherPlayer) || isPlayerProjectile(tOtherPlayer)) return;
	if (getPlayerStateType(tOtherPlayer) == MUGEN_STATE_TYPE_AIR) return;
	if (!isPlayerInCorner(p, getActiveHitDataIsFacingRight(p))) return;
	if (getActiveHitDataAttackClass(p) == MUGEN_ATTACK_CLASS_HYPER || getActiveHitDataAttackClass(p) == MUGEN_ATTACK_CLASS_SPECIAL || getActiveHitDataAttackType(p) == MUGEN_ATTACK_TYPE_THROW) return;

	double pushOffsetX;
	if (isPlayerGuarding(p)) {

		if (getPlayerStateType(p) == MUGEN_STATE_TYPE_AIR) {
			pushOffsetX = getActiveAirGuardCornerPushVelocityOffset(p);
		}
		else {
			pushOffsetX = getActiveGuardCornerPushVelocityOffset(p);
		}
	}
	else if (getPlayerStateType(p) == MUGEN_STATE_TYPE_AIR) {
		pushOffsetX = getActiveAirCornerPushVelocityOffset(p);
	}
	else  if (getPlayerStateType(p) == MUGEN_STATE_TYPE_LYING) {
		pushOffsetX = getActiveDownCornerPushVelocityOffset(p);
	}
	else {
		pushOffsetX = getActiveGroundCornerPushVelocityOffset(p);
	}

	static const double PUSH_VELOCITY_SCALE = 2.0;
	pushOffsetX += getActiveHitDataVelocityX(p)*PUSH_VELOCITY_SCALE;

	addPlayerVelocityX(tOtherPlayer, pushOffsetX, getPlayerCoordinateP(p));
}

static void addPlayerAsActiveTarget(DreamPlayer* p, DreamPlayer* tNewTarget) {
	if (isPlayerProjectile(tNewTarget)) return;

	p->mActiveTargets.insert(std::make_pair(getActiveHitDataHitID(tNewTarget), tNewTarget));
}

static void setPlayerForceStand(DreamPlayer* p) {
	if (getActiveHitDataForceStanding(p)) {
		const auto type = getPlayerStateType(p);
		if (type == MUGEN_STATE_TYPE_CROUCHING) {
			setPlayerStateType(p, MUGEN_STATE_TYPE_STANDING);
		}
	}
}

static void setPlayerHitDataSnap(DreamPlayer* p, DreamPlayer* tAttackingPlayer) {
	if (!getActiveHitDataHasSnap(p)) return;

	const auto offset = getActiveHitDataSnap(p);
	const auto basePos = getPlayerPosition(tAttackingPlayer, getPlayerCoordinateP(p));
	const auto newPos = (getPlayerIsFacingRight(tAttackingPlayer)) ? (basePos + offset) : (basePos - offset);
	setPlayerPositionX(p, newPos.x, getPlayerCoordinateP(p));
	setPlayerPositionY(p, newPos.y, getPlayerCoordinateP(p));
}

static double getPlayerAxisDistanceForTwoReferencesX(DreamPlayer* p1, DreamPlayer* p2, int tCoordinateP);
static double getPlayerAxisDistanceForTwoReferencesY(DreamPlayer* p1, DreamPlayer* p2, int tCoordinateP);

static void setPlayerHitDataDistanceConstraints(DreamPlayer* p, DreamPlayer* tAttackingPlayer) {
	if (!getActiveHitDataHasMinimumDistance(p) && !getActiveHitDataHasMaximumDistance(p)) return;

	const auto basePos = getPlayerPosition(tAttackingPlayer, getPlayerCoordinateP(p));
	const auto distX = getPlayerAxisDistanceForTwoReferencesX(tAttackingPlayer, p, getPlayerCoordinateP(p));
	const auto distY = getPlayerAxisDistanceForTwoReferencesY(tAttackingPlayer, p, getPlayerCoordinateP(p));
	if (getActiveHitDataHasMinimumDistance(p)) {
		const auto minDistance = getActiveHitDataMinimumDistance(p);
		if (distX < minDistance.x) {
			const auto newOffset = (getPlayerIsFacingRight(tAttackingPlayer)) ? minDistance.x : (-minDistance.x);
			setPlayerPositionX(p, basePos.x + newOffset, getPlayerCoordinateP(p));
		}
		if (distY < minDistance.y) {
			setPlayerPositionY(p, basePos.y + minDistance.y, getPlayerCoordinateP(p));
		}
	}
	if (getActiveHitDataHasMaximumDistance(p)) {
		const auto maxDistance = getActiveHitDataMaximumDistance(p);
		if (distX > maxDistance.x) {
			const auto newOffset = (getPlayerIsFacingRight(tAttackingPlayer)) ? maxDistance.x : (-maxDistance.x);
			setPlayerPositionX(p, basePos.x + newOffset, getPlayerCoordinateP(p));
		}
		if (distY > maxDistance.y) {
			setPlayerPositionY(p, basePos.y + maxDistance.y, getPlayerCoordinateP(p));
		}
	}
}


static void setPlayerHitPosition(DreamPlayer* p, DreamPlayer* tAttackingPlayer) {
	setPlayerHitDataSnap(p, tAttackingPlayer);
	setPlayerHitDataDistanceConstraints(p, tAttackingPlayer);
}

static void setPlayerHitPaletteEffects(DreamPlayer* p) {
	const auto duration = getActiveHitDataPaletteEffectTime(p);
	if (!duration) return;
	setPlayerPaletteEffect(p, duration, getActiveHitDataPaletteEffectAddition(p), getActiveHitDataPaletteEffectMultiplication(p), Vector3D(0, 0, 0), 1, 0, 1.0, 0);
}

static void setPlayerHit(DreamPlayer* p, DreamPlayer* tOtherPlayer, void* tHitData) {
	setProfilingSectionMarkerCurrentFunction();
	setPlayerControl(p, 0);
	stopSoundEffect(parsePlayerSoundEffectChannel(0, p));
	removeExplodsForPlayerAfterHit(p);

	copyHitDataToActive(p, tHitData);

	const auto player2ChangeFaceDirectionRelativeToPlayer1 = getActiveHitDataPlayer2ChangeFaceDirectionRelativeToPlayer1(p);
	if (player2ChangeFaceDirectionRelativeToPlayer1) {
		if (player2ChangeFaceDirectionRelativeToPlayer1 > 0) {
			setPlayerIsFacingRight(p, !getActiveHitDataIsFacingRight(p));
		}
		else {
			setPlayerIsFacingRight(p, getActiveHitDataIsFacingRight(p));
		}
	}
	else {
		setPlayerIsFacingRight(p, !getActiveHitDataIsFacingRight(p));
	}

	if (getPlayerUnguardableFlag(tOtherPlayer) || (isPlayerGuarding(p) && !checkPlayerHitGuardFlagsAndReturnIfGuardable(p, getActiveHitDataGuardFlag(p)))) {
		setPlayerUnguarding(p);
	}

	if (isPlayerGuarding(p)) {
		setActiveHitDataVelocityX(p, getActiveHitDataGuardVelocity(p), getPlayerCoordinateP(p));
		setActiveHitDataVelocityY(p, 0, getPlayerCoordinateP(p));
		p->mIsFalling = 0;
	}
	else if (getPlayerStateType(p) == MUGEN_STATE_TYPE_AIR) {
		setActiveHitDataVelocityX(p, getActiveHitDataAirVelocityX(p), getPlayerCoordinateP(p));
		setActiveHitDataVelocityY(p, getActiveHitDataAirVelocityY(p), getPlayerCoordinateP(p));
		p->mIsFalling = getActiveHitDataAirFall(p);
	}
	else {
		setPlayerForceStand(p);
		setActiveHitDataVelocityX(p, getActiveHitDataGroundVelocityX(p), getPlayerCoordinateP(p));
		setActiveHitDataVelocityY(p, getActiveHitDataGroundVelocityY(p), getPlayerCoordinateP(p));
		p->mIsFalling = getActiveHitDataFall(p);
	}
	p->mIsFalling = p->mIsFalling && !getActiveHitDataForceNoFall(p);

	if (!isPlayerGuarding(p)) {
		setPlayerHitPosition(p, tOtherPlayer);
		setPlayerHitPaletteEffects(p);
	}
	handlePlayerCornerPush(p, tOtherPlayer);

	int hitShakeDuration, hitPauseDuration;
	int damage;
	int powerUp1;
	int powerUp2;
	if (isPlayerGuarding(p)) {
		damage = (int)(getActiveHitDataGuardDamage(p) * getPlayerAttackMultiplier(tOtherPlayer) * getInvertedPlayerDefenseMultiplier(p));
		powerUp1 = getActiveHitDataPlayer2GuardPowerAdded(p);
		powerUp2 = getActiveHitDataPlayer1GuardPowerAdded(p);
		hitPauseDuration = getActiveHitDataPlayer1GuardPauseTime(p);
		hitShakeDuration = getActiveHitDataPlayer2GuardPauseTime(p);
	}
	else {
		damage = (int)(getActiveHitDataDamage(p) * getPlayerAttackMultiplier(tOtherPlayer) * getInvertedPlayerDefenseMultiplier(p));
		powerUp1 = getActiveHitDataPlayer2PowerAdded(p);
		powerUp2 = getActiveHitDataPlayer1PowerAdded(p);
		hitPauseDuration = getActiveHitDataPlayer1PauseTime(p);
		hitShakeDuration = getActiveHitDataPlayer2PauseTime(p);
	}

	p->mIsHitShakeActive = 1;
	p->mIsHitOver = 0;

	const auto hasPlayerMatchingHitOverride = hasMatchingHitOverride(p, tOtherPlayer);
	if (!hasPlayerMatchingHitOverride) {
		addPlayerAsActiveTarget(tOtherPlayer, p);
	}

	p->mCanRecoverFromFall = getActiveHitDataFallRecovery(p);
	p->mRecoverTime = getActiveHitDataFallRecoveryTime(p);
	p->mRecoverTimeSinceHitPause = 0;

	const auto isPlayerStateMachinePaused = isDreamRegisteredStateMachinePaused(p->mRegisteredStateMachine);
	const auto isOtherPlayerStateMachinePaused = isDreamRegisteredStateMachinePaused(tOtherPlayer->mRegisteredStateMachine);
	unpauseDreamRegisteredStateMachine(p->mRegisteredStateMachine);
	unpauseDreamRegisteredStateMachine(tOtherPlayer->mRegisteredStateMachine);
	setPlayerHitStates(p, tOtherPlayer, hasPlayerMatchingHitOverride);
	setDreamRegisteredStateMachinePauseStatus(tOtherPlayer->mRegisteredStateMachine, isOtherPlayerStateMachinePaused);
	setDreamRegisteredStateMachinePauseStatus(p->mRegisteredStateMachine, isPlayerStateMachinePaused);

	if (isPlayerGuarding(p)) {
		setPlayerMoveGuarded(tOtherPlayer);
		p->mLastHitGuarded = 1;
	} else {
		setPlayerMoveHit(tOtherPlayer);
		p->mLastHitGuarded = 0;
	}
	setPlayerMoveContactCounterActive(tOtherPlayer);

	if (!isPlayerProjectile(p)) {
		addPlayerDamage(getPlayerRoot(p), tOtherPlayer, damage);
	}
	addPlayerPower(getPlayerRoot(p), powerUp1);
	addPlayerPower(tOtherPlayer, powerUp2);

	if (hitShakeDuration && isPlayerAlive(p)) {
		setPlayerHitPaused(p, hitShakeDuration);
	}

	if (hitPauseDuration) {
		setPlayerHitPaused(tOtherPlayer, hitPauseDuration);
	}

	p->mIsHitOverWaitActive = 0;
	p->mHitShakeNow = 0;
	p->mHitShakeDuration = hitShakeDuration;
}

static void playPlayerHitSpark(DreamPlayer* p1, DreamPlayer* p2, DreamPlayer* tFileOwner, int tIsInPlayerFile, int tNumber, Position2D tSparkOffset) {
	if (tNumber == -1) return;

	Position pos1 = *getHandledPhysicsPositionReference(p1->mPhysicsElement);
	Position pos2 = *getHandledPhysicsPositionReference(p2->mPhysicsElement);

	Position base;
	if (pos1.x < pos2.x) {
		double width = p1->mFaceDirection == FACE_DIRECTION_RIGHT ? getPlayerFrontWidth(p1, getDreamMugenStageHandlerCameraCoordinateP()) : transformDreamCoordinates(p1->mCustomSizeData.mGroundBackWidth, getPlayerCoordinateP(p1), getDreamMugenStageHandlerCameraCoordinateP());
		base = vecAdd(pos1, Vector3D(width, 0, 0));
	}
	else {
		double width = p1->mFaceDirection == FACE_DIRECTION_LEFT ? getPlayerFrontWidth(p1, getDreamMugenStageHandlerCameraCoordinateP()) : transformDreamCoordinates(p1->mCustomSizeData.mGroundBackWidth, getPlayerCoordinateP(p1), getDreamMugenStageHandlerCameraCoordinateP());
		base = vecAdd(pos1, Vector3D(-width, 0, 0));
	}
	base.y = pos2.y;

	if (!getActiveHitDataIsFacingRight(p1)) {
		tSparkOffset.x = -tSparkOffset.x;
	}

	tSparkOffset = transformDreamCoordinatesVector2D(tSparkOffset, getPlayerCoordinateP(p1), getDreamMugenStageHandlerCameraCoordinateP()) + base.xy();
	playDreamHitSpark(tSparkOffset, tFileOwner, tIsInPlayerFile, tNumber, getActiveHitDataIsFacingRight(p1), getDreamMugenStageHandlerCameraCoordinateP());
}

typedef struct {
	int mFound;
	MugenAttackType mType;
	MugenAttackClass mClass;
} HitDefAttributeFlag2Caller;

static void checkSingleFlag2InHitDefAttributeSlot(HitDefAttributeFlag2Caller* tCaller, const std::string& tFlag) {
	if (tFlag.size() != 2) {
		logWarningFormat("Unable to check hitdef attribute flag %s, invalid size.", tFlag.c_str());
		return;
	}
	if (tFlag[0] != 'a') {
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
		if (tSlot->mFlag1.find('s') != tSlot->mFlag1.npos) return tSlot->mIsHitBy;
	} 
	else if (type == MUGEN_STATE_TYPE_CROUCHING) {
		if (tSlot->mFlag1.find('c') != tSlot->mFlag1.npos) return tSlot->mIsHitBy;
	}
	else if (type == MUGEN_STATE_TYPE_AIR) {
		if (tSlot->mFlag1.find('a') != tSlot->mFlag1.npos) return tSlot->mIsHitBy;
	}
	else {
		logWarningFormat("Invalid hitdef type %d. Defaulting to not not hit.", type);
		return 0;
	}

	HitDefAttributeFlag2Caller caller;
	caller.mFound = 0;
	caller.mType = getHitDataAttackType(p2);
	caller.mClass = getHitDataAttackClass(p2);

	for (size_t i = 0; i < tSlot->mFlag2.size(); i++) {
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

static int isIgnoredBecauseOfJuggle(DreamPlayer* p, DreamPlayer* tOtherPlayer) {
	int isJuggableState;

	if (isPlayerGuarding(p)) {
		isJuggableState = 0;
	}
	else if (getPlayerStateType(p) == MUGEN_STATE_TYPE_AIR) {
		setActiveHitDataVelocityX(p, getActiveHitDataAirVelocityX(p), getPlayerCoordinateP(p));
		setActiveHitDataVelocityY(p, getActiveHitDataAirVelocityY(p), getPlayerCoordinateP(p));
		isJuggableState = isPlayerFalling(p) || getActiveHitDataAirFall(p);
	}
	else {
		isJuggableState = getPlayerStateType(p) == MUGEN_STATE_TYPE_LYING;
	}

	if (!isJuggableState) return 0;
	if (tOtherPlayer->mNoJuggleCheckFlag) return 0;

	p->mIsBeingJuggled = 1;
	p->mAirJugglePoints -= getPlayerStateJugglePoints(tOtherPlayer);

	return p->mAirJugglePoints < 0;
}

static void playPlayerHitSound(DreamPlayer* p, void(*tFunc)(DreamPlayer*,int*,Vector2DI*)) {
	setProfilingSectionMarkerCurrentFunction();
	int isInPlayerFile;
	Vector2DI sound;
	tFunc(p, &isInPlayerFile, &sound);

	MugenSounds* soundFile;
	if (isInPlayerFile) {
		soundFile = getPlayerSounds(p);
	}
	else {
		soundFile = getDreamCommonSounds();
	}

	tryPlayMugenSoundAdvanced(soundFile, sound.x, sound.y, getPlayerMidiVolumeForPrism(p));
}

static void increaseDisplayedComboCounter(DreamPlayer* p, int tValue) {
	p->mDisplayedComboCounter += tValue;
	setComboUIDisplay(p->mRootID, p->mDisplayedComboCounter);

}

static int isReversalDefActiveForHit(DreamPlayer* p, DreamPlayer* tOtherPlayer) {
	if (!isHitDataReversalDefActive(p)) return 0;
	if (isPlayerProjectile(tOtherPlayer)) return 0;
	if (!checkSingleNoHitDefSlot(getHitDataReversalDefReversalAttribute(p), tOtherPlayer)) return 0;
	if (p->mReceivedReversalDefPlayers.find(tOtherPlayer) == p->mReceivedReversalDefPlayers.end()) return 0;
	return 1;
}

static void handleReversalDefHit(DreamPlayer* p, DreamPlayer* tOtherPlayer) {
	const auto hitPauseDuration = getReversalDefPlayer1PauseTime(p);
	const auto hitShakeDuration = getReversalDefPlayer2PauseTime(p);

	if (hasReversalDefP2StateNo(p)) {
		changePlayerStateToOtherPlayerStateMachine(tOtherPlayer, p, getReversalDefP2StateNo(p));
	}
	if (hasReversalDefP1StateNo(p)) {
		changePlayerStateToSelf(p, getReversalDefP1StateNo(p));
	}

	playPlayerHitSound(p, getReversalDefHitSound);
	playPlayerHitSpark(p, tOtherPlayer, p, isReversalDefSparkInPlayerFile(p), getReversalDefSparkNumber(p), getActiveHitDataSparkXY(p) + getReversalDefSparkXY(p));
	addPlayerAsActiveTarget(p, tOtherPlayer);

	if (hitShakeDuration && isPlayerAlive(tOtherPlayer)) {
		setPlayerHitPaused(tOtherPlayer, hitShakeDuration);
	}

	if (hitPauseDuration) {
		setPlayerHitPaused(p, hitPauseDuration);
	}

	tOtherPlayer->mMoveReversed = 1;
}

static int isPlayerHitStillValidAfterReceive(DreamPlayer* tOtherPlayer) {
	if (!isValidPlayerOrProjectile(tOtherPlayer)) return 0;
	if (!isInOwnStateMachine(getPlayerRoot(tOtherPlayer)->mRegisteredStateMachine)) return 0;
	if (isDreamAnyPauseActive()) return 0;
	return 1;
}

static int checkHitPriorityTypeDisablingHit(const PlayerHitData& tGettingHitData, const PlayerHitData& tOtherHitData) {
	if (tGettingHitData.mPriorityType == MUGEN_HIT_PRIORITY_DODGE) return 1;
	if (tGettingHitData.mPriorityType == MUGEN_HIT_PRIORITY_MISS) return 1;
	if (tOtherHitData.mPriorityType == MUGEN_HIT_PRIORITY_DODGE) return 1;
	return 0;
}

static int isHitDisabledDueToPriority(DreamPlayer* p, DreamPlayer* tOtherPlayer, const PlayerHitData& tGettingHitData) {
	for (const auto& tOtherHitData : tOtherPlayer->mReceivedHitData) {
		if (tOtherHitData.mPlayer != p) continue;
		if (tGettingHitData.mPriority > tOtherHitData.mPriority) continue;
		if (checkHitPriorityTypeDisablingHit(tGettingHitData, tOtherHitData)) return 1;
	}
	return 0;
}

static int isProjectileHitDisabledDueToPriority(DreamPlayer* tHitProjectile, DreamPlayer* tAttackingProjectile) {
	if (!isPlayerProjectile(tHitProjectile) || !isPlayerProjectile(tAttackingProjectile)) return 0;

	const auto prioHit = getProjectilePriority(tHitProjectile);
	const auto prioAttack = getProjectilePriority(tAttackingProjectile);
	return prioHit > prioAttack;
}

static void playerHitEval(DreamPlayer* p, PlayerHitData& tHitDataReference) {
	setProfilingSectionMarkerCurrentFunction();
	if (!isValidPlayerOrProjectile(p)) return;
	void* hitData = (void*)&tHitDataReference;
	DreamPlayer* otherPlayer = getReceivedHitDataPlayer(hitData);

	if (!isPlayerHitStillValidAfterReceive(otherPlayer)) return;
	if (isHitDisabledDueToPriority(p, otherPlayer, tHitDataReference)) return;
	if (isReversalDefActiveForHit(p, otherPlayer)) {
		handleReversalDefHit(p, otherPlayer);
		return;
	}
	if (isProjectileHitDisabledDueToPriority(p, otherPlayer)) {
		reduceProjectilePriorityAndResetHitData(p);
		return;
	}

	setPlayerHit(p, otherPlayer, hitData);

	const auto playerGuarding = isPlayerGuarding(p);
	if (playerGuarding) {
		playPlayerHitSound(p, getActiveHitDataGuardSound);
		playPlayerHitSpark(p, otherPlayer, otherPlayer, isActiveHitDataGuardSparkInPlayerFile(p), getActiveHitDataGuardSparkNumber(p), getActiveHitDataSparkXY(p));
	}
	else {
		increasePlayerHitCount(otherPlayer);
		increaseDisplayedComboCounter(getPlayerOtherPlayer(p), 1);
		playPlayerHitSound(p, getActiveHitDataHitSound);
		playPlayerHitSpark(p, otherPlayer, otherPlayer, isActiveHitDataSparkInPlayerFile(p), getActiveHitDataSparkNumber(p), getActiveHitDataSparkXY(p));
		setEnvironmentShake(getActiveHitDataEnvironmentShakeTime(p), getActiveHitDataEnvironmentShakeFrequency(p), getActiveHitDataEnvironmentShakeAmplitude(p), getActiveHitDataEnvironmentShakePhase(p), getPlayerCoordinateP(p));
	}

	const auto isOtherPlayerProjectile = isPlayerProjectile(otherPlayer);
	if (isPlayerProjectile(otherPlayer) && (!isPlayerProjectile(p) || isProjectileHitDisabledDueToPriority(p, otherPlayer))) {
		handleProjectileHit(otherPlayer, playerGuarding, isPlayerProjectile(p));
	}

	if (isPlayerProjectile(p)) {
		handleProjectileHit(p, 0, isOtherPlayerProjectile);
	}
}

static void updatePlayerReceivedHits(DreamPlayer* p) {
	setProfilingSectionMarkerCurrentFunction();
	auto hitListCopy = p->mReceivedHitData;
	stl_list_map(hitListCopy, playerHitEval, p);
}

static void clearPlayerReceivedHits(DreamPlayer* p) {
	p->mReceivedHitData.clear();
	p->mReceivedReversalDefPlayers.clear();
}

static int isValidAffectTeam(DreamPlayer* p, DreamPlayer* attackingPlayer) {
	const auto affectTeam = getHitDataAffectTeam(attackingPlayer);
	if (affectTeam == MUGEN_AFFECT_TEAM_ENEMY) return p->mRootID != attackingPlayer->mRootID;
	else if (affectTeam == MUGEN_AFFECT_TEAM_FRIENDLY) return p->mRootID == attackingPlayer->mRootID;
	else return 1;
}

static int isPlayerStateMachineValid(DreamPlayer* p) {
	return isValidDreamRegisteredStateMachine(p->mRegisteredStateMachine);
}

void playerHitCB(void* tData, void* tHitData, int /*tOtherCollisionList*/)
{
	DreamPlayer* p = (DreamPlayer*)tData;
	if (p->mIsDestroyed) return;

	PlayerHitData* receivedHitData = (PlayerHitData*)tHitData;
	DreamPlayer* otherPlayer = getReceivedHitDataPlayer(receivedHitData);

	if (!isValidPlayerOrProjectile(otherPlayer)) return;
	if (!isValidAffectTeam(p, otherPlayer)) return;
	if (isPlayerProjectile(p) && (!isPlayerProjectile(otherPlayer) || !isPlayerStateMachineValid(p))) return;
	if (isPlayerProjectile(otherPlayer) && (!canProjectileHit(otherPlayer) || !isPlayerStateMachineValid(otherPlayer))) return;
	if (!isInOwnStateMachine(getPlayerRoot(otherPlayer)->mRegisteredStateMachine)) return;
	if (isDreamAnyPauseActive()) return;

	if (!isReceivedHitDataActive(receivedHitData)) return;
	if (!checkActiveHitDefAttributeSlots(p, otherPlayer)) return;
	if (isIgnoredBecauseOfHitOverride(p, otherPlayer)) return;
	if (isIgnoredBecauseOfJuggle(p, otherPlayer)) return;
	if (getDreamTimeSinceKO() > getOverHitTime()) return;
	
	PlayerHitData copyHitData = *receivedHitData;
	setReceivedHitDataInactive(tHitData);
	p->mReceivedHitData.push_back(copyHitData);
}

void playerReversalHitCB(void* tData, void* tHitData, int tOtherCollisionList)
{
	DreamPlayer* p = (DreamPlayer*)tData;
	if (p->mIsDestroyed) return;
	if (tOtherCollisionList != getDreamPlayerAttackCollisionList(getPlayerOtherPlayer(p))->mID) return;
	PlayerHitData* receivedHitData = (PlayerHitData*)tHitData;
	const auto otherPlayer = receivedHitData->mPlayer;
	if (otherPlayer->mIsDestroyed) return;
	p->mReceivedReversalDefPlayers.insert(otherPlayer);
}

void setPlayerDefinitionPath(int i, const char * tDefinitionPath)
{
	strcpy(gPlayerDefinition.mPlayerHeader[i].mFiles.mDefinitionPath, tDefinitionPath);
}

void getPlayerDefinitionPath(char* tDst, int i)
{
	strcpy(tDst, gPlayerDefinition.mPlayerHeader[i].mFiles.mDefinitionPath);
}

void setPlayerPreferredPalette(int i, int tPalette)
{
	gPlayerDefinition.mPlayers[i].mPreferredPalette = tPalette;
}

void setPlayerPreferredPaletteRandom(int i)
{
	gPlayerDefinition.mPlayers[i].mPreferredPalette = -1;
}

DreamPlayer * getRootPlayer(int i)
{
	return &gPlayerDefinition.mPlayers[i];
}

DreamPlayer * getPlayerRoot(DreamPlayer* p)
{
	return p->mRoot;
}

DreamPlayer * getPlayerParent(DreamPlayer* p)
{
	if (!p->mParent) {
		logWarningFormat("%d %d trying to access parents as root. Returning self.\n", p->mRootID, p->mID);
		return p;
	}
	return p->mParent;
}

int getPlayerState(DreamPlayer* p)
{
	return getDreamRegisteredStateState(p->mRegisteredStateMachine);
}

int getPlayerPreviousState(DreamPlayer* p)
{
	return getDreamRegisteredStatePreviousState(p->mRegisteredStateMachine);
}

int getPlayerStateJugglePoints(DreamPlayer* p)
{
	return getDreamRegisteredStateJugglePoints(p->mRegisteredStateMachine);;
}

DreamMugenStateType getPlayerStateType(DreamPlayer* p)
{
	return p->mStateType;
}

void setPlayerStateType(DreamPlayer* p, DreamMugenStateType tType)
{
	if (tType == MUGEN_STATE_TYPE_UNCHANGED) {
		return;
	}

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
		setHandledPhysicsDragCoefficient(p->mPhysicsElement, Vector3D(p->mHeader->mFiles.mConstants.mMovementData.mStandFiction, 0, 0));
		setHandledPhysicsGravity(p->mPhysicsElement, Vector3D(0, 0, 0));
		Velocity* vel = getHandledPhysicsVelocityReference(p->mPhysicsElement);
		Acceleration* acc = getHandledPhysicsAccelerationReference(p->mPhysicsElement);
		
		vel->y = 0;
		acc->y = 0;
	}
	else if (tNewPhysics == MUGEN_STATE_PHYSICS_CROUCHING) {
		setHandledPhysicsDragCoefficient(p->mPhysicsElement, Vector3D(p->mHeader->mFiles.mConstants.mMovementData.mCrouchFriction, 0, 0));
		setHandledPhysicsGravity(p->mPhysicsElement, Vector3D(0, 0, 0));
		Position* pos = getHandledPhysicsPositionReference(p->mPhysicsElement);
		Velocity* vel = getHandledPhysicsVelocityReference(p->mPhysicsElement);
		Acceleration* acc = getHandledPhysicsAccelerationReference(p->mPhysicsElement);

		pos->y = 0;
		vel->y = 0;
		acc->y = 0;
	}
	else if (tNewPhysics == MUGEN_STATE_PHYSICS_NONE) {
		setHandledPhysicsDragCoefficient(p->mPhysicsElement, Vector3D(0, 0, 0));
		setHandledPhysicsGravity(p->mPhysicsElement, Vector3D(0, 0, 0));


		if (getPlayerStateType(p) == MUGEN_STATE_TYPE_LYING) {
			Acceleration* acc = getHandledPhysicsAccelerationReference(p->mPhysicsElement);
			acc->y = 0;
		}
	}
	else if (tNewPhysics == MUGEN_STATE_PHYSICS_AIR) {
		setHandledPhysicsDragCoefficient(p->mPhysicsElement, Vector3D(0, 0, 0));
		setHandledPhysicsGravity(p->mPhysicsElement, Vector3D(0, transformDreamCoordinates(p->mHeader->mFiles.mConstants.mMovementData.mVerticalAcceleration, getPlayerCoordinateP(p), getDreamMugenStageHandlerCameraCoordinateP()), 0));

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

static void increaseComboCounter(DreamPlayer* p, int tAmount) {

	p->mComboCounter += tAmount;
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
	increaseComboCounter(p, 1);
}

int getPlayerVariable(DreamPlayer* p, int tIndex)
{
	return p->mVars[tIndex];
}

int * getPlayerVariableReference(DreamPlayer* p, int tIndex)
{
	return &p->mVars[tIndex];
}

void setPlayerVariable(DreamPlayer* p, int tIndex, int tValue)
{
	p->mVars[tIndex] = tValue;
}

void addPlayerVariable(DreamPlayer* p, int tIndex, int tValue)
{
	int cur = getPlayerVariable(p, tIndex);
	cur += tValue;
	setPlayerVariable(p, tIndex, cur);
}

int getPlayerSystemVariable(DreamPlayer* p, int tIndex)
{
	return p->mSystemVars[tIndex];
}

void setPlayerSystemVariable(DreamPlayer* p, int tIndex, int tValue)
{
	p->mSystemVars[tIndex] = tValue;
}

void addPlayerSystemVariable(DreamPlayer* p, int tIndex, int tValue)
{
	int cur = getPlayerSystemVariable(p, tIndex);
	cur += tValue;
	setPlayerSystemVariable(p, tIndex, cur);
}

double getPlayerFloatVariable(DreamPlayer* p, int tIndex)
{
	return p->mFloatVars[tIndex];
}

void setPlayerFloatVariable(DreamPlayer* p, int tIndex, double tValue)
{
	p->mFloatVars[tIndex] = tValue;
}

void addPlayerFloatVariable(DreamPlayer* p, int tIndex, double tValue)
{
	double cur = getPlayerFloatVariable(p, tIndex);
	cur += tValue;
	setPlayerFloatVariable(p, tIndex, cur);
}

double getPlayerSystemFloatVariable(DreamPlayer* p, int tIndex)
{
	return p->mSystemFloatVars[tIndex];
}

void setPlayerSystemFloatVariable(DreamPlayer* p, int tIndex, double tValue)
{
	p->mSystemFloatVars[tIndex] = tValue;
}

void addPlayerSystemFloatVariable(DreamPlayer* p, int tIndex, double tValue)
{
	double cur = getPlayerSystemFloatVariable(p, tIndex);
	cur += tValue;
	setPlayerSystemFloatVariable(p, tIndex, cur);
}

int getPlayerTimeInState(DreamPlayer* p)
{
	return getDreamRegisteredStateTimeInState(p->mRegisteredStateMachine);
}

int getPlayerAnimationNumber(DreamPlayer* p)
{
	return getMugenAnimationAnimationNumber(p->mAnimationElement);
}

int getPlayerAnimationStep(DreamPlayer* p) {
	return getMugenAnimationAnimationStep(p->mAnimationElement);
}

int getPlayerAnimationStepAmount(DreamPlayer* p) {
	return getMugenAnimationAnimationStepAmount(p->mAnimationElement);
}

int getPlayerAnimationStepDuration(DreamPlayer* p)
{
	return getMugenAnimationAnimationStepDuration(p->mAnimationElement);
}

int getPlayerAnimationTimeDeltaUntilFinished(DreamPlayer* p)
{
	if (isMugenAnimationDurationInfinite(p->mAnimationElement)) {
		return getMugenAnimationTime(p->mAnimationElement);
	}
	else {
		return -getMugenAnimationRemainingAnimationTime(p->mAnimationElement);
	}
}

int isPlayerAnimationTimeInfinite(DreamPlayer* p)
{
	return isMugenAnimationDurationInfinite(p->mAnimationElement);
}

int hasPlayerAnimationLooped(DreamPlayer* p)
{
	return hasMugenAnimationLooped(p->mAnimationElement);
}

int getPlayerAnimationDuration(DreamPlayer* p) {
	return getMugenAnimationDuration(p->mAnimationElement);
}

int getPlayerAnimationTime(DreamPlayer* p) {
	return getMugenAnimationTime(p->mAnimationElement);
}

int getPlayerSpriteGroup(DreamPlayer* p) {
	return getMugenAnimationSprite(p->mAnimationElement).x;
}

int getPlayerSpriteElement(DreamPlayer* p) {
	return getMugenAnimationSprite(p->mAnimationElement).y;
}

Vector2D getPlayerPosition(DreamPlayer* p, int tCoordinateP)
{
	return Vector2D(getPlayerPositionX(p, tCoordinateP), getPlayerPositionY(p, tCoordinateP));
}

double getPlayerPositionBasedOnScreenCenterX(DreamPlayer* p, int tCoordinateP)
{
	const auto pos = getDreamStageCenterOfScreenBasedOnPlayer(tCoordinateP);
	const auto ret = getPlayerPosition(p, tCoordinateP) - pos;
	return ret.x;
}

double getPlayerScreenPositionX(DreamPlayer* p, int tCoordinateP)
{
	double physicsPosition = getHandledPhysicsPositionReference(p->mPhysicsElement)->x;
	double cameraPosition = getDreamMugenStageHandlerCameraPositionReference()->x;
	double animationPosition = getMugenAnimationPosition(p->mAnimationElement).x;
	double unscaledPosition = physicsPosition + animationPosition - cameraPosition;
	return transformDreamCoordinates(unscaledPosition, getDreamMugenStageHandlerCameraCoordinateP(), tCoordinateP);
}

double getPlayerPositionX(DreamPlayer* p, int tCoordinateP)
{
	return transformDreamCoordinates(getHandledPhysicsPositionReference(p->mPhysicsElement)->x, getDreamMugenStageHandlerCameraCoordinateP(), tCoordinateP);
}

double getPlayerPositionBasedOnStageFloorY(DreamPlayer* p, int tCoordinateP)
{
	const auto ret = getPlayerPosition(p, tCoordinateP);
	return ret.y;
}

double getPlayerScreenPositionY(DreamPlayer* p, int tCoordinateP)
{
	double physicsPosition = getHandledPhysicsPositionReference(p->mPhysicsElement)->y;
	double cameraPosition = getDreamMugenStageHandlerCameraPositionReference()->y;
	double animationPosition = getMugenAnimationPosition(p->mAnimationElement).y;
	double unscaledPosition = physicsPosition + animationPosition - cameraPosition;
	return transformDreamCoordinates(unscaledPosition, getDreamMugenStageHandlerCameraCoordinateP(), tCoordinateP);
}

double getPlayerPositionY(DreamPlayer* p, int tCoordinateP)
{
	return transformDreamCoordinates(getHandledPhysicsPositionReference(p->mPhysicsElement)->y, getDreamMugenStageHandlerCameraCoordinateP(), tCoordinateP);
}

double getPlayerVelocityX(DreamPlayer* p, int tCoordinateP)
{
	auto x = transformDreamCoordinates(getHandledPhysicsVelocityReference(p->mPhysicsElement)->x, getDreamMugenStageHandlerCameraCoordinateP(), tCoordinateP);
	if (p->mFaceDirection == FACE_DIRECTION_LEFT) x *= -1;

	return x;
}

double getPlayerVelocityY(DreamPlayer* p, int tCoordinateP)
{
	return transformDreamCoordinates(getHandledPhysicsVelocityReference(p->mPhysicsElement)->y, getDreamMugenStageHandlerCameraCoordinateP(), tCoordinateP);
}

int getPlayerDataLife(DreamPlayer* p)
{
	return p->mHeader->mFiles.mConstants.mHeader.mLife;
}

int getPlayerDataAttack(DreamPlayer* p)
{
	return p->mHeader->mFiles.mConstants.mHeader.mAttack;
}

double getPlayerDataAttackFactor(DreamPlayer* p)
{
	return getPlayerDataAttack(p) / 100.0;
}

int getPlayerDataDefense(DreamPlayer* p)
{
	return p->mHeader->mFiles.mConstants.mHeader.mDefense;
}

double getPlayerDataDefenseFactor(DreamPlayer* p)
{
	return getPlayerDataDefense(p) / 100.0;
}

int getPlayerDataLiedownTime(DreamPlayer* p)
{
	return p->mHeader->mFiles.mConstants.mHeader.mLiedownTime;
}

int getPlayerDataAirjuggle(DreamPlayer* p)
{
	return p->mHeader->mFiles.mConstants.mHeader.mAirJugglePoints;
}

int getPlayerDataSparkNo(DreamPlayer* p)
{
	return p->mHeader->mFiles.mConstants.mHeader.mSparkNo;
}

int getPlayerDataGuardSparkNo(DreamPlayer* p)
{
	return p->mHeader->mFiles.mConstants.mHeader.mGuardSparkNo;
}

int getPlayerDataKOEcho(DreamPlayer* p)
{
	return p->mHeader->mFiles.mConstants.mHeader.mKOEcho;
}

int getPlayerDataIntPersistIndex(DreamPlayer* p)
{
	return p->mHeader->mFiles.mConstants.mHeader.mIntPersistIndex;
}

int getPlayerDataFloatPersistIndex(DreamPlayer* p)
{
	return p->mHeader->mFiles.mConstants.mHeader.mFloatPersistIndex;
}

int getPlayerSizeAirBack(DreamPlayer* p, int tCoordinateP)
{
	return transformDreamCoordinatesI(p->mCustomSizeData.mAirBackWidth, getPlayerCoordinateP(p), tCoordinateP);
}

int getPlayerSizeAirFront(DreamPlayer* p, int tCoordinateP)
{
	return transformDreamCoordinatesI(p->mCustomSizeData.mAirFrontWidth, getPlayerCoordinateP(p), tCoordinateP);
}

int getPlayerSizeAttackDist(DreamPlayer* p, int tCoordinateP)
{
	return transformDreamCoordinatesI(p->mCustomSizeData.mAttackDistance, getPlayerCoordinateP(p), tCoordinateP);
}

int getPlayerSizeProjectileAttackDist(DreamPlayer* p, int tCoordinateP)
{
	return transformDreamCoordinatesI(p->mCustomSizeData.mProjectileAttackDistance, getPlayerCoordinateP(p), tCoordinateP);
}

int getPlayerSizeProjectilesDoScale(DreamPlayer* p)
{
	return p->mCustomSizeData.mDoesScaleProjectiles;
}

int getPlayerSizeShadowOffset(DreamPlayer* p, int tCoordinateP)
{
	return transformDreamCoordinatesI(p->mCustomSizeData.mShadowOffset, getPlayerCoordinateP(p), tCoordinateP);
}

int getPlayerSizeDrawOffsetX(DreamPlayer* p, int tCoordinateP)
{
	return transformDreamCoordinatesI(p->mCustomSizeData.mDrawOffset.x, getPlayerCoordinateP(p), tCoordinateP);
}

int getPlayerSizeDrawOffsetY(DreamPlayer* p, int tCoordinateP)
{
	return transformDreamCoordinatesI(p->mCustomSizeData.mDrawOffset.y, getPlayerCoordinateP(p), tCoordinateP);
}

double getPlayerVelocityAirGetHitGroundRecoverX(DreamPlayer* p, int tCoordinateP)
{
	return transformDreamCoordinates(p->mHeader->mFiles.mConstants.mVelocityData.mAirGetHitGroundRecovery.x, getPlayerCoordinateP(p), tCoordinateP);
}

double getPlayerVelocityAirGetHitGroundRecoverY(DreamPlayer* p, int tCoordinateP)
{
	return transformDreamCoordinates(p->mHeader->mFiles.mConstants.mVelocityData.mAirGetHitGroundRecovery.y, getPlayerCoordinateP(p), tCoordinateP);
}

double getPlayerVelocityAirGetHitAirRecoverMulX(DreamPlayer* p)
{
	return p->mHeader->mFiles.mConstants.mVelocityData.mAirGetHitAirRecoveryMultiplier.x;
}

double getPlayerVelocityAirGetHitAirRecoverMulY(DreamPlayer* p)
{
	return p->mHeader->mFiles.mConstants.mVelocityData.mAirGetHitAirRecoveryMultiplier.y;
}

double getPlayerVelocityAirGetHitAirRecoverAddX(DreamPlayer* p, int tCoordinateP)
{
	return transformDreamCoordinates(p->mHeader->mFiles.mConstants.mVelocityData.mAirGetHitAirRecoveryOffset.x, getPlayerCoordinateP(p), tCoordinateP);
}

double getPlayerVelocityAirGetHitAirRecoverAddY(DreamPlayer* p, int tCoordinateP)
{
	return transformDreamCoordinates(p->mHeader->mFiles.mConstants.mVelocityData.mAirGetHitAirRecoveryOffset.y, getPlayerCoordinateP(p), tCoordinateP);
}

double getPlayerVelocityAirGetHitAirRecoverBack(DreamPlayer* p, int tCoordinateP)
{
	return transformDreamCoordinates(p->mHeader->mFiles.mConstants.mVelocityData.mAirGetHitExtraXWhenHoldingBackward, getPlayerCoordinateP(p), tCoordinateP);
}

double getPlayerVelocityAirGetHitAirRecoverFwd(DreamPlayer* p, int tCoordinateP)
{
	return transformDreamCoordinates(p->mHeader->mFiles.mConstants.mVelocityData.mAirGetHitExtraXWhenHoldingForward, getPlayerCoordinateP(p), tCoordinateP);
}

double getPlayerVelocityAirGetHitAirRecoverUp(DreamPlayer* p, int tCoordinateP)
{
	return transformDreamCoordinates(p->mHeader->mFiles.mConstants.mVelocityData.mAirGetHitExtraYWhenHoldingUp, getPlayerCoordinateP(p), tCoordinateP);
}

double getPlayerVelocityAirGetHitAirRecoverDown(DreamPlayer* p, int tCoordinateP)
{
	return transformDreamCoordinates(p->mHeader->mFiles.mConstants.mVelocityData.mAirGetHitExtraYWhenHoldingDown, getPlayerCoordinateP(p), tCoordinateP);
}

int getPlayerMovementAirJumpNum(DreamPlayer* p)
{
	return p->mHeader->mFiles.mConstants.mMovementData.mAllowedAirJumpAmount;
}

void setPlayerMovementAirJumpNum(DreamPlayer* p, int tAmount)
{
	p->mHeader->mFiles.mConstants.mMovementData.mAllowedAirJumpAmount = tAmount;
}

int getPlayerMovementAirJumpHeight(DreamPlayer* p, int tCoordinateP)
{
	return transformDreamCoordinatesI(p->mHeader->mFiles.mConstants.mMovementData.mAirJumpMinimumHeight, getPlayerCoordinateP(p), tCoordinateP);
}

double getPlayerMovementJumpChangeAnimThreshold(DreamPlayer* p, int tCoordinateP)
{
	return transformDreamCoordinates(p->mHeader->mFiles.mConstants.mMovementData.mJumpChangeAnimThreshold, getPlayerCoordinateP(p), tCoordinateP);
}

double getPlayerMovementAirGetHitAirRecoverYAccel(DreamPlayer* p, int tCoordinateP)
{
	return transformDreamCoordinates(p->mHeader->mFiles.mConstants.mMovementData.mAirGetHitAirRecoveryVerticalAcceleration, getPlayerCoordinateP(p), tCoordinateP);
}

double getPlayerStandFriction(DreamPlayer* p)
{
	return p->mHeader->mFiles.mConstants.mMovementData.mStandFiction;
}

double getPlayerStandFrictionThreshold(DreamPlayer* p, int tCoordinateP)
{
	return transformDreamCoordinates(p->mHeader->mFiles.mConstants.mMovementData.mStandFrictionThreshold, getPlayerCoordinateP(p), tCoordinateP);
}

double getPlayerCrouchFriction(DreamPlayer* p)
{
	return p->mHeader->mFiles.mConstants.mMovementData.mCrouchFriction;
}

double getPlayerCrouchFrictionThreshold(DreamPlayer* p, int tCoordinateP)
{
	return transformDreamCoordinates(p->mHeader->mFiles.mConstants.mMovementData.mCrouchFrictionThreshold, getPlayerCoordinateP(p), tCoordinateP);
}

double getPlayerAirGetHitGroundLevelY(DreamPlayer* p, int tCoordinateP)
{
	return transformDreamCoordinates(p->mHeader->mFiles.mConstants.mMovementData.mAirGetHitGroundLevelY, getPlayerCoordinateP(p), tCoordinateP);
}

double getPlayerAirGetHitGroundRecoveryGroundLevelY(DreamPlayer* p, int tCoordinateP)
{
	return transformDreamCoordinates(p->mHeader->mFiles.mConstants.mMovementData.mAirGetHitGroundRecoveryGroundGoundLevelY, getPlayerCoordinateP(p), tCoordinateP);
}

double getPlayerAirGetHitGroundRecoveryGroundYTheshold(DreamPlayer* p, int tCoordinateP)
{
	return transformDreamCoordinates(p->mHeader->mFiles.mConstants.mMovementData.mAirGetHitGroundRecoveryGroundYTheshold, getPlayerCoordinateP(p), tCoordinateP);
}

double getPlayerAirGetHitAirRecoveryVelocityYThreshold(DreamPlayer* p, int tCoordinateP)
{
	return transformDreamCoordinates(p->mHeader->mFiles.mConstants.mMovementData.mAirGetHitAirRecoveryVelocityYThreshold, getPlayerCoordinateP(p), tCoordinateP);
}

double getPlayerAirGetHitTripGroundLevelY(DreamPlayer* p, int tCoordinateP)
{
	return transformDreamCoordinates(p->mHeader->mFiles.mConstants.mMovementData.mAirGetHitTripGroundLevelY, getPlayerCoordinateP(p), tCoordinateP);
}

double getPlayerDownBounceOffsetX(DreamPlayer* p, int tCoordinateP)
{
	return transformDreamCoordinates(p->mHeader->mFiles.mConstants.mMovementData.mBounceOffset.x, getPlayerCoordinateP(p), tCoordinateP);
}

double getPlayerDownBounceOffsetY(DreamPlayer* p, int tCoordinateP)
{
	return transformDreamCoordinates(p->mHeader->mFiles.mConstants.mMovementData.mBounceOffset.y, getPlayerCoordinateP(p), tCoordinateP);
}

double getPlayerDownVerticalBounceAcceleration(DreamPlayer* p, int tCoordinateP)
{
	return transformDreamCoordinates(p->mHeader->mFiles.mConstants.mMovementData.mVerticalBounceAcceleration, getPlayerCoordinateP(p), tCoordinateP);
}

double getPlayerDownBounceGroundLevel(DreamPlayer* p, int tCoordinateP)
{
	return transformDreamCoordinates(p->mHeader->mFiles.mConstants.mMovementData.mBounceGroundLevel, getPlayerCoordinateP(p), tCoordinateP);
}

double getPlayerLyingDownFrictionThreshold(DreamPlayer* p, int tCoordinateP)
{
	return transformDreamCoordinates(p->mHeader->mFiles.mConstants.mMovementData.mLyingDownFrictionThreshold, getPlayerCoordinateP(p), tCoordinateP);
}

double getPlayerVerticalAcceleration(DreamPlayer* p, int tCoordinateP)
{
	return transformDreamCoordinates(p->mHeader->mFiles.mConstants.mMovementData.mVerticalAcceleration, getPlayerCoordinateP(p), tCoordinateP);
}

double getPlayerForwardWalkVelocityX(DreamPlayer* p, int tCoordinateP)
{
	return transformDreamCoordinates(p->mHeader->mFiles.mConstants.mVelocityData.mWalkForward.x, getPlayerCoordinateP(p), tCoordinateP);
}

double getPlayerBackwardWalkVelocityX(DreamPlayer* p, int tCoordinateP)
{
	return transformDreamCoordinates(p->mHeader->mFiles.mConstants.mVelocityData.mWalkBackward.x, getPlayerCoordinateP(p), tCoordinateP);
}

double getPlayerForwardRunVelocityX(DreamPlayer* p, int tCoordinateP)
{
	return transformDreamCoordinates(p->mHeader->mFiles.mConstants.mVelocityData.mRunForward.x, getPlayerCoordinateP(p), tCoordinateP);
}

double getPlayerForwardRunVelocityY(DreamPlayer* p, int tCoordinateP)
{
	return transformDreamCoordinates(p->mHeader->mFiles.mConstants.mVelocityData.mRunForward.y, getPlayerCoordinateP(p), tCoordinateP);
}

double getPlayerBackwardRunVelocityX(DreamPlayer* p, int tCoordinateP)
{
	return transformDreamCoordinates(p->mHeader->mFiles.mConstants.mVelocityData.mRunBackward.x, getPlayerCoordinateP(p), tCoordinateP);
}

double getPlayerBackwardRunVelocityY(DreamPlayer* p, int tCoordinateP)
{
	return transformDreamCoordinates(p->mHeader->mFiles.mConstants.mVelocityData.mRunBackward.y, getPlayerCoordinateP(p), tCoordinateP);
}

double getPlayerBackwardRunJumpVelocityX(DreamPlayer* p, int tCoordinateP)
{
	return transformDreamCoordinates(p->mHeader->mFiles.mConstants.mVelocityData.mRunJumpBackward.x, getPlayerCoordinateP(p), tCoordinateP);
}

double getPlayerForwardRunJumpVelocityX(DreamPlayer* p, int tCoordinateP)
{
	return transformDreamCoordinates(p->mHeader->mFiles.mConstants.mVelocityData.mRunJumpForward.x, getPlayerCoordinateP(p), tCoordinateP);
}

double getPlayerNeutralJumpVelocityX(DreamPlayer* p, int tCoordinateP)
{
	return transformDreamCoordinates(p->mHeader->mFiles.mConstants.mVelocityData.mJumpNeutral.x, getPlayerCoordinateP(p), tCoordinateP);
}

double getPlayerForwardJumpVelocityX(DreamPlayer* p, int tCoordinateP)
{
	return transformDreamCoordinates(p->mHeader->mFiles.mConstants.mVelocityData.mJumpForward.x, getPlayerCoordinateP(p), tCoordinateP);
}

double getPlayerBackwardJumpVelocityX(DreamPlayer* p, int tCoordinateP)
{
	return transformDreamCoordinates(p->mHeader->mFiles.mConstants.mVelocityData.mJumpBackward.x, getPlayerCoordinateP(p), tCoordinateP);
}

double getPlayerJumpVelocityY(DreamPlayer* p, int tCoordinateP)
{
	return transformDreamCoordinates(p->mHeader->mFiles.mConstants.mVelocityData.mJumpNeutral.y, getPlayerCoordinateP(p), tCoordinateP);
}

double getPlayerNeutralAirJumpVelocityX(DreamPlayer* p, int tCoordinateP)
{
	return transformDreamCoordinates(p->mHeader->mFiles.mConstants.mVelocityData.mAirJumpNeutral.x, getPlayerCoordinateP(p), tCoordinateP);
}

double getPlayerForwardAirJumpVelocityX(DreamPlayer* p, int tCoordinateP)
{
	return transformDreamCoordinates(p->mHeader->mFiles.mConstants.mVelocityData.mAirJumpForward.x, getPlayerCoordinateP(p), tCoordinateP);
}

double getPlayerBackwardAirJumpVelocityX(DreamPlayer* p, int tCoordinateP)
{
	return transformDreamCoordinates(p->mHeader->mFiles.mConstants.mVelocityData.mAirJumpBackward.x, getPlayerCoordinateP(p), tCoordinateP);
}

double getPlayerAirJumpVelocityY(DreamPlayer* p, int tCoordinateP)
{
	return transformDreamCoordinates(p->mHeader->mFiles.mConstants.mVelocityData.mAirJumpNeutral.y, getPlayerCoordinateP(p), tCoordinateP);
}

int isPlayerAlive(DreamPlayer* p)
{
	return p->mIsAlive;
}

int isPlayerDestroyed(DreamPlayer* p)
{
	return p->mIsDestroyed;
}

void setPlayerVelocityX(DreamPlayer* p, double x, int tCoordinateP)
{
	Velocity* vel = getHandledPhysicsVelocityReference(p->mPhysicsElement);
	double fx = transformDreamCoordinates(x, tCoordinateP, getDreamMugenStageHandlerCameraCoordinateP());
	if (p->mFaceDirection == FACE_DIRECTION_LEFT) fx *= -1;
	vel->x = fx;
}

void setPlayerVelocityY(DreamPlayer* p, double y, int tCoordinateP)
{
	Velocity* vel = getHandledPhysicsVelocityReference(p->mPhysicsElement);
	vel->y = transformDreamCoordinates(y, tCoordinateP, getDreamMugenStageHandlerCameraCoordinateP());
}

void multiplyPlayerVelocityX(DreamPlayer* p, double x)
{
	Velocity* vel = getHandledPhysicsVelocityReference(p->mPhysicsElement);
	vel->x *= x;
}

void multiplyPlayerVelocityY(DreamPlayer* p, double y)
{
	Velocity* vel = getHandledPhysicsVelocityReference(p->mPhysicsElement);
	vel->y *= y;
}

void addPlayerVelocityX(DreamPlayer* p, double x, int tCoordinateP)
{
	if (isPlayerPaused(p)) return;

	Velocity* vel = getHandledPhysicsVelocityReference(p->mPhysicsElement);
	double fx = transformDreamCoordinates(x, tCoordinateP, getDreamMugenStageHandlerCameraCoordinateP());
	if (p->mFaceDirection == FACE_DIRECTION_LEFT) fx *= -1;
	vel->x += fx;
}

void addPlayerVelocityY(DreamPlayer* p, double y, int tCoordinateP)
{
	if (isPlayerPaused(p)) return;

	Velocity* vel = getHandledPhysicsVelocityReference(p->mPhysicsElement);
	vel->y += transformDreamCoordinates(y, tCoordinateP, getDreamMugenStageHandlerCameraCoordinateP());
}

void setPlayerPosition(DreamPlayer* p, const Position2D& tPosition, int tCoordinateP)
{
	setPlayerPositionX(p, tPosition.x, tCoordinateP);
	setPlayerPositionY(p, tPosition.y, tCoordinateP);
}

void setPlayerPositionX(DreamPlayer* p, double x, int tCoordinateP)
{
	Position* pos = getHandledPhysicsPositionReference(p->mPhysicsElement);
	pos->x = transformDreamCoordinates(x, tCoordinateP, getDreamMugenStageHandlerCameraCoordinateP());
}

void setPlayerPositionY(DreamPlayer* p, double y, int tCoordinateP)
{
	Position* pos = getHandledPhysicsPositionReference(p->mPhysicsElement);
	pos->y = transformDreamCoordinates(y, tCoordinateP, getDreamMugenStageHandlerCameraCoordinateP());
}

void addPlayerPositionX(DreamPlayer* p, double x, int tCoordinateP)
{
	if (isPlayerPaused(p)) return;

	Position* pos = getHandledPhysicsPositionReference(p->mPhysicsElement);
	double fx = transformDreamCoordinates(x, tCoordinateP, getDreamMugenStageHandlerCameraCoordinateP());
	if (p->mFaceDirection == FACE_DIRECTION_LEFT) fx *= -1;
	pos->x += fx;
}

void addPlayerPositionY(DreamPlayer* p, double y, int tCoordinateP)
{
	if (isPlayerPaused(p)) return;

	Position* pos = getHandledPhysicsPositionReference(p->mPhysicsElement);
	pos->y += transformDreamCoordinates(y, tCoordinateP, getDreamMugenStageHandlerCameraCoordinateP());;
}

void setPlayerPositionBasedOnScreenCenterX(DreamPlayer* p, double x, int tCoordinateP) {
	const auto pos = getDreamStageCenterOfScreenBasedOnPlayer(tCoordinateP);
	double nx = x + pos.x;
	setPlayerPositionX(p, nx, tCoordinateP);
}

int isPlayerCommandActive(DreamPlayer* p, const char * tCommandName)
{
	return isDreamCommandActive(p->mCommandID, tCommandName);
}

int isPlayerCommandActiveWithLookup(DreamPlayer* p, int tCommandLookupIndex)
{
	return isDreamCommandActiveByLookupIndex(p->mCommandID, tCommandLookupIndex);
}



int hasPlayerState(DreamPlayer* p, int mNewState)
{
	return hasDreamHandledStateMachineState(p->mRegisteredStateMachine, mNewState);
}

int hasPlayerStateSelf(DreamPlayer* p, int mNewState)
{
	return hasDreamHandledStateMachineStateSelf(p->mRegisteredStateMachine, mNewState);
}

void changePlayerState(DreamPlayer* p, int mNewState)
{
	setProfilingSectionMarkerCurrentFunction();
	if (getPlayerControl(p) && !isInOwnStateMachine(p->mRegisteredStateMachine)) {
		changeDreamHandledStateMachineStateToOwnStateMachineWithoutChangingState(p->mRegisteredStateMachine);
	}

	if (!hasPlayerState(p, mNewState)) {
		logWarning("Trying to change into state that does not exist");
		logWarningInteger(getPlayerState(p));
		logWarningInteger(mNewState);
		return;
	}
	changeDreamHandledStateMachineState(p->mRegisteredStateMachine, mNewState);
	setDreamRegisteredStateTimeInState(p->mRegisteredStateMachine, -1);
	updateDreamSingleStateMachineByID(p->mRegisteredStateMachine);
}

void changePlayerStateToSelf(DreamPlayer* p, int mNewState)
{
	changeDreamHandledStateMachineStateToOwnStateMachineWithoutChangingState(p->mRegisteredStateMachine);
	changePlayerState(p, mNewState);
}

void changePlayerStateToOtherPlayerStateMachine(DreamPlayer* p, DreamPlayer * tOtherPlayer, int mNewState)
{
	changeDreamHandledStateMachineStateToOtherPlayerStateMachine(p->mRegisteredStateMachine, tOtherPlayer->mRegisteredStateMachine, mNewState);
	setDreamRegisteredStateTimeInState(p->mRegisteredStateMachine, -1);
	updateDreamSingleStateMachineByID(p->mRegisteredStateMachine);
}

void changePlayerStateToOtherPlayerStateMachineBeforeImmediatelyEvaluatingIt(DreamPlayer* p, DreamPlayer * tOtherPlayer, int mNewState)
{
	changeDreamHandledStateMachineStateToOtherPlayerStateMachine(p->mRegisteredStateMachine, tOtherPlayer->mRegisteredStateMachine, mNewState);
}

void changePlayerStateBeforeImmediatelyEvaluatingIt(DreamPlayer* p, int mNewState)
{
	if (getPlayerControl(p) && !isInOwnStateMachine(p->mRegisteredStateMachine)) {
		changeDreamHandledStateMachineStateToOwnStateMachineWithoutChangingState(p->mRegisteredStateMachine);
	}

	if (!hasPlayerState(p, mNewState)) {
		logWarning("Trying to change into state that does not exist");
		logWarningInteger(getPlayerState(p));
		logWarningInteger(mNewState);
		return;
	}
	changeDreamHandledStateMachineState(p->mRegisteredStateMachine, mNewState);

}

void changePlayerStateToSelfBeforeImmediatelyEvaluatingIt(DreamPlayer* p, int tNewState)
{
	changeDreamHandledStateMachineStateToOwnStateMachine(p->mRegisteredStateMachine, tNewState);
}

void changePlayerStateIfDifferent(DreamPlayer* p, int tNewState)
{
	if(getPlayerState(p) == tNewState) return;
	changePlayerState(p, tNewState);
}

void setPlayerStatemachineToUpdateAgain(DreamPlayer* p)
{
	setDreamSingleStateMachineToUpdateAgainByID(p->mRegisteredStateMachine);
}

void changePlayerAnimation(DreamPlayer* p, int tNewAnimation)
{
	changePlayerAnimationWithStartStep(p, tNewAnimation, 1);
}

void changePlayerAnimationWithStartStep(DreamPlayer* p, int tNewAnimation, int tStartStep)
{
	p->mActiveAnimations = &p->mHeader->mFiles.mAnimations;
	if (!hasMugenAnimation(&p->mHeader->mFiles.mAnimations, tNewAnimation)) {
		logWarningFormat("Unable to find animation %d for player %d %d. Ignoring.", tNewAnimation, p->mRootID, p->mID);
		return;
	}
	MugenAnimation* newAnimation = getMugenAnimation(&p->mHeader->mFiles.mAnimations, tNewAnimation);
	changeMugenAnimationWithStartStep(p->mAnimationElement, newAnimation, tStartStep);
	changeMugenAnimationWithStartStep(p->mShadow.mAnimationElement, newAnimation, tStartStep);
	changeMugenAnimationWithStartStep(p->mReflection.mAnimationElement, newAnimation, tStartStep);

	setMugenAnimationCoordinateSystemScale(p->mAnimationElement, 1.0);
	setMugenAnimationCoordinateSystemScale(p->mShadow.mAnimationElement, 1.0);
	setMugenAnimationCoordinateSystemScale(p->mReflection.mAnimationElement, 1.0);
}

void changePlayerAnimationToPlayer2AnimationWithStartStep(DreamPlayer* p, int tNewAnimation, int tStartStep)
{
	if (isInOwnStateMachine(p->mRegisteredStateMachine)) {
		changePlayerAnimationWithStartStep(p, tNewAnimation, tStartStep);
		return;
	}

	DreamPlayer* otherPlayer = getPlayerOtherPlayer(p);
	p->mActiveAnimations = &otherPlayer->mHeader->mFiles.mAnimations;
	if (!hasMugenAnimation(&otherPlayer->mHeader->mFiles.mAnimations, tNewAnimation)) {
		logWarningFormat("Unable to find animation %d for player %d %d from other player2. Ignoring.", tNewAnimation, p->mRootID, p->mID);
		return;
	}
	MugenAnimation* newAnimation = getMugenAnimation(&otherPlayer->mHeader->mFiles.mAnimations, tNewAnimation);
	changeMugenAnimationWithStartStep(p->mAnimationElement, newAnimation, tStartStep);
	changeMugenAnimationWithStartStep(p->mShadow.mAnimationElement, newAnimation, tStartStep);
	changeMugenAnimationWithStartStep(p->mReflection.mAnimationElement, newAnimation, tStartStep);

	const auto playerCoordinateSystemScale = getPlayerCoordinateP(p) / double(getPlayerCoordinateP(otherPlayer));
	setMugenAnimationCoordinateSystemScale(p->mAnimationElement, playerCoordinateSystemScale);
	setMugenAnimationCoordinateSystemScale(p->mShadow.mAnimationElement, playerCoordinateSystemScale);
	setMugenAnimationCoordinateSystemScale(p->mReflection.mAnimationElement, playerCoordinateSystemScale);
}

void setPlayerAnimationFinishedCallback(DreamPlayer* p, void(*tFunc)(void *), void * tCaller)
{
	setMugenAnimationCallback(p->mAnimationElement, tFunc, tCaller);
}

int isPlayerStartingAnimationElementWithID(DreamPlayer* p, int tStepID)
{
	return isStartingMugenAnimationElementWithID(p->mAnimationElement, tStepID);
}

int getPlayerTimeFromAnimationElement(DreamPlayer* p, int tStep)
{
	return getTimeFromMugenAnimationElement(p->mAnimationElement, tStep);
}

int getPlayerAnimationElementFromTimeOffset(DreamPlayer* p, int tTime)
{
	return getMugenAnimationElementFromTimeOffset(p->mAnimationElement, tTime);
}

int isPlayerAnimationTimeOffsetInAnimation(DreamPlayer* p, int tTime) {
	
	return isMugenAnimationTimeOffsetInAnimation(p->mAnimationElement, tTime);
}

int getPlayerAnimationTimeWhenStepStarts(DreamPlayer* p, int tStep) {
	return getMugenAnimationTimeWhenStepStarts(p->mAnimationElement, tStep);
}

double calculateSpriteZFromSpritePriority(int tPriority, int tRootID, int tIsExplod)
{
	return PLAYER_Z + tPriority * PLAYER_Z_PRIORITY_DELTA + tRootID * PLAYER_Z_PLAYER_2_OFFSET - tIsExplod * EXPLOD_SPRITE_Z_OFFSET;
}

void setPlayerSpritePriority(DreamPlayer* p, int tPriority)
{
	Position pos = getMugenAnimationPosition(p->mAnimationElement);
	pos.z = calculateSpriteZFromSpritePriority(tPriority, p->mRootID, 0);
	setMugenAnimationPosition(p->mAnimationElement, pos);
}

void setPlayerNoWalkFlag(DreamPlayer* p)
{
	p->mNoWalkFlag = 1;
}

void setPlayerNoAutoTurnFlag(DreamPlayer* p)
{
	p->mNoAutoTurnFlag = 1;
}

void setPlayerInvisibleFlag(DreamPlayer* p)
{
	setMugenAnimationInvisibleForOneFrame(p->mAnimationElement); 
	setMugenAnimationInvisibleForOneFrame(p->mShadow.mAnimationElement);
	setMugenAnimationInvisibleForOneFrame(p->mReflection.mAnimationElement);
	p->mInvisibilityFlag = 1;
}

void setPlayerNoLandFlag(DreamPlayer* p)
{
	p->mNoLandFlag = 1;
}

void setPlayerNoShadow(DreamPlayer* p)
{
	setMugenAnimationInvisibleForOneFrame(p->mShadow.mAnimationElement);
}

void setAllPlayersNoShadow()
{
	setPlayerNoShadow(getRootPlayer(0));
	setPlayerNoShadow(getRootPlayer(1));
}

void setPlayerPushDisabledFlag(DreamPlayer* p, int tIsDisabled)
{
	p->mPushDisabledFlag = tIsDisabled;
}

void setPlayerNoJuggleCheckFlag(DreamPlayer* p)
{
	p->mNoJuggleCheckFlag = 1;
}

void setPlayerIntroFlag(DreamPlayer* p)
{
	p->mIntroFlag = 1;
}

void setPlayerNoAirGuardFlag(DreamPlayer* p)
{
	p->mNoAirGuardFlag = 1;
}

void setPlayerNoCrouchGuardFlag(DreamPlayer* p)
{
	p->mNoCrouchGuardFlag = 1;

}

void setPlayerNoStandGuardFlag(DreamPlayer* p)
{
	p->mNoStandGuardFlag = 1;
}

void setPlayerNoKOSoundFlag(DreamPlayer* p)
{
	p->mNoKOSoundFlag = 1;
}

void setPlayerNoKOSlowdownFlag(DreamPlayer* p)
{
	p->mNoKOSlowdownFlag = 1;
}

void setPlayerUnguardableFlag(DreamPlayer* p)
{
	p->mUnguardableFlag = 1;
}

int getPlayerNoKOSlowdownFlag(DreamPlayer* p)
{
	return p->mNoKOSlowdownFlag;
}

int getPlayerUnguardableFlag(DreamPlayer* p)
{
	return p->mUnguardableFlag;
}

int isPlayerInIntro(DreamPlayer* p)
{
	int ret = p->mIntroFlag;
	p->mIntroFlag = 0;
	return ret;
}

int doesPlayerHaveAnimation(DreamPlayer* p, int tAnimation)
{
	return hasMugenAnimation(p->mActiveAnimations, tAnimation);
}

int doesPlayerHaveAnimationHimself(DreamPlayer* p, int tAnimation)
{
	return hasMugenAnimation(&p->mHeader->mFiles.mAnimations, tAnimation);
}

int isPlayerHitShakeOver(DreamPlayer* p)
{
	return !p->mIsHitShakeActive;
}

void setPlayerHitShakeOver(DreamPlayer* p) {
	p->mIsHitShakeActive = 0;

	int hitDuration;
	if (isPlayerGuarding(p)) {
		hitDuration = getActiveHitDataGuardHitTime(p);
	}
	else if (getPlayerStateType(p) == MUGEN_STATE_TYPE_AIR) {
		hitDuration = getActiveHitDataAirHitTime(p);
	}
	else {
		hitDuration = getActiveHitDataGroundHitTime(p);
	}

	setPlayerVelocityX(p, getActiveHitDataVelocityX(p), getPlayerCoordinateP(p));
	setPlayerVelocityY(p, getActiveHitDataVelocityY(p), getPlayerCoordinateP(p));

	p->mIsHitOverWaitActive = 1;
	p->mHitOverNow = 0;
	p->mHitOverDuration = hitDuration;
}

int isPlayerHitOver(DreamPlayer* p)
{
	return p->mIsHitOver;
}

void setPlayerHitOver(DreamPlayer* p) {
	setActiveHitDataInactive(p);
	setPlayerSuperDefenseMultiplier(p, 1.0);
	p->mIsHitOverWaitActive = 0;
	p->mIsHitOver = 1;
}

int getPlayerHitTime(DreamPlayer* p)
{
	if (!p->mIsHitShakeActive) return -1;
	return p->mHitShakeDuration - p->mHitShakeNow;
}

double getPlayerHitVelocityX(DreamPlayer* p, int tCoordinateP)
{
	if (isPlayerHitOver(p)) return 0.0;
	return -getPlayerVelocityX(p, tCoordinateP);	
}

double getPlayerHitVelocityY(DreamPlayer* p, int tCoordinateP)
{
	if (isPlayerHitOver(p)) return 0.0;
	return -getPlayerVelocityY(p, tCoordinateP);
}

int isPlayerFalling(DreamPlayer* p) {
	return p->mIsFalling;
}

int canPlayerRecoverFromFalling(DreamPlayer* p)
{
	return isPlayerFalling(p) && p->mCanRecoverFromFall && p->mRecoverTimeSinceHitPause >= p->mRecoverTime;
}

int getPlayerSlideTime(DreamPlayer* p)
{
	return isPlayerGuarding(p) ? getActiveHitDataGuardSlideTime(p) : getActiveHitDataGroundSlideTime(p);
}
 
double getPlayerDefenseMultiplier(DreamPlayer* p)
{
	return p->mDefenseMultiplier * p->mSuperDefenseMultiplier * getPlayerDataDefenseFactor(p);
}

double getInvertedPlayerDefenseMultiplier(DreamPlayer* p)
{
	return 1.0 / getPlayerDefenseMultiplier(p);
}

void setPlayerDefenseMultiplier(DreamPlayer* p, double tValue)
{
	p->mDefenseMultiplier = tValue;
}

void setPlayerSuperDefenseMultiplier(DreamPlayer* p, double tValue)
{
	p->mSuperDefenseMultiplier = tValue;
}

void setPlayerPositionFrozen(DreamPlayer* p)
{
	p->mIsFrozen = 1;
	p->mFreezePosition = *getHandledPhysicsPositionReference(p->mPhysicsElement);
}

void setPlayerPositionUnfrozen(DreamPlayer* p)
{
	p->mIsFrozen = 0;
}

MugenSpriteFile * getPlayerSprites(DreamPlayer* p)
{
	return &p->mHeader->mFiles.mSprites;
}

MugenAnimations * getPlayerAnimations(DreamPlayer* p)
{
	return &p->mHeader->mFiles.mAnimations;
}

MugenAnimation * getPlayerAnimation(DreamPlayer* p, int tNumber)
{
	return getMugenAnimation(&p->mHeader->mFiles.mAnimations, tNumber);
}

MugenSounds * getPlayerSounds(DreamPlayer* p)
{
	return &p->mHeader->mFiles.mSounds;
}

void setCustomPlayerDisplayName(int i, const std::string& tName)
{
	auto header = &gPlayerDefinition.mPlayerHeader[i];
	strcpy(header->mCustomOverrides.mDisplayName, tName.c_str());
	header->mCustomOverrides.mHasCustomDisplayName = 1;
}

int getPlayerCoordinateP(DreamPlayer* p)
{
	return p->mHeader->mConstants.mLocalCoordinates.x;
}

char * getPlayerDisplayName(DreamPlayer* p)
{
	return p->mHeader->mCustomOverrides.mHasCustomDisplayName ? p->mHeader->mCustomOverrides.mDisplayName : p->mHeader->mConstants.mDisplayName;
}

char * getPlayerName(DreamPlayer* p)
{
	return p->mHeader->mConstants.mName;
}

char * getPlayerAuthorName(DreamPlayer* p)
{
	return p->mHeader->mConstants.mAuthor;
}

int isPlayerPaused(DreamPlayer* p)
{
	return p->mIsHitPaused || (getPlayerSpeed(p) == 0);
}

int isPlayerHitPaused(DreamPlayer* p)
{
	return p->mIsHitPaused;
}

static void pausePlayer(DreamPlayer* p) {
	if (isPlayerPaused(p)) return;

	pauseHandledPhysics(p->mPhysicsElement);
	pauseMugenAnimation(p->mAnimationElement);
	pauseMugenAnimation(p->mShadow.mAnimationElement);
	pauseMugenAnimation(p->mReflection.mAnimationElement);
	pauseDreamRegisteredStateMachine(p->mRegisteredStateMachine);
}

static void forceUnpausePlayer(DreamPlayer* p) {
	resumeHandledPhysics(p->mPhysicsElement);
	unpauseMugenAnimation(p->mAnimationElement);
	unpauseMugenAnimation(p->mShadow.mAnimationElement);
	unpauseMugenAnimation(p->mReflection.mAnimationElement);
	unpauseDreamRegisteredStateMachine(p->mRegisteredStateMachine);
	p->mIsHitPaused = 0;
}

static void unpausePlayer(DreamPlayer* p) {
	if (isPlayerPaused(p)) return;
	forceUnpausePlayer(p);
}

void setPlayerHitPaused(DreamPlayer* p, int tDuration)
{
	if (isPlayerProjectile(p)) return;
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

static void setPlayerDead(DreamPlayer* p) {
	if (gPlayerDefinition.mIsInTrainingMode) return;
	if (!p->mIsAlive) return;

	p->mIsAlive = 0;
	
	if (!p->mNoKOSoundFlag) {
		tryPlayMugenSoundAdvanced(&p->mHeader->mFiles.mSounds, 11, 0, getPlayerMidiVolumeForPrism(p));
	}
	const auto activeVelocity = transformDreamCoordinates(getActiveHitDataVelocityY(p), getPlayerCoordinateP(p), 320);
	if (activeVelocity >= -6.0) {
		setActiveHitDataVelocityY(p, -6.0, 320);
	}
	else {
		setActiveHitDataVelocityY(p, activeVelocity - 2.0, 320);
	}
	changePlayerStateToSelf(p, 5080);
}

double getPlayerDeathVelAddY(DreamPlayer* p, int tCoordinateP)
{
	const auto activeVelocity = getActiveHitDataVelocityY(p);
	const auto offsetPlayerSpace = transformDreamCoordinates(-6.0, 320, getPlayerCoordinateP(p));
	if (activeVelocity >= offsetPlayerSpace) {
		return transformDreamCoordinates(offsetPlayerSpace - activeVelocity, getPlayerCoordinateP(p), tCoordinateP);
	}
	else {
		return transformDreamCoordinates(-2.0, 320, tCoordinateP);
	}
}

void addPlayerDamage(DreamPlayer* p, DreamPlayer* tDamagingPlayer, int tDamage)
{
	p->mLife -= tDamage;
	if (p->mLife <= 0) {
		p->mCheeseWinFlag = isPlayerGuarding(p);
		p->mSuicideWinFlag = (p->mRootID == tDamagingPlayer->mRootID);
		setPlayerDead(p);
		p->mLife = 0;
	}
	p->mLife = min(p->mLife, getPlayerLifeMax(p));

	if (!isPlayerHelper(p) && !isPlayerProjectile(p)) {
		double perc = p->mLife / (double)p->mHeader->mFiles.mConstants.mHeader.mLife;
		setDreamLifeBarPercentage(p, perc);
	}

}

int getPlayerTargetAmount(DreamPlayer* p)
{
	return getPlayerTargetAmountWithID(p, -1);
}

int getPlayerTargetAmountWithID(DreamPlayer* p, int tID)
{
	int amount = 0;
	for (const auto& targetPair : p->mActiveTargets) {
		if (!isPlayer(targetPair.second)) continue;
		if (tID == -1 || tID == targetPair.first) {
			amount++;
		}
	}
	return amount;
}

DreamPlayer* getPlayerTargetWithID(DreamPlayer* p, int tID)
{
	DreamPlayer* returnPlayer = NULL;
	for (const auto& targetPair : p->mActiveTargets) {
		if (!isPlayer(targetPair.second)) continue;
		if (tID == -1 || tID == targetPair.first) {
			returnPlayer = targetPair.second;
		}
	}
	return returnPlayer;
}

void dropPlayerTargets(DreamPlayer* p, int tExcludeID, int tIsKeepingOneAtMost)
{
	auto it = p->mActiveTargets.begin();
	while (it != p->mActiveTargets.end()) {
		auto current = it;
		it++;
		if (tExcludeID == -1 || tExcludeID != current->first) {
			p->mActiveTargets.erase(current);
		}
	}

	while (tIsKeepingOneAtMost && p->mActiveTargets.size() > 1) {
		p->mActiveTargets.erase(p->mActiveTargets.begin());
	}
}

DreamPlayer* getPlayerByIndex(int i) {
	i = min(i, list_size(&gPlayerDefinition.mAllPlayers) - 1);
	DreamPlayer* p = (DreamPlayer*)list_get(&gPlayerDefinition.mAllPlayers, i);
	return p;
}

int getTotalPlayerAmount()
{
	return list_size(&gPlayerDefinition.mAllPlayers);
}

int getPlayerHelperAmount(DreamPlayer* p)
{
	return getPlayerHelperAmountWithID(p, -1);
}

typedef struct {
	int mSearchID;
	int mFoundAmount;
} PlayerHelperCountCaller;

static void countHelperByIDCB(void* tCaller, void* tData) {
	PlayerHelperCountCaller* caller = (PlayerHelperCountCaller*)tCaller;
	DreamPlayer* helper = (DreamPlayer*)tData;

	if (helper->mID == caller->mSearchID || caller->mSearchID <= 0) {
		caller->mFoundAmount++;
	}

	list_map(&helper->mHelpers, countHelperByIDCB, caller);
}

int getPlayerHelperAmountWithID(DreamPlayer* p, int tID)
{
	auto root = getPlayerRoot(p);

	PlayerHelperCountCaller caller;
	caller.mFoundAmount = 0;
	caller.mSearchID = tID;
	list_map(&root->mHelpers, countHelperByIDCB, &caller);

	return caller.mFoundAmount;
}

typedef struct {
	DreamPlayer* mFoundHelper;
	int mSearchID;

} PlayerHelperFindCaller;

static void findHelperByIDCB(void* tCaller, void* tData) {
	PlayerHelperFindCaller* caller = (PlayerHelperFindCaller*)tCaller;
	DreamPlayer* helper = (DreamPlayer*)tData;

	if (helper->mID == caller->mSearchID) {
		caller->mFoundHelper = helper;
	}

	list_map(&helper->mHelpers, findHelperByIDCB, caller);
}

DreamPlayer * getPlayerHelperOrNullIfNonexistant(DreamPlayer* p, int tID)
{
	auto root = getPlayerRoot(p);

	PlayerHelperFindCaller caller;
	caller.mFoundHelper = NULL;
	caller.mSearchID = tID;
	list_map(&root->mHelpers, findHelperByIDCB, &caller);

	return caller.mFoundHelper;
}

int getPlayerProjectileAmount(DreamPlayer* p)
{
	return int_map_size(&p->mProjectiles);
}

typedef struct {
	int mAmount;
	int mSearchID;
} ProjectileSearchCaller;

static void playerProjectileSearchCB(void* tCaller, void* tData) {
	DreamPlayer* projectile = (DreamPlayer*)tData;
	ProjectileSearchCaller* caller = (ProjectileSearchCaller*)tCaller;

	int id = getProjectileID(projectile);
	if (id == caller->mSearchID) {
		caller->mAmount++;
	}
}

int getPlayerProjectileAmountWithID(DreamPlayer* p, int tID)
{
	DreamPlayer* root = p->mRoot;
	ProjectileSearchCaller caller;
	caller.mAmount = 0;
	caller.mSearchID = tID;
	int_map_map(&root->mProjectiles, playerProjectileSearchCB, &caller);
	return caller.mAmount;
}

int getPlayerProjectileTimeSinceCancel(DreamPlayer* p, int tID)
{
	if (!p->mHasLastContactProjectile || !p->mLastContactProjectileWasCanceled) return -1;
	if (tID > 0 && tID != p->mLastContactProjectileID) return -1;
	return p->mLastContactProjectileTime;
}

int getPlayerProjectileTimeSinceContact(DreamPlayer* p, int tID)
{
	if (!p->mHasLastContactProjectile) return -1;
	if (tID > 0 && tID != p->mLastContactProjectileID) return -1;
	return p->mLastContactProjectileTime;
}

int getPlayerProjectileTimeSinceGuarded(DreamPlayer* p, int tID)
{
	if (!p->mHasLastContactProjectile || !p->mLastContactProjectileWasGuarded) return -1;
	if (tID > 0 && tID != p->mLastContactProjectileID) return -1;
	return p->mLastContactProjectileTime;
}

int getPlayerProjectileTimeSinceHit(DreamPlayer* p, int tID)
{
	if (!p->mHasLastContactProjectile || !p->mLastContactProjectileWasHit) return -1;
	if (tID > 0 && tID != p->mLastContactProjectileID) return -1;
	return p->mLastContactProjectileTime;
}

int getPlayerProjectileHit(DreamPlayer* p, int tID)
{
	return getPlayerProjectileTimeSinceHit(p, tID);
}

int getPlayerProjectileContact(DreamPlayer* p, int tID)
{
	return getPlayerProjectileTimeSinceContact(p, tID);
}

int getPlayerProjectileGuarded(DreamPlayer* p, int tID)
{
	return getPlayerProjectileTimeSinceGuarded(p, tID);
}

void setPlayerHasOwnPalette(DreamPlayer* p, int tHasOwnPalette)
{
	p->mHasOwnPalette = tHasOwnPalette;
}

typedef struct {
	int mDuration;
	Vector3D mAddition;
	Vector3D mMultiplier;
	Vector3D mSineAmplitude;
	int mSinePeriod;
	int mInvertAll;
	double mColorFactor;
	int mIgnoreOwnPal;
} PlayerPaletteEffectCaller;

static void setPlayerSubPlayerPaletteEffectCB(void* tCaller, void* tData) {
	PlayerPaletteEffectCaller* caller = (PlayerPaletteEffectCaller*)tCaller;
	DreamPlayer* p = (DreamPlayer*)tData;
	if (!caller->mIgnoreOwnPal && p->mHasOwnPalette) return;
	setPlayerPaletteEffect(p, caller->mDuration, caller->mAddition, caller->mMultiplier, caller->mSineAmplitude, caller->mSinePeriod, caller->mInvertAll, caller->mColorFactor, caller->mIgnoreOwnPal);
}

void setPlayerPaletteEffect(DreamPlayer* p, int tDuration, const Vector3D& tAddition, const Vector3D& tMultiplier, const Vector3D& tSineAmplitude, int tSinePeriod, int tInvertAll, double tColorFactor, int tIgnoreOwnPal)
{
	setMugenAnimationPaletteEffectForDuration(p->mAnimationElement, tDuration, tAddition, tMultiplier, tSineAmplitude, tSinePeriod, tInvertAll, tColorFactor);
	setPlayerExplodPaletteEffects(p, tDuration, tAddition, tMultiplier, tSineAmplitude, tSinePeriod, tInvertAll, tColorFactor, tIgnoreOwnPal);

	PlayerPaletteEffectCaller caller;
	caller.mDuration = tDuration;
	caller.mAddition = tAddition;
	caller.mMultiplier = tMultiplier;
	caller.mSineAmplitude = tSineAmplitude;
	caller.mSinePeriod = tSinePeriod;
	caller.mInvertAll = tInvertAll;
	caller.mColorFactor = tColorFactor;
	caller.mIgnoreOwnPal = tIgnoreOwnPal;
	list_map(&p->mHelpers, setPlayerSubPlayerPaletteEffectCB, &caller);
	int_map_map(&p->mProjectiles, setPlayerSubPlayerPaletteEffectCB, &caller);
}

void remapPlayerPalette(DreamPlayer* p, const Vector2DI& tSource, const Vector2DI& tDestination)
{
	remapMugenSpriteFilePalette(getPlayerSprites(p), tSource, tDestination, p->mRootID);
}

int getPlayerTimeLeftInHitPause(DreamPlayer* p)
{
	if (!p->mIsHitPaused) return 0;

	return p->mHitPauseDuration - p->mHitPauseNow;
}

void setPlayerSuperMoveTime(DreamPlayer* p, int tSuperMoveTime)
{
	p->mSuperMoveTime = tSuperMoveTime;
}

void setPlayerPauseMoveTime(DreamPlayer* p, int tPauseMoveTime)
{
	p->mPauseMoveTime = tPauseMoveTime;
}

double getPlayerFrontAxisDistanceToScreen(DreamPlayer* p, int tCoordinateP)
{
	double x = getPlayerPositionX(p, tCoordinateP);
	double screenX = getPlayerScreenEdgeInFrontX(p, tCoordinateP);
	if (getPlayerIsFacingRight(p)) return screenX - x;
	else return x - screenX;
}

double getPlayerBackAxisDistanceToScreen(DreamPlayer* p, int tCoordinateP)
{
	double x = getPlayerPositionX(p, tCoordinateP);
	double screenX = getPlayerScreenEdgeInBackX(p, tCoordinateP);

	if (getPlayerIsFacingRight(p)) return x - screenX;
	else return screenX - x;
}

double getPlayerFrontBodyDistanceToScreen(DreamPlayer* p, int tCoordinateP)
{
	double x = getPlayerFrontXStage(p, tCoordinateP);
	double screenX = getPlayerScreenEdgeInFrontX(p, tCoordinateP);

	return fabs(screenX - x);
}

double getPlayerBackBodyDistanceToScreen(DreamPlayer* p, int tCoordinateP)
{
	double x = getPlayerBackXStage(p, tCoordinateP);
	double screenX = getPlayerScreenEdgeInBackX(p, tCoordinateP);

	return fabs(screenX - x);
}

double getPlayerFrontWidth(DreamPlayer* p, int tCoordinateP) {
	if (p->mCustomSizeData.mHasAttackWidth && getPlayerStateMoveType(p) == MUGEN_STATE_MOVE_TYPE_ATTACK) {
		return double(transformDreamCoordinatesI(p->mCustomSizeData.mAttackWidth.y, getPlayerCoordinateP(p), tCoordinateP));
	}
	else if (getPlayerStateType(p) == MUGEN_STATE_TYPE_AIR) {
		return double(transformDreamCoordinatesI(p->mCustomSizeData.mAirFrontWidth, getPlayerCoordinateP(p), tCoordinateP));
	}
	else {
		return double(transformDreamCoordinatesI(p->mCustomSizeData.mGroundFrontWidth, getPlayerCoordinateP(p), tCoordinateP));
	}
}

double getPlayerFrontWidthPlayer(DreamPlayer* p, int tCoordinateP)
{
	if (p->mWidthFlag) {
		return transformDreamCoordinates(p->mOneTickPlayerWidth.x, getPlayerCoordinateP(p), tCoordinateP);
	}
	else {
		return getPlayerFrontWidth(p, tCoordinateP);
	}
}

double getPlayerFrontWidthStage(DreamPlayer* p, int tCoordinateP)
{
	if (p->mWidthFlag) {
		return transformDreamCoordinates(p->mOneTickStageWidth.x, getPlayerCoordinateP(p), tCoordinateP);
	}
	else {
		return getPlayerFrontWidth(p, tCoordinateP);
	}
}

double getPlayerBackWidth(DreamPlayer* p, int tCoordinateP) {
	if (p->mCustomSizeData.mHasAttackWidth && getPlayerStateMoveType(p) == MUGEN_STATE_MOVE_TYPE_ATTACK) {
		return transformDreamCoordinatesI(p->mCustomSizeData.mAttackWidth.x, getPlayerCoordinateP(p), tCoordinateP);
	}
	else if (getPlayerStateType(p) == MUGEN_STATE_TYPE_AIR) {
		return double(transformDreamCoordinatesI(p->mCustomSizeData.mAirBackWidth, getPlayerCoordinateP(p), tCoordinateP));
	}
	else {
		return double(transformDreamCoordinatesI(p->mCustomSizeData.mGroundBackWidth, getPlayerCoordinateP(p), tCoordinateP));
	}
}

double getPlayerBackWidthPlayer(DreamPlayer* p, int tCoordinateP)
{
	if (p->mWidthFlag) {
		return transformDreamCoordinates(p->mOneTickPlayerWidth.y, getPlayerCoordinateP(p), tCoordinateP);
	}
	else {
		return getPlayerBackWidth(p, tCoordinateP);
	}
}

double getPlayerBackWidthStage(DreamPlayer* p, int tCoordinateP)
{
	if (p->mWidthFlag) {
		return transformDreamCoordinates(p->mOneTickStageWidth.y, getPlayerCoordinateP(p), tCoordinateP);
	}
	else {
		return getPlayerBackWidth(p, tCoordinateP);
	}
}


static double getPlayerFrontXGeneral(DreamPlayer* p, int tCoordinateP, double(*tWidthFunction)(DreamPlayer*, int))
{
	double x = getPlayerPositionX(p, tCoordinateP);
	if (p->mFaceDirection == FACE_DIRECTION_RIGHT) return x + tWidthFunction(p, tCoordinateP);
	else return x - tWidthFunction(p, tCoordinateP);
}

double getPlayerFrontX(DreamPlayer* p, int tCoordinateP)
{
	return getPlayerFrontXGeneral(p, tCoordinateP, getPlayerFrontWidth);
}

double getPlayerFrontXPlayer(DreamPlayer* p, int tCoordinateP)
{
	return getPlayerFrontXGeneral(p, tCoordinateP, getPlayerFrontWidthPlayer);
}

double getPlayerFrontXStage(DreamPlayer* p, int tCoordinateP)
{
	return getPlayerFrontXGeneral(p, tCoordinateP, getPlayerFrontWidthStage);
}

static double getPlayerBackXGeneral(DreamPlayer* p, int tCoordinateP, double(*tWidthFunction)(DreamPlayer*, int)) {
	double x = getPlayerPositionX(p, tCoordinateP);
	if (p->mFaceDirection == FACE_DIRECTION_RIGHT) return x - tWidthFunction(p, tCoordinateP);
	else return x + tWidthFunction(p, tCoordinateP);
}

double getPlayerBackX(DreamPlayer* p, int tCoordinateP)
{
	return getPlayerBackXGeneral(p, tCoordinateP, getPlayerBackWidth);
}

double getPlayerBackXPlayer(DreamPlayer* p, int tCoordinateP)
{
	return getPlayerBackXGeneral(p, tCoordinateP, getPlayerBackWidthPlayer);
}

double getPlayerBackXStage(DreamPlayer* p, int tCoordinateP)
{
	return getPlayerBackXGeneral(p, tCoordinateP, getPlayerBackWidthStage);
}

int isPlayerInCorner(DreamPlayer* p)
{
	return isPlayerInCorner(p, !getPlayerIsFacingRight(p));
}

double getPlayerScreenEdgeInFrontX(DreamPlayer* p, int tCoordinateP)
{
	const auto x = getDreamCameraPositionX(getPlayerCoordinateP(p));

	double ret;
	if (p->mFaceDirection == FACE_DIRECTION_RIGHT) ret = x + p->mHeader->mConstants.mLocalCoordinates.x;
	else ret = x;

	return transformDreamCoordinates(ret, getPlayerCoordinateP(p), tCoordinateP);
}

double getPlayerScreenEdgeInBackX(DreamPlayer* p, int tCoordinateP)
{
	const auto x = getDreamCameraPositionX(getPlayerCoordinateP(p));

	double ret;
	if (p->mFaceDirection == FACE_DIRECTION_RIGHT) ret = x;
	else ret = x + p->mHeader->mConstants.mLocalCoordinates.x;

	return transformDreamCoordinates(ret, getPlayerCoordinateP(p), tCoordinateP);
}

double getPlayerDistanceToFrontOfOtherPlayerX(DreamPlayer* p, int tCoordinateP)
{
	DreamPlayer* otherPlayer = getPlayerOtherPlayer(p);
	double x1 = getPlayerFrontXPlayer(p, tCoordinateP);
	double x2 = getPlayerFrontXPlayer(otherPlayer, tCoordinateP);

	return fabs(x2-x1);
}

static double getPlayerAxisDistanceForTwoReferencesX(DreamPlayer* p1, DreamPlayer* p2, int tCoordinateP) {

	double x1 = getPlayerPositionX(p1, tCoordinateP);
	double x2 = getPlayerPositionX(p2, tCoordinateP);

	if(getPlayerIsFacingRight(p1)) return x2 - x1;
	else return x1 - x2;
}

double getPlayerAxisDistanceForTwoReferencesY(DreamPlayer* p1, DreamPlayer* p2, int tCoordinateP)
{
	double y1 = getPlayerPositionY(p1, tCoordinateP);
	double y2 = getPlayerPositionY(p2, tCoordinateP);

	return y2 - y1;
}

double getPlayerAxisDistanceX(DreamPlayer* p, int tCoordinateP)
{
	DreamPlayer* otherPlayer = getPlayerOtherPlayer(p);
	return getPlayerAxisDistanceForTwoReferencesX(p, otherPlayer, tCoordinateP);
}

double getPlayerAxisDistanceY(DreamPlayer* p, int tCoordinateP)
{
	DreamPlayer* otherPlayer = getPlayerOtherPlayer(p);
	return getPlayerAxisDistanceForTwoReferencesY(p, otherPlayer, tCoordinateP);
}

double getPlayerDistanceToRootX(DreamPlayer* p, int tCoordinateP)
{
	DreamPlayer* otherPlayer = p->mRoot;
	return getPlayerAxisDistanceForTwoReferencesX(p, otherPlayer, tCoordinateP);
}

double getPlayerDistanceToRootY(DreamPlayer* p, int tCoordinateP)
{
	DreamPlayer* otherPlayer = p->mRoot;
	return getPlayerAxisDistanceForTwoReferencesY(p, otherPlayer, tCoordinateP);
}

double getPlayerDistanceToParentX(DreamPlayer* p, int tCoordinateP)
{
	if (!p->mParent) return 0;
	DreamPlayer* otherPlayer = p->mParent;
	return getPlayerAxisDistanceForTwoReferencesX(p, otherPlayer, tCoordinateP);
}

double getPlayerDistanceToParentY(DreamPlayer* p, int tCoordinateP)
{
	if (!p->mParent) return 0;
	DreamPlayer* otherPlayer = p->mParent;
	return getPlayerAxisDistanceForTwoReferencesY(p, otherPlayer, tCoordinateP);
}

int getPlayerGroundSizeFront(DreamPlayer* p, int tCoordinateP)
{
	return transformDreamCoordinatesI(p->mCustomSizeData.mGroundFrontWidth, getPlayerCoordinateP(p), tCoordinateP);
}

void setPlayerGroundSizeFront(DreamPlayer* p, int tGroundSizeFront, int tCoordinateP)
{
	p->mCustomSizeData.mGroundFrontWidth = transformDreamCoordinatesI(tGroundSizeFront, tCoordinateP, getPlayerCoordinateP(p));
}

int getPlayerGroundSizeBack(DreamPlayer* p, int tCoordinateP)
{
	return transformDreamCoordinatesI(p->mCustomSizeData.mGroundBackWidth, getPlayerCoordinateP(p), tCoordinateP);
}

void setPlayerGroundSizeBack(DreamPlayer* p, int tGroundSizeBack, int tCoordinateP)
{
	p->mCustomSizeData.mGroundBackWidth = transformDreamCoordinatesI(tGroundSizeBack, tCoordinateP, getPlayerCoordinateP(p));
}

int getPlayerAirSizeFront(DreamPlayer* p, int tCoordinateP)
{
	return transformDreamCoordinatesI(p->mCustomSizeData.mAirFrontWidth, getPlayerCoordinateP(p), tCoordinateP);
}

void setPlayerAirSizeFront(DreamPlayer* p, int tAirSizeFront, int tCoordinateP)
{
	p->mCustomSizeData.mAirFrontWidth = transformDreamCoordinatesI(tAirSizeFront, tCoordinateP, getPlayerCoordinateP(p));
}

int getPlayerAirSizeBack(DreamPlayer* p, int tCoordinateP)
{
	return transformDreamCoordinatesI(p->mCustomSizeData.mAirBackWidth, getPlayerCoordinateP(p), tCoordinateP);
}

void setPlayerAirSizeBack(DreamPlayer* p, int tAirSizeBack, int tCoordinateP)
{
	p->mCustomSizeData.mAirBackWidth = transformDreamCoordinatesI(tAirSizeBack, tCoordinateP, getPlayerCoordinateP(p));
}

int getPlayerHeight(DreamPlayer* p, int tCoordinateP)
{
	return transformDreamCoordinatesI(p->mCustomSizeData.mHeight, getPlayerCoordinateP(p), tCoordinateP);
}

void setPlayerHeight(DreamPlayer* p, int tHeight, int tCoordinateP)
{
	p->mCustomSizeData.mHeight = transformDreamCoordinatesI(tHeight, tCoordinateP, getPlayerCoordinateP(p));
}

static void increaseSinglePlayerRoundsExisted(DreamPlayer* p);
static void increasePlayerRoundsExistedCB(void* tCaller, void* tData) {
	(void)tCaller;
	DreamPlayer* p = (DreamPlayer*)tData;
	increaseSinglePlayerRoundsExisted(p);
}

static void increaseSinglePlayerRoundsExisted(DreamPlayer* p)
{
	p->mRoundsExisted++;
	list_map(&p->mHelpers, increasePlayerRoundsExistedCB, NULL);
}

void increasePlayerRoundsExisted()
{
	increaseSinglePlayerRoundsExisted(&gPlayerDefinition.mPlayers[0]);
	increaseSinglePlayerRoundsExisted(&gPlayerDefinition.mPlayers[1]);
}


void increasePlayerRoundsWon(DreamPlayer* p)
{
	p->mRoundsWon++;
}

int hasPlayerWonByKO(DreamPlayer* p)
{
	return isDreamRoundKO() && (getPlayerLife(getPlayerRoot(p)) > getPlayerLife(getPlayerOtherPlayer(p))) && !isTimerFinished();
}

int hasPlayerLostByKO(DreamPlayer* p)
{
	return isDreamRoundKO() && (getPlayerLife(getPlayerRoot(p)) < getPlayerLife(getPlayerOtherPlayer(p)));
}

int hasPlayerWonPerfectly(DreamPlayer* p)
{
	return hasPlayerWon(p) && p->mLife == p->mHeader->mFiles.mConstants.mHeader.mLife;
}

int hasPlayerWonByTime(DreamPlayer* p)
{
	return isDreamRoundKO() && (getPlayerLife(getPlayerRoot(p)) > getPlayerLife(getPlayerOtherPlayer(p))) && isTimerFinished();
}

int hasPlayerWon(DreamPlayer* p)
{
	return p->mRoundsWon == getRoundsToWin(); 
}

int hasPlayerLost(DreamPlayer* p)
{
	DreamPlayer* otherPlayer = getPlayerOtherPlayer(p);
	return hasPlayerWon(otherPlayer); 
}

int hasPlayerDrawn(DreamPlayer * /*p*/)
{
	return isDreamRoundDraw();
}

int hasPlayerMoveHitOtherPlayer(DreamPlayer* p)
{
	DreamMugenStateMoveType type = getPlayerStateMoveType(p);
	int isInAttackState = type == MUGEN_STATE_MOVE_TYPE_ATTACK;
	int isOtherPlayerHit = isPlayerHit(getPlayerOtherPlayer(p));
	int wasItCurrentAttack = !isHitDataActive(p);

	return isInAttackState && isOtherPlayerHit && wasItCurrentAttack;
}

int isPlayerHit(DreamPlayer* p)
{
	DreamMugenStateMoveType moveType = getPlayerStateMoveType(p);
	return moveType == MUGEN_STATE_MOVE_TYPE_BEING_HIT;
}

int getPlayerMoveReversed(DreamPlayer* p)
{
	return p->mMoveReversed;
}

int getPlayerMoveHit(DreamPlayer* p)
{
	return p->mMoveHit;
}

void setPlayerMoveHit(DreamPlayer* p)
{
	p->mMoveHit = 1;
}

void setPlayerMoveHitReset(DreamPlayer* p)
{
	p->mMoveContactCounter = 0;
	p->mMoveHit = 0;
	p->mMoveGuarded = 0;
	p->mMoveReversed = 0;
}

int getPlayerMoveGuarded(DreamPlayer* p)
{
	return p->mMoveGuarded;
}

void setPlayerMoveGuarded(DreamPlayer* p)
{
	p->mMoveGuarded = 1;
}

int getLastPlayerHitGuarded(DreamPlayer* p)
{
	return p->mLastHitGuarded;
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

int getPlayerUniqueHitCount(DreamPlayer* p)
{
	return getPlayerHitCount(p);
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

void increasePlayerComboCounter(DreamPlayer* p, int tValue)
{
	increaseComboCounter(p, tValue);
	increaseDisplayedComboCounter(p, tValue);
}

double getPlayerAttackMultiplier(DreamPlayer* p)
{
	return p->mAttackMultiplier * getPlayerDataAttackFactor(p);
}

void setPlayerAttackMultiplier(DreamPlayer* p, double tValue)
{
	p->mAttackMultiplier = tValue;
}

double getPlayerFallDefenseMultiplier(DreamPlayer* p)
{
	int f = p->mHeader->mFiles.mConstants.mHeader.mFallDefenseUp;
	return 100.0 / (f+100);
}

void setPlayerHuman(int i, int tCustomControllerUsed)
{
	DreamPlayer* p = getRootPlayer(i);
	p->mAILevel = 0;
	setDreamCommandInputControllerUsed(i, ((tCustomControllerUsed == -1) ? i : tCustomControllerUsed));
}

void setPlayerArtificial(int i, int tValue)
{
	DreamPlayer* p = getRootPlayer(i);
	p->mAILevel = tValue;
	setDreamCommandInputControllerUsed(i, i);
}

int isPlayerHuman(DreamPlayer* p) {
	return !getPlayerAILevel(p);
}

int getPlayerAILevel(DreamPlayer* p)
{
	return p->mAILevel;
}

void setPlayerStartLifePercentage(int tIndex, double tPercentage) {
	gPlayerDefinition.mPlayers[tIndex].mStartLifePercentage = tPercentage;
}

double getPlayerLifePercentage(DreamPlayer* p) {
	return p->mLife / (double)p->mHeader->mFiles.mConstants.mHeader.mLife;
}

void setPlayerLife(DreamPlayer* p, DreamPlayer* tLifeGivingPlayer, int tLife)
{
	int delta = tLife - p->mLife;
	addPlayerDamage(p, tLifeGivingPlayer, -delta);
}

void addPlayerLife(DreamPlayer* p, DreamPlayer* tLifeGivingPlayer, int tLife)
{
	addPlayerDamage(p, tLifeGivingPlayer, -tLife);
}

int getPlayerLife(DreamPlayer* p)
{
	return p->mLife;
}

int getPlayerLifeMax(DreamPlayer* p)
{
	return p->mHeader->mFiles.mConstants.mHeader.mLife;
}

int getPlayerPower(DreamPlayer* p)
{
	return p->mPower;
}

int getPlayerPowerMax(DreamPlayer* p)
{
	return p->mHeader->mFiles.mConstants.mHeader.mPower;
}

void setPlayerPower(DreamPlayer* p, int tPower)
{
	p->mPower = max(0, min(getPlayerPowerMax(p), tPower));

	double perc = p->mPower / (double)p->mHeader->mFiles.mConstants.mHeader.mPower;
	setDreamPowerBarPercentage(p, perc, p->mPower);
}

void addPlayerPower(DreamPlayer* p, int tPower)
{
	setPlayerPower(p, p->mPower + tPower);
}

typedef struct {
	int mIsAttacking;

	int mIsInDistance;
	DreamPlayer* p;
} PlayerBeingAttackedSearchCaller;

static void searchPlayerBeingAttackedRecursive(void* tCaller, void* tData) {
	PlayerBeingAttackedSearchCaller* caller = (PlayerBeingAttackedSearchCaller*)tCaller;
	DreamPlayer* p = (DreamPlayer*)tData;
	caller->mIsAttacking |= isHitDataActive(p);

	list_map(&p->mHelpers, searchPlayerBeingAttackedRecursive, caller);
	int_map_map(&p->mProjectiles, searchPlayerBeingAttackedRecursive, caller);
}

int isPlayerBeingAttacked(DreamPlayer* p) {
	DreamPlayer* otherPlayer = getPlayerOtherPlayer(p);
	PlayerBeingAttackedSearchCaller caller;
	caller.mIsAttacking = 0;
	searchPlayerBeingAttackedRecursive(&caller, otherPlayer);
	return caller.mIsAttacking;
}

static void searchPlayerInGuardDistanceRecursive(void* tCaller, void* tData) {
	PlayerBeingAttackedSearchCaller* caller = (PlayerBeingAttackedSearchCaller*)tCaller;
	DreamPlayer* otherPlayer = (DreamPlayer*)tData;

	int isAttacking = isHitDataActive(otherPlayer);
	if (isAttacking) {
		caller->mIsAttacking = 1;
		double dist = fabs(getPlayerAxisDistanceForTwoReferencesX(otherPlayer, caller->p, getPlayerCoordinateP(otherPlayer)));
		caller->mIsInDistance = dist < getHitDataGuardDistance(otherPlayer);
	}

	list_map(&otherPlayer->mHelpers, searchPlayerInGuardDistanceRecursive, caller);
	int_map_map(&otherPlayer->mProjectiles, searchPlayerInGuardDistanceRecursive, caller);
}

int isPlayerInGuardDistance(DreamPlayer* p)
{
	DreamPlayer* otherPlayer = getPlayerOtherPlayer(p);

	PlayerBeingAttackedSearchCaller caller;
	caller.mIsAttacking = 0;
	caller.mIsInDistance = 0;
	caller.p = p;

	searchPlayerInGuardDistanceRecursive(&caller, otherPlayer);

	return caller.mIsAttacking && caller.mIsInDistance;
}

int getDefaultPlayerAttackDistance(DreamPlayer* p, int tCoordinateP)
{
	return transformDreamCoordinatesI(p->mCustomSizeData.mAttackDistance, getPlayerCoordinateP(p), tCoordinateP);
}

Position2D getPlayerHeadPosition(DreamPlayer* p, int tCoordinateP)
{
	return Vector2D(getPlayerHeadPositionX(p, tCoordinateP), getPlayerHeadPositionY(p, tCoordinateP));
}

double getPlayerHeadPositionX(DreamPlayer* p, int tCoordinateP)
{
	return transformDreamCoordinates(p->mCustomSizeData.mHeadPosition.x, getPlayerCoordinateP(p), tCoordinateP);
}

double getPlayerHeadPositionY(DreamPlayer* p, int tCoordinateP)
{
	return transformDreamCoordinates(p->mCustomSizeData.mHeadPosition.y, getPlayerCoordinateP(p), tCoordinateP);
}

void setPlayerHeadPosition(DreamPlayer* p, double tX, double tY, int tCoordinateP)
{
	p->mCustomSizeData.mHeadPosition = transformDreamCoordinatesVector2D(Vector2D(tX, tY), tCoordinateP, getPlayerCoordinateP(p));
}

Position2D getPlayerMiddlePosition(DreamPlayer* p, int tCoordinateP)
{
	return Vector2D(getPlayerMiddlePositionX(p, tCoordinateP), getPlayerMiddlePositionY(p, tCoordinateP));
}

double getPlayerMiddlePositionX(DreamPlayer* p, int tCoordinateP)
{
	return transformDreamCoordinates(p->mCustomSizeData.mMidPosition.x, getPlayerCoordinateP(p), tCoordinateP);
}

double getPlayerMiddlePositionY(DreamPlayer* p, int tCoordinateP)
{
	return transformDreamCoordinates(p->mCustomSizeData.mMidPosition.y, getPlayerCoordinateP(p), tCoordinateP);
}

void setPlayerMiddlePosition(DreamPlayer* p, double tX, double tY, int tCoordinateP)
{
	p->mCustomSizeData.mMidPosition = transformDreamCoordinatesVector2D(Vector2D(tX, tY), tCoordinateP, getPlayerCoordinateP(p));
}

int getPlayerShadowOffset(DreamPlayer* p, int tCoordinateP)
{
	return transformDreamCoordinatesI(p->mCustomSizeData.mShadowOffset, getPlayerCoordinateP(p), tCoordinateP);
}

void setPlayerShadowOffset(DreamPlayer* p, int tOffset, int tCoordinateP)
{
	p->mCustomSizeData.mShadowOffset = transformDreamCoordinatesI(tOffset, tCoordinateP, getPlayerCoordinateP(p));
}

int isPlayerHelper(DreamPlayer* p)
{
	return p->mIsHelper;
}

void setPlayerIsFacingRight(DreamPlayer* p, int tIsFacingRight)
{
	setPlayerFaceDirection(p, tIsFacingRight ? FACE_DIRECTION_RIGHT : FACE_DIRECTION_LEFT);
}

int getPlayerIsFacingRight(DreamPlayer* p)
{
	return p->mFaceDirection == FACE_DIRECTION_RIGHT;
}

void turnPlayerAround(DreamPlayer* p)
{
	setPlayerIsFacingRight(p, !getPlayerIsFacingRight(p));
}

DreamPlayer* getPlayerOtherPlayer(DreamPlayer* p) {
	return p->mOtherPlayer;
}

double getPlayerScaleX(DreamPlayer* p)
{
	return p->mCustomSizeData.mScale.x;
}

void setPlayerScaleX(DreamPlayer* p, double tScaleX)
{
	p->mCustomSizeData.mScale.x = tScaleX;
}

double getPlayerScaleY(DreamPlayer* p)
{
	return p->mCustomSizeData.mScale.y;
}

void setPlayerScaleY(DreamPlayer* p, double tScaleY)
{
	p->mCustomSizeData.mScale.y = tScaleY;
}

double getPlayerToCameraScale(DreamPlayer* p) {
	return getDreamMugenStageHandlerCameraCoordinateP() / double(getPlayerCoordinateP(p));
}

int getPlayerDoesScaleProjectiles(DreamPlayer* p)
{
	return p->mCustomSizeData.mDoesScaleProjectiles;
}

void setPlayerDoesScaleProjectiles(DreamPlayer* p, int tDoesScaleProjectiles)
{
	p->mCustomSizeData.mDoesScaleProjectiles = tDoesScaleProjectiles;
}

DreamPlayer * clonePlayerAsHelper(DreamPlayer* p)
{
	int helperIDInStore = stl_int_map_get_id();
	DreamPlayer* helper = &gPlayerDefinition.mHelperStore[helperIDInStore];
	*helper = *p;
	helper->mHelperIDInStore = helperIDInStore;

	resetHelperState(helper);
	setPlayerExternalDependencies(helper);
	loadPlayerShadow(helper);
	loadPlayerReflection(helper);
	loadPlayerDebug(helper);
	setDreamRegisteredStateToHelperMode(helper->mRegisteredStateMachine);

	helper->mParent = p;
	helper->mIsHelper = 1;
	helper->mHelperIDInParent = list_push_back(&p->mHelpers, helper);
	helper->mHelperIDInRoot = list_push_back(&gPlayerDefinition.mAllPlayers, helper);

	return helper;
}

typedef struct {
	DreamPlayer* mParent;

} MovePlayerHelperCaller;

static int moveSinglePlayerHelper(void* tCaller, void* tData) {
	MovePlayerHelperCaller* caller = (MovePlayerHelperCaller*)tCaller;
	DreamPlayer* helper = (DreamPlayer*)tData;
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
	DreamPlayer* p = (DreamPlayer*)tData;
	removePlayerBindingInternal(p);
}

static void removePlayerBoundHelpers(DreamPlayer* p) {
	list_map(&p->mBoundHelpers, removePlayerBindingCB, NULL);
	delete_list(&p->mBoundHelpers);
}

static void destroyGeneralPlayer(DreamPlayer* p) {
	removeAllExplodsForPlayer(p);
	removePlayerAfterImage(p);
	removeMugenAnimation(p->mAnimationElement);
	removeMugenAnimation(p->mShadow.mAnimationElement);
	removeMugenAnimation(p->mReflection.mAnimationElement);
	removeMugenText(p->mDebug.mCollisionTextID);
	removeFromPhysicsHandler(p->mPhysicsElement);
	p->mIsDestroyed = 1;
}

int destroyPlayer(DreamPlayer* p)
{
	if (!p->mIsHelper) {
		logWarningFormat("Warning: trying to destroy player %d %d who is not a helper. Ignoring.\n", p->mRootID, p->mID);
		return 0;
	}
	if (p->mIsDestroyed) {
		logWarningFormat("Warning: trying to destroy destroyed player %d %d. Ignoring.\n", p->mRootID, p->mID);
		return 0;
	}

	assert(p->mIsHelper);
	assert(p->mParent);
	assert(p->mHelperIDInParent != -1);

	logFormat("destroy %d %d\n", p->mRootID, p->mID);

	list_remove(&gPlayerDefinition.mAllPlayers, p->mHelperIDInRoot);
	removePlayerBoundHelpers(p);
	removePlayerBindingInternal(p);
	movePlayerHelpersToParent(p);
	destroyGeneralPlayer(p);
	return 1;
}

int getPlayerID(DreamPlayer* p)
{
	return p->mID;
}

void setPlayerID(DreamPlayer* p, int tID)
{
	logFormat("%d add helper %d\n", p->mRootID, tID);
	p->mID = tID;
}

void setPlayerHelperControl(DreamPlayer* p, int tCanControl)
{
	if (!tCanControl) {
		setDreamRegisteredStateDisableCommandState(p->mRegisteredStateMachine);
	}
}

static void addProjectileToRoot(DreamPlayer* tPlayer, DreamPlayer* tProjectile) {
	DreamPlayer* root = tPlayer->mRoot;
	tProjectile->mProjectileID = int_map_push_back(&root->mProjectiles, tProjectile);
}

DreamPlayer * createNewProjectileFromPlayer(DreamPlayer* p)
{
	int helperIDInStore = stl_int_map_get_id();
	DreamPlayer* helper = &gPlayerDefinition.mHelperStore[helperIDInStore];
	*helper = *p;
	helper->mHelperIDInStore = helperIDInStore;

	resetHelperState(helper);
	setPlayerExternalDependencies(helper);
	loadPlayerShadow(helper);
	loadPlayerReflection(helper);
	loadPlayerDebug(helper);
	disableDreamRegisteredStateMachine(helper->mRegisteredStateMachine);
	addProjectileToRoot(p, helper);
	addAdditionalProjectileData(helper);
	setPlayerPhysics(helper, MUGEN_STATE_PHYSICS_NONE);
	setPlayerIsFacingRight(helper, getPlayerIsFacingRight(p));
	helper->mParent = p;
	helper->mIsProjectile = 1;
	assert(!stl_set_contains(gPlayerDefinition.mAllProjectiles, helper));
	gPlayerDefinition.mAllProjectiles.insert(helper);

	return helper;
}

static void removeProjectileFromPlayer(DreamPlayer* p) {
	DreamPlayer* root = getPlayerRoot(p);
	int_map_remove(&root->mProjectiles, p->mProjectileID);
}

void removeProjectile(DreamPlayer* p) {
	assert(p->mIsProjectile);
	assert(p->mProjectileID != -1);
	const auto eraseAmount = gPlayerDefinition.mAllProjectiles.erase(p);
	(void)eraseAmount;
	assert(eraseAmount);
	removeAdditionalProjectileData(p);
	removeProjectileFromPlayer(p);
	unloadHelperStateWithoutFreeingOwnedHelpersAndProjectile(p);
	destroyGeneralPlayer(p);
	updatePlayerDestruction(p);
}

int getPlayerControlTime(DreamPlayer* p)
{
	if (getPlayerStateType(p) == MUGEN_STATE_TYPE_AIR) {
		return getActiveHitDataAirGuardControlTime(p);
	}
	else {
		return getActiveHitDataGuardControlTime(p);
	}
}

int getPlayerRecoverTime(DreamPlayer* p)
{
	if (!p->mIsLyingDown) return 0;
	return p->mHeader->mFiles.mConstants.mHeader.mLiedownTime - p->mLyingDownTime;
}

void setPlayerTempScaleActive(DreamPlayer* p, const Vector2D& tScale)
{
	p->mTempScale = tScale;
}

void setPlayerDrawAngleActive(DreamPlayer* p)
{
	p->mIsAngleActive = 1;
}

void addPlayerDrawAngle(DreamPlayer* p, double tAngle)
{
	p->mAngle += tAngle;
}

void multiplyPlayerDrawAngle(DreamPlayer* p, double tFactor)
{
	p->mAngle *= tFactor;
}

void setPlayerDrawAngleValue(DreamPlayer* p, double tAngle)
{
	p->mAngle = tAngle;
}

static int isHelperBoundToPlayer(DreamPlayer* tHelper, DreamPlayer* tTestBind) {
	if (!tHelper->mIsBound) return 0;
	return tHelper->mBoundTarget == tTestBind;
}

static void bindHelperToPlayer(DreamPlayer* tHelper, DreamPlayer* tBind, int tTime, int tFacing, const Vector2D& tOffsetCameraSpace, DreamPlayerBindPositionType tType) {
	removePlayerBindingInternal(tHelper);
	tHelper->mIsBound = 1;
	tHelper->mBoundNow = 0;
	tHelper->mBoundDuration = tTime;
	tHelper->mBoundFaceSet = tFacing;
	tHelper->mBoundOffsetCameraSpace = tOffsetCameraSpace;
	tHelper->mBoundPositionType = tType;
	tHelper->mBoundTarget = tBind;

	if (isHelperBoundToPlayer(tBind, tHelper)) {
		removePlayerBindingInternal(tBind);
	}

	updateBindingPosition(tHelper);

	tHelper->mBoundID = list_push_back(&tBind->mBoundHelpers, tHelper);
}

void bindPlayerToRoot(DreamPlayer* p, int tTime, int tFacing, const Vector2D& tOffset, int tCoordinateP)
{
	bindHelperToPlayer(p, p->mRoot, tTime, tFacing, transformDreamCoordinatesVector2D(tOffset, tCoordinateP, getDreamMugenStageHandlerCameraCoordinateP()), PLAYER_BIND_POSITION_TYPE_AXIS);
}

void bindPlayerToParent(DreamPlayer* p, int tTime, int tFacing, const Vector2D& tOffset, int tCoordinateP)
{
	bindHelperToPlayer(p, p->mParent, tTime, tFacing, transformDreamCoordinatesVector2D(tOffset, tCoordinateP, getDreamMugenStageHandlerCameraCoordinateP()), PLAYER_BIND_POSITION_TYPE_AXIS);
}

typedef struct {
	DreamPlayer* mBindRoot;
	int mID;
	Position mOffset;
	int mTime;
	DreamPlayerBindPositionType mBindPositionType; 
} BindPlayerTargetsCaller;

void bindPlayerToTarget(DreamPlayer* p, int tTime, const Vector2D& tOffset, DreamPlayerBindPositionType tBindPositionType, int tID, int tCoordinateP)
{
	const auto offsetCameraSpace = transformDreamCoordinatesVector2D(tOffset, tCoordinateP, getDreamMugenStageHandlerCameraCoordinateP());
	for (const auto& targetPair : p->mActiveTargets) {
		if (!isPlayer(targetPair.second)) continue;
		if (tID == -1 || tID == targetPair.first) {
			bindHelperToPlayer(p, targetPair.second, tTime + 1, 0, offsetCameraSpace, tBindPositionType);
		}
	}
}

int isPlayerBound(DreamPlayer* p)
{
	return p->mIsBound;
}

void bindPlayerTargetToPlayer(DreamPlayer* p, int tTime, const Vector2D& tOffset, int tID, int tCoordinateP)
{
	const auto offsetCameraSpace = transformDreamCoordinatesVector2D(tOffset, tCoordinateP, getDreamMugenStageHandlerCameraCoordinateP());
	for (const auto& targetPair : p->mActiveTargets) {
		if (!isPlayer(targetPair.second)) continue;
		if (tID == -1 || tID == targetPair.first) {
			bindHelperToPlayer(targetPair.second, p, tTime + 1, 0, offsetCameraSpace, PLAYER_BIND_POSITION_TYPE_AXIS);
		}
	}
}

static void performOnPlayerTargetsWithID(DreamPlayer* p, int tID, const std::function<void(DreamPlayer*)>& tFunc)
{
	for (const auto& targetPair : p->mActiveTargets) {
		if (!isPlayer(targetPair.second)) continue;
		if (tID == -1 || tID == targetPair.first) {
			tFunc(targetPair.second);
		}
	}
}

void addPlayerTargetLife(DreamPlayer* p, DreamPlayer* tLifeGivingPlayer, int tID, int tLife, int tCanKill, int tIsAbsolute)
{
	const std::function<void(DreamPlayer*)> func = [&p, &tLifeGivingPlayer, &tLife, &tCanKill, &tIsAbsolute](DreamPlayer* tPlayer){
		if (!tIsAbsolute) {
			const auto attackMultiplier = getPlayerAttackMultiplier(p);
			const auto defenseMultiplier = getInvertedPlayerDefenseMultiplier(tPlayer);
			if (attackMultiplier != 1.0) tLife = int(tLife * attackMultiplier);
			if (defenseMultiplier != 1.0) tLife = int(tLife * defenseMultiplier);
		}

		int playerLife = getPlayerLife(tPlayer);
		if (!tCanKill && playerLife + tLife <= 0) {
			tLife = -(playerLife - 1);
		}

		addPlayerDamage(tPlayer, tLifeGivingPlayer, -tLife);
	};

	performOnPlayerTargetsWithID(p, tID, func);
}

void addPlayerTargetPower(DreamPlayer* p, int tID, int tPower)
{
	const std::function<void(DreamPlayer*)> func = [&tPower](DreamPlayer* tPlayer) {
		addPlayerPower(tPlayer, tPower);
	};

	performOnPlayerTargetsWithID(p, tID, func);
}

void addPlayerTargetVelocityX(DreamPlayer* p, int tID, double tValue, int tCoordinateP)
{
	const std::function<void(DreamPlayer*)> func = [&tValue, &tCoordinateP](DreamPlayer* tPlayer) {
		addPlayerVelocityX(tPlayer, tValue, tCoordinateP);
	};

	performOnPlayerTargetsWithID(p, tID, func);
}

void addPlayerTargetVelocityY(DreamPlayer* p, int tID, double tValue, int tCoordinateP)
{
	const std::function<void(DreamPlayer*)> func = [&tValue, &tCoordinateP](DreamPlayer* tPlayer) {
		addPlayerVelocityY(tPlayer, tValue, tCoordinateP);
	};

	performOnPlayerTargetsWithID(p, tID, func);
}

void setPlayerTargetVelocityX(DreamPlayer* p, int tID, double tValue, int tCoordinateP)
{
	const std::function<void(DreamPlayer*)> func = [&tValue, &tCoordinateP](DreamPlayer* tPlayer) {
		setPlayerVelocityX(tPlayer, tValue, tCoordinateP);
	};

	performOnPlayerTargetsWithID(p, tID, func);
}

void setPlayerTargetVelocityY(DreamPlayer* p, int tID, double tValue, int tCoordinateP)
{
	const std::function<void(DreamPlayer*)> func = [&tValue, &tCoordinateP](DreamPlayer* tPlayer) {
		setPlayerVelocityY(tPlayer, tValue, tCoordinateP);
	};

	performOnPlayerTargetsWithID(p, tID, func);
}

void setPlayerTargetControl(DreamPlayer* p, int tID, int tControl)
{
	const std::function<void(DreamPlayer*)> func = [&tControl](DreamPlayer* tPlayer) {
		setPlayerControl(tPlayer, tControl);
	};

	performOnPlayerTargetsWithID(p, tID, func);
}

void setPlayerTargetHitOver(DreamPlayer* p, int tID)
{
	const std::function<void(DreamPlayer*)> func = [](DreamPlayer* tPlayer) {
		if (!isPlayerHitShakeOver(tPlayer))
		{
			setPlayerHitShakeOver(tPlayer);
		}
		if (!isPlayerHitOver(tPlayer))
		{
			setPlayerHitOver(tPlayer);
		}
	};

	performOnPlayerTargetsWithID(p, tID, func);
}

void setPlayerTargetFacing(DreamPlayer* p, int tID, int tFacing)
{
	const std::function<void(DreamPlayer*)> func = [&p, &tFacing](DreamPlayer* tPlayer) {
		if (tFacing == 1) {
			setPlayerIsFacingRight(tPlayer, getPlayerIsFacingRight(p));
		}
		else {
			setPlayerIsFacingRight(tPlayer, !getPlayerIsFacingRight(p));
		}
	};

	performOnPlayerTargetsWithID(p, tID, func);
}

void changePlayerTargetState(DreamPlayer* p, int tID, int tNewState)
{
	const std::function<void(DreamPlayer*)> func = [&p, &tNewState](DreamPlayer* tPlayer) {
		changePlayerStateToOtherPlayerStateMachine(tPlayer, p, tNewState);
	};

	performOnPlayerTargetsWithID(p, tID, func);
}

typedef struct {
	int mSearchID;
	DreamPlayer* mPlayer;

} SearchPlayerIDCaller;

static DreamPlayer* searchSinglePlayerForID(DreamPlayer* p, int tID);

static void searchPlayerForIDCB(void* tCaller, void* tData) {
	SearchPlayerIDCaller* caller = (SearchPlayerIDCaller*)tCaller;
	DreamPlayer* p = (DreamPlayer*)tData;

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

int doesPlayerIDExist(DreamPlayer* p, int tID)
{
	return searchSinglePlayerForID(p->mRoot, tID) != NULL;
}

DreamPlayer * getPlayerByIDOrNullIfNonexistant(DreamPlayer* p, int tID)
{
	return searchSinglePlayerForID(p->mRoot, tID);
}

int getPlayerRoundsExisted(DreamPlayer* p)
{
	return p->mRoundsExisted;
}

int getPlayerPaletteNumber(DreamPlayer* p)
{
	return p->mPreferredPalette;
}

void setPlayerScreenBoundForTick(DreamPlayer* p, int tIsBoundToScreen, int tIsCameraFollowingX, int tIsCameraFollowingY)
{
	p->mIsBoundToScreenForTick = tIsBoundToScreen;
	p->mIsCameraFollowing.x = tIsCameraFollowingX;
	p->mIsCameraFollowing.y = tIsCameraFollowingY;
}

void setPlayerScreenBoundForever(DreamPlayer* p, int tIsBoundToScreen)
{
	p->mIsBoundToScreenForever = tIsBoundToScreen;
}

static void resetPlayerHitBySlotGeneral(DreamPlayer* p, int tSlot) {
	p->mNotHitBy[tSlot].mFlag2.clear();
	p->mNotHitBy[tSlot].mNow = 0;
	p->mNotHitBy[tSlot].mIsActive = 1;
}

void resetPlayerHitBy(DreamPlayer* p, int tSlot)
{
	resetPlayerHitBySlotGeneral(p, tSlot);
	p->mNotHitBy[tSlot].mIsHitBy = 1;
}

void resetPlayerNotHitBy(DreamPlayer* p, int tSlot)
{
	resetPlayerHitBySlotGeneral(p, tSlot);
	p->mNotHitBy[tSlot].mIsHitBy = 0;
}

void setPlayerNotHitByFlag1(DreamPlayer* p, int tSlot, const char* tFlag)
{
	p->mNotHitBy[tSlot].mFlag1 = std::string(tFlag);
	turnStringLowercase(p->mNotHitBy[tSlot].mFlag1);
}

void addPlayerNotHitByFlag2(DreamPlayer* p, int tSlot, const char* tFlag)
{
	std::string nFlag = copyOverCleanHitDefAttributeFlag(tFlag);
	if (nFlag.size() != 2) {
		logErrorFormat("Unable to parse nothitby flag %s. Ignoring.", tFlag);
		return;
	}
	turnStringLowercase(nFlag);

	p->mNotHitBy[tSlot].mFlag2.push_back(nFlag);
}

void setPlayerNotHitByTime(DreamPlayer* p, int tSlot, int tTime)
{
	p->mNotHitBy[tSlot].mTime = tTime;
}

int getDefaultPlayerSparkNumberIsInPlayerFile(DreamPlayer* p)
{
	return p->mHeader->mFiles.mConstants.mHeader.mIsSparkNoInPlayerFile;
}

int getDefaultPlayerSparkNumber(DreamPlayer* p)
{
	return p->mHeader->mFiles.mConstants.mHeader.mSparkNo;
}

int getDefaultPlayerGuardSparkNumberIsInPlayerFile(DreamPlayer* p)
{
	return p->mHeader->mFiles.mConstants.mHeader.mIsGuardSparkNoInPlayerFile;
}

int getDefaultPlayerGuardSparkNumber(DreamPlayer* p)
{
	return p->mHeader->mFiles.mConstants.mHeader.mGuardSparkNo;
}

int isPlayerProjectile(DreamPlayer* p)
{
	return p->mIsProjectile;
}

int isPlayerHomeTeam(DreamPlayer* p)
{
	return p->mRootID;
}

void setPlayerDrawOffsetX(DreamPlayer* p, double tValue, int tCoordinateP) {
	tValue = transformDreamCoordinates(tValue, tCoordinateP, getDreamMugenStageHandlerCameraCoordinateP());
	const auto pos = getMugenAnimationPosition(p->mAnimationElement);
	const auto basePos = getDreamStageCoordinateSystemOffset(getDreamMugenStageHandlerCameraCoordinateP());
	auto newPos = basePos + transformDreamCoordinatesVector2D(Vector2D(p->mCustomSizeData.mDrawOffset.x, p->mCustomSizeData.mDrawOffset.y), getPlayerCoordinateP(p), getDreamMugenStageHandlerCameraCoordinateP());

	newPos.x += tValue;
	newPos.y = pos.y;
	p->mDrawOffset.x = tValue;

	setMugenAnimationPosition(p->mAnimationElement, newPos.xyz(pos.z));
}

void setPlayerDrawOffsetY(DreamPlayer* p, double tValue, int tCoordinateP) {
	tValue = transformDreamCoordinates(tValue, tCoordinateP, getDreamMugenStageHandlerCameraCoordinateP());
	const auto pos = getMugenAnimationPosition(p->mAnimationElement);
	const auto basePos = getDreamStageCoordinateSystemOffset(getDreamMugenStageHandlerCameraCoordinateP());
	auto newPos = basePos + transformDreamCoordinatesVector2D(Vector2D(p->mCustomSizeData.mDrawOffset.x, p->mCustomSizeData.mDrawOffset.y), getPlayerCoordinateP(p), getDreamMugenStageHandlerCameraCoordinateP());

	newPos.x = pos.x;
	newPos.y += tValue;
	p->mDrawOffset.y = tValue;

	setMugenAnimationPosition(p->mAnimationElement, newPos.xyz(pos.z));
}

void setPlayerOneFrameTransparency(DreamPlayer* p, BlendType tType, int tAlphaSource, int tAlphaDest)
{
	setMugenAnimationBlendType(p->mAnimationElement, tType);
	setMugenAnimationTransparency(p->mAnimationElement, tAlphaSource / 256.0);
	setMugenAnimationDestinationTransparency(p->mAnimationElement, tAlphaDest / 256.0);
	p->mTransparencyFlag = 1;
}

void setPlayerWidthOneFrame(DreamPlayer* p, const Vector2DI& tEdgeWidth, const Vector2DI& tPlayerWidth, int tCoordinateP)
{
	p->mOneTickStageWidth = transformDreamCoordinatesVector2DI(tEdgeWidth, tCoordinateP, getPlayerCoordinateP(p));
	p->mOneTickPlayerWidth = transformDreamCoordinatesVector2DI(tPlayerWidth, tCoordinateP, getPlayerCoordinateP(p));

	p->mWidthFlag = 1;
}

void addPlayerDust(DreamPlayer* p, int tDustIndex, const Position2D& tPos, int tSpacing, int tCoordinateP)
{
	int time = getDreamGameTime();
	int since = time - p->mDustClouds[tDustIndex].mLastDustTime;
	if (since < tSpacing) return;

	p->mDustClouds[tDustIndex].mLastDustTime = time;
	const auto playerPosition = getPlayerPosition(p, getDreamMugenStageHandlerCameraCoordinateP());
	const auto pos = transformDreamCoordinatesVector2D(tPos, tCoordinateP, getDreamMugenStageHandlerCameraCoordinateP()) + playerPosition;
	addDreamDustCloud(pos.xyz(DUST_Z), getPlayerIsFacingRight(p));
}

VictoryType getPlayerVictoryType(DreamPlayer* p)
{
	DreamPlayer* otherPlayer = getPlayerOtherPlayer(p);

	if (isTimerFinished()) {
		return VICTORY_TYPE_TIMEOVER;
	}
	else if (otherPlayer->mSuicideWinFlag) {
		return VICTORY_TYPE_SUICIDE;
	}
	else if (getActiveHitDataAttackType(otherPlayer) == MUGEN_ATTACK_TYPE_THROW) {
		return VICTORY_TYPE_THROW;
	}
	else if (otherPlayer->mCheeseWinFlag) {
		return VICTORY_TYPE_CHEESE;
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

int isPlayerAtFullLife(DreamPlayer* p)
{
	return getPlayerLife(p) == getPlayerLifeMax(p);
}

int getPlayerVictoryQuoteIndex(DreamPlayer* p) {
	return p->mHeader->mFiles.mConstants.mQuoteData.mVictoryQuoteIndex;
}

void setPlayerVictoryQuoteIndex(DreamPlayer* p, int tIndex)
{
	if(p->mIsHelper) return;
	p->mHeader->mFiles.mConstants.mQuoteData.mVictoryQuoteIndex = tIndex;
}

void setPlayersToTrainingMode()
{
	gPlayerDefinition.mIsInTrainingMode = 1;
}

void setPlayersToRealFightMode()
{
	gPlayerDefinition.mIsInTrainingMode = 0;
}

int isPlayer(DreamPlayer* p)
{
	return list_contains(&gPlayerDefinition.mAllPlayers, p);
}

int isValidPlayerOrProjectile(DreamPlayer* p)
{
	return list_contains(&gPlayerDefinition.mAllPlayers, p) || stl_set_contains(gPlayerDefinition.mAllProjectiles, p);
}

int isGeneralPlayer(DreamPlayer* p)
{
	if (!p) return 0;
	if ((isPlayerHelper(p) || isPlayerProjectile(p)) && !stl_map_contains(gPlayerDefinition.mHelperStore, p->mHelperIDInStore)) return 0;
	return !isPlayerDestroyed(p);
}

int isPlayerTargetValid(DreamPlayer* p) {
	return p && !isPlayerDestroyed(p);
}

int isPlayerCollisionDebugActive() {
	return gPlayerDefinition.mIsCollisionDebugActive;
}

static void setPlayerCollisionDebugRecursiveCB(void* tCaller, void* tData) {
	(void)tCaller;
	DreamPlayer* p = (DreamPlayer*)tData;
	setMugenAnimationCollisionDebug(p->mAnimationElement, gPlayerDefinition.mIsCollisionDebugActive);

	if (!gPlayerDefinition.mIsCollisionDebugActive) {
		char text[3];
		text[0] = '\0';
		changeMugenText(p->mDebug.mCollisionTextID, text);
	}
}

void setPlayerCollisionDebug(int tIsActive) {
	gPlayerDefinition.mIsCollisionDebugActive = tIsActive;
	list_map(&gPlayerDefinition.mAllPlayers, setPlayerCollisionDebugRecursiveCB, NULL);
}

void turnPlayerTowardsOtherPlayer(DreamPlayer* p) {
	DreamPlayer* p2 = getPlayerOtherPlayer(p);

	double x1 = getHandledPhysicsPositionReference(p->mPhysicsElement)->x;
	double x2 = getHandledPhysicsPositionReference(p2->mPhysicsElement)->x;

	if (x1 > x2) setPlayerFaceDirection(p, FACE_DIRECTION_LEFT);
	else if (x1 < x2) setPlayerFaceDirection(p, FACE_DIRECTION_RIGHT);
}

int isPlayerInputAllowed(DreamPlayer* p)
{
	return (getGameMode() != GAME_MODE_OSU) || isOsuPlayerCommandInputAllowed(p->mRootID);
}

int getPlayerTimeDilationUpdates(DreamPlayer* p)
{
	return p->mTimeDilatationUpdates;
}

double getPlayerSpeed(DreamPlayer* p)
{
	return p->mTimeDilatation;
}

typedef struct {
	double mSpeed;
} SetPlayerSpeedCaller;

static void setSinglePlayerSpeed(DreamPlayer* p, double tSpeed) {
	p->mTimeDilatation = tSpeed;
	setMugenAnimationSpeed(p->mAnimationElement, tSpeed);
	setMugenAnimationSpeed(p->mShadow.mAnimationElement, tSpeed);
	setMugenAnimationSpeed(p->mReflection.mAnimationElement, tSpeed);
	setHandledPhysicsSpeed(p->mPhysicsElement, tSpeed);
	setDreamHandledStateMachineSpeed(p->mRegisteredStateMachine, tSpeed);
}

static void setSinglePlayerSpeedCB(SetPlayerSpeedCaller* tCaller, DreamPlayer* tData) {
	setSinglePlayerSpeed(tData, tCaller->mSpeed);
}

static void setSinglePlayerSpeedCBOld(void* tCaller, void* tData) {
	SetPlayerSpeedCaller* caller = (SetPlayerSpeedCaller*)tCaller;
	DreamPlayer* player = (DreamPlayer*)tData;
	setSinglePlayerSpeedCB(caller, player);
}

void setPlayersSpeed(double tSpeed)
{
	SetPlayerSpeedCaller caller;
	caller.mSpeed = tSpeed;
	list_map(&gPlayerDefinition.mAllPlayers, setSinglePlayerSpeedCBOld, &caller);
	stl_set_map(gPlayerDefinition.mAllProjectiles, setSinglePlayerSpeedCB, &caller);
}

typedef struct {
	double mMultiplier;
} SetPlayerSuperDefenseMultiplierCaller;

static void setSinglePlayerSuperDefenseMultiplierRecursive(DreamPlayer* tPlayer, double tMultiplier);

static void setSinglePlayerSuperDefenseMultiplierRecursiveCB(void* tCaller, void* tData) {
	auto caller = (SetPlayerSuperDefenseMultiplierCaller*)tCaller;
	setSinglePlayerSuperDefenseMultiplierRecursive((DreamPlayer*)tData, caller->mMultiplier);
}

static void setSinglePlayerSuperDefenseMultiplierRecursive(DreamPlayer* tPlayer, double tMultiplier) {
	if (!isGeneralPlayer(tPlayer)) return;

	if (!isPlayerHitOver(tPlayer)) {
		setPlayerSuperDefenseMultiplier(tPlayer, tMultiplier);
	}

	SetPlayerSuperDefenseMultiplierCaller caller;
	caller.mMultiplier = tMultiplier;
	list_map(&tPlayer->mHelpers, setSinglePlayerSuperDefenseMultiplierRecursiveCB, &caller);
}

void setPlayerTargetsSuperDefenseMultiplier(DreamPlayer* tPlayer, double tMultiplier)
{
	auto rootPlayer = getPlayerRoot(getPlayerOtherPlayer(tPlayer));
	setSinglePlayerSuperDefenseMultiplierRecursive(rootPlayer, tMultiplier);
}

Vector3DI getIsCameraFollowingPlayer(DreamPlayer* p)
{
	return p->mIsCameraFollowing;
}

int parsePlayerSoundEffectChannel(int tChannel, DreamPlayer* tPlayer) {
	if (tChannel == -1) return -1;

	static const auto CHANNEL_AMOUNT = 64;
	static const auto STEREO_CHANNEL_FACTOR = 2;
	static const auto CHANNEL_AMOUNT_PER_PLAYER = (CHANNEL_AMOUNT / STEREO_CHANNEL_FACTOR) / 2;
	return (CHANNEL_AMOUNT_PER_PLAYER * tPlayer->mRootID + tChannel) * STEREO_CHANNEL_FACTOR;
}

double getPlayerVolumeModifier(DreamPlayer* tPlayer)
{
	return clamp((tPlayer->mHeader->mFiles.mConstants.mHeader.mVolume + 100) / double(100), 0.0, 1.0);
}

double getPlayerMidiVolumeForPrism(DreamPlayer* tPlayer)
{
	return parseGameMidiVolumeToPrism(getGameMidiVolume()) * getPlayerVolumeModifier(tPlayer);
}
