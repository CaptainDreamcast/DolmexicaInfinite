#include "mugensound.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <prism/sound.h>
#include <prism/file.h>

DreamMugenSound makeDreamMugenSound(int tGroup, int tItem)
{
	DreamMugenSound ret;
	ret.mGroup = tGroup;
	ret.mItem = tItem;
	return ret;
}


int isMugenBGMMusicPath(char* tPath) {
	if (!strchr(tPath, '.')) return 0;
	char* fileExtension = getFileExtension(tPath);
	if (!strcmp("da", fileExtension)) return 1;

	char inFolderPath[1024];
	sprintf(inFolderPath, "assets/music/%s", tPath);
	if (isFile(inFolderPath)) return 1;

	return isFile(tPath);
}

static void playMugenBGMTrack(char* tPath, int tIsLooping) {
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
}

static void playMugenBGMMusicCompletePath(char* tPath, int tIsLooping) {
	if (tIsLooping) {
		streamMusicFile(tPath);
	}
	else {
		streamMusicFileOnce(tPath);
	}
}


void playMugenBGMMusicPath(char* tPath, int tIsLooping) {
	char* fileExtension = getFileExtension(tPath);
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

	playMugenBGMMusicCompletePath(tPath, tIsLooping);
}