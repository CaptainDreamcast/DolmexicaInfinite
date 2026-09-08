#include "censuscommon.h"

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <map>
#include <set>

#include <prism/datastructures.h>

#include "playerdefinition.h"
#include "stage.h"

#define CENSUS_ANOMALY_TEXT_SIZE 512

static const auto CENSUS_GETHIT_STATE_FIRST = 5000;
static const auto CENSUS_GETHIT_STATE_LAST = 5999;
static const auto CENSUS_GETHIT_ANIMATION_FIRST = 5000;
static const auto CENSUS_GETHIT_ANIMATION_LAST = 5299;
static const auto CENSUS_ROOT_STUCK_FRAME_LIMIT = 900;
static const auto CENSUS_ENTITY_RUNAWAY_AMOUNT = 60;
static const auto CENSUS_ENTITY_RUNAWAY_FRAME_LIMIT = 60 * 5;
static const auto CENSUS_ABSURD_POSITION = 100000.0f;
static const auto CENSUS_HIGH_POSITION_SCREEN_HEIGHT_AMOUNT = 2.0f;
static const auto CENSUS_HIGH_POSITION_FRAME_LIMIT = 300;
static const auto CENSUS_POSITION_COORDINATE_P = 320;

struct CensusRootStuckTracker {
	int mState = -1;
	int mFrames = 0;
};

static struct {
	int mFrame;
	int mRound;

	std::vector<CensusAnomaly> mAnomalies;
	std::set<std::string> mSeenAnomalyKeys;

	std::map<int, std::map<int, int>> mHelperStateDwellFrames;

	int mMaximumEntityAmount;
	int mEntityRunawayFrames;
	int mRootGetHitEntries[2];
	int mRootHighFrames[2];
	int mRootWasGetHit[2];
	CensusRootStuckTracker mRootStuck[2];
} gCensusData;

static int isCensusGetHitState(int tState) {
	return tState >= CENSUS_GETHIT_STATE_FIRST && tState <= CENSUS_GETHIT_STATE_LAST;
}

static int isCensusGetHitAnimation(int tAnimation) {
	return tAnimation >= CENSUS_GETHIT_ANIMATION_FIRST && tAnimation <= CENSUS_GETHIT_ANIMATION_LAST;
}

static int isCensusEntityShowingGetHit(DreamPlayer* tEntity) {
	return isCensusGetHitState(getPlayerState(tEntity)) || isCensusGetHitAnimation(getPlayerAnimationNumber(tEntity));
}

static void addCensusAnomaly(const std::string& tKey, const char* tFormat, ...) {
	if (gCensusData.mSeenAnomalyKeys.count(tKey)) return;
	gCensusData.mSeenAnomalyKeys.insert(tKey);

	char text[CENSUS_ANOMALY_TEXT_SIZE];
	va_list args;
	va_start(args, tFormat);
	vsnprintf(text, sizeof(text), tFormat, args);
	va_end(args);

	CensusAnomaly anomaly;
	anomaly.mFrame = gCensusData.mFrame;
	anomaly.mRound = gCensusData.mRound;
	anomaly.mText = text;
	gCensusData.mAnomalies.push_back(anomaly);
}

static std::string makeCensusAnomalyKey(const char* tKind, int tIdentifier) {
	return std::string(tKind) + ":" + std::to_string(tIdentifier);
}

static int isPositionNumericallySane(float tValue) {
	if (std::isnan(tValue) || std::isinf(tValue)) return 0;
	return std::fabs(tValue) <= CENSUS_ABSURD_POSITION;
}

static void checkCensusEntityPosition(DreamPlayer* tEntity) {
	const auto x = getPlayerPositionX(tEntity, CENSUS_POSITION_COORDINATE_P);
	const auto y = getPlayerPositionY(tEntity, CENSUS_POSITION_COORDINATE_P);
	if (isPositionNumericallySane(x) && isPositionNumericallySane(y)) return;

	addCensusAnomaly(makeCensusAnomalyKey("position", tEntity->mID),
		"entity(root=%d id=%d) at nonsense position %f/%f", tEntity->mRootID, tEntity->mID, (double)x, (double)y);
}

