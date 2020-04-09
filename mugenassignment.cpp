#include "mugenassignment.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include <prism/memoryhandler.h>
#include <prism/log.h>
#include <prism/system.h>
#include <prism/math.h>

#include "playerdefinition.h"
#include "stage.h"
#include "playerhitdata.h"
#include "gamelogic.h"
#include "mugencommandhandler.h"

using namespace std;

int gDebugAssignmentAmount;

static struct {
	MemoryStack* mMemoryStack;

	int mHasCommandHandlerEntryForLookup;
	int mCommandHandlerID;
} gMugenAssignmentData;

void setupDreamAssignmentReader(MemoryStack* tMemoryStack) {
	gMugenAssignmentData.mMemoryStack = tMemoryStack;
}

void shutdownDreamAssignmentReader()
{
	gMugenAssignmentData.mMemoryStack = NULL;
}

void setDreamAssignmentCommandLookupID(int tID)
{
	gMugenAssignmentData.mCommandHandlerID = tID;
	gMugenAssignmentData.mHasCommandHandlerEntryForLookup = 1;
}

void resetDreamAssignmentCommandLookupID()
{
	gMugenAssignmentData.mHasCommandHandlerEntryForLookup = 0;
}

static void* allocMemoryOnMemoryStackOrMemory(uint32_t tSize) {
	if (gMugenAssignmentData.mMemoryStack && canFitOnMemoryStack(gMugenAssignmentData.mMemoryStack, tSize)) return allocMemoryOnMemoryStack(gMugenAssignmentData.mMemoryStack, tSize);
	else return allocMemory(tSize);
}

DreamMugenAssignment * makeDreamTrueMugenAssignment()
{
	DreamMugenFixedBooleanAssignment* data = (DreamMugenFixedBooleanAssignment*)allocMemoryOnMemoryStackOrMemory(sizeof(DreamMugenFixedBooleanAssignment));
	data->mType = MUGEN_ASSIGNMENT_TYPE_FIXED_BOOLEAN;
	data->mValue = 1;
	return (DreamMugenAssignment*)data;
}

void destroyDreamFalseMugenAssignment(DreamMugenAssignment* tAssignment) {
	freeMemory(tAssignment);
}

static void unloadDreamMugenAssignmentFixedBoolean(DreamMugenAssignment * tAssignment) {
	(void)tAssignment;
}

static void unloadDreamMugenAssignmentDependOnOne(DreamMugenAssignment * tAssignment) {
	DreamMugenDependOnOneAssignment* e = (DreamMugenDependOnOneAssignment*)tAssignment;
	destroyDreamMugenAssignment(e->a);
}

static void unloadDreamMugenAssignmentDependOnTwo(DreamMugenAssignment * tAssignment) {
	DreamMugenDependOnTwoAssignment* e = (DreamMugenDependOnTwoAssignment*)tAssignment;
	destroyDreamMugenAssignment(e->a);
	destroyDreamMugenAssignment(e->b);
}

static void unloadDreamMugenAssignmentArray(DreamMugenAssignment * tAssignment) {
	DreamMugenArrayAssignment* e = (DreamMugenArrayAssignment*)tAssignment;
	destroyDreamMugenAssignment(e->mIndex);
}

static void unloadDreamMugenAssignmentRange(DreamMugenAssignment * tAssignment) {
	DreamMugenRangeAssignment* e = (DreamMugenRangeAssignment*)tAssignment;
	destroyDreamMugenAssignment(e->a);
}

static void unloadDreamMugenAssignmentNumber(DreamMugenAssignment * tAssignment) {
	(void)tAssignment;
}

static void unloadDreamMugenAssignmentFloat(DreamMugenAssignment * tAssignment) {
	(void)tAssignment;
}
static void unloadDreamMugenAssignmentString(DreamMugenAssignment * tAssignment) {
	DreamMugenStringAssignment* e = (DreamMugenStringAssignment*)tAssignment;
	freeMemory(e->mValue);
}

static void unloadDreamMugenAssignmentVariable(DreamMugenAssignment * tAssignment) {
	DreamMugenVariableAssignment* e = (DreamMugenVariableAssignment*)tAssignment;
	(void)e;
}

static void unloadDreamMugenAssignmentRawVariable(DreamMugenAssignment * tAssignment) {
	DreamMugenRawVariableAssignment* e = (DreamMugenRawVariableAssignment*)tAssignment;
	freeMemory(e->mName);
}

void destroyDreamMugenAssignment(DreamMugenAssignment * tAssignment)
{
	switch (tAssignment->mType) {
	case MUGEN_ASSIGNMENT_TYPE_FIXED_BOOLEAN:
	case MUGEN_ASSIGNMENT_TYPE_NULL:
		unloadDreamMugenAssignmentFixedBoolean(tAssignment);
		break;
	case MUGEN_ASSIGNMENT_TYPE_UNARY_MINUS:
	case MUGEN_ASSIGNMENT_TYPE_NEGATION:
		unloadDreamMugenAssignmentDependOnOne(tAssignment);
		break;
	case MUGEN_ASSIGNMENT_TYPE_AND:
	case MUGEN_ASSIGNMENT_TYPE_OR:
	case MUGEN_ASSIGNMENT_TYPE_COMPARISON:
	case MUGEN_ASSIGNMENT_TYPE_INEQUALITY:
	case MUGEN_ASSIGNMENT_TYPE_LESS_OR_EQUAL:
	case MUGEN_ASSIGNMENT_TYPE_GREATER_OR_EQUAL:
	case MUGEN_ASSIGNMENT_TYPE_SET_VARIABLE:
	case MUGEN_ASSIGNMENT_TYPE_EXPONENTIATION:
	case MUGEN_ASSIGNMENT_TYPE_BITWISE_AND:
	case MUGEN_ASSIGNMENT_TYPE_BITWISE_OR:
	case MUGEN_ASSIGNMENT_TYPE_LESS:
	case MUGEN_ASSIGNMENT_TYPE_GREATER:
	case MUGEN_ASSIGNMENT_TYPE_ADDITION:
	case MUGEN_ASSIGNMENT_TYPE_MULTIPLICATION:
	case MUGEN_ASSIGNMENT_TYPE_MODULO:
	case MUGEN_ASSIGNMENT_TYPE_SUBTRACTION:
	case MUGEN_ASSIGNMENT_TYPE_DIVISION:
	case MUGEN_ASSIGNMENT_TYPE_VECTOR:
	case MUGEN_ASSIGNMENT_TYPE_OPERATOR_ARGUMENT:
		unloadDreamMugenAssignmentDependOnTwo(tAssignment);
		break;
	case MUGEN_ASSIGNMENT_TYPE_ARRAY:
		unloadDreamMugenAssignmentArray(tAssignment);
		break;
	case MUGEN_ASSIGNMENT_TYPE_RANGE:
		unloadDreamMugenAssignmentRange(tAssignment);
		break;
	case MUGEN_ASSIGNMENT_TYPE_VARIABLE:
		unloadDreamMugenAssignmentVariable(tAssignment);
		break;
	case MUGEN_ASSIGNMENT_TYPE_RAW_VARIABLE:
		unloadDreamMugenAssignmentRawVariable(tAssignment);
		break;
	case MUGEN_ASSIGNMENT_TYPE_NUMBER:
		unloadDreamMugenAssignmentNumber(tAssignment);
		break;
	case MUGEN_ASSIGNMENT_TYPE_FLOAT:
		unloadDreamMugenAssignmentFloat(tAssignment);
		break;
	case MUGEN_ASSIGNMENT_TYPE_STRING:
		unloadDreamMugenAssignmentString(tAssignment);
		break;	
	default:
		logWarningFormat("Unrecognized assignment format %d. Treating as NULL.\n", tAssignment->mType);
		unloadDreamMugenAssignmentFixedBoolean(tAssignment);
		break;
	}
	
	freeMemory(tAssignment);
}

