#include "dolmexicastoryscreen.h"

#include <string.h>

#include <prism/log.h>
#include <prism/math.h>
#include <prism/actorhandler.h>
#include <prism/mugenanimationhandler.h>
#include <prism/mugentexthandler.h>
#include <prism/input.h>
#include <prism/stlutil.h>

#include "mugenassignmentevaluator.h"
#include "mugenstatecontrollers.h"
#include "mugenstatehandler.h"
#include "titlescreen.h"
#include "stage.h"
#include "mugenstagehandler.h"
#include "storymode.h"
#include "characterselectscreen.h"
#include "mugencommandhandler.h"

using namespace std;

#define COORD_P 240


static struct {
	char mPath[1024];
	MugenSpriteFile mSprites;
	MugenAnimations mAnimations;

	int mHasCommands;
	DreamMugenCommands mCommands;
	int mCommandID;

	DreamMugenStates mStoryStates;

	map<int, StoryInstance> mHelperInstances;
} gData;

static void loadStoryFilesFromScript(MugenDefScript* tScript) {
	char pathToFile[1024];
	char path[1024];
	char fullPath[1024];


	getPathToFile(pathToFile, gData.mPath);

	getMugenDefStringOrDefault(path, tScript, "Info", "spr", "");
	sprintf(fullPath, "%s%s", pathToFile, path);
	gData.mSprites = loadMugenSpriteFileWithoutPalette(fullPath);

	getMugenDefStringOrDefault(path, tScript, "Info", "anim", "");
	sprintf(fullPath, "%s%s", pathToFile, path);
	gData.mAnimations = loadMugenAnimationFile(fullPath);

	getMugenDefStringOrDefault(path, tScript, "Info", "cmd", "");
	sprintf(fullPath, "%s%s", pathToFile, path);
	gData.mHasCommands = isFile(fullPath);
	if (gData.mHasCommands) {
		gData.mCommands = loadDreamMugenCommandFile(fullPath);
		gData.mCommandID = registerDreamMugenCommands(0, &gData.mCommands);
	}

	getMugenDefStringOrDefault(path, tScript, "Info", "stage", "");
	sprintf(fullPath, "assets/%s", path);
	char dummyMusicPath[2];
	*dummyMusicPath = '\0'; // TODO: story music
	setDreamStageMugenDefinition(fullPath, dummyMusicPath);

	for (int i = 0; i < 10; i++) {
		char name[20];
		
		sprintf(name, "helper%d", i);
		getMugenDefStringOrDefault(path, tScript, "Info", name, "");
		sprintf(fullPath, "%s%s", pathToFile, path);
		if (!isFile(fullPath)) continue;

		loadDreamMugenStateDefinitionsFromFile(&gData.mStoryStates, fullPath);
	}

}

static void initStoryInstance(StoryInstance& e){
	e.mStateMachineID = registerDreamMugenStoryStateMachine(&gData.mStoryStates, &e);
	e.mStoryAnimations.clear();
	e.mStoryTexts = new_int_map();
	e.mStoryCharacters.clear();

	e.mIntVars.clear();
	e.mFloatVars.clear();
	e.mStringVars.clear();
}

static void loadStoryScreen() {
	setStateMachineHandlerToStory();

	setupDreamStoryAssignmentEvaluator();
	setupDreamMugenStoryStateControllerHandler();
	instantiateActor(getDreamMugenStateHandler());
	instantiateActor(getDreamMugenCommandHandler());


	gData.mStoryStates = createEmptyMugenStates();
	loadDreamMugenStateDefinitionsFromFile(&gData.mStoryStates, gData.mPath);

	MugenDefScript script = loadMugenDefScript(gData.mPath);
	loadStoryFilesFromScript(&script);
	unloadMugenDefScript(script);


	gData.mHelperInstances.clear();
	StoryInstance root;
	gData.mHelperInstances[-1] = root;
	initStoryInstance(gData.mHelperInstances[-1]);

	instantiateActor(getDreamStageBP());
	setDreamStageNoAutomaticCameraMovement();
}

static void unloadStoryScreen() {
	gData.mHelperInstances.clear();
}

static void unloadDolmexicaStoryText(StoryText* e);
static void changeDolmexicaStoryStateOutsideStateHandler(StoryInstance* e, int tNextState);

