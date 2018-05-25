#include "storyscreen.h"

#include <assert.h>
#include <prism/mugenanimationhandler.h>
#include <prism/mugendefreader.h>
#include <prism/input.h>
#include <prism/math.h>
#include <prism/screeneffect.h>
#include <prism/mugentexthandler.h>
#include <prism/clipboardhandler.h>

#include "titlescreen.h"
#include "versusscreen.h"

typedef struct {
	int mIsActive;
	int mStage;

	MugenAnimation* mAnimation;
	Vector3D mOffset;
	int mTime;

	int mAnimationID;
} Layer;

typedef struct {
	int mEndTime;
	
	int mFadeInTime;
	Vector3DI mFadeInColor;

	int mFadeOutTime;
	Vector3DI mFadeOutColor;

	Vector3DI mClearColor;

	Vector3D mLayerAllPosition;

	Layer mLayers[10];
} Scene;

static struct {
	char mDefinitionPath[1024];
	MugenDefScript mScript;
	MugenAnimations mAnimations;
	MugenSpriteFile mSprites;
	void(*mCB)();

	Vector mScenes;

	int mCurrentScene;
	Duration mNow;
	int mIsFadingOut;
} gData;

static void loadScriptAndSprites() {
	gData.mScript = loadMugenDefScript(gData.mDefinitionPath);
	gData.mAnimations = loadMugenAnimationFile(gData.mDefinitionPath);

	char folder[1024];
	getPathToFile(folder, gData.mDefinitionPath);
	setWorkingDirectory(folder);

	char* text = getAllocatedMugenDefStringVariable(&gData.mScript, "SceneDef", "spr");
	gData.mSprites = loadMugenSpriteFileWithoutPalette(text);
	freeMemory(text);

	setWorkingDirectory("/");
}

static int isSceneGroup(MugenDefScriptGroup* tGroup) {
	char firstW[100];
	int items = sscanf(tGroup->mName, "%s", firstW);
	if (items != 1) return 0;

	return !strcmp("Scene", firstW);
}

static void loadSingleLayer(MugenDefScriptGroup* tGroup, Scene* tScene, int i) {
	Layer* e = &tScene->mLayers[i];
	char variableName[100];
	
	sprintf(variableName, "layer%d.anim", i);
	e->mIsActive = isMugenDefNumberVariableAsGroup(tGroup, variableName);

	if (!e->mIsActive) return;

	sprintf(variableName, "layer%d.anim", i);
	int animation = getMugenDefIntegerOrDefaultAsGroup(tGroup, variableName, -1);
	e->mAnimation = getMugenAnimation(&gData.mAnimations, animation);

	sprintf(variableName, "layer%d.offset", i);
	e->mOffset = getMugenDefVectorOrDefaultAsGroup(tGroup, variableName, makePosition(0, 0, 0));

	sprintf(variableName, "layer%d.starttime", i);
	e->mTime = getMugenDefIntegerOrDefaultAsGroup(tGroup, variableName, 0);

	e->mStage = 0;
}

static void loadSingleScene(MugenDefScriptGroup* tGroup) {
	Scene* e = allocMemory(sizeof(Scene));

	e->mEndTime = getMugenDefIntegerOrDefaultAsGroup(tGroup, "end.time", INF);
	e->mFadeInTime = getMugenDefIntegerOrDefaultAsGroup(tGroup, "fadein.time", 0);
	e->mFadeInColor = getMugenDefVectorIOrDefaultAsGroup(tGroup, "fadein.col", makeVector3DI(0,0,0));

	e->mFadeOutTime = getMugenDefIntegerOrDefaultAsGroup(tGroup, "fadeout.time", 0);
	e->mFadeOutColor = getMugenDefVectorIOrDefaultAsGroup(tGroup, "fadeout.col", makeVector3DI(0, 0, 0));

	if (vector_size(&gData.mScenes)) {
		Scene* previousScene = vector_get(&gData.mScenes, vector_size(&gData.mScenes) - 1);
		e->mClearColor = getMugenDefVectorIOrDefaultAsGroup(tGroup, "clearcolor", previousScene->mClearColor);
		e->mLayerAllPosition = getMugenDefVectorOrDefaultAsGroup(tGroup, "layerall.pos", previousScene->mLayerAllPosition);
	}
	else {
		e->mClearColor = getMugenDefVectorIOrDefaultAsGroup(tGroup, "clearcolor", makeVector3DI(0, 0, 0));
		e->mLayerAllPosition = getMugenDefVectorOrDefaultAsGroup(tGroup, "layerall.pos", makePosition(0, 0, 0));
	}

	int i;
	for (i = 0; i < 10; i++) {
		loadSingleLayer(tGroup, e, i);
	}

	vector_push_back_owned(&gData.mScenes, e);
}

