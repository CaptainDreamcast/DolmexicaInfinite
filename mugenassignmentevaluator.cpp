#define HAVE_M_PI 

#include "mugenassignmentevaluator.h"

#include <assert.h>

#define _USE_MATH_DEFINES
#include <math.h>

#define LOGGER_WARNINGS_DISABLED

#include <prism/log.h>
#include <prism/system.h>
#include <prism/math.h>

#include "gamelogic.h"
#include "stage.h"
#include "playerhitdata.h"
#include "mugenexplod.h"
#include "dolmexicastoryscreen.h"

typedef enum {
	ASSIGNMENT_RETURN_TYPE_STRING,
	ASSIGNMENT_RETURN_TYPE_NUMBER,
	ASSIGNMENT_RETURN_TYPE_FLOAT,
	ASSIGNMENT_RETURN_TYPE_BOOLEAN,
	ASSIGNMENT_RETURN_TYPE_BOTTOM,

} AssignmentReturnType;

typedef struct {
	AssignmentReturnType mType;
	char mData[100];
} AssignmentReturnValue;

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
	Vector3D mVector;
} AssignmentReturnVectorFloat;

typedef struct {
	AssignmentReturnType mType;
	Vector3DI mVectorI;
} AssignmentReturnVectorInteger;

typedef struct {
	AssignmentReturnType mType;
} AssignmentReturnBottom;

static AssignmentReturnValue evaluateAssignmentInternal(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* oIsStatic);

static AssignmentReturnValue evaluateAssignmentDependency(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* tIsStatic) {
	int isDependencyStatic;
	AssignmentReturnValue ret = evaluateAssignmentInternal(tAssignment, tPlayer, &isDependencyStatic);
	(*tIsStatic) &= isDependencyStatic;
	return ret;
}

static int isFloatReturn(AssignmentReturnValue tReturn);
static double convertAssignmentReturnToFloat(AssignmentReturnValue tAssignmentReturn);

static void destroyAssignmentReturn(AssignmentReturnValue tAssignmentReturn) {
	(void)tAssignmentReturn;
}

static char* getStringAssignmentReturnValue(AssignmentReturnValue* tAssignmentReturn) {
	AssignmentReturnString* string = (AssignmentReturnString*)tAssignmentReturn;
	return string->mString;
}

static int getNumberAssignmentReturnValue(AssignmentReturnValue tAssignmentReturn) {
	AssignmentReturnNumber* number = (AssignmentReturnNumber*)&tAssignmentReturn;
	return number->mNumber;
}

static double getFloatAssignmentReturnValue(AssignmentReturnValue tAssignmentReturn) {
	AssignmentReturnFloat* f = (AssignmentReturnFloat*)&tAssignmentReturn;
	return f->mFloat;
}

static int getBooleanAssignmentReturnValue(AssignmentReturnValue tAssignmentReturn) {
	AssignmentReturnBoolean* boolean = (AssignmentReturnBoolean*)&tAssignmentReturn;
	return boolean->mBoolean;
}

static AssignmentReturnValue makeBooleanAssignmentReturn(int tValue);


