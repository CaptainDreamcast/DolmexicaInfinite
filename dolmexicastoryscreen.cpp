#include "dolmexicastoryscreen.h"

#include <string.h>

#include <prism/log.h>
#include <prism/math.h>
#include <prism/actorhandler.h>
#include <prism/mugenanimationhandler.h>
#include <prism/mugentexthandler.h>
#include <prism/input.h>
#include <prism/stlutil.h>
#include <prism/sound.h>
#include <prism/debug.h>
#include <prism/system.h>

#include "mugenassignmentevaluator.h"
#include "mugenstatecontrollers.h"
#include "mugenstatehandler.h"
#include "titlescreen.h"
#include "stage.h"
#include "mugenstagehandler.h"
#include "storymode.h"
#include "characterselectscreen.h"
#include "mugencommandhandler.h"
#include "mugensound.h"
#include "dolmexicadebug.h"
#include "config.h"

using namespace std;

#define COORD_P 320
#define DOLMEXICA_STORY_ANIMATION_BASE_Z 30
#define DOLMEXICA_STORY_SHADOW_BASE_Z (DOLMEXICA_STORY_ANIMATION_BASE_Z - 1)
#define DOLMEXICA_STORY_TEXT_BASE_Z 60

static struct {
	char mPath[1024];
	MugenSpriteFile mSprites;
	MugenAnimations mAnimations;
	MugenSounds mSounds;

	int mHasCommands;
	DreamMugenCommands mCommands;
	int mCommandID;

	DreamMugenStates mStoryStates;
	int mHasStage;
	int mHasMusic;
	int mHasFonts;

	map<int, StoryInstance> mHelperInstances;
} gDolmexicaStoryScreenData;

static void loadSystemFonts(void*) {
	unloadMugenFonts();
	loadMugenSystemFonts();
}

static void loadStoryFonts(void*) {
	unloadMugenFonts();
	loadMugenStoryFonts(gDolmexicaStoryScreenData.mPath);
}

static void loadStoryFilesFromScript(MugenDefScript* tScript) {
	char pathToFile[1024];
	char path[1024];
	char fullPath[1024];

	getPathToFile(pathToFile, gDolmexicaStoryScreenData.mPath);

	getMugenDefStringOrDefault(path, tScript, "Info", "spr", "");
	sprintf(fullPath, "%s%s", pathToFile, path);
	gDolmexicaStoryScreenData.mSprites = loadMugenSpriteFileWithoutPalette(fullPath);

	getMugenDefStringOrDefault(path, tScript, "Info", "anim", "");
	sprintf(fullPath, "%s%s", pathToFile, path);
	gDolmexicaStoryScreenData.mAnimations = loadMugenAnimationFile(fullPath);

	getMugenDefStringOrDefault(path, tScript, "Info", "snd", "");
	sprintf(fullPath, "%s%s", pathToFile, path);
	if (isFile(fullPath)) {
		gDolmexicaStoryScreenData.mSounds = loadMugenSoundFile(fullPath);
	}
	else {
		gDolmexicaStoryScreenData.mSounds = createEmptyMugenSoundFile();
	}

	getMugenDefStringOrDefault(path, tScript, "Info", "cmd", "");
	sprintf(fullPath, "%s%s", pathToFile, path);
	gDolmexicaStoryScreenData.mHasCommands = isFile(fullPath);
	if (gDolmexicaStoryScreenData.mHasCommands) {
		gDolmexicaStoryScreenData.mCommands = loadDreamMugenCommandFile(fullPath);
		gDolmexicaStoryScreenData.mCommandID = registerDreamMugenCommands(0, &gDolmexicaStoryScreenData.mCommands);
	}

	getMugenDefStringOrDefault(path, tScript, "Info", "stage", "");
	sprintf(fullPath, "%s%s", getDolmexicaAssetFolder().c_str(), path);
	const auto musicPath = getSTLMugenDefStringOrDefault(tScript, "Info", "bgm", "");
	setDreamStageMugenDefinition(fullPath, musicPath.c_str());
	gDolmexicaStoryScreenData.mHasStage = isFile(fullPath);
	gDolmexicaStoryScreenData.mHasMusic = isMugenBGMMusicPath(musicPath.c_str(), fullPath);
	gDolmexicaStoryScreenData.mHasMusic |= getMugenDefIntegerOrDefault(tScript, "Info", "stage.bgm", 0);

	for (int i = 0; i < 10; i++) {
		char name[20];
		
		sprintf(name, "helper%d", i);
		getMugenDefStringOrDefault(path, tScript, "Info", name, "");
		sprintf(fullPath, "%s%s", pathToFile, path);
		if (!isFile(fullPath)) continue;

		loadDreamMugenStateDefinitionsFromFile(&gDolmexicaStoryScreenData.mStoryStates, fullPath);
	}

	if (gDolmexicaStoryScreenData.mHasFonts) {
		setWrapperBetweenScreensCB(loadSystemFonts, NULL);
	}
}

static void initStoryInstance(StoryInstance& e){
	e.mStateMachineID = registerDreamMugenStoryStateMachine(&gDolmexicaStoryScreenData.mStoryStates, &e);
	e.mStoryAnimations.clear();
	e.mStoryTexts.clear();
	e.mStoryCharacters.clear();

	e.mIntVars.clear();
	e.mFloatVars.clear();
	e.mStringVars.clear();
	e.mTextNames.clear();
	e.mIsScheduledForDeletion = 0;
	e.mParent = &e;
}

static void unloadStoryInstance(StoryInstance& e) {
	removeDreamRegisteredStateMachine(e.mStateMachineID);
}

static void loadStoryScreen() {
	setStateMachineHandlerToStory();

	setupDreamStoryAssignmentEvaluator();
	setupDreamMugenStoryStateControllerHandler();
	instantiateActor(getDreamMugenStateHandler());
	instantiateActor(getDreamMugenCommandHandler());

	gDolmexicaStoryScreenData.mStoryStates = createEmptyMugenStates();
	loadDreamMugenStateDefinitionsFromFile(&gDolmexicaStoryScreenData.mStoryStates, gDolmexicaStoryScreenData.mPath);

	MugenDefScript script;
	loadMugenDefScript(&script, gDolmexicaStoryScreenData.mPath);
	loadStoryFilesFromScript(&script);
	unloadMugenDefScript(script);
	
	gDolmexicaStoryScreenData.mHelperInstances.clear();
	StoryInstance root;
	gDolmexicaStoryScreenData.mHelperInstances[-1] = root;
	initStoryInstance(gDolmexicaStoryScreenData.mHelperInstances[-1]);

	if (gDolmexicaStoryScreenData.mHasStage) {
		instantiateActor(getDreamStageBP());
		setDreamStageNoAutomaticCameraMovement();
	}

	updateDreamSingleStateMachineByID(gDolmexicaStoryScreenData.mHelperInstances[-1].mStateMachineID);

	if (gDolmexicaStoryScreenData.mHasMusic) {
		playDreamStageMusic();
	}
}

static void unloadStoryScreen() {
	gDolmexicaStoryScreenData.mHelperInstances.clear();
	shutdownDreamMugenStateControllerHandler();
}

static void unloadDolmexicaStoryText(StoryText* e);

static int updateSingleTextInput(StoryInstance* tInstance, StoryText* e) {
	if (e->mHasFinished) return 0;

	if (e->mHasContinue) {
		if (isMugenTextBuiltUp(e->mTextID)) {
			setMugenAnimationVisibility(e->mContinueAnimationElement, 1);
		}
	}

	if (hasPressedAFlank()) {
		if (isMugenTextBuiltUp(e->mTextID)) {
			if (e->mGoesToNextState) {
				changeDolmexicaStoryStateOutsideStateHandler(tInstance, e->mNextState);
				e->mGoesToNextState = 0;
				e->mHasFinished = 1;
				setDolmexicaStoryTextInactive(tInstance, e->mID);
			}
		}
		else {
			setMugenTextBuiltUp(e->mTextID);
		}
	}

	return 0;
}

