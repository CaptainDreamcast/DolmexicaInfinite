#include "mugencommandhandler.h"

#include <assert.h>

#include <prism/datastructures.h>
#include <prism/log.h>
#include <prism/system.h>
#include <prism/input.h>
#include <prism/math.h>

typedef struct {
	char mName[100];
	int mIsActive;
	Duration mNow;
	Duration mBufferTime;
} MugenCommandState;

typedef struct {
	StringMap mStates;
} MugenCommandStates;

typedef struct {
	char mName[100];
	DreamMugenCommandInput* mInput;
	int mStep;
	Duration mNow;
} ActiveMugenCommand;

typedef struct {
	int mIsBeingProcessed;

} InternalMugenCommandState;

typedef struct {
	DreamMugenCommands* tCommands;
	MugenCommandStates* tStates;
	StringMap mInternalStates;

	List mActiveCommands;

	DreamPlayer* mPlayer;
	int mIsFacingRight;
} RegisteredMugenCommand;

static struct {
	Vector mRegisteredCommands;

	uint32_t mHeldMask[2];
	uint32_t mPreviousHeldMask[2];
} gData;

static void loadMugenCommandHandler(void* tData) {
	(void)tData;
	gData.mRegisteredCommands = new_vector();
}

static void addSingleMugenCommandState(void* tCaller, char* tKey, void* tData) {
	(void)tData;
	RegisteredMugenCommand* s = tCaller;

	MugenCommandState* e = allocMemory(sizeof(MugenCommandState));
	strcpy(e->mName, tKey);
	e->mIsActive = 0;
	string_map_push_owned(&s->tStates->mStates, tKey, e);
	
	InternalMugenCommandState* internalState = allocMemory(sizeof(InternalMugenCommandState));
	internalState->mIsBeingProcessed = 0;
	string_map_push_owned(&s->mInternalStates, tKey, internalState);
}

static void setupMugenCommandStates(RegisteredMugenCommand* e) {
	e->tStates->mStates = new_string_map();
	string_map_map(&e->tCommands->mCommands, addSingleMugenCommandState, e);
}

int registerDreamMugenCommands(DreamPlayer* tPlayer, DreamMugenCommands * tCommands)
{
	
	RegisteredMugenCommand* e = allocMemory(sizeof(RegisteredMugenCommand));
	e->mActiveCommands = new_list();
	e->tCommands = tCommands;
	e->tStates = allocMemory(sizeof(MugenCommandStates));;
	e->mInternalStates = new_string_map();
	e->mPlayer = tPlayer;
	e->mIsFacingRight = 1;

	setupMugenCommandStates(e);

	vector_push_back_owned(&gData.mRegisteredCommands, e);
	return vector_size(&gData.mRegisteredCommands) - 1;
}

int isDreamCommandActive(int tID, char * tCommandName)
{
	RegisteredMugenCommand* e = vector_get(&gData.mRegisteredCommands, tID);
	if (!string_map_contains(&e->tStates->mStates, tCommandName)) {
		logWarningFormat("Querying nonexistant command name %s.", tCommandName);
		return 0;
	}
	MugenCommandState* state = string_map_get(&e->tStates->mStates, tCommandName);
	
	return state->mIsActive;
}

static void setCommandStateActive(RegisteredMugenCommand* tRegisteredCommand, char* tName, Duration tBufferTime);

void setDreamPlayerCommandActiveForAI(int tID, char * tCommandName, Duration tBufferTime)
{
	RegisteredMugenCommand* e = vector_get(&gData.mRegisteredCommands, tID);
	setCommandStateActive(e, tCommandName, tBufferTime);
}

void setDreamMugenCommandFaceDirection(int tID, FaceDirection tDirection)
{
	RegisteredMugenCommand* e = vector_get(&gData.mRegisteredCommands, tID);
	e->mIsFacingRight = tDirection == FACE_DIRECTION_RIGHT;
}

static int handleSingleCommandInputStepAndReturnIfActive(DreamMugenCommandInputStep* tStep, int* oIsStepOver, DreamPlayer* tPlayer, int tIsFacingRight);

