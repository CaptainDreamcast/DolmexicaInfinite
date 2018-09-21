#pragma once

#include <prism/actorhandler.h>


extern ActorBlueprint OsuHandler;

int isOsuHandlerActive();
int shouldPlayOsuMusicInTheBeginning();
void startPlayingOsuSong();
void resetOsuHandler();
void setOsuFile(char* tPath);
void stopOsuHandler();