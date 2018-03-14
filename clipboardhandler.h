#pragma once

#include <prism/actorhandler.h>


extern ActorBlueprint ClipboardHandler;

void initClipboardForGame();

void addClipboardLine(char* tLine);
void addClipboardLineFormatString(char* tFormatString, char* tParameterString); // expects parameter list in "arg1 , arg2 , arg3" format