#include "osufilereader.h"

#include <prism/mugendefreader.h>
#include <prism/memoryhandler.h>
#include <prism/system.h>
#include <prism/log.h>

static OsuFile createEmptyOsuFile() {
	OsuFile ret;
	ret.mOsuTimingPoints = new_list();
	ret.mOsuColors = new_list();
	ret.mOsuHitObjects = new_list();
	return ret;
}

static int isLinebreak(char p) {
	return p == '\n' || p == 0xD || p == 0xA;
}



static Buffer removeOsuComments(Buffer b) {
	Buffer dstBuffer = makeBuffer(allocMemory(b.mLength), b.mLength);
	char* src = (char*)b.mData;
	char* dst = (char*)dstBuffer.mData;


	int j = 0;
	uint32_t i;
	for (i = 0; i < b.mLength - 1; i++) {
		if (src[i] == '/' && src[i + 1] == '/') {
			while (i < b.mLength && !isLinebreak(src[i])) i++;
			while (i < b.mLength && isLinebreak(src[i])) i++;
			i--;
		}
		else {
			dst[j] = src[i];
			j++;
		}
	}

	dstBuffer.mData = reallocMemory(dstBuffer.mData, j);
	dstBuffer.mLength = j;
	freeBuffer(b);
	return dstBuffer;
}


static void loadOsuFileGeneral(OsuFileGeneral* tDst, MugenDefScript* tScipt) {
	tDst->mAudioFileName = getAllocatedMugenDefStringOrDefault(tScipt, "General", "audiofilename", "music.wav");
	tDst->mAudioLeadIn = getMugenDefIntegerOrDefault(tScipt, "General", "audioleadin", 0);
	tDst->mPreviewTime = getMugenDefIntegerOrDefault(tScipt, "General", "previewtime", 0);
	tDst->mCountdown = getMugenDefIntegerOrDefault(tScipt, "General", "countdown", 0);
	tDst->mStackLeniency = getMugenDefFloatOrDefault(tScipt, "General", "stackleniency", 0);
}

static void loadOsuFileDifficulty(OsuFileDifficulty* tDst, MugenDefScript* tScipt) {
	tDst->mHPDrainRate = getMugenDefIntegerOrDefault(tScipt, "Difficulty", "hpdrainrate", 0);	
	tDst->mCircleSize = getMugenDefFloatOrDefault(tScipt, "Difficulty", "circlesize", 5);
	tDst->mOverallDifficulty = getMugenDefIntegerOrDefault(tScipt, "Difficulty", "overalldifficulty", 5);
	tDst->mApproachRate = getMugenDefFloatOrDefault(tScipt, "Difficulty", "approachrate", 5);
	tDst->mSliderMultiplier = getMugenDefFloatOrDefault(tScipt, "Difficulty", "slidermultiplier", 0);
	tDst->mSliderTickRate = getMugenDefIntegerOrDefault(tScipt, "Difficulty", "slidertickrate", 0);
}


typedef struct {
	OsuEvents* mDst;

} OsuEventLoadCaller;

static void loadSingleBreak(OsuEvents* tDst, MugenStringVector* tVector) {
	if (tVector->mSize < 3) return;

	OsuEventBreak* e = (OsuEventBreak*)allocMemory(sizeof(OsuEventBreak));
	e->mStart = atoi(tVector->mElement[1]);
	e->mEnd = atoi(tVector->mElement[2]);

	list_push_back_owned(&tDst->mBreaks, e);
}

static void loadSingleOsuEvent(void* tCaller, void* tData) {
	OsuEventLoadCaller* caller = (OsuEventLoadCaller*)tCaller;
	MugenDefScriptGroupElement* element = (MugenDefScriptGroupElement*)tData;

	if (element->mType != MUGEN_DEF_SCRIPT_GROUP_VECTOR_ELEMENT) return;
	MugenDefScriptVectorElement* vectorElement = (MugenDefScriptVectorElement*)element->mData;
	int type = atoi(vectorElement->mVector.mElement[0]);

	if (type == 2) {
		loadSingleBreak(caller->mDst, &vectorElement->mVector);
	}
}

static void loadOsuFileEvent(OsuEvents* tDst, MugenDefScript* tScript) {
	tDst->mBreaks = new_list();

	MugenDefScriptGroup* group = &tScript->mGroups["Events"];
	
	OsuEventLoadCaller caller;
	caller.mDst = tDst;
	list_map(&group->mOrderedElementList, loadSingleOsuEvent, &caller);
	
}


typedef struct {
	OsuFile* mDst;
} OsuFileTimingPointLoadCaller;

