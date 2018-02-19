#include "mugenassignmentevaluator.h"

#include <assert.h>

#include <prism/log.h>
#include <prism/system.h>
#include <prism/math.h>

#include "gamelogic.h"
#include "stage.h"
#include "playerhitdata.h"
#include "mugenexplod.h"

typedef struct {
	char mValue[100];
} AssignmentReturnValue;

static AssignmentReturnValue evaluateAssignmentInternal(DreamMugenAssignment* tAssignment, DreamPlayer* tPlayer);

static AssignmentReturnValue getAssignmentReturnValueToBool(AssignmentReturnValue tAssignmentReturn) {
	AssignmentReturnValue ret;
	if (!strcmp("", tAssignmentReturn.mValue) || !strcmp("0", tAssignmentReturn.mValue)) {
		strcpy(ret.mValue, "0");
	}
	else {
		strcpy(ret.mValue, "1");
	}

	return ret;
}

static int evaluateAssignmentReturnAsBool(AssignmentReturnValue tAssignmentReturn) {
	AssignmentReturnValue rest = getAssignmentReturnValueToBool(tAssignmentReturn);

	return strcmp("0", rest.mValue);
}


static int evaluateAssignmentReturnAsNumber(AssignmentReturnValue tAssignmentReturn) {
	return atoi(tAssignmentReturn.mValue);
}

static double evaluateAssignmentReturnAsFloat(AssignmentReturnValue tAssignmentReturn) {
	return atof(tAssignmentReturn.mValue);
}


static AssignmentReturnValue makeBooleanAssignmentReturn(int tValue) {
	AssignmentReturnValue ret;
	if (tValue) {
		strcpy(ret.mValue, "1");
	}
	else {
		strcpy(ret.mValue, "0");
	}
	return ret;
}

static AssignmentReturnValue makeNumberAssignmentReturn(int tValue) {
	AssignmentReturnValue ret;
	sprintf(ret.mValue, "%d", tValue);
	return ret;
}

static AssignmentReturnValue makeFloatAssignmentReturn(double tValue) {
	AssignmentReturnValue ret;
	sprintf(ret.mValue, "%f", tValue);
	return ret;
}

static AssignmentReturnValue makeStringAssignmentReturn(char* tValue) {
	AssignmentReturnValue ret;
	strcpy(ret.mValue, tValue);
	return ret;
}

static AssignmentReturnValue evaluateOrAssignment(DreamMugenAssignment* tAssignment, DreamPlayer* tPlayer) {
	DreamMugenDependOnTwoAssignment* orAssignment = tAssignment->mData;

	AssignmentReturnValue a = evaluateAssignmentInternal(orAssignment->a, tPlayer);
	int valA = evaluateAssignmentReturnAsBool(a);
	if (valA) return makeBooleanAssignmentReturn(valA);

	AssignmentReturnValue b = evaluateAssignmentInternal(orAssignment->b, tPlayer);
	int valB = evaluateAssignmentReturnAsBool(b);

	return makeBooleanAssignmentReturn(valB);
}

static AssignmentReturnValue evaluateAndAssignment(DreamMugenAssignment* tAssignment, DreamPlayer* tPlayer) {
	DreamMugenDependOnTwoAssignment* andAssignment = tAssignment->mData;

	AssignmentReturnValue a = evaluateAssignmentInternal(andAssignment->a, tPlayer);
	int valA = evaluateAssignmentReturnAsBool(a);
	if (!valA) return makeBooleanAssignmentReturn(valA);

	AssignmentReturnValue b = evaluateAssignmentInternal(andAssignment->b, tPlayer);
	int valB = evaluateAssignmentReturnAsBool(b);

	return makeBooleanAssignmentReturn(valB);
}

static AssignmentReturnValue evaluateBitwiseOrAssignment(DreamMugenAssignment* tAssignment, DreamPlayer* tPlayer) {
	DreamMugenDependOnTwoAssignment* bitwiseOrAssignment = tAssignment->mData;

	AssignmentReturnValue a = evaluateAssignmentInternal(bitwiseOrAssignment->a, tPlayer);
	int valA = evaluateAssignmentReturnAsNumber(a);

	AssignmentReturnValue b = evaluateAssignmentInternal(bitwiseOrAssignment->b, tPlayer);
	int valB = evaluateAssignmentReturnAsNumber(b);

	return makeNumberAssignmentReturn(valA | valB);
}

static AssignmentReturnValue evaluateBitwiseAndAssignment(DreamMugenAssignment* tAssignment, DreamPlayer* tPlayer) {
	DreamMugenDependOnTwoAssignment* bitwiseAndAssignment = tAssignment->mData;

	AssignmentReturnValue a = evaluateAssignmentInternal(bitwiseAndAssignment->a, tPlayer);
	int valA = evaluateAssignmentReturnAsNumber(a);

	AssignmentReturnValue b = evaluateAssignmentInternal(bitwiseAndAssignment->b, tPlayer);
	int valB = evaluateAssignmentReturnAsNumber(b);

	return makeNumberAssignmentReturn(valA & valB);
}

static AssignmentReturnValue evaluateCommandAssignment(AssignmentReturnValue tCommand, DreamPlayer* tPlayer) {
	int ret = isPlayerCommandActive(tPlayer, tCommand.mValue);
	// printf("%d %d eval command %s as %d\n", tPlayer->mRootID, tPlayer->mID, tCommand.mValue, ret);
	return makeBooleanAssignmentReturn(ret);
}

static AssignmentReturnValue evaluateStateTypeAssignment(AssignmentReturnValue tCommand, DreamPlayer* tPlayer) {
	DreamMugenStateType playerState = getPlayerStateType(tPlayer);

	int ret = 0;
	if (playerState == MUGEN_STATE_TYPE_STANDING) ret = strchr(tCommand.mValue, 'S') != NULL;
	else if (playerState == MUGEN_STATE_TYPE_AIR) ret = strchr(tCommand.mValue, 'A') != NULL;
	else if (playerState == MUGEN_STATE_TYPE_CROUCHING) ret = strchr(tCommand.mValue, 'C') != NULL;
	else if (playerState == MUGEN_STATE_TYPE_LYING) ret = strchr(tCommand.mValue, 'L') != NULL;
	else {
		logError("Undefined player state.");
		logErrorInteger(playerState);
		abortSystem();
	}

	return makeBooleanAssignmentReturn(ret);
}

static AssignmentReturnValue evaluateMoveTypeAssignment(AssignmentReturnValue tCommand, DreamPlayer* tPlayer) {
	DreamMugenStateMoveType playerMoveType = getPlayerStateMoveType(tPlayer);

	char test[20];
	strcpy(test, tCommand.mValue);
	turnStringLowercase(test);

	int ret = 0;
	if (playerMoveType == MUGEN_STATE_MOVE_TYPE_ATTACK) ret = strchr(test, 'a') != NULL;
	else if (playerMoveType == MUGEN_STATE_MOVE_TYPE_BEING_HIT) ret = strchr(test, 'h') != NULL;
	else if (playerMoveType == MUGEN_STATE_MOVE_TYPE_IDLE) ret = strchr(test, 'i') != NULL;
	else {
		logError("Undefined player state.");
		logErrorInteger(playerMoveType);
		abortSystem();
	}

	return makeBooleanAssignmentReturn(ret);
}

static AssignmentReturnValue evaluateAnimElemVectorAssignment(AssignmentReturnValue tCommand, DreamPlayer* tPlayer) {

	char number1[100], comma[10], oper[100], number2[100];
	int items = sscanf(tCommand.mValue, "%s %s %s %s", number1, comma, oper, number2);
	assert(strcmp("", number1));
	assert(!strcmp(",", comma));

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
			logError("Unrecognized operator.");
			logErrorString(oper);
			abortSystem();
			ret = 0;
		}
	}
	else {
		logError("Invalid animelem format.");
		logErrorString(tCommand.mValue);
		abortSystem();
		ret = 0;
	}

	return makeBooleanAssignmentReturn(ret);
}

static AssignmentReturnValue evaluateAnimElemNumberAssignment(AssignmentReturnValue tCommand, DreamPlayer* tPlayer) {

	int elem = evaluateAssignmentReturnAsNumber(tCommand);
	int ret = isPlayerStartingAnimationElementWithID(tPlayer, elem);

	return makeBooleanAssignmentReturn(ret);
}

static AssignmentReturnValue evaluateAnimElemAssignment(AssignmentReturnValue tCommand, DreamPlayer* tPlayer) {

	if (strchr(tCommand.mValue, ',')) {
		return evaluateAnimElemVectorAssignment(tCommand, tPlayer);
	}
	else {
		return evaluateAnimElemNumberAssignment(tCommand, tPlayer);
	}
}

static AssignmentReturnValue evaluateTimeModAssignment(AssignmentReturnValue tCommand, DreamPlayer* tPlayer) {

	char divisor[100], comma[10], oper[100], compareNumber[100];
	printf("timemod: %s\n", tCommand.mValue);
	int items = sscanf(tCommand.mValue, "%s %s %s %s", oper, divisor, comma, compareNumber);
	assert(strcmp("", divisor));
	assert(!strcmp(",", comma));
	assert(items == 4);


	int divisorValue = atoi(divisor);
	int compareValue = atoi(compareNumber);
	int stateTime = getPlayerTimeInState(tPlayer);

	if (divisorValue == 0) {
		return makeBooleanAssignmentReturn(0);
	}

	int modValue = stateTime % divisorValue;
	int ret;
	if (!strcmp("=", oper)) {
		ret = modValue == compareValue;
	}
	else if (!strcmp("!=", oper)) {
		ret = modValue != compareValue;
	}
	else if (!strcmp("<", oper)) {
		ret = modValue < compareValue;
	}
	else if (!strcmp(">", oper)) {
		ret = modValue > compareValue;
	}
	else if (!strcmp("<=", oper)) {
		ret = modValue <= compareValue;
	}
	else if (!strcmp(">=", oper)) {
		ret = modValue >= compareValue;
	}
	else {
		logError("Unrecognized operator.");
		logErrorString(oper);
		abortSystem();
		ret = 0;
	}



	return makeBooleanAssignmentReturn(ret);
}

static DreamPlayer* getPlayerFromFirstVectorPartOrNullIfNonexistant(AssignmentReturnValue a, DreamPlayer* tPlayer) {
	char firstWord[100];
	int id;

	int items = sscanf(a.mValue, "%s %d", firstWord, &id);

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
		assert(ret);
		return ret;
	}
	else if (items == 2 && !strcmp("playerid", firstWord)) {
		DreamPlayer* ret = getPlayerByIDOrNullIfNonexistant(tPlayer, id);
		assert(ret);
		return ret;
	}
	else {
		return NULL;
	}
}

static int isPlayerAccessVectorAssignment(AssignmentReturnValue a, DreamPlayer* tPlayer) {
	return getPlayerFromFirstVectorPartOrNullIfNonexistant(a, tPlayer) != NULL;
}

static AssignmentReturnValue evaluateTeamModeAssignment(AssignmentReturnValue tCommand, DreamPlayer* tPlayer) {
	(void)tPlayer;
	return makeBooleanAssignmentReturn(!strcmp(tCommand.mValue, "single")); // TODO
}

static int isRangeAssignmentReturn(AssignmentReturnValue ret) {
	char brace[100];
	sscanf(ret.mValue, "%s", brace);
	return !strcmp("[", brace);
}