DreamMugenAssignment * makeDreamFalseMugenAssignment()
{
	DreamMugenFixedBooleanAssignment* data = (DreamMugenFixedBooleanAssignment*)allocMemoryOnMemoryStackOrMemory(sizeof(DreamMugenFixedBooleanAssignment));
	gDebugAssignmentAmount++;
	data->mType = MUGEN_ASSIGNMENT_TYPE_FIXED_BOOLEAN;
	data->mValue = 0;
	return (DreamMugenAssignment*)data;
}

static DreamMugenAssignment * makeMugenOneElementAssignment(DreamMugenAssignmentType tType, DreamMugenAssignment * a) 
{

	DreamMugenDependOnOneAssignment* data = (DreamMugenDependOnOneAssignment*)allocMemoryOnMemoryStackOrMemory(sizeof(DreamMugenDependOnOneAssignment));
	gDebugAssignmentAmount++;
	data->a = a;
	data->mType = tType;
	return (DreamMugenAssignment*)data;
}


static DreamMugenAssignment * makeMugenTwoElementAssignment(DreamMugenAssignmentType tType, DreamMugenAssignment * a, DreamMugenAssignment * b)
{
	DreamMugenDependOnTwoAssignment* data = (DreamMugenDependOnTwoAssignment*)allocMemoryOnMemoryStackOrMemory(sizeof(DreamMugenDependOnTwoAssignment));
	gDebugAssignmentAmount++;
	data->a = a;
	data->b = b;
	data->mType = tType;
	return (DreamMugenAssignment*)data;
}

extern std::map<string, AssignmentReturnValue*(*)(DreamMugenAssignment**, DreamPlayer*, int*)>& getActiveMugenAssignmentArrayMap();

static DreamMugenAssignment * makeMugenArrayAssignment(char* tName, DreamMugenAssignment * tIndex)
{
	std::map<string, AssignmentReturnValue*(*)(DreamMugenAssignment**, DreamPlayer*, int*)>& m = getActiveMugenAssignmentArrayMap();

	string s = string(tName);
	if (!stl_map_contains(m, s)) {
		logWarningFormat("Unrecognized array %s\n. Defaulting to bottom.", tName);
		DreamMugenAssignment* bottomReplacementReturn = makeDreamFalseMugenAssignment();
		return bottomReplacementReturn;
	}
	AssignmentReturnValue*(*func)(DreamMugenAssignment**, DreamPlayer*, int*) = m[s];

	DreamMugenArrayAssignment* data = (DreamMugenArrayAssignment*)allocMemoryOnMemoryStackOrMemory(sizeof(DreamMugenArrayAssignment));
	gDebugAssignmentAmount++;
	data->mFunc = (void*)func;
	data->mIndex = tIndex;
	data->mType = MUGEN_ASSIGNMENT_TYPE_ARRAY;
	return (DreamMugenAssignment*)data;
}

DreamMugenAssignment * makeDreamNumberMugenAssignment(int tVal)
{

	DreamMugenNumberAssignment* number = (DreamMugenNumberAssignment*)allocMemoryOnMemoryStackOrMemory(sizeof(DreamMugenNumberAssignment));
	gDebugAssignmentAmount++;
	number->mValue = tVal;
	number->mType = MUGEN_ASSIGNMENT_TYPE_NUMBER;
	return (DreamMugenAssignment*)number;
}

DreamMugenAssignment * makeDreamFloatMugenAssignment(double tVal)
{
	DreamMugenFloatAssignment* f = (DreamMugenFloatAssignment*)allocMemoryOnMemoryStackOrMemory(sizeof(DreamMugenFloatAssignment));
	gDebugAssignmentAmount++;
	f->mValue = tVal;
	f->mType = MUGEN_ASSIGNMENT_TYPE_FLOAT;

	return (DreamMugenAssignment*)f;
}

DreamMugenAssignment * makeDreamStringMugenAssignment(const char * tVal)
{
	DreamMugenStringAssignment* s = (DreamMugenStringAssignment*)allocMemoryOnMemoryStackOrMemory(sizeof(DreamMugenStringAssignment));
	gDebugAssignmentAmount++;
	s->mValue = (char*)allocMemoryOnMemoryStackOrMemory(strlen(tVal) + 2);
	strcpy(s->mValue, tVal);
	s->mType = MUGEN_ASSIGNMENT_TYPE_STRING;

	return (DreamMugenAssignment*)s;
}

DreamMugenAssignment * makeDream2DVectorMugenAssignment(Vector3D tVal)
{
	DreamMugenDependOnTwoAssignment* data = (DreamMugenDependOnTwoAssignment*)allocMemoryOnMemoryStackOrMemory(sizeof(DreamMugenDependOnTwoAssignment));
	gDebugAssignmentAmount++;
	data->a = makeDreamFloatMugenAssignment(tVal.x);
	data->b = makeDreamFloatMugenAssignment(tVal.y);
	data->mType = MUGEN_ASSIGNMENT_TYPE_VECTOR;
	return (DreamMugenAssignment*)data;
}

DreamMugenAssignment * makeDreamAndMugenAssignment(DreamMugenAssignment * a, DreamMugenAssignment * b)
{
	if (!a) return b;
	if (!b) return a;

	return makeMugenTwoElementAssignment(MUGEN_ASSIGNMENT_TYPE_AND, a, b);
}

DreamMugenAssignment * makeDreamOrMugenAssignment(DreamMugenAssignment * a, DreamMugenAssignment * b)
{
	if (!a) return b;
	if (!b) return a;

	return makeMugenTwoElementAssignment(MUGEN_ASSIGNMENT_TYPE_OR, a, b);
}

static int isEmpty(char* tChar) {
	return !strcmp("", tChar);
}

static int isEmptyCharacter(char tChar) {
	return tChar == ' ';
}

