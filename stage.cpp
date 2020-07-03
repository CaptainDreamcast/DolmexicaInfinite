#include "stage.h"

#include <stdio.h>
#include <assert.h>
#include <algorithm>

#include <prism/memoryhandler.h>
#include <prism/log.h>
#include <prism/system.h>
#include <prism/animation.h>
#include <prism/stagehandler.h>
#include <prism/math.h>
#include <prism/input.h>
#include <prism/mugendefreader.h>
#include <prism/mugenspritefilereader.h>
#include <prism/sound.h>

#include "playerdefinition.h"
#include "mugenstagehandler.h"
#include "mugenbackgroundstatehandler.h"
#include "mugensound.h"
#include "config.h"

using namespace std;

typedef struct {
	char mName[1024];
	char mDisplayName[1024];
	char mVersionDate[100];
	char mMugenVersion[100];
	char mAuthor[100];
} StageInfo;

typedef struct {
	Position2D mStartPosition;
	double mBoundLeft;
	double mBoundRight;
	double mBoundHigh;
	double mBoundLow;

	double mTension;
	double mTensionHigh;
	double mTensionLow;

	double mVerticalFollow;
	double mFloorTension;

	int mOverdrawHigh;
	int mOverdrawLow;
	int mCutHigh;
	int mCutLow;

	double mStartZoom;
	double mZoomOut;
	double mZoomIn;
	double mCurrentZoom;

} StageCamera;

typedef struct {
	Position2D mP1Start;
	Position2D mP2Start;

	int mP1Facing;
	int mP2Facing;

	double mLeftBound;
	double mRightBound;

} StagePlayerInfo;

typedef struct {
	double mScreenLeft;
	double mScreenRight;
} StageBound;

typedef struct {
	double mZOffset;
	int mHasZOffsetLink;
	int mZOffsetLink;
	int mAutoturn;
	int mResetBG;
	Vector2DI mLocalCoordinates;
	Vector2D mScale;

} StageStageInfo;

typedef struct {
	int mIntensity;
	Vector3DI mColor;
	double mScaleY;
	Vector2D mFadeRange;
	double mXShear;

} StageShadow;

typedef struct {
	int mReflect;
	int mIntensity;
} StageReflection;

typedef struct {
	char mBGMusic[1024];
	int mBGVolume;
} StageMusic;

typedef struct {
	char mSpritePath[1024];
	int mDebugBG;

} StageBackgroundDefinition;

typedef enum {
	STAGE_BACKGROUND_STATIC,
	STAGE_BACKGROUND_ANIMATED,
	STAGE_BACKGROUND_PARALLAX,
} StageBackgroundType;

typedef struct {
	StageBackgroundType mType;
	Vector2DI mSpriteNo;
	int mLayerNo;
	Position2D mStart;
	Position2D mDelta;
	Vector2DI mTile;
	Vector2DI mTileSpacing;
	int mListPosition;
	BlendType mBlendType;
	Vector2D mAlpha;

	GeoRectangle2D mConstraintRectangle;
	Vector2D mConstraintRectangleDelta;
	Vector2D mVelocity;
	Vector3D mSinX;
	Vector3D mSinY;

	double mScaleStartY;
	double mScaleDeltaY;
	Vector2D mScaleStart;
	Vector2D mScaleDelta;

	Vector2DI mWidth;
	Vector2D mXScale;
	double mZoomDelta;
	int mPositionLink;

	int mActionNumber;
	int mID;
} StageBackgroundElement;

static struct {
	StageInfo mInfo;
	StageCamera mCamera;
	StagePlayerInfo mPlayerInfo;
	StageBound mBound;
	StageStageInfo mStageInfo;
	StageShadow mShadow;
	StageReflection mReflection;
	StageMusic mMusic;
	StageBackgroundDefinition mBackgroundDefinition;

	MugenSpriteFile mSprites;
	MugenAnimations mAnimations;

	int mIsCameraManual;
	char mDefinitionPath[1024];

	int mHasCustomMusicPath;
	char mCustomMusicPath[1024];

	double mZOffset;
} gStageData;

static void loadStageInfo(MugenDefScript* s) {
	getMugenDefStringOrDefault(gStageData.mInfo.mName, s, "info", "name", "");
	getMugenDefStringOrDefault(gStageData.mInfo.mDisplayName, s, "info", "displayname", gStageData.mInfo.mName);
	getMugenDefStringOrDefault(gStageData.mInfo.mVersionDate, s, "info", "versiondate", "1.0");
	getMugenDefStringOrDefault(gStageData.mInfo.mMugenVersion, s, "info", "mugenversion", "1.0");
	getMugenDefStringOrDefault(gStageData.mInfo.mAuthor, s, "info", "author", "John Doe");
}

