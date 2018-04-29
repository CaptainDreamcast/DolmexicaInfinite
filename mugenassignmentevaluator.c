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
#include "dolmexicastoryscreen.h"

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
	return makeBooleanAssignmentReturn(ret);
}

static AssignmentReturnValue evaluateStateTypeAssignment(AssignmentReturnValue tCommand, DreamPlayer* tPlayer) {
	DreamMugenStateType playerState = getPlayerStateType(tPlayer);

	int ret;
	if (playerState == MUGEN_STATE_TYPE_STANDING) ret = strchr(tCommand.mValue, 's') != NULL;
	else if (playerState == MUGEN_STATE_TYPE_AIR) ret = strchr(tCommand.mValue, 'a') != NULL;
	else if (playerState == MUGEN_STATE_TYPE_CROUCHING) ret = strchr(tCommand.mValue, 'c') != NULL;
	else if (playerState == MUGEN_STATE_TYPE_LYING) ret = strchr(tCommand.mValue, 'l') != NULL;
	else {
		logWarningFormat("Undefined player state %d. Default to false.", playerState);
		ret = 0;
	}

	return makeBooleanAssignmentReturn(ret);
}

static AssignmentReturnValue evaluateMoveTypeAssignment(AssignmentReturnValue tCommand, DreamPlayer* tPlayer) {
	DreamMugenStateMoveType playerMoveType = getPlayerStateMoveType(tPlayer);

	char test[20];
	strcpy(test, tCommand.mValue);
	turnStringLowercase(test);

	int ret;
	if (playerMoveType == MUGEN_STATE_MOVE_TYPE_ATTACK) ret = strchr(test, 'a') != NULL;
	else if (playerMoveType == MUGEN_STATE_MOVE_TYPE_BEING_HIT) ret = strchr(test, 'h') != NULL;
	else if (playerMoveType == MUGEN_STATE_MOVE_TYPE_IDLE) ret = strchr(test, 'i') != NULL;
	else {
		logWarningFormat("Undefined player state %d. Default to false.", playerMoveType);
		ret = 0;
	}

	return makeBooleanAssignmentReturn(ret);
}

static AssignmentReturnValue evaluateAnimElemVectorAssignment(AssignmentReturnValue tCommand, DreamPlayer* tPlayer) {

	char number1[100], comma[10], oper[100], number2[100];
	int items = sscanf(tCommand.mValue, "%s %s %s %s", number1, comma, oper, number2);
	if(!strcmp("", number1) || strcmp(",", comma)) { 
		logWarningFormat("Unable to parse animelem vector assignment %s. Defaulting to bottom.", tCommand.mValue);
		return makeBooleanAssignmentReturn(0); // TODO: use bottom
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

	char divisor[100], comma[10], compareNumber[100];
	int items = sscanf(tCommand.mValue, "%s %s %s", divisor, comma, compareNumber);
	if (!strcmp("", divisor) || strcmp(",", comma) || items != 3) {
		logWarningFormat("Unable to parse timemod assignment %s. Defaulting to bottom.", tCommand.mValue);
		return makeBooleanAssignmentReturn(0); // TODO: use bottom
	}

	int divisorValue = atoi(divisor);
	int compareValue = atoi(compareNumber);
	int stateTime = getPlayerTimeInState(tPlayer);
	
	if (divisorValue == 0) {
		return makeBooleanAssignmentReturn(0);
	}

	int modValue = stateTime % divisorValue;
	int ret = modValue == compareValue;
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
	if (strcmp("[", openBrace) || !strcmp("", valString1) || strcmp(",", comma) || !strcmp("", valString2) || strcmp("]", closeBrace)) {
		logWarningFormat("Unable to parse range comparison assignment %s. Defaulting to bottom.", tRange.mValue);
		return makeBooleanAssignmentReturn(0); // TODO: use bottom
	}

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
	
	if (varSetAssignment->a->mType != MUGEN_ASSIGNMENT_TYPE_ARRAY){
		logWarningFormat("Incorrect varset type %d. Defaulting to bottom.", varSetAssignment->a->mType);
		return makeBooleanAssignmentReturn(0); // TODO: use bottom
	}
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
		logWarningFormat("Unrecognized varset name %s. Returning bottom.", a.mValue);
		return makeBooleanAssignmentReturn(0); // TODO: use bottom
	}

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
		logWarningFormat("Invalid hitdef type %d. Default to false.", type);
		isFlag1OK = 0;
	}

	if (!isFlag1OK) return makeBooleanAssignmentReturn(0);

	if (items == 1) return makeBooleanAssignmentReturn(isFlag1OK);
	if (strcmp(",", comma)) {
		logWarningFormat("Invalid hitdef attribute string %s. Defaulting to bottom.", tValue.mValue);
		return makeBooleanAssignmentReturn(0); // TODO: use bottom
	}

	int hasNext = 1;
	while (hasNext) {
		items = sscanf(pos, "%s %s%n", flag, comma, &positionsRead);
		if (items < 1) {
			logWarningFormat("Error reading next flag %s. Defaulting to bottom.", pos);
			return makeBooleanAssignmentReturn(0); // TODO: use bottom
		}
		pos += positionsRead;


		int isFlag2OK = evaluateSingleHitDefAttributeFlag2(flag, getHitDataAttackClass(tPlayer), getHitDataAttackType(tPlayer));
		if (isFlag2OK) {
			return makeBooleanAssignmentReturn(1);
		}

		if (items == 1) hasNext = 0;
		else if (strcmp(",", comma)) {
			logWarningFormat("Invalid hitdef attribute string %s. Defaulting to bottom.", tValue.mValue);
			return makeBooleanAssignmentReturn(0); // TODO: use bottom
		}
	}

	return makeBooleanAssignmentReturn(0);
}

