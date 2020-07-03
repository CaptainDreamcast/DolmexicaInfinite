#include "scriptbackground.h"

#include <assert.h>

#include <prism/log.h>
#include <prism/system.h>
#include <prism/math.h>
#include <prism/screeneffect.h>

#include "mugenstagehandler.h"
#include "stage.h"

static int isScriptBackgroundGroup(MugenDefScriptGroup* tGroup, const char* tBackgroundGroupName) {
	if (tGroup == NULL) return 0;

	char firstW[100];
	int items = sscanf(tGroup->mName.data(), "%s", firstW);
	return items == 1 && !strcmp(tBackgroundGroupName, firstW);
}

void loadScriptBackground(MugenDefScript * tScript, MugenSpriteFile * tSprites, MugenAnimations * tAnimations, const char * tDefinitionGroupName, const char * tBackgroundGroupName, const Vector2DI& tLocalCoordinates, int tDoesInstantiateMugenStageHandler)
{
	if (tDoesInstantiateMugenStageHandler) {
		instantiateActor(getDreamMugenStageHandler());
	}
	setDreamStageCoordinates(tLocalCoordinates);
	setDreamMugenStageHandlerCameraCoordinates(tLocalCoordinates);
	setDreamMugenStageHandlerCameraRange(GeoRectangle2D(0, 0, tLocalCoordinates.x, tLocalCoordinates.y));
	setDreamMugenStageHandlerCameraPosition(Vector2D(0, 0));

	assert(isStringLowercase(tDefinitionGroupName));
	assert(isStringLowercase(tBackgroundGroupName));
	MugenDefScriptGroup* current = &tScript->mGroups[tDefinitionGroupName];
	const auto clearColor = getMugenDefVectorIOrDefaultAsGroup(current, "bgclearcolor", Vector3DI(0, 0, 0));
	setScreenBackgroundColorRGB(clearColor.x / 255.0, clearColor.y / 255.0, clearColor.z / 255.0);

	current = current->mNext;

	int i = 1;
	while (current) {
		if (isScriptBackgroundGroup(current, tBackgroundGroupName)) {
			loadBackgroundElementGroup(current, i, tSprites, tAnimations, tLocalCoordinates, Vector2D(1.0, 1.0));
			i++;
		}
		current = current->mNext;
	}
}