static void loadSingleOsuFileTimingPoint(void* tCaller, void* tData) {
	OsuFileTimingPointLoadCaller* caller = (OsuFileTimingPointLoadCaller*)tCaller;
	MugenDefScriptGroupElement* element = (MugenDefScriptGroupElement*)tData;

	if (element->mType != MUGEN_DEF_SCRIPT_GROUP_VECTOR_ELEMENT) return;
	MugenDefScriptVectorElement* vectorElement = (MugenDefScriptVectorElement*)element->mData;
	if (vectorElement->mVector.mSize < 5) return;

	OsuTimingPoint* e = (OsuTimingPoint*)allocMemory(sizeof(OsuTimingPoint));
	e->mOffset = atoi(vectorElement->mVector.mElement[0]);
	e->mMillisecondsPerBeat = atof(vectorElement->mVector.mElement[1]);
	e->mMeter = atoi(vectorElement->mVector.mElement[2]);
	e->mSampleIndex = atoi(vectorElement->mVector.mElement[4]);
	e->mVolume = (vectorElement->mVector.mSize >= 6) ? atoi(vectorElement->mVector.mElement[5]) : 100;
	e->mInherited = (vectorElement->mVector.mSize >= 7) ? atoi(vectorElement->mVector.mElement[6]) : 1;
	e->mKiaiMode = (vectorElement->mVector.mSize >= 8) ? atoi(vectorElement->mVector.mElement[7]) : 0;

	list_push_back_owned(&caller->mDst->mOsuTimingPoints, e);
}

static void loadOsuFileTimingPoints(OsuFile* tDst, MugenDefScript* tScript) {
	MugenDefScriptGroup* group = &tScript->mGroups["TimingPoints"];

	OsuFileTimingPointLoadCaller caller;
	caller.mDst = tDst;
	list_map(&group->mOrderedElementList, loadSingleOsuFileTimingPoint, &caller);
}

typedef struct {
	OsuFile* mDst;
} OsuFileColorLoadCaller;

static void createAndAddOsuColor(OsuFile* tFile, double r, double g, double b) {
	OsuColor* e = (OsuColor*)allocMemory(sizeof(OsuColor));
	e->mR = r;
	e->mG = g;
	e->mB = b;
	list_push_back_owned(&tFile->mOsuColors, e);
}

static void loadSingleOsuFileColor(void* tCaller, void* tData) {
	OsuFileColorLoadCaller* caller = (OsuFileColorLoadCaller*)tCaller;
	MugenDefScriptGroupElement* element = (MugenDefScriptGroupElement*)tData;

	if (element->mType != MUGEN_DEF_SCRIPT_GROUP_VECTOR_ELEMENT) return;
	MugenDefScriptVectorElement* vectorElement = (MugenDefScriptVectorElement*)element->mData;
	if (vectorElement->mVector.mSize != 3) return;

	double r = atoi(vectorElement->mVector.mElement[0]) / 255.0;
	double g = atoi(vectorElement->mVector.mElement[1]) / 255.0;
	double b = atoi(vectorElement->mVector.mElement[2]) / 255.0;
	createAndAddOsuColor(caller->mDst, r, g, b);
}

static void loadOsuFileColors(OsuFile* tDst, MugenDefScript* tScript) {
	if (tScript->mGroups.find("Colours") == tScript->mGroups.end()) {
		createAndAddOsuColor(tDst, 1, 1, 1);
		return;
	}
	MugenDefScriptGroup* group = &tScript->mGroups["Colours"];

	OsuFileColorLoadCaller caller;
	caller.mDst = tDst;
	list_map(&group->mOrderedElementList, loadSingleOsuFileColor, &caller);
}

typedef struct {
	OsuFile* mDst;
} OsuFileHitObjectLoadCaller;

static MugenStringVector parseVectorString(char* tText, char tSeparator) {

	MugenStringVector vector;

	char* comma = tText;
	vector.mSize = 0;
	while (comma != NULL) {
		vector.mSize++;
		comma = strchr(comma + 1, tSeparator);
	}

	vector.mElement = (char**)allocMemory(sizeof(char*)*vector.mSize);

	comma = tText - 1;
	int i = 0;
	while (comma != NULL) {
		char temp[MUGEN_DEF_STRING_LENGTH];
		strcpy(temp, comma + 1);
		char* tempComma = strchr(temp, tSeparator);
		if (tempComma != NULL) *tempComma = '\0';
		comma = strchr(comma + 1, tSeparator);

		int offset = 0;
		while (temp[offset] == ' ') offset++;
		char* start = temp + offset;

		vector.mElement[i] = (char*)allocMemory(strlen(start) + 3);
		strcpy(vector.mElement[i], start);

		i++;
	}

	if (i != vector.mSize) {
		logWarningFormat("Unable to parse vector %s: %d vs %d sizes detected.", tText, i, vector.mSize);
		recoverFromError();
	}

	return vector;
}

static void unloadMugenStringVector(MugenStringVector* e) {
	int i;
	for (i = 0; i < e->mSize; i++) {
		freeMemory(e->mElement[i]);
	}

	freeMemory(e->mElement);
}