static char* convertAssignmentReturnToAllocatedString(AssignmentReturnValue tAssignmentReturn) {
	char* ret;
	char buffer[100]; // TODO: without buffer

	char* string;
	int valueI;
	double valueF;
	switch (tAssignmentReturn.mType) {
	case ASSIGNMENT_RETURN_TYPE_STRING:
		string = getStringAssignmentReturnValue(&tAssignmentReturn);
		ret = copyToAllocatedString(string);
		break;
	case ASSIGNMENT_RETURN_TYPE_NUMBER:
		valueI = getNumberAssignmentReturnValue(tAssignmentReturn);
		sprintf(buffer, "%d", valueI);
		ret = copyToAllocatedString(buffer);
		break;
	case ASSIGNMENT_RETURN_TYPE_FLOAT:
		valueF = getFloatAssignmentReturnValue(tAssignmentReturn);
		sprintf(buffer, "%f", valueF);
		ret = copyToAllocatedString(buffer);
		break;
		ret = copyToAllocatedString(buffer);
	case ASSIGNMENT_RETURN_TYPE_BOOLEAN:
		valueI = getBooleanAssignmentReturnValue(tAssignmentReturn);
		sprintf(buffer, "%d", valueI);
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

static int convertAssignmentReturnToBool(AssignmentReturnValue tAssignmentReturn) {
	int ret;

	char* string;
	int valueI;
	double valueF;
	switch (tAssignmentReturn.mType) {
	case ASSIGNMENT_RETURN_TYPE_STRING:
		string = getStringAssignmentReturnValue(&tAssignmentReturn);
		ret = strcmp("", string);
		ret &= strcmp("0", string);
		break;
	case ASSIGNMENT_RETURN_TYPE_NUMBER:
		valueI = getNumberAssignmentReturnValue(tAssignmentReturn);
		ret = valueI;
		break;
	case ASSIGNMENT_RETURN_TYPE_FLOAT:
		valueF = getFloatAssignmentReturnValue(tAssignmentReturn);
		ret = (int)valueF;
		break;
	case ASSIGNMENT_RETURN_TYPE_BOOLEAN:
		valueI = getBooleanAssignmentReturnValue(tAssignmentReturn);
		ret = valueI;
		break;
	default:
		ret = 0;
		break;
	}

	destroyAssignmentReturn(tAssignmentReturn);

	return ret;
}


static int convertAssignmentReturnToNumber(AssignmentReturnValue tAssignmentReturn) {
	int ret;

	char* string;
	int valueI;
	double valueF;
	switch (tAssignmentReturn.mType) {
	case ASSIGNMENT_RETURN_TYPE_STRING:
		string = getStringAssignmentReturnValue(&tAssignmentReturn);
		ret = atoi(string);
		break;
	case ASSIGNMENT_RETURN_TYPE_NUMBER:
		valueI = getNumberAssignmentReturnValue(tAssignmentReturn);
		ret = valueI;
		break;
	case ASSIGNMENT_RETURN_TYPE_FLOAT:
		valueF = getFloatAssignmentReturnValue(tAssignmentReturn);
		ret = (int)valueF;
		break;
	case ASSIGNMENT_RETURN_TYPE_BOOLEAN:
		valueI = getBooleanAssignmentReturnValue(tAssignmentReturn);
		ret = valueI;
		break;
	default:
		ret = 0;
		break;
	}

	destroyAssignmentReturn(tAssignmentReturn);

	return ret;
}

static double convertAssignmentReturnToFloat(AssignmentReturnValue tAssignmentReturn) {
	double ret;

	char* string;
	int valueI;
	double valueF;
	switch (tAssignmentReturn.mType) {
	case ASSIGNMENT_RETURN_TYPE_STRING:
		string = getStringAssignmentReturnValue(&tAssignmentReturn);
		ret = atof(string);
		break;
	case ASSIGNMENT_RETURN_TYPE_NUMBER:
		valueI = getNumberAssignmentReturnValue(tAssignmentReturn);
		ret = valueI;
		break;
	case ASSIGNMENT_RETURN_TYPE_FLOAT:
		valueF = getFloatAssignmentReturnValue(tAssignmentReturn);
		ret = valueF;
		break;
	case ASSIGNMENT_RETURN_TYPE_BOOLEAN:
		valueI = getBooleanAssignmentReturnValue(tAssignmentReturn);
		ret = valueI;
		break;
	default:
		ret = 0;
		break;
	}

	destroyAssignmentReturn(tAssignmentReturn);

	return ret;
}


static AssignmentReturnValue makeBooleanAssignmentReturn(int tValue) {
	AssignmentReturnValue val;
	AssignmentReturnBoolean* ret = (AssignmentReturnBoolean*)&val;
	ret->mType = ASSIGNMENT_RETURN_TYPE_BOOLEAN;
	ret->mBoolean = tValue;
	return val;
}

static AssignmentReturnValue makeNumberAssignmentReturn(int tValue) {
	AssignmentReturnValue val;
	AssignmentReturnNumber* ret = (AssignmentReturnNumber*)&val;
	ret->mType = ASSIGNMENT_RETURN_TYPE_NUMBER;
	ret->mNumber = tValue;
	return val;
}

static AssignmentReturnValue makeFloatAssignmentReturn(double tValue) {
	AssignmentReturnValue val;
	AssignmentReturnFloat* ret = (AssignmentReturnFloat*)&val;
	ret->mType = ASSIGNMENT_RETURN_TYPE_FLOAT;
	ret->mFloat = tValue;
	return val;
}

static AssignmentReturnValue makeStringAssignmentReturn(char* tValue) {
	AssignmentReturnValue val;
	AssignmentReturnString* ret = (AssignmentReturnString*)&val;
	ret->mType = ASSIGNMENT_RETURN_TYPE_STRING;
	strcpy(ret->mString, tValue);
	return val;
}

static AssignmentReturnValue makeBottomAssignmentReturn() {
	AssignmentReturnValue val;
	AssignmentReturnBottom* ret = (AssignmentReturnBottom*)&val;
	ret->mType = ASSIGNMENT_RETURN_TYPE_BOTTOM;
	return val;
}

static AssignmentReturnValue evaluateOrAssignment(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* tIsStatic) {
	DreamMugenDependOnTwoAssignment* orAssignment = (DreamMugenDependOnTwoAssignment*)*tAssignment;

	AssignmentReturnValue a = evaluateAssignmentDependency(&orAssignment->a, tPlayer, tIsStatic);
	int valA = convertAssignmentReturnToBool(a);
	if (valA) return makeBooleanAssignmentReturn(valA);

	AssignmentReturnValue b = evaluateAssignmentDependency(&orAssignment->b, tPlayer, tIsStatic);
	int valB = convertAssignmentReturnToBool(b);

	return makeBooleanAssignmentReturn(valB);
}

static AssignmentReturnValue evaluateAndAssignment(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* tIsStatic) {
	DreamMugenDependOnTwoAssignment* andAssignment = (DreamMugenDependOnTwoAssignment*)*tAssignment;

	AssignmentReturnValue a = evaluateAssignmentDependency(&andAssignment->a, tPlayer, tIsStatic);
	int valA = convertAssignmentReturnToBool(a);
	if (!valA) return makeBooleanAssignmentReturn(valA);

	AssignmentReturnValue b = evaluateAssignmentDependency(&andAssignment->b, tPlayer, tIsStatic);
	int valB = convertAssignmentReturnToBool(b);

	return makeBooleanAssignmentReturn(valB);
}

static AssignmentReturnValue evaluateBitwiseOrAssignment(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* tIsStatic) {
	DreamMugenDependOnTwoAssignment* bitwiseOrAssignment = (DreamMugenDependOnTwoAssignment*)*tAssignment;

	AssignmentReturnValue a = evaluateAssignmentDependency(&bitwiseOrAssignment->a, tPlayer, tIsStatic);
	int valA = convertAssignmentReturnToNumber(a);

	AssignmentReturnValue b = evaluateAssignmentDependency(&bitwiseOrAssignment->b, tPlayer, tIsStatic);
	int valB = convertAssignmentReturnToNumber(b);

	return makeNumberAssignmentReturn(valA | valB);
}

static AssignmentReturnValue evaluateBitwiseAndAssignment(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* tIsStatic) {
	DreamMugenDependOnTwoAssignment* bitwiseAndAssignment = (DreamMugenDependOnTwoAssignment*)*tAssignment;

	AssignmentReturnValue a = evaluateAssignmentDependency(&bitwiseAndAssignment->a, tPlayer, tIsStatic);
	int valA = convertAssignmentReturnToNumber(a);

	AssignmentReturnValue b = evaluateAssignmentDependency(&bitwiseAndAssignment->b, tPlayer, tIsStatic);
	int valB = convertAssignmentReturnToNumber(b);

	return makeNumberAssignmentReturn(valA & valB);
}

static AssignmentReturnValue evaluateCommandAssignment(AssignmentReturnValue tCommand, DreamPlayer* tPlayer, int* tIsStatic) {
	char* string = convertAssignmentReturnToAllocatedString(tCommand);
	int ret = isPlayerCommandActive(tPlayer, string);
	freeMemory(string);

	*tIsStatic = 0;
	return makeBooleanAssignmentReturn(ret);
}

static AssignmentReturnValue evaluateStateTypeAssignment(AssignmentReturnValue tCommand, DreamPlayer* tPlayer, int* tIsStatic) {
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

static AssignmentReturnValue evaluateMoveTypeAssignment(AssignmentReturnValue tCommand, DreamPlayer* tPlayer, int* tIsStatic) {
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

static AssignmentReturnValue evaluateAnimElemVectorAssignment(AssignmentReturnValue tCommand, DreamPlayer* tPlayer, int* tIsStatic) {

	char number1[100], comma[10], oper[100], number2[100];
	char* test = convertAssignmentReturnToAllocatedString(tCommand);
	int items = sscanf(test, "%s %s %s %s", number1, comma, oper, number2);
	freeMemory(test);

	if(!strcmp("", number1) || strcmp(",", comma)) { 
		logWarningFormat("Unable to parse animelem vector assignment %s. Defaulting to bottom.", test);
		return makeBottomAssignmentReturn(); 
	}

	int ret;
	int animationStep = atoi(number1);
	if (items == 3) {
		int time = atoi(oper);

		int timeTillAnimation = getPlayerTimeFromAnimationElement(tPlayer, animationStep);
		ret = time == timeTillAnimation;
	}
	else if (items == 4) {
		int time = atoi(number2);
		int timeTillAnimation = getPlayerTimeFromAnimationElement(tPlayer, animationStep);
		int stepTime = getPlayerAnimationTimeWhenStepStarts(tPlayer, animationStep);
		if (!isPlayerAnimationTimeOffsetInAnimation(tPlayer, stepTime + time)) {
			time = 0; // TODO: fix to total animation time
		}

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
		logWarningFormat("Invalid animelem format %s. Default to false.", tCommand.mValue);
		ret = 0;
	}

	*tIsStatic = 0;
	return makeBooleanAssignmentReturn(ret);
}

static AssignmentReturnValue evaluateAnimElemNumberAssignment(AssignmentReturnValue tCommand, DreamPlayer* tPlayer, int* tIsStatic) {

	int elem = convertAssignmentReturnToNumber(tCommand);
	int ret = isPlayerStartingAnimationElementWithID(tPlayer, elem);

	*tIsStatic = 0;
	return makeBooleanAssignmentReturn(ret);
}

static AssignmentReturnValue evaluateAnimElemAssignment(AssignmentReturnValue tCommand, DreamPlayer* tPlayer, int* tIsStatic) {

	if (tCommand.mType == ASSIGNMENT_RETURN_TYPE_STRING && strchr(getStringAssignmentReturnValue(&tCommand), ',')) {
		return evaluateAnimElemVectorAssignment(tCommand, tPlayer, tIsStatic);
	}
	else {
		return evaluateAnimElemNumberAssignment(tCommand, tPlayer, tIsStatic);
	}
}

static AssignmentReturnValue evaluateTimeModAssignment(AssignmentReturnValue tCommand, DreamPlayer* tPlayer, int* tIsStatic) {

	char divisor[100], comma[10], compareNumber[100];
	char* test = convertAssignmentReturnToAllocatedString(tCommand);
	int items = sscanf(test, "%s %s %s", divisor, comma, compareNumber);
	freeMemory(test);

	if (!strcmp("", divisor) || strcmp(",", comma) || items != 3) {
		logWarningFormat("Unable to parse timemod assignment %s. Defaulting to bottom.", tCommand.mValue);
		return makeBottomAssignmentReturn(); 
	}

	int divisorValue = atoi(divisor);
	int compareValue = atoi(compareNumber);
	int stateTime = getPlayerTimeInState(tPlayer);
	
	if (divisorValue == 0) {
		return makeBooleanAssignmentReturn(0);
	}

	int modValue = stateTime % divisorValue;
	int ret = modValue == compareValue;
	*tIsStatic = 0;
	return makeBooleanAssignmentReturn(ret);
}

static DreamPlayer* getPlayerFromFirstVectorPartOrNullIfNonexistant(AssignmentReturnValue a, DreamPlayer* tPlayer) {
	if (a.mType != ASSIGNMENT_RETURN_TYPE_STRING) return NULL;
	char* test = getStringAssignmentReturnValue(&a);

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

static int isPlayerAccessVectorAssignment(AssignmentReturnValue a, DreamPlayer* tPlayer) {
	return getPlayerFromFirstVectorPartOrNullIfNonexistant(a, tPlayer) != NULL;
}

static AssignmentReturnValue evaluateTeamModeAssignment(AssignmentReturnValue tCommand, DreamPlayer* tPlayer, int* tIsStatic) {
	(void)tPlayer;

	char* test = convertAssignmentReturnToAllocatedString(tCommand); 
	turnStringLowercase(test);
	int ret = !strcmp(test, "single"); // TODO
	freeMemory(test);

	(void)tIsStatic;
	return makeBooleanAssignmentReturn(ret); 
}

static int isRangeAssignmentReturn(AssignmentReturnValue ret) {
	if (ret.mType != ASSIGNMENT_RETURN_TYPE_STRING) return 0;
	char* test = getStringAssignmentReturnValue(&ret);

	return test[0] == '[' && test[1] == ' ';
}

static AssignmentReturnValue evaluateRangeComparisonAssignment(AssignmentReturnValue a, AssignmentReturnValue tRange) {
	int val = convertAssignmentReturnToNumber(a);

	char* test = convertAssignmentReturnToAllocatedString(tRange);
	char openBrace[10], valString1[20], comma[10], valString2[20], closeBrace[10];
	sscanf(test, "%s %s %s %s %s", openBrace, valString1, comma, valString2, closeBrace);
	freeMemory(test);

	if (strcmp("[", openBrace) || !strcmp("", valString1) || strcmp(",", comma) || !strcmp("", valString2) || strcmp("]", closeBrace)) {
		logWarningFormat("Unable to parse range comparison assignment %s. Defaulting to bottom.", tRange.mValue);
		return makeBottomAssignmentReturn(); 
	}

	int val1 = atoi(valString1);
	int val2 = atoi(valString2);

	return makeBooleanAssignmentReturn(val >= val1 && val <= val2);
}

static int isFloatReturn(AssignmentReturnValue tReturn) {
	return tReturn.mType == ASSIGNMENT_RETURN_TYPE_FLOAT;
}


static AssignmentReturnValue evaluateSetVariableAssignment(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* tIsStatic) {
	DreamMugenDependOnTwoAssignment* varSetAssignment = (DreamMugenDependOnTwoAssignment*)*tAssignment;
	
	if (varSetAssignment->a->mType != MUGEN_ASSIGNMENT_TYPE_ARRAY){
		logWarningFormat("Incorrect varset type %d. Defaulting to bottom.", varSetAssignment->a->mType);
		return makeBottomAssignmentReturn(); 
	}
	DreamMugenDependOnTwoAssignment* varArrayAccess = (DreamMugenDependOnTwoAssignment*)varSetAssignment->a;
	AssignmentReturnValue a = evaluateAssignmentDependency(&varArrayAccess->a, tPlayer, tIsStatic);
	int index = evaluateDreamAssignmentAndReturnAsInteger(&varArrayAccess->b, tPlayer);

	char* test = convertAssignmentReturnToAllocatedString(a);
	turnStringLowercase(test);

	AssignmentReturnValue ret;
	if (!strcmp("var", test)) {
		int value = evaluateDreamAssignmentAndReturnAsInteger(&varSetAssignment->b, tPlayer);
		setPlayerVariable(tPlayer, index, value);
		ret = makeNumberAssignmentReturn(value);
	}
	else if (!strcmp("fvar", test)) {
		double value = evaluateDreamAssignmentAndReturnAsFloat(&varSetAssignment->b, tPlayer);
		setPlayerFloatVariable(tPlayer, index, value);
		ret = makeFloatAssignmentReturn(value);
	}
	else if (!strcmp("sysvar", test)) {
		int value = evaluateDreamAssignmentAndReturnAsInteger(&varSetAssignment->b, tPlayer);
		setPlayerSystemVariable(tPlayer, index, value);
		ret = makeNumberAssignmentReturn(value);
	}
	else if (!strcmp("sysfvar", test)) {
		double value = evaluateDreamAssignmentAndReturnAsFloat(&varSetAssignment->b, tPlayer);
		setPlayerSystemFloatVariable(tPlayer, index, value);
		ret = makeFloatAssignmentReturn(value);
	}
	else {
		logWarningFormat("Unrecognized varset name %s. Returning bottom.", a.mValue);
		ret = makeBottomAssignmentReturn(); 
	}

	freeMemory(test);

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

static AssignmentReturnValue evaluateHitDefAttributeAssignment(AssignmentReturnValue tValue, DreamPlayer* tPlayer, int* tIsStatic) {

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
static AssignmentReturnValue evaluateProjVectorAssignment(AssignmentReturnValue tCommand, DreamPlayer* tPlayer, int tProjectileID, int(*tTimeFunc)(DreamPlayer*, int), int* tIsStatic) {
	char* test = convertAssignmentReturnToAllocatedString(tCommand);

	char number1[100], comma[10], oper[100], number2[100];
	int items = sscanf(test, "%s %s %s %s", number1, comma, oper, number2);
	freeMemory(test);
	if (!strcmp("", number1) || strcmp(",", comma)) {
		logWarningFormat("Unable to parse proj vector assignment %s. Defaulting to bottom.", tCommand.mValue);
		return makeBottomAssignmentReturn();
	}

	int ret;
	int compareValue = atoi(number1);
	if (items == 3) {
		int time = atoi(oper);

		int timeOffset = tTimeFunc(tPlayer, tProjectileID);
		ret = time == timeOffset;
	}
	else if (items == 4) {
		int time = atoi(number2);
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
			ret = 0;
		}
	}
	else {
		logWarningFormat("Invalid animelem format %s. Default to false.", tCommand.mValue);
		ret = 0;
	}

	*tIsStatic = 0;
	return makeBooleanAssignmentReturn(ret == compareValue);
}

static AssignmentReturnValue evaluateProjNumberAssignment(AssignmentReturnValue tCommand, DreamPlayer* tPlayer, int tProjectileID, int(*tTimeFunc)(DreamPlayer*, int), int* tIsStatic) {

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

static AssignmentReturnValue evaluateProjAssignment(char* tName, char* tBaseName, AssignmentReturnValue tCommand, DreamPlayer* tPlayer, int(*tTimeFunc)(DreamPlayer*, int), int* tIsStatic) {
	int projID = getProjectileIDFromAssignmentName(tName, tBaseName);

	if (tCommand.mType == ASSIGNMENT_RETURN_TYPE_STRING && strchr(getStringAssignmentReturnValue(&tCommand), ',')) {
		return evaluateProjVectorAssignment(tCommand, tPlayer, projID, tTimeFunc, tIsStatic);
	}
	else {
		return evaluateProjNumberAssignment(tCommand, tPlayer, projID, tTimeFunc, tIsStatic);
	}
}

static int isProjAssignment(char* tName, char* tBaseName) {
	return !strncmp(tName, tBaseName, strlen(tBaseName));

}

typedef AssignmentReturnValue(*VariableFunction)(DreamPlayer*);
typedef AssignmentReturnValue(*ArrayFunction)(DreamMugenDependOnTwoAssignment*, DreamPlayer*, int*);
typedef AssignmentReturnValue(*ComparisonFunction)(char*, AssignmentReturnValue, DreamPlayer*, int*);

static struct {
	StringMap mVariables; // contains VariableFunction
	StringMap mConstants; // contains VariableFunction
	StringMap mArrays; // contains ArrayFunction
	StringMap mComparisons; // contains ComparisonFunction
} gVariableHandler;

static int tryEvaluateVariableComparison(DreamMugenVariableAssignment* tVariableAssignment, AssignmentReturnValue* oRet, AssignmentReturnValue b, DreamPlayer* tPlayer, int* tIsStatic) {
	char name[MUGEN_DEF_STRING_LENGTH];
	strcpy(name, tVariableAssignment->mName);
	turnStringLowercase(name);

	int hasReturn = 0;
	if(!strcmp("command", name)) {
		hasReturn = 1;
		if(b.mType != ASSIGNMENT_RETURN_TYPE_STRING) {
			*oRet = makeBooleanAssignmentReturn(0);
		} else {
			char* comp = getStringAssignmentReturnValue(&b);
			*oRet = makeBooleanAssignmentReturn(isPlayerCommandActive(tPlayer, comp));
		}	
		*tIsStatic = 0;
	}
	else if (string_map_contains(&gVariableHandler.mComparisons, name)) {
		ComparisonFunction func = (ComparisonFunction)string_map_get(&gVariableHandler.mComparisons, name);
		hasReturn = 1;
		*oRet = func(name, b, tPlayer, tIsStatic);
	}
	else if (isProjAssignment(name, "projcontact")) {
		hasReturn = 1;
		*oRet = evaluateProjAssignment(name, "projcontact", b, tPlayer, getPlayerProjectileTimeSinceContact, tIsStatic);
	}
	else if (isProjAssignment(name, "projguarded")) {
		hasReturn = 1;
		*oRet = evaluateProjAssignment(name, "projguarded", b, tPlayer, getPlayerProjectileTimeSinceGuarded, tIsStatic);
	}
	else if (isProjAssignment(name, "projhit")) {
		hasReturn = 1;
		*oRet = evaluateProjAssignment(name, "projhit", b, tPlayer, getPlayerProjectileTimeSinceHit, tIsStatic);
	}

	return hasReturn;
}

static AssignmentReturnValue evaluateComparisonAssignmentInternal(DreamMugenAssignment** mAssignment, AssignmentReturnValue b, DreamPlayer* tPlayer, int* tIsStatic) {
	if ((*mAssignment)->mType == MUGEN_ASSIGNMENT_TYPE_NEGATION) {
		DreamMugenDependOnOneAssignment* neg = (DreamMugenDependOnOneAssignment*)(*mAssignment);
		if (neg->a->mType == MUGEN_ASSIGNMENT_TYPE_VARIABLE) {
			DreamMugenVariableAssignment* negVar = (DreamMugenVariableAssignment*)neg->a;
			AssignmentReturnValue retVal;
			if (tryEvaluateVariableComparison(negVar, &retVal, b, tPlayer, tIsStatic)) {
				return makeBooleanAssignmentReturn(!convertAssignmentReturnToBool(retVal));
			}
		}
	}
	else if ((*mAssignment)->mType == MUGEN_ASSIGNMENT_TYPE_VARIABLE) {
		DreamMugenVariableAssignment* var = (DreamMugenVariableAssignment*)*mAssignment;
		AssignmentReturnValue retVal;
		if (tryEvaluateVariableComparison(var, &retVal, b, tPlayer, tIsStatic)) {
			return retVal;
		}	
	}

	AssignmentReturnValue a = evaluateAssignmentDependency(mAssignment, tPlayer, tIsStatic);
	if (isRangeAssignmentReturn(b)) {
		return evaluateRangeComparisonAssignment(a, b);
	}
	else if (isFloatReturn(a) || isFloatReturn(b)) {
		int value = convertAssignmentReturnToFloat(a) == convertAssignmentReturnToFloat(b);
		return makeBooleanAssignmentReturn(value);
	}
	else {
		char* val1 = convertAssignmentReturnToAllocatedString(a); // TODO: more efficient
		char* val2 = convertAssignmentReturnToAllocatedString(b);
		int value = !strcmp(val1, val2);
		freeMemory(val1);
		freeMemory(val2);
		return makeBooleanAssignmentReturn(value);
	}
}

static AssignmentReturnValue evaluateComparisonAssignment(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* tIsStatic) {
	DreamMugenDependOnTwoAssignment* comparisonAssignment = (DreamMugenDependOnTwoAssignment*)*tAssignment;


	if (comparisonAssignment->a->mType == MUGEN_ASSIGNMENT_TYPE_VECTOR) {
		DreamMugenDependOnTwoAssignment* vectorAssignment = (DreamMugenDependOnTwoAssignment*)comparisonAssignment->a;
		AssignmentReturnValue vecA = evaluateAssignmentDependency(&vectorAssignment->a, tPlayer, tIsStatic);
		if (isPlayerAccessVectorAssignment(vecA, tPlayer)) {
			DreamPlayer* target = getPlayerFromFirstVectorPartOrNullIfNonexistant(vecA, tPlayer);
			destroyAssignmentReturn(vecA);
			if (!isPlayerTargetValid(target)) {
				logWarning("Accessed player was NULL. Defaulting to bottom.");
				return makeBottomAssignmentReturn(); 
			}
			AssignmentReturnValue b = evaluateAssignmentDependency(&comparisonAssignment->b, tPlayer, tIsStatic);
			return evaluateComparisonAssignmentInternal(&vectorAssignment->b, b, target, tIsStatic);
		}
		else destroyAssignmentReturn(vecA);
	}

	AssignmentReturnValue b = evaluateAssignmentDependency(&comparisonAssignment->b, tPlayer, tIsStatic);
	return evaluateComparisonAssignmentInternal(&comparisonAssignment->a, b, tPlayer, tIsStatic);
}

static AssignmentReturnValue evaluateInequalityAssignment(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* tIsStatic) {
	AssignmentReturnValue equal = evaluateComparisonAssignment(tAssignment, tPlayer, tIsStatic);
	int val = convertAssignmentReturnToBool(equal);

	return makeBooleanAssignmentReturn(!val);
}

static AssignmentReturnValue evaluateGreaterIntegers(AssignmentReturnValue a, AssignmentReturnValue b) {
	int val = convertAssignmentReturnToNumber(a) > convertAssignmentReturnToNumber(b);
	return makeBooleanAssignmentReturn(val);
}

static AssignmentReturnValue evaluateGreaterFloats(AssignmentReturnValue a, AssignmentReturnValue b) {
	int val = convertAssignmentReturnToFloat(a) > convertAssignmentReturnToFloat(b);
	return makeBooleanAssignmentReturn(val);
}

static AssignmentReturnValue evaluateGreaterAssignment(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* tIsStatic) {
	DreamMugenDependOnTwoAssignment* greaterAssignment = (DreamMugenDependOnTwoAssignment*)*tAssignment;
	AssignmentReturnValue a = evaluateAssignmentDependency(&greaterAssignment->a, tPlayer, tIsStatic);
	AssignmentReturnValue b = evaluateAssignmentDependency(&greaterAssignment->b, tPlayer, tIsStatic);

	if (isFloatReturn(a) || isFloatReturn(b)) {
		return evaluateGreaterFloats(a, b);
	}
	else {
		return evaluateGreaterIntegers(a, b);
	}
}

static AssignmentReturnValue evaluateGreaterOrEqualIntegers(AssignmentReturnValue a, AssignmentReturnValue b) {
	int val = convertAssignmentReturnToNumber(a) >= convertAssignmentReturnToNumber(b);
	return makeBooleanAssignmentReturn(val);
}

static AssignmentReturnValue evaluateGreaterOrEqualFloats(AssignmentReturnValue a, AssignmentReturnValue b) {
	int val = convertAssignmentReturnToFloat(a) >= convertAssignmentReturnToFloat(b);
	return makeBooleanAssignmentReturn(val);
}

static AssignmentReturnValue evaluateGreaterOrEqualAssignment(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* tIsStatic) {
	DreamMugenDependOnTwoAssignment* greaterOrEqualAssignment = (DreamMugenDependOnTwoAssignment*)*tAssignment;
	AssignmentReturnValue a = evaluateAssignmentDependency(&greaterOrEqualAssignment->a, tPlayer, tIsStatic);
	AssignmentReturnValue b = evaluateAssignmentDependency(&greaterOrEqualAssignment->b, tPlayer, tIsStatic);

	if (isFloatReturn(a) || isFloatReturn(b)) {
		return evaluateGreaterOrEqualFloats(a, b);
	}
	else {
		return evaluateGreaterOrEqualIntegers(a, b);
	}
}

static AssignmentReturnValue evaluateLessIntegers(AssignmentReturnValue a, AssignmentReturnValue b) {
	int val = convertAssignmentReturnToNumber(a) < convertAssignmentReturnToNumber(b);
	return makeBooleanAssignmentReturn(val);
}

static AssignmentReturnValue evaluateLessFloats(AssignmentReturnValue a, AssignmentReturnValue b) {
	int val = convertAssignmentReturnToFloat(a) < convertAssignmentReturnToFloat(b);
	return makeBooleanAssignmentReturn(val);
}

static AssignmentReturnValue evaluateLessAssignment(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* tIsStatic) {
	DreamMugenDependOnTwoAssignment* lessAssignment = (DreamMugenDependOnTwoAssignment*)*tAssignment;
	AssignmentReturnValue a = evaluateAssignmentDependency(&lessAssignment->a, tPlayer, tIsStatic);
	AssignmentReturnValue b = evaluateAssignmentDependency(&lessAssignment->b, tPlayer, tIsStatic);

	if (isFloatReturn(a) || isFloatReturn(b)) {
		return evaluateLessFloats(a, b);
	}
	else {
		return evaluateLessIntegers(a, b);
	}
}

static AssignmentReturnValue evaluateLessOrEqualIntegers(AssignmentReturnValue a, AssignmentReturnValue b) {
	int val = convertAssignmentReturnToNumber(a) <= convertAssignmentReturnToNumber(b);
	return makeBooleanAssignmentReturn(val);
}

static AssignmentReturnValue evaluateLessOrEqualFloats(AssignmentReturnValue a, AssignmentReturnValue b) {
	int val = convertAssignmentReturnToFloat(a) <= convertAssignmentReturnToFloat(b);
	return makeBooleanAssignmentReturn(val);
}

static AssignmentReturnValue evaluateLessOrEqualAssignment(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* tIsStatic) {
	DreamMugenDependOnTwoAssignment* lessOrEqualAssignment = (DreamMugenDependOnTwoAssignment*)*tAssignment;
	AssignmentReturnValue a = evaluateAssignmentDependency(&lessOrEqualAssignment->a, tPlayer, tIsStatic);
	AssignmentReturnValue b = evaluateAssignmentDependency(&lessOrEqualAssignment->b, tPlayer, tIsStatic);

	if (isFloatReturn(a) || isFloatReturn(b)) {
		return evaluateLessOrEqualFloats(a, b);
	}
	else {
		return evaluateLessOrEqualIntegers(a, b);
	}
}

static AssignmentReturnValue evaluateModuloIntegers(AssignmentReturnValue a, AssignmentReturnValue b) {
	int val1 = convertAssignmentReturnToNumber(a);
	int val2 = convertAssignmentReturnToNumber(b);
	if (!val2) return makeBottomAssignmentReturn();

	int val = val1 % val2;
	return makeNumberAssignmentReturn(val);
}

static AssignmentReturnValue evaluateModuloAssignment(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* tIsStatic) {
	DreamMugenDependOnTwoAssignment* moduloAssignment = (DreamMugenDependOnTwoAssignment*)*tAssignment;
	AssignmentReturnValue a = evaluateAssignmentDependency(&moduloAssignment->a, tPlayer, tIsStatic);
	AssignmentReturnValue b = evaluateAssignmentDependency(&moduloAssignment->b, tPlayer, tIsStatic);

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

static AssignmentReturnValue evaluateExponentiationIntegers(AssignmentReturnValue a, AssignmentReturnValue b) {
	int val1 = convertAssignmentReturnToNumber(a);
	int val2 = convertAssignmentReturnToNumber(b);
	return makeNumberAssignmentReturn(powI(val1, val2));
}

static AssignmentReturnValue evaluateExponentiationFloats(AssignmentReturnValue a, AssignmentReturnValue b) {
	double val1 = convertAssignmentReturnToFloat(a);
	double val2 = convertAssignmentReturnToFloat(b);
	return makeFloatAssignmentReturn(pow(val1, val2));
}

static AssignmentReturnValue evaluateExponentiationAssignment(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* tIsStatic) {
	DreamMugenDependOnTwoAssignment* exponentiationAssignment = (DreamMugenDependOnTwoAssignment*)*tAssignment;
	AssignmentReturnValue a = evaluateAssignmentDependency(&exponentiationAssignment->a, tPlayer, tIsStatic);
	AssignmentReturnValue b = evaluateAssignmentDependency(&exponentiationAssignment->b, tPlayer, tIsStatic);

	if (isFloatReturn(a) || isFloatReturn(b) || convertAssignmentReturnToNumber(b) < 0) {
		return evaluateExponentiationFloats(a, b);
	}
	else {
		return evaluateExponentiationIntegers(a, b);
	}
}


static AssignmentReturnValue evaluateMultiplicationIntegers(AssignmentReturnValue a, AssignmentReturnValue b) {
	int val = convertAssignmentReturnToNumber(a) * convertAssignmentReturnToNumber(b);
	return makeNumberAssignmentReturn(val);
}

static AssignmentReturnValue evaluateMultiplicationFloats(AssignmentReturnValue a, AssignmentReturnValue b) {
	double val = convertAssignmentReturnToFloat(a) * convertAssignmentReturnToFloat(b);
	return makeFloatAssignmentReturn(val);
}

static AssignmentReturnValue evaluateMultiplicationAssignment(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* tIsStatic) {
	DreamMugenDependOnTwoAssignment* multiplicationAssignment = (DreamMugenDependOnTwoAssignment*)*tAssignment;
	AssignmentReturnValue a = evaluateAssignmentDependency(&multiplicationAssignment->a, tPlayer, tIsStatic);
	AssignmentReturnValue b = evaluateAssignmentDependency(&multiplicationAssignment->b, tPlayer, tIsStatic);

	if (isFloatReturn(a) || isFloatReturn(b)) {
		return evaluateMultiplicationFloats(a, b);
	}
	else {
		return evaluateMultiplicationIntegers(a, b);
	}
}

static AssignmentReturnValue evaluateDivisionIntegers(AssignmentReturnValue a, AssignmentReturnValue b) {
	int val1 = convertAssignmentReturnToNumber(a);
	int val2 = convertAssignmentReturnToNumber(b);
	if (!val2) return makeBottomAssignmentReturn();
	int val = val1 / val2;
	return makeNumberAssignmentReturn(val);
}

static AssignmentReturnValue evaluateDivisionFloats(AssignmentReturnValue a, AssignmentReturnValue b) {
	double val = convertAssignmentReturnToFloat(a) / convertAssignmentReturnToFloat(b);
	return makeFloatAssignmentReturn(val);
}

static AssignmentReturnValue evaluateDivisionAssignment(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* tIsStatic) {
	DreamMugenDependOnTwoAssignment* divisionAssignment = (DreamMugenDependOnTwoAssignment*)*tAssignment;
	AssignmentReturnValue a = evaluateAssignmentDependency(&divisionAssignment->a, tPlayer, tIsStatic);
	AssignmentReturnValue b = evaluateAssignmentDependency(&divisionAssignment->b, tPlayer, tIsStatic);

	if (isFloatReturn(a) || isFloatReturn(b)) {
		return evaluateDivisionFloats(a, b);
	}
	else {
		return evaluateDivisionIntegers(a, b);
	}
}

static int isSparkFileReturn(AssignmentReturnValue a) {
	if (a.mType != ASSIGNMENT_RETURN_TYPE_STRING) return 0;

	char* test = getStringAssignmentReturnValue(&a);
	char firstW[200];
	int items = sscanf(test, "%s", firstW);
	if (!items) return 0;
	turnStringLowercase(firstW);

	return !strcmp("isinotherfilef", firstW) || !strcmp("isinotherfiles", firstW);
}

static AssignmentReturnValue evaluateAdditionSparkFile(AssignmentReturnValue a, AssignmentReturnValue b) {
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


static AssignmentReturnValue evaluateAdditionIntegers(AssignmentReturnValue a, AssignmentReturnValue b) {
	int val = convertAssignmentReturnToNumber(a) + convertAssignmentReturnToNumber(b);
	return makeNumberAssignmentReturn(val);
}

static AssignmentReturnValue evaluateAdditionFloats(AssignmentReturnValue a, AssignmentReturnValue b) {
	double val = convertAssignmentReturnToFloat(a) + convertAssignmentReturnToFloat(b);
	return makeFloatAssignmentReturn(val);
}

static AssignmentReturnValue evaluateAdditionAssignment(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* tIsStatic) {
	DreamMugenDependOnTwoAssignment* additionAssignment = (DreamMugenDependOnTwoAssignment*)*tAssignment;
	AssignmentReturnValue a = evaluateAssignmentDependency(&additionAssignment->a, tPlayer, tIsStatic);
	AssignmentReturnValue b = evaluateAssignmentDependency(&additionAssignment->b, tPlayer, tIsStatic);

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


static AssignmentReturnValue evaluateSubtractionIntegers(AssignmentReturnValue a, AssignmentReturnValue b) {
	int val = convertAssignmentReturnToNumber(a) - convertAssignmentReturnToNumber(b);
	return makeNumberAssignmentReturn(val);
}

static AssignmentReturnValue evaluateSubtractionFloats(AssignmentReturnValue a, AssignmentReturnValue b) {
	double val = convertAssignmentReturnToFloat(a) - convertAssignmentReturnToFloat(b);
	return makeFloatAssignmentReturn(val);
}

static AssignmentReturnValue evaluateSubtractionAssignment(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* tIsStatic) {
	DreamMugenDependOnTwoAssignment* subtractionAssignment = (DreamMugenDependOnTwoAssignment*)*tAssignment;
	AssignmentReturnValue a = evaluateAssignmentDependency(&subtractionAssignment->a, tPlayer, tIsStatic);
	AssignmentReturnValue b = evaluateAssignmentDependency(&subtractionAssignment->b, tPlayer, tIsStatic);

	if (isFloatReturn(a) || isFloatReturn(b)) {
		return evaluateSubtractionFloats(a, b);
	}
	else {
		return evaluateSubtractionIntegers(a, b);
	}
}


static AssignmentReturnValue evaluateOperatorArgumentAssignment(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* tIsStatic) {
	DreamMugenDependOnTwoAssignment* operatorAssignment = (DreamMugenDependOnTwoAssignment*)*tAssignment;
	AssignmentReturnValue a = evaluateAssignmentDependency(&operatorAssignment->a, tPlayer, tIsStatic);
	AssignmentReturnValue b = evaluateAssignmentDependency(&operatorAssignment->b, tPlayer, tIsStatic);


	char* val1 = convertAssignmentReturnToAllocatedString(a);
	char* val2 = convertAssignmentReturnToAllocatedString(b);
	char buffer[100]; // TODO: dynamic
	sprintf(buffer, "%s %s", val1, val2);
	freeMemory(val1);
	freeMemory(val2);
	return makeStringAssignmentReturn(buffer);
}

static AssignmentReturnValue evaluateBooleanAssignment(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* tIsStatic) {
	(void)tPlayer;
	(void)tIsStatic;
	DreamMugenFixedBooleanAssignment* fixedAssignment = (DreamMugenFixedBooleanAssignment*)*tAssignment;

	return makeBooleanAssignmentReturn(fixedAssignment->mValue);
}

static AssignmentReturnValue evaluateNumberAssignment(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* tIsStatic) {
	(void)tPlayer;
	(void)tIsStatic;
	DreamMugenNumberAssignment* number = (DreamMugenNumberAssignment*)*tAssignment;

	return makeNumberAssignmentReturn(number->mValue);
}

static AssignmentReturnValue evaluateFloatAssignment(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* tIsStatic) {
	(void)tPlayer;
	(void)tIsStatic;
	DreamMugenFloatAssignment* f = (DreamMugenFloatAssignment*)*tAssignment;

	return makeFloatAssignmentReturn(f->mValue);
}

static AssignmentReturnValue evaluateStringAssignment(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* tIsStatic) {
	(void)tPlayer;
	(void)tIsStatic;
	DreamMugenStringAssignment* s = (DreamMugenStringAssignment*)*tAssignment;

	return makeStringAssignmentReturn(s->mValue);
}

static AssignmentReturnValue evaluatePlayerVectorAssignment(AssignmentReturnValue tFirstValue, DreamMugenDependOnTwoAssignment* tVectorAssignment, DreamPlayer* tPlayer, int* tIsStatic) {
	DreamPlayer* target = getPlayerFromFirstVectorPartOrNullIfNonexistant(tFirstValue, tPlayer);
	destroyAssignmentReturn(tFirstValue);
	if (!isPlayerTargetValid(target)) {
		logWarning("Unable to evaluate player vector assignment with NULL. Defaulting to bottom."); // TODO: debug
		return makeBottomAssignmentReturn(); 
	}
	if (tVectorAssignment->b->mType != MUGEN_ASSIGNMENT_TYPE_VARIABLE && tVectorAssignment->b->mType != MUGEN_ASSIGNMENT_TYPE_ARRAY) {
		logWarningFormat("Invalid player vector assignment type %d. Defaulting to bottom.", tVectorAssignment->b->mType);
		return makeBottomAssignmentReturn();
	}

	return evaluateAssignmentDependency(&tVectorAssignment->b, target, tIsStatic);
}

static AssignmentReturnValue evaluateVectorAssignment(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* tIsStatic) {
	DreamMugenDependOnTwoAssignment* vectorAssignment = (DreamMugenDependOnTwoAssignment*)*tAssignment;
	AssignmentReturnValue a = evaluateAssignmentDependency(&vectorAssignment->a, tPlayer, tIsStatic);

	if (isPlayerAccessVectorAssignment(a, tPlayer)) {
		return evaluatePlayerVectorAssignment(a, vectorAssignment, tPlayer, tIsStatic);
	}
	else {
		AssignmentReturnValue b = evaluateAssignmentDependency(&vectorAssignment->b, tPlayer, tIsStatic);
		char* val1 = convertAssignmentReturnToAllocatedString(a);
		char* val2 = convertAssignmentReturnToAllocatedString(b);
		char buffer[100]; // TODO: dynamic
		sprintf(buffer, "%s , %s", val1, val2);
		freeMemory(val1);
		freeMemory(val2);
		return makeStringAssignmentReturn(buffer);
	}
}

static AssignmentReturnValue evaluateRangeAssignment(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* tIsStatic) {
	DreamMugenRangeAssignment* rangeAssignment = (DreamMugenRangeAssignment*)*tAssignment;
	AssignmentReturnValue a = evaluateAssignmentDependency(&rangeAssignment->a, tPlayer, tIsStatic);

	char* test = convertAssignmentReturnToAllocatedString(a);
	char valString1[100], comma[10], valString2[100];
	sscanf(test, "%s %s %s", valString1, comma, valString2);
	if (!strcmp("", valString1) || strcmp(",", comma) || !strcmp("", valString2)) {
		logWarningFormat("Unable to parse range assignment %s. Defaulting to bottom.", test);
		freeMemory(test);
		return makeBottomAssignmentReturn(); 
	}
	freeMemory(test);

	int val1 = atoi(valString1);
	int val2 = atoi(valString2);
	if (rangeAssignment->mExcludeLeft) val1++;
	if (rangeAssignment->mExcludeRight) val2--;

	char buffer[100]; // TODO: dynamic
	sprintf(buffer, "[ %d , %d ]", val1, val2);
	return makeStringAssignmentReturn(buffer);
}

static AssignmentReturnValue commandComparisonFunction(char* tName, AssignmentReturnValue b, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateCommandAssignment(b, tPlayer, tIsStatic); }
static AssignmentReturnValue stateTypeComparisonFunction(char* tName, AssignmentReturnValue b, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateStateTypeAssignment(b, tPlayer, tIsStatic); }
static AssignmentReturnValue p2StateTypeComparisonFunction(char* tName, AssignmentReturnValue b, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateStateTypeAssignment(b, getPlayerOtherPlayer(tPlayer), tIsStatic); }
static AssignmentReturnValue moveTypeComparisonFunction(char* tName, AssignmentReturnValue b, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateMoveTypeAssignment(b, tPlayer, tIsStatic); }
static AssignmentReturnValue p2MoveTypeComparisonFunction(char* tName, AssignmentReturnValue b, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateMoveTypeAssignment(b, getPlayerOtherPlayer(tPlayer), tIsStatic); }
static AssignmentReturnValue animElemComparisonFunction(char* tName, AssignmentReturnValue b, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateAnimElemAssignment(b, tPlayer, tIsStatic); }
static AssignmentReturnValue timeModComparisonFunction(char* tName, AssignmentReturnValue b, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateTimeModAssignment(b, tPlayer, tIsStatic); }
static AssignmentReturnValue teamModeComparisonFunction(char* tName, AssignmentReturnValue b, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateTeamModeAssignment(b, tPlayer, tIsStatic); }
static AssignmentReturnValue hitDefAttributeComparisonFunction(char* tName, AssignmentReturnValue b, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateHitDefAttributeAssignment(b, tPlayer, tIsStatic); }


static void setupComparisons() {
	gVariableHandler.mComparisons = new_string_map();

	string_map_push(&gVariableHandler.mComparisons, "command", commandComparisonFunction);
	string_map_push(&gVariableHandler.mComparisons, "statetype", stateTypeComparisonFunction);
	string_map_push(&gVariableHandler.mComparisons, "p2statetype", p2StateTypeComparisonFunction);
	string_map_push(&gVariableHandler.mComparisons, "movetype", moveTypeComparisonFunction);
	string_map_push(&gVariableHandler.mComparisons, "p2movetype", p2MoveTypeComparisonFunction);
	string_map_push(&gVariableHandler.mComparisons, "animelem", animElemComparisonFunction);
	string_map_push(&gVariableHandler.mComparisons, "timemod", timeModComparisonFunction);
	string_map_push(&gVariableHandler.mComparisons, "teammode", teamModeComparisonFunction);
	string_map_push(&gVariableHandler.mComparisons, "hitdefattr", hitDefAttributeComparisonFunction);
}

static AssignmentReturnValue aiLevelFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerAILevel(tPlayer)); }
static AssignmentReturnValue aliveFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(isPlayerAlive(tPlayer)); }
static AssignmentReturnValue animFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerAnimationNumber(tPlayer)); }
//static AssignmentReturnValue animElemFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(0); }
static AssignmentReturnValue animTimeFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getRemainingPlayerAnimationTime(tPlayer)); }
static AssignmentReturnValue authorNameFunction(DreamPlayer* tPlayer) { return makeStringAssignmentReturn(getPlayerAuthorName(tPlayer)); }
static AssignmentReturnValue backEdgeFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerScreenEdgeInBackX(tPlayer)); }
static AssignmentReturnValue backEdgeBodyDistFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerBackBodyDistanceToScreen(tPlayer)); }
static AssignmentReturnValue backEdgeDistFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerBackAxisDistanceToScreen(tPlayer)); }
static AssignmentReturnValue bottomEdgeFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getDreamStageBottomEdgeY(getPlayerCoordinateP(tPlayer))); }
static AssignmentReturnValue cameraPosXFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getDreamCameraPositionX(getPlayerCoordinateP(tPlayer))); }
static AssignmentReturnValue cameraPosYFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getDreamCameraPositionY(getPlayerCoordinateP(tPlayer))); }
static AssignmentReturnValue cameraZoomFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getDreamCameraZoom(getPlayerCoordinateP(tPlayer))); }
static AssignmentReturnValue canRecoverFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(canPlayerRecoverFromFalling(tPlayer)); }
//static AssignmentReturnValue commandFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(0); }
static AssignmentReturnValue ctrlFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(getPlayerControl(tPlayer)); }
static AssignmentReturnValue drawGameFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(hasPlayerDrawn(tPlayer)); }
static AssignmentReturnValue eFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(M_E); }
static AssignmentReturnValue facingFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerIsFacingRight(tPlayer) ? 1 : -1); }
static AssignmentReturnValue frontEdgeFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerScreenEdgeInFrontX(tPlayer)); }
static AssignmentReturnValue frontEdgeBodyDistFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerFrontBodyDistanceToScreen(tPlayer)); }
static AssignmentReturnValue frontEdgeDistFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerFrontAxisDistanceToScreen(tPlayer)); }
static AssignmentReturnValue gameHeightFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getDreamGameHeight(getPlayerCoordinateP(tPlayer))); }
static AssignmentReturnValue gameTimeFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getDreamGameTime()); }
static AssignmentReturnValue gameWidthFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getDreamGameWidth(getPlayerCoordinateP(tPlayer))); }
static AssignmentReturnValue hitCountFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerHitCount(tPlayer)); }
//static AssignmentReturnValue hitDefAttrFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(0); }
static AssignmentReturnValue hitFallFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(isPlayerFalling(tPlayer)); }
static AssignmentReturnValue hitOverFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(isPlayerHitOver(tPlayer)); }
static AssignmentReturnValue hitPauseTimeFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerTimeLeftInHitPause(tPlayer)); }
static AssignmentReturnValue hitShakeOverFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(isPlayerHitShakeOver(tPlayer));  }
static AssignmentReturnValue hitVelXFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerHitVelocityX(tPlayer, getPlayerCoordinateP(tPlayer))); }
static AssignmentReturnValue hitVelYFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerHitVelocityY(tPlayer, getPlayerCoordinateP(tPlayer))); }
static AssignmentReturnValue idFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(tPlayer->mID); }
static AssignmentReturnValue inGuardDistFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(isPlayerInGuardDistance(tPlayer)); }
static AssignmentReturnValue isHelperFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(isPlayerHelper(tPlayer)); }
static AssignmentReturnValue isHomeTeamFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(isPlayerHomeTeam(tPlayer)); }
static AssignmentReturnValue leftEdgeFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getDreamStageLeftEdgeX(getPlayerCoordinateP(tPlayer))); }
static AssignmentReturnValue lifeFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerLife(tPlayer)); }
static AssignmentReturnValue lifeMaxFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerLifeMax(tPlayer)); }
static AssignmentReturnValue loseFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(hasPlayerLost(tPlayer)); }
static AssignmentReturnValue matchNoFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getDreamMatchNumber()); }
static AssignmentReturnValue matchOverFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(isDreamMatchOver()); }
static AssignmentReturnValue moveContactFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerMoveContactCounter(tPlayer)); }
static AssignmentReturnValue moveGuardedFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerMoveGuarded(tPlayer)); }
static AssignmentReturnValue moveHitFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerMoveHit(tPlayer)); }
//static AssignmentReturnValue moveTypeFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(0); }
static AssignmentReturnValue moveReversedFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(hasPlayerMoveBeenReversedByOtherPlayer(tPlayer)); }
static AssignmentReturnValue nameFunction(DreamPlayer* tPlayer) { return makeStringAssignmentReturn(getPlayerName(tPlayer)); }
static AssignmentReturnValue numEnemyFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(1); } // TODO: fix once teams exist
static AssignmentReturnValue numExplodFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getExplodAmount(tPlayer)); }
static AssignmentReturnValue numHelperFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerHelperAmount(tPlayer)); }
static AssignmentReturnValue numPartnerFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(0); } // TODO: fix when partners are implemented
static AssignmentReturnValue numProjFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerProjectileAmount(tPlayer)); }
static AssignmentReturnValue numTargetFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerTargetAmount(tPlayer)); }
static AssignmentReturnValue p1NameFunction(DreamPlayer* tPlayer) { return nameFunction(tPlayer); }
static AssignmentReturnValue p2BodyDistFunctionX(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerDistanceToFrontOfOtherPlayerX(tPlayer)); }
static AssignmentReturnValue p2BodyDistFunctionY(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerAxisDistanceY(tPlayer)); }
static AssignmentReturnValue p2DistFunctionX(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerAxisDistanceX(tPlayer)); }
static AssignmentReturnValue p2DistFunctionY(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerAxisDistanceY(tPlayer)); }
static AssignmentReturnValue p2LifeFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerLife(getPlayerOtherPlayer(tPlayer))); }
//static AssignmentReturnValue p2MoveTypeFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(0); }
static AssignmentReturnValue p2NameFunction(DreamPlayer* tPlayer) { return makeStringAssignmentReturn(getPlayerName(getPlayerOtherPlayer(tPlayer))); }
static AssignmentReturnValue p2StateNoFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerState(getPlayerOtherPlayer(tPlayer))); }
//static AssignmentReturnValue p2StateTypeFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(0); }
static AssignmentReturnValue p3NameFunction(DreamPlayer* tPlayer) { return makeStringAssignmentReturn(""); } // TODO: after teams work
static AssignmentReturnValue p4NameFunction(DreamPlayer* tPlayer) { return makeStringAssignmentReturn(""); } // TODO: after teams work
static AssignmentReturnValue palNoFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerPaletteNumber(tPlayer)); }
static AssignmentReturnValue parentDistXFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerDistanceToParentX(tPlayer)); }
static AssignmentReturnValue parentDistYFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerDistanceToParentY(tPlayer)); }
static AssignmentReturnValue piFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(M_PI); }
static AssignmentReturnValue posXFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerPositionBasedOnScreenCenterX(tPlayer, getPlayerCoordinateP(tPlayer))); }
static AssignmentReturnValue posYFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerPositionBasedOnStageFloorY(tPlayer, getPlayerCoordinateP(tPlayer))); }
static AssignmentReturnValue powerFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerPower(tPlayer)); }
static AssignmentReturnValue powerMaxFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerPowerMax(tPlayer)); }
static AssignmentReturnValue prevStateNoFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerPreviousState(tPlayer)); }
//static AssignmentReturnValue projContactFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(0); }
//static AssignmentReturnValue projGuardedFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(0); }
//static AssignmentReturnValue projHitFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(0); }
static AssignmentReturnValue randomFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(randfromInteger(0, 999)); }
static AssignmentReturnValue rightEdgeFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getDreamStageRightEdgeX(getPlayerCoordinateP(tPlayer))); }
static AssignmentReturnValue rootDistXFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerDistanceToRootX(tPlayer)); }
static AssignmentReturnValue rootDistYFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerDistanceToRootY(tPlayer)); }
static AssignmentReturnValue roundNoFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getDreamRoundNumber()); }
static AssignmentReturnValue roundsExistedFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerRoundsExisted(tPlayer)); }
static AssignmentReturnValue roundStateFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getDreamRoundStateNumber()); }
static AssignmentReturnValue screenPosXFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerScreenPositionX(tPlayer, getPlayerCoordinateP(tPlayer))); }
static AssignmentReturnValue screenPosYFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerScreenPositionY(tPlayer, getPlayerCoordinateP(tPlayer))); }
static AssignmentReturnValue screenHeightFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getDreamScreenHeight(getPlayerCoordinateP(tPlayer))); }
static AssignmentReturnValue screenWidthFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getDreamScreenWidth(getPlayerCoordinateP(tPlayer))); }
static AssignmentReturnValue stateNoFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerState(tPlayer)); }
//static AssignmentReturnValue stateTypeFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(0); }
//static AssignmentReturnValue teamModeFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(0); }
static AssignmentReturnValue teamSideFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(tPlayer->mRootID + 1); }
static AssignmentReturnValue ticksPerSecondFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getDreamTicksPerSecond()); }
static AssignmentReturnValue timeFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerTimeInState(tPlayer)); }
//static AssignmentReturnValue timeModFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(0); }
static AssignmentReturnValue topEdgeFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getDreamStageTopEdgeY(getPlayerCoordinateP(tPlayer))); }
static AssignmentReturnValue uniqHitCountFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerUniqueHitCount(tPlayer)); }
static AssignmentReturnValue velXFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerVelocityX(tPlayer, getPlayerCoordinateP(tPlayer))); }
static AssignmentReturnValue velYFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerVelocityY(tPlayer, getPlayerCoordinateP(tPlayer))); }
static AssignmentReturnValue winFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(hasPlayerWon(tPlayer)); }