static AssignmentReturnValue evaluateRangeComparisonAssignment(AssignmentReturnValue a, AssignmentReturnValue tRange) {
	int val = evaluateAssignmentReturnAsNumber(a);

	char openBrace[10], valString1[20], comma[10], valString2[20], closeBrace[10];
	sscanf(tRange.mValue, "%s %s %s %s %s", openBrace, valString1, comma, valString2, closeBrace);
	assert(!strcmp("[", openBrace));
	assert(strcmp("", valString1));
	assert(!strcmp(",", comma));
	assert(strcmp("", valString1));
	assert(!strcmp("]", closeBrace));

	int val1 = atoi(valString1);
	int val2 = atoi(valString2);

	return makeBooleanAssignmentReturn(val >= val1 && val <= val2);
}

static int isFloatReturn(AssignmentReturnValue tReturn) {
	if (strchr(tReturn.mValue, '.') == NULL) return 0;

	char* text = tReturn.mValue;
	int n = strlen(text);

	int i;
	for (i = 0; i < n; i++) {
		if (text[i] != '.' && (text[i] < '0' || text[i] > '9')) return 0;
	}

	return 1;
}


static AssignmentReturnValue evaluateSetVariableAssignment(DreamMugenAssignment* tAssignment, DreamPlayer* tPlayer) {
	DreamMugenDependOnTwoAssignment* varSetAssignment = tAssignment->mData;

	assert(varSetAssignment->a->mType == MUGEN_ASSIGNMENT_TYPE_ARRAY);
	DreamMugenDependOnTwoAssignment* varArrayAccess = varSetAssignment->a->mData;
	AssignmentReturnValue a = evaluateAssignmentInternal(varArrayAccess->a, tPlayer);
	int index = evaluateDreamAssignmentAndReturnAsInteger(varArrayAccess->b, tPlayer);

	if (!strcmp("var", a.mValue) || !strcmp("Var", a.mValue)) {
		int value = evaluateDreamAssignmentAndReturnAsInteger(varSetAssignment->b, tPlayer);
		setPlayerVariable(tPlayer, index, value);
		return makeNumberAssignmentReturn(value);
	}
	else if (!strcmp("fvar", a.mValue) || !strcmp("FVar", a.mValue)) {
		double value = evaluateDreamAssignmentAndReturnAsFloat(varSetAssignment->b, tPlayer);
		setPlayerFloatVariable(tPlayer, index, value);
		return makeFloatAssignmentReturn(value);
	}
	else if (!strcmp("sysvar", a.mValue) || !strcmp("SysVar", a.mValue)) {
		int value = evaluateDreamAssignmentAndReturnAsInteger(varSetAssignment->b, tPlayer);
		setPlayerSystemVariable(tPlayer, index, value);
		return makeNumberAssignmentReturn(value);
	}
	else if (!strcmp("sysfvar", a.mValue) || !strcmp("SysFVar", a.mValue)) {
		double value = evaluateDreamAssignmentAndReturnAsFloat(varSetAssignment->b, tPlayer);
		setPlayerSystemFloatVariable(tPlayer, index, value);
		return makeFloatAssignmentReturn(value);
	}
	else {
		logError("Unrecognized varset name.");
		logErrorString(a.mValue);
		abortSystem();
		return makeBooleanAssignmentReturn(0);
	}

}

static int evaluateSingleHitDefAttributeFlag2(char* tFlag, MugenAttackClass tClass, MugenAttackType tType) {
	turnStringLowercase(tFlag);

	assert(strlen(tFlag) == 2);

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
		logError("Unrecognized attack class");
		logErrorInteger(tClass);
		abortSystem();
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
		logError("Unrecognized attack type");
		logErrorInteger(tType);
		abortSystem();
		isPart2OK = 0;
	}

	return isPart2OK;
}

static AssignmentReturnValue evaluateHitDefAttributeAssignment(AssignmentReturnValue tValue, DreamPlayer* tPlayer) {

	if (!isHitDataActive(tPlayer)) {
		return makeBooleanAssignmentReturn(0);
	}

	char* pos = tValue.mValue;
	int positionsRead;
	char flag[10], comma[10];
	int items = sscanf(pos, "%s %s%n", flag, comma, &positionsRead);
	pos += positionsRead;

	turnStringLowercase(flag);

	int isFlag1OK;
	DreamMugenStateType type = getHitDataType(tPlayer);
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
		logError("Invalid hitdef type");
		logErrorInteger(type);
		abortSystem();
		isFlag1OK = 0;
	}

	if(!isFlag1OK) return makeBooleanAssignmentReturn(0);

	if(items == 1) return makeBooleanAssignmentReturn(isFlag1OK);
	assert(items == 2);
	assert(!strcmp(",", comma));

	int hasNext = 1;
	while (hasNext) {
		items = sscanf(pos, "%s %s%n", flag, comma, &positionsRead);
		pos += positionsRead;
		assert(items >= 1);

		int isFlag2OK = evaluateSingleHitDefAttributeFlag2(flag, getHitDataAttackClass(tPlayer), getHitDataAttackType(tPlayer));
		if (isFlag2OK) {
			return makeBooleanAssignmentReturn(1);
		}

		if (items == 1) hasNext = 0;
		else assert(!strcmp(",", comma));
	}

	return makeBooleanAssignmentReturn(0);
}


static AssignmentReturnValue evaluateComparisonAssignmentInternal(DreamMugenAssignment* mAssignment, AssignmentReturnValue b, DreamPlayer* tPlayer) {
	char name[MUGEN_DEF_STRING_LENGTH];
	
	if (mAssignment->mType == MUGEN_ASSIGNMENT_TYPE_VARIABLE) {
		DreamMugenVariableAssignment* var = mAssignment->mData;
		strcpy(name, var->mName);
		turnStringLowercase(name);
	}
	else {
		name[0] = '\0';
	}

	AssignmentReturnValue a = evaluateAssignmentInternal(mAssignment, tPlayer);

	if (!strcmp("command", name)) {
		return evaluateCommandAssignment(b, tPlayer);
	}
	else if (!strcmp("statetype", name)) {
		return evaluateStateTypeAssignment(b, tPlayer);
	}
	else if (!strcmp("p2statetype", name)) {
		return evaluateStateTypeAssignment(b, getPlayerOtherPlayer(tPlayer));
	}
	else if (!strcmp("movetype", name)) {
		return evaluateMoveTypeAssignment(b, tPlayer);
	}
	else if (!strcmp("p2movetype", name)) {
		return evaluateMoveTypeAssignment(b, getPlayerOtherPlayer(tPlayer));
	}
	else if (!strcmp("animelem", name)) {
		return evaluateAnimElemAssignment(b, tPlayer);
	}
	else if (!strcmp("timemod", name)) {
		return evaluateTimeModAssignment(b, tPlayer);
	}
	else if (!strcmp("teammode", name)) {
		return evaluateTeamModeAssignment(b, tPlayer);
	}
	else if (!strcmp("hitdefattr", name)) {
		return evaluateHitDefAttributeAssignment(b, tPlayer);
	}
	else if (isRangeAssignmentReturn(b)) {
		return evaluateRangeComparisonAssignment(a, b);
	}
	else if (isFloatReturn(a) || isFloatReturn(b)) {
		int value = evaluateAssignmentReturnAsFloat(a) == evaluateAssignmentReturnAsFloat(b);
		return makeBooleanAssignmentReturn(value);
	}
	else {

		int value = !strcmp(a.mValue, b.mValue);
		return makeBooleanAssignmentReturn(value);
	}
}

static AssignmentReturnValue evaluateComparisonAssignment(DreamMugenAssignment* tAssignment, DreamPlayer* tPlayer) {
	DreamMugenDependOnTwoAssignment* comparisonAssignment = tAssignment->mData;
	//printf("eval assign a\n");

	AssignmentReturnValue b = evaluateAssignmentInternal(comparisonAssignment->b, tPlayer);

	if (comparisonAssignment->a->mType == MUGEN_ASSIGNMENT_TYPE_VECTOR) {
		DreamMugenDependOnTwoAssignment* vectorAssignment = comparisonAssignment->a->mData;
		AssignmentReturnValue vecA = evaluateAssignmentInternal(vectorAssignment->a, tPlayer);

		if (isPlayerAccessVectorAssignment(vecA, tPlayer)) {
			DreamPlayer* target = getPlayerFromFirstVectorPartOrNullIfNonexistant(vecA, tPlayer);
			assert(target);
			return evaluateComparisonAssignmentInternal(vectorAssignment->b, b, target);
		}
	}

	return evaluateComparisonAssignmentInternal(comparisonAssignment->a, b, tPlayer);
}

static AssignmentReturnValue evaluateInequalityAssignment(DreamMugenAssignment* tAssignment, DreamPlayer* tPlayer) {
	AssignmentReturnValue equal = evaluateComparisonAssignment(tAssignment, tPlayer);
	int val = evaluateAssignmentReturnAsBool(equal);

	return makeBooleanAssignmentReturn(!val);
}

static AssignmentReturnValue evaluateGreaterIntegers(AssignmentReturnValue a, AssignmentReturnValue b) {
	int val = evaluateAssignmentReturnAsNumber(a) > evaluateAssignmentReturnAsNumber(b);
	return makeBooleanAssignmentReturn(val);
}

static AssignmentReturnValue evaluateGreaterFloats(AssignmentReturnValue a, AssignmentReturnValue b) {
	int val = evaluateAssignmentReturnAsFloat(a) > evaluateAssignmentReturnAsFloat(b);
	return makeBooleanAssignmentReturn(val);
}

static AssignmentReturnValue evaluateGreaterAssignment(DreamMugenAssignment* tAssignment, DreamPlayer* tPlayer) {
	DreamMugenDependOnTwoAssignment* greaterAssignment = tAssignment->mData;
	AssignmentReturnValue a = evaluateAssignmentInternal(greaterAssignment->a, tPlayer);
	AssignmentReturnValue b = evaluateAssignmentInternal(greaterAssignment->b, tPlayer);

	if (isFloatReturn(a) || isFloatReturn(b)) {
		return evaluateGreaterFloats(a, b);
	}
	else {
		return evaluateGreaterIntegers(a, b);
	}
}

static AssignmentReturnValue evaluateGreaterOrEqualIntegers(AssignmentReturnValue a, AssignmentReturnValue b) {
	int val = evaluateAssignmentReturnAsNumber(a) >= evaluateAssignmentReturnAsNumber(b);
	return makeBooleanAssignmentReturn(val);
}

static AssignmentReturnValue evaluateGreaterOrEqualFloats(AssignmentReturnValue a, AssignmentReturnValue b) {
	int val = evaluateAssignmentReturnAsFloat(a) >= evaluateAssignmentReturnAsFloat(b);
	return makeBooleanAssignmentReturn(val);
}

static AssignmentReturnValue evaluateGreaterOrEqualAssignment(DreamMugenAssignment* tAssignment, DreamPlayer* tPlayer) {
	DreamMugenDependOnTwoAssignment* greaterOrEqualAssignment = tAssignment->mData;
	AssignmentReturnValue a = evaluateAssignmentInternal(greaterOrEqualAssignment->a, tPlayer);
	AssignmentReturnValue b = evaluateAssignmentInternal(greaterOrEqualAssignment->b, tPlayer);

	if (isFloatReturn(a) || isFloatReturn(b)) {
		return evaluateGreaterOrEqualFloats(a, b);
	}
	else {
		return evaluateGreaterOrEqualIntegers(a, b);
	}
}

static AssignmentReturnValue evaluateLessIntegers(AssignmentReturnValue a, AssignmentReturnValue b) {
	int val = evaluateAssignmentReturnAsNumber(a) < evaluateAssignmentReturnAsNumber(b);
	return makeBooleanAssignmentReturn(val);
}

static AssignmentReturnValue evaluateLessFloats(AssignmentReturnValue a, AssignmentReturnValue b) {
	int val = evaluateAssignmentReturnAsFloat(a) < evaluateAssignmentReturnAsFloat(b);
	return makeBooleanAssignmentReturn(val);
}