Vector3DI parseEndPositionFromPathString(char* tPathString) {
	MugenStringVector vector = parseVectorString(tPathString, '|');
	if (vector.mSize < 2) {
		unloadMugenStringVector(&vector);
		return makeVector3DI(-1, -1, 0);
	}

	char* positionString = vector.mElement[vector.mSize - 1];
	char* sepPosition = strchr(positionString, ':');
	if (!sepPosition) {
		unloadMugenStringVector(&vector);
		return makeVector3DI(-1, -1, 0);
	}

	*sepPosition = '\0';
	char* posXString = positionString;
	char* posYString = sepPosition + 1;

	int posX = atoi(posXString);
	int posY = atoi(posYString);

	unloadMugenStringVector(&vector);
	return makeVector3DI(posX, posY, 0);
}

static void loadSingleOsuFileHitObject(void* tCaller, void* tData) {
	OsuFileHitObjectLoadCaller* caller = (OsuFileHitObjectLoadCaller*)tCaller;
	MugenDefScriptGroupElement* element = (MugenDefScriptGroupElement*)tData;

	if (element->mType != MUGEN_DEF_SCRIPT_GROUP_STRING_ELEMENT) return;
	MugenDefScriptStringElement* stringElement = (MugenDefScriptStringElement*)element->mData;
	MugenStringVector vector = parseVectorString(stringElement->mString, ',');
	if (vector.mSize < 5) {
		unloadMugenStringVector(&vector);
		return;
	}

	uint8_t type = (uint8_t)atoi(vector.mElement[3]);
	if (type & OSU_TYPE_MASK_HIT_OBJECT) {
		OsuHitObject* e = (OsuHitObject*)allocMemory(sizeof(OsuHitObject));
		e->mX = atoi(vector.mElement[0]);
		e->mY = atoi(vector.mElement[1]);
		e->mTime = atoi(vector.mElement[2]);
		e->mType = (uint8_t)atoi(vector.mElement[3]);
		e->mHitSound = (uint8_t)atoi(vector.mElement[4]);

		list_push_back_owned(&caller->mDst->mOsuHitObjects, e);
	}
	else if (type & OSU_TYPE_MASK_SLIDER) {
		if (vector.mSize < 8) {
			unloadMugenStringVector(&vector);
			return;
		}
		OsuSliderObject* e = (OsuSliderObject*)allocMemory(sizeof(OsuSliderObject));
		e->mX = atoi(vector.mElement[0]);
		e->mY = atoi(vector.mElement[1]);
		e->mTime = atoi(vector.mElement[2]);
		e->mType = (uint8_t)atoi(vector.mElement[3]);
		e->mHitSound = (uint8_t)atoi(vector.mElement[4]);
		e->mEndPosition = parseEndPositionFromPathString(vector.mElement[5]);
		e->mRepeat = atoi(vector.mElement[6]);
		e->mPixelLength = atof(vector.mElement[7]);

		list_push_back_owned(&caller->mDst->mOsuHitObjects, e);
	}
	else if (type & OSU_TYPE_MASK_SPINNER) {
		if (vector.mSize < 6) {
			unloadMugenStringVector(&vector);
			return;
		}
		OsuSpinnerObject* e = (OsuSpinnerObject*)allocMemory(sizeof(OsuSpinnerObject));
		e->mX = atoi(vector.mElement[0]);
		e->mY = atoi(vector.mElement[1]);
		e->mTime = atoi(vector.mElement[2]);
		e->mType = (uint8_t)atoi(vector.mElement[3]);
		e->mHitSound = (uint8_t)atoi(vector.mElement[4]);
		e->mEndTime = atoi(vector.mElement[5]);

		list_push_back_owned(&caller->mDst->mOsuHitObjects, e);
	}

	unloadMugenStringVector(&vector);
}


static void loadOsuFileHitObjects(OsuFile* tDst, MugenDefScript* tScript) {
	MugenDefScriptGroup* group = &tScript->mGroups["HitObjects"];

	OsuFileHitObjectLoadCaller caller;
	caller.mDst = tDst;
	list_map(&group->mOrderedElementList, loadSingleOsuFileHitObject, &caller);
}

static void loadOsuFileFromScript(OsuFile * tDst, MugenDefScript* tScript) {
	loadOsuFileGeneral(&tDst->mGeneral, tScript);
	loadOsuFileDifficulty(&tDst->mDifficulty, tScript);
	loadOsuFileEvent(&tDst->mEvents, tScript);
	loadOsuFileTimingPoints(tDst, tScript);
	loadOsuFileColors(tDst, tScript);
	loadOsuFileHitObjects(tDst, tScript);
}

OsuFile loadOsuFile(char * tPath)
{
	OsuFile ret = createEmptyOsuFile();
	Buffer b = fileToBuffer(tPath);
	b = removeOsuComments(b);
	MugenDefScript script; 
	loadMugenDefScriptFromBufferAndFreeBuffer(&script, b);
	loadOsuFileFromScript(&ret, &script);
	unloadMugenDefScript(script);
	return ret;
}