static void checkCensusProjectileCB(void* tCaller, void* tData) {
	(void)tCaller;
	DreamPlayer* projectile = (DreamPlayer*)tData;
	if (!projectile || !isPlayer(projectile)) return;
	if (!isCensusEntityShowingGetHit(projectile)) return;

	addCensusAnomaly(makeCensusAnomalyKey("projectile_gethit", projectile->mID),
		"projectile(root=%d id=%d) in gethit state %d anim %d",
		projectile->mRootID, projectile->mID, getPlayerState(projectile), getPlayerAnimationNumber(projectile));
}

static void trackCensusHelperStateDwell(DreamPlayer* tHelper) {
	gCensusData.mHelperStateDwellFrames[tHelper->mID][getPlayerState(tHelper)]++;
}

static void checkCensusHelper(DreamPlayer* tHelper) {
	trackCensusHelperStateDwell(tHelper);
	if (!isCensusEntityShowingGetHit(tHelper)) return;

	addCensusAnomaly(makeCensusAnomalyKey("helper_gethit", tHelper->mID),
		"helper(root=%d id=%d) in gethit state %d anim %d at %.1f/%.1f",
		tHelper->mRootID, tHelper->mID, getPlayerState(tHelper), getPlayerAnimationNumber(tHelper),
		(double)getPlayerPositionX(tHelper, CENSUS_POSITION_COORDINATE_P),
		(double)getPlayerPositionY(tHelper, CENSUS_POSITION_COORDINATE_P));
}

static void checkCensusRootStuckInGetHit(DreamPlayer* tRoot) {
	const auto side = tRoot->mRootID;
	if (side < 0 || side > 1) return;

	const auto state = getPlayerState(tRoot);
	auto& tracker = gCensusData.mRootStuck[side];
	if (!isCensusGetHitState(state) || tracker.mState != state) {
		tracker.mState = isCensusGetHitState(state) ? state : -1;
		tracker.mFrames = 1;
		return;
	}

	tracker.mFrames++;
	if (tracker.mFrames < CENSUS_ROOT_STUCK_FRAME_LIMIT) return;

	addCensusAnomaly(makeCensusAnomalyKey("root_stuck", side),
		"root %d stuck in gethit state %d anim %d for %d frames",
		side, state, getPlayerAnimationNumber(tRoot), CENSUS_ROOT_STUCK_FRAME_LIMIT);
}

static float getCensusHighPositionLimit() {
	return -CENSUS_HIGH_POSITION_SCREEN_HEIGHT_AMOUNT * getDreamGameHeight(CENSUS_POSITION_COORDINATE_P);
}

static int hasCensusRootStayedHighLongEnough(DreamPlayer* tRoot, int tSide) {
	if (getPlayerPositionY(tRoot, CENSUS_POSITION_COORDINATE_P) > getCensusHighPositionLimit()) {
		gCensusData.mRootHighFrames[tSide] = 0;
		return 0;
	}

	gCensusData.mRootHighFrames[tSide]++;
	return gCensusData.mRootHighFrames[tSide] >= CENSUS_HIGH_POSITION_FRAME_LIMIT;
}

// Only roots are checked here, because helpers are routinely parked far off-screen as invisible logic entities.
static void checkCensusRootStuckHigh(DreamPlayer* tRoot) {
	const auto side = tRoot->mRootID;
	if (side < 0 || side > 1) return;
	if (!hasCensusRootStayedHighLongEnough(tRoot, side)) return;

	addCensusAnomaly(makeCensusAnomalyKey("root_high", side),
		"root %d stayed more than %.0f above the ground for %d frames, at %.1f/%.1f in state %d anim %d",
		side, (double)-getCensusHighPositionLimit(), CENSUS_HIGH_POSITION_FRAME_LIMIT,
		(double)getPlayerPositionX(tRoot, CENSUS_POSITION_COORDINATE_P),
		(double)getPlayerPositionY(tRoot, CENSUS_POSITION_COORDINATE_P),
		getPlayerState(tRoot), getPlayerAnimationNumber(tRoot));
}

