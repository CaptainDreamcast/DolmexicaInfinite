#include "fightdeterminism.h"

#ifndef FIGHT_DETERMINISM_DISABLED

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include <prism/math.h>
#include <prism/log.h>

#include "playerdefinition.h"
#include "gamelogic.h"
#include "mugenstagehandler.h"

static uint32_t hashDataStep(uint32_t tHash, const void* tData, size_t tSize) {
	const uint8_t* bytes = (const uint8_t*)tData;
	for (size_t i = 0; i < tSize; i++) {
		tHash ^= bytes[i];
		tHash *= 16777619u; // FNV-1a
	}
	return tHash;
}

static uint32_t hashIntStep(uint32_t tHash, int tValue) {
	return hashDataStep(tHash, &tValue, sizeof(tValue));
}

static uint32_t hashDoubleStep(uint32_t tHash, float tValue) {
	return hashDataStep(tHash, &tValue, sizeof(tValue));
}

static uint32_t hashSinglePlayerStep(uint32_t h, DreamPlayer* p) {
	h = hashIntStep(h, p->mRootID);
	h = hashIntStep(h, p->mIsHelper);
	h = hashIntStep(h, p->mIsProjectile);

	const int coordP = getPlayerCoordinateP(p);
	h = hashIntStep(h, getPlayerState(p));
	h = hashIntStep(h, getPlayerTimeInState(p));
	h = hashIntStep(h, getPlayerAnimationNumber(p));
	h = hashDoubleStep(h, getPlayerPositionX(p, coordP));
	h = hashDoubleStep(h, getPlayerPositionY(p, coordP));
	h = hashDoubleStep(h, getPlayerVelocityX(p, coordP));
	h = hashDoubleStep(h, getPlayerVelocityY(p, coordP));
	h = hashIntStep(h, getPlayerLife(p));
	h = hashIntStep(h, getPlayerPower(p));

	h = hashIntStep(h, (int)p->mFaceDirection);
	h = hashIntStep(h, p->mIsInControl);
	h = hashIntStep(h, p->mIsAlive);
	h = hashIntStep(h, (int)p->mStateType);
	h = hashIntStep(h, (int)p->mMoveType);
	h = hashIntStep(h, (int)p->mStatePhysics);
	h = hashIntStep(h, p->mIsHitPaused);
	h = hashIntStep(h, p->mHitPauseNow);
	h = hashIntStep(h, p->mMoveContactCounter);
	h = hashIntStep(h, p->mMoveHit);
	h = hashIntStep(h, p->mMoveGuarded);
	h = hashIntStep(h, p->mIsFalling);
	h = hashIntStep(h, p->mIsLyingDown);
	h = hashIntStep(h, p->mComboCounter);

	h = hashIntStep(h, getPlayerPreviousState(p));
	h = hashIntStep(h, p->mLyingDownTime);
	h = hashIntStep(h, p->mRecoverTime);
	h = hashIntStep(h, p->mRecoverTimeSinceHitPause);
	h = hashIntStep(h, p->mCanRecoverFromFall);
	h = hashIntStep(h, p->mIsHitOver);
	h = hashIntStep(h, p->mIsHitOverridden);
	h = hashIntStep(h, p->mHitCount);
	h = hashIntStep(h, p->mFallAmountInCombo);
	h = hashIntStep(h, p->mAirJumpCounter);
	h = hashIntStep(h, p->mJumpFlank);
	h = hashIntStep(h, p->mSuperMoveTime);
	h = hashIntStep(h, p->mPauseMoveTime);
	h = hashIntStep(h, p->mAirJugglePoints);
	h = hashIntStep(h, p->mIsBeingJuggled);
	h = hashIntStep(h, p->mIsGuardingInternally);
	h = hashIntStep(h, p->mIsHitShakeActive);
	h = hashIntStep(h, p->mHitShakeNow);
	h = hashIntStep(h, p->mHitShakeDuration);
	h = hashIntStep(h, p->mIsHitOverWaitActive);
	h = hashIntStep(h, p->mHitOverNow);
	h = hashIntStep(h, p->mHitOverDuration);
	h = hashIntStep(h, p->mIsBound);
	h = hashIntStep(h, p->mBoundNow);
	h = hashIntStep(h, p->mHasLastContactProjectile);
	h = hashIntStep(h, p->mPassiveHitData.mIsActive);
	h = hashIntStep(h, p->mActiveHitData.mIsActive);
	for (int slot = 0; slot < 2; slot++) {
		h = hashIntStep(h, p->mNotHitBy[slot].mIsActive);
		h = hashIntStep(h, p->mNotHitBy[slot].mNow);
		h = hashIntStep(h, p->mNotHitBy[slot].mTime);
		h = hashIntStep(h, p->mNotHitBy[slot].mIsHitBy);
	}
	h = hashDoubleStep(h, p->mDefenseMultiplier);
	h = hashDoubleStep(h, p->mSuperDefenseMultiplier);
	h = hashDoubleStep(h, p->mAttackMultiplier);
	h = hashIntStep(h, p->mIsFrozen);
	h = hashIntStep(h, p->mMoveReversed);
	h = hashIntStep(h, p->mLastHitGuarded);
	h = hashIntStep(h, (int)p->mActiveTargets.size());
	h = hashIntStep(h, (int)p->mReceivedHitData.size());
	h = hashIntStep(h, list_size(&p->mHelpers));
	h = hashIntStep(h, int_map_size(&p->mProjectiles));
	h = hashIntStep(h, p->mRoundsExisted);
	h = hashIntStep(h, p->mRoundsWon);
	h = hashDoubleStep(h, p->mTimeDilatationNow);
	h = hashIntStep(h, p->mTimeDilatationUpdates);
	h = hashDoubleStep(h, p->mTimeDilatation);

	h = hashDataStep(h, p->mVars, sizeof(p->mVars));
	h = hashDataStep(h, p->mSystemVars, sizeof(p->mSystemVars));
	h = hashDataStep(h, p->mFloatVars, sizeof(p->mFloatVars));
	h = hashDataStep(h, p->mSystemFloatVars, sizeof(p->mSystemFloatVars));
	return h;
}

