#include "dolmexicastoryscreen.h"

#include <string.h>

#include <prism/actorhandler.h>
#include <prism/mugenanimationhandler.h>
#include <prism/mugentexthandler.h>
#include <prism/input.h>

#include "mugenassignmentevaluator.h"
#include "mugenstatecontrollers.h"
#include "mugenstatehandler.h"
#include "titlescreen.h"
#include "stage.h"
#include "storymode.h"

typedef struct {
	int mID;
	int mAnimationID;

} StoryAnimation;

typedef struct {
	int mID;
	Position mPosition;

	Position mTextOffset;
	int mTextID;

	int mHasBackground;
	Position mBackgroundOffset;
	MugenAnimation* mBackgroundAnimation;
	int mBackgroundAnimationID;

	int mHasFace;
	Position mFaceOffset;
	MugenAnimation* mFaceAnimation;
	int mFaceAnimationID;

	int mGoesToNextState;
	int mNextState;

	int mHasFinished;
} StoryText;

static struct {
	char mPath[1024];
	MugenSpriteFile mSprites;
	MugenAnimations mAnimations;

	IntMap mStoryAnimations;
	IntMap mStoryTexts;
	DreamMugenStates mStoryStates;
	int mStateMachineID;
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

	getMugenDefStringOrDefault(path, tScript, "Info", "stage", "");
	sprintf(fullPath, "assets/%s", path);
	setDreamStageMugenDefinition(fullPath);
}

static void loadStoryScreen() {
	setupDreamStoryAssignmentEvaluator();
	setupDreamMugenStoryStateControllerHandler();
	instantiateActor(getMugenAnimationHandlerActorBlueprint());
	instantiateActor(MugenTextHandler);
	instantiateActor(DreamMugenStateHandler);

	gData.mStoryAnimations = new_int_map();
	gData.mStoryTexts = new_int_map();
	
	gData.mStoryStates = createEmptyMugenStates();
	loadDreamMugenStateDefinitionsFromFile(&gData.mStoryStates, gData.mPath);
	gData.mStateMachineID = registerDreamMugenStoryStateMachine(&gData.mStoryStates);

	MugenDefScript script = loadMugenDefScript(gData.mPath);
	loadStoryFilesFromScript(&script);
	unloadMugenDefScript(script);

	instantiateActor(DreamStageBP);
	setDreamStageNoAutomaticCameraMovement();
}

static void unloadDolmexicaStoryText(StoryText* e);

static int updateSingleText(void* tCaller, void* tData) {
	(void)tCaller;
	StoryText* e = tData;
	if (e->mHasFinished) return 0;

	if (hasPressedAFlank()) {
		if (isMugenTextBuiltUp(e->mTextID)) {
			if (e->mGoesToNextState) {
				changeDolmexicaStoryState(e->mNextState);
			}
			e->mGoesToNextState = 0;
			e->mHasFinished = 1;
		}
		else {
			setMugenTextBuiltUp(e->mTextID);
		}
	}

	return 0;
}

static void updateTexts() {
	int_map_remove_predicate(&gData.mStoryTexts, updateSingleText, NULL);
}

static void updateStoryScreen() {
	updateTexts();

	if (hasPressedAbortFlank()) {
		setNewScreen(&DreamTitleScreen);
	}
}

Screen DolmexicaStoryScreen = {
	.mLoad = loadStoryScreen,
	.mUpdate = updateStoryScreen,
};

void setDolmexicaStoryScreenFile(char * tPath)
{
	strcpy(gData.mPath, tPath);
}

void addDolmexicaStoryAnimation(int tID, int tAnimation, Position tPosition)
{
	if (int_map_contains(&gData.mStoryAnimations, tID)) {
		removeDolmexicaStoryAnimation(tID);
	}
	tPosition.z = 30 + tID;

	StoryAnimation* e = allocMemory(sizeof(StoryAnimation));
	e->mID = tID;
	e->mAnimationID = addMugenAnimation(getMugenAnimation(&gData.mAnimations, tAnimation), &gData.mSprites, tPosition);

	int_map_push_owned(&gData.mStoryAnimations, e->mID, e);
}

void removeDolmexicaStoryAnimation(int tID)
{
	StoryAnimation* e = int_map_get(&gData.mStoryAnimations, tID);
	removeMugenAnimation(e->mAnimationID);
	int_map_remove(&gData.mStoryAnimations, e->mID);
}

