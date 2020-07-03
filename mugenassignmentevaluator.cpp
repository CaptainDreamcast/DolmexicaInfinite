#define HAVE_M_PI 

#include "mugenassignmentevaluator.h"

#include <assert.h>
#include <sstream>
#include <string>

#define _USE_MATH_DEFINES
#include <math.h>

#define LOGGER_WARNINGS_DISABLED

#include <prism/log.h>
#include <prism/system.h>
#include <prism/math.h>
#include <prism/stlutil.h>
#include <prism/profiling.h>

#include "gamelogic.h"
#include "stage.h"
#include "playerhitdata.h"
#include "mugenexplod.h"
#include "dolmexicastoryscreen.h"
#include "config.h"
#include "mugenstatehandler.h"

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

#define REGULAR_STACK_SIZE 500

static struct {
	int mStackSize;
	AssignmentReturnValue mStack[REGULAR_STACK_SIZE];
	std::list<AssignmentReturnValue> mEmergencyStack;
	int mFreePointer;

} gAssignmentEvaluator;

static void initEvaluationStack() {
	gAssignmentEvaluator.mStackSize = REGULAR_STACK_SIZE;
	gAssignmentEvaluator.mEmergencyStack.clear();
}

static AssignmentReturnValue* getFreeAssignmentReturnValue() {
	if (gAssignmentEvaluator.mFreePointer >= gAssignmentEvaluator.mStackSize) {
		gAssignmentEvaluator.mEmergencyStack.push_back(AssignmentReturnValue());
		return &gAssignmentEvaluator.mEmergencyStack.back();
	}
	else {
		return &gAssignmentEvaluator.mStack[gAssignmentEvaluator.mFreePointer++];
	}
}

typedef AssignmentReturnValue*(*VariableFunction)(DreamPlayer*);
typedef AssignmentReturnValue*(*ArrayFunction)(DreamMugenAssignment**, DreamPlayer*, int*);
typedef AssignmentReturnValue*(*ComparisonFunction)(AssignmentReturnValue*, DreamPlayer*, int*);
typedef AssignmentReturnValue*(*OrdinalFunction)(AssignmentReturnValue*, DreamPlayer*, int*, int(*tCompare)(int, int));

enum MugenAssignmentEvaluatorType {
	MUGEN_ASSIGNMENT_EVALUATOR_TYPE_REGULAR,
	MUGEN_ASSIGNMENT_EVALUATOR_TYPE_STORY,
};

static struct {
	map<string, VariableFunction> mVariables;
	map<string, ArrayFunction> mArrays;
	map<string, ComparisonFunction> mComparisons;
	map<string, OrdinalFunction> mOrdinals;

	MugenAssignmentEvaluatorType mType;
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

static void convertAssignmentReturnToString(string& ret, AssignmentReturnValue* tAssignmentReturn);

static void convertVectorAssignmentReturnToString(std::string& ret, AssignmentReturnValue* tAssignmentReturn) {
	stringstream ss;
	string valueVA, valueVB;
	convertAssignmentReturnToString(valueVA, getVectorAssignmentReturnFirstDependency(tAssignmentReturn));
	convertAssignmentReturnToString(valueVB, getVectorAssignmentReturnSecondDependency(tAssignmentReturn));
	ss << valueVA << " , " << valueVB;
	ret = ss.str();
}

static void convertRangeAssignmentReturnToString(std::string& ret, AssignmentReturnValue* tAssignmentReturn) {
	stringstream ss;
	string valueVA, valueVB;
	convertAssignmentReturnToString(valueVA, getVectorAssignmentReturnFirstDependency(tAssignmentReturn));
	convertAssignmentReturnToString(valueVB, getVectorAssignmentReturnSecondDependency(tAssignmentReturn));
	ss << "[ " << valueVA << " , " << valueVB << " ]";
	ret = ss.str();
}

static void convertAssignmentReturnToString(string& ret, AssignmentReturnValue* tAssignmentReturn) {
	setProfilingSectionMarkerCurrentFunction();
	switch (tAssignmentReturn->mType) {
	case MUGEN_ASSIGNMENT_RETURN_TYPE_STRING:
		ret = getStringAssignmentReturnValue(tAssignmentReturn);
		break;
	case MUGEN_ASSIGNMENT_RETURN_TYPE_NUMBER:
		convertIntegerToStringFast(ret, getNumberAssignmentReturnValue(tAssignmentReturn));
		break;
	case MUGEN_ASSIGNMENT_RETURN_TYPE_FLOAT:
		convertFloatToStringFast(ret, getFloatAssignmentReturnValue(tAssignmentReturn));
		break;
	case MUGEN_ASSIGNMENT_RETURN_TYPE_BOOLEAN:
		convertIntegerToStringFast(ret, getBooleanAssignmentReturnValue(tAssignmentReturn));
		break;
	case MUGEN_ASSIGNMENT_RETURN_TYPE_VECTOR:
		convertVectorAssignmentReturnToString(ret, tAssignmentReturn);
		break;
	case MUGEN_ASSIGNMENT_RETURN_TYPE_RANGE:
		convertRangeAssignmentReturnToString(ret, tAssignmentReturn);
		break;
	default:
		ret = string();
		break;
	}

	destroyAssignmentReturn(tAssignmentReturn);
}

static int convertAssignmentReturnToBool(AssignmentReturnValue* tAssignmentReturn) {
	setProfilingSectionMarkerCurrentFunction();
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
	setProfilingSectionMarkerCurrentFunction();
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
	setProfilingSectionMarkerCurrentFunction();
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

static AssignmentReturnValue* makeAssignmentReturnAssignmentReturn(const AssignmentReturnValue& tValue) {
	AssignmentReturnValue* val = getFreeAssignmentReturnValue();
	*val = tValue;
	return val;
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
	setProfilingSectionMarkerCurrentFunction();
	DreamMugenDependOnTwoAssignment* orAssignment = (DreamMugenDependOnTwoAssignment*)*tAssignment;

	AssignmentReturnValue* a = evaluateAssignmentDependency(&orAssignment->a, tPlayer, tIsStatic);
	int valA = convertAssignmentReturnToBool(a);
	if (valA) return makeBooleanAssignmentReturn(valA);

	AssignmentReturnValue* b = evaluateAssignmentDependency(&orAssignment->b, tPlayer, tIsStatic);
	int valB = convertAssignmentReturnToBool(b);

	return makeBooleanAssignmentReturn(valB);
}

static AssignmentReturnValue* evaluateXorAssignment(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* tIsStatic) {
	DreamMugenDependOnTwoAssignment* xorAssignment = (DreamMugenDependOnTwoAssignment*)*tAssignment;

	AssignmentReturnValue* a = evaluateAssignmentDependency(&xorAssignment->a, tPlayer, tIsStatic);
	const auto valA = convertAssignmentReturnToBool(a);
	AssignmentReturnValue* b = evaluateAssignmentDependency(&xorAssignment->b, tPlayer, tIsStatic);
	const auto valB = convertAssignmentReturnToBool(b);

	return makeBooleanAssignmentReturn((!valA) ^ (!valB));
}

static AssignmentReturnValue* evaluateAndAssignment(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* tIsStatic) {
	setProfilingSectionMarkerCurrentFunction();
	DreamMugenDependOnTwoAssignment* andAssignment = (DreamMugenDependOnTwoAssignment*)*tAssignment;

	AssignmentReturnValue* a = evaluateAssignmentDependency(&andAssignment->a, tPlayer, tIsStatic);
	int valA = convertAssignmentReturnToBool(a);
	if (!valA) return makeBooleanAssignmentReturn(valA);

	AssignmentReturnValue* b = evaluateAssignmentDependency(&andAssignment->b, tPlayer, tIsStatic);
	int valB = convertAssignmentReturnToBool(b);

	return makeBooleanAssignmentReturn(valB);
}

static AssignmentReturnValue* evaluateBitwiseXorAssignment(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* tIsStatic) {
	DreamMugenDependOnTwoAssignment* bitwiseXorAssignment = (DreamMugenDependOnTwoAssignment*)*tAssignment;

	AssignmentReturnValue* a = evaluateAssignmentDependency(&bitwiseXorAssignment->a, tPlayer, tIsStatic);
	const auto valA = convertAssignmentReturnToNumber(a);

	AssignmentReturnValue* b = evaluateAssignmentDependency(&bitwiseXorAssignment->b, tPlayer, tIsStatic);
	const auto valB = convertAssignmentReturnToNumber(b);

	return makeNumberAssignmentReturn(valA ^ valB);
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
	string commandName;
	convertAssignmentReturnToString(commandName, tCommand);
	int ret = isPlayerCommandActive(tPlayer, commandName.data());

	*tIsStatic = 0;
	return makeBooleanAssignmentReturn(ret);
}

static AssignmentReturnValue* evaluateStateTypeAssignment(AssignmentReturnValue* tCommand, DreamPlayer* tPlayer, int* tIsStatic) {
	DreamMugenStateType playerState = getPlayerStateType(tPlayer);
	string test;
	convertAssignmentReturnToString(test, tCommand);
	int ret;
	if (playerState == MUGEN_STATE_TYPE_STANDING) ret = test.find('s') != test.npos;
	else if (playerState == MUGEN_STATE_TYPE_AIR) ret = test.find('a') != test.npos;
	else if (playerState == MUGEN_STATE_TYPE_CROUCHING) ret = test.find('c') != test.npos;
	else if (playerState == MUGEN_STATE_TYPE_LYING) ret = test.find('l') != test.npos;
	else {
		logWarningFormat("Undefined player state %d. Default to false.", playerState);
		ret = 0;
	}

	*tIsStatic = 0;
	return makeBooleanAssignmentReturn(ret);
}

static AssignmentReturnValue* evaluateMoveTypeAssignment(AssignmentReturnValue* tCommand, DreamPlayer* tPlayer, int* tIsStatic) {
	DreamMugenStateMoveType playerMoveType = getPlayerStateMoveType(tPlayer);
	string test;
	convertAssignmentReturnToString(test, tCommand);

	int ret;
	if (playerMoveType == MUGEN_STATE_MOVE_TYPE_ATTACK) ret = test.find('a') != test.npos;
	else if (playerMoveType == MUGEN_STATE_MOVE_TYPE_BEING_HIT) ret = test.find('h') != test.npos;
	else if (playerMoveType == MUGEN_STATE_MOVE_TYPE_IDLE) ret = test.find('i') != test.npos;
	else {
		logWarningFormat("Undefined player state %d. Default to false.", playerMoveType);
		ret = 0;
	}

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
			time = 0;
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

	*tIsStatic = 0;
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

static AssignmentReturnValue* numTargetArrayFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic);
static AssignmentReturnValue* helperFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic);
static AssignmentReturnValue* enemyNearFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic);
static AssignmentReturnValue* playerIDFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic);

static DreamPlayer* getRegularPlayerFromFirstVectorPartOrNullIfNonexistant(DreamMugenAssignment** a, DreamPlayer* tPlayer, int* tIsStatic) {
	static const size_t PLAYER_TEXT_BUFFER_SIZE = 100;
	char text[PLAYER_TEXT_BUFFER_SIZE];
	int id = -1;
	if ((*a)->mType == MUGEN_ASSIGNMENT_TYPE_RAW_VARIABLE)
	{
		auto rawVar = (DreamMugenRawVariableAssignment*)(*a);
		if (strlen(text) >= PLAYER_TEXT_BUFFER_SIZE) return NULL;
		strcpy(text, rawVar->mName);
	}
	else if ((*a)->mType == MUGEN_ASSIGNMENT_TYPE_ARRAY) {
		auto arrayVar = (DreamMugenArrayAssignment*)(*a);
		if (arrayVar->mFunc == numTargetArrayFunction) {
			strcpy(text, "target");
		}
		else if (arrayVar->mFunc == helperFunction) {
			strcpy(text, "helper");
		}
		else if (arrayVar->mFunc == enemyNearFunction) {
			strcpy(text, "enemynear");
		}
		else if (arrayVar->mFunc == playerIDFunction) {
			strcpy(text, "playerid");
		}
		else {
			return NULL;
		}
		auto indexReturn = evaluateAssignmentDependency(&arrayVar->mIndex, tPlayer, tIsStatic);
		id = convertAssignmentReturnToNumber(indexReturn);
		destroyAssignmentReturn(indexReturn);
	}
	else {
		return NULL;
	}

	if (!strcmp("p1", text)) {
		return getRootPlayer(0);
	}
	else if (!strcmp("p2", text)) {
		return getRootPlayer(1);
	}
	else if (!strcmp("target", text)) {
			const auto ret = getPlayerTargetWithID(tPlayer, id);
			if (!ret) {
				logWarningFormat("Unable to find target with id %d. Returning NULL.", id);
			}
			return ret;
		}
	else if (!strcmp("enemy", text)) {
		return getPlayerOtherPlayer(tPlayer);
	}
	else if (!strcmp("enemynear", text)) {
		return getPlayerOtherPlayer(tPlayer);
	}
	else if (!strcmp("root", text)) {
		return getPlayerRoot(tPlayer);
	}
	else if (!strcmp("parent", text)) {
		return getPlayerParent(tPlayer);
	}
	else if (!strcmp("helper", text)) {
		DreamPlayer* ret = getPlayerHelperOrNullIfNonexistant(tPlayer, id);
		if (!ret) {
			logWarningFormat("Unable to find helper with id %d. Returning NULL.", id);
		}
		return ret;
	}
	else if (!strcmp("playerid", text)) {
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

static AssignmentReturnValue* helperStoryFunction(DreamMugenAssignment** /*tIndexAssignment*/, DreamPlayer* /*tPlayer*/, int* /*tIsStatic*/);

static DreamPlayer* getStoryPlayerFromFirstVectorPartOrNullIfNonexistant(DreamMugenAssignment** a, DreamPlayer* tPlayer, int* tIsStatic) {
	static const size_t PLAYER_TEXT_BUFFER_SIZE = 100;
	char text[PLAYER_TEXT_BUFFER_SIZE];
	int id = -1;
	if ((*a)->mType == MUGEN_ASSIGNMENT_TYPE_RAW_VARIABLE)
	{
		auto rawVar = (DreamMugenRawVariableAssignment*)(*a);
		if (strlen(text) >= PLAYER_TEXT_BUFFER_SIZE) return NULL;
		strcpy(text, rawVar->mName);
	}
	else if ((*a)->mType == MUGEN_ASSIGNMENT_TYPE_ARRAY) {
		auto arrayVar = (DreamMugenArrayAssignment*)(*a);
		if (arrayVar->mFunc == helperStoryFunction) {
			strcpy(text, "helper");
		}
		else {
			return NULL;
		}
		auto indexReturn = evaluateAssignmentDependency(&arrayVar->mIndex, tPlayer, tIsStatic);
		id = convertAssignmentReturnToNumber(indexReturn);
		destroyAssignmentReturn(indexReturn);
	}
	else {
		return NULL;
	}

	if (!strcmp("root", text)) {
		return (DreamPlayer*)getDolmexicaStoryRootInstance();
	}
	else if (!strcmp("parent", text)) {
		return (DreamPlayer*)getDolmexicaStoryInstanceParent((StoryInstance*)tPlayer);
	}
	else if (!strcmp("helper", text)) {
		auto ret = (DreamPlayer*)getDolmexicaStoryHelperInstance(id);
		if (!ret) {
			logWarningFormat("Unable to find helper with id %d. Returning NULL.", id);
		}
		return ret;
	}
	else {
		return NULL;
	}
}

static DreamPlayer* getPlayerFromFirstVectorPartOrNullIfNonexistant(DreamMugenAssignment** a, DreamPlayer* tPlayer, int* tIsStatic) {
	if (gVariableHandler.mType == MUGEN_ASSIGNMENT_EVALUATOR_TYPE_REGULAR) {
		return getRegularPlayerFromFirstVectorPartOrNullIfNonexistant(a, tPlayer, tIsStatic);
	}
	else {
		return getStoryPlayerFromFirstVectorPartOrNullIfNonexistant(a, tPlayer, tIsStatic);
	}
}

static int isPlayerAccessVectorAssignment(DreamMugenAssignment** a, DreamPlayer* tPlayer, int* tIsStatic) {
	return getPlayerFromFirstVectorPartOrNullIfNonexistant(a, tPlayer, tIsStatic) != NULL;
}

static AssignmentReturnValue* evaluateTeamModeAssignment(AssignmentReturnValue* tCommand, DreamPlayer* tPlayer, int* tIsStatic) {
	(void)tPlayer;

	string test;
	convertAssignmentReturnToString(test, tCommand); 
	int ret = test == "single";

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
static AssignmentReturnValue* globalVarFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic);
static AssignmentReturnValue* globalFVarFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic);

static AssignmentReturnValue* evaluateSetVariableRegularAssignment(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* tIsStatic) {
	DreamMugenDependOnTwoAssignment* varSetAssignment = (DreamMugenDependOnTwoAssignment*)*tAssignment;
	
	if (varSetAssignment->a->mType != MUGEN_ASSIGNMENT_TYPE_ARRAY){
		logWarningFormat("Incorrect varset type %d. Defaulting to bottom.", varSetAssignment->a->mType);
		return makeBottomAssignmentReturn(); 
	}
	DreamMugenArrayAssignment* varArrayAccess = (DreamMugenArrayAssignment*)varSetAssignment->a;
	int index = convertAssignmentReturnToNumber(evaluateAssignmentInternal(&varArrayAccess->mIndex, tPlayer, tIsStatic));

	ArrayFunction func = (ArrayFunction)varArrayAccess->mFunc;
	AssignmentReturnValue* ret = NULL;
	if (func == varFunction) {
		int value = convertAssignmentReturnToNumber(evaluateAssignmentInternal(&varSetAssignment->b, tPlayer, tIsStatic));
		setPlayerVariable(tPlayer, index, value);
		ret = makeNumberAssignmentReturn(value);
	}
	else if (func == fVarFunction) {
		double value = convertAssignmentReturnToFloat(evaluateAssignmentInternal(&varSetAssignment->b, tPlayer, tIsStatic));
		setPlayerFloatVariable(tPlayer, index, value);
		ret = makeFloatAssignmentReturn(value);
	}
	else if (func == sysVarFunction) {
		int value = convertAssignmentReturnToNumber(evaluateAssignmentInternal(&varSetAssignment->b, tPlayer, tIsStatic));
		setPlayerSystemVariable(tPlayer, index, value);
		ret = makeNumberAssignmentReturn(value);
	}
	else if (func == sysFVarFunction) {
		double value = convertAssignmentReturnToFloat(evaluateAssignmentInternal(&varSetAssignment->b, tPlayer, tIsStatic));
		setPlayerSystemFloatVariable(tPlayer, index, value);
		ret = makeFloatAssignmentReturn(value);
	}
	else if (func == globalVarFunction) {
		int value = convertAssignmentReturnToNumber(evaluateAssignmentInternal(&varSetAssignment->b, tPlayer, tIsStatic));
		setGlobalVariable(index, value);
		ret = makeNumberAssignmentReturn(value);
	}
	else if (func == globalFVarFunction) {
		double value = convertAssignmentReturnToFloat(evaluateAssignmentInternal(&varSetAssignment->b, tPlayer, tIsStatic));
		setGlobalFloatVariable(index, value);
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

static AssignmentReturnValue* sVarStoryFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic);
static AssignmentReturnValue* fVarStoryFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic);
static AssignmentReturnValue* varStoryFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic);
static AssignmentReturnValue* globalVarStoryFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic);
static AssignmentReturnValue* globalFVarStoryFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic);
static AssignmentReturnValue* globalSVarStoryFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic);
static AssignmentReturnValue* rootSVarStoryFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic);
static AssignmentReturnValue* rootFVarStoryFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic);
static AssignmentReturnValue* rootVarStoryFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic);
static AssignmentReturnValue* parentSVarStoryFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic);
static AssignmentReturnValue* parentFVarStoryFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic);
static AssignmentReturnValue* parentVarStoryFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic);

