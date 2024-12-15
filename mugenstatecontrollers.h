#pragma once

#include <prism/mugendefreader.h>
#include <prism/memorystack.h>

#include "mugenassignment.h"
#include "playerdefinition.h"

using namespace prism;

DreamMugenStateController* parseDreamMugenStateControllerFromGroup(MugenDefScriptGroup* tGroup);
void unloadDreamMugenStateController(DreamMugenStateController* tController);
int handleDreamMugenStateControllerAndReturnWhetherStateChanged(DreamMugenStateController* tController, DreamPlayer* tPlayer);

void setupDreamMugenStateControllerHandler(MemoryStack* tMemoryStack);
void setupDreamMugenStoryStateControllerHandler();
void shutdownDreamMugenStateControllerHandler();

#ifdef _WIN32
void imguiMugenStateController(int tIndex, void* tData, const std::string_view& tScriptPath, const std::string_view& tGroupName);
#endif