static void updateLockedTextPosition(StoryInstance* tInstance, StoryText* e) {
	if (!e->mIsLockedOnToCharacter) return;

	Position p = vecAdd(*e->mLockCharacterPositionReference, e->mLockOffset);
	if (e->mLockCharacterIsBoundToStage)
	{
		p = vecSub(p, *getDreamMugenStageHandlerCameraPositionReference());
	}
	setDolmexicaStoryTextBasePosition(tInstance, e->mID, p);
}

static int updateSingleText(StoryInstance* instance, StoryText& tData) {
	StoryText* e = &tData;
	updateLockedTextPosition(instance, e);
	if (e->mIsDisabled) return 0;

	return updateSingleTextInput(instance, e);
}

static void updateTexts(StoryInstance& e) {
	stl_int_map_remove_predicate(e.mStoryTexts, updateSingleText, &e);
}

static int updateSingleAnimation(void* /*tCaller*/, StoryAnimation& e) {
	return e.mIsDeleted;
}

static void updateAnimations(StoryInstance& e) {
	stl_int_map_remove_predicate(e.mStoryAnimations, updateSingleAnimation);
}

static int updateSingleInstance(void* /*tCaller*/, StoryInstance& tInstance) {
	if (tInstance.mIsScheduledForDeletion)
	{
		unloadStoryInstance(tInstance);
		return 1;
	}

	updateTexts(tInstance);
	updateAnimations(tInstance);

	return 0;
}

static void updateStoryScreen() {
	stl_int_map_remove_predicate(gDolmexicaStoryScreenData.mHelperInstances, updateSingleInstance);
}

static Screen gDolmexicaStoryScreen;

Screen* getDolmexicaStoryScreen() {
	gDolmexicaStoryScreen = makeScreen(loadStoryScreen, updateStoryScreen, NULL, unloadStoryScreen);
	return &gDolmexicaStoryScreen;
};

static void setupFontTransition() {
	MugenDefScript script;
	loadMugenDefScript(&script, gDolmexicaStoryScreenData.mPath);

	gDolmexicaStoryScreenData.mHasFonts = 0;
	for (int i = 0; i < 100; i++) {
		char name[100];
		sprintf(name, "font%d", i);
		gDolmexicaStoryScreenData.mHasFonts |= isMugenDefStringVariable(&script, "Info", name);
	}
	unloadMugenDefScript(script);

	if (gDolmexicaStoryScreenData.mHasFonts) {
		setWrapperBetweenScreensCB(loadStoryFonts, NULL);
	}
}

void setDolmexicaStoryScreenFileAndPrepareScreen(char * tPath)
{
	strcpy(gDolmexicaStoryScreenData.mPath, tPath);
	setupFontTransition();
}

MugenSounds* getDolmexicaStorySounds()
{
	return &gDolmexicaStoryScreenData.mSounds;
}

int isStoryCommandActive(const char* tCommand)
{
	if (!gDolmexicaStoryScreenData.mHasCommands) return 0;
	else return isDreamCommandActive(gDolmexicaStoryScreenData.mCommandID, tCommand);
}

static void initDolmexicaStoryAnimation(StoryAnimation& e, int tID, int tAnimationNumber, const Position& tPosition, MugenSpriteFile* mSprites, MugenAnimations* tAnimations) {
	e.mID = tID;
	e.mAnimationElement = addMugenAnimation(getMugenAnimation(tAnimations, tAnimationNumber), mSprites, tPosition);
	e.mIsBoundToStage = 0;
	e.mHasShadow = 0;
	e.mIsDeleted = 0;
}

void addDolmexicaStoryAnimation(StoryInstance* tInstance, int tID, int tAnimation, const Position& tPosition)
{
	if (stl_map_contains(tInstance->mStoryAnimations, tID)) {
		removeDolmexicaStoryAnimation(tInstance, tID);
	}
	const auto z = DOLMEXICA_STORY_ANIMATION_BASE_Z + tID * 0.1;
	initDolmexicaStoryAnimation(tInstance->mStoryAnimations[tID], tID, tAnimation, makePosition(tPosition.x, tPosition.y, z), &gDolmexicaStoryScreenData.mSprites, &gDolmexicaStoryScreenData.mAnimations);
}

static void unloadDolmexicaStoryAnimation(StoryAnimation& e) {
	removeMugenAnimation(e.mAnimationElement);
	if (e.mHasShadow) {
		removeMugenAnimation(e.mShadowAnimationElement);
	}

}

void removeDolmexicaStoryAnimation(StoryInstance* tInstance, int tID)
{
	StoryAnimation& e = tInstance->mStoryAnimations[tID];
	unloadDolmexicaStoryAnimation(e);
	tInstance->mStoryAnimations.erase(e.mID);
}

int getDolmexicaStoryAnimationIsLooping(StoryInstance * tInstance, int tID)
{
	StoryAnimation& e = tInstance->mStoryAnimations[tID];
	return getMugenAnimationIsLooping(e.mAnimationElement);
}

static void storyAnimationOverCB(void* tCaller) {
	StoryAnimation* e = (StoryAnimation*)tCaller;
	e->mIsDeleted = 1;
}

void setDolmexicaStoryAnimationLooping(StoryInstance* tInstance, int tID, int tIsLooping)
{
	StoryAnimation& e = tInstance->mStoryAnimations[tID];
	if (!tIsLooping) {
		setMugenAnimationNoLoop(e.mAnimationElement);
		if (e.mHasShadow) {
			setMugenAnimationNoLoop(e.mShadowAnimationElement);
		}

		setMugenAnimationCallback(e.mAnimationElement, storyAnimationOverCB, &e);
	}
}

int getDolmexicaStoryAnimationIsBoundToStage(StoryInstance * tInstance, int tID)
{
	StoryAnimation& e = tInstance->mStoryAnimations[tID];
	return e.mIsBoundToStage;
}

static void setDolmexicaStoryAnimationBoundToStageInternal(StoryAnimation& e, int tIsBoundToStage)
{
	if (tIsBoundToStage) {
		Position* p = getMugenAnimationPositionReference(e.mAnimationElement);
		*p = vecAdd(*p, getDreamStageCoordinateSystemOffset(COORD_P));
		*p = vecAdd(*p, makePosition(160, 0, 0));
		setMugenAnimationCameraPositionReference(e.mAnimationElement, getDreamMugenStageHandlerCameraPositionReference());
		setMugenAnimationCameraEffectPositionReference(e.mAnimationElement, getDreamMugenStageHandlerCameraEffectPositionReference());
		setMugenAnimationCameraScaleReference(e.mAnimationElement, getDreamMugenStageHandlerCameraZoomReference());
		if (e.mHasShadow) {
			p = getMugenAnimationPositionReference(e.mShadowAnimationElement);
			*p = vecAdd(*p, getDreamStageCoordinateSystemOffset(COORD_P));
			*p = vecAdd(*p, makePosition(160, 0, 0));
			setMugenAnimationCameraPositionReference(e.mShadowAnimationElement, getDreamMugenStageHandlerCameraPositionReference());
			setMugenAnimationCameraEffectPositionReference(e.mShadowAnimationElement, getDreamMugenStageHandlerCameraEffectPositionReference());
			setMugenAnimationCameraScaleReference(e.mShadowAnimationElement, getDreamMugenStageHandlerCameraZoomReference());
		}
		e.mIsBoundToStage = tIsBoundToStage;
	}
	
}

