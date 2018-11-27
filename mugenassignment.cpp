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

using namespace std;

int gDebugAssignmentAmount;

static struct {
	MemoryStack* mMemoryStack;

} gData;

void setupDreamAssignmentReader(MemoryStack* tMemoryStack) {
	gData.mMemoryStack = tMemoryStack;
}

void shutdownDreamAssignmentReader()
{
	gData.mMemoryStack = NULL;
}

static void* allocMemoryOnMemoryStackOrMemory(uint32_t tSize) {
	if (gData.mMemoryStack) return allocMemoryOnMemoryStack(gData.mMemoryStack, tSize);
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
	case MUGEN_ASSIGNMENT_TYPE_ARRAY:
		unloadDreamMugenAssignmentDependOnTwo(tAssignment);
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

extern std::map<string, AssignmentReturnValue(*)(DreamMugenAssignment**, DreamPlayer*, int*)>& getActiveMugenAssignmentArrayMap();

static DreamMugenAssignment * makeMugenArrayAssignment(char* tName, DreamMugenAssignment * tIndex)
{
	std::map<string, AssignmentReturnValue(*)(DreamMugenAssignment**, DreamPlayer*, int*)>& m = getActiveMugenAssignmentArrayMap();

	string s = string(tName);
	if (!stl_map_contains(m, s)) {
		logWarningFormat("Unrecognized array %s\n. Defaulting to bottom.", tName);
		return makeDreamFalseMugenAssignment(); // TODO: proper bottom
	}
	AssignmentReturnValue(*func)(DreamMugenAssignment**, DreamPlayer*, int*) = m[s];

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

DreamMugenAssignment * makeDreamStringMugenAssignment(char * tVal)
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

static int isOnHighestLevelWithStartPosition(char* tText, char* tPattern, int* tOptionalPosition, int tStart) {
	int n = strlen(tText);
	int m = strlen(tPattern);

	int depth1 = 0;
	int depth2 = 0;
	int depth3 = 0;
	int i;
	for (i = tStart; i < n - m + 1; i++) {
		if (tText[i] == '(') depth1++;
		if (tText[i] == ')') depth1--;
		if (tText[i] == '[') depth2++;
		if (tText[i] == ']') depth2--;
		if (tText[i] == '"') depth3 ^= 1;

		if (depth1 || depth2 || depth3) continue;

		int isSame = 1;
		int j;
		for (j = 0; j < m; j++) {
			if (tText[i + j] != tPattern[j]) {
				isSame = 0;
				break;
			}
		}

		if (isSame) {
			if (tOptionalPosition) *tOptionalPosition = i;
			return 1;
		}
	}

	return 0;
}

static int isOnHighestLevel(char* tText, char* tPattern, int* tOptionalPosition) {
	return isOnHighestLevelWithStartPosition(tText, tPattern, tOptionalPosition, 0);
}

static DreamMugenAssignment* parseOneElementMugenAssignmentFromString(char* tText, DreamMugenAssignmentType tType) {
	
	DreamMugenAssignment* a = parseDreamMugenAssignmentFromString(tText);

	return makeMugenOneElementAssignment(tType, a);
}

static DreamMugenAssignment* parseTwoElementMugenAssignmentFromStringWithFixedPosition(char* tText, DreamMugenAssignmentType tType, char* tPattern, int tPosition) {
	int n = strlen(tText);
	int m = strlen(tPattern);

	assert(tPosition != -1);
	assert(tPosition < n - m);

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

static DreamMugenAssignment* parseTwoElementMugenAssignmentFromString(char* tText, DreamMugenAssignmentType tType, char* tPattern) {
	int pos = -1;
	isOnHighestLevel(tText, tPattern, &pos);
	return parseTwoElementMugenAssignmentFromStringWithFixedPosition(tText, tType, tPattern, pos);
}

static int isEmpty(char* tChar) {
	return !strcmp("", tChar);
}

static int isEmptyCharacter(char tChar) {
	return tChar == ' ';
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


static int isInequality(char* tText) {
	return isOnHighestLevel(tText, "!=", NULL);
}

static DreamMugenAssignment* parseMugenInequalityFromString(char* tText) {
	return parseTwoElementMugenAssignmentFromString(tText, MUGEN_ASSIGNMENT_TYPE_INEQUALITY, "!=");
}

static int isVariableSet(char* tText) {
	return isOnHighestLevel(tText, ":=", NULL);
}

static DreamMugenAssignment* parseMugenVariableSetFromString(char* tText) {
	return parseTwoElementMugenAssignmentFromString(tText, MUGEN_ASSIGNMENT_TYPE_SET_VARIABLE, ":=");
}

static int isComparison(char* tText) {
	int position;

	// TODO: check for multiple
	if(!isOnHighestLevel(tText, "=", &position)) return 0;
	if (position == 0) return 0;
	if (tText[position - 1] == '!') return 0;
	if (tText[position - 1] == '<') return 0;
	if (tText[position - 1] == '>') return 0;
	if (tText[position - 1] == ':') return 0;

	return 1;
}

static DreamMugenAssignment* parseMugenComparisonFromString(char* tText) {
	return parseTwoElementMugenAssignmentFromString(tText, MUGEN_ASSIGNMENT_TYPE_COMPARISON, "=");
}

static int isAnd(char* tText) {
	return isOnHighestLevel(tText, "&&", NULL);
}

static DreamMugenAssignment* parseMugenAndFromString(char* tText) {
	return parseTwoElementMugenAssignmentFromString(tText, MUGEN_ASSIGNMENT_TYPE_AND, "&&");
}

static int isOr(char* tText) {
	return isOnHighestLevel(tText, "||", NULL);
}

static DreamMugenAssignment* parseMugenOrFromString(char* tText) {
	return parseTwoElementMugenAssignmentFromString(tText, MUGEN_ASSIGNMENT_TYPE_OR, "||");
}

static int isBitwiseAnd(char* tText) {
	return isOnHighestLevel(tText, "&", NULL);
}

static DreamMugenAssignment* parseMugenBitwiseAndFromString(char* tText) {
	return parseTwoElementMugenAssignmentFromString(tText, MUGEN_ASSIGNMENT_TYPE_BITWISE_AND, "&");
}

static int isBitwiseOr(char* tText) {
	return isOnHighestLevel(tText, "|", NULL);
}

static DreamMugenAssignment* parseMugenBitwiseOrFromString(char* tText) {
	return parseTwoElementMugenAssignmentFromString(tText, MUGEN_ASSIGNMENT_TYPE_BITWISE_OR, "|");
}

static int isLessOrEqualThan(char* tText) {
	int position;
	if (!isOnHighestLevel(tText, "<=", &position)) return 0;
	if (position == 0) return 0;

	return 1;
}

static DreamMugenAssignment* parseMugenLessOrEqualFromString(char* tText) {
	return parseTwoElementMugenAssignmentFromString(tText, MUGEN_ASSIGNMENT_TYPE_LESS_OR_EQUAL, "<=");
}

static int isLessThan(char* tText) {
	int position;
	if (!isOnHighestLevel(tText, "<", &position)) return 0;
	if (position == 0) return 0;

	return 1;
}

static DreamMugenAssignment* parseMugenLessFromString(char* tText) {
	return parseTwoElementMugenAssignmentFromString(tText, MUGEN_ASSIGNMENT_TYPE_LESS, "<");
}

static int isGreaterOrEqualThan(char* tText) {
	int position;
	if (!isOnHighestLevel(tText, ">=", &position)) return 0;
	if (position == 0) return 0;

	return 1;
}

static DreamMugenAssignment* parseMugenGreaterOrEqualFromString(char* tText) {
	return parseTwoElementMugenAssignmentFromString(tText, MUGEN_ASSIGNMENT_TYPE_GREATER_OR_EQUAL, ">=");
}

static int isGreaterThan(char* tText) {
	int position;
	if(!isOnHighestLevel(tText, ">", &position)) return 0;
	if (position == 0) return 0;

	return 1;
}

static DreamMugenAssignment* parseMugenGreaterFromString(char* tText) {
	return parseTwoElementMugenAssignmentFromString(tText, MUGEN_ASSIGNMENT_TYPE_GREATER, ">");
}

static int isExponentiation(char* tText) {
	return isOnHighestLevel(tText, "**", NULL);
}

static DreamMugenAssignment* parseMugenExponentiationFromString(char* tText) {
	return parseTwoElementMugenAssignmentFromString(tText, MUGEN_ASSIGNMENT_TYPE_EXPONENTIATION, "**");
}

static int isAddition(char* tText) {
	int plusPosition;
	if(!isOnHighestLevel(tText, "+", &plusPosition)) return 0;
	int len = strlen(tText);
	return plusPosition < len -1;
}

static DreamMugenAssignment* parseMugenAdditionFromString(char* tText) {
	return parseTwoElementMugenAssignmentFromString(tText, MUGEN_ASSIGNMENT_TYPE_ADDITION, "+");
}

static int isOperatorCharacter(char tChar) {
	return tChar == '-' || tChar == '+' || tChar == '|' || tChar == '&' || tChar == '*' || tChar == '/';
}

static int isBinaryOperator(char* tText, int tPosition) {
	int p = tPosition - 1;
	int poss = 0;
	while (p >= 0) {
		if (isEmptyCharacter(tText[p])) p--;
		else if (isOperatorCharacter(tText[p])) return 0;
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
		else if (isOperatorCharacter(tText[p])) return 0;
		else {
			poss = 1;
			break;
		}
	}

	return poss;
}

static int isSubtraction(char* tText) {
	int position;
	if(!isOnHighestLevel(tText, "-", &position)) return 0;
	return isBinaryOperator(tText, position);
}

static DreamMugenAssignment* parseMugenSubtractionFromString(char* tText) {
	return parseTwoElementMugenAssignmentFromString(tText, MUGEN_ASSIGNMENT_TYPE_SUBTRACTION, "-");
}

static int isMultiplication(char* tText) {
	int position = 0;
	int n = strlen(tText);

	// TODO: check for multiple
	if (!isOnHighestLevel(tText, "*", &position)) return 0;
	if (position == 0) return 0;
	if (position < n-1 && tText[position + 1] == '*') return 0;

	return 1;
}

static DreamMugenAssignment* parseMugenMultiplicationFromString(char* tText) {
	return parseTwoElementMugenAssignmentFromString(tText, MUGEN_ASSIGNMENT_TYPE_MULTIPLICATION, "*");
}

static int isDivision(char* tText) {
	return isOnHighestLevel(tText, "/", NULL);
}

static DreamMugenAssignment* parseMugenDivisionFromString(char* tText) {
	return parseTwoElementMugenAssignmentFromString(tText, MUGEN_ASSIGNMENT_TYPE_DIVISION, "/");
}

static int isModulo(char* tText) {
	return isOnHighestLevel(tText, "%", NULL);
}

static DreamMugenAssignment* parseMugenModuloFromString(char* tText) {
	return parseTwoElementMugenAssignmentFromString(tText, MUGEN_ASSIGNMENT_TYPE_MODULO, "%");
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
	DreamMugenStringAssignment* s = (DreamMugenStringAssignment*)allocMemoryOnMemoryStackOrMemory(sizeof(DreamMugenStringAssignment));
	gDebugAssignmentAmount++;
	s->mValue = (char*)allocMemoryOnMemoryStackOrMemory(strlen(tText + 1) + 10);
	strcpy(s->mValue, tText+1);
	s->mValue[strlen(s->mValue) - 1] = '\0';

	s->mType = MUGEN_ASSIGNMENT_TYPE_STRING;
	return (DreamMugenAssignment*)s;
}

int doDreamAssignmentStringsBeginsWithPattern(char* tPattern, char* tText) {
	int n = strlen(tPattern);
	int m = strlen(tText);
	if (m < n) return 0;

	int i;
	for (i = 0; i < n; i++) {
		if (tPattern[i] != tText[i]) return 0;
	}

	return 1;
}

extern std::map<std::string, AssignmentReturnValue(*)(DreamPlayer*)>& getActiveMugenAssignmentVariableMap();

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
	data->mFunc = m[text];
	data->mType = MUGEN_ASSIGNMENT_TYPE_VARIABLE;
	freeMemory(text);
	return (DreamMugenAssignment*)data;
}

static DreamMugenAssignment* parseMugenRawVariableFromString(char* tText) {
	DreamMugenRawVariableAssignment* data = (DreamMugenRawVariableAssignment*)allocMemoryOnMemoryStackOrMemory(sizeof(DreamMugenRawVariableAssignment));
	gDebugAssignmentAmount++;
	data->mName = (char*)allocMemoryOnMemoryStackOrMemory(strlen(tText) + 2);
	strcpy(data->mName, tText);
	data->mType = MUGEN_ASSIGNMENT_TYPE_RAW_VARIABLE;
	return (DreamMugenAssignment*)data;
}

static void sanitizeTextFront(char** tText) {
	int n = strlen(*tText);
	int i;
	for (i = 0; i < n; i++) {
		if (**tText != ' ') {
			return;
		}

		(*tText)++;
	}
}

static void sanitizeTextBack(char* tText) {
	int n = strlen(tText);

	int i;
	for (i = n - 1; i >= 0; i--) {
		if (tText[i] == ' ') tText[i] = '\0';
		else if (tText[i] == ',') tText[i] = '\0'; // TODO: think about trailing commas
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
	int posOpen = -1, posClose = -1;
	posOpen = strchr(tText, '(') - tText;
	posClose = strrchr(tText, ')') - tText;
	assert(posOpen >= 0);
	assert(posClose >= 0);

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

static int isVectorAssignment(char* tText) {
	char* text = (char*)allocMemory(strlen(tText) + 2);
	strcpy(text, tText);
	turnStringLowercase(text);

	int ret;
	if (doDreamAssignmentStringsBeginsWithPattern("animelem", text)) ret = 1;
	else if (doDreamAssignmentStringsBeginsWithPattern("timemod", text)) ret = 1;
	else ret = 0;
	// TODO: properly

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
	// TODO: properly

	freeMemory(text);
	return ret;
}

static int isCommaContextFree(char* tText, int tPosition) {
	assert(tText[tPosition] == ',');
	tPosition--;
	while (tPosition >= 0 && isEmptyCharacter(tText[tPosition])) tPosition--;
	int end = tPosition+1;

	int depth1 = 0;
	int depth2 = 0;
	while (tPosition >= 0) {
		if (tText[tPosition] == ')') depth1++;
		if (tText[tPosition] == '(') {
			if (!depth1) break;
			else depth1--;
		}
		if (tText[tPosition] == ']') depth2++;
		if (tText[tPosition] == '[') {
			if (!depth2) break;
			else depth2--;
		}

		if (!depth1 && !depth2 && (tText[tPosition] == ',' || isEmptyCharacter(tText[tPosition]) || isOperatorCharacter(tText[tPosition]))) break;
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
		if (!isOnHighestLevelWithStartPosition(tText, ",", &position, position)) return 0;
		if (isCommaContextFree(tText, position)) {
			if (tPosition) *tPosition = position;
			return 1;
		}

		position++;
	}

	return 0;
}

static int isVector(char* tText) {
	return hasContextFreeComma(tText, NULL) && !isVectorAssignment(tText);
}

static DreamMugenAssignment* parseMugenContextFreeVectorFromString(char* tText) {
	int position;
	assert(hasContextFreeComma(tText, &position));

	// TODO: handle when second element is gone
	return parseTwoElementMugenAssignmentFromStringWithFixedPosition(tText, MUGEN_ASSIGNMENT_TYPE_VECTOR, ",", position);
}

static int isVectorInsideAssignmentOrTargetAccess(char* tText) {
	return isOnHighestLevel(tText, ",", NULL);
}

static DreamMugenAssignment* parseMugenVectorFromString(char* tText) {
	return parseTwoElementMugenAssignmentFromString(tText, MUGEN_ASSIGNMENT_TYPE_VECTOR, ",");
}

static int isOperatorAndReturnType(char* tText, char* tDst) {
	int position;
	int isThere;
	int isDifferent;

	isThere = isOnHighestLevel(tText, ">=", &position);
	isDifferent = strcmp(">=", tText);
	if (isThere && isDifferent && position == 0) {
		strcpy(tDst, ">=");
		return 1;
	}

	isThere = isOnHighestLevel(tText, "<=", &position);
	isDifferent = strcmp("<=", tText);
	if (isThere && isDifferent && position == 0) {
		strcpy(tDst, "<=");
		return 1;
	}

	isThere = isOnHighestLevel(tText, "=", &position);
	isDifferent = strcmp("=", tText);
	if (isThere && isDifferent && position == 0) {
		strcpy(tDst, "=");
		return 1;
	}

	isThere = isOnHighestLevel(tText, "<", &position);
	isDifferent = strcmp("<", tText);
	if (isThere && isDifferent && position == 0) {
		strcpy(tDst, "<");
		return 1;
	}

	isThere = isOnHighestLevel(tText, ">", &position);
	isDifferent = strcmp(">", tText);
	if (isThere && isDifferent && position == 0) {
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
	assert(isOperatorAndReturnType(tText, dst));

	sprintf(text, "%s $$ %s", dst, tText + strlen(dst));
	return parseTwoElementMugenAssignmentFromString(text, MUGEN_ASSIGNMENT_TYPE_OPERATOR_ARGUMENT, "$$");
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
	else if (isAnd(tText)) {
		return parseMugenAndFromString(tText);
	}
	else if (isBitwiseOr(tText)) {
		return parseMugenBitwiseOrFromString(tText);
	}
	else if (isBitwiseAnd(tText)) {
		return parseMugenBitwiseAndFromString(tText);
	}
	else if (isVariableSet(tText)) {
		return parseMugenVariableSetFromString(tText);
	}
	else if (isInequality(tText)) {
		return parseMugenInequalityFromString(tText);
	}
	else if (isComparison(tText)) {
		return parseMugenComparisonFromString(tText);
	}
	else if (isGreaterOrEqualThan(tText)) {
		return parseMugenGreaterOrEqualFromString(tText);
	}
	else if (isLessOrEqualThan(tText)) {
		return parseMugenLessOrEqualFromString(tText);
	}
	else if (isLessThan(tText)) {
		return parseMugenLessFromString(tText);
	}
	else if (isGreaterThan(tText)) {
		return parseMugenGreaterFromString(tText);
	}
	else if (isAddition(tText)) {
		return parseMugenAdditionFromString(tText);
	}
	else if (isSubtraction(tText)) {
		return parseMugenSubtractionFromString(tText);
	}
	else if (isModulo(tText)) {
		return parseMugenModuloFromString(tText);
	}
	else if (isMultiplication(tText)) {
		return parseMugenMultiplicationFromString(tText);
	}
	else if (isDivision(tText)) {
		return parseMugenDivisionFromString(tText);
	}
	else if (isExponentiation(tText)) {
		return parseMugenExponentiationFromString(tText);
	}
	else if (isNegation(tText)) {
		return parseMugenNegationFromString(tText);
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


int fetchDreamAssignmentFromGroupAndReturnWhetherItExists(char* tName, MugenDefScriptGroup* tGroup, DreamMugenAssignment** tOutput) {
	if (!string_map_contains(&tGroup->mElements, tName)) return 0;

	MugenDefScriptGroupElement* e = (MugenDefScriptGroupElement*)string_map_get(&tGroup->mElements, tName);
	char* text = getAllocatedMugenDefStringVariableForAssignmentAsElement(e);
	*tOutput = parseDreamMugenAssignmentFromString(text);
	freeMemory(text);

	return 1;
}