static AssignmentReturnValue* evaluateSetVariableStoryAssignment(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* tIsStatic) {
	DreamMugenDependOnTwoAssignment* varSetAssignment = (DreamMugenDependOnTwoAssignment*)*tAssignment;

	if (varSetAssignment->a->mType != MUGEN_ASSIGNMENT_TYPE_ARRAY) {
		logWarningFormat("Incorrect varset type %d. Defaulting to bottom.", varSetAssignment->a->mType);
		return makeBottomAssignmentReturn();
	}
	DreamMugenArrayAssignment* varArrayAccess = (DreamMugenArrayAssignment*)varSetAssignment->a;
	int index = convertAssignmentReturnToNumber(evaluateAssignmentInternal(&varArrayAccess->mIndex, tPlayer, tIsStatic));
	auto storyInstance = (StoryInstance*)tPlayer;

	ArrayFunction func = (ArrayFunction)varArrayAccess->mFunc;
	AssignmentReturnValue* ret = NULL;
	if (func == varStoryFunction) {
		int value = convertAssignmentReturnToNumber(evaluateAssignmentInternal(&varSetAssignment->b, tPlayer, tIsStatic));
		setDolmexicaStoryIntegerVariable(storyInstance, index, value);
		ret = makeNumberAssignmentReturn(value);
	}
	else if (func == fVarStoryFunction) {
		double value = convertAssignmentReturnToFloat(evaluateAssignmentInternal(&varSetAssignment->b, tPlayer, tIsStatic));
		setDolmexicaStoryFloatVariable(storyInstance, index, value);
		ret = makeFloatAssignmentReturn(value);
	}
	else if (func == sVarStoryFunction) {
		std::string value;
		convertAssignmentReturnToString(value, evaluateAssignmentInternal(&varSetAssignment->b, tPlayer, tIsStatic));
		setDolmexicaStoryStringVariable(storyInstance, index, value);
		ret = makeStringAssignmentReturn(value.c_str());
	}
	else if (func == parentVarStoryFunction) {
		int value = convertAssignmentReturnToNumber(evaluateAssignmentInternal(&varSetAssignment->b, tPlayer, tIsStatic));
		setDolmexicaStoryIntegerVariable(getDolmexicaStoryInstanceParent(storyInstance), index, value);
		ret = makeNumberAssignmentReturn(value);
	}
	else if (func == parentFVarStoryFunction) {
		double value = convertAssignmentReturnToFloat(evaluateAssignmentInternal(&varSetAssignment->b, tPlayer, tIsStatic));
		setDolmexicaStoryFloatVariable(getDolmexicaStoryInstanceParent(storyInstance), index, value);
		ret = makeFloatAssignmentReturn(value);
	}
	else if (func == parentSVarStoryFunction) {
		std::string value;
		convertAssignmentReturnToString(value, evaluateAssignmentInternal(&varSetAssignment->b, tPlayer, tIsStatic));
		setDolmexicaStoryStringVariable(getDolmexicaStoryInstanceParent(storyInstance), index, value);
		ret = makeStringAssignmentReturn(value.c_str());
	}
	else if (func == rootVarStoryFunction) {
		int value = convertAssignmentReturnToNumber(evaluateAssignmentInternal(&varSetAssignment->b, tPlayer, tIsStatic));
		setDolmexicaStoryIntegerVariable(getDolmexicaStoryRootInstance(), index, value);
		ret = makeNumberAssignmentReturn(value);
	}
	else if (func == rootFVarStoryFunction) {
		double value = convertAssignmentReturnToFloat(evaluateAssignmentInternal(&varSetAssignment->b, tPlayer, tIsStatic));
		setDolmexicaStoryFloatVariable(getDolmexicaStoryRootInstance(), index, value);
		ret = makeFloatAssignmentReturn(value);
	}
	else if (func == rootSVarStoryFunction) {
		std::string value;
		convertAssignmentReturnToString(value, evaluateAssignmentInternal(&varSetAssignment->b, tPlayer, tIsStatic));
		setDolmexicaStoryStringVariable(getDolmexicaStoryRootInstance(), index, value);
		ret = makeStringAssignmentReturn(value.c_str());
	}
	else if (func == globalVarStoryFunction) {
		int value = convertAssignmentReturnToNumber(evaluateAssignmentInternal(&varSetAssignment->b, tPlayer, tIsStatic));
		setGlobalVariable(index, value);
		ret = makeNumberAssignmentReturn(value);
	}
	else if (func == globalFVarStoryFunction) {
		double value = convertAssignmentReturnToFloat(evaluateAssignmentInternal(&varSetAssignment->b, tPlayer, tIsStatic));
		setGlobalFloatVariable(index, value);
		ret = makeFloatAssignmentReturn(value);
	}
	else if (func == globalSVarStoryFunction) {
		std::string value;
		convertAssignmentReturnToString(value, evaluateAssignmentInternal(&varSetAssignment->b, tPlayer, tIsStatic));
		setGlobalStringVariable(index, value);
		ret = makeStringAssignmentReturn(value.c_str());
	}
	else {
		logWarningFormat("Unrecognized varset function %X. Returning bottom.", (void*)func);
		ret = makeBottomAssignmentReturn();
	}
	assert(ret);

	*tIsStatic = 0;
	return ret;
}

static AssignmentReturnValue* evaluateSetVariableAssignment(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* tIsStatic) {
	if (gVariableHandler.mType == MUGEN_ASSIGNMENT_EVALUATOR_TYPE_REGULAR) {
		return evaluateSetVariableRegularAssignment(tAssignment, tPlayer, tIsStatic);
	}
	else {
		return evaluateSetVariableStoryAssignment(tAssignment, tPlayer, tIsStatic);
	}
}

static int evaluateSingleHitDefAttributeFlag2(char* tFlag, MugenAttackClass tClass, MugenAttackType tType) {
	turnStringLowercase(tFlag);

	if (strlen(tFlag) != 2) {
		logWarningFormat("Invalid hitdef attribute flag %s. Default to false.", tFlag);
		return 0;
	}

	int isPart1OK;
	if (tFlag[0] == 'a') {
		isPart1OK = 1;
	}
	else if (tClass == MUGEN_ATTACK_CLASS_NORMAL) {
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

	string test;
	convertAssignmentReturnToString(test, tValue);
	const char* pos = test.data();
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
			return makeBooleanAssignmentReturn(1);
		}

		if (items == 1) hasNext = 0;
		else if (strcmp(",", comma)) {
			logWarningFormat("Invalid hitdef attribute string %s. Defaulting to bottom.", tValue.mValue);
			return makeBottomAssignmentReturn(); 
		}
	}

	return makeBooleanAssignmentReturn(0);
}

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
	int ret = timeOffset == 1;

	*tIsStatic = 0;
	return makeBooleanAssignmentReturn(ret == compareValue);
}

static int getProjectileIDFromAssignmentName(char* tName, const char* tBaseName) {
	char* idOffset = tName += strlen(tBaseName);
	if (*idOffset == '\0') return 0;

	return atoi(idOffset);
}

static AssignmentReturnValue* evaluateProjAssignment(char* tName, const char* tBaseName, AssignmentReturnValue* tCommand, DreamPlayer* tPlayer, int(*tTimeFunc)(DreamPlayer*, int), int* tIsStatic) {
	int projID = getProjectileIDFromAssignmentName(tName, tBaseName);

	if (tCommand->mType == MUGEN_ASSIGNMENT_RETURN_TYPE_VECTOR) {
		return evaluateProjVectorAssignment(tCommand, tPlayer, projID, tTimeFunc, tIsStatic);
	}
	else {
		return evaluateProjNumberAssignment(tCommand, tPlayer, projID, tTimeFunc, tIsStatic);
	}
}

static int isProjAssignment(char* tName, const char* tBaseName) {
	return !strncmp(tName, tBaseName, strlen(tBaseName));

}

static int tryEvaluateVariableComparison(DreamMugenRawVariableAssignment* tVariableAssignment, AssignmentReturnValue** oRet, AssignmentReturnValue* b, DreamPlayer* tPlayer, int* tIsStatic) {

	int hasReturn = 0;
	if(!strcmp("command", tVariableAssignment->mName)) {
		hasReturn = 1;
		if (b->mType == MUGEN_ASSIGNMENT_RETURN_TYPE_NUMBER) {
			*oRet = makeBooleanAssignmentReturn(isPlayerCommandActiveWithLookup(tPlayer, convertAssignmentReturnToNumber(b)));
		}
		else  if (b->mType == MUGEN_ASSIGNMENT_RETURN_TYPE_STRING) {
			*oRet = makeBooleanAssignmentReturn(0); // only triggered when lookup failed before
		}
		else {
			*oRet = makeBooleanAssignmentReturn(0);
		}
		*tIsStatic = 0;
	}
	else if (stl_string_map_contains_array(gVariableHandler.mComparisons, tVariableAssignment->mName)) {
		ComparisonFunction func = gVariableHandler.mComparisons[tVariableAssignment->mName];
		hasReturn = 1;
		*oRet = func(b, tPlayer, tIsStatic);
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
		string val1, val2;
		convertAssignmentReturnToString(val1, a);
		convertAssignmentReturnToString(val2, b);
		int value = val1 == val2;
		return makeBooleanAssignmentReturn(value);
	}
}