static void setupVariableAssignments() {
	gVariableHandler.mVariables = new_string_map();
	string_map_push(&gVariableHandler.mVariables, "ailevel", aiLevelFunction);
	string_map_push(&gVariableHandler.mVariables, "alive", aliveFunction);
	string_map_push(&gVariableHandler.mVariables, "anim", animFunction);
	string_map_push(&gVariableHandler.mVariables, "animtime", animTimeFunction);
	string_map_push(&gVariableHandler.mVariables, "authorname", authorNameFunction);

	string_map_push(&gVariableHandler.mVariables, "backedge", backEdgeFunction);
	string_map_push(&gVariableHandler.mVariables, "backedgebodydist", backEdgeBodyDistFunction);
	string_map_push(&gVariableHandler.mVariables, "backedgedist", backEdgeDistFunction);
	string_map_push(&gVariableHandler.mVariables, "bottomedge", bottomEdgeFunction);

	string_map_push(&gVariableHandler.mVariables, "camerapos x", cameraPosXFunction);
	string_map_push(&gVariableHandler.mVariables, "camerapos y", cameraPosYFunction);
	string_map_push(&gVariableHandler.mVariables, "camerazoom", cameraZoomFunction);
	string_map_push(&gVariableHandler.mVariables, "canrecover", canRecoverFunction);
	string_map_push(&gVariableHandler.mVariables, "ctrl", ctrlFunction);

	string_map_push(&gVariableHandler.mVariables, "drawgame", drawGameFunction);

	string_map_push(&gVariableHandler.mVariables, "e", eFunction);

	string_map_push(&gVariableHandler.mVariables, "facing", facingFunction);
	string_map_push(&gVariableHandler.mVariables, "frontedge", frontEdgeFunction);
	string_map_push(&gVariableHandler.mVariables, "frontedgebodydist", frontEdgeBodyDistFunction);
	string_map_push(&gVariableHandler.mVariables, "frontedgedist", frontEdgeDistFunction);

	string_map_push(&gVariableHandler.mVariables, "gameheight", gameHeightFunction);
	string_map_push(&gVariableHandler.mVariables, "gametime", gameTimeFunction);
	string_map_push(&gVariableHandler.mVariables, "gamewidth", gameWidthFunction);

	string_map_push(&gVariableHandler.mVariables, "hitcount", hitCountFunction);
	string_map_push(&gVariableHandler.mVariables, "hitfall", hitFallFunction);
	string_map_push(&gVariableHandler.mVariables, "hitover", hitOverFunction);
	string_map_push(&gVariableHandler.mVariables, "hitpausetime", hitPauseTimeFunction);
	string_map_push(&gVariableHandler.mVariables, "hitshakeover", hitShakeOverFunction);
	string_map_push(&gVariableHandler.mVariables, "hitvel x", hitVelXFunction);
	string_map_push(&gVariableHandler.mVariables, "hitvel y", hitVelYFunction);

	string_map_push(&gVariableHandler.mVariables, "id", idFunction);
	string_map_push(&gVariableHandler.mVariables, "inguarddist", inGuardDistFunction);
	string_map_push(&gVariableHandler.mVariables, "ishelper", isHelperFunction);
	string_map_push(&gVariableHandler.mVariables, "ishometeam", isHomeTeamFunction);

	string_map_push(&gVariableHandler.mVariables, "leftedge", leftEdgeFunction);
	string_map_push(&gVariableHandler.mVariables, "life", lifeFunction);
	string_map_push(&gVariableHandler.mVariables, "lifemax", lifeMaxFunction);
	string_map_push(&gVariableHandler.mVariables, "lose", loseFunction);

	string_map_push(&gVariableHandler.mVariables, "matchno", matchNoFunction);
	string_map_push(&gVariableHandler.mVariables, "matchover", matchOverFunction);
	string_map_push(&gVariableHandler.mVariables, "movecontact", moveContactFunction);
	string_map_push(&gVariableHandler.mVariables, "moveguarded", moveGuardedFunction);
	string_map_push(&gVariableHandler.mVariables, "movehit", moveHitFunction);
	string_map_push(&gVariableHandler.mVariables, "movereversed", moveReversedFunction);

	string_map_push(&gVariableHandler.mVariables, "name", nameFunction);
	string_map_push(&gVariableHandler.mVariables, "numenemy", numEnemyFunction);
	string_map_push(&gVariableHandler.mVariables, "numexplod", numExplodFunction);
	string_map_push(&gVariableHandler.mVariables, "numhelper", numHelperFunction);
	string_map_push(&gVariableHandler.mVariables, "numpartner", numPartnerFunction);
	string_map_push(&gVariableHandler.mVariables, "numproj", numProjFunction);
	string_map_push(&gVariableHandler.mVariables, "numtarget", numTargetFunction);

	string_map_push(&gVariableHandler.mVariables, "p1name", p1NameFunction);
	string_map_push(&gVariableHandler.mVariables, "p2bodydist x", p2BodyDistFunctionX);
	string_map_push(&gVariableHandler.mVariables, "p2bodydist y", p2BodyDistFunctionY);
	string_map_push(&gVariableHandler.mVariables, "p2dist x", p2DistFunctionX);
	string_map_push(&gVariableHandler.mVariables, "p2dist y", p2DistFunctionY);
	string_map_push(&gVariableHandler.mVariables, "p2life", p2LifeFunction);
	string_map_push(&gVariableHandler.mVariables, "p2name", p2NameFunction);
	string_map_push(&gVariableHandler.mVariables, "p2stateno", p2StateNoFunction);
	string_map_push(&gVariableHandler.mVariables, "p3name", p3NameFunction);
	string_map_push(&gVariableHandler.mVariables, "p4name", p4NameFunction);
	string_map_push(&gVariableHandler.mVariables, "palno", palNoFunction);
	string_map_push(&gVariableHandler.mVariables, "parentdist x", parentDistXFunction);
	string_map_push(&gVariableHandler.mVariables, "parentdist y", parentDistYFunction);
	string_map_push(&gVariableHandler.mVariables, "pi", piFunction);
	string_map_push(&gVariableHandler.mVariables, "pos x", posXFunction);
	string_map_push(&gVariableHandler.mVariables, "pos y", posYFunction);
	string_map_push(&gVariableHandler.mVariables, "power", powerFunction);
	string_map_push(&gVariableHandler.mVariables, "powermax", powerMaxFunction);
	string_map_push(&gVariableHandler.mVariables, "prevstateno", prevStateNoFunction);

	string_map_push(&gVariableHandler.mVariables, "random", randomFunction);
	string_map_push(&gVariableHandler.mVariables, "rightedge", rightEdgeFunction);
	string_map_push(&gVariableHandler.mVariables, "rootdist x", rootDistXFunction);
	string_map_push(&gVariableHandler.mVariables, "rootdist y", rootDistYFunction);
	string_map_push(&gVariableHandler.mVariables, "roundno", roundNoFunction);
	string_map_push(&gVariableHandler.mVariables, "roundsexisted", roundsExistedFunction);
	string_map_push(&gVariableHandler.mVariables, "roundstate", roundStateFunction);

	string_map_push(&gVariableHandler.mVariables, "screenpos x", screenPosXFunction);
	string_map_push(&gVariableHandler.mVariables, "screenpos y", screenPosYFunction);
	string_map_push(&gVariableHandler.mVariables, "screenheight", screenHeightFunction);
	string_map_push(&gVariableHandler.mVariables, "screenwidth", screenWidthFunction);
	string_map_push(&gVariableHandler.mVariables, "stateno", stateNoFunction);
	string_map_push(&gVariableHandler.mVariables, "statetime", timeFunction);

	string_map_push(&gVariableHandler.mVariables, "teamside", teamSideFunction);
	string_map_push(&gVariableHandler.mVariables, "tickspersecond", ticksPerSecondFunction);
	string_map_push(&gVariableHandler.mVariables, "time", timeFunction);
	string_map_push(&gVariableHandler.mVariables, "topedge", topEdgeFunction);

	string_map_push(&gVariableHandler.mVariables, "uniquehitcount", uniqHitCountFunction);

	string_map_push(&gVariableHandler.mVariables, "vel x", velXFunction);
	string_map_push(&gVariableHandler.mVariables, "vel y", velYFunction);

	string_map_push(&gVariableHandler.mVariables, "win", winFunction);
}