typedef struct {
	int isActiveAmount;
	int isStepOverAmount;
	DreamPlayer* mPlayer;
	int mIsFacingRight;
} MultipleCommandInputStepCaller;

static void handleSingleMultipleCommandInputStep(void* tCaller, void* tData) {
	MultipleCommandInputStepCaller* caller = tCaller;
	DreamMugenCommandInputStep* step = tData;

	int isStepOver = 0;
	int isActive = handleSingleCommandInputStepAndReturnIfActive(step, &isStepOver, caller->mPlayer, caller->mIsFacingRight);

	caller->isActiveAmount += isActive;
	caller->isStepOverAmount += isStepOver;
}

static int handleMultipleCommandInputStep(DreamMugenCommandInputStep* tStep, int* oIsStepOver, DreamPlayer* tPlayer, int tIsFacingRight) {
	DreamMugenCommandInputStepMultipleTargetData* data = tStep->mData;
	assert(tStep->mTarget == MUGEN_COMMAND_INPUT_STEP_TARGET_MULTIPLE);

	MultipleCommandInputStepCaller caller;
	caller.isActiveAmount = 0;
	caller.isStepOverAmount = 0;
	caller.mPlayer = tPlayer;
	caller.mIsFacingRight = tIsFacingRight;
	vector_map(&data->mSubSteps, handleSingleMultipleCommandInputStep, &caller);

	if (caller.isActiveAmount < vector_size(&data->mSubSteps)) return 0;

	assert(caller.isStepOverAmount == 0 || caller.isStepOverAmount == vector_size(&data->mSubSteps));

	if (caller.isStepOverAmount) *oIsStepOver = 1;
	else *oIsStepOver = 0;

	return 1;
}

#define MASK_A (1 << 0)
#define MASK_B (1 << 1)
#define MASK_C (1 << 2)
#define MASK_X (1 << 3)
#define MASK_Y (1 << 4)
#define MASK_Z (1 << 5)

#define MASK_START (1 << 6)

#define MASK_LEFT (1 << 7)
#define MASK_RIGHT (1 << 8)
#define MASK_UP (1 << 9)
#define MASK_DOWN (1 << 10)

#define MASK_DOWN_LEFT (MASK_DOWN | MASK_LEFT)
#define MASK_DOWN_RIGHT (MASK_DOWN | MASK_RIGHT)
#define MASK_UP_LEFT (MASK_UP | MASK_LEFT)
#define MASK_UP_RIGHT (MASK_UP | MASK_RIGHT)

