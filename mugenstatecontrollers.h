#pragma once

#include <prism/mugendefreader.h>

#include "mugenassignment.h"
#include "playerdefinition.h"

DreamMugenStateController* parseDreamMugenStateControllerFromGroup(MugenDefScriptGroup* tGroup);
int handleDreamMugenStateControllerAndReturnWhetherStateChanged(DreamMugenStateController* tController, DreamPlayer* tPlayer);