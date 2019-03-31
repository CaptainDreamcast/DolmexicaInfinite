#define HAVE_M_PI 

#include "mugenassignmentevaluator.h"

#include <assert.h>

#define _USE_MATH_DEFINES
#include <math.h>

#define LOGGER_WARNINGS_DISABLED

#include <prism/log.h>
#include <prism/system.h>
#include <prism/math.h>
#include <prism/stlutil.h>

#include "gamelogic.h"
#include "stage.h"
#include "playerhitdata.h"
#include "mugenexplod.h"
#include "dolmexicastoryscreen.h"
#include "config.h"

using namespace std;

typedef struct {
	AssignmentReturnType mType;
	char mString[100];
} AssignmentReturnString;

typedef struct {
	AssignmentReturnType mType;
	int mNumber;
} AssignmentReturnNumber;

typedef struct {
	AssignmentReturnType mType;
	double mFloat;
} AssignmentReturnFloat;

typedef struct {
	AssignmentReturnType mType;
	int mBoolean;
} AssignmentReturnBoolean;

typedef struct {
	AssignmentReturnType mType;
	AssignmentReturnValue* a;
	AssignmentReturnValue* b;
} AssignmentReturnVector;

typedef struct {
	AssignmentReturnType mType;
} AssignmentReturnBottom;



static struct {
	int mStackSize;
	AssignmentReturnValue mStack[500]; // TODO: dynamic
	int mFreePointer;

} gAssignmentEvaluator;

static void initEvaluationStack() {
	gAssignmentEvaluator.mStackSize = 500;
	//gAssignmentEvaluator.mStack = (AssignmentReturnValue*)allocMemory(gAssignmentEvaluator.mStackSize * sizeof(AssignmentReturnValue));
}


static void increaseEvaluationStack() {
	gAssignmentEvaluator.mStackSize += 100;
	//gAssignmentEvaluator.mStack = (AssignmentReturnValue*)reallocMemory(gAssignmentEvaluator.mStack, gAssignmentEvaluator.mStackSize * sizeof(AssignmentReturnValue));
}

static AssignmentReturnValue* getFreeAssignmentReturnValue() {
	if (gAssignmentEvaluator.mFreePointer >= gAssignmentEvaluator.mStackSize) {
		increaseEvaluationStack();
	}
	return &gAssignmentEvaluator.mStack[gAssignmentEvaluator.mFreePointer++];
}

static void popAssignmentReturn() {
	gAssignmentEvaluator.mFreePointer--;
}

typedef AssignmentReturnValue*(*VariableFunction)(DreamPlayer*);
typedef AssignmentReturnValue*(*ArrayFunction)(DreamMugenAssignment**, DreamPlayer*, int*);
typedef AssignmentReturnValue*(*ComparisonFunction)(char*, AssignmentReturnValue*, DreamPlayer*, int*);
typedef AssignmentReturnValue*(*OrdinalFunction)(char*, AssignmentReturnValue*, DreamPlayer*, int*, int(*tCompare)(int, int));

static struct {
	map<string, VariableFunction> mVariables;
	map<string, ArrayFunction> mArrays;
	map<string, ComparisonFunction> mComparisons;
	map<string, OrdinalFunction> mOrdinals;
} gVariableHandler;

std::map<string, AssignmentReturnValue*(*)(DreamPlayer*)>& getActiveMugenAssignmentVariableMap() {
	return gVariableHandler.mVariables;
}

std::map<string, AssignmentReturnValue*(*)(DreamMugenAssignment**, DreamPlayer*, int*)>& getActiveMugenAssignmentArrayMap() {
	return gVariableHandler.mArrays;
}

static AssignmentReturnValue* evaluateAssignmentInternal(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* oIsStatic);

static AssignmentReturnValue* evaluateAssignmentDependency(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* tIsStatic) {
	int isDependencyStatic;
	AssignmentReturnValue* ret = evaluateAssignmentInternal(tAssignment, tPlayer, &isDependencyStatic);
	(*tIsStatic) &= isDependencyStatic;
	return ret;
}

static int isFloatReturn(AssignmentReturnValue* tReturn);

static void destroyAssignmentReturn(AssignmentReturnValue* tAssignmentReturn) {
	(void)tAssignmentReturn;
}

static char* getStringAssignmentReturnValue(AssignmentReturnValue* tAssignmentReturn) {
	AssignmentReturnString* string = (AssignmentReturnString*)tAssignmentReturn;
	return string->mString;
}

static int getNumberAssignmentReturnValue(AssignmentReturnValue* tAssignmentReturn) {
	AssignmentReturnNumber* number = (AssignmentReturnNumber*)tAssignmentReturn;
	return number->mNumber;
}

static double getFloatAssignmentReturnValue(AssignmentReturnValue* tAssignmentReturn) {
	AssignmentReturnFloat* f = (AssignmentReturnFloat*)tAssignmentReturn;
	return f->mFloat;
}

static int getBooleanAssignmentReturnValue(AssignmentReturnValue* tAssignmentReturn) {
	AssignmentReturnBoolean* boolean = (AssignmentReturnBoolean*)tAssignmentReturn;
	return boolean->mBoolean;
}

static AssignmentReturnValue* getVectorAssignmentReturnFirstDependency(AssignmentReturnValue* tAssignmentReturn) {
	AssignmentReturnVector* vector = (AssignmentReturnVector*)tAssignmentReturn;
	return vector->a;
}

static AssignmentReturnValue* getVectorAssignmentReturnSecondDependency(AssignmentReturnValue* tAssignmentReturn) {
	AssignmentReturnVector* vector = (AssignmentReturnVector*)tAssignmentReturn;
	return vector->b;
}

static AssignmentReturnValue* makeBooleanAssignmentReturn(int tValue);


static char* convertAssignmentReturnToAllocatedString(AssignmentReturnValue* tAssignmentReturn) {
	char* ret;
	char buffer[100]; // TODO: without buffer

	char* string, *valueVA, *valueVB;
	int valueI;
	double valueF;
	switch (tAssignmentReturn->mType) {
	case MUGEN_ASSIGNMENT_RETURN_TYPE_STRING:
		string = getStringAssignmentReturnValue(tAssignmentReturn);
		ret = copyToAllocatedString(string);
		break;
	case MUGEN_ASSIGNMENT_RETURN_TYPE_NUMBER:
		valueI = getNumberAssignmentReturnValue(tAssignmentReturn);
		sprintf(buffer, "%d", valueI);
		ret = copyToAllocatedString(buffer);
		break;
	case MUGEN_ASSIGNMENT_RETURN_TYPE_FLOAT:
		valueF = getFloatAssignmentReturnValue(tAssignmentReturn);
		sprintf(buffer, "%f", valueF);
		ret = copyToAllocatedString(buffer);
		break;
		ret = copyToAllocatedString(buffer);
	case MUGEN_ASSIGNMENT_RETURN_TYPE_BOOLEAN:
		valueI = getBooleanAssignmentReturnValue(tAssignmentReturn);
		sprintf(buffer, "%d", valueI);
		ret = copyToAllocatedString(buffer);
		break;
	case MUGEN_ASSIGNMENT_RETURN_TYPE_VECTOR:
		valueVA = convertAssignmentReturnToAllocatedString(getVectorAssignmentReturnFirstDependency(tAssignmentReturn));
		valueVB = convertAssignmentReturnToAllocatedString(getVectorAssignmentReturnSecondDependency(tAssignmentReturn));
		sprintf(buffer, "%s , %s", valueVA, valueVB);
		freeMemory(valueVA);
		freeMemory(valueVB);
		ret = copyToAllocatedString(buffer);
		break;
	case MUGEN_ASSIGNMENT_RETURN_TYPE_RANGE:
		valueVA = convertAssignmentReturnToAllocatedString(getVectorAssignmentReturnFirstDependency(tAssignmentReturn));
		valueVB = convertAssignmentReturnToAllocatedString(getVectorAssignmentReturnSecondDependency(tAssignmentReturn));
		sprintf(buffer, "[ %s , %s ]", valueVA, valueVB);
		freeMemory(valueVA);
		freeMemory(valueVB);
		ret = copyToAllocatedString(buffer);
		break;
	default:
		*buffer = '\0';
		ret = copyToAllocatedString(buffer);
		break;
	}

	destroyAssignmentReturn(tAssignmentReturn);

	return ret;
}

static int convertAssignmentReturnToBool(AssignmentReturnValue* tAssignmentReturn) {
	int ret;

	char* string;
	int valueI;
	double valueF;
	switch (tAssignmentReturn->mType) {
	case MUGEN_ASSIGNMENT_RETURN_TYPE_STRING:
		string = getStringAssignmentReturnValue(tAssignmentReturn);
		ret = strcmp("", string);
		ret &= strcmp("0", string);
		break;
	case MUGEN_ASSIGNMENT_RETURN_TYPE_NUMBER:
		valueI = getNumberAssignmentReturnValue(tAssignmentReturn);
		ret = valueI;
		break;
	case MUGEN_ASSIGNMENT_RETURN_TYPE_FLOAT:
		valueF = getFloatAssignmentReturnValue(tAssignmentReturn);
		ret = (int)valueF;
		break;
	case MUGEN_ASSIGNMENT_RETURN_TYPE_BOOLEAN:
		valueI = getBooleanAssignmentReturnValue(tAssignmentReturn);
		ret = valueI;
		break;
	case MUGEN_ASSIGNMENT_RETURN_TYPE_VECTOR:
	case MUGEN_ASSIGNMENT_RETURN_TYPE_RANGE:
		ret = 1;
		break;
	default:
		ret = 0;
		break;
	}

	destroyAssignmentReturn(tAssignmentReturn);

	return ret;
}


static int convertAssignmentReturnToNumber(AssignmentReturnValue* tAssignmentReturn) {
	int ret;

	char* string;
	int valueI;
	double valueF;
	switch (tAssignmentReturn->mType) {
	case MUGEN_ASSIGNMENT_RETURN_TYPE_STRING:
		string = getStringAssignmentReturnValue(tAssignmentReturn);
		ret = atoi(string);
		break;
	case MUGEN_ASSIGNMENT_RETURN_TYPE_NUMBER:
		valueI = getNumberAssignmentReturnValue(tAssignmentReturn);
		ret = valueI;
		break;
	case MUGEN_ASSIGNMENT_RETURN_TYPE_FLOAT:
		valueF = getFloatAssignmentReturnValue(tAssignmentReturn);
		ret = (int)valueF;
		break;
	case MUGEN_ASSIGNMENT_RETURN_TYPE_BOOLEAN:
		valueI = getBooleanAssignmentReturnValue(tAssignmentReturn);
		ret = valueI;
		break;
	case MUGEN_ASSIGNMENT_RETURN_TYPE_VECTOR:
	case MUGEN_ASSIGNMENT_RETURN_TYPE_RANGE:
		ret = 0;
		break;
	default:
		ret = 0;
		break;
	}

	destroyAssignmentReturn(tAssignmentReturn);

	return ret;
}

static double convertAssignmentReturnToFloat(AssignmentReturnValue* tAssignmentReturn) {
	double ret;

	char* string;
	int valueI;
	double valueF;
	switch (tAssignmentReturn->mType) {
	case MUGEN_ASSIGNMENT_RETURN_TYPE_STRING:
		string = getStringAssignmentReturnValue(tAssignmentReturn);
		ret = atof(string);
		break;
	case MUGEN_ASSIGNMENT_RETURN_TYPE_NUMBER:
		valueI = getNumberAssignmentReturnValue(tAssignmentReturn);
		ret = valueI;
		break;
	case MUGEN_ASSIGNMENT_RETURN_TYPE_FLOAT:
		valueF = getFloatAssignmentReturnValue(tAssignmentReturn);
		ret = valueF;
		break;
	case MUGEN_ASSIGNMENT_RETURN_TYPE_BOOLEAN:
		valueI = getBooleanAssignmentReturnValue(tAssignmentReturn);
		ret = valueI;
		break;
	case MUGEN_ASSIGNMENT_RETURN_TYPE_VECTOR:
	case MUGEN_ASSIGNMENT_RETURN_TYPE_RANGE:
		ret = 0;
		break;
	default:
		ret = 0;
		break;
	}

	destroyAssignmentReturn(tAssignmentReturn);

	return ret;
}


static AssignmentReturnValue* makeBooleanAssignmentReturn(int tValue) {
	AssignmentReturnValue* val = getFreeAssignmentReturnValue();
	AssignmentReturnBoolean* ret = (AssignmentReturnBoolean*)val;
	ret->mType = MUGEN_ASSIGNMENT_RETURN_TYPE_BOOLEAN;
	ret->mBoolean = tValue;
	return val;
}

static AssignmentReturnValue* makeNumberAssignmentReturn(int tValue) {
	AssignmentReturnValue* val = getFreeAssignmentReturnValue();
	AssignmentReturnNumber* ret = (AssignmentReturnNumber*)val;
	ret->mType = MUGEN_ASSIGNMENT_RETURN_TYPE_NUMBER;
	ret->mNumber = tValue;
	return val;
}

static AssignmentReturnValue* makeFloatAssignmentReturn(double tValue) {
	AssignmentReturnValue* val = getFreeAssignmentReturnValue();
	AssignmentReturnFloat* ret = (AssignmentReturnFloat*)val;
	ret->mType = MUGEN_ASSIGNMENT_RETURN_TYPE_FLOAT;
	ret->mFloat = tValue;
	return val;
}

static AssignmentReturnValue* makeStringAssignmentReturn(const char* tValue) {
	AssignmentReturnValue* val = getFreeAssignmentReturnValue();
	AssignmentReturnString* ret = (AssignmentReturnString*)val;
	ret->mType = MUGEN_ASSIGNMENT_RETURN_TYPE_STRING;
	strcpy(ret->mString, tValue);
	return val;
}

static AssignmentReturnValue* makeVectorAssignmentReturn(AssignmentReturnValue* a, AssignmentReturnValue* b) {
	AssignmentReturnValue* val = getFreeAssignmentReturnValue();
	AssignmentReturnVector* ret = (AssignmentReturnVector*)val;
	ret->mType = MUGEN_ASSIGNMENT_RETURN_TYPE_VECTOR;
	ret->a = a;
	ret->b = b;
	return val;
}

static AssignmentReturnValue* makeRangeAssignmentReturn(AssignmentReturnValue* a, AssignmentReturnValue* b) {
	AssignmentReturnValue* val = getFreeAssignmentReturnValue();
	AssignmentReturnVector* ret = (AssignmentReturnVector*)val;
	ret->mType = MUGEN_ASSIGNMENT_RETURN_TYPE_RANGE;
	ret->a = a;
	ret->b = b;
	return val;
}

static AssignmentReturnValue* makeBottomAssignmentReturn() {
	AssignmentReturnValue* val = getFreeAssignmentReturnValue();
	AssignmentReturnBottom* ret = (AssignmentReturnBottom*)val;
	ret->mType = MUGEN_ASSIGNMENT_RETURN_TYPE_BOTTOM;
	return val;
}

static AssignmentReturnValue* evaluateOrAssignment(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* tIsStatic) {
	DreamMugenDependOnTwoAssignment* orAssignment = (DreamMugenDependOnTwoAssignment*)*tAssignment;

	AssignmentReturnValue* a = evaluateAssignmentDependency(&orAssignment->a, tPlayer, tIsStatic);
	int valA = convertAssignmentReturnToBool(a);
	if (valA) return makeBooleanAssignmentReturn(valA);

	AssignmentReturnValue* b = evaluateAssignmentDependency(&orAssignment->b, tPlayer, tIsStatic);
	int valB = convertAssignmentReturnToBool(b);

	return makeBooleanAssignmentReturn(valB);
}

static AssignmentReturnValue* evaluateAndAssignment(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* tIsStatic) {
	DreamMugenDependOnTwoAssignment* andAssignment = (DreamMugenDependOnTwoAssignment*)*tAssignment;

	AssignmentReturnValue* a = evaluateAssignmentDependency(&andAssignment->a, tPlayer, tIsStatic);
	int valA = convertAssignmentReturnToBool(a);
	if (!valA) return makeBooleanAssignmentReturn(valA);

	AssignmentReturnValue* b = evaluateAssignmentDependency(&andAssignment->b, tPlayer, tIsStatic);
	int valB = convertAssignmentReturnToBool(b);

	return makeBooleanAssignmentReturn(valB);
}

static AssignmentReturnValue* evaluateBitwiseOrAssignment(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* tIsStatic) {
	DreamMugenDependOnTwoAssignment* bitwiseOrAssignment = (DreamMugenDependOnTwoAssignment*)*tAssignment;

	AssignmentReturnValue* a = evaluateAssignmentDependency(&bitwiseOrAssignment->a, tPlayer, tIsStatic);
	int valA = convertAssignmentReturnToNumber(a);

	AssignmentReturnValue* b = evaluateAssignmentDependency(&bitwiseOrAssignment->b, tPlayer, tIsStatic);
	int valB = convertAssignmentReturnToNumber(b);

	return makeNumberAssignmentReturn(valA | valB);
}

static AssignmentReturnValue* evaluateBitwiseAndAssignment(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* tIsStatic) {
	DreamMugenDependOnTwoAssignment* bitwiseAndAssignment = (DreamMugenDependOnTwoAssignment*)*tAssignment;

	AssignmentReturnValue* a = evaluateAssignmentDependency(&bitwiseAndAssignment->a, tPlayer, tIsStatic);
	int valA = convertAssignmentReturnToNumber(a);

	AssignmentReturnValue* b = evaluateAssignmentDependency(&bitwiseAndAssignment->b, tPlayer, tIsStatic);
	int valB = convertAssignmentReturnToNumber(b);

	return makeNumberAssignmentReturn(valA & valB);
}

static AssignmentReturnValue* evaluateCommandAssignment(AssignmentReturnValue* tCommand, DreamPlayer* tPlayer, int* tIsStatic) {
	char* string = convertAssignmentReturnToAllocatedString(tCommand);
	int ret = isPlayerCommandActive(tPlayer, string);
	freeMemory(string);

	*tIsStatic = 0;
	return makeBooleanAssignmentReturn(ret);
}

static AssignmentReturnValue* evaluateStateTypeAssignment(AssignmentReturnValue* tCommand, DreamPlayer* tPlayer, int* tIsStatic) {
	DreamMugenStateType playerState = getPlayerStateType(tPlayer);
	char* test = convertAssignmentReturnToAllocatedString(tCommand);
	turnStringLowercase(test);

	int ret;
	if (playerState == MUGEN_STATE_TYPE_STANDING) ret = strchr(test, 's') != NULL;
	else if (playerState == MUGEN_STATE_TYPE_AIR) ret = strchr(test, 'a') != NULL;
	else if (playerState == MUGEN_STATE_TYPE_CROUCHING) ret = strchr(test, 'c') != NULL;
	else if (playerState == MUGEN_STATE_TYPE_LYING) ret = strchr(test, 'l') != NULL;
	else {
		logWarningFormat("Undefined player state %d. Default to false.", playerState);
		ret = 0;
	}
	freeMemory(test);

	*tIsStatic = 0;
	return makeBooleanAssignmentReturn(ret);
}