static int isButtonCommandActive(DreamMugenCommandInputStepTarget tTarget, uint32_t tMask, int isFacingRight) {
	int directionMask = 0;
	directionMask |= (tMask & (MASK_UP | MASK_DOWN | MASK_LEFT | MASK_RIGHT));

	if (tTarget == MUGEN_COMMAND_INPUT_STEP_TARGET_A) return (tMask & MASK_A) == MASK_A;
	else if (tTarget == MUGEN_COMMAND_INPUT_STEP_TARGET_B) return (tMask & MASK_B) == MASK_B;
	else if (tTarget == MUGEN_COMMAND_INPUT_STEP_TARGET_C) return (tMask & MASK_C) == MASK_C;
	else if (tTarget == MUGEN_COMMAND_INPUT_STEP_TARGET_X) return (tMask & MASK_X) == MASK_X;
	else if (tTarget == MUGEN_COMMAND_INPUT_STEP_TARGET_Y) return (tMask & MASK_Y) == MASK_Y;
	else if (tTarget == MUGEN_COMMAND_INPUT_STEP_TARGET_Z) return (tMask & MASK_Z) == MASK_Z;

	else if (tTarget == MUGEN_COMMAND_INPUT_STEP_TARGET_START) return (tMask & MASK_START) == MASK_START;

	else if (tTarget == MUGEN_COMMAND_INPUT_STEP_TARGET_UP) return directionMask == MASK_UP;
	else if (tTarget == MUGEN_COMMAND_INPUT_STEP_TARGET_DOWN) return directionMask == MASK_DOWN;
	else if (tTarget == MUGEN_COMMAND_INPUT_STEP_TARGET_FORWARD) {
		if (isFacingRight) return  directionMask == MASK_RIGHT;
		else return directionMask == MASK_LEFT;
	}
	else if (tTarget == MUGEN_COMMAND_INPUT_STEP_TARGET_BACKWARD) {
		if (isFacingRight) return  directionMask == MASK_LEFT;
		else return directionMask == MASK_RIGHT;
	}

	else if (tTarget == MUGEN_COMMAND_INPUT_STEP_TARGET_DOWN_FORWARD) {
		if (isFacingRight) return  directionMask == MASK_DOWN_RIGHT;
		else return directionMask == MASK_DOWN_LEFT;
	}
	else if (tTarget == MUGEN_COMMAND_INPUT_STEP_TARGET_DOWN_BACKWARD) {
		if (isFacingRight) return directionMask == MASK_DOWN_LEFT;
		else return directionMask == MASK_DOWN_RIGHT;
	}
	else if (tTarget == MUGEN_COMMAND_INPUT_STEP_TARGET_UP_FORWARD) {
		if (isFacingRight) return  directionMask == MASK_UP_RIGHT;
		else return directionMask == MASK_UP_LEFT;
	}
	else if (tTarget == MUGEN_COMMAND_INPUT_STEP_TARGET_UP_BACKWARD) {
		if (isFacingRight) return  directionMask == MASK_UP_LEFT;
		else return directionMask == MASK_UP_RIGHT;
	}

	else if (tTarget == MUGEN_COMMAND_INPUT_STEP_TARGET_MULTI_FORWARD) {
		return isButtonCommandActive(MUGEN_COMMAND_INPUT_STEP_TARGET_FORWARD, tMask, isFacingRight)
			|| isButtonCommandActive(MUGEN_COMMAND_INPUT_STEP_TARGET_UP_FORWARD, tMask, isFacingRight)
			|| isButtonCommandActive(MUGEN_COMMAND_INPUT_STEP_TARGET_DOWN_FORWARD, tMask, isFacingRight);
	}
	else if (tTarget == MUGEN_COMMAND_INPUT_STEP_TARGET_MULTI_BACKWARD) {
		return isButtonCommandActive(MUGEN_COMMAND_INPUT_STEP_TARGET_BACKWARD, tMask, isFacingRight)
			|| isButtonCommandActive(MUGEN_COMMAND_INPUT_STEP_TARGET_UP_BACKWARD, tMask, isFacingRight)
			|| isButtonCommandActive(MUGEN_COMMAND_INPUT_STEP_TARGET_DOWN_BACKWARD, tMask, isFacingRight);
	}
	else if (tTarget == MUGEN_COMMAND_INPUT_STEP_TARGET_MULTI_UP) {
		return isButtonCommandActive(MUGEN_COMMAND_INPUT_STEP_TARGET_UP, tMask, isFacingRight)
			|| isButtonCommandActive(MUGEN_COMMAND_INPUT_STEP_TARGET_UP_FORWARD, tMask, isFacingRight)
			|| isButtonCommandActive(MUGEN_COMMAND_INPUT_STEP_TARGET_UP_BACKWARD, tMask, isFacingRight);
	}
	else if (tTarget == MUGEN_COMMAND_INPUT_STEP_TARGET_MULTI_DOWN) {
		return isButtonCommandActive(MUGEN_COMMAND_INPUT_STEP_TARGET_DOWN, tMask, isFacingRight)
			|| isButtonCommandActive(MUGEN_COMMAND_INPUT_STEP_TARGET_DOWN_FORWARD, tMask, isFacingRight)
			|| isButtonCommandActive(MUGEN_COMMAND_INPUT_STEP_TARGET_DOWN_BACKWARD, tMask, isFacingRight);
	}
	else {
		logWarningFormat("Unrecognized input target %d. Default to false.", tTarget);
		return 0;
	}
}