static void loadStageCamera(MugenDefScript* s) {
	gStageData.mCamera.mStartPosition.x = getMugenDefFloatOrDefault(s, "camera", "startx", 0);
	gStageData.mCamera.mStartPosition.y = getMugenDefFloatOrDefault(s, "camera", "starty", 0);
	gStageData.mCamera.mBoundLeft = getMugenDefFloatOrDefault(s, "camera", "boundleft", 0);
	gStageData.mCamera.mBoundRight = getMugenDefFloatOrDefault(s, "camera", "boundright", 0);
	gStageData.mCamera.mBoundHigh = getMugenDefFloatOrDefault(s, "camera", "boundhigh", 0);
	gStageData.mCamera.mBoundLow = getMugenDefFloatOrDefault(s, "camera", "boundlow", 0);
	gStageData.mCamera.mTension = getMugenDefFloatOrDefault(s, "camera", "tension", 0);
	gStageData.mCamera.mTensionHigh = getMugenDefFloatOrDefault(s, "camera", "tensionhigh", 0);
	gStageData.mCamera.mTensionLow = getMugenDefFloatOrDefault(s, "camera", "tensionlow", 0);
	gStageData.mCamera.mVerticalFollow = getMugenDefFloatOrDefault(s, "camera", "verticalfollow", 0);
	gStageData.mCamera.mFloorTension = getMugenDefFloatOrDefault(s, "camera", "floortension", 0);
	gStageData.mCamera.mOverdrawHigh = getMugenDefIntegerOrDefault(s, "camera", "overdrawhigh", 0);
	gStageData.mCamera.mOverdrawLow = getMugenDefIntegerOrDefault(s, "camera", "overdrawlow", 0);
	gStageData.mCamera.mCutHigh = getMugenDefIntegerOrDefault(s, "camera", "cuthigh", 0);
	gStageData.mCamera.mCutLow = getMugenDefIntegerOrDefault(s, "camera", "cutlow", 0);
	gStageData.mCamera.mStartZoom = getMugenDefFloatOrDefault(s, "camera", "startzoom", 1);
	gStageData.mCamera.mZoomOut = getMugenDefFloatOrDefault(s, "camera", "zoomout", 1);
	gStageData.mCamera.mZoomIn = getMugenDefFloatOrDefault(s, "camera", "zoomin", 1);
	gStageData.mCamera.mCurrentZoom = gStageData.mCamera.mStartZoom;
}

static void loadStagePlayerInfo(MugenDefScript* s) {

	gStageData.mPlayerInfo.mP1Start.x = getMugenDefFloatOrDefault(s, "playerinfo", "p1startx", 0);
	gStageData.mPlayerInfo.mP1Start.y = getMugenDefFloatOrDefault(s, "playerinfo", "p1starty", 0);

	gStageData.mPlayerInfo.mP2Start.x = getMugenDefFloatOrDefault(s, "playerinfo", "p2startx", 0);
	gStageData.mPlayerInfo.mP2Start.y = getMugenDefFloatOrDefault(s, "playerinfo", "p2starty", 0);

	gStageData.mPlayerInfo.mP1Facing = getMugenDefIntegerOrDefault(s, "playerinfo", "p1facing", 0);
	gStageData.mPlayerInfo.mP2Facing = getMugenDefIntegerOrDefault(s, "playerinfo", "p2facing", 0);

	gStageData.mPlayerInfo.mLeftBound = getMugenDefFloatOrDefault(s, "playerinfo", "leftbound", 0);
	gStageData.mPlayerInfo.mRightBound = getMugenDefFloatOrDefault(s, "playerinfo", "rightbound", 0);
}

static void loadStageBound(MugenDefScript* s) {
	gStageData.mBound.mScreenLeft = getMugenDefFloatOrDefault(s, "bound", "screenleft", 0);
	gStageData.mBound.mScreenRight = getMugenDefFloatOrDefault(s, "bound", "screenright", 0);
}

static void loadStageStageInfo(MugenDefScript* s) {
	gStageData.mStageInfo.mZOffset = getMugenDefFloatOrDefault(s, "stageinfo", "zoffset", 0);
	gStageData.mStageInfo.mHasZOffsetLink = isMugenDefNumberVariable(s, "stageinfo", "zoffsetlink");
	if (gStageData.mStageInfo.mHasZOffsetLink) {
		gStageData.mStageInfo.mZOffsetLink = getMugenDefIntegerOrDefault(s, "stageinfo", "zoffsetlink", 0);
	}
	gStageData.mStageInfo.mAutoturn = getMugenDefIntegerOrDefault(s, "stageinfo", "autoturn", 1);
	gStageData.mStageInfo.mResetBG = getMugenDefIntegerOrDefault(s, "stageinfo", "resetbg", 0);
	gStageData.mStageInfo.mLocalCoordinates = getMugenDefVector2DIOrDefault(s, "stageinfo", "localcoord", Vector2DI(320, 240));

	gStageData.mStageInfo.mScale.x = getMugenDefFloatOrDefault(s, "stageinfo", "xscale", 1);
	gStageData.mStageInfo.mScale.y = getMugenDefFloatOrDefault(s, "stageinfo", "yscale", 1);

	gStageData.mZOffset = gStageData.mStageInfo.mZOffset;
}

static void loadStageShadow(MugenDefScript* s) {
	gStageData.mShadow.mIntensity = getMugenDefIntegerOrDefault(s, "shadow", "intensity", 128);
	gStageData.mShadow.mColor = getMugenDefVectorIOrDefault(s, "shadow", "color", Vector3DI(0, 0, 0));
	gStageData.mShadow.mScaleY = getMugenDefFloatOrDefault(s, "shadow", "yscale", 1);
	gStageData.mShadow.mFadeRange = getMugenDefVector2DOrDefault(s, "shadow", "fade.range", Vector2D(-1001, -1000));
	if (gStageData.mShadow.mFadeRange.x >= gStageData.mShadow.mFadeRange.y) {
		gStageData.mShadow.mFadeRange = Vector2D(-1001, -1000);
	}
	gStageData.mShadow.mXShear = getMugenDefFloatOrDefault(s, "shadow", "xshear", 0);
}