static void storyAnimationOverCB(void* tCaller) {
	StoryAnimation* e = tCaller;
	int_map_remove(&gData.mStoryAnimations, e->mID);
}

void setDolmexicaStoryAnimationLooping(int tID, int tIsLooping)
{
	StoryAnimation* e = int_map_get(&gData.mStoryAnimations, tID);
	if (!tIsLooping) {
		setMugenAnimationNoLoop(e->mAnimationID);
		setMugenAnimationCallback(e->mAnimationID, storyAnimationOverCB, e);
	}
}

void changeDolmexicaStoryAnimation(int tID, int tAnimation)
{
	StoryAnimation* e = int_map_get(&gData.mStoryAnimations, tID);
	changeMugenAnimation(e->mAnimationID, getMugenAnimation(&gData.mAnimations, tAnimation));
}

void setDolmexicaStoryAnimationPositionX(int tID, double tX)
{
	StoryAnimation* e = int_map_get(&gData.mStoryAnimations, tID);
	Position pos = getMugenAnimationPosition(e->mAnimationID);
	pos.x = tX;
	setMugenAnimationPosition(e->mAnimationID, pos);
}

void setDolmexicaStoryAnimationPositionY(int tID, double tY)
{
	StoryAnimation* e = int_map_get(&gData.mStoryAnimations, tID);
	Position pos = getMugenAnimationPosition(e->mAnimationID);
	pos.y = tY;
	setMugenAnimationPosition(e->mAnimationID, pos);
}

void addDolmexicaStoryAnimationPositionX(int tID, double tX)
{
	StoryAnimation* e = int_map_get(&gData.mStoryAnimations, tID);
	Position pos = getMugenAnimationPosition(e->mAnimationID);
	pos.x += tX;
	setMugenAnimationPosition(e->mAnimationID, pos);
}

void addDolmexicaStoryAnimationPositionY(int tID, double tY)
{
	StoryAnimation* e = int_map_get(&gData.mStoryAnimations, tID);
	Position pos = getMugenAnimationPosition(e->mAnimationID);
	pos.y += tY;
	setMugenAnimationPosition(e->mAnimationID, pos);
}

void setDolmexicaStoryAnimationScaleX(int tID, double tX)
{
	StoryAnimation* e = int_map_get(&gData.mStoryAnimations, tID);
	Vector3D scale = getMugenAnimationDrawScale(e->mAnimationID);
	scale.x = tX;
	setMugenAnimationDrawScale(e->mAnimationID, scale);
}

void setDolmexicaStoryAnimationScaleY(int tID, double tY)
{
	StoryAnimation* e = int_map_get(&gData.mStoryAnimations, tID);
	Vector3D scale = getMugenAnimationDrawScale(e->mAnimationID);
	scale.y = tY;
	setMugenAnimationDrawScale(e->mAnimationID, scale);
}

void setDolmexicaStoryAnimationIsFacingRight(int tID, int tIsFacingRight)
{
	StoryAnimation* e = int_map_get(&gData.mStoryAnimations, tID);
	setMugenAnimationFaceDirection(e->mAnimationID, tIsFacingRight);
}

