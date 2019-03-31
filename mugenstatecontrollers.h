#pragma once

#include <prism/mugendefreader.h>
#include <prism/memorystack.h>

#include "mugenassignment.h"
#include "playerdefinition.h"


DreamMugenStateController* parseDreamMugenStateControllerFromGroup(MugenDefScriptGroup* tGroup);
void unloadDreamMugenStateController(DreamMugenStateController* tController);
int handleDreamMugenStateControllerAndReturnWhetherStateChanged(DreamMugenStateController* tController, DreamPlayer* tPlayer);

void setupDreamMugenStateControllerHandler(MemoryStack* tMemoryStack);
void setupDreamMugenStoryStateControllerHandler();
void shutdownDreamMugenStateControllerHandler();