static AssignmentReturnValue* evaluateComparisonAssignment(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* tIsStatic) {
	DreamMugenDependOnTwoAssignment* comparisonAssignment = (DreamMugenDependOnTwoAssignment*)*tAssignment;


	if (comparisonAssignment->a->mType == MUGEN_ASSIGNMENT_TYPE_VECTOR) {
		DreamMugenDependOnTwoAssignment* vectorAssignment = (DreamMugenDependOnTwoAssignment*)comparisonAssignment->a;
		if (isPlayerAccessVectorAssignment(&vectorAssignment->a, tPlayer, tIsStatic)) {
			DreamPlayer* target = getPlayerFromFirstVectorPartOrNullIfNonexistant(&vectorAssignment->a, tPlayer, tIsStatic);
			if (!isPlayerTargetValid(target)) {
				logWarning("Accessed player was NULL. Defaulting to bottom.");
				return makeBottomAssignmentReturn(); 
			}
			AssignmentReturnValue* b = evaluateAssignmentDependency(&comparisonAssignment->b, tPlayer, tIsStatic);
			return evaluateComparisonAssignmentInternal(&vectorAssignment->b, b, target, tIsStatic);
		}
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
		*oRet = func(b, tPlayer, tIsStatic, tCompareFunction);
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
	int items = sscanf(test, "%199s", firstW);
	if (!items) return 0;
	turnStringLowercase(firstW);

	return !strcmp("isinotherfilef", firstW) || !strcmp("isinotherfiles", firstW);
}

static AssignmentReturnValue* evaluateAdditionSparkFile(AssignmentReturnValue* a, AssignmentReturnValue* b) {
	char firstW[200];
	int val1;

	string test;
	convertAssignmentReturnToString(test, a);
	int items = sscanf(test.data(), "%199s %d", firstW, &val1);

	int val2 = convertAssignmentReturnToNumber(b);

	if (items != 2) {
		logWarningFormat("Unable to parse sparkfile addition %s. Defaulting to bottom.", test);
		return makeBottomAssignmentReturn();
	}

	std::stringstream ss;
	ss << firstW << " " << val1 + val2;
	return makeStringAssignmentReturn(ss.str().c_str());
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

static AssignmentReturnValue* evaluatePlayerVectorAssignment(DreamMugenAssignment** tFirstValue, DreamMugenDependOnTwoAssignment* tVectorAssignment, DreamPlayer* tPlayer, int* tIsStatic) {
	DreamPlayer* target = getPlayerFromFirstVectorPartOrNullIfNonexistant(tFirstValue, tPlayer, tIsStatic);
	if (!isPlayerTargetValid(target)) {
		logWarning("Unable to evaluate player vector assignment with NULL. Defaulting to bottom.");
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

	if (isPlayerAccessVectorAssignment(&vectorAssignment->a, tPlayer, tIsStatic)) {
		return evaluatePlayerVectorAssignment(&vectorAssignment->a, vectorAssignment, tPlayer, tIsStatic);
	}
	else {
		AssignmentReturnValue* a = evaluateAssignmentDependency(&vectorAssignment->a, tPlayer, tIsStatic);
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

static AssignmentReturnValue* commandComparisonFunction(AssignmentReturnValue* b, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateCommandAssignment(b, tPlayer, tIsStatic); }
static AssignmentReturnValue* stateTypeComparisonFunction(AssignmentReturnValue* b, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateStateTypeAssignment(b, tPlayer, tIsStatic); }
static AssignmentReturnValue* p2StateTypeComparisonFunction(AssignmentReturnValue* b, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateStateTypeAssignment(b, getPlayerOtherPlayer(tPlayer), tIsStatic); }
static AssignmentReturnValue* moveTypeComparisonFunction(AssignmentReturnValue* b, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateMoveTypeAssignment(b, tPlayer, tIsStatic); }
static AssignmentReturnValue* p2MoveTypeComparisonFunction(AssignmentReturnValue* b, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateMoveTypeAssignment(b, getPlayerOtherPlayer(tPlayer), tIsStatic); }
static AssignmentReturnValue* animElemComparisonFunction(AssignmentReturnValue* b, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateAnimElemAssignment(b, tPlayer, tIsStatic); }
static AssignmentReturnValue* timeModComparisonFunction(AssignmentReturnValue* b, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateTimeModAssignment(b, tPlayer, tIsStatic); }
static AssignmentReturnValue* teamModeComparisonFunction(AssignmentReturnValue* b, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateTeamModeAssignment(b, tPlayer, tIsStatic); }
static AssignmentReturnValue* hitDefAttributeComparisonFunction(AssignmentReturnValue* b, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateHitDefAttributeAssignment(b, tPlayer, tIsStatic); }


static AssignmentReturnValue* animElemOrdinalFunction(AssignmentReturnValue* b, DreamPlayer* tPlayer, int* tIsStatic, int(*tCompareFunction)(int, int)) { return evaluateAnimElemOrdinalAssignment(b, tPlayer, tIsStatic, tCompareFunction); }

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

static const char* getPlatformString() {
	if (isOnDreamcast()) return "dreamcast";
	else if(isOnWeb()) return "web";
	else return "windows";
}

static AssignmentReturnValue* aiLevelFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerAILevel(tPlayer)); }
static AssignmentReturnValue* aliveFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(isPlayerAlive(tPlayer)); }
static AssignmentReturnValue* animFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerAnimationNumber(tPlayer)); }
//static AssignmentReturnValue* animElemFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(0); }
static AssignmentReturnValue* animTimeFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerAnimationTimeDeltaUntilFinished(tPlayer)); }
static AssignmentReturnValue* authorNameFunction(DreamPlayer* tPlayer) { return makeStringAssignmentReturn(getPlayerAuthorName(tPlayer)); }
static AssignmentReturnValue* backEdgeFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerScreenEdgeInBackX(tPlayer, getActiveStateMachineCoordinateP())); }
static AssignmentReturnValue* backEdgeBodyDistFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerBackBodyDistanceToScreen(tPlayer, getActiveStateMachineCoordinateP())); }
static AssignmentReturnValue* backEdgeDistFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerBackAxisDistanceToScreen(tPlayer, getActiveStateMachineCoordinateP())); }
static AssignmentReturnValue* bottomEdgeFunction(DreamPlayer* /*tPlayer*/) { return makeFloatAssignmentReturn(getDreamStageBottomEdgeY(getActiveStateMachineCoordinateP())); }
static AssignmentReturnValue* cameraPosXFunction(DreamPlayer* /*tPlayer*/) { return makeFloatAssignmentReturn(getDreamCameraPositionX(getActiveStateMachineCoordinateP())); }
static AssignmentReturnValue* cameraPosYFunction(DreamPlayer* /*tPlayer*/) { return makeFloatAssignmentReturn(getDreamCameraPositionY(getActiveStateMachineCoordinateP())); }
static AssignmentReturnValue* cameraZoomFunction(DreamPlayer* /*tPlayer*/) { return makeFloatAssignmentReturn(getDreamCameraZoom()); }
static AssignmentReturnValue* canRecoverFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(canPlayerRecoverFromFalling(tPlayer)); }
//static AssignmentReturnValue* commandFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(0); }
static AssignmentReturnValue* ctrlFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(getPlayerControl(tPlayer)); }
static AssignmentReturnValue* drawGameFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(hasPlayerDrawn(tPlayer)); }
static AssignmentReturnValue* eFunction(DreamPlayer* /*tPlayer*/) { return makeFloatAssignmentReturn(M_E); }
static AssignmentReturnValue* facingFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerIsFacingRight(tPlayer) ? 1 : -1); }
static AssignmentReturnValue* frontEdgeFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerScreenEdgeInFrontX(tPlayer, getActiveStateMachineCoordinateP())); }
static AssignmentReturnValue* frontEdgeBodyDistFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerFrontBodyDistanceToScreen(tPlayer, getActiveStateMachineCoordinateP())); }
static AssignmentReturnValue* frontEdgeDistFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerFrontAxisDistanceToScreen(tPlayer, getActiveStateMachineCoordinateP())); }
static AssignmentReturnValue* gameHeightFunction(DreamPlayer* /*tPlayer*/) { return makeFloatAssignmentReturn(getDreamGameHeight(getActiveStateMachineCoordinateP())); }
static AssignmentReturnValue* gameTimeFunction(DreamPlayer* /*tPlayer*/) { return makeNumberAssignmentReturn(getDreamGameTime()); }
static AssignmentReturnValue* gameWidthFunction(DreamPlayer* /*tPlayer*/) { return makeFloatAssignmentReturn(getDreamGameWidth(getActiveStateMachineCoordinateP())); }
static AssignmentReturnValue* hitCountFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerHitCount(tPlayer)); }
//static AssignmentReturnValue* hitDefAttrFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(0); }
static AssignmentReturnValue* hitFallFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(isPlayerFalling(tPlayer)); }
static AssignmentReturnValue* hitOverFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(isPlayerHitOver(tPlayer)); }
static AssignmentReturnValue* hitPauseTimeFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerTimeLeftInHitPause(tPlayer)); }
static AssignmentReturnValue* hitShakeOverFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(isPlayerHitShakeOver(tPlayer));  }
static AssignmentReturnValue* hitVelXFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerHitVelocityX(tPlayer, getActiveStateMachineCoordinateP())); }
static AssignmentReturnValue* hitVelYFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerHitVelocityY(tPlayer, getActiveStateMachineCoordinateP())); }
static AssignmentReturnValue* idFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(tPlayer->mID); }
static AssignmentReturnValue* inGuardDistFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(isPlayerInGuardDistance(tPlayer)); }
static AssignmentReturnValue* isHelperFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(isPlayerHelper(tPlayer)); }
static AssignmentReturnValue* isHomeTeamFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(isPlayerHomeTeam(tPlayer)); }
static AssignmentReturnValue* leftEdgeFunction(DreamPlayer* /*tPlayer*/) { return makeFloatAssignmentReturn(getDreamStageLeftEdgeX(getActiveStateMachineCoordinateP())); }
static AssignmentReturnValue* lifeFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerLife(tPlayer)); }
static AssignmentReturnValue* lifeMaxFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerLifeMax(tPlayer)); }
static AssignmentReturnValue* loseFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(hasPlayerLost(tPlayer)); }
static AssignmentReturnValue* matchNoFunction(DreamPlayer* /*tPlayer*/) { return makeNumberAssignmentReturn(getDreamMatchNumber()); }
static AssignmentReturnValue* matchOverFunction(DreamPlayer* /*tPlayer*/) { return makeBooleanAssignmentReturn(isDreamMatchOver()); }
static AssignmentReturnValue* moveContactFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerMoveContactCounter(tPlayer)); }
static AssignmentReturnValue* moveGuardedFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerMoveGuarded(tPlayer)); }
static AssignmentReturnValue* moveHitFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerMoveHit(tPlayer)); }
//static AssignmentReturnValue* moveTypeFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(0); }
static AssignmentReturnValue* moveReversedFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerMoveReversed(tPlayer)); }
static AssignmentReturnValue* nameFunction(DreamPlayer* tPlayer) { return makeStringAssignmentReturn(getPlayerName(tPlayer)); }
static AssignmentReturnValue* numEnemyFunction(DreamPlayer* /*tPlayer*/) { return makeNumberAssignmentReturn(1); }
static AssignmentReturnValue* numExplodFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getExplodAmount(tPlayer)); }
static AssignmentReturnValue* numHelperFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerHelperAmount(tPlayer)); }
static AssignmentReturnValue* numPartnerFunction(DreamPlayer* /*tPlayer*/) { return makeNumberAssignmentReturn(0); }
static AssignmentReturnValue* numProjFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerProjectileAmount(tPlayer)); }
static AssignmentReturnValue* numTargetFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerTargetAmount(tPlayer)); }
static AssignmentReturnValue* p1NameFunction(DreamPlayer* tPlayer) { return nameFunction(tPlayer); }
static AssignmentReturnValue* p2BodyDistFunctionX(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerDistanceToFrontOfOtherPlayerX(tPlayer, getActiveStateMachineCoordinateP())); }
static AssignmentReturnValue* p2BodyDistFunctionY(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerAxisDistanceY(tPlayer, getActiveStateMachineCoordinateP())); }
static AssignmentReturnValue* p2DistFunctionX(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerAxisDistanceX(tPlayer, getActiveStateMachineCoordinateP())); }
static AssignmentReturnValue* p2DistFunctionY(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerAxisDistanceY(tPlayer, getActiveStateMachineCoordinateP())); }
static AssignmentReturnValue* p2LifeFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerLife(getPlayerOtherPlayer(tPlayer))); }
//static AssignmentReturnValue* p2MoveTypeFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(0); }
static AssignmentReturnValue* p2NameFunction(DreamPlayer* tPlayer) { return makeStringAssignmentReturn(getPlayerName(getPlayerOtherPlayer(tPlayer))); }
static AssignmentReturnValue* p2StateNoFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerState(getPlayerOtherPlayer(tPlayer))); }
//static AssignmentReturnValue* p2StateTypeFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(0); }
static AssignmentReturnValue* p3NameFunction(DreamPlayer* /*tPlayer*/) { return makeStringAssignmentReturn(""); }
static AssignmentReturnValue* p4NameFunction(DreamPlayer* /*tPlayer*/) { return makeStringAssignmentReturn(""); }
static AssignmentReturnValue* palNoFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerPaletteNumber(tPlayer)); }
static AssignmentReturnValue* parentDistXFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerDistanceToParentX(tPlayer, getActiveStateMachineCoordinateP())); }
static AssignmentReturnValue* parentDistYFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerDistanceToParentY(tPlayer, getActiveStateMachineCoordinateP())); }
static AssignmentReturnValue* piFunction(DreamPlayer* /*tPlayer*/) { return makeFloatAssignmentReturn(M_PI); }
static AssignmentReturnValue* posXFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerPositionBasedOnScreenCenterX(tPlayer, getActiveStateMachineCoordinateP())); }
static AssignmentReturnValue* posYFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerPositionBasedOnStageFloorY(tPlayer, getActiveStateMachineCoordinateP())); }
static AssignmentReturnValue* powerFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerPower(tPlayer)); }
static AssignmentReturnValue* powerMaxFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerPowerMax(tPlayer)); }
static AssignmentReturnValue* prevStateNoFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerPreviousState(tPlayer)); }
//static AssignmentReturnValue* projContactFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(0); }
//static AssignmentReturnValue* projGuardedFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(0); }
//static AssignmentReturnValue* projHitFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(0); }
static AssignmentReturnValue* randomFunction(DreamPlayer* /*tPlayer*/) { return makeNumberAssignmentReturn(randfromInteger(0, 999)); }
static AssignmentReturnValue* rightEdgeFunction(DreamPlayer* /*tPlayer*/) { return makeFloatAssignmentReturn(getDreamStageRightEdgeX(getActiveStateMachineCoordinateP())); }
static AssignmentReturnValue* rootDistXFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerDistanceToRootX(tPlayer, getActiveStateMachineCoordinateP())); }
static AssignmentReturnValue* rootDistYFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerDistanceToRootY(tPlayer, getActiveStateMachineCoordinateP())); }
static AssignmentReturnValue* roundNoFunction(DreamPlayer* /*tPlayer*/) { return makeNumberAssignmentReturn(getDreamRoundNumber()); }
static AssignmentReturnValue* roundsExistedFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerRoundsExisted(tPlayer)); }
static AssignmentReturnValue* roundStateFunction(DreamPlayer* /*tPlayer*/) { return makeNumberAssignmentReturn(getDreamRoundStateNumber()); }
static AssignmentReturnValue* screenPosXFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerScreenPositionX(tPlayer, getActiveStateMachineCoordinateP())); }
static AssignmentReturnValue* screenPosYFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerScreenPositionY(tPlayer, getActiveStateMachineCoordinateP())); }
static AssignmentReturnValue* screenHeightFunction(DreamPlayer* /*tPlayer*/) { return makeFloatAssignmentReturn(getDreamScreenHeight(getActiveStateMachineCoordinateP())); }
static AssignmentReturnValue* screenWidthFunction(DreamPlayer* /*tPlayer*/) { return makeFloatAssignmentReturn(getDreamScreenWidth(getActiveStateMachineCoordinateP())); }
static AssignmentReturnValue* stateNoFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerState(tPlayer)); }
//static AssignmentReturnValue* stateTypeFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(0); }
//static AssignmentReturnValue* teamModeFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(0); }
static AssignmentReturnValue* teamSideFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(tPlayer->mRootID + 1); }
static AssignmentReturnValue* ticksPerSecondFunction(DreamPlayer* /*tPlayer*/) { return makeNumberAssignmentReturn(getDreamTicksPerSecond()); }
static AssignmentReturnValue* timeFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerTimeInState(tPlayer)); }
//static AssignmentReturnValue* timeModFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(0); }
static AssignmentReturnValue* topEdgeFunction(DreamPlayer* /*tPlayer*/) { return makeFloatAssignmentReturn(getDreamStageTopEdgeY(getActiveStateMachineCoordinateP())); }
static AssignmentReturnValue* uniqHitCountFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerUniqueHitCount(tPlayer)); }
static AssignmentReturnValue* velXFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerVelocityX(tPlayer, getActiveStateMachineCoordinateP())); }
static AssignmentReturnValue* velYFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerVelocityY(tPlayer, getActiveStateMachineCoordinateP())); }
static AssignmentReturnValue* winFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(hasPlayerWon(tPlayer)); }
static AssignmentReturnValue* winKOFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(hasPlayerWonByKO(tPlayer)); }
static AssignmentReturnValue* winPerfectFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(hasPlayerWonPerfectly(tPlayer)); }
static AssignmentReturnValue* winTimeFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(hasPlayerWonByTime(tPlayer)); }

static AssignmentReturnValue* inputAllowedFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(isPlayerInputAllowed(tPlayer)); }
static AssignmentReturnValue* platformFunction(DreamPlayer* /*tPlayer*/) { return makeStringAssignmentReturn(getPlatformString()); }

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
static AssignmentReturnValue* sizeGroundBackFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerGroundSizeBack(tPlayer, getActiveStateMachineCoordinateP())); }
static AssignmentReturnValue* sizeGroundFrontFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerGroundSizeFront(tPlayer, getActiveStateMachineCoordinateP())); }
static AssignmentReturnValue* sizeAirBackFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerSizeAirBack(tPlayer, getActiveStateMachineCoordinateP())); }
static AssignmentReturnValue* sizeAirFrontFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerSizeAirFront(tPlayer, getActiveStateMachineCoordinateP())); }
static AssignmentReturnValue* sizeHeightFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerHeight(tPlayer, getActiveStateMachineCoordinateP())); }
static AssignmentReturnValue* sizeAttackDistFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerSizeAttackDist(tPlayer, getActiveStateMachineCoordinateP())); }
static AssignmentReturnValue* sizeProjAttackDistFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerSizeProjectileAttackDist(tPlayer, getActiveStateMachineCoordinateP())); }
static AssignmentReturnValue* sizeProjDoScaleFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerSizeProjectilesDoScale(tPlayer)); }
static AssignmentReturnValue* sizeHeadPosXFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerHeadPositionX(tPlayer, getActiveStateMachineCoordinateP())); }
static AssignmentReturnValue* sizeHeadPosYFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerHeadPositionY(tPlayer, getActiveStateMachineCoordinateP())); }
static AssignmentReturnValue* sizeMidPosXFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerMiddlePositionX(tPlayer, getActiveStateMachineCoordinateP())); }
static AssignmentReturnValue* sizeMidPosYFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerMiddlePositionY(tPlayer, getActiveStateMachineCoordinateP())); }
static AssignmentReturnValue* sizeShadowOffsetFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerSizeShadowOffset(tPlayer, getActiveStateMachineCoordinateP())); }
static AssignmentReturnValue* sizeDrawOffsetXFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerSizeDrawOffsetX(tPlayer, getActiveStateMachineCoordinateP())); }
static AssignmentReturnValue* sizeDrawOffsetYFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerSizeDrawOffsetY(tPlayer, getActiveStateMachineCoordinateP())); }

static AssignmentReturnValue* velocityWalkFwdXFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerForwardWalkVelocityX(tPlayer, getActiveStateMachineCoordinateP())); }
static AssignmentReturnValue* velocityWalkBackXFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerBackwardWalkVelocityX(tPlayer, getActiveStateMachineCoordinateP())); }
static AssignmentReturnValue* velocityRunFwdXFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerForwardRunVelocityX(tPlayer, getActiveStateMachineCoordinateP())); }
static AssignmentReturnValue* velocityRunFwdYFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerForwardRunVelocityY(tPlayer, getActiveStateMachineCoordinateP())); }
static AssignmentReturnValue* velocityRunBackXFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerBackwardRunVelocityX(tPlayer, getActiveStateMachineCoordinateP())); }
static AssignmentReturnValue* velocityRunBackYFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerBackwardRunVelocityY(tPlayer, getActiveStateMachineCoordinateP()));; }
static AssignmentReturnValue* velocityJumpYFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerJumpVelocityY(tPlayer, getActiveStateMachineCoordinateP())); }
static AssignmentReturnValue* velocityJumpNeuXFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerNeutralJumpVelocityX(tPlayer, getActiveStateMachineCoordinateP())); }
static AssignmentReturnValue* velocityJumpBackXFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerBackwardJumpVelocityX(tPlayer, getActiveStateMachineCoordinateP())); }
static AssignmentReturnValue* velocityJumpFwdXFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerForwardJumpVelocityX(tPlayer, getActiveStateMachineCoordinateP())); }
static AssignmentReturnValue* velocityRunJumpBackXFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerBackwardRunJumpVelocityX(tPlayer, getActiveStateMachineCoordinateP())); }
static AssignmentReturnValue* velocityRunJumpFwdXFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerForwardRunJumpVelocityX(tPlayer, getActiveStateMachineCoordinateP())); }
static AssignmentReturnValue* velocityAirJumpYFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerAirJumpVelocityY(tPlayer, getActiveStateMachineCoordinateP())); }
static AssignmentReturnValue* velocityAirJumpNeuXFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerNeutralAirJumpVelocityX(tPlayer, getActiveStateMachineCoordinateP())); }
static AssignmentReturnValue* velocityAirJumpBackXFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerBackwardAirJumpVelocityX(tPlayer, getActiveStateMachineCoordinateP())); }
static AssignmentReturnValue* velocityAirJumpFwdXFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerForwardAirJumpVelocityX(tPlayer, getActiveStateMachineCoordinateP())); }
static AssignmentReturnValue* velocityAirGetHitGroundRecoverXFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerVelocityAirGetHitGroundRecoverX(tPlayer, getActiveStateMachineCoordinateP())); }
static AssignmentReturnValue* velocityAirGetHitGroundRecoverYFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerVelocityAirGetHitGroundRecoverY(tPlayer, getActiveStateMachineCoordinateP())); }
static AssignmentReturnValue* velocityAirGetHitAirRecoverMulXFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerVelocityAirGetHitAirRecoverMulX(tPlayer)); }
static AssignmentReturnValue* velocityAirGetHitAirRecoverMulYFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerVelocityAirGetHitAirRecoverMulY(tPlayer)); }
static AssignmentReturnValue* velocityAirGetHitAirRecoverAddXFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerVelocityAirGetHitAirRecoverAddX(tPlayer, getActiveStateMachineCoordinateP())); }
static AssignmentReturnValue* velocityAirGetHitAirRecoverAddYFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerVelocityAirGetHitAirRecoverAddY(tPlayer, getActiveStateMachineCoordinateP())); }
static AssignmentReturnValue* velocityAirGetHitAirRecoverBackFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerVelocityAirGetHitAirRecoverBack(tPlayer, getActiveStateMachineCoordinateP())); }
static AssignmentReturnValue* velocityAirGetHitAirRecoverFwdFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerVelocityAirGetHitAirRecoverFwd(tPlayer, getActiveStateMachineCoordinateP())); }
static AssignmentReturnValue* velocityAirGetHitAirRecoverUpFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerVelocityAirGetHitAirRecoverUp(tPlayer, getActiveStateMachineCoordinateP())); }
static AssignmentReturnValue* velocityAirGetHitAirRecoverDownFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerVelocityAirGetHitAirRecoverDown(tPlayer, getActiveStateMachineCoordinateP())); }