static int isOperatorCharacter(char tChar) {
	return tChar == '-' || tChar == '+' || tChar == '|' || tChar == '^' || tChar == '&' || tChar == '*' || tChar == '/' || tChar == '!' || tChar == '~';
}

static int isNonUnaryOperatorCharacter(char tChar) {
	return tChar == '+' || tChar == '|' || tChar == '^' || tChar == '&' || tChar == '*' || tChar == '/';
}

static int isBinaryOperator(const char* tText, int tPosition) {
	int p = tPosition - 1;
	int poss = 0;
	while (p >= 0) {
		if (isEmptyCharacter(tText[p])) p--;
		else if (isOperatorCharacter(tText[p])) return 0;
		else if (tText[p] == ',') return 0;
		else {
			poss = 1;
			break;
		}
	}

	if (!poss) return 0;

	int n = strlen(tText);
	p = tPosition + 1;
	poss = 0;
	while (p < n) {
		if (isEmptyCharacter(tText[p])) p++;
		else if (isNonUnaryOperatorCharacter(tText[p])) return 0;
		else {
			poss = 1;
			break;
		}
	}

	return poss;
}

static int isOnHighestLevelWithStartPositionGoingLeftToRight(char* tText, const char* tPattern, int* tOptionalPosition, int tStart, int isBinary) {
	int n = strlen(tText);
	int m = strlen(tPattern);

	int depth1 = 0;
	int depth2 = 0;
	int i;
	for (i = tStart; i < n - m + 1; i++) {
		if (!depth2) {
			if (tText[i] == '(') depth1++;
			if (tText[i] == ')') depth1--;
			if (tText[i] == '[') depth1++;
			if (tText[i] == ']') depth1--;
		}
		if (tText[i] == '"') depth2 ^= 1;

		if (depth1 || depth2) continue;

		int isSame = 1;
		int j;
		for (j = 0; j < m; j++) {
			if (tText[i + j] != tPattern[j]) {
				isSame = 0;
				break;
			}
		}

		if (isSame && (!isBinary || isBinaryOperator(tText, i))) {
			if (tOptionalPosition) *tOptionalPosition = i;
			return 1;
		}
	}

	return 0;
}

// start is first position in string that can hold end of pattern, e.g. strlen(tText) - 1
static int isOnHighestLevelWithStartPositionGoingRightToLeft(char* tText, const char* tPattern, int* tOptionalPosition, int tStart, int isBinary) {
	int m = strlen(tPattern);

	int depth1 = 0;
	int depth2 = 0;
	int i;
	for (i = tStart; i >= 0; i--) {
		if (!depth2) {
			if (tText[i] == ')') depth1++;
			if (tText[i] == '(') depth1--;
			if (tText[i] == ']') depth1++;
			if (tText[i] == '[') depth1--;
		}
		if (tText[i] == '"') depth2 ^= 1;

		if (depth1 || depth2) continue;
		if (i > tStart - m + 1) continue;

		int isSame = 1;
		int j;
		for (j = 0; j < m; j++) {
			if (tText[i + j] != tPattern[j]) {
				isSame = 0;
				break;
			}
		}

		if (isSame && (!isBinary || isBinaryOperator(tText, i))) {
			if (tOptionalPosition) *tOptionalPosition = i;
			return 1;
		}
	}

	return 0;
}

static int isOnHighestLevelMultipleWithStartPositionGoingRightToLeft(const char* tText, const std::vector<std::tuple<int(*)(const char*, int, int), DreamMugenAssignmentType, std::string>>& tPatterns, int tPatternSize, int* tOptionalPosition, int* tOptionalFoundIndex, int tStart, int tIsBinary) {
	int n = strlen(tText);
	int m = tPatternSize;

	int depth1 = 0;
	int depth2 = 0;
	int i;
	for (i = tStart; i >= 0; i--) {
		if (!depth2) {
			if (tText[i] == ')') depth1++;
			if (tText[i] == '(') depth1--;
			if (tText[i] == ']') depth1++;
			if (tText[i] == '[') depth1--;
		}
		if (tText[i] == '"') depth2 ^= 1;

		if (depth1 || depth2) continue;
		if (i > tStart - m + 1) continue;

		auto isSame = 0;
		for (int j = 0; j < int(tPatterns.size()); j++) {
			if (std::get<0>(tPatterns[j])(tText, n, i)) {
				if (tOptionalPosition) *tOptionalPosition = i;
				if (tOptionalFoundIndex) *tOptionalFoundIndex = j;
				isSame = 1;
				break;
			}
		}

		if (isSame && (!tIsBinary || isBinaryOperator(tText, i))) {
			return 1;
		}
	}

	return 0;
}

static int isOnHighestLevelLeftToRight(char* tText, const char* tPattern, int* tOptionalPosition) {
	return isOnHighestLevelWithStartPositionGoingLeftToRight(tText, tPattern, tOptionalPosition, 0, 0);
}

static int isOnHighestLevelRightToLeft(char* tText, const char* tPattern, int* tOptionalPosition) {
	return isOnHighestLevelWithStartPositionGoingRightToLeft(tText, tPattern, tOptionalPosition, strlen(tText) - 1, 0);
}

static int isOnHighestLevelBinaryRightToLeft(char* tText, const char* tPattern, int* tOptionalPosition) {
	return isOnHighestLevelWithStartPositionGoingRightToLeft(tText, tPattern, tOptionalPosition, strlen(tText) - 1, 1);
}

static int isOnHighestLevelMultipleRightToLeft(const char* tText, const std::vector<std::tuple<int(*)(const char*, int, int), DreamMugenAssignmentType, std::string>>& tPatterns, int tPatternSize, int* tOptionalPosition, int* tOptionalFoundIndex) {
	return isOnHighestLevelMultipleWithStartPositionGoingRightToLeft(tText, tPatterns, tPatternSize, tOptionalPosition, tOptionalFoundIndex, strlen(tText) - 1, 0);
}

static int isOnHighestLevelBinaryMultipleRightToLeft(const char* tText, const std::vector<std::tuple<int(*)(const char*, int, int), DreamMugenAssignmentType, std::string>>& tPatterns, int tPatternSize, int* tOptionalPosition, int* tOptionalFoundIndex) {
	return isOnHighestLevelMultipleWithStartPositionGoingRightToLeft(tText, tPatterns, tPatternSize, tOptionalPosition, tOptionalFoundIndex, strlen(tText) - 1, 1);
}

static DreamMugenAssignment* parseOneElementMugenAssignmentFromString(char* tText, DreamMugenAssignmentType tType) {
	
	DreamMugenAssignment* a = parseDreamMugenAssignmentFromString(tText);

	return makeMugenOneElementAssignment(tType, a);
}

