#define HAVE_M_PI // TODO fix

#include "mugenassignmentevaluator.h"

#include <assert.h>

#define _USE_MATH_DEFINES
#include <math.h>

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
	if (playerState == MUGEN_STATE_TYPE_STANDING) ret = strchr(tCommand.mValue, 's') != NULL;
	else if (playerState == MUGEN_STATE_TYPE_AIR) ret = strchr(tCommand.mValue, 'a') != NULL;
	else if (playerState == MUGEN_STATE_TYPE_CROUCHING) ret = strchr(tCommand.mValue, 'c') != NULL;
	else if (playerState == MUGEN_STATE_TYPE_LYING) ret = strchr(tCommand.mValue, 'l') != NULL;
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

// TODO: merge with AnimElem
static AssignmentReturnValue evaluateProjVectorAssignment(AssignmentReturnValue tCommand, DreamPlayer* tPlayer, int tProjectileID, int(*tTimeFunc)(DreamPlayer*, int)) {
	char number1[100], comma[10], oper[100], number2[100];
	int items = sscanf(tCommand.mValue, "%s %s %s %s", number1, comma, oper, number2);
	assert(strcmp("", number1));
	assert(!strcmp(",", comma));

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

	return makeBooleanAssignmentReturn(ret == compareValue);
}

static AssignmentReturnValue evaluateProjNumberAssignment(AssignmentReturnValue tCommand, DreamPlayer* tPlayer, int tProjectileID, int(*tTimeFunc)(DreamPlayer*, int)) {

	int compareValue = evaluateAssignmentReturnAsNumber(tCommand);
	int timeOffset = tTimeFunc(tPlayer, tProjectileID);
	int ret = timeOffset == 1; // TODO: check

	return makeBooleanAssignmentReturn(ret == compareValue);
}

int getProjectileIDFromAssignmentName(char* tName, char* tBaseName) {
	char* idOffset = tName += strlen(tBaseName);
	if (*idOffset == '\0') return 0;

	return atoi(idOffset);
}

static AssignmentReturnValue evaluateProjAssignment(char* tName, char* tBaseName, AssignmentReturnValue tCommand, DreamPlayer* tPlayer, int(*tTimeFunc)(DreamPlayer*, int)) {
	int projID = getProjectileIDFromAssignmentName(tName, tBaseName);

	if (strchr(tCommand.mValue, ',')) {
		return evaluateProjVectorAssignment(tCommand, tPlayer, projID, tTimeFunc);
	}
	else {
		return evaluateProjNumberAssignment(tCommand, tPlayer, projID, tTimeFunc);
	}
}

