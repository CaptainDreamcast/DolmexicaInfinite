#include "storyscreen.h"

#include <assert.h>
#include <prism/mugenanimationhandler.h>
#include <prism/mugendefreader.h>
#include <prism/input.h>
#include <prism/math.h>
#include <prism/screeneffect.h>
#include <prism/mugentexthandler.h>
#include <prism/clipboardhandler.h>
#include <prism/mugensoundfilereader.h>
#include <prism/sound.h>

#include "titlescreen.h"
#include "versusscreen.h"
#include "scriptbackground.h"
#include "mugenstagehandler.h"
#include "stage.h"
#include "mugensound.h"
#include "config.h"

enum class StoryScreenLayerType {
	ANIMATION,
	TEXT
};

typedef struct {
	int mStage;
	StoryScreenLayerType mType;

	MugenAnimation* mAnimation;
	Vector3D mOffset;
	int mStartTime;
	int mEndTime;

	std::string mText;
	Vector3DI mFont;
	Vector3D mTextColor;
	int mTextDelay;

	MugenAnimationHandlerElement* mAnimationElement;
	int mTextID;
} Layer;

typedef struct {
	int mStage;
	Vector2DI mValue;
	int mStartTime;
	double mVolumeScale;
	double mPanning;
} StorySound;

typedef struct {
	int mEndTime;
	
	int mFadeInTime;
	Vector3DI mFadeInColor;

	int mFadeOutTime;
	Vector3DI mFadeOutColor;

	Vector3DI mClearColor;

	Vector3D mLayerAllPosition;

	int mHasBGM;
	std::string mBGMPath;
	int mIsBGMLooping;

	GeoRectangle2D mWindow;
	std::string mBGName;

	std::unordered_map<int, Layer> mLayers;
	std::unordered_map<int, StorySound> mSounds;
} Scene;

static struct {
	char mDefinitionPath[1024];
	MugenDefScript mScript;
	MugenAnimations mAnimations;
	MugenSpriteFile mSprites;
	MugenSounds mSounds;
	Vector2DI mLocalCoordinates;
	void(*mCB)();

	std::vector<Scene> mScenes;

	int mCurrentScene;
	Duration mNow;
	int mIsFadingOut;
} gStoryScreenData;

static void loadSystemFonts(void*) {
	unloadMugenFonts();
	loadMugenSystemFonts();
}

static void loadStoryFonts(void*) {
	unloadMugenFonts();
	loadMugenStoryFonts(gStoryScreenData.mDefinitionPath, "scenedef");
}

static void loadScriptAndSprites() {
	loadMugenDefScript(&gStoryScreenData.mScript, gStoryScreenData.mDefinitionPath);
	gStoryScreenData.mAnimations = loadMugenAnimationFile(gStoryScreenData.mDefinitionPath);

	gStoryScreenData.mLocalCoordinates = getMugenDefVector2DIOrDefault(&gStoryScreenData.mScript, "info", "localcoord", Vector2DI(320, 240));

	std::string folder;
	getPathToFile(folder, gStoryScreenData.mDefinitionPath);
	setWorkingDirectory(folder.c_str());

	const auto spritePath = getSTLMugenDefStringVariable(&gStoryScreenData.mScript, "scenedef", "spr");
	gStoryScreenData.mSprites = loadMugenSpriteFileWithoutPalette(spritePath);

	const auto soundPath = getSTLMugenDefStringOrDefault(&gStoryScreenData.mScript, "scenedef", "snd", "");
	if (isFile(soundPath.c_str())) {
		gStoryScreenData.mSounds = loadMugenSoundFile(soundPath.c_str());
	}
	else {
		gStoryScreenData.mSounds = createEmptyMugenSoundFile();
	}

	gStoryScreenData.mCurrentScene = getMugenDefIntegerOrDefault(&gStoryScreenData.mScript, "scenedef", "startscene", 0);

	setWorkingDirectory("/");
}

static int isSceneGroup(MugenDefScriptGroup* tGroup) {
	char firstW[100];
	int items = sscanf(tGroup->mName.data(), "%s", firstW);

	return items == 1 && !strcmp("scene", firstW);
}

