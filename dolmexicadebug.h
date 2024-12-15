#pragma once

#include <prism/actorhandler.h>

using namespace prism;

void initDolmexicaDebug();

ActorBlueprint getDolmexicaDebug();

int isDebugOverridingTimeDilatation();
void addDebugDolmexicaStoryCharacterAnimation(const char* tCharacter, int tAnimation);