static void loadStageReflection(MugenDefScript* s) {
	gStageData.mReflection.mReflect = getMugenDefIntegerOrDefault(s, "reflection", "reflect", 0);
	gStageData.mReflection.mIntensity = getMugenDefIntegerOrDefault(s, "reflection", "intensity", 0);
}

static void loadStageMusic(MugenDefScript* s) {
	getMugenDefStringOrDefault(gStageData.mMusic.mBGMusic, s, "music", "bgmusic", "");
	gStageData.mMusic.mBGVolume = getMugenDefIntegerOrDefault(s, "music", "bgvolume", 0);
}

static MugenDefScriptGroup* loadStageBackgroundDefinitionAndReturnGroup(MugenDefScript* s) {
	MugenDefScriptGroup* bgdef;
	char name[100];
	if (stl_string_map_contains_array(s->mGroups, "bgdef")) {
		strcpy(name, "bgdef");
	}
	else {
		strcpy(name, "");
		logWarning("No background definition found. Defaulting to 0.");
		return NULL;
	}

	bgdef = &s->mGroups[name];

	getMugenDefStringOrDefault(gStageData.mBackgroundDefinition.mSpritePath, s, name, "spr", "");
	gStageData.mBackgroundDefinition.mDebugBG = getMugenDefIntegerOrDefault(s, name, "debugbg", 0);

	return bgdef;
}

static int isBackgroundElementGroup(MugenDefScriptGroup* tGroup) {
	return tGroup->mName[0] == 'b' && tGroup->mName[1] == 'g' && tGroup->mName[2] != 'c';
}

static void addBackgroundElementToStageHandler(StageBackgroundElement& e, MugenAnimation* tAnimation, int tOwnsAnimation, MugenSpriteFile* tSprites, const Vector2DI& tLocalCoordinates, const Vector2D& tGlobalScale) {
	const auto z = e.mListPosition*0.01 + e.mLayerNo * BACKGROUND_UPPER_BASE_Z;
	addDreamMugenStageHandlerAnimatedBackgroundElement(e.mStart.xyz(z), tAnimation, tOwnsAnimation, tSprites, e.mDelta, e.mTile, e.mTileSpacing, e.mBlendType, e.mAlpha, e.mConstraintRectangle, e.mConstraintRectangleDelta, e.mVelocity, e.mSinX, e.mSinY, e.mScaleStartY, e.mScaleDeltaY, e.mScaleStart, e.mScaleDelta, tGlobalScale, e.mLayerNo, e.mID, e.mType == STAGE_BACKGROUND_PARALLAX, e.mWidth, e.mXScale, e.mZoomDelta, e.mPositionLink, tLocalCoordinates);
}

static GeoRectangle2D getBackgroundElementWindow(MugenDefScriptGroup* tGroup, const Vector2DI& tLocalCoordinates) {
	GeoRectangle2D returnRectangle;
	if (isMugenDefGeoRectangle2DVariableAsGroup(tGroup, "maskwindow")) {
		returnRectangle = getMugenDefGeoRectangle2DOrDefaultAsGroup(tGroup, "maskwindow", GeoRectangle2D(-INF / 2, -INF / 2, INF, INF));
		returnRectangle.mTopLeft.x += tLocalCoordinates.x / 2.0;
		returnRectangle.mBottomRight.x += tLocalCoordinates.x / 2.0;
		returnRectangle.mTopLeft = returnRectangle.mTopLeft + Vector2D(1, 1);
		returnRectangle.mBottomRight = returnRectangle.mBottomRight - Vector2D(1, 1);
	}
	else {
		returnRectangle = getMugenDefGeoRectangle2DOrDefaultAsGroup(tGroup, "window", GeoRectangle2D(-INF / 2, -INF / 2, INF, INF));
	}

	// Padding to avoid black lines when pixel not totally on screen
	returnRectangle.mTopLeft = returnRectangle.mTopLeft - Vector2D(1.0, 1.0);
	returnRectangle.mBottomRight = returnRectangle.mBottomRight + Vector2D(1.0, 1.0);
	return returnRectangle;
}