static AssignmentReturnValue* movementAirJumpNumFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerMovementAirJumpNum(tPlayer)); }
static AssignmentReturnValue* movementAirJumpHeightFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerMovementAirJumpHeight(tPlayer, getActiveStateMachineCoordinateP())); }
static AssignmentReturnValue* movementYAccelFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerVerticalAcceleration(tPlayer, getActiveStateMachineCoordinateP())); }
static AssignmentReturnValue* movementStandFrictionFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerStandFriction(tPlayer)); }
static AssignmentReturnValue* movementCrouchFrictionFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerCrouchFriction(tPlayer)); }
static AssignmentReturnValue* movementStandFrictionThresholdFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerStandFrictionThreshold(tPlayer, getActiveStateMachineCoordinateP())); }
static AssignmentReturnValue* movementCrouchFrictionThresholdFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerCrouchFrictionThreshold(tPlayer, getActiveStateMachineCoordinateP())); }
static AssignmentReturnValue* movementJumpChangeAnimThresholdFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerMovementJumpChangeAnimThreshold(tPlayer, getActiveStateMachineCoordinateP())); }
static AssignmentReturnValue* movementAirGetHitGroundLevelFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerAirGetHitGroundLevelY(tPlayer, getActiveStateMachineCoordinateP())); }
static AssignmentReturnValue* movementAirGetHitGroundRecoverGroundThresholdFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerAirGetHitGroundRecoveryGroundYTheshold(tPlayer, getActiveStateMachineCoordinateP())); }
static AssignmentReturnValue* movementAirGetHitGroundRecoverGroundLevelFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerAirGetHitGroundRecoveryGroundLevelY(tPlayer, getActiveStateMachineCoordinateP())); }
static AssignmentReturnValue* movementAirGetHitAirRecoverThresholdFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerAirGetHitAirRecoveryVelocityYThreshold(tPlayer, getActiveStateMachineCoordinateP())); }
static AssignmentReturnValue* movementAirGetHitAirRecoverYAccelFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerMovementAirGetHitAirRecoverYAccel(tPlayer, getActiveStateMachineCoordinateP())); }
static AssignmentReturnValue* movementAirGetHitTripGroundLevelFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerAirGetHitTripGroundLevelY(tPlayer, getActiveStateMachineCoordinateP())); }
static AssignmentReturnValue* movementDownBounceOffsetXFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerDownBounceOffsetX(tPlayer, getActiveStateMachineCoordinateP())); }
static AssignmentReturnValue* movementDownBounceOffsetYFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerDownBounceOffsetY(tPlayer, getActiveStateMachineCoordinateP())); }
static AssignmentReturnValue* movementDownBounceYAccelFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerDownVerticalBounceAcceleration(tPlayer, getActiveStateMachineCoordinateP())); }
static AssignmentReturnValue* movementDownBounceGroundLevelFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerDownBounceGroundLevel(tPlayer, getActiveStateMachineCoordinateP())); }
static AssignmentReturnValue* movementDownFrictionThresholdFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerLyingDownFrictionThreshold(tPlayer, getActiveStateMachineCoordinateP())); }

static AssignmentReturnValue* getHitVarXVelAddFunction(DreamPlayer* /*tPlayer*/) { return makeFloatAssignmentReturn(0.0); }
static AssignmentReturnValue* getHitVarYVelAddFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerDeathVelAddY(tPlayer, getActiveStateMachineCoordinateP())); }
static AssignmentReturnValue* getHitVarTypeFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getActiveHitDataAttackType(tPlayer)); }
static AssignmentReturnValue* getHitVarAnimtypeFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getActiveHitDataAnimationType(tPlayer)); }
static AssignmentReturnValue* getHitVarGroundtypeFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getActiveHitDataGroundType(tPlayer)); }
static AssignmentReturnValue* getHitVarAirtypeFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getActiveHitDataAirType(tPlayer)); }
static AssignmentReturnValue* getHitVarXOffFunction(DreamPlayer* /*tPlayer*/) { return makeFloatAssignmentReturn(0.0); } // deprecated
static AssignmentReturnValue* getHitVarYOffFunction(DreamPlayer* /*tPlayer*/) { return makeFloatAssignmentReturn(0.0); } // deprecated
static AssignmentReturnValue* getHitVarXvelFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getActiveHitDataVelocityX(tPlayer)); }
static AssignmentReturnValue* getHitVarYvelFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getActiveHitDataVelocityY(tPlayer)); }
static AssignmentReturnValue* getHitVarYaccelFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getActiveHitDataYAccel(tPlayer)); }
static AssignmentReturnValue* getHitVarFallFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(getActiveHitDataFall(tPlayer)); }
static AssignmentReturnValue* getHitVarFallDamageFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getActiveHitDataFallDamage(tPlayer)); }
static AssignmentReturnValue* getHitVarFallRecoverFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(getActiveHitDataFallRecovery(tPlayer)); }
static AssignmentReturnValue* getHitVarFallRecoverTimeFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getActiveHitDataFallRecoveryTime(tPlayer)); }
static AssignmentReturnValue* getHitVarFallXVelFunction(DreamPlayer* tPlayer) {	return makeFloatAssignmentReturn(getActiveHitDataFallXVelocity(tPlayer));}
static AssignmentReturnValue* getHitVarFallYVelFunction(DreamPlayer* tPlayer) {	return makeFloatAssignmentReturn(getActiveHitDataFallYVelocity(tPlayer));}
static AssignmentReturnValue* getHitVarFallKillFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(getActiveHitDataFallKill(tPlayer)); }
static AssignmentReturnValue* getHitVarFallEnvshakeTimeFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getActiveHitDataFallEnvironmentShakeTime(tPlayer)); }
static AssignmentReturnValue* getHitVarFallEnvshakeFreqFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getActiveHitDataFallEnvironmentShakeFrequency(tPlayer)); }
static AssignmentReturnValue* getHitVarFallEnvshakeAmplFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getActiveHitDataFallEnvironmentShakeAmplitude(tPlayer)); }
static AssignmentReturnValue* getHitVarFallEnvshakePhaseFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getActiveHitDataFallEnvironmentShakePhase(tPlayer)); }
static AssignmentReturnValue* getHitVarSlidetimeFunction(DreamPlayer* tPlayer) {	return makeBooleanAssignmentReturn(getPlayerSlideTime(tPlayer));}
static AssignmentReturnValue* getHitVarHitshaketimeFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(getActiveHitDataPlayer2PauseTime(tPlayer));}
static AssignmentReturnValue* getHitVarHitTimeFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerHitTime(tPlayer));}
static AssignmentReturnValue* getHitVarHitcountFunction(DreamPlayer* tPlayer) {	return makeNumberAssignmentReturn(getPlayerHitCount(tPlayer));}
static AssignmentReturnValue* getHitVarDamageFunction(DreamPlayer* tPlayer) {return makeNumberAssignmentReturn(getActiveHitDataDamage(tPlayer));}
static AssignmentReturnValue* getHitVarFallcountFunction(DreamPlayer* tPlayer) {	return makeNumberAssignmentReturn(getPlayerFallAmountInCombo(tPlayer));}
static AssignmentReturnValue* getHitVarCtrltimeFunction(DreamPlayer* tPlayer) {	return makeNumberAssignmentReturn(getPlayerControlTime(tPlayer));}
static AssignmentReturnValue* getHitVarRecoverTimeFunction(DreamPlayer* tPlayer) {	return makeNumberAssignmentReturn(getPlayerRecoverTime(tPlayer));}
static AssignmentReturnValue* getHitVarIsBoundFunction(DreamPlayer* tPlayer) {	return makeBooleanAssignmentReturn(isPlayerBound(tPlayer));}
static AssignmentReturnValue* getHitVarChainIDFunction(DreamPlayer* tPlayer) {	return makeNumberAssignmentReturn(getActiveHitDataChainID(tPlayer));}
static AssignmentReturnValue* getHitVarGuardedFunction(DreamPlayer* tPlayer) {	return makeBooleanAssignmentReturn(getLastPlayerHitGuarded(tPlayer));}

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

	gVariableHandler.mVariables["uniqhitcount"] = uniqHitCountFunction;
	gVariableHandler.mVariables["uniquehitcount"] = uniqHitCountFunction;

	gVariableHandler.mVariables["vel x"] = velXFunction;
	gVariableHandler.mVariables["vel y"] = velYFunction;

	gVariableHandler.mVariables["win"] = winFunction;
	gVariableHandler.mVariables["winko"] = winKOFunction;
	gVariableHandler.mVariables["winperfect"] = winPerfectFunction;
	gVariableHandler.mVariables["wintime"] = winTimeFunction;

	// Dolmexica vars
	gVariableHandler.mVariables["inputallowed"] = inputAllowedFunction;
	gVariableHandler.mVariables["platform"] = platformFunction;

	// Arrays
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
	gVariableHandler.mVariables["const(velocity.runjump.y)"] = velocityJumpYFunction;
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

	gVariableHandler.mVariables["gethitvar(xveladd)"] = getHitVarXVelAddFunction;
	gVariableHandler.mVariables["gethitvar(yveladd)"] = getHitVarYVelAddFunction;
	gVariableHandler.mVariables["gethitvar(type)"] = getHitVarTypeFunction;
	gVariableHandler.mVariables["gethitvar(animtype)"] = getHitVarAnimtypeFunction;
	gVariableHandler.mVariables["gethitvar(airtype)"] = getHitVarAirtypeFunction;
	gVariableHandler.mVariables["gethitvar(groundtype)"] = getHitVarGroundtypeFunction;
	gVariableHandler.mVariables["gethitvar(damage)"] = getHitVarDamageFunction;
	gVariableHandler.mVariables["gethitvar(hitcount)"] = getHitVarHitcountFunction;
	gVariableHandler.mVariables["gethitvar(fallcount)"] = getHitVarFallcountFunction;
	gVariableHandler.mVariables["gethitvar(hitshaketime)"] = getHitVarHitshaketimeFunction;
	gVariableHandler.mVariables["gethitvar(hittime)"] = getHitVarHitTimeFunction;
	gVariableHandler.mVariables["gethitvar(slidetime)"] = getHitVarSlidetimeFunction;
	gVariableHandler.mVariables["gethitvar(ctrltime)"] = getHitVarCtrltimeFunction;
	gVariableHandler.mVariables["gethitvar(recovertime)"] = getHitVarRecoverTimeFunction;
	gVariableHandler.mVariables["gethitvar(xoff)"] = getHitVarXOffFunction;
	gVariableHandler.mVariables["gethitvar(yoff)"] = getHitVarYOffFunction;
	gVariableHandler.mVariables["gethitvar(xvel)"] = getHitVarXvelFunction;
	gVariableHandler.mVariables["gethitvar(yvel)"] = getHitVarYvelFunction;
	gVariableHandler.mVariables["gethitvar(yaccel)"] = getHitVarYaccelFunction;
	gVariableHandler.mVariables["gethitvar(chainid)"] = getHitVarChainIDFunction;
	gVariableHandler.mVariables["gethitvar(guarded)"] = getHitVarGuardedFunction;
	gVariableHandler.mVariables["gethitvar(isbound)"] = getHitVarIsBoundFunction;
	gVariableHandler.mVariables["gethitvar(fall)"] = getHitVarFallFunction;
	gVariableHandler.mVariables["gethitvar(fall.damage)"] = getHitVarFallDamageFunction;
	gVariableHandler.mVariables["gethitvar(fall.xvel)"] = getHitVarFallXVelFunction;
	gVariableHandler.mVariables["gethitvar(fall.yvel)"] = getHitVarFallYVelFunction;
	gVariableHandler.mVariables["gethitvar(fall.recover)"] = getHitVarFallRecoverFunction;
	gVariableHandler.mVariables["gethitvar(fall.recovertime)"] = getHitVarFallRecoverTimeFunction;
	gVariableHandler.mVariables["gethitvar(fall.kill)"] = getHitVarFallKillFunction;
	gVariableHandler.mVariables["gethitvar(fall.envshake.time)"] = getHitVarFallEnvshakeTimeFunction;
	gVariableHandler.mVariables["gethitvar(fall.envshake.freq)"] = getHitVarFallEnvshakeFreqFunction;
	gVariableHandler.mVariables["gethitvar(fall.envshake.ampl)"] = getHitVarFallEnvshakeAmplFunction;
	gVariableHandler.mVariables["gethitvar(fall.envshake.phase)"] = getHitVarFallEnvshakePhaseFunction;
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

static AssignmentReturnValue* makeExternalFileAssignmentReturn(char tIdentifierCharacter, const char* tValueString) {
	std::stringstream ss;
	ss << "isinotherfile" << tIdentifierCharacter << " " << tValueString;
	return makeStringAssignmentReturn(ss.str().c_str());
}

static AssignmentReturnValue* evaluateVariableAssignment(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* tIsStatic) {
	DreamMugenVariableAssignment* variable = (DreamMugenVariableAssignment*)*tAssignment;
	*tIsStatic = 0;
	AssignmentReturnValue*(*func)(DreamPlayer*) = (AssignmentReturnValue*(*)(DreamPlayer*))variable->mFunc;

	return func(tPlayer);
}

static AssignmentReturnValue* evaluateRawVariableAssignment(DreamMugenAssignment** tAssignment, DreamPlayer* /*tPlayer*/, int* tIsStatic) {
	DreamMugenRawVariableAssignment* variable = (DreamMugenRawVariableAssignment*)*tAssignment;
	*tIsStatic = 0;

	if (isIsInOtherFileVariable(variable->mName)) {
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

static AssignmentReturnValue* evaluateGlobalVarArrayAssignment(AssignmentReturnValue* tIndex, DreamPlayer* /*tPlayer*/, int* tIsStatic) {
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

static AssignmentReturnValue* evaluateGlobalFVarArrayAssignment(AssignmentReturnValue* tIndex, DreamPlayer* /*tPlayer*/, int* tIsStatic) {
	int id = convertAssignmentReturnToNumber(tIndex);
	double val = getGlobalFloatVariable(id);

	*tIsStatic = 0;
	return makeFloatAssignmentReturn(val);
}

static AssignmentReturnValue* evaluateStageVarArrayAssignment(AssignmentReturnValue* tIndex, DreamPlayer* /*tPlayer*/, int* /*tIsStatic*/) {
	string var;
	convertAssignmentReturnToString(var, tIndex);

	AssignmentReturnValue* ret = NULL;
	if ("info.author" == var) {
		ret = makeStringAssignmentReturn(getDreamStageAuthor());
	}
	else if("info.displayname" == var) {
		ret = makeStringAssignmentReturn(getDreamStageDisplayName());
	}
	else if ("info.name" == var) {
		ret = makeStringAssignmentReturn(getDreamStageName());
	}
	else {
		logWarningFormat("Unknown stage variable %s. Returning bottom.", var);
		ret = makeBottomAssignmentReturn(); 
	}
	assert(ret);

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

	string test;
	convertAssignmentReturnToString(test, tIndex);
	int items = sscanf(test.data(), "%99s %19s %99s %19s %99s", condText, comma1, yesText, comma2, noText);

	if (items != 5 || strcmp(",", comma1) || strcmp(",", comma2) || !strcmp("", condText) || !strcmp("", yesText) || !strcmp("", noText)) {
		logWarningFormat("Unable to parse if else array assignment %s. Defaulting to bottom.", test);
		return makeBottomAssignmentReturn(); 
	}

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
	string val2;
	convertAssignmentReturnToString(val2, b);
	AssignmentReturnValue* ret = makeExternalFileAssignmentReturn(tIdentifierCharacter, val2.data());
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

static AssignmentReturnValue* evaluateTargetArrayAssignment(AssignmentReturnValue* tIndex, const char* tTargetName) {
	int id = convertAssignmentReturnToNumber(tIndex);

	std::stringstream ss;
	ss << tTargetName << " " << id;
	return makeStringAssignmentReturn(ss.str().c_str());
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

static AssignmentReturnValue* evaluateProjectileContactArrayAssignment(AssignmentReturnValue* tIndex, DreamPlayer* tPlayer, int* tIsStatic) {
	int id = convertAssignmentReturnToNumber(tIndex);
	*tIsStatic = 0;
	return makeNumberAssignmentReturn(getPlayerProjectileContact(tPlayer, id));
}

static AssignmentReturnValue* evaluateProjectileGuardedArrayAssignment(AssignmentReturnValue* tIndex, DreamPlayer* tPlayer, int* tIsStatic) {
	int id = convertAssignmentReturnToNumber(tIndex);
	*tIsStatic = 0;
	return makeNumberAssignmentReturn(getPlayerProjectileGuarded(tPlayer, id));
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
static AssignmentReturnValue* const240pFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateConstCoordinatesArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), 320); }
static AssignmentReturnValue* const480pFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateConstCoordinatesArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), 640); }
static AssignmentReturnValue* const720pFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateConstCoordinatesArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), 1280); }
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
static AssignmentReturnValue* projContactArrayFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateProjectileContactArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), tPlayer, tIsStatic); }
static AssignmentReturnValue* projGuardedArrayFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateProjectileGuardedArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), tPlayer, tIsStatic); }

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
	gVariableHandler.mArrays["target"] = numTargetArrayFunction;
	gVariableHandler.mArrays["enemy"] = numTargetArrayFunction;
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
	gVariableHandler.mArrays["projcontact"] = projContactArrayFunction;
	gVariableHandler.mArrays["projguarded"] = projGuardedArrayFunction;
}