static AssignmentReturnValue dataLifeFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerDataLife(tPlayer)); }
static AssignmentReturnValue dataPowerFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerPowerMax(tPlayer)); }
static AssignmentReturnValue dataAttackFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerDataAttack(tPlayer)); }
static AssignmentReturnValue dataDefenceFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerDataDefense(tPlayer)); }
static AssignmentReturnValue dataFallDefenceMultiplierFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerFallDefenseMultiplier(tPlayer)); }
static AssignmentReturnValue dataLiedownTimeFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerDataLiedownTime(tPlayer)); }
static AssignmentReturnValue dataAirjuggleFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerDataAirjuggle(tPlayer)); }
static AssignmentReturnValue dataSparkNoFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerDataSparkNo(tPlayer)); }
static AssignmentReturnValue dataGuardSparkNoFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerDataGuardSparkNo(tPlayer)); }
static AssignmentReturnValue dataKOEchoFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerDataKOEcho(tPlayer)); }
static AssignmentReturnValue dataIntPersistIndexFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerDataIntPersistIndex(tPlayer)); }
static AssignmentReturnValue dataFloatPersistIndexFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerDataFloatPersistIndex(tPlayer)); }

static AssignmentReturnValue sizeXScaleFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerScaleX(tPlayer)); }
static AssignmentReturnValue sizeYScaleFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerScaleY(tPlayer)); }
static AssignmentReturnValue sizeGroundBackFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerGroundSizeBack(tPlayer)); }
static AssignmentReturnValue sizeGroundFrontFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerGroundSizeFront(tPlayer)); }
static AssignmentReturnValue sizeAirBackFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerSizeAirBack(tPlayer)); }
static AssignmentReturnValue sizeAirFrontFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerSizeAirFront(tPlayer)); }
static AssignmentReturnValue sizeHeightFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerHeight(tPlayer)); }
static AssignmentReturnValue sizeAttackDistFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerSizeAttackDist(tPlayer)); }
static AssignmentReturnValue sizeProjAttackDistFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerSizeProjectileAttackDist(tPlayer)); }
static AssignmentReturnValue sizeProjDoScaleFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerSizeProjectilesDoScale(tPlayer)); }
static AssignmentReturnValue sizeHeadPosXFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerHeadPositionX(tPlayer)); }
static AssignmentReturnValue sizeHeadPosYFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerHeadPositionY(tPlayer)); }
static AssignmentReturnValue sizeMidPosXFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerMiddlePositionX(tPlayer)); }
static AssignmentReturnValue sizeMidPosYFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerMiddlePositionY(tPlayer)); }
static AssignmentReturnValue sizeShadowOffsetFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerSizeShadowOffset(tPlayer)); }
static AssignmentReturnValue sizeDrawOffsetXFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerSizeDrawOffsetX(tPlayer)); }
static AssignmentReturnValue sizeDrawOffsetYFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerSizeDrawOffsetY(tPlayer)); }