static AssignmentReturnValue evaluateLessAssignment(DreamMugenAssignment* tAssignment, DreamPlayer* tPlayer) {
	DreamMugenDependOnTwoAssignment* lessAssignment = tAssignment->mData;
	AssignmentReturnValue a = evaluateAssignmentInternal(lessAssignment->a, tPlayer);
	AssignmentReturnValue b = evaluateAssignmentInternal(lessAssignment->b, tPlayer);

	if (isFloatReturn(a) || isFloatReturn(b)) {
		return evaluateLessFloats(a, b);
	}
	else {
		return evaluateLessIntegers(a, b);
	}
}

static AssignmentReturnValue evaluateLessOrEqualIntegers(AssignmentReturnValue a, AssignmentReturnValue b) {
	int val = evaluateAssignmentReturnAsNumber(a) <= evaluateAssignmentReturnAsNumber(b);
	return makeBooleanAssignmentReturn(val);
}

static AssignmentReturnValue evaluateLessOrEqualFloats(AssignmentReturnValue a, AssignmentReturnValue b) {
	int val = evaluateAssignmentReturnAsFloat(a) <= evaluateAssignmentReturnAsFloat(b);
	return makeBooleanAssignmentReturn(val);
}

static AssignmentReturnValue evaluateLessOrEqualAssignment(DreamMugenAssignment* tAssignment, DreamPlayer* tPlayer) {
	DreamMugenDependOnTwoAssignment* lessOrEqualAssignment = tAssignment->mData;
	AssignmentReturnValue a = evaluateAssignmentInternal(lessOrEqualAssignment->a, tPlayer);
	AssignmentReturnValue b = evaluateAssignmentInternal(lessOrEqualAssignment->b, tPlayer);

	if (isFloatReturn(a) || isFloatReturn(b)) {
		return evaluateLessOrEqualFloats(a, b);
	}
	else {
		return evaluateLessOrEqualIntegers(a, b);
	}
}

static AssignmentReturnValue evaluateModuloIntegers(AssignmentReturnValue a, AssignmentReturnValue b) {
	int val = evaluateAssignmentReturnAsNumber(a) % evaluateAssignmentReturnAsNumber(b);
	return makeNumberAssignmentReturn(val);
}

static AssignmentReturnValue evaluateModuloAssignment(DreamMugenAssignment* tAssignment, DreamPlayer* tPlayer) {
	DreamMugenDependOnTwoAssignment* moduloAssignment = tAssignment->mData;
	AssignmentReturnValue a = evaluateAssignmentInternal(moduloAssignment->a, tPlayer);
	AssignmentReturnValue b = evaluateAssignmentInternal(moduloAssignment->b, tPlayer);

	if (isFloatReturn(a) || isFloatReturn(b)) {
		logError("Unable to parse modulo of floats.");
		logErrorString(a.mValue);
		logErrorString(b.mValue);
		abortSystem();
		return makeBooleanAssignmentReturn(0);
	}
	else {
		return evaluateModuloIntegers(a, b);
	}
}

static int powI(int a, int b) {
	assert(b >= 0);

	if (a == 0 && b == 0) return 1;

	int ret = 1;
	while (b--) ret *= a;

	return ret;
}

static AssignmentReturnValue evaluateExponentiationIntegers(AssignmentReturnValue a, AssignmentReturnValue b) {
	int val1 = evaluateAssignmentReturnAsNumber(a);
	int val2 = evaluateAssignmentReturnAsNumber(b);
	return makeNumberAssignmentReturn(powI(val1, val2));
}

static AssignmentReturnValue evaluateExponentiationFloats(AssignmentReturnValue a, AssignmentReturnValue b) {
	double val1 = evaluateAssignmentReturnAsFloat(a);
	double val2 = evaluateAssignmentReturnAsFloat(b);
	return makeFloatAssignmentReturn(pow(val1, val2));
}

static AssignmentReturnValue evaluateExponentiationAssignment(DreamMugenAssignment* tAssignment, DreamPlayer* tPlayer) {
	DreamMugenDependOnTwoAssignment* exponentiationAssignment = tAssignment->mData;
	AssignmentReturnValue a = evaluateAssignmentInternal(exponentiationAssignment->a, tPlayer);
	AssignmentReturnValue b = evaluateAssignmentInternal(exponentiationAssignment->b, tPlayer);

	if (isFloatReturn(a) || isFloatReturn(b) || evaluateAssignmentReturnAsNumber(b) < 0) {
		return evaluateExponentiationFloats(a, b);
	}
	else {
		return evaluateExponentiationIntegers(a, b);
	}
}


static AssignmentReturnValue evaluateMultiplicationIntegers(AssignmentReturnValue a, AssignmentReturnValue b) {
	int val = evaluateAssignmentReturnAsNumber(a) * evaluateAssignmentReturnAsNumber(b);
	return makeNumberAssignmentReturn(val);
}

static AssignmentReturnValue evaluateMultiplicationFloats(AssignmentReturnValue a, AssignmentReturnValue b) {
	double val = evaluateAssignmentReturnAsFloat(a) * evaluateAssignmentReturnAsFloat(b);
	return makeFloatAssignmentReturn(val);
}

static AssignmentReturnValue evaluateMultiplicationAssignment(DreamMugenAssignment* tAssignment, DreamPlayer* tPlayer) {
	DreamMugenDependOnTwoAssignment* multiplicationAssignment = tAssignment->mData;
	AssignmentReturnValue a = evaluateAssignmentInternal(multiplicationAssignment->a, tPlayer);
	AssignmentReturnValue b = evaluateAssignmentInternal(multiplicationAssignment->b, tPlayer);

	if (isFloatReturn(a) || isFloatReturn(b)) {
		return evaluateMultiplicationFloats(a, b);
	}
	else {
		return evaluateMultiplicationIntegers(a, b);
	}
}

static AssignmentReturnValue evaluateDivisionIntegers(AssignmentReturnValue a, AssignmentReturnValue b) {
	int val = evaluateAssignmentReturnAsNumber(a) / evaluateAssignmentReturnAsNumber(b);
	return makeNumberAssignmentReturn(val);
}

static AssignmentReturnValue evaluateDivisionFloats(AssignmentReturnValue a, AssignmentReturnValue b) {
	double val = evaluateAssignmentReturnAsFloat(a) / evaluateAssignmentReturnAsFloat(b);
	return makeFloatAssignmentReturn(val);
}

static AssignmentReturnValue evaluateDivisionAssignment(DreamMugenAssignment* tAssignment, DreamPlayer* tPlayer) {
	DreamMugenDependOnTwoAssignment* divisionAssignment = tAssignment->mData;
	AssignmentReturnValue a = evaluateAssignmentInternal(divisionAssignment->a, tPlayer);
	AssignmentReturnValue b = evaluateAssignmentInternal(divisionAssignment->b, tPlayer);

	if (isFloatReturn(a) || isFloatReturn(b)) {
		return evaluateDivisionFloats(a, b);
	}
	else {
		return evaluateDivisionIntegers(a, b);
	}
}

static int isSparkFileReturn(AssignmentReturnValue a) {
	char firstW[200];
	int items = sscanf(a.mValue, "%s", firstW);
	if (!items) return 0;

	return !strcmp("isinotherfile", firstW);
}

static AssignmentReturnValue evaluateAdditionSparkFile(AssignmentReturnValue a, AssignmentReturnValue b) {
	char firstW[200];
	int val1;

	int items = sscanf(a.mValue, "%s %d", firstW, &val1);
	assert(items == 2);

	int val2 = evaluateAssignmentReturnAsNumber(b);

	AssignmentReturnValue ret;
	sprintf(ret.mValue, "isinotherfile %d", val1+val2);

	return ret;
}


static AssignmentReturnValue evaluateAdditionIntegers(AssignmentReturnValue a, AssignmentReturnValue b) {
	int val = evaluateAssignmentReturnAsNumber(a) + evaluateAssignmentReturnAsNumber(b);
	return makeNumberAssignmentReturn(val);
}

static AssignmentReturnValue evaluateAdditionFloats(AssignmentReturnValue a, AssignmentReturnValue b) {
	double val = evaluateAssignmentReturnAsFloat(a) + evaluateAssignmentReturnAsFloat(b);
	return makeFloatAssignmentReturn(val);
}

static AssignmentReturnValue evaluateAdditionAssignment(DreamMugenAssignment* tAssignment, DreamPlayer* tPlayer) {
	DreamMugenDependOnTwoAssignment* additionAssignment = tAssignment->mData;
	AssignmentReturnValue a = evaluateAssignmentInternal(additionAssignment->a, tPlayer);
	AssignmentReturnValue b = evaluateAssignmentInternal(additionAssignment->b, tPlayer);

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
	int val = evaluateAssignmentReturnAsNumber(a) - evaluateAssignmentReturnAsNumber(b);
	return makeNumberAssignmentReturn(val);
}

static AssignmentReturnValue evaluateSubtractionFloats(AssignmentReturnValue a, AssignmentReturnValue b) {
	double val = evaluateAssignmentReturnAsFloat(a) - evaluateAssignmentReturnAsFloat(b);
	return makeFloatAssignmentReturn(val);
}

static AssignmentReturnValue evaluateSubtractionAssignment(DreamMugenAssignment* tAssignment, DreamPlayer* tPlayer) {
	DreamMugenDependOnTwoAssignment* subtractionAssignment = tAssignment->mData;
	AssignmentReturnValue a = evaluateAssignmentInternal(subtractionAssignment->a, tPlayer);
	AssignmentReturnValue b = evaluateAssignmentInternal(subtractionAssignment->b, tPlayer);

	if (isFloatReturn(a) || isFloatReturn(b)) {
		return evaluateSubtractionFloats(a, b);
	}
	else {
		return evaluateSubtractionIntegers(a, b);
	}
}


static AssignmentReturnValue evaluateOperatorArgumentAssignment(DreamMugenAssignment* tAssignment, DreamPlayer* tPlayer) {
	DreamMugenDependOnTwoAssignment* operatorAssignment = tAssignment->mData;
	AssignmentReturnValue a = evaluateAssignmentInternal(operatorAssignment->a, tPlayer);
	AssignmentReturnValue b = evaluateAssignmentInternal(operatorAssignment->b, tPlayer);

	AssignmentReturnValue ret;
	sprintf(ret.mValue, "%s %s", a.mValue, b.mValue);
	return ret;
}

static AssignmentReturnValue evaluateBooleanAssignment(DreamMugenAssignment* tAssignment) {
	DreamMugenFixedBooleanAssignment* fixedAssignment = tAssignment->mData;

	return makeBooleanAssignmentReturn(fixedAssignment->mValue);
}

static AssignmentReturnValue evaluateNumberAssignment(DreamMugenAssignment* tAssignment) {
	DreamMugenNumberAssignment* number = tAssignment->mData;

	return makeNumberAssignmentReturn(number->mValue);
}

static AssignmentReturnValue evaluateFloatAssignment(DreamMugenAssignment* tAssignment) {
	DreamMugenFloatAssignment* f = tAssignment->mData;

	return makeFloatAssignmentReturn(f->mValue);
}

static AssignmentReturnValue evaluateStringAssignment(DreamMugenAssignment* tAssignment) {
	DreamMugenStringAssignment* s = tAssignment->mData;

	return makeStringAssignmentReturn(s->mValue);
}

static AssignmentReturnValue evaluatePlayerVectorAssignment(AssignmentReturnValue tFirstValue, DreamMugenDependOnTwoAssignment* tVectorAssignment, DreamPlayer* tPlayer) {
	DreamPlayer* target = getPlayerFromFirstVectorPartOrNullIfNonexistant(tFirstValue, tPlayer);
	assert(target != NULL);
	assert(tVectorAssignment->b->mType == MUGEN_ASSIGNMENT_TYPE_VARIABLE || tVectorAssignment->b->mType == MUGEN_ASSIGNMENT_TYPE_ARRAY);

	return evaluateAssignmentInternal(tVectorAssignment->b, target);
}