// TODO: merge with AnimElem
static AssignmentReturnValue evaluateProjVectorAssignment(AssignmentReturnValue tCommand, DreamPlayer* tPlayer, int tProjectileID, int(*tTimeFunc)(DreamPlayer*, int)) {
	char number1[100], comma[10], oper[100], number2[100];
	int items = sscanf(tCommand.mValue, "%s %s %s %s", number1, comma, oper, number2);
	if (!strcmp("", number1) || strcmp(",", comma)) {
		logWarningFormat("Unable to parse proj vector assignment %s. Defaulting to bottom.", tCommand.mValue);
		return makeBooleanAssignmentReturn(0); // TODO: use bottom
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

	AssignmentReturnValue b = evaluateAssignmentInternal(comparisonAssignment->b, tPlayer);

	if (comparisonAssignment->a->mType == MUGEN_ASSIGNMENT_TYPE_VECTOR) {
		DreamMugenDependOnTwoAssignment* vectorAssignment = comparisonAssignment->a->mData;
		AssignmentReturnValue vecA = evaluateAssignmentInternal(vectorAssignment->a, tPlayer);

		if (isPlayerAccessVectorAssignment(vecA, tPlayer)) {
			DreamPlayer* target = getPlayerFromFirstVectorPartOrNullIfNonexistant(vecA, tPlayer);
			if (!target) {
				logWarning("Accessed player was NULL. Defaulting to bottom.");
				return makeBooleanAssignmentReturn(0); // TODO: use bottom
			}
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
		logWarningFormat("Unable to parse modulo of floats %s and %s. Returning bottom.", a.mValue, b.mValue);
		return makeBooleanAssignmentReturn(0); // TODO: use bottom
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

	return !strcmp("isinotherfilef", firstW) || !strcmp("isinotherfiles", firstW);
}

static AssignmentReturnValue evaluateAdditionSparkFile(AssignmentReturnValue a, AssignmentReturnValue b) {
	char firstW[200];
	int val1;

	int items = sscanf(a.mValue, "%s %d", firstW, &val1);
	if (items != 2) {
		logWarningFormat("Unable to parse sparkfile addition %s. Defaulting to bottom.", a.mValue);
		return makeBooleanAssignmentReturn(0); // TODO: use bottom
	}

	int val2 = evaluateAssignmentReturnAsNumber(b);

	AssignmentReturnValue ret;
	sprintf(ret.mValue, "%s %d", firstW, val1+val2);

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
	if (!target) {
		logWarningFormat("Unable to evaluate player vector assignment with NULL (%s). Defaulting to bottom.", tFirstValue.mValue);
		return makeBooleanAssignmentReturn(0); // TODO: use bottom
	}
	if (tVectorAssignment->b->mType != MUGEN_ASSIGNMENT_TYPE_VARIABLE && tVectorAssignment->b->mType != MUGEN_ASSIGNMENT_TYPE_ARRAY) {
		logWarningFormat("Invalid player vector assignment type %d. Defaulting to bottom.", tVectorAssignment->b->mType);
		return makeBooleanAssignmentReturn(0); // TODO: use bottom
	}

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
	if (!strcmp("", valString1) || strcmp(",", comma) || !strcmp("", valString2)) {
		logWarningFormat("Unable to parse range assignment %s. Defaulting to bottom.", a.mValue);
		return makeBooleanAssignmentReturn(0); // TODO: use bottom
	}

	int val1 = atoi(valString1);
	int val2 = atoi(valString2);
	if (rangeAssignment->mExcludeLeft) val1++;
	if (rangeAssignment->mExcludeRight) val2--;

	AssignmentReturnValue ret;
	sprintf(ret.mValue, "[ %d , %d ]", val1, val2);
	return ret;
}

typedef AssignmentReturnValue(*VariableFunction)(DreamPlayer*);
typedef AssignmentReturnValue(*ArrayFunction)(DreamMugenDependOnTwoAssignment*, DreamPlayer*);

static struct {
	StringMap mVariables; // contains VariableFunction
	StringMap mConstants; // contains VariableFunction
	StringMap mArrays; // contains ArrayFunction
} gVariableHandler;

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
		sprintf(ret.mValue, "isinotherfile%c %s", testString[0], testString + 1);
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
		logWarningFormat("Unknown stage variable %s. Returning bottom.", var);
		return makeBooleanAssignmentReturn(0); // TODO: use bottom
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
	if (items != 3 || strcmp(",", comma)) {
		logWarningFormat("Unable to parse log array assignment %s. Defaulting to bottom.", text);
		return makeBooleanAssignmentReturn(0); // TODO: use bottom
	}

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

	if(!string_map_contains(&gVariableHandler.mConstants, var)) {
		logWarningFormat("Unrecognized constant %s. Returning bottom.", var);
		return makeBooleanAssignmentReturn(0); // TODO: use bottom
	}

	VariableFunction func = string_map_get(&gVariableHandler.mConstants, var);
	return func(tPlayer);
}

static AssignmentReturnValue evaluateGetHitVarArrayAssignment(DreamMugenAssignment* tIndexAssignment, DreamPlayer* tPlayer) {
	if (tIndexAssignment->mType != MUGEN_ASSIGNMENT_TYPE_VARIABLE) {
		logWarningFormat("Unable to parse get hit var array assignment of type %d. Defaulting to bottom.", tIndexAssignment->mType);
		return makeBooleanAssignmentReturn(0); // TODO: use bottom
	}

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
		logWarningFormat("Unrecognized GetHitVar Constant %s. Returning bottom.", var);
		return makeBooleanAssignmentReturn(0); // TODO: use bottom
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
	if (strcmp(",", comma1) || strcmp(",", comma2) || !strcmp("", condText) || !strcmp("", yesText) || !strcmp("", noText)) {
		logWarningFormat("Unable to parse if else array assignment %s. Defaulting to bottom.", tIndex.mValue);
		return makeBooleanAssignmentReturn(0); // TODO: use bottom
	}


	AssignmentReturnValue condRet = makeStringAssignmentReturn(condText);
	AssignmentReturnValue yesRet = makeStringAssignmentReturn(yesText);
	AssignmentReturnValue noRet = makeStringAssignmentReturn(noText);

	int cond = evaluateAssignmentReturnAsBool(condRet);

	if (cond) return yesRet;
	else return noRet;
}

static AssignmentReturnValue evaluateCondArrayAssignment(DreamMugenAssignment* tCondVector, DreamPlayer* tPlayer) {
	if (tCondVector->mType != MUGEN_ASSIGNMENT_TYPE_VECTOR) {
		logWarningFormat("Invalid cond array cond vector type %d. Defaulting to bottom.", tCondVector->mType);
		return makeBooleanAssignmentReturn(0); // TODO: use bottom
	}
	DreamMugenDependOnTwoAssignment* firstV = tCondVector->mData;
	if(firstV->b->mType != MUGEN_ASSIGNMENT_TYPE_VECTOR) {
		logWarningFormat("Invalid cond array second vector type %d. Defaulting to bottom.", firstV->b->mType);
		return makeBooleanAssignmentReturn(0); // TODO: use bottom
	}
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





static AssignmentReturnValue varFunction(DreamMugenDependOnTwoAssignment* tArrays, DreamPlayer* tPlayer) { return evaluateVarArrayAssignment(evaluateAssignmentInternal(tArrays->b, tPlayer), tPlayer); }
static AssignmentReturnValue sysVarFunction(DreamMugenDependOnTwoAssignment* tArrays, DreamPlayer* tPlayer) { return evaluateSysVarArrayAssignment(evaluateAssignmentInternal(tArrays->b, tPlayer), tPlayer); }
static AssignmentReturnValue fVarFunction(DreamMugenDependOnTwoAssignment* tArrays, DreamPlayer* tPlayer) { return evaluateFVarArrayAssignment(evaluateAssignmentInternal(tArrays->b, tPlayer), tPlayer); }
static AssignmentReturnValue sysFVarFunction(DreamMugenDependOnTwoAssignment* tArrays, DreamPlayer* tPlayer) { return evaluateSysFVarArrayAssignment(evaluateAssignmentInternal(tArrays->b, tPlayer), tPlayer); }
static AssignmentReturnValue stageVarFunction(DreamMugenDependOnTwoAssignment* tArrays, DreamPlayer* tPlayer) { return evaluateStageVarArrayAssignment(evaluateAssignmentInternal(tArrays->b, tPlayer), tPlayer); }
static AssignmentReturnValue absFunction(DreamMugenDependOnTwoAssignment* tArrays, DreamPlayer* tPlayer) { return evaluateAbsArrayAssignment(evaluateAssignmentInternal(tArrays->b, tPlayer)); }
static AssignmentReturnValue expFunction(DreamMugenDependOnTwoAssignment* tArrays, DreamPlayer* tPlayer) { return evaluateExpArrayAssignment(evaluateAssignmentInternal(tArrays->b, tPlayer)); }
static AssignmentReturnValue lnFunction(DreamMugenDependOnTwoAssignment* tArrays, DreamPlayer* tPlayer) { return evaluateNaturalLogArrayAssignment(evaluateAssignmentInternal(tArrays->b, tPlayer)); }
static AssignmentReturnValue logFunction(DreamMugenDependOnTwoAssignment* tArrays, DreamPlayer* tPlayer) { return evaluateLogArrayAssignment(evaluateAssignmentInternal(tArrays->b, tPlayer)); }
static AssignmentReturnValue cosFunction(DreamMugenDependOnTwoAssignment* tArrays, DreamPlayer* tPlayer) { return evaluateCosineArrayAssignment(evaluateAssignmentInternal(tArrays->b, tPlayer)); }
static AssignmentReturnValue acosFunction(DreamMugenDependOnTwoAssignment* tArrays, DreamPlayer* tPlayer) { return evaluateAcosineArrayAssignment(evaluateAssignmentInternal(tArrays->b, tPlayer)); }
static AssignmentReturnValue sinFunction(DreamMugenDependOnTwoAssignment* tArrays, DreamPlayer* tPlayer) { return evaluateSineArrayAssignment(evaluateAssignmentInternal(tArrays->b, tPlayer)); }
static AssignmentReturnValue asinFunction(DreamMugenDependOnTwoAssignment* tArrays, DreamPlayer* tPlayer) { return evaluateAsineArrayAssignment(evaluateAssignmentInternal(tArrays->b, tPlayer)); }
static AssignmentReturnValue tanFunction(DreamMugenDependOnTwoAssignment* tArrays, DreamPlayer* tPlayer) { return evaluateTangentArrayAssignment(evaluateAssignmentInternal(tArrays->b, tPlayer)); }
static AssignmentReturnValue atanFunction(DreamMugenDependOnTwoAssignment* tArrays, DreamPlayer* tPlayer) { return evaluateAtangentArrayAssignment(evaluateAssignmentInternal(tArrays->b, tPlayer)); }
static AssignmentReturnValue floorFunction(DreamMugenDependOnTwoAssignment* tArrays, DreamPlayer* tPlayer) { return evaluateFloorArrayAssignment(evaluateAssignmentInternal(tArrays->b, tPlayer)); }
static AssignmentReturnValue ceilFunction(DreamMugenDependOnTwoAssignment* tArrays, DreamPlayer* tPlayer) { return evaluateCeilArrayAssignment(evaluateAssignmentInternal(tArrays->b, tPlayer)); }
static AssignmentReturnValue constFunction(DreamMugenDependOnTwoAssignment* tArrays, DreamPlayer* tPlayer) { return evaluateConstArrayAssignment(evaluateAssignmentInternal(tArrays->b, tPlayer), tPlayer); }
static AssignmentReturnValue getHitVarFunction(DreamMugenDependOnTwoAssignment* tArrays, DreamPlayer* tPlayer) { return evaluateGetHitVarArrayAssignment(tArrays->b, tPlayer); }
static AssignmentReturnValue animElemTimeFunction(DreamMugenDependOnTwoAssignment* tArrays, DreamPlayer* tPlayer) { return evaluateAnimationElementTimeArrayAssignment(evaluateAssignmentInternal(tArrays->b, tPlayer), tPlayer); }
static AssignmentReturnValue animElemNoFunction(DreamMugenDependOnTwoAssignment* tArrays, DreamPlayer* tPlayer) { return evaluateAnimationElementNumberArrayAssignment(evaluateAssignmentInternal(tArrays->b, tPlayer), tPlayer); }
static AssignmentReturnValue ifElseFunction(DreamMugenDependOnTwoAssignment* tArrays, DreamPlayer* tPlayer) { return evaluateIfElseArrayAssignment(evaluateAssignmentInternal(tArrays->b, tPlayer)); }
static AssignmentReturnValue condFunction(DreamMugenDependOnTwoAssignment* tArrays, DreamPlayer* tPlayer) { return evaluateCondArrayAssignment(tArrays->b, tPlayer); }
static AssignmentReturnValue animExistFunction(DreamMugenDependOnTwoAssignment* tArrays, DreamPlayer* tPlayer) { return evaluateAnimationExistArrayAssignment(evaluateAssignmentInternal(tArrays->b, tPlayer), tPlayer); }
static AssignmentReturnValue selfAnimExistFunction(DreamMugenDependOnTwoAssignment* tArrays, DreamPlayer* tPlayer) { return evaluateSelfAnimationExistArrayAssignment(evaluateAssignmentInternal(tArrays->b, tPlayer), tPlayer); }
static AssignmentReturnValue const240pFunction(DreamMugenDependOnTwoAssignment* tArrays, DreamPlayer* tPlayer) { return evaluateConstCoordinatesArrayAssignment(evaluateAssignmentInternal(tArrays->b, tPlayer), 240); }
static AssignmentReturnValue const480pFunction(DreamMugenDependOnTwoAssignment* tArrays, DreamPlayer* tPlayer) { return evaluateConstCoordinatesArrayAssignment(evaluateAssignmentInternal(tArrays->b, tPlayer), 480); }
static AssignmentReturnValue const720pFunction(DreamMugenDependOnTwoAssignment* tArrays, DreamPlayer* tPlayer) { return evaluateConstCoordinatesArrayAssignment(evaluateAssignmentInternal(tArrays->b, tPlayer), 720); }
static AssignmentReturnValue externalFileFunction(DreamMugenDependOnTwoAssignment* tArrays, DreamPlayer* tPlayer) { return evaluateExternalFileAnimationArrayAssignment(evaluateAssignmentInternal(tArrays->a, tPlayer), evaluateAssignmentInternal(tArrays->b, tPlayer)); }
static AssignmentReturnValue numTargetArrayFunction(DreamMugenDependOnTwoAssignment* tArrays, DreamPlayer* tPlayer) { return evaluateNumTargetArrayAssignment(evaluateAssignmentInternal(tArrays->b, tPlayer), tPlayer); }
static AssignmentReturnValue numHelperArrayFunction(DreamMugenDependOnTwoAssignment* tArrays, DreamPlayer* tPlayer) { return evaluateNumHelperArrayAssignment(evaluateAssignmentInternal(tArrays->b, tPlayer), tPlayer); }
static AssignmentReturnValue numExplodArrayFunction(DreamMugenDependOnTwoAssignment* tArrays, DreamPlayer* tPlayer) { return evaluateNumExplodArrayAssignment(evaluateAssignmentInternal(tArrays->b, tPlayer), tPlayer); }
static AssignmentReturnValue isHelperArrayFunction(DreamMugenDependOnTwoAssignment* tArrays, DreamPlayer* tPlayer) { return evaluateIsHelperArrayAssignment(evaluateAssignmentInternal(tArrays->b, tPlayer), tPlayer); }
static AssignmentReturnValue helperFunction(DreamMugenDependOnTwoAssignment* tArrays, DreamPlayer* tPlayer) { return evaluateTargetArrayAssignment(evaluateAssignmentInternal(tArrays->b, tPlayer), "helper"); }
static AssignmentReturnValue enemyNearFunction(DreamMugenDependOnTwoAssignment* tArrays, DreamPlayer* tPlayer) { return evaluateTargetArrayAssignment(evaluateAssignmentInternal(tArrays->b, tPlayer), "enemynear"); }
static AssignmentReturnValue playerIDFunction(DreamMugenDependOnTwoAssignment* tArrays, DreamPlayer* tPlayer) { return evaluateTargetArrayAssignment(evaluateAssignmentInternal(tArrays->b, tPlayer), "playerid"); }
static AssignmentReturnValue playerIDExistFunction(DreamMugenDependOnTwoAssignment* tArrays, DreamPlayer* tPlayer) { return evaluatePlayerIDExistArrayAssignment(evaluateAssignmentInternal(tArrays->b, tPlayer), tPlayer); }
static AssignmentReturnValue numProjIDFunction(DreamMugenDependOnTwoAssignment* tArrays, DreamPlayer* tPlayer) { return evaluateNumberOfProjectilesWithIDArrayAssignment(evaluateAssignmentInternal(tArrays->b, tPlayer), tPlayer); }
static AssignmentReturnValue projCancelTimeFunction(DreamMugenDependOnTwoAssignment* tArrays, DreamPlayer* tPlayer) { return evaluateProjectileCancelTimeArrayAssignment(evaluateAssignmentInternal(tArrays->b, tPlayer), tPlayer); }
static AssignmentReturnValue projContactTimeFunction(DreamMugenDependOnTwoAssignment* tArrays, DreamPlayer* tPlayer) { return evaluateProjectileContactTimeArrayAssignment(evaluateAssignmentInternal(tArrays->b, tPlayer), tPlayer); }
static AssignmentReturnValue projGuardedTimeFunction(DreamMugenDependOnTwoAssignment* tArrays, DreamPlayer* tPlayer) { return evaluateProjectileGuardedTimeArrayAssignment(evaluateAssignmentInternal(tArrays->b, tPlayer), tPlayer); }
static AssignmentReturnValue projHitTimeFunction(DreamMugenDependOnTwoAssignment* tArrays, DreamPlayer* tPlayer) { return evaluateProjectileHitTimeArrayAssignment(evaluateAssignmentInternal(tArrays->b, tPlayer), tPlayer); }


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


static AssignmentReturnValue evaluateArrayAssignment(DreamMugenAssignment* tAssignment, DreamPlayer* tPlayer) {
	DreamMugenDependOnTwoAssignment* arrays = tAssignment->mData;

	char test[100];
	if (arrays->a->mType != MUGEN_ASSIGNMENT_TYPE_VARIABLE) {
		logWarningFormat("Invalid array type %d. Defaulting to bottom.", arrays->a->mType);
		return makeBooleanAssignmentReturn(0); // TODO: use bottom
	}
	DreamMugenVariableAssignment* arrayName = arrays->a->mData;
	strcpy(test, arrayName->mName);
	turnStringLowercase(test);

	
	if(!string_map_contains(&gVariableHandler.mArrays, test)) {
		logWarningFormat("Unknown array %s. Returning bottom.", test); 
		return makeBooleanAssignmentReturn(0); // TODO: use bottom
	}

	ArrayFunction func = string_map_get(&gVariableHandler.mArrays, test);
	return func(arrays, tPlayer);
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
	if (!tAssignment || !tAssignment->mData) {
		logWarning("Invalid assignment. Defaulting to bottom.");
		return makeBooleanAssignmentReturn(0); // TODO: use bottom
	}

	if (tAssignment->mType == MUGEN_ASSIGNMENT_TYPE_OR) {
		return evaluateOrAssignment(tAssignment, tPlayer);
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
		logWarningFormat("Unidentified assignment type %d. Returning bottom.", tAssignment->mType);
		return makeBooleanAssignmentReturn(0); // TODO: use bottom
	}
}







static AssignmentReturnValue timeStoryFunction(DreamPlayer* tPlayer) { return makeNumberAssignmentReturn(getDolmexicaStoryTimeInState()); }

static void setupStoryVariableAssignments() {
	gVariableHandler.mVariables = new_string_map();
	string_map_push(&gVariableHandler.mVariables, "time", timeStoryFunction);
}

//static AssignmentReturnValue movementDownFrictionThresholdFunction(DreamPlayer* tPlayer) { return makeFloatAssignmentReturn(getPlayerLyingDownFrictionThreshold(tPlayer)); }

static void setupStoryConstantAssignments() {
	gVariableHandler.mConstants = new_string_map();
}

static AssignmentReturnValue evaluateAnimTimeStoryArrayAssignment(AssignmentReturnValue tIndex) {
	int id = evaluateAssignmentReturnAsNumber(tIndex);
	return makeNumberAssignmentReturn(getDolmexicaStoryAnimationTimeLeft(id));
}

static AssignmentReturnValue animTimeStoryFunction(DreamMugenDependOnTwoAssignment* tArrays, DreamPlayer* tPlayer) { return evaluateAnimTimeStoryArrayAssignment(evaluateAssignmentInternal(tArrays->b, tPlayer)); }

static void setupStoryArrayAssignments() {
	gVariableHandler.mArrays = new_string_map();

	string_map_push(&gVariableHandler.mArrays, "animtime", animTimeStoryFunction);
}

void setupDreamStoryAssignmentEvaluator()
{
	setupStoryVariableAssignments();
	setupStoryConstantAssignments();
	setupStoryArrayAssignments();
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

Vector3DI evaluateDreamAssignmentAndReturnAsVector3DI(DreamMugenAssignment * tAssignment, DreamPlayer * tPlayer)
{
	AssignmentReturnValue ret = evaluateAssignmentInternal(tAssignment, tPlayer);

	int x, y, z;
	char tX[100], comma1[100], tY[100], comma2[100], tZ[100];

	int items = sscanf(ret.mValue, "%s %s %s %s %s", tX, comma1, tY, comma2, tZ);

	if (items >= 1) x = atoi(tX);
	else x = 0;
	if (items >= 3) y = atoi(tY);
	else y = 0;
	if (items >= 5) z = atoi(tZ);
	else z = 0;

	return makeVector3DI(x, y, z);
}