static AssignmentReturnValue* evaluateMoveTypeAssignment(AssignmentReturnValue* tCommand, DreamPlayer* tPlayer, int* tIsStatic) {
	DreamMugenStateMoveType playerMoveType = getPlayerStateMoveType(tPlayer);
	char* test = convertAssignmentReturnToAllocatedString(tCommand);
	turnStringLowercase(test);

	int ret;
	if (playerMoveType == MUGEN_STATE_MOVE_TYPE_ATTACK) ret = strchr(test, 'a') != NULL;
	else if (playerMoveType == MUGEN_STATE_MOVE_TYPE_BEING_HIT) ret = strchr(test, 'h') != NULL;
	else if (playerMoveType == MUGEN_STATE_MOVE_TYPE_IDLE) ret = strchr(test, 'i') != NULL;
	else {
		logWarningFormat("Undefined player state %d. Default to false.", playerMoveType);
		ret = 0;
	}
	freeMemory(test);

	*tIsStatic = 0;
	return makeBooleanAssignmentReturn(ret);
}

static AssignmentReturnValue* evaluateAnimElemVectorAssignment(AssignmentReturnValue* tCommand, DreamPlayer* tPlayer, int* tIsStatic) {

	auto numberReturn = getVectorAssignmentReturnFirstDependency(tCommand);
	auto operAndNumberReturn = getVectorAssignmentReturnSecondDependency(tCommand);

	int ret;
	int animationStep = convertAssignmentReturnToNumber(numberReturn);
	if (operAndNumberReturn->mType == MUGEN_ASSIGNMENT_RETURN_TYPE_VECTOR) {
		auto operReturn = getVectorAssignmentReturnFirstDependency(operAndNumberReturn);
		auto secondNumberReturn = getVectorAssignmentReturnSecondDependency(operAndNumberReturn);
		int time = convertAssignmentReturnToNumber(secondNumberReturn);
		int timeTillAnimation = getPlayerTimeFromAnimationElement(tPlayer, animationStep);
		int stepTime = getPlayerAnimationTimeWhenStepStarts(tPlayer, animationStep);
		if (!isPlayerAnimationTimeOffsetInAnimation(tPlayer, stepTime + time)) {
			time = 0; // TODO: fix to total animation time
		}

		if (operReturn->mType == MUGEN_ASSIGNMENT_RETURN_TYPE_STRING) {
			char* oper = getStringAssignmentReturnValue(operReturn);
			if (!strcmp("=", oper)) {
				ret = timeTillAnimation == time;
			}
			else if (!strcmp("!=", oper)) {
				ret = timeTillAnimation != time;
			}
			else if (!strcmp("<", oper)) {
				ret = timeTillAnimation < time;
			}
			else if (!strcmp(">", oper)) {
				ret = timeTillAnimation > time;
			}
			else if (!strcmp("<=", oper)) {
				ret = timeTillAnimation <= time;
			}
			else if (!strcmp(">=", oper)) {
				ret = timeTillAnimation >= time;
			}
			else {
				logWarningFormat("Unrecognized operator %s. Default to false.", oper);
				ret = 0;
			}
		}
		else {
			logWarningFormat("Unrecognized operator %s. Default to false.", oper);
			ret = 0;
		}

	}
	else {
		int time = convertAssignmentReturnToNumber(operAndNumberReturn);

		int timeTillAnimation = getPlayerTimeFromAnimationElement(tPlayer, animationStep);
		ret = time == timeTillAnimation;
	}

	*tIsStatic = 0;
	return makeBooleanAssignmentReturn(ret);
}

static AssignmentReturnValue* evaluateAnimElemNumberAssignment(AssignmentReturnValue* tCommand, DreamPlayer* tPlayer, int* tIsStatic) {

	int elem = convertAssignmentReturnToNumber(tCommand);
	int ret = isPlayerStartingAnimationElementWithID(tPlayer, elem);
	
	*tIsStatic = 0;
	return makeBooleanAssignmentReturn(ret);
}

static AssignmentReturnValue* evaluateAnimElemAssignment(AssignmentReturnValue* tCommand, DreamPlayer* tPlayer, int* tIsStatic) {

	if (tCommand->mType == MUGEN_ASSIGNMENT_RETURN_TYPE_VECTOR) {
		return evaluateAnimElemVectorAssignment(tCommand, tPlayer, tIsStatic);
	}
	else {
		return evaluateAnimElemNumberAssignment(tCommand, tPlayer, tIsStatic);
	}
}

static AssignmentReturnValue* evaluateAnimElemOrdinalAssignment(AssignmentReturnValue* tCommand, DreamPlayer* tPlayer, int* tIsStatic, int(*tCompareFunction)(int, int)) {
	int elem = convertAssignmentReturnToNumber(tCommand);
	int time = getPlayerTimeFromAnimationElement(tPlayer, elem);

	return makeBooleanAssignmentReturn(tCompareFunction(time, 0));
}

static AssignmentReturnValue* evaluateTimeModAssignment(AssignmentReturnValue* tCommand, DreamPlayer* tPlayer, int* tIsStatic) {

	if (tCommand->mType != MUGEN_ASSIGNMENT_RETURN_TYPE_VECTOR) {
		logWarningFormat("Unable to parse timemod assignment %s. Defaulting to bottom.", tCommand.mValue);
		return makeBottomAssignmentReturn();
	}

	int divisorValue = convertAssignmentReturnToNumber(getVectorAssignmentReturnFirstDependency(tCommand));
	int compareValue = convertAssignmentReturnToNumber(getVectorAssignmentReturnSecondDependency(tCommand));
	int stateTime = getPlayerTimeInState(tPlayer);
	
	if (divisorValue == 0) {
		return makeBooleanAssignmentReturn(0);
	}

	int modValue = stateTime % divisorValue;
	int ret = modValue == compareValue;
	*tIsStatic = 0;
	return makeBooleanAssignmentReturn(ret);
}

static DreamPlayer* getPlayerFromFirstVectorPartOrNullIfNonexistant(AssignmentReturnValue* a, DreamPlayer* tPlayer) {
	if (a->mType != MUGEN_ASSIGNMENT_RETURN_TYPE_STRING) return NULL;
	char* test = getStringAssignmentReturnValue(a);

	char firstWord[100];
	int id;

	int items = sscanf(test, "%s %d", firstWord, &id);
	turnStringLowercase(firstWord);

	if (items == 1 && !strcmp("p1", firstWord)) {
		return getRootPlayer(0);
	}
	else if (items == 1 && !strcmp("p2", firstWord)) {
		return getRootPlayer(1);
	}
	else if (items == 1 && !strcmp("target", firstWord)) {
		return getPlayerOtherPlayer(tPlayer); // TODO
	}
	else if (items == 1 && !strcmp("enemy", firstWord)) {
		return getPlayerOtherPlayer(tPlayer); // TODO
	}
	else if (items == 1 && !strcmp("enemynear", firstWord)) {
		return getPlayerOtherPlayer(tPlayer); // TODO
	}
	else if (items == 1 && !strcmp("root", firstWord)) {
		return getPlayerRoot(tPlayer);
	}
	else if (items == 1 && !strcmp("parent", firstWord)) {
		return getPlayerParent(tPlayer);
	}
	else if (items == 2 && !strcmp("helper", firstWord)) {
		DreamPlayer* ret = getPlayerHelperOrNullIfNonexistant(tPlayer, id);
		if (!ret) {
			logWarningFormat("Unable to find helper with id %d. Returning NULL.", id);
		}
		return ret;
	}
	else if (items == 2 && !strcmp("playerid", firstWord)) {
		DreamPlayer* ret = getPlayerByIDOrNullIfNonexistant(tPlayer, id);
		if (!ret) {
			logWarningFormat("Unable to find helper with id %d. Returning NULL.", id);
		}
		return ret;
	}
	else {
		return NULL;
	}
}

static int isPlayerAccessVectorAssignment(AssignmentReturnValue* a, DreamPlayer* tPlayer) {
	return getPlayerFromFirstVectorPartOrNullIfNonexistant(a, tPlayer) != NULL;
}

static AssignmentReturnValue* evaluateTeamModeAssignment(AssignmentReturnValue* tCommand, DreamPlayer* tPlayer, int* tIsStatic) {
	(void)tPlayer;

	char* test = convertAssignmentReturnToAllocatedString(tCommand); 
	turnStringLowercase(test);
	int ret = !strcmp(test, "single"); // TODO
	freeMemory(test);

	(void)tIsStatic;
	return makeBooleanAssignmentReturn(ret); 
}

static int isRangeAssignmentReturn(AssignmentReturnValue* ret) {
	return ret->mType == MUGEN_ASSIGNMENT_RETURN_TYPE_RANGE;
}

static AssignmentReturnValue* evaluateRangeComparisonAssignment(AssignmentReturnValue* a, AssignmentReturnValue* tRange) {
	if (!isRangeAssignmentReturn(tRange)) {
#ifndef LOGGER_WARNINGS_DISABLED
		char* test = convertAssignmentReturnToAllocatedString(a);
		logWarningFormat("Unable to parse range comparison assignment %s. Defaulting to bottom.", test);
		freeMemory(test);
#endif
		return makeBottomAssignmentReturn();
	}

	int val = convertAssignmentReturnToNumber(a);

	int val1 = convertAssignmentReturnToNumber(getVectorAssignmentReturnFirstDependency(tRange));
	int val2 = convertAssignmentReturnToNumber(getVectorAssignmentReturnSecondDependency(tRange));

	return makeBooleanAssignmentReturn(val >= val1 && val <= val2);
}

static int isFloatReturn(AssignmentReturnValue* tReturn) {
	return tReturn->mType == MUGEN_ASSIGNMENT_RETURN_TYPE_FLOAT;
}

static int isStringReturn(AssignmentReturnValue* tReturn) {
	return tReturn->mType == MUGEN_ASSIGNMENT_RETURN_TYPE_STRING;
}

static AssignmentReturnValue* varFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic);
static AssignmentReturnValue* sysVarFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic);
static AssignmentReturnValue* fVarFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic); 
static AssignmentReturnValue* sysFVarFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic);

static AssignmentReturnValue* evaluateSetVariableAssignment(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* tIsStatic) {
	DreamMugenDependOnTwoAssignment* varSetAssignment = (DreamMugenDependOnTwoAssignment*)*tAssignment;
	
	if (varSetAssignment->a->mType != MUGEN_ASSIGNMENT_TYPE_ARRAY){
		logWarningFormat("Incorrect varset type %d. Defaulting to bottom.", varSetAssignment->a->mType);
		return makeBottomAssignmentReturn(); 
	}
	DreamMugenArrayAssignment* varArrayAccess = (DreamMugenArrayAssignment*)varSetAssignment->a;
	int index = evaluateDreamAssignmentAndReturnAsInteger(&varArrayAccess->mIndex, tPlayer);

	ArrayFunction func = (ArrayFunction)varArrayAccess->mFunc;
	AssignmentReturnValue* ret = NULL;
	if (func == varFunction) {
		int value = evaluateDreamAssignmentAndReturnAsInteger(&varSetAssignment->b, tPlayer);
		setPlayerVariable(tPlayer, index, value);
		ret = makeNumberAssignmentReturn(value);
	}
	else if (func == fVarFunction) {
		double value = evaluateDreamAssignmentAndReturnAsFloat(&varSetAssignment->b, tPlayer);
		setPlayerFloatVariable(tPlayer, index, value);
		ret = makeFloatAssignmentReturn(value);
	}
	else if (func == sysVarFunction) {
		int value = evaluateDreamAssignmentAndReturnAsInteger(&varSetAssignment->b, tPlayer);
		setPlayerSystemVariable(tPlayer, index, value);
		ret = makeNumberAssignmentReturn(value);
	}
	else if (func == sysFVarFunction) {
		double value = evaluateDreamAssignmentAndReturnAsFloat(&varSetAssignment->b, tPlayer);
		setPlayerSystemFloatVariable(tPlayer, index, value);
		ret = makeFloatAssignmentReturn(value);
	}
	else {
		logWarningFormat("Unrecognized varset function %X. Returning bottom.", (void*)func);
		ret = makeBottomAssignmentReturn(); 
	}
	assert(ret);

	*tIsStatic = 0;
	return ret;
}

static int evaluateSingleHitDefAttributeFlag2(char* tFlag, MugenAttackClass tClass, MugenAttackType tType) {
	turnStringLowercase(tFlag);

	if (strlen(tFlag) != 2) {
		logWarningFormat("Invalid hitdef attribute flag %s. Default to false.", tFlag);
		return 0;
	}

	int isPart1OK;
	if (tClass == MUGEN_ATTACK_CLASS_NORMAL) {
		isPart1OK = tFlag[0] == 'n';
	} else if (tClass == MUGEN_ATTACK_CLASS_SPECIAL) {
		isPart1OK = tFlag[0] == 's';
	}
	else if (tClass == MUGEN_ATTACK_CLASS_HYPER) {
		isPart1OK = tFlag[0] == 'h';
	}
	else {
		logWarningFormat("Unrecognized attack class %d. Default to false.", tClass);
		isPart1OK = 0;
	}

	if (!isPart1OK) return 0;

	int isPart2OK;
	if (tType == MUGEN_ATTACK_TYPE_ATTACK) {
		isPart2OK = tFlag[1] == 'a';
	}
	else if (tType == MUGEN_ATTACK_TYPE_PROJECTILE) {
		isPart2OK = tFlag[1] == 'p';
	}
	else if (tType == MUGEN_ATTACK_TYPE_THROW) {
		isPart2OK = tFlag[1] == 't';
	}
	else {
		logWarningFormat("Unrecognized attack type %d. Default to false.", tType);
		isPart2OK = 0;
	}

	return isPart2OK;
}

static AssignmentReturnValue* evaluateHitDefAttributeAssignment(AssignmentReturnValue* tValue, DreamPlayer* tPlayer, int* tIsStatic) {

	if (!isHitDataActive(tPlayer)) {
		destroyAssignmentReturn(tValue);
		return makeBooleanAssignmentReturn(0);
	}

	char buffer[100]; // TODO: think about non-messy way to do dynamic
	char* test = convertAssignmentReturnToAllocatedString(tValue);
	strcpy(buffer, test);
	freeMemory(test);
	char* pos = buffer;
	int positionsRead;
	char flag[10], comma[10];
	int items = sscanf(pos, "%s %s%n", flag, comma, &positionsRead);
	pos += positionsRead;

	turnStringLowercase(flag);

	int isFlag1OK;
	DreamMugenStateType type = getHitDataType(tPlayer);
	*tIsStatic = 0;

	if (type == MUGEN_STATE_TYPE_STANDING) {
		isFlag1OK = strchr(flag, 's') != NULL;
	}
	else if (type == MUGEN_STATE_TYPE_CROUCHING) {
		isFlag1OK = strchr(flag, 'c') != NULL;
	}
	else if (type == MUGEN_STATE_TYPE_AIR) {
		isFlag1OK = strchr(flag, 'a') != NULL;
	}
	else {
		logWarningFormat("Invalid hitdef type %d. Default to false.", type);
		isFlag1OK = 0;
	}

	if (!isFlag1OK) return makeBooleanAssignmentReturn(0);

	if (items == 1) return makeBooleanAssignmentReturn(isFlag1OK);
	if (strcmp(",", comma)) {
		logWarningFormat("Invalid hitdef attribute string %s. Defaulting to bottom.", tValue.mValue);
		return makeBottomAssignmentReturn(); 
	}

	int hasNext = 1;
	while (hasNext) {
		items = sscanf(pos, "%s %s%n", flag, comma, &positionsRead);
		if (items < 1) {
			logWarningFormat("Error reading next flag %s. Defaulting to bottom.", pos);
			return makeBottomAssignmentReturn();
		}
		pos += positionsRead;


		int isFlag2OK = evaluateSingleHitDefAttributeFlag2(flag, getHitDataAttackClass(tPlayer), getHitDataAttackType(tPlayer));
		if (isFlag2OK) {
			return makeBottomAssignmentReturn();
		}

		if (items == 1) hasNext = 0;
		else if (strcmp(",", comma)) {
			logWarningFormat("Invalid hitdef attribute string %s. Defaulting to bottom.", tValue.mValue);
			return makeBottomAssignmentReturn(); 
		}
	}

	return makeBooleanAssignmentReturn(0);
}

// TODO: merge with AnimElem
static AssignmentReturnValue* evaluateProjVectorAssignment(AssignmentReturnValue* tCommand, DreamPlayer* tPlayer, int tProjectileID, int(*tTimeFunc)(DreamPlayer*, int), int* tIsStatic) {
	auto numberReturn = getVectorAssignmentReturnFirstDependency(tCommand);
	auto operAndNumberReturn = getVectorAssignmentReturnSecondDependency(tCommand);

	int ret;
	int compareValue = convertAssignmentReturnToNumber(numberReturn);
	if (operAndNumberReturn->mType == MUGEN_ASSIGNMENT_RETURN_TYPE_VECTOR) {
		auto operReturn = getVectorAssignmentReturnFirstDependency(operAndNumberReturn);
		auto secondNumberReturn = getVectorAssignmentReturnSecondDependency(operAndNumberReturn);
		int time = convertAssignmentReturnToNumber(secondNumberReturn);

		if (operReturn->mType == MUGEN_ASSIGNMENT_RETURN_TYPE_STRING) {
			char* oper = getStringAssignmentReturnValue(operReturn);
			int timeOffset = tTimeFunc(tPlayer, tProjectileID);

			if (!strcmp("=", oper)) {
				ret = timeOffset == time;
			}
			else if (!strcmp("!=", oper)) {
				ret = timeOffset != time;
			}
			else if (!strcmp("<", oper)) {
				ret = timeOffset < time;
			}
			else if (!strcmp(">", oper)) {
				ret = timeOffset > time;
			}
			else if (!strcmp("<=", oper)) {
				ret = timeOffset <= time;
			}
			else if (!strcmp(">=", oper)) {
				ret = timeOffset >= time;
			}
			else {
				logWarningFormat("Unrecognized operator %s. Default to false.", oper);
				ret = compareValue + 1;
			}
		}
		else {
			logWarningFormat("Unrecognized operator %s. Default to false.", oper);
			ret = compareValue + 1;
		}

	}
	else {
		int time = convertAssignmentReturnToNumber(operAndNumberReturn);

		int timeOffset = tTimeFunc(tPlayer, tProjectileID);
		ret = time == timeOffset;
	}

	*tIsStatic = 0;
	return makeBooleanAssignmentReturn(ret == compareValue);
}

static AssignmentReturnValue* evaluateProjNumberAssignment(AssignmentReturnValue* tCommand, DreamPlayer* tPlayer, int tProjectileID, int(*tTimeFunc)(DreamPlayer*, int), int* tIsStatic) {

	int compareValue = convertAssignmentReturnToNumber(tCommand);
	int timeOffset = tTimeFunc(tPlayer, tProjectileID);
	int ret = timeOffset == 1; // TODO: check

	*tIsStatic = 0;
	return makeBooleanAssignmentReturn(ret == compareValue);
}

int getProjectileIDFromAssignmentName(char* tName, char* tBaseName) {
	char* idOffset = tName += strlen(tBaseName);
	if (*idOffset == '\0') return 0;

	return atoi(idOffset);
}