static AssignmentReturnValue evaluateVectorAssignment(DreamMugenAssignment* tAssignment, DreamPlayer* tPlayer) {
	DreamMugenDependOnTwoAssignment* vectorAssignment = tAssignment->mData;
	AssignmentReturnValue a = evaluateAssignmentInternal(vectorAssignment->a, tPlayer);

	if (isPlayerAccessVectorAssignment(a, tPlayer)) {
		return evaluatePlayerVectorAssignment(a, vectorAssignment, tPlayer);
	}
	else {
		AssignmentReturnValue b = evaluateAssignmentInternal(vectorAssignment->b, tPlayer);

		AssignmentReturnValue ret;
		sprintf(ret.mValue, "%s , %s", a.mValue, b.mValue);
		return ret;
	}
}

static AssignmentReturnValue evaluateRangeAssignment(DreamMugenAssignment* tAssignment, DreamPlayer* tPlayer) {
	DreamMugenRangeAssignment* rangeAssignment = tAssignment->mData;
	AssignmentReturnValue a = evaluateAssignmentInternal(rangeAssignment->a, tPlayer);

	char valString1[100], comma[10], valString2[100];
	sscanf(a.mValue, "%s %s %s", valString1, comma, valString2);
	assert(strcmp("", valString1));
	assert(!strcmp(",", comma));
	assert(strcmp("", valString2));

	int val1 = atoi(valString1);
	int val2 = atoi(valString2);
	if (rangeAssignment->mExcludeLeft) val1++;
	if (rangeAssignment->mExcludeRight) val2--;

	AssignmentReturnValue ret;
	sprintf(ret.mValue, "[ %d , %d ]", val1, val2);
	return ret;
}

