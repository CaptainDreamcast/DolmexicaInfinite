#include "survivalmode.h"

#include <assert.h>

#include <prism/mugendefreader.h>
#include <prism/math.h>
#include <prism/log.h>

#include "characterselectscreen.h"
#include "titlescreen.h"
#include "storyscreen.h"
#include "playerdefinition.h"
#include "versusscreen.h"
#include "fightscreen.h"
#include "gamelogic.h"
#include "stage.h"
#include "fightui.h"
#include "fightresultdisplay.h"
#include "config.h"

static struct {
	int mCurrentEnemy;

	char mResultMessageFormat[100];
	int mRoundsToWin;

	double mLifePercentage;
} gSurvivalModeData;

static void updateSurvivalResultMessage() {
	char message[100];


	int srcI;
	char* dst = message;
	int len = strlen(gSurvivalModeData.mResultMessageFormat);
	for (srcI = 0; srcI < len; srcI++) {
		if (gSurvivalModeData.mResultMessageFormat[srcI] == '%' && gSurvivalModeData.mResultMessageFormat[srcI + 1] == 'i') {
			sprintf(dst, "%d", gSurvivalModeData.mCurrentEnemy);
			dst += strlen(dst);
			srcI++;
		}
		else {
			*dst = gSurvivalModeData.mResultMessageFormat[srcI];
			dst++;
		}

	}
	*dst = '\0';

	setFightResultMessage(message);
}

static void updateSurvivalResultIsShowingWinPose() {
	setFightResultIsShowingWinPose(gSurvivalModeData.mCurrentEnemy >= gSurvivalModeData.mRoundsToWin);
}

static void updateSurvivalEnemy() {
	MugenDefScript script;
	loadMugenDefScript(&script, getDolmexicaAssetFolder() + "data/select.def");
	setCharacterRandom(&script, 1);
}


static void fightFinishedCB() {
	gSurvivalModeData.mCurrentEnemy++;
	if (gSurvivalModeData.mCurrentEnemy) {
		gSurvivalModeData.mLifePercentage = getPlayerLifePercentage(getRootPlayer(0));
	}

	updateSurvivalEnemy();
	updateSurvivalResultMessage();
	updateSurvivalResultIsShowingWinPose();
	setGameModeSurvival(gSurvivalModeData.mLifePercentage, gSurvivalModeData.mCurrentEnemy+1);
	setPlayerArtificial(1, calculateAIRampDifficulty(gSurvivalModeData.mCurrentEnemy, getSurvivalAIRampStart(), getSurvivalAIRampEnd()));
	startFightScreen(fightFinishedCB);
}

static void loadResultScreenFromScript(MugenDefScript* tScript) {
	int isEnabled = getMugenDefIntegerOrDefault(tScript, "Survival Results Screen", "enabled", 0);
	char* message = getAllocatedMugenDefStringOrDefault(tScript, "Survival Results Screen", "winstext.text", "Congratulations!");
	strcpy(gSurvivalModeData.mResultMessageFormat, message);
	Vector3DI font = getMugenDefVectorIOrDefault(tScript, "Survival Results Screen", "winstext.font", makeVector3DI(2, 0, 0));
	Position offset = getMugenDefVectorOrDefault(tScript, "Survival Results Screen", "winstext.offset", makePosition(0, 0, 0));
	int displayTime = getMugenDefIntegerOrDefault(tScript, "Survival Results Screen", "winstext.displaytime", -1);
	int layerNo = getMugenDefIntegerOrDefault(tScript, "Survival Results Screen", "winstext.layerno", 2);
	int fadeInTime = getMugenDefIntegerOrDefault(tScript, "Survival Results Screen", "fadein.time", 30);
	int poseTime = getMugenDefIntegerOrDefault(tScript, "Survival Results Screen", "pose.time", 300);
	int fadeOutTime = getMugenDefIntegerOrDefault(tScript, "Survival Results Screen", "fadeout.time", 30);
	gSurvivalModeData.mRoundsToWin = getMugenDefIntegerOrDefault(tScript, "Survival Results Screen", "roundstowin", 5);

	setFightResultData(isEnabled, message, font, offset, displayTime, layerNo, fadeInTime, poseTime, fadeOutTime, 1);

	freeMemory(message);
}

static void loadSurvivalModeHeaderFromScript() {
	char scriptPath[1000];

	strcpy(scriptPath, (getDolmexicaAssetFolder() + getMotifPath()).c_str());

	MugenDefScript script; 
	loadMugenDefScript(&script, scriptPath);

	loadResultScreenFromScript(&script);

	unloadMugenDefScript(script);
}

void startSurvivalMode()
{
	gSurvivalModeData.mCurrentEnemy = -1;
	gSurvivalModeData.mLifePercentage = 1;
	setPlayerStartLifePercentage(0, 1);

	loadSurvivalModeHeaderFromScript();
	setCharacterSelectScreenModeName("Survival");
	setCharacterSelectOnePlayer();
	setCharacterSelectStageActive();
	setCharacterSelectFinishedCB(fightFinishedCB);
	setNewScreen(getCharacterSelectScreen());
}

int getSurvivalModeMatchNumber()
{
	return gSurvivalModeData.mCurrentEnemy + 1;
}
