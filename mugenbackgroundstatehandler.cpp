#include "mugenbackgroundstatehandler.h"

#include <prism/log.h>
#include <prism/math.h>

#include "mugenstagehandler.h"
#include "mugenassignmentevaluator.h"

using namespace std;

typedef enum {
	BACKGROUND_STATE_CONTROLLER_NULL,
	BACKGROUND_STATE_CONTROLLER_VISIBLE,
	BACKGROUND_STATE_CONTROLLER_ENABLED,
	BACKGROUND_STATE_CONTROLLER_VEL_SET,
	BACKGROUND_STATE_CONTROLLER_VEL_ADD,
	BACKGROUND_STATE_CONTROLLER_POS_SET,
	BACKGROUND_STATE_CONTROLLER_POS_ADD,
	BACKGROUND_STATE_CONTROLLER_ANIM,
	BACKGROUND_STATE_CONTROLLER_SIN_X,
	BACKGROUND_STATE_CONTROLLER_SIN_Y,
} BackgroundControllerType;

typedef struct {
	int mHasAssignmentValue;
	DreamMugenAssignment* mValue;
} BackgroundSingleValueController;

typedef struct {
	int mHasAssignmentX;
	DreamMugenAssignment* x;
	int mHasAssignmentY;
	DreamMugenAssignment* y;
} BackgroundPhysicsController;


typedef struct {
	int mHasAssignment1;
	DreamMugenAssignment* mValue1;
	int mHasAssignment2;
	DreamMugenAssignment* mValue2;
} BackgroundDummyController;

typedef struct {
	Vector mElements;

	int mTime;
	int mStartTime;
	int mEndTime;
	int mLoopTime;

	BackgroundControllerType mType;
	BackgroundDummyController mController;
} BackgroundState;

typedef struct {

	int mTime;
	int mLoopTime;

	Vector mElements;
	Vector mStates;
} BackgroundStateGroup;

typedef struct {
	Vector mBackgroundStateGroups;
} BackgroundStates;

static struct {
	BackgroundStates mStates;

} gMugenBackgroundStateHandlerData;

static void loadBackgroundStateHandler(void* /*tData*/) {
	setProfilingSectionMarkerCurrentFunction();
	gMugenBackgroundStateHandlerData.mStates.mBackgroundStateGroups = new_vector();
}

static void handleNullController() {}
static void handleVisibleController(BackgroundState* e, StaticStageHandlerElement* tElement) {
	BackgroundSingleValueController* controller = (BackgroundSingleValueController*)(&e->mController);
	if (!controller->mHasAssignmentValue) return;
	int isInvisible = evaluateDreamAssignmentAndReturnAsInteger(&controller->mValue, NULL);
	setStageElementInvisible(tElement, isInvisible);
}
static void handleEnabledController(BackgroundState* e, StaticStageHandlerElement* tElement) {
	BackgroundSingleValueController* controller = (BackgroundSingleValueController*)(&e->mController);
	if (!controller->mHasAssignmentValue) return;
	int isEnabled = evaluateDreamAssignmentAndReturnAsInteger(&controller->mValue, NULL);
	setStageElementEnabled(tElement, isEnabled);
}
static void handleVelSetController(BackgroundState* e, StaticStageHandlerElement* tElement) {
	BackgroundPhysicsController* controller = (BackgroundPhysicsController*)(&e->mController);
	if (controller->mHasAssignmentX) {
		double x = evaluateDreamAssignmentAndReturnAsFloat(&controller->x, NULL);
		setStageElementVelocityX(tElement, x);
	}
	if (controller->mHasAssignmentY) {
		double y = evaluateDreamAssignmentAndReturnAsFloat(&controller->y, NULL);
		setStageElementVelocityY(tElement, y);
	}
}
static void handleVelAddController(BackgroundState* e, StaticStageHandlerElement* tElement) {
	BackgroundPhysicsController* controller = (BackgroundPhysicsController*)(&e->mController);
	if (controller->mHasAssignmentX) {
		double x = evaluateDreamAssignmentAndReturnAsFloat(&controller->x, NULL);
		addStageElementVelocityX(tElement, x);
	}
	if (controller->mHasAssignmentY) {
		double y = evaluateDreamAssignmentAndReturnAsFloat(&controller->y, NULL);
		addStageElementVelocityY(tElement, y);
	}
}
static void handlePosSetController(BackgroundState* e, StaticStageHandlerElement* tElement) {
	BackgroundPhysicsController* controller = (BackgroundPhysicsController*)(&e->mController);
	if (controller->mHasAssignmentX) {
		double x = evaluateDreamAssignmentAndReturnAsFloat(&controller->x, NULL);
		setStageElementPositionX(tElement, x);
	}
	if (controller->mHasAssignmentY) {
		double y = evaluateDreamAssignmentAndReturnAsFloat(&controller->y, NULL);
		setStageElementPositionY(tElement, y);
	}
}
static void handlePosAddController(BackgroundState* e, StaticStageHandlerElement* tElement) {
	BackgroundPhysicsController* controller = (BackgroundPhysicsController*)(&e->mController);
	if (controller->mHasAssignmentX) {
		double x = evaluateDreamAssignmentAndReturnAsFloat(&controller->x, NULL);
		addStageElementPositionX(tElement, x);
	}
	if (controller->mHasAssignmentY) {
		double y = evaluateDreamAssignmentAndReturnAsFloat(&controller->y, NULL);
		addStageElementPositionY(tElement, y);
	}
}
static void handleAnimController(BackgroundState* e, StaticStageHandlerElement* tElement) {
	BackgroundSingleValueController* controller = (BackgroundSingleValueController*)(&e->mController);
	if (!controller->mHasAssignmentValue) return;
	int anim = evaluateDreamAssignmentAndReturnAsInteger(&controller->mValue, NULL);
	setStageElementAnimation(tElement, anim);
}