static void loadScenes() {
	gData.mScenes = new_vector();
	
	MugenDefScriptGroup* group = gData.mScript.mFirstGroup;
	while (group != NULL) {
		if (isSceneGroup(group)) {
			loadSingleScene(group);
		}

		group = group->mNext;
	}
}

static void startScene();

static void loadStoryScreen() {
	instantiateActor(MugenTextHandler);
	instantiateActor(getMugenAnimationHandlerActorBlueprint());
	instantiateActor(ClipboardHandler);

	loadScriptAndSprites();
	loadScenes();

	gData.mCurrentScene = 0;
	startScene();
}




static void unloadScenes() {
	delete_vector(&gData.mScenes);
}

static void unloadStoryScreen() {
	unloadMugenDefScript(gData.mScript);
	unloadMugenAnimationFile(&gData.mAnimations);
	unloadMugenSpriteFile(&gData.mSprites);

	unloadScenes();
}

static void startScene() {
	assert(gData.mCurrentScene < vector_size(&gData.mScenes));
	Scene* scene = vector_get(&gData.mScenes, gData.mCurrentScene);

	gData.mNow = 0;
	gData.mIsFadingOut = 0;

	setScreenBackgroundColorRGB(scene->mClearColor.x / 255.0, scene->mClearColor.y / 255.0, scene->mClearColor.z / 255.0);
	setFadeColorRGB(scene->mFadeInColor.x / 255.0, scene->mFadeInColor.y / 255.0, scene->mFadeInColor.z / 255.0);
	addFadeIn(scene->mFadeInTime, NULL, NULL);
}

static void activateLayer(Scene* tScene, Layer* tLayer, int i) {
	Position pos = vecAdd(tScene->mLayerAllPosition, tLayer->mOffset);
	pos.z = 10 + i;

	tLayer->mAnimationID = addMugenAnimation(tLayer->mAnimation, &gData.mSprites, pos);
	tLayer->mStage = 1;
}

static void updateLayerActivation(Scene* tScene, Layer* tLayer, int i) {
	if (isDurationOver(gData.mNow, tLayer->mTime)) {
		activateLayer(tScene, tLayer, i);
	}
}

static void updateSingleLayer(int i) {
	Scene* scene = vector_get(&gData.mScenes, gData.mCurrentScene);
	Layer* layer = &scene->mLayers[i];

	if (!layer->mIsActive) return;

	if (layer->mStage == 0) {
		updateLayerActivation(scene, layer, i);
	}
}

static void updateLayers() {
	int i;
	for (i = 0; i < 10; i++) {
		updateSingleLayer(i);
	}
}

static void unloadLayer(int i) {
	Scene* scene = vector_get(&gData.mScenes, gData.mCurrentScene);
	Layer* layer = &scene->mLayers[i];

	if (!layer->mIsActive) return;
	if (layer->mStage != 1) return;

	removeMugenAnimation(layer->mAnimationID);
}

static void unloadScene() {
	int i;
	for (i = 0; i < 10; i++) {
		unloadLayer(i);
	}
}

static void gotoNextScreen() {
	gData.mCB();
}

static void fadeOutSceneOver(void* tCaller) {
	(void)tCaller;
	unloadScene();
	enableDrawing();

	gData.mCurrentScene++;

	if (gData.mCurrentScene < vector_size(&gData.mScenes)) {
		startScene();
	}
	else {
		gData.mCurrentScene--; // TODO: check
		gotoNextScreen();
	}
}

static void fadeOutScene() {
	Scene* scene = vector_get(&gData.mScenes, gData.mCurrentScene);

	setFadeColorRGB(scene->mFadeOutColor.x / 255.0, scene->mFadeOutColor.y / 255.0, scene->mFadeOutColor.z / 255.0);
	addFadeOut(scene->mFadeOutTime, fadeOutSceneOver, NULL);

	gData.mIsFadingOut = 1;
}

static void updateScene() {
	if (gData.mIsFadingOut) return;

	Scene* scene = vector_get(&gData.mScenes, gData.mCurrentScene);
	if (isDurationOver(gData.mNow, scene->mEndTime)) {
		fadeOutScene();
	}
}

static void updateStoryScreen() {
	updateLayers();
	updateScene();

	handleDurationAndCheckIfOver(&gData.mNow, INF);

	if (hasPressedAFlank() || hasPressedStartFlank()) {
		gotoNextScreen();
	}

	if (hasPressedAbortFlank()) {
		setNewScreen(&DreamTitleScreen);
	}

}

Screen StoryScreen = {
	.mLoad = loadStoryScreen,
	.mUnload = unloadStoryScreen,
	.mUpdate = updateStoryScreen,
};

void setStoryDefinitionFile(char* tPath) {
	strcpy(gData.mDefinitionPath, tPath);
}

void setStoryScreenFinishedCB(void(*tCB)())
{
	gData.mCB = tCB;
}