static void checkCensusEntity(DreamPlayer* tEntity) {
	checkCensusEntityPosition(tEntity);
	if (isPlayerHelper(tEntity)) checkCensusHelper(tEntity);
	else if (!isPlayerProjectile(tEntity)) {
		checkCensusRootStuckInGetHit(tEntity);
		checkCensusRootStuckHigh(tEntity);
	}
	int_map_map(&tEntity->mProjectiles, checkCensusProjectileCB, NULL);
}

static void checkCensusEntities() {
	const auto total = getTotalPlayerAmount();
	for (int i = 0; i < total; i++) {
		DreamPlayer* entity = getPlayerByIndex(i);
		if (!entity) continue;
		checkCensusEntity(entity);
	}
}

static void updateCensusRootGetHitEntries() {
	for (int side = 0; side < 2; side++) {
		DreamPlayer* root = getRootPlayer(side);
		if (!root) continue;
		const auto isGetHit = isCensusGetHitState(getPlayerState(root));
		if (isGetHit && !gCensusData.mRootWasGetHit[side]) gCensusData.mRootGetHitEntries[side]++;
		gCensusData.mRootWasGetHit[side] = isGetHit;
	}
}

static int hasEntityAmountStayedRunawayLongEnough(int tTotal) {
	if (tTotal <= CENSUS_ENTITY_RUNAWAY_AMOUNT) {
		gCensusData.mEntityRunawayFrames = 0;
		return 0;
	}
	gCensusData.mEntityRunawayFrames++;
	return gCensusData.mEntityRunawayFrames >= CENSUS_ENTITY_RUNAWAY_FRAME_LIMIT;
}

static void updateCensusEntityAmount() {
	const auto total = getTotalPlayerAmount();
	if (total > gCensusData.mMaximumEntityAmount) gCensusData.mMaximumEntityAmount = total;
	if (!hasEntityAmountStayedRunawayLongEnough(total)) return;

	addCensusAnomaly("entity_runaway", "entity count stayed above %d for %d frames, currently %d", CENSUS_ENTITY_RUNAWAY_AMOUNT, gCensusData.mEntityRunawayFrames, total);
}

void resetCensus() {
	gCensusData.mFrame = 0;
	gCensusData.mRound = 0;
	gCensusData.mAnomalies.clear();
	gCensusData.mSeenAnomalyKeys.clear();
	gCensusData.mHelperStateDwellFrames.clear();
	gCensusData.mMaximumEntityAmount = 0;
	gCensusData.mEntityRunawayFrames = 0;
	for (int side = 0; side < 2; side++) {
		gCensusData.mRootGetHitEntries[side] = 0;
		gCensusData.mRootWasGetHit[side] = 0;
		gCensusData.mRootHighFrames[side] = 0;
		gCensusData.mRootStuck[side] = CensusRootStuckTracker();
	}
}

int updateCensusWithCurrentFrame(int tFrame, int tRound) {
	const auto anomalyAmountBefore = (int)gCensusData.mAnomalies.size();
	gCensusData.mFrame = tFrame;
	gCensusData.mRound = tRound;

	updateCensusRootGetHitEntries();
	updateCensusEntityAmount();
	checkCensusEntities();

	return (int)gCensusData.mAnomalies.size() - anomalyAmountBefore;
}

const std::vector<CensusAnomaly>& getCensusAnomalies() {
	return gCensusData.mAnomalies;
}

std::vector<CensusHelperStateDwell> getCensusHelperStateDwellsOfAtLeast(int tFrames) {
	std::vector<CensusHelperStateDwell> ret;
	for (const auto& helperEntry : gCensusData.mHelperStateDwellFrames) {
		for (const auto& stateEntry : helperEntry.second) {
			if (stateEntry.second < tFrames) continue;
			ret.push_back(CensusHelperStateDwell{ helperEntry.first, stateEntry.first, stateEntry.second });
		}
	}
	return ret;
}

int getCensusAnomalyAmount() {
	return (int)gCensusData.mAnomalies.size();
}

int getCensusMaximumEntityAmount() {
	return gCensusData.mMaximumEntityAmount;
}

int getCensusRootGetHitEntryAmount(int tSide) {
	return gCensusData.mRootGetHitEntries[tSide];
}