static AssignmentReturnValue* evaluateProjAssignment(char* tName, char* tBaseName, AssignmentReturnValue* tCommand, DreamPlayer* tPlayer, int(*tTimeFunc)(DreamPlayer*, int), int* tIsStatic) {
	int projID = getProjectileIDFromAssignmentName(tName, tBaseName);

	if (tCommand->mType == MUGEN_ASSIGNMENT_RETURN_TYPE_VECTOR) {
		return evaluateProjVectorAssignment(tCommand, tPlayer, projID, tTimeFunc, tIsStatic);
	}
	else {
		return evaluateProjNumberAssignment(tCommand, tPlayer, projID, tTimeFunc, tIsStatic);
	}
}

static int isProjAssignment(char* tName, char* tBaseName) {
	return !strncmp(tName, tBaseName, strlen(tBaseName));

}

static int tryEvaluateVariableComparison(DreamMugenRawVariableAssignment* tVariableAssignment, AssignmentReturnValue** oRet, AssignmentReturnValue* b, DreamPlayer* tPlayer, int* tIsStatic) {

	int hasReturn = 0;
	if(!strcmp("command", tVariableAssignment->mName)) {
		hasReturn = 1;
		if(b->mType != MUGEN_ASSIGNMENT_RETURN_TYPE_STRING) {
			*oRet = makeBooleanAssignmentReturn(0);
		} else {
			char* comp = getStringAssignmentReturnValue(b);
			*oRet = makeBooleanAssignmentReturn(isPlayerCommandActive(tPlayer, comp));
		}	
		*tIsStatic = 0;
	}
	else if (stl_string_map_contains_array(gVariableHandler.mComparisons, tVariableAssignment->mName)) {
		ComparisonFunction func = gVariableHandler.mComparisons[tVariableAssignment->mName];
		hasReturn = 1;
		*oRet = func(tVariableAssignment->mName, b, tPlayer, tIsStatic);
	}
	else if (isProjAssignment(tVariableAssignment->mName, "projcontact")) {
		hasReturn = 1;
		*oRet = evaluateProjAssignment(tVariableAssignment->mName, "projcontact", b, tPlayer, getPlayerProjectileTimeSinceContact, tIsStatic);
	}
	else if (isProjAssignment(tVariableAssignment->mName, "projguarded")) {
		hasReturn = 1;
		*oRet = evaluateProjAssignment(tVariableAssignment->mName, "projguarded", b, tPlayer, getPlayerProjectileTimeSinceGuarded, tIsStatic);
	}
	else if (isProjAssignment(tVariableAssignment->mName, "projhit")) {
		hasReturn = 1;
		*oRet = evaluateProjAssignment(tVariableAssignment->mName, "projhit", b, tPlayer, getPlayerProjectileTimeSinceHit, tIsStatic);
	}

	return hasReturn;
}

static int tryEvaluateVariableComparisonOrNegation(DreamMugenAssignment** tAssignment, AssignmentReturnValue** oRet, AssignmentReturnValue* b, DreamPlayer* tPlayer, int* tIsStatic) {
	if ((*tAssignment)->mType == MUGEN_ASSIGNMENT_TYPE_NEGATION) {
		DreamMugenDependOnOneAssignment* neg = (DreamMugenDependOnOneAssignment*)(*tAssignment);
		if (neg->a->mType == MUGEN_ASSIGNMENT_TYPE_RAW_VARIABLE) {
			DreamMugenRawVariableAssignment* negVar = (DreamMugenRawVariableAssignment*)neg->a;
			AssignmentReturnValue* retVal = NULL;
			if (tryEvaluateVariableComparison(negVar, &retVal, b, tPlayer, tIsStatic)) {
				*oRet = makeBooleanAssignmentReturn(!convertAssignmentReturnToBool(retVal));
				return 1;
			}
		}
	}
	else if ((*tAssignment)->mType == MUGEN_ASSIGNMENT_TYPE_RAW_VARIABLE) {
		DreamMugenRawVariableAssignment* var = (DreamMugenRawVariableAssignment*)*tAssignment;
		AssignmentReturnValue* retVal = NULL;
		if (tryEvaluateVariableComparison(var, &retVal, b, tPlayer, tIsStatic)) {
			*oRet = retVal;
			return 1;
		}
	}

	return 0;
}

static AssignmentReturnValue* evaluateComparisonAssignmentInternal(DreamMugenAssignment** mAssignment, AssignmentReturnValue* b, DreamPlayer* tPlayer, int* tIsStatic) {

	AssignmentReturnValue* retVal = NULL;
	if (tryEvaluateVariableComparisonOrNegation(mAssignment, &retVal, b, tPlayer, tIsStatic)) {
		return retVal;
	}

	AssignmentReturnValue* a = evaluateAssignmentDependency(mAssignment, tPlayer, tIsStatic);
	if (isRangeAssignmentReturn(b)) {
		return evaluateRangeComparisonAssignment(a, b);
	}
	else if (isFloatReturn(a) || isFloatReturn(b)) {
		int value = convertAssignmentReturnToFloat(a) == convertAssignmentReturnToFloat(b);
		return makeBooleanAssignmentReturn(value);
	}
	else if (!isStringReturn(a) && !isStringReturn(b)) {
		int value = convertAssignmentReturnToNumber(a) == convertAssignmentReturnToNumber(b);
		return makeBooleanAssignmentReturn(value);
	}
	else {
		char* val1 = convertAssignmentReturnToAllocatedString(a);
		char* val2 = convertAssignmentReturnToAllocatedString(b);
		int value = !strcmp(val1, val2);
		freeMemory(val1);
		freeMemory(val2);
		return makeBooleanAssignmentReturn(value);
	}
}

static AssignmentReturnValue* evaluateComparisonAssignment(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* tIsStatic) {
	DreamMugenDependOnTwoAssignment* comparisonAssignment = (DreamMugenDependOnTwoAssignment*)*tAssignment;


	if (comparisonAssignment->a->mType == MUGEN_ASSIGNMENT_TYPE_VECTOR) {
		DreamMugenDependOnTwoAssignment* vectorAssignment = (DreamMugenDependOnTwoAssignment*)comparisonAssignment->a;
		AssignmentReturnValue* vecA = evaluateAssignmentDependency(&vectorAssignment->a, tPlayer, tIsStatic);
		if (isPlayerAccessVectorAssignment(vecA, tPlayer)) {
			DreamPlayer* target = getPlayerFromFirstVectorPartOrNullIfNonexistant(vecA, tPlayer);
			destroyAssignmentReturn(vecA);
			if (!isPlayerTargetValid(target)) {
				logWarning("Accessed player was NULL. Defaulting to bottom.");
				return makeBottomAssignmentReturn(); 
			}
			AssignmentReturnValue* b = evaluateAssignmentDependency(&comparisonAssignment->b, tPlayer, tIsStatic);
			return evaluateComparisonAssignmentInternal(&vectorAssignment->b, b, target, tIsStatic);
		}
		else destroyAssignmentReturn(vecA);
	}

	AssignmentReturnValue* b = evaluateAssignmentDependency(&comparisonAssignment->b, tPlayer, tIsStatic);
	return evaluateComparisonAssignmentInternal(&comparisonAssignment->a, b, tPlayer, tIsStatic);
}

static AssignmentReturnValue* evaluateInequalityAssignment(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* tIsStatic) {
	AssignmentReturnValue* equal = evaluateComparisonAssignment(tAssignment, tPlayer, tIsStatic);
	int val = convertAssignmentReturnToBool(equal);

	return makeBooleanAssignmentReturn(!val);
}

static int tryEvaluateVariableOrdinalInternal(DreamMugenRawVariableAssignment* tVariableAssignment, AssignmentReturnValue** oRet, AssignmentReturnValue* b, DreamPlayer* tPlayer, int* tIsStatic, int(*tCompareFunction)(int, int)) {

	if (stl_string_map_contains_array(gVariableHandler.mOrdinals, tVariableAssignment->mName)) {
		OrdinalFunction func = gVariableHandler.mOrdinals[tVariableAssignment->mName];
		*oRet = func(tVariableAssignment->mName, b, tPlayer, tIsStatic, tCompareFunction);
		return 1;
	}

	return 0;
}

static int tryEvaluateVariableOrdinal(DreamMugenAssignment** tAssignment, AssignmentReturnValue** oRet, AssignmentReturnValue* b, DreamPlayer* tPlayer, int* tIsStatic, int(*tCompareFunction)(int, int)) {
	if ((*tAssignment)->mType == MUGEN_ASSIGNMENT_TYPE_RAW_VARIABLE) {
		DreamMugenRawVariableAssignment* var = (DreamMugenRawVariableAssignment*)*tAssignment;
		AssignmentReturnValue* retVal = NULL;
		if (tryEvaluateVariableOrdinalInternal(var, &retVal, b, tPlayer, tIsStatic, tCompareFunction)) {
			*oRet = retVal;
			return 1;
		}
	}

	return 0;
}

static int greaterFunc(int a, int b) { return a > b; }
static int greaterEqualFunc(int a, int b) { return a >= b; }
static int lesserFunc(int a, int b) { return a < b; }
static int lesserEqualFunc(int a, int b) { return a <= b; }


static AssignmentReturnValue* evaluateGreaterIntegers(AssignmentReturnValue* a, AssignmentReturnValue* b) {
	int val = convertAssignmentReturnToNumber(a) > convertAssignmentReturnToNumber(b);
	return makeBooleanAssignmentReturn(val);
}

static AssignmentReturnValue* evaluateGreaterFloats(AssignmentReturnValue* a, AssignmentReturnValue* b) {
	int val = convertAssignmentReturnToFloat(a) > convertAssignmentReturnToFloat(b);
	return makeBooleanAssignmentReturn(val);
}


static AssignmentReturnValue* evaluateGreaterAssignment(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* tIsStatic) {
	DreamMugenDependOnTwoAssignment* greaterAssignment = (DreamMugenDependOnTwoAssignment*)*tAssignment;
	AssignmentReturnValue* b = evaluateAssignmentDependency(&greaterAssignment->b, tPlayer, tIsStatic);

	AssignmentReturnValue* retVal = NULL;
	if (tryEvaluateVariableOrdinal(&greaterAssignment->a, &retVal, b, tPlayer, tIsStatic, greaterFunc)) {
		return retVal;
	}

	AssignmentReturnValue* a = evaluateAssignmentDependency(&greaterAssignment->a, tPlayer, tIsStatic);

	if (isFloatReturn(a) || isFloatReturn(b)) {
		return evaluateGreaterFloats(a, b);
	}
	else {
		return evaluateGreaterIntegers(a, b);
	}
}

static AssignmentReturnValue* evaluateGreaterOrEqualIntegers(AssignmentReturnValue* a, AssignmentReturnValue* b) {
	int val = convertAssignmentReturnToNumber(a) >= convertAssignmentReturnToNumber(b);
	return makeBooleanAssignmentReturn(val);
}

static AssignmentReturnValue* evaluateGreaterOrEqualFloats(AssignmentReturnValue* a, AssignmentReturnValue* b) {
	int val = convertAssignmentReturnToFloat(a) >= convertAssignmentReturnToFloat(b);
	return makeBooleanAssignmentReturn(val);
}

static AssignmentReturnValue* evaluateGreaterOrEqualAssignment(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* tIsStatic) {
	DreamMugenDependOnTwoAssignment* greaterOrEqualAssignment = (DreamMugenDependOnTwoAssignment*)*tAssignment;
	AssignmentReturnValue* b = evaluateAssignmentDependency(&greaterOrEqualAssignment->b, tPlayer, tIsStatic);

	AssignmentReturnValue* retVal = NULL;
	if (tryEvaluateVariableOrdinal(&greaterOrEqualAssignment->a, &retVal, b, tPlayer, tIsStatic, greaterEqualFunc)) {
		return retVal;
	}

	AssignmentReturnValue* a = evaluateAssignmentDependency(&greaterOrEqualAssignment->a, tPlayer, tIsStatic);

	if (isFloatReturn(a) || isFloatReturn(b)) {
		return evaluateGreaterOrEqualFloats(a, b);
	}
	else {
		return evaluateGreaterOrEqualIntegers(a, b);
	}
}

static AssignmentReturnValue* evaluateLessIntegers(AssignmentReturnValue* a, AssignmentReturnValue* b) {
	int val = convertAssignmentReturnToNumber(a) < convertAssignmentReturnToNumber(b);
	return makeBooleanAssignmentReturn(val);
}

static AssignmentReturnValue* evaluateLessFloats(AssignmentReturnValue* a, AssignmentReturnValue* b) {
	int val = convertAssignmentReturnToFloat(a) < convertAssignmentReturnToFloat(b);
	return makeBooleanAssignmentReturn(val);
}

static AssignmentReturnValue* evaluateLessAssignment(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* tIsStatic) {
	DreamMugenDependOnTwoAssignment* lessAssignment = (DreamMugenDependOnTwoAssignment*)*tAssignment;
	AssignmentReturnValue* b = evaluateAssignmentDependency(&lessAssignment->b, tPlayer, tIsStatic);

	AssignmentReturnValue* retVal = NULL;
	if (tryEvaluateVariableOrdinal(&lessAssignment->a, &retVal, b, tPlayer, tIsStatic, lesserFunc)) {
		return retVal;
	}

	AssignmentReturnValue* a = evaluateAssignmentDependency(&lessAssignment->a, tPlayer, tIsStatic);

	if (isFloatReturn(a) || isFloatReturn(b)) {
		return evaluateLessFloats(a, b);
	}
	else {
		return evaluateLessIntegers(a, b);
	}
}

static AssignmentReturnValue* evaluateLessOrEqualIntegers(AssignmentReturnValue* a, AssignmentReturnValue* b) {
	int val = convertAssignmentReturnToNumber(a) <= convertAssignmentReturnToNumber(b);
	return makeBooleanAssignmentReturn(val);
}

static AssignmentReturnValue* evaluateLessOrEqualFloats(AssignmentReturnValue* a, AssignmentReturnValue* b) {
	int val = convertAssignmentReturnToFloat(a) <= convertAssignmentReturnToFloat(b);
	return makeBooleanAssignmentReturn(val);
}

static AssignmentReturnValue* evaluateLessOrEqualAssignment(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* tIsStatic) {
	DreamMugenDependOnTwoAssignment* lessOrEqualAssignment = (DreamMugenDependOnTwoAssignment*)*tAssignment;
	AssignmentReturnValue* b = evaluateAssignmentDependency(&lessOrEqualAssignment->b, tPlayer, tIsStatic);

	AssignmentReturnValue* retVal = NULL;
	if (tryEvaluateVariableOrdinal(&lessOrEqualAssignment->a, &retVal, b, tPlayer, tIsStatic, lesserEqualFunc)) {
		return retVal;
	}

	AssignmentReturnValue* a = evaluateAssignmentDependency(&lessOrEqualAssignment->a, tPlayer, tIsStatic);

	if (isFloatReturn(a) || isFloatReturn(b)) {
		return evaluateLessOrEqualFloats(a, b);
	}
	else {
		return evaluateLessOrEqualIntegers(a, b);
	}
}

static AssignmentReturnValue* evaluateModuloIntegers(AssignmentReturnValue* a, AssignmentReturnValue* b) {
	int val1 = convertAssignmentReturnToNumber(a);
	int val2 = convertAssignmentReturnToNumber(b);
	if (!val2) return makeBottomAssignmentReturn();

	int val = val1 % val2;
	return makeNumberAssignmentReturn(val);
}

static AssignmentReturnValue* evaluateModuloAssignment(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* tIsStatic) {
	DreamMugenDependOnTwoAssignment* moduloAssignment = (DreamMugenDependOnTwoAssignment*)*tAssignment;
	AssignmentReturnValue* a = evaluateAssignmentDependency(&moduloAssignment->a, tPlayer, tIsStatic);
	AssignmentReturnValue* b = evaluateAssignmentDependency(&moduloAssignment->b, tPlayer, tIsStatic);

	if (isFloatReturn(a) || isFloatReturn(b)) {
		logWarningFormat("Unable to parse modulo of floats %f and %f. Returning bottom.", convertAssignmentReturnToFloat(a), convertAssignmentReturnToFloat(b));
		return makeBottomAssignmentReturn();
	}
	else {
		return evaluateModuloIntegers(a, b);
	}
}

static int powI(int a, int b) {
	if (b < 0) {
			logWarningFormat("Invalid power function %d^%d. Returning 1.", a, b);
			return 1;
	}

	if (a == 0 && b == 0) return 1;

	int ret = 1;
	while (b--) ret *= a;

	return ret;
}

static AssignmentReturnValue* evaluateExponentiationIntegers(AssignmentReturnValue* a, AssignmentReturnValue* b) {
	int val1 = convertAssignmentReturnToNumber(a);
	int val2 = convertAssignmentReturnToNumber(b);
	return makeNumberAssignmentReturn(powI(val1, val2));
}

static AssignmentReturnValue* evaluateExponentiationFloats(AssignmentReturnValue* a, AssignmentReturnValue* b) {
	double val1 = convertAssignmentReturnToFloat(a);
	double val2 = convertAssignmentReturnToFloat(b);
	return makeFloatAssignmentReturn(pow(val1, val2));
}

static AssignmentReturnValue* evaluateExponentiationAssignment(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* tIsStatic) {
	DreamMugenDependOnTwoAssignment* exponentiationAssignment = (DreamMugenDependOnTwoAssignment*)*tAssignment;
	AssignmentReturnValue* a = evaluateAssignmentDependency(&exponentiationAssignment->a, tPlayer, tIsStatic);
	AssignmentReturnValue* b = evaluateAssignmentDependency(&exponentiationAssignment->b, tPlayer, tIsStatic);

	if (isFloatReturn(a) || isFloatReturn(b) || convertAssignmentReturnToNumber(b) < 0) {
		return evaluateExponentiationFloats(a, b);
	}
	else {
		return evaluateExponentiationIntegers(a, b);
	}
}


static AssignmentReturnValue* evaluateMultiplicationIntegers(AssignmentReturnValue* a, AssignmentReturnValue* b) {
	int val = convertAssignmentReturnToNumber(a) * convertAssignmentReturnToNumber(b);
	return makeNumberAssignmentReturn(val);
}

static AssignmentReturnValue* evaluateMultiplicationFloats(AssignmentReturnValue* a, AssignmentReturnValue* b) {
	double val = convertAssignmentReturnToFloat(a) * convertAssignmentReturnToFloat(b);
	return makeFloatAssignmentReturn(val);
}

static AssignmentReturnValue* evaluateMultiplicationAssignment(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* tIsStatic) {
	DreamMugenDependOnTwoAssignment* multiplicationAssignment = (DreamMugenDependOnTwoAssignment*)*tAssignment;
	AssignmentReturnValue* a = evaluateAssignmentDependency(&multiplicationAssignment->a, tPlayer, tIsStatic);
	AssignmentReturnValue* b = evaluateAssignmentDependency(&multiplicationAssignment->b, tPlayer, tIsStatic);

	if (isFloatReturn(a) || isFloatReturn(b)) {
		return evaluateMultiplicationFloats(a, b);
	}
	else {
		return evaluateMultiplicationIntegers(a, b);
	}
}

static AssignmentReturnValue* evaluateDivisionIntegers(AssignmentReturnValue* a, AssignmentReturnValue* b) {
	int val1 = convertAssignmentReturnToNumber(a);
	int val2 = convertAssignmentReturnToNumber(b);
	if (!val2) return makeBottomAssignmentReturn();
	int val = val1 / val2;
	return makeNumberAssignmentReturn(val);
}

