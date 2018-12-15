#include "mugencommandreader.h"

#include <stdio.h>
#include <assert.h>
#include <string.h>

#include <prism/stlutil.h>
#include <prism/system.h>
#include <prism/log.h>
#include <prism/memoryhandler.h>
#include <prism/mugendefreader.h>

using namespace std;

static struct {
	int mDefaultTime;
	int mDefaultBufferTime;

} gCommandReader;

static int isCommand(char* tLowercaseName) {
	return !strcmp("command", tLowercaseName);
}

static int isStateDef(char* tLowercaseName) {
	char name[100];
	sscanf(tLowercaseName, "%s", name);
	

	return !strcmp("statedef", name);
}

static int isRemap(char* tLowercaseName) {
	return !strcmp("remap", tLowercaseName);
}

static void handleRemap() {

}

static int isDefaults(char* tLowercaseName) {
	return !strcmp("defaults", tLowercaseName);
}

static void setDefaultInteger(int* tDst, MugenDefScriptGroupElement* tElement) {
	assert(isMugenDefNumberVariableAsElement(tElement));
	*tDst = getMugenDefNumberVariableAsElement(tElement);
}

static void handleSingleDefault(void* tCaller, void* tData) {
	(void)tCaller;

	MugenDefScriptGroupElement* element = (MugenDefScriptGroupElement*)tData;

	if (element->mName == "command.time") {
		setDefaultInteger(&gCommandReader.mDefaultTime, element);
	} else if (element->mName == "command.buffer.time") {
		setDefaultInteger(&gCommandReader.mDefaultBufferTime, element);
	}
	else {
		logWarningFormat("Unable to parse default state %s.", element->mName);
	}

}

static void handleDefaults(MugenDefScriptGroup* tGroup) {
	list_map(&tGroup->mOrderedElementList, handleSingleDefault, NULL);
}

typedef struct {
	char mName[100];
	DreamMugenCommandInput mInput;
} CommandCaller;

static int isInputStepWithMultipleSubsteps(char* tInputStep) {
	return strchr(tInputStep, '+') != NULL;
}

static void parseSingleInputStepAndAddItToInput(Vector* tVector, char* tInputStep, int tDoesNotAllowOtherInputBetween);

static void handleInputStepWithMultipleSubsteps(Vector* tVector, char* tInputStep, int tDoesNotAllowOtherInputBetween) {
	DreamMugenCommandInputStep* multipleStep = (DreamMugenCommandInputStep*)allocMemory(sizeof(DreamMugenCommandInputStep));
	multipleStep->mTarget = MUGEN_COMMAND_INPUT_STEP_TARGET_MULTIPLE;
	multipleStep->mType = MUGEN_COMMAND_INPUT_STEP_TYPE_MULTIPLE;
	multipleStep->mDoesNotAllowOtherInputBetween = tDoesNotAllowOtherInputBetween;

	DreamMugenCommandInputStepMultipleTargetData* data = (DreamMugenCommandInputStepMultipleTargetData*)allocMemory(sizeof(DreamMugenCommandInputStepMultipleTargetData));
	multipleStep->mData = data;
	data->mSubSteps = new_vector();

	char* s = tInputStep;
	while (s) {
		char text[100];
		strcpy(text, s);
		char* sep = strchr(text, '+');
		if (sep) *sep = '\0';
		parseSingleInputStepAndAddItToInput(&data->mSubSteps, text, tDoesNotAllowOtherInputBetween);

		s = strchr(s, '+');
		if (s) s++;
	}

	vector_push_back_owned(tVector, multipleStep);
}

static int isHoldingInputStep(char* tInputStep) {
	return tInputStep[0] == '/';
}