static int isTargetHeld(DreamMugenCommandInputStepTarget tTarget, DreamPlayer* tPlayer, int tIsFacingRight) {
	return isButtonCommandActive(tTarget, gData.mHeldMask[tPlayer->mControllerID], tIsFacingRight);
}

static int isTargetPressed(DreamMugenCommandInputStepTarget tTarget, DreamPlayer* tPlayer, int tIsFacingRight) {
	return isButtonCommandActive(tTarget, gData.mHeldMask[tPlayer->mControllerID], tIsFacingRight) && !isButtonCommandActive(tTarget, gData.mPreviousHeldMask[tPlayer->mControllerID], tIsFacingRight);
}

static int isTargetReleased(DreamMugenCommandInputStepTarget tTarget, DreamPlayer* tPlayer, int tIsFacingRight) {
	return !isButtonCommandActive(tTarget, gData.mHeldMask[tPlayer->mControllerID], tIsFacingRight) && isButtonCommandActive(tTarget, gData.mPreviousHeldMask[tPlayer->mControllerID], tIsFacingRight);
}

static int handleHoldingCommandInputStep(DreamMugenCommandInputStep* tStep, int* oIsStepOver, DreamPlayer* tPlayer, int tIsFacingRight) {
	int ret = isTargetHeld(tStep->mTarget, tPlayer, tIsFacingRight);
	*oIsStepOver = 1; // TODO add to active steps for command and always check

	return ret;
}

static int handlePressingCommandInputStep(DreamMugenCommandInputStep* tStep, int* oIsStepOver, DreamPlayer* tPlayer, int tIsFacingRight) {
	int ret = isTargetPressed(tStep->mTarget, tPlayer, tIsFacingRight);
	*oIsStepOver = 1;

	return ret;
}

static int handleReleasingCommandInputStep(DreamMugenCommandInputStep* tStep, int* oIsStepOver, DreamPlayer* tPlayer, int tIsFacingRight) {
	int ret = isTargetReleased(tStep->mTarget, tPlayer, tIsFacingRight);
	*oIsStepOver = 1; 

	return ret;
}



static int handleSingleCommandInputStepAndReturnIfActive(DreamMugenCommandInputStep* tStep, int* oIsStepOver, DreamPlayer* tPlayer, int tIsFacingRight) {

	if (tStep->mType == MUGEN_COMMAND_INPUT_STEP_TYPE_MULTIPLE) {
		return handleMultipleCommandInputStep(tStep, oIsStepOver, tPlayer, tIsFacingRight);
	}
	else if (tStep->mType == MUGEN_COMMAND_INPUT_STEP_TYPE_HOLDING) {
		return handleHoldingCommandInputStep(tStep, oIsStepOver, tPlayer, tIsFacingRight);
	}
	else if (tStep->mType == MUGEN_COMMAND_INPUT_STEP_TYPE_PRESS) {
		return handlePressingCommandInputStep(tStep, oIsStepOver, tPlayer, tIsFacingRight);
	}
	else if (tStep->mType == MUGEN_COMMAND_INPUT_STEP_TYPE_RELEASE) {
		return handleReleasingCommandInputStep(tStep, oIsStepOver, tPlayer, tIsFacingRight);
	}
	else {
		logWarningFormat("Unrecognized input type %d. Default to inactive.", tStep->mType);
		return 0;
	}
}

static void removeActiveCommand(ActiveMugenCommand* tCommand, RegisteredMugenCommand* tRegisteredCommand) {
	InternalMugenCommandState* state = string_map_get(&tRegisteredCommand->mInternalStates, tCommand->mName);
	state->mIsBeingProcessed = 0;
}

static void setCommandStateActive(RegisteredMugenCommand* tRegisteredCommand, char* tName, Duration tBufferTime) {
	MugenCommandState* state = string_map_get(&tRegisteredCommand->tStates->mStates, tName);
	state->mIsActive = 1;
	state->mNow = 0;
	state->mBufferTime = tBufferTime;
}

static void setCommandStateInactive(MugenCommandState* tState) {
	tState->mIsActive = 0;
}