static AssignmentReturnValue* evaluateDivisionFloats(AssignmentReturnValue* a, AssignmentReturnValue* b) {
	double val = convertAssignmentReturnToFloat(a) / convertAssignmentReturnToFloat(b);
	return makeFloatAssignmentReturn(val);
}

static AssignmentReturnValue* evaluateDivisionAssignment(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* tIsStatic) {
	DreamMugenDependOnTwoAssignment* divisionAssignment = (DreamMugenDependOnTwoAssignment*)*tAssignment;
	AssignmentReturnValue* a = evaluateAssignmentDependency(&divisionAssignment->a, tPlayer, tIsStatic);
	AssignmentReturnValue* b = evaluateAssignmentDependency(&divisionAssignment->b, tPlayer, tIsStatic);

	if (isFloatReturn(a) || isFloatReturn(b)) {
		return evaluateDivisionFloats(a, b);
	}
	else {
		return evaluateDivisionIntegers(a, b);
	}
}

static int isSparkFileReturn(AssignmentReturnValue* a) {
	if (a->mType != MUGEN_ASSIGNMENT_RETURN_TYPE_STRING) return 0;

	char* test = getStringAssignmentReturnValue(a);
	char firstW[200];
	int items = sscanf(test, "%s", firstW);
	if (!items) return 0;
	turnStringLowercase(firstW);

	return !strcmp("isinotherfilef", firstW) || !strcmp("isinotherfiles", firstW);
}

static AssignmentReturnValue* evaluateAdditionSparkFile(AssignmentReturnValue* a, AssignmentReturnValue* b) {
	char firstW[200];
	int val1;

	char* test = convertAssignmentReturnToAllocatedString(a);
	int items = sscanf(test, "%s %d", firstW, &val1);
	freeMemory(test);

	int val2 = convertAssignmentReturnToNumber(b);

	if (items != 2) {
		logWarningFormat("Unable to parse sparkfile addition %s. Defaulting to bottom.", test);
		return makeBottomAssignmentReturn();
	}

	char buffer[100]; // TODO: dynamic
	sprintf(buffer, "%s %d", firstW, val1+val2);

	return makeStringAssignmentReturn(buffer);
}


static AssignmentReturnValue* evaluateAdditionIntegers(AssignmentReturnValue* a, AssignmentReturnValue* b) {
	int val = convertAssignmentReturnToNumber(a) + convertAssignmentReturnToNumber(b);
	return makeNumberAssignmentReturn(val);
}

static AssignmentReturnValue* evaluateAdditionFloats(AssignmentReturnValue* a, AssignmentReturnValue* b) {
	double val = convertAssignmentReturnToFloat(a) + convertAssignmentReturnToFloat(b);
	return makeFloatAssignmentReturn(val);
}

static AssignmentReturnValue* evaluateAdditionAssignment(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* tIsStatic) {
	DreamMugenDependOnTwoAssignment* additionAssignment = (DreamMugenDependOnTwoAssignment*)*tAssignment;
	AssignmentReturnValue* a = evaluateAssignmentDependency(&additionAssignment->a, tPlayer, tIsStatic);
	AssignmentReturnValue* b = evaluateAssignmentDependency(&additionAssignment->b, tPlayer, tIsStatic);

	if (isSparkFileReturn(a)) {
		return evaluateAdditionSparkFile(a, b);
	}
	else if (isFloatReturn(a) || isFloatReturn(b)) {
		return evaluateAdditionFloats(a, b);
	}
	else {
		return evaluateAdditionIntegers(a, b);
	}
}


static AssignmentReturnValue* evaluateSubtractionIntegers(AssignmentReturnValue* a, AssignmentReturnValue* b) {
	int val = convertAssignmentReturnToNumber(a) - convertAssignmentReturnToNumber(b);
	return makeNumberAssignmentReturn(val);
}

static AssignmentReturnValue* evaluateSubtractionFloats(AssignmentReturnValue* a, AssignmentReturnValue* b) {
	double val = convertAssignmentReturnToFloat(a) - convertAssignmentReturnToFloat(b);
	return makeFloatAssignmentReturn(val);
}

static AssignmentReturnValue* evaluateSubtractionAssignment(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* tIsStatic) {
	DreamMugenDependOnTwoAssignment* subtractionAssignment = (DreamMugenDependOnTwoAssignment*)*tAssignment;
	AssignmentReturnValue* a = evaluateAssignmentDependency(&subtractionAssignment->a, tPlayer, tIsStatic);
	AssignmentReturnValue* b = evaluateAssignmentDependency(&subtractionAssignment->b, tPlayer, tIsStatic);

	if (isFloatReturn(a) || isFloatReturn(b)) {
		return evaluateSubtractionFloats(a, b);
	}
	else {
		return evaluateSubtractionIntegers(a, b);
	}
}


static AssignmentReturnValue* evaluateOperatorArgumentAssignment(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* tIsStatic) {
	DreamMugenDependOnTwoAssignment* operatorAssignment = (DreamMugenDependOnTwoAssignment*)*tAssignment;
	AssignmentReturnValue* a = evaluateAssignmentDependency(&operatorAssignment->a, tPlayer, tIsStatic);
	AssignmentReturnValue* b = evaluateAssignmentDependency(&operatorAssignment->b, tPlayer, tIsStatic);

	return makeVectorAssignmentReturn(a, b);
}

static AssignmentReturnValue* evaluateBooleanAssignment(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* tIsStatic) {
	(void)tPlayer;
	(void)tIsStatic;
	DreamMugenFixedBooleanAssignment* fixedAssignment = (DreamMugenFixedBooleanAssignment*)*tAssignment;

	return makeBooleanAssignmentReturn(fixedAssignment->mValue);
}

static AssignmentReturnValue* evaluateNumberAssignment(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* tIsStatic) {
	(void)tPlayer;
	(void)tIsStatic;
	DreamMugenNumberAssignment* number = (DreamMugenNumberAssignment*)*tAssignment;

	return makeNumberAssignmentReturn(number->mValue);
}

static AssignmentReturnValue* evaluateFloatAssignment(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* tIsStatic) {
	(void)tPlayer;
	(void)tIsStatic;
	DreamMugenFloatAssignment* f = (DreamMugenFloatAssignment*)*tAssignment;

	return makeFloatAssignmentReturn(f->mValue);
}

static AssignmentReturnValue* evaluateStringAssignment(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* tIsStatic) {
	(void)tPlayer;
	(void)tIsStatic;
	DreamMugenStringAssignment* s = (DreamMugenStringAssignment*)*tAssignment;

	return makeStringAssignmentReturn(s->mValue);
}

static AssignmentReturnValue* evaluatePlayerVectorAssignment(AssignmentReturnValue* tFirstValue, DreamMugenDependOnTwoAssignment* tVectorAssignment, DreamPlayer* tPlayer, int* tIsStatic) {
	DreamPlayer* target = getPlayerFromFirstVectorPartOrNullIfNonexistant(tFirstValue, tPlayer);
	destroyAssignmentReturn(tFirstValue);
	if (!isPlayerTargetValid(target)) {
		logWarning("Unable to evaluate player vector assignment with NULL. Defaulting to bottom."); // TODO: debug
		return makeBottomAssignmentReturn(); 
	}
	if (tVectorAssignment->b->mType != MUGEN_ASSIGNMENT_TYPE_VARIABLE && tVectorAssignment->b->mType != MUGEN_ASSIGNMENT_TYPE_RAW_VARIABLE && tVectorAssignment->b->mType != MUGEN_ASSIGNMENT_TYPE_ARRAY) {
		logWarningFormat("Invalid player vector assignment type %d. Defaulting to bottom.", tVectorAssignment->b->mType);
		return makeBottomAssignmentReturn();
	}

	return evaluateAssignmentDependency(&tVectorAssignment->b, target, tIsStatic);
}

static AssignmentReturnValue* evaluateVectorAssignment(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* tIsStatic) {
	DreamMugenDependOnTwoAssignment* vectorAssignment = (DreamMugenDependOnTwoAssignment*)*tAssignment;
	AssignmentReturnValue* a = evaluateAssignmentDependency(&vectorAssignment->a, tPlayer, tIsStatic);

	if (isPlayerAccessVectorAssignment(a, tPlayer)) {
		return evaluatePlayerVectorAssignment(a, vectorAssignment, tPlayer, tIsStatic);
	}
	else {
		AssignmentReturnValue* b = evaluateAssignmentDependency(&vectorAssignment->b, tPlayer, tIsStatic);
		return makeVectorAssignmentReturn(a, b);
	}
}

static AssignmentReturnValue* evaluateRangeAssignment(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* tIsStatic) {
	DreamMugenRangeAssignment* rangeAssignment = (DreamMugenRangeAssignment*)*tAssignment;
	AssignmentReturnValue* a = evaluateAssignmentDependency(&rangeAssignment->a, tPlayer, tIsStatic);

	if (a->mType != MUGEN_ASSIGNMENT_RETURN_TYPE_VECTOR) {
#ifndef LOGGER_WARNINGS_DISABLED
		char* test = convertAssignmentReturnToAllocatedString(a);
		logWarningFormat("Unable to parse range assignment %s. Defaulting to bottom.", test);
		freeMemory(test);
#endif
		return makeBottomAssignmentReturn();
	}

	auto val1Return = getVectorAssignmentReturnFirstDependency(a);
	auto val2Return = getVectorAssignmentReturnSecondDependency(a);

	int val1 = convertAssignmentReturnToNumber(val1Return);
	int val2 = convertAssignmentReturnToNumber(val2Return);
	if (rangeAssignment->mExcludeLeft) {
		val1Return = makeNumberAssignmentReturn(val1 + 1);
	}
	if (rangeAssignment->mExcludeRight) {
		val2Return = makeNumberAssignmentReturn(val2 - 1);
	}

	return makeRangeAssignmentReturn(val1Return, val2Return);
}

static AssignmentReturnValue* commandComparisonFunction(char* tName, AssignmentReturnValue* b, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateCommandAssignment(b, tPlayer, tIsStatic); }
static AssignmentReturnValue* stateTypeComparisonFunction(char* tName, AssignmentReturnValue* b, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateStateTypeAssignment(b, tPlayer, tIsStatic); }
static AssignmentReturnValue* p2StateTypeComparisonFunction(char* tName, AssignmentReturnValue* b, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateStateTypeAssignment(b, getPlayerOtherPlayer(tPlayer), tIsStatic); }
static AssignmentReturnValue* moveTypeComparisonFunction(char* tName, AssignmentReturnValue* b, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateMoveTypeAssignment(b, tPlayer, tIsStatic); }
static AssignmentReturnValue* p2MoveTypeComparisonFunction(char* tName, AssignmentReturnValue* b, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateMoveTypeAssignment(b, getPlayerOtherPlayer(tPlayer), tIsStatic); }
static AssignmentReturnValue* animElemComparisonFunction(char* tName, AssignmentReturnValue* b, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateAnimElemAssignment(b, tPlayer, tIsStatic); }
static AssignmentReturnValue* timeModComparisonFunction(char* tName, AssignmentReturnValue* b, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateTimeModAssignment(b, tPlayer, tIsStatic); }
static AssignmentReturnValue* teamModeComparisonFunction(char* tName, AssignmentReturnValue* b, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateTeamModeAssignment(b, tPlayer, tIsStatic); }
static AssignmentReturnValue* hitDefAttributeComparisonFunction(char* tName, AssignmentReturnValue* b, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateHitDefAttributeAssignment(b, tPlayer, tIsStatic); }


static AssignmentReturnValue* animElemOrdinalFunction(char* tName, AssignmentReturnValue* b, DreamPlayer* tPlayer, int* tIsStatic, int(*tCompareFunction)(int, int)) { return evaluateAnimElemOrdinalAssignment(b, tPlayer, tIsStatic, tCompareFunction); }

static void setupComparisons() {
	gVariableHandler.mComparisons.clear();

	gVariableHandler.mComparisons["command"] = commandComparisonFunction;
	gVariableHandler.mComparisons["statetype"] = stateTypeComparisonFunction;
	gVariableHandler.mComparisons["p2statetype"] = p2StateTypeComparisonFunction;
	gVariableHandler.mComparisons["movetype"] = moveTypeComparisonFunction;
	gVariableHandler.mComparisons["p2movetype"] = p2MoveTypeComparisonFunction;
	gVariableHandler.mComparisons["animelem"] = animElemComparisonFunction;
	gVariableHandler.mComparisons["timemod"] = timeModComparisonFunction;
	gVariableHandler.mComparisons["teammode"] = teamModeComparisonFunction;
	gVariableHandler.mComparisons["hitdefattr"] = hitDefAttributeComparisonFunction;

	gVariableHandler.mOrdinals.clear();
	gVariableHandler.mOrdinals["animelem"] = animElemOrdinalFunction;

}

static AssignmentReturnValue* aiLevelFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerAILevel(tPlayer)); }
static AssignmentReturnValue* aliveFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(isPlayerAlive(tPlayer)); }
static AssignmentReturnValue* animFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerAnimationNumber(tPlayer)); }
//static AssignmentReturnValue* animElemFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(0); }
static AssignmentReturnValue* animTimeFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerAnimationTimeDeltaUntilFinished(tPlayer)); }
static AssignmentReturnValue* authorNameFunction(DreamPlayer* tPlayer) { return makeStringAssignmentReturn(getPlayerAuthorName(tPlayer)); }
static AssignmentReturnValue* backEdgeFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerScreenEdgeInBackX(tPlayer)); }
static AssignmentReturnValue* backEdgeBodyDistFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerBackBodyDistanceToScreen(tPlayer)); }
static AssignmentReturnValue* backEdgeDistFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerBackAxisDistanceToScreen(tPlayer)); }
static AssignmentReturnValue* bottomEdgeFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getDreamStageBottomEdgeY(getPlayerCoordinateP(tPlayer))); }
static AssignmentReturnValue* cameraPosXFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getDreamCameraPositionX(getPlayerCoordinateP(tPlayer))); }
static AssignmentReturnValue* cameraPosYFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getDreamCameraPositionY(getPlayerCoordinateP(tPlayer))); }
static AssignmentReturnValue* cameraZoomFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getDreamCameraZoom(getPlayerCoordinateP(tPlayer))); }
static AssignmentReturnValue* canRecoverFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(canPlayerRecoverFromFalling(tPlayer)); }
//static AssignmentReturnValue* commandFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(0); }
static AssignmentReturnValue* ctrlFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(getPlayerControl(tPlayer)); }
static AssignmentReturnValue* drawGameFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(hasPlayerDrawn(tPlayer)); }
static AssignmentReturnValue* eFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(M_E); }
static AssignmentReturnValue* facingFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerIsFacingRight(tPlayer) ? 1 : -1); }
static AssignmentReturnValue* frontEdgeFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerScreenEdgeInFrontX(tPlayer)); }
static AssignmentReturnValue* frontEdgeBodyDistFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerFrontBodyDistanceToScreen(tPlayer)); }
static AssignmentReturnValue* frontEdgeDistFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerFrontAxisDistanceToScreen(tPlayer)); }
static AssignmentReturnValue* gameHeightFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getDreamGameHeight(getPlayerCoordinateP(tPlayer))); }
static AssignmentReturnValue* gameTimeFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getDreamGameTime()); }
static AssignmentReturnValue* gameWidthFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getDreamGameWidth(getPlayerCoordinateP(tPlayer))); }
static AssignmentReturnValue* hitCountFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerHitCount(tPlayer)); }
//static AssignmentReturnValue* hitDefAttrFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(0); }
static AssignmentReturnValue* hitFallFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(isPlayerFalling(tPlayer)); }
static AssignmentReturnValue* hitOverFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(isPlayerHitOver(tPlayer)); }
static AssignmentReturnValue* hitPauseTimeFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerTimeLeftInHitPause(tPlayer)); }
static AssignmentReturnValue* hitShakeOverFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(isPlayerHitShakeOver(tPlayer));  }
static AssignmentReturnValue* hitVelXFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerHitVelocityX(tPlayer, getPlayerCoordinateP(tPlayer))); }
static AssignmentReturnValue* hitVelYFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerHitVelocityY(tPlayer, getPlayerCoordinateP(tPlayer))); }
static AssignmentReturnValue* idFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(tPlayer->mID); }
static AssignmentReturnValue* inGuardDistFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(isPlayerInGuardDistance(tPlayer)); }
static AssignmentReturnValue* isHelperFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(isPlayerHelper(tPlayer)); }
static AssignmentReturnValue* isHomeTeamFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(isPlayerHomeTeam(tPlayer)); }
static AssignmentReturnValue* leftEdgeFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getDreamStageLeftEdgeX(getPlayerCoordinateP(tPlayer))); }
static AssignmentReturnValue* lifeFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerLife(tPlayer)); }
static AssignmentReturnValue* lifeMaxFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerLifeMax(tPlayer)); }
static AssignmentReturnValue* loseFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(hasPlayerLost(tPlayer)); }
static AssignmentReturnValue* matchNoFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getDreamMatchNumber()); }
static AssignmentReturnValue* matchOverFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(isDreamMatchOver()); }
static AssignmentReturnValue* moveContactFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerMoveContactCounter(tPlayer)); }
static AssignmentReturnValue* moveGuardedFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerMoveGuarded(tPlayer)); }
static AssignmentReturnValue* moveHitFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerMoveHit(tPlayer)); }
//static AssignmentReturnValue* moveTypeFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(0); }
static AssignmentReturnValue* moveReversedFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(hasPlayerMoveBeenReversedByOtherPlayer(tPlayer)); }
static AssignmentReturnValue* nameFunction(DreamPlayer* tPlayer) { return makeStringAssignmentReturn(getPlayerName(tPlayer)); }
static AssignmentReturnValue* numEnemyFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(1); } // TODO: fix once teams exist
static AssignmentReturnValue* numExplodFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getExplodAmount(tPlayer)); }
static AssignmentReturnValue* numHelperFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerHelperAmount(tPlayer)); }
static AssignmentReturnValue* numPartnerFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(0); } // TODO: fix when partners are implemented
static AssignmentReturnValue* numProjFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerProjectileAmount(tPlayer)); }
static AssignmentReturnValue* numTargetFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerTargetAmount(tPlayer)); }
static AssignmentReturnValue* p1NameFunction(DreamPlayer* tPlayer) { return nameFunction(tPlayer); }
static AssignmentReturnValue* p2BodyDistFunctionX(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerDistanceToFrontOfOtherPlayerX(tPlayer)); }
static AssignmentReturnValue* p2BodyDistFunctionY(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerAxisDistanceY(tPlayer)); }
static AssignmentReturnValue* p2DistFunctionX(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerAxisDistanceX(tPlayer)); }
static AssignmentReturnValue* p2DistFunctionY(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerAxisDistanceY(tPlayer)); }
static AssignmentReturnValue* p2LifeFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerLife(getPlayerOtherPlayer(tPlayer))); }
//static AssignmentReturnValue* p2MoveTypeFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(0); }
static AssignmentReturnValue* p2NameFunction(DreamPlayer* tPlayer) { return makeStringAssignmentReturn(getPlayerName(getPlayerOtherPlayer(tPlayer))); }
static AssignmentReturnValue* p2StateNoFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerState(getPlayerOtherPlayer(tPlayer))); }
//static AssignmentReturnValue* p2StateTypeFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(0); }
static AssignmentReturnValue* p3NameFunction(DreamPlayer* tPlayer) { return makeStringAssignmentReturn(""); } // TODO: after teams work
static AssignmentReturnValue* p4NameFunction(DreamPlayer* tPlayer) { return makeStringAssignmentReturn(""); } // TODO: after teams work
static AssignmentReturnValue* palNoFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerPaletteNumber(tPlayer)); }
static AssignmentReturnValue* parentDistXFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerDistanceToParentX(tPlayer)); }
static AssignmentReturnValue* parentDistYFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerDistanceToParentY(tPlayer)); }
static AssignmentReturnValue* piFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(M_PI); }
static AssignmentReturnValue* posXFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerPositionBasedOnScreenCenterX(tPlayer, getPlayerCoordinateP(tPlayer))); }
static AssignmentReturnValue* posYFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerPositionBasedOnStageFloorY(tPlayer, getPlayerCoordinateP(tPlayer))); }
static AssignmentReturnValue* powerFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerPower(tPlayer)); }
static AssignmentReturnValue* powerMaxFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerPowerMax(tPlayer)); }
static AssignmentReturnValue* prevStateNoFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerPreviousState(tPlayer)); }
//static AssignmentReturnValue* projContactFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(0); }
//static AssignmentReturnValue* projGuardedFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(0); }
//static AssignmentReturnValue* projHitFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(0); }
static AssignmentReturnValue* randomFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(randfromInteger(0, 999)); }
static AssignmentReturnValue* rightEdgeFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getDreamStageRightEdgeX(getPlayerCoordinateP(tPlayer))); }
static AssignmentReturnValue* rootDistXFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerDistanceToRootX(tPlayer)); }
static AssignmentReturnValue* rootDistYFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerDistanceToRootY(tPlayer)); }
static AssignmentReturnValue* roundNoFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getDreamRoundNumber()); }
static AssignmentReturnValue* roundsExistedFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerRoundsExisted(tPlayer)); }
static AssignmentReturnValue* roundStateFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getDreamRoundStateNumber()); }
static AssignmentReturnValue* screenPosXFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerScreenPositionX(tPlayer, getPlayerCoordinateP(tPlayer))); }
static AssignmentReturnValue* screenPosYFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerScreenPositionY(tPlayer, getPlayerCoordinateP(tPlayer))); }
static AssignmentReturnValue* screenHeightFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getDreamScreenHeight(getPlayerCoordinateP(tPlayer))); }
static AssignmentReturnValue* screenWidthFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getDreamScreenWidth(getPlayerCoordinateP(tPlayer))); }
static AssignmentReturnValue* stateNoFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerState(tPlayer)); }
//static AssignmentReturnValue* stateTypeFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(0); }
//static AssignmentReturnValue* teamModeFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(0); }
static AssignmentReturnValue* teamSideFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(tPlayer->mRootID + 1); }
static AssignmentReturnValue* ticksPerSecondFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getDreamTicksPerSecond()); }
static AssignmentReturnValue* timeFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerTimeInState(tPlayer)); }
//static AssignmentReturnValue* timeModFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(0); }
static AssignmentReturnValue* topEdgeFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getDreamStageTopEdgeY(getPlayerCoordinateP(tPlayer))); }
static AssignmentReturnValue* uniqHitCountFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerUniqueHitCount(tPlayer)); }
static AssignmentReturnValue* velXFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerVelocityX(tPlayer, getPlayerCoordinateP(tPlayer))); }
static AssignmentReturnValue* velYFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerVelocityY(tPlayer, getPlayerCoordinateP(tPlayer))); }
static AssignmentReturnValue* winFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(hasPlayerWon(tPlayer)); }


