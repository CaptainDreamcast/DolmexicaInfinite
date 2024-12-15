#pragma once

#include <prism/actorhandler.h>
#include <prism/geometry.h>

using namespace prism;

namespace prism {
	struct MugenAnimationHandlerElement;
}

ActorBlueprint getMugenAnimationUtilityHandler();

void setMugenAnimationInvisibleForOneFrame(MugenAnimationHandlerElement* tElement);
void setMugenTextInvisibleForOneFrame(int tID);

void setMugenAnimationPaletteEffectForDuration(MugenAnimationHandlerElement* tElement, int tDuration, const Vector3D& tAddition, const Vector3D& tMultiplier, const Vector3D& tSineAmplitude, int tSinePeriod, int tInvertAll, double tColorFactor);
void removeMugenAnimationPaletteEffectIfExists(MugenAnimationHandlerElement* tElement);