static AssignmentReturnValue evaluateVariableAssignment(DreamMugenAssignment* tAssignment, DreamPlayer* tPlayer) {
	DreamMugenVariableAssignment* variable = tAssignment->mData;
	char testString[100];
	strcpy(testString, variable->mName);
	turnStringLowercase(testString);

	if (!strcmp("stateno", testString)) {
		return makeNumberAssignmentReturn(getPlayerState(tPlayer));
	}
	else if (!strcmp("p2stateno", testString)) {
		return makeNumberAssignmentReturn(getPlayerState(getPlayerOtherPlayer(tPlayer)));
	}
	else if (!strcmp("prevstateno", testString)) {
		return makeNumberAssignmentReturn(getPlayerPreviousState(tPlayer));
	}
	else if (!strcmp("statetype", testString)) {
		return makeStringAssignmentReturn("statetype");
	}
	else if (!strcmp("p2statetype", testString)) {
		return makeStringAssignmentReturn("p2statetype");
	}
	else if (!strcmp("movetype", testString)) {
		return makeStringAssignmentReturn("movetype");
	}
	else if (!strcmp("p2movetype", testString)) {
		return makeStringAssignmentReturn("p2movetype");
	}
	else if (!strcmp("ctrl", testString)) {
		return makeBooleanAssignmentReturn(getPlayerControl(tPlayer));
	}
	else if (!strcmp("movecontact", testString)) {
		return makeNumberAssignmentReturn(getPlayerMoveContactCounter(tPlayer));
	}
	else if (!strcmp("hitdefattr", testString)) {
		return makeStringAssignmentReturn("hitdefattr");
	}
	else if (!strcmp("mf", testString) || !strcmp("af", testString) || !strcmp("la", testString) || !strcmp("lma", testString) || !strcmp("lam", testString) || !strcmp("map", testString) || !strcmp("ap", testString) || !strcmp("aa", testString) || !strcmp("miss", testString) || !strcmp("m-", testString) || !strcmp("a-", testString) || !strcmp("nt", testString) || !strcmp("hp", testString) || !strcmp("n", testString) || !strcmp("heavy", testString) || !strcmp("sp", testString) || !strcmp("at", testString) || !strcmp("sca", testString) || !strcmp("h", testString) || !strcmp("i", testString) || !strcmp("a", testString) || !strcmp("m", testString) || !strcmp("trip", testString) || !strcmp("l", testString) || !strcmp("mafd", testString) || !strcmp("med", testString) || !strcmp("hit", testString) || !strcmp("light", testString) || !strcmp("high", testString) || !strcmp("low", testString) || !strcmp("medium", testString) || !strcmp("maf", testString) || !strcmp("ma", testString) || !strcmp("na", testString) || !strcmp("sc", testString) || !strcmp("sa", testString) || !strcmp("ha", testString) || !strcmp("s", testString) || !strcmp("a", testString) || !strcmp("c", testString)) {
		return makeStringAssignmentReturn(variable->mName); //TODO
	}
	else if (!strcmp("var", testString) || !strcmp("sysvar", testString) || !strcmp("sysfvar", testString) || !strcmp("fvar", testString)) {
		return makeStringAssignmentReturn(testString);
	}
	else if (!strcmp("time", testString) || !strcmp("statetime", testString)) {
		return makeNumberAssignmentReturn(getPlayerTimeInState(tPlayer));
	}
	else if (!strcmp("command", testString)) {
		return makeStringAssignmentReturn("command");
	}
	else if (!strcmp("anim", testString)) {
		return makeNumberAssignmentReturn(getPlayerAnimationNumber(tPlayer));
	}
	else if (!strcmp("animtime", testString)) {
		return makeNumberAssignmentReturn(getRemainingPlayerAnimationTime(tPlayer));
	}
	else if (!strcmp("abs", testString)) {
		return makeStringAssignmentReturn("abs");
	}
	else if (!strcmp("exp", testString)) {
		return makeStringAssignmentReturn("exp");
	}
	else if (!strcmp("ln", testString)) {
		return makeStringAssignmentReturn("ln");
	}
	else if (!strcmp("cos", testString)) {
		return makeStringAssignmentReturn("cos");
	}
	else if (!strcmp("sin", testString)) {
		return makeStringAssignmentReturn("sin");
	}
	else if (!strcmp("pi", testString)) {
		return makeFloatAssignmentReturn(M_PI);
	}
	else if (!strcmp("floor", testString)) {
		return makeStringAssignmentReturn("floor");
	}
	else if (!strcmp("ceil", testString)) {
		return makeStringAssignmentReturn("ceil");
	}
	else if (!strcmp("const", testString)) {
		return makeStringAssignmentReturn("const");
	}
	else if (!strcmp("f", testString) || !strcmp("s", testString)) {
		return makeStringAssignmentReturn(variable->mName);
	}
	else if (!strcmp("gethitvar", testString)) {
		return makeStringAssignmentReturn("gethitvar");
	}
	else if (!strcmp("ifelse", testString)) {
		return makeStringAssignmentReturn("ifelse");
	}
	else if (!strcmp("sifelse", testString)) {
		return makeStringAssignmentReturn("sifelse"); // TODO: what is this
	}
	else if (!strcmp("cond", testString)) {
		return makeStringAssignmentReturn("cond");
	}
	else if (!strcmp("vel x", testString)) {
		return makeFloatAssignmentReturn(getPlayerVelocityX(tPlayer, getPlayerCoordinateP(tPlayer)));
	}
	else if (!strcmp("vel y", testString)) {
		return makeFloatAssignmentReturn(getPlayerVelocityY(tPlayer, getPlayerCoordinateP(tPlayer)));
	}
	else if (!strcmp("pos x", testString)) {
		return makeFloatAssignmentReturn(getPlayerPositionBasedOnScreenCenterX(tPlayer, getPlayerCoordinateP(tPlayer)));
	}
	else if (!strcmp("pos y", testString)) {
		return makeFloatAssignmentReturn(getPlayerPositionBasedOnStageFloorY(tPlayer, getPlayerCoordinateP(tPlayer)));
	}
	else if (!strcmp("alive", testString)) {
		return makeBooleanAssignmentReturn(isPlayerAlive(tPlayer));
	}
	else if (!strcmp("hitshakeover", testString)) {
		return makeBooleanAssignmentReturn(isPlayerHitShakeOver(tPlayer));
	}
	else if (!strcmp("hitover", testString)) {
		return makeBooleanAssignmentReturn(isPlayerHitOver(tPlayer));
	}
	else if (!strcmp("hitfall", testString)) {
		return makeBooleanAssignmentReturn(isPlayerFalling(tPlayer));
	}
	else if (!strcmp("canrecover", testString)) {
		return makeBooleanAssignmentReturn(canPlayerRecoverFromFalling(tPlayer));
	}
	else if (!strcmp("camerapos x", testString)) {
		return makeFloatAssignmentReturn(getDreamCameraPositionX(getPlayerCoordinateP(tPlayer)));
	}
	else if (!strcmp("animelem", testString)) {
		return makeStringAssignmentReturn("animelem");
	}
	else if (!strcmp("animelemno", testString)) {
		return makeStringAssignmentReturn("animelemno");
	}
	else if (!strcmp("animelemtime", testString)) {
		return makeStringAssignmentReturn("animelemtime");
	}
	else if (!strcmp("timemod", testString)) {
		return makeStringAssignmentReturn("timemod");
	}
	else if (!strcmp("selfanimexist", testString)) {
		return makeStringAssignmentReturn("selfanimexist");
	}
	else if (!strcmp("const240p", testString)) {
		return makeStringAssignmentReturn("const240p");
	}
	else if (!strcmp("const720p", testString)) {
		return makeStringAssignmentReturn("const720p");
	}
	else if (!strcmp("nowalk", testString)) {
		return makeStringAssignmentReturn("nowalk");
	}
	else if (!strcmp("nojugglecheck", testString)) {
		return makeStringAssignmentReturn("nojugglecheck");
	}
	else if (!strcmp("noautoturn", testString)) {
		return makeStringAssignmentReturn("NoAutoTurn");
	}
	else if (!strcmp("noshadow", testString)) {
		return makeStringAssignmentReturn("noshadow");
	}
	else if (!strcmp("noairguard", testString)) {
		return makeStringAssignmentReturn("noairguard");
	}
	else if (!strcmp("nobardisplay", testString)) {
		return makeStringAssignmentReturn("nobardisplay");
	}
	else if (!strcmp("intro", testString)) {
		return makeStringAssignmentReturn("intro");
	}
	else if (!strcmp("gametime", testString)) {
		return makeNumberAssignmentReturn(getDreamGameTime());
	}
	else if (!strcmp("numtarget", testString)) {
		return makeNumberAssignmentReturn(getPlayerTargetAmount(tPlayer));
	}
	else if (!strcmp("target", testString)) {
		return makeStringAssignmentReturn("target");
	}
	else if (!strcmp("root", testString)) {
		return makeStringAssignmentReturn("root");
	}
	else if (!strcmp("hitpausetime", testString)) {
		return makeNumberAssignmentReturn(getPlayerTimeLeftInHitPause(tPlayer));
	}
	else if (!strcmp("frontedgebodydist", testString)) {
		return makeFloatAssignmentReturn(getPlayerFrontDistanceToScreen(tPlayer));
	}
	else if (!strcmp("backedgebodydist", testString)) {
		return makeFloatAssignmentReturn(getPlayerBackDistanceToScreen(tPlayer));
	}
	else if (!strcmp("frontedgedist", testString)) {
		return makeFloatAssignmentReturn(getPlayerFrontAxisDistanceToScreen(tPlayer));
	}
	else if (!strcmp("backedgedist", testString)) {
		return makeFloatAssignmentReturn(getPlayerBackAxisDistanceToScreen(tPlayer));
	}
	else if (!strcmp("id", testString)) {
		return makeNumberAssignmentReturn(tPlayer->mRootID); // TODO: fix
	}
	else if (!strcmp("p1", testString)) {
		return makeStringAssignmentReturn("p1");
	}
	else if (!strcmp("p2", testString)) {
		return makeStringAssignmentReturn("p2");
	}
	else if (!strcmp("enemynear", testString)) {
		return makeStringAssignmentReturn("enemynear");
	}
	else if (!strcmp("enemy", testString)) {
		return makeStringAssignmentReturn("enemy");
	}
	else if (!strcmp("parent", testString)) {
		return makeStringAssignmentReturn("parent");
	}
	else if (!strcmp("numenemy", testString)) {
		return makeNumberAssignmentReturn(1); // TODO: fix if necessary
	}
	else if (!strcmp("numexplod", testString)) {
		return makeNumberAssignmentReturn(getExplodAmount(tPlayer));
	}
	else if (!strcmp("p2bodydist x", testString)) {
		return makeFloatAssignmentReturn(getPlayerDistanceToFrontOfOtherPlayerX(tPlayer));
	}
	else if (!strcmp("p2bodydist y", testString)) {
		return makeFloatAssignmentReturn(getPlayerAxisDistanceY(tPlayer));
	}
	else if (!strcmp("p2dist x", testString)) {
		return makeFloatAssignmentReturn(getPlayerAxisDistanceX(tPlayer));
	}
	else if (!strcmp("p2dist y", testString)) {
		return makeFloatAssignmentReturn(getPlayerAxisDistanceY(tPlayer));
	}
	else if (!strcmp("rootdist x", testString)) {
		return makeFloatAssignmentReturn(getPlayerDistanceToRootX(tPlayer));
	}
	else if (!strcmp("rootdist y", testString)) {
		return makeFloatAssignmentReturn(getPlayerDistanceToRootY(tPlayer));
	}
	else if (!strcmp("random", testString)) {
		return makeNumberAssignmentReturn(randfromInteger(0, 999));
	}
	else if (!strcmp("winko", testString)) {
		return makeBooleanAssignmentReturn(hasPlayerWonByKO(tPlayer));
	}
	else if (!strcmp("loseko", testString)) {
		return makeBooleanAssignmentReturn(hasPlayerLostByKO(tPlayer));
	}
	else if (!strcmp("winperfect", testString)) {
		return makeBooleanAssignmentReturn(hasPlayerWonPerfectly(tPlayer));
	}
	else if (!strcmp("win", testString)) {
		return makeBooleanAssignmentReturn(hasPlayerWon(tPlayer));
	}
	else if (!strcmp("lose", testString)) {
		return makeBooleanAssignmentReturn(hasPlayerLost(tPlayer));
	}
	else if (!strcmp("numhelper", testString)) {
		return makeNumberAssignmentReturn(getPlayerHelperAmount(tPlayer));
	}
	else if (!strcmp("numproj", testString)) {
		return makeNumberAssignmentReturn(getPlayerProjectileAmount(tPlayer));
	}
	else if (!strcmp("numprojid", testString)) {
		return makeStringAssignmentReturn("numprojid");
	}
	else if (!strcmp("movehit", testString)) {
		return makeNumberAssignmentReturn(hasPlayerMoveHitOtherPlayer(tPlayer));
	}
	else if (!strcmp("movereversed", testString)) {
		return makeNumberAssignmentReturn(hasPlayerMoveBeenReversedByOtherPlayer(tPlayer));
	}
	else if (!strcmp("helper", testString)) {
		return makeStringAssignmentReturn("helper");
	}
	else if (!strcmp("playerid", testString)) {
		return makeStringAssignmentReturn("playerid");
	}
	else if (!strcmp("playeridexist", testString)) {
		return makeStringAssignmentReturn("playeridexist");
	}
	else if (!strcmp("roundstate", testString)) {
		return makeNumberAssignmentReturn(getDreamRoundStateNumber());
	}
	else if (!strcmp("roundsexisted", testString)) {
		return makeNumberAssignmentReturn(getPlayerRoundsExisted(tPlayer));
	}
	else if (!strcmp("hitcount", testString)) {
		return makeStringAssignmentReturn("hitcount");
	}
	else if (!strcmp("damage", testString)) {
		return makeStringAssignmentReturn("damage");
	}
	else if (!strcmp("fallcount", testString)) {
		return makeStringAssignmentReturn("fallcount");
	}
	else if (!strcmp("ailevel", testString)) {
		return makeNumberAssignmentReturn(getPlayerAILevel(tPlayer));
	}
	else if (!strcmp("life", testString)) {
		return makeNumberAssignmentReturn(getPlayerLife(tPlayer));
	}
	else if (!strcmp("p2life", testString)) {
		return makeNumberAssignmentReturn(getPlayerLife(getPlayerOtherPlayer(tPlayer)));
	}
	else if (!strcmp("lifemax", testString)) {
		return makeNumberAssignmentReturn(getPlayerLifeMax(tPlayer));
	}
	else if (!strcmp("power", testString)) {
		return makeNumberAssignmentReturn(getPlayerPower(tPlayer));
	}
	else if (!strcmp("powermax", testString)) {
		return makeNumberAssignmentReturn(getPlayerPowerMax(tPlayer));
	}
	else if (!strcmp("inguarddist", testString)) {
		return makeBooleanAssignmentReturn(isPlayerInGuardDistance(tPlayer));
	}
	else if (!strcmp("ishelper", testString)) {
		return makeBooleanAssignmentReturn(isPlayerHelper(tPlayer));
	}
	else if (!strcmp("matchover", testString)) {
		return makeBooleanAssignmentReturn(isDreamMatchOver(tPlayer));
	}
	else if (!strcmp("roundno", testString)) {
		return makeNumberAssignmentReturn(getDreamRoundNumber());
	}
	else if (!strcmp("matchno", testString)) {
		return makeNumberAssignmentReturn(getDreamMatchNumber());
	}
	else if (!strcmp("palno", testString)) {
		return makeNumberAssignmentReturn(getPlayerPaletteNumber(tPlayer));
	}
	else if (!strcmp("gamewidth", testString)) {
		return makeNumberAssignmentReturn(getDreamGameWidth(getPlayerCoordinateP(tPlayer)));
	}
	else if (!strcmp("teamside", testString)) {
		return makeNumberAssignmentReturn(tPlayer->mRootID + 1);
	}
	else if (!strcmp("teammode", testString)) {
		return makeStringAssignmentReturn("teammode");
	}
	else if (!strcmp("name", testString)) {
		return makeStringAssignmentReturn(getPlayerName(tPlayer));
	}
	else if (!strcmp("p2name", testString)) {
		return makeStringAssignmentReturn(getPlayerName(getPlayerOtherPlayer(tPlayer)));
	}
	else if (!strcmp("p3name", testString)) {
		return makeStringAssignmentReturn(""); // TODO
	}
	else if (!strcmp("p4name", testString)) {
		return makeStringAssignmentReturn(""); // TODO
	}
	else if (!strcmp("authorname", testString)) {
		return makeStringAssignmentReturn(getPlayerAuthorName(tPlayer));
	}
	else if (!strcmp("data.fall.defence_mul", testString)) {
		return makeStringAssignmentReturn("data.fall.defence_mul");
	}
	else if (!strcmp("data.power", testString)) {
		return makeStringAssignmentReturn("data.power");
	}
	else if (!strcmp("size.ground.back", testString)) {
		return makeStringAssignmentReturn("size.ground.back");
	}
	else if (!strcmp("size.ground.front", testString)) {
		return makeStringAssignmentReturn("size.ground.front");
	}
	else if (!strcmp("size.height", testString)) {
		return makeStringAssignmentReturn("size.height");
	}
	else if (!strcmp("size.head.pos.x", testString)) {
		return makeStringAssignmentReturn("size.head.pos.x");
	}
	else if (!strcmp("size.head.pos.y", testString)) {
		return makeStringAssignmentReturn("size.head.pos.y");
	}
	else if (!strcmp("size.mid.pos.x", testString)) {
		return makeStringAssignmentReturn("size.mid.pos.x");
	}
	else if (!strcmp("size.mid.pos.y", testString)) {
		return makeStringAssignmentReturn("size.mid.pos.y");
	}
	else if (!strcmp("size.xscale", testString)) {
		return makeStringAssignmentReturn("size.xscale");
	}
	else if (!strcmp("size.yscale", testString)) {
		return makeStringAssignmentReturn("size.yscale");
	}
	else if (!strcmp("movement.stand.friction.threshold", testString)) {
		return makeStringAssignmentReturn("movement.stand.friction.threshold");
	}
	else if (!strcmp("movement.crouch.friction.threshold", testString)) {
		return makeStringAssignmentReturn("movement.crouch.friction.threshold");
	}
	else if (!strcmp("movement.air.gethit.groundrecover.groundlevel", testString)) {
		return makeStringAssignmentReturn("movement.air.gethit.groundrecover.groundlevel");
	}
	else if (!strcmp("movement.air.gethit.groundlevel", testString)) {
		return makeStringAssignmentReturn("movement.air.gethit.groundlevel");
	}
	else if (!strcmp("movement.air.gethit.groundrecover.ground.threshold", testString)) {
		return makeStringAssignmentReturn("movement.air.gethit.groundrecover.ground.threshold");
	}
	else if (!strcmp("movement.air.gethit.airrecover.threshold", testString)) {
		return makeStringAssignmentReturn("movement.air.gethit.airrecover.threshold");
	}
	else if (!strcmp("movement.air.gethit.trip.groundlevel", testString)) {
		return makeStringAssignmentReturn("movement.air.gethit.trip.groundlevel");
	}
	else if (!strcmp("movement.down.bounce.offset.x", testString)) {
		return makeStringAssignmentReturn("movement.down.bounce.offset.x");
	}
	else if (!strcmp("movement.down.bounce.offset.y", testString)) {
		return makeStringAssignmentReturn("movement.down.bounce.offset.y");
	}
	else if (!strcmp("movement.down.bounce.yaccel", testString)) {
		return makeStringAssignmentReturn("movement.down.bounce.yaccel");
	}
	else if (!strcmp("movement.down.bounce.groundlevel", testString)) {
		return makeStringAssignmentReturn("movement.down.bounce.groundlevel");
	}
	else if (!strcmp("movement.down.friction.threshold", testString)) {
		return makeStringAssignmentReturn("movement.down.friction.threshold");
	}
	else if (!strcmp("movement.yaccel", testString)) {
		return makeStringAssignmentReturn("movement.yaccel");
	}
	else if (!strcmp("velocity.walk.fwd.x", testString)) {
		return makeStringAssignmentReturn("velocity.walk.fwd.x");
	}
	else if (!strcmp("velocity.walk.back.x", testString)) {
		return makeStringAssignmentReturn("velocity.walk.back.x");
	}
	else if (!strcmp("velocity.run.fwd.x", testString)) {
		return makeStringAssignmentReturn("velocity.run.fwd.x");
	}
	else if (!strcmp("velocity.run.fwd.y", testString)) {
		return makeStringAssignmentReturn("velocity.run.fwd.y");
	}
	else if (!strcmp("velocity.run.back.x", testString)) {
		return makeStringAssignmentReturn("velocity.run.back.x");
	}
	else if (!strcmp("velocity.run.back.y", testString)) {
		return makeStringAssignmentReturn("velocity.run.back.y");
	}
	else if (!strcmp("velocity.runjump.fwd.x", testString)) {
		return makeStringAssignmentReturn("velocity.runjump.fwd.x");
	}
	else if (!strcmp("velocity.jump.neu.x", testString)) {
		return makeStringAssignmentReturn("velocity.jump.neu.x");
	}
	else if (!strcmp("velocity.jump.fwd.x", testString)) {
		return makeStringAssignmentReturn("velocity.jump.fwd.x");
	}
	else if (!strcmp("velocity.jump.back.x", testString)) {
		return makeStringAssignmentReturn("velocity.jump.back.x");
	}
	else if (!strcmp("velocity.jump.y", testString)) {
		return makeStringAssignmentReturn("velocity.jump.y");
	}
	else if (!strcmp("velocity.airjump.neu.x", testString)) {
		return makeStringAssignmentReturn("velocity.airjump.neu.x");
	}
	else if (!strcmp("velocity.airjump.fwd.x", testString)) {
		return makeStringAssignmentReturn("velocity.airjump.fwd.x");
	}
	else if (!strcmp("velocity.airjump.back.x", testString)) {
		return makeStringAssignmentReturn("velocity.airjump.back.x");
	}
	else if (!strcmp("velocity.airjump.y", testString)) {
		return makeStringAssignmentReturn("velocity.airjump.y");
	}
	// TODO: move somewhere else
	else if (!strcmp("=", testString) || !strcmp(">", testString) || !strcmp("<", testString) || !strcmp("roundnotover", testString) || !strcmp("isbound", testString) || !strcmp("ctrltime", testString) || !strcmp("addalpha", testString) || !strcmp("diagup", testString) || !strcmp("turns", testString) || !strcmp("single", testString) || !strcmp("simul", testString) || !strcmp("right", testString) || !strcmp("left", testString) || !strcmp("invisible", testString) || !strcmp("back", testString) || !strcmp("<=", testString) || !strcmp(">=", testString) || !strcmp("up", testString) || !strcmp("hard", testString) || !strcmp("fall.recover", testString) || !strcmp("xvel", testString) || !strcmp("hitshaketime", testString) || !strcmp("airtype", testString) || !strcmp("facing", testString) || !strcmp("none", testString) || !strcmp("fall.yvel", testString) || !strcmp("slidetime", testString) || !strcmp("yaccel", testString) || !strcmp("fall", testString) || !strcmp("yvel", testString) || !strcmp("groundtype", testString) || !strcmp("animtype", testString)) {
		return makeStringAssignmentReturn(variable->mName);
	}
	else if (testString[0] == 's' || testString[0] == 'f') { // TODO: fix
		AssignmentReturnValue ret;
		sprintf(ret.mValue, "isinotherfile %s", testString + 1);
		return ret;
	}
	else if (doDreamAssignmentStringsBeginsWithPattern("projhit", testString)) { // TODO: fix
		return makeBooleanAssignmentReturn(0); // TODO: implement
	}
	else if (doDreamAssignmentStringsBeginsWithPattern("projcontact", testString)) { // TODO: fix
		return makeBooleanAssignmentReturn(0); // TODO: implement
	}
	else {
		logError("Unknown variable.");
		logErrorString(testString);
		abortSystem();
		return makeBooleanAssignmentReturn(0);
	}
}