static DreamMugenCommandInputStepTarget extractTargetFromInputStep(char* tInputStep) {
	int isDown = strchr(tInputStep, 'D') != NULL;
	int isUp = strchr(tInputStep, 'U') != NULL;
	int isForward = strchr(tInputStep, 'F') != NULL;
	int isBackward = strchr(tInputStep, 'B') != NULL;

	int isA = strchr(tInputStep, 'a') != NULL;
	int isB = strchr(tInputStep, 'b') != NULL;
	int isC = strchr(tInputStep, 'c') != NULL;
	int isX = strchr(tInputStep, 'x') != NULL;
	int isY = strchr(tInputStep, 'y') != NULL;
	int isZ = strchr(tInputStep, 'z') != NULL;
	int isStart = strchr(tInputStep, 's') != NULL;

	int isMultiDirection = strchr(tInputStep, '$') != NULL;

	uint16_t mask = 0;
	mask |= isDown << 0;
	mask |= isUp << 1;
	mask |= isForward << 2;
	mask |= isBackward << 3;

	mask |= isA << 4;
	mask |= isB << 5;
	mask |= isC << 6;
	mask |= isX << 7;
	mask |= isY << 8;
	mask |= isZ << 9;

	mask |= isStart << 10;

	mask |= isMultiDirection << 11;

	DreamMugenCommandInputStepTarget ret;
	if (mask == 1 << 0) ret = MUGEN_COMMAND_INPUT_STEP_TARGET_DOWN;
	else if (mask == 1 << 1) ret = MUGEN_COMMAND_INPUT_STEP_TARGET_UP;
	else if (mask == 1 << 2) ret = MUGEN_COMMAND_INPUT_STEP_TARGET_FORWARD;
	else if (mask == 1 << 3) ret = MUGEN_COMMAND_INPUT_STEP_TARGET_BACKWARD;
	else if (mask & (1 << 4)) ret = MUGEN_COMMAND_INPUT_STEP_TARGET_A;
	else if (mask & (1 << 5)) ret = MUGEN_COMMAND_INPUT_STEP_TARGET_B;
	else if (mask & (1 << 6)) ret = MUGEN_COMMAND_INPUT_STEP_TARGET_C;
	else if (mask & (1 << 7)) ret = MUGEN_COMMAND_INPUT_STEP_TARGET_X;
	else if (mask & (1 << 8)) ret = MUGEN_COMMAND_INPUT_STEP_TARGET_Y;
	else if (mask & (1 << 9)) ret = MUGEN_COMMAND_INPUT_STEP_TARGET_Z;
	else if (mask & (1 << 10)) ret = MUGEN_COMMAND_INPUT_STEP_TARGET_START;
	else if (mask == ((1 << 0) | (1 << 2))) ret = MUGEN_COMMAND_INPUT_STEP_TARGET_DOWN_FORWARD;
	else if (mask == ((1 << 0) | (1 << 3))) ret = MUGEN_COMMAND_INPUT_STEP_TARGET_DOWN_BACKWARD;
	else if (mask == ((1 << 1) | (1 << 2))) ret = MUGEN_COMMAND_INPUT_STEP_TARGET_UP_FORWARD;
	else if (mask == ((1 << 1) | (1 << 3))) ret = MUGEN_COMMAND_INPUT_STEP_TARGET_UP_BACKWARD;
	else if (mask == ((1 << 0) | (1 << 11))) ret = MUGEN_COMMAND_INPUT_STEP_TARGET_MULTI_DOWN;
	else if (mask == ((1 << 1) | (1 << 11))) ret = MUGEN_COMMAND_INPUT_STEP_TARGET_MULTI_UP;
	else if (mask == ((1 << 2) | (1 << 11))) ret = MUGEN_COMMAND_INPUT_STEP_TARGET_MULTI_FORWARD;
	else if (mask == ((1 << 3) | (1 << 11))) ret = MUGEN_COMMAND_INPUT_STEP_TARGET_MULTI_BACKWARD;
	else {
		logWarningFormat("Unable to determine target %s. Defaulting to invalid input.", tInputStep);
		ret = (DreamMugenCommandInputStepTarget)-1;
	}

	return ret;
}

