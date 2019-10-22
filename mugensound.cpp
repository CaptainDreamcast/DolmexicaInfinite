#include "mugensound.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <prism/sound.h>
#include <prism/file.h>
#include <prism/stlutil.h>

#include "osuhandler.h"
#include "gamelogic.h"

using namespace std;

static struct {
	int mIsPlayingMusic;
	int mIsPausedFlag;
} gDolmexicaMugenSoundData;

static void loadDolmexicaSoundHandler(void*) {
	gDolmexicaMugenSoundData.mIsPlayingMusic = 0;
	gDolmexicaMugenSoundData.mIsPausedFlag = 0;
}

static void updatePauseFlag() {
	if (!gDolmexicaMugenSoundData.mIsPausedFlag) return;

	if (gDolmexicaMugenSoundData.mIsPausedFlag == 2) {
		resumeMusic();
		gDolmexicaMugenSoundData.mIsPausedFlag = 0;
	}
	else {
		gDolmexicaMugenSoundData.mIsPausedFlag++;
	}
}

static void updateDolmexicaSoundHandler(void*) {
	if (!gDolmexicaMugenSoundData.mIsPlayingMusic) return;
	updatePauseFlag();
}

ActorBlueprint getDolmexicaSoundHandler()
{
	return makeActorBlueprint(loadDolmexicaSoundHandler, NULL, updateDolmexicaSoundHandler);
}

DreamMugenSound makeDreamMugenSound(int tGroup, int tItem)
{
	DreamMugenSound ret;
	ret.mGroup = tGroup;
	ret.mItem = tItem;
	return ret;
}

void setNoMusicFlag()
{
	pauseMusic();
	gDolmexicaMugenSoundData.mIsPausedFlag = 1;
}

int isMugenBGMMusicPath(const char * tPath)
{
	return isMugenBGMMusicPath(tPath, "");
}

int isMugenBGMMusicPath(const char* tPath, const char* tStagePath) {
	if (!strchr(tPath, '.')) return 0;
	const char* fileExtension = getFileExtension(tPath);
	if (!strcmp("da", fileExtension)) return 1;
	if (getGameMode() == GAME_MODE_OSU && !strcmp("osu", fileExtension)) return 1;

	char inFolderPath[1024];
	sprintf(inFolderPath, "assets/music/%s", tPath);
	if (isFile(inFolderPath)) return 1;

	if (*tStagePath) {
		string s;
		const char* folderEnd = strrchr(tStagePath, '/');
		if (folderEnd) {
			s = string(tStagePath, size_t(folderEnd - tStagePath + 1));
		}
		sprintf(inFolderPath, "%s%s", s.data(), tPath);
		if (isFile(inFolderPath)) return 1;
	}

	return isFile(tPath);
}

void playMugenBGMMusicPath(const char * tPath, int tIsLooping)
{
	playMugenBGMMusicPath(tPath, "", tIsLooping);
}

static void playMugenBGMTrack(const char* tPath, int tIsLooping) {
	char modPath[1024];
	strcpy(modPath, tPath);
	*strrchr(modPath, '.') = '\0';
	int trackNumber = atoi(modPath);
	if (tIsLooping) {
		playTrack(trackNumber);
	}
	else {
		playTrackOnce(trackNumber);
	}
	gDolmexicaMugenSoundData.mIsPlayingMusic = 1;
}

static void playMugenBGMMusicCompletePath(const char* tPath, int tIsLooping) {
	if (tIsLooping) {
		streamMusicFile(tPath);
	}
	else {
		streamMusicFileOnce(tPath);
	}
	gDolmexicaMugenSoundData.mIsPlayingMusic = 1;
}


void playMugenBGMMusicPath(const char* tPath, const char* tStagePath, int tIsLooping) {
	const char* fileExtension = getFileExtension(tPath);
	if (!strcmp("osu", fileExtension)) {
		setOsuFile(tPath);
		return;
	}

	if (!strcmp("da", fileExtension)) {
		playMugenBGMTrack(tPath, tIsLooping);
		return;
	}

	char inFolderPath[1024];
	sprintf(inFolderPath, "assets/music/%s", tPath); 
	if (isFile(inFolderPath)) {
		playMugenBGMMusicCompletePath(inFolderPath, tIsLooping);
		return;
	}

	if (*tStagePath) {
		string s;
		const char* folderEnd = strrchr(tStagePath, '/');
		if (folderEnd) {
			s = string(tStagePath, size_t(folderEnd - tStagePath + 1));
		}
		sprintf(inFolderPath, "%s%s", s.data(), tPath);
		if (isFile(inFolderPath)) {
			playMugenBGMMusicCompletePath(inFolderPath, tIsLooping);
			return;
		}
	}

	playMugenBGMMusicCompletePath(tPath, tIsLooping);
}