static int updateSingleTextInput(StoryInstance* tInstance, StoryText* e) {
	if (e->mHasFinished) return 0;

	if (hasPressedAFlank()) {
		if (isMugenTextBuiltUp(e->mTextID)) {
			if (e->mGoesToNextState) {
				changeDolmexicaStoryStateOutsideStateHandler(tInstance, e->mNextState);
			}
			e->mGoesToNextState = 0;
			e->mHasFinished = 1;
			setDolmexicaStoryTextInactive(tInstance, e->mID);
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
	setDolmexicaStoryTextBasePosition(tInstance, e->mID, p);
}

static int updateSingleText(void* tCaller, void* tData) {
	StoryInstance* instance = (StoryInstance*)tCaller;
	StoryText* e = (StoryText*)tData;
	if (e->mIsDisabled) return 0;

	updateLockedTextPosition(instance, e);
	return updateSingleTextInput(instance, e);
}

static void updateTexts(StoryInstance& e) {
	int_map_remove_predicate(&e.mStoryTexts, updateSingleText, &e);
}

static int updateSingleAnimation(void* tCaller, StoryAnimation& e) {
	return e.mIsDeleted;
}

static void updateAnimations(StoryInstance& e) {
	stl_int_map_remove_predicate(e.mStoryAnimations, updateSingleAnimation);
}

static void updateSingleInstance(void* tCaller, StoryInstance& tInstance) {
	updateTexts(tInstance);
	updateAnimations(tInstance);
}

static void updateStoryScreen() {
	stl_int_map_map(gData.mHelperInstances, updateSingleInstance);
}

static Screen gDolmexicaStoryScreen;

Screen* getDolmexicaStoryScreen() {
	gDolmexicaStoryScreen = makeScreen(loadStoryScreen, updateStoryScreen, NULL, unloadStoryScreen);
	return &gDolmexicaStoryScreen;
};

void setDolmexicaStoryScreenFile(char * tPath)
{
	strcpy(gData.mPath, tPath);
}

int isStoryCommandActive(char* tCommand)
{
	if (!gData.mHasCommands) return 0;
	else return isDreamCommandActive(gData.mCommandID, tCommand);
}

static void initDolmexicaStoryAnimation(StoryAnimation& e, int tID, int tAnimationNumber, Position tPosition, MugenSpriteFile* mSprites, MugenAnimations* tAnimations) {
	e.mID = tID;
	e.mAnimationID = addMugenAnimation(getMugenAnimation(tAnimations, tAnimationNumber), mSprites, tPosition);
	e.mHasShadow = 0;
	e.mIsDeleted = 0;
}

void addDolmexicaStoryAnimation(StoryInstance* tInstance, int tID, int tAnimation, Position tPosition)
{
	if (stl_map_contains(tInstance->mStoryAnimations, tID)) {
		removeDolmexicaStoryAnimation(tInstance, tID);
	}
	tPosition.z = 30 + tID;

	StoryAnimation e;
	tInstance->mStoryAnimations[e.mID] = e;
	initDolmexicaStoryAnimation(tInstance->mStoryAnimations[e.mID], tID, tAnimation, tPosition, &gData.mSprites, &gData.mAnimations);
}

static void unloadDolmexicaStoryAnimation(StoryAnimation& e) {
	removeMugenAnimation(e.mAnimationID);
	if (e.mHasShadow) {
		removeMugenAnimation(e.mShadowAnimationID);
	}

}

void removeDolmexicaStoryAnimation(StoryInstance* tInstance, int tID)
{
	StoryAnimation& e = tInstance->mStoryAnimations[tID];
	unloadDolmexicaStoryAnimation(e);
	tInstance->mStoryAnimations.erase(e.mID);
}

static void storyAnimationOverCB(void* tCaller) {
	StoryAnimation* e = (StoryAnimation*)tCaller;
	e->mIsDeleted = 1;
}

void setDolmexicaStoryAnimationLooping(StoryInstance* tInstance, int tID, int tIsLooping)
{
	StoryAnimation& e = tInstance->mStoryAnimations[tID];
	if (!tIsLooping) {
		setMugenAnimationNoLoop(e.mAnimationID);
		if (e.mHasShadow) {
			setMugenAnimationNoLoop(e.mShadowAnimationID);
		}

		setMugenAnimationCallback(e.mAnimationID, storyAnimationOverCB, &e);
	}
}

static void setDolmexicaStoryAnimationBoundToStageInternal(StoryAnimation& e, int tIsBoundToStage)
{
	if (tIsBoundToStage) {
		Position* p = getMugenAnimationPositionReference(e.mAnimationID);
		*p = vecAdd(*p, getDreamStageCoordinateSystemOffset(COORD_P));
		*p = vecAdd(*p, makePosition(160, 0, 0));
		setMugenAnimationCameraPositionReference(e.mAnimationID, getDreamMugenStageHandlerCameraPositionReference());
		if (e.mHasShadow) {
			p = getMugenAnimationPositionReference(e.mShadowAnimationID);
			*p = vecAdd(*p, getDreamStageCoordinateSystemOffset(COORD_P));
			*p = vecAdd(*p, makePosition(160, 0, 0));
			setMugenAnimationCameraPositionReference(e.mShadowAnimationID, getDreamMugenStageHandlerCameraPositionReference());
		}
	}
}


void setDolmexicaStoryAnimationBoundToStage(StoryInstance* tInstance, int tID, int tIsBoundToStage)
{
	StoryAnimation& e = tInstance->mStoryAnimations[tID];
	setDolmexicaStoryAnimationBoundToStageInternal(e, tIsBoundToStage);
}

static void setDolmexicaStoryAnimationPositionXInternal(StoryAnimation& e, double tX);
static void setDolmexicaStoryAnimationPositionYInternal(StoryAnimation& e, double tY);


static void setDolmexicaStoryAnimationShadowInternal(StoryAnimation& e, double tBasePositionY, MugenSpriteFile* tSprites, MugenAnimations* tAnimations) {
	e.mHasShadow = 1;
	e.mShadowBasePositionY = tBasePositionY;
	Position pos = getMugenAnimationPosition(e.mAnimationID);
	pos.z = 30; // TODO: #define
	e.mShadowAnimationID = addMugenAnimation(getMugenAnimation(tAnimations, getMugenAnimationAnimationNumber(e.mAnimationID)), tSprites, pos);
	setMugenAnimationDrawScale(e.mShadowAnimationID, makePosition(1, -getDreamStageShadowScaleY(), 1));
	Vector3D color = getDreamStageShadowColor();
	(void)color; // TODO: proper shadow color
	setMugenAnimationColor(e.mShadowAnimationID, 0, 0, 0); // TODO: proper shadow color
	setMugenAnimationTransparency(e.mShadowAnimationID, getDreamStageShadowTransparency());
	setMugenAnimationFaceDirection(e.mShadowAnimationID, getMugenAnimationIsFacingRight(e.mShadowAnimationID));

	setDolmexicaStoryAnimationPositionXInternal(e, pos.x);
	setDolmexicaStoryAnimationPositionYInternal(e, pos.y);
}

// TODO: put together with playerdefinition shadow and improve in general
void setDolmexicaStoryAnimationShadow(StoryInstance* tInstance, int tID, double tBasePositionY)
{
	StoryAnimation& e = tInstance->mStoryAnimations[tID];
	setDolmexicaStoryAnimationShadowInternal(e, tBasePositionY, &gData.mSprites, &gData.mAnimations);
}

static void changeDolmexicaStoryAnimationInternal(StoryAnimation& e, int tAnimation, MugenAnimations* tAnimations)
{
	changeMugenAnimation(e.mAnimationID, getMugenAnimation(tAnimations, tAnimation));
	if (e.mHasShadow) {
		changeMugenAnimation(e.mShadowAnimationID, getMugenAnimation(tAnimations, tAnimation));
	}
}

void changeDolmexicaStoryAnimation(StoryInstance* tInstance, int tID, int tAnimation)
{
	StoryAnimation& e = tInstance->mStoryAnimations[tID];
	changeDolmexicaStoryAnimationInternal(e, tAnimation, &gData.mAnimations);
}

static void setDolmexicaStoryAnimationPositionXInternal(StoryAnimation& e, double tX)
{
	Position pos = getMugenAnimationPosition(e.mAnimationID);
	pos.x = tX;
	setMugenAnimationPosition(e.mAnimationID, pos);
	if (e.mHasShadow) {
		pos = getMugenAnimationPosition(e.mShadowAnimationID);
		pos.x = tX;
		setMugenAnimationPosition(e.mShadowAnimationID, pos);
	}
}

void setDolmexicaStoryAnimationPositionX(StoryInstance* tInstance, int tID, double tX)
{
	StoryAnimation& e = tInstance->mStoryAnimations[tID];
	setDolmexicaStoryAnimationPositionXInternal(e, tX);
}

static void setDolmexicaStoryAnimationPositionYInternal(StoryAnimation& e, double tY)
{
	Position pos = getMugenAnimationPosition(e.mAnimationID);
	pos.y = tY;
	setMugenAnimationPosition(e.mAnimationID, pos);
	if (e.mHasShadow) {
		double offsetY = pos.y - e.mShadowBasePositionY;
		Position pos = getMugenAnimationPosition(e.mShadowAnimationID);
		pos.y = e.mShadowBasePositionY + getDreamStageShadowScaleY() * offsetY;
		setMugenAnimationPosition(e.mShadowAnimationID, pos);
	}
}

void setDolmexicaStoryAnimationPositionY(StoryInstance* tInstance, int tID, double tY)
{
	StoryAnimation& e = tInstance->mStoryAnimations[tID];
	setDolmexicaStoryAnimationPositionYInternal(e, tY);
}

static void addDolmexicaStoryAnimationPositionXInternal(StoryAnimation& e, double tX)
{
	Position pos = getMugenAnimationPosition(e.mAnimationID);
	pos.x += tX;
	setMugenAnimationPosition(e.mAnimationID, pos);
	if (e.mHasShadow) {
		pos = getMugenAnimationPosition(e.mShadowAnimationID);
		pos.x += tX;
		setMugenAnimationPosition(e.mShadowAnimationID, pos);
	}
}

void addDolmexicaStoryAnimationPositionX(StoryInstance* tInstance, int tID, double tX)
{
	StoryAnimation& e = tInstance->mStoryAnimations[tID];
	addDolmexicaStoryAnimationPositionXInternal(e, tX);
}

static void addDolmexicaStoryAnimationPositionYInternal(StoryAnimation& e, double tY)
{
	Position pos = getMugenAnimationPosition(e.mAnimationID);
	pos.y += tY;
	setMugenAnimationPosition(e.mAnimationID, pos);
	if (e.mHasShadow) {
		double offsetY = pos.y - e.mShadowBasePositionY;
		Position pos = getMugenAnimationPosition(e.mShadowAnimationID);
		pos.y = e.mShadowBasePositionY + getDreamStageShadowScaleY() * offsetY;
		setMugenAnimationPosition(e.mShadowAnimationID, pos);
	}
}

void addDolmexicaStoryAnimationPositionY(StoryInstance* tInstance, int tID, double tY)
{
	StoryAnimation& e = tInstance->mStoryAnimations[tID];
	addDolmexicaStoryAnimationPositionYInternal(e, tY);
}

static void setDolmexicaStoryAnimationScaleXInternal(StoryAnimation& e, double tX)
{
	Vector3D scale = getMugenAnimationDrawScale(e.mAnimationID);
	scale.x = tX;
	setMugenAnimationDrawScale(e.mAnimationID, scale);

	if (e.mHasShadow) {
		scale.y *= -getDreamStageShadowScaleY();
		setMugenAnimationDrawScale(e.mShadowAnimationID, scale);
	}
}


void setDolmexicaStoryAnimationScaleX(StoryInstance* tInstance, int tID, double tX)
{
	StoryAnimation& e = tInstance->mStoryAnimations[tID];
	setDolmexicaStoryAnimationScaleXInternal(e, tX);
}

static void setDolmexicaStoryAnimationScaleYInternal(StoryAnimation& e, double tY)
{
	Vector3D scale = getMugenAnimationDrawScale(e.mAnimationID);
	scale.y = tY;
	setMugenAnimationDrawScale(e.mAnimationID, scale);

	if (e.mHasShadow) {
		scale.y *= -getDreamStageShadowScaleY();
		setMugenAnimationDrawScale(e.mShadowAnimationID, scale);
	}
}


void setDolmexicaStoryAnimationScaleY(StoryInstance* tInstance, int tID, double tY)
{
	StoryAnimation& e = tInstance->mStoryAnimations[tID];
	setDolmexicaStoryAnimationScaleYInternal(e, tY);
}

static void setDolmexicaStoryAnimationIsFacingRightInternal(StoryAnimation& e, int tIsFacingRight)
{
	setMugenAnimationFaceDirection(e.mAnimationID, tIsFacingRight);

	if (e.mHasShadow) {
		setMugenAnimationFaceDirection(e.mShadowAnimationID, tIsFacingRight);
	}
}


void setDolmexicaStoryAnimationIsFacingRight(StoryInstance* tInstance, int tID, int tIsFacingRight)
{
	StoryAnimation& e = tInstance->mStoryAnimations[tID];
	setDolmexicaStoryAnimationIsFacingRightInternal(e, tIsFacingRight);
}

void addDolmexicaStoryText(StoryInstance* tInstance, int tID, char * tText, Vector3DI tFont, Position tBasePosition, Position tTextOffset, double tTextBoxWidth)
{
	if (int_map_contains(&tInstance->mStoryTexts, tID)) {
		removeDolmexicaStoryText(tInstance, tID);
	}
	tBasePosition.z = 50 + tID;

	StoryText* e = (StoryText*)allocMemory(sizeof(StoryText));
	e->mID = tID;
	e->mPosition = tBasePosition;
	e->mTextOffset = tTextOffset;

	Position textPosition = vecAdd(e->mPosition, e->mTextOffset);
	e->mTextID = addMugenText(tText, textPosition, tFont.x);
	setMugenTextColor(e->mTextID, getMugenTextColorFromMugenTextColorIndex(tFont.y));
	setMugenTextAlignment(e->mTextID, getMugenTextAlignmentFromMugenAlignmentIndex(tFont.z));

	setMugenTextBuildup(e->mTextID, 1);
	setMugenTextTextBoxWidth(e->mTextID, tTextBoxWidth);
	
	e->mHasBackground = 0;
	e->mHasFace = 0;
	e->mGoesToNextState = 0;
	e->mHasFinished = 0;
	e->mIsLockedOnToCharacter = 0;
	e->mIsDisabled = 0;

	int_map_push_owned(&tInstance->mStoryTexts, e->mID, e);
}

static void unloadDolmexicaStoryText(StoryText* e) {
	removeMugenText(e->mTextID);
	if (e->mHasBackground) {
		destroyMugenAnimation(e->mBackgroundAnimation);
		removeMugenAnimation(e->mBackgroundAnimationID);
	}
	if (e->mHasFace) {
		destroyMugenAnimation(e->mFaceAnimation);
		removeMugenAnimation(e->mFaceAnimationID);
	}
}

void removeDolmexicaStoryText(StoryInstance* tInstance, int tID)
{
	StoryText* e = (StoryText*)int_map_get(&tInstance->mStoryTexts, tID);
	unloadDolmexicaStoryText(e);
	
	int_map_remove(&tInstance->mStoryTexts, e->mID);
}

void setDolmexicaStoryTextBackground(StoryInstance* tInstance, int tID, Vector3DI tSprite, Position tOffset)
{
	StoryText* e = (StoryText*)int_map_get(&tInstance->mStoryTexts, tID);
	e->mBackgroundOffset = makePosition(tOffset.x, tOffset.y, -2);
	Position p = vecAdd(e->mPosition, e->mBackgroundOffset);
	e->mBackgroundAnimation = createOneFrameMugenAnimationForSprite(tSprite.x, tSprite.y);
	e->mBackgroundAnimationID = addMugenAnimation(e->mBackgroundAnimation, &gData.mSprites, p);
	e->mHasBackground = 1;
}

void setDolmexicaStoryTextFace(StoryInstance* tInstance, int tID, Vector3DI tSprite, Position tOffset)
{
	StoryText* e = (StoryText*)int_map_get(&tInstance->mStoryTexts, tID);
	e->mFaceOffset = makePosition(tOffset.x, tOffset.y, -1);
	Position p = vecAdd(e->mPosition, e->mFaceOffset);
	p.z = 50 + tID - 1;
	e->mFaceAnimation = createOneFrameMugenAnimationForSprite(tSprite.x, tSprite.y);
	e->mFaceAnimationID = addMugenAnimation(e->mFaceAnimation, &gData.mSprites, p);
	e->mHasFace = 1;
}
 
void setDolmexicaStoryTextBasePosition(StoryInstance* tInstance, int tID, Position tPosition)
{
	StoryText* e = (StoryText*)int_map_get(&tInstance->mStoryTexts, tID);
	tPosition.z = 50 + tID;
	e->mPosition = tPosition;

	Position textPosition = vecAdd(e->mPosition, e->mTextOffset);
	setMugenTextPosition(e->mTextID, textPosition);
	if (e->mHasBackground) {	
		setMugenAnimationPosition(e->mBackgroundAnimationID, vecAdd(e->mPosition, e->mBackgroundOffset));
	}
	if (e->mHasFace) {
		setMugenAnimationPosition(e->mFaceAnimationID, vecAdd(e->mPosition, e->mFaceOffset));
	}
}

void setDolmexicaStoryTextText(StoryInstance* tInstance, int tID, char * tText)
{
	StoryText* e = (StoryText*)int_map_get(&tInstance->mStoryTexts, tID);
	changeMugenText(e->mTextID, tText);
	setMugenTextBuildup(e->mTextID, 1);

	if (e->mIsDisabled) {
		if (e->mHasBackground) {
			setMugenAnimationVisibility(e->mBackgroundAnimationID, 1);
		}
		if (e->mHasFace) {
			setMugenAnimationVisibility(e->mFaceAnimationID, 1);
		}

		e->mIsDisabled = 0;
	}

	e->mHasFinished = 0;
}

void setDolmexicaStoryTextBackgroundSprite(StoryInstance* tInstance, int tID, Vector3DI tSprite)
{
	StoryText* e = (StoryText*)int_map_get(&tInstance->mStoryTexts, tID);
	destroyMugenAnimation(e->mBackgroundAnimation);
	e->mBackgroundAnimation = createOneFrameMugenAnimationForSprite(tSprite.x, tSprite.y);
	changeMugenAnimation(e->mBackgroundAnimationID, e->mBackgroundAnimation);
}

void setDolmexicaStoryTextBackgroundOffset(StoryInstance* tInstance, int tID, Position tOffset)
{
	StoryText* e = (StoryText*)int_map_get(&tInstance->mStoryTexts, tID);
	e->mBackgroundOffset = makePosition(tOffset.x, tOffset.y, -1);
	setMugenAnimationPosition(e->mBackgroundAnimationID, vecAdd(getMugenTextPosition(e->mTextID), e->mBackgroundOffset));
}

void setDolmexicaStoryTextFaceSprite(StoryInstance* tInstance, int tID, Vector3DI tSprite)
{
	StoryText* e = (StoryText*)int_map_get(&tInstance->mStoryTexts, tID);
	destroyMugenAnimation(e->mFaceAnimation);
	e->mFaceAnimation = createOneFrameMugenAnimationForSprite(tSprite.x, tSprite.y);
	changeMugenAnimation(e->mFaceAnimationID, e->mFaceAnimation);
}

void setDolmexicaStoryTextFaceOffset(StoryInstance* tInstance, int tID, Position tOffset)
{
	StoryText* e = (StoryText*)int_map_get(&tInstance->mStoryTexts, tID);
	e->mFaceOffset = makePosition(tOffset.x, tOffset.y, -1);
	setMugenAnimationPosition(e->mFaceAnimationID, vecAdd(getMugenTextPosition(e->mTextID), e->mFaceOffset));
}

void setDolmexicaStoryTextNextState(StoryInstance* tInstance, int tID, int tNextState)
{
	StoryText* e = (StoryText*)int_map_get(&tInstance->mStoryTexts, tID);
	e->mNextState = tNextState;
	e->mGoesToNextState = 1;
}

static void setDolmexicaStoryTextLockToCharacterInternal(StoryInstance * tInstance, StoryInstance* tCharacterInstance, int tID, int tCharacterID, Position tOffset) {
	StoryText* e = (StoryText*)int_map_get(&tInstance->mStoryTexts, tID);
	StoryCharacter& character = tCharacterInstance->mStoryCharacters[tCharacterID];

	e->mLockCharacterPositionReference = getMugenAnimationPositionReference(character.mAnimation.mAnimationID);
	e->mLockOffset = tOffset;
	e->mIsLockedOnToCharacter = 1;
}

void setDolmexicaStoryTextLockToCharacter(StoryInstance * tInstance, int tID, int tCharacterID, Position tOffset)
{
	setDolmexicaStoryTextLockToCharacterInternal(tInstance, tInstance, tID, tCharacterID, tOffset);
}

void setDolmexicaStoryTextLockToCharacter(StoryInstance * tInstance, int tID, int tCharacterID, Position tOffset, int tHelperID)
{
	setDolmexicaStoryTextLockToCharacterInternal(tInstance, &gData.mHelperInstances[tHelperID], tID, tCharacterID, tOffset);
}

void setDolmexicaStoryTextInactive(StoryInstance * tInstance, int tID)
{
	StoryText* e = (StoryText*)int_map_get(&tInstance->mStoryTexts, tID);
	
	char text[2];
	text[0] = '\0';
	changeMugenText(e->mTextID, text);
	if (e->mHasBackground) {
		setMugenAnimationVisibility(e->mBackgroundAnimationID, 0);
	}
	if (e->mHasFace) {
		setMugenAnimationVisibility(e->mFaceAnimationID, 0);
	}

	e->mIsDisabled = 1;
}

void changeDolmexicaStoryState(StoryInstance* tInstance, int tNextState)
{
	changeDreamHandledStateMachineState(tInstance->mStateMachineID, tNextState);
	setDreamRegisteredStateTimeInState(tInstance->mStateMachineID, 0);
}

static void changeDolmexicaStoryStateOutsideStateHandler(StoryInstance* tInstance, int tNextState)
{
	changeDreamHandledStateMachineState(tInstance->mStateMachineID, tNextState);
	setDreamRegisteredStateTimeInState(tInstance->mStateMachineID, -1);
}

void endDolmexicaStoryboard(StoryInstance* tInstance, int tNextStoryState)
{
	storyModeOverCB(tNextStoryState);
}

int getDolmexicaStoryTimeInState(StoryInstance* tInstance)
{
	int ret = getDreamRegisteredStateTimeInState(tInstance->mStateMachineID);
	return getDreamRegisteredStateTimeInState(tInstance->mStateMachineID);
}

static int getDolmexicaStoryAnimationTimeLeftInternal(StoryAnimation& e)
{
	return getMugenAnimationRemainingAnimationTime(e.mAnimationID);
}


int getDolmexicaStoryAnimationTimeLeft(StoryInstance* tInstance, int tID)
{
	if (!stl_map_contains(tInstance->mStoryAnimations, tID)) {
		logWarningFormat("Querying time left for non-existing story animation %d. Defaulting to infinite.", tID); // TODO: fix
		return INF;
	}
	StoryAnimation& e = tInstance->mStoryAnimations[tID];
	return getDolmexicaStoryAnimationTimeLeftInternal(e);
}

double getDolmexicaStoryAnimationPositionX(StoryInstance* tInstance, int tID)
{
	if (!stl_map_contains(tInstance->mStoryAnimations, tID)) {
		logWarningFormat("Querying for non-existing story animation %d. Defaulting to 0.", tID);
		return 0;
	}
	StoryAnimation& e = tInstance->mStoryAnimations[tID];
	return getMugenAnimationPosition(e.mAnimationID).x;
}

void addDolmexicaStoryCharacter(StoryInstance* tInstance, int tID, char* tName, int tAnimation, Position tPosition)
{
	if (stl_map_contains(tInstance->mStoryCharacters, tID)) {
		logWarningFormat("Trying to create existing character %d. Erasing previous.", tID);
		removeDolmexicaStoryCharacter(tInstance, tID);
	}

	StoryCharacter e;
	
	char file[1024];
	char path[1024];
	char fullPath[1024];
	char name[100];
	getCharacterSelectNamePath(tName, fullPath);
	getPathToFile(path, fullPath);
	MugenDefScript script = loadMugenDefScript(fullPath);
	

	char palettePath[1024];
	int preferredPalette = 1; // TODO: palette
	sprintf(name, "pal%d", preferredPalette);
	getMugenDefStringOrDefault(file, &script, "Files", name, "");
	int hasPalettePath = strcmp("", file);
	sprintf(palettePath, "%s%s", path, file);
	getMugenDefStringOrDefault(file, &script, "Files", "sprite", "");
	sprintf(fullPath, "%s%s", path, file);

	e.mSprites = loadMugenSpriteFile(fullPath, preferredPalette, hasPalettePath, palettePath);

	getMugenDefStringOrDefault(file, &script, "Files", "anim", "");
	sprintf(fullPath, "%s%s", path, file);
	if (!isFile(fullPath)) {
		logWarningFormat("Unable to load animation file %s from def file %s. Ignoring.", fullPath, tName);
		return;
	}
	e.mAnimations = loadMugenAnimationFile(fullPath);

	unloadMugenDefScript(script);

	tInstance->mStoryCharacters[tID] = e;

	tPosition.z = 30 + tID;
	initDolmexicaStoryAnimation(tInstance->mStoryCharacters[tID].mAnimation, tID, tAnimation, tPosition, &tInstance->mStoryCharacters[tID].mSprites, &tInstance->mStoryCharacters[tID].mAnimations);
}

void removeDolmexicaStoryCharacter(StoryInstance* tInstance, int tID)
{
	StoryCharacter& e = tInstance->mStoryCharacters[tID];
	unloadDolmexicaStoryAnimation(e.mAnimation);
	unloadMugenAnimationFile(&e.mAnimations);
	unloadMugenSpriteFile(&e.mSprites);
	tInstance->mStoryCharacters.erase(tID);
}

void setDolmexicaStoryCharacterBoundToStage(StoryInstance* tInstance, int tID, int tIsBoundToStage)
{
	StoryCharacter& e = tInstance->mStoryCharacters[tID];
	setDolmexicaStoryAnimationBoundToStageInternal(e.mAnimation, tIsBoundToStage);
}

void setDolmexicaStoryCharacterShadow(StoryInstance* tInstance, int tID, double tBasePositionY)
{
	StoryCharacter& e = tInstance->mStoryCharacters[tID];
	setDolmexicaStoryAnimationShadowInternal(e.mAnimation, tBasePositionY, &e.mSprites, &e.mAnimations);
}

int getDolmexicaStoryCharacterAnimation(StoryInstance * tInstance, int tID)
{
	StoryCharacter& e = tInstance->mStoryCharacters[tID];
	return getMugenAnimationAnimationNumber(e.mAnimation.mAnimationID);
}

void changeDolmexicaStoryCharacterAnimation(StoryInstance* tInstance, int tID, int tAnimation)
{
	StoryCharacter& e = tInstance->mStoryCharacters[tID];
	changeDolmexicaStoryAnimationInternal(e.mAnimation, tAnimation, &e.mAnimations);
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

int getDolmexicaStoryCharacterTimeLeft(StoryInstance * tInstance, int tID)
{
	if (!stl_map_contains(tInstance->mStoryCharacters, tID)) {
		logWarningFormat("Querying time left for non-existing story animation %d. Defaulting to infinite.", tID); // TODO: fix
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

double getDolmexicaStoryFloatVariable(StoryInstance * tInstance, int tID)
{
	return tInstance->mFloatVars[tID];
}

void setDolmexicaStoryFloatVariable(StoryInstance * tInstance, int tID, double tValue)
{
	tInstance->mFloatVars[tID] = tValue;
}

std::string getDolmexicaStoryStringVariable(StoryInstance * tInstance, int tID)
{
	return tInstance->mStringVars[tID];
}

void setDolmexicaStoryStringVariable(StoryInstance * tInstance, int tID, std::string tValue)
{
	tInstance->mStringVars[tID] = tValue;
}

StoryInstance * getDolmexicaStoryRootInstance()
{
	return &gData.mHelperInstances[-1];
}

void addDolmexicaStoryHelper(int tID, int tState)
{
	StoryInstance e;
	gData.mHelperInstances[tID] = e;
	initStoryInstance(gData.mHelperInstances[tID]);

	changeDolmexicaStoryStateOutsideStateHandler(&gData.mHelperInstances[tID], tState);
}