static int isProjAssignment(char* tName, char* tBaseName) {
	return !strncmp(tName, tBaseName, strlen(tBaseName));

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
	else if (isProjAssignment(name, "projcontact")) {
		return evaluateProjAssignment(name, "projcontact", b, tPlayer, getPlayerProjectileTimeSinceContact);
	}
	else if (isProjAssignment(name, "projguarded")) {
		return evaluateProjAssignment(name, "projguarded", b, tPlayer, getPlayerProjectileTimeSinceGuarded);
	}
	else if (isProjAssignment(name, "projhit")) {
		return evaluateProjAssignment(name, "projhit", b, tPlayer, getPlayerProjectileTimeSinceHit);
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

typedef AssignmentReturnValue(*VariableFunction)(DreamPlayer*);



static struct {
	StringMap mVariables; // contains VariableFunction
} gVariableHandler;

//static AssignmentReturnValue absFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(0); }
//static AssignmentReturnValue acosFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(0); }
static AssignmentReturnValue aiLevelFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerAILevel(tPlayer)); }
static AssignmentReturnValue aliveFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(isPlayerAlive(tPlayer)); }
static AssignmentReturnValue animFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerAnimationNumber(tPlayer)); }
//static AssignmentReturnValue animElemFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(0); }
//static AssignmentReturnValue animElemNoFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(0); }
//static AssignmentReturnValue animElemTimeFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(0); }
//static AssignmentReturnValue animExistFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(0); }
static AssignmentReturnValue animTimeFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getRemainingPlayerAnimationTime(tPlayer)); }
//static AssignmentReturnValue asinFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(0); }
//static AssignmentReturnValue atanFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(0); }
static AssignmentReturnValue authorNameFunction(DreamPlayer* tPlayer) { return makeStringAssignmentReturn(getPlayerAuthorName(tPlayer)); }
static AssignmentReturnValue backEdgeFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerScreenEdgeInBackX(tPlayer)); }
static AssignmentReturnValue backEdgeBodyDistFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerBackBodyDistanceToScreen(tPlayer)); }
static AssignmentReturnValue backEdgeDistFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerBackAxisDistanceToScreen(tPlayer)); }
static AssignmentReturnValue bottomEdgeFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getDreamStageBottomEdgeY(getPlayerCoordinateP(tPlayer))); }
static AssignmentReturnValue cameraPosXFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getDreamCameraPositionX(getPlayerCoordinateP(tPlayer))); }
static AssignmentReturnValue cameraPosYFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getDreamCameraPositionY(getPlayerCoordinateP(tPlayer))); }
static AssignmentReturnValue cameraZoomFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getDreamCameraZoom(getPlayerCoordinateP(tPlayer))); }
static AssignmentReturnValue canRecoverFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(canPlayerRecoverFromFalling(tPlayer)); }
//static AssignmentReturnValue ceilFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(0); }
//static AssignmentReturnValue commandFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(0); }
//static AssignmentReturnValue condFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(0); }
//static AssignmentReturnValue constFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(0); }
//static AssignmentReturnValue const240pFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(0); }
//static AssignmentReturnValue const480pFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(0); }
//static AssignmentReturnValue const720pFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(0); }
//static AssignmentReturnValue cosFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(0); }
static AssignmentReturnValue ctrlFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(getPlayerControl(tPlayer)); }
static AssignmentReturnValue drawGameFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(hasPlayerDrawn(tPlayer)); }
static AssignmentReturnValue eFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(M_E); }
//static AssignmentReturnValue expFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(0); }
static AssignmentReturnValue facingFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerIsFacingRight(tPlayer) ? 1 : -1); }
//static AssignmentReturnValue floorFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(0); }
static AssignmentReturnValue frontEdgeFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerScreenEdgeInFrontX(tPlayer)); }
static AssignmentReturnValue frontEdgeBodyDistFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerFrontBodyDistanceToScreen(tPlayer)); }
static AssignmentReturnValue frontEdgeDistFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerFrontAxisDistanceToScreen(tPlayer)); }
//static AssignmentReturnValue fVarFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(0); }
static AssignmentReturnValue gameHeightFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getDreamGameHeight(getPlayerCoordinateP(tPlayer))); }
static AssignmentReturnValue gameTimeFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getDreamGameTime()); }
static AssignmentReturnValue gameWidthFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getDreamGameWidth(getPlayerCoordinateP(tPlayer))); }
//static AssignmentReturnValue getHitVarFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(0); }
static AssignmentReturnValue hitCountFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerHitCount(tPlayer)); }
//static AssignmentReturnValue hitDefAttrFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(0); }
static AssignmentReturnValue hitFallFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(isPlayerFalling(tPlayer)); }
static AssignmentReturnValue hitOverFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(isPlayerHitOver(tPlayer)); }
static AssignmentReturnValue hitPauseTimeFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerTimeLeftInHitPause(tPlayer)); }
static AssignmentReturnValue hitShakeOverFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(isPlayerHitShakeOver(tPlayer));  }
static AssignmentReturnValue hitVelXFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerHitVelocityX(tPlayer, getPlayerCoordinateP(tPlayer))); }
static AssignmentReturnValue hitVelYFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerHitVelocityY(tPlayer, getPlayerCoordinateP(tPlayer))); }
static AssignmentReturnValue idFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(tPlayer->mID); }
//static AssignmentReturnValue ifElseFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(0); }
static AssignmentReturnValue inGuardDistFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(isPlayerInGuardDistance(tPlayer)); }
static AssignmentReturnValue isHelperFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(isPlayerHelper(tPlayer)); }
static AssignmentReturnValue isHomeTeamFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(isPlayerHomeTeam(tPlayer)); }
static AssignmentReturnValue leftEdgeFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getDreamStageLeftEdgeX(getPlayerCoordinateP(tPlayer))); }
static AssignmentReturnValue lifeFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerLife(tPlayer)); }
static AssignmentReturnValue lifeMaxFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerLifeMax(tPlayer)); }
//static AssignmentReturnValue lnFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(0); }
//static AssignmentReturnValue logFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(0); }
static AssignmentReturnValue loseFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(hasPlayerLost(tPlayer)); }
static AssignmentReturnValue matchNoFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getDreamMatchNumber()); }
static AssignmentReturnValue matchOverFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(isDreamMatchOver(tPlayer)); }
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
//static AssignmentReturnValue numProjIDFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(0); }
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
//static AssignmentReturnValue playerIDExistFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(0); }
static AssignmentReturnValue prevStateNoFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerPreviousState(tPlayer)); }
//static AssignmentReturnValue projCancelTimeFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(0); }
//static AssignmentReturnValue projContactFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(0); }
//static AssignmentReturnValue projContactTimeFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(0); }
//static AssignmentReturnValue projGuardedFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(0); }
//static AssignmentReturnValue projGuardedTimeFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(0); }
//static AssignmentReturnValue projHitFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(0); }
//static AssignmentReturnValue projHitTimeFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(0); }
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
//static AssignmentReturnValue selfAnimExistFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(0); }
//static AssignmentReturnValue sinFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(0); }
static AssignmentReturnValue stateNoFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerState(tPlayer)); }
//static AssignmentReturnValue stateTypeFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(0); }
//static AssignmentReturnValue stageVarFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(0); }
//static AssignmentReturnValue sysFVarFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(0); }
//static AssignmentReturnValue sysVarFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(0); }
//static AssignmentReturnValue tanFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(0); }
//static AssignmentReturnValue teamModeFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(0); }
static AssignmentReturnValue teamSideFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(tPlayer->mRootID + 1); }
static AssignmentReturnValue ticksPerSecondFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getDreamTicksPerSecond()); }
static AssignmentReturnValue timeFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerTimeInState(tPlayer)); }
//static AssignmentReturnValue timeModFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(0); }
static AssignmentReturnValue topEdgeFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getDreamStageTopEdgeY(getPlayerCoordinateP(tPlayer))); }
static AssignmentReturnValue uniqHitCountFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getPlayerUniqueHitCount(tPlayer)); }
//static AssignmentReturnValue varFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(0); }
static AssignmentReturnValue velXFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerVelocityX(tPlayer, getPlayerCoordinateP(tPlayer))); }
static AssignmentReturnValue velYFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerVelocityY(tPlayer, getPlayerCoordinateP(tPlayer))); }
static AssignmentReturnValue winFunction(DreamPlayer* tPlayer) { return makeBooleanAssignmentReturn(hasPlayerWon(tPlayer)); }