static AssignmentReturnValue* dataLifeFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerDataLife(tPlayer)); }
static AssignmentReturnValue* dataPowerFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerPowerMax(tPlayer)); }
static AssignmentReturnValue* dataAttackFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerDataAttack(tPlayer)); }
static AssignmentReturnValue* dataDefenceFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerDataDefense(tPlayer)); }
static AssignmentReturnValue* dataFallDefenceMultiplierFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerFallDefenseMultiplier(tPlayer)); }
static AssignmentReturnValue* dataLiedownTimeFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerDataLiedownTime(tPlayer)); }
static AssignmentReturnValue* dataAirjuggleFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerDataAirjuggle(tPlayer)); }
static AssignmentReturnValue* dataSparkNoFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerDataSparkNo(tPlayer)); }
static AssignmentReturnValue* dataGuardSparkNoFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerDataGuardSparkNo(tPlayer)); }
static AssignmentReturnValue* dataKOEchoFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerDataKOEcho(tPlayer)); }
static AssignmentReturnValue* dataIntPersistIndexFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerDataIntPersistIndex(tPlayer)); }
static AssignmentReturnValue* dataFloatPersistIndexFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerDataFloatPersistIndex(tPlayer)); }

static AssignmentReturnValue* sizeXScaleFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerScaleX(tPlayer)); }
static AssignmentReturnValue* sizeYScaleFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerScaleY(tPlayer)); }
static AssignmentReturnValue* sizeGroundBackFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerGroundSizeBack(tPlayer)); }
static AssignmentReturnValue* sizeGroundFrontFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerGroundSizeFront(tPlayer)); }
static AssignmentReturnValue* sizeAirBackFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerSizeAirBack(tPlayer)); }
static AssignmentReturnValue* sizeAirFrontFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerSizeAirFront(tPlayer)); }
static AssignmentReturnValue* sizeHeightFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerHeight(tPlayer)); }
static AssignmentReturnValue* sizeAttackDistFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerSizeAttackDist(tPlayer)); }
static AssignmentReturnValue* sizeProjAttackDistFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerSizeProjectileAttackDist(tPlayer)); }
static AssignmentReturnValue* sizeProjDoScaleFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerSizeProjectilesDoScale(tPlayer)); }
static AssignmentReturnValue* sizeHeadPosXFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerHeadPositionX(tPlayer)); }
static AssignmentReturnValue* sizeHeadPosYFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerHeadPositionY(tPlayer)); }
static AssignmentReturnValue* sizeMidPosXFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerMiddlePositionX(tPlayer)); }
static AssignmentReturnValue* sizeMidPosYFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerMiddlePositionY(tPlayer)); }
static AssignmentReturnValue* sizeShadowOffsetFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerSizeShadowOffset(tPlayer)); }
static AssignmentReturnValue* sizeDrawOffsetXFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerSizeDrawOffsetX(tPlayer)); }
static AssignmentReturnValue* sizeDrawOffsetYFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerSizeDrawOffsetY(tPlayer)); }

static AssignmentReturnValue* velocityWalkFwdXFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerForwardWalkVelocityX(tPlayer)); }
static AssignmentReturnValue* velocityWalkBackXFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerBackwardWalkVelocityX(tPlayer)); }
static AssignmentReturnValue* velocityRunFwdXFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerForwardRunVelocityX(tPlayer)); }
static AssignmentReturnValue* velocityRunFwdYFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerForwardRunVelocityY(tPlayer)); }
static AssignmentReturnValue* velocityRunBackXFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerBackwardRunVelocityX(tPlayer)); }
static AssignmentReturnValue* velocityRunBackYFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerBackwardRunVelocityY(tPlayer));; }
static AssignmentReturnValue* velocityJumpYFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerJumpVelocityY(tPlayer)); }
static AssignmentReturnValue* velocityJumpNeuXFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerNeutralJumpVelocityX(tPlayer)); }
static AssignmentReturnValue* velocityJumpBackXFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerBackwardJumpVelocityX(tPlayer)); }
static AssignmentReturnValue* velocityJumpFwdXFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerForwardJumpVelocityX(tPlayer)); }
static AssignmentReturnValue* velocityRunJumpBackXFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerBackwardRunJumpVelocityX(tPlayer)); }
static AssignmentReturnValue* velocityRunJumpFwdXFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerForwardRunJumpVelocityX(tPlayer)); }
static AssignmentReturnValue* velocityAirJumpYFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerAirJumpVelocityY(tPlayer)); }
static AssignmentReturnValue* velocityAirJumpNeuXFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerNeutralAirJumpVelocityX(tPlayer)); }
static AssignmentReturnValue* velocityAirJumpBackXFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerBackwardAirJumpVelocityX(tPlayer)); }
static AssignmentReturnValue* velocityAirJumpFwdXFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerForwardAirJumpVelocityX(tPlayer)); }
static AssignmentReturnValue* velocityAirGetHitGroundRecoverXFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerVelocityAirGetHitGroundRecoverX(tPlayer)); }
static AssignmentReturnValue* velocityAirGetHitGroundRecoverYFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerVelocityAirGetHitGroundRecoverY(tPlayer)); }
static AssignmentReturnValue* velocityAirGetHitAirRecoverMulXFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerVelocityAirGetHitAirRecoverMulX(tPlayer)); }
static AssignmentReturnValue* velocityAirGetHitAirRecoverMulYFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerVelocityAirGetHitAirRecoverMulY(tPlayer)); }
static AssignmentReturnValue* velocityAirGetHitAirRecoverAddXFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerVelocityAirGetHitAirRecoverAddX(tPlayer)); }
static AssignmentReturnValue* velocityAirGetHitAirRecoverAddYFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerVelocityAirGetHitAirRecoverAddY(tPlayer)); }
static AssignmentReturnValue* velocityAirGetHitAirRecoverBackFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerVelocityAirGetHitAirRecoverBack(tPlayer)); }
static AssignmentReturnValue* velocityAirGetHitAirRecoverFwdFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerVelocityAirGetHitAirRecoverFwd(tPlayer)); }
static AssignmentReturnValue* velocityAirGetHitAirRecoverUpFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerVelocityAirGetHitAirRecoverUp(tPlayer)); }
static AssignmentReturnValue* velocityAirGetHitAirRecoverDownFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerVelocityAirGetHitAirRecoverDown(tPlayer)); }

static AssignmentReturnValue* movementAirJumpNumFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerMovementAirJumpNum(tPlayer)); }
static AssignmentReturnValue* movementAirJumpHeightFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerMovementAirJumpHeight(tPlayer)); }
static AssignmentReturnValue* movementYAccelFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerVerticalAcceleration(tPlayer)); }
static AssignmentReturnValue* movementStandFrictionFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerStandFriction(tPlayer)); }
static AssignmentReturnValue* movementCrouchFrictionFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerCrouchFriction(tPlayer)); }
static AssignmentReturnValue* movementStandFrictionThresholdFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerStandFrictionThreshold(tPlayer)); }
static AssignmentReturnValue* movementCrouchFrictionThresholdFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerCrouchFrictionThreshold(tPlayer)); }
static AssignmentReturnValue* movementJumpChangeAnimThresholdFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerMovementJumpChangeAnimThreshold(tPlayer)); }
static AssignmentReturnValue* movementAirGetHitGroundLevelFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerAirGetHitGroundLevelY(tPlayer)); }
static AssignmentReturnValue* movementAirGetHitGroundRecoverGroundThresholdFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerAirGetHitGroundRecoveryGroundYTheshold(tPlayer)); }
static AssignmentReturnValue* movementAirGetHitGroundRecoverGroundLevelFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerAirGetHitGroundRecoveryGroundLevelY(tPlayer)); }
static AssignmentReturnValue* movementAirGetHitAirRecoverThresholdFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerAirGetHitAirRecoveryVelocityYThreshold(tPlayer)); }
static AssignmentReturnValue* movementAirGetHitAirRecoverYAccelFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerMovementAirGetHitAirRecoverYAccel(tPlayer)); }
static AssignmentReturnValue* movementAirGetHitTripGroundLevelFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerAirGetHitTripGroundLevelY(tPlayer)); }
static AssignmentReturnValue* movementDownBounceOffsetXFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerDownBounceOffsetX(tPlayer, getPlayerCoordinateP(tPlayer))); }
static AssignmentReturnValue* movementDownBounceOffsetYFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerDownBounceOffsetY(tPlayer, getPlayerCoordinateP(tPlayer))); }
static AssignmentReturnValue* movementDownBounceYAccelFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerDownVerticalBounceAcceleration(tPlayer)); }
static AssignmentReturnValue* movementDownBounceGroundLevelFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerDownBounceGroundLevel(tPlayer, getPlayerCoordinateP(tPlayer))); }
static AssignmentReturnValue* movementDownFrictionThresholdFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerLyingDownFrictionThreshold(tPlayer)); }


static AssignmentReturnValue* getHitVarAnimtypeFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getActiveHitDataAnimationType(tPlayer)); }
static AssignmentReturnValue* getHitVarGroundtypeFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getActiveHitDataGroundType(tPlayer)); }
static AssignmentReturnValue* getHitVarAirtypeFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getActiveHitDataAirType(tPlayer)); }
static AssignmentReturnValue* getHitVarXvelFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getActiveHitDataVelocityX(tPlayer)); }
static AssignmentReturnValue* getHitVarYvelFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getActiveHitDataVelocityY(tPlayer)); }
static AssignmentReturnValue* getHitVarYaccelFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getActiveHitDataYAccel(tPlayer)); }
static AssignmentReturnValue* getHitVarFallFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(getActiveHitDataFall(tPlayer)); }
static AssignmentReturnValue* getHitVarFallRecoverFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(getActiveHitDataFallRecovery(tPlayer)); }
static AssignmentReturnValue* getHitVarFallYvelFunction(DreamPlayer* tPlayer) {	return makeFloatAssignmentReturn(getActiveHitDataFallYVelocity(tPlayer));}
static AssignmentReturnValue* getHitVarSlidetimeFunction(DreamPlayer* tPlayer) {	return makeBooleanAssignmentReturn(getPlayerSlideTime(tPlayer));}
static AssignmentReturnValue* getHitVarHitshaketimeFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(getActiveHitDataPlayer2PauseTime(tPlayer));}
static AssignmentReturnValue* getHitVarHitcountFunction(DreamPlayer* tPlayer) {	return makeNumberAssignmentReturn(getPlayerHitCount(tPlayer));}
static AssignmentReturnValue* getHitVarDamageFunction(DreamPlayer* tPlayer) {return makeNumberAssignmentReturn(getActiveHitDataDamage(tPlayer));}
static AssignmentReturnValue* getHitVarFallcountFunction(DreamPlayer* tPlayer) {	return makeNumberAssignmentReturn(getPlayerFallAmountInCombo(tPlayer));}
static AssignmentReturnValue* getHitVarCtrltimeFunction(DreamPlayer* tPlayer) {	return makeNumberAssignmentReturn(getPlayerControlTime(tPlayer));}
static AssignmentReturnValue* getHitVarIsboundFunction(DreamPlayer* tPlayer) {	return makeBooleanAssignmentReturn(isPlayerBound(tPlayer));}

