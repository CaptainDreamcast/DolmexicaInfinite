#include "mugencommandhandler.h"

#include <assert.h>
#include <algorithm>
#include <vector>

#include <prism/profiling.h>
#include <prism/datastructures.h>
#include <prism/log.h>
#include <prism/system.h>
#include <prism/input.h>
#include <prism/math.h>
#include <prism/stlutil.h>

#include "gamelogic.h"
#include "fightnetplay.h"

using namespace std;

typedef struct {
	string mName;
	int mIsActive;
	int mNow;
	int mBufferTime;
	int mLookupID;
} MugenCommandState;

typedef struct {
	unordered_map<string, MugenCommandState> mStates;
	vector<MugenCommandState*> mStateLookup;
} MugenCommandStates;

typedef struct {
	string mName;
	DreamMugenCommandInput* mInput;
	int mStep;
	int mStepNow;
	int mNow;
} ActiveMugenCommand;

typedef struct {
	int mIsBeingProcessed;

} InternalMugenCommandState;

typedef struct {
	DreamMugenCommands* tCommands;
	MugenCommandStates tStates;
	map<string, InternalMugenCommandState> mInternalStates;

	list<ActiveMugenCommand> mActiveCommands;

	int mControllerID;
	int mIsFacingRight;
} RegisteredMugenCommand;

static struct {
	vector<RegisteredMugenCommand> mRegisteredCommands;
	int mRegisteredCommandAmount;

	uint32_t mHeldMask[2];
	uint32_t mPreviousHeldMask[2];
	uint32_t mOverrideMask[2];

	int mOsuInputAllowedFlag[2];
	int mControllerUsed[2];
} gMugenCommandHandler;

#define MAXIMUM_REGISTERED_COMMAND_AMOUNT 2

static void loadMugenCommandHandler(void* tData) {
	(void)tData;
	setProfilingSectionMarkerCurrentFunction();

	gMugenCommandHandler.mRegisteredCommands = vector<RegisteredMugenCommand>(MAXIMUM_REGISTERED_COMMAND_AMOUNT);
	gMugenCommandHandler.mRegisteredCommandAmount = 0;

	if (getGameMode() == GAME_MODE_OSU) {
		int i;
		for (i = 0; i < 2; i++) {
			gMugenCommandHandler.mOsuInputAllowedFlag[i] = 0;
			gMugenCommandHandler.mControllerUsed[i] = i;
		}
	}
}

static void unloadSingleRegisteredCommand(void* tCaller, RegisteredMugenCommand& tData) {
	(void)tCaller;
	RegisteredMugenCommand* e = &tData;
	e->mActiveCommands.clear();
	e->tStates.mStates.clear();
	e->tStates.mStateLookup.clear();
	e->mInternalStates.clear();
}

static void unloadMugenCommandHandler(void* tData) {
	(void)tData;
	setProfilingSectionMarkerCurrentFunction();

	stl_vector_map(gMugenCommandHandler.mRegisteredCommands, unloadSingleRegisteredCommand);
	stl_delete_vector(gMugenCommandHandler.mRegisteredCommands);
}

static void addSingleMugenCommandState(RegisteredMugenCommand* tCaller, const string &tKey, DreamMugenCommand& tData) {
	(void)tData;
	RegisteredMugenCommand* s = (RegisteredMugenCommand*)tCaller;

	MugenCommandState e;
	e.mName = tKey;
	e.mIsActive = 0;
	e.mLookupID = int(s->tStates.mStateLookup.size());
	s->tStates.mStates[tKey] = e;
	s->tStates.mStateLookup.push_back(&s->tStates.mStates[tKey]);

	InternalMugenCommandState internalState;
	internalState.mIsBeingProcessed = 0;
	
	s->mInternalStates[tKey] = internalState;
}

static void setupMugenCommandStates(RegisteredMugenCommand* e) {
	e->tStates.mStates.clear();
	e->tStates.mStateLookup.clear();
	stl_string_map_map(e->tCommands->mCommands, addSingleMugenCommandState, e);
}

