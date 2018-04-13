#include "menubackground.h"

#include <assert.h>

#include <prism/log.h>
#include <prism/system.h>
#include <prism/math.h>

#include "mugenstagehandler.h"
#include "stage.h"

static struct {
	Vector3DI mLocalCoordinates;
} gData;

static int isMenuBackgroundGroup(MugenDefScriptGroup* tGroup, char* tBackgroundGroupName) {
	if (tGroup == NULL) return 0;

	char firstW[100];
	int items = sscanf(tGroup->mName, "%s", firstW);
	assert(items == 1);

	return !strcmp(tBackgroundGroupName, firstW);
}

static BlendType getBackgroundBlendType(MugenDefScriptGroup* tGroup) {
	if (!isMugenDefStringVariableAsGroup(tGroup, "trans")) return BLEND_TYPE_NORMAL;

	BlendType ret;
	char* text = getAllocatedMugenDefStringVariableAsGroup(tGroup, "trans");
	turnStringLowercase(text);

	if (!strcmp("add", text) || !strcmp("addalpha", text)) {
		ret = BLEND_TYPE_ADDITION;
	} else if (!strcmp("sub", text)) {
		ret = BLEND_TYPE_SUBTRACTION;
	}
	else if (!strcmp("none", text)) {
		ret = BLEND_TYPE_NORMAL;
	}
	else {
		ret = BLEND_TYPE_NORMAL;
		logError("Unknown transparency type.");
		logErrorString(text);
		abortSystem();
	}

	freeMemory(text);

	return ret;
}

static void loadNormalMenuBackgroundGroup(MugenDefScriptGroup* tGroup, int i, MugenSpriteFile* tSprites, MugenAnimations* tAnimations) {
	Vector3DI spriteNo = getMugenDefVectorIOrDefaultAsGroup(tGroup, "spriteno", makeVector3DI(0, 0, 0));
	int layerNo = getMugenDefIntegerOrDefaultAsGroup(tGroup, "layerno", 0);
	Vector3D start = getMugenDefVectorOrDefaultAsGroup(tGroup, "start", makePosition(0, 0, 0));
	start.z = i;
	Vector3D delta = getMugenDefVectorOrDefaultAsGroup(tGroup, "delta", makePosition(1, 1, 0));
	int mask = getMugenDefIntegerOrDefaultAsGroup(tGroup, "mask", 0);
	Vector3DI tile = getMugenDefVectorIOrDefaultAsGroup(tGroup, "tile", makeVector3DI(0, 0, 0));
	Vector3DI tileSpacing = getMugenDefVectorIOrDefaultAsGroup(tGroup, "tilespacing", makeVector3DI(0, 0, 0));
	BlendType blendType = getBackgroundBlendType(tGroup);
	GeoRectangle constraintRectangle = getMugenDefGeoRectangleOrDefaultAsGroup(tGroup, "window", makeGeoRectangle(-INF / 2, -INF / 2, INF, INF));
	Vector3D velocity = getMugenDefVectorOrDefaultAsGroup(tGroup, "velocity", makePosition(0, 0, 0));

	(void)mask; // TODO
	(void)layerNo; // TODO
	addDreamMugenStageHandlerAnimatedBackgroundElement(start, createOneFrameMugenAnimationForSprite(spriteNo.x, spriteNo.y), tSprites, delta, tile, tileSpacing, blendType, constraintRectangle, velocity, 1, 0, 0, gData.mLocalCoordinates);
}

static void loadAnimatedMenuBackgroundGroup(MugenDefScriptGroup* tGroup, int i, MugenSpriteFile* tSprites, MugenAnimations* tAnimations) {
	int animation = getMugenDefIntegerOrDefaultAsGroup(tGroup, "actionno", -1);
	int layerNo = getMugenDefIntegerOrDefaultAsGroup(tGroup, "layerno", 0);
	Vector3D start = getMugenDefVectorOrDefaultAsGroup(tGroup, "start", makePosition(0, 0, 0));
	start.z = i+layerNo*60;
	Vector3D delta = getMugenDefVectorOrDefaultAsGroup(tGroup, "delta", makePosition(1, 1, 0));
	int mask = getMugenDefIntegerOrDefaultAsGroup(tGroup, "mask", 0);
	Vector3DI tile = getMugenDefVectorIOrDefaultAsGroup(tGroup, "tile", makeVector3DI(0, 0, 0));
	Vector3DI tileSpacing = getMugenDefVectorIOrDefaultAsGroup(tGroup, "tilespacing", makeVector3DI(0, 0, 0));
	BlendType blendType = getBackgroundBlendType(tGroup);
	GeoRectangle constraintRectangle = getMugenDefGeoRectangleOrDefaultAsGroup(tGroup, "window", makeGeoRectangle(-INF / 2, -INF / 2, INF, INF));
	Vector3D velocity = getMugenDefVectorOrDefaultAsGroup(tGroup, "velocity", makePosition(0, 0, 0));

	(void)mask; // TODO
	addDreamMugenStageHandlerAnimatedBackgroundElement(start, getMugenAnimation(tAnimations, animation), tSprites, delta, tile, tileSpacing, blendType, constraintRectangle, velocity, 1, 0, 0, gData.mLocalCoordinates);
}

static void loadMenuBackgroundGroup(MugenDefScriptGroup* tGroup, int i, MugenSpriteFile* tSprites, MugenAnimations* tAnimations) {

	char* type = getAllocatedMugenDefStringVariableAsGroup(tGroup, "type");
	if (!strcmp("normal", type) || !strcmp("parallax", type)) {
		loadNormalMenuBackgroundGroup(tGroup, i, tSprites, tAnimations);
	} else if (!strcmp("anim", type)) {
		loadAnimatedMenuBackgroundGroup(tGroup, i, tSprites, tAnimations);
	}
	else {
		logError("Unimplemented menu background type.");
		logErrorString(type);
		abortSystem();
	}

	freeMemory(type);
}

void loadMenuBackground(MugenDefScript * tScript, MugenSpriteFile * tSprites, MugenAnimations * tAnimations, char * tDefinitionGroupName, char * tBackgroundGroupName)
{
	instantiateActor(DreamMugenStageHandler);
	gData.mLocalCoordinates = makeVector3DI(320, 240, 0); // TODO: move outside
	setDreamStageCoordinates(gData.mLocalCoordinates);
	setDreamMugenStageHandlerCameraCoordinates(gData.mLocalCoordinates);
	setDreamMugenStageHandlerCameraRange(makeGeoRectangle(0, 0, gData.mLocalCoordinates.x, gData.mLocalCoordinates.y));
	setDreamMugenStageHandlerCameraPosition(makePosition(0, 0, 0));

	MugenDefScriptGroup* current = string_map_get(&tScript->mGroups, tDefinitionGroupName);
	current = current->mNext;

	int i = 1;
	int isRunning = 1;
	while (isRunning) {
		if (!isMenuBackgroundGroup(current, tBackgroundGroupName)) break;

		loadMenuBackgroundGroup(current, i, tSprites, tAnimations);

		i++;
		current = current->mNext;
	}
}
