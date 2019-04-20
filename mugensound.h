#pragma once 

typedef struct {
	int mGroup;
	int mItem;
} DreamMugenSound;

DreamMugenSound makeDreamMugenSound(int tGroup, int tItem);

int isMugenBGMMusicPath(const char* tPath);
int isMugenBGMMusicPath(const char* tPath, const char* tStagePath);
void playMugenBGMMusicPath(const char* tPath, int tIsLooping);
void playMugenBGMMusicPath(const char* tPath, const char* tStagePath, int tIsLooping);