static void setupVariableAssignments() {
	gVariableHandler.mVariables.clear();
	gVariableHandler.mVariables["ailevel"] = aiLevelFunction;
	gVariableHandler.mVariables["alive"] = aliveFunction;
	gVariableHandler.mVariables["anim"] = animFunction;
	gVariableHandler.mVariables["animtime"] = animTimeFunction;
	gVariableHandler.mVariables["authorname"] = authorNameFunction;

	gVariableHandler.mVariables["backedge"] = backEdgeFunction;
	gVariableHandler.mVariables["backedgebodydist"] = backEdgeBodyDistFunction;
	gVariableHandler.mVariables["backedgedist"] = backEdgeDistFunction;
	gVariableHandler.mVariables["bottomedge"] = bottomEdgeFunction;

	gVariableHandler.mVariables["camerapos x"] = cameraPosXFunction;
	gVariableHandler.mVariables["camerapos y"] = cameraPosYFunction;
	gVariableHandler.mVariables["camerazoom"] = cameraZoomFunction;
	gVariableHandler.mVariables["canrecover"] = canRecoverFunction;
	gVariableHandler.mVariables["ctrl"] = ctrlFunction;

	gVariableHandler.mVariables["drawgame"] = drawGameFunction;

	gVariableHandler.mVariables["e"] = eFunction;

	gVariableHandler.mVariables["facing"] = facingFunction;
	gVariableHandler.mVariables["frontedge"] = frontEdgeFunction;
	gVariableHandler.mVariables["frontedgebodydist"] = frontEdgeBodyDistFunction;
	gVariableHandler.mVariables["frontedgedist"] = frontEdgeDistFunction;

	gVariableHandler.mVariables["gameheight"] = gameHeightFunction;
	gVariableHandler.mVariables["gametime"] = gameTimeFunction;
	gVariableHandler.mVariables["gamewidth"] = gameWidthFunction;

	gVariableHandler.mVariables["hitcount"] = hitCountFunction;
	gVariableHandler.mVariables["hitfall"] = hitFallFunction;
	gVariableHandler.mVariables["hitover"] = hitOverFunction;
	gVariableHandler.mVariables["hitpausetime"] = hitPauseTimeFunction;
	gVariableHandler.mVariables["hitshakeover"] = hitShakeOverFunction;
	gVariableHandler.mVariables["hitvel x"] = hitVelXFunction;
	gVariableHandler.mVariables["hitvel y"] = hitVelYFunction;

	gVariableHandler.mVariables["id"] = idFunction;
	gVariableHandler.mVariables["inguarddist"] = inGuardDistFunction;
	gVariableHandler.mVariables["ishelper"] = isHelperFunction;
	gVariableHandler.mVariables["ishometeam"] = isHomeTeamFunction;

	gVariableHandler.mVariables["leftedge"] = leftEdgeFunction;
	gVariableHandler.mVariables["life"] = lifeFunction;
	gVariableHandler.mVariables["lifemax"] = lifeMaxFunction;
	gVariableHandler.mVariables["lose"] = loseFunction;

	gVariableHandler.mVariables["matchno"] = matchNoFunction;
	gVariableHandler.mVariables["matchover"] = matchOverFunction;
	gVariableHandler.mVariables["movecontact"] = moveContactFunction;
	gVariableHandler.mVariables["moveguarded"] = moveGuardedFunction;
	gVariableHandler.mVariables["movehit"] = moveHitFunction;
	gVariableHandler.mVariables["movereversed"] = moveReversedFunction;

	gVariableHandler.mVariables["name"] = nameFunction;
	gVariableHandler.mVariables["numenemy"] = numEnemyFunction;
	gVariableHandler.mVariables["numexplod"] = numExplodFunction;
	gVariableHandler.mVariables["numhelper"] = numHelperFunction;
	gVariableHandler.mVariables["numpartner"] = numPartnerFunction;
	gVariableHandler.mVariables["numproj"] = numProjFunction;
	gVariableHandler.mVariables["numtarget"] = numTargetFunction;

	gVariableHandler.mVariables["p1name"] = p1NameFunction;
	gVariableHandler.mVariables["p2bodydist x"] = p2BodyDistFunctionX;
	gVariableHandler.mVariables["p2bodydist y"] = p2BodyDistFunctionY;
	gVariableHandler.mVariables["p2dist x"] = p2DistFunctionX;
	gVariableHandler.mVariables["p2dist y"] = p2DistFunctionY;
	gVariableHandler.mVariables["p2life"] = p2LifeFunction;
	gVariableHandler.mVariables["p2name"] = p2NameFunction;
	gVariableHandler.mVariables["p2stateno"] = p2StateNoFunction;
	gVariableHandler.mVariables["p3name"] = p3NameFunction;
	gVariableHandler.mVariables["p4name"] = p4NameFunction;
	gVariableHandler.mVariables["palno"] = palNoFunction;
	gVariableHandler.mVariables["parentdist x"] = parentDistXFunction;
	gVariableHandler.mVariables["parentdist y"] = parentDistYFunction;
	gVariableHandler.mVariables["pi"] = piFunction;
	gVariableHandler.mVariables["pos x"] = posXFunction;
	gVariableHandler.mVariables["pos y"] = posYFunction;
	gVariableHandler.mVariables["power"] = powerFunction;
	gVariableHandler.mVariables["powermax"] = powerMaxFunction;
	gVariableHandler.mVariables["prevstateno"] = prevStateNoFunction;

	gVariableHandler.mVariables["random"] = randomFunction;
	gVariableHandler.mVariables["rightedge"] = rightEdgeFunction;
	gVariableHandler.mVariables["rootdist x"] = rootDistXFunction;
	gVariableHandler.mVariables["rootdist y"] = rootDistYFunction;
	gVariableHandler.mVariables["roundno"] = roundNoFunction;
	gVariableHandler.mVariables["roundsexisted"] = roundsExistedFunction;
	gVariableHandler.mVariables["roundstate"] = roundStateFunction;

	gVariableHandler.mVariables["screenpos x"] = screenPosXFunction;
	gVariableHandler.mVariables["screenpos y"] = screenPosYFunction;
	gVariableHandler.mVariables["screenheight"] = screenHeightFunction;
	gVariableHandler.mVariables["screenwidth"] = screenWidthFunction;
	gVariableHandler.mVariables["stateno"] = stateNoFunction;
	gVariableHandler.mVariables["statetime"] = timeFunction;

	gVariableHandler.mVariables["teamside"] = teamSideFunction;
	gVariableHandler.mVariables["tickspersecond"] = ticksPerSecondFunction;
	gVariableHandler.mVariables["time"] = timeFunction;
	gVariableHandler.mVariables["topedge"] = topEdgeFunction;

	gVariableHandler.mVariables["uniquehitcount"] = uniqHitCountFunction;

	gVariableHandler.mVariables["vel x"] = velXFunction;
	gVariableHandler.mVariables["vel y"] = velYFunction;

	gVariableHandler.mVariables["win"] = winFunction;

	gVariableHandler.mVariables["const(data.life)"] = dataLifeFunction;
	gVariableHandler.mVariables["const(data.power)"] = dataPowerFunction;
	gVariableHandler.mVariables["const(data.attack)"] = dataAttackFunction;
	gVariableHandler.mVariables["const(data.defence)"] = dataDefenceFunction;
	gVariableHandler.mVariables["const(data.fall.defence_mul)"] = dataFallDefenceMultiplierFunction;
	gVariableHandler.mVariables["const(data.liedown.time)"] = dataLiedownTimeFunction;
	gVariableHandler.mVariables["const(data.airjuggle)"] = dataAirjuggleFunction;
	gVariableHandler.mVariables["const(data.sparkno)"] = dataSparkNoFunction;
	gVariableHandler.mVariables["const(data.guard.sparkno)"] = dataGuardSparkNoFunction;
	gVariableHandler.mVariables["const(data.ko.echo)"] = dataKOEchoFunction;
	gVariableHandler.mVariables["const(data.intpersistindex)"] = dataIntPersistIndexFunction;
	gVariableHandler.mVariables["const(data.floatpersistindex)"] = dataFloatPersistIndexFunction;

	gVariableHandler.mVariables["const(size.xscale)"] = sizeXScaleFunction;
	gVariableHandler.mVariables["const(size.yscale)"] = sizeYScaleFunction;
	gVariableHandler.mVariables["const(size.ground.back)"] = sizeGroundBackFunction;
	gVariableHandler.mVariables["const(size.ground.front)"] = sizeGroundFrontFunction;
	gVariableHandler.mVariables["const(size.air.back)"] = sizeAirBackFunction;
	gVariableHandler.mVariables["const(size.air.front)"] = sizeAirFrontFunction;
	gVariableHandler.mVariables["const(size.height)"] = sizeHeightFunction;
	gVariableHandler.mVariables["const(size.attack.dist)"] = sizeAttackDistFunction;
	gVariableHandler.mVariables["const(size.proj.attack.dist)"] = sizeProjAttackDistFunction;
	gVariableHandler.mVariables["const(size.proj.doscale)"] = sizeProjDoScaleFunction;
	gVariableHandler.mVariables["const(size.head.pos.x)"] = sizeHeadPosXFunction;
	gVariableHandler.mVariables["const(size.head.pos.y)"] = sizeHeadPosYFunction;
	gVariableHandler.mVariables["const(size.mid.pos.x)"] = sizeMidPosXFunction;
	gVariableHandler.mVariables["const(size.mid.pos.y)"] = sizeMidPosYFunction;
	gVariableHandler.mVariables["const(size.shadowoffset)"] = sizeShadowOffsetFunction;
	gVariableHandler.mVariables["const(size.draw.offset.x)"] = sizeDrawOffsetXFunction;
	gVariableHandler.mVariables["const(size.draw.offset.y)"] = sizeDrawOffsetYFunction;

	gVariableHandler.mVariables["const(velocity.walk.fwd.x)"] = velocityWalkFwdXFunction;
	gVariableHandler.mVariables["const(velocity.walk.back.x)"] = velocityWalkBackXFunction;
	gVariableHandler.mVariables["const(velocity.run.fwd.x)"] = velocityRunFwdXFunction;
	gVariableHandler.mVariables["const(velocity.run.fwd.y)"] = velocityRunFwdYFunction;
	gVariableHandler.mVariables["const(velocity.run.back.x)"] = velocityRunBackXFunction;
	gVariableHandler.mVariables["const(velocity.run.back.y)"] = velocityRunBackYFunction;
	gVariableHandler.mVariables["const(velocity.jump.y)"] = velocityJumpYFunction;
	gVariableHandler.mVariables["const(velocity.jump.neu.x)"] = velocityJumpNeuXFunction;
	gVariableHandler.mVariables["const(velocity.jump.back.x)"] = velocityJumpBackXFunction;
	gVariableHandler.mVariables["const(velocity.jump.fwd.x)"] = velocityJumpFwdXFunction;
	gVariableHandler.mVariables["const(velocity.runjump.back.x)"] = velocityRunJumpBackXFunction;
	gVariableHandler.mVariables["const(velocity.runjump.fwd.x)"] = velocityRunJumpFwdXFunction;
	gVariableHandler.mVariables["const(velocity.airjump.y)"] = velocityAirJumpYFunction;
	gVariableHandler.mVariables["const(velocity.airjump.neu.x)"] = velocityAirJumpNeuXFunction;
	gVariableHandler.mVariables["const(velocity.airjump.back.x)"] = velocityAirJumpBackXFunction;
	gVariableHandler.mVariables["const(velocity.airjump.fwd.x)"] = velocityAirJumpFwdXFunction;
	gVariableHandler.mVariables["const(velocity.air.gethit.groundrecover.x)"] = velocityAirGetHitGroundRecoverXFunction;
	gVariableHandler.mVariables["const(velocity.air.gethit.groundrecover.y)"] = velocityAirGetHitGroundRecoverYFunction;
	gVariableHandler.mVariables["const(velocity.air.gethit.airrecover.mul.x)"] = velocityAirGetHitAirRecoverMulXFunction;
	gVariableHandler.mVariables["const(velocity.air.gethit.airrecover.mul.y)"] = velocityAirGetHitAirRecoverMulYFunction;
	gVariableHandler.mVariables["const(velocity.air.gethit.airrecover.add.x)"] = velocityAirGetHitAirRecoverAddXFunction;
	gVariableHandler.mVariables["const(velocity.air.gethit.airrecover.add.y)"] = velocityAirGetHitAirRecoverAddYFunction;
	gVariableHandler.mVariables["const(velocity.air.gethit.airrecover.back)"] = velocityAirGetHitAirRecoverBackFunction;
	gVariableHandler.mVariables["const(velocity.air.gethit.airrecover.fwd)"] = velocityAirGetHitAirRecoverFwdFunction;
	gVariableHandler.mVariables["const(velocity.air.gethit.airrecover.up)"] = velocityAirGetHitAirRecoverUpFunction;
	gVariableHandler.mVariables["const(velocity.air.gethit.airrecover.down)"] = velocityAirGetHitAirRecoverDownFunction;

	gVariableHandler.mVariables["const(movement.airjump.num)"] = movementAirJumpNumFunction;
	gVariableHandler.mVariables["const(movement.airjump.height)"] = movementAirJumpHeightFunction;
	gVariableHandler.mVariables["const(movement.yaccel)"] = movementYAccelFunction;
	gVariableHandler.mVariables["const(movement.stand.friction)"] = movementStandFrictionFunction;
	gVariableHandler.mVariables["const(movement.crouch.friction)"] = movementCrouchFrictionFunction;
	gVariableHandler.mVariables["const(movement.stand.friction.threshold)"] = movementStandFrictionThresholdFunction;
	gVariableHandler.mVariables["const(movement.crouch.friction.threshold)"] = movementCrouchFrictionThresholdFunction;
	gVariableHandler.mVariables["const(movement.jump.changeanim.threshold)"] = movementJumpChangeAnimThresholdFunction;
	gVariableHandler.mVariables["const(movement.air.gethit.groundlevel)"] = movementAirGetHitGroundLevelFunction;
	gVariableHandler.mVariables["const(movement.air.gethit.groundrecover.ground.threshold)"] = movementAirGetHitGroundRecoverGroundThresholdFunction;
	gVariableHandler.mVariables["const(movement.air.gethit.groundrecover.groundlevel)"] = movementAirGetHitGroundRecoverGroundLevelFunction;
	gVariableHandler.mVariables["const(movement.air.gethit.airrecover.threshold)"] = movementAirGetHitAirRecoverThresholdFunction;
	gVariableHandler.mVariables["const(movement.air.gethit.airrecover.yaccel)"] = movementAirGetHitAirRecoverYAccelFunction;
	gVariableHandler.mVariables["const(movement.air.gethit.trip.groundlevel)"] = movementAirGetHitTripGroundLevelFunction;
	gVariableHandler.mVariables["const(movement.down.bounce.offset.x)"] = movementDownBounceOffsetXFunction;
	gVariableHandler.mVariables["const(movement.down.bounce.offset.y)"] = movementDownBounceOffsetYFunction;
	gVariableHandler.mVariables["const(movement.down.bounce.yaccel)"] = movementDownBounceYAccelFunction;
	gVariableHandler.mVariables["const(movement.down.bounce.groundlevel)"] = movementDownBounceGroundLevelFunction;
	gVariableHandler.mVariables["const(movement.down.friction.threshold)"] = movementDownFrictionThresholdFunction;


	gVariableHandler.mVariables["gethitvar(animtype)"] = getHitVarAnimtypeFunction;
	gVariableHandler.mVariables["gethitvar(groundtype)"] = getHitVarGroundtypeFunction;
	gVariableHandler.mVariables["gethitvar(airtype)"] = getHitVarAirtypeFunction;
	gVariableHandler.mVariables["gethitvar(xvel)"] = getHitVarXvelFunction;
	gVariableHandler.mVariables["gethitvar(yvel)"] = getHitVarYvelFunction;
	gVariableHandler.mVariables["gethitvar(yaccel)"] = getHitVarYaccelFunction;
	gVariableHandler.mVariables["gethitvar(fall)"] = getHitVarFallFunction;
	gVariableHandler.mVariables["gethitvar(fall.recover)"] = getHitVarFallRecoverFunction;
	gVariableHandler.mVariables["gethitvar(fall.yvel)"] = getHitVarFallYvelFunction;
	gVariableHandler.mVariables["gethitvar(slidetime)"] = getHitVarSlidetimeFunction;
	gVariableHandler.mVariables["gethitvar(hitshaketime)"] = getHitVarHitshaketimeFunction;
	gVariableHandler.mVariables["gethitvar(hitcount)"] = getHitVarHitcountFunction;
	gVariableHandler.mVariables["gethitvar(damage)"] = getHitVarDamageFunction;
	gVariableHandler.mVariables["gethitvar(fallcount)"] = getHitVarFallcountFunction;
	gVariableHandler.mVariables["gethitvar(ctrltime)"] = getHitVarCtrltimeFunction;
	gVariableHandler.mVariables["gethitvar(isbound)"] = getHitVarIsboundFunction;
}

static void setupArrayAssignments();

void setupDreamAssignmentEvaluator() {
	initEvaluationStack();
	setupVariableAssignments();
	setupArrayAssignments();
	setupComparisons();
}

static int isIsInOtherFileVariable(char* tName) {
	if (tName[0] == '\0' || tName[1] == '\0') return 0;

	int hasFlag = tName[0] == 's' || tName[0] == 'f';
	if (!hasFlag) return 0;

	int i;
	for (i = 1; tName[i] != '\0'; i++) {
		if (tName[i] < '0' || tName[i] > '9') return 0;
	}

	return 1;
}

static AssignmentReturnValue* makeExternalFileAssignmentReturn(char tIdentifierCharacter, char* tValueString) {
	char buffer[100];
	sprintf(buffer, "isinotherfile%c %s", tIdentifierCharacter, tValueString);
	return makeStringAssignmentReturn(buffer);
}


static AssignmentReturnValue* evaluateVariableAssignment(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* tIsStatic) {
	DreamMugenVariableAssignment* variable = (DreamMugenVariableAssignment*)*tAssignment;
	*tIsStatic = 0;
	AssignmentReturnValue*(*func)(DreamPlayer*) = (AssignmentReturnValue*(*)(DreamPlayer*))variable->mFunc;

	return func(tPlayer);
}

static AssignmentReturnValue* evaluateRawVariableAssignment(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* tIsStatic) {
	DreamMugenRawVariableAssignment* variable = (DreamMugenRawVariableAssignment*)*tAssignment;
	*tIsStatic = 0;

	if (isIsInOtherFileVariable(variable->mName)) { // TODO: fix
		return makeExternalFileAssignmentReturn(variable->mName[0], variable->mName + 1);
	}

	return makeStringAssignmentReturn(variable->mName);
}

static AssignmentReturnValue* evaluateVarArrayAssignment(AssignmentReturnValue* tIndex, DreamPlayer* tPlayer, int* tIsStatic) {
	int id = convertAssignmentReturnToNumber(tIndex);
	int val = getPlayerVariable(tPlayer, id);

	*tIsStatic = 0;
	return makeNumberAssignmentReturn(val);
}

static AssignmentReturnValue* evaluateSysVarArrayAssignment(AssignmentReturnValue* tIndex, DreamPlayer* tPlayer, int* tIsStatic) {
	int id = convertAssignmentReturnToNumber(tIndex);
	int val = getPlayerSystemVariable(tPlayer, id);

	*tIsStatic = 0;
	return makeNumberAssignmentReturn(val);
}

static AssignmentReturnValue* evaluateGlobalVarArrayAssignment(AssignmentReturnValue* tIndex, DreamPlayer* tPlayer, int* tIsStatic) {
	int id = convertAssignmentReturnToNumber(tIndex);
	int val = getGlobalVariable(id);

	*tIsStatic = 0;
	return makeNumberAssignmentReturn(val);
}

static AssignmentReturnValue* evaluateFVarArrayAssignment(AssignmentReturnValue* tIndex, DreamPlayer* tPlayer, int* tIsStatic) {
	int id = convertAssignmentReturnToNumber(tIndex);
	double val = getPlayerFloatVariable(tPlayer, id);

	*tIsStatic = 0;
	return makeFloatAssignmentReturn(val);
}

static AssignmentReturnValue* evaluateSysFVarArrayAssignment(AssignmentReturnValue* tIndex, DreamPlayer* tPlayer, int* tIsStatic) {
	int id = convertAssignmentReturnToNumber(tIndex);
	double val = getPlayerSystemFloatVariable(tPlayer, id);

	*tIsStatic = 0;
	return makeFloatAssignmentReturn(val);
}

static AssignmentReturnValue* evaluateGlobalFVarArrayAssignment(AssignmentReturnValue* tIndex, DreamPlayer* tPlayer, int* tIsStatic) {
	int id = convertAssignmentReturnToNumber(tIndex);
	double val = getGlobalFloatVariable(id);

	*tIsStatic = 0;
	return makeFloatAssignmentReturn(val);
}

static AssignmentReturnValue* evaluateStageVarArrayAssignment(AssignmentReturnValue* tIndex, DreamPlayer* tPlayer, int* tIsStatic) {
	char* var = convertAssignmentReturnToAllocatedString(tIndex);
	turnStringLowercase(var);

	AssignmentReturnValue* ret = NULL;
	if (!strcmp("info.author", var)) {
		ret = makeStringAssignmentReturn(getDreamStageAuthor());
	}
	else if(!strcmp("info.displayname", var)) {
		ret = makeStringAssignmentReturn(getDreamStageDisplayName());
	}
	else if (!strcmp("info.name", var)) {
		ret = makeStringAssignmentReturn(getDreamStageName());
	}
	else {
		logWarningFormat("Unknown stage variable %s. Returning bottom.", var);
		ret = makeBottomAssignmentReturn(); 
	}
	assert(ret);

	freeMemory(var);

	(void)tIsStatic; // TODO: check
	return ret;
}

static AssignmentReturnValue* evaluateAbsArrayAssignment(AssignmentReturnValue* tIndex) {
	if (isFloatReturn(tIndex)) {
		double val = convertAssignmentReturnToFloat(tIndex);
		return makeFloatAssignmentReturn(fabs(val));
	}
	else {
		int val = convertAssignmentReturnToNumber(tIndex);
		return makeNumberAssignmentReturn(abs(val));
	}
}

static AssignmentReturnValue* evaluateExpArrayAssignment(AssignmentReturnValue* tIndex) {
		double val = convertAssignmentReturnToFloat(tIndex);
		return makeFloatAssignmentReturn(exp(val));
}

static AssignmentReturnValue* evaluateNaturalLogArrayAssignment(AssignmentReturnValue* tIndex) {
	double val = convertAssignmentReturnToFloat(tIndex);
	return makeFloatAssignmentReturn(log(val));
}

static AssignmentReturnValue* evaluateLogArrayAssignment(AssignmentReturnValue* tIndex) {
	if (tIndex->mType != MUGEN_ASSIGNMENT_RETURN_TYPE_VECTOR) {
#ifndef LOGGER_WARNINGS_DISABLED
		char* text = convertAssignmentReturnToAllocatedString(tIndex);
		logWarningFormat("Unable to parse log array assignment %s. Defaulting to bottom.", text);
		freeMemory(text);
#endif
		return makeBottomAssignmentReturn();
	}

	double base = convertAssignmentReturnToFloat(getVectorAssignmentReturnFirstDependency(tIndex));
	double value = convertAssignmentReturnToFloat(getVectorAssignmentReturnSecondDependency(tIndex));

	return makeFloatAssignmentReturn(log(value) / log(base));
}

static AssignmentReturnValue* evaluateCosineArrayAssignment(AssignmentReturnValue* tIndex) {
		double val = convertAssignmentReturnToFloat(tIndex);
		return makeFloatAssignmentReturn(cos(val));
}