static AssignmentReturnValue* evaluateArrayAssignment(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* tIsStatic) {
	DreamMugenArrayAssignment* arrays = (DreamMugenArrayAssignment*)*tAssignment;

	ArrayFunction func = (ArrayFunction)arrays->mFunc;
	return func(&arrays->mIndex, tPlayer, tIsStatic);
}

static AssignmentReturnValue* evaluateBitwiseInversionAssignment(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* tIsStatic) {
	DreamMugenDependOnOneAssignment* baseVal = (DreamMugenDependOnOneAssignment*)*tAssignment;

	const auto a = evaluateAssignmentDependency(&baseVal->a, tPlayer, tIsStatic);
	return makeNumberAssignmentReturn(~convertAssignmentReturnToNumber(a));
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
	AssignmentReturnValue mValue;
} DreamMugenStaticAssignment;

int gPruneAmount;

static DreamMugenAssignment* makeStaticDreamMugenAssignment(AssignmentReturnValue* tValue) {
	DreamMugenStaticAssignment* e = (DreamMugenStaticAssignment*)allocMemory(sizeof(DreamMugenStaticAssignment));
	e->mType = MUGEN_ASSIGNMENT_TYPE_STATIC;
	e->mValue = *tValue;

	gPruneAmount++;
	return (DreamMugenAssignment*)e;
}

static AssignmentReturnValue* evaluateStaticAssignment(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* tIsStatic) {
	(void)tPlayer;
	(void)tIsStatic;

	DreamMugenStaticAssignment* stat = (DreamMugenStaticAssignment*)*tAssignment;
	return makeAssignmentReturnAssignmentReturn(stat->mValue);
}

typedef AssignmentReturnValue*(AssignmentEvaluationFunction)(DreamMugenAssignment**, DreamPlayer*, int*);

static void* gEvaluationFunctions[] = {
	(void*)evaluateBooleanAssignment,
	(void*)evaluateAndAssignment,
	(void*)evaluateXorAssignment,
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
	(void*)evaluateBitwiseInversionAssignment,
	(void*)evaluateUnaryMinusAssignment,
	(void*)evaluateOperatorArgumentAssignment,
	(void*)evaluateBitwiseAndAssignment,
	(void*)evaluateBitwiseXorAssignment,
	(void*)evaluateBitwiseOrAssignment,
	(void*)evaluateStaticAssignment,
};

static void setAssignmentStatic(DreamMugenAssignment** tAssignment, AssignmentReturnValue* tValue) {
	if (tValue->mType == MUGEN_ASSIGNMENT_RETURN_TYPE_VECTOR) return;
	if (tValue->mType == MUGEN_ASSIGNMENT_RETURN_TYPE_RANGE) return;

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
		*tAssignment = makeStaticDreamMugenAssignment(tValue);
		break;
	default:
		break;
	}
}

static AssignmentReturnValue* evaluateAssignmentInternal(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* oIsStatic) {
	setProfilingSectionMarkerCurrentFunction();
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

	if (isUsingStaticAssignments() && *oIsStatic) {
		setAssignmentStatic(tAssignment, ret);
	}

	return ret;
}

static void setupArrayAssignments();

void setupDreamAssignmentEvaluator() {
	gVariableHandler.mType = MUGEN_ASSIGNMENT_EVALUATOR_TYPE_REGULAR;
	initEvaluationStack();
	setupVariableAssignments();
	setupArrayAssignments();
	setupComparisons();
}

static AssignmentReturnValue* evaluateAssignmentStart(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* oIsStatic) {
	setProfilingSectionMarkerCurrentFunction();
	gAssignmentEvaluator.mFreePointer = 0;

	auto ret = evaluateAssignmentInternal(tAssignment, tPlayer, oIsStatic);

	if(!gAssignmentEvaluator.mEmergencyStack.empty()) gAssignmentEvaluator.mEmergencyStack.clear();
	return ret;
}

static AssignmentReturnValue* stateNoStoryFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getDolmexicaStoryStateNumber((StoryInstance*)tPlayer)); }
static AssignmentReturnValue* timeStoryFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getDolmexicaStoryTimeInState((StoryInstance*)tPlayer)); }

static void setupStoryVariableAssignments() {
	gVariableHandler.mVariables.clear();
	gVariableHandler.mVariables["e"] = eFunction;
	gVariableHandler.mVariables["pi"] = piFunction;
	gVariableHandler.mVariables["platform"] = platformFunction;
	gVariableHandler.mVariables["random"] = randomFunction;
	gVariableHandler.mVariables["stateno"] = stateNoStoryFunction;
	gVariableHandler.mVariables["time"] = timeStoryFunction;
}

//static AssignmentReturnValue* movementDownFrictionThresholdFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerLyingDownFrictionThreshold(tPlayer)); }

static AssignmentReturnValue* evaluateAnimStoryArrayAssignment(AssignmentReturnValue* tIndex, StoryInstance* tInstance, int* tIsStatic) {
	int id = convertAssignmentReturnToNumber(tIndex);
	*tIsStatic = 0;
	return makeNumberAssignmentReturn(getDolmexicaStoryAnimationAnimation(tInstance, id));
}

static AssignmentReturnValue* evaluateAnimLoopStoryArrayAssignment(AssignmentReturnValue* tIndex, StoryInstance* tInstance, int* tIsStatic) {
	int id = convertAssignmentReturnToNumber(tIndex);
	*tIsStatic = 0;
	return makeBooleanAssignmentReturn(getDolmexicaStoryAnimationIsLooping(tInstance, id));
}

static AssignmentReturnValue* evaluateAnimStageStoryArrayAssignment(AssignmentReturnValue* tIndex, StoryInstance* tInstance, int* tIsStatic) {
	int id = convertAssignmentReturnToNumber(tIndex);
	*tIsStatic = 0;
	return makeBooleanAssignmentReturn(getDolmexicaStoryAnimationIsBoundToStage(tInstance, id));
}

static AssignmentReturnValue* evaluateAnimShadowStoryArrayAssignment(AssignmentReturnValue* tIndex, StoryInstance* tInstance, int* tIsStatic) {
	int id = convertAssignmentReturnToNumber(tIndex);
	*tIsStatic = 0;
	return makeBooleanAssignmentReturn(getDolmexicaStoryAnimationHasShadow(tInstance, id));
}

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

static AssignmentReturnValue* evaluateAnimPosYStoryArrayAssignment(AssignmentReturnValue* tIndex, StoryInstance* tInstance, int* tIsStatic) {
	int id = convertAssignmentReturnToNumber(tIndex);
	*tIsStatic = 0;
	return makeFloatAssignmentReturn(getDolmexicaStoryAnimationPositionY(tInstance, id));
}

static AssignmentReturnValue* evaluateCharPosXStoryArrayAssignment(AssignmentReturnValue* tIndex, StoryInstance* tInstance, int* tIsStatic) {
	int id = convertAssignmentReturnToNumber(tIndex);
	*tIsStatic = 0;
	return makeFloatAssignmentReturn(getDolmexicaStoryCharacterPositionX(tInstance, id));
}

static AssignmentReturnValue* evaluateCharPosYStoryArrayAssignment(AssignmentReturnValue* tIndex, StoryInstance* tInstance, int* tIsStatic) {
	int id = convertAssignmentReturnToNumber(tIndex);
	*tIsStatic = 0;
	return makeFloatAssignmentReturn(getDolmexicaStoryCharacterPositionY(tInstance, id));
}

static AssignmentReturnValue* evaluateTextPosXStoryArrayAssignment(AssignmentReturnValue* tIndex, StoryInstance* tInstance, int* tIsStatic) {
	int id = convertAssignmentReturnToNumber(tIndex);
	*tIsStatic = 0;
	return makeFloatAssignmentReturn(getDolmexicaStoryTextBasePositionX(tInstance, id));
}

static AssignmentReturnValue* evaluateTextPosYStoryArrayAssignment(AssignmentReturnValue* tIndex, StoryInstance* tInstance, int* tIsStatic) {
	int id = convertAssignmentReturnToNumber(tIndex);
	*tIsStatic = 0;
	return makeFloatAssignmentReturn(getDolmexicaStoryTextBasePositionY(tInstance, id));
}

static AssignmentReturnValue* evaluateTextVisibleStoryArrayAssignment(AssignmentReturnValue* tIndex, StoryInstance* tInstance, int* tIsStatic) {
	int id = convertAssignmentReturnToNumber(tIndex);
	*tIsStatic = 0;
	return makeBooleanAssignmentReturn(isDolmexicaStoryTextVisible(tInstance, id));
}

static AssignmentReturnValue* evaluateTextBuiltUpStoryArrayAssignment(AssignmentReturnValue* tIndex, StoryInstance* tInstance, int* tIsStatic) {
	int id = convertAssignmentReturnToNumber(tIndex);
	*tIsStatic = 0;
	return makeBooleanAssignmentReturn(isDolmexicaStoryTextBuiltUp(tInstance, id));
}

static AssignmentReturnValue* evaluateTextStoryArrayAssignment(AssignmentReturnValue* tIndex, StoryInstance* tInstance, int* tIsStatic) {
	int id = convertAssignmentReturnToNumber(tIndex);
	*tIsStatic = 0;
	return makeStringAssignmentReturn(getDolmexicaStoryTextText(tInstance, id));
}

static AssignmentReturnValue* evaluateDisplayedTextStoryArrayAssignment(AssignmentReturnValue* tIndex, StoryInstance* tInstance, int* tIsStatic) {
	int id = convertAssignmentReturnToNumber(tIndex);
	*tIsStatic = 0;
	return makeStringAssignmentReturn(getDolmexicaStoryTextDisplayedText(tInstance, id));
}

static AssignmentReturnValue* evaluateNameTextStoryArrayAssignment(AssignmentReturnValue* tIndex, StoryInstance* tInstance, int* tIsStatic) {
	int id = convertAssignmentReturnToNumber(tIndex);
	*tIsStatic = 0;
	return makeStringAssignmentReturn(getDolmexicaStoryTextNameText(tInstance, id));
}

static AssignmentReturnValue* evaluateTextNextStateStoryArrayAssignment(AssignmentReturnValue* tIndex, StoryInstance* tInstance, int* tIsStatic) {
	int id = convertAssignmentReturnToNumber(tIndex);
	*tIsStatic = 0;
	return makeNumberAssignmentReturn(getDolmexicaStoryTextNextState(tInstance, id));
}

static AssignmentReturnValue* evaluateCharAnimStoryArrayAssignment(AssignmentReturnValue* tIndex, StoryInstance* tInstance, int* tIsStatic) {
	int id = convertAssignmentReturnToNumber(tIndex);
	*tIsStatic = 0;
	return makeNumberAssignmentReturn(getDolmexicaStoryCharacterAnimation(tInstance, id));
}

static AssignmentReturnValue* evaluateCharStageStoryArrayAssignment(AssignmentReturnValue* tIndex, StoryInstance* tInstance, int* tIsStatic) {
	int id = convertAssignmentReturnToNumber(tIndex);
	*tIsStatic = 0;
	return makeNumberAssignmentReturn(getDolmexicaStoryCharacterIsBoundToStage(tInstance, id));
}

static AssignmentReturnValue* evaluateCharShadowStoryArrayAssignment(AssignmentReturnValue* tIndex, StoryInstance* tInstance, int* tIsStatic) {
	int id = convertAssignmentReturnToNumber(tIndex);
	*tIsStatic = 0;
	return makeNumberAssignmentReturn(getDolmexicaStoryCharacterHasShadow(tInstance, id));
}

static AssignmentReturnValue* evaluateCharAnimTimeStoryArrayAssignment(AssignmentReturnValue* tIndex, StoryInstance* tInstance, int* tIsStatic) {
	string val;
	convertAssignmentReturnToString(val, tIndex);
	int id = getDolmexicaStoryIDFromString(val.data(), tInstance);
	*tIsStatic = 0;
	return makeNumberAssignmentReturn(getDolmexicaStoryCharacterTimeLeft(tInstance, id));
}

static AssignmentReturnValue* evaluateSVarStoryArrayAssignment(AssignmentReturnValue* tIndex, StoryInstance* tInstance, int* tIsStatic) {
	int id = convertAssignmentReturnToNumber(tIndex);
	*tIsStatic = 0;
	return makeStringAssignmentReturn(getDolmexicaStoryStringVariable(tInstance, id).data());
}