static void setupVariableAssignment() {
	gVariableHandler.mVariables = new_string_map();
	//string_map_push(&gVariableHandler.mVariables, "abs", animElemFunction);
	//string_map_push(&gVariableHandler.mVariables, "acos", animElemFunction);
	string_map_push(&gVariableHandler.mVariables, "ailevel", aiLevelFunction);
	string_map_push(&gVariableHandler.mVariables, "alive", aliveFunction);
	string_map_push(&gVariableHandler.mVariables, "anim", animFunction);
	//string_map_push(&gVariableHandler.mVariables, "animelem", animElemFunction);
	//string_map_push(&gVariableHandler.mVariables, "animelemno", animElemNoFunction);
	//string_map_push(&gVariableHandler.mVariables, "animelemtime", animElemTimeFunction);
	//string_map_push(&gVariableHandler.mVariables, "animexist", animExistFunction);
	string_map_push(&gVariableHandler.mVariables, "animtime", animTimeFunction);
	//string_map_push(&gVariableHandler.mVariables, "asin", asinFunction);
	//string_map_push(&gVariableHandler.mVariables, "atan", atanFunction);
	string_map_push(&gVariableHandler.mVariables, "authorname", authorNameFunction);

	string_map_push(&gVariableHandler.mVariables, "backedge", backEdgeFunction);
	string_map_push(&gVariableHandler.mVariables, "backedgebodydist", backEdgeBodyDistFunction);
	string_map_push(&gVariableHandler.mVariables, "backedgedist", backEdgeDistFunction);
	string_map_push(&gVariableHandler.mVariables, "bottomedge", bottomEdgeFunction);

	string_map_push(&gVariableHandler.mVariables, "camerapos x", cameraPosXFunction);
	string_map_push(&gVariableHandler.mVariables, "camerapos y", cameraPosYFunction);
	string_map_push(&gVariableHandler.mVariables, "camerazoom", cameraZoomFunction);
	string_map_push(&gVariableHandler.mVariables, "canrecover", canRecoverFunction);
	//string_map_push(&gVariableHandler.mVariables, "ceil", ceilFunction);
	//string_map_push(&gVariableHandler.mVariables, "command", commandFunction);
	//string_map_push(&gVariableHandler.mVariables, "cond", condFunction);
	//string_map_push(&gVariableHandler.mVariables, "const", constFunction);
	//string_map_push(&gVariableHandler.mVariables, "const240p", const240pFunction);
	//string_map_push(&gVariableHandler.mVariables, "const480p", const480pFunction);
	//string_map_push(&gVariableHandler.mVariables, "const720p", const720pFunction);
	//string_map_push(&gVariableHandler.mVariables, "cos", cosFunction);
	string_map_push(&gVariableHandler.mVariables, "ctrl", ctrlFunction);

	string_map_push(&gVariableHandler.mVariables, "drawgame", drawGameFunction);

	string_map_push(&gVariableHandler.mVariables, "e", eFunction);
	//string_map_push(&gVariableHandler.mVariables, "exp", expFunction);

	string_map_push(&gVariableHandler.mVariables, "facing", facingFunction);
	//string_map_push(&gVariableHandler.mVariables, "floor", floorFunction);
	string_map_push(&gVariableHandler.mVariables, "frontedge", frontEdgeFunction);
	string_map_push(&gVariableHandler.mVariables, "frontedgebodydist", frontEdgeBodyDistFunction);
	string_map_push(&gVariableHandler.mVariables, "frontedgedist", frontEdgeDistFunction);
	//string_map_push(&gVariableHandler.mVariables, "fvar", fVarFunction);

	string_map_push(&gVariableHandler.mVariables, "gameheight", gameHeightFunction);
	string_map_push(&gVariableHandler.mVariables, "gametime", gameTimeFunction);
	string_map_push(&gVariableHandler.mVariables, "gamewidth", gameWidthFunction);
	//string_map_push(&gVariableHandler.mVariables, "gethitvar", getHitVarFunction);

	string_map_push(&gVariableHandler.mVariables, "hitcount", hitCountFunction);
	//string_map_push(&gVariableHandler.mVariables, "hitdefattr", hitDefAttrFunction);
	string_map_push(&gVariableHandler.mVariables, "hitfall", hitFallFunction);
	string_map_push(&gVariableHandler.mVariables, "hitover", hitOverFunction);
	string_map_push(&gVariableHandler.mVariables, "hitpausetime", hitPauseTimeFunction);
	string_map_push(&gVariableHandler.mVariables, "hitshakeover", hitShakeOverFunction);
	string_map_push(&gVariableHandler.mVariables, "hitvel x", hitVelXFunction);
	string_map_push(&gVariableHandler.mVariables, "hitvel y", hitVelYFunction);

	string_map_push(&gVariableHandler.mVariables, "id", idFunction);
	//string_map_push(&gVariableHandler.mVariables, "ifelse", ifElseFunction);
	string_map_push(&gVariableHandler.mVariables, "inguarddist", inGuardDistFunction);
	string_map_push(&gVariableHandler.mVariables, "ishelper", isHelperFunction);
	string_map_push(&gVariableHandler.mVariables, "ishometeam", isHomeTeamFunction);

	string_map_push(&gVariableHandler.mVariables, "leftedge", leftEdgeFunction);
	string_map_push(&gVariableHandler.mVariables, "life", lifeFunction);
	string_map_push(&gVariableHandler.mVariables, "lifemax", lifeMaxFunction);
	//string_map_push(&gVariableHandler.mVariables, "ln", lnFunction);
	//string_map_push(&gVariableHandler.mVariables, "log", logFunction);
	string_map_push(&gVariableHandler.mVariables, "lose", loseFunction);

	string_map_push(&gVariableHandler.mVariables, "matchno", matchNoFunction);
	string_map_push(&gVariableHandler.mVariables, "matchover", matchOverFunction);
	string_map_push(&gVariableHandler.mVariables, "movecontact", moveContactFunction);
	string_map_push(&gVariableHandler.mVariables, "moveguarded", moveGuardedFunction);
	string_map_push(&gVariableHandler.mVariables, "movehit", moveHitFunction);
	//string_map_push(&gVariableHandler.mVariables, "movetype", moveTypeFunction);
	string_map_push(&gVariableHandler.mVariables, "movereversed", moveReversedFunction);

	string_map_push(&gVariableHandler.mVariables, "name", nameFunction);
	string_map_push(&gVariableHandler.mVariables, "numenemy", numEnemyFunction);
	string_map_push(&gVariableHandler.mVariables, "numexplod", numExplodFunction);
	string_map_push(&gVariableHandler.mVariables, "numhelper", numHelperFunction);
	string_map_push(&gVariableHandler.mVariables, "numpartner", numPartnerFunction);
	string_map_push(&gVariableHandler.mVariables, "numproj", numProjFunction);
	//string_map_push(&gVariableHandler.mVariables, "numprojid", numProjIDFunction);
	string_map_push(&gVariableHandler.mVariables, "numtarget", numTargetFunction);

	string_map_push(&gVariableHandler.mVariables, "p1name", p1NameFunction);
	string_map_push(&gVariableHandler.mVariables, "p2bodydist x", p2BodyDistFunctionX);
	string_map_push(&gVariableHandler.mVariables, "p2bodydist y", p2BodyDistFunctionY);
	string_map_push(&gVariableHandler.mVariables, "p2dist x", p2DistFunctionX);
	string_map_push(&gVariableHandler.mVariables, "p2dist y", p2DistFunctionY);
	string_map_push(&gVariableHandler.mVariables, "p2life", p2LifeFunction);
	//string_map_push(&gVariableHandler.mVariables, "p2movetype", p2MoveTypeFunction);
	string_map_push(&gVariableHandler.mVariables, "p2name", p2NameFunction);
	string_map_push(&gVariableHandler.mVariables, "p2stateno", p2StateNoFunction);
	//string_map_push(&gVariableHandler.mVariables, "p2statetype", p2StateTypeFunction);
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
	//string_map_push(&gVariableHandler.mVariables, "playeridexist", playerIDExistFunction);
	string_map_push(&gVariableHandler.mVariables, "prevstateno", prevStateNoFunction);
	//string_map_push(&gVariableHandler.mVariables, "projcanceltime", projCancelTimeFunction);
	//string_map_push(&gVariableHandler.mVariables, "projcontact", projContactFunction);
	//string_map_push(&gVariableHandler.mVariables, "projcontacttime", projContactTimeFunction);
	//string_map_push(&gVariableHandler.mVariables, "projguarded", projGuardedFunction);
	//string_map_push(&gVariableHandler.mVariables, "projguardedtime", projGuardedTimeFunction);
	//string_map_push(&gVariableHandler.mVariables, "projhit", projHitFunction);
	//string_map_push(&gVariableHandler.mVariables, "projhittime", projHitTimeFunction);

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
	//string_map_push(&gVariableHandler.mVariables, "selfanimexist", selfAnimExistFunction);
	//string_map_push(&gVariableHandler.mVariables, "sin", sinFunction);
	string_map_push(&gVariableHandler.mVariables, "stateno", stateNoFunction);
	string_map_push(&gVariableHandler.mVariables, "statetime", timeFunction);
	//string_map_push(&gVariableHandler.mVariables, "statetype", stateTypeFunction);
	//string_map_push(&gVariableHandler.mVariables, "stagevar", stageVarFunction);
	//string_map_push(&gVariableHandler.mVariables, "sysfvar", sysFVarFunction);
	//string_map_push(&gVariableHandler.mVariables, "sysvar", sysVarFunction);

	//string_map_push(&gVariableHandler.mVariables, "tan", tanFunction);
	//string_map_push(&gVariableHandler.mVariables, "teammode", teamModeFunction);
	string_map_push(&gVariableHandler.mVariables, "teamside", teamSideFunction);
	string_map_push(&gVariableHandler.mVariables, "tickspersecond", ticksPerSecondFunction);
	string_map_push(&gVariableHandler.mVariables, "time", timeFunction);
	//string_map_push(&gVariableHandler.mVariables, "timemod", timeModFunction);
	string_map_push(&gVariableHandler.mVariables, "topedge", topEdgeFunction);

	string_map_push(&gVariableHandler.mVariables, "uniquehitcount", uniqHitCountFunction);

	//string_map_push(&gVariableHandler.mVariables, "var", varFunction);
	string_map_push(&gVariableHandler.mVariables, "vel x", velXFunction);
	string_map_push(&gVariableHandler.mVariables, "vel y", velYFunction);

	string_map_push(&gVariableHandler.mVariables, "win", winFunction);
}