static AssignmentReturnValue* evaluateAcosineArrayAssignment(AssignmentReturnValue* tIndex) {
	double val = convertAssignmentReturnToFloat(tIndex);
	return makeFloatAssignmentReturn(acos(val));
}

static AssignmentReturnValue* evaluateSineArrayAssignment(AssignmentReturnValue* tIndex) {
	double val = convertAssignmentReturnToFloat(tIndex);
	return makeFloatAssignmentReturn(sin(val));
}

static AssignmentReturnValue* evaluateAsineArrayAssignment(AssignmentReturnValue* tIndex) {
	double val = convertAssignmentReturnToFloat(tIndex);
	return makeFloatAssignmentReturn(asin(val));
}

static AssignmentReturnValue* evaluateTangentArrayAssignment(AssignmentReturnValue* tIndex) {
	double val = convertAssignmentReturnToFloat(tIndex);
	return makeFloatAssignmentReturn(tan(val));
}

static AssignmentReturnValue* evaluateAtangentArrayAssignment(AssignmentReturnValue* tIndex) {
	double val = convertAssignmentReturnToFloat(tIndex);
	return makeFloatAssignmentReturn(atan(val));
}

static AssignmentReturnValue* evaluateFloorArrayAssignment(AssignmentReturnValue* tIndex) {
	if (isFloatReturn(tIndex)) {
		double val = convertAssignmentReturnToFloat(tIndex);
		return makeNumberAssignmentReturn((int)floor(val));
	}
	else {
		int val = convertAssignmentReturnToNumber(tIndex);
		return makeNumberAssignmentReturn(val);
	}
}

static AssignmentReturnValue* evaluateCeilArrayAssignment(AssignmentReturnValue* tIndex) {
	if (isFloatReturn(tIndex)) {
		double val = convertAssignmentReturnToFloat(tIndex);
		return makeNumberAssignmentReturn((int)ceil(val));
	}
	else {
		int val = convertAssignmentReturnToNumber(tIndex);
		return makeNumberAssignmentReturn(val);
	}
}

static AssignmentReturnValue* evaluateAnimationElementTimeArrayAssignment(AssignmentReturnValue* tIndex, DreamPlayer* tPlayer, int* tIsStatic) {
	int id = convertAssignmentReturnToNumber(tIndex);
	int ret = getPlayerTimeFromAnimationElement(tPlayer, id);

	*tIsStatic = 0;
	return makeNumberAssignmentReturn(ret);
}

static AssignmentReturnValue* evaluateAnimationElementNumberArrayAssignment(AssignmentReturnValue* tIndex, DreamPlayer* tPlayer, int* tIsStatic) {
	int time = convertAssignmentReturnToNumber(tIndex);
	int ret = getPlayerAnimationElementFromTimeOffset(tPlayer, time);

	*tIsStatic = 0;
	return makeNumberAssignmentReturn(ret);
}

static AssignmentReturnValue* evaluateIfElseArrayAssignment(AssignmentReturnValue* tIndex) {
	char condText[100], yesText[100], noText[100];
	char comma1[20], comma2[20];

	char* test = convertAssignmentReturnToAllocatedString(tIndex);
	sscanf(test, "%s %s %s %s %s", condText, comma1, yesText, comma2, noText);

	if (strcmp(",", comma1) || strcmp(",", comma2) || !strcmp("", condText) || !strcmp("", yesText) || !strcmp("", noText)) {
		logWarningFormat("Unable to parse if else array assignment %s. Defaulting to bottom.", test);
		freeMemory(test);
		return makeBottomAssignmentReturn(); 
	}
	freeMemory(test);

	int cond = atof(condText) != 0;
	if (cond) {
		AssignmentReturnValue* yesRet = makeStringAssignmentReturn(yesText);
		return yesRet;
	}
	else {
		AssignmentReturnValue* noRet = makeStringAssignmentReturn(noText);
		return noRet;
	}
}

static AssignmentReturnValue* evaluateCondArrayAssignment(DreamMugenAssignment** tCondVector, DreamPlayer* tPlayer, int* tIsStatic) {
	if ((*tCondVector)->mType != MUGEN_ASSIGNMENT_TYPE_VECTOR) {
		logWarningFormat("Invalid cond array cond vector type %d. Defaulting to bottom.", (*tCondVector)->mType);
		return makeBottomAssignmentReturn(); 
	}
	DreamMugenDependOnTwoAssignment* firstV = (DreamMugenDependOnTwoAssignment*)*tCondVector;
	if(firstV->b->mType != MUGEN_ASSIGNMENT_TYPE_VECTOR) {
		logWarningFormat("Invalid cond array second vector type %d. Defaulting to bottom.", firstV->b->mType);
		return makeBottomAssignmentReturn();
	}
	DreamMugenDependOnTwoAssignment* secondV = (DreamMugenDependOnTwoAssignment*)firstV->b;

	AssignmentReturnValue* ret = NULL;

	AssignmentReturnValue* truthValue = evaluateAssignmentDependency(&firstV->a, tPlayer, tIsStatic);
	int isTrue = convertAssignmentReturnToNumber(truthValue);
	if (isTrue) {
		ret = evaluateAssignmentDependency(&secondV->a, tPlayer, tIsStatic);
	}
	else {
		ret = evaluateAssignmentDependency(&secondV->b, tPlayer, tIsStatic);
	}
	assert(ret);

	return ret;
}

static AssignmentReturnValue* evaluateAnimationExistArrayAssignment(AssignmentReturnValue* tIndex, DreamPlayer* tPlayer, int* tIsStatic) {
	int id = convertAssignmentReturnToNumber(tIndex);
	int ret = doesPlayerHaveAnimation(tPlayer, id);

	*tIsStatic = 0;
	return makeNumberAssignmentReturn(ret);
}

static AssignmentReturnValue* evaluateSelfAnimationExistArrayAssignment(AssignmentReturnValue* tIndex, DreamPlayer* tPlayer, int* tIsStatic) {
	int id = convertAssignmentReturnToNumber(tIndex);
	int ret = doesPlayerHaveAnimationHimself(tPlayer, id);

	(void)*tIsStatic;
	return makeNumberAssignmentReturn(ret);
}

static AssignmentReturnValue* evaluateConstCoordinatesArrayAssignment(AssignmentReturnValue* tIndex, int p) {
	int coords = convertAssignmentReturnToNumber(tIndex);
	double ret = parseDreamCoordinatesToLocalCoordinateSystem(coords, p);
	return makeFloatAssignmentReturn(ret);
}


static AssignmentReturnValue* evaluateExternalFileAnimationArrayAssignment(char tIdentifierCharacter, AssignmentReturnValue* b) {
	char* val2 = convertAssignmentReturnToAllocatedString(b);
	AssignmentReturnValue* ret = makeExternalFileAssignmentReturn(tIdentifierCharacter, val2);
	freeMemory(val2);
	return ret;
}

static AssignmentReturnValue* evaluateNumTargetArrayAssignment(AssignmentReturnValue* tIndex, DreamPlayer* tPlayer, int* tIsStatic) {
	int id = convertAssignmentReturnToNumber(tIndex);
	int ret = getPlayerTargetAmountWithID(tPlayer, id);

	*tIsStatic = 0;
	return makeNumberAssignmentReturn(ret);
}

static AssignmentReturnValue* evaluateNumHelperArrayAssignment(AssignmentReturnValue* tIndex, DreamPlayer* tPlayer, int* tIsStatic) {
	int id = convertAssignmentReturnToNumber(tIndex);
	int ret = getPlayerHelperAmountWithID(tPlayer, id);

	*tIsStatic = 0;
	return makeNumberAssignmentReturn(ret);
}

static AssignmentReturnValue* evaluateNumExplodArrayAssignment(AssignmentReturnValue* tIndex, DreamPlayer* tPlayer, int* tIsStatic) {
	int id = convertAssignmentReturnToNumber(tIndex);
	int ret = getExplodAmountWithID(tPlayer, id);

	*tIsStatic = 0;
	return makeNumberAssignmentReturn(ret);
}

static AssignmentReturnValue* evaluateIsHelperArrayAssignment(AssignmentReturnValue* tIndex, DreamPlayer* tPlayer, int* tIsStatic) {
	int id = convertAssignmentReturnToNumber(tIndex);
	int isHelper = isPlayerHelper(tPlayer);
	int playerID = getPlayerID(tPlayer);

	*tIsStatic = 0;
	return makeBooleanAssignmentReturn(isHelper && id == playerID);
}

static AssignmentReturnValue* evaluateTargetArrayAssignment(AssignmentReturnValue* tIndex, char* tTargetName) {
	int id = convertAssignmentReturnToNumber(tIndex);

	char buffer[100]; // TODO: dynamic
	sprintf(buffer, "%s %d", tTargetName, id);
	return makeStringAssignmentReturn(buffer);
}

static AssignmentReturnValue* evaluatePlayerIDExistArrayAssignment(AssignmentReturnValue* tIndex, DreamPlayer* tPlayer, int* tIsStatic) {
	int id = convertAssignmentReturnToNumber(tIndex);
	*tIsStatic = 0;
	return makeBooleanAssignmentReturn(doesPlayerIDExist(tPlayer, id));
}

static AssignmentReturnValue* evaluateNumberOfProjectilesWithIDArrayAssignment(AssignmentReturnValue* tIndex, DreamPlayer* tPlayer, int* tIsStatic) {
	int id = convertAssignmentReturnToNumber(tIndex);
	*tIsStatic = 0;
	return makeNumberAssignmentReturn(getPlayerProjectileAmountWithID(tPlayer, id));
}

static AssignmentReturnValue* evaluateProjectileCancelTimeArrayAssignment(AssignmentReturnValue* tIndex, DreamPlayer* tPlayer, int* tIsStatic) {
	int id = convertAssignmentReturnToNumber(tIndex);
	*tIsStatic = 0;
	return makeNumberAssignmentReturn(getPlayerProjectileTimeSinceCancel(tPlayer, id));
}

static AssignmentReturnValue* evaluateProjectileContactTimeArrayAssignment(AssignmentReturnValue* tIndex, DreamPlayer* tPlayer, int* tIsStatic) {
	int id = convertAssignmentReturnToNumber(tIndex);
	*tIsStatic = 0;
	return makeNumberAssignmentReturn(getPlayerProjectileTimeSinceContact(tPlayer, id));
}

static AssignmentReturnValue* evaluateProjectileGuardedTimeArrayAssignment(AssignmentReturnValue* tIndex, DreamPlayer* tPlayer, int* tIsStatic) {
	int id = convertAssignmentReturnToNumber(tIndex);
	*tIsStatic = 0;
	return makeNumberAssignmentReturn(getPlayerProjectileTimeSinceGuarded(tPlayer, id));
}

static AssignmentReturnValue* evaluateProjectileHitTimeArrayAssignment(AssignmentReturnValue* tIndex, DreamPlayer* tPlayer, int* tIsStatic) {
	int id = convertAssignmentReturnToNumber(tIndex);
	*tIsStatic = 0;
	return makeNumberAssignmentReturn(getPlayerProjectileTimeSinceHit(tPlayer, id));
}

static AssignmentReturnValue* evaluateProjectileHitArrayAssignment(AssignmentReturnValue* tIndex, DreamPlayer* tPlayer, int* tIsStatic) {
	int id = convertAssignmentReturnToNumber(tIndex);
	*tIsStatic = 0;
	return makeNumberAssignmentReturn(getPlayerProjectileHit(tPlayer, id));
}





static AssignmentReturnValue* varFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateVarArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), tPlayer, tIsStatic); }
static AssignmentReturnValue* sysVarFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateSysVarArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), tPlayer, tIsStatic); }
static AssignmentReturnValue* globalVarFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateGlobalVarArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), tPlayer, tIsStatic); }
static AssignmentReturnValue* fVarFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateFVarArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), tPlayer, tIsStatic); }
static AssignmentReturnValue* sysFVarFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateSysFVarArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), tPlayer, tIsStatic); }
static AssignmentReturnValue* globalFVarFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateGlobalFVarArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), tPlayer, tIsStatic); }
static AssignmentReturnValue* stageVarFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateStageVarArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), tPlayer, tIsStatic); }
static AssignmentReturnValue* absFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateAbsArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic)); }
static AssignmentReturnValue* expFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateExpArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic)); }
static AssignmentReturnValue* lnFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateNaturalLogArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic)); }
static AssignmentReturnValue* logFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateLogArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic)); }
static AssignmentReturnValue* cosFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateCosineArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic)); }
static AssignmentReturnValue* acosFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateAcosineArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic)); }
static AssignmentReturnValue* sinFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateSineArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic)); }
static AssignmentReturnValue* asinFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateAsineArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic)); }
static AssignmentReturnValue* tanFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateTangentArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic)); }
static AssignmentReturnValue* atanFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateAtangentArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic)); }
static AssignmentReturnValue* floorFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateFloorArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic)); }
static AssignmentReturnValue* ceilFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateCeilArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic)); }
static AssignmentReturnValue* animElemTimeFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateAnimationElementTimeArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), tPlayer, tIsStatic); }
static AssignmentReturnValue* animElemNoFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateAnimationElementNumberArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), tPlayer, tIsStatic); }
static AssignmentReturnValue* ifElseFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateIfElseArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic)); }
static AssignmentReturnValue* condFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateCondArrayAssignment(tIndexAssignment, tPlayer, tIsStatic); }
static AssignmentReturnValue* animExistFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateAnimationExistArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), tPlayer, tIsStatic); }
static AssignmentReturnValue* selfAnimExistFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateSelfAnimationExistArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), tPlayer, tIsStatic); }
static AssignmentReturnValue* const240pFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateConstCoordinatesArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), 240); }
static AssignmentReturnValue* const480pFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateConstCoordinatesArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), 480); }
static AssignmentReturnValue* const720pFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateConstCoordinatesArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), 720); }
static AssignmentReturnValue* externalFileFunctionF(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateExternalFileAnimationArrayAssignment('f', evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic)); }
static AssignmentReturnValue* externalFileFunctionS(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateExternalFileAnimationArrayAssignment('s', evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic)); }
static AssignmentReturnValue* numTargetArrayFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateNumTargetArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), tPlayer, tIsStatic); }
static AssignmentReturnValue* numHelperArrayFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateNumHelperArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), tPlayer, tIsStatic); }
static AssignmentReturnValue* numExplodArrayFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateNumExplodArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), tPlayer, tIsStatic); }
static AssignmentReturnValue* isHelperArrayFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateIsHelperArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), tPlayer, tIsStatic); }
static AssignmentReturnValue* helperFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateTargetArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), "helper"); }
static AssignmentReturnValue* enemyNearFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateTargetArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), "enemynear"); }
static AssignmentReturnValue* playerIDFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateTargetArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), "playerid"); }
static AssignmentReturnValue* playerIDExistFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluatePlayerIDExistArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), tPlayer, tIsStatic); }
static AssignmentReturnValue* numProjIDFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateNumberOfProjectilesWithIDArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), tPlayer, tIsStatic); }
static AssignmentReturnValue* projCancelTimeFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateProjectileCancelTimeArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), tPlayer, tIsStatic); }
static AssignmentReturnValue* projContactTimeFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateProjectileContactTimeArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), tPlayer, tIsStatic); }
static AssignmentReturnValue* projGuardedTimeFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateProjectileGuardedTimeArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), tPlayer, tIsStatic); }
static AssignmentReturnValue* projHitTimeFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateProjectileHitTimeArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), tPlayer, tIsStatic); }
static AssignmentReturnValue* projHitArrayFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateProjectileHitArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), tPlayer, tIsStatic); }


static void setupArrayAssignments() {
	gVariableHandler.mArrays.clear();

	gVariableHandler.mArrays["var"] = varFunction;
	gVariableHandler.mArrays["sysvar"] = sysVarFunction;
	gVariableHandler.mArrays["globalvar"] = globalVarFunction;
	gVariableHandler.mArrays["fvar"] = fVarFunction;
	gVariableHandler.mArrays["sysfvar"] = sysFVarFunction;
	gVariableHandler.mArrays["globalfvar"] = globalFVarFunction;
	gVariableHandler.mArrays["stagevar"] = stageVarFunction;
	gVariableHandler.mArrays["abs"] = absFunction;
	gVariableHandler.mArrays["exp"] = expFunction;
	gVariableHandler.mArrays["ln"] = lnFunction;
	gVariableHandler.mArrays["log"] = logFunction;
	gVariableHandler.mArrays["cos"] = cosFunction;
	gVariableHandler.mArrays["acos"] = acosFunction;
	gVariableHandler.mArrays["sin"] = sinFunction;
	gVariableHandler.mArrays["asin"] = asinFunction;
	gVariableHandler.mArrays["tan"] = tanFunction;
	gVariableHandler.mArrays["atan"] = atanFunction;
	gVariableHandler.mArrays["floor"] = floorFunction;
	gVariableHandler.mArrays["ceil"] = ceilFunction;
	gVariableHandler.mArrays["animelemtime"] = animElemTimeFunction;
	gVariableHandler.mArrays["animelemno"] = animElemNoFunction;
	gVariableHandler.mArrays["ifelse"] = ifElseFunction;
	gVariableHandler.mArrays["sifelse"] = ifElseFunction;
	gVariableHandler.mArrays["cond"] = condFunction;
	gVariableHandler.mArrays["animexist"] = animExistFunction;
	gVariableHandler.mArrays["selfanimexist"] = selfAnimExistFunction;
	gVariableHandler.mArrays["const240p"] = const240pFunction;
	gVariableHandler.mArrays["const480p"] = const480pFunction;
	gVariableHandler.mArrays["const720p"] = const720pFunction;
	gVariableHandler.mArrays["f"] = externalFileFunctionF;
	gVariableHandler.mArrays["s"] = externalFileFunctionS;
	gVariableHandler.mArrays["numtarget"] = numTargetArrayFunction;
	gVariableHandler.mArrays["target"] = numTargetArrayFunction; // TODO: check?
	gVariableHandler.mArrays["enemy"] = numTargetArrayFunction; // TODO: check?
	gVariableHandler.mArrays["numhelper"] = numHelperArrayFunction;
	gVariableHandler.mArrays["numexplod"] = numExplodArrayFunction;
	gVariableHandler.mArrays["ishelper"] = isHelperArrayFunction;
	gVariableHandler.mArrays["helper"] = helperFunction;
	gVariableHandler.mArrays["enemynear"] = enemyNearFunction;
	gVariableHandler.mArrays["playerid"] = playerIDFunction;
	gVariableHandler.mArrays["playeridexist"] = playerIDExistFunction;
	gVariableHandler.mArrays["numprojid"] = numProjIDFunction;
	gVariableHandler.mArrays["projcanceltime"] = projCancelTimeFunction;
	gVariableHandler.mArrays["projcontacttime"] = projContactTimeFunction;
	gVariableHandler.mArrays["projguardedtime"] = projGuardedTimeFunction;
	gVariableHandler.mArrays["projhittime"] = projHitTimeFunction;
	gVariableHandler.mArrays["projhit"] = projHitArrayFunction;
}