static AssignmentReturnValue evaluateVarArrayAssignment(AssignmentReturnValue tIndex, DreamPlayer* tPlayer) {
	int id = evaluateAssignmentReturnAsNumber(tIndex);
	int val = getPlayerVariable(tPlayer, id);

	return makeNumberAssignmentReturn(val);
}

static AssignmentReturnValue evaluateSysVarArrayAssignment(AssignmentReturnValue tIndex, DreamPlayer* tPlayer) {
	int id = evaluateAssignmentReturnAsNumber(tIndex);
	int val = getPlayerSystemVariable(tPlayer, id);

	return makeNumberAssignmentReturn(val);
}

static AssignmentReturnValue evaluateFVarArrayAssignment(AssignmentReturnValue tIndex, DreamPlayer* tPlayer) {
	int id = evaluateAssignmentReturnAsNumber(tIndex);
	double val = getPlayerFloatVariable(tPlayer, id);

	return makeFloatAssignmentReturn(val);
}

static AssignmentReturnValue evaluateSysFVarArrayAssignment(AssignmentReturnValue tIndex, DreamPlayer* tPlayer) {
	int id = evaluateAssignmentReturnAsNumber(tIndex);
	double val = getPlayerSystemFloatVariable(tPlayer, id);

	return makeFloatAssignmentReturn(val);
}

static AssignmentReturnValue evaluateAbsArrayAssignment(AssignmentReturnValue tIndex) {
	if (isFloatReturn(tIndex)) {
		double val = evaluateAssignmentReturnAsFloat(tIndex);
		return makeFloatAssignmentReturn(fabs(val));
	}
	else {
		int val = evaluateAssignmentReturnAsNumber(tIndex);
		return makeNumberAssignmentReturn(abs(val));
	}
}

static AssignmentReturnValue evaluateExpArrayAssignment(AssignmentReturnValue tIndex) {
		double val = evaluateAssignmentReturnAsFloat(tIndex);
		return makeFloatAssignmentReturn(exp(val));
}

static AssignmentReturnValue evaluateNaturalLogArrayAssignment(AssignmentReturnValue tIndex) {
	double val = evaluateAssignmentReturnAsFloat(tIndex);
	return makeFloatAssignmentReturn(log(val));
}

static AssignmentReturnValue evaluateCosineArrayAssignment(AssignmentReturnValue tIndex) {
		double val = evaluateAssignmentReturnAsFloat(tIndex);
		return makeFloatAssignmentReturn(cos(val));
}

static AssignmentReturnValue evaluateSineArrayAssignment(AssignmentReturnValue tIndex) {
	double val = evaluateAssignmentReturnAsFloat(tIndex);
	return makeFloatAssignmentReturn(sin(val));
}

static AssignmentReturnValue evaluateFloorArrayAssignment(AssignmentReturnValue tIndex) {
	if (isFloatReturn(tIndex)) {
		double val = evaluateAssignmentReturnAsFloat(tIndex);
		return makeNumberAssignmentReturn((int)floor(val));
	}
	else {
		int val = evaluateAssignmentReturnAsNumber(tIndex);
		return makeNumberAssignmentReturn(val);
	}
}

static AssignmentReturnValue evaluateCeilArrayAssignment(AssignmentReturnValue tIndex) {
	if (isFloatReturn(tIndex)) {
		double val = evaluateAssignmentReturnAsFloat(tIndex);
		return makeNumberAssignmentReturn((int)ceil(val));
	}
	else {
		int val = evaluateAssignmentReturnAsNumber(tIndex);
		return makeNumberAssignmentReturn(val);
	}
}

static AssignmentReturnValue evaluateConstArrayAssignment(AssignmentReturnValue tIndex, DreamPlayer* tPlayer) {
	char* var = tIndex.mValue;


	if (!strcmp("data.fall.defence_mul", var)) {
		return makeFloatAssignmentReturn(getPlayerFallDefenseMultiplier(tPlayer));
	}
	else if (!strcmp("data.power", var)) {
		return makeNumberAssignmentReturn(getPlayerPowerMax(tPlayer));
	}
	else if (!strcmp("size.ground.back", var)) {
		return makeNumberAssignmentReturn(getPlayerGroundSizeBack(tPlayer));
	}
	else if (!strcmp("size.ground.front", var)) {
		return makeNumberAssignmentReturn(getPlayerGroundSizeFront(tPlayer));
	}
	else if (!strcmp("size.height", var)) {
		return makeNumberAssignmentReturn(getPlayerHeight(tPlayer));
	}
	else if (!strcmp("size.head.pos.x", var)) {
		return makeFloatAssignmentReturn(getPlayerHeadPositionX(tPlayer));
	}
	else if (!strcmp("size.head.pos.y", var)) {
		return makeFloatAssignmentReturn(getPlayerHeadPositionY(tPlayer));
	}
	else if (!strcmp("size.mid.pos.x", var)) {
		return makeFloatAssignmentReturn(getPlayerMiddlePositionX(tPlayer));
	}
	else if (!strcmp("size.mid.pos.y", var)) {
		return makeFloatAssignmentReturn(getPlayerMiddlePositionY(tPlayer));
	}
	else if (!strcmp("size.xscale", var)) {
		return makeFloatAssignmentReturn(getPlayerScaleX(tPlayer));
	}
	else if (!strcmp("size.yscale", var)) {
		return makeFloatAssignmentReturn(getPlayerScaleY(tPlayer));
	}
	else if (!strcmp("movement.stand.friction.threshold", var)) {
		return makeFloatAssignmentReturn(getPlayerStandFrictionThreshold(tPlayer));
	}
	else if (!strcmp("movement.crouch.friction.threshold", var)) {
		return makeFloatAssignmentReturn(getPlayerCrouchFrictionThreshold(tPlayer));
	}
	else if (!strcmp("movement.air.gethit.groundlevel", var)) {
		return makeFloatAssignmentReturn(getPlayerAirGetHitGroundLevelY(tPlayer));
	}
	else if (!strcmp("movement.air.gethit.groundrecover.groundlevel", var)) {
		return makeFloatAssignmentReturn(getPlayerAirGetHitGroundRecoveryGroundLevelY(tPlayer));
	}
	else if (!strcmp("movement.air.gethit.groundrecover.ground.threshold", var)) {
		return makeFloatAssignmentReturn(getPlayerAirGetHitGroundRecoveryGroundYTheshold(tPlayer));
	}
	else if (!strcmp("movement.air.gethit.airrecover.threshold", var)) {
		return makeFloatAssignmentReturn(getPlayerAirGetHitAirRecoveryVelocityYThreshold(tPlayer));
	}
	else if (!strcmp("movement.air.gethit.trip.groundlevel", var)) {
		return makeFloatAssignmentReturn(getPlayerAirGetHitTripGroundLevelY(tPlayer));
	}
	else if (!strcmp("movement.down.bounce.offset.x", var)) {
		return makeFloatAssignmentReturn(getPlayerDownBounceOffsetX(tPlayer));
	}
	else if (!strcmp("movement.down.bounce.offset.y", var)) {
		return makeFloatAssignmentReturn(getPlayerDownBounceOffsetY(tPlayer));
	}
	else if (!strcmp("movement.down.bounce.yaccel", var)) {
		return makeFloatAssignmentReturn(getPlayerDownVerticalBounceAcceleration(tPlayer));
	}
	else if (!strcmp("movement.down.bounce.groundlevel", var)) {
		return makeFloatAssignmentReturn(getPlayerDownBounceGroundLevel(tPlayer));
	}
	else if (!strcmp("movement.down.friction.threshold", var)) {
		return makeFloatAssignmentReturn(getPlayerLyingDownFrictionThreshold(tPlayer));
	}
	else if (!strcmp("movement.yaccel", var)) {
		return makeFloatAssignmentReturn(getPlayerVerticalAcceleration(tPlayer));
	}
	else if (!strcmp("velocity.walk.fwd.x", var)) {
		return makeFloatAssignmentReturn(getPlayerForwardWalkVelocityX(tPlayer));
	}
	else if (!strcmp("velocity.walk.back.x", var)) {
		return makeFloatAssignmentReturn(getPlayerBackwardWalkVelocityX(tPlayer));
	}
	else if (!strcmp("velocity.run.fwd.x", var)) {
		return makeFloatAssignmentReturn(getPlayerForwardRunVelocityX(tPlayer));
	}
	else if (!strcmp("velocity.run.fwd.y", var)) {
		return makeFloatAssignmentReturn(getPlayerForwardRunVelocityY(tPlayer));
	}
	else if (!strcmp("velocity.run.back.x", var)) {
		return makeFloatAssignmentReturn(getPlayerBackwardRunVelocityX(tPlayer));
	}
	else if (!strcmp("velocity.run.back.y", var)) {
		return makeFloatAssignmentReturn(getPlayerBackwardRunVelocityY(tPlayer));
	}
	else if (!strcmp("velocity.runjump.fwd.x", var)) {
		return makeFloatAssignmentReturn(getPlayerForwardRunJumpVelocityX(tPlayer));
	}
	else if (!strcmp("velocity.jump.neu.x", var)) {
		return makeFloatAssignmentReturn(getPlayerNeutralJumpVelocityX(tPlayer));
	}
	else if (!strcmp("velocity.jump.fwd.x", var)) {
		return makeFloatAssignmentReturn(getPlayerForwardJumpVelocityX(tPlayer));
	}
	else if (!strcmp("velocity.jump.back.x", var)) {
		return makeFloatAssignmentReturn(getPlayerBackwardJumpVelocityX(tPlayer));
	}
	else if (!strcmp("velocity.jump.y", var)) {
		return makeFloatAssignmentReturn(getPlayerJumpVelocityY(tPlayer));
	}
	else if (!strcmp("velocity.airjump.neu.x", var)) {
		return makeFloatAssignmentReturn(getPlayerNeutralAirJumpVelocityX(tPlayer));
	}
	else if (!strcmp("velocity.airjump.fwd.x", var)) {
		return makeFloatAssignmentReturn(getPlayerForwardAirJumpVelocityX(tPlayer));
	}
	else if (!strcmp("velocity.airjump.back.x", var)) {
		return makeFloatAssignmentReturn(getPlayerBackwardAirJumpVelocityX(tPlayer));
	}
	else if (!strcmp("velocity.airjump.y", var)) {
		return makeFloatAssignmentReturn(getPlayerAirJumpVelocityY(tPlayer));
	}
	else {
		logError("Unrecognized Constant");
		logErrorString(var);
		abortSystem();
		return makeBooleanAssignmentReturn(0);
	}
}