static void loadSingleLayerGeneral(MugenDefScriptGroup* tGroup, Layer* tLayer, int i) {
	char variableName[100];
	sprintf(variableName, "layer%d.offset", i);
	tLayer->mOffset = getMugenDefVectorOrDefaultAsGroup(tGroup, variableName, Vector3D(0, 0, 0));

	sprintf(variableName, "layer%d.starttime", i);
	tLayer->mStartTime = getMugenDefIntegerOrDefaultAsGroup(tGroup, variableName, 0);
	
	sprintf(variableName, "layer%d.endtime", i);
	tLayer->mEndTime = getMugenDefIntegerOrDefaultAsGroup(tGroup, variableName, INF);

	tLayer->mStage = 0;
}

static void loadSingleLayerAnimation(MugenDefScriptGroup* tGroup, Scene* tScene, int i) {
	char variableName[100];

	Layer* e = &tScene->mLayers[i];
	sprintf(variableName, "layer%d.anim", i);
	const auto animation = getMugenDefIntegerOrDefaultAsGroup(tGroup, variableName, -1);
	e->mAnimation = getMugenAnimation(&gStoryScreenData.mAnimations, animation);

	loadSingleLayerGeneral(tGroup, e, i);
	e->mType = StoryScreenLayerType::ANIMATION;
}

static void loadSingleLayerText(MugenDefScriptGroup* tGroup, Scene* tScene, int i) {
	char variableName[100];

	Layer* e = &tScene->mLayers[i];
	sprintf(variableName, "layer%d.text", i);
	e->mText = getSTLMugenDefStringOrDefaultAsGroup(tGroup, variableName, "");

	e->mFont = Vector3DI(-1, 0, 0);
	e->mTextColor = Vector3D(1.0, 1.0, 1.0);
	sprintf(variableName, "layer%d.font", i);
	if (isMugenDefStringVectorVariableAsGroup(tGroup, variableName))
	{
		auto stringVector = getMugenDefStringVectorVariableAsGroup(tGroup, variableName);
		if(stringVector.mSize > 0) e->mFont.x = atoi(stringVector.mElement[0]);
		if(stringVector.mSize > 1) e->mFont.y = atoi(stringVector.mElement[1]);
		if(stringVector.mSize > 2) e->mFont.z = atoi(stringVector.mElement[2]);
		if(stringVector.mSize > 3) e->mTextColor.x = atoi(stringVector.mElement[3]) / 255.0;
		if(stringVector.mSize > 4) e->mTextColor.y = atoi(stringVector.mElement[4]) / 255.0;
		if(stringVector.mSize > 5) e->mTextColor.z = atoi(stringVector.mElement[5]) / 255.0;
		destroyMugenStringVector(stringVector);
	}

	sprintf(variableName, "layer%d.textdelay", i);
	e->mTextDelay = getMugenDefIntegerOrDefaultAsGroup(tGroup, variableName, 0);

	loadSingleLayerGeneral(tGroup, e, i);
	e->mType = StoryScreenLayerType::TEXT;
}

static void loadSingleLayer(MugenDefScriptGroup* tGroup, Scene* tScene, int i) {
	char variableName[100];
	sprintf(variableName, "layer%d.anim", i);
	if (isMugenDefNumberVariableAsGroup(tGroup, variableName)) {
		loadSingleLayerAnimation(tGroup, tScene, i);
		return;
	} 
	
	sprintf(variableName, "layer%d.text", i);
	if(isMugenDefStringVariableAsGroup(tGroup, variableName)) {
		loadSingleLayerText(tGroup, tScene, i);
	}
}