static AssignmentReturnValue velocityWalkFwdXFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerForwardWalkVelocityX(tPlayer)); }
static AssignmentReturnValue velocityWalkBackXFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerBackwardWalkVelocityX(tPlayer)); }
static AssignmentReturnValue velocityRunFwdXFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerForwardRunVelocityX(tPlayer)); }
static AssignmentReturnValue velocityRunFwdYFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerForwardRunVelocityY(tPlayer)); }
static AssignmentReturnValue velocityRunBackXFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerBackwardRunVelocityX(tPlayer)); }
static AssignmentReturnValue velocityRunBackYFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerBackwardRunVelocityY(tPlayer));; }
static AssignmentReturnValue velocityJumpYFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerJumpVelocityY(tPlayer)); }
static AssignmentReturnValue velocityJumpNeuXFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerNeutralJumpVelocityX(tPlayer)); }
static AssignmentReturnValue velocityJumpBackXFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerBackwardJumpVelocityX(tPlayer)); }
static AssignmentReturnValue velocityJumpFwdXFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerForwardJumpVelocityX(tPlayer)); }
static AssignmentReturnValue velocityRunJumpBackXFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerBackwardRunJumpVelocityX(tPlayer)); }
static AssignmentReturnValue velocityRunJumpFwdXFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerForwardRunJumpVelocityX(tPlayer)); }
static AssignmentReturnValue velocityAirJumpYFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerAirJumpVelocityY(tPlayer)); }
static AssignmentReturnValue velocityAirJumpNeuXFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerNeutralAirJumpVelocityX(tPlayer)); }
static AssignmentReturnValue velocityAirJumpBackXFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerBackwardAirJumpVelocityX(tPlayer)); }
static AssignmentReturnValue velocityAirJumpFwdXFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerForwardAirJumpVelocityX(tPlayer)); }
static AssignmentReturnValue velocityAirGetHitGroundRecoverXFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerVelocityAirGetHitGroundRecoverX(tPlayer)); }
static AssignmentReturnValue velocityAirGetHitGroundRecoverYFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerVelocityAirGetHitGroundRecoverY(tPlayer)); }
static AssignmentReturnValue velocityAirGetHitAirRecoverMulXFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerVelocityAirGetHitAirRecoverMulX(tPlayer)); }
static AssignmentReturnValue velocityAirGetHitAirRecoverMulYFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerVelocityAirGetHitAirRecoverMulY(tPlayer)); }
static AssignmentReturnValue velocityAirGetHitAirRecoverAddXFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerVelocityAirGetHitAirRecoverAddX(tPlayer)); }
static AssignmentReturnValue velocityAirGetHitAirRecoverAddYFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerVelocityAirGetHitAirRecoverAddY(tPlayer)); }
static AssignmentReturnValue velocityAirGetHitAirRecoverBackFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerVelocityAirGetHitAirRecoverBack(tPlayer)); }
static AssignmentReturnValue velocityAirGetHitAirRecoverFwdFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerVelocityAirGetHitAirRecoverFwd(tPlayer)); }
static AssignmentReturnValue velocityAirGetHitAirRecoverUpFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerVelocityAirGetHitAirRecoverUp(tPlayer)); }
static AssignmentReturnValue velocityAirGetHitAirRecoverDownFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerVelocityAirGetHitAirRecoverDown(tPlayer)); }

static AssignmentReturnValue movementAirJumpNumFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerMovementAirJumpNum(tPlayer)); }
static AssignmentReturnValue movementAirJumpHeightFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerMovementAirJumpHeight(tPlayer)); }
static AssignmentReturnValue movementYAccelFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerVerticalAcceleration(tPlayer)); }
static AssignmentReturnValue movementStandFrictionFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerStandFriction(tPlayer)); }
static AssignmentReturnValue movementCrouchFrictionFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerCrouchFriction(tPlayer)); }
static AssignmentReturnValue movementStandFrictionThresholdFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerStandFrictionThreshold(tPlayer)); }
static AssignmentReturnValue movementCrouchFrictionThresholdFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerCrouchFrictionThreshold(tPlayer)); }
static AssignmentReturnValue movementJumpChangeAnimThresholdFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerMovementJumpChangeAnimThreshold(tPlayer)); }
static AssignmentReturnValue movementAirGetHitGroundLevelFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerAirGetHitGroundLevelY(tPlayer)); }
static AssignmentReturnValue movementAirGetHitGroundRecoverGroundThresholdFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerAirGetHitGroundRecoveryGroundYTheshold(tPlayer)); }
static AssignmentReturnValue movementAirGetHitGroundRecoverGroundLevelFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerAirGetHitGroundRecoveryGroundLevelY(tPlayer)); }
static AssignmentReturnValue movementAirGetHitAirRecoverThresholdFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerAirGetHitAirRecoveryVelocityYThreshold(tPlayer)); }
static AssignmentReturnValue movementAirGetHitAirRecoverYAccelFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerMovementAirGetHitAirRecoverYAccel(tPlayer)); }
static AssignmentReturnValue movementAirGetHitTripGroundLevelFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerAirGetHitTripGroundLevelY(tPlayer)); }
static AssignmentReturnValue movementDownBounceOffsetXFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerDownBounceOffsetX(tPlayer, getPlayerCoordinateP(tPlayer))); }
static AssignmentReturnValue movementDownBounceOffsetYFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerDownBounceOffsetY(tPlayer, getPlayerCoordinateP(tPlayer))); }
static AssignmentReturnValue movementDownBounceYAccelFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerDownVerticalBounceAcceleration(tPlayer)); }
static AssignmentReturnValue movementDownBounceGroundLevelFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerDownBounceGroundLevel(tPlayer, getPlayerCoordinateP(tPlayer))); }
static AssignmentReturnValue movementDownFrictionThresholdFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerLyingDownFrictionThreshold(tPlayer)); }