static AssignmentReturnValue evaluateGetHitVarArrayAssignment(AssignmentReturnValue tIndex, DreamPlayer* tPlayer) {
	char* var = tIndex.mValue;

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
		logError("Unrecognized GetHitVar Constant");
		logErrorString(var);
		abortSystem();
		return makeBooleanAssignmentReturn(0);
	}


}

static AssignmentReturnValue evaluateAnimationElementTimeArrayAssignment(AssignmentReturnValue tIndex, DreamPlayer* tPlayer) {
	int id = evaluateAssignmentReturnAsNumber(tIndex);
	int ret = getPlayerTimeFromAnimationElement(tPlayer, id);
	return makeNumberAssignmentReturn(ret);
}

static AssignmentReturnValue evaluateAnimationElementNumberArrayAssignment(AssignmentReturnValue tIndex, DreamPlayer* tPlayer) {
	int time = evaluateAssignmentReturnAsNumber(tIndex);
	int ret = getPlayerAnimationElementFromTimeOffset(tPlayer, time);
	return makeNumberAssignmentReturn(ret);
}

static AssignmentReturnValue evaluateIfElseArrayAssignment(AssignmentReturnValue tIndex) {
	char condText[100], yesText[100], noText[100];
	char comma1[20], comma2[20];

	sscanf(tIndex.mValue, "%s %s %s %s %s", condText, comma1, yesText, comma2, noText);
	assert(!strcmp(",", comma1));
	assert(!strcmp(",", comma2));
	assert(strcmp("", condText));
	assert(strcmp("", yesText));
	assert(strcmp("", noText));



	AssignmentReturnValue condRet = makeStringAssignmentReturn(condText);
	AssignmentReturnValue yesRet = makeStringAssignmentReturn(yesText);
	AssignmentReturnValue noRet = makeStringAssignmentReturn(noText);

	int cond = evaluateAssignmentReturnAsBool(condRet);

	if (cond) return yesRet;
	else return noRet;
}

static AssignmentReturnValue evaluateCondArrayAssignment(DreamMugenAssignment* tCondVector, DreamPlayer* tPlayer) {
	assert(tCondVector->mType == MUGEN_ASSIGNMENT_TYPE_VECTOR);
	DreamMugenDependOnTwoAssignment* firstV = tCondVector->mData;
	assert(firstV->b->mType == MUGEN_ASSIGNMENT_TYPE_VECTOR);
	DreamMugenDependOnTwoAssignment* secondV = firstV->b->mData;

	AssignmentReturnValue ret;
	int isTrue = evaluateDreamAssignment(firstV->a, tPlayer);
	if (isTrue) {
		ret = evaluateAssignmentInternal(secondV->a, tPlayer);
	}
	else {
		ret = evaluateAssignmentInternal(secondV->b, tPlayer);
	}

	return ret;
}

static AssignmentReturnValue evaluateSelfAnimationExistArrayAssignment(AssignmentReturnValue tIndex, DreamPlayer* tPlayer) {
	int id = evaluateAssignmentReturnAsNumber(tIndex);
	int ret = doesPlayerHaveAnimationHimself(tPlayer, id);
	return makeNumberAssignmentReturn(ret);
}

static AssignmentReturnValue evaluateConstCoordinatesArrayAssignment(AssignmentReturnValue tIndex, int p) {
	int coords = evaluateAssignmentReturnAsNumber(tIndex);
	double ret = parseDreamCoordinatesToLocalCoordinateSystem(coords, p);
	return makeFloatAssignmentReturn(ret);
}

static AssignmentReturnValue evaluateExternalFileAnimationArrayAssignment(AssignmentReturnValue a, AssignmentReturnValue b) {
	AssignmentReturnValue ret;
	sprintf(ret.mValue, "%s%s", a.mValue, b.mValue);
	return ret;
}

static AssignmentReturnValue evaluateNumTargetArrayAssignment(AssignmentReturnValue tIndex, DreamPlayer* tPlayer) {
	int id = evaluateAssignmentReturnAsNumber(tIndex);
	int ret = getPlayerTargetAmountWithID(tPlayer, id);
	return makeNumberAssignmentReturn(ret);
}

static AssignmentReturnValue evaluateNumHelperArrayAssignment(AssignmentReturnValue tIndex, DreamPlayer* tPlayer) {
	int id = evaluateAssignmentReturnAsNumber(tIndex);
	int ret = getPlayerHelperAmountWithID(tPlayer, id);
	return makeNumberAssignmentReturn(ret);
}

static AssignmentReturnValue evaluateNumExplodArrayAssignment(AssignmentReturnValue tIndex, DreamPlayer* tPlayer) {
	int id = evaluateAssignmentReturnAsNumber(tIndex);
	int ret = getExplodAmountWithID(tPlayer, id);
	return makeNumberAssignmentReturn(ret);
}

static AssignmentReturnValue evaluateIsHelperArrayAssignment(AssignmentReturnValue tIndex, DreamPlayer* tPlayer) {
	int id = evaluateAssignmentReturnAsNumber(tIndex);
	int isHelper = isPlayerHelper(tPlayer);
	int playerID = getPlayerID(tPlayer);
	return makeBooleanAssignmentReturn(isHelper && id == playerID);
}

static AssignmentReturnValue evaluateTargetArrayAssignment(AssignmentReturnValue tIndex, char* tTargetName) {
	int id = evaluateAssignmentReturnAsNumber(tIndex);
	
	AssignmentReturnValue ret;
	sprintf(ret.mValue, "%s %d", tTargetName, id);

	return ret;
}

static AssignmentReturnValue evaluatePlayerIDExistArrayAssignment(AssignmentReturnValue tIndex, DreamPlayer* tPlayer) {
	int id = evaluateAssignmentReturnAsNumber(tIndex);
	return makeBooleanAssignmentReturn(doesPlayerIDExist(tPlayer, id));
}

static AssignmentReturnValue evaluateNumberOfProjectilesWithIDArrayAssignment(AssignmentReturnValue tIndex, DreamPlayer* tPlayer) {
	int id = evaluateAssignmentReturnAsNumber(tIndex);
	return makeNumberAssignmentReturn(getPlayerProjectileAmountWithID(tPlayer, id));
}

static AssignmentReturnValue evaluateArrayAssignment(DreamMugenAssignment* tAssignment, DreamPlayer* tPlayer) {
	DreamMugenDependOnTwoAssignment* arrays = tAssignment->mData;

	AssignmentReturnValue a = evaluateAssignmentInternal(arrays->a, tPlayer); // TODO: remove once CHECK FOR CONSISTENCY no longer required

	char test[100];
	assert(arrays->a->mType == MUGEN_ASSIGNMENT_TYPE_VARIABLE);
	DreamMugenVariableAssignment* arrayName = arrays->a->mData;
	strcpy(test, arrayName->mName);
	turnStringLowercase(test);

	if (!strcmp("var", test)) {
		AssignmentReturnValue b = evaluateAssignmentInternal(arrays->b, tPlayer);
		return evaluateVarArrayAssignment(b, tPlayer);
	}
	else if (!strcmp("sysvar", test)) {
		AssignmentReturnValue b = evaluateAssignmentInternal(arrays->b, tPlayer);
		return evaluateSysVarArrayAssignment(b, tPlayer);
	}
	else if (!strcmp("fvar", test)) {
		AssignmentReturnValue b = evaluateAssignmentInternal(arrays->b, tPlayer);
		return evaluateFVarArrayAssignment(b, tPlayer);
	}
	else if (!strcmp("sysfvar", test)) {
		AssignmentReturnValue b = evaluateAssignmentInternal(arrays->b, tPlayer);
		return evaluateSysFVarArrayAssignment(b, tPlayer);
	}
	else if (!strcmp("abs", test)) {
		AssignmentReturnValue b = evaluateAssignmentInternal(arrays->b, tPlayer);
		return evaluateAbsArrayAssignment(b);
	}
	else if (!strcmp("exp", test)) {
		AssignmentReturnValue b = evaluateAssignmentInternal(arrays->b, tPlayer);
		return evaluateExpArrayAssignment(b);
	}
	else if (!strcmp("ln", test)) {
		AssignmentReturnValue b = evaluateAssignmentInternal(arrays->b, tPlayer);
		return evaluateNaturalLogArrayAssignment(b);
	}
	else if (!strcmp("cos", test)) {
		AssignmentReturnValue b = evaluateAssignmentInternal(arrays->b, tPlayer);
		return evaluateCosineArrayAssignment(b);
	}
	else if (!strcmp("sin", test)) {
		AssignmentReturnValue b = evaluateAssignmentInternal(arrays->b, tPlayer);
		return evaluateSineArrayAssignment(b);
	}
	else if (!strcmp("floor", test)) {
		AssignmentReturnValue b = evaluateAssignmentInternal(arrays->b, tPlayer);
		return evaluateFloorArrayAssignment(b);
	}
	else if (!strcmp("ceil", test)) {
		AssignmentReturnValue b = evaluateAssignmentInternal(arrays->b, tPlayer);
		return evaluateCeilArrayAssignment(b);
	}
	else if (!strcmp("const", test)) {
		AssignmentReturnValue b = evaluateAssignmentInternal(arrays->b, tPlayer);
		return evaluateConstArrayAssignment(b, tPlayer);
	}
	else if (!strcmp("gethitvar", test)) {
		AssignmentReturnValue b = evaluateAssignmentInternal(arrays->b, tPlayer);
		return evaluateGetHitVarArrayAssignment(b, tPlayer);
	}
	else if (!strcmp("animelemtime", test)) {
		AssignmentReturnValue b = evaluateAssignmentInternal(arrays->b, tPlayer);
		return evaluateAnimationElementTimeArrayAssignment(b, tPlayer);
	}
	else if (!strcmp("animelemno", test)) {
		AssignmentReturnValue b = evaluateAssignmentInternal(arrays->b, tPlayer);
		return evaluateAnimationElementNumberArrayAssignment(b, tPlayer);
	}
	else if (!strcmp("ifelse", test) || !strcmp("sifelse", test)) {
		AssignmentReturnValue b = evaluateAssignmentInternal(arrays->b, tPlayer);
		return evaluateIfElseArrayAssignment(b);
	}
	else if (!strcmp("cond", test)) {
		return evaluateCondArrayAssignment(arrays->b, tPlayer);
	}
	else if (!strcmp("selfanimexist", test)) {
		AssignmentReturnValue b = evaluateAssignmentInternal(arrays->b, tPlayer);
		return evaluateSelfAnimationExistArrayAssignment(b, tPlayer);
	}
	else if (!strcmp("const240p", test)) {
		AssignmentReturnValue b = evaluateAssignmentInternal(arrays->b, tPlayer);
		return evaluateConstCoordinatesArrayAssignment(b, 240);
	}
	else if (!strcmp("const720p", test)) {
		AssignmentReturnValue b = evaluateAssignmentInternal(arrays->b, tPlayer);
		return evaluateConstCoordinatesArrayAssignment(b, 720);
	}
	else if (!strcmp("f", test) || !strcmp("s", test)) {
		AssignmentReturnValue b = evaluateAssignmentInternal(arrays->b, tPlayer);
		return evaluateExternalFileAnimationArrayAssignment(a, b);
	}
	else if (!strcmp("numtarget", test)) {
		AssignmentReturnValue b = evaluateAssignmentInternal(arrays->b, tPlayer);
		return evaluateNumTargetArrayAssignment(b, tPlayer);
	}
	else if (!strcmp("numhelper", test)) {
		AssignmentReturnValue b = evaluateAssignmentInternal(arrays->b, tPlayer);
		return evaluateNumHelperArrayAssignment(b, tPlayer);
	}
	else if (!strcmp("numexplod", test)) {
		AssignmentReturnValue b = evaluateAssignmentInternal(arrays->b, tPlayer);
		return evaluateNumExplodArrayAssignment(b, tPlayer);
	}
	else if (!strcmp("ishelper", test)) {
		AssignmentReturnValue b = evaluateAssignmentInternal(arrays->b, tPlayer);
		return evaluateIsHelperArrayAssignment(b, tPlayer);
	}
	else if (!strcmp("helper", test)) {
		AssignmentReturnValue b = evaluateAssignmentInternal(arrays->b, tPlayer);
		return evaluateTargetArrayAssignment(b, "helper");
	}
	else if (!strcmp("enemynear", test)) {
		AssignmentReturnValue b = evaluateAssignmentInternal(arrays->b, tPlayer);
		return evaluateTargetArrayAssignment(b, "enemynear");
	}
	else if (!strcmp("playerid", test)) {
		AssignmentReturnValue b = evaluateAssignmentInternal(arrays->b, tPlayer);
		return evaluateTargetArrayAssignment(b, "playerid");
	}
	else if (!strcmp("playeridexist", test)) {
		AssignmentReturnValue b = evaluateAssignmentInternal(arrays->b, tPlayer);
		return evaluatePlayerIDExistArrayAssignment(b, tPlayer);
	}
	else if (!strcmp("numprojid", test)) {
		AssignmentReturnValue b = evaluateAssignmentInternal(arrays->b, tPlayer);
		return evaluateNumberOfProjectilesWithIDArrayAssignment(b, tPlayer);
	}
	else {
		logError("Unknown array.");
		logErrorString(test);
		abortSystem();
		return makeBooleanAssignmentReturn(0);
	}
}