void setDolmexicaStoryAnimationBoundToStage(StoryInstance* tInstance, int tID, int tIsBoundToStage)
{
	StoryAnimation& e = tInstance->mStoryAnimations[tID];
	setDolmexicaStoryAnimationBoundToStageInternal(e, tIsBoundToStage);
}

int getDolmexicaStoryAnimationHasShadow(StoryInstance * tInstance, int tID)
{
	StoryAnimation& e = tInstance->mStoryAnimations[tID];
	return e.mHasShadow;
}

static void setDolmexicaStoryAnimationPositionXInternal(StoryAnimation& e, double tX);
static void setDolmexicaStoryAnimationPositionYInternal(StoryAnimation& e, double tY);

static void setDolmexicaStoryAnimationShadowInternal(StoryAnimation& e, double tBasePositionY, MugenSpriteFile* tSprites, MugenAnimations* tAnimations) {
	if (!isDrawingShadowsConfig()) return;
	e.mHasShadow = 1;
	e.mShadowBasePositionY = tBasePositionY;
	Position pos = getMugenAnimationPosition(e.mAnimationElement);
	pos.z = DOLMEXICA_STORY_SHADOW_BASE_Z;
	e.mShadowAnimationElement = addMugenAnimation(getMugenAnimation(tAnimations, getMugenAnimationAnimationNumber(e.mAnimationElement)), tSprites, pos);
	setMugenAnimationDrawScale(e.mShadowAnimationElement, makePosition(1, -getDreamStageShadowScaleY(), 1));
	Vector3D color = getDreamStageShadowColor();
	if (isOnDreamcast()) {
		setMugenAnimationColor(e.mShadowAnimationElement, 0, 0, 0);
	}
	else {
		setMugenAnimationColorSolid(e.mShadowAnimationElement, color.x, color.y, color.z);
	}
	setMugenAnimationTransparency(e.mShadowAnimationElement, getDreamStageShadowTransparency());
	setMugenAnimationFaceDirection(e.mShadowAnimationElement, getMugenAnimationIsFacingRight(e.mShadowAnimationElement));

	setDolmexicaStoryAnimationPositionXInternal(e, pos.x);
	setDolmexicaStoryAnimationPositionYInternal(e, pos.y);
}

void setDolmexicaStoryAnimationShadow(StoryInstance* tInstance, int tID, double tBasePositionY)
{
	StoryAnimation& e = tInstance->mStoryAnimations[tID];
	setDolmexicaStoryAnimationShadowInternal(e, tBasePositionY, &gDolmexicaStoryScreenData.mSprites, &gDolmexicaStoryScreenData.mAnimations);
}

int getDolmexicaStoryAnimationAnimation(StoryInstance* tInstance, int tID)
{
	StoryAnimation& e = tInstance->mStoryAnimations[tID];
	return getMugenAnimationAnimationNumber(e.mAnimationElement);
}

static void changeDolmexicaStoryAnimationInternal(StoryAnimation& e, int tAnimation, MugenAnimations* tAnimations)
{
	changeMugenAnimation(e.mAnimationElement, getMugenAnimation(tAnimations, tAnimation));
	if (e.mHasShadow) {
		changeMugenAnimation(e.mShadowAnimationElement, getMugenAnimation(tAnimations, tAnimation));
	}
}

void changeDolmexicaStoryAnimation(StoryInstance* tInstance, int tID, int tAnimation)
{
	StoryAnimation& e = tInstance->mStoryAnimations[tID];
	changeDolmexicaStoryAnimationInternal(e, tAnimation, &gDolmexicaStoryScreenData.mAnimations);
}

static void setDolmexicaStoryAnimationPositionXInternal(StoryAnimation& e, double tX)
{
	Position pos = getMugenAnimationPosition(e.mAnimationElement);
	pos.x = tX;
	setMugenAnimationPosition(e.mAnimationElement, pos);
	if (e.mHasShadow) {
		pos = getMugenAnimationPosition(e.mShadowAnimationElement);
		pos.x = tX;
		setMugenAnimationPosition(e.mShadowAnimationElement, pos);
	}
}

void setDolmexicaStoryAnimationPositionX(StoryInstance* tInstance, int tID, double tX)
{
	StoryAnimation& e = tInstance->mStoryAnimations[tID];
	setDolmexicaStoryAnimationPositionXInternal(e, tX);
}

static void setDolmexicaStoryAnimationPositionYInternal(StoryAnimation& e, double tY)
{
	Position pos = getMugenAnimationPosition(e.mAnimationElement);
	pos.y = tY;
	setMugenAnimationPosition(e.mAnimationElement, pos);
	if (e.mHasShadow) {
		double offsetY = pos.y - e.mShadowBasePositionY;
		pos = getMugenAnimationPosition(e.mShadowAnimationElement);
		pos.y = e.mShadowBasePositionY + getDreamStageShadowScaleY() * offsetY;
		setMugenAnimationPosition(e.mShadowAnimationElement, pos);
	}
}

void setDolmexicaStoryAnimationPositionY(StoryInstance* tInstance, int tID, double tY)
{
	StoryAnimation& e = tInstance->mStoryAnimations[tID];
	setDolmexicaStoryAnimationPositionYInternal(e, tY);
}

static void addDolmexicaStoryAnimationPositionXInternal(StoryAnimation& e, double tX)
{
	Position pos = getMugenAnimationPosition(e.mAnimationElement);
	pos.x += tX;
	setMugenAnimationPosition(e.mAnimationElement, pos);
	if (e.mHasShadow) {
		pos = getMugenAnimationPosition(e.mShadowAnimationElement);
		pos.x += tX;
		setMugenAnimationPosition(e.mShadowAnimationElement, pos);
	}
}

void addDolmexicaStoryAnimationPositionX(StoryInstance* tInstance, int tID, double tX)
{
	StoryAnimation& e = tInstance->mStoryAnimations[tID];
	addDolmexicaStoryAnimationPositionXInternal(e, tX);
}

static void addDolmexicaStoryAnimationPositionYInternal(StoryAnimation& e, double tY)
{
	Position pos = getMugenAnimationPosition(e.mAnimationElement);
	pos.y += tY;
	setMugenAnimationPosition(e.mAnimationElement, pos);
	if (e.mHasShadow) {
		double offsetY = pos.y - e.mShadowBasePositionY;
		pos = getMugenAnimationPosition(e.mShadowAnimationElement);
		pos.y = e.mShadowBasePositionY + getDreamStageShadowScaleY() * offsetY;
		setMugenAnimationPosition(e.mShadowAnimationElement, pos);
	}
}

void addDolmexicaStoryAnimationPositionY(StoryInstance* tInstance, int tID, double tY)
{
	StoryAnimation& e = tInstance->mStoryAnimations[tID];
	addDolmexicaStoryAnimationPositionYInternal(e, tY);
}

static void setDolmexicaStoryAnimationScaleXInternal(StoryAnimation& e, double tX)
{
	Vector3D scale = getMugenAnimationDrawScale(e.mAnimationElement);
	scale.x = tX;
	setMugenAnimationDrawScale(e.mAnimationElement, scale);

	if (e.mHasShadow) {
		scale.y *= -getDreamStageShadowScaleY();
		setMugenAnimationDrawScale(e.mShadowAnimationElement, scale);
	}
}


void setDolmexicaStoryAnimationScaleX(StoryInstance* tInstance, int tID, double tX)
{
	StoryAnimation& e = tInstance->mStoryAnimations[tID];
	setDolmexicaStoryAnimationScaleXInternal(e, tX);
}

static void setDolmexicaStoryAnimationScaleYInternal(StoryAnimation& e, double tY)
{
	Vector3D scale = getMugenAnimationDrawScale(e.mAnimationElement);
	scale.y = tY;
	setMugenAnimationDrawScale(e.mAnimationElement, scale);

	if (e.mHasShadow) {
		scale.y *= -getDreamStageShadowScaleY();
		setMugenAnimationDrawScale(e.mShadowAnimationElement, scale);
	}
}