static void handleHoldingInputStep(Vector* tVector, char* tInputStep, int tDoesNotAllowOtherInputBetween) {
	DreamMugenCommandInputStep* e = (DreamMugenCommandInputStep*)allocMemory(sizeof(DreamMugenCommandInputStep));
	e->mTarget = extractTargetFromInputStep(tInputStep);
	e->mType = MUGEN_COMMAND_INPUT_STEP_TYPE_HOLDING;
	e->mDoesNotAllowOtherInputBetween = tDoesNotAllowOtherInputBetween;

	vector_push_back_owned(tVector, e);
}

int isNothingInbetweenStep(char* tInputStep) {
	return tInputStep[0] == '>';
}

static void handleNothingInbetweenStep(Vector* tVector, char* tInputStep) {
	parseSingleInputStepAndAddItToInput(tVector, tInputStep + 1, 1);
}

int isReleaseStep(char* tInputStep) {
	return tInputStep[0] == '~';
}

static Duration extractDurationFromReleaseInputStep(char* tInputStep) {
	assert(tInputStep[0] == '~');

	char temp[100];
	strcpy(temp, tInputStep + 1);

	char* end = temp;
	while (*end >= '0' && *end <= '9') end++;

	if (end == temp) return 0;
	
	*end = '\0';
	return atoi(temp);
}

static void handleReleaseStep(Vector* tVector, char* tInputStep, int tDoesNotAllowOtherInputBetween) {
	DreamMugenCommandInputStepReleaseData* data = (DreamMugenCommandInputStepReleaseData*)allocMemory(sizeof(DreamMugenCommandInputStepReleaseData));
	data->mDuration = extractDurationFromReleaseInputStep(tInputStep);

	DreamMugenCommandInputStep* e = (DreamMugenCommandInputStep*)allocMemory(sizeof(DreamMugenCommandInputStep));
	e->mTarget = extractTargetFromInputStep(tInputStep);
	e->mType = MUGEN_COMMAND_INPUT_STEP_TYPE_RELEASE;
	e->mData = data;
	e->mDoesNotAllowOtherInputBetween = tDoesNotAllowOtherInputBetween;

	vector_push_back_owned(tVector, e);
}

static void handlePressStep(Vector* tVector, char* tInputStep, int tDoesNotAllowOtherInputBetween) {
	DreamMugenCommandInputStep* e = (DreamMugenCommandInputStep*)allocMemory(sizeof(DreamMugenCommandInputStep));
	e->mTarget = extractTargetFromInputStep(tInputStep);
	e->mType = MUGEN_COMMAND_INPUT_STEP_TYPE_PRESS;
	e->mDoesNotAllowOtherInputBetween = tDoesNotAllowOtherInputBetween;

	vector_push_back_owned(tVector, e);
}

static void parseSingleInputStepAndAddItToInput(Vector* tVector, char* tInputStep, int tDoesNotAllowOtherInputBetween) {
	if (isInputStepWithMultipleSubsteps(tInputStep)) {
		handleInputStepWithMultipleSubsteps(tVector, tInputStep, tDoesNotAllowOtherInputBetween);
	}
	else if (isHoldingInputStep(tInputStep)) {
		handleHoldingInputStep(tVector, tInputStep, tDoesNotAllowOtherInputBetween);
	}
	else if (isNothingInbetweenStep(tInputStep)) {
		handleNothingInbetweenStep(tVector, tInputStep);
	}
	else if (isReleaseStep(tInputStep)) {
		handleReleaseStep(tVector, tInputStep, tDoesNotAllowOtherInputBetween);
	}
	else {
		handlePressStep(tVector, tInputStep, tDoesNotAllowOtherInputBetween);
	}
	
}