uint32_t getFightStateChecksum() {
	uint32_t h = 2166136261u;

	const int totalPlayerAmount = getTotalPlayerAmount();
	h = hashIntStep(h, totalPlayerAmount);
	for (int i = 0; i < totalPlayerAmount; i++) {
		DreamPlayer* p = getPlayerByIndex(i);
		if (!p) {
			h = hashIntStep(h, -1);
			continue;
		}
		if (isPlayerDestroyed(p)) {
			h = hashIntStep(h, -2);
			continue;
		}
		h = hashSinglePlayerStep(h, p);
	}

	h = hashIntStep(h, getDreamGameTime());
	h = hashIntStep(h, getDreamRoundNumber());
	h = hashIntStep(h, getDreamRoundStateNumber());

	const Position* cameraPosition = getDreamMugenStageHandlerCameraPositionReference();
	h = hashDoubleStep(h, cameraPosition->x);
	h = hashDoubleStep(h, cameraPosition->y);

	const unsigned int randomState = getRandomState();
	h = hashDataStep(h, &randomState, sizeof(randomState));

	return h;
}

// Debugging aid for divergence hunting: prints every checksum component.
static void dumpFightStateChecksumComponents(const char* tTag, int tFrame) {
	printf("[checksum dump %s frame %d]\n", tTag, tFrame);
	const int totalPlayerAmount = getTotalPlayerAmount();
	printf("  totalPlayers=%d gameTime=%d round=%d roundState=%d\n",
		totalPlayerAmount, getDreamGameTime(), getDreamRoundNumber(), getDreamRoundStateNumber());
	const Position* cam = getDreamMugenStageHandlerCameraPositionReference();
	printf("  camera=%.17g/%.17g rng=%08x rngCalls=%llu\n", (double)cam->x, (double)cam->y, getRandomState(), getRandomCallAmount());
	for (int i = 0; i < totalPlayerAmount; i++) {
		DreamPlayer* p = getPlayerByIndex(i);
		if (!p || isPlayerDestroyed(p)) {
			printf("  player %d: %s\n", i, p ? "destroyed" : "null");
			continue;
		}
		const int coordP = getPlayerCoordinateP(p);
		printf("  player %d: root=%d helper=%d proj=%d state=%d stime=%d anim=%d pos=%.17g/%.17g vel=%.17g/%.17g life=%d pow=%d face=%d ctrl=%d alive=%d stype=%d mtype=%d phys=%d hitpause=%d/%d mcc=%d mh=%d mg=%d fall=%d lie=%d combo=%d\n",
			i, p->mRootID, p->mIsHelper, p->mIsProjectile,
			getPlayerState(p), getPlayerTimeInState(p), getPlayerAnimationNumber(p),
			(double)getPlayerPositionX(p, coordP), (double)getPlayerPositionY(p, coordP),
			(double)getPlayerVelocityX(p, coordP), (double)getPlayerVelocityY(p, coordP),
			getPlayerLife(p), getPlayerPower(p),
			(int)p->mFaceDirection, p->mIsInControl, p->mIsAlive,
			(int)p->mStateType, (int)p->mMoveType, (int)p->mStatePhysics,
			p->mIsHitPaused, p->mHitPauseNow, p->mMoveContactCounter,
			p->mMoveHit, p->mMoveGuarded, p->mIsFalling, p->mIsLyingDown, p->mComboCounter);
		uint32_t varHash = hashDataStep(hashDataStep(2166136261u, p->mVars, sizeof(p->mVars)), p->mSystemVars, sizeof(p->mSystemVars));
		uint32_t floatVarHash = hashDataStep(hashDataStep(2166136261u, p->mFloatVars, sizeof(p->mFloatVars)), p->mSystemFloatVars, sizeof(p->mSystemFloatVars));
		printf("    varHash=%08x floatVarHash=%08x\n", varHash, floatVarHash);
		printf("    x2: prev=%d lieT=%d recT=%d recTHP=%d canRec=%d hitOver=%d hitCnt=%d fallCombo=%d airJmp=%d jmpFlank=%d superMT=%d pauseMT=%d juggleP=%d juggled=%d guardInt=%d shake=%d/%d/%d overWait=%d/%d/%d bound=%d/%d lastProj=%d pasAct=%d actAct=%d nhb0=%d/%d/%d/%d nhb1=%d/%d/%d/%d defM=%.17g sdefM=%.17g atkM=%.17g frozen=%d movRev=%d lastGuard=%d targets=%d recvHit=%d helpers=%d projs=%d rndsEx=%d rndsWon=%d tdil=%.17g/%d/%.17g\n",
			getPlayerPreviousState(p), p->mLyingDownTime, p->mRecoverTime, p->mRecoverTimeSinceHitPause, p->mCanRecoverFromFall,
			p->mIsHitOver, p->mHitCount, p->mFallAmountInCombo, p->mAirJumpCounter, p->mJumpFlank,
			p->mSuperMoveTime, p->mPauseMoveTime, p->mAirJugglePoints, p->mIsBeingJuggled, p->mIsGuardingInternally,
			p->mIsHitShakeActive, p->mHitShakeNow, p->mHitShakeDuration,
			p->mIsHitOverWaitActive, p->mHitOverNow, p->mHitOverDuration,
			p->mIsBound, p->mBoundNow, p->mHasLastContactProjectile,
			p->mPassiveHitData.mIsActive, p->mActiveHitData.mIsActive,
			p->mNotHitBy[0].mIsActive, p->mNotHitBy[0].mNow, p->mNotHitBy[0].mTime, p->mNotHitBy[0].mIsHitBy,
			p->mNotHitBy[1].mIsActive, p->mNotHitBy[1].mNow, p->mNotHitBy[1].mTime, p->mNotHitBy[1].mIsHitBy,
			(double)p->mDefenseMultiplier, (double)p->mSuperDefenseMultiplier, (double)p->mAttackMultiplier,
			p->mIsFrozen, p->mMoveReversed, p->mLastHitGuarded,
			(int)p->mActiveTargets.size(), (int)p->mReceivedHitData.size(), list_size(&p->mHelpers), int_map_size(&p->mProjectiles),
			p->mRoundsExisted, p->mRoundsWon, (double)p->mTimeDilatationNow, p->mTimeDilatationUpdates, (double)p->mTimeDilatation);
	}
	fflush(stdout);
}