void setDolmexicaStoryAnimationScaleY(StoryInstance* tInstance, int tID, double tY)
{
	StoryAnimation& e = tInstance->mStoryAnimations[tID];
	setDolmexicaStoryAnimationScaleYInternal(e, tY);
}

static void setDolmexicaStoryAnimationIsFacingRightInternal(StoryAnimation& e, int tIsFacingRight)
{
	setMugenAnimationFaceDirection(e.mAnimationElement, tIsFacingRight);

	if (e.mHasShadow) {
		setMugenAnimationFaceDirection(e.mShadowAnimationElement, tIsFacingRight);
	}
}


void setDolmexicaStoryAnimationIsFacingRight(StoryInstance* tInstance, int tID, int tIsFacingRight)
{
	StoryAnimation& e = tInstance->mStoryAnimations[tID];
	setDolmexicaStoryAnimationIsFacingRightInternal(e, tIsFacingRight);
}

static void setDolmexicaStoryAnimationAngleInternal(StoryAnimation& e, double tAngle)
{
	const auto newAngle = degreesToRadians(tAngle);
	setMugenAnimationDrawAngle(e.mAnimationElement, newAngle);
	if (e.mHasShadow) {
		setMugenAnimationDrawAngle(e.mShadowAnimationElement, newAngle);
	}
}

void setDolmexicaStoryAnimationAngle(StoryInstance * tInstance, int tID, double tAngle)
{
	StoryAnimation& e = tInstance->mStoryAnimations[tID];
	setDolmexicaStoryAnimationAngleInternal(e, tAngle);
}

static void addDolmexicaStoryAnimationAngleInternal(StoryAnimation& e, double tAngle)
{
	const auto newAngle = degreesToRadians(radiansToDegrees(getMugenAnimationDrawAngle(e.mAnimationElement)) + tAngle);
	setMugenAnimationDrawAngle(e.mAnimationElement, newAngle);
	if (e.mHasShadow) {
		setMugenAnimationDrawAngle(e.mShadowAnimationElement, newAngle);
	}
}

void addDolmexicaStoryAnimationAngle(StoryInstance * tInstance, int tID, double tAngle)
{
	StoryAnimation& e = tInstance->mStoryAnimations[tID];
	addDolmexicaStoryAnimationAngleInternal(e, tAngle);
}

static void setDolmexicaStoryAnimationColorInternal(StoryAnimation& e, const Vector3D& tColor)
{
	setMugenAnimationColor(e.mAnimationElement, tColor.x, tColor.y, tColor.z);
}

void setDolmexicaStoryAnimationColor(StoryInstance * tInstance, int tID, const Vector3D& tColor)
{
	StoryAnimation& e = tInstance->mStoryAnimations[tID];
	setDolmexicaStoryAnimationColorInternal(e, tColor);
}

static void setDolmexicaStoryAnimationOpacityInternal(StoryAnimation& e, double tOpacity)
{
	setMugenAnimationTransparency(e.mAnimationElement, tOpacity);
	if (e.mHasShadow) {
		setMugenAnimationTransparency(e.mShadowAnimationElement, tOpacity);
	}
}

void setDolmexicaStoryAnimationOpacity(StoryInstance * tInstance, int tID, double tOpacity)
{
	StoryAnimation& e = tInstance->mStoryAnimations[tID];
	setDolmexicaStoryAnimationOpacityInternal(e, tOpacity);
}

void addDolmexicaStoryText(StoryInstance* tInstance, int tID, const char * tText, const Vector3DI& tFont, const Position& tBasePosition, const Position& tTextOffset, double tTextBoxWidth)
{
	if (stl_map_contains(tInstance->mStoryTexts, tID)) {
		removeDolmexicaStoryText(tInstance, tID);
	}

	StoryText& e = tInstance->mStoryTexts[tID];
	e.mID = tID;
	const auto z = DOLMEXICA_STORY_TEXT_BASE_Z + tID * 0.1;
	e.mPosition = makePosition(tBasePosition.x, tBasePosition.y, z);
	e.mTextOffset = tTextOffset;

	Position textPosition = vecAdd(e.mPosition, e.mTextOffset);
	e.mTextID = addMugenText(tText, textPosition, tFont.x);
	setMugenTextColor(e.mTextID, getMugenTextColorFromMugenTextColorIndex(tFont.y));
	setMugenTextAlignment(e.mTextID, getMugenTextAlignmentFromMugenAlignmentIndex(tFont.z));

	setMugenTextBuildup(e.mTextID, 1);
	setMugenTextTextBoxWidth(e.mTextID, tTextBoxWidth);
	
	e.mHasBackground = 0;
	e.mHasFace = 0;
	e.mHasContinue = 0;
	e.mHasName = 0;
	e.mGoesToNextState = 0;
	e.mHasFinished = 0;
	e.mIsLockedOnToCharacter = 0;
	e.mIsDisabled = 0;
}

static void unloadDolmexicaStoryText(StoryText* e) {
	removeMugenText(e->mTextID);
	if (e->mHasBackground) {
		if (e->mIsBackgroundAnimationOwned) {
			destroyMugenAnimation(e->mBackgroundAnimation);
		}
		removeMugenAnimation(e->mBackgroundAnimationElement);
	}
	if (e->mHasFace) {
		if (e->mIsFaceAnimationOwned) {
			destroyMugenAnimation(e->mFaceAnimation);
		}
		removeMugenAnimation(e->mFaceAnimationElement);
	}
	if (e->mHasContinue) {
		if (e->mIsContinueAnimationOwned) {
			destroyMugenAnimation(e->mContinueAnimation);
		}
		removeMugenAnimation(e->mContinueAnimationElement);
	}
	if (e->mHasName) {
		removeMugenText(e->mNameID);
	}
}

void removeDolmexicaStoryText(StoryInstance* tInstance, int tID)
{
	StoryText* e = &tInstance->mStoryTexts[tID];
	unloadDolmexicaStoryText(e);
	
	tInstance->mStoryTexts.erase(e->mID);
}

const char* getDolmexicaStoryTextText(StoryInstance * tInstance, int tID)
{
	StoryText* e = &tInstance->mStoryTexts[tID];
	return getMugenTextText(e->mTextID);
}

const char* getDolmexicaStoryTextDisplayedText(StoryInstance * tInstance, int tID)
{
	StoryText* e = &tInstance->mStoryTexts[tID];
	return getMugenTextDisplayedText(e->mTextID);
}

const char* getDolmexicaStoryTextNameText(StoryInstance * tInstance, int tID)
{
	StoryText* e = &tInstance->mStoryTexts[tID];
	if (!e->mHasName) return "";
	return getMugenTextText(e->mNameID);
}

int isDolmexicaStoryTextVisible(StoryInstance * tInstance, int tID)
{
	StoryText* e = &tInstance->mStoryTexts[tID];
	return !e->mIsDisabled;
}

static void setDolmexicaStoryTextBackgroundInternal(StoryInstance* tInstance, int tID, MugenAnimation* tAnimation, int tIsOwned, const Position& tOffset) {
	StoryText* e = &tInstance->mStoryTexts[tID];
	e->mBackgroundOffset = makePosition(tOffset.x, tOffset.y, -2);
	Position p = vecAdd(e->mPosition, e->mBackgroundOffset);
	e->mBackgroundAnimation = tAnimation;
	e->mIsBackgroundAnimationOwned = tIsOwned;
	e->mBackgroundAnimationElement = addMugenAnimation(e->mBackgroundAnimation, &gDolmexicaStoryScreenData.mSprites, p);
	e->mHasBackground = 1;
}