static AssignmentReturnValue evaluateUnaryMinusAssignment(DreamMugenAssignment* tAssignment, DreamPlayer* tPlayer) {
	DreamMugenDependOnOneAssignment* min = tAssignment->mData;

	AssignmentReturnValue a = evaluateAssignmentInternal(min->a, tPlayer);
	if (isFloatReturn(a)) {
		return makeFloatAssignmentReturn(-evaluateAssignmentReturnAsFloat(a));
	}
	else {
		return makeNumberAssignmentReturn(-evaluateAssignmentReturnAsNumber(a));
	}
}

static AssignmentReturnValue evaluateNegationAssignment(DreamMugenAssignment* tAssignment, DreamPlayer* tPlayer) {
	DreamMugenDependOnOneAssignment* neg = tAssignment->mData;

	AssignmentReturnValue a = evaluateAssignmentInternal(neg->a, tPlayer);
	int val = evaluateAssignmentReturnAsBool(a);

	return makeBooleanAssignmentReturn(!val);
}

static AssignmentReturnValue evaluateAssignmentInternal(DreamMugenAssignment* tAssignment, DreamPlayer* tPlayer) {
	assert(tAssignment != NULL);
	assert(tAssignment->mData != NULL);


	AssignmentReturnValue ret;
	if (tAssignment->mType == MUGEN_ASSIGNMENT_TYPE_OR) {
		ret = evaluateOrAssignment(tAssignment, tPlayer);
	}
	else if (tAssignment->mType == MUGEN_ASSIGNMENT_TYPE_AND) {
		return evaluateAndAssignment(tAssignment, tPlayer);
	}
	else if (tAssignment->mType == MUGEN_ASSIGNMENT_TYPE_BITWISE_OR) {
		return evaluateBitwiseOrAssignment(tAssignment, tPlayer);
	}
	else if (tAssignment->mType == MUGEN_ASSIGNMENT_TYPE_BITWISE_AND) {
		return evaluateBitwiseAndAssignment(tAssignment, tPlayer);
	}
	else if (tAssignment->mType == MUGEN_ASSIGNMENT_TYPE_COMPARISON) {
		return evaluateComparisonAssignment(tAssignment, tPlayer);
	}
	else if (tAssignment->mType == MUGEN_ASSIGNMENT_TYPE_SET_VARIABLE) {
		return evaluateSetVariableAssignment(tAssignment, tPlayer);
	}
	else if (tAssignment->mType == MUGEN_ASSIGNMENT_TYPE_FIXED_BOOLEAN) {
		return evaluateBooleanAssignment(tAssignment);
	}
	else if (tAssignment->mType == MUGEN_ASSIGNMENT_TYPE_VARIABLE) {
		return evaluateVariableAssignment(tAssignment, tPlayer);
	}
	else if (tAssignment->mType == MUGEN_ASSIGNMENT_TYPE_NUMBER) {
		return evaluateNumberAssignment(tAssignment);
	}
	else if (tAssignment->mType == MUGEN_ASSIGNMENT_TYPE_VECTOR) {
		return evaluateVectorAssignment(tAssignment, tPlayer);
	}
	else if (tAssignment->mType == MUGEN_ASSIGNMENT_TYPE_INEQUALITY) {
		return evaluateInequalityAssignment(tAssignment, tPlayer);
	}
	else if (tAssignment->mType == MUGEN_ASSIGNMENT_TYPE_RANGE) {
		return evaluateRangeAssignment(tAssignment, tPlayer);
	}
	else if (tAssignment->mType == MUGEN_ASSIGNMENT_TYPE_ARRAY) {
		return evaluateArrayAssignment(tAssignment, tPlayer);
	}
	else if (tAssignment->mType == MUGEN_ASSIGNMENT_TYPE_NULL) {
		return evaluateBooleanAssignment(tAssignment);
	}
	else if (tAssignment->mType == MUGEN_ASSIGNMENT_TYPE_GREATER) {
		return evaluateGreaterAssignment(tAssignment, tPlayer);
	}
	else if (tAssignment->mType == MUGEN_ASSIGNMENT_TYPE_GREATER_OR_EQUAL) {
		return evaluateGreaterOrEqualAssignment(tAssignment, tPlayer);
	}
	else if (tAssignment->mType == MUGEN_ASSIGNMENT_TYPE_LESS) {
		return evaluateLessAssignment(tAssignment, tPlayer);
	}
	else if (tAssignment->mType == MUGEN_ASSIGNMENT_TYPE_LESS_OR_EQUAL) {
		return evaluateLessOrEqualAssignment(tAssignment, tPlayer);
	}
	else if (tAssignment->mType == MUGEN_ASSIGNMENT_TYPE_MODULO) {
		return evaluateModuloAssignment(tAssignment, tPlayer);
	}
	else if (tAssignment->mType == MUGEN_ASSIGNMENT_TYPE_EXPONENTIATION) {
		return evaluateExponentiationAssignment(tAssignment, tPlayer);
	}
	else if (tAssignment->mType == MUGEN_ASSIGNMENT_TYPE_MULTIPLICATION) {
		return evaluateMultiplicationAssignment(tAssignment, tPlayer);
	}
	else if (tAssignment->mType == MUGEN_ASSIGNMENT_TYPE_DIVISION) {
		return evaluateDivisionAssignment(tAssignment, tPlayer);
	}
	else if (tAssignment->mType == MUGEN_ASSIGNMENT_TYPE_ADDITION) {
		return evaluateAdditionAssignment(tAssignment, tPlayer);
	}
	else if (tAssignment->mType == MUGEN_ASSIGNMENT_TYPE_SUBTRACTION) {
		return evaluateSubtractionAssignment(tAssignment, tPlayer);
	}
	else if (tAssignment->mType == MUGEN_ASSIGNMENT_TYPE_OPERATOR_ARGUMENT) {
		return evaluateOperatorArgumentAssignment(tAssignment, tPlayer);
	}
	else if (tAssignment->mType == MUGEN_ASSIGNMENT_TYPE_UNARY_MINUS) {
		return evaluateUnaryMinusAssignment(tAssignment, tPlayer);
	}
	else if (tAssignment->mType == MUGEN_ASSIGNMENT_TYPE_NEGATION) {
		return evaluateNegationAssignment(tAssignment, tPlayer);
	}
	else if (tAssignment->mType == MUGEN_ASSIGNMENT_TYPE_FLOAT) {
		return evaluateFloatAssignment(tAssignment);
	}
	else if (tAssignment->mType == MUGEN_ASSIGNMENT_TYPE_STRING) {
		return evaluateStringAssignment(tAssignment);
	}
	else {
		logError("Unidentified assignment type.");
		logErrorInteger(tAssignment->mType);
		abortSystem();
		*ret.mValue = '\0';
	}

	return ret;
}

int evaluateDreamAssignment(DreamMugenAssignment * tAssignment, DreamPlayer* tPlayer)
{
	AssignmentReturnValue ret = evaluateAssignmentInternal(tAssignment, tPlayer);
	return evaluateAssignmentReturnAsBool(ret);
}

double evaluateDreamAssignmentAndReturnAsFloat(DreamMugenAssignment * tAssignment, DreamPlayer* tPlayer)
{
	AssignmentReturnValue ret = evaluateAssignmentInternal(tAssignment, tPlayer);
	return evaluateAssignmentReturnAsFloat(ret);
}

int evaluateDreamAssignmentAndReturnAsInteger(DreamMugenAssignment * tAssignment, DreamPlayer* tPlayer)
{
	AssignmentReturnValue ret = evaluateAssignmentInternal(tAssignment, tPlayer);
	return evaluateAssignmentReturnAsNumber(ret);
}

char * evaluateDreamAssignmentAndReturnAsAllocatedString(DreamMugenAssignment * tAssignment, DreamPlayer* tPlayer)
{
	AssignmentReturnValue ret = evaluateAssignmentInternal(tAssignment, tPlayer);
	char* str = allocMemory(strlen(ret.mValue) + 10);
	strcpy(str, ret.mValue);

	return str;
}

Vector3D evaluateDreamAssignmentAndReturnAsVector3D(DreamMugenAssignment * tAssignment, DreamPlayer* tPlayer)
{
	AssignmentReturnValue ret = evaluateAssignmentInternal(tAssignment, tPlayer);

	double x, y, z;
	char tX[100], comma1[100], tY[100], comma2[100], tZ[100];

	int items = sscanf(ret.mValue, "%s %s %s %s %s", tX, comma1, tY, comma2, tZ);

	if (items >= 1) x = atof(tX);
	else x = 0;
	if (items >= 3) y = atof(tY);
	else y = 0;
	if (items >= 5) z = atof(tZ);
	else z = 0;

	return makePosition(x, y, z);
}