static void setupConstantAssignments() { 
	gVariableHandler.mConstants = new_string_map();
	string_map_push(&gVariableHandler.mConstants, "data.life", dataLifeFunction);
	string_map_push(&gVariableHandler.mConstants, "data.power", dataPowerFunction);
	string_map_push(&gVariableHandler.mConstants, "data.attack", dataAttackFunction);
	string_map_push(&gVariableHandler.mConstants, "data.defence", dataDefenceFunction);
	string_map_push(&gVariableHandler.mConstants, "data.fall.defence_mul", dataFallDefenceMultiplierFunction);
	string_map_push(&gVariableHandler.mConstants, "data.liedown.time", dataLiedownTimeFunction);
	string_map_push(&gVariableHandler.mConstants, "data.airjuggle", dataAirjuggleFunction);
	string_map_push(&gVariableHandler.mConstants, "data.sparkno", dataSparkNoFunction);
	string_map_push(&gVariableHandler.mConstants, "data.guard.sparkno", dataGuardSparkNoFunction);
	string_map_push(&gVariableHandler.mConstants, "data.ko.echo", dataKOEchoFunction);
	string_map_push(&gVariableHandler.mConstants, "data.intpersistindex", dataIntPersistIndexFunction);
	string_map_push(&gVariableHandler.mConstants, "data.floatpersistindex", dataFloatPersistIndexFunction);
	
	string_map_push(&gVariableHandler.mConstants, "size.xscale", sizeXScaleFunction);
	string_map_push(&gVariableHandler.mConstants, "size.yscale", sizeYScaleFunction);
	string_map_push(&gVariableHandler.mConstants, "size.ground.back", sizeGroundBackFunction);
	string_map_push(&gVariableHandler.mConstants, "size.ground.front", sizeGroundFrontFunction);
	string_map_push(&gVariableHandler.mConstants, "size.air.back", sizeAirBackFunction);
	string_map_push(&gVariableHandler.mConstants, "size.air.front", sizeAirFrontFunction);
	string_map_push(&gVariableHandler.mConstants, "size.height", sizeHeightFunction);
	string_map_push(&gVariableHandler.mConstants, "size.attack.dist", sizeAttackDistFunction);
	string_map_push(&gVariableHandler.mConstants, "size.proj.attack.dist", sizeProjAttackDistFunction);
	string_map_push(&gVariableHandler.mConstants, "size.proj.doscale", sizeProjDoScaleFunction);
	string_map_push(&gVariableHandler.mConstants, "size.head.pos.x", sizeHeadPosXFunction);
	string_map_push(&gVariableHandler.mConstants, "size.head.pos.y", sizeHeadPosYFunction);
	string_map_push(&gVariableHandler.mConstants, "size.mid.pos.x", sizeMidPosXFunction);
	string_map_push(&gVariableHandler.mConstants, "size.mid.pos.y", sizeMidPosYFunction);
	string_map_push(&gVariableHandler.mConstants, "size.shadowoffset", sizeShadowOffsetFunction);
	string_map_push(&gVariableHandler.mConstants, "size.draw.offset.x", sizeDrawOffsetXFunction);
	string_map_push(&gVariableHandler.mConstants, "size.draw.offset.y", sizeDrawOffsetYFunction);

	string_map_push(&gVariableHandler.mConstants, "velocity.walk.fwd.x", velocityWalkFwdXFunction);
	string_map_push(&gVariableHandler.mConstants, "velocity.walk.back.x", velocityWalkBackXFunction);
	string_map_push(&gVariableHandler.mConstants, "velocity.run.fwd.x", velocityRunFwdXFunction);
	string_map_push(&gVariableHandler.mConstants, "velocity.run.fwd.y", velocityRunFwdYFunction);
	string_map_push(&gVariableHandler.mConstants, "velocity.run.back.x", velocityRunBackXFunction);
	string_map_push(&gVariableHandler.mConstants, "velocity.run.back.y", velocityRunBackYFunction);
	string_map_push(&gVariableHandler.mConstants, "velocity.jump.y", velocityJumpYFunction);
	string_map_push(&gVariableHandler.mConstants, "velocity.jump.neu.x", velocityJumpNeuXFunction);
	string_map_push(&gVariableHandler.mConstants, "velocity.jump.back.x", velocityJumpBackXFunction);
	string_map_push(&gVariableHandler.mConstants, "velocity.jump.fwd.x", velocityJumpFwdXFunction);
	string_map_push(&gVariableHandler.mConstants, "velocity.runjump.back.x", velocityRunJumpBackXFunction);
	string_map_push(&gVariableHandler.mConstants, "velocity.runjump.fwd.x", velocityRunJumpFwdXFunction);
	string_map_push(&gVariableHandler.mConstants, "velocity.airjump.y", velocityAirJumpYFunction);
	string_map_push(&gVariableHandler.mConstants, "velocity.airjump.neu.x", velocityAirJumpNeuXFunction);
	string_map_push(&gVariableHandler.mConstants, "velocity.airjump.back.x", velocityAirJumpBackXFunction);
	string_map_push(&gVariableHandler.mConstants, "velocity.airjump.fwd.x", velocityAirJumpFwdXFunction);
	string_map_push(&gVariableHandler.mConstants, "velocity.air.gethit.groundrecover.x", velocityAirGetHitGroundRecoverXFunction);
	string_map_push(&gVariableHandler.mConstants, "velocity.air.gethit.groundrecover.y", velocityAirGetHitGroundRecoverYFunction);
	string_map_push(&gVariableHandler.mConstants, "velocity.air.gethit.airrecover.mul.x", velocityAirGetHitAirRecoverMulXFunction);
	string_map_push(&gVariableHandler.mConstants, "velocity.air.gethit.airrecover.mul.y", velocityAirGetHitAirRecoverMulYFunction);
	string_map_push(&gVariableHandler.mConstants, "velocity.air.gethit.airrecover.add.x", velocityAirGetHitAirRecoverAddXFunction);
	string_map_push(&gVariableHandler.mConstants, "velocity.air.gethit.airrecover.add.y", velocityAirGetHitAirRecoverAddYFunction);
	string_map_push(&gVariableHandler.mConstants, "velocity.air.gethit.airrecover.back", velocityAirGetHitAirRecoverBackFunction);
	string_map_push(&gVariableHandler.mConstants, "velocity.air.gethit.airrecover.fwd", velocityAirGetHitAirRecoverFwdFunction);
	string_map_push(&gVariableHandler.mConstants, "velocity.air.gethit.airrecover.up", velocityAirGetHitAirRecoverUpFunction);
	string_map_push(&gVariableHandler.mConstants, "velocity.air.gethit.airrecover.down", velocityAirGetHitAirRecoverDownFunction);

	string_map_push(&gVariableHandler.mConstants, "movement.airjump.num", movementAirJumpNumFunction);
	string_map_push(&gVariableHandler.mConstants, "movement.airjump.height", movementAirJumpHeightFunction);
	string_map_push(&gVariableHandler.mConstants, "movement.yaccel", movementYAccelFunction);
	string_map_push(&gVariableHandler.mConstants, "movement.stand.friction", movementStandFrictionFunction);
	string_map_push(&gVariableHandler.mConstants, "movement.crouch.friction", movementCrouchFrictionFunction);
	string_map_push(&gVariableHandler.mConstants, "movement.stand.friction.threshold", movementStandFrictionThresholdFunction);
	string_map_push(&gVariableHandler.mConstants, "movement.crouch.friction.threshold", movementCrouchFrictionThresholdFunction);
	string_map_push(&gVariableHandler.mConstants, "movement.jump.changeanim.threshold", movementJumpChangeAnimThresholdFunction);
	string_map_push(&gVariableHandler.mConstants, "movement.air.gethit.groundlevel", movementAirGetHitGroundLevelFunction);
	string_map_push(&gVariableHandler.mConstants, "movement.air.gethit.groundrecover.ground.threshold", movementAirGetHitGroundRecoverGroundThresholdFunction);
	string_map_push(&gVariableHandler.mConstants, "movement.air.gethit.groundrecover.groundlevel", movementAirGetHitGroundRecoverGroundLevelFunction);
	string_map_push(&gVariableHandler.mConstants, "movement.air.gethit.airrecover.threshold", movementAirGetHitAirRecoverThresholdFunction);
	string_map_push(&gVariableHandler.mConstants, "movement.air.gethit.airrecover.yaccel", movementAirGetHitAirRecoverYAccelFunction);
	string_map_push(&gVariableHandler.mConstants, "movement.air.gethit.trip.groundlevel", movementAirGetHitTripGroundLevelFunction);
	string_map_push(&gVariableHandler.mConstants, "movement.down.bounce.offset.x", movementDownBounceOffsetXFunction);
	string_map_push(&gVariableHandler.mConstants, "movement.down.bounce.offset.y", movementDownBounceOffsetYFunction);
	string_map_push(&gVariableHandler.mConstants, "movement.down.bounce.yaccel", movementDownBounceYAccelFunction);
	string_map_push(&gVariableHandler.mConstants, "movement.down.bounce.groundlevel", movementDownBounceGroundLevelFunction);
	string_map_push(&gVariableHandler.mConstants, "movement.down.friction.threshold", movementDownFrictionThresholdFunction);
}


static void setupArrayAssignments();

void setupDreamAssignmentEvaluator() {
	setupVariableAssignments();
	setupConstantAssignments();
	setupArrayAssignments();
	setupComparisons();
}

static int isIsInOtherFileVariable(char* tName) {
	int len = strlen(tName);
	if (len < 2) return 0;

	int hasFlag = tName[0] == 's' || tName[0] == 'f';
	if (!hasFlag) return 0;

	int i;
	for (i = 1; i < len; i++) {
		if (tName[i] < '0' || tName[i] > '9') return 0;
	}

	return 1;
}

static AssignmentReturnValue makeExternalFileAssignmentReturn(char tIdentifierCharacter, char* tValueString) {
	char buffer[100];
	sprintf(buffer, "isinotherfile%c %s", tIdentifierCharacter, tValueString);
	return makeStringAssignmentReturn(buffer);
}


static AssignmentReturnValue evaluateVariableAssignment(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* tIsStatic) {
	DreamMugenVariableAssignment* variable = (DreamMugenVariableAssignment*)*tAssignment;
	char testString[100];
	strcpy(testString, variable->mName);
	turnStringLowercase(testString);
	*tIsStatic = 0;

	if (string_map_contains(&gVariableHandler.mVariables, testString)) {
		VariableFunction func = (VariableFunction)string_map_get(&gVariableHandler.mVariables, testString);
		return func(tPlayer);
	}
	
	if (isIsInOtherFileVariable(testString)) { // TODO: fix
		return makeExternalFileAssignmentReturn(testString[0], testString + 1);
	}
	
	return makeStringAssignmentReturn(testString);
}

static AssignmentReturnValue evaluateVarArrayAssignment(AssignmentReturnValue tIndex, DreamPlayer* tPlayer, int* tIsStatic) {
	int id = convertAssignmentReturnToNumber(tIndex);
	int val = getPlayerVariable(tPlayer, id);

	*tIsStatic = 0;
	return makeNumberAssignmentReturn(val);
}

static AssignmentReturnValue evaluateSysVarArrayAssignment(AssignmentReturnValue tIndex, DreamPlayer* tPlayer, int* tIsStatic) {
	int id = convertAssignmentReturnToNumber(tIndex);
	int val = getPlayerSystemVariable(tPlayer, id);

	*tIsStatic = 0;
	return makeNumberAssignmentReturn(val);
}

static AssignmentReturnValue evaluateFVarArrayAssignment(AssignmentReturnValue tIndex, DreamPlayer* tPlayer, int* tIsStatic) {
	int id = convertAssignmentReturnToNumber(tIndex);
	double val = getPlayerFloatVariable(tPlayer, id);

	*tIsStatic = 0;
	return makeFloatAssignmentReturn(val);
}

static AssignmentReturnValue evaluateSysFVarArrayAssignment(AssignmentReturnValue tIndex, DreamPlayer* tPlayer, int* tIsStatic) {
	int id = convertAssignmentReturnToNumber(tIndex);
	double val = getPlayerSystemFloatVariable(tPlayer, id);

	*tIsStatic = 0;
	return makeFloatAssignmentReturn(val);
}

static AssignmentReturnValue evaluateStageVarArrayAssignment(AssignmentReturnValue tIndex, DreamPlayer* tPlayer, int* tIsStatic) {
	char* var = convertAssignmentReturnToAllocatedString(tIndex);
	turnStringLowercase(var);

	AssignmentReturnValue ret;
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

	freeMemory(var);

	(void)tIsStatic; // TODO: check
	return ret;
}

static AssignmentReturnValue evaluateAbsArrayAssignment(AssignmentReturnValue tIndex) {
	if (isFloatReturn(tIndex)) {
		double val = convertAssignmentReturnToFloat(tIndex);
		return makeFloatAssignmentReturn(fabs(val));
	}
	else {
		int val = convertAssignmentReturnToNumber(tIndex);
		return makeNumberAssignmentReturn(abs(val));
	}
}

static AssignmentReturnValue evaluateExpArrayAssignment(AssignmentReturnValue tIndex) {
		double val = convertAssignmentReturnToFloat(tIndex);
		return makeFloatAssignmentReturn(exp(val));
}

static AssignmentReturnValue evaluateNaturalLogArrayAssignment(AssignmentReturnValue tIndex) {
	double val = convertAssignmentReturnToFloat(tIndex);
	return makeFloatAssignmentReturn(log(val));
}

static AssignmentReturnValue evaluateLogArrayAssignment(AssignmentReturnValue tIndex) {
	char* text = convertAssignmentReturnToAllocatedString(tIndex);
	double base, value;
	char comma[10];
	int items = sscanf(text, "%lf %s %lf", &base, comma, &value);
	if (items != 3 || strcmp(",", comma)) {
		logWarningFormat("Unable to parse log array assignment %s. Defaulting to bottom.", text);
		freeMemory(text);
		return makeBottomAssignmentReturn(); 
	}
	freeMemory(text);

	return makeFloatAssignmentReturn(log(value) / log(base));
}

static AssignmentReturnValue evaluateCosineArrayAssignment(AssignmentReturnValue tIndex) {
		double val = convertAssignmentReturnToFloat(tIndex);
		return makeFloatAssignmentReturn(cos(val));
}

static AssignmentReturnValue evaluateAcosineArrayAssignment(AssignmentReturnValue tIndex) {
	double val = convertAssignmentReturnToFloat(tIndex);
	return makeFloatAssignmentReturn(acos(val));
}

static AssignmentReturnValue evaluateSineArrayAssignment(AssignmentReturnValue tIndex) {
	double val = convertAssignmentReturnToFloat(tIndex);
	return makeFloatAssignmentReturn(sin(val));
}

static AssignmentReturnValue evaluateAsineArrayAssignment(AssignmentReturnValue tIndex) {
	double val = convertAssignmentReturnToFloat(tIndex);
	return makeFloatAssignmentReturn(asin(val));
}

static AssignmentReturnValue evaluateTangentArrayAssignment(AssignmentReturnValue tIndex) {
	double val = convertAssignmentReturnToFloat(tIndex);
	return makeFloatAssignmentReturn(tan(val));
}

static AssignmentReturnValue evaluateAtangentArrayAssignment(AssignmentReturnValue tIndex) {
	double val = convertAssignmentReturnToFloat(tIndex);
	return makeFloatAssignmentReturn(atan(val));
}

static AssignmentReturnValue evaluateFloorArrayAssignment(AssignmentReturnValue tIndex) {
	if (isFloatReturn(tIndex)) {
		double val = convertAssignmentReturnToFloat(tIndex);
		return makeNumberAssignmentReturn((int)floor(val));
	}
	else {
		int val = convertAssignmentReturnToNumber(tIndex);
		return makeNumberAssignmentReturn(val);
	}
}

static AssignmentReturnValue evaluateCeilArrayAssignment(AssignmentReturnValue tIndex) {
	if (isFloatReturn(tIndex)) {
		double val = convertAssignmentReturnToFloat(tIndex);
		return makeNumberAssignmentReturn((int)ceil(val));
	}
	else {
		int val = convertAssignmentReturnToNumber(tIndex);
		return makeNumberAssignmentReturn(val);
	}
}

static AssignmentReturnValue evaluateConstArrayAssignment(AssignmentReturnValue tIndex, DreamPlayer* tPlayer, int* tIsStatic) {
	char* var = convertAssignmentReturnToAllocatedString(tIndex);

	if(!string_map_contains(&gVariableHandler.mConstants, var)) {
		logWarningFormat("Unrecognized constant %s. Returning bottom.", var);
		freeMemory(var);
		return makeBottomAssignmentReturn(); 
	}

	VariableFunction func = (VariableFunction)string_map_get(&gVariableHandler.mConstants, var);
	freeMemory(var);

	*tIsStatic = 0;
	return func(tPlayer);
}

static AssignmentReturnValue evaluateGetHitVarArrayAssignment(DreamMugenAssignment** tIndexAssignment, DreamPlayer* tPlayer, int* tIsStatic) {
	if ((*tIndexAssignment)->mType != MUGEN_ASSIGNMENT_TYPE_VARIABLE) {
		logWarningFormat("Unable to parse get hit var array assignment of type %d. Defaulting to bottom.", (*tIndexAssignment)->mType);
		return makeBottomAssignmentReturn(); 
	}

	*tIsStatic = 0;

	DreamMugenVariableAssignment* variableAssignment = (DreamMugenVariableAssignment*)*tIndexAssignment;
	char var[100];
	strcpy(var, variableAssignment->mName);
	turnStringLowercase(var);

	if (!strcmp("animtype", var)) {
		return makeNumberAssignmentReturn(getActiveHitDataAnimationType(tPlayer));
	}
	else if (!strcmp("groundtype", var)) {
		return makeNumberAssignmentReturn(getActiveHitDataGroundType(tPlayer));
	}
	else if (!strcmp("airtype", var)) {
		return makeNumberAssignmentReturn(getActiveHitDataAirType(tPlayer));
	}
	else if (!strcmp("xvel", var)) {
		return makeFloatAssignmentReturn(getActiveHitDataVelocityX(tPlayer));
	}
	else if (!strcmp("yvel", var)) {
		return makeFloatAssignmentReturn(getActiveHitDataVelocityY(tPlayer));
	}
	else if (!strcmp("yaccel", var)) {
		return makeFloatAssignmentReturn(getActiveHitDataYAccel(tPlayer));
	}
	else if (!strcmp("fall", var)) {
		return makeBooleanAssignmentReturn(getActiveHitDataFall(tPlayer));
	}
	else if (!strcmp("fall.recover", var)) {
		return makeBooleanAssignmentReturn(getActiveHitDataFallRecovery(tPlayer));
	}
	else if (!strcmp("fall.yvel", var)) {
		return makeFloatAssignmentReturn(getActiveHitDataFallYVelocity(tPlayer));
	}
	else if (!strcmp("slidetime", var)) {
		return makeBooleanAssignmentReturn(getPlayerSlideTime(tPlayer));
	}
	else if (!strcmp("hitshaketime", var)) {
		return makeBooleanAssignmentReturn(getActiveHitDataPlayer2PauseTime(tPlayer));
	}
	else if (!strcmp("hitcount", var)) {
		return makeNumberAssignmentReturn(getPlayerHitCount(tPlayer));
	}
	else if (!strcmp("damage", var)) {
		return makeNumberAssignmentReturn(getActiveHitDataDamage(tPlayer));
	}
	else if (!strcmp("fallcount", var)) {
		return makeNumberAssignmentReturn(getPlayerFallAmountInCombo(tPlayer));
	}
	else if (!strcmp("ctrltime", var)) {
		return makeNumberAssignmentReturn(getPlayerControlTime(tPlayer));
	}
	else if (!strcmp("isbound", var)) {
		return makeBooleanAssignmentReturn(isPlayerBound(tPlayer));
	}
	else {
		logWarningFormat("Unrecognized GetHitVar Constant %s. Returning bottom.", var);
		return makeBottomAssignmentReturn(); 
	}


}