void loadBackgroundElementGroup(MugenDefScriptGroup* tGroup, int i, MugenSpriteFile* tSprites, MugenAnimations* tAnimations, const Vector2DI& tLocalCoordinates, const Vector2D& tGlobalScale) {
	StageBackgroundElement e;

	debugLog("Load background element.");
 	debugString(tName);

	auto type = getSTLMugenDefStringOrDefaultAsGroup(tGroup, "type", "unknown");
	turnStringLowercase(type);

	e.mSpriteNo = getMugenDefVector2DIOrDefaultAsGroup(tGroup, "spriteno", Vector2DI(0, 0));
	e.mLayerNo = getMugenDefIntegerOrDefaultAsGroup(tGroup, "layerno", 0);
	e.mStart = getMugenDefVector2DOrDefaultAsGroup(tGroup, "start", Vector2D(0, 0));
	e.mDelta = getMugenDefVector2DOrDefaultAsGroup(tGroup, "delta", Vector2D(1, 1));
	e.mTile = getMugenDefVector2DIOrDefaultAsGroup(tGroup, "tile", Vector2DI(0, 0));
	e.mTileSpacing = getMugenDefVector2DIOrDefaultAsGroup(tGroup, "tilespacing", Vector2DI(0, 0));
	e.mConstraintRectangle = getBackgroundElementWindow(tGroup, tLocalCoordinates);
	e.mConstraintRectangleDelta = getMugenDefVector2DOrDefaultAsGroup(tGroup, "windowdelta", Vector2D(0, 0));
	e.mVelocity = getMugenDefVector2DOrDefaultAsGroup(tGroup, "velocity", Vector2D(0, 0));
	e.mSinX = getMugenDefVectorOrDefaultAsGroup(tGroup, "sin.x", Vector3D(0, 0, 0));
	e.mSinY = getMugenDefVectorOrDefaultAsGroup(tGroup, "sin.y", Vector3D(0, 0, 0));
	e.mBlendType = getBackgroundBlendType(tGroup);
	e.mAlpha = getBackgroundAlphaVector(tGroup);

	e.mListPosition = i;

	e.mScaleStartY = getMugenDefFloatOrDefaultAsGroup(tGroup, "yscalestart", 100) / 100.0;
	e.mScaleDeltaY = getMugenDefFloatOrDefaultAsGroup(tGroup, "yscaledelta", 0) / 100.0;
	e.mScaleStart = getMugenDefVector2DOrDefaultAsGroup(tGroup, "scalestart", Vector2D(1, 1));
	e.mScaleDelta = getMugenDefVector2DOrDefaultAsGroup(tGroup, "scaledelta", Vector2D(0, 0));

	e.mZoomDelta = getMugenDefFloatOrDefaultAsGroup(tGroup, "zoomdelta", 1.0);
	e.mPositionLink = getMugenDefIntegerOrDefaultAsGroup(tGroup, "positionlink", 0);

	e.mID = getMugenDefIntegerOrDefaultAsGroup(tGroup, "id", -1);

	if (!strcmp("normal", type.c_str())) {
		e.mBlendType = handleBackgroundMask(tGroup, e.mBlendType);
		e.mType = STAGE_BACKGROUND_STATIC;
		e.mWidth = Vector2DI(1, 1);
		e.mXScale = Vector2D(1, 1);
		MugenAnimation* animation;
		if (hasMugenSprite(tSprites, e.mSpriteNo.x, e.mSpriteNo.y))
		{
			animation = createOneFrameMugenAnimationForSprite(e.mSpriteNo.x, e.mSpriteNo.y);
		}
		else {
			logWarningFormat("Unable to find sprite element %d %d for stage group", e.mSpriteNo.x, e.mSpriteNo.y);
			animation = NULL;
		}
		addBackgroundElementToStageHandler(e, animation, animation != NULL, tSprites, tLocalCoordinates, tGlobalScale);
	}
	else if (!strcmp("dummy", type.c_str())) {
		e.mBlendType = BLEND_TYPE_NORMAL;
		e.mType = STAGE_BACKGROUND_STATIC;
		e.mWidth = Vector2DI(1, 1);
		e.mXScale = Vector2D(1, 1);
		addBackgroundElementToStageHandler(e, NULL, 0, tSprites, tLocalCoordinates, tGlobalScale);
	}
	else if (!strcmp("parallax", type.c_str())) {
		e.mType = STAGE_BACKGROUND_PARALLAX;
		MugenAnimation* animation;
		int ownsAnimation;
		if (isMugenDefNumberVariableAsGroup(tGroup, "actionno")) {
			e.mActionNumber = getMugenDefIntegerOrDefaultAsGroup(tGroup, "actionno", -1);
			if (hasMugenAnimation(tAnimations, e.mActionNumber))
			{
				animation = getMugenAnimation(tAnimations, e.mActionNumber);
			}
			else {
				logWarningFormat("Unable to find animation %d for stage group", e.mActionNumber);
				animation = NULL;
			}
			ownsAnimation = 0;
		}
		else {
			if (hasMugenSprite(tSprites, e.mSpriteNo.x, e.mSpriteNo.y))
			{
				animation = createOneFrameMugenAnimationForSprite(e.mSpriteNo.x, e.mSpriteNo.y);
			}
			else {
				logWarningFormat("Unable to find sprite element %d %d for stage group", e.mSpriteNo.x, e.mSpriteNo.y);
				animation = NULL;
			}
			ownsAnimation = animation != NULL;
		}
		const auto spriteSize = animation ? getAnimationFirstElementSpriteSize(animation, tSprites) : Vector2DI(0, 0);
		e.mWidth = getMugenDefVector2DIOrDefaultAsGroup(tGroup, "width", Vector2DI(spriteSize.x, spriteSize.y));
		e.mXScale = getMugenDefVector2DOrDefaultAsGroup(tGroup, "xscale", Vector2D(1, 1));

		addBackgroundElementToStageHandler(e, animation, ownsAnimation, tSprites, tLocalCoordinates, tGlobalScale);
	}
	else if (!strcmp("anim", type.c_str())) {
		e.mType = STAGE_BACKGROUND_ANIMATED;
		e.mWidth = Vector2DI(1, 1);
		e.mXScale = Vector2D(1, 1);
		e.mActionNumber = getMugenDefIntegerOrDefaultAsGroup(tGroup, "actionno", -1);
		MugenAnimation* animation;
		if (hasMugenAnimation(tAnimations, e.mActionNumber))
		{
			animation = getMugenAnimation(tAnimations, e.mActionNumber);
		}
		else {
			logWarningFormat("Unable to find animation %d for stage group", e.mActionNumber);
			animation = NULL;
		}
		addBackgroundElementToStageHandler(e, animation, 0, tSprites, tLocalCoordinates, tGlobalScale);
	}
	else {
		logWarningFormat("Unknown type %s. Ignore.", type.c_str());
	}
}