void setDolmexicaStoryTextBackground(StoryInstance* tInstance, int tID, const Vector3DI& tSprite, const Position& tOffset)
{
	setDolmexicaStoryTextBackgroundInternal(tInstance, tID, createOneFrameMugenAnimationForSprite(tSprite.x, tSprite.y), 1, tOffset);
}

void setDolmexicaStoryTextBackground(StoryInstance * tInstance, int tID, int tAnimation, const Position& tOffset)
{
	setDolmexicaStoryTextBackgroundInternal(tInstance, tID, getMugenAnimation(&gDolmexicaStoryScreenData.mAnimations, tAnimation), 0, tOffset);
}

static void setDolmexicaStoryTextFaceInternal(StoryInstance* tInstance, int tID, MugenAnimation* tAnimation, int tIsOwned, const Position& tOffset)
{
	StoryText* e = &tInstance->mStoryTexts[tID];
	e->mFaceOffset = makePosition(tOffset.x, tOffset.y, -1);
	Position p = vecAdd(e->mPosition, e->mFaceOffset);
	p.z = DOLMEXICA_STORY_TEXT_BASE_Z + (tID - 1) * 0.1;
	e->mFaceAnimation = tAnimation;
	e->mIsFaceAnimationOwned = tIsOwned;
	e->mFaceAnimationElement = addMugenAnimation(e->mFaceAnimation, &gDolmexicaStoryScreenData.mSprites, p);
	e->mHasFace = 1;
}

void setDolmexicaStoryTextFace(StoryInstance* tInstance, int tID, const Vector3DI& tSprite, const Position& tOffset)
{
	setDolmexicaStoryTextFaceInternal(tInstance, tID, createOneFrameMugenAnimationForSprite(tSprite.x, tSprite.y), 1, tOffset);
}

void setDolmexicaStoryTextFace(StoryInstance* tInstance, int tID, int tAnimation, const Position& tOffset)
{
	setDolmexicaStoryTextFaceInternal(tInstance, tID, getMugenAnimation(&gDolmexicaStoryScreenData.mAnimations, tAnimation), 0, tOffset);
}

void setDolmexicaStoryTextName(StoryInstance* tInstance, int tID, const char * tText, const Vector3DI& tFont, const Position& tOffset)
{
	StoryText* e = &tInstance->mStoryTexts[tID];
	e->mNameOffset = makePosition(tOffset.x, tOffset.y, 0);
	Position p = vecAdd(e->mPosition, e->mNameOffset);
	e->mNameID = addMugenTextMugenStyle(tText, p, tFont);
	e->mHasName = 1;
}

static void setDolmexicaStoryTextContinueInternal(StoryInstance* tInstance, int tID, MugenAnimation* tAnimation, int tIsOwned, const Position& tOffset)
{
	StoryText* e = &tInstance->mStoryTexts[tID];
	e->mContinueOffset = makePosition(tOffset.x, tOffset.y, -1);
	Position p = vecAdd(e->mPosition, e->mContinueOffset);
	p.z = DOLMEXICA_STORY_TEXT_BASE_Z + (tID - 1) * 0.1;
	e->mContinueAnimation = tAnimation;
	e->mIsContinueAnimationOwned = tIsOwned;
	e->mContinueAnimationElement = addMugenAnimation(e->mContinueAnimation, &gDolmexicaStoryScreenData.mSprites, p);
	setMugenAnimationVisibility(e->mContinueAnimationElement, 0);
	e->mHasContinue = 1;
}

void setDolmexicaStoryTextContinue(StoryInstance* tInstance, int tID, const Vector3DI& tSprite, const Position& tOffset)
{
	setDolmexicaStoryTextContinueInternal(tInstance, tID, createOneFrameMugenAnimationForSprite(tSprite.x, tSprite.y), 1, tOffset);
}

void setDolmexicaStoryTextContinue(StoryInstance* tInstance, int tID, int tAnimation, const Position& tOffset)
{
	setDolmexicaStoryTextContinueInternal(tInstance, tID, getMugenAnimation(&gDolmexicaStoryScreenData.mAnimations, tAnimation), 0, tOffset);
}

static Position getDolmexicaStoryTextBasePosition(StoryInstance* tInstance, int tID) {
	StoryText* e = &tInstance->mStoryTexts[tID];
	return e->mPosition;
}

double getDolmexicaStoryTextBasePositionX(StoryInstance * tInstance, int tID)
{
	return getDolmexicaStoryTextBasePosition(tInstance, tID).x;
}

double getDolmexicaStoryTextBasePositionY(StoryInstance * tInstance, int tID)
{
	return getDolmexicaStoryTextBasePosition(tInstance, tID).y;
}

void setDolmexicaStoryTextBasePosition(StoryInstance* tInstance, int tID, const Position& tPosition)
{
	StoryText* e = &tInstance->mStoryTexts[tID];
	const auto z = DOLMEXICA_STORY_TEXT_BASE_Z + tID * 0.1;
	e->mPosition = makePosition(tPosition.x, tPosition.y, z);

	Position textPosition = e->mPosition + e->mTextOffset;
	setMugenTextPosition(e->mTextID, textPosition);
	if (e->mHasBackground) {	
		setMugenAnimationPosition(e->mBackgroundAnimationElement, e->mPosition + e->mBackgroundOffset);
	}
	if (e->mHasFace) {
		setMugenAnimationPosition(e->mFaceAnimationElement, e->mPosition + e->mFaceOffset);
	}
	if (e->mHasContinue) {
		setMugenAnimationPosition(e->mContinueAnimationElement, e->mPosition + e->mContinueOffset);
	}
	if (e->mHasName) {
		setMugenTextPosition(e->mNameID, e->mPosition + e->mNameOffset);
	}
}

void setDolmexicaStoryTextText(StoryInstance* tInstance, int tID, const char * tText)
{
	StoryText* e = &tInstance->mStoryTexts[tID];
	changeMugenText(e->mTextID, tText);
	setMugenTextBuildup(e->mTextID, 1);

	if (e->mIsDisabled) {
		if (e->mHasBackground) {
			setMugenAnimationVisibility(e->mBackgroundAnimationElement, 1);
		}
		if (e->mHasFace) {
			setMugenAnimationVisibility(e->mFaceAnimationElement, 1);
		}
		if (e->mHasName) {
			setMugenTextVisibility(e->mNameID, 1);
		}

		e->mIsDisabled = 0;
	}

	e->mHasFinished = 0;
}

void setDolmexicaStoryTextTextOffset(StoryInstance * tInstance, int tID, const Position& tOffset)
{
	StoryText* e = &tInstance->mStoryTexts[tID];
	e->mTextOffset = makePosition(tOffset.x, tOffset.y, 0);
	setDolmexicaStoryTextBasePosition(tInstance, tID, e->mPosition);
}

void setDolmexicaStoryTextBackgroundSprite(StoryInstance* tInstance, int tID, const Vector3DI& tSprite)
{
	StoryText* e = &tInstance->mStoryTexts[tID];
	destroyMugenAnimation(e->mBackgroundAnimation);
	e->mBackgroundAnimation = createOneFrameMugenAnimationForSprite(tSprite.x, tSprite.y);
	changeMugenAnimation(e->mBackgroundAnimationElement, e->mBackgroundAnimation);
}

void setDolmexicaStoryTextBackgroundOffset(StoryInstance* tInstance, int tID, const Position& tOffset)
{
	StoryText* e = &tInstance->mStoryTexts[tID];
	e->mBackgroundOffset = makePosition(tOffset.x, tOffset.y, -2);
	setMugenAnimationPosition(e->mBackgroundAnimationElement, vecAdd(getMugenTextPosition(e->mTextID), e->mBackgroundOffset));
}