static int isSameStepAsBefore(ActiveMugenCommand* tCommand) {
	assert(tCommand->mStep > 0);
	assert(tCommand->mStep < vector_size(&tCommand->mInput->mInputSteps));

	DreamMugenCommandInputStep* mPreviousStep = vector_get(&tCommand->mInput->mInputSteps, tCommand->mStep - 1);
	DreamMugenCommandInputStep* mStep = vector_get(&tCommand->mInput->mInputSteps, tCommand->mStep);

	int isSameType = mPreviousStep->mType == mStep->mType;
	if (!isSameType) return 0;

	int isPressType = mPreviousStep->mType == MUGEN_COMMAND_INPUT_STEP_TYPE_PRESS;
	if (!isPressType) return 0;

	int haveSameTarget = mPreviousStep->mTarget == mStep->mTarget;
	return haveSameTarget;
}

static int updateSingleActiveMugenCommand(void* tCaller, void* tData) {
	RegisteredMugenCommand* registeredCommand = tCaller;
	ActiveMugenCommand* command = tData;

	if (handleDurationAndCheckIfOver(&command->mNow, command->mInput->mTime)) {
		removeActiveCommand(command, registeredCommand);
		return 1;
	}

	int isRunning = 1;
	while (isRunning) {
		DreamMugenCommandInputStep* step = vector_get(&command->mInput->mInputSteps, command->mStep);

		int isStepOver = 0;
		int isActive = handleSingleCommandInputStepAndReturnIfActive(step, &isStepOver, registeredCommand->mPlayer, registeredCommand->mIsFacingRight);
		if (!isActive) return 0;

		if (isStepOver) {
			command->mStep++;
			if (command->mStep == vector_size(&command->mInput->mInputSteps)) {
				setCommandStateActive(registeredCommand, command->mName, command->mInput->mBufferTime);
				removeActiveCommand(command, registeredCommand);
				return 1;
			}

			if (isSameStepAsBefore(command)) break;
		}
	}

	return 0;
}

static void updateActiveMugenCommands(RegisteredMugenCommand* tCommand) {
	list_remove_predicate(&tCommand->mActiveCommands, updateSingleActiveMugenCommand, tCommand);
}

typedef struct {
	RegisteredMugenCommand* mRegisteredCommand;
	char* mName;
} StaticMugenCommandInputCaller;

static void addNewActiveMugenCommand(DreamMugenCommandInput* tInput, RegisteredMugenCommand* tRegisteredCommand, char* tName, int mIsStepOver) {
	
	if (vector_size(&tInput->mInputSteps) == 1) {
		setCommandStateActive(tRegisteredCommand, tName, tInput->mBufferTime);
		return;
	}

	/*
	if (!strcmp("holdfwd", tName)) return;
	if (!strcmp("holdback", tName)) return;
	if (!strcmp("holdup", tName)) return;
	if (!strcmp("holddown", tName)) return;
	if (!strcmp("a", tName)) return;
	if (!strcmp("b", tName)) return;
	if (!strcmp("c", tName)) return;
	if (!strcmp("x", tName)) return;
	if (!strcmp("y", tName)) return;
	if (!strcmp("z", tName)) return;
	if (!strcmp("FF", tName)) return;
	if (!strcmp("BB", tName)) return;
	if (strcmp("QCF_x", tName)) return;
	printf("%d check %s\n", tRegisteredCommand->mPlayer->mRootID, tName);
	*/

	ActiveMugenCommand* e = allocMemory(sizeof(ActiveMugenCommand));
	e->mInput = tInput;
	strcpy(e->mName, tName);
	e->mNow = 0;
	e->mStep = min(1, mIsStepOver);

	int isAlreadyOver = 0;
	if (!isSameStepAsBefore(e)) {
		isAlreadyOver = updateSingleActiveMugenCommand(tRegisteredCommand, e);
	}
	if(!isAlreadyOver) list_push_back_owned(&tRegisteredCommand->mActiveCommands, e);
}