static int getNewRegisteredCommandIndex() {
	int ret = gMugenCommandHandler.mRegisteredCommandAmount;
	gMugenCommandHandler.mRegisteredCommandAmount++;
	return ret;
}

int registerDreamMugenCommands(int tControllerID, DreamMugenCommands * tCommands)
{
	
	RegisteredMugenCommand e;
	e.mActiveCommands.clear();
	e.tCommands = tCommands;
	e.mInternalStates.clear();
	e.mControllerID = tControllerID;
	e.mIsFacingRight = 1;

	int returnIndex = getNewRegisteredCommandIndex();
	gMugenCommandHandler.mRegisteredCommands[returnIndex] = e;
	setupMugenCommandStates(&gMugenCommandHandler.mRegisteredCommands[returnIndex]);

	return returnIndex;
}

int isDreamCommandActive(int tID, const char * tCommandName)
{
	RegisteredMugenCommand* e = &gMugenCommandHandler.mRegisteredCommands[tID];
	string key(tCommandName);
	if (!stl_map_contains(e->tStates.mStates, key)) {
		logWarningFormat("Querying nonexistant command name %s.", tCommandName);
		return 0;
	}
	MugenCommandState* state = &e->tStates.mStates[key];
	
	return state->mIsActive;
}

int isDreamCommandActiveByLookupIndex(int tID, int tLookupIndex)
{
	RegisteredMugenCommand* e = &gMugenCommandHandler.mRegisteredCommands[tID];
	if (tLookupIndex < 0 || tLookupIndex >= (int)e->tStates.mStateLookup.size()) {
		logWarningFormat("Querying nonexistant command lookup %d.", tLookupIndex);
		return 0;
	}
	MugenCommandState* state = e->tStates.mStateLookup[tLookupIndex];

	return state->mIsActive;
}

static void setCommandStateActive(RegisteredMugenCommand* tRegisteredCommand, const string& tName, int tBufferTime);

int isDreamCommandForLookup(int tID, const char * tCommandName, int * oLookupIndex)
{
	RegisteredMugenCommand* e = &gMugenCommandHandler.mRegisteredCommands[tID];
	string key(tCommandName);
	if (!stl_map_contains(e->tStates.mStates, key)) {
		return 0;
	}
	MugenCommandState* state = &e->tStates.mStates[key];
	*oLookupIndex = state->mLookupID;
	return 1;
}

int getDreamCommandMinimumDuration(int tID, const char * tCommandName)
{
	RegisteredMugenCommand* e = &gMugenCommandHandler.mRegisteredCommands[tID];
	string key(tCommandName);
	if (!stl_map_contains(e->tCommands->mCommands, key)) {
		logWarningFormat("Querying nonexistant command name %s.", tCommandName);
		return INF;
	}	
	return e->tCommands->mCommands[key].mMinimumDuration;
}

void setDreamPlayerCommandActiveForAI(int tID, const char * tCommandName, int tBufferTime)
{
	RegisteredMugenCommand* e = &gMugenCommandHandler.mRegisteredCommands[tID];
	setCommandStateActive(e, tCommandName, tBufferTime);
}

int setDreamPlayerCommandNumberActiveForDebug(int tID, int tCommandNumber)
{
	RegisteredMugenCommand* e = &gMugenCommandHandler.mRegisteredCommands[tID];
	if (tCommandNumber >= (int)e->tCommands->mCommands.size()) return 0;
	auto command = stl_map_get_pair_by_index(e->tCommands->mCommands, tCommandNumber);

	setDreamPlayerCommandActiveForAI(tID, command->first.data(), 2);
	return 1;
}

int getDreamPlayerCommandAmount(int tID)
{
	RegisteredMugenCommand* e = &gMugenCommandHandler.mRegisteredCommands[tID];
	return int(e->tCommands->mCommands.size());
}

void setDreamMugenCommandFaceDirection(int tID, FaceDirection tDirection)
{
	RegisteredMugenCommand* e = &gMugenCommandHandler.mRegisteredCommands[tID];
	e->mIsFacingRight = tDirection == FACE_DIRECTION_RIGHT;
}