static void loadSingleSound(MugenDefScriptGroup* tGroup, Scene* tScene, int i) {
	char variableName[100];
	sprintf(variableName, "sound%d.value", i);
	if (!isMugenDefVector2DIVariableAsGroup(tGroup, variableName)) return;

	StorySound* e = &tScene->mSounds[i];
	sprintf(variableName, "sound%d.value", i);
	e->mValue = getMugenDefVector2DIOrDefaultAsGroup(tGroup, variableName, Vector2DI(1, 0));

	sprintf(variableName, "sound%d.starttime", i);
	e->mStartTime = getMugenDefIntegerOrDefaultAsGroup(tGroup, variableName, 0);

	sprintf(variableName, "sound%d.volumescale", i);
	e->mVolumeScale = getMugenDefFloatOrDefaultAsGroup(tGroup, variableName, 100.0) / 100.0;

	sprintf(variableName, "sound%d.pan", i);
	e->mPanning = getMugenDefIntegerOrDefaultAsGroup(tGroup, variableName, 0) / 127.0;

	e->mStage = 0;
}


static void loadSingleScene(MugenDefScriptGroup* tGroup) {
	gStoryScreenData.mScenes.push_back(Scene());
	Scene& e = gStoryScreenData.mScenes.back();

	e.mEndTime = getMugenDefIntegerOrDefaultAsGroup(tGroup, "end.time", INF);
	e.mFadeInTime = getMugenDefIntegerOrDefaultAsGroup(tGroup, "fadein.time", 0);
	e.mFadeInColor = getMugenDefVectorIOrDefaultAsGroup(tGroup, "fadein.col", Vector3DI(0,0,0));

	e.mFadeOutTime = getMugenDefIntegerOrDefaultAsGroup(tGroup, "fadeout.time", 0);
	e.mFadeOutColor = getMugenDefVectorIOrDefaultAsGroup(tGroup, "fadeout.col", Vector3DI(0, 0, 0));

	if (gStoryScreenData.mScenes.size() > 1) {
		Scene* previousScene = &gStoryScreenData.mScenes[gStoryScreenData.mScenes.size() - 2];
		e.mClearColor = getMugenDefVectorIOrDefaultAsGroup(tGroup, "clearcolor", previousScene->mClearColor);
		e.mLayerAllPosition = getMugenDefVectorOrDefaultAsGroup(tGroup, "layerall.pos", previousScene->mLayerAllPosition);
	}
	else {
		e.mClearColor = getMugenDefVectorIOrDefaultAsGroup(tGroup, "clearcolor", Vector3DI(0, 0, 0));
		e.mLayerAllPosition = getMugenDefVectorOrDefaultAsGroup(tGroup, "layerall.pos", Vector3D(0, 0, 0));
	}

	e.mHasBGM = isMugenDefStringVariableAsGroup(tGroup, "bgm");
	if (e.mHasBGM) {
		e.mBGMPath = getSTLMugenDefStringVariableAsGroup(tGroup, "bgm");
		e.mIsBGMLooping = getMugenDefIntegerOrDefaultAsGroup(tGroup, "bgm.loop", 0);
	}

	e.mWindow = getMugenDefGeoRectangle2DOrDefaultAsGroup(tGroup, "window", GeoRectangle2D(-INF / 2, -INF / 2, INF, INF));
	e.mBGName = getSTLMugenDefStringOrDefaultAsGroup(tGroup, "bg.name", "");
	turnStringLowercase(e.mBGName);

	for (int i = 0; i < 100; i++) {
		loadSingleLayer(tGroup, &e, i);
	}
	for (int i = 0; i < 100; i++) {
		loadSingleSound(tGroup, &e, i);
	}
}

static void loadScenes() {
	gStoryScreenData.mScenes.clear();
	
	MugenDefScriptGroup* group = gStoryScreenData.mScript.mFirstGroup;
	while (group != NULL) {
		if (isSceneGroup(group)) {
			loadSingleScene(group);
		}

		group = group->mNext;
	}
}

static void startScene();

static void loadStoryScreen() {
	instantiateActor(getDreamMugenStageHandler());
	loadScriptAndSprites();
	loadScenes();

	startScene();

	setWrapperBetweenScreensCB(loadSystemFonts, NULL);
}

static void unloadScenes() {
	gStoryScreenData.mScenes.clear();
}

static void unloadStoryScreen() {
	unloadMugenDefScript(&gStoryScreenData.mScript);
	unloadMugenAnimationFile(&gStoryScreenData.mAnimations);
	unloadMugenSpriteFile(&gStoryScreenData.mSprites);

	unloadScenes();
}