void setDolmexicaStoryTextFaceSprite(StoryInstance* tInstance, int tID, const Vector3DI& tSprite)
{
	StoryText* e = &tInstance->mStoryTexts[tID];
	destroyMugenAnimation(e->mFaceAnimation);
	e->mFaceAnimation = createOneFrameMugenAnimationForSprite(tSprite.x, tSprite.y);
	changeMugenAnimation(e->mFaceAnimationElement, e->mFaceAnimation);
}

void setDolmexicaStoryTextFaceOffset(StoryInstance* tInstance, int tID, const Position& tOffset)
{
	StoryText* e = &tInstance->mStoryTexts[tID];
	e->mFaceOffset = makePosition(tOffset.x, tOffset.y, -1);
	setMugenAnimationPosition(e->mFaceAnimationElement, vecAdd(getMugenTextPosition(e->mTextID), e->mFaceOffset));
}

void setDolmexicaStoryTextContinueAnimation(StoryInstance* tInstance, int tID, int tAnimation)
{
	StoryText* e = &tInstance->mStoryTexts[tID];
	changeMugenAnimation(e->mContinueAnimationElement, getMugenAnimation(&gDolmexicaStoryScreenData.mAnimations, tAnimation));
}

void setDolmexicaStoryTextContinueOffset(StoryInstance* tInstance, int tID, const Position& tOffset)
{
	StoryText* e = &tInstance->mStoryTexts[tID];
	e->mContinueOffset = makePosition(tOffset.x, tOffset.y, -1);
	setMugenAnimationPosition(e->mContinueAnimationElement, vecAdd(getMugenTextPosition(e->mTextID), e->mContinueOffset));
}

void setDolmexicaStoryTextNameText(StoryInstance * tInstance, int tID, const char * tText)
{
	StoryText* e = &tInstance->mStoryTexts[tID];
	changeMugenText(e->mNameID, tText);
}

void setDolmexicaStoryTextNameFont(StoryInstance * tInstance, int tID, const Vector3DI& tFont)
{
	StoryText* e = &tInstance->mStoryTexts[tID];
	setMugenTextFont(e->mNameID, tFont.x);
	setMugenTextColor(e->mNameID, getMugenTextColorFromMugenTextColorIndex(tFont.y));
	setMugenTextAlignment(e->mNameID, getMugenTextAlignmentFromMugenAlignmentIndex(tFont.z));
}

void setDolmexicaStoryTextNameOffset(StoryInstance * tInstance, int tID, const Position& tOffset)
{
	StoryText* e = &tInstance->mStoryTexts[tID];
	e->mNameOffset = makePosition(tOffset.x, tOffset.y, e->mNameOffset.z);
	setDolmexicaStoryTextBasePosition(tInstance, tID, e->mPosition);
}

int getDolmexicaStoryTextNextState(StoryInstance * tInstance, int tID)
{
	StoryText* e = &tInstance->mStoryTexts[tID];
	if (!e->mGoesToNextState) return 0;
	else return e->mNextState;
}

void setDolmexicaStoryTextNextState(StoryInstance* tInstance, int tID, int tNextState)
{
	StoryText* e = &tInstance->mStoryTexts[tID];
	e->mNextState = tNextState;
	e->mGoesToNextState = 1;
}

static void setDolmexicaStoryTextLockToCharacterInternal(StoryInstance * tInstance, StoryInstance* tCharacterInstance, int tID, int tCharacterID, const Position& tOffset) {
	StoryText* e = &tInstance->mStoryTexts[tID];
	StoryCharacter& character = tCharacterInstance->mStoryCharacters[tCharacterID];

	e->mLockCharacterPositionReference = getMugenAnimationPositionReference(character.mAnimation.mAnimationElement);
	e->mLockCharacterIsBoundToStage = character.mAnimation.mIsBoundToStage;
	e->mLockOffset = tOffset;
	e->mIsLockedOnToCharacter = 1;
	updateLockedTextPosition(tInstance, e);
}

void setDolmexicaStoryTextLockToCharacter(StoryInstance * tInstance, int tID, int tCharacterID, const Position& tOffset)
{
	setDolmexicaStoryTextLockToCharacterInternal(tInstance, tInstance, tID, tCharacterID, tOffset);
}

void setDolmexicaStoryTextLockToCharacter(StoryInstance * tInstance, int tID, int tCharacterID, const Position& tOffset, int tHelperID)
{
	setDolmexicaStoryTextLockToCharacterInternal(tInstance, &gDolmexicaStoryScreenData.mHelperInstances[tHelperID], tID, tCharacterID, tOffset);
}

void setDolmexicaStoryTextInactive(StoryInstance * tInstance, int tID)
{
	StoryText* e = &tInstance->mStoryTexts[tID];
	
	char text[2];
	text[0] = '\0';
	changeMugenText(e->mTextID, text);
	if (e->mHasBackground) {
		setMugenAnimationVisibility(e->mBackgroundAnimationElement, 0);
	}
	if (e->mHasFace) {
		setMugenAnimationVisibility(e->mFaceAnimationElement, 0);
	}
	if (e->mHasContinue) {
		setMugenAnimationVisibility(e->mContinueAnimationElement, 0);
	}
	if (e->mHasName) {
		setMugenTextVisibility(e->mNameID, 0);
	}

	e->mIsDisabled = 1;
}

int isDolmexicaStoryTextBuiltUp(StoryInstance * tInstance, int tID)
{
	StoryText* e = &tInstance->mStoryTexts[tID];
	return isMugenTextBuiltUp(e->mTextID);
}

void setDolmexicaStoryTextBuiltUp(StoryInstance * tInstance, int tID)
{
	StoryText* e = &tInstance->mStoryTexts[tID];
	setMugenTextBuiltUp(e->mTextID);
}

void setDolmexicaStoryTextPositionX(StoryInstance * tInstance, int tID, double tX)
{
	Position p = getDolmexicaStoryTextBasePosition(tInstance, tID);
	p.x = tX;
	setDolmexicaStoryTextBasePosition(tInstance, tID, p);
}

void setDolmexicaStoryTextPositionY(StoryInstance * tInstance, int tID, double tY)
{
	Position p = getDolmexicaStoryTextBasePosition(tInstance, tID);
	p.y = tY;
	setDolmexicaStoryTextBasePosition(tInstance, tID, p);
}

void addDolmexicaStoryTextPositionX(StoryInstance * tInstance, int tID, double tX)
{
	Position p = getDolmexicaStoryTextBasePosition(tInstance, tID);
	p.x += tX;
	setDolmexicaStoryTextBasePosition(tInstance, tID, p);
}

void addDolmexicaStoryTextPositionY(StoryInstance * tInstance, int tID, double tY)
{
	Position p = getDolmexicaStoryTextBasePosition(tInstance, tID);
	p.y += tY;
	setDolmexicaStoryTextBasePosition(tInstance, tID, p);
}

void setDolmexicaStoryIDName(StoryInstance * tInstance, int tID, const std::string& tName)
{
	tInstance->mTextNames[tName] = tID;
}

int getDolmexicaStoryTextIDFromName(StoryInstance * tInstance, const std::string& tName)
{
	return tInstance->mTextNames[tName];
}

void changeDolmexicaStoryState(StoryInstance* tInstance, int tNextState)
{
	changeDreamHandledStateMachineState(tInstance->mStateMachineID, tNextState);
	setDreamRegisteredStateTimeInState(tInstance->mStateMachineID, 0);
}

void changeDolmexicaStoryStateOutsideStateHandler(StoryInstance* tInstance, int tNextState)
{
	changeDreamHandledStateMachineState(tInstance->mStateMachineID, tNextState);
	setDreamRegisteredStateTimeInState(tInstance->mStateMachineID, -1);
}