static AssignmentReturnValue* evaluateFVarStoryArrayAssignment(AssignmentReturnValue* tIndex, StoryInstance* tInstance, int* tIsStatic) {
	int id = convertAssignmentReturnToNumber(tIndex);
	*tIsStatic = 0;
	return makeFloatAssignmentReturn(getDolmexicaStoryFloatVariable(tInstance, id));
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

static AssignmentReturnValue* evaluateRootSVarStoryArrayAssignment(AssignmentReturnValue* tIndex, int* tIsStatic) {
	int id = convertAssignmentReturnToNumber(tIndex);
	*tIsStatic = 0;
	return makeStringAssignmentReturn(getDolmexicaStoryStringVariable(getDolmexicaStoryRootInstance(), id).data());
}

static AssignmentReturnValue* evaluateRootFVarStoryArrayAssignment(AssignmentReturnValue* tIndex, int* tIsStatic) {
	int id = convertAssignmentReturnToNumber(tIndex);
	*tIsStatic = 0;
	return makeFloatAssignmentReturn(getDolmexicaStoryFloatVariable(getDolmexicaStoryRootInstance(), id));
}

static AssignmentReturnValue* evaluateRootVarStoryArrayAssignment(AssignmentReturnValue* tIndex, int* tIsStatic) {
	int id = convertAssignmentReturnToNumber(tIndex);
	*tIsStatic = 0;
	return makeNumberAssignmentReturn(getDolmexicaStoryIntegerVariable(getDolmexicaStoryRootInstance(), id));
}

static AssignmentReturnValue* evaluateParentSVarStoryArrayAssignment(AssignmentReturnValue* tIndex, StoryInstance* tInstance, int* tIsStatic) {
	int id = convertAssignmentReturnToNumber(tIndex);
	*tIsStatic = 0;
	return makeStringAssignmentReturn(getDolmexicaStoryStringVariable(getDolmexicaStoryInstanceParent(tInstance), id).data());
}

static AssignmentReturnValue* evaluateParentFVarStoryArrayAssignment(AssignmentReturnValue* tIndex, StoryInstance* tInstance, int* tIsStatic) {
	int id = convertAssignmentReturnToNumber(tIndex);
	*tIsStatic = 0;
	return makeFloatAssignmentReturn(getDolmexicaStoryFloatVariable(getDolmexicaStoryInstanceParent(tInstance), id));
}

static AssignmentReturnValue* evaluateParentVarStoryArrayAssignment(AssignmentReturnValue* tIndex, StoryInstance* tInstance, int* tIsStatic) {
	int id = convertAssignmentReturnToNumber(tIndex);
	*tIsStatic = 0;
	return makeNumberAssignmentReturn(getDolmexicaStoryIntegerVariable(getDolmexicaStoryInstanceParent(tInstance), id));
}

static AssignmentReturnValue* evaluateNumHelperStoryArrayAssignment(AssignmentReturnValue* tIndex, int* tIsStatic) {
	int id = convertAssignmentReturnToNumber(tIndex);
	*tIsStatic = 0;
	return makeNumberAssignmentReturn(getDolmexicaStoryGetHelperAmount(id));
}

static AssignmentReturnValue* evaluateNameIDStoryArrayAssignment(AssignmentReturnValue* tIndex, StoryInstance* tInstance, int* tIsStatic) {
	string text;
	convertAssignmentReturnToString(text, tIndex);
	int id = getDolmexicaStoryIDFromString(text.data(), tInstance);
	*tIsStatic = 0;
	return makeNumberAssignmentReturn(id);
}

static AssignmentReturnValue* evaluateHelperStoryArrayAssignment() {
	return makeBottomAssignmentReturn();
}

static AssignmentReturnValue* evaluateHelperStateNoStoryArrayAssignment(AssignmentReturnValue* tIndex, int* tIsStatic) {
	const auto id = convertAssignmentReturnToNumber(tIndex);
	const auto helper = getDolmexicaStoryHelperInstance(id);

	*tIsStatic = 0;
	return makeNumberAssignmentReturn(getDolmexicaStoryStateNumber(helper));
}

static AssignmentReturnValue* animStoryFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateAnimStoryArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), (StoryInstance*)tPlayer, tIsStatic); }
static AssignmentReturnValue* animLoopStoryFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateAnimLoopStoryArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), (StoryInstance*)tPlayer, tIsStatic); }
static AssignmentReturnValue* animStageStoryFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateAnimStageStoryArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), (StoryInstance*)tPlayer, tIsStatic); }
static AssignmentReturnValue* animShadowStoryFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateAnimShadowStoryArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), (StoryInstance*)tPlayer, tIsStatic); }
static AssignmentReturnValue* animTimeStoryFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateAnimTimeStoryArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), (StoryInstance*)tPlayer, tIsStatic); }
static AssignmentReturnValue* parentAnimTimeStoryFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateAnimTimeStoryArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), getDolmexicaStoryInstanceParent((StoryInstance*)tPlayer), tIsStatic); }
static AssignmentReturnValue* rootAnimTimeStoryFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateAnimTimeStoryArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), getDolmexicaStoryRootInstance(), tIsStatic); }
static AssignmentReturnValue* animPosXStoryFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateAnimPosXStoryArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), (StoryInstance*)tPlayer, tIsStatic); }
static AssignmentReturnValue* animPosYStoryFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateAnimPosYStoryArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), (StoryInstance*)tPlayer, tIsStatic); }
static AssignmentReturnValue* parentAnimPosXStoryFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateAnimPosXStoryArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), getDolmexicaStoryInstanceParent((StoryInstance*)tPlayer), tIsStatic); }
static AssignmentReturnValue* parentAnimPosYStoryFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateAnimPosYStoryArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), getDolmexicaStoryInstanceParent((StoryInstance*)tPlayer), tIsStatic); }
static AssignmentReturnValue* rootAnimPosXStoryFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateAnimPosXStoryArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), getDolmexicaStoryRootInstance(), tIsStatic); }
static AssignmentReturnValue* rootAnimPosYStoryFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateAnimPosYStoryArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), getDolmexicaStoryRootInstance(), tIsStatic); }
static AssignmentReturnValue* charPosXStoryFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateCharPosXStoryArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), (StoryInstance*)tPlayer, tIsStatic); }
static AssignmentReturnValue* charPosYStoryFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateCharPosYStoryArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), (StoryInstance*)tPlayer, tIsStatic); }
static AssignmentReturnValue* parentCharPosXStoryFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateCharPosXStoryArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), getDolmexicaStoryInstanceParent((StoryInstance*)tPlayer), tIsStatic); }
static AssignmentReturnValue* parentCharPosYStoryFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateCharPosYStoryArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), getDolmexicaStoryInstanceParent((StoryInstance*)tPlayer), tIsStatic); }
static AssignmentReturnValue* rootCharPosXStoryFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateCharPosXStoryArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), getDolmexicaStoryRootInstance(), tIsStatic); }
static AssignmentReturnValue* rootCharPosYStoryFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateCharPosYStoryArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), getDolmexicaStoryRootInstance(), tIsStatic); }
static AssignmentReturnValue* textPosXStoryFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateTextPosXStoryArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), (StoryInstance*)tPlayer, tIsStatic); }
static AssignmentReturnValue* textPosYStoryFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateTextPosYStoryArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), (StoryInstance*)tPlayer, tIsStatic); }
static AssignmentReturnValue* parentTextPosXStoryFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateTextPosXStoryArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), getDolmexicaStoryInstanceParent((StoryInstance*)tPlayer), tIsStatic); }
static AssignmentReturnValue* parentTextPosYStoryFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateTextPosYStoryArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), getDolmexicaStoryInstanceParent((StoryInstance*)tPlayer), tIsStatic); }
static AssignmentReturnValue* rootTextPosXStoryFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateTextPosXStoryArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), getDolmexicaStoryRootInstance(), tIsStatic); }
static AssignmentReturnValue* rootTextPosYStoryFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateTextPosYStoryArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), getDolmexicaStoryRootInstance(), tIsStatic); }
static AssignmentReturnValue* textVisibleStoryFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateTextVisibleStoryArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), (StoryInstance*)tPlayer, tIsStatic); }
static AssignmentReturnValue* parentTextVisibleStoryFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateTextVisibleStoryArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), getDolmexicaStoryInstanceParent((StoryInstance*)tPlayer), tIsStatic); }
static AssignmentReturnValue* rootTextVisibleStoryFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateTextVisibleStoryArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), getDolmexicaStoryRootInstance(), tIsStatic); }
static AssignmentReturnValue* textBuiltUpStoryFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateTextBuiltUpStoryArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), (StoryInstance*)tPlayer, tIsStatic); }
static AssignmentReturnValue* textStoryFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateTextStoryArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), (StoryInstance*)tPlayer, tIsStatic); }
static AssignmentReturnValue* displayedTextStoryFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateDisplayedTextStoryArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), (StoryInstance*)tPlayer, tIsStatic); }
static AssignmentReturnValue* nameTextStoryFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateNameTextStoryArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), (StoryInstance*)tPlayer, tIsStatic); }
static AssignmentReturnValue* textNextStateStoryFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateTextNextStateStoryArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), (StoryInstance*)tPlayer, tIsStatic); }
static AssignmentReturnValue* charAnimStoryFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateCharAnimStoryArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), (StoryInstance*)tPlayer, tIsStatic); }
static AssignmentReturnValue* parentCharAnimStoryFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateCharAnimStoryArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), getDolmexicaStoryInstanceParent((StoryInstance*)tPlayer), tIsStatic); }
static AssignmentReturnValue* rootCharAnimStoryFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateCharAnimStoryArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), getDolmexicaStoryRootInstance(), tIsStatic); }
static AssignmentReturnValue* charStageStoryFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateCharStageStoryArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), (StoryInstance*)tPlayer, tIsStatic); }
static AssignmentReturnValue* charShadowStoryFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateCharShadowStoryArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), (StoryInstance*)tPlayer, tIsStatic); }
static AssignmentReturnValue* charAnimTimeStoryFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateCharAnimTimeStoryArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), (StoryInstance*)tPlayer, tIsStatic); }
static AssignmentReturnValue* parentCharAnimTimeStoryFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateCharAnimTimeStoryArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), getDolmexicaStoryInstanceParent((StoryInstance*)tPlayer), tIsStatic); }
static AssignmentReturnValue* rootCharAnimTimeStoryFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateCharAnimTimeStoryArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), getDolmexicaStoryRootInstance(), tIsStatic); }
static AssignmentReturnValue* sVarStoryFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateSVarStoryArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), (StoryInstance*)tPlayer, tIsStatic); }
static AssignmentReturnValue* fVarStoryFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateFVarStoryArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), (StoryInstance*)tPlayer, tIsStatic); }
static AssignmentReturnValue* varStoryFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateVarStoryArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), (StoryInstance*)tPlayer, tIsStatic); }
static AssignmentReturnValue* globalVarStoryFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateGlobalVarStoryArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), tIsStatic); }
static AssignmentReturnValue* globalFVarStoryFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateGlobalFVarStoryArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), tIsStatic); }
static AssignmentReturnValue* globalSVarStoryFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateGlobalSVarStoryArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), tIsStatic); }
static AssignmentReturnValue* rootSVarStoryFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateRootSVarStoryArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), tIsStatic); }
static AssignmentReturnValue* rootFVarStoryFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateRootFVarStoryArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), tIsStatic); }
static AssignmentReturnValue* rootVarStoryFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateRootVarStoryArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), tIsStatic); }
static AssignmentReturnValue* parentSVarStoryFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateParentSVarStoryArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), (StoryInstance*)tPlayer, tIsStatic); }
static AssignmentReturnValue* parentFVarStoryFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateParentFVarStoryArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), (StoryInstance*)tPlayer, tIsStatic); }
static AssignmentReturnValue* parentVarStoryFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateParentVarStoryArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), (StoryInstance*)tPlayer, tIsStatic); }
static AssignmentReturnValue* numHelperStoryFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateNumHelperStoryArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), tIsStatic); }
static AssignmentReturnValue* nameIDStoryFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateNameIDStoryArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), (StoryInstance*)tPlayer, tIsStatic); }
static AssignmentReturnValue* helperStoryFunction(DreamMugenAssignment** /*tIndexAssignment*/, DreamPlayer* /*tPlayer*/, int* /*tIsStatic*/) { return evaluateHelperStoryArrayAssignment(); }
static AssignmentReturnValue* helperStateNoStoryFunction(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateHelperStateNoStoryArrayAssignment(evaluateAssignmentDependency(tIndexAssignment, tPlayer, tIsStatic), tIsStatic); }

static void setupStoryArrayAssignments() {
	gVariableHandler.mArrays.clear();

	gVariableHandler.mArrays["anim"] = animStoryFunction;
	gVariableHandler.mArrays["loop"] = animLoopStoryFunction;
	gVariableHandler.mArrays["stage"] = animStageStoryFunction;
	gVariableHandler.mArrays["shadow"] = animShadowStoryFunction;
	gVariableHandler.mArrays["animtime"] = animTimeStoryFunction;
	gVariableHandler.mArrays["parentanimtime"] = parentAnimTimeStoryFunction;
	gVariableHandler.mArrays["rootanimtime"] = rootAnimTimeStoryFunction;
	gVariableHandler.mArrays["x"] = animPosXStoryFunction;
	gVariableHandler.mArrays["y"] = animPosYStoryFunction;
	gVariableHandler.mArrays["parentx"] = parentAnimPosXStoryFunction;
	gVariableHandler.mArrays["parenty"] = parentAnimPosYStoryFunction;
	gVariableHandler.mArrays["rootx"] = rootAnimPosXStoryFunction;
	gVariableHandler.mArrays["rooty"] = rootAnimPosYStoryFunction;
	gVariableHandler.mArrays["charx"] = charPosXStoryFunction;
	gVariableHandler.mArrays["chary"] = charPosYStoryFunction;
	gVariableHandler.mArrays["parentcharx"] = parentCharPosXStoryFunction;
	gVariableHandler.mArrays["parentchary"] = parentCharPosYStoryFunction;
	gVariableHandler.mArrays["rootcharx"] = rootCharPosXStoryFunction;
	gVariableHandler.mArrays["rootchary"] = rootCharPosYStoryFunction;
	gVariableHandler.mArrays["textx"] = textPosXStoryFunction;
	gVariableHandler.mArrays["texty"] = textPosYStoryFunction;
	gVariableHandler.mArrays["parenttextx"] = parentTextPosXStoryFunction;
	gVariableHandler.mArrays["parenttexty"] = parentTextPosYStoryFunction;
	gVariableHandler.mArrays["roottextx"] = rootTextPosXStoryFunction;
	gVariableHandler.mArrays["roottexty"] = rootTextPosYStoryFunction;
	gVariableHandler.mArrays["textvisible"] = textVisibleStoryFunction;
	gVariableHandler.mArrays["parenttextvisible"] = parentTextVisibleStoryFunction;
	gVariableHandler.mArrays["roottextvisible"] = rootTextVisibleStoryFunction;
	gVariableHandler.mArrays["textbuiltup"] = textBuiltUpStoryFunction;
	gVariableHandler.mArrays["text"] = textStoryFunction;
	gVariableHandler.mArrays["displayedtext"] = displayedTextStoryFunction;
	gVariableHandler.mArrays["nametext"] = nameTextStoryFunction;
	gVariableHandler.mArrays["textnextstate"] = textNextStateStoryFunction;
	gVariableHandler.mArrays["ifelse"] = ifElseFunction;
	gVariableHandler.mArrays["charanim"] = charAnimStoryFunction;
	gVariableHandler.mArrays["parentcharanim"] = parentCharAnimStoryFunction;
	gVariableHandler.mArrays["rootcharanim"] = rootCharAnimStoryFunction;
	gVariableHandler.mArrays["charstage"] = charStageStoryFunction;
	gVariableHandler.mArrays["charshadow"] = charShadowStoryFunction;
	gVariableHandler.mArrays["charanimtime"] = charAnimTimeStoryFunction;
	gVariableHandler.mArrays["parentcharanimtime"] = parentCharAnimTimeStoryFunction;
	gVariableHandler.mArrays["rootcharanimtime"] = rootCharAnimTimeStoryFunction;
	gVariableHandler.mArrays["svar"] = sVarStoryFunction;
	gVariableHandler.mArrays["fvar"] = fVarStoryFunction;
	gVariableHandler.mArrays["var"] = varStoryFunction;
	gVariableHandler.mArrays["globalvar"] = globalVarStoryFunction;
	gVariableHandler.mArrays["globalfvar"] = globalFVarStoryFunction;
	gVariableHandler.mArrays["globalsvar"] = globalSVarStoryFunction;
	gVariableHandler.mArrays["rootsvar"] = rootSVarStoryFunction;
	gVariableHandler.mArrays["rootfvar"] = rootFVarStoryFunction;
	gVariableHandler.mArrays["rootvar"] = rootVarStoryFunction;
	gVariableHandler.mArrays["parentsvar"] = parentSVarStoryFunction;
	gVariableHandler.mArrays["parentfvar"] = parentFVarStoryFunction;
	gVariableHandler.mArrays["parentvar"] = parentVarStoryFunction;
	gVariableHandler.mArrays["numhelper"] = numHelperStoryFunction;
	gVariableHandler.mArrays["nameid"] = nameIDStoryFunction;
	gVariableHandler.mArrays["helper"] = helperStoryFunction;
	gVariableHandler.mArrays["helperstateno"] = helperStateNoStoryFunction;
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
}

static AssignmentReturnValue* evaluateStoryCommandAssignment(AssignmentReturnValue* tCommand, int* tIsStatic) {
	string cstring;
	convertAssignmentReturnToString(cstring, tCommand);
	int ret = isStoryCommandActive(cstring.data());

	*tIsStatic = 0;
	return makeBooleanAssignmentReturn(ret);
}


static AssignmentReturnValue* commandComparisonStoryFunction(AssignmentReturnValue* b, DreamPlayer* /*tPlayer*/, int* tIsStatic) { return evaluateStoryCommandAssignment(b, tIsStatic); }

static void setupStoryComparisons() {
	gVariableHandler.mComparisons.clear();

	gVariableHandler.mComparisons["storycommand"] = commandComparisonStoryFunction;

	gVariableHandler.mOrdinals.clear();
}

void setupDreamStoryAssignmentEvaluator()
{
	gVariableHandler.mType = MUGEN_ASSIGNMENT_EVALUATOR_TYPE_STORY;
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
	if (!(*tAssignment)) return 0;

	int isStatic;
	AssignmentReturnValue* ret = evaluateAssignmentStart(tAssignment, tPlayer, &isStatic);
	return convertAssignmentReturnToBool(ret);
}

double evaluateDreamAssignmentAndReturnAsFloat(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer)
{
	if (!(*tAssignment)) return 0;

	int isStatic;
	AssignmentReturnValue* ret = evaluateAssignmentStart(tAssignment, tPlayer, &isStatic);
	return convertAssignmentReturnToFloat(ret);
}

int evaluateDreamAssignmentAndReturnAsInteger(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer)
{
	if (!(*tAssignment)) return 0;

	int isStatic;
	AssignmentReturnValue* ret = evaluateAssignmentStart(tAssignment, tPlayer, &isStatic);
	return convertAssignmentReturnToNumber(ret);
}

void evaluateDreamAssignmentAndReturnAsString(string& oString, DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer)
{
	setProfilingSectionMarkerCurrentFunction();
	if (!(*tAssignment)) {
		oString = "";
		return;
	}

	int isStatic;
	AssignmentReturnValue* ret = evaluateAssignmentStart(tAssignment, tPlayer, &isStatic);
	convertAssignmentReturnToString(oString, ret);
}

Vector2D evaluateDreamAssignmentAndReturnAsVector2D(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer)
{
	if (!(*tAssignment)) return Vector2D(0, 0);

	int isStatic;
	AssignmentReturnValue* ret = evaluateAssignmentStart(tAssignment, tPlayer, &isStatic);
	string test;
	convertAssignmentReturnToString(test, ret);

	double x, y;
	char tX[100], comma1[20], tY[100];

	int items = sscanf(test.data(), "%99s %19s %99s", tX, comma1, tY);

	if (items >= 1) x = atof(tX);
	else x = 0;
	if (items >= 3) y = atof(tY);
	else y = 0;

	return Vector2D(x, y);
}

Vector3D evaluateDreamAssignmentAndReturnAsVector3D(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer)
{
	if (!(*tAssignment)) return Vector3D(0, 0, 0);

	int isStatic;
	AssignmentReturnValue* ret = evaluateAssignmentStart(tAssignment, tPlayer, &isStatic);
	string test;
	convertAssignmentReturnToString(test, ret);

	double x, y, z;
	char tX[100], comma1[20], tY[100], comma2[20], tZ[100];

	int items = sscanf(test.data(), "%99s %19s %99s %19s %99s", tX, comma1, tY, comma2, tZ);

	if (items >= 1) x = atof(tX);
	else x = 0;
	if (items >= 3) y = atof(tY);
	else y = 0;
	if (items >= 5) z = atof(tZ);
	else z = 0;

	return Vector3D(x, y, z);
}

Vector2DI evaluateDreamAssignmentAndReturnAsVector2DI(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer)
{
	if (!(*tAssignment)) return Vector2DI(0, 0);

	int isStatic;
	AssignmentReturnValue* ret = evaluateAssignmentStart(tAssignment, tPlayer, &isStatic);
	string test;
	convertAssignmentReturnToString(test, ret);

	int x, y;
	char tX[100], comma1[20], tY[100];

	int items = sscanf(test.data(), "%99s %19s %99s", tX, comma1, tY);

	if (items >= 1) x = atoi(tX);
	else x = 0;
	if (items >= 3) y = atoi(tY);
	else y = 0;

	return Vector2DI(x, y);
}