void allowOsuPlayerCommandInputOneFrame(int tRootIndex)
{
	gMugenCommandHandler.mOsuInputAllowedFlag[tRootIndex] = 1;
}

void resetOsuPlayerCommandInputAllowed(int tRootIndex)
{
	gMugenCommandHandler.mOsuInputAllowedFlag[tRootIndex] = 0;
}

int isOsuPlayerCommandInputAllowed(int tRootIndex)
{
	return gMugenCommandHandler.mOsuInputAllowedFlag[tRootIndex];
}

static int handleSingleCommandInputStepAndReturnIfActive(DreamMugenCommandInputStep* tStep, int* oIsStepOver, int* oIsStepRuined, int tControllerID, int tIsFacingRight, int tStepNow);

typedef struct {
	int mIsActiveAmount;
	int mIsStepOverAmount;
	int mControllerID;
	int mIsFacingRight;
	int mStepNow;
	int* mIsStepRuined;
} MultipleCommandInputStepCaller;

static void handleSingleMultipleCommandInputStep(void* tCaller, void* tData) {
	MultipleCommandInputStepCaller* caller = (MultipleCommandInputStepCaller*)tCaller;
	DreamMugenCommandInputStep* step = (DreamMugenCommandInputStep*)tData;

	int isStepOver = 0;
	int isActive = handleSingleCommandInputStepAndReturnIfActive(step, &isStepOver, caller->mIsStepRuined, caller->mControllerID, caller->mIsFacingRight, caller->mStepNow);

	caller->mIsActiveAmount += isActive;
	caller->mIsStepOverAmount += isStepOver;
}

static int handleMultipleCommandInputStep(DreamMugenCommandInputStep* tStep, int* oIsStepOver, int* oIsStepRuined, int tControllerID, int tIsFacingRight, int tStepNow) {
	DreamMugenCommandInputStepMultipleTargetData* data = (DreamMugenCommandInputStepMultipleTargetData*)tStep->mData;
	assert(tStep->mTarget == MUGEN_COMMAND_INPUT_STEP_TARGET_MULTIPLE);

	MultipleCommandInputStepCaller caller;
	caller.mIsActiveAmount = 0;
	caller.mIsStepOverAmount = 0;
	caller.mControllerID = tControllerID;
	caller.mIsFacingRight = tIsFacingRight;
	caller.mStepNow = tStepNow;
	caller.mIsStepRuined = oIsStepRuined;
	vector_map(&data->mSubSteps, handleSingleMultipleCommandInputStep, &caller);

	if (caller.mIsActiveAmount < vector_size(&data->mSubSteps)) return 0;

	assert(caller.mIsStepOverAmount == 0 || caller.mIsStepOverAmount == vector_size(&data->mSubSteps));

	if (caller.mIsStepOverAmount) *oIsStepOver = 1;
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
		return 0;
	}
}

static int isTargetHeld(DreamMugenCommandInputStepTarget tTarget, int tControllerID, int tIsFacingRight) {
	return isButtonCommandActive(tTarget, gMugenCommandHandler.mHeldMask[tControllerID], tIsFacingRight);
}

static int isTargetPressed(DreamMugenCommandInputStepTarget tTarget, int tControllerID, int tIsFacingRight) {
	return isButtonCommandActive(tTarget, gMugenCommandHandler.mHeldMask[tControllerID], tIsFacingRight) && !isButtonCommandActive(tTarget, gMugenCommandHandler.mPreviousHeldMask[tControllerID], tIsFacingRight);
}

static int isTargetReleased(DreamMugenCommandInputStepTarget tTarget, int tControllerID, int tIsFacingRight) {
	return !isButtonCommandActive(tTarget, gMugenCommandHandler.mHeldMask[tControllerID], tIsFacingRight) && isButtonCommandActive(tTarget, gMugenCommandHandler.mPreviousHeldMask[tControllerID], tIsFacingRight);
}

static int handleHoldingCommandInputStep(DreamMugenCommandInputStep* tStep, int* oIsStepOver, int tControllerID, int tIsFacingRight) {
	int ret = isTargetHeld(tStep->mTarget, tControllerID, tIsFacingRight);
	*oIsStepOver = 1;

	return ret;
}