static void loadBackgroundDefinitionGroup(MugenDefScriptGroup* tGroup, int i) {
	if (isBackgroundElementGroup(tGroup)) {
		loadBackgroundElementGroup(tGroup, i, &gStageData.mSprites, &gStageData.mAnimations, gStageData.mStageInfo.mLocalCoordinates, gStageData.mStageInfo.mScale);
	}
}

static void loadStageTextures(char* tPath) {
	char path[1024];
	getPathToFile(path, tPath);
	char sffFile[1024];
	sprintf(sffFile, "%s%s", path, gStageData.mBackgroundDefinition.mSpritePath);
	if (!isFile(sffFile)) {
		sprintf(sffFile, "%s%s", getDolmexicaAssetFolder().c_str(), gStageData.mBackgroundDefinition.mSpritePath);
	}

	setMugenSpriteFileReaderToUsePalette(2);
	gStageData.mSprites = loadMugenSpriteFileWithoutPalette(sffFile);
	setMugenSpriteFileReaderToNotUsePalette();
}

static void loadStageBackgroundElements(char* tPath, MugenDefScript* s) {
	MugenDefScriptGroup* bgdef = loadStageBackgroundDefinitionAndReturnGroup(s);
	if (!bgdef) return;

	loadStageTextures(tPath);

	int i = 0;
	MugenDefScriptGroup* cur = bgdef->mNext;
	while (cur != NULL) {
		loadBackgroundDefinitionGroup(cur, i);
		i++;
		cur = cur->mNext;
	}
}

static void setStageCamera() {
	double sizeX = gStageData.mCamera.mBoundRight - gStageData.mCamera.mBoundLeft;
	double sizeY = gStageData.mCamera.mBoundLow - gStageData.mCamera.mBoundHigh;
	double scale = gStageData.mStageInfo.mLocalCoordinates.x / double(getDreamMugenStageHandlerCameraCoordinateP());
	setDreamMugenStageHandlerCameraRange(GeoRectangle2D(gStageData.mCamera.mBoundLeft, gStageData.mCamera.mBoundHigh, sizeX, sizeY) * scale);
}

static void loadStage(void* tData)
{
	(void)tData;
	setProfilingSectionMarkerCurrentFunction();
	instantiateActor(getDreamMugenStageHandler());
	instantiateActor(getBackgroundStateHandler());

	gStageData.mAnimations = loadMugenAnimationFile(gStageData.mDefinitionPath);

	MugenDefScript s; 
	loadMugenDefScript(&s, gStageData.mDefinitionPath);

	loadStageInfo(&s);
	loadStageCamera(&s);
	loadStagePlayerInfo(&s);
	loadStageBound(&s);
	loadStageStageInfo(&s);
	loadStageShadow(&s);
	loadStageReflection(&s);
	loadStageMusic(&s);
	const auto sz = getScreenSize();
	setDreamMugenStageHandlerCameraCoordinates(Vector2DI(sz.x, sz.y));

	setStageCamera();
	loadStageBackgroundElements(gStageData.mDefinitionPath, &s);
	gStageData.mIsCameraManual = 0;

	setBackgroundStatesFromScript(&s);

	unloadMugenDefScript(&s);
	resetDreamMugenStageHandlerCameraPosition();
}

static void unloadStage(void* tData)
{
	(void)tData;
	setProfilingSectionMarkerCurrentFunction();
	unloadMugenSpriteFile(&gStageData.mSprites);
	unloadMugenAnimationFile(&gStageData.mAnimations);
}

static void updateZOffset() {
	if (!gStageData.mStageInfo.mHasZOffsetLink) return;

	auto elements = getStageHandlerElementsWithID(gStageData.mStageInfo.mZOffsetLink);
	if (elements.empty()) return;
	gStageData.mZOffset = elements[0]->mTileBasePosition.y;
}

static double getDreamCameraTargetPositionX(int tCoordinateP)
{
	auto p = *getDreamMugenStageHandlerCameraTargetPositionReference();
	p = transformDreamCoordinatesVector2D(p, getDreamMugenStageHandlerCameraCoordinateP(), tCoordinateP);
	return p.x;
}

static void updateCameraMovementX() {
	std::vector<double> xPositions;
	if (getRootPlayer(0)->mIsCameraFollowing.x) {
		xPositions.push_back(getPlayerPositionX(getRootPlayer(0), gStageData.mStageInfo.mLocalCoordinates.x));
	}
	if (getRootPlayer(1)->mIsCameraFollowing.x) {
		xPositions.push_back(getPlayerPositionX(getRootPlayer(1), gStageData.mStageInfo.mLocalCoordinates.x));
	}
	if (xPositions.empty()) return;

	double minX = xPositions[0];
	double maxX = xPositions[0];
	if (xPositions.size() > 1) {
		minX = std::min(minX, xPositions[1]);
		maxX = std::max(maxX, xPositions[1]);
	}
	minX -= gStageData.mStageInfo.mLocalCoordinates.x / 2;
	maxX -= gStageData.mStageInfo.mLocalCoordinates.x / 2;

	double right = getDreamCameraTargetPositionX(gStageData.mStageInfo.mLocalCoordinates.x) + gStageData.mStageInfo.mLocalCoordinates.x / 2;
	double left = getDreamCameraTargetPositionX(gStageData.mStageInfo.mLocalCoordinates.x) - gStageData.mStageInfo.mLocalCoordinates.x / 2;

	double lx = (left + gStageData.mCamera.mTension) - minX;
	double rx = maxX - (right - gStageData.mCamera.mTension);

	if (lx <= 0 && rx > 0) {
		double delta = min(rx, -lx);
		addDreamMugenStageHandlerCameraPositionX(delta);
	}
	else if (lx > 0 && rx <= 0) {
		double delta = min(lx, -rx);
		addDreamMugenStageHandlerCameraPositionX(-delta);
	}
}