static int isFightDeterminismTraceActive() {
	return getenv("DOLMEXICA_TRACE_CHECKSUM") != NULL;
}

// dumps checksum components at DOLMEXICA_DUMP_FRAMES frames during both recording and playback for two-pass divergence hunting
static int isFightDeterminismDumpFrame(int tFrame) {
	const char* dumpEnv = getenv("DOLMEXICA_DUMP_FRAMES");
	if (!dumpEnv) return 0;
	for (const char* c = dumpEnv; *c;) {
		if (atoi(c) == tFrame) return 1;
		const char* comma = strchr(c, ',');
		if (!comma) break;
		c = comma + 1;
	}
	return 0;
}

#define FIGHT_REPLAY_MAGIC "DXR1"
#define FIGHT_REPLAY_VERSION 1

typedef struct {
	char mMagic[4];
	uint32_t mVersion;
	uint32_t mRandomState;
	uint32_t mFrameAmount;
	char mPlayerDefinitionPaths[2][1024];
} FightReplayHeader;

typedef struct {
	uint32_t mHeldMasks[2];
	uint32_t mChecksum;
} FightReplayFrame;

static struct {
	int mIsRecording;
	int mHasRecordingCapturedStart;
	char mRecordingPath[1024];
	FightReplayHeader mRecordingHeader;
	std::vector<FightReplayFrame> mRecordedFrames;

