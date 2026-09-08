#pragma once

#include <string>
#include <vector>

/* Per-frame entity anomaly checks shared by EntityCensusTest and
 * MediaCaptureTest. Every distinct anomaly is recorded once, keyed by what
 * went wrong and on which entity, so a permanently broken entity does not
 * flood the run with one anomaly per frame. */

struct CensusAnomaly {
	int mFrame;
	int mRound;
	std::string mText;
};

void resetCensus();
int updateCensusWithCurrentFrame(int tFrame, int tRound);

struct CensusHelperStateDwell {
	int mHelperID;
	int mState;
	int mFrames;
};

const std::vector<CensusAnomaly>& getCensusAnomalies();
std::vector<CensusHelperStateDwell> getCensusHelperStateDwellsOfAtLeast(int tFrames);
int getCensusAnomalyAmount();
int getCensusMaximumEntityAmount();
int getCensusRootGetHitEntryAmount(int tSide);