static int handlePressingCommandInputStep(DreamMugenCommandInputStep* tStep, int* oIsStepOver, int tControllerID, int tIsFacingRight) {
	int ret = isTargetPressed(tStep->mTarget, tControllerID, tIsFacingRight);
	*oIsStepOver = 1;

	return ret;
}

static int handleReleasingCommandInputStep(DreamMugenCommandInputStep* tStep, int* oIsStepOver, int* oIsStepRuined, int tControllerID, int tIsFacingRight, int tStepNow) {
	int ret = isTargetReleased(tStep->mTarget, tControllerID, tIsFacingRight);
	*oIsStepOver = 1; 

	const auto data = (DreamMugenCommandInputStepReleaseData*)tStep->mData;
	if (ret && tStepNow < data->mDuration) {
		*oIsStepRuined = 1;
		ret = 0;
	}

	return ret;
}



static int handleSingleCommandInputStepAndReturnIfActive(DreamMugenCommandInputStep* tStep, int* oIsStepOver, int* oIsStepRuined, int tControllerID, int tIsFacingRight, int tStepNow) {

	if (tStep->mType == MUGEN_COMMAND_INPUT_STEP_TYPE_MULTIPLE) {
		return handleMultipleCommandInputStep(tStep, oIsStepOver, oIsStepRuined, tControllerID, tIsFacingRight, tStepNow);
	}
	else if (tStep->mType == MUGEN_COMMAND_INPUT_STEP_TYPE_HOLDING) {
		return handleHoldingCommandInputStep(tStep, oIsStepOver, tControllerID, tIsFacingRight);
	}
	else if (tStep->mType == MUGEN_COMMAND_INPUT_STEP_TYPE_PRESS) {
		return handlePressingCommandInputStep(tStep, oIsStepOver, tControllerID, tIsFacingRight);
	}
	else if (tStep->mType == MUGEN_COMMAND_INPUT_STEP_TYPE_RELEASE) {
		return handleReleasingCommandInputStep(tStep, oIsStepOver, oIsStepRuined, tControllerID, tIsFacingRight, tStepNow);
	}
	else {
		return 0;
	}
}

static void removeActiveCommand(ActiveMugenCommand* tCommand, RegisteredMugenCommand* tRegisteredCommand) {
	InternalMugenCommandState* state = &tRegisteredCommand->mInternalStates[tCommand->mName];
	state->mIsBeingProcessed = 0;
}

static void setCommandStateActive(RegisteredMugenCommand* tRegisteredCommand, const string& tName, int tBufferTime) {
	MugenCommandState* state = &tRegisteredCommand->tStates.mStates[tName];
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

	DreamMugenCommandInputStep* mPreviousStep = (DreamMugenCommandInputStep*)vector_get(&tCommand->mInput->mInputSteps, tCommand->mStep - 1);
	DreamMugenCommandInputStep* mStep = (DreamMugenCommandInputStep*)vector_get(&tCommand->mInput->mInputSteps, tCommand->mStep);

	int isSameType = mPreviousStep->mType == mStep->mType;
	if (!isSameType) return 0;

	int isPressType = mPreviousStep->mType == MUGEN_COMMAND_INPUT_STEP_TYPE_PRESS;
	if (!isPressType) return 0;

	int haveSameTarget = mPreviousStep->mTarget == mStep->mTarget;
	return haveSameTarget;
}

static int isPreviousCommandInputStepNoHoldingInapplicableOrActive(RegisteredMugenCommand* tRegisteredCommand, ActiveMugenCommand* tActiveCommand) {
	if (tActiveCommand->mStep == 0) return 1;
	const auto previousStep = (DreamMugenCommandInputStep*)vector_get(&tActiveCommand->mInput->mInputSteps, tActiveCommand->mStep - 1);
	if (previousStep->mType != MUGEN_COMMAND_INPUT_STEP_TYPE_HOLDING) return 1;
	const auto step = (DreamMugenCommandInputStep*)vector_get(&tActiveCommand->mInput->mInputSteps, tActiveCommand->mStep);
	if (step->mType == MUGEN_COMMAND_INPUT_STEP_TYPE_RELEASE && step->mTarget == previousStep->mTarget) return 1;

	int isStepOver;
	int isStepRuined;
	const auto ret = handleSingleCommandInputStepAndReturnIfActive(previousStep, &isStepOver, &isStepRuined, tRegisteredCommand->mControllerID, tRegisteredCommand->mIsFacingRight, INF);
	return ret;
}