void endDolmexicaStoryboard(StoryInstance* /*tInstance*/, int tNextStoryState)
{
	storyModeOverCB(tNextStoryState);
}

int getDolmexicaStoryTimeInState(StoryInstance* tInstance)
{
	return getDreamRegisteredStateTimeInState(tInstance->mStateMachineID);
}

int getDolmexicaStoryStateNumber(StoryInstance* tInstance)
{
	return getDreamRegisteredStateState(tInstance->mStateMachineID);
}

static int getDolmexicaStoryAnimationTimeLeftInternal(StoryAnimation& e)
{
	return getMugenAnimationRemainingAnimationTime(e.mAnimationElement);
}


int getDolmexicaStoryAnimationTimeLeft(StoryInstance* tInstance, int tID)
{
	if (!stl_map_contains(tInstance->mStoryAnimations, tID)) {
		logWarningFormat("Querying time left for non-existing story animation %d. Defaulting to infinite.", tID);
		return INF;
	}
	StoryAnimation& e = tInstance->mStoryAnimations[tID];
	return getDolmexicaStoryAnimationTimeLeftInternal(e);
}

static double getDolmexicaStoryAnimationPositionXInternal(StoryAnimation& e) {
	return getMugenAnimationPosition(e.mAnimationElement).x;
}

double getDolmexicaStoryAnimationPositionX(StoryInstance* tInstance, int tID)
{
	if (!stl_map_contains(tInstance->mStoryAnimations, tID)) {
		logWarningFormat("Querying for non-existing story animation %d. Defaulting to 0.", tID);
		return 0;
	}
	StoryAnimation& e = tInstance->mStoryAnimations[tID];
	return getDolmexicaStoryAnimationPositionXInternal(e);
}

static double getDolmexicaStoryAnimationPositionYInternal(StoryAnimation& e) {
	return getMugenAnimationPosition(e.mAnimationElement).y;
}

double getDolmexicaStoryAnimationPositionY(StoryInstance* tInstance, int tID)
{
	if (!stl_map_contains(tInstance->mStoryAnimations, tID)) {
		logWarningFormat("Querying for non-existing story animation %d. Defaulting to 0.", tID);
		return 0;
	}
	StoryAnimation& e = tInstance->mStoryAnimations[tID];
	return getDolmexicaStoryAnimationPositionYInternal(e);
}

void addDolmexicaStoryCharacter(StoryInstance* tInstance, int tID, const char* tName, int tPreferredPalette, int tAnimation, const Position& tPosition)
{
	if (stl_map_contains(tInstance->mStoryCharacters, tID)) {
		logWarningFormat("Trying to create existing character %d. Erasing previous.", tID);
		removeDolmexicaStoryCharacter(tInstance, tID);
	}

	StoryCharacter e;
	e.mName = tName;

	char file[1024];
	char path[1024];
	char fullPath[1024];
	char name[100];
	getCharacterSelectNamePath(tName, fullPath);
	getPathToFile(path, fullPath);
	MugenDefScript script;
	loadMugenDefScript(&script, fullPath);
	

	char palettePath[1024];
	sprintf(name, "pal%d", tPreferredPalette);
	getMugenDefStringOrDefault(file, &script, "Files", name, "");
	int hasPalettePath = strcmp("", file);
	sprintf(palettePath, "%s%s", path, file);
	getMugenDefStringOrDefault(file, &script, "Files", "sprite", "");
	sprintf(fullPath, "%s%s", path, file);

	e.mSprites = loadMugenSpriteFile(fullPath, hasPalettePath, palettePath);

	getMugenDefStringOrDefault(file, &script, "Files", "anim", "");
	sprintf(fullPath, "%s%s", path, file);
	if (!isFile(fullPath)) {
		logWarningFormat("Unable to load animation file %s from def file %s. Ignoring.", fullPath, tName);
		return;
	}
	e.mAnimations = loadMugenAnimationFile(fullPath);

	unloadMugenDefScript(script);

	tInstance->mStoryCharacters[tID] = e;

	const auto z = DOLMEXICA_STORY_ANIMATION_BASE_Z + tID * 0.1;
	initDolmexicaStoryAnimation(tInstance->mStoryCharacters[tID].mAnimation, tID, tAnimation, makePosition(tPosition.x, tPosition.y, z), &tInstance->mStoryCharacters[tID].mSprites, &tInstance->mStoryCharacters[tID].mAnimations);

	if (isInDevelopMode()) {
		addDebugDolmexicaStoryCharacterAnimation(e.mName.c_str(), tAnimation);
	}
}

void removeDolmexicaStoryCharacter(StoryInstance* tInstance, int tID)
{
	StoryCharacter& e = tInstance->mStoryCharacters[tID];
	unloadDolmexicaStoryAnimation(e.mAnimation);
	unloadMugenAnimationFile(&e.mAnimations);
	unloadMugenSpriteFile(&e.mSprites);
	tInstance->mStoryCharacters.erase(tID);
}

int getDolmexicaStoryCharacterIsBoundToStage(StoryInstance* tInstance, int tID)
{
	StoryCharacter& e = tInstance->mStoryCharacters[tID];
	return e.mAnimation.mIsBoundToStage;
}

void setDolmexicaStoryCharacterBoundToStage(StoryInstance* tInstance, int tID, int tIsBoundToStage)
{
	StoryCharacter& e = tInstance->mStoryCharacters[tID];
	setDolmexicaStoryAnimationBoundToStageInternal(e.mAnimation, tIsBoundToStage);
}

int getDolmexicaStoryCharacterHasShadow(StoryInstance* tInstance, int tID)
{
	StoryCharacter& e = tInstance->mStoryCharacters[tID];
	return e.mAnimation.mHasShadow;
}

void setDolmexicaStoryCharacterShadow(StoryInstance* tInstance, int tID, double tBasePositionY)
{
	StoryCharacter& e = tInstance->mStoryCharacters[tID];
	setDolmexicaStoryAnimationShadowInternal(e.mAnimation, tBasePositionY, &e.mSprites, &e.mAnimations);
}

int getDolmexicaStoryCharacterAnimation(StoryInstance * tInstance, int tID)
{
	StoryCharacter& e = tInstance->mStoryCharacters[tID];
	return getMugenAnimationAnimationNumber(e.mAnimation.mAnimationElement);
}

void changeDolmexicaStoryCharacterAnimation(StoryInstance* tInstance, int tID, int tAnimation)
{
	StoryCharacter& e = tInstance->mStoryCharacters[tID];
	if (isInDevelopMode()) {
		addDebugDolmexicaStoryCharacterAnimation(e.mName.c_str(), tAnimation);
	}
	changeDolmexicaStoryAnimationInternal(e.mAnimation, tAnimation, &e.mAnimations);
}

double getDolmexicaStoryCharacterPositionX(StoryInstance * tInstance, int tID)
{
	StoryCharacter& e = tInstance->mStoryCharacters[tID];
	return getDolmexicaStoryAnimationPositionXInternal(e.mAnimation);
}

double getDolmexicaStoryCharacterPositionY(StoryInstance * tInstance, int tID)
{
	StoryCharacter& e = tInstance->mStoryCharacters[tID];
	return getDolmexicaStoryAnimationPositionYInternal(e.mAnimation);
}

void setDolmexicaStoryCharacterPositionX(StoryInstance* tInstance, int tID, double tX)
{
	StoryCharacter& e = tInstance->mStoryCharacters[tID];
	setDolmexicaStoryAnimationPositionXInternal(e.mAnimation, tX);
}

void setDolmexicaStoryCharacterPositionY(StoryInstance* tInstance, int tID, double tY)
{
	StoryCharacter& e = tInstance->mStoryCharacters[tID];
	setDolmexicaStoryAnimationPositionYInternal(e.mAnimation, tY);
}