void setupDreamAssignmentEvaluator() {
	setupVariableAssignment();
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

static AssignmentReturnValue evaluateVariableAssignment(DreamMugenAssignment* tAssignment, DreamPlayer* tPlayer) {
	DreamMugenVariableAssignment* variable = tAssignment->mData;
	char testString[100];
	strcpy(testString, variable->mName);
	turnStringLowercase(testString);

	if (string_map_contains(&gVariableHandler.mVariables, testString)) {
		VariableFunction func = string_map_get(&gVariableHandler.mVariables, testString);
		return func(tPlayer);
	}
	
	if (isIsInOtherFileVariable(testString)) { // TODO: fix
		AssignmentReturnValue ret;
		sprintf(ret.mValue, "isinotherfile %s", testString + 1);
		return ret;
	}
	
	return makeStringAssignmentReturn(testString);
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

static AssignmentReturnValue evaluateStageVarArrayAssignment(AssignmentReturnValue tIndex, DreamPlayer* tPlayer) {
	char* var = tIndex.mValue;
	turnStringLowercase(var);

	if (!strcmp("info.author", var)) {
		return makeStringAssignmentReturn(getDreamStageAuthor());
	}
	else if(!strcmp("info.displayname", var)) {
		return makeStringAssignmentReturn(getDreamStageDisplayName());
	}
	else if (!strcmp("info.name", var)) {
		return makeStringAssignmentReturn(getDreamStageName());
	}
	else {
		logError("Unknown stage variable.");
		logErrorString(var);
		abortSystem();
		makeStringAssignmentReturn("");
	}
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

static AssignmentReturnValue evaluateLogArrayAssignment(AssignmentReturnValue tIndex) {
	char* text = tIndex.mValue;
	double base, value;
	char comma[10];
	int items = sscanf(text, "%lf %s %lf", &base, comma, &value);
	assert(items == 3);
	assert(!strcmp(",", comma));

	return makeFloatAssignmentReturn(log(value) / log(base));
}

static AssignmentReturnValue evaluateCosineArrayAssignment(AssignmentReturnValue tIndex) {
		double val = evaluateAssignmentReturnAsFloat(tIndex);
		return makeFloatAssignmentReturn(cos(val));
}

static AssignmentReturnValue evaluateAcosineArrayAssignment(AssignmentReturnValue tIndex) {
	double val = evaluateAssignmentReturnAsFloat(tIndex);
	return makeFloatAssignmentReturn(acos(val));
}

static AssignmentReturnValue evaluateSineArrayAssignment(AssignmentReturnValue tIndex) {
	double val = evaluateAssignmentReturnAsFloat(tIndex);
	return makeFloatAssignmentReturn(sin(val));
}

static AssignmentReturnValue evaluateAsineArrayAssignment(AssignmentReturnValue tIndex) {
	double val = evaluateAssignmentReturnAsFloat(tIndex);
	return makeFloatAssignmentReturn(asin(val));
}

static AssignmentReturnValue evaluateTangentArrayAssignment(AssignmentReturnValue tIndex) {
	double val = evaluateAssignmentReturnAsFloat(tIndex);
	return makeFloatAssignmentReturn(tan(val));
}

static AssignmentReturnValue evaluateAtangentArrayAssignment(AssignmentReturnValue tIndex) {
	double val = evaluateAssignmentReturnAsFloat(tIndex);
	return makeFloatAssignmentReturn(atan(val));
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

static AssignmentReturnValue evaluateGetHitVarArrayAssignment(DreamMugenAssignment* tIndexAssignment, DreamPlayer* tPlayer) {
	assert(tIndexAssignment->mType == MUGEN_ASSIGNMENT_TYPE_VARIABLE);
	DreamMugenVariableAssignment* variableAssignment = tIndexAssignment->mData;
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

static AssignmentReturnValue evaluateAnimationExistArrayAssignment(AssignmentReturnValue tIndex, DreamPlayer* tPlayer) {
	int id = evaluateAssignmentReturnAsNumber(tIndex);
	int ret = doesPlayerHaveAnimation(tPlayer, id);
	return makeNumberAssignmentReturn(ret);
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

static AssignmentReturnValue evaluateProjectileCancelTimeArrayAssignment(AssignmentReturnValue tIndex, DreamPlayer* tPlayer) {
	int id = evaluateAssignmentReturnAsNumber(tIndex);
	return makeNumberAssignmentReturn(getPlayerProjectileTimeSinceCancel(tPlayer, id));
}

static AssignmentReturnValue evaluateProjectileContactTimeArrayAssignment(AssignmentReturnValue tIndex, DreamPlayer* tPlayer) {
	int id = evaluateAssignmentReturnAsNumber(tIndex);
	return makeNumberAssignmentReturn(getPlayerProjectileTimeSinceContact(tPlayer, id));
}

static AssignmentReturnValue evaluateProjectileGuardedTimeArrayAssignment(AssignmentReturnValue tIndex, DreamPlayer* tPlayer) {
	int id = evaluateAssignmentReturnAsNumber(tIndex);
	return makeNumberAssignmentReturn(getPlayerProjectileTimeSinceGuarded(tPlayer, id));
}

static AssignmentReturnValue evaluateProjectileHitTimeArrayAssignment(AssignmentReturnValue tIndex, DreamPlayer* tPlayer) {
	int id = evaluateAssignmentReturnAsNumber(tIndex);
	return makeNumberAssignmentReturn(getPlayerProjectileTimeSinceHit(tPlayer, id));
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
	else if (!strcmp("stagevar", test)) {
		AssignmentReturnValue b = evaluateAssignmentInternal(arrays->b, tPlayer);
		return evaluateStageVarArrayAssignment(b, tPlayer);
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
	else if (!strcmp("log", test)) {
		AssignmentReturnValue b = evaluateAssignmentInternal(arrays->b, tPlayer);
		return evaluateLogArrayAssignment(b);
	}
	else if (!strcmp("cos", test)) {
		AssignmentReturnValue b = evaluateAssignmentInternal(arrays->b, tPlayer);
		return evaluateCosineArrayAssignment(b);
	}
	else if (!strcmp("acos", test)) {
		AssignmentReturnValue b = evaluateAssignmentInternal(arrays->b, tPlayer);
		return evaluateAcosineArrayAssignment(b);
	}
	else if (!strcmp("sin", test)) {
		AssignmentReturnValue b = evaluateAssignmentInternal(arrays->b, tPlayer);
		return evaluateSineArrayAssignment(b);
	}
	else if (!strcmp("asin", test)) {
		AssignmentReturnValue b = evaluateAssignmentInternal(arrays->b, tPlayer);
		return evaluateAsineArrayAssignment(b);
	}
	else if (!strcmp("tan", test)) {
		AssignmentReturnValue b = evaluateAssignmentInternal(arrays->b, tPlayer);
		return evaluateTangentArrayAssignment(b);
	}
	else if (!strcmp("atan", test)) {
		AssignmentReturnValue b = evaluateAssignmentInternal(arrays->b, tPlayer);
		return evaluateAtangentArrayAssignment(b);
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
		return evaluateGetHitVarArrayAssignment(arrays->b, tPlayer);
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
	else if (!strcmp("animexist", test)) {
		AssignmentReturnValue b = evaluateAssignmentInternal(arrays->b, tPlayer);
		return evaluateAnimationExistArrayAssignment(b, tPlayer);
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
	else if (!strcmp("projcanceltime", test)) {
		AssignmentReturnValue b = evaluateAssignmentInternal(arrays->b, tPlayer);
		return evaluateProjectileCancelTimeArrayAssignment(b, tPlayer);
	}
	else if (!strcmp("projcontacttime", test)) {
		AssignmentReturnValue b = evaluateAssignmentInternal(arrays->b, tPlayer);
		return evaluateProjectileContactTimeArrayAssignment(b, tPlayer);
	}
	else if (!strcmp("projguardedtime", test)) {
		AssignmentReturnValue b = evaluateAssignmentInternal(arrays->b, tPlayer);
		return evaluateProjectileGuardedTimeArrayAssignment(b, tPlayer);
	}
	else if (!strcmp("projhittime", test)) {
		AssignmentReturnValue b = evaluateAssignmentInternal(arrays->b, tPlayer);
		return evaluateProjectileHitTimeArrayAssignment(b, tPlayer);
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