static void handleCommandInputEntryAsVector(CommandCaller* tCaller, MugenDefScriptGroupElement* tElement) {
	assert(isMugenDefStringVectorVariableAsElement(tElement));

	MugenStringVector input = getMugenDefStringVectorVariableAsElement(tElement);
	tCaller->mInput.mInputSteps = new_vector();
	int i;
	for (i = 0; i < input.mSize; i++) {
		parseSingleInputStepAndAddItToInput(&tCaller->mInput.mInputSteps, input.mElement[i], 0);
	}
}

static void handleCommandInputEntryAsString(CommandCaller* tCaller, MugenDefScriptGroupElement* tElement) {
	assert(isMugenDefStringVariableAsElement(tElement));

	tCaller->mInput.mInputSteps = new_vector();
	char* command = getAllocatedMugenDefStringVariableAsElement(tElement);
	parseSingleInputStepAndAddItToInput(&tCaller->mInput.mInputSteps, command, 0);
	freeMemory(command);
}

static void handleCommandInputEntry(CommandCaller* tCaller, MugenDefScriptGroupElement* tElement) {
	
	if (isMugenDefStringVectorVariableAsElement(tElement)) {
		handleCommandInputEntryAsVector(tCaller, tElement);
	}
	else if (isMugenDefStringVariableAsElement(tElement)) {
		handleCommandInputEntryAsString(tCaller, tElement);
	}
	else {
		logWarningFormat("Unable to parse input entry %s.", tElement->mName);
	}
}

static void handleCommandNameEntry(CommandCaller* tCaller, MugenDefScriptGroupElement* tElement) {
	assert(isMugenDefStringVariableAsElement(tElement));

	char* name = getAllocatedMugenDefStringVariableAsElement(tElement);
	strcpy(tCaller->mName, name);
	freeMemory(name);
}

static void handleCommandTimeEntry(Duration* tDst, MugenDefScriptGroupElement* tElement) {
	assert(isMugenDefNumberVariableAsElement(tElement));
	*tDst = getMugenDefNumberVariableAsElement(tElement);
}


static void handleSingleCommandEntry(void* tCaller, void* tData) {
	CommandCaller* command = (CommandCaller*)tCaller;
	MugenDefScriptGroupElement* element = (MugenDefScriptGroupElement*)tData;

	if (element->mName == "name") {
		handleCommandNameEntry(command, element);
	} else if (element->mName == "command") {
		handleCommandInputEntry(command, element);
	} else if (element->mName == "time") {
		handleCommandTimeEntry(&command->mInput.mTime, element);
	}
	else if (element->mName == "buffer.time") {
		handleCommandTimeEntry(&command->mInput.mBufferTime, element);
	}
	else {
		logWarningFormat("Unable to determine type %s.", element->mName);
	}
}

static void addCallerToExistingCommand(DreamMugenCommand* tCommand, CommandCaller* tCaller) {
	DreamMugenCommandInput* input = (DreamMugenCommandInput*)allocMemory(sizeof(DreamMugenCommandInput));
	*input = tCaller->mInput;

	vector_push_back_owned(&tCommand->mInputs, input);
}

static void addEmptyCommandToCommands(DreamMugenCommands* tCommands, string tKey) {
	DreamMugenCommand e;
	e.mInputs = new_vector();
	assert(!stl_map_contains(tCommands->mCommands, tKey));
	tCommands->mCommands[tKey] = e;
}

static void addCallerToCommands(DreamMugenCommands* tCommands, CommandCaller* tCaller) {
	string key(tCaller->mName);

	if (!stl_map_contains(tCommands->mCommands, key)) {
		addEmptyCommandToCommands(tCommands, key);
	}
	assert(stl_map_contains(tCommands->mCommands, key));

	DreamMugenCommand* command = &tCommands->mCommands[key]; 
	addCallerToExistingCommand(command, tCaller);

}