static void startScene() {
	assert(gStoryScreenData.mCurrentScene < int(gStoryScreenData.mScenes.size()));
	Scene* scene = &gStoryScreenData.mScenes[gStoryScreenData.mCurrentScene];

	gStoryScreenData.mNow = 0;
	gStoryScreenData.mIsFadingOut = 0;

	const auto definitionGroupName = scene->mBGName + "def";
	if (hasMugenDefScriptGroup(&gStoryScreenData.mScript, definitionGroupName.c_str())) {
		loadScriptBackground(&gStoryScreenData.mScript, &gStoryScreenData.mSprites, &gStoryScreenData.mAnimations, definitionGroupName.c_str(), scene->mBGName.c_str(), gStoryScreenData.mLocalCoordinates, 0);
	}

	if (scene->mHasBGM) {
		stopMusic();
		if (isMugenBGMMusicPath(scene->mBGMPath.c_str(), gStoryScreenData.mDefinitionPath)) {
			playMugenBGMMusicPath(scene->mBGMPath.c_str(), gStoryScreenData.mDefinitionPath, scene->mIsBGMLooping);
		}
	}

	setScreenBackgroundColorRGB(scene->mClearColor.x / 255.0, scene->mClearColor.y / 255.0, scene->mClearColor.z / 255.0);
	setFadeColorRGB(scene->mFadeInColor.x / 255.0, scene->mFadeInColor.y / 255.0, scene->mFadeInColor.z / 255.0);
	addFadeIn(scene->mFadeInTime, NULL, NULL);
}

static void activateLayerAnimation(Scene* tScene, Layer* tLayer, const Position& tPos) {
	tLayer->mAnimationElement = addMugenAnimation(tLayer->mAnimation, &gStoryScreenData.mSprites, tPos);
	setMugenAnimationConstraintRectangle(tLayer->mAnimationElement, tScene->mWindow);
}

static void activateLayerText(Scene* tScene, Layer* tLayer, const Position& tPos) {
	tLayer->mTextID = addMugenTextMugenStyle(tLayer->mText.c_str(), tPos, tLayer->mFont);
	setMugenTextColorRGB(tLayer->mTextID, tLayer->mTextColor.x, tLayer->mTextColor.y, tLayer->mTextColor.z);
	if (tLayer->mTextDelay) {
		setMugenTextBuildup(tLayer->mTextID, tLayer->mTextDelay);
	}
	setMugenTextRectangle(tLayer->mTextID, tScene->mWindow);
}

static void activateLayer(Scene* tScene, Layer* tLayer, int i) {
	Position pos = vecAdd(tScene->mLayerAllPosition, tLayer->mOffset);
	pos.z = 10 + i;
	const auto transformedPosition = transformDreamCoordinatesVector(pos, gStoryScreenData.mLocalCoordinates.x, 320);

	if (tLayer->mType == StoryScreenLayerType::ANIMATION) {
		activateLayerAnimation(tScene, tLayer, transformedPosition);
	}
	else {
		activateLayerText(tScene, tLayer, transformedPosition);
	}

	tLayer->mStage = 1;
}

static void unloadLayerAnimation(Layer* tLayer) {
	removeMugenAnimation(tLayer->mAnimationElement);
}

static void unloadLayerText(Layer* tLayer) {
	removeMugenText(tLayer->mTextID);
}

static void unloadLayer(Layer* tLayer) {
	if (tLayer->mStage != 1) return;

	if (tLayer->mType == StoryScreenLayerType::ANIMATION) {
		unloadLayerAnimation(tLayer);
	}
	else {
		unloadLayerText(tLayer);
	}
	tLayer->mStage = 2;
}

static void updateLayerActivationAndDeactivation(Scene* tScene, Layer* tLayer, int i) {
	if (tLayer->mStage == 0 && isDurationOver(gStoryScreenData.mNow, tLayer->mStartTime)) {
		activateLayer(tScene, tLayer, i);
	}
	if (tLayer->mStage == 1 && isDurationOver(gStoryScreenData.mNow, tLayer->mEndTime)) {
		unloadLayer(tLayer);
	}
}