	int mIsPlaybackActive;
	int mIsPlaybackOver;
	int mHasPlaybackAppliedStart;
	FightReplayHeader mPlaybackHeader;
	std::vector<FightReplayFrame> mPlaybackFrames;
	uint32_t mPlaybackFrame;
	int mChecksumMismatchAmount;
	int mFirstChecksumMismatchFrame;
} gFightDeterminismData;

int startFightInputRecording(const char* tPath) {
	if (gFightDeterminismData.mIsRecording) return 0;

	memset(&gFightDeterminismData.mRecordingHeader, 0, sizeof(FightReplayHeader));
	memcpy(gFightDeterminismData.mRecordingHeader.mMagic, FIGHT_REPLAY_MAGIC, 4);
	gFightDeterminismData.mRecordingHeader.mVersion = FIGHT_REPLAY_VERSION;
	getPlayerDefinitionPath(gFightDeterminismData.mRecordingHeader.mPlayerDefinitionPaths[0], 0);
	getPlayerDefinitionPath(gFightDeterminismData.mRecordingHeader.mPlayerDefinitionPaths[1], 1);

	strncpy(gFightDeterminismData.mRecordingPath, tPath, sizeof(gFightDeterminismData.mRecordingPath) - 1);
	gFightDeterminismData.mRecordingPath[sizeof(gFightDeterminismData.mRecordingPath) - 1] = '\0';
	gFightDeterminismData.mRecordedFrames.clear();
	gFightDeterminismData.mHasRecordingCapturedStart = 0;
	gFightDeterminismData.mIsRecording = 1;
	return 1;
}

void stopFightInputRecording() {
	if (!gFightDeterminismData.mIsRecording) return;
	gFightDeterminismData.mIsRecording = 0;

	FILE* file = fopen(gFightDeterminismData.mRecordingPath, "wb");
	if (!file) {
		logErrorFormat("Unable to open fight replay file for writing: %s", gFightDeterminismData.mRecordingPath);
		return;
	}
	gFightDeterminismData.mRecordingHeader.mFrameAmount = (uint32_t)gFightDeterminismData.mRecordedFrames.size();
	fwrite(&gFightDeterminismData.mRecordingHeader, sizeof(FightReplayHeader), 1, file);
	if (!gFightDeterminismData.mRecordedFrames.empty()) {
		fwrite(gFightDeterminismData.mRecordedFrames.data(), sizeof(FightReplayFrame), gFightDeterminismData.mRecordedFrames.size(), file);
	}
	fclose(file);
	gFightDeterminismData.mRecordedFrames.clear();
}

int isFightInputRecordingActive() {
	return gFightDeterminismData.mIsRecording;
}

int startFightInputPlayback(const char* tPath) {
	if (gFightDeterminismData.mIsPlaybackActive) return 0;

	FILE* file = fopen(tPath, "rb");
	if (!file) {
		logErrorFormat("Unable to open fight replay file for reading: %s", tPath);
		return 0;
	}
	if (fread(&gFightDeterminismData.mPlaybackHeader, sizeof(FightReplayHeader), 1, file) != 1
		|| memcmp(gFightDeterminismData.mPlaybackHeader.mMagic, FIGHT_REPLAY_MAGIC, 4)
		|| gFightDeterminismData.mPlaybackHeader.mVersion != FIGHT_REPLAY_VERSION) {
		logErrorFormat("Invalid fight replay file: %s", tPath);
		fclose(file);
		return 0;
	}
	gFightDeterminismData.mPlaybackFrames.resize(gFightDeterminismData.mPlaybackHeader.mFrameAmount);
	if (gFightDeterminismData.mPlaybackHeader.mFrameAmount) {
		if (fread(gFightDeterminismData.mPlaybackFrames.data(), sizeof(FightReplayFrame), gFightDeterminismData.mPlaybackHeader.mFrameAmount, file) != gFightDeterminismData.mPlaybackHeader.mFrameAmount) {
			logErrorFormat("Truncated fight replay file: %s", tPath);
			fclose(file);
			gFightDeterminismData.mPlaybackFrames.clear();
			return 0;
		}
	}
	fclose(file);

	gFightDeterminismData.mPlaybackFrame = 0;
	gFightDeterminismData.mChecksumMismatchAmount = 0;
	gFightDeterminismData.mFirstChecksumMismatchFrame = -1;
	gFightDeterminismData.mHasPlaybackAppliedStart = 0;
	gFightDeterminismData.mIsPlaybackOver = 0;
	gFightDeterminismData.mIsPlaybackActive = 1;
	return 1;
}

