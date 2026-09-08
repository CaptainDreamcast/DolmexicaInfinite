#pragma once

#include <set>
#include <string>

#include <prism/actorhandler.h>

using namespace prism;

void initDolmexicaDebug();

ActorBlueprint getDolmexicaDebug();

int isDebugOverridingTimeDilatation();
void addDebugDolmexicaStoryCharacterAnimation(const char* tCharacter, int tAnimation);
const std::set<int>& getDebugDolmexicaStoryCharacterAnimations(const char* tCharacter);