#pragma once

#include <prism/mugendefreader.h>
#include <prism/mugenanimationhandler.h>

void loadScriptBackground(MugenDefScript* tScript, MugenSpriteFile* tSprites, MugenAnimations* tAnimations, const char* tDefinitionGroupName, const char* tBackgroundGroupName, const Vector2DI& tLocalCoordinates = Vector2DI(320, 240), int tDoesInstantiateMugenStageHandler = 1);