static void handleSinXController(BackgroundState* e, StaticStageHandlerElement* tElement) {
	BackgroundSingleValueController* controller = (BackgroundSingleValueController*)(&e->mController);

	if (controller->mHasAssignmentValue) {
		const auto amplitudePeriodPhase = evaluateDreamAssignmentAndReturnAsVector3D(&controller->mValue, NULL);
		const auto val = calculateStageElementSinOffset(e->mTime, amplitudePeriodPhase.x, amplitudePeriodPhase.y, amplitudePeriodPhase.z);
		setStageElementSinOffsetX(tElement, val);
	}
}
static void handleSinYController(BackgroundState* e, StaticStageHandlerElement* tElement) {
	BackgroundSingleValueController* controller = (BackgroundSingleValueController*)(&e->mController);

	if (controller->mHasAssignmentValue) {
		const auto amplitudePeriodPhase = evaluateDreamAssignmentAndReturnAsVector3D(&controller->mValue, NULL);
		const auto val = calculateStageElementSinOffset(e->mTime, amplitudePeriodPhase.x, amplitudePeriodPhase.y, amplitudePeriodPhase.z);
		setStageElementSinOffsetY(tElement, val);
	}
}

static void handleSingleBackgroundControllerForOneElement(void* tCaller, void* tData) {
	BackgroundState* e = (BackgroundState*)tCaller;
	StaticStageHandlerElement* element = (StaticStageHandlerElement*)tData;

	switch (e->mType) {
	case BACKGROUND_STATE_CONTROLLER_NULL:
		handleNullController();
		break;
	case BACKGROUND_STATE_CONTROLLER_VISIBLE:
		handleVisibleController(e, element);
		break;
	case BACKGROUND_STATE_CONTROLLER_ENABLED:
		handleEnabledController(e, element);
		break;
	case BACKGROUND_STATE_CONTROLLER_VEL_SET:
		handleVelSetController(e, element);
		break;
	case BACKGROUND_STATE_CONTROLLER_VEL_ADD:
		handleVelAddController(e, element);
		break;
	case BACKGROUND_STATE_CONTROLLER_POS_SET:
		handlePosSetController(e, element);
		break;
	case BACKGROUND_STATE_CONTROLLER_POS_ADD:
		handlePosAddController(e, element);
		break;
	case BACKGROUND_STATE_CONTROLLER_ANIM:
		handleAnimController(e, element);
		break;
	case BACKGROUND_STATE_CONTROLLER_SIN_X:
		handleSinXController(e, element);
		break;
	case BACKGROUND_STATE_CONTROLLER_SIN_Y:
		handleSinYController(e, element);
		break;
	default:
		logWarningFormat("Warning, unrecognized state controller type %d. Defaulting to null.", e->mType);
		handleNullController();
		break;
	}
}

static void updateSingleBackgroundController(void* tCaller, void* tData) {
	(void)tCaller;
	BackgroundStateGroup* group = (BackgroundStateGroup*)tCaller;
	BackgroundState* e = (BackgroundState*)tData;

	if (group->mLoopTime != -1 && group->mTime == group->mLoopTime) e->mTime = 0;
	if (e->mLoopTime != -1 && e->mTime == e->mLoopTime) e->mTime = 0;


	if (e->mTime >= e->mStartTime && e->mTime <= e->mEndTime) {
		Vector* elements;
		if (vector_size(&e->mElements)) elements = &e->mElements;
		else elements = &group->mElements;
	
		vector_map(elements, handleSingleBackgroundControllerForOneElement, e);
	}

	e->mTime++;
}