static int isFiniteCommandTimeStep(DreamMugenCommandInputStep* tStep) {
	if (tStep->mType != MUGEN_COMMAND_INPUT_STEP_TYPE_RELEASE) return 1;

	const auto data = (DreamMugenCommandInputStepReleaseData*)tStep->mData;
	return data->mDuration == 0;
}

static int updateSingleActiveMugenCommand(RegisteredMugenCommand* tCaller, ActiveMugenCommand& tData) {
	RegisteredMugenCommand* registeredCommand = (RegisteredMugenCommand*)tCaller;
	ActiveMugenCommand* command = &tData;

	DreamMugenCommandInputStep* step = (DreamMugenCommandInputStep*)vector_get(&command->mInput->mInputSteps, command->mStep);
	if (isFiniteCommandTimeStep(step)) {
		command->mNow++;
		if (command->mNow >= command->mInput->mTime) {
			removeActiveCommand(command, registeredCommand);
			return 1;
		}
	}
	command->mStepNow++;

	int isRunning = 1;
	while (isRunning) {
		step = (DreamMugenCommandInputStep*)vector_get(&command->mInput->mInputSteps, command->mStep);

		int isStepOver = 0;
		int isStepRuined = 0;
		int isActive = handleSingleCommandInputStepAndReturnIfActive(step, &isStepOver, &isStepRuined, registeredCommand->mControllerID, registeredCommand->mIsFacingRight, command->mStepNow);

		if (isStepRuined) {
			removeActiveCommand(command, registeredCommand);
			return 1;
		}

		isActive = isActive && isPreviousCommandInputStepNoHoldingInapplicableOrActive(registeredCommand, command);
		if (!isActive) return 0;

		if (isStepOver) {
			command->mStep++;
			command->mStepNow = 0;
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
	stl_list_remove_predicate(tCommand->mActiveCommands, updateSingleActiveMugenCommand, tCommand);
}

typedef struct {
	RegisteredMugenCommand* mRegisteredCommand;
	const string& mName;
} StaticMugenCommandInputCaller;

static void addNewActiveMugenCommand(DreamMugenCommandInput* tInput, RegisteredMugenCommand* tRegisteredCommand, const string& tName, int mIsStepOver) {
	
	if (vector_size(&tInput->mInputSteps) == 1) {
		setCommandStateActive(tRegisteredCommand, tName, tInput->mBufferTime);
		return;
	}

	ActiveMugenCommand e;
	e.mInput = tInput;
	e.mName = tName;
	e.mNow = 0;
	e.mStep = min(1, mIsStepOver);
	e.mStepNow = 0;

	int isAlreadyOver = 0;
	if (!isSameStepAsBefore(&e)) {
		isAlreadyOver = updateSingleActiveMugenCommand(tRegisteredCommand, e);
	}
	if(!isAlreadyOver) tRegisteredCommand->mActiveCommands.push_back(e);
}

static void updateSingleStaticMugenCommandInput(void* tCaller, void* tData) {
	DreamMugenCommandInput* input = (DreamMugenCommandInput*)tData;
	StaticMugenCommandInputCaller* caller = (StaticMugenCommandInputCaller*)tCaller;

	DreamMugenCommandInputStep* firstStep = (DreamMugenCommandInputStep*)vector_get(&input->mInputSteps, 0);

	int mIsStepOver = 0;
	int mIsStepRuined = 0;
	int mIsActive = handleSingleCommandInputStepAndReturnIfActive(firstStep, &mIsStepOver, &mIsStepRuined, caller->mRegisteredCommand->mControllerID, caller->mRegisteredCommand->mIsFacingRight, 0);

	if (!mIsActive) return;

	addNewActiveMugenCommand(input, caller->mRegisteredCommand, caller->mName, mIsStepOver);
}

static void updateSingleStaticMugenCommand(RegisteredMugenCommand* tCaller, const string& tKey, DreamMugenCommand& tData) {
	RegisteredMugenCommand* registeredCommand = (RegisteredMugenCommand*)tCaller;
	DreamMugenCommand* command = &tData;
	
	InternalMugenCommandState* internalState = &registeredCommand->mInternalStates[tKey];
	if (internalState->mIsBeingProcessed) return;

	StaticMugenCommandInputCaller caller = {
		registeredCommand,
		tKey
	};
	vector_map(&command->mInputs, updateSingleStaticMugenCommandInput, &caller);
}

static void updateStaticMugenCommands(RegisteredMugenCommand* tCommand) {
	stl_string_map_map(tCommand->tCommands->mCommands, updateSingleStaticMugenCommand, tCommand);
}

static void updateSingleCommandState(void* tCaller, const string& tKey, MugenCommandState& tData) {
	(void)tCaller;
	(void)tKey;
	MugenCommandState* state = &tData;
	if (!state->mIsActive) return;

	state->mNow++;
	if (state->mNow >= state->mBufferTime) {
		setCommandStateInactive(state);
	}
}

static void updateCommandStates(RegisteredMugenCommand* tCommand) {
	stl_string_map_map(tCommand->tStates.mStates, updateSingleCommandState);
}

static void updateSingleInputMaskEntry(int i, uint32_t tMask, int tHoldValue) {
	gMugenCommandHandler.mHeldMask[i] |= (tMask * min(tHoldValue, 1));
}

static void updateInputMaskGeneral(int i, int tButtonPrecondition) {
	gMugenCommandHandler.mPreviousHeldMask[i] = gMugenCommandHandler.mHeldMask[i];
	gMugenCommandHandler.mHeldMask[i] = 0;

	updateSingleInputMaskEntry(i, MASK_A, tButtonPrecondition && hasPressedASingle(gMugenCommandHandler.mControllerUsed[i]));
	updateSingleInputMaskEntry(i, MASK_B, tButtonPrecondition && hasPressedBSingle(gMugenCommandHandler.mControllerUsed[i]));
	updateSingleInputMaskEntry(i, MASK_C, tButtonPrecondition && hasPressedRSingle(gMugenCommandHandler.mControllerUsed[i]));
	updateSingleInputMaskEntry(i, MASK_X, tButtonPrecondition && hasPressedXSingle(gMugenCommandHandler.mControllerUsed[i]));
	updateSingleInputMaskEntry(i, MASK_Y, tButtonPrecondition && hasPressedYSingle(gMugenCommandHandler.mControllerUsed[i]));
	updateSingleInputMaskEntry(i, MASK_Z, tButtonPrecondition && hasPressedLSingle(gMugenCommandHandler.mControllerUsed[i]));

	updateSingleInputMaskEntry(i, MASK_START, tButtonPrecondition && hasPressedStartSingle(gMugenCommandHandler.mControllerUsed[i]));

	updateSingleInputMaskEntry(i, MASK_LEFT, hasPressedLeftSingle(gMugenCommandHandler.mControllerUsed[i]));
	updateSingleInputMaskEntry(i, MASK_RIGHT, hasPressedRightSingle(gMugenCommandHandler.mControllerUsed[i]));
	updateSingleInputMaskEntry(i, MASK_UP, hasPressedUpSingle(gMugenCommandHandler.mControllerUsed[i]));
	updateSingleInputMaskEntry(i, MASK_DOWN, hasPressedDownSingle(gMugenCommandHandler.mControllerUsed[i]));

	gMugenCommandHandler.mHeldMask[i] |= gMugenCommandHandler.mOverrideMask[i];
	gMugenCommandHandler.mOverrideMask[i] = 0;
}

static void updateInputMask(int i) {
	if (getGameMode() != GAME_MODE_OSU) {
		updateInputMaskGeneral(i, 1);
	}
	else {
		updateInputMaskGeneral(i, gMugenCommandHandler.mOsuInputAllowedFlag[i]);
	}
}

static void updateInputMasks() {
	int i;
	for (i = 0; i < 2; i++) {
		updateInputMask(i);
	}
}

static void updateSingleRegisteredCommand(RegisteredMugenCommand& tData) {
	RegisteredMugenCommand* command = &tData;

	updateCommandStates(command);
	updateActiveMugenCommands(command);
	updateStaticMugenCommands(command);
}

static void updateMugenCommandHandler(void* tData) {
	(void)tData;
	setProfilingSectionMarkerCurrentFunction();
	updateInputMasks();

	for (int i = 0; i < gMugenCommandHandler.mRegisteredCommandAmount; i++) {
		updateSingleRegisteredCommand(gMugenCommandHandler.mRegisteredCommands[i]);
	}
}

ActorBlueprint getDreamMugenCommandHandler() {
	return makeActorBlueprint(loadMugenCommandHandler, unloadMugenCommandHandler, updateMugenCommandHandler);
}
void setDreamButtonAActiveForPlayer(int tControllerIndex)
{
	gMugenCommandHandler.mOverrideMask[tControllerIndex] |= MASK_A;
}
void setDreamButtonBActiveForPlayer(int tControllerIndex)
{
	gMugenCommandHandler.mOverrideMask[tControllerIndex] |= MASK_B;
}
void setDreamButtonCActiveForPlayer(int tControllerIndex)
{
	gMugenCommandHandler.mOverrideMask[tControllerIndex] |= MASK_C;
}
void setDreamButtonXActiveForPlayer(int tControllerIndex)
{
	gMugenCommandHandler.mOverrideMask[tControllerIndex] |= MASK_X;
}
void setDreamButtonYActiveForPlayer(int tControllerIndex)
{
	gMugenCommandHandler.mOverrideMask[tControllerIndex] |= MASK_Y;
}
void setDreamButtonZActiveForPlayer(int tControllerIndex)
{
	gMugenCommandHandler.mOverrideMask[tControllerIndex] |= MASK_Z;
}
void setDreamButtonStartActiveForPlayer(int tControllerIndex)
{
	gMugenCommandHandler.mOverrideMask[tControllerIndex] |= MASK_START;
}

void setDreamCommandInputControllerUsed(int i, int tControllerIndex)
{
	gMugenCommandHandler.mControllerUsed[i] = tControllerIndex;
}

void updateCommandNetplaySend(int tID) {
	assert(getGameMode() == GAME_MODE_NETPLAY);

	RegisteredMugenCommand* e = &gMugenCommandHandler.mRegisteredCommands[tID];
	Buffer b = makeBufferEmptyOwned();

	appendBufferUint32(&b, uint32_t(e->tStates.mStateLookup.size()));
	for (size_t i = 0; i < e->tStates.mStateLookup.size(); i++)
	{
		const auto state = e->tStates.mStateLookup[i];

		appendBufferUint32(&b, uint32_t(state->mName.size()));
		appendBufferString(&b, state->mName.c_str(), int(state->mName.size()));
		appendBufferUint32(&b, state->mIsActive);
		appendBufferUint32(&b, state->mNow);
		appendBufferUint32(&b, state->mBufferTime);
	}

	sendFightNetplayData(b);
	freeBuffer(b);
}

void updateCommandNetplayReceive(int tID)
{
	assert(getGameMode() == GAME_MODE_NETPLAY);
	if (!hasNewFightNetplayReceivedData()) return;

	RegisteredMugenCommand* e = &gMugenCommandHandler.mRegisteredCommands[tID];
	const auto newNetplayData = popFightNetplayReceivedData();
	assert(e->tStates.mStateLookup.size() == newNetplayData.mCommandStatus.size());

	for (size_t i = 0; i < e->tStates.mStateLookup.size(); i++) {
		auto& state = e->tStates.mStateLookup[i];
		const auto& receivedState = newNetplayData.mCommandStatus[i];

		assert(state->mName == receivedState.mName);
		state->mIsActive = receivedState.mIsActive;
		state->mNow = receivedState.mNow - 1;
		state->mBufferTime = receivedState.mBufferTime;
	}
}