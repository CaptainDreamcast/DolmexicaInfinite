#pragma once 

#include <prism/actorhandler.h>

typedef struct {
	int mGroup;
	int mItem;
} DreamMugenSound;

ActorBlueprint getDolmexicaSoundHandler();

DreamMugenSound makeDreamMugenSound(int tGroup, int tItem);

void setNoMusicFlag();
int isMugenBGMMusicPath(const char* tPath);
int isMugenBGMMusicPath(const char* tPath, const char* tRootFilePath);
void playMugenBGMMusicPath(const char* tPath, int tIsLooping);
void playMugenBGMMusicPath(const char* tPath, const char* tRootFilePath, int tIsLooping);