static void updateSingleLayer(int i, Layer* tLayer) {
	Scene* scene = &gStoryScreenData.mScenes[gStoryScreenData.mCurrentScene];
	updateLayerActivationAndDeactivation(scene, tLayer, i);
}

static void updateLayers() {
	assert(gStoryScreenData.mCurrentScene < int(gStoryScreenData.mScenes.size()));
	Scene* scene = &gStoryScreenData.mScenes[gStoryScreenData.mCurrentScene];
	for (auto& layerEntry : scene->mLayers) {
		updateSingleLayer(layerEntry.first, &layerEntry.second);
	}
}

static void updateSingleSound(StorySound& tSound) {
	if (tSound.mStage == 0 && isDurationOver(gStoryScreenData.mNow, tSound.mStartTime)) {
		tryPlayMugenSoundAdvanced(&gStoryScreenData.mSounds, tSound.mValue.x, tSound.mValue.y, parseGameMidiVolumeToPrism(getGameMidiVolume()) * tSound.mVolumeScale, -1, 1.0, 0, tSound.mPanning);
		tSound.mStage = 1;
	}
}

static void updateSounds() {
	assert(gStoryScreenData.mCurrentScene < int(gStoryScreenData.mScenes.size()));
	Scene* scene = &gStoryScreenData.mScenes[gStoryScreenData.mCurrentScene];
	for (auto& soundEntry : scene->mSounds) {
		updateSingleSound(soundEntry.second);
	}
}

static void unloadScene() {
	assert(gStoryScreenData.mCurrentScene < int(gStoryScreenData.mScenes.size()));
	Scene* scene = &gStoryScreenData.mScenes[gStoryScreenData.mCurrentScene];
	for (auto& layerEntry : scene->mLayers) {
		unloadLayer(&layerEntry.second);
	}
	clearDreamMugenStageHandler();
}

static void gotoNextScreen() {
	gStoryScreenData.mCB();
}

static void fadeOutSceneOver(void* tCaller) {
	(void)tCaller;
	unloadScene();
	enableDrawing();

	gStoryScreenData.mCurrentScene++;

	if (gStoryScreenData.mCurrentScene < int(gStoryScreenData.mScenes.size())) {
		startScene();
	}
	else {
		gStoryScreenData.mCurrentScene--;
		gotoNextScreen();
	}
}

static void fadeOutScene() {
	Scene* scene = &gStoryScreenData.mScenes[gStoryScreenData.mCurrentScene];

	setFadeColorRGB(scene->mFadeOutColor.x / 255.0, scene->mFadeOutColor.y / 255.0, scene->mFadeOutColor.z / 255.0);
	addFadeOut(scene->mFadeOutTime, fadeOutSceneOver, NULL);

	gStoryScreenData.mIsFadingOut = 1;
}

static void updateScene() {
	if (gStoryScreenData.mIsFadingOut) return;

	Scene* scene = &gStoryScreenData.mScenes[gStoryScreenData.mCurrentScene];
	if (isDurationOver(gStoryScreenData.mNow, scene->mEndTime)) {
		fadeOutScene();
	}
}

static void updateStoryScreen() {
	updateLayers();
	updateSounds();
	updateScene();

	handleDurationAndCheckIfOver(&gStoryScreenData.mNow, INF);

	if (hasPressedAFlank() || hasPressedStartFlank()) {
		gotoNextScreen();
	}

}

static Screen gStoryScreen;

Screen* getStoryScreen() {
	gStoryScreen = makeScreen(loadStoryScreen, updateStoryScreen, NULL, unloadStoryScreen);
	return &gStoryScreen;
};

void setStoryDefinitionFileAndPrepareScreen(const char* tPath) {
	strcpy(gStoryScreenData.mDefinitionPath, tPath);
	setWrapperBetweenScreensCB(loadStoryFonts, NULL);
}

void setStoryScreenFinishedCB(void(*tCB)())
{
	gStoryScreenData.mCB = tCB;
}
