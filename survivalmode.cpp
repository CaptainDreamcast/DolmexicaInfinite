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
	int len = int(strlen(gSurvivalModeData.mResultMessageFormat));
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
	setCharacterRandomAndReturnIfSuccessful(&script, 1);
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
	int isEnabled = getMugenDefIntegerOrDefault(tScript, "survival results screen", "enabled", 0);
	char* message = getAllocatedMugenDefStringOrDefault(tScript, "survival results screen", "winstext.text", "Congratulations!");
	strcpy(gSurvivalModeData.mResultMessageFormat, message);
	Vector3DI font = getMugenDefVectorIOrDefault(tScript, "survival results screen", "winstext.font", Vector3DI(-1, 0, 0));
	Position2D offset = getMugenDefVector2DOrDefault(tScript, "survival results screen", "winstext.offset", Vector2D(0, 0));
	int displayTime = getMugenDefIntegerOrDefault(tScript, "survival results screen", "winstext.displaytime", -1);
	int layerNo = getMugenDefIntegerOrDefault(tScript, "survival results screen", "winstext.layerno", 2);
	int fadeInTime = getMugenDefIntegerOrDefault(tScript, "survival results screen", "fadein.time", 30);
	int poseTime = getMugenDefIntegerOrDefault(tScript, "survival results screen", "pose.time", 300);
	int fadeOutTime = getMugenDefIntegerOrDefault(tScript, "survival results screen", "fadeout.time", 30);
	gSurvivalModeData.mRoundsToWin = getMugenDefIntegerOrDefault(tScript, "survival results screen", "roundstowin", 5);

	setFightResultData(isEnabled, message, font, offset, displayTime, layerNo, fadeInTime, poseTime, fadeOutTime, 1);

	freeMemory(message);
}

static void loadSurvivalModeHeaderFromScript() {
	char scriptPath[1000];

	strcpy(scriptPath, (getDolmexicaAssetFolder() + getMotifPath()).c_str());

	MugenDefScript script; 
	loadMugenDefScript(&script, scriptPath);

	loadResultScreenFromScript(&script);

	unloadMugenDefScript(&script);
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