static void handleCommand(DreamMugenCommands* tCommands, MugenDefScriptGroup* tGroup) {
	CommandCaller caller;
	caller.mInput.mTime = gCommandReader.mDefaultTime;
	caller.mInput.mBufferTime = gCommandReader.mDefaultBufferTime;

	list_map(&tGroup->mOrderedElementList, handleSingleCommandEntry, &caller);

	addCallerToCommands(tCommands, &caller);	
}

static void handleStateDef(MugenDefScriptGroup** tCurrentGroup) {
	while ((*tCurrentGroup)->mNext != NULL) {
		(*tCurrentGroup) = (*tCurrentGroup)->mNext;
	}
}



static void loadMugenCommandsFromDefScript(DreamMugenCommands* tCommands, MugenDefScript* tScript) {

	gCommandReader.mDefaultTime = 15;
	gCommandReader.mDefaultBufferTime = 1;

	MugenDefScriptGroup* current = tScript->mFirstGroup;

	while (current != NULL) {
		char lowercase[200];
		copyStringLowercase(lowercase, current->mName.data());

		if (isCommand(lowercase)) {
			handleCommand(tCommands, current);
		} else if (isStateDef(lowercase)) {
			handleStateDef(&current);
		} else if (isRemap(lowercase)) {
			handleRemap();
		}
		else if (isDefaults(lowercase)) {
			handleDefaults(current);
		}
		else {
			logWarningFormat("Unrecognized type %s.", current->mName);
		}

		current = current->mNext;
	}

}

static DreamMugenCommands makeEmptyMugenCommands() {
	DreamMugenCommands ret;
	stl_new_map(ret.mCommands);
	return ret;
}

DreamMugenCommands loadDreamMugenCommandFile(char * tPath)
{
	MugenDefScript script = loadMugenDefScript(tPath);
	DreamMugenCommands ret = makeEmptyMugenCommands();

    loadMugenCommandsFromDefScript(&ret, &script);

	unloadMugenDefScript(script);

	return ret;
}

static void unloadSingleInputStep(void* tCaller, void* tData);

static void unloadInputStepMultiple(DreamMugenCommandInputStep* e) {
	DreamMugenCommandInputStepMultipleTargetData* data = (DreamMugenCommandInputStepMultipleTargetData*)e->mData;

	vector_map(&data->mSubSteps, unloadSingleInputStep, NULL);
	delete_vector(&data->mSubSteps);

	freeMemory(data);
}

static void unloadInputStepRelease(DreamMugenCommandInputStep* e) {
	DreamMugenCommandInputStepReleaseData* data = (DreamMugenCommandInputStepReleaseData*)e->mData;
	freeMemory(data);
}

static void unloadSingleInputStep(void* tCaller, void* tData) {
	(void)tCaller;
	DreamMugenCommandInputStep* e = (DreamMugenCommandInputStep*)tData;
	
	if (e->mType == MUGEN_COMMAND_INPUT_STEP_TYPE_MULTIPLE) {
		unloadInputStepMultiple(e);
	}
	else if (e->mType == MUGEN_COMMAND_INPUT_STEP_TYPE_RELEASE) {
		unloadInputStepRelease(e);
	}
}

static void unloadSingleInput(void* tCaller, void* tData) {
	(void)tCaller;
	DreamMugenCommandInput* e = (DreamMugenCommandInput*)tData;
	vector_map(&e->mInputSteps, unloadSingleInputStep, NULL);
	delete_vector(&e->mInputSteps);
}

static void unloadSingleCommand(void* tCaller, const string& tKey, DreamMugenCommand& tData) {
	(void)tCaller;
	(void)tKey;
	DreamMugenCommand* e = &tData;
	vector_map(&e->mInputs, unloadSingleInput, NULL);
	delete_vector(&e->mInputs);
}

void unloadDreamMugenCommandFile(DreamMugenCommands * tCommands)
{
	stl_string_map_map(tCommands->mCommands, unloadSingleCommand);
	stl_delete_map(tCommands->mCommands);
}
