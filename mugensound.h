#pragma once 

typedef struct {
	int mGroup;
	int mItem;
} DreamMugenSound;

DreamMugenSound makeDreamMugenSound(int tGroup, int tItem);

int isMugenBGMMusicPath(char* tPath);
void playMugenBGMMusicPath(char* tPath, int tIsLooping);