static void updateSingleBackgroundGroup(void* tCaller, void* tData) {
	(void)tCaller;
	BackgroundStateGroup* group = (BackgroundStateGroup*)tData;
	vector_map(&group->mStates, updateSingleBackgroundController, group);

	if (group->mLoopTime != -1 && group->mTime == group->mLoopTime) group->mTime = 0;
	group->mTime++;
}

static void updateBackgroundStateHandler(void* tData) {
	(void)tData;
	setProfilingSectionMarkerCurrentFunction();
	vector_map(&gMugenBackgroundStateHandlerData.mStates.mBackgroundStateGroups, updateSingleBackgroundGroup, NULL);
}

ActorBlueprint getBackgroundStateHandler() {
	return makeActorBlueprint(loadBackgroundStateHandler, NULL, updateBackgroundStateHandler);
}


static void loadBackgroundStateControllerElements(MugenDefScriptGroup* tGroup, Vector* oElements) {
	
	*oElements = new_vector();

	if (isMugenDefStringVectorVariableAsGroup(tGroup, "ctrlid")) {
		MugenStringVector vector = getMugenDefStringVectorVariableAsGroup(tGroup, "ctrlid");
		int i;
		for (i = 0; i < vector.mSize; i++) {
			int id = atoi(vector.mElement[i]);
			auto elementList = getStageHandlerElementsWithID(id);
			int j;
			for (j = 0; j < (int)elementList.size(); j++) {
				StaticStageHandlerElement* element = elementList[j];
				vector_push_back(oElements, element);
			}
		}
	}
	else if(isMugenDefNumberVariableAsGroup(tGroup, "ctrlid")){
		int id = getMugenDefNumberVariableAsGroup(tGroup, "ctrlid");
		auto elementList = getStageHandlerElementsWithID(id);
		int j;
		for (j = 0; j < (int)elementList.size(); j++) {
			StaticStageHandlerElement* element = elementList[j];
			vector_push_back(oElements, element);
		}
	}
}

static void loadNullController(BackgroundState* e) {
	e->mType = BACKGROUND_STATE_CONTROLLER_NULL;
}

static void loadSingleValueController(BackgroundState* e, BackgroundControllerType tType, MugenDefScriptGroup* tGroup) {
	BackgroundSingleValueController* controller = (BackgroundSingleValueController*)(&e->mController);
	controller->mHasAssignmentValue = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("value", tGroup, &controller->mValue);
	e->mType = tType;
}

static void loadPhysicsController(BackgroundState* e, BackgroundControllerType tType, MugenDefScriptGroup* tGroup) {
	BackgroundPhysicsController* controller = (BackgroundPhysicsController*)(&e->mController);
	controller->mHasAssignmentX = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("x", tGroup, &controller->x);
	controller->mHasAssignmentY = fetchDreamAssignmentFromGroupAndReturnWhetherItExists("y", tGroup, &controller->y);
	e->mType = tType;
}


static void loadControllerType(BackgroundState* e, MugenDefScriptGroup* tGroup) {
	char* text = getAllocatedMugenDefStringOrDefaultAsGroup(tGroup, "type", "null");
	turnStringLowercase(text);

	if (!strcmp("null", text)) {
		loadNullController(e);
	} else if (!strcmp("visible", text)) {
		loadSingleValueController(e, BACKGROUND_STATE_CONTROLLER_VISIBLE, tGroup);
	}
	else if (!strncmp("enable", text, (sizeof("enable") - 1))) {
		loadSingleValueController(e, BACKGROUND_STATE_CONTROLLER_ENABLED, tGroup);
	}
	else if (!strcmp("velset", text)) {
		loadPhysicsController(e, BACKGROUND_STATE_CONTROLLER_VEL_SET, tGroup);
	}
	else if (!strcmp("veladd", text)) {
		loadPhysicsController(e, BACKGROUND_STATE_CONTROLLER_VEL_ADD, tGroup);
	}
	else if (!strcmp("posset", text)) {
		loadPhysicsController(e, BACKGROUND_STATE_CONTROLLER_POS_SET, tGroup);
	}
	else if (!strcmp("posadd", text)) {
		loadPhysicsController(e, BACKGROUND_STATE_CONTROLLER_POS_ADD, tGroup);
	}
	else if (!strcmp("anim", text)) {
		loadSingleValueController(e, BACKGROUND_STATE_CONTROLLER_ANIM, tGroup);
	}
	else if (!strcmp("sinx", text)) {
		loadSingleValueController(e, BACKGROUND_STATE_CONTROLLER_SIN_X, tGroup);

	}
	else if (!strcmp("siny", text)) {
		loadSingleValueController(e, BACKGROUND_STATE_CONTROLLER_SIN_Y, tGroup);
	}
	else {
		logErrorFormat("Unrecognized state controller %s. Defaulting to null.", text);
		loadNullController(e);
	}

	freeMemory(text);
}