void addDolmexicaStoryText(int tID, char * tText, Vector3DI tFont, Position tBasePosition, Position tTextOffset, double tTextBoxWidth)
{
	if (int_map_contains(&gData.mStoryTexts, tID)) {
		removeDolmexicaStoryText(tID);
	}
	tBasePosition.z = 50 + tID;

	StoryText* e = allocMemory(sizeof(StoryText));
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

	int_map_push_owned(&gData.mStoryTexts, e->mID, e);
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

void removeDolmexicaStoryText(int tID)
{
	StoryText* e = int_map_get(&gData.mStoryTexts, tID);
	unloadDolmexicaStoryText(e);
	
	int_map_remove(&gData.mStoryTexts, e->mID);
}

void setDolmexicaStoryTextBackground(int tID, Vector3DI tSprite, Position tOffset)
{
	StoryText* e = int_map_get(&gData.mStoryTexts, tID);
	e->mBackgroundOffset = makePosition(tOffset.x, tOffset.y, -2);
	Position p = vecAdd(e->mPosition, e->mBackgroundOffset);
	e->mBackgroundAnimation = createOneFrameMugenAnimationForSprite(tSprite.x, tSprite.y);
	e->mBackgroundAnimationID = addMugenAnimation(e->mBackgroundAnimation, &gData.mSprites, p);
	e->mHasBackground = 1;
}

void setDolmexicaStoryTextFace(int tID, Vector3DI tSprite, Position tOffset)
{
	StoryText* e = int_map_get(&gData.mStoryTexts, tID);
	e->mFaceOffset = makePosition(tOffset.x, tOffset.y, -1);
	Position p = vecAdd(e->mPosition, e->mFaceOffset);
	p.z = 50 + tID - 1;
	e->mFaceAnimation = createOneFrameMugenAnimationForSprite(tSprite.x, tSprite.y);
	e->mFaceAnimationID = addMugenAnimation(e->mFaceAnimation, &gData.mSprites, p);
	e->mHasFace = 1;
}
 
void setDolmexicaStoryTextBasePosition(int tID, Position tPosition)
{
	StoryText* e = int_map_get(&gData.mStoryTexts, tID);
	tPosition.z = 50 + tID;
	e->mPosition = tPosition;

	setMugenTextPosition(e->mTextID, vecAdd(e->mPosition, e->mTextOffset));
	if (e->mHasBackground) {	
		setMugenAnimationPosition(e->mBackgroundAnimationID, vecAdd(e->mPosition, e->mBackgroundOffset));
	}
	if (e->mHasFace) {
		setMugenAnimationPosition(e->mFaceAnimationID, vecAdd(e->mPosition, e->mFaceOffset));
	}
}

void setDolmexicaStoryTextText(int tID, char * tText)
{
	StoryText* e = int_map_get(&gData.mStoryTexts, tID);
	changeMugenText(e->mTextID, tText);
	setMugenTextBuildup(e->mTextID, 1);
	e->mHasFinished = 0;
}

void setDolmexicaStoryTextBackgroundSprite(int tID, Vector3DI tSprite)
{
	StoryText* e = int_map_get(&gData.mStoryTexts, tID);
	destroyMugenAnimation(e->mBackgroundAnimation);
	e->mBackgroundAnimation = createOneFrameMugenAnimationForSprite(tSprite.x, tSprite.y);
	changeMugenAnimation(e->mBackgroundAnimationID, e->mBackgroundAnimation);
}

void setDolmexicaStoryTextBackgroundOffset(int tID, Position tOffset)
{
	StoryText* e = int_map_get(&gData.mStoryTexts, tID);
	e->mBackgroundOffset = makePosition(tOffset.x, tOffset.y, -1);
	setMugenAnimationPosition(e->mBackgroundAnimationID, vecAdd(getMugenTextPosition(e->mTextID), e->mBackgroundOffset));
}

void setDolmexicaStoryTextFaceSprite(int tID, Vector3DI tSprite)
{
	StoryText* e = int_map_get(&gData.mStoryTexts, tID);
	destroyMugenAnimation(e->mFaceAnimation);
	e->mFaceAnimation = createOneFrameMugenAnimationForSprite(tSprite.x, tSprite.y);
	changeMugenAnimation(e->mFaceAnimationID, e->mFaceAnimation);
}

void setDolmexicaStoryTextFaceOffset(int tID, Position tOffset)
{
	StoryText* e = int_map_get(&gData.mStoryTexts, tID);
	e->mFaceOffset = makePosition(tOffset.x, tOffset.y, -1);
	setMugenAnimationPosition(e->mFaceAnimationID, vecAdd(getMugenTextPosition(e->mTextID), e->mFaceOffset));
}

void setDolmexicaStoryTextNextState(int tID, int tNextState)
{
	StoryText* e = int_map_get(&gData.mStoryTexts, tID);
	e->mNextState = tNextState;
	e->mGoesToNextState = 1;
}

void changeDolmexicaStoryState(int tNextState)
{
	changeDreamHandledStateMachineState(gData.mStateMachineID, tNextState);
	setDreamRegisteredStateTimeInState(gData.mStateMachineID, -1);

}

void endDolmexicaStoryboard(int tNextStoryState)
{
	storyModeOverCB(tNextStoryState);
}

int getDolmexicaStoryTimeInState()
{
	return getDreamRegisteredStateTimeInState(gData.mStateMachineID);
}

int getDolmexicaStoryAnimationTimeLeft(int tID)
{
	StoryAnimation* e = int_map_get(&gData.mStoryAnimations, tID);
	return getMugenAnimationRemainingAnimationTime(e->mAnimationID);
}