Vector3DI evaluateDreamAssignmentAndReturnAsVector3DI(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer)
{
	if (!(*tAssignment)) return Vector3DI(0, 0, 0);

	int isStatic;
	AssignmentReturnValue* ret = evaluateAssignmentStart(tAssignment, tPlayer, &isStatic);
	string test;
	convertAssignmentReturnToString(test, ret);

	int x, y, z;
	char tX[100], comma1[20], tY[100], comma2[20], tZ[100];

	int items = sscanf(test.data(), "%99s %19s %99s %19s %99s", tX, comma1, tY, comma2, tZ);

	if (items >= 1) x = atoi(tX);
	else x = 0;
	if (items >= 3) y = atoi(tY);
	else y = 0;
	if (items >= 5) z = atoi(tZ);
	else z = 0;

	return Vector3DI(x, y, z);
}

static void evaluateDreamAssignmentReturnAndReturnAsOneFloatWithDefaultValue(AssignmentReturnValue* tRet, double* v1, double tDefault1) {
	switch (tRet->mType) {
	case MUGEN_ASSIGNMENT_RETURN_TYPE_STRING:
	case MUGEN_ASSIGNMENT_RETURN_TYPE_NUMBER:
	case MUGEN_ASSIGNMENT_RETURN_TYPE_FLOAT:
	case MUGEN_ASSIGNMENT_RETURN_TYPE_BOOLEAN:
		*v1 = convertAssignmentReturnToFloat(tRet);
		break;
	case MUGEN_ASSIGNMENT_RETURN_TYPE_VECTOR:
	case MUGEN_ASSIGNMENT_RETURN_TYPE_RANGE:
		evaluateDreamAssignmentReturnAndReturnAsOneFloatWithDefaultValue(getVectorAssignmentReturnFirstDependency(tRet), v1, tDefault1);
		break;
	default:
		*v1 = tDefault1;
		break;
	}
}

void evaluateDreamAssignmentAndReturnAsOneFloatWithDefaultValue(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, double* v1, double tDefault1)
{
	if (!(*tAssignment)) {
		*v1 = tDefault1;
		return;
	}
	int isStatic;
	const auto ret = evaluateAssignmentStart(tAssignment, tPlayer, &isStatic);
	evaluateDreamAssignmentReturnAndReturnAsOneFloatWithDefaultValue(ret, v1, tDefault1);
}

static size_t evaluateTwoFloatsWithDefaultFromStringAndReturnProcessedReadAmount(const char* tValue, double* v1, double* v2, double tDefault1) {
	char string1[20], comma[10], string2[20];
	const auto items = sscanf(tValue, "%19s %9s %19s", string1, comma, string2);

	if (items < 1 || !strcmp("", string1)) *v1 = tDefault1;
	else *v1 = atof(string1);
	if (items < 3 || !strcmp("", string2)) {
		return 1;
	}
	else {
		*v2 = atof(string2);
		return 2;
	}
}

static size_t evaluateDreamAssignmentReturnAndReturnAsTwoFloatsWithDefaultValuesAndProcessedReadAmount(AssignmentReturnValue* tRet, double* v1, double* v2, double tDefault1, double tDefault2);

static size_t evaluateTwoFloatsWithDefaultFromVectorAndReturnProcessedReadAmount(AssignmentReturnValue* tRet, double* v1, double* v2, double tDefault1, double tDefault2) {
	if (getVectorAssignmentReturnFirstDependency(tRet)->mType == MUGEN_ASSIGNMENT_RETURN_TYPE_VECTOR || getVectorAssignmentReturnFirstDependency(tRet)->mType == MUGEN_ASSIGNMENT_RETURN_TYPE_RANGE) {
		const auto readAmountFirst = evaluateDreamAssignmentReturnAndReturnAsTwoFloatsWithDefaultValuesAndProcessedReadAmount(getVectorAssignmentReturnFirstDependency(tRet), v1, v2, tDefault1, tDefault2);
		if (readAmountFirst == 2) return 2;
		evaluateDreamAssignmentReturnAndReturnAsOneFloatWithDefaultValue(getVectorAssignmentReturnSecondDependency(tRet), v2, tDefault2);
	}
	else {
		*v1 = convertAssignmentReturnToFloat(getVectorAssignmentReturnFirstDependency(tRet));
		evaluateDreamAssignmentReturnAndReturnAsOneFloatWithDefaultValue(getVectorAssignmentReturnSecondDependency(tRet), v2, tDefault2);
	}
	return 2;
}

static size_t evaluateDreamAssignmentReturnAndReturnAsTwoFloatsWithDefaultValuesAndProcessedReadAmount(AssignmentReturnValue* tRet, double* v1, double* v2, double tDefault1, double tDefault2) {
	switch (tRet->mType) {
	case MUGEN_ASSIGNMENT_RETURN_TYPE_STRING:
		return evaluateTwoFloatsWithDefaultFromStringAndReturnProcessedReadAmount(getStringAssignmentReturnValue(tRet), v1, v2, tDefault1);
	case MUGEN_ASSIGNMENT_RETURN_TYPE_NUMBER:
		*v1 = double(getNumberAssignmentReturnValue(tRet));
		return 1;
	case MUGEN_ASSIGNMENT_RETURN_TYPE_FLOAT:
		*v1 = double(getFloatAssignmentReturnValue(tRet));
		return 1;
	case MUGEN_ASSIGNMENT_RETURN_TYPE_BOOLEAN:
		*v1 = double(getBooleanAssignmentReturnValue(tRet));
		return 1;
	case MUGEN_ASSIGNMENT_RETURN_TYPE_VECTOR:
	case MUGEN_ASSIGNMENT_RETURN_TYPE_RANGE:
		return evaluateTwoFloatsWithDefaultFromVectorAndReturnProcessedReadAmount(tRet, v1, v2, tDefault1, tDefault2);
	default:
		*v1 = tDefault1;
		return 1;
	}
}

void evaluateDreamAssignmentAndReturnAsTwoFloatsWithDefaultValues(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, double* v1, double* v2, double tDefault1, double tDefault2)
{
	if (!(*tAssignment)) {
		*v1 = tDefault1;
		*v2 = tDefault2;
		return;
	}
	int isStatic;
	const auto ret = evaluateAssignmentStart(tAssignment, tPlayer, &isStatic);
	const auto setAmount = evaluateDreamAssignmentReturnAndReturnAsTwoFloatsWithDefaultValuesAndProcessedReadAmount(ret, v1, v2, tDefault1, tDefault2);
	assert(setAmount >= 1);
	if (setAmount < 2) {
		*v2 = tDefault2;
	}
}

static size_t evaluateThreeFloatsWithDefaultFromStringAndReturnProcessedReadAmount(const char* tValue, double* v1, double* v2, double* v3, double tDefault1, double tDefault2, double tDefault3) {
	double vals[3];
	char string[3][20], comma[2][10];
	const auto items = sscanf(tValue, "%19s %9s %19s %9s %19s", string[0], comma[0], string[1], comma[1], string[2]);

	double defaults[3];
	defaults[0] = tDefault1;
	defaults[1] = tDefault2;
	defaults[2] = tDefault3;

	size_t ret = 0;
	for (int j = 0; j < 3; j++) {
		if (items < (1 + j * 2) || !strcmp("", string[j])) vals[j] = defaults[j];
		else {
			vals[j] = atof(string[j]);
			ret = j + 1;
		}
	}

	*v1 = vals[0];
	*v2 = vals[1];
	*v3 = vals[2];
	return std::max(size_t(1), ret);
}

static size_t evaluateDreamAssignmentReturnAndReturnAsThreeFloatsWithDefaultValuesAndProcessedReadAmount(AssignmentReturnValue* tRet, double* v1, double* v2, double* v3, double tDefault1, double tDefault2, double tDefault3);

static size_t evaluateThreeFloatsWithDefaultFromVectorAndReturnProcessedReadAmount(AssignmentReturnValue* tRet, double* v1, double* v2, double* v3, double tDefault1, double tDefault2, double tDefault3) {
	if (getVectorAssignmentReturnFirstDependency(tRet)->mType == MUGEN_ASSIGNMENT_RETURN_TYPE_VECTOR || getVectorAssignmentReturnFirstDependency(tRet)->mType == MUGEN_ASSIGNMENT_RETURN_TYPE_RANGE) {
		const auto readAmountFirst = evaluateDreamAssignmentReturnAndReturnAsThreeFloatsWithDefaultValuesAndProcessedReadAmount(getVectorAssignmentReturnFirstDependency(tRet), v1, v2, v3, tDefault1, tDefault2, tDefault3);
		if (readAmountFirst == 3) {
			return 3;
		}
		else if (readAmountFirst == 2) {
			evaluateDreamAssignmentReturnAndReturnAsOneFloatWithDefaultValue(getVectorAssignmentReturnSecondDependency(tRet), v3, tDefault3);
			return 3;
		}
		else {
			assert(readAmountFirst == 1);
			const auto subRet = evaluateDreamAssignmentReturnAndReturnAsTwoFloatsWithDefaultValuesAndProcessedReadAmount(getVectorAssignmentReturnSecondDependency(tRet), v2, v3, tDefault2, tDefault3);
			return subRet + readAmountFirst;
		}
	}
	else {
		*v1 = convertAssignmentReturnToFloat(getVectorAssignmentReturnFirstDependency(tRet));
		const auto subRet = evaluateDreamAssignmentReturnAndReturnAsTwoFloatsWithDefaultValuesAndProcessedReadAmount(getVectorAssignmentReturnSecondDependency(tRet), v2, v3, tDefault2, tDefault3);
		return subRet + 1;
	}
}

static size_t evaluateDreamAssignmentReturnAndReturnAsThreeFloatsWithDefaultValuesAndProcessedReadAmount(AssignmentReturnValue* tRet, double* v1, double* v2, double* v3, double tDefault1, double tDefault2, double tDefault3) {
	switch (tRet->mType) {
	case MUGEN_ASSIGNMENT_RETURN_TYPE_STRING:
		return evaluateThreeFloatsWithDefaultFromStringAndReturnProcessedReadAmount(getStringAssignmentReturnValue(tRet), v1, v2, v3, tDefault1, tDefault2, tDefault3);
	case MUGEN_ASSIGNMENT_RETURN_TYPE_NUMBER:
		*v1 = double(getNumberAssignmentReturnValue(tRet));
		return 1;
	case MUGEN_ASSIGNMENT_RETURN_TYPE_FLOAT:
		*v1 = double(getFloatAssignmentReturnValue(tRet));
		return 1;
	case MUGEN_ASSIGNMENT_RETURN_TYPE_BOOLEAN:
		*v1 = double(getBooleanAssignmentReturnValue(tRet));
		return 1;
	case MUGEN_ASSIGNMENT_RETURN_TYPE_VECTOR:
	case MUGEN_ASSIGNMENT_RETURN_TYPE_RANGE:
		return evaluateThreeFloatsWithDefaultFromVectorAndReturnProcessedReadAmount(tRet, v1, v2, v3, tDefault1, tDefault2, tDefault3);
	default:
		*v1 = tDefault1;
		return 1;
	}
}

void evaluateDreamAssignmentAndReturnAsThreeFloatsWithDefaultValues(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, double* v1, double* v2, double* v3, double tDefault1, double tDefault2, double tDefault3)
{
	if (!(*tAssignment)) {
		*v1 = tDefault1;
		*v2 = tDefault2;
		*v3 = tDefault3;
		return;
	}
	int isStatic;
	const auto ret = evaluateAssignmentStart(tAssignment, tPlayer, &isStatic);
	const auto setAmount = evaluateDreamAssignmentReturnAndReturnAsThreeFloatsWithDefaultValuesAndProcessedReadAmount(ret, v1, v2, v3, tDefault1, tDefault2, tDefault3);

	assert(setAmount >= 1);
	if (setAmount < 2) {
		*v2 = tDefault2;
	}
	if (setAmount < 3) {
		*v3 = tDefault3;
	}
}

static size_t evaluateFourFloatsWithDefaultFromStringAndReturnProcessedReadAmount(const char* tValue, double* v1, double* v2, double* v3, double* v4, double tDefault1, double tDefault2, double tDefault3, double tDefault4) {
	double vals[4];
	char string[4][20], comma[3][10];
	const auto items = sscanf(tValue, "%19s %9s %19s %9s %19s %9s %19s", string[0], comma[0], string[1], comma[1], string[2], comma[2], string[3]);

	double defaults[4];
	defaults[0] = tDefault1;
	defaults[1] = tDefault2;
	defaults[2] = tDefault3;
	defaults[3] = tDefault4;

	size_t ret = 0;
	for (int j = 0; j < 4; j++) {
		if (items < (1 + j * 2) || !strcmp("", string[j])) vals[j] = defaults[j];
		else {
			vals[j] = atof(string[j]);
			ret = j + 1;
		}
	}

	*v1 = vals[0];
	*v2 = vals[1];
	*v3 = vals[2];
	*v4 = vals[3];
	return std::max(size_t(1), ret);
}

static size_t evaluateDreamAssignmentReturnAndReturnAsFourFloatsWithDefaultValuesAndProcessedReadAmount(AssignmentReturnValue* tRet, double* v1, double* v2, double* v3, double* v4, double tDefault1, double tDefault2, double tDefault3, double tDefault4);

static size_t evaluateFourFloatsWithDefaultFromVectorAndReturnProcessedReadAmount(AssignmentReturnValue* tRet, double* v1, double* v2, double* v3, double* v4, double tDefault1, double tDefault2, double tDefault3, double tDefault4) {
	if (getVectorAssignmentReturnFirstDependency(tRet)->mType == MUGEN_ASSIGNMENT_RETURN_TYPE_VECTOR || getVectorAssignmentReturnFirstDependency(tRet)->mType == MUGEN_ASSIGNMENT_RETURN_TYPE_RANGE) {
		const auto readAmountFirst = evaluateDreamAssignmentReturnAndReturnAsFourFloatsWithDefaultValuesAndProcessedReadAmount(getVectorAssignmentReturnFirstDependency(tRet), v1, v2, v3, v4, tDefault1, tDefault2, tDefault3, tDefault4);
		if (readAmountFirst == 4) {
			return 4;
		}
		else if (readAmountFirst == 3) {
			evaluateDreamAssignmentReturnAndReturnAsOneFloatWithDefaultValue(getVectorAssignmentReturnSecondDependency(tRet), v4, tDefault4);
			return 4;
		}
		else if (readAmountFirst == 2) {
			const auto subRet = evaluateDreamAssignmentReturnAndReturnAsTwoFloatsWithDefaultValuesAndProcessedReadAmount(getVectorAssignmentReturnSecondDependency(tRet), v3, v4, tDefault3, tDefault4);
			return subRet + readAmountFirst;
		}
		else {
			assert(readAmountFirst == 1);
			const auto subRet = evaluateDreamAssignmentReturnAndReturnAsThreeFloatsWithDefaultValuesAndProcessedReadAmount(getVectorAssignmentReturnSecondDependency(tRet), v2, v3, v4, tDefault2, tDefault3, tDefault4);
			return subRet + readAmountFirst;
		}
	}
	else {
		*v1 = convertAssignmentReturnToFloat(getVectorAssignmentReturnFirstDependency(tRet));
		const auto subRet = evaluateDreamAssignmentReturnAndReturnAsThreeFloatsWithDefaultValuesAndProcessedReadAmount(getVectorAssignmentReturnSecondDependency(tRet), v2, v3, v4, tDefault2, tDefault3, tDefault4);
		return subRet + 1;
	}
}

static size_t evaluateDreamAssignmentReturnAndReturnAsFourFloatsWithDefaultValuesAndProcessedReadAmount(AssignmentReturnValue* tRet, double* v1, double* v2, double* v3, double* v4, double tDefault1, double tDefault2, double tDefault3, double tDefault4) {
	switch (tRet->mType) {
	case MUGEN_ASSIGNMENT_RETURN_TYPE_STRING:
		return evaluateFourFloatsWithDefaultFromStringAndReturnProcessedReadAmount(getStringAssignmentReturnValue(tRet), v1, v2, v3, v4, tDefault1, tDefault2, tDefault3, tDefault4);
	case MUGEN_ASSIGNMENT_RETURN_TYPE_NUMBER:
		*v1 = double(getNumberAssignmentReturnValue(tRet));
		return 1;
	case MUGEN_ASSIGNMENT_RETURN_TYPE_FLOAT:
		*v1 = double(getFloatAssignmentReturnValue(tRet));
		return 1;
	case MUGEN_ASSIGNMENT_RETURN_TYPE_BOOLEAN:
		*v1 = double(getBooleanAssignmentReturnValue(tRet));
		return 1;
	case MUGEN_ASSIGNMENT_RETURN_TYPE_VECTOR:
	case MUGEN_ASSIGNMENT_RETURN_TYPE_RANGE:
		return evaluateFourFloatsWithDefaultFromVectorAndReturnProcessedReadAmount(tRet, v1, v2, v3, v4, tDefault1, tDefault2, tDefault3, tDefault4);
	default:
		*v1 = tDefault1;
		return 1;
	}
}

void evaluateDreamAssignmentAndReturnAsFourFloatsWithDefaultValues(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, double* v1, double* v2, double* v3, double* v4, double tDefault1, double tDefault2, double tDefault3, double tDefault4)
{
	if (!(*tAssignment)) {
		*v1 = tDefault1;
		*v2 = tDefault2;
		*v3 = tDefault3;
		*v4 = tDefault4;
		return;
	}
	int isStatic;
	const auto ret = evaluateAssignmentStart(tAssignment, tPlayer, &isStatic);
	const auto setAmount = evaluateDreamAssignmentReturnAndReturnAsFourFloatsWithDefaultValuesAndProcessedReadAmount(ret, v1, v2, v3, v4, tDefault1, tDefault2, tDefault3, tDefault4);

	assert(setAmount >= 1);
	if (setAmount < 2) {
		*v2 = tDefault2;
	}
	if (setAmount < 3) {
		*v3 = tDefault3;
	}
	if (setAmount < 4) {
		*v4 = tDefault4;
	}
}