static void updateCameraMovementY() {
	std::vector<double> yPositions;
	if (getRootPlayer(0)->mIsCameraFollowing.y) {
		yPositions.push_back(getPlayerPositionY(getRootPlayer(0), getDreamMugenStageHandlerCameraCoordinateP()));
	}
	if (getRootPlayer(1)->mIsCameraFollowing.y) {
		yPositions.push_back(getPlayerPositionY(getRootPlayer(1), getDreamMugenStageHandlerCameraCoordinateP()));
	}
	if (yPositions.empty()) return;

	double mini = yPositions[0];
	if (yPositions.size() > 1) {
		mini = std::min(mini, yPositions[1]);
	}

	const auto cameraY = mini * gStageData.mCamera.mVerticalFollow + gStageData.mCamera.mStartPosition.y;
	setDreamMugenStageHandlerCameraPositionY(cameraY);
}

static void updateCameraZoom() {
	setDreamMugenStageHandlerCameraZoom(gStageData.mCamera.mCurrentZoom);
	gStageData.mCamera.mCurrentZoom = gStageData.mCamera.mStartZoom;
}

static void updateCameraActions() {
	if (gStageData.mIsCameraManual) return;

	updateCameraMovementX();
	updateCameraMovementY();
	updateCameraZoom();
}

static void updateStage(void* tData) {
	(void)tData;
	setProfilingSectionMarkerCurrentFunction();
	updateZOffset();
	updateCameraActions();
}

ActorBlueprint getDreamStageBP() {
	return makeActorBlueprint(loadStage, unloadStage, updateStage);
}

void setDreamStageMugenDefinition(const char * tPath, const char* tCustomMusicPath)
{
	strcpy(gStageData.mDefinitionPath, tPath);
	strcpy(gStageData.mCustomMusicPath, tCustomMusicPath);
}

MugenAnimations * getStageAnimations()
{
	return &gStageData.mAnimations;
}

void playDreamStageMusic()
{
	if (isMugenBGMMusicPath(gStageData.mCustomMusicPath, gStageData.mDefinitionPath)) {
		playMugenBGMMusicPath(gStageData.mCustomMusicPath, gStageData.mDefinitionPath, 1);
	}
	else if (isMugenBGMMusicPath(gStageData.mMusic.mBGMusic, gStageData.mDefinitionPath)) {
		playMugenBGMMusicPath(gStageData.mMusic.mBGMusic, gStageData.mDefinitionPath, 1);
	}
}

double parseDreamCoordinatesToLocalCoordinateSystem(double tCoordinate, int tOtherCoordinateSystemAsP)
{
	int currentP = gStageData.mStageInfo.mLocalCoordinates.x; 
	double fac = currentP / (double)tOtherCoordinateSystemAsP;

	return tCoordinate*fac;
}

Position2D getDreamPlayerStartingPositionInCameraCoordinates(int i)
{
	Position2D ret;
	if (!i) ret = gStageData.mPlayerInfo.mP1Start;
	else ret = gStageData.mPlayerInfo.mP2Start;

	ret = ret + Vector2D(gStageData.mStageInfo.mLocalCoordinates.x / 2, 0);
	return ret * (getDreamMugenStageHandlerCameraCoordinateP() / (double)gStageData.mStageInfo.mLocalCoordinates.x);
}

Position2D getDreamCameraStartPosition(int tCoordinateP)
{
	return transformDreamCoordinatesVector2D(gStageData.mCamera.mStartPosition, getDreamStageCoordinateP(), tCoordinateP);
}

Position2D getDreamStageCoordinateSystemOffset(int tCoordinateP)
{
	const auto ret = Vector2D(0, gStageData.mZOffset);
	return ret * (tCoordinateP / (double)gStageData.mStageInfo.mLocalCoordinates.x);
}

int doesDreamPlayerStartFacingLeft(int i)
{
	if (!i) return gStageData.mPlayerInfo.mP1Facing == -1;
	else return gStageData.mPlayerInfo.mP2Facing == -1;
}

double getDreamCameraPositionX(int tCoordinateP)
{
	Position p = *getDreamMugenStageHandlerCameraPositionReference();
	p = transformDreamCoordinatesVector(p, getDreamMugenStageHandlerCameraCoordinateP(), tCoordinateP);

	return p.x;
}

double getDreamCameraPositionY(int tCoordinateP)
{
	Position p = *getDreamMugenStageHandlerCameraPositionReference();
	p = transformDreamCoordinatesVector(p, getDreamMugenStageHandlerCameraCoordinateP(), tCoordinateP);

	return p.y;
}

double getDreamCameraZoom()
{
	return gStageData.mCamera.mCurrentZoom;
}