static Vector3DI getTimeFromControllerGroup(MugenDefScriptGroup* tGroup) {
	if (isMugenDefStringVectorVariableAsGroup(tGroup, "time")) {
		MugenStringVector vector = getMugenDefStringVectorVariableAsGroup(tGroup, "time");
		if (vector.mSize == 1) {
			return Vector3DI(atoi(vector.mElement[0]), atoi(vector.mElement[0]), -1);
		}
		else if (vector.mSize == 2) {
			return Vector3DI(atoi(vector.mElement[0]), atoi(vector.mElement[1]), -1);
		}
		else if (vector.mSize >= 3) {
			return Vector3DI(atoi(vector.mElement[0]), atoi(vector.mElement[1]), atoi(vector.mElement[2]));
		}
		else {
			logWarningFormat("Unable to read time vector with size %d. Default to 0, INF, -1.", vector.mSize);
			return Vector3DI(0, INF, -1);
		}
	}
	else if (isMugenDefNumberVariableAsGroup(tGroup, "time")) {
		int time = getMugenDefNumberVariableAsGroup(tGroup, "time");
		return Vector3DI(time, time, -1);
	}
	else {
		logWarning("Unable to read time variable. Default to 0, INF, -1.");
		return Vector3DI(0, INF, -1);
	}
}

static void loadBackgroundStateController(MugenDefScriptGroup* tGroup) {
	if (!vector_size(&gMugenBackgroundStateHandlerData.mStates.mBackgroundStateGroups)) {
		logErrorFormat("Unable to add background state %s, no state group defined. Ignoring.", tGroup->mName.data());
		return;
	}
	BackgroundStateGroup* group = (BackgroundStateGroup*)vector_get_back(&gMugenBackgroundStateHandlerData.mStates.mBackgroundStateGroups);
	BackgroundState* e = (BackgroundState*)allocMemory(sizeof(BackgroundState));

	loadBackgroundStateControllerElements(tGroup, &e->mElements);
	Vector3DI timeVector = getTimeFromControllerGroup(tGroup);
	e->mStartTime = timeVector.x;
	e->mEndTime = timeVector.y;
	e->mLoopTime = timeVector.z;
	e->mTime = 0;

	loadControllerType(e, tGroup);

	vector_push_back_owned(&group->mStates, e);
}

static void loadBackgroundStateControllerDef(MugenDefScriptGroup* tGroup) {
	BackgroundStateGroup* group = (BackgroundStateGroup*)allocMemory(sizeof(BackgroundStateGroup));
	loadBackgroundStateControllerElements(tGroup, &group->mElements);
	group->mLoopTime = getMugenDefIntegerOrDefaultAsGroup(tGroup, "looptime", -1);
	group->mStates = new_vector();
	group->mTime = 0;
	vector_push_back_owned(&gMugenBackgroundStateHandlerData.mStates.mBackgroundStateGroups, group);
}


static void loadBackgroundStatesFromGroup(MugenDefScriptGroup* tGroup) {
	char firstW[100];
	int items = sscanf(tGroup->mName.data(), "%s", firstW);
	if (!items) return;
	turnStringLowercase(firstW);

	if (!strcmp("bgctrldef", firstW)) {
		loadBackgroundStateControllerDef(tGroup);
	}
	else if (!strcmp("bgctrl", firstW)) {
		loadBackgroundStateController(tGroup);
	}
}

void setBackgroundStatesFromScript(MugenDefScript * tScript)
{
	MugenDefScriptGroup* current = tScript->mFirstGroup;
	while (current) {
		loadBackgroundStatesFromGroup(current);
		current = current->mNext;
	}
}

static void resetSingleBackgroundController(void* /*tCaller*/, void* tData) {
	BackgroundState* e = (BackgroundState*)tData;
	e->mTime = 0;
}

static void resetSingleBackgroundGroup(void* /*tCaller*/, void* tData) {
	BackgroundStateGroup* group = (BackgroundStateGroup*)tData;
	vector_map(&group->mStates, resetSingleBackgroundController, NULL);
	group->mTime = 0;
}

void resetBackgroundStateHandler()
{
	vector_map(&gMugenBackgroundStateHandlerData.mStates.mBackgroundStateGroups, resetSingleBackgroundGroup, NULL);
}


