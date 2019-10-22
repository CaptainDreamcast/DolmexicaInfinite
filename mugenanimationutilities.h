#pragma once

#include <prism/actorhandler.h>

struct MugenAnimationHandlerElement;

ActorBlueprint getMugenAnimationUtilityHandler();

void setMugenAnimationInvisibleForOneFrame(MugenAnimationHandlerElement* tElement);
void setMugenTextInvisibleForOneFrame(int tID);