void setDreamStageZoomOneFrame(double tScale, const Position2D& tStagePos)
{
	// gStageData.mCamera.mCurrentZoom = std::min(std::max(tScale, gStageData.mCamera.mZoomOut), gStageData.mCamera.mZoomIn); // Mugen 1.1b doesn't respect the zoom limits, so Dolmexica doesn't either
	gStageData.mCamera.mCurrentZoom = std::max(tScale, 1.0); // Mugen 1.1b only supports zooming in, so Dolmexica does too
	setDreamMugenStageHandlerCameraEffectPositionX(tStagePos.x + gStageData.mStageInfo.mLocalCoordinates.x / 2.0); // Mugen 1.1b does not care about camera position, so Dolmexica doesn't either
	setDreamMugenStageHandlerCameraEffectPositionY(tStagePos.y + getDreamStageCoordinateSystemOffset(getDreamStageCoordinateP()).y);
	setDreamMugenStageHandlerCameraZoom(gStageData.mCamera.mCurrentZoom);
}

double getDreamScreenFactorFromCoordinateP(int tCoordinateP)
{
	return gStageData.mStageInfo.mLocalCoordinates.x / (double)tCoordinateP;
}

int getDreamStageCoordinateP()
{
	return gStageData.mStageInfo.mLocalCoordinates.x;
}

double getDreamStageLeftEdgeX(int tCoordinateP)
{
	const auto center = getDreamStageCenterOfScreenBasedOnPlayer(tCoordinateP);
	return center.x - (getDreamGameWidth(tCoordinateP) / 2);
}

double getDreamStageRightEdgeX(int tCoordinateP)
{
	const auto center = getDreamStageCenterOfScreenBasedOnPlayer(tCoordinateP);
	return center.x + (getDreamGameWidth(tCoordinateP) / 2);
}

double getDreamStageTopEdgeY(int tCoordinateP)
{
	return getDreamCameraPositionY(tCoordinateP);
}

double getDreamStageBottomEdgeY(int tCoordinateP)
{
	return getDreamCameraPositionY(tCoordinateP) + getDreamGameHeight(tCoordinateP);
}

double getDreamStageBoundLeft(int tCoordinateP)
{
	return transformDreamCoordinates(gStageData.mCamera.mBoundLeft, getDreamStageCoordinateP(), tCoordinateP);
}

double getDreamStageBoundRight(int tCoordinateP)
{
	return transformDreamCoordinates(gStageData.mCamera.mBoundRight + gStageData.mStageInfo.mLocalCoordinates.x, getDreamStageCoordinateP(), tCoordinateP);
}

double transformDreamCoordinates(double tVal, int tSrcP, int tDstP)
{
	if (tSrcP == tDstP) return tVal;
	return tVal * (tDstP / (double) tSrcP);
}

int transformDreamCoordinatesI(int tVal, int tSrcP, int tDstP)
{
	if (tSrcP == tDstP) return tVal;
	return int(tVal * (tDstP / (double)tSrcP));
}

Vector2D transformDreamCoordinatesVector2D(const Vector2D& tVal, int tSrcP, int tDstP)
{
	if (tSrcP == tDstP) return tVal;
	return tVal * (tDstP / (double)tSrcP);
}

Vector3D transformDreamCoordinatesVector(const Vector3D& tVal, int tSrcP, int tDstP)
{
	if (tSrcP == tDstP) return tVal;
	return vecScale(tVal, (tDstP / (double)tSrcP));
}

Vector2DI transformDreamCoordinatesVector2DI(const Vector2DI& tVal, int tSrcP, int tDstP)
{
	if (tSrcP == tDstP) return tVal;
	return vecScaleI2D(tVal, (tDstP / (double)tSrcP));
}

Vector3DI transformDreamCoordinatesVectorI(const Vector3DI& tVal, int tSrcP, int tDstP)
{
	if (tSrcP == tDstP) return tVal;
	return vecScaleI(tVal, (tDstP / (double)tSrcP));
}

double getDreamStageTopOfScreenBasedOnPlayer(int tCoordinateP)
{
	return getDreamCameraPositionY(tCoordinateP);
}

double getDreamStageTopOfScreenBasedOnPlayerInStageCoordinateOffset(int tCoordinateP)
{
	const auto stageOffset = getDreamStageCoordinateSystemOffset(tCoordinateP);
	return getDreamStageTopOfScreenBasedOnPlayer(tCoordinateP) - stageOffset.y;
}

double getDreamStageLeftOfScreenBasedOnPlayer(int tCoordinateP)
{
	Position p = *getDreamMugenStageHandlerCameraPositionReference();
	p = transformDreamCoordinatesVector(p, getDreamMugenStageHandlerCameraCoordinateP(), tCoordinateP);

	return p.x;
}

double getDreamStageRightOfScreenBasedOnPlayer(int tCoordinateP)
{
	Position p = *getDreamMugenStageHandlerCameraPositionReference();
	p = transformDreamCoordinatesVector(p, getDreamMugenStageHandlerCameraCoordinateP(), tCoordinateP);
	double screenSize = transformDreamCoordinates(gStageData.mStageInfo.mLocalCoordinates.x, gStageData.mStageInfo.mLocalCoordinates.x, tCoordinateP);

	return p.x + screenSize;
}

Position2D getDreamStageCenterOfScreenBasedOnPlayer(int tCoordinateP)
{
	auto ret = getDreamMugenStageHandlerCameraPositionReference()->xy();
	ret = ret + Vector2D(gStageData.mStageInfo.mLocalCoordinates.x / 2, 0);
	return ret * (tCoordinateP / (double)gStageData.mStageInfo.mLocalCoordinates.x);
}