void stopFightInputPlayback() {
	gFightDeterminismData.mIsPlaybackActive = 0;
	gFightDeterminismData.mPlaybackFrames.clear();
}

int isFightInputPlaybackActive() {
	return gFightDeterminismData.mIsPlaybackActive;
}

int isFightInputPlaybackOver() {
	return gFightDeterminismData.mIsPlaybackOver;
}

int getFightInputPlaybackFrame() {
	return (int)gFightDeterminismData.mPlaybackFrame;
}

int getFightInputPlaybackFrameAmount() {
	return (int)gFightDeterminismData.mPlaybackFrames.size();
}

int getFightInputPlaybackChecksumMismatchAmount() {
	return gFightDeterminismData.mChecksumMismatchAmount;
}

int getFightInputPlaybackFirstChecksumMismatchFrame() {
	return gFightDeterminismData.mFirstChecksumMismatchFrame;
}

void getFightInputPlaybackPlayerDefinitionPath(char* tDst, int i) {
	strcpy(tDst, gFightDeterminismData.mPlaybackHeader.mPlayerDefinitionPaths[i]);
}

static void updateFightInputRecording(const uint32_t* tHeldMasks) {
	if (!gFightDeterminismData.mHasRecordingCapturedStart) {
		gFightDeterminismData.mRecordingHeader.mRandomState = getRandomState();
		gFightDeterminismData.mHasRecordingCapturedStart = 1;
	}
	FightReplayFrame frame;
	frame.mHeldMasks[0] = tHeldMasks[0];
	frame.mHeldMasks[1] = tHeldMasks[1];
	frame.mChecksum = getFightStateChecksum();
	if ((isFightDeterminismTraceActive() && gFightDeterminismData.mRecordedFrames.size() < 2)
		|| isFightDeterminismDumpFrame((int)gFightDeterminismData.mRecordedFrames.size())) {
		dumpFightStateChecksumComponents("record", (int)gFightDeterminismData.mRecordedFrames.size());
	}
	gFightDeterminismData.mRecordedFrames.push_back(frame);
}

static void updateFightInputPlayback(uint32_t* tHeldMasks) {
	if (!gFightDeterminismData.mHasPlaybackAppliedStart) {
		setRandomState(gFightDeterminismData.mPlaybackHeader.mRandomState);
		gFightDeterminismData.mHasPlaybackAppliedStart = 1;
	}
	if (gFightDeterminismData.mPlaybackFrame >= gFightDeterminismData.mPlaybackFrames.size()) {
		gFightDeterminismData.mIsPlaybackOver = 1;
		return;
	}

	const FightReplayFrame& frame = gFightDeterminismData.mPlaybackFrames[gFightDeterminismData.mPlaybackFrame];
	const uint32_t checksum = getFightStateChecksum();
	if (isFightDeterminismDumpFrame((int)gFightDeterminismData.mPlaybackFrame)) {
		dumpFightStateChecksumComponents("playback", (int)gFightDeterminismData.mPlaybackFrame);
	}
	if (checksum != frame.mChecksum) {
		gFightDeterminismData.mChecksumMismatchAmount++;
		if (gFightDeterminismData.mFirstChecksumMismatchFrame < 0) {
			gFightDeterminismData.mFirstChecksumMismatchFrame = (int)gFightDeterminismData.mPlaybackFrame;
			printf("fight replay DIVERGENCE at frame %d: checksum %08x, expected %08x\n",
				(int)gFightDeterminismData.mPlaybackFrame, checksum, frame.mChecksum);
			fflush(stdout);
			if (isFightDeterminismTraceActive()) {
				dumpFightStateChecksumComponents("playback-mismatch", (int)gFightDeterminismData.mPlaybackFrame);
			}
		}
	}
	else if (isFightDeterminismTraceActive() && gFightDeterminismData.mPlaybackFrame < 2) {
		dumpFightStateChecksumComponents("playback", (int)gFightDeterminismData.mPlaybackFrame);
	}
	tHeldMasks[0] = frame.mHeldMasks[0];
	tHeldMasks[1] = frame.mHeldMasks[1];
	gFightDeterminismData.mPlaybackFrame++;
}

void updateFightDeterminismInputHook(uint32_t* tHeldMasks) {
	if (gFightDeterminismData.mIsRecording) {
		updateFightInputRecording(tHeldMasks);
	}
	if (gFightDeterminismData.mIsPlaybackActive && !gFightDeterminismData.mIsPlaybackOver) {
		updateFightInputPlayback(tHeldMasks);
	}
}

#endif // FIGHT_DETERMINISM_DISABLED