static AssignmentReturnValue evaluateAnimationElementTimeArrayAssignment(AssignmentReturnValue tIndex, DreamPlayer* tPlayer, int* tIsStatic) {
	int id = convertAssignmentReturnToNumber(tIndex);
	int ret = getPlayerTimeFromAnimationElement(tPlayer, id);

	*tIsStatic = 0;
	return makeNumberAssignmentReturn(ret);
}

static AssignmentReturnValue evaluateAnimationElementNumberArrayAssignment(AssignmentReturnValue tIndex, DreamPlayer* tPlayer, int* tIsStatic) {
	int time = convertAssignmentReturnToNumber(tIndex);
	int ret = getPlayerAnimationElementFromTimeOffset(tPlayer, time);

	*tIsStatic = 0;
	return makeNumberAssignmentReturn(ret);
}

static AssignmentReturnValue evaluateIfElseArrayAssignment(AssignmentReturnValue tIndex) {
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
		AssignmentReturnValue yesRet = makeStringAssignmentReturn(yesText);
		return yesRet;
	}
	else {
		AssignmentReturnValue noRet = makeStringAssignmentReturn(noText);
		return noRet;
	}
}

static AssignmentReturnValue evaluateCondArrayAssignment(DreamMugenAssignment** tCondVector, DreamPlayer* tPlayer, int* tIsStatic) {
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

	AssignmentReturnValue ret;

	AssignmentReturnValue truthValue = evaluateAssignmentDependency(&firstV->a, tPlayer, tIsStatic);
	int isTrue = convertAssignmentReturnToNumber(truthValue);
	if (isTrue) {
		ret = evaluateAssignmentDependency(&secondV->a, tPlayer, tIsStatic);
	}
	else {
		ret = evaluateAssignmentDependency(&secondV->b, tPlayer, tIsStatic);
	}

	return ret;
}

static AssignmentReturnValue evaluateAnimationExistArrayAssignment(AssignmentReturnValue tIndex, DreamPlayer* tPlayer, int* tIsStatic) {
	int id = convertAssignmentReturnToNumber(tIndex);
	int ret = doesPlayerHaveAnimation(tPlayer, id);

	*tIsStatic = 0;
	return makeNumberAssignmentReturn(ret);
}

static AssignmentReturnValue evaluateSelfAnimationExistArrayAssignment(AssignmentReturnValue tIndex, DreamPlayer* tPlayer, int* tIsStatic) {
	int id = convertAssignmentReturnToNumber(tIndex);
	int ret = doesPlayerHaveAnimationHimself(tPlayer, id);

	(void)*tIsStatic;
	return makeNumberAssignmentReturn(ret);
}

static AssignmentReturnValue evaluateConstCoordinatesArrayAssignment(AssignmentReturnValue tIndex, int p) {
	int coords = convertAssignmentReturnToNumber(tIndex);
	double ret = parseDreamCoordinatesToLocalCoordinateSystem(coords, p);
	return makeFloatAssignmentReturn(ret);
}


static AssignmentReturnValue evaluateExternalFileAnimationArrayAssignment(AssignmentReturnValue a, AssignmentReturnValue b) {
	char* val1 = convertAssignmentReturnToAllocatedString(a);
	char* val2 = convertAssignmentReturnToAllocatedString(b);
	AssignmentReturnValue ret = makeExternalFileAssignmentReturn(val1[0], val2);
	freeMemory(val1);
	freeMemory(val2);
	return ret;
}

static AssignmentReturnValue evaluateNumTargetArrayAssignment(AssignmentReturnValue tIndex, DreamPlayer* tPlayer, int* tIsStatic) {
	int id = convertAssignmentReturnToNumber(tIndex);
	int ret = getPlayerTargetAmountWithID(tPlayer, id);

	*tIsStatic = 0;
	return makeNumberAssignmentReturn(ret);
}

static AssignmentReturnValue evaluateNumHelperArrayAssignment(AssignmentReturnValue tIndex, DreamPlayer* tPlayer, int* tIsStatic) {
	int id = convertAssignmentReturnToNumber(tIndex);
	int ret = getPlayerHelperAmountWithID(tPlayer, id);

	*tIsStatic = 0;
	return makeNumberAssignmentReturn(ret);
}

static AssignmentReturnValue evaluateNumExplodArrayAssignment(AssignmentReturnValue tIndex, DreamPlayer* tPlayer, int* tIsStatic) {
	int id = convertAssignmentReturnToNumber(tIndex);
	int ret = getExplodAmountWithID(tPlayer, id);

	*tIsStatic = 0;
	return makeNumberAssignmentReturn(ret);
}

static AssignmentReturnValue evaluateIsHelperArrayAssignment(AssignmentReturnValue tIndex, DreamPlayer* tPlayer, int* tIsStatic) {
	int id = convertAssignmentReturnToNumber(tIndex);
	int isHelper = isPlayerHelper(tPlayer);
	int playerID = getPlayerID(tPlayer);

	*tIsStatic = 0;
	return makeBooleanAssignmentReturn(isHelper && id == playerID);
}

static AssignmentReturnValue evaluateTargetArrayAssignment(AssignmentReturnValue tIndex, char* tTargetName) {
	int id = convertAssignmentReturnToNumber(tIndex);

	char buffer[100]; // TODO: dynamic
	sprintf(buffer, "%s %d", tTargetName, id);
	return makeStringAssignmentReturn(buffer);
}

static AssignmentReturnValue evaluatePlayerIDExistArrayAssignment(AssignmentReturnValue tIndex, DreamPlayer* tPlayer, int* tIsStatic) {
	int id = convertAssignmentReturnToNumber(tIndex);
	*tIsStatic = 0;
	return makeBooleanAssignmentReturn(doesPlayerIDExist(tPlayer, id));
}

static AssignmentReturnValue evaluateNumberOfProjectilesWithIDArrayAssignment(AssignmentReturnValue tIndex, DreamPlayer* tPlayer, int* tIsStatic) {
	int id = convertAssignmentReturnToNumber(tIndex);
	*tIsStatic = 0;
	return makeNumberAssignmentReturn(getPlayerProjectileAmountWithID(tPlayer, id));
}

static AssignmentReturnValue evaluateProjectileCancelTimeArrayAssignment(AssignmentReturnValue tIndex, DreamPlayer* tPlayer, int* tIsStatic) {
	int id = convertAssignmentReturnToNumber(tIndex);
	*tIsStatic = 0;
	return makeNumberAssignmentReturn(getPlayerProjectileTimeSinceCancel(tPlayer, id));
}

static AssignmentReturnValue evaluateProjectileContactTimeArrayAssignment(AssignmentReturnValue tIndex, DreamPlayer* tPlayer, int* tIsStatic) {
	int id = convertAssignmentReturnToNumber(tIndex);
	*tIsStatic = 0;
	return makeNumberAssignmentReturn(getPlayerProjectileTimeSinceContact(tPlayer, id));
}

static AssignmentReturnValue evaluateProjectileGuardedTimeArrayAssignment(AssignmentReturnValue tIndex, DreamPlayer* tPlayer, int* tIsStatic) {
	int id = convertAssignmentReturnToNumber(tIndex);
	*tIsStatic = 0;
	return makeNumberAssignmentReturn(getPlayerProjectileTimeSinceGuarded(tPlayer, id));
}

static AssignmentReturnValue evaluateProjectileHitTimeArrayAssignment(AssignmentReturnValue tIndex, DreamPlayer* tPlayer, int* tIsStatic) {
	int id = convertAssignmentReturnToNumber(tIndex);
	*tIsStatic = 0;
	return makeNumberAssignmentReturn(getPlayerProjectileTimeSinceHit(tPlayer, id));
}





static AssignmentReturnValue varFunction(DreamMugenDependOnTwoAssignment* tArrays, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateVarArrayAssignment(evaluateAssignmentDependency(&tArrays->b, tPlayer, tIsStatic), tPlayer, tIsStatic); }
static AssignmentReturnValue sysVarFunction(DreamMugenDependOnTwoAssignment* tArrays, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateSysVarArrayAssignment(evaluateAssignmentDependency(&tArrays->b, tPlayer, tIsStatic), tPlayer, tIsStatic); }
static AssignmentReturnValue fVarFunction(DreamMugenDependOnTwoAssignment* tArrays, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateFVarArrayAssignment(evaluateAssignmentDependency(&tArrays->b, tPlayer, tIsStatic), tPlayer, tIsStatic); }
static AssignmentReturnValue sysFVarFunction(DreamMugenDependOnTwoAssignment* tArrays, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateSysFVarArrayAssignment(evaluateAssignmentDependency(&tArrays->b, tPlayer, tIsStatic), tPlayer, tIsStatic); }
static AssignmentReturnValue stageVarFunction(DreamMugenDependOnTwoAssignment* tArrays, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateStageVarArrayAssignment(evaluateAssignmentDependency(&tArrays->b, tPlayer, tIsStatic), tPlayer, tIsStatic); }
static AssignmentReturnValue absFunction(DreamMugenDependOnTwoAssignment* tArrays, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateAbsArrayAssignment(evaluateAssignmentDependency(&tArrays->b, tPlayer, tIsStatic)); }
static AssignmentReturnValue expFunction(DreamMugenDependOnTwoAssignment* tArrays, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateExpArrayAssignment(evaluateAssignmentDependency(&tArrays->b, tPlayer, tIsStatic)); }
static AssignmentReturnValue lnFunction(DreamMugenDependOnTwoAssignment* tArrays, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateNaturalLogArrayAssignment(evaluateAssignmentDependency(&tArrays->b, tPlayer, tIsStatic)); }
static AssignmentReturnValue logFunction(DreamMugenDependOnTwoAssignment* tArrays, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateLogArrayAssignment(evaluateAssignmentDependency(&tArrays->b, tPlayer, tIsStatic)); }
static AssignmentReturnValue cosFunction(DreamMugenDependOnTwoAssignment* tArrays, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateCosineArrayAssignment(evaluateAssignmentDependency(&tArrays->b, tPlayer, tIsStatic)); }
static AssignmentReturnValue acosFunction(DreamMugenDependOnTwoAssignment* tArrays, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateAcosineArrayAssignment(evaluateAssignmentDependency(&tArrays->b, tPlayer, tIsStatic)); }
static AssignmentReturnValue sinFunction(DreamMugenDependOnTwoAssignment* tArrays, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateSineArrayAssignment(evaluateAssignmentDependency(&tArrays->b, tPlayer, tIsStatic)); }
static AssignmentReturnValue asinFunction(DreamMugenDependOnTwoAssignment* tArrays, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateAsineArrayAssignment(evaluateAssignmentDependency(&tArrays->b, tPlayer, tIsStatic)); }
static AssignmentReturnValue tanFunction(DreamMugenDependOnTwoAssignment* tArrays, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateTangentArrayAssignment(evaluateAssignmentDependency(&tArrays->b, tPlayer, tIsStatic)); }
static AssignmentReturnValue atanFunction(DreamMugenDependOnTwoAssignment* tArrays, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateAtangentArrayAssignment(evaluateAssignmentDependency(&tArrays->b, tPlayer, tIsStatic)); }
static AssignmentReturnValue floorFunction(DreamMugenDependOnTwoAssignment* tArrays, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateFloorArrayAssignment(evaluateAssignmentDependency(&tArrays->b, tPlayer, tIsStatic)); }
static AssignmentReturnValue ceilFunction(DreamMugenDependOnTwoAssignment* tArrays, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateCeilArrayAssignment(evaluateAssignmentDependency(&tArrays->b, tPlayer, tIsStatic)); }
static AssignmentReturnValue constFunction(DreamMugenDependOnTwoAssignment* tArrays, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateConstArrayAssignment(evaluateAssignmentDependency(&tArrays->b, tPlayer, tIsStatic), tPlayer, tIsStatic); }
static AssignmentReturnValue getHitVarFunction(DreamMugenDependOnTwoAssignment* tArrays, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateGetHitVarArrayAssignment(&tArrays->b, tPlayer, tIsStatic); }
static AssignmentReturnValue animElemTimeFunction(DreamMugenDependOnTwoAssignment* tArrays, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateAnimationElementTimeArrayAssignment(evaluateAssignmentDependency(&tArrays->b, tPlayer, tIsStatic), tPlayer, tIsStatic); }
static AssignmentReturnValue animElemNoFunction(DreamMugenDependOnTwoAssignment* tArrays, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateAnimationElementNumberArrayAssignment(evaluateAssignmentDependency(&tArrays->b, tPlayer, tIsStatic), tPlayer, tIsStatic); }
static AssignmentReturnValue ifElseFunction(DreamMugenDependOnTwoAssignment* tArrays, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateIfElseArrayAssignment(evaluateAssignmentDependency(&tArrays->b, tPlayer, tIsStatic)); }
static AssignmentReturnValue condFunction(DreamMugenDependOnTwoAssignment* tArrays, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateCondArrayAssignment(&tArrays->b, tPlayer, tIsStatic); }
static AssignmentReturnValue animExistFunction(DreamMugenDependOnTwoAssignment* tArrays, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateAnimationExistArrayAssignment(evaluateAssignmentDependency(&tArrays->b, tPlayer, tIsStatic), tPlayer, tIsStatic); }
static AssignmentReturnValue selfAnimExistFunction(DreamMugenDependOnTwoAssignment* tArrays, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateSelfAnimationExistArrayAssignment(evaluateAssignmentDependency(&tArrays->b, tPlayer, tIsStatic), tPlayer, tIsStatic); }
static AssignmentReturnValue const240pFunction(DreamMugenDependOnTwoAssignment* tArrays, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateConstCoordinatesArrayAssignment(evaluateAssignmentDependency(&tArrays->b, tPlayer, tIsStatic), 240); }
static AssignmentReturnValue const480pFunction(DreamMugenDependOnTwoAssignment* tArrays, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateConstCoordinatesArrayAssignment(evaluateAssignmentDependency(&tArrays->b, tPlayer, tIsStatic), 480); }
static AssignmentReturnValue const720pFunction(DreamMugenDependOnTwoAssignment* tArrays, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateConstCoordinatesArrayAssignment(evaluateAssignmentDependency(&tArrays->b, tPlayer, tIsStatic), 720); }
static AssignmentReturnValue externalFileFunction(DreamMugenDependOnTwoAssignment* tArrays, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateExternalFileAnimationArrayAssignment(evaluateAssignmentDependency(&tArrays->a, tPlayer, tIsStatic), evaluateAssignmentDependency(&tArrays->b, tPlayer, tIsStatic)); }
static AssignmentReturnValue numTargetArrayFunction(DreamMugenDependOnTwoAssignment* tArrays, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateNumTargetArrayAssignment(evaluateAssignmentDependency(&tArrays->b, tPlayer, tIsStatic), tPlayer, tIsStatic); }
static AssignmentReturnValue numHelperArrayFunction(DreamMugenDependOnTwoAssignment* tArrays, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateNumHelperArrayAssignment(evaluateAssignmentDependency(&tArrays->b, tPlayer, tIsStatic), tPlayer, tIsStatic); }
static AssignmentReturnValue numExplodArrayFunction(DreamMugenDependOnTwoAssignment* tArrays, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateNumExplodArrayAssignment(evaluateAssignmentDependency(&tArrays->b, tPlayer, tIsStatic), tPlayer, tIsStatic); }
static AssignmentReturnValue isHelperArrayFunction(DreamMugenDependOnTwoAssignment* tArrays, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateIsHelperArrayAssignment(evaluateAssignmentDependency(&tArrays->b, tPlayer, tIsStatic), tPlayer, tIsStatic); }
static AssignmentReturnValue helperFunction(DreamMugenDependOnTwoAssignment* tArrays, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateTargetArrayAssignment(evaluateAssignmentDependency(&tArrays->b, tPlayer, tIsStatic), "helper"); }
static AssignmentReturnValue enemyNearFunction(DreamMugenDependOnTwoAssignment* tArrays, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateTargetArrayAssignment(evaluateAssignmentDependency(&tArrays->b, tPlayer, tIsStatic), "enemynear"); }
static AssignmentReturnValue playerIDFunction(DreamMugenDependOnTwoAssignment* tArrays, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateTargetArrayAssignment(evaluateAssignmentDependency(&tArrays->b, tPlayer, tIsStatic), "playerid"); }
static AssignmentReturnValue playerIDExistFunction(DreamMugenDependOnTwoAssignment* tArrays, DreamPlayer* tPlayer, int* tIsStatic) { return evaluatePlayerIDExistArrayAssignment(evaluateAssignmentDependency(&tArrays->b, tPlayer, tIsStatic), tPlayer, tIsStatic); }
static AssignmentReturnValue numProjIDFunction(DreamMugenDependOnTwoAssignment* tArrays, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateNumberOfProjectilesWithIDArrayAssignment(evaluateAssignmentDependency(&tArrays->b, tPlayer, tIsStatic), tPlayer, tIsStatic); }
static AssignmentReturnValue projCancelTimeFunction(DreamMugenDependOnTwoAssignment* tArrays, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateProjectileCancelTimeArrayAssignment(evaluateAssignmentDependency(&tArrays->b, tPlayer, tIsStatic), tPlayer, tIsStatic); }
static AssignmentReturnValue projContactTimeFunction(DreamMugenDependOnTwoAssignment* tArrays, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateProjectileContactTimeArrayAssignment(evaluateAssignmentDependency(&tArrays->b, tPlayer, tIsStatic), tPlayer, tIsStatic); }
static AssignmentReturnValue projGuardedTimeFunction(DreamMugenDependOnTwoAssignment* tArrays, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateProjectileGuardedTimeArrayAssignment(evaluateAssignmentDependency(&tArrays->b, tPlayer, tIsStatic), tPlayer, tIsStatic); }
static AssignmentReturnValue projHitTimeFunction(DreamMugenDependOnTwoAssignment* tArrays, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateProjectileHitTimeArrayAssignment(evaluateAssignmentDependency(&tArrays->b, tPlayer, tIsStatic), tPlayer, tIsStatic); }