int getDreamGameWidth(int tCoordinateP)
{
	return (int)transformDreamCoordinates(640, 640, tCoordinateP);
}

int getDreamGameHeight(int tCoordinateP)
{
	return (int)transformDreamCoordinates(480, 640, tCoordinateP);
}

int getDreamScreenWidth(int tCoordinateP)
{
	return int(getDreamGameWidth(tCoordinateP) * getDreamCameraZoom());
}

int getDreamScreenHeight(int tCoordinateP)
{
	return int(getDreamGameHeight(tCoordinateP) * getDreamCameraZoom());
}

char * getDreamStageAuthor()
{
	return gStageData.mInfo.mAuthor;
}

char * getDreamStageDisplayName()
{
	return gStageData.mInfo.mDisplayName;
}

char * getDreamStageName()
{
	return gStageData.mInfo.mName;
}

int getDreamStageLeftEdgeMinimumPlayerDistance(int tCoordinateP)
{
	return (int)transformDreamCoordinates(gStageData.mBound.mScreenLeft, getDreamStageCoordinateP(), tCoordinateP);
}

int getDreamStageRightEdgeMinimumPlayerDistance(int tCoordinateP)
{
	return (int)transformDreamCoordinates(gStageData.mBound.mScreenRight, getDreamStageCoordinateP(), tCoordinateP);
}

void setDreamStageCoordinates(const Vector2DI& tCoordinates)
{
	gStageData.mStageInfo.mLocalCoordinates = tCoordinates;
	gStageData.mZOffset = gStageData.mStageInfo.mZOffset = 0;
}

double getDreamStageShadowTransparency()
{
	return gStageData.mShadow.mIntensity / 255.0;
}

Vector3D getDreamStageShadowColor()
{
	return Vector3D(gStageData.mShadow.mColor.x / 255.0, gStageData.mShadow.mColor.y / 255.0, gStageData.mShadow.mColor.z / 255.0);
}

double getDreamStageShadowScaleY()
{
	return gStageData.mShadow.mScaleY;
}

static Vector2D getDreamStageShadowFadeRange(int tCoordinateP)
{
	return transformDreamCoordinatesVector2D(gStageData.mShadow.mFadeRange, getDreamStageCoordinateP(), tCoordinateP);
}

double getDreamStageReflectionTransparency()
{
	return gStageData.mReflection.mIntensity / 256.0;
}

double getDreamStageShadowFadeRangeFactor(double tPosY, int tCoordinateP)
{
	auto fadeRange = getDreamStageShadowFadeRange(tCoordinateP);
	fadeRange = fadeRange * 0.5;
	if (tPosY <= fadeRange.x) {
		return 0;
	}
	else if (tPosY <= fadeRange.y) {
		return 1 - (((-tPosY) - (-fadeRange.y)) / ((-fadeRange.x) - (-fadeRange.y)));
	}
	else {
		return 1;
	}
}

void setDreamStageNoAutomaticCameraMovement()
{
	gStageData.mIsCameraManual = 1;
}

void setDreamStageAutomaticCameraMovement()
{
	gStageData.mIsCameraManual = 0;
}

BlendType getBackgroundBlendType(MugenDefScriptGroup* tGroup) {
	if (!isMugenDefStringVariableAsGroup(tGroup, "trans")) return BLEND_TYPE_NORMAL;

	BlendType ret;
	char* text = getAllocatedMugenDefStringVariableAsGroup(tGroup, "trans");
	turnStringLowercase(text);

	if (!strcmp("add", text) || !strcmp("addalpha", text)) {
		ret = BLEND_TYPE_ADDITION;
	}
	else if (!strcmp("sub", text)) {
		ret = BLEND_TYPE_SUBTRACTION;
	}
	else if (!strcmp("none", text)) {
		ret = BLEND_TYPE_NORMAL;
	}
	else {
		ret = BLEND_TYPE_NORMAL;
		logWarningFormat("Unknown transparency type %s. Default to normal.", text);
	}

	freeMemory(text);

	return ret;
}

BlendType handleBackgroundMask(MugenDefScriptGroup* tGroup, BlendType tBlendType) {
	const auto mask = getMugenDefIntegerOrDefaultAsGroup(tGroup, "mask", (tBlendType == BLEND_TYPE_NORMAL) ? 0 : 1);
	return (mask || tBlendType != BLEND_TYPE_NORMAL) ? tBlendType : BLEND_TYPE_ONE;
}

Vector2D getBackgroundAlphaVector(MugenDefScriptGroup* tGroup)
{
	const auto text = getSTLMugenDefStringOrDefaultAsGroup(tGroup, "alpha", "256 , 256");

	const auto commaPos = text.find(',');
	std::string t1 = text.substr(0, commaPos);
	std::string t2;
	if (commaPos != text.npos) {
		t2 = text.substr(commaPos + 1, text.npos);
	}

	Vector2D ret;
	if (t1 == "") ret.x = 1.0;
	else ret.x = atof(t1.c_str()) / 256.0;
	if (t2 == "") ret.y = 1.0;
	else ret.y = atof(t2.c_str()) / 256.0;
	return ret;
}

static void resetStage()
{
	resetDreamMugenStageHandler();
	resetBackgroundStateHandler();
}

void resetStageIfNotResetForRound()
{
	if (gStageData.mStageInfo.mResetBG) return;
	resetStage();
}

void resetStageForRound()
{
	if (!gStageData.mStageInfo.mResetBG) return;
	resetStage();
}