static DreamMugenAssignment* parseTwoElementMugenAssignmentFromStringWithFixedPosition(char* tText, DreamMugenAssignmentType tType, const char* tPattern, int tPosition) {
	int n = strlen(tText);
	int m = strlen(tPattern);

	assert(tPosition != -1);
	if (tPosition >= n - m) {
		logWarningFormat("Parsing error: Incorrect two element assignment: text %s with pattern %s", tText, tPattern);
	}

	int start = tPosition;
	int end = tPosition + m;

	char text1[MUGEN_DEF_STRING_LENGTH];
	char text2[MUGEN_DEF_STRING_LENGTH];

	strcpy(text1, tText);
	text1[start] = '\0';

	strcpy(text2, &tText[end]);

	DreamMugenAssignment* a = parseDreamMugenAssignmentFromString(text1);
	DreamMugenAssignment* b = parseDreamMugenAssignmentFromString(text2);

	return makeMugenTwoElementAssignment(tType, a, b);
}

static DreamMugenAssignment* parseTwoElementLeftToRightMugenAssignmentFromString(char* tText, DreamMugenAssignmentType tType, const char* tPattern) {
	int pos = -1;
	isOnHighestLevelLeftToRight(tText, tPattern, &pos);
	return parseTwoElementMugenAssignmentFromStringWithFixedPosition(tText, tType, tPattern, pos);
}

static DreamMugenAssignment* parseTwoElementRightToLeftMugenAssignmentFromString(char* tText, DreamMugenAssignmentType tType, const char* tPattern) {
	int pos = -1;
	isOnHighestLevelRightToLeft(tText, tPattern, &pos);
	return parseTwoElementMugenAssignmentFromStringWithFixedPosition(tText, tType, tPattern, pos);
}

static DreamMugenAssignment* parseBinaryTwoElementRightToLeftMugenAssignmentFromString(char* tText, DreamMugenAssignmentType tType, const char* tPattern) {
	int pos = -1;
	isOnHighestLevelBinaryRightToLeft(tText, tPattern, &pos);
	return parseTwoElementMugenAssignmentFromStringWithFixedPosition(tText, tType, tPattern, pos);
}

static DreamMugenAssignment* parseTwoElementMultipleRightToLeftMugenAssignmentFromString(char* tText, const std::vector<std::tuple<int(*)(const char*, int, int), DreamMugenAssignmentType, std::string>>& tPatterns, int tPatternSize) {
	int pos = -1;
	int index = -1;
	isOnHighestLevelMultipleRightToLeft(tText, tPatterns, tPatternSize, &pos, &index);
	return parseTwoElementMugenAssignmentFromStringWithFixedPosition(tText, std::get<1>(tPatterns[index]), std::get<2>(tPatterns[index]).c_str(), pos);
}

static DreamMugenAssignment* parseBinaryTwoElementMultipleRightToLeftMugenAssignmentFromString(char* tText, const std::vector<std::tuple<int(*)(const char*, int, int), DreamMugenAssignmentType, std::string>>& tPatterns, int tPatternSize) {
	int pos = -1;
	int index = -1;
	isOnHighestLevelBinaryMultipleRightToLeft(tText, tPatterns, tPatternSize, &pos, &index);
	return parseTwoElementMugenAssignmentFromStringWithFixedPosition(tText, std::get<1>(tPatterns[index]), std::get<2>(tPatterns[index]).c_str(), pos);
}

static DreamMugenAssignment* parseMugenNullFromString() {
	DreamMugenFixedBooleanAssignment* data = (DreamMugenFixedBooleanAssignment*)allocMemoryOnMemoryStackOrMemory(sizeof(DreamMugenFixedBooleanAssignment));
	gDebugAssignmentAmount++;
	data->mValue = 0;
	data->mType = MUGEN_ASSIGNMENT_TYPE_NULL;
	return (DreamMugenAssignment*)data;
}



static int isInBraces(char* tText) {
	int n = strlen(tText);
	if (tText[0] != '(' || tText[n - 1] != ')') return 0;

	int depth = 0;
	int i;
	for (i = 0; i < n - 1; i++) {
		if (tText[i] == '(') depth++;
		if (tText[i] == ')') depth--;

		if (!depth) return 0;
	}

	return 1;
}

static DreamMugenAssignment* parseMugenAssignmentStringInBraces(char* tText) {
	int n = strlen(tText);
	tText[n - 1] = '\0';
	tText++;

	return parseDreamMugenAssignmentFromString(tText);
}

static int isNegation(char* tText) {
	return (tText[0] == '!');
}

static DreamMugenAssignment* parseMugenNegationFromString(char* tText) {
	tText++;
	return parseOneElementMugenAssignmentFromString(tText, MUGEN_ASSIGNMENT_TYPE_NEGATION);
}

static int isBitwiseInversion(const char* tText) {
	return (tText[0] == '~');
}

static DreamMugenAssignment* parseMugenBitwiseInversionFromString(char* tText) {
	tText++;
	return parseOneElementMugenAssignmentFromString(tText, MUGEN_ASSIGNMENT_TYPE_BITWISE_INVERSION);
}

static int isUnaryMinus(char* tText) {
	return (tText[0] == '-');
}

static DreamMugenAssignment* parseMugenUnaryMinusFromString(char* tText) {
	tText++;
	return parseOneElementMugenAssignmentFromString(tText, MUGEN_ASSIGNMENT_TYPE_UNARY_MINUS);
}

static int isRange(char* tText) {
	int n = strlen(tText);
	if ((tText[0] != '[' && tText[0] != '(') || (tText[n - 1] != ']' && tText[n - 1] != ')')) return 0;

	int depth = 0;
	int i;
	for (i = 0; i < n - 1; i++) {
		if (tText[i] == '(' || tText[i] == '[') depth++;
		if (tText[i] == ')' || tText[i] == ']') depth--;

		if (!depth) return 0;
	}

	return 1;
}

static DreamMugenAssignment* parseMugenRangeFromString(char* tText) {
	DreamMugenRangeAssignment* e = (DreamMugenRangeAssignment*)allocMemoryOnMemoryStackOrMemory(sizeof(DreamMugenRangeAssignment));
	gDebugAssignmentAmount++;
	int n = strlen(tText);
	e->mExcludeLeft = tText[0] == '(';
	e->mExcludeRight = tText[n - 1] == ')';
	
	tText[n - 1] = '\0';
	tText++;

	e->a = parseDreamMugenAssignmentFromString(tText);
	e->mType = MUGEN_ASSIGNMENT_TYPE_RANGE;
	return (DreamMugenAssignment*)e;
}

static int isVariableSet(char* tText) {
	return isOnHighestLevelLeftToRight(tText, ":=", NULL);
}

static DreamMugenAssignment* parseMugenVariableSetFromString(char* tText) {
	return parseTwoElementLeftToRightMugenAssignmentFromString(tText, MUGEN_ASSIGNMENT_TYPE_SET_VARIABLE, ":=");
}