static void setupArrayAssignments() {
	gVariableHandler.mArrays = new_string_map();

	string_map_push(&gVariableHandler.mArrays, "var", varFunction);
	string_map_push(&gVariableHandler.mArrays, "sysvar", sysVarFunction);
	string_map_push(&gVariableHandler.mArrays, "fvar", fVarFunction);
	string_map_push(&gVariableHandler.mArrays, "sysfvar", sysFVarFunction);
	string_map_push(&gVariableHandler.mArrays, "stagevar", stageVarFunction);
	string_map_push(&gVariableHandler.mArrays, "abs", absFunction);
	string_map_push(&gVariableHandler.mArrays, "exp", expFunction);
	string_map_push(&gVariableHandler.mArrays, "ln", lnFunction);
	string_map_push(&gVariableHandler.mArrays, "log", logFunction);
	string_map_push(&gVariableHandler.mArrays, "cos", cosFunction);
	string_map_push(&gVariableHandler.mArrays, "acos", acosFunction);
	string_map_push(&gVariableHandler.mArrays, "sin", sinFunction);
	string_map_push(&gVariableHandler.mArrays, "asin", asinFunction);
	string_map_push(&gVariableHandler.mArrays, "tan", tanFunction);
	string_map_push(&gVariableHandler.mArrays, "atan", atanFunction);
	string_map_push(&gVariableHandler.mArrays, "floor", floorFunction);
	string_map_push(&gVariableHandler.mArrays, "ceil", ceilFunction);
	string_map_push(&gVariableHandler.mArrays, "const", constFunction);
	string_map_push(&gVariableHandler.mArrays, "gethitvar", getHitVarFunction);
	string_map_push(&gVariableHandler.mArrays, "animelemtime", animElemTimeFunction);
	string_map_push(&gVariableHandler.mArrays, "animelemno", animElemNoFunction);
	string_map_push(&gVariableHandler.mArrays, "ifelse", ifElseFunction);
	string_map_push(&gVariableHandler.mArrays, "sifelse", ifElseFunction);
	string_map_push(&gVariableHandler.mArrays, "cond", condFunction);
	string_map_push(&gVariableHandler.mArrays, "animexist", animExistFunction);
	string_map_push(&gVariableHandler.mArrays, "selfanimexist", selfAnimExistFunction);
	string_map_push(&gVariableHandler.mArrays, "const240p", const240pFunction);
	string_map_push(&gVariableHandler.mArrays, "const480p", const480pFunction);
	string_map_push(&gVariableHandler.mArrays, "const720p", const720pFunction);
	string_map_push(&gVariableHandler.mArrays, "f", externalFileFunction);
	string_map_push(&gVariableHandler.mArrays, "s", externalFileFunction);
	string_map_push(&gVariableHandler.mArrays, "numtarget", numTargetArrayFunction);
	string_map_push(&gVariableHandler.mArrays, "numhelper", numHelperArrayFunction);
	string_map_push(&gVariableHandler.mArrays, "numexplod", numExplodArrayFunction);
	string_map_push(&gVariableHandler.mArrays, "ishelper", isHelperArrayFunction);
	string_map_push(&gVariableHandler.mArrays, "helper", helperFunction);
	string_map_push(&gVariableHandler.mArrays, "enemynear", enemyNearFunction);
	string_map_push(&gVariableHandler.mArrays, "playerid", playerIDFunction);
	string_map_push(&gVariableHandler.mArrays, "playeridexist", playerIDExistFunction);
	string_map_push(&gVariableHandler.mArrays, "numprojid", numProjIDFunction);
	string_map_push(&gVariableHandler.mArrays, "projcanceltime", projCancelTimeFunction);
	string_map_push(&gVariableHandler.mArrays, "projcontacttime", projContactTimeFunction);
	string_map_push(&gVariableHandler.mArrays, "projguardedtime", projGuardedTimeFunction);
	string_map_push(&gVariableHandler.mArrays, "projhittime", projHitTimeFunction);
}


static AssignmentReturnValue evaluateArrayAssignment(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* tIsStatic) {
	DreamMugenDependOnTwoAssignment* arrays = (DreamMugenDependOnTwoAssignment*)*tAssignment;

	char test[100];
	if (arrays->a->mType != MUGEN_ASSIGNMENT_TYPE_VARIABLE) {
		logWarningFormat("Invalid array type %d. Defaulting to bottom.", arrays->a->mType);
		return makeBottomAssignmentReturn(); 
	}
	DreamMugenVariableAssignment* arrayName = (DreamMugenVariableAssignment*)arrays->a;
	strcpy(test, arrayName->mName);
	turnStringLowercase(test);

	
	if(!string_map_contains(&gVariableHandler.mArrays, test)) {
		logWarningFormat("Unknown array %s. Returning bottom.", test); 
		return makeBottomAssignmentReturn(); 
	}

	ArrayFunction func = (ArrayFunction)string_map_get(&gVariableHandler.mArrays, test);
	return func(arrays, tPlayer, tIsStatic);
}

static AssignmentReturnValue evaluateUnaryMinusAssignment(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* tIsStatic) {
	DreamMugenDependOnOneAssignment* min = (DreamMugenDependOnOneAssignment*)*tAssignment;

	AssignmentReturnValue a = evaluateAssignmentDependency(&min->a, tPlayer, tIsStatic);
	if (isFloatReturn(a)) {
		return makeFloatAssignmentReturn(-convertAssignmentReturnToFloat(a));
	}
	else {
		return makeNumberAssignmentReturn(-convertAssignmentReturnToNumber(a));
	}
}

static AssignmentReturnValue evaluateNegationAssignment(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* tIsStatic) {
	DreamMugenDependOnOneAssignment* neg = (DreamMugenDependOnOneAssignment*)*tAssignment;

	AssignmentReturnValue a = evaluateAssignmentDependency(&neg->a, tPlayer, tIsStatic);
	int val = convertAssignmentReturnToBool(a);

	return makeBooleanAssignmentReturn(!val);
}

typedef struct {
	uint8_t mType;
	AssignmentReturnValue mValue;
} DreamMugenStaticAssignment;

int gPruneAmount;

static DreamMugenAssignment* makeStaticDreamMugenAssignment(AssignmentReturnValue tValue) {
	DreamMugenStaticAssignment* e = (DreamMugenStaticAssignment*)allocMemory(sizeof(DreamMugenStaticAssignment));
	e->mType = MUGEN_ASSIGNMENT_TYPE_STATIC;
	e->mValue = tValue;

	gPruneAmount++;
	return (DreamMugenAssignment*)e;
}

static AssignmentReturnValue evaluateStaticAssignment(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* tIsStatic) {
	(void)tPlayer;
	(void)tIsStatic;

	DreamMugenStaticAssignment* stat = (DreamMugenStaticAssignment*)*tAssignment;
	return stat->mValue;
}

typedef AssignmentReturnValue(AssignmentEvaluationFunction)(DreamMugenAssignment**, DreamPlayer*, int*);

static void* gEvaluationFunctions[] = {
	evaluateBooleanAssignment,
	evaluateAndAssignment,
	evaluateOrAssignment,
	evaluateComparisonAssignment,
	evaluateInequalityAssignment,
	evaluateLessOrEqualAssignment,
	evaluateGreaterOrEqualAssignment,
	evaluateVectorAssignment,
	evaluateRangeAssignment,
	evaluateBooleanAssignment,
	evaluateNegationAssignment,
	evaluateVariableAssignment,
	evaluateNumberAssignment,
	evaluateFloatAssignment,
	evaluateStringAssignment,
	evaluateArrayAssignment,
	evaluateLessAssignment,
	evaluateGreaterAssignment,
	evaluateAdditionAssignment,
	evaluateMultiplicationAssignment,
	evaluateModuloAssignment,
	evaluateSubtractionAssignment,
	evaluateSetVariableAssignment,
	evaluateDivisionAssignment,
	evaluateExponentiationAssignment,
	evaluateUnaryMinusAssignment,
	evaluateOperatorArgumentAssignment,
	evaluateBitwiseAndAssignment,
	evaluateBitwiseOrAssignment,
	evaluateStaticAssignment,
};


static void setAssignmentStatic(DreamMugenAssignment** tAssignment, AssignmentReturnValue tValue) {
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

static AssignmentReturnValue evaluateAssignmentInternal(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer, int* oIsStatic) {
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
	AssignmentReturnValue ret = func(tAssignment, tPlayer, oIsStatic);

	if (*oIsStatic) {
		setAssignmentStatic(tAssignment, ret);
	}

	return ret;
}

static AssignmentReturnValue timeStoryFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getDolmexicaStoryTimeInState()); }

static void setupStoryVariableAssignments() {
	gVariableHandler.mVariables = new_string_map();
	string_map_push(&gVariableHandler.mVariables, "time", timeStoryFunction);

	string_map_push(&gVariableHandler.mVariables, "random", randomFunction);
}

//static AssignmentReturnValue movementDownFrictionThresholdFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerLyingDownFrictionThreshold(tPlayer)); }

static void setupStoryConstantAssignments() {
	gVariableHandler.mConstants = new_string_map();
}

static AssignmentReturnValue evaluateAnimTimeStoryArrayAssignment(AssignmentReturnValue tIndex, int* tIsStatic) {
	int id = convertAssignmentReturnToNumber(tIndex);
	*tIsStatic = 0;
	return makeNumberAssignmentReturn(getDolmexicaStoryAnimationTimeLeft(id));
}

static AssignmentReturnValue evaluateAnimPosXStoryArrayAssignment(AssignmentReturnValue tIndex, int* tIsStatic) {
	int id = convertAssignmentReturnToNumber(tIndex);
	*tIsStatic = 0;
	return makeFloatAssignmentReturn(getDolmexicaStoryAnimationPositionX(id));
}

static AssignmentReturnValue animTimeStoryFunction(DreamMugenDependOnTwoAssignment* tArrays, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateAnimTimeStoryArrayAssignment(evaluateAssignmentDependency(&tArrays->b, tPlayer, tIsStatic), tIsStatic); }
static AssignmentReturnValue animPosXStoryFunction(DreamMugenDependOnTwoAssignment* tArrays, DreamPlayer* tPlayer, int* tIsStatic) { return evaluateAnimPosXStoryArrayAssignment(evaluateAssignmentDependency(&tArrays->b, tPlayer, tIsStatic), tIsStatic); }

static void setupStoryArrayAssignments() {
	gVariableHandler.mArrays = new_string_map();

	string_map_push(&gVariableHandler.mArrays, "animtime", animTimeStoryFunction);
	string_map_push(&gVariableHandler.mArrays, "x", animPosXStoryFunction);
	string_map_push(&gVariableHandler.mArrays, "ifelse", ifElseFunction);
}

void setupDreamStoryAssignmentEvaluator()
{
	setupStoryVariableAssignments();
	setupStoryConstantAssignments();
	setupStoryArrayAssignments();
	setupComparisons();
}

void shutdownDreamAssignmentEvaluator()
{
	delete_string_map(&gVariableHandler.mComparisons);
	delete_string_map(&gVariableHandler.mArrays);
	delete_string_map(&gVariableHandler.mConstants);
	delete_string_map(&gVariableHandler.mVariables);
}

int evaluateDreamAssignment(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer)
{
	int isStatic;
	AssignmentReturnValue ret = evaluateAssignmentInternal(tAssignment, tPlayer, &isStatic);
	return convertAssignmentReturnToBool(ret);
}

double evaluateDreamAssignmentAndReturnAsFloat(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer)
{
	int isStatic;
	AssignmentReturnValue ret = evaluateAssignmentInternal(tAssignment, tPlayer, &isStatic);
	return convertAssignmentReturnToFloat(ret);
}

int evaluateDreamAssignmentAndReturnAsInteger(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer)
{
	int isStatic;
	AssignmentReturnValue ret = evaluateAssignmentInternal(tAssignment, tPlayer, &isStatic);
	return convertAssignmentReturnToNumber(ret);
}

char * evaluateDreamAssignmentAndReturnAsAllocatedString(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer)
{
	int isStatic;
	AssignmentReturnValue ret = evaluateAssignmentInternal(tAssignment, tPlayer, &isStatic);
	return convertAssignmentReturnToAllocatedString(ret);
}

Vector3D evaluateDreamAssignmentAndReturnAsVector3D(DreamMugenAssignment** tAssignment, DreamPlayer* tPlayer)
{
	int isStatic;
	AssignmentReturnValue ret = evaluateAssignmentInternal(tAssignment, tPlayer, &isStatic);
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
	AssignmentReturnValue ret = evaluateAssignmentInternal(tAssignment, tPlayer, &isStatic);
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