void addDolmexicaStoryCharacterPositionX(StoryInstance* tInstance, int tID, double tX)
{
	StoryCharacter& e = tInstance->mStoryCharacters[tID];
	addDolmexicaStoryAnimationPositionXInternal(e.mAnimation, tX);
}

void addDolmexicaStoryCharacterPositionY(StoryInstance* tInstance, int tID, double tY)
{
	StoryCharacter& e = tInstance->mStoryCharacters[tID];
	addDolmexicaStoryAnimationPositionYInternal(e.mAnimation, tY);
}

void setDolmexicaStoryCharacterScaleX(StoryInstance* tInstance, int tID, double tX)
{
	StoryCharacter& e = tInstance->mStoryCharacters[tID];
	setDolmexicaStoryAnimationScaleXInternal(e.mAnimation, tX);
}

void setDolmexicaStoryCharacterScaleY(StoryInstance* tInstance, int tID, double tY)
{
	StoryCharacter& e = tInstance->mStoryCharacters[tID];
	setDolmexicaStoryAnimationScaleYInternal(e.mAnimation, tY);
}

void setDolmexicaStoryCharacterIsFacingRight(StoryInstance* tInstance, int tID, int tIsFacingRight)
{
	StoryCharacter& e = tInstance->mStoryCharacters[tID];
	setDolmexicaStoryAnimationIsFacingRightInternal(e.mAnimation, tIsFacingRight);
}

void setDolmexicaStoryCharacterColor(StoryInstance * tInstance, int tID, const Vector3D& tColor)
{
	StoryCharacter& e = tInstance->mStoryCharacters[tID];
	setDolmexicaStoryAnimationColorInternal(e.mAnimation, tColor);
}

void setDolmexicaStoryCharacterOpacity(StoryInstance * tInstance, int tID, double tOpacity)
{
	StoryCharacter& e = tInstance->mStoryCharacters[tID];
	setDolmexicaStoryAnimationOpacityInternal(e.mAnimation, tOpacity);
}

void setDolmexicaStoryCharacterAngle(StoryInstance * tInstance, int tID, double tAngle)
{
	StoryCharacter& e = tInstance->mStoryCharacters[tID];
	setDolmexicaStoryAnimationAngleInternal(e.mAnimation, tAngle);
}

void addDolmexicaStoryCharacterAngle(StoryInstance * tInstance, int tID, double tAngle)
{
	StoryCharacter& e = tInstance->mStoryCharacters[tID];
	addDolmexicaStoryAnimationAngleInternal(e.mAnimation, tAngle);
}

int getDolmexicaStoryCharacterTimeLeft(StoryInstance * tInstance, int tID)
{
	if (!stl_map_contains(tInstance->mStoryCharacters, tID)) {
		logWarningFormat("Querying time left for non-existing story animation %d. Defaulting to infinite.", tID);
		return INF;
	}
	StoryCharacter& e = tInstance->mStoryCharacters[tID];
	return getDolmexicaStoryAnimationTimeLeftInternal(e.mAnimation);
}

int getDolmexicaStoryIntegerVariable(StoryInstance * tInstance, int tID)
{
	return tInstance->mIntVars[tID];
}

void setDolmexicaStoryIntegerVariable(StoryInstance * tInstance, int tID, int tValue)
{
	tInstance->mIntVars[tID] = tValue;
}

void addDolmexicaStoryIntegerVariable(StoryInstance * tInstance, int tID, int tValue)
{
	tInstance->mIntVars[tID] += tValue;
}

double getDolmexicaStoryFloatVariable(StoryInstance * tInstance, int tID)
{
	return tInstance->mFloatVars[tID];
}

void setDolmexicaStoryFloatVariable(StoryInstance * tInstance, int tID, double tValue)
{
	tInstance->mFloatVars[tID] = tValue;
}

void addDolmexicaStoryFloatVariable(StoryInstance * tInstance, int tID, double tValue)
{
	tInstance->mFloatVars[tID] += tValue;
}

std::string getDolmexicaStoryStringVariable(StoryInstance * tInstance, int tID)
{
	return tInstance->mStringVars[tID];
}

void setDolmexicaStoryStringVariable(StoryInstance * tInstance, int tID, const std::string& tValue)
{
	tInstance->mStringVars[tID] = tValue;
}

void addDolmexicaStoryStringVariable(StoryInstance * tInstance, int tID, const std::string& tValue)
{
	tInstance->mStringVars[tID] = tInstance->mStringVars[tID] + tValue;
}

void addDolmexicaStoryStringVariable(StoryInstance * tInstance, int tID, int tValue)
{
	tInstance->mStringVars[tID][0] += (char)tValue;
}

StoryInstance* getDolmexicaStoryRootInstance()
{
	return &gDolmexicaStoryScreenData.mHelperInstances[-1];
}

StoryInstance* getDolmexicaStoryInstanceParent(StoryInstance * tInstance)
{
	return tInstance->mParent;
}

StoryInstance * getDolmexicaStoryHelperInstance(int tID)
{
	return &gDolmexicaStoryScreenData.mHelperInstances[tID];
}

void addDolmexicaStoryHelper(int tID, int tState, StoryInstance* tParent)
{
	StoryInstance e;
	gDolmexicaStoryScreenData.mHelperInstances[tID] = e;
	initStoryInstance(gDolmexicaStoryScreenData.mHelperInstances[tID]);
	gDolmexicaStoryScreenData.mHelperInstances[tID].mParent = tParent;

	changeDolmexicaStoryStateOutsideStateHandler(&gDolmexicaStoryScreenData.mHelperInstances[tID], tState);
	updateDreamSingleStateMachineByID(gDolmexicaStoryScreenData.mHelperInstances[tID].mStateMachineID);
}

int getDolmexicaStoryGetHelperAmount(int tID)
{
	return stl_map_contains(gDolmexicaStoryScreenData.mHelperInstances, tID);
}

void removeDolmexicaStoryHelper(int tID)
{
	unloadStoryInstance(gDolmexicaStoryScreenData.mHelperInstances[tID]);
	gDolmexicaStoryScreenData.mHelperInstances.erase(tID);
}

void destroyDolmexicaStoryHelper(StoryInstance * tInstance)
{
	tInstance->mIsScheduledForDeletion = 1;
}

int getDolmexicaStoryIDFromString(const char * tString, StoryInstance * tInstance)
{
	int id;
	if (!strcmp("", tString)) {
		id = 1;
	}
	else {
		char* p;
		id = (int)strtol(tString, &p, 10);
		if (p == tString) {
			id = getDolmexicaStoryTextIDFromName(tInstance, tString);
		}
	}
	return id;
}

void playDolmexicaStoryMusic(const string& tPath) {
	stopMusic();
	if (isMugenBGMMusicPath(tPath.data(), gDolmexicaStoryScreenData.mPath)) {
		playMugenBGMMusicPath(tPath.data(), gDolmexicaStoryScreenData.mPath, 1);
	}
}

void stopDolmexicaStoryMusic()
{
	stopMusic();
}

void pauseDolmexicaStoryMusic()
{
	pauseMusic();
}

void resumeDolmexicaStoryMusic()
{
	resumeMusic();
}

void setDolmexicaStoryCameraFocusX(double x)
{
	setDreamMugenStageHandlerCameraEffectPositionX(x);
}

void setDolmexicaStoryCameraFocusY(double y)
{
	setDreamMugenStageHandlerCameraEffectPositionY(y);
}

void setDolmexicaStoryCameraZoom(double tScale)
{
	setDreamMugenStageHandlerCameraZoom(tScale);
}

int getDolmexicaStoryCoordinateP()
{
	return COORD_P;
}