static int isEqualityCharacter(const char* tText, int /*n*/, int tPos) {
	if (tText[tPos] != '=') return 0;
	if (tPos > 0 && (tText[tPos - 1] == '!' || tText[tPos - 1] == '<' || tText[tPos - 1] == '>' || tText[tPos - 1] == ':')) return 0;
	if (tPos == 0) return 0;
	return 1;
}

static int isInequalityCharacter(const char* tText, int n, int tPos) {
	if (tText[tPos] != '!') return 0;
	if (tPos >= n - 1) return 0;
	if (tText[tPos + 1] != '=') return 0;
	if (tPos == 0) return 0;
	return 1;
}

static int isComparisonGroup(const char* tText) {
	std::vector<std::tuple<int(*)(const char*, int, int), DreamMugenAssignmentType, std::string>> patterns;
	patterns.push_back(std::make_tuple(isEqualityCharacter, MUGEN_ASSIGNMENT_TYPE_COMPARISON, "="));
	patterns.push_back(std::make_tuple(isInequalityCharacter, MUGEN_ASSIGNMENT_TYPE_INEQUALITY, "!="));
	return isOnHighestLevelBinaryMultipleRightToLeft(tText, patterns, 1, NULL, NULL);
}

static DreamMugenAssignment* parseMugenComparisonGroupFromString(char* tText) {
	std::vector<std::tuple<int(*)(const char*, int, int), DreamMugenAssignmentType, std::string>> patterns;
	patterns.push_back(std::make_tuple(isEqualityCharacter, MUGEN_ASSIGNMENT_TYPE_COMPARISON, "="));
	patterns.push_back(std::make_tuple(isInequalityCharacter, MUGEN_ASSIGNMENT_TYPE_INEQUALITY, "!="));
	return parseBinaryTwoElementMultipleRightToLeftMugenAssignmentFromString(tText, patterns, 1);
}

static int isAnd(char* tText) {
	return isOnHighestLevelRightToLeft(tText, "&&", NULL);
}

static DreamMugenAssignment* parseMugenAndFromString(char* tText) {
	return parseTwoElementRightToLeftMugenAssignmentFromString(tText, MUGEN_ASSIGNMENT_TYPE_AND, "&&");
}

static int isXor(char* tText) {
	return isOnHighestLevelRightToLeft(tText, "^^", NULL);
}

static DreamMugenAssignment* parseMugenXorFromString(char* tText) {
	return parseTwoElementRightToLeftMugenAssignmentFromString(tText, MUGEN_ASSIGNMENT_TYPE_XOR, "^^");
}

static int isOr(char* tText) {
	return isOnHighestLevelRightToLeft(tText, "||", NULL);
}

static DreamMugenAssignment* parseMugenOrFromString(char* tText) {
	return parseTwoElementRightToLeftMugenAssignmentFromString(tText, MUGEN_ASSIGNMENT_TYPE_OR, "||");
}

static int isBitwiseAnd(char* tText) {
	return isOnHighestLevelRightToLeft(tText, "&", NULL);
}

static DreamMugenAssignment* parseMugenBitwiseAndFromString(char* tText) {
	return parseTwoElementRightToLeftMugenAssignmentFromString(tText, MUGEN_ASSIGNMENT_TYPE_BITWISE_AND, "&");
}

static int isBitwiseXor(char* tText) {
	return isOnHighestLevelRightToLeft(tText, "^", NULL);
}

static DreamMugenAssignment* parseMugenBitwiseXorFromString(char* tText) {
	return parseTwoElementRightToLeftMugenAssignmentFromString(tText, MUGEN_ASSIGNMENT_TYPE_BITWISE_XOR, "^");
}

static int isBitwiseOr(char* tText) {
	return isOnHighestLevelRightToLeft(tText, "|", NULL);
}

static DreamMugenAssignment* parseMugenBitwiseOrFromString(char* tText) {
	return parseTwoElementRightToLeftMugenAssignmentFromString(tText, MUGEN_ASSIGNMENT_TYPE_BITWISE_OR, "|");
}

static int isGreaterThanCharacter(const char* tText, int n, int tPos) {
	if (tText[tPos] != '>') return 0;
	if (tPos < (n - 1) && tText[tPos + 1] == '=') return 0;
	if (tPos == 0) return 0;
	return 1;
}

static int isGreaterThanEqualCharacter(const char* tText, int n, int tPos) {
	if (tText[tPos] != '>') return 0;
	if (tPos >= n - 1) return 0;
	if (tText[tPos + 1] != '=') return 0;
	if (tPos == 0) return 0;
	return 1;
}

static int isLessThanCharacter(const char* tText, int n, int tPos) {
	if (tText[tPos] != '<') return 0;
	if (tPos < (n - 1) && tText[tPos + 1] == '=') return 0;
	if (tPos == 0) return 0;
	return 1;
}

static int isLessThanEqualCharacter(const char* tText, int n, int tPos) {
	if (tText[tPos] != '<') return 0;
	if (tPos >= n - 1) return 0;
	if (tText[tPos + 1] != '=') return 0;
	if (tPos == 0) return 0;
	return 1;
}

static int isOrdinalGroup(const char* tText) {
	std::vector<std::tuple<int(*)(const char*, int, int), DreamMugenAssignmentType, std::string>> patterns;
	patterns.push_back(std::make_tuple(isGreaterThanCharacter, MUGEN_ASSIGNMENT_TYPE_GREATER, ">"));
	patterns.push_back(std::make_tuple(isGreaterThanEqualCharacter, MUGEN_ASSIGNMENT_TYPE_GREATER_OR_EQUAL, ">="));
	patterns.push_back(std::make_tuple(isLessThanCharacter, MUGEN_ASSIGNMENT_TYPE_LESS, "<"));
	patterns.push_back(std::make_tuple(isLessThanEqualCharacter, MUGEN_ASSIGNMENT_TYPE_LESS_OR_EQUAL, "<="));
	return isOnHighestLevelMultipleRightToLeft(tText, patterns, 1, NULL, NULL);
}

static DreamMugenAssignment* parseMugenOrdinalGroupFromString(char* tText) {
	std::vector<std::tuple<int(*)(const char*, int, int), DreamMugenAssignmentType, std::string>> patterns;
	patterns.push_back(std::make_tuple(isGreaterThanCharacter, MUGEN_ASSIGNMENT_TYPE_GREATER, ">"));
	patterns.push_back(std::make_tuple(isGreaterThanEqualCharacter, MUGEN_ASSIGNMENT_TYPE_GREATER_OR_EQUAL, ">="));
	patterns.push_back(std::make_tuple(isLessThanCharacter, MUGEN_ASSIGNMENT_TYPE_LESS, "<"));
	patterns.push_back(std::make_tuple(isLessThanEqualCharacter, MUGEN_ASSIGNMENT_TYPE_LESS_OR_EQUAL, "<="));
	return parseTwoElementMultipleRightToLeftMugenAssignmentFromString(tText, patterns, 1);
}