static void evaluateDreamAssignmentReturnAndReturnAsOneIntegerWithDefaultValue(AssignmentReturnValue* tRet, int* v1, int tDefault1) {
	switch (tRet->mType) {
	case MUGEN_ASSIGNMENT_RETURN_TYPE_STRING:
	case MUGEN_ASSIGNMENT_RETURN_TYPE_NUMBER:
	case MUGEN_ASSIGNMENT_RETURN_TYPE_FLOAT:
	case MUGEN_ASSIGNMENT_RETURN_TYPE_BOOLEAN:
		*v1 = convertAssignmentReturnToNumber(tRet);
		break;
	case MUGEN_ASSIGNMENT_RETURN_TYPE_VECTOR:
	case MUGEN_ASSIGNMENT_RETURN_TYPE_RANGE:
		evaluateDreamAssignmentReturnAndReturnAsOneIntegerWithDefaultValue(getVectorAssignmentReturnFirstDependency(tRet), v1, tDefault1);
		break;
	default:
		*v1 = tDefault1;
		break;
	}
}

void evaluateDreamAssignmentAndReturnAsOneIntegerWithDefaultValue(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* v1, int tDefault1)
{
	if (!(*tAssignment)) {
		*v1 = tDefault1;
		return;
	}
	int isStatic;
	const auto ret = evaluateAssignmentStart(tAssignment, tPlayer, &isStatic);
	evaluateDreamAssignmentReturnAndReturnAsOneIntegerWithDefaultValue(ret, v1, tDefault1);
}

static size_t evaluateTwoIntegersWithDefaultFromStringAndReturnProcessedReadAmount(const char* tValue, int* v1, int* v2, int tDefault1) {
	char string1[20], comma[10], string2[20];
	const auto items = sscanf(tValue, "%19s %9s %19s", string1, comma, string2);

	if (items < 1 || !strcmp("", string1)) *v1 = tDefault1;
	else *v1 = atoi(string1);
	if (items < 3 || !strcmp("", string2)) {
		return 1;
	}
	else {
		*v2 = atoi(string2);
		return 2;
	}
}

static size_t evaluateDreamAssignmentReturnAndReturnAsTwoIntegersWithDefaultValuesAndProcessedReadAmount(AssignmentReturnValue* tRet, int* v1, int* v2, int tDefault1, int tDefault2);

static size_t evaluateTwoIntegersWithDefaultFromVectorAndReturnProcessedReadAmount(AssignmentReturnValue* tRet, int* v1, int* v2, int tDefault1, int tDefault2) {
	if (getVectorAssignmentReturnFirstDependency(tRet)->mType == MUGEN_ASSIGNMENT_RETURN_TYPE_VECTOR || getVectorAssignmentReturnFirstDependency(tRet)->mType == MUGEN_ASSIGNMENT_RETURN_TYPE_RANGE) {
		const auto readAmountFirst = evaluateDreamAssignmentReturnAndReturnAsTwoIntegersWithDefaultValuesAndProcessedReadAmount(getVectorAssignmentReturnFirstDependency(tRet), v1, v2, tDefault1, tDefault2);
		if (readAmountFirst == 2) return 2;
		evaluateDreamAssignmentReturnAndReturnAsOneIntegerWithDefaultValue(getVectorAssignmentReturnSecondDependency(tRet), v2, tDefault2);
	}
	else {
		*v1 = convertAssignmentReturnToNumber(getVectorAssignmentReturnFirstDependency(tRet));
		evaluateDreamAssignmentReturnAndReturnAsOneIntegerWithDefaultValue(getVectorAssignmentReturnSecondDependency(tRet), v2, tDefault2);
	}
	return 2;
}

static size_t evaluateDreamAssignmentReturnAndReturnAsTwoIntegersWithDefaultValuesAndProcessedReadAmount(AssignmentReturnValue* tRet, int* v1, int* v2, int tDefault1, int tDefault2) {
	switch (tRet->mType) {
	case MUGEN_ASSIGNMENT_RETURN_TYPE_STRING:
		return evaluateTwoIntegersWithDefaultFromStringAndReturnProcessedReadAmount(getStringAssignmentReturnValue(tRet), v1, v2, tDefault1);
	case MUGEN_ASSIGNMENT_RETURN_TYPE_NUMBER:
		*v1 = int(getNumberAssignmentReturnValue(tRet));
		return 1;
	case MUGEN_ASSIGNMENT_RETURN_TYPE_FLOAT:
		*v1 = int(getFloatAssignmentReturnValue(tRet));
		return 1;
	case MUGEN_ASSIGNMENT_RETURN_TYPE_BOOLEAN:
		*v1 = int(getBooleanAssignmentReturnValue(tRet));
		return 1;
	case MUGEN_ASSIGNMENT_RETURN_TYPE_VECTOR:
	case MUGEN_ASSIGNMENT_RETURN_TYPE_RANGE:
		return evaluateTwoIntegersWithDefaultFromVectorAndReturnProcessedReadAmount(tRet, v1, v2, tDefault1, tDefault2);
	default:
		*v1 = tDefault1;
		return 1;
	}
}

void evaluateDreamAssignmentAndReturnAsTwoIntegersWithDefaultValues(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* v1, int* v2, int tDefault1, int tDefault2)
{
	if (!(*tAssignment)) {
		*v1 = tDefault1;
		*v2 = tDefault2;
		return;
	}
	int isStatic;
	const auto ret = evaluateAssignmentStart(tAssignment, tPlayer, &isStatic);
	const auto setAmount = evaluateDreamAssignmentReturnAndReturnAsTwoIntegersWithDefaultValuesAndProcessedReadAmount(ret, v1, v2, tDefault1, tDefault2);
	assert(setAmount >= 1);
	if (setAmount < 2) {
		*v2 = tDefault2;
	}
}

static size_t evaluateThreeIntegersWithDefaultFromStringAndReturnProcessedReadAmount(const char* tValue, int* v1, int* v2, int* v3, int tDefault1, int tDefault2, int tDefault3) {
	int vals[3];
	char string[3][20], comma[2][10];
	const auto items = sscanf(tValue, "%19s %9s %19s %9s %19s", string[0], comma[0], string[1], comma[1], string[2]);

	int defaults[3];
	defaults[0] = tDefault1;
	defaults[1] = tDefault2;
	defaults[2] = tDefault3;

	size_t ret = 0;
	for (int j = 0; j < 3; j++) {
		if (items < (1 + j * 2) || !strcmp("", string[j])) vals[j] = defaults[j];
		else {
			vals[j] = atoi(string[j]);
			ret = j + 1;
		}
	}

	*v1 = vals[0];
	*v2 = vals[1];
	*v3 = vals[2];
	return std::max(size_t(1), ret);
}

static size_t evaluateDreamAssignmentReturnAndReturnAsThreeIntegersWithDefaultValuesAndProcessedReadAmount(AssignmentReturnValue* tRet, int* v1, int* v2, int* v3, int tDefault1, int tDefault2, int tDefault3);

static size_t evaluateThreeIntegersWithDefaultFromVectorAndReturnProcessedReadAmount(AssignmentReturnValue* tRet, int* v1, int* v2, int* v3, int tDefault1, int tDefault2, int tDefault3) {
	if (getVectorAssignmentReturnFirstDependency(tRet)->mType == MUGEN_ASSIGNMENT_RETURN_TYPE_VECTOR || getVectorAssignmentReturnFirstDependency(tRet)->mType == MUGEN_ASSIGNMENT_RETURN_TYPE_RANGE) {
		const auto readAmountFirst = evaluateDreamAssignmentReturnAndReturnAsThreeIntegersWithDefaultValuesAndProcessedReadAmount(getVectorAssignmentReturnFirstDependency(tRet), v1, v2, v3, tDefault1, tDefault2, tDefault3);
		if (readAmountFirst == 3) {
			return 3;
		}
		else if (readAmountFirst == 2) {
			evaluateDreamAssignmentReturnAndReturnAsOneIntegerWithDefaultValue(getVectorAssignmentReturnSecondDependency(tRet), v3, tDefault3);
			return 3;
		}
		else {
			assert(readAmountFirst == 1);
			const auto subRet = evaluateDreamAssignmentReturnAndReturnAsTwoIntegersWithDefaultValuesAndProcessedReadAmount(getVectorAssignmentReturnSecondDependency(tRet), v2, v3, tDefault2, tDefault3);
			return subRet + readAmountFirst;
		}
	}
	else {
		*v1 = convertAssignmentReturnToNumber(getVectorAssignmentReturnFirstDependency(tRet));
		const auto subRet = evaluateDreamAssignmentReturnAndReturnAsTwoIntegersWithDefaultValuesAndProcessedReadAmount(getVectorAssignmentReturnSecondDependency(tRet), v2, v3, tDefault2, tDefault3);
		return subRet + 1;
	}
}

static size_t evaluateDreamAssignmentReturnAndReturnAsThreeIntegersWithDefaultValuesAndProcessedReadAmount(AssignmentReturnValue* tRet, int* v1, int* v2, int* v3, int tDefault1, int tDefault2, int tDefault3) {
	switch (tRet->mType) {
	case MUGEN_ASSIGNMENT_RETURN_TYPE_STRING:
		return evaluateThreeIntegersWithDefaultFromStringAndReturnProcessedReadAmount(getStringAssignmentReturnValue(tRet), v1, v2, v3, tDefault1, tDefault2, tDefault3);
	case MUGEN_ASSIGNMENT_RETURN_TYPE_NUMBER:
		*v1 = int(getNumberAssignmentReturnValue(tRet));
		return 1;
	case MUGEN_ASSIGNMENT_RETURN_TYPE_FLOAT:
		*v1 = int(getFloatAssignmentReturnValue(tRet));
		return 1;
	case MUGEN_ASSIGNMENT_RETURN_TYPE_BOOLEAN:
		*v1 = int(getBooleanAssignmentReturnValue(tRet));
		return 1;
	case MUGEN_ASSIGNMENT_RETURN_TYPE_VECTOR:
	case MUGEN_ASSIGNMENT_RETURN_TYPE_RANGE:
		return evaluateThreeIntegersWithDefaultFromVectorAndReturnProcessedReadAmount(tRet, v1, v2, v3, tDefault1, tDefault2, tDefault3);
	default:
		*v1 = tDefault1;
		return 1;
	}
}

void evaluateDreamAssignmentAndReturnAsThreeIntegersWithDefaultValues(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* v1, int* v2, int* v3, int tDefault1, int tDefault2, int tDefault3)
{
	if (!(*tAssignment)) {
		*v1 = tDefault1;
		*v2 = tDefault2;
		*v3 = tDefault3;
		return;
	}
	int isStatic;
	const auto ret = evaluateAssignmentStart(tAssignment, tPlayer, &isStatic);
	const auto setAmount = evaluateDreamAssignmentReturnAndReturnAsThreeIntegersWithDefaultValuesAndProcessedReadAmount(ret, v1, v2, v3, tDefault1, tDefault2, tDefault3);

	assert(setAmount >= 1);
	if (setAmount < 2) {
		*v2 = tDefault2;
	}
	if (setAmount < 3) {
		*v3 = tDefault3;
	}
}

static size_t evaluateFourIntegersWithDefaultFromStringAndReturnProcessedReadAmount(const char* tValue, int* v1, int* v2, int* v3, int* v4, int tDefault1, int tDefault2, int tDefault3, int tDefault4) {
	int vals[4];
	char string[4][20], comma[3][10];
	const auto items = sscanf(tValue, "%19s %9s %19s %9s %19s %9s %19s", string[0], comma[0], string[1], comma[1], string[2], comma[2], string[3]);

	int defaults[4];
	defaults[0] = tDefault1;
	defaults[1] = tDefault2;
	defaults[2] = tDefault3;
	defaults[3] = tDefault4;

	size_t ret = 0;
	for (int j = 0; j < 4; j++) {
		if (items < (1 + j * 2) || !strcmp("", string[j])) vals[j] = defaults[j];
		else {
			vals[j] = atoi(string[j]);
			ret = j + 1;
		}
	}

	*v1 = vals[0];
	*v2 = vals[1];
	*v3 = vals[2];
	*v4 = vals[3];
	return std::max(size_t(1), ret);
}

static size_t evaluateDreamAssignmentReturnAndReturnAsFourIntegersWithDefaultValuesAndProcessedReadAmount(AssignmentReturnValue* tRet, int* v1, int* v2, int* v3, int* v4, int tDefault1, int tDefault2, int tDefault3, int tDefault4);

static size_t evaluateFourIntegersWithDefaultFromVectorAndReturnProcessedReadAmount(AssignmentReturnValue* tRet, int* v1, int* v2, int* v3, int* v4, int tDefault1, int tDefault2, int tDefault3, int tDefault4) {
	if (getVectorAssignmentReturnFirstDependency(tRet)->mType == MUGEN_ASSIGNMENT_RETURN_TYPE_VECTOR || getVectorAssignmentReturnFirstDependency(tRet)->mType == MUGEN_ASSIGNMENT_RETURN_TYPE_RANGE) {
		const auto readAmountFirst = evaluateDreamAssignmentReturnAndReturnAsFourIntegersWithDefaultValuesAndProcessedReadAmount(getVectorAssignmentReturnFirstDependency(tRet), v1, v2, v3, v4, tDefault1, tDefault2, tDefault3, tDefault4);
		if (readAmountFirst == 4) {
			return 4;
		}
		else if (readAmountFirst == 3) {
			evaluateDreamAssignmentReturnAndReturnAsOneIntegerWithDefaultValue(getVectorAssignmentReturnSecondDependency(tRet), v4, tDefault4);
			return 4;
		}
		else if (readAmountFirst == 2) {
			const auto subRet = evaluateDreamAssignmentReturnAndReturnAsTwoIntegersWithDefaultValuesAndProcessedReadAmount(getVectorAssignmentReturnSecondDependency(tRet), v3, v4, tDefault3, tDefault4);
			return subRet + readAmountFirst;
		}
		else {
			assert(readAmountFirst == 1);
			const auto subRet = evaluateDreamAssignmentReturnAndReturnAsThreeIntegersWithDefaultValuesAndProcessedReadAmount(getVectorAssignmentReturnSecondDependency(tRet), v2, v3, v4, tDefault2, tDefault3, tDefault4);
			return subRet + readAmountFirst;
		}
	}
	else {
		*v1 = convertAssignmentReturnToNumber(getVectorAssignmentReturnFirstDependency(tRet));
		const auto subRet = evaluateDreamAssignmentReturnAndReturnAsThreeIntegersWithDefaultValuesAndProcessedReadAmount(getVectorAssignmentReturnSecondDependency(tRet), v2, v3, v4, tDefault2, tDefault3, tDefault4);
		return subRet + 1;
	}
}

static size_t evaluateDreamAssignmentReturnAndReturnAsFourIntegersWithDefaultValuesAndProcessedReadAmount(AssignmentReturnValue* tRet, int* v1, int* v2, int* v3, int* v4, int tDefault1, int tDefault2, int tDefault3, int tDefault4) {
	switch (tRet->mType) {
	case MUGEN_ASSIGNMENT_RETURN_TYPE_STRING:
		return evaluateFourIntegersWithDefaultFromStringAndReturnProcessedReadAmount(getStringAssignmentReturnValue(tRet), v1, v2, v3, v4, tDefault1, tDefault2, tDefault3, tDefault4);
	case MUGEN_ASSIGNMENT_RETURN_TYPE_NUMBER:
		*v1 = int(getNumberAssignmentReturnValue(tRet));
		return 1;
	case MUGEN_ASSIGNMENT_RETURN_TYPE_FLOAT:
		*v1 = int(getFloatAssignmentReturnValue(tRet));
		return 1;
	case MUGEN_ASSIGNMENT_RETURN_TYPE_BOOLEAN:
		*v1 = int(getBooleanAssignmentReturnValue(tRet));
		return 1;
	case MUGEN_ASSIGNMENT_RETURN_TYPE_VECTOR:
	case MUGEN_ASSIGNMENT_RETURN_TYPE_RANGE:
		return evaluateFourIntegersWithDefaultFromVectorAndReturnProcessedReadAmount(tRet, v1, v2, v3, v4, tDefault1, tDefault2, tDefault3, tDefault4);
	default:
		*v1 = tDefault1;
		return 1;
	}
}

void evaluateDreamAssignmentAndReturnAsFourIntegersWithDefaultValues(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* v1, int* v2, int* v3, int* v4, int tDefault1, int tDefault2, int tDefault3, int tDefault4)
{
	if (!(*tAssignment)) {
		*v1 = tDefault1;
		*v2 = tDefault2;
		*v3 = tDefault3;
		*v4 = tDefault4;
		return;
	}
	int isStatic;
	const auto ret = evaluateAssignmentStart(tAssignment, tPlayer, &isStatic);
	const auto setAmount = evaluateDreamAssignmentReturnAndReturnAsFourIntegersWithDefaultValuesAndProcessedReadAmount(ret, v1, v2, v3, v4, tDefault1, tDefault2, tDefault3, tDefault4);

	assert(setAmount >= 1);
	if (setAmount < 2) {
		*v2 = tDefault2;
	}
	if (setAmount < 3) {
		*v3 = tDefault3;
	}
	if (setAmount < 4) {
		*v4 = tDefault4;
	}
}