static void updateSingleStaticMugenCommandInput(void* tCaller, void* tData) {
	DreamMugenCommandInput* input = tData;
	StaticMugenCommandInputCaller* caller = tCaller;

	DreamMugenCommandInputStep* firstStep = vector_get(&input->mInputSteps, 0);

	int mIsStepOver = 0;
	int mIsActive = handleSingleCommandInputStepAndReturnIfActive(firstStep, &mIsStepOver, caller->mRegisteredCommand->mPlayer, caller->mRegisteredCommand->mIsFacingRight);

	if (!mIsActive) return;

	addNewActiveMugenCommand(input, caller->mRegisteredCommand, caller->mName, mIsStepOver);
}

static void updateSingleStaticMugenCommand(void* tCaller, char* tKey, void* tData) {
	RegisteredMugenCommand* registeredCommand = tCaller;
	DreamMugenCommand* command = tData;
	
	InternalMugenCommandState* internalState = string_map_get(&registeredCommand->mInternalStates, tKey);
	if (internalState->mIsBeingProcessed) return;

	StaticMugenCommandInputCaller caller;
	caller.mRegisteredCommand = registeredCommand;
	caller.mName = tKey;
	vector_map(&command->mInputs, updateSingleStaticMugenCommandInput, &caller);
}

static void updateStaticMugenCommands(RegisteredMugenCommand* tCommand) {
	string_map_map(&tCommand->tCommands->mCommands, updateSingleStaticMugenCommand, tCommand);
}

static void updateSingleCommandState(void* tCaller, char* tKey, void* tData) {
	(void)tCaller;
	(void)tKey;
	MugenCommandState* state = tData;
	if (!state->mIsActive) return;

	if (handleDurationAndCheckIfOver(&state->mNow, state->mBufferTime)) {
		setCommandStateInactive(state);
	}
}

static void updateCommandStates(RegisteredMugenCommand* tCommand) {
	string_map_map(&tCommand->tStates->mStates, updateSingleCommandState, NULL);
}

static void updateSingleInputMaskEntry(int i, uint32_t tMask, int tHoldValue) {
	gData.mHeldMask[i] |= (tMask * min(tHoldValue, 1));
}

static void updateInputMask(int i) {
	gData.mPreviousHeldMask[i] = gData.mHeldMask[i];
	gData.mHeldMask[i] = 0;

	updateSingleInputMaskEntry(i, MASK_A, hasPressedASingle(i));
	updateSingleInputMaskEntry(i, MASK_B, hasPressedBSingle(i));
	updateSingleInputMaskEntry(i, MASK_C, hasPressedRSingle(i));
	updateSingleInputMaskEntry(i, MASK_X, hasPressedXSingle(i));
	updateSingleInputMaskEntry(i, MASK_Y, hasPressedYSingle(i));
	updateSingleInputMaskEntry(i, MASK_Z, hasPressedLSingle(i));

	updateSingleInputMaskEntry(i, MASK_START, hasPressedStartSingle(i));

	updateSingleInputMaskEntry(i, MASK_LEFT, hasPressedLeftSingle(i));
	updateSingleInputMaskEntry(i, MASK_RIGHT, hasPressedRightSingle(i));
	updateSingleInputMaskEntry(i, MASK_UP, hasPressedUpSingle(i));
	updateSingleInputMaskEntry(i, MASK_DOWN, hasPressedDownSingle(i));

	
}

static void updateInputMasks() {
	int i;
	for (i = 0; i < 2; i++) {
		updateInputMask(i);
	}
}

static void updateSingleRegisteredCommand(void* tCaller, void* tData) {
	(void)tCaller;
	RegisteredMugenCommand* command = tData;

	updateCommandStates(command);
	updateActiveMugenCommands(command);
	updateStaticMugenCommands(command);
}

static void updateMugenCommandHandler(void* tData) {
	(void)tData;
	updateInputMasks();

	vector_map(&gData.mRegisteredCommands, updateSingleRegisteredCommand, NULL);
}

ActorBlueprint DreamMugenCommandHandler = {
	.mLoad = loadMugenCommandHandler,
	.mUpdate = updateMugenCommandHandler,
};