static int isExponentiation(char* tText) {
	return isOnHighestLevelRightToLeft(tText, "**", NULL);
}

static DreamMugenAssignment* parseMugenExponentiationFromString(char* tText) {
	return parseTwoElementRightToLeftMugenAssignmentFromString(tText, MUGEN_ASSIGNMENT_TYPE_EXPONENTIATION, "**");
}

static int isAddition(char* tText) {
	int plusPosition;
	if(!isOnHighestLevelRightToLeft(tText, "+", &plusPosition)) return 0;
	int len = strlen(tText);
	return plusPosition < len -1;
}

static DreamMugenAssignment* parseMugenAdditionFromString(char* tText) {
	return parseTwoElementRightToLeftMugenAssignmentFromString(tText, MUGEN_ASSIGNMENT_TYPE_ADDITION, "+");
}

static int isSubtraction(char* tText) {
	return isOnHighestLevelBinaryRightToLeft(tText, "-", NULL);
}

static DreamMugenAssignment* parseMugenSubtractionFromString(char* tText) {
	return parseBinaryTwoElementRightToLeftMugenAssignmentFromString(tText, MUGEN_ASSIGNMENT_TYPE_SUBTRACTION, "-");
}

static int isMultiplicationCharacter(const char* tText, int n, int tPos) {
	if (tText[tPos] != '*') return 0;
	if (tPos == 0) return 0;
	if (tPos > 0 && tText[tPos - 1] == '*') return 0;
	if (tPos < n - 1 && tText[tPos + 1] == '*') return 0;
	return 1;
}

static int isDivisionCharacter(const char* tText, int /*n*/, int tPos) {
	return (tText[tPos] == '/');
}

static int isModuloCharacter(const char* tText, int /*n*/, int tPos) {
	return (tText[tPos] == '%');
}

static int isMultiplicativeGroup(const char* tText) {
	std::vector<std::tuple<int(*)(const char*, int, int), DreamMugenAssignmentType, std::string>> patterns;
	patterns.push_back(std::make_tuple(isMultiplicationCharacter, MUGEN_ASSIGNMENT_TYPE_MULTIPLICATION, "*"));
	patterns.push_back(std::make_tuple(isDivisionCharacter, MUGEN_ASSIGNMENT_TYPE_DIVISION, "/"));
	patterns.push_back(std::make_tuple(isModuloCharacter, MUGEN_ASSIGNMENT_TYPE_MODULO, "%"));
	return isOnHighestLevelMultipleRightToLeft(tText, patterns, 1, NULL, NULL);
}

static DreamMugenAssignment* parseMugenMultiplicativeGroupFromString(char* tText) {
	std::vector<std::tuple<int(*)(const char*, int, int), DreamMugenAssignmentType, std::string>> patterns;
	patterns.push_back(std::make_tuple(isMultiplicationCharacter, MUGEN_ASSIGNMENT_TYPE_MULTIPLICATION, "*"));
	patterns.push_back(std::make_tuple(isDivisionCharacter, MUGEN_ASSIGNMENT_TYPE_DIVISION, "/"));
	patterns.push_back(std::make_tuple(isModuloCharacter, MUGEN_ASSIGNMENT_TYPE_MODULO, "%"));
	return parseTwoElementMultipleRightToLeftMugenAssignmentFromString(tText, patterns, 1);
}

static int isNumericalConstant(char* tText) {
	if (*tText == '-') tText++;

	int n = strlen(tText);
	if (n == 0) return 0;

	int mPointAmount = 0;
	int i;
	for (i = 0; i < n; i++) {
		if (tText[i] == '.') mPointAmount++;
		else if (tText[i] >= '0' && tText[i] <= '9') continue;
		else return 0;
	}

	return mPointAmount <= 0;
}

static DreamMugenAssignment* parseNumericalConstantFromString(char* tText) {
	int val = atoi(tText);

	return makeDreamNumberMugenAssignment(val);
}

static int isFloatConstant(char* tText) {
	if (*tText == '-') tText++;

	int n = strlen(tText);
	if (n == 0) return 0;

	int mPointAmount = 0;
	int i;
	for (i = 0; i < n; i++) {
		if (tText[i] == '.') mPointAmount++;
		else if (tText[i] >= '0' && tText[i] <= '9') continue;
		else return 0;
	}

	return mPointAmount <= 1;
}

static DreamMugenAssignment* parseFloatConstantFromString(char* tText) {
	double f = atof(tText);

	return makeDreamFloatMugenAssignment(f);
}

static int isStringConstant(char* tText) {
	int n = strlen(tText);
	if (n == 0) return 0;
	return tText[0] == '"' && tText[n - 1] == '"';
}

static DreamMugenAssignment* parseStringConstantFromString(char* tText) {
	if (gMugenAssignmentData.mHasCommandHandlerEntryForLookup) {
		string potentialCommand(tText + 1, strlen(tText + 1) - 1);
		int potentialCommandIndex;
		if (isDreamCommandForLookup(gMugenAssignmentData.mCommandHandlerID, potentialCommand.data(), &potentialCommandIndex)) {
			return makeDreamNumberMugenAssignment(potentialCommandIndex);
		}
	}

	DreamMugenStringAssignment* s = (DreamMugenStringAssignment*)allocMemoryOnMemoryStackOrMemory(sizeof(DreamMugenStringAssignment));
	gDebugAssignmentAmount++;
	s->mValue = (char*)allocMemoryOnMemoryStackOrMemory(strlen(tText + 1) + 10);
	strcpy(s->mValue, tText+1);
	s->mValue[strlen(s->mValue) - 1] = '\0';

	s->mType = MUGEN_ASSIGNMENT_TYPE_STRING;
	return (DreamMugenAssignment*)s;
}

int doDreamAssignmentStringsBeginsWithPattern(const char* tPattern, char* tText) {
	int n = strlen(tPattern);
	int m = strlen(tText);
	if (m < n) return 0;

	int i;
	for (i = 0; i < n; i++) {
		if (tPattern[i] != tText[i]) return 0;
	}

	return 1;
}

extern std::map<std::string, AssignmentReturnValue*(*)(DreamPlayer*)>& getActiveMugenAssignmentVariableMap();

static int isMugenVariable(char* tText) {
	char* text = (char*)allocMemory(strlen(tText) + 2);
	strcpy(text, tText);
	turnStringLowercase(text);

	auto m = getActiveMugenAssignmentVariableMap();
	
	int ret = m.find(text) != m.end();
	freeMemory(text);

	return ret;
}