static AssignmentReturnValue* evaluateArrayAssignment(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* tIsStatic) {
	DreamMugenArrayAssignment* arrays = (DreamMugenArrayAssignment*)*tAssignment;

	ArrayFunction func = (ArrayFunction)arrays->mFunc;
	return func(&arrays->mIndex, tPlayer, tIsStatic);
}

static AssignmentReturnValue* evaluateUnaryMinusAssignment(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* tIsStatic) {
	DreamMugenDependOnOneAssignment* min = (DreamMugenDependOnOneAssignment*)*tAssignment;

	AssignmentReturnValue* a = evaluateAssignmentDependency(&min->a, tPlayer, tIsStatic);
	if (isFloatReturn(a)) {
		return makeFloatAssignmentReturn(-convertAssignmentReturnToFloat(a));
	}
	else {
		return makeNumberAssignmentReturn(-convertAssignmentReturnToNumber(a));
	}
}

static AssignmentReturnValue* evaluateNegationAssignment(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* tIsStatic) {
	DreamMugenDependOnOneAssignment* neg = (DreamMugenDependOnOneAssignment*)*tAssignment;

	AssignmentReturnValue* a = evaluateAssignmentDependency(&neg->a, tPlayer, tIsStatic);
	int val = convertAssignmentReturnToBool(a);

	return makeBooleanAssignmentReturn(!val);
}

typedef struct {
	uint8_t mType;
	AssignmentReturnValue* mValue;
} DreamMugenStaticAssignment;

int gPruneAmount;

static DreamMugenAssignment* makeStaticDreamMugenAssignment(AssignmentReturnValue* tValue) {
	DreamMugenStaticAssignment* e = (DreamMugenStaticAssignment*)allocMemory(sizeof(DreamMugenStaticAssignment));
	e->mType = MUGEN_ASSIGNMENT_TYPE_STATIC;
	e->mValue = tValue;

	gPruneAmount++;
	return (DreamMugenAssignment*)e;
}

static AssignmentReturnValue* evaluateStaticAssignment(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* tIsStatic) {
	(void)tPlayer;
	(void)tIsStatic;

	DreamMugenStaticAssignment* stat = (DreamMugenStaticAssignment*)*tAssignment;
	return stat->mValue;
}

typedef AssignmentReturnValue*(AssignmentEvaluationFunction)(DreamMugenAssignment**, DreamPlayer*, int*);

static void* gEvaluationFunctions[] = {
	(void*)evaluateBooleanAssignment,
	(void*)evaluateAndAssignment,
	(void*)evaluateOrAssignment,
	(void*)evaluateComparisonAssignment,
	(void*)evaluateInequalityAssignment,
	(void*)evaluateLessOrEqualAssignment,
	(void*)evaluateGreaterOrEqualAssignment,
	(void*)evaluateVectorAssignment,
	(void*)evaluateRangeAssignment,
	(void*)evaluateBooleanAssignment,
	(void*)evaluateNegationAssignment,
	(void*)evaluateVariableAssignment,
	(void*)evaluateRawVariableAssignment,
	(void*)evaluateNumberAssignment,
	(void*)evaluateFloatAssignment,
	(void*)evaluateStringAssignment,
	(void*)evaluateArrayAssignment,
	(void*)evaluateLessAssignment,
	(void*)evaluateGreaterAssignment,
	(void*)evaluateAdditionAssignment,
	(void*)evaluateMultiplicationAssignment,
	(void*)evaluateModuloAssignment,
	(void*)evaluateSubtractionAssignment,
	(void*)evaluateSetVariableAssignment,
	(void*)evaluateDivisionAssignment,
	(void*)evaluateExponentiationAssignment,
	(void*)evaluateUnaryMinusAssignment,
	(void*)evaluateOperatorArgumentAssignment,
	(void*)evaluateBitwiseAndAssignment,
	(void*)evaluateBitwiseOrAssignment,
	(void*)evaluateStaticAssignment,
};


static void setAssignmentStatic(DreamMugenAssignment** tAssignment, AssignmentReturnValue* tValue) {
	switch ((*tAssignment)->mType) {
	case MUGEN_ASSIGNMENT_TYPE_AND:
	case MUGEN_ASSIGNMENT_TYPE_OR:
	case MUGEN_ASSIGNMENT_TYPE_COMPARISON:
	case MUGEN_ASSIGNMENT_TYPE_INEQUALITY:
	case MUGEN_ASSIGNMENT_TYPE_LESS_OR_EQUAL:
	case MUGEN_ASSIGNMENT_TYPE_GREATER_OR_EQUAL:
	case MUGEN_ASSIGNMENT_TYPE_VECTOR:
	case MUGEN_ASSIGNMENT_TYPE_RANGE:
	case MUGEN_ASSIGNMENT_TYPE_NEGATION:
	case MUGEN_ASSIGNMENT_TYPE_VARIABLE:
	case MUGEN_ASSIGNMENT_TYPE_RAW_VARIABLE:
	case MUGEN_ASSIGNMENT_TYPE_ARRAY:
	case MUGEN_ASSIGNMENT_TYPE_LESS:
	case MUGEN_ASSIGNMENT_TYPE_GREATER:
	case MUGEN_ASSIGNMENT_TYPE_ADDITION:
	case MUGEN_ASSIGNMENT_TYPE_MULTIPLICATION:
	case MUGEN_ASSIGNMENT_TYPE_MODULO:
	case MUGEN_ASSIGNMENT_TYPE_SUBTRACTION:
	case MUGEN_ASSIGNMENT_TYPE_SET_VARIABLE:
	case MUGEN_ASSIGNMENT_TYPE_DIVISION:
	case MUGEN_ASSIGNMENT_TYPE_EXPONENTIATION:
	case MUGEN_ASSIGNMENT_TYPE_UNARY_MINUS:
	case MUGEN_ASSIGNMENT_TYPE_OPERATOR_ARGUMENT:
	case MUGEN_ASSIGNMENT_TYPE_BITWISE_AND:
	case MUGEN_ASSIGNMENT_TYPE_BITWISE_OR:
		// *tAssignment = makeStaticDreamMugenAssignment(tValue);
		break;
	default:
		break;
	}
}

static AssignmentReturnValue* evaluateAssignmentInternal(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* oIsStatic) {
	*oIsStatic = 1;
	
	if (!tAssignment || !(*tAssignment)) {
		logWarning("Invalid assignment. Defaulting to bottom.");
		return makeBottomAssignmentReturn(); 
	}


	if ((*tAssignment)->mType >= MUGEN_ASSIGNMENT_TYPE_AMOUNT) {
		logWarningFormat("Unidentified assignment type %d. Returning bottom.", (*tAssignment)->mType);
		return makeBottomAssignmentReturn();
	}

	AssignmentEvaluationFunction* func = (AssignmentEvaluationFunction*)gEvaluationFunctions[(*tAssignment)->mType];
	AssignmentReturnValue* ret = func(tAssignment, tPlayer, oIsStatic);

	if (*oIsStatic) {
		setAssignmentStatic(tAssignment, ret);
	}

	return ret;
}

static AssignmentReturnValue* evaluateAssignmentStart(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* oIsStatic) {
	gAssignmentEvaluator.mFreePointer = 0;

	return evaluateAssignmentInternal(tAssignment, tPlayer, oIsStatic);
}

static AssignmentReturnValue* timeStoryFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getDolmexicaStoryTimeInState((StoryInstance*)tPlayer)); }

static void setupStoryVariableAssignments() {
	gVariableHandler.mVariables.clear();
	gVariableHandler.mVariables["time"] = timeStoryFunction;

	gVariableHandler.mVariables["random"] = randomFunction;
}

//static AssignmentReturnValue* movementDownFrictionThresholdFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerLyingDownFrictionThreshold(tPlayer)); }

static AssignmentReturnValue* evaluateAnimTimeStoryArrayAssignment(AssignmentReturnValue* tIndex, StoryInstance* tInstance, int* tIsStatic) {
	int id = convertAssignmentReturnToNumber(tIndex);
	*tIsStatic = 0;
	return makeNumberAssignmentReturn(getDolmexicaStoryAnimationTimeLeft(tInstance, id));
}

static AssignmentReturnValue* evaluateAnimPosXStoryArrayAssignment(AssignmentReturnValue* tIndex, StoryInstance* tInstance, int* tIsStatic) {
	int id = convertAssignmentReturnToNumber(tIndex);
	*tIsStatic = 0;
	return makeFloatAssignmentReturn(getDolmexicaStoryAnimationPositionX(tInstance, id));
}

static AssignmentReturnValue* evaluateCharAnimStoryArrayAssignment(AssignmentReturnValue* tIndex, StoryInstance* tInstance, int* tIsStatic) {
	int id = convertAssignmentReturnToNumber(tIndex);
	*tIsStatic = 0;
	return makeNumberAssignmentReturn(getDolmexicaStoryCharacterAnimation(tInstance, id));
}

static AssignmentReturnValue* evaluateCharAnimTimeStoryArrayAssignment(AssignmentReturnValue* tIndex, StoryInstance* tInstance, int* tIsStatic) {
	char* val = convertAssignmentReturnToAllocatedString(tIndex);
	int id = getDolmexicaStoryIDFromString(val, tInstance);
	freeMemory(val);
	*tIsStatic = 0;
	return makeNumberAssignmentReturn(getDolmexicaStoryCharacterTimeLeft(tInstance, id));
}

static AssignmentReturnValue* evaluateSVarStoryArrayAssignment(AssignmentReturnValue* tIndex, StoryInstance* tInstance, int* tIsStatic) {
	int id = convertAssignmentReturnToNumber(tIndex);
	*tIsStatic = 0;
	return makeStringAssignmentReturn(getDolmexicaStoryStringVariable(tInstance, id).data());
}

static AssignmentReturnValue* evaluateVarStoryArrayAssignment(AssignmentReturnValue* tIndex, StoryInstance* tInstance, int* tIsStatic) {
	int id = convertAssignmentReturnToNumber(tIndex);
	*tIsStatic = 0;
	return makeNumberAssignmentReturn(getDolmexicaStoryIntegerVariable(tInstance, id));
}

static AssignmentReturnValue* evaluateGlobalVarStoryArrayAssignment(AssignmentReturnValue* tIndex, int* tIsStatic) {
	int id = convertAssignmentReturnToNumber(tIndex);
	*tIsStatic = 0;
	return makeNumberAssignmentReturn(getGlobalVariable(id));
}

static AssignmentReturnValue* evaluateGlobalFVarStoryArrayAssignment(AssignmentReturnValue* tIndex, int* tIsStatic) {
	int id = convertAssignmentReturnToNumber(tIndex);
	*tIsStatic = 0;
	return makeFloatAssignmentReturn(getGlobalFloatVariable(id));
}

static AssignmentReturnValue* evaluateGlobalSVarStoryArrayAssignment(AssignmentReturnValue* tIndex, int* tIsStatic) {
	int id = convertAssignmentReturnToNumber(tIndex);
	*tIsStatic = 0;
	return makeStringAssignmentReturn(getGlobalStringVariable(id).data());
}

static AssignmentReturnValue* evaluateRootSVarStoryArrayAssignment(AssignmentReturnValue* tIndex, StoryInstance* tInstance, int* tIsStatic) {
	int id = convertAssignmentReturnToNumber(tIndex);
	*tIsStatic = 0;
	return makeStringAssignmentReturn(getDolmexicaStoryStringVariable(getDolmexicaStoryRootInstance(), id).data());
}

static AssignmentReturnValue* evaluateRootFVarStoryArrayAssignment(AssignmentReturnValue* tIndex, StoryInstance* tInstance, int* tIsStatic) {
	int id = convertAssignmentReturnToNumber(tIndex);
	*tIsStatic = 0;
	return makeFloatAssignmentReturn(getDolmexicaStoryFloatVariable(getDolmexicaStoryRootInstance(), id));
}

static AssignmentReturnValue* evaluateRootVarStoryArrayAssignment(AssignmentReturnValue* tIndex, StoryInstance* tInstance, int* tIsStatic) {
	int id = convertAssignmentReturnToNumber(tIndex);
	*tIsStatic = 0;
	return makeNumberAssignmentReturn(getDolmexicaStoryIntegerVariable(getDolmexicaStoryRootInstance(), id));
}

static AssignmentReturnValue* evaluateNameIDStoryArrayAssignment(AssignmentReturnValue* tIndex, StoryInstance* tInstance, int* tIsStatic) {
	char* text = convertAssignmentReturnToAllocatedString(tIndex);
	int id = getDolmexicaStoryIDFromString(text, tInstance);
	freeMemory(text);
	*tIsStatic = 0;
	return makeNumberAssignmentReturn(id);
}

static AssignmentReturnValue* animTimeStoryFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateAnimTimeStoryArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), (StoryInstance*)tPlayer, tIsStatic); }
static AssignmentReturnValue* animPosXStoryFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateAnimPosXStoryArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), (StoryInstance*)tPlayer, tIsStatic); }
static AssignmentReturnValue* charAnimStoryFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateCharAnimStoryArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), (StoryInstance*)tPlayer, tIsStatic); }
static AssignmentReturnValue* charAnimTimeStoryFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateCharAnimTimeStoryArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), (StoryInstance*)tPlayer, tIsStatic); }
static AssignmentReturnValue* sVarStoryFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateSVarStoryArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), (StoryInstance*)tPlayer, tIsStatic); }
static AssignmentReturnValue* varStoryFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateVarStoryArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), (StoryInstance*)tPlayer, tIsStatic); }
static AssignmentReturnValue* globalVarStoryFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateGlobalVarStoryArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), tIsStatic); }
static AssignmentReturnValue* globalFVarStoryFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateGlobalFVarStoryArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), tIsStatic); }
static AssignmentReturnValue* globalSVarStoryFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateGlobalSVarStoryArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), tIsStatic); }
static AssignmentReturnValue* rootSVarStoryFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateRootSVarStoryArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), (StoryInstance*)tPlayer, tIsStatic); }
static AssignmentReturnValue* rootFVarStoryFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateRootFVarStoryArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), (StoryInstance*)tPlayer, tIsStatic); }
static AssignmentReturnValue* rootVarStoryFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateRootVarStoryArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), (StoryInstance*)tPlayer, tIsStatic); }
static AssignmentReturnValue* nameIDStoryFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateNameIDStoryArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), (StoryInstance*)tPlayer, tIsStatic); }

static void setupStoryArrayAssignments() {
	gVariableHandler.mArrays.clear();

	gVariableHandler.mArrays["animtime"] = animTimeStoryFunction;
	gVariableHandler.mArrays["x"] = animPosXStoryFunction;
	gVariableHandler.mArrays["ifelse"] = ifElseFunction;
	gVariableHandler.mArrays["charanim"] = charAnimStoryFunction;
	gVariableHandler.mArrays["charanimtime"] = charAnimTimeStoryFunction;
	gVariableHandler.mArrays["svar"] = sVarStoryFunction;
	gVariableHandler.mArrays["var"] = varStoryFunction;
	gVariableHandler.mArrays["globalvar"] = globalVarStoryFunction;
	gVariableHandler.mArrays["globalfvar"] = globalFVarStoryFunction;
	gVariableHandler.mArrays["globalsvar"] = globalSVarStoryFunction;
	gVariableHandler.mArrays["rootsvar"] = rootSVarStoryFunction;
	gVariableHandler.mArrays["rootfvar"] = rootFVarStoryFunction;
	gVariableHandler.mArrays["rootvar"] = rootVarStoryFunction;
	gVariableHandler.mArrays["nameid"] = nameIDStoryFunction;
}

static AssignmentReturnValue* evaluateStoryCommandAssignment(AssignmentReturnValue* tCommand, StoryInstance* tInstance, int* tIsStatic) {
	char* string = convertAssignmentReturnToAllocatedString(tCommand);
	int ret = isStoryCommandActive(string);
	freeMemory(string);

	*tIsStatic = 0;
	return makeBooleanAssignmentReturn(ret);
}


static AssignmentReturnValue* commandComparisonStoryFunction(char* tName, AssignmentReturnValue* b, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateStoryCommandAssignment(b, (StoryInstance*)tPlayer, tIsStatic); }

static void setupStoryComparisons() {
	gVariableHandler.mComparisons.clear();

	gVariableHandler.mComparisons["storycommand"] = commandComparisonStoryFunction;

	gVariableHandler.mOrdinals.clear();
}

void setupDreamStoryAssignmentEvaluator()
{
	initEvaluationStack();
	setupStoryVariableAssignments();
	setupStoryArrayAssignments();
	setupStoryComparisons();
}

void shutdownDreamAssignmentEvaluator()
{
	gVariableHandler.mComparisons.clear();
	gVariableHandler.mArrays.clear();
	gVariableHandler.mVariables.clear();
}

int evaluateDreamAssignment(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer)
{
	int isStatic;
	AssignmentReturnValue* ret = evaluateAssignmentStart(tAssignment, tPlayer, &isStatic);
	return convertAssignmentReturnToBool(ret);
}

double evaluateDreamAssignmentAndReturnAsFloat(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer)
{
	int isStatic;
	AssignmentReturnValue* ret = evaluateAssignmentStart(tAssignment, tPlayer, &isStatic);
	return convertAssignmentReturnToFloat(ret);
}

int evaluateDreamAssignmentAndReturnAsInteger(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer)
{
	int isStatic;
	AssignmentReturnValue* ret = evaluateAssignmentStart(tAssignment, tPlayer, &isStatic);
	return convertAssignmentReturnToNumber(ret);
}

char * evaluateDreamAssignmentAndReturnAsAllocatedString(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer)
{
	int isStatic;
	AssignmentReturnValue* ret = evaluateAssignmentStart(tAssignment, tPlayer, &isStatic);
	return convertAssignmentReturnToAllocatedString(ret);
}

Vector3D evaluateDreamAssignmentAndReturnAsVector3D(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer)
{
	int isStatic;
	AssignmentReturnValue* ret = evaluateAssignmentStart(tAssignment, tPlayer, &isStatic);
	char* test = convertAssignmentReturnToAllocatedString(ret);

	double x, y, z;
	char tX[100], comma1[100], tY[100], comma2[100], tZ[100];

	int items = sscanf(test, "%s %s %s %s %s", tX, comma1, tY, comma2, tZ);
	freeMemory(test);

	if (items >= 1) x = atof(tX);
	else x = 0;
	if (items >= 3) y = atof(tY);
	else y = 0;
	if (items >= 5) z = atof(tZ);
	else z = 0;

	return makePosition(x, y, z);
}

Vector3DI evaluateDreamAssignmentAndReturnAsVector3DI(DreamMugenAssignment** tAssignment, DreamPlayer * tPlayer)
{
	int isStatic;
	AssignmentReturnValue* ret = evaluateAssignmentStart(tAssignment, tPlayer, &isStatic);
	char* test = convertAssignmentReturnToAllocatedString(ret);

	int x, y, z;
	char tX[100], comma1[100], tY[100], comma2[100], tZ[100];

	int items = sscanf(test, "%s %s %s %s %s", tX, comma1, tY, comma2, tZ);
	freeMemory(test);

	if (items >= 1) x = atoi(tX);
	else x = 0;
	if (items >= 3) y = atoi(tY);
	else y = 0;
	if (items >= 5) z = atoi(tZ);
	else z = 0;

	return makeVector3DI(x, y, z);
}
