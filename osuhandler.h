#pragma once

#include <prism/actorhandler.h>
#include <prism/mugendefreader.h>

ActorBlueprint getOsuHandler();

int isOsuHandlerActive();
int shouldPlayOsuMusicInTheBeginning();
void startPlayingOsuSong();
void resetOsuHandler();
void setOsuFile(const char* tPath);
void stopOsuHandler();

void loadOsuParametersFromScript(MugenDefScript* tScript, const char* tFightPath);