static DreamMugenAssignment* parseMugenVariableFromString(char* tText) {
	char* text = (char*)allocMemory(strlen(tText) + 2);
	strcpy(text, tText);
	turnStringLowercase(text);
	
	DreamMugenVariableAssignment* data = (DreamMugenVariableAssignment*)allocMemoryOnMemoryStackOrMemory(sizeof(DreamMugenVariableAssignment));
	gDebugAssignmentAmount++;
	auto m = getActiveMugenAssignmentVariableMap();
	data->mFunc = (void*)m[text];
	data->mType = MUGEN_ASSIGNMENT_TYPE_VARIABLE;
	freeMemory(text);
	return (DreamMugenAssignment*)data;
}

static DreamMugenAssignment* parseMugenRawVariableFromString(char* tText) {
	DreamMugenRawVariableAssignment* data = (DreamMugenRawVariableAssignment*)allocMemoryOnMemoryStackOrMemory(sizeof(DreamMugenRawVariableAssignment));
	gDebugAssignmentAmount++;
	data->mName = (char*)allocMemoryOnMemoryStackOrMemory(strlen(tText) + 2);
	strcpy(data->mName, tText);
	turnStringLowercase(data->mName);
	data->mType = MUGEN_ASSIGNMENT_TYPE_RAW_VARIABLE;
	return (DreamMugenAssignment*)data;
}

static void sanitizeTextFront(char** tText) {
	int n = strlen(*tText);
	int i;
	for (i = 0; i < n; i++) {
		if (**tText != ' ' && **tText != '\t') {
			return;
		}

		(*tText)++;
	}
}

static void sanitizeTextBack(char* tText) {
	int n = strlen(tText);

	int i;
	for (i = n - 1; i >= 0; i--) {
		if (tText[i] == ' ' || tText[i] == '\t') tText[i] = '\0';
		else if (tText[i] == ',') tText[i] = '\0';
		else return;
	}
}

static void sanitizeText(char** tText) {
	sanitizeTextFront(tText);
	sanitizeTextBack(*tText);
}


static int isArray(char* tText) {

	int n = strlen(tText);
	char* open = strchr(tText, '(');
	char* close = strrchr(tText, ')');

	return open != NULL && close != NULL && close > open && open != tText && close == (tText + n - 1);
}

static DreamMugenAssignment* parseArrayFromString(char* tText) {
	int posOpen = -1;
	posOpen = strchr(tText, '(') - tText;
	assert(posOpen >= 0);
	assert((strrchr(tText, ')') - tText) >= 0);

	char text1[MUGEN_DEF_STRING_LENGTH];
	char text2[MUGEN_DEF_STRING_LENGTH];

	strcpy(text1, tText);
	text1[posOpen] = '\0';

	char* saneText1 = text1;
	sanitizeText(&saneText1);
	turnStringLowercase(saneText1);

	strcpy(text2, &tText[posOpen+1]);
	char* text2End = strrchr(text2, ')');
	*text2End = '\0';

	DreamMugenAssignment* b = parseDreamMugenAssignmentFromString(text2);
	return makeMugenArrayAssignment(saneText1, b);
}

static int isVectorAssignment(char* tText, int tPotentialCommaPosition) {
	int endPosition = -1;
	while (endPosition < tPotentialCommaPosition) {
		int newPosition;
		if (!isOnHighestLevelWithStartPositionGoingLeftToRight(tText, "=", &newPosition, endPosition + 1, 1)) break;
		if (newPosition >= tPotentialCommaPosition) break;
		endPosition = newPosition;
	}
	endPosition--;
	while (endPosition >= 0 && isEmptyCharacter(tText[endPosition])) {
		endPosition--;
	}
	if (endPosition <= 0) return 0;
	int startPosition = endPosition;
	while (startPosition > 0 && !isEmptyCharacter(tText[startPosition])) {
		startPosition--;
	}
	if (isEmptyCharacter(tText[startPosition])) startPosition++;

	const char* textArea = tText + startPosition;
	char* text = (char*)allocMemory(strlen(textArea) + 2);
	strcpy(text, textArea);
	turnStringLowercase(text);

	int ret;
	if (doDreamAssignmentStringsBeginsWithPattern("animelem", text)) ret = 1;
	else if (doDreamAssignmentStringsBeginsWithPattern("timemod", text)) ret = 1;
	else if (doDreamAssignmentStringsBeginsWithPattern("hitdefattr", text)) ret = 1;
	else ret = 0;

	freeMemory(text);
	return ret;
}

static int isVectorTarget(char* tText) {
	char* text = (char*)allocMemory(strlen(tText) + 2);
	strcpy(text, tText);
	turnStringLowercase(text);

	int ret;
	if (doDreamAssignmentStringsBeginsWithPattern("target", text)) ret = 1;
	else if (doDreamAssignmentStringsBeginsWithPattern("p1", text)) ret = 1;
	else if (doDreamAssignmentStringsBeginsWithPattern("p2", text)) ret = 1;
	else if (doDreamAssignmentStringsBeginsWithPattern("helper", text)) ret = 1;
	else if (doDreamAssignmentStringsBeginsWithPattern("enemy", text)) ret = 1;
	else if (doDreamAssignmentStringsBeginsWithPattern("enemynear", text)) ret = 1;
	else if (doDreamAssignmentStringsBeginsWithPattern("root", text)) ret = 1;
	else if (doDreamAssignmentStringsBeginsWithPattern("playerid", text)) ret = 1;
	else if (doDreamAssignmentStringsBeginsWithPattern("parent", text)) ret = 1;
	else ret = 0;

	freeMemory(text);
	return ret;
}

static int isCommaContextFree(char* tText, int tPosition) {
	assert(tText[tPosition] == ',');
	tPosition--;
	while (tPosition >= 0 && isEmptyCharacter(tText[tPosition])) tPosition--;
	int end = tPosition+1;

	int depth1 = 0;
	while (tPosition >= 0) {
		if (tText[tPosition] == ')') depth1++;
		if (tText[tPosition] == '(') {
			if (!depth1) break;
			else depth1--;
		}
		if (tText[tPosition] == ']') depth1++;
		if (tText[tPosition] == '[') {
			if (!depth1) break;
			else depth1--;
		}

		if (!depth1 && (tText[tPosition] == ',' || isEmptyCharacter(tText[tPosition]) || isOperatorCharacter(tText[tPosition]))) break;
		tPosition--;
	}
	assert(tPosition >= -1);
	int start = tPosition + 1;

	char* prevWord = (char*)allocMemory(strlen(&tText[start]) + 2);
	strcpy(prevWord, &tText[start]);
	int length = end - start;
	prevWord[length] = '\0';

	int ret = !isVectorTarget(prevWord);
	freeMemory(prevWord);
	return ret;
}

static int hasContextFreeComma(char* tText, int* tPosition) {
	int isRunning = 1;
	int position = 0;
	while (isRunning) {
		if (!isOnHighestLevelWithStartPositionGoingLeftToRight(tText, ",", &position, position, 0)) return 0;
		if (isCommaContextFree(tText, position)) {
			if (tPosition) *tPosition = position;
			return 1;
		}

		position++;
	}

	return 0;
}

