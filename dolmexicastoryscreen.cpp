#include "dolmexicastoryscreen.h"

#include <string.h>

#include <prism/log.h>
#include <prism/math.h>
#include <prism/actorhandler.h>
#include <prism/mugenanimationhandler.h>
#include <prism/mugentexthandler.h>
#include <prism/input.h>

#include "mugenassignmentevaluator.h"
#include "mugenstatecontrollers.h"
#include "mugenstatehandler.h"
#include "titlescreen.h"
#include "stage.h"
#include "mugenstagehandler.h"
#include "storymode.h"

typedef struct {
	int mID;
	int mAnimationID;

	int mHasShadow;
	double mShadowBasePositionY;
	int mShadowAnimationID;

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
	char dummyMusicPath[2];
	*dummyMusicPath = '\0'; // TODO: story music
	setDreamStageMugenDefinition(fullPath, dummyMusicPath);
}

static void loadStoryScreen() {
	setupDreamStoryAssignmentEvaluator();
	setupDreamMugenStoryStateControllerHandler();
	instantiateActor(getDreamMugenStateHandler());

	gData.mStoryAnimations = new_int_map();
	gData.mStoryTexts = new_int_map();
	
	gData.mStoryStates = createEmptyMugenStates();
	loadDreamMugenStateDefinitionsFromFile(&gData.mStoryStates, gData.mPath);
	gData.mStateMachineID = registerDreamMugenStoryStateMachine(&gData.mStoryStates);

	MugenDefScript script = loadMugenDefScript(gData.mPath);
	loadStoryFilesFromScript(&script);
	unloadMugenDefScript(script);

	instantiateActor(getDreamStageBP());
	setDreamStageNoAutomaticCameraMovement();
}

static void unloadDolmexicaStoryText(StoryText* e);
static void changeDolmexicaStoryStateOutsideStateHandler(int tNextState);

static int updateSingleText(void* tCaller, void* tData) {
	(void)tCaller;
	StoryText* e = (StoryText*)tData;
	if (e->mHasFinished) return 0;

	if (hasPressedAFlank()) {
		if (isMugenTextBuiltUp(e->mTextID)) {
			if (e->mGoesToNextState) {
				changeDolmexicaStoryStateOutsideStateHandler(e->mNextState);
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

}

static Screen gDolmexicaStoryScreen;

Screen* getDolmexicaStoryScreen() {
	gDolmexicaStoryScreen = makeScreen(loadStoryScreen, updateStoryScreen);
	return &gDolmexicaStoryScreen;
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

	StoryAnimation* e = (StoryAnimation*)allocMemory(sizeof(StoryAnimation));
	e->mID = tID;
	e->mAnimationID = addMugenAnimation(getMugenAnimation(&gData.mAnimations, tAnimation), &gData.mSprites, tPosition);
	e->mHasShadow = 0;

	int_map_push_owned(&gData.mStoryAnimations, e->mID, e);
}

void removeDolmexicaStoryAnimation(int tID)
{
	StoryAnimation* e = (StoryAnimation*)int_map_get(&gData.mStoryAnimations, tID);
	removeMugenAnimation(e->mAnimationID);
	if (e->mHasShadow) {
		removeMugenAnimation(e->mShadowAnimationID);
	}
	int_map_remove(&gData.mStoryAnimations, e->mID);
}

static void storyAnimationOverCB(void* tCaller) {
	StoryAnimation* e = (StoryAnimation*)tCaller;
	int_map_remove(&gData.mStoryAnimations, e->mID);
}

void setDolmexicaStoryAnimationLooping(int tID, int tIsLooping)
{
	StoryAnimation* e = (StoryAnimation*)int_map_get(&gData.mStoryAnimations, tID);
	if (!tIsLooping) {
		setMugenAnimationNoLoop(e->mAnimationID);
		if (e->mHasShadow) {
			setMugenAnimationNoLoop(e->mShadowAnimationID);
		}

		setMugenAnimationCallback(e->mAnimationID, storyAnimationOverCB, e);
	}
}

void setDolmexicaStoryAnimationBoundToStage(int tID, int tIsBoundToStage)
{
	StoryAnimation* e = (StoryAnimation*)int_map_get(&gData.mStoryAnimations, tID);
	if (tIsBoundToStage) {
		setMugenAnimationCameraPositionReference(e->mAnimationID, getDreamMugenStageHandlerCameraPositionReference());
		if (e->mHasShadow) {
			setMugenAnimationCameraPositionReference(e->mShadowAnimationID, getDreamMugenStageHandlerCameraPositionReference());
		}
	}
}

// TODO: put together with playerdefinition shadow and improve in general
void setDolmexicaStoryAnimationShadow(int tID, double tBasePositionY)
{
	StoryAnimation* e = (StoryAnimation*)int_map_get(&gData.mStoryAnimations, tID);
	e->mHasShadow = 1;
	e->mShadowBasePositionY = tBasePositionY;
	Position pos = getMugenAnimationPosition(e->mAnimationID);
	pos.z = 30; // TODO: #define
	e->mShadowAnimationID = addMugenAnimation(getMugenAnimation(&gData.mAnimations, getMugenAnimationAnimationNumber(e->mAnimationID)), &gData.mSprites, pos);
	setMugenAnimationDrawScale(e->mShadowAnimationID, makePosition(1, -getDreamStageShadowScaleY(), 1));
	Vector3D color = getDreamStageShadowColor();
	(void)color; // TODO: proper shadow color
	setMugenAnimationColor(e->mShadowAnimationID, 0, 0, 0); // TODO: proper shadow color
	setMugenAnimationTransparency(e->mShadowAnimationID, getDreamStageShadowTransparency());
	setMugenAnimationFaceDirection(e->mShadowAnimationID, getMugenAnimationIsFacingRight(e->mShadowAnimationID));

	setDolmexicaStoryAnimationPositionX(tID, pos.x);
	setDolmexicaStoryAnimationPositionY(tID, pos.y);
}

void changeDolmexicaStoryAnimation(int tID, int tAnimation)
{
	StoryAnimation* e = (StoryAnimation*)int_map_get(&gData.mStoryAnimations, tID);
	changeMugenAnimation(e->mAnimationID, getMugenAnimation(&gData.mAnimations, tAnimation));
	if (e->mHasShadow) {
		changeMugenAnimation(e->mShadowAnimationID, getMugenAnimation(&gData.mAnimations, tAnimation));
	}
}

void setDolmexicaStoryAnimationPositionX(int tID, double tX)
{
	StoryAnimation* e = (StoryAnimation*)int_map_get(&gData.mStoryAnimations, tID);
	Position pos = getMugenAnimationPosition(e->mAnimationID);
	pos.x = tX;
	setMugenAnimationPosition(e->mAnimationID, pos);
	if (e->mHasShadow) {
		pos = getMugenAnimationPosition(e->mShadowAnimationID);
		pos.x = tX;
		setMugenAnimationPosition(e->mShadowAnimationID, pos);
	}
}

void setDolmexicaStoryAnimationPositionY(int tID, double tY)
{
	StoryAnimation* e = (StoryAnimation*)int_map_get(&gData.mStoryAnimations, tID);
	Position pos = getMugenAnimationPosition(e->mAnimationID);
	pos.y = tY;
	setMugenAnimationPosition(e->mAnimationID, pos);
	if (e->mHasShadow) {
		double offsetY = pos.y - e->mShadowBasePositionY;
		Position pos = getMugenAnimationPosition(e->mShadowAnimationID);
		pos.y = e->mShadowBasePositionY + getDreamStageShadowScaleY() * offsetY;
		setMugenAnimationPosition(e->mShadowAnimationID, pos);
	}
}

void addDolmexicaStoryAnimationPositionX(int tID, double tX)
{
	StoryAnimation* e = (StoryAnimation*)int_map_get(&gData.mStoryAnimations, tID);
	Position pos = getMugenAnimationPosition(e->mAnimationID);
	pos.x += tX;
	setMugenAnimationPosition(e->mAnimationID, pos);
	if (e->mHasShadow) {
		pos = getMugenAnimationPosition(e->mShadowAnimationID);
		pos.x += tX;
		setMugenAnimationPosition(e->mShadowAnimationID, pos);
	}
}

void addDolmexicaStoryAnimationPositionY(int tID, double tY)
{
	StoryAnimation* e = (StoryAnimation*)int_map_get(&gData.mStoryAnimations, tID);
	Position pos = getMugenAnimationPosition(e->mAnimationID);
	pos.y += tY;
	setMugenAnimationPosition(e->mAnimationID, pos);
	if (e->mHasShadow) {
		double offsetY = pos.y - e->mShadowBasePositionY;
		Position pos = getMugenAnimationPosition(e->mShadowAnimationID);
		pos.y = e->mShadowBasePositionY + getDreamStageShadowScaleY() * offsetY;
		setMugenAnimationPosition(e->mShadowAnimationID, pos);
	}
}

void setDolmexicaStoryAnimationScaleX(int tID, double tX)
{
	StoryAnimation* e = (StoryAnimation*)int_map_get(&gData.mStoryAnimations, tID);
	Vector3D scale = getMugenAnimationDrawScale(e->mAnimationID);
	scale.x = tX;
	setMugenAnimationDrawScale(e->mAnimationID, scale);

	if (e->mHasShadow) {
		scale.y *= -getDreamStageShadowScaleY();
		setMugenAnimationDrawScale(e->mShadowAnimationID, scale);
	}
}

void setDolmexicaStoryAnimationScaleY(int tID, double tY)
{
	StoryAnimation* e = (StoryAnimation*)int_map_get(&gData.mStoryAnimations, tID);
	Vector3D scale = getMugenAnimationDrawScale(e->mAnimationID);
	scale.y = tY;
	setMugenAnimationDrawScale(e->mAnimationID, scale);

	if (e->mHasShadow) {
		scale.y *= -getDreamStageShadowScaleY();
		setMugenAnimationDrawScale(e->mShadowAnimationID, scale);
	}
}

void setDolmexicaStoryAnimationIsFacingRight(int tID, int tIsFacingRight)
{
	StoryAnimation* e = (StoryAnimation*)int_map_get(&gData.mStoryAnimations, tID);
	setMugenAnimationFaceDirection(e->mAnimationID, tIsFacingRight);

	if (e->mHasShadow) {
		setMugenAnimationFaceDirection(e->mShadowAnimationID, tIsFacingRight);
	}
}

void addDolmexicaStoryText(int tID, char * tText, Vector3DI tFont, Position tBasePosition, Position tTextOffset, double tTextBoxWidth)
{
	if (int_map_contains(&gData.mStoryTexts, tID)) {
		removeDolmexicaStoryText(tID);
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
	StoryText* e = (StoryText*)int_map_get(&gData.mStoryTexts, tID);
	unloadDolmexicaStoryText(e);
	
	int_map_remove(&gData.mStoryTexts, e->mID);
}

void setDolmexicaStoryTextBackground(int tID, Vector3DI tSprite, Position tOffset)
{
	StoryText* e = (StoryText*)int_map_get(&gData.mStoryTexts, tID);
	e->mBackgroundOffset = makePosition(tOffset.x, tOffset.y, -2);
	Position p = vecAdd(e->mPosition, e->mBackgroundOffset);
	e->mBackgroundAnimation = createOneFrameMugenAnimationForSprite(tSprite.x, tSprite.y);
	e->mBackgroundAnimationID = addMugenAnimation(e->mBackgroundAnimation, &gData.mSprites, p);
	e->mHasBackground = 1;
}

void setDolmexicaStoryTextFace(int tID, Vector3DI tSprite, Position tOffset)
{
	StoryText* e = (StoryText*)int_map_get(&gData.mStoryTexts, tID);
	e->mFaceOffset = makePosition(tOffset.x, tOffset.y, -1);
	Position p = vecAdd(e->mPosition, e->mFaceOffset);
	p.z = 50 + tID - 1;
	e->mFaceAnimation = createOneFrameMugenAnimationForSprite(tSprite.x, tSprite.y);
	e->mFaceAnimationID = addMugenAnimation(e->mFaceAnimation, &gData.mSprites, p);
	e->mHasFace = 1;
}
 
void setDolmexicaStoryTextBasePosition(int tID, Position tPosition)
{
	StoryText* e = (StoryText*)int_map_get(&gData.mStoryTexts, tID);
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

void setDolmexicaStoryTextText(int tID, char * tText)
{
	StoryText* e = (StoryText*)int_map_get(&gData.mStoryTexts, tID);
	changeMugenText(e->mTextID, tText);
	setMugenTextBuildup(e->mTextID, 1);
	e->mHasFinished = 0;
}

void setDolmexicaStoryTextBackgroundSprite(int tID, Vector3DI tSprite)
{
	StoryText* e = (StoryText*)int_map_get(&gData.mStoryTexts, tID);
	destroyMugenAnimation(e->mBackgroundAnimation);
	e->mBackgroundAnimation = createOneFrameMugenAnimationForSprite(tSprite.x, tSprite.y);
	changeMugenAnimation(e->mBackgroundAnimationID, e->mBackgroundAnimation);
}

void setDolmexicaStoryTextBackgroundOffset(int tID, Position tOffset)
{
	StoryText* e = (StoryText*)int_map_get(&gData.mStoryTexts, tID);
	e->mBackgroundOffset = makePosition(tOffset.x, tOffset.y, -1);
	setMugenAnimationPosition(e->mBackgroundAnimationID, vecAdd(getMugenTextPosition(e->mTextID), e->mBackgroundOffset));
}

void setDolmexicaStoryTextFaceSprite(int tID, Vector3DI tSprite)
{
	StoryText* e = (StoryText*)int_map_get(&gData.mStoryTexts, tID);
	destroyMugenAnimation(e->mFaceAnimation);
	e->mFaceAnimation = createOneFrameMugenAnimationForSprite(tSprite.x, tSprite.y);
	changeMugenAnimation(e->mFaceAnimationID, e->mFaceAnimation);
}

void setDolmexicaStoryTextFaceOffset(int tID, Position tOffset)
{
	StoryText* e = (StoryText*)int_map_get(&gData.mStoryTexts, tID);
	e->mFaceOffset = makePosition(tOffset.x, tOffset.y, -1);
	setMugenAnimationPosition(e->mFaceAnimationID, vecAdd(getMugenTextPosition(e->mTextID), e->mFaceOffset));
}

void setDolmexicaStoryTextNextState(int tID, int tNextState)
{
	StoryText* e = (StoryText*)int_map_get(&gData.mStoryTexts, tID);
	e->mNextState = tNextState;
	e->mGoesToNextState = 1;
}

void changeDolmexicaStoryState(int tNextState)
{
	changeDreamHandledStateMachineState(gData.mStateMachineID, tNextState);
	setDreamRegisteredStateTimeInState(gData.mStateMachineID, 0);
}

static void changeDolmexicaStoryStateOutsideStateHandler(int tNextState)
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
	int ret = getDreamRegisteredStateTimeInState(gData.mStateMachineID);
	return getDreamRegisteredStateTimeInState(gData.mStateMachineID);
}

int getDolmexicaStoryAnimationTimeLeft(int tID)
{
	if (!int_map_contains(&gData.mStoryAnimations, tID)) {
		logWarningFormat("Querying time left for non-existing story animation %d. Defaulting to infinite.", tID); // TODO: fix
		return INF;
	}
	StoryAnimation* e = (StoryAnimation*)int_map_get(&gData.mStoryAnimations, tID);
	return getMugenAnimationRemainingAnimationTime(e->mAnimationID);
}

double getDolmexicaStoryAnimationPositionX(int tID)
{
	if (!int_map_contains(&gData.mStoryAnimations, tID)) {
		logWarningFormat("Querying for non-existing story animation %d. Defaulting to 0.", tID);
		return 0;
	}
	StoryAnimation* e = (StoryAnimation*)int_map_get(&gData.mStoryAnimations, tID);
	return getMugenAnimationPosition(e->mAnimationID).x;
}
