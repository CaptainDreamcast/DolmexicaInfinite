#pragma once

#include <prism/actorhandler.h>


ActorBlueprint getOsuHandler();

int isOsuHandlerActive();
int shouldPlayOsuMusicInTheBeginning();
void startPlayingOsuSong();
void resetOsuHandler();
void setOsuFile(const char* tPath);
void stopOsuHandler();