static int isVector(char* tText) {
	int position;
	return hasContextFreeComma(tText, &position) && !isVectorAssignment(tText, position);
}

static DreamMugenAssignment* parseMugenContextFreeVectorFromString(char* tText) {
	int position;
	hasContextFreeComma(tText, &position);

	return parseTwoElementMugenAssignmentFromStringWithFixedPosition(tText, MUGEN_ASSIGNMENT_TYPE_VECTOR, ",", position);
}

static int isVectorInsideAssignmentOrTargetAccess(char* tText) {
	return isOnHighestLevelLeftToRight(tText, ",", NULL);
}

static DreamMugenAssignment* parseMugenVectorFromString(char* tText) {
	return parseTwoElementLeftToRightMugenAssignmentFromString(tText, MUGEN_ASSIGNMENT_TYPE_VECTOR, ",");
}

static int isOperatorAndReturnType(char* tText, char* tDst) {
	int position;
	int isThere;
	int isDifferent;

	isThere = isOnHighestLevelLeftToRight(tText, ">=", &position);
	isDifferent = strcmp(">=", tText);
	if (isThere && isDifferent && position == 0) {
		strcpy(tDst, ">=");
		return 1;
	}

	isThere = isOnHighestLevelLeftToRight(tText, "<=", &position);
	isDifferent = strcmp("<=", tText);
	if (isThere && isDifferent && position == 0) {
		strcpy(tDst, "<=");
		return 1;
	}

	isThere = isOnHighestLevelLeftToRight(tText, "=", &position);
	isDifferent = strcmp("=", tText);
	if (isThere && isDifferent && position == 0) {
		strcpy(tDst, "=");
		return 1;
	}

	isThere = isOnHighestLevelLeftToRight(tText, "<", &position);
	isDifferent = strcmp("<", tText);
	if (isThere && isDifferent && position == 0 && tText[1] != '=') {
		strcpy(tDst, "<");
		return 1;
	}

	isThere = isOnHighestLevelLeftToRight(tText, ">", &position);
	isDifferent = strcmp(">", tText);
	if (isThere && isDifferent && position == 0 && tText[1] != '=') {
		strcpy(tDst, ">");
		return 1;
	}

	return 0;
}

static int isOperatorArgument(char* tText) {
	char dst[10];

	int ret = isOperatorAndReturnType(tText, dst);
	return ret;
}

static DreamMugenAssignment* parseMugenOperatorArgumentFromString(char* tText) {
	char dst[10];
	char text[200];
	isOperatorAndReturnType(tText, dst);

	sprintf(text, "%s $$ %s", dst, tText + strlen(dst));
	return parseTwoElementLeftToRightMugenAssignmentFromString(text, MUGEN_ASSIGNMENT_TYPE_OPERATOR_ARGUMENT, "$$");
}

DreamMugenAssignment * parseDreamMugenAssignmentFromString(char * tText)
{
	sanitizeText(&tText);

	if (isEmpty(tText)) {
		return parseMugenNullFromString();
	}
	else if (isVector(tText)) {
		return parseMugenContextFreeVectorFromString(tText);
	}
	else if (isOperatorArgument(tText)) {
		return parseMugenOperatorArgumentFromString(tText);
	}
	else if (isOr(tText)) {
		return parseMugenOrFromString(tText);
	}
	else if (isXor(tText)) {
		return parseMugenXorFromString(tText);
	}
	else if (isAnd(tText)) {
		return parseMugenAndFromString(tText);
	}
	else if (isBitwiseOr(tText)) {
		return parseMugenBitwiseOrFromString(tText);
	}
	else if (isBitwiseXor(tText)) {
		return parseMugenBitwiseXorFromString(tText);
	}
	else if (isBitwiseAnd(tText)) {
		return parseMugenBitwiseAndFromString(tText);
	}
	else if (isVariableSet(tText)) {
		return parseMugenVariableSetFromString(tText);
	}
	else if (isComparisonGroup(tText)) {
		return parseMugenComparisonGroupFromString(tText);
	}
	else if (isOrdinalGroup(tText)) {
		return parseMugenOrdinalGroupFromString(tText);
	}
	else if (isAddition(tText)) {
		return parseMugenAdditionFromString(tText);
	}
	else if (isSubtraction(tText)) {
		return parseMugenSubtractionFromString(tText);
	}
	else if (isMultiplicativeGroup(tText)) {
		return parseMugenMultiplicativeGroupFromString(tText);
	}
	else if (isExponentiation(tText)) {
		return parseMugenExponentiationFromString(tText);
	}
	else if (isNegation(tText)) {
		return parseMugenNegationFromString(tText);
	}
	else if (isBitwiseInversion(tText)) {
		return parseMugenBitwiseInversionFromString(tText);
	}
	else if (isUnaryMinus(tText)) {
		return parseMugenUnaryMinusFromString(tText);
	}
	else if (isInBraces(tText)) {
		return parseMugenAssignmentStringInBraces(tText);
	}
	else if (isRange(tText)) {
		return parseMugenRangeFromString(tText);
	}
	else if (isNumericalConstant(tText)) {
		return parseNumericalConstantFromString(tText);
	}
	else if (isFloatConstant(tText)) {
		return parseFloatConstantFromString(tText);
	}
	else if (isStringConstant(tText)) {
		return parseStringConstantFromString(tText);
	}
	else if (isVectorInsideAssignmentOrTargetAccess(tText)) {
		return parseMugenVectorFromString(tText);
	}
	else if (isMugenVariable(tText)) {
		return parseMugenVariableFromString(tText);
	}
	else if (isArray(tText)) {
		return parseArrayFromString(tText);
	}
	else {
		return parseMugenRawVariableFromString(tText);
	}
}

DreamMugenAssignment*  parseDreamMugenAssignmentFromString(const char* tText) {
	char evalText[1024];
	strcpy(evalText, tText);
	return parseDreamMugenAssignmentFromString(evalText);
}

uint8_t fetchDreamAssignmentFromGroupAndReturnWhetherItExists(const char* tName, MugenDefScriptGroup* tGroup, DreamMugenAssignment** tOutput) {
	if (!stl_string_map_contains_array(tGroup->mElements, tName)) {
		*tOutput = NULL;
		return 0;
	}
	MugenDefScriptGroupElement* e = &tGroup->mElements[tName];
	fetchDreamAssignmentFromGroupAsElement(e, tOutput);

	return 1;
}

void fetchDreamAssignmentFromGroupAsElement(MugenDefScriptGroupElement * tElement, DreamMugenAssignment ** tOutput)
{
	char* text = getAllocatedMugenDefStringVariableForAssignmentAsElement(tElement);
	*tOutput = parseDreamMugenAssignmentFromString(text);
	freeMemory(text);
}

