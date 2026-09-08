#include "dolmexicastoryscreen.h"

#include <string.h>
#include <assert.h>

#include <prism/log.h>
#include <prism/math.h>
#include <prism/actorhandler.h>
#include <prism/mugenanimationhandler.h>
#include <prism/mugentexthandler.h>
#include <prism/input.h>
#include <prism/stlutil.h>
#include <prism/sound.h>
#include <prism/debug.h>
#include <prism/system.h>

#include "mugenassignmentevaluator.h"
#include "mugenstatecontrollers.h"
#include "mugenstatehandler.h"
#include "titlescreen.h"
#include "stage.h"
#include "mugenstagehandler.h"
#include "storymode.h"
#include "characterselectscreen.h"
#include "mugencommandhandler.h"
#include "mugensound.h"
#include "dolmexicadebug.h"
#include "config.h"
#include "storyhelper.h"

using namespace std;

#define COORD_P 320
#define DOLMEXICA_STORY_ANIMATION_BASE_Z 30
#define DOLMEXICA_STORY_SHADOW_BASE_Z (DOLMEXICA_STORY_ANIMATION_BASE_Z - 1)
#define DOLMEXICA_STORY_TEXT_BASE_Z 60
#define DOLMEXICA_STORY_ID_Z_FACTOR 0.1f
#define DOLMEXICA_STORY_OFFSET_Z_FACTOR 0.01f

static struct {
	char mPath[1024];
	MugenSpriteFile mSprites;
	MugenAnimations mAnimations;
	MugenSounds mSounds;

	int mHasCommands;
	DreamMugenCommands mCommands;
	int mCommandID;

	DreamMugenStates mStoryStates;
	int mHasStage;
	int mHasMusic;
	int mHasFonts;

	map<int, StoryInstance> mHelperInstances;

	int mDebugStartState;
	int mDebugStartStateFrom;
} gDolmexicaStoryScreenData;

#ifdef _WIN32
#include <algorithm>
#include <cctype>
#include <climits>
#include <cmath>
#include <functional>
#include <set>
#include <sstream>
#include <tuple>
#include <sys/stat.h>
#include <imgui/imgui.h>
#include <prism/file.h>
#include <prism/mugendefwriter.h>
#include <prism/wrapper.h>
#include "prism/windows/debugimgui_win.h"
#include "prism/windows/debugimgui_texteditor_win.h"
#include "mugenstatereader.h"

// undo/redo core: commands capture values (never pointers) and re-resolve their target by (instance key, kind, id) on every execution, so they survive object churn
// mApply/mRevert return false when the target no longer exists, the command is then skipped with a status note.
struct StoryEditorCommand {
	std::string mDescription;
	std::string mCoalesceKey;
	int mFrame = 0;
	std::function<bool()> mApply;
	std::function<bool()> mRevert;
};

static struct {
	bool mIsPlaybackPaused = false;
	int mPendingPlaybackSteps = 0;
	bool mIsPlaybackHeld = false;
	float mPlaybackSpeed = 1.0f;
	int mPendingRestartTargetState = -1;
	std::vector<int> mStateHistory;
	bool mHasLastHistoryState = false;
	int mLastHistoryState = 0;
	bool mDoesDismissTextsOnJump = true;

	std::vector<StoryEditorCommand> mUndoStack;
	size_t mUndoPosition = 0;
	std::string mUndoStatusMessage;

	// hot reload: watches the loaded .def state files for on-disk changes
	bool mIsStoryScreenActive = false; // reload paths use setNewScreen(story screen), keep them off the fight-embedded story actor
	bool mIsAutoReloadEnabled = false;
	int mHotReloadCheckCounter = 0;
	std::map<std::string, long long> mWatchedFileModificationTimes;
	std::vector<std::string> mChangedWatchedFiles;
} gStoryEditorData;

static void clearStoryEditorUndoStack(const char* tReason)
{
	if (gStoryEditorData.mUndoStack.empty()) return;
	gStoryEditorData.mUndoStack.clear();
	gStoryEditorData.mUndoPosition = 0;
	gStoryEditorData.mUndoStatusMessage = std::string("Undo history cleared (") + tReason + ")";
}

// tApply is NOT executed here, callers apply the new value live before pushing
// A non-empty coalesce key merges rapid same-target edits (e.g. InputInt +/- bursts) into one entry
static void pushStoryEditorCommand(const std::string& tDescription, const std::string& tCoalesceKey, std::function<bool()> tApply, std::function<bool()> tRevert)
{
	static const size_t STORY_EDITOR_UNDO_STACK_MAX = 256;
	static const int STORY_EDITOR_COALESCE_FRAME_WINDOW = 30;
	auto& s = gStoryEditorData;
	s.mUndoStack.resize(s.mUndoPosition); // pushing invalidates the redo tail
	const auto frame = ImGui::GetFrameCount();
	if (!tCoalesceKey.empty() && !s.mUndoStack.empty())
	{
		auto& top = s.mUndoStack.back();
		if (top.mCoalesceKey == tCoalesceKey && (frame - top.mFrame) <= STORY_EDITOR_COALESCE_FRAME_WINDOW)
		{
			top.mDescription = tDescription;
			top.mFrame = frame;
			top.mApply = std::move(tApply); // keep the oldest revert, take the newest apply
			return;
		}
	}
	StoryEditorCommand cmd;
	cmd.mDescription = tDescription;
	cmd.mCoalesceKey = tCoalesceKey;
	cmd.mFrame = frame;
	cmd.mApply = std::move(tApply);
	cmd.mRevert = std::move(tRevert);
	s.mUndoStack.push_back(std::move(cmd));
	if (s.mUndoStack.size() > STORY_EDITOR_UNDO_STACK_MAX)
	{
		s.mUndoStack.erase(s.mUndoStack.begin());
	}
	s.mUndoPosition = s.mUndoStack.size();
}

static void undoStoryEditorCommand()
{
	auto& s = gStoryEditorData;
	if (!s.mUndoPosition) return;
	s.mUndoPosition--;
	const auto& cmd = s.mUndoStack[s.mUndoPosition];
	s.mUndoStatusMessage = (cmd.mRevert() ? "Undid: " : "Undo skipped, target gone: ") + cmd.mDescription;
}

static void redoStoryEditorCommand()
{
	auto& s = gStoryEditorData;
	if (s.mUndoPosition >= s.mUndoStack.size()) return;
	const auto& cmd = s.mUndoStack[s.mUndoPosition];
	s.mUndoPosition++;
	s.mUndoStatusMessage = (cmd.mApply() ? "Redid: " : "Redo skipped, target gone: ") + cmd.mDescription;
}

static bool isStoryEditorHoldingPlayback()
{
	return gStoryEditorData.mIsPlaybackHeld;
}

static void setStoryInstanceStateMachinesPaused(int tIsPaused)
{
	for (auto& instancePair : gDolmexicaStoryScreenData.mHelperInstances)
	{
		setDreamRegisteredStateMachinePauseStatus(instancePair.second.mRegisteredStateMachine, tIsPaused);
	}
}

static void updateStoryEditorStateHistory()
{
	if (gDolmexicaStoryScreenData.mHelperInstances.empty()) return;
	const auto currentState = getDolmexicaStoryStateNumber(getDolmexicaStoryRootInstance());

	if (gStoryEditorData.mPendingRestartTargetState != -1 && currentState == gStoryEditorData.mPendingRestartTargetState)
	{
		// the restart redirect has fired, clear it so it does not fire again on later visits to the from-state
		setDolmexicaStoryDebugStartState(0, 0);
		gStoryEditorData.mPendingRestartTargetState = -1;
	}

	if (!gStoryEditorData.mHasLastHistoryState || currentState != gStoryEditorData.mLastHistoryState)
	{
		static const size_t STORY_EDITOR_STATE_HISTORY_MAX = 32;
		gStoryEditorData.mStateHistory.push_back(currentState);
		if (gStoryEditorData.mStateHistory.size() > STORY_EDITOR_STATE_HISTORY_MAX)
		{
			gStoryEditorData.mStateHistory.erase(gStoryEditorData.mStateHistory.begin());
		}
		gStoryEditorData.mHasLastHistoryState = true;
		gStoryEditorData.mLastHistoryState = currentState;
	}
}

static void updateStoryEditorHotReload();
static void updateStoryEditorWatch();

// called at the start of updateStoryScreen, i.e. after the state machines have already been updated by the actor handler this frame
// pause flags set here therefore take effect starting with the next frame's state machine update
static void updateStoryEditorPlayback()
{
	const auto shouldHold = gStoryEditorData.mIsPlaybackPaused && !gStoryEditorData.mPendingPlaybackSteps;
	if (gStoryEditorData.mPendingPlaybackSteps > 0) gStoryEditorData.mPendingPlaybackSteps--;
	if (shouldHold || shouldHold != gStoryEditorData.mIsPlaybackHeld)
	{
		setStoryInstanceStateMachinesPaused(shouldHold); // while holding, set every frame so helpers created during a step get paused too
	}
	if (shouldHold != gStoryEditorData.mIsPlaybackHeld)
	{
		if (shouldHold) pauseMugenAnimationHandler();
		else unpauseMugenAnimationHandler();
		gStoryEditorData.mIsPlaybackHeld = shouldHold;
	}
	updateStoryEditorStateHistory();
	updateStoryEditorWatch(); // breakpoint checks, state machines have already run this frame, so var/state changes are visible here
	updateStoryEditorHotReload(); // runs even while playback is held (this function is called before the hold early-out)
}

static void resetStoryEditorSceneSelection();
static void resetStoryEditorValidation();
static void resetStoryEditorSoundLog();
static void resetStoryEditorWatchSnapshots();
static void resetStoryEditorTimeline();

static void resetStoryEditorPlayback()
{
	if (gStoryEditorData.mIsPlaybackHeld) unpauseMugenAnimationHandler();
	gStoryEditorData.mIsPlaybackPaused = false;
	gStoryEditorData.mPendingPlaybackSteps = 0;
	gStoryEditorData.mIsPlaybackHeld = false;
	gStoryEditorData.mStateHistory.clear();
	gStoryEditorData.mHasLastHistoryState = false;
	// mPendingRestartTargetState deliberately survives the reset since restarting at a state reloads the scene
	gStoryEditorData.mHotReloadCheckCounter = 0;
	gStoryEditorData.mWatchedFileModificationTimes.clear(); // re-recorded as the fresh provenances come in, mIsAutoReloadEnabled survives
	gStoryEditorData.mChangedWatchedFiles.clear();
	clearStoryEditorUndoStack("scene load");
	resetStoryEditorSceneSelection(); // scene editing stays enabled across scene loads, only the selection resets
	resetStoryEditorValidation(); // results refer to the previous scene's objects
	resetStoryEditorSoundLog(); // entries reference the previous scene's sound file
	resetStoryEditorWatchSnapshots(); // the watch list and break toggles survive, value baselines re-record on the fresh scene
	resetStoryEditorTimeline(); // rebuilt lazily from the fresh provenances
}

static void restartStoryScreenAtState(int tState)
{
	setDolmexicaStoryDebugStartState(0, tState);
	gStoryEditorData.mPendingRestartTargetState = tState;
	setNewScreen(getDolmexicaStoryScreen());
}

// hot reload 
// Watches every .def file the loaded statedefs came from (statedef provenance map), a change on disk triggers, or, with auto-reload off, offers, a scene reload at the current root state via the M1 restart path. Editor-initiated writes refresh the stored timestamp first, so saving from the Save window does not trigger a reload by itself, saving from the built-in source editor (M5 jump-to-source) deliberately does, closing the edit-source -> see-result loop

static long long getStoryEditorFileModificationTime(const std::string& tPath)
{
	char fullPath[1024];
	getFullPath(fullPath, tPath.c_str());
	struct stat attributes;
	if (::stat(fullPath, &attributes)) return -1;
	return (long long)attributes.st_mtime;
}

static void refreshStoryEditorWatchedFileTime(const std::string& tPath)
{
	gStoryEditorData.mWatchedFileModificationTimes[tPath] = getStoryEditorFileModificationTime(tPath);
}

static void reloadStoryScreenAtCurrentState()
{
	if (!gStoryEditorData.mIsStoryScreenActive || gDolmexicaStoryScreenData.mHelperInstances.empty()) return;
	restartStoryScreenAtState(getDolmexicaStoryStateNumber(getDolmexicaStoryRootInstance()));
}

static void updateStoryEditorHotReload()
{
	static const int STORY_EDITOR_HOT_RELOAD_CHECK_INTERVAL = 30;
	if (!gStoryEditorData.mIsStoryScreenActive) return;
	if (++gStoryEditorData.mHotReloadCheckCounter < STORY_EDITOR_HOT_RELOAD_CHECK_INTERVAL) return;
	gStoryEditorData.mHotReloadCheckCounter = 0;

	auto hasChanged = false;
	for (const auto& provenancePair : getDreamMugenStateDefProvenances())
	{
		const auto& path = provenancePair.second;
		const auto modificationTime = getStoryEditorFileModificationTime(path);
		const auto it = gStoryEditorData.mWatchedFileModificationTimes.find(path);
		if (it == gStoryEditorData.mWatchedFileModificationTimes.end())
		{
			gStoryEditorData.mWatchedFileModificationTimes[path] = modificationTime; // first sighting establishes the baseline
			continue;
		}
		if (modificationTime == it->second) continue;
		it->second = modificationTime;
		hasChanged = true;
		if (!std::count(gStoryEditorData.mChangedWatchedFiles.begin(), gStoryEditorData.mChangedWatchedFiles.end(), path))
		{
			gStoryEditorData.mChangedWatchedFiles.push_back(path);
		}
	}
	if (hasChanged && gStoryEditorData.mIsAutoReloadEnabled)
	{
		reloadStoryScreenAtCurrentState();
	}
}

// mirrors the teardown the A-press input path performs when a finished textbox is dismissed, minus the state change
// texts dismissed this way get revived by the target state's ChangeText (setDolmexicaStoryTextText re-enables disabled texts)
static void dismissStoryInstanceTextsForStateJump(StoryInstance& tInstance)
{
	for (auto& textPair : tInstance.mStoryTexts)
	{
		auto& e = textPair.second;
		if (e.mIsDisabled) continue;
		e.mGoesToNextState = 0;
		e.mHasFinished = 1;
		setDolmexicaStoryTextInactive(&tInstance, textPair.first);
	}
}

static void performStoryEditorStateJump(StoryInstance* tRootInstance, int tState)
{
	if (gStoryEditorData.mDoesDismissTextsOnJump)
	{
		for (auto& instancePair : gDolmexicaStoryScreenData.mHelperInstances)
		{
			dismissStoryInstanceTextsForStateJump(instancePair.second);
		}
	}
	changeDolmexicaStoryStateOutsideStateHandler(tRootInstance, tState);
	clearStoryEditorUndoStack("state jump");
}

static std::string getStoryInstanceDisplayName(const StoryInstance* tInstance)
{
	for (const auto& instancePair : gDolmexicaStoryScreenData.mHelperInstances)
	{
		if (&instancePair.second == tInstance)
		{
			return (instancePair.first == -1) ? std::string("Root (-1)") : ("Helper " + std::to_string(instancePair.first));
		}
	}
	return "Unknown";
}

// provenance stamps + dirty tracking for .def write-back 
// Objects are stamped when the create/change/lock controllers execute, edits mark (def key, value) pairs dirty per object. Dirtiness is a value comparison (current vs last-saved, or vs the pre-first-edit baseline before any save), so undo/redo keeps the flags correct without tracking stack positions.

struct StoryEditorProvenanceStamp {
	int mIsValid = 0; // source file location resolved
	const void* mController = nullptr;
	std::string mScriptPath;
	int mStateID = -1;
	int mControllerIndex = -1; // == group offset after the [Statedef N] group, matching saveMugenDefString
};

struct StoryEditorObjectStamps {
	StoryEditorProvenanceStamp mCreation;
	StoryEditorProvenanceStamp mChangeText;
	StoryEditorProvenanceStamp mLockText;
	StoryEditorProvenanceStamp mChangeAnim;
	StoryEditorProvenanceStamp mSetFacing;
	StoryEditorProvenanceStamp mSetScale;
};

struct StoryEditorDirtyField {
	std::string mLabel;
	std::string mBaselineValue; // live value before the first edit
	std::string mCurrentValue;
	std::string mSavedValue;
	int mHasSavedValue = 0;
	int mUsesLockProvenance = 0; // routes to the LockText controller group instead of CreateText/ChangeText
	int mHasPendingConflict = 0; // original file value is an expression, awaiting explicit confirmation
	std::string mConflictOriginalValue;
	int mHasNewControllerTargetState = 0; // user overrode the target state for "save as new controller"
	int mNewControllerTargetState = 0;
};

typedef std::tuple<int, int, int> StoryEditorObjectKey; // (instance key, object kind, id)

static struct {
	std::map<StoryEditorObjectKey, StoryEditorObjectStamps> mObjectStamps;
	std::map<StoryEditorObjectKey, std::map<std::string, StoryEditorDirtyField>> mDirtyFields;
	std::set<std::string> mBackedUpPaths; // session-lifetime: survives scene reloads, one .bak per file per run
	std::map<std::pair<std::string, int>, int> mAppendedControllerCounts; // (file, statedef) -> groups appended since load; keeps insertion anchors correct before a reload
	std::string mStatusMessage;
} gStoryEditorSaveData;

// called at the start of both story load paths, before the state files parse (which repopulates the controller provenances) and before state 0 executes (which re-stamps the objects it creates)
static void resetStoryEditorSaveData()
{
	gStoryEditorSaveData.mObjectStamps.clear();
	gStoryEditorSaveData.mDirtyFields.clear();
	gStoryEditorSaveData.mAppendedControllerCounts.clear();
	gStoryEditorSaveData.mStatusMessage.clear();
	clearDreamMugenStateControllerProvenances();
}

static bool getStoryEditorInstanceKey(const StoryInstance* tInstance, int* oKey)
{
	for (const auto& instancePair : gDolmexicaStoryScreenData.mHelperInstances)
	{
		if (&instancePair.second == tInstance)
		{
			*oKey = instancePair.first;
			return true;
		}
	}
	return false;
}

static const StoryEditorObjectStamps* getStoryEditorObjectStamps(int tInstanceKey, StoryEditorObjectKind tObjectKind, int tID)
{
	const auto it = gStoryEditorSaveData.mObjectStamps.find(StoryEditorObjectKey(tInstanceKey, int(tObjectKind), tID));
	return (it == gStoryEditorSaveData.mObjectStamps.end()) ? nullptr : &it->second;
}

static void imguiStoryEditorProvenanceLine(int tInstanceKey, StoryEditorObjectKind tObjectKind, int tID);
static void openStoryEditorCurrentStateSource();

static void markStoryEditorFieldEdited(int tInstanceKey, StoryEditorObjectKind tObjectKind, int tID, const char* tDefKey, const char* tLabel, const std::string& tOldValue, const std::string& tNewValue, int tUsesLockProvenance = 0)
{
	auto& fields = gStoryEditorSaveData.mDirtyFields[StoryEditorObjectKey(tInstanceKey, int(tObjectKind), tID)];
	const auto it = fields.find(tDefKey);
	if (it == fields.end())
	{
		auto& field = fields[tDefKey];
		field.mLabel = tLabel;
		field.mBaselineValue = tOldValue;
		field.mCurrentValue = tNewValue;
		field.mUsesLockProvenance = tUsesLockProvenance;
		return;
	}
	it->second.mCurrentValue = tNewValue;
}

static bool isStoryEditorFieldDirty(const StoryEditorDirtyField& tField)
{
	return tField.mCurrentValue != (tField.mHasSavedValue ? tField.mSavedValue : tField.mBaselineValue);
}

static std::string formatStoryEditorDefNumber(float tValue)
{
	if (!std::isfinite(tValue)) return "";
	const auto rounded = std::round(tValue);
	if (std::abs(tValue - rounded) < 0.005) return std::to_string(int(rounded));
	char buffer[64];
	sprintf(buffer, "%.2f", tValue);
	return buffer;
}

static std::string formatStoryEditorDefVector2(const Vector2D& tValue)
{
	return formatStoryEditorDefNumber(tValue.x) + ", " + formatStoryEditorDefNumber(tValue.y);
}

void storyEditorStampStoryObjectController(StoryInstance* tInstance, StoryEditorObjectKind tObjectKind, int tID, StoryEditorStampKind tStampKind, const void* tController)
{
	if (!isInDevelopMode()) return;
	int instanceKey;
	if (!getStoryEditorInstanceKey(tInstance, &instanceKey)) return;
	auto& stamps = gStoryEditorSaveData.mObjectStamps[StoryEditorObjectKey(instanceKey, int(tObjectKind), tID)];
	if (tStampKind == StoryEditorStampKind::Creation) stamps = StoryEditorObjectStamps(); // recreation invalidates older ChangeText/LockText/ChangeAnim/SetFacing stamps
	auto& stamp = (tStampKind == StoryEditorStampKind::Creation) ? stamps.mCreation : (tStampKind == StoryEditorStampKind::ChangeText) ? stamps.mChangeText : (tStampKind == StoryEditorStampKind::LockText) ? stamps.mLockText : (tStampKind == StoryEditorStampKind::ChangeAnim) ? stamps.mChangeAnim : (tStampKind == StoryEditorStampKind::SetFacing) ? stamps.mSetFacing : stamps.mSetScale;
	stamp = StoryEditorProvenanceStamp();
	stamp.mController = tController;
	const auto provenance = getDreamMugenStateControllerProvenance(tController);
	if (!provenance) return;
	stamp.mScriptPath = provenance->mScriptPath;
	stamp.mStateID = provenance->mStateID;
	stamp.mControllerIndex = provenance->mControllerIndex;
	stamp.mIsValid = 1;
}

// target resolution and value appliers 
// Everything below re-resolves targets from keys/ids on every call so undo/redo commands survive object churn, a null resolve makes the applier return false ("target gone")

static StoryInstance* getStoryEditorInstance(int tInstanceKey)
{
	auto it = gDolmexicaStoryScreenData.mHelperInstances.find(tInstanceKey);
	return (it == gDolmexicaStoryScreenData.mHelperInstances.end()) ? nullptr : &it->second;
}

// NameID reverse lookup (mTextNames stores name -> id). The namespace is per instance and shared across object kinds, nameid() is kind-agnostic, so a name registered for a text can hit an animation that merely reuses the same id for z-ordering. That false-positive risk is why names render as a suffix NEXT TO the numeric id, never instead of it
static std::string getStoryEditorIDNameSuffix(int tInstanceKey, int tID)
{
	const auto instance = getStoryEditorInstance(tInstanceKey);
	if (!instance || instance->mTextNames.empty()) return std::string();
	std::set<std::string> names; // sorted for a stable display order, mTextNames is unordered
	for (const auto& textNamePair : instance->mTextNames)
	{
		if (textNamePair.second == tID) names.insert(textNamePair.first);
	}
	if (names.empty()) return std::string();
	std::string ret;
	for (const auto& name : names)
	{
		ret += (ret.empty() ? " '" : "/") + name;
	}
	return ret + "'";
}

// visibility flags hoisted out of the window functions (they were function-local statics) so the Ctrl+E shortcut can open every story editor window at once
static struct {
	bool mInstances = false;
	bool mPlayback = false;
	bool mUndo = false;
	bool mScene = false;
	bool mSave = false;
	bool mValidate = false;
	bool mTimeline = false;
	bool mCamera = false;
	bool mSound = false;
	bool mWatch = false;
} gStoryEditorWindowVisibility;

static void addStoryEditorWatch(int tInstanceKey, int tVariableKind, int tID);

struct StoryEditorAnimationTarget {
	int mInstanceKey;
	int mIsCharacter;
	int mID;
};

static StoryAnimation* getStoryEditorAnimationElement(const StoryEditorAnimationTarget& tTarget, StoryInstance** oInstance = nullptr, StoryCharacter** oCharacter = nullptr)
{
	const auto instance = getStoryEditorInstance(tTarget.mInstanceKey);
	if (!instance) return nullptr;
	StoryAnimation* animation;
	if (tTarget.mIsCharacter)
	{
		if (!stl_map_contains(instance->mStoryCharacters, tTarget.mID)) return nullptr;
		auto& character = instance->mStoryCharacters[tTarget.mID];
		if (oCharacter) *oCharacter = &character;
		animation = &character.mAnimation;
	}
	else
	{
		if (!stl_map_contains(instance->mStoryAnimations, tTarget.mID)) return nullptr;
		animation = &instance->mStoryAnimations[tTarget.mID];
	}
	if (!animation->mAnimationElement || !isRegisteredMugenAnimation(animation->mAnimationElement)) return nullptr;
	if (oInstance) *oInstance = instance;
	return animation;
}

enum class StoryEditorAnimationField {
	PositionX,
	PositionY,
	ScaleX,
	ScaleY,
	AngleDegrees,
	Opacity,
};

static bool applyStoryEditorAnimationField(const StoryEditorAnimationTarget& tTarget, StoryEditorAnimationField tField, float tValue)
{
	StoryInstance* instance;
	const auto animation = getStoryEditorAnimationElement(tTarget, &instance);
	if (!animation) return false;
	const auto id = tTarget.mID;
	const auto isPosition = (tField == StoryEditorAnimationField::PositionX) || (tField == StoryEditorAnimationField::PositionY);
	const auto isScale = (tField == StoryEditorAnimationField::ScaleX) || (tField == StoryEditorAnimationField::ScaleY);
	Vector2D oldPosition(0, 0);
	if (isPosition)
	{
		oldPosition.x = tTarget.mIsCharacter ? getDolmexicaStoryCharacterPositionX(instance, id) : getDolmexicaStoryAnimationPositionX(instance, id);
		oldPosition.y = tTarget.mIsCharacter ? getDolmexicaStoryCharacterPositionY(instance, id) : getDolmexicaStoryAnimationPositionY(instance, id);
	}
	Vector2D oldScale(0, 0);
	if (isScale && animation->mAnimationElement && isRegisteredMugenAnimation(animation->mAnimationElement))
	{
		oldScale = getMugenAnimationDrawScale(animation->mAnimationElement);
	}
	switch (tField)
	{
	case StoryEditorAnimationField::PositionX:
		tTarget.mIsCharacter ? setDolmexicaStoryCharacterPositionX(instance, id, tValue) : setDolmexicaStoryAnimationPositionX(instance, id, tValue);
		break;
	case StoryEditorAnimationField::PositionY:
		tTarget.mIsCharacter ? setDolmexicaStoryCharacterPositionY(instance, id, tValue) : setDolmexicaStoryAnimationPositionY(instance, id, tValue);
		break;
	case StoryEditorAnimationField::ScaleX:
		tTarget.mIsCharacter ? setDolmexicaStoryCharacterScaleX(instance, id, tValue) : setDolmexicaStoryAnimationScaleX(instance, id, tValue);
		break;
	case StoryEditorAnimationField::ScaleY:
		tTarget.mIsCharacter ? setDolmexicaStoryCharacterScaleY(instance, id, tValue) : setDolmexicaStoryAnimationScaleY(instance, id, tValue);
		break;
	case StoryEditorAnimationField::AngleDegrees:
		tTarget.mIsCharacter ? setDolmexicaStoryCharacterAngle(instance, id, tValue) : setDolmexicaStoryAnimationAngle(instance, id, tValue);
		break;
	case StoryEditorAnimationField::Opacity:
		tTarget.mIsCharacter ? setDolmexicaStoryCharacterOpacity(instance, id, tValue) : setDolmexicaStoryAnimationOpacity(instance, id, tValue);
		break;
	}
	if (isPosition)
	{
		auto newPosition = oldPosition;
		((tField == StoryEditorAnimationField::PositionX) ? newPosition.x : newPosition.y) = tValue;
		// stage-bound objects (CreateChar/CreateAnim with stage = 1): the creating controller's pos param is stage-space, but the editor operates on the raw element position, which has the stage coordinate-system offset (+ half screen on x) baked in by setDolmexicaStoryAnimationBoundToStageInternal. Serialize tracked values in stage space so save-back writes what the controller expects on reload (PosAdd deltas are unaffected either way, the offset cancels out)
		auto serializedOldPosition = oldPosition;
		auto serializedNewPosition = newPosition;
		if (animation->mIsBoundToStage)
		{
			const auto stageOffset = getDreamStageCoordinateSystemOffset(COORD_P);
			serializedOldPosition.x -= stageOffset.x + (COORD_P / 2);
			serializedOldPosition.y -= stageOffset.y;
			serializedNewPosition.x -= stageOffset.x + (COORD_P / 2);
			serializedNewPosition.y -= stageOffset.y;
		}
		markStoryEditorFieldEdited(tTarget.mInstanceKey, tTarget.mIsCharacter ? StoryEditorObjectKind::Character : StoryEditorObjectKind::Animation, id, "pos", "Position", formatStoryEditorDefVector2(serializedOldPosition), formatStoryEditorDefVector2(serializedNewPosition));
	}
	if (isScale)
	{
		// AnimScaleSet/CharScaleSet take separate x/y params (both optional), so each axis is tracked as its own dirty field, unlike pos which is one vector param on the creating controller. The widget value writes into the draw scale 1:1, same as the controllers, so it matches the .def convention
		const auto isX = tField == StoryEditorAnimationField::ScaleX;
		markStoryEditorFieldEdited(tTarget.mInstanceKey, tTarget.mIsCharacter ? StoryEditorObjectKind::Character : StoryEditorObjectKind::Animation, id, isX ? "x" : "y", isX ? "ScaleX" : "ScaleY", formatStoryEditorDefNumber(isX ? oldScale.x : oldScale.y), formatStoryEditorDefNumber(tValue));
	}
	return true;
}

static bool applyStoryEditorAnimationFacing(const StoryEditorAnimationTarget& tTarget, int tIsFacingRight)
{
	StoryInstance* instance;
	const auto animation = getStoryEditorAnimationElement(tTarget, &instance);
	if (!animation) return false;
	const auto oldIsFacingRight = getMugenAnimationIsFacingRight(animation->mAnimationElement);
	tTarget.mIsCharacter ? setDolmexicaStoryCharacterIsFacingRight(instance, tTarget.mID, tIsFacingRight) : setDolmexicaStoryAnimationIsFacingRight(instance, tTarget.mID, tIsFacingRight);
	markStoryEditorFieldEdited(tTarget.mInstanceKey, tTarget.mIsCharacter ? StoryEditorObjectKind::Character : StoryEditorObjectKind::Animation, tTarget.mID, "facing", "FacingRight", oldIsFacingRight ? "1" : "-1", tIsFacingRight ? "1" : "-1"); // .def facing convention: 1 = right, -1 = left
	return true;
}

static bool applyStoryEditorAnimationColor(const StoryEditorAnimationTarget& tTarget, const Vector3D& tColor)
{
	StoryInstance* instance;
	if (!getStoryEditorAnimationElement(tTarget, &instance)) return false;
	tTarget.mIsCharacter ? setDolmexicaStoryCharacterColor(instance, tTarget.mID, tColor) : setDolmexicaStoryAnimationColor(instance, tTarget.mID, tColor);
	return true;
}

static bool applyStoryEditorAnimationNumber(const StoryEditorAnimationTarget& tTarget, int tAnimation)
{
	StoryInstance* instance;
	StoryCharacter* character = nullptr;
	if (!getStoryEditorAnimationElement(tTarget, &instance, &character)) return false;
	auto animations = tTarget.mIsCharacter ? &character->mAnimations : &gDolmexicaStoryScreenData.mAnimations;
	if (!hasMugenAnimation(animations, tAnimation)) return true; // target alive, value no longer available: no-op
	const auto oldAnimation = tTarget.mIsCharacter ? getDolmexicaStoryCharacterAnimation(instance, tTarget.mID) : getDolmexicaStoryAnimationAnimation(instance, tTarget.mID);
	tTarget.mIsCharacter ? changeDolmexicaStoryCharacterAnimation(instance, tTarget.mID, tAnimation) : changeDolmexicaStoryAnimation(instance, tTarget.mID, tAnimation);
	markStoryEditorFieldEdited(tTarget.mInstanceKey, tTarget.mIsCharacter ? StoryEditorObjectKind::Character : StoryEditorObjectKind::Animation, tTarget.mID, "anim", "Animation", std::to_string(oldAnimation), std::to_string(tAnimation));
	return true;
}

static StoryText* getStoryEditorText(int tInstanceKey, int tID, StoryInstance** oInstance = nullptr)
{
	const auto instance = getStoryEditorInstance(tInstanceKey);
	if (!instance) return nullptr;
	if (!stl_map_contains(instance->mStoryTexts, tID)) return nullptr;
	if (oInstance) *oInstance = instance;
	return &instance->mStoryTexts[tID];
}

enum class StoryEditorTextVectorField {
	BasePosition,
	TextOffset,
	BackgroundOffset,
	FaceOffset,
	ContinueOffset,
	NameOffset,
};

// bg/face/continue offsets write the struct field directly (keeping the stored, already-scaled z) and then reposition everything through setDolmexicaStoryTextBasePosition, mirroring what the text/name offset setters do
// the public bg/continue offset setters are avoided since they position relative to the text element instead of the base
static bool applyStoryEditorTextVectorField(int tInstanceKey, int tID, StoryEditorTextVectorField tField, const Vector2D& tValue)
{
	StoryInstance* instance;
	const auto e = getStoryEditorText(tInstanceKey, tID, &instance);
	if (!e) return false;
	const char* defKey;
	const char* label;
	Vector2D oldValue(0, 0);
	switch (tField)
	{
	case StoryEditorTextVectorField::BasePosition:
		defKey = "pos"; label = "BasePosition"; oldValue = e->mPosition.xy();
		break;
	case StoryEditorTextVectorField::TextOffset:
		defKey = "text.offset"; label = "TextOffset"; oldValue = e->mTextOffset;
		break;
	case StoryEditorTextVectorField::BackgroundOffset:
		if (!e->mHasBackground) return false;
		defKey = "bg.offset"; label = "BgOffset"; oldValue = e->mBackgroundOffset.xy();
		break;
	case StoryEditorTextVectorField::FaceOffset:
		if (!e->mHasFace) return false;
		defKey = "face.offset"; label = "FaceOffset"; oldValue = e->mFaceOffset.xy();
		break;
	case StoryEditorTextVectorField::ContinueOffset:
		if (!e->mHasContinue) return false;
		defKey = "continue.offset"; label = "ContinueOffset"; oldValue = e->mContinueOffset.xy();
		break;
	case StoryEditorTextVectorField::NameOffset:
		if (!e->mHasName) return false;
		defKey = "name.offset"; label = "NameOffset"; oldValue = e->mNameOffset;
		break;
	default:
		return false;
	}
	switch (tField)
	{
	case StoryEditorTextVectorField::BasePosition:
		setDolmexicaStoryTextBasePosition(instance, tID, tValue);
		break;
	case StoryEditorTextVectorField::TextOffset:
		setDolmexicaStoryTextTextOffset(instance, tID, tValue);
		break;
	case StoryEditorTextVectorField::BackgroundOffset:
		e->mBackgroundOffset = Vector3D(tValue.x, tValue.y, e->mBackgroundOffset.z);
		setDolmexicaStoryTextBasePosition(instance, tID, e->mPosition.xy());
		break;
	case StoryEditorTextVectorField::FaceOffset:
		e->mFaceOffset = Vector3D(tValue.x, tValue.y, e->mFaceOffset.z);
		setDolmexicaStoryTextBasePosition(instance, tID, e->mPosition.xy());
		break;
	case StoryEditorTextVectorField::ContinueOffset:
		e->mContinueOffset = Vector3D(tValue.x, tValue.y, e->mContinueOffset.z);
		setDolmexicaStoryTextBasePosition(instance, tID, e->mPosition.xy());
		break;
	case StoryEditorTextVectorField::NameOffset:
		setDolmexicaStoryTextNameOffset(instance, tID, tValue);
		break;
	default:
		break;
	}
	markStoryEditorFieldEdited(tInstanceKey, StoryEditorObjectKind::Text, tID, defKey, label, formatStoryEditorDefVector2(oldValue), formatStoryEditorDefVector2(tValue));
	return true;
}

static bool applyStoryEditorTextString(int tInstanceKey, int tID, const std::string& tText)
{
	StoryInstance* instance;
	if (!getStoryEditorText(tInstanceKey, tID, &instance)) return false;
	const std::string oldText = getDolmexicaStoryTextText(instance, tID);
	setDolmexicaStoryTextText(instance, tID, tText.c_str());
	markStoryEditorFieldEdited(tInstanceKey, StoryEditorObjectKind::Text, tID, "text", "Text", oldText, tText);
	return true;
}

static bool applyStoryEditorTextNameString(int tInstanceKey, int tID, const std::string& tText)
{
	StoryInstance* instance;
	const auto e = getStoryEditorText(tInstanceKey, tID, &instance);
	if (!e || !e->mHasName) return false;
	const std::string oldText = getDolmexicaStoryTextNameText(instance, tID);
	setDolmexicaStoryTextNameText(instance, tID, tText.c_str());
	markStoryEditorFieldEdited(tInstanceKey, StoryEditorObjectKind::Text, tID, "name", "Name", oldText, tText);
	return true;
}

static bool applyStoryEditorTextWidth(int tInstanceKey, int tID, float tWidth)
{
	const auto e = getStoryEditorText(tInstanceKey, tID);
	if (!e) return false;
	const auto oldWidth = getMugenTextTextBoxWidth(e->mTextID);
	setMugenTextTextBoxWidth(e->mTextID, tWidth);
	if (std::isfinite(tWidth)) markStoryEditorFieldEdited(tInstanceKey, StoryEditorObjectKind::Text, tID, "width", "Width", formatStoryEditorDefNumber(oldWidth), formatStoryEditorDefNumber(tWidth)); // infinite width = key absent from the .def, nothing to write
	return true;
}

static bool applyStoryEditorTextFont(int tInstanceKey, int tID, int tFont)
{
	const auto e = getStoryEditorText(tInstanceKey, tID);
	if (!e) return false;
	if (!hasMugenFont(tFont)) return true; // target alive, font not loaded: no-op
	const auto oldFont = getMugenTextFont(e->mTextID);
	setMugenTextFont(e->mTextID, tFont);
	markStoryEditorFieldEdited(tInstanceKey, StoryEditorObjectKind::Text, tID, "font", "Font", std::to_string(oldFont), std::to_string(tFont)); // bank/alignment components are preserved from the file on save
	return true;
}

static bool applyStoryEditorTextColor(int tInstanceKey, int tID, const Vector3D& tColor)
{
	const auto e = getStoryEditorText(tInstanceKey, tID);
	if (!e) return false;
	setMugenTextColorRGB(e->mTextID, tColor.x, tColor.y, tColor.z);
	return true;
}

static bool applyStoryEditorTextNextState(int tInstanceKey, int tID, int tGoesToNextState, int tNextState)
{
	const auto e = getStoryEditorText(tInstanceKey, tID);
	if (!e) return false;
	const std::string oldValue = e->mGoesToNextState ? std::to_string(e->mNextState) : std::string();
	e->mGoesToNextState = tGoesToNextState;
	e->mNextState = tNextState;
	if (tGoesToNextState) markStoryEditorFieldEdited(tInstanceKey, StoryEditorObjectKind::Text, tID, "nextstate", "NextState", oldValue, std::to_string(tNextState));
	return true;
}

static void updateLockedTextPosition(StoryInstance* tInstance, StoryText* e);

// re-locking reuses the retained character position reference (StoryText is zero-initialized, so it is null until a lock controller ran)
// if the referenced character was removed in the meantime, re-locking dereferences a dangling pointer — same engine-wide caveat as locked texts whose character gets removed
static bool applyStoryEditorTextLockedToCharacter(int tInstanceKey, int tID, int tIsLocked)
{
	StoryInstance* instance;
	const auto e = getStoryEditorText(tInstanceKey, tID, &instance);
	if (!e) return false;
	if (tIsLocked && !e->mLockCharacterPositionReference) return true; // never was locked, nothing to re-lock to: no-op
	e->mIsLockedOnToCharacter = tIsLocked;
	if (tIsLocked) updateLockedTextPosition(instance, e);
	return true;
}

static bool applyStoryEditorTextLockOffset(int tInstanceKey, int tID, const Vector2D& tOffset)
{
	StoryInstance* instance;
	const auto e = getStoryEditorText(tInstanceKey, tID, &instance);
	if (!e || !e->mIsLockedOnToCharacter) return false;
	const auto oldOffset = e->mLockOffset;
	e->mLockOffset = tOffset;
	updateLockedTextPosition(instance, e); // reposition immediately, the per-frame lock update is skipped while playback is held
	markStoryEditorFieldEdited(tInstanceKey, StoryEditorObjectKind::Text, tID, "offset", "LockOffset", formatStoryEditorDefVector2(oldOffset), formatStoryEditorDefVector2(tOffset), 1);
	return true;
}

static bool applyStoryEditorIntegerVariable(int tInstanceKey, int tID, int tValue)
{
	const auto instance = getStoryEditorInstance(tInstanceKey);
	if (!instance || !stl_map_contains(instance->mIntVars, tID)) return false;
	setDolmexicaStoryIntegerVariable(instance, tID, tValue);
	return true;
}

static bool applyStoryEditorFloatVariable(int tInstanceKey, int tID, float tValue)
{
	const auto instance = getStoryEditorInstance(tInstanceKey);
	if (!instance || !stl_map_contains(instance->mFloatVars, tID)) return false;
	setDolmexicaStoryFloatVariable(instance, tID, tValue);
	return true;
}

static bool applyStoryEditorStringVariable(int tInstanceKey, int tID, const std::string& tValue)
{
	const auto instance = getStoryEditorInstance(tInstanceKey);
	if (!instance || !stl_map_contains(instance->mStringVars, tID)) return false;
	setDolmexicaStoryStringVariable(instance, tID, tValue);
	return true;
}

// widget edit-boundary tracking 
// Continuous widgets (drags, inputs) apply live every changed frame, the pre-edit value is captured when the widget activates and one undo command is pushed when it deactivates after an edit
// Only one imgui item can be active at a time, so a single slot suffices

static struct {
	ImGuiID mID = 0;
	float mDoubles[4] = {};
	std::string mString;
	std::string mPendingString;
} gStoryEditorActiveEdit;

// call directly after the widget, tPreEditValues = the value(s) the widget was fed this frame, before any edit
static bool updateStoryEditorEditBoundary(ImGuiID tWidgetID, const float* tPreEditValues, int tValueAmount, float* oOldValues)
{
	if (ImGui::IsItemActivated())
	{
		gStoryEditorActiveEdit.mID = tWidgetID;
		for (int i = 0; i < tValueAmount; i++) gStoryEditorActiveEdit.mDoubles[i] = tPreEditValues[i];
	}
	if (gStoryEditorActiveEdit.mID != tWidgetID) return false;
	if (ImGui::IsItemDeactivatedAfterEdit())
	{
		gStoryEditorActiveEdit.mID = 0;
		for (int i = 0; i < tValueAmount; i++) oOldValues[i] = gStoryEditorActiveEdit.mDoubles[i];
		return true;
	}
	if (ImGui::IsItemDeactivated()) gStoryEditorActiveEdit.mID = 0;
	return false;
}

// commit-based text input: edits are buffered while the item is active and only returned once on commit
static bool imguiStoryEditorInputText(const char* tLabel, const char* tCurrentValue, std::string& oOldValue, std::string& oNewValue)
{
	const auto widgetID = ImGui::GetID(tLabel);
	char buffer[1024];
	strncpy(buffer, tCurrentValue, sizeof(buffer) - 1);
	buffer[sizeof(buffer) - 1] = '\0';
	const auto changed = ImGui::InputText(tLabel, buffer, sizeof(buffer));
	if (ImGui::IsItemActivated())
	{
		gStoryEditorActiveEdit.mID = widgetID;
		gStoryEditorActiveEdit.mString = tCurrentValue;
		gStoryEditorActiveEdit.mPendingString = tCurrentValue;
	}
	if (gStoryEditorActiveEdit.mID != widgetID) return false;
	if (changed) gStoryEditorActiveEdit.mPendingString = buffer;
	if (ImGui::IsItemDeactivatedAfterEdit())
	{
		gStoryEditorActiveEdit.mID = 0;
		oOldValue = gStoryEditorActiveEdit.mString;
		oNewValue = gStoryEditorActiveEdit.mPendingString;
		return oOldValue != oNewValue;
	}
	if (ImGui::IsItemDeactivated()) gStoryEditorActiveEdit.mID = 0;
	return false;
}

template <typename tMapType, typename tElementFunction>
static void imguiStorySortedIntMap(const char* tName, tMapType& tMap, tElementFunction tFunc)
{
	if (!ImGui::TreeNode(tName, "%s (%d)", tName, int(tMap.size()))) return;
	std::vector<int> keys;
	for (const auto& elementPair : tMap) keys.push_back(elementPair.first);
	std::sort(keys.begin(), keys.end());
	for (int key : keys)
	{
		tFunc(key, tMap[key]);
	}
	ImGui::TreePop();
}

static void imguiStoryEditorAnimationDragDouble(const StoryEditorAnimationTarget& tTarget, const std::string& tTargetName, const char* tLabel, StoryEditorAnimationField tField, float tCurrentValue, float tSpeed, float tMin, float tMax)
{
	const auto widgetID = ImGui::GetID(tLabel);
	auto value = float(tCurrentValue);
	if (ImGui::DragFloat(tLabel, &value, tSpeed, tMin, tMax))
	{
		applyStoryEditorAnimationField(tTarget, tField, value);
	}
	float oldValue;
	if (updateStoryEditorEditBoundary(widgetID, &tCurrentValue, 1, &oldValue))
	{
		const float newValue = value;
		pushStoryEditorCommand(
			tTargetName + " " + tLabel + " = " + std::to_string(newValue), "",
			[tTarget, tField, newValue]() { return applyStoryEditorAnimationField(tTarget, tField, newValue); },
			[tTarget, tField, oldValue]() { return applyStoryEditorAnimationField(tTarget, tField, oldValue); });
	}
}

static void imguiStoryEditorAnimationDragVector2(const StoryEditorAnimationTarget& tTarget, const std::string& tTargetName, const char* tLabel, StoryEditorAnimationField tFieldX, StoryEditorAnimationField tFieldY, const Vector2D& tCurrentValue, float tSpeed)
{
	const auto widgetID = ImGui::GetID(tLabel);
	float values[2] = { float(tCurrentValue.x), float(tCurrentValue.y) };
	if (ImGui::DragFloat2(tLabel, values, tSpeed))
	{
		applyStoryEditorAnimationField(tTarget, tFieldX, values[0]);
		applyStoryEditorAnimationField(tTarget, tFieldY, values[1]);
	}
	const float preEditValues[2] = { tCurrentValue.x, tCurrentValue.y };
	float oldValues[2];
	if (updateStoryEditorEditBoundary(widgetID, preEditValues, 2, oldValues))
	{
		const float newX = values[0], newY = values[1], oldX = oldValues[0], oldY = oldValues[1];
		pushStoryEditorCommand(
			tTargetName + " " + tLabel + " = (" + std::to_string(newX) + ", " + std::to_string(newY) + ")", "",
			[tTarget, tFieldX, tFieldY, newX, newY]() { return applyStoryEditorAnimationField(tTarget, tFieldX, newX) && applyStoryEditorAnimationField(tTarget, tFieldY, newY); },
			[tTarget, tFieldX, tFieldY, oldX, oldY]() { return applyStoryEditorAnimationField(tTarget, tFieldX, oldX) && applyStoryEditorAnimationField(tTarget, tFieldY, oldY); });
	}
}

// copy as controller snippet
// Serializes the object's current transform as ready-to-paste [State] blocks (OS clipboard via imgui, prism's clipboardhandler is the on-screen mugen debug clipboard, not the OS one). Values match what the panel shows, params that cannot be recovered from runtime state (bg.spr etc.) are called out in a comment instead of being guessed.

static std::string getStoryEditorSnippetHeader(int tInstanceKey)
{
	const auto instance = getStoryEditorInstance(tInstanceKey);
	const auto state = instance ? getDolmexicaStoryStateNumber(instance) : 0;
	return "[State " + std::to_string(state) + ", editor]\n";
}

static std::string getStoryEditorAnimationControllerSnippet(const StoryEditorAnimationTarget& tTarget)
{
	const auto animation = getStoryEditorAnimationElement(tTarget);
	if (!animation || !animation->mAnimationElement || !isRegisteredMugenAnimation(animation->mAnimationElement)) return "";
	const auto header = getStoryEditorSnippetHeader(tTarget.mInstanceKey);
	const auto idLine = "id = " + std::to_string(tTarget.mID) + "\n";
	const auto pos = getMugenAnimationPosition(animation->mAnimationElement);

	std::string ret = header;
	ret += std::string("type = ") + (tTarget.mIsCharacter ? "CharPosSet" : "AnimPosSet") + "\n";
	ret += "trigger1 = time = 0\n" + idLine;
	ret += "x = " + formatStoryEditorDefNumber(pos.x) + "\n";
	ret += "y = " + formatStoryEditorDefNumber(pos.y) + "\n";
	if (animation->mIsBoundToStage) ret += "; object is stage-bound: x/y above are the raw element position (stage offset baked in), which is what CharPosSet/AnimPosSet write - do NOT paste them into a stage-space param like CreateChar pos with stage = 1\n";
	ret += "\n" + header;
	ret += std::string("type = ") + (tTarget.mIsCharacter ? "CharChangeAnim" : "ChangeAnim") + "\n";
	ret += "trigger1 = time = 0\n" + idLine;
	ret += "anim = " + std::to_string(getMugenAnimationAnimationNumber(animation->mAnimationElement)) + "\n";
	return ret;
}

static std::string getStoryEditorTextControllerSnippet(int tInstanceKey, int tID)
{
	static const float STORY_EDITOR_SNIPPET_INFINITE_WIDTH_THRESHOLD = 100000.0;
	const auto instance = getStoryEditorInstance(tInstanceKey);
	if (!instance || !stl_map_contains(instance->mStoryTexts, tID)) return "";
	auto& e = instance->mStoryTexts[tID];

	std::string ret = getStoryEditorSnippetHeader(tInstanceKey);
	ret += "type = CreateText\n";
	ret += "trigger1 = time = 0\n";
	ret += "id = " + std::to_string(tID) + "\n";
	ret += "pos = " + formatStoryEditorDefVector2(e.mPosition.xy()) + "\n";
	ret += "text.offset = " + formatStoryEditorDefVector2(e.mTextOffset) + "\n";
	if (e.mHasBackground) ret += "bg.offset = " + formatStoryEditorDefVector2(Vector2D(e.mBackgroundOffset.x, e.mBackgroundOffset.y)) + "\n";
	if (e.mHasFace) ret += "face.offset = " + formatStoryEditorDefVector2(Vector2D(e.mFaceOffset.x, e.mFaceOffset.y)) + "\n";
	if (e.mHasContinue) ret += "continue.offset = " + formatStoryEditorDefVector2(Vector2D(e.mContinueOffset.x, e.mContinueOffset.y)) + "\n";
	if (e.mHasName)
	{
		ret += std::string("name = \"") + getMugenTextText(e.mNameID) + "\"\n";
		ret += "name.offset = " + formatStoryEditorDefVector2(e.mNameOffset) + "\n";
	}
	ret += std::string("text = \"") + getMugenTextText(e.mTextID) + "\"\n";
	ret += "font = " + std::to_string(int(getMugenTextFont(e.mTextID))) + "\n";
	const auto width = getMugenTextTextBoxWidth(e.mTextID);
	if (width < STORY_EDITOR_SNIPPET_INFINITE_WIDTH_THRESHOLD) ret += "width = " + formatStoryEditorDefNumber(width) + "\n";
	if (e.mGoesToNextState) ret += "nextstate = " + std::to_string(e.mNextState) + "\n";
	if (e.mHasBackground || e.mHasFace || e.mHasContinue) ret += "; bg/face/continue spr/anim/snd params are not recoverable at runtime - copy them from the source controller\n";
	if (e.mIsLockedOnToCharacter)
	{
		ret += "; text is locked to a character (pos above is the current lock result); reproduce with a LockText:\n";
		ret += "\n" + getStoryEditorSnippetHeader(tInstanceKey);
		ret += "type = LockText\n";
		ret += "trigger1 = time = 0\n";
		ret += "id = " + std::to_string(tID) + "\n";
		ret += "offset = " + formatStoryEditorDefVector2(e.mLockOffset) + "\n";
		ret += "; character = <id of the lock target - not recoverable at runtime>\n";
	}
	return ret;
}

static void imguiStoryEditorCopySnippetButton(const std::string& tSnippet)
{
	if (tSnippet.empty()) return;
	if (ImGui::SmallButton("Copy controller snippet"))
	{
		ImGui::SetClipboardText(tSnippet.c_str());
	}
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
	{
		ImGui::SetTooltip("%s", tSnippet.c_str());
	}
}

static void collectStoryEditorAnimationIDCB(void* tCaller, void* tData)
{
	((std::vector<int>*)tCaller)->push_back(((MugenAnimation*)tData)->mID);
}

static void imguiStoryEditorAnimationPicker(const StoryEditorAnimationTarget& tTarget, const std::string& tTargetName, StoryAnimation& e, StoryCharacter* tCharacter)
{
	const auto currentAnimation = getMugenAnimationAnimationNumber(e.mAnimationElement);
	auto animations = tCharacter ? &tCharacter->mAnimations : &gDolmexicaStoryScreenData.mAnimations;
	std::vector<int> animationIDs;
	int_map_map(&animations->mAnimations, collectStoryEditorAnimationIDCB, &animationIDs);
	std::sort(animationIDs.begin(), animationIDs.end());
	const auto storyAnimations = tCharacter ? &getDebugDolmexicaStoryCharacterAnimations(tCharacter->mName.c_str()) : nullptr;

	char currentLabel[128];
	sprintf(currentLabel, "%d%s", currentAnimation, (storyAnimations && storyAnimations->count(currentAnimation)) ? " [story]" : "");
	if (ImGui::BeginCombo("Animation", currentLabel))
	{
		for (const auto animationID : animationIDs)
		{
			if (animationID < 0) continue; // -1 is the internal fallback animation
			char label[128];
			sprintf(label, "%d%s", animationID, (storyAnimations && storyAnimations->count(animationID)) ? " [story]" : "");
			if (ImGui::Selectable(label, animationID == currentAnimation) && animationID != currentAnimation)
			{
				applyStoryEditorAnimationNumber(tTarget, animationID);
				const auto oldAnimation = currentAnimation;
				pushStoryEditorCommand(
					tTargetName + " Animation " + std::to_string(oldAnimation) + " -> " + std::to_string(animationID), "",
					[tTarget, animationID]() { return applyStoryEditorAnimationNumber(tTarget, animationID); },
					[tTarget, oldAnimation]() { return applyStoryEditorAnimationNumber(tTarget, oldAnimation); });
			}
		}
		ImGui::EndCombo();
	}
	if (!hasMugenAnimation(animations, currentAnimation))
	{
		ImGui::TextColored(ImVec4(1.f, 0.4f, 0.4f, 1.f), "anim %d not in loaded .air", currentAnimation);
	}
	else if (storyAnimations && !storyAnimations->empty() && !storyAnimations->count(currentAnimation))
	{
		ImGui::TextColored(ImVec4(1.f, 1.f, 0.4f, 1.f), "anim %d unusual (never seen in story usage)", currentAnimation);
	}
}

// animation scrubber
// Frame-precise preview of the current animation: tick forward/back, jump to a step or time, per-element pause, loop toggle. Everything here is PREVIEW ONLY, no undo commands, no dirty tracking (nothing maps to a controller param), the anim picker remains the undoable way to change what plays

// advanceMugenAnimationOneTick no-ops on paused elements, so the pause flag is lifted around the advance
// the advance can remove the element (non-looping animation over -> deletion callback), hence the re-check before restoring
static void advanceStoryEditorAnimationElementOneTick(MugenAnimationHandlerElement* tElement)
{
	if (!tElement || !isRegisteredMugenAnimation(tElement)) return;
	const auto wasPaused = tElement->mIsPaused;
	tElement->mIsPaused = 0;
	advanceMugenAnimationOneTick(tElement);
	if (isRegisteredMugenAnimation(tElement)) tElement->mIsPaused = wasPaused;
}

// absolute time jump: restart at the last step starting at or before the target, then tick the remainder
static void setStoryEditorAnimationElementTime(MugenAnimationHandlerElement* tElement, int tTime)
{
	static const int STORY_EDITOR_SCRUBBER_ADVANCE_GUARD = 10000;
	if (!tElement || !isRegisteredMugenAnimation(tElement)) return;
	tTime = (std::max)(0, tTime);
	const auto stepAmount = getMugenAnimationAnimationStepAmount(tElement);
	int step = 1;
	for (int i = 2; i <= stepAmount; i++)
	{
		if (getMugenAnimationTimeWhenStepStarts(tElement, i) <= tTime) step = i;
		else break;
	}
	changeMugenAnimationWithStartStep(tElement, tElement->mAnimation, step);
	int guard = 0;
	while (isRegisteredMugenAnimation(tElement) && getMugenAnimationTime(tElement) < tTime && guard++ < STORY_EDITOR_SCRUBBER_ADVANCE_GUARD)
	{
		advanceStoryEditorAnimationElementOneTick(tElement);
	}
}

static void setStoryEditorAnimationElementStep(MugenAnimationHandlerElement* tElement, int tStep)
{
	if (!tElement || !isRegisteredMugenAnimation(tElement)) return;
	changeMugenAnimationWithStartStep(tElement, tElement->mAnimation, tStep);
}

// all scrubber operations mirror onto the shadow element so it stays in sync
static void imguiStoryEditorAnimationScrubber(StoryAnimation& e)
{
	if (!ImGui::TreeNode("Scrubber")) return;
	const auto element = e.mAnimationElement;
	const auto forBothElements = [&e](void (*tFunc)(MugenAnimationHandlerElement*, int), int tValue) {
		tFunc(e.mAnimationElement, tValue);
		if (e.mHasShadow) tFunc(e.mShadowAnimationElement, tValue);
	};

	const auto step = getMugenAnimationAnimationStep(element); // 0-based internally
	const auto stepAmount = getMugenAnimationAnimationStepAmount(element);
	const auto time = getMugenAnimationTime(element);
	const auto isInfinite = isMugenAnimationDurationInfinite(element);
	const auto duration = getMugenAnimationDuration(element);
	ImGui::Text("Step %d/%d (step duration %d)", step + 1, stepAmount, getMugenAnimationAnimationStepDuration(element));
	if (!isStoryEditorHoldingPlayback()) ImGui::TextDisabled("(playback running: scrubbed frames advance again immediately - pause first)");

	if (ImGui::SmallButton("|<"))
	{
		forBothElements([](MugenAnimationHandlerElement* tElement, int tTime) { setStoryEditorAnimationElementTime(tElement, tTime); }, 0);
	}
	ImGui::SameLine();
	ImGui::BeginDisabled(time <= 0);
	if (ImGui::SmallButton("< Tick"))
	{
		forBothElements([](MugenAnimationHandlerElement* tElement, int tTime) { setStoryEditorAnimationElementTime(tElement, tTime); }, time - 1);
	}
	ImGui::EndDisabled();
	ImGui::SameLine();
	if (ImGui::SmallButton("Tick >"))
	{
		advanceStoryEditorAnimationElementOneTick(e.mAnimationElement);
		if (e.mHasShadow) advanceStoryEditorAnimationElementOneTick(e.mShadowAnimationElement);
	}
	ImGui::SameLine();
	if (isInfinite) ImGui::Text("Time %d (infinite)", time);
	else ImGui::Text("Time %d/%d", time, duration);

	{
		auto stepValue = step + 1;
		if (stepAmount > 1 && ImGui::SliderInt("Step##ScrubberStep", &stepValue, 1, stepAmount) && stepValue != step + 1)
		{
			forBothElements([](MugenAnimationHandlerElement* tElement, int tStep) { setStoryEditorAnimationElementStep(tElement, tStep); }, stepValue);
		}
	}
	if (!isInfinite && duration > 1)
	{
		auto timeValue = time;
		if (ImGui::SliderInt("Time##ScrubberTime", &timeValue, 0, duration - 1) && timeValue != time)
		{
			forBothElements([](MugenAnimationHandlerElement* tElement, int tTime) { setStoryEditorAnimationElementTime(tElement, tTime); }, timeValue);
		}
	}

	// the element may have been removed by a scrub past the end of a non-looping animation
	if (!isRegisteredMugenAnimation(e.mAnimationElement))
	{
		ImGui::TextColored(ImVec4(1.f, 0.4f, 0.4f, 1.f), "animation ended and was removed");
		ImGui::TreePop();
		return;
	}

	{
		auto isPaused = element->mIsPaused != 0;
		if (ImGui::Checkbox("Pause this animation", &isPaused))
		{
			isPaused ? pauseMugenAnimation(element) : unpauseMugenAnimation(element);
			if (e.mHasShadow) isPaused ? pauseMugenAnimation(e.mShadowAnimationElement) : unpauseMugenAnimation(e.mShadowAnimationElement);
		}
	}
	{
		// direct field flip: prism only exposes setMugenAnimationNoLoop, which also registers the deletion callback, not what a preview toggle wants
		auto isLooping = getMugenAnimationIsLooping(element) != 0;
		if (ImGui::Checkbox("Looping", &isLooping))
		{
			element->mIsLooping = isLooping;
			if (e.mHasShadow) e.mShadowAnimationElement->mIsLooping = isLooping;
		}
	}
	ImGui::TextDisabled("Preview only: not undoable, not saved. Ticking a non-looping\nanimation past its end removes it (like organic playback would).");
	ImGui::TreePop();
}

// tCharacter is null for plain story animations, the target name is used for undo descriptions
static void imguiSingleStoryAnimationElement(int tInstanceKey, int tIsCharacter, int tID, StoryAnimation& e, StoryCharacter* tCharacter)
{
	const StoryEditorAnimationTarget target = { tInstanceKey, tIsCharacter, tID };
	const auto targetName = getStoryInstanceDisplayName(getStoryEditorInstance(tInstanceKey)) + (tIsCharacter ? " Character " : " Animation ") + std::to_string(tID) + getStoryEditorIDNameSuffix(tInstanceKey, tID);
	imguiStoryEditorProvenanceLine(tInstanceKey, tIsCharacter ? StoryEditorObjectKind::Character : StoryEditorObjectKind::Animation, tID);
	if (e.mAnimationElement && isRegisteredMugenAnimation(e.mAnimationElement))
	{
		imguiStoryEditorCopySnippetButton(getStoryEditorAnimationControllerSnippet(target));
		imguiStoryEditorAnimationPicker(target, targetName, e, tCharacter);

		const auto pos = getMugenAnimationPosition(e.mAnimationElement);
		imguiStoryEditorAnimationDragVector2(target, targetName, "Position", StoryEditorAnimationField::PositionX, StoryEditorAnimationField::PositionY, Vector2D(pos.x, pos.y), 1.0f);
		ImGui::Text("PositionZ = %.2f", pos.z);
		const auto scale = getMugenAnimationDrawScale(e.mAnimationElement);
		imguiStoryEditorAnimationDragVector2(target, targetName, "Scale", StoryEditorAnimationField::ScaleX, StoryEditorAnimationField::ScaleY, scale, 0.01f);

		auto isFacingRight = getMugenAnimationIsFacingRight(e.mAnimationElement) != 0;
		if (ImGui::Checkbox("FacingRight", &isFacingRight))
		{
			applyStoryEditorAnimationFacing(target, isFacingRight);
			const auto newFacing = int(isFacingRight);
			pushStoryEditorCommand(
				targetName + " FacingRight = " + std::to_string(newFacing), "",
				[target, newFacing]() { return applyStoryEditorAnimationFacing(target, newFacing); },
				[target, newFacing]() { return applyStoryEditorAnimationFacing(target, !newFacing); });
		}

		imguiStoryEditorAnimationDragDouble(target, targetName, "Angle", StoryEditorAnimationField::AngleDegrees, radiansToDegrees(getMugenAnimationDrawAngle(e.mAnimationElement)), 1.0f, -360.f, 360.f);
		imguiStoryEditorAnimationDragDouble(target, targetName, "Opacity", StoryEditorAnimationField::Opacity, getMugenAnimationTransparency(e.mAnimationElement), 0.01f, 0.f, 1.f);

		const auto widgetID = ImGui::GetID("Color");
		const float currentColor[3] = { getMugenAnimationColorRed(e.mAnimationElement), getMugenAnimationColorGreen(e.mAnimationElement), getMugenAnimationColorBlue(e.mAnimationElement) };
		float colorValues[3] = { float(currentColor[0]), float(currentColor[1]), float(currentColor[2]) };
		if (ImGui::ColorEdit3("Color", colorValues))
		{
			applyStoryEditorAnimationColor(target, Vector3D(colorValues[0], colorValues[1], colorValues[2]));
		}
		float oldColor[3];
		if (updateStoryEditorEditBoundary(widgetID, currentColor, 3, oldColor))
		{
			const Vector3D newValue(colorValues[0], colorValues[1], colorValues[2]);
			const Vector3D oldValue(oldColor[0], oldColor[1], oldColor[2]);
			pushStoryEditorCommand(
				targetName + " Color", "",
				[target, newValue]() { return applyStoryEditorAnimationColor(target, newValue); },
				[target, oldValue]() { return applyStoryEditorAnimationColor(target, oldValue); });
		}

		ImGui::Text("Visible = %d", getMugenAnimationVisibility(e.mAnimationElement));
		const auto sprite = getMugenAnimationSprite(e.mAnimationElement);
		ImGui::Text("Sprite = (%d, %d)", sprite.x, sprite.y);
		ImGui::Text("AnimTime = %d / %d%s", getMugenAnimationTime(e.mAnimationElement), getMugenAnimationDuration(e.mAnimationElement), isMugenAnimationDurationInfinite(e.mAnimationElement) ? " (infinite)" : "");
		ImGui::Text("Looping = %d", getMugenAnimationIsLooping(e.mAnimationElement));
		imguiStoryEditorAnimationScrubber(e);
	}
	else
	{
		ImGui::Text("<no registered animation element>");
	}
	ImGui::Text("BoundToStage = %d", e.mIsBoundToStage);
	ImGui::Text("HasShadow = %d", e.mHasShadow);
	if (e.mHasShadow)
	{
		ImGui::Text("ShadowBasePositionY = %.2f", e.mShadowBasePositionY);
	}
	ImGui::Text("IsDeleted = %d", e.mIsDeleted);
}

static void imguiStoryEditorTextDragVector2(int tInstanceKey, int tID, const std::string& tTargetName, const char* tLabel, StoryEditorTextVectorField tField, const Vector2D& tCurrentValue, float tSpeed)
{
	const auto widgetID = ImGui::GetID(tLabel);
	float values[2] = { float(tCurrentValue.x), float(tCurrentValue.y) };
	if (ImGui::DragFloat2(tLabel, values, tSpeed))
	{
		applyStoryEditorTextVectorField(tInstanceKey, tID, tField, Vector2D(values[0], values[1]));
	}
	const float preEditValues[2] = { tCurrentValue.x, tCurrentValue.y };
	float oldValues[2];
	if (updateStoryEditorEditBoundary(widgetID, preEditValues, 2, oldValues))
	{
		const Vector2D newValue(values[0], values[1]);
		const Vector2D oldValue(oldValues[0], oldValues[1]);
		pushStoryEditorCommand(
			tTargetName + " " + tLabel + " = (" + std::to_string(newValue.x) + ", " + std::to_string(newValue.y) + ")", "",
			[tInstanceKey, tID, tField, newValue]() { return applyStoryEditorTextVectorField(tInstanceKey, tID, tField, newValue); },
			[tInstanceKey, tID, tField, oldValue]() { return applyStoryEditorTextVectorField(tInstanceKey, tID, tField, oldValue); });
	}
}

static void imguiStoryEditorTextWidth(int tInstanceKey, int tID, const std::string& tTargetName, StoryText& e)
{
	static const float STORY_EDITOR_INFINITE_WIDTH_THRESHOLD = 100000.0;
	static const float STORY_EDITOR_DEFAULT_LIMITED_WIDTH = 240.0;
	const auto currentWidth = getMugenTextTextBoxWidth(e.mTextID);
	const auto pushWidthCommand = [tInstanceKey, tID, &tTargetName](float tNewWidth, float tOldWidth) {
		pushStoryEditorCommand(
			tTargetName + " TextBoxWidth = " + std::to_string(tNewWidth), "",
			[tInstanceKey, tID, tNewWidth]() { return applyStoryEditorTextWidth(tInstanceKey, tID, tNewWidth); },
			[tInstanceKey, tID, tOldWidth]() { return applyStoryEditorTextWidth(tInstanceKey, tID, tOldWidth); });
	};
	if (currentWidth >= STORY_EDITOR_INFINITE_WIDTH_THRESHOLD)
	{
		ImGui::Text("TextBoxWidth = infinite");
		ImGui::SameLine();
		if (ImGui::SmallButton("Limit"))
		{
			applyStoryEditorTextWidth(tInstanceKey, tID, STORY_EDITOR_DEFAULT_LIMITED_WIDTH);
			pushWidthCommand(STORY_EDITOR_DEFAULT_LIMITED_WIDTH, currentWidth);
		}
	}
	else
	{
		const auto widgetID = ImGui::GetID("TextBoxWidth");
		auto value = float(currentWidth);
		if (ImGui::DragFloat("TextBoxWidth", &value, 1.0f, 1.0f, 10000.0f))
		{
			applyStoryEditorTextWidth(tInstanceKey, tID, value);
		}
		float oldWidth;
		if (updateStoryEditorEditBoundary(widgetID, &currentWidth, 1, &oldWidth))
		{
			pushWidthCommand(value, oldWidth);
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("Make infinite"))
		{
			applyStoryEditorTextWidth(tInstanceKey, tID, INF);
			pushWidthCommand(INF, currentWidth);
		}
	}
}

// layout presets
// Named textbox layouts (base position + offsets + width + font) persisted to debug/storytextlayouts.def, since most scenes reuse a handful of arrangements (bottom bubble, top-left speaker, ...). one undo entry for the whole apply, and the fields become dirty/savable like any panel edit

static std::string getStoryEditorTrimmedString(const std::string& tValue);
static std::vector<std::string> splitStoryEditorDefValueComponents(const std::string& tValue);
static std::string formatStoryEditorDefVector2(const Vector2D& tValue);

struct StoryEditorTextLayoutPreset {
	int mHasPosition = 0;
	Vector2D mPosition = Vector2D(0, 0);
	int mHasTextOffset = 0;
	Vector2D mTextOffset = Vector2D(0, 0);
	int mHasBackgroundOffset = 0;
	Vector2D mBackgroundOffset = Vector2D(0, 0);
	int mHasFaceOffset = 0;
	Vector2D mFaceOffset = Vector2D(0, 0);
	int mHasNameOffset = 0;
	Vector2D mNameOffset = Vector2D(0, 0);
	int mHasContinueOffset = 0;
	Vector2D mContinueOffset = Vector2D(0, 0);
	int mHasWidth = 0;
	int mIsWidthInfinite = 0;
	float mWidth = 0;
	int mHasFont = 0;
	int mFont = 0;
};

static struct {
	int mIsLoaded = 0;
	std::map<std::string, StoryEditorTextLayoutPreset> mPresets; // sorted by name for stable display
	std::string mStatusMessage;
	char mNameBuffer[64] = "";
} gStoryEditorPresetData;

static const char* STORY_EDITOR_PRESET_FILE_PATH = "debug/storytextlayouts.def";

static void loadStoryEditorTextLayoutPresets()
{
	auto& p = gStoryEditorPresetData;
	if (p.mIsLoaded) return;
	p.mIsLoaded = 1;
	p.mPresets.clear();
	if (!isFile(STORY_EDITOR_PRESET_FILE_PATH)) return;
	auto b = fileToBuffer(STORY_EDITOR_PRESET_FILE_PATH);
	const std::string content((const char*)b.mData, size_t(b.mLength));
	freeBuffer(b);

	StoryEditorTextLayoutPreset* currentPreset = nullptr;
	size_t lineStart = 0;
	while (lineStart <= content.size())
	{
		auto lineEnd = content.find('\n', lineStart);
		if (lineEnd == std::string::npos) lineEnd = content.size();
		auto line = content.substr(lineStart, lineEnd - lineStart);
		lineStart = lineEnd + 1;
		if (!line.empty() && line.back() == '\r') line.pop_back();
		const auto trimmed = getStoryEditorTrimmedString(line);
		if (trimmed.empty() || trimmed[0] == ';') continue;

		if (trimmed[0] == '[')
		{
			static const std::string prefix = "[layout ";
			std::string lower = trimmed;
			for (auto& c : lower) c = char(tolower((unsigned char)c));
			if (lower.size() > prefix.size() + 1 && lower.compare(0, prefix.size(), prefix) == 0 && trimmed.back() == ']')
			{
				const auto name = getStoryEditorTrimmedString(trimmed.substr(prefix.size(), trimmed.size() - prefix.size() - 1));
				currentPreset = name.empty() ? nullptr : &p.mPresets[name];
			}
			else
			{
				currentPreset = nullptr;
			}
			continue;
		}
		if (!currentPreset) continue;
		const auto equalsPosition = trimmed.find('=');
		if (equalsPosition == std::string::npos) continue;
		auto key = getStoryEditorTrimmedString(trimmed.substr(0, equalsPosition));
		for (auto& c : key) c = char(tolower((unsigned char)c));
		const auto value = getStoryEditorTrimmedString(trimmed.substr(equalsPosition + 1));
		const auto readVector = [&value](int* oHas, Vector2D* oVector) {
			const auto components = splitStoryEditorDefValueComponents(value);
			if (components.size() < 2) return;
			*oHas = 1;
			*oVector = Vector2D(atof(components[0].c_str()), atof(components[1].c_str()));
		};
		if (key == "pos") readVector(&currentPreset->mHasPosition, &currentPreset->mPosition);
		else if (key == "text.offset") readVector(&currentPreset->mHasTextOffset, &currentPreset->mTextOffset);
		else if (key == "bg.offset") readVector(&currentPreset->mHasBackgroundOffset, &currentPreset->mBackgroundOffset);
		else if (key == "face.offset") readVector(&currentPreset->mHasFaceOffset, &currentPreset->mFaceOffset);
		else if (key == "name.offset") readVector(&currentPreset->mHasNameOffset, &currentPreset->mNameOffset);
		else if (key == "continue.offset") readVector(&currentPreset->mHasContinueOffset, &currentPreset->mContinueOffset);
		else if (key == "width")
		{
			currentPreset->mHasWidth = 1;
			currentPreset->mIsWidthInfinite = (value == "infinite");
			if (!currentPreset->mIsWidthInfinite) currentPreset->mWidth = atof(value.c_str());
		}
		else if (key == "font")
		{
			currentPreset->mHasFont = 1;
			currentPreset->mFont = atoi(value.c_str());
		}
	}
}

static void saveStoryEditorTextLayoutPresets()
{
	std::stringstream ss;
	ss << "; story editor text layout presets (edit freely, the editor rewrites this file on save/delete)\n\n";
	for (const auto& presetPair : gStoryEditorPresetData.mPresets)
	{
		const auto& preset = presetPair.second;
		ss << "[Layout " << presetPair.first << "]\n";
		if (preset.mHasPosition) ss << "pos = " << formatStoryEditorDefVector2(preset.mPosition) << "\n";
		if (preset.mHasTextOffset) ss << "text.offset = " << formatStoryEditorDefVector2(preset.mTextOffset) << "\n";
		if (preset.mHasBackgroundOffset) ss << "bg.offset = " << formatStoryEditorDefVector2(preset.mBackgroundOffset) << "\n";
		if (preset.mHasFaceOffset) ss << "face.offset = " << formatStoryEditorDefVector2(preset.mFaceOffset) << "\n";
		if (preset.mHasNameOffset) ss << "name.offset = " << formatStoryEditorDefVector2(preset.mNameOffset) << "\n";
		if (preset.mHasContinueOffset) ss << "continue.offset = " << formatStoryEditorDefVector2(preset.mContinueOffset) << "\n";
		if (preset.mHasWidth) ss << "width = " << (preset.mIsWidthInfinite ? std::string("infinite") : formatStoryEditorDefNumber(preset.mWidth)) << "\n";
		if (preset.mHasFont) ss << "font = " << preset.mFont << "\n";
		ss << "\n";
	}
	createDirectory("debug");
	bufferToFile(STORY_EDITOR_PRESET_FILE_PATH, makeBuffer((void*)ss.str().c_str(), uint32_t(ss.str().size())));
}

static StoryEditorTextLayoutPreset captureStoryEditorTextLayoutPreset(StoryText& e)
{
	static const float STORY_EDITOR_PRESET_INFINITE_WIDTH_THRESHOLD = 100000.0;
	StoryEditorTextLayoutPreset preset;
	preset.mHasPosition = 1;
	preset.mPosition = e.mPosition.xy();
	preset.mHasTextOffset = 1;
	preset.mTextOffset = e.mTextOffset;
	if (e.mHasBackground)
	{
		preset.mHasBackgroundOffset = 1;
		preset.mBackgroundOffset = Vector2D(e.mBackgroundOffset.x, e.mBackgroundOffset.y);
	}
	if (e.mHasFace)
	{
		preset.mHasFaceOffset = 1;
		preset.mFaceOffset = Vector2D(e.mFaceOffset.x, e.mFaceOffset.y);
	}
	if (e.mHasName)
	{
		preset.mHasNameOffset = 1;
		preset.mNameOffset = e.mNameOffset;
	}
	if (e.mHasContinue)
	{
		preset.mHasContinueOffset = 1;
		preset.mContinueOffset = Vector2D(e.mContinueOffset.x, e.mContinueOffset.y);
	}
	preset.mHasWidth = 1;
	const auto width = getMugenTextTextBoxWidth(e.mTextID);
	preset.mIsWidthInfinite = width >= STORY_EDITOR_PRESET_INFINITE_WIDTH_THRESHOLD;
	if (!preset.mIsWidthInfinite) preset.mWidth = width;
	preset.mHasFont = 1;
	preset.mFont = int(getMugenTextFont(e.mTextID));
	return preset;
}

// applies every preset field the text supports (live + dirty tracking), then pushes ONE undo command for the whole apply
static void applyStoryEditorTextLayoutPreset(int tInstanceKey, int tID, const std::string& tTargetName, const std::string& tPresetName)
{
	auto& p = gStoryEditorPresetData;
	const auto presetIt = p.mPresets.find(tPresetName);
	if (presetIt == p.mPresets.end()) return;
	const auto& preset = presetIt->second;
	StoryInstance* instance;
	const auto e = getStoryEditorText(tInstanceKey, tID, &instance);
	if (!e) return;

	struct FieldOperation {
		std::function<bool()> mApply;
		std::function<bool()> mRevert;
	};
	std::vector<FieldOperation> operations;
	const auto addVectorOperation = [&operations, tInstanceKey, tID](StoryEditorTextVectorField tField, const Vector2D& tOldValue, const Vector2D& tNewValue) {
		operations.push_back({
			[tInstanceKey, tID, tField, tNewValue]() { return applyStoryEditorTextVectorField(tInstanceKey, tID, tField, tNewValue); },
			[tInstanceKey, tID, tField, tOldValue]() { return applyStoryEditorTextVectorField(tInstanceKey, tID, tField, tOldValue); } });
	};

	auto skippedPositionForLock = false;
	if (preset.mHasPosition)
	{
		if (e->mIsLockedOnToCharacter) skippedPositionForLock = true; // the lock overwrites base position every frame, LockOffset stays authoritative
		else addVectorOperation(StoryEditorTextVectorField::BasePosition, e->mPosition.xy(), preset.mPosition);
	}
	if (preset.mHasTextOffset) addVectorOperation(StoryEditorTextVectorField::TextOffset, e->mTextOffset, preset.mTextOffset);
	if (preset.mHasBackgroundOffset && e->mHasBackground) addVectorOperation(StoryEditorTextVectorField::BackgroundOffset, Vector2D(e->mBackgroundOffset.x, e->mBackgroundOffset.y), preset.mBackgroundOffset);
	if (preset.mHasFaceOffset && e->mHasFace) addVectorOperation(StoryEditorTextVectorField::FaceOffset, Vector2D(e->mFaceOffset.x, e->mFaceOffset.y), preset.mFaceOffset);
	if (preset.mHasNameOffset && e->mHasName) addVectorOperation(StoryEditorTextVectorField::NameOffset, e->mNameOffset, preset.mNameOffset);
	if (preset.mHasContinueOffset && e->mHasContinue) addVectorOperation(StoryEditorTextVectorField::ContinueOffset, Vector2D(e->mContinueOffset.x, e->mContinueOffset.y), preset.mContinueOffset);
	if (preset.mHasWidth)
	{
		const auto oldWidth = getMugenTextTextBoxWidth(e->mTextID);
		const auto newWidth = preset.mIsWidthInfinite ? INF : preset.mWidth;
		operations.push_back({
			[tInstanceKey, tID, newWidth]() { return applyStoryEditorTextWidth(tInstanceKey, tID, newWidth); },
			[tInstanceKey, tID, oldWidth]() { return applyStoryEditorTextWidth(tInstanceKey, tID, oldWidth); } });
	}
	if (preset.mHasFont)
	{
		const auto oldFont = int(getMugenTextFont(e->mTextID));
		const auto newFont = preset.mFont;
		operations.push_back({
			[tInstanceKey, tID, newFont]() { return applyStoryEditorTextFont(tInstanceKey, tID, newFont); },
			[tInstanceKey, tID, oldFont]() { return applyStoryEditorTextFont(tInstanceKey, tID, oldFont); } });
	}
	if (operations.empty())
	{
		p.mStatusMessage = "Preset '" + tPresetName + "' has no fields this text supports";
		return;
	}

	for (const auto& operation : operations) operation.mApply();
	pushStoryEditorCommand(
		tTargetName + " layout preset '" + tPresetName + "'", "",
		[operations]() { auto ok = true; for (const auto& operation : operations) ok = operation.mApply() && ok; return ok; },
		[operations]() { auto ok = true; for (const auto& operation : operations) ok = operation.mRevert() && ok; return ok; });
	p.mStatusMessage = "Applied '" + tPresetName + "' (" + std::to_string(operations.size()) + " field(s), one undo entry)" + (skippedPositionForLock ? "; position skipped (text is locked to a character)" : "");
}

static void imguiStoryEditorTextLayoutPresets(int tInstanceKey, int tID, const std::string& tTargetName, StoryText& e)
{
	if (!ImGui::TreeNode("Layout presets")) return;
	loadStoryEditorTextLayoutPresets();
	auto& p = gStoryEditorPresetData;

	if (p.mPresets.empty()) ImGui::TextDisabled("No presets saved yet.");
	std::string pendingDelete;
	for (const auto& presetPair : p.mPresets)
	{
		ImGui::PushID(presetPair.first.c_str());
		ImGui::Text("%s", presetPair.first.c_str());
		ImGui::SameLine();
		if (ImGui::SmallButton("Apply")) applyStoryEditorTextLayoutPreset(tInstanceKey, tID, tTargetName, presetPair.first);
		ImGui::SameLine();
		if (ImGui::SmallButton("Delete")) pendingDelete = presetPair.first;
		ImGui::PopID();
	}
	if (!pendingDelete.empty())
	{
		p.mPresets.erase(pendingDelete);
		saveStoryEditorTextLayoutPresets();
		p.mStatusMessage = "Deleted '" + pendingDelete + "'";
	}

	ImGui::SetNextItemWidth(150.f);
	ImGui::InputText("##PresetName", p.mNameBuffer, sizeof(p.mNameBuffer));
	ImGui::SameLine();
	const auto name = getStoryEditorTrimmedString(p.mNameBuffer);
	ImGui::BeginDisabled(name.empty());
	if (ImGui::SmallButton("Save this layout as"))
	{
		const auto isOverwrite = p.mPresets.count(name) != 0;
		p.mPresets[name] = captureStoryEditorTextLayoutPreset(e);
		saveStoryEditorTextLayoutPresets();
		p.mStatusMessage = std::string(isOverwrite ? "Overwrote '" : "Saved '") + name + "' -> " + STORY_EDITOR_PRESET_FILE_PATH;
	}
	ImGui::EndDisabled();
	ImGui::SameLine();
	if (ImGui::SmallButton("Reload file"))
	{
		p.mIsLoaded = 0;
		loadStoryEditorTextLayoutPresets();
		p.mStatusMessage = std::string("Reloaded ") + STORY_EDITOR_PRESET_FILE_PATH;
	}
	if (!p.mStatusMessage.empty()) ImGui::TextWrapped("%s", p.mStatusMessage.c_str());
	ImGui::TextDisabled("Stores base position, offsets, width, font. Applying is one undo entry;\nfields the text lacks (no bg/face/name/continue) are skipped.");
	ImGui::TreePop();
}

static void imguiSingleStoryText(int tInstanceKey, int tID, StoryText& e, bool tShowInstanceName = false)
{
	const auto nameSuffix = getStoryEditorIDNameSuffix(tInstanceKey, tID);
	const auto targetName = getStoryInstanceDisplayName(getStoryEditorInstance(tInstanceKey)) + " Text " + std::to_string(tID) + nameSuffix;
	if (!ImGui::TreeNode(&e, "%s", tShowInstanceName ? targetName.c_str() : ("Text " + std::to_string(tID) + nameSuffix).c_str())) return;

	imguiStoryEditorProvenanceLine(tInstanceKey, StoryEditorObjectKind::Text, tID);
	imguiStoryEditorCopySnippetButton(getStoryEditorTextControllerSnippet(tInstanceKey, tID));

	// a locked text's base position is overwritten by the lock every frame, LockOffset is the handle for moving it
	ImGui::BeginDisabled(e.mIsLockedOnToCharacter != 0);
	imguiStoryEditorTextDragVector2(tInstanceKey, tID, targetName, "BasePosition", StoryEditorTextVectorField::BasePosition, e.mPosition.xy(), 1.0f);
	ImGui::EndDisabled();
	if (e.mIsLockedOnToCharacter)
	{
		ImGui::SameLine();
		ImGui::TextDisabled("(locked, edit LockOffset)");
	}
	ImGui::Text("PositionZ = %.2f", e.mPosition.z);
	imguiStoryEditorTextDragVector2(tInstanceKey, tID, targetName, "TextOffset", StoryEditorTextVectorField::TextOffset, e.mTextOffset, 1.0f);
	if (e.mHasBackground)
	{
		imguiStoryEditorTextDragVector2(tInstanceKey, tID, targetName, "BackgroundOffset", StoryEditorTextVectorField::BackgroundOffset, Vector2D(e.mBackgroundOffset.x, e.mBackgroundOffset.y), 1.0f);
	}
	if (e.mHasFace)
	{
		imguiStoryEditorTextDragVector2(tInstanceKey, tID, targetName, "FaceOffset", StoryEditorTextVectorField::FaceOffset, Vector2D(e.mFaceOffset.x, e.mFaceOffset.y), 1.0f);
	}
	if (e.mHasName)
	{
		imguiStoryEditorTextDragVector2(tInstanceKey, tID, targetName, "NameOffset", StoryEditorTextVectorField::NameOffset, e.mNameOffset, 1.0f);
		std::string oldValue, newValue;
		if (imguiStoryEditorInputText("NameText", getMugenTextText(e.mNameID), oldValue, newValue))
		{
			applyStoryEditorTextNameString(tInstanceKey, tID, newValue);
			pushStoryEditorCommand(
				targetName + " NameText = " + newValue, "",
				[tInstanceKey, tID, newValue]() { return applyStoryEditorTextNameString(tInstanceKey, tID, newValue); },
				[tInstanceKey, tID, oldValue]() { return applyStoryEditorTextNameString(tInstanceKey, tID, oldValue); });
		}
	}
	if (e.mHasContinue)
	{
		imguiStoryEditorTextDragVector2(tInstanceKey, tID, targetName, "ContinueOffset", StoryEditorTextVectorField::ContinueOffset, Vector2D(e.mContinueOffset.x, e.mContinueOffset.y), 1.0f);
	}

	{
		std::string oldValue, newValue;
		if (imguiStoryEditorInputText("Text", getMugenTextText(e.mTextID), oldValue, newValue))
		{
			applyStoryEditorTextString(tInstanceKey, tID, newValue);
			pushStoryEditorCommand(
				targetName + " Text = " + newValue, "",
				[tInstanceKey, tID, newValue]() { return applyStoryEditorTextString(tInstanceKey, tID, newValue); },
				[tInstanceKey, tID, oldValue]() { return applyStoryEditorTextString(tInstanceKey, tID, oldValue); });
		}
	}

	imguiStoryEditorTextWidth(tInstanceKey, tID, targetName, e);

	{
		// live-applied so the +/- buttons work, the applier no-ops on fonts that are not loaded
		const auto widgetID = ImGui::GetID("Font");
		const float currentFont = getMugenTextFont(e.mTextID);
		auto value = int(currentFont);
		if (ImGui::InputInt("Font", &value))
		{
			applyStoryEditorTextFont(tInstanceKey, tID, value);
		}
		float oldFont;
		if (updateStoryEditorEditBoundary(widgetID, &currentFont, 1, &oldFont))
		{
			const auto newFont = getMugenTextFont(e.mTextID); // last valid applied font
			const auto oldFontInt = int(oldFont);
			pushStoryEditorCommand(
				targetName + " Font = " + std::to_string(newFont), "",
				[tInstanceKey, tID, newFont]() { return applyStoryEditorTextFont(tInstanceKey, tID, newFont); },
				[tInstanceKey, tID, oldFontInt]() { return applyStoryEditorTextFont(tInstanceKey, tID, oldFontInt); });
		}
	}

	{
		const auto widgetID = ImGui::GetID("TextColor");
		const auto color = getMugenTextColor(e.mTextID);
		const float currentColor[3] = { color.x, color.y, color.z };
		float colorValues[3] = { float(color.x), float(color.y), float(color.z) };
		if (ImGui::ColorEdit3("TextColor", colorValues))
		{
			applyStoryEditorTextColor(tInstanceKey, tID, Vector3D(colorValues[0], colorValues[1], colorValues[2]));
		}
		float oldColor[3];
		if (updateStoryEditorEditBoundary(widgetID, currentColor, 3, oldColor))
		{
			const Vector3D newValue(colorValues[0], colorValues[1], colorValues[2]);
			const Vector3D oldValue(oldColor[0], oldColor[1], oldColor[2]);
			pushStoryEditorCommand(
				targetName + " TextColor", "",
				[tInstanceKey, tID, newValue]() { return applyStoryEditorTextColor(tInstanceKey, tID, newValue); },
				[tInstanceKey, tID, oldValue]() { return applyStoryEditorTextColor(tInstanceKey, tID, oldValue); });
		}
	}

	ImGui::Text("DisplayedText = %s", getMugenTextDisplayedText(e.mTextID));
	ImGui::Text("BuiltUp = %d", isMugenTextBuiltUp(e.mTextID));
	if (e.mHasTextSound)
	{
		ImGui::Text("TextSound = (%d, %d), frequency %d", e.mTextSound.x, e.mTextSound.y, e.mTextSoundFrequency);
	}
	if (e.mHasContinueSound)
	{
		ImGui::Text("ContinueSound = (%d, %d)", e.mContinueSound.x, e.mContinueSound.y);
	}
	{
		ImGui::Text("GoesToNextState = %d", e.mGoesToNextState);
		const auto widgetID = ImGui::GetID("NextState");
		const float preEditValues[2] = { float(e.mGoesToNextState), float(e.mNextState) };
		auto value = e.mNextState;
		if (ImGui::InputInt("NextState", &value))
		{
			applyStoryEditorTextNextState(tInstanceKey, tID, 1, value); // editing implies GoesToNextState, matching the ChangeText controller
		}
		float oldValues[2];
		if (updateStoryEditorEditBoundary(widgetID, preEditValues, 2, oldValues))
		{
			const auto newNextState = value;
			const auto oldGoes = int(oldValues[0]), oldNextState = int(oldValues[1]);
			pushStoryEditorCommand(
				targetName + " NextState = " + std::to_string(newNextState), "",
				[tInstanceKey, tID, newNextState]() { return applyStoryEditorTextNextState(tInstanceKey, tID, 1, newNextState); },
				[tInstanceKey, tID, oldGoes, oldNextState]() { return applyStoryEditorTextNextState(tInstanceKey, tID, oldGoes, oldNextState); });
		}
	}
	{
		auto isLocked = e.mIsLockedOnToCharacter != 0;
		ImGui::BeginDisabled(!e.mIsLockedOnToCharacter && !e.mLockCharacterPositionReference); // can only re-lock once a lock target exists
		if (ImGui::Checkbox("LockedToCharacter", &isLocked))
		{
			applyStoryEditorTextLockedToCharacter(tInstanceKey, tID, isLocked);
			const auto newLocked = int(isLocked);
			pushStoryEditorCommand(
				targetName + " LockedToCharacter = " + std::to_string(newLocked), "",
				[tInstanceKey, tID, newLocked]() { return applyStoryEditorTextLockedToCharacter(tInstanceKey, tID, newLocked); },
				[tInstanceKey, tID, newLocked]() { return applyStoryEditorTextLockedToCharacter(tInstanceKey, tID, !newLocked); });
		}
		ImGui::EndDisabled();
	}
	if (e.mIsLockedOnToCharacter)
	{
		const auto widgetID = ImGui::GetID("LockOffset");
		float values[2] = { float(e.mLockOffset.x), float(e.mLockOffset.y) };
		const float preEditValues[2] = { e.mLockOffset.x, e.mLockOffset.y };
		if (ImGui::DragFloat2("LockOffset", values, 1.0f))
		{
			applyStoryEditorTextLockOffset(tInstanceKey, tID, Vector2D(values[0], values[1]));
		}
		float oldValues[2];
		if (updateStoryEditorEditBoundary(widgetID, preEditValues, 2, oldValues))
		{
			const Vector2D newValue(values[0], values[1]);
			const Vector2D oldValue(oldValues[0], oldValues[1]);
			pushStoryEditorCommand(
				targetName + " LockOffset = (" + std::to_string(newValue.x) + ", " + std::to_string(newValue.y) + ")", "",
				[tInstanceKey, tID, newValue]() { return applyStoryEditorTextLockOffset(tInstanceKey, tID, newValue); },
				[tInstanceKey, tID, oldValue]() { return applyStoryEditorTextLockOffset(tInstanceKey, tID, oldValue); });
		}
		ImGui::Text("LockCharacterBoundToStage = %d", e.mLockCharacterIsBoundToStage);
	}
	imguiStoryEditorTextLayoutPresets(tInstanceKey, tID, targetName, e);
	ImGui::Text("HasFinished = %d", e.mHasFinished);
	ImGui::Text("IsDisabled = %d", e.mIsDisabled);
	ImGui::TreePop();
}

static void imguiSingleStoryInstance(int tInstanceKey, StoryInstance& e)
{
	const auto displayName = getStoryInstanceDisplayName(&e);
	if (!ImGui::TreeNode(&e, "%s", displayName.c_str())) return;

	ImGui::Text("State = %d", getDolmexicaStoryStateNumber(&e));
	ImGui::Text("TimeInState = %d", getDolmexicaStoryTimeInState(&e));
	ImGui::Text("Parent = %s", getStoryInstanceDisplayName(e.mParent).c_str());
	ImGui::Text("ScheduledForDeletion = %d", e.mIsScheduledForDeletion);

	imguiStorySortedIntMap("Characters", e.mStoryCharacters, [tInstanceKey](int tID, StoryCharacter& tCharacter) {
		if (!ImGui::TreeNode(&tCharacter, "Character %d%s (%s)", tID, getStoryEditorIDNameSuffix(tInstanceKey, tID).c_str(), tCharacter.mName.c_str())) return;
		imguiSingleStoryAnimationElement(tInstanceKey, 1, tID, tCharacter.mAnimation, &tCharacter);
		ImGui::TreePop();
	});

	imguiStorySortedIntMap("Animations", e.mStoryAnimations, [tInstanceKey](int tID, StoryAnimation& tAnimation) {
		if (!ImGui::TreeNode(&tAnimation, "Animation %d%s", tID, getStoryEditorIDNameSuffix(tInstanceKey, tID).c_str())) return;
		imguiSingleStoryAnimationElement(tInstanceKey, 0, tID, tAnimation, nullptr);
		ImGui::TreePop();
	});

	imguiStorySortedIntMap("Texts", e.mStoryTexts, [tInstanceKey](int tID, StoryText& tText) {
		imguiSingleStoryText(tInstanceKey, tID, tText);
	});

	const auto instanceName = displayName;
	imguiStorySortedIntMap("IntVars", e.mIntVars, [tInstanceKey, &instanceName](int tID, int& tValue) {
		char label[32];
		sprintf(label, "var(%d)", tID);
		const auto widgetID = ImGui::GetID(label);
		const float currentValue = tValue;
		auto value = tValue;
		if (ImGui::InputInt(label, &value))
		{
			applyStoryEditorIntegerVariable(tInstanceKey, tID, value);
		}
		float oldValue;
		if (updateStoryEditorEditBoundary(widgetID, &currentValue, 1, &oldValue))
		{
			const auto newValue = value;
			const auto oldValueInt = int(oldValue);
			pushStoryEditorCommand(
				instanceName + " " + label + " = " + std::to_string(newValue),
				"i" + std::to_string(tInstanceKey) + "/ivar/" + std::to_string(tID),
				[tInstanceKey, tID, newValue]() { return applyStoryEditorIntegerVariable(tInstanceKey, tID, newValue); },
				[tInstanceKey, tID, oldValueInt]() { return applyStoryEditorIntegerVariable(tInstanceKey, tID, oldValueInt); });
		}
		ImGui::SameLine();
		ImGui::PushID(tID);
		if (ImGui::SmallButton("Watch")) addStoryEditorWatch(tInstanceKey, 0, tID);
		ImGui::PopID();
	});
	imguiStorySortedIntMap("FloatVars", e.mFloatVars, [tInstanceKey, &instanceName](int tID, float& tValue) {
		char label[32];
		sprintf(label, "fvar(%d)", tID);
		const auto widgetID = ImGui::GetID(label);
		const float currentValue = tValue;
		auto value = tValue;
		if (ImGui::InputFloat(label, &value, 1.0f, 10.0f, "%.3f"))
		{
			applyStoryEditorFloatVariable(tInstanceKey, tID, value);
		}
		float oldValue;
		if (updateStoryEditorEditBoundary(widgetID, &currentValue, 1, &oldValue))
		{
			const auto newValue = value;
			pushStoryEditorCommand(
				instanceName + " " + label + " = " + std::to_string(newValue),
				"i" + std::to_string(tInstanceKey) + "/fvar/" + std::to_string(tID),
				[tInstanceKey, tID, newValue]() { return applyStoryEditorFloatVariable(tInstanceKey, tID, newValue); },
				[tInstanceKey, tID, oldValue]() { return applyStoryEditorFloatVariable(tInstanceKey, tID, oldValue); });
		}
		ImGui::SameLine();
		ImGui::PushID(tID);
		if (ImGui::SmallButton("Watch")) addStoryEditorWatch(tInstanceKey, 1, tID);
		ImGui::PopID();
	});
	imguiStorySortedIntMap("StringVars", e.mStringVars, [tInstanceKey, &instanceName](int tID, std::string& tValue) {
		char label[32];
		sprintf(label, "svar(%d)", tID);
		std::string oldValue, newValue;
		if (imguiStoryEditorInputText(label, tValue.c_str(), oldValue, newValue))
		{
			applyStoryEditorStringVariable(tInstanceKey, tID, newValue);
			pushStoryEditorCommand(
				instanceName + " " + label + " = " + newValue, "",
				[tInstanceKey, tID, newValue]() { return applyStoryEditorStringVariable(tInstanceKey, tID, newValue); },
				[tInstanceKey, tID, oldValue]() { return applyStoryEditorStringVariable(tInstanceKey, tID, oldValue); });
		}
		ImGui::SameLine();
		ImGui::PushID(tID);
		if (ImGui::SmallButton("Watch")) addStoryEditorWatch(tInstanceKey, 2, tID);
		ImGui::PopID();
	});

	if (ImGui::TreeNode("TextNames", "TextNames (%d)", int(e.mTextNames.size())))
	{
		for (const auto& textNamePair : e.mTextNames)
		{
			ImGui::Text("%s = %d", textNamePair.first.c_str(), textNamePair.second);
		}
		ImGui::TreePop();
	}

	ImGui::TreePop();
}

// on-screen manipulation

// Runs inside the imgui frame (called from imguiStoryScreen), mouse positions come from imgui IO and are converted between window space and the game's logical screen space. All edits route through the appliers + command stack, a drag coalesces into a single undo entry (old value captured at drag start, one command pushed at release), nudges coalesce per burst via a coalesce key

enum class StoryEditorSceneKind : int {
	Character,
	Animation,
	Text,
};

// which draggable handle is being manipulated, Object is the object itself (characters/animations: position, unlocked texts: base position, locked texts: lock offset), the rest are the selected text's sub-anchors
enum class StoryEditorSceneAnchor : int {
	None,
	Object,
	TextOffset,
	BackgroundOffset,
	FaceOffset,
	NameOffset,
	ContinueOffset,
};

static const char* getStoryEditorSceneAnchorName(StoryEditorSceneAnchor tAnchor)
{
	switch (tAnchor)
	{
	case StoryEditorSceneAnchor::Object: return "Position";
	case StoryEditorSceneAnchor::TextOffset: return "TextOffset";
	case StoryEditorSceneAnchor::BackgroundOffset: return "BackgroundOffset";
	case StoryEditorSceneAnchor::FaceOffset: return "FaceOffset";
	case StoryEditorSceneAnchor::NameOffset: return "NameOffset";
	case StoryEditorSceneAnchor::ContinueOffset: return "ContinueOffset";
	default: return "None";
	}
}

struct StoryEditorSceneTarget {
	int mInstanceKey = 0;
	StoryEditorSceneKind mKind = StoryEditorSceneKind::Character;
	int mID = 0;

	bool operator==(const StoryEditorSceneTarget& tOther) const {
		return mInstanceKey == tOther.mInstanceKey && mKind == tOther.mKind && mID == tOther.mID;
	}
};

struct StoryEditorSceneCandidate {
	StoryEditorSceneTarget mTarget;
	GeoRectangle2D mRectangle; // game screen space
	float mZ = 0.0;
};

static struct {
	bool mIsActive = false;
	bool mDoesShowAllBoxes = false;
	// default on with size 1: the mouse-to-game conversion divides by the window scale, so unsnapped drags produce fractional positions, and the Dreamcast struggles drawing text/sprites at 0.5 offsets. Snapping to integers by default keeps accidental 0.5 LockText offsets / position adds out of saved .defs
	bool mIsGridSnapActive = true;
	int mGridSize = 1;

	bool mHasSelection = false;
	StoryEditorSceneTarget mSelection;

	// click-through cycling for overlapping objects
	bool mHasLastClick = false;
	Vector2D mLastClickPosition;

	// active drag
	StoryEditorSceneAnchor mDragAnchor = StoryEditorSceneAnchor::None;
	Vector2D mDragStartMousePosition;
	Vector2D mDragStartValue;

	// onion skin: ghost of the drag-start situation while dragging, to judge movement distances
	bool mDoesShowOnionSkin = true;
	bool mHasDragGhostRectangle = false;
	GeoRectangle2D mDragGhostRectangle; // game screen space
	bool mHasDragStartHandlePosition = false;
	Vector2D mDragStartHandlePosition; // game screen space
} gStoryEditorSceneData;

static void resetStoryEditorSceneSelection()
{
	gStoryEditorSceneData.mHasSelection = false;
	gStoryEditorSceneData.mHasLastClick = false;
	gStoryEditorSceneData.mDragAnchor = StoryEditorSceneAnchor::None;
}

// game logical screen space (getScreenSize) <-> imgui space. Prism renders the full logical screen into the full game window without letterboxing, so the scale is main-viewport-size / logical-size. Prism enables ImGuiConfigFlags_ViewportsEnable, which makes imgui coordinates (mouse, draw lists) DESKTOP-absolute, the game window's top-left is at GetMainViewport()->Pos, so all conversions have to add/subtract it
static Vector2D getStoryEditorSceneWindowScale()
{
	const auto screenSize = getScreenSize();
	const auto viewportSize = ImGui::GetMainViewport()->Size;
	return Vector2D(viewportSize.x / screenSize.x, viewportSize.y / screenSize.y);
}

static ImVec2 getStoryEditorSceneWindowPosition(const Vector2D& tGamePosition)
{
	const auto scale = getStoryEditorSceneWindowScale();
	const auto viewportPosition = ImGui::GetMainViewport()->Pos;
	return ImVec2(float(viewportPosition.x + tGamePosition.x * scale.x), float(viewportPosition.y + tGamePosition.y * scale.y));
}

static Vector2D getStoryEditorSceneMousePosition()
{
	const auto scale = getStoryEditorSceneWindowScale();
	const auto viewportPosition = ImGui::GetMainViewport()->Pos;
	const auto mouse = ImGui::GetMousePos();
	return Vector2D((mouse.x - viewportPosition.x) / scale.x, (mouse.y - viewportPosition.y) / scale.y);
}

static bool isStoryEditorScenePointInRectangle(const GeoRectangle2D& tRectangle, const Vector2D& tPoint)
{
	return tPoint.x >= tRectangle.mTopLeft.x && tPoint.x <= tRectangle.mBottomRight.x && tPoint.y >= tRectangle.mTopLeft.y && tPoint.y <= tRectangle.mBottomRight.y;
}

// texts are hit-tested via their background element bounds when they have one, otherwise the text block is approximated from the text element (left alignment assumed, line count estimated from the text box width)
static bool getStoryEditorSceneTextRectangle(StoryText& e, GeoRectangle2D* oRectangle)
{
	if (e.mHasBackground && e.mBackgroundAnimationElement && isRegisteredMugenAnimation(e.mBackgroundAnimationElement) && getMugenAnimationScreenBoundingBox(e.mBackgroundAnimationElement, oRectangle)) return true;

	static const float STORY_EDITOR_SCENE_MINIMUM_TEXT_EXTENT = 8.0;
	const auto position = getMugenTextPosition(e.mTextID);
	const auto font = getMugenTextFont(e.mTextID);
	auto width = getMugenTextSizeX(e.mTextID);
	const auto boxWidth = getMugenTextTextBoxWidth(e.mTextID);
	auto lineAmount = 1;
	if (boxWidth > 0 && boxWidth < width)
	{
		lineAmount = int(std::ceil(width / boxWidth));
		width = boxWidth;
	}
	const auto height = float(lineAmount * getMugenFontSizeY(font) + (lineAmount - 1) * getMugenFontSpacingY(font));
	*oRectangle = GeoRectangle2D(position.x, position.y, std::max(width, STORY_EDITOR_SCENE_MINIMUM_TEXT_EXTENT), std::max(height, STORY_EDITOR_SCENE_MINIMUM_TEXT_EXTENT));
	return true;
}

// candidates sorted topmost (highest z) first, so hover/click picks match the visual draw order
static void collectStoryEditorSceneCandidates(std::vector<StoryEditorSceneCandidate>& oCandidates)
{
	for (auto& instancePair : gDolmexicaStoryScreenData.mHelperInstances)
	{
		const auto instanceKey = instancePair.first;
		auto& instance = instancePair.second;
		const auto addAnimationCandidate = [&oCandidates, instanceKey](StoryEditorSceneKind tKind, int tID, StoryAnimation& tAnimation) {
			if (!tAnimation.mAnimationElement || !isRegisteredMugenAnimation(tAnimation.mAnimationElement)) return;
			StoryEditorSceneCandidate candidate;
			if (!getMugenAnimationScreenBoundingBox(tAnimation.mAnimationElement, &candidate.mRectangle)) return;
			candidate.mTarget = { instanceKey, tKind, tID };
			candidate.mZ = getMugenAnimationPosition(tAnimation.mAnimationElement).z;
			oCandidates.push_back(candidate);
		};
		for (auto& characterPair : instance.mStoryCharacters)
		{
			addAnimationCandidate(StoryEditorSceneKind::Character, characterPair.first, characterPair.second.mAnimation);
		}
		for (auto& animationPair : instance.mStoryAnimations)
		{
			addAnimationCandidate(StoryEditorSceneKind::Animation, animationPair.first, animationPair.second);
		}
		for (auto& textPair : instance.mStoryTexts)
		{
			auto& text = textPair.second;
			if (text.mIsDisabled) continue;
			StoryEditorSceneCandidate candidate;
			if (!getStoryEditorSceneTextRectangle(text, &candidate.mRectangle)) continue;
			candidate.mTarget = { instanceKey, StoryEditorSceneKind::Text, textPair.first };
			candidate.mZ = text.mPosition.z;
			oCandidates.push_back(candidate);
		}
	}
	std::stable_sort(oCandidates.begin(), oCandidates.end(), [](const StoryEditorSceneCandidate& a, const StoryEditorSceneCandidate& b) { return a.mZ > b.mZ; });
}

static bool isStoryEditorSceneTargetAlive(const StoryEditorSceneTarget& tTarget)
{
	if (tTarget.mKind == StoryEditorSceneKind::Text)
	{
		const auto e = getStoryEditorText(tTarget.mInstanceKey, tTarget.mID);
		return e && !e->mIsDisabled;
	}
	const StoryEditorAnimationTarget target = { tTarget.mInstanceKey, tTarget.mKind == StoryEditorSceneKind::Character, tTarget.mID };
	return getStoryEditorAnimationElement(target) != nullptr;
}

static std::string getStoryEditorSceneTargetName(const StoryEditorSceneTarget& tTarget)
{
	static const char* kindNames[] = { " Character ", " Animation ", " Text " };
	return getStoryInstanceDisplayName(getStoryEditorInstance(tTarget.mInstanceKey)) + kindNames[int(tTarget.mKind)] + std::to_string(tTarget.mID) + getStoryEditorIDNameSuffix(tTarget.mInstanceKey, tTarget.mID);
}

// current value of a draggable handle, the Object handle of a locked text is its lock offset since the lock overwrites the base position every frame (matching the panel widgets)
static bool getStoryEditorSceneValue(const StoryEditorSceneTarget& tTarget, StoryEditorSceneAnchor tAnchor, Vector2D* oValue)
{
	if (tTarget.mKind != StoryEditorSceneKind::Text)
	{
		if (tAnchor != StoryEditorSceneAnchor::Object) return false;
		const StoryEditorAnimationTarget target = { tTarget.mInstanceKey, tTarget.mKind == StoryEditorSceneKind::Character, tTarget.mID };
		const auto animation = getStoryEditorAnimationElement(target);
		if (!animation) return false;
		*oValue = getMugenAnimationPosition(animation->mAnimationElement).xy();
		return true;
	}
	const auto e = getStoryEditorText(tTarget.mInstanceKey, tTarget.mID);
	if (!e) return false;
	switch (tAnchor)
	{
	case StoryEditorSceneAnchor::Object:
		*oValue = e->mIsLockedOnToCharacter ? e->mLockOffset : e->mPosition.xy();
		return true;
	case StoryEditorSceneAnchor::TextOffset:
		*oValue = e->mTextOffset;
		return true;
	case StoryEditorSceneAnchor::BackgroundOffset:
		if (!e->mHasBackground) return false;
		*oValue = Vector2D(e->mBackgroundOffset.x, e->mBackgroundOffset.y);
		return true;
	case StoryEditorSceneAnchor::FaceOffset:
		if (!e->mHasFace) return false;
		*oValue = Vector2D(e->mFaceOffset.x, e->mFaceOffset.y);
		return true;
	case StoryEditorSceneAnchor::NameOffset:
		if (!e->mHasName) return false;
		*oValue = e->mNameOffset;
		return true;
	case StoryEditorSceneAnchor::ContinueOffset:
		if (!e->mHasContinue) return false;
		*oValue = Vector2D(e->mContinueOffset.x, e->mContinueOffset.y);
		return true;
	default:
		return false;
	}
}

static bool applyStoryEditorSceneValue(const StoryEditorSceneTarget& tTarget, StoryEditorSceneAnchor tAnchor, const Vector2D& tValue)
{
	if (tTarget.mKind != StoryEditorSceneKind::Text)
	{
		if (tAnchor != StoryEditorSceneAnchor::Object) return false;
		const StoryEditorAnimationTarget target = { tTarget.mInstanceKey, tTarget.mKind == StoryEditorSceneKind::Character, tTarget.mID };
		return applyStoryEditorAnimationField(target, StoryEditorAnimationField::PositionX, tValue.x) && applyStoryEditorAnimationField(target, StoryEditorAnimationField::PositionY, tValue.y);
	}
	switch (tAnchor)
	{
	case StoryEditorSceneAnchor::Object:
	{
		const auto e = getStoryEditorText(tTarget.mInstanceKey, tTarget.mID);
		if (!e) return false;
		if (e->mIsLockedOnToCharacter) return applyStoryEditorTextLockOffset(tTarget.mInstanceKey, tTarget.mID, tValue);
		return applyStoryEditorTextVectorField(tTarget.mInstanceKey, tTarget.mID, StoryEditorTextVectorField::BasePosition, tValue);
	}
	case StoryEditorSceneAnchor::TextOffset:
		return applyStoryEditorTextVectorField(tTarget.mInstanceKey, tTarget.mID, StoryEditorTextVectorField::TextOffset, tValue);
	case StoryEditorSceneAnchor::BackgroundOffset:
		return applyStoryEditorTextVectorField(tTarget.mInstanceKey, tTarget.mID, StoryEditorTextVectorField::BackgroundOffset, tValue);
	case StoryEditorSceneAnchor::FaceOffset:
		return applyStoryEditorTextVectorField(tTarget.mInstanceKey, tTarget.mID, StoryEditorTextVectorField::FaceOffset, tValue);
	case StoryEditorSceneAnchor::NameOffset:
		return applyStoryEditorTextVectorField(tTarget.mInstanceKey, tTarget.mID, StoryEditorTextVectorField::NameOffset, tValue);
	case StoryEditorSceneAnchor::ContinueOffset:
		return applyStoryEditorTextVectorField(tTarget.mInstanceKey, tTarget.mID, StoryEditorTextVectorField::ContinueOffset, tValue);
	default:
		return false;
	}
}

// on-screen position of a sub-anchor handle of the selected text (game screen space)
static bool getStoryEditorSceneAnchorScreenPosition(const StoryEditorSceneTarget& tTarget, StoryEditorSceneAnchor tAnchor, Vector2D* oPosition)
{
	if (tTarget.mKind != StoryEditorSceneKind::Text) return false;
	const auto e = getStoryEditorText(tTarget.mInstanceKey, tTarget.mID);
	if (!e) return false;
	const auto base = e->mPosition.xy();
	switch (tAnchor)
	{
	case StoryEditorSceneAnchor::Object:
		*oPosition = base;
		return true;
	case StoryEditorSceneAnchor::TextOffset:
		*oPosition = base + e->mTextOffset;
		return true;
	case StoryEditorSceneAnchor::BackgroundOffset:
		if (!e->mHasBackground) return false;
		*oPosition = base + Vector2D(e->mBackgroundOffset.x, e->mBackgroundOffset.y);
		return true;
	case StoryEditorSceneAnchor::FaceOffset:
		if (!e->mHasFace) return false;
		*oPosition = base + Vector2D(e->mFaceOffset.x, e->mFaceOffset.y);
		return true;
	case StoryEditorSceneAnchor::NameOffset:
		if (!e->mHasName) return false;
		*oPosition = base + e->mNameOffset;
		return true;
	case StoryEditorSceneAnchor::ContinueOffset:
		if (!e->mHasContinue) return false;
		*oPosition = base + Vector2D(e->mContinueOffset.x, e->mContinueOffset.y);
		return true;
	default:
		return false;
	}
}

static const StoryEditorSceneAnchor gStoryEditorSceneTextAnchors[] = {
	StoryEditorSceneAnchor::Object,
	StoryEditorSceneAnchor::TextOffset,
	StoryEditorSceneAnchor::BackgroundOffset,
	StoryEditorSceneAnchor::FaceOffset,
	StoryEditorSceneAnchor::NameOffset,
	StoryEditorSceneAnchor::ContinueOffset,
};

static StoryEditorSceneAnchor getStoryEditorSceneAnchorUnderMouse(const StoryEditorSceneTarget& tTarget, const Vector2D& tMousePosition)
{
	static const float STORY_EDITOR_SCENE_ANCHOR_RADIUS = 4.0;
	auto ret = StoryEditorSceneAnchor::None;
	auto bestDistance = STORY_EDITOR_SCENE_ANCHOR_RADIUS;
	for (const auto anchor : gStoryEditorSceneTextAnchors)
	{
		Vector2D position;
		if (!getStoryEditorSceneAnchorScreenPosition(tTarget, anchor, &position)) continue;
		const auto delta = tMousePosition - position;
		const auto distance = std::max(std::abs(delta.x), std::abs(delta.y));
		if (distance <= bestDistance)
		{
			bestDistance = distance;
			ret = anchor;
		}
	}
	return ret;
}

// onion skin helpers: the selection's current screen rectangle and the on-screen position of the dragged handle
// stage-bound animations report their pre-camera position here, so their ghost line drifts while the camera moves, acceptable for a preview aid
static bool getStoryEditorSceneTargetRectangle(const StoryEditorSceneTarget& tTarget, GeoRectangle2D* oRectangle)
{
	if (tTarget.mKind == StoryEditorSceneKind::Text)
	{
		const auto e = getStoryEditorText(tTarget.mInstanceKey, tTarget.mID);
		return e && getStoryEditorSceneTextRectangle(*e, oRectangle);
	}
	const StoryEditorAnimationTarget target = { tTarget.mInstanceKey, tTarget.mKind == StoryEditorSceneKind::Character, tTarget.mID };
	const auto animation = getStoryEditorAnimationElement(target);
	return animation && getMugenAnimationScreenBoundingBox(animation->mAnimationElement, oRectangle) != 0;
}

static bool getStoryEditorSceneDragHandleScreenPosition(const StoryEditorSceneTarget& tTarget, StoryEditorSceneAnchor tAnchor, Vector2D* oPosition)
{
	if (tTarget.mKind == StoryEditorSceneKind::Text) return getStoryEditorSceneAnchorScreenPosition(tTarget, tAnchor, oPosition);
	if (tAnchor != StoryEditorSceneAnchor::Object) return false;
	const StoryEditorAnimationTarget target = { tTarget.mInstanceKey, tTarget.mKind == StoryEditorSceneKind::Character, tTarget.mID };
	const auto animation = getStoryEditorAnimationElement(target);
	if (!animation) return false;
	*oPosition = getMugenAnimationPosition(animation->mAnimationElement).xy();
	return true;
}

static void startStoryEditorSceneDrag(StoryEditorSceneAnchor tAnchor, const Vector2D& tMousePosition)
{
	auto& s = gStoryEditorSceneData;
	Vector2D value;
	if (!getStoryEditorSceneValue(s.mSelection, tAnchor, &value)) return;
	s.mDragAnchor = tAnchor;
	s.mDragStartMousePosition = tMousePosition;
	s.mDragStartValue = value;
	// onion skin snapshot (only the object itself gets a ghost box, anchor drags just get the start crosshair + delta line)
	s.mHasDragGhostRectangle = (tAnchor == StoryEditorSceneAnchor::Object) && getStoryEditorSceneTargetRectangle(s.mSelection, &s.mDragGhostRectangle);
	s.mHasDragStartHandlePosition = getStoryEditorSceneDragHandleScreenPosition(s.mSelection, tAnchor, &s.mDragStartHandlePosition);
}

static void pushStoryEditorSceneMoveCommand(const StoryEditorSceneTarget& tTarget, StoryEditorSceneAnchor tAnchor, const Vector2D& tOldValue, const Vector2D& tNewValue, const char* tVerb, const std::string& tCoalesceKey)
{
	const auto anchorName = (tTarget.mKind == StoryEditorSceneKind::Text) ? getStoryEditorSceneAnchorName(tAnchor) : "Position";
	pushStoryEditorCommand(
		getStoryEditorSceneTargetName(tTarget) + " " + anchorName + " " + tVerb + " to (" + std::to_string(tNewValue.x) + ", " + std::to_string(tNewValue.y) + ")", tCoalesceKey,
		[tTarget, tAnchor, tNewValue]() { return applyStoryEditorSceneValue(tTarget, tAnchor, tNewValue); },
		[tTarget, tAnchor, tOldValue]() { return applyStoryEditorSceneValue(tTarget, tAnchor, tOldValue); });
}

static void updateStoryEditorSceneMouse(const std::vector<StoryEditorSceneCandidate>& tCandidates, const Vector2D& tMousePosition, StoryEditorSceneAnchor tHoveredAnchor)
{
	auto& s = gStoryEditorSceneData;

	if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && s.mDragAnchor == StoryEditorSceneAnchor::None)
	{
		// sub-anchor handles of the selected text take priority over object picking
		if (tHoveredAnchor != StoryEditorSceneAnchor::None)
		{
			startStoryEditorSceneDrag(tHoveredAnchor, tMousePosition);
			return;
		}

		std::vector<const StoryEditorSceneCandidate*> underMouse;
		for (const auto& candidate : tCandidates)
		{
			if (isStoryEditorScenePointInRectangle(candidate.mRectangle, tMousePosition)) underMouse.push_back(&candidate);
		}
		if (underMouse.empty())
		{
			s.mHasSelection = false;
			s.mHasLastClick = false;
			return;
		}

		// selection rules: clicking ~the same spot again cycles through overlapping candidates, clicking a fresh spot keeps the current selection if it is under the cursor (so overlapped selections stay draggable), otherwise picks the topmost candidate
		static const float STORY_EDITOR_SCENE_SAME_CLICK_DISTANCE = 3.0;
		int selectedIndex = -1;
		if (s.mHasSelection)
		{
			for (int i = 0; i < int(underMouse.size()); i++)
			{
				if (underMouse[i]->mTarget == s.mSelection) { selectedIndex = i; break; }
			}
		}
		const auto lastClickDelta = tMousePosition - s.mLastClickPosition;
		const auto isSameSpot = s.mHasLastClick && std::max(std::abs(lastClickDelta.x), std::abs(lastClickDelta.y)) <= STORY_EDITOR_SCENE_SAME_CLICK_DISTANCE;
		int chosenIndex;
		if (selectedIndex >= 0) chosenIndex = isSameSpot ? ((selectedIndex + 1) % int(underMouse.size())) : selectedIndex;
		else chosenIndex = 0;

		s.mSelection = underMouse[chosenIndex]->mTarget;
		s.mHasSelection = true;
		s.mHasLastClick = true;
		s.mLastClickPosition = tMousePosition;
		startStoryEditorSceneDrag(StoryEditorSceneAnchor::Object, tMousePosition);
	}
}

static void updateStoryEditorSceneDrag(const Vector2D& tMousePosition)
{
	auto& s = gStoryEditorSceneData;
	if (s.mDragAnchor == StoryEditorSceneAnchor::None) return;
	if (!s.mHasSelection)
	{
		s.mDragAnchor = StoryEditorSceneAnchor::None;
		return;
	}

	auto value = s.mDragStartValue + (tMousePosition - s.mDragStartMousePosition);
	if (s.mIsGridSnapActive && s.mGridSize > 0)
	{
		value.x = std::round(value.x / s.mGridSize) * s.mGridSize;
		value.y = std::round(value.y / s.mGridSize) * s.mGridSize;
	}

	if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
	{
		if (!applyStoryEditorSceneValue(s.mSelection, s.mDragAnchor, value)) s.mDragAnchor = StoryEditorSceneAnchor::None; // target gone mid-drag
	}
	else
	{
		// drag ended: one undo entry for the whole drag
		Vector2D newValue;
		if (getStoryEditorSceneValue(s.mSelection, s.mDragAnchor, &newValue) && !(newValue == s.mDragStartValue))
		{
			pushStoryEditorSceneMoveCommand(s.mSelection, s.mDragAnchor, s.mDragStartValue, newValue, "dragged", "");
		}
		s.mDragAnchor = StoryEditorSceneAnchor::None;
	}
}

static void updateStoryEditorSceneKeyboard()
{
	auto& s = gStoryEditorSceneData;
	const auto& io = ImGui::GetIO();
	if (io.WantCaptureKeyboard || io.WantTextInput) return;

	if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
	{
		s.mHasSelection = false;
		s.mDragAnchor = StoryEditorSceneAnchor::None;
	}
	if (!s.mHasSelection || s.mDragAnchor != StoryEditorSceneAnchor::None) return;

	Vector2D delta(0, 0);
	if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) delta.x -= 1;
	if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) delta.x += 1;
	if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) delta.y -= 1;
	if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) delta.y += 1;
	if (delta == Vector2D(0, 0)) return;

	if (io.KeyShift) delta = delta * 10.0;
	Vector2D oldValue;
	if (!getStoryEditorSceneValue(s.mSelection, StoryEditorSceneAnchor::Object, &oldValue)) return;
	const auto newValue = oldValue + delta;
	if (!applyStoryEditorSceneValue(s.mSelection, StoryEditorSceneAnchor::Object, newValue)) return;
	// nudge bursts coalesce into one undo entry (the coalescing keeps the oldest revert)
	const auto coalesceKey = "scene-nudge/" + std::to_string(s.mSelection.mInstanceKey) + "/" + std::to_string(int(s.mSelection.mKind)) + "/" + std::to_string(s.mSelection.mID);
	pushStoryEditorSceneMoveCommand(s.mSelection, StoryEditorSceneAnchor::Object, oldValue, newValue, "nudged", coalesceKey);
}

static void drawStoryEditorSceneLabel(ImDrawList* tDrawList, const ImVec2& tPosition, ImU32 tColor, const char* tText)
{
	tDrawList->AddText(ImVec2(tPosition.x + 1, tPosition.y + 1), IM_COL32(0, 0, 0, 220), tText);
	tDrawList->AddText(tPosition, tColor, tText);
}

static void drawStoryEditorSceneOverlay(const std::vector<StoryEditorSceneCandidate>& tCandidates, const StoryEditorSceneCandidate* tHovered, StoryEditorSceneAnchor tHoveredAnchor)
{
	static const ImU32 STORY_EDITOR_SCENE_SELECTION_COLOR = IM_COL32(255, 220, 60, 255);
	static const ImU32 STORY_EDITOR_SCENE_HOVER_COLOR = IM_COL32(255, 255, 255, 160);
	static const ImU32 STORY_EDITOR_SCENE_ALL_BOXES_COLOR = IM_COL32(255, 255, 255, 60);
	static const ImU32 STORY_EDITOR_SCENE_ANCHOR_COLOR = IM_COL32(80, 220, 255, 255);

	auto& s = gStoryEditorSceneData;
	// the main viewport's background draw list renders above the game but below the imgui windows
	const auto drawList = ImGui::GetBackgroundDrawList(ImGui::GetMainViewport());

	const auto drawCandidateRectangle = [drawList](const StoryEditorSceneCandidate& tCandidate, ImU32 tColor, float tThickness) {
		const auto topLeft = getStoryEditorSceneWindowPosition(tCandidate.mRectangle.mTopLeft);
		const auto bottomRight = getStoryEditorSceneWindowPosition(tCandidate.mRectangle.mBottomRight);
		drawList->AddRect(topLeft, bottomRight, tColor, 0.f, 0, tThickness);
	};

	const StoryEditorSceneCandidate* selected = nullptr;
	for (const auto& candidate : tCandidates)
	{
		if (s.mHasSelection && candidate.mTarget == s.mSelection) selected = &candidate;
		else if (s.mDoesShowAllBoxes) drawCandidateRectangle(candidate, STORY_EDITOR_SCENE_ALL_BOXES_COLOR, 1.f);
	}

	if (tHovered && (!selected || !(tHovered->mTarget == s.mSelection)))
	{
		drawCandidateRectangle(*tHovered, STORY_EDITOR_SCENE_HOVER_COLOR, 1.f);
		const auto labelPosition = getStoryEditorSceneWindowPosition(tHovered->mRectangle.mTopLeft);
		drawStoryEditorSceneLabel(drawList, ImVec2(labelPosition.x, std::max(ImGui::GetMainViewport()->Pos.y, labelPosition.y - 16)), STORY_EDITOR_SCENE_HOVER_COLOR, getStoryEditorSceneTargetName(tHovered->mTarget).c_str());
	}

	if (selected)
	{
		drawCandidateRectangle(*selected, STORY_EDITOR_SCENE_SELECTION_COLOR, 2.f);
		Vector2D value;
		char label[256];
		if (getStoryEditorSceneValue(s.mSelection, StoryEditorSceneAnchor::Object, &value))
		{
			const auto isLockOffset = s.mSelection.mKind == StoryEditorSceneKind::Text && getStoryEditorText(s.mSelection.mInstanceKey, s.mSelection.mID)->mIsLockedOnToCharacter;
			sprintf(label, "%s  %s(%.1f, %.1f)", getStoryEditorSceneTargetName(s.mSelection).c_str(), isLockOffset ? "lock offset " : "", value.x, value.y);
		}
		else
		{
			sprintf(label, "%s", getStoryEditorSceneTargetName(s.mSelection).c_str());
		}
		const auto labelPosition = getStoryEditorSceneWindowPosition(selected->mRectangle.mTopLeft);
		drawStoryEditorSceneLabel(drawList, ImVec2(labelPosition.x, std::max(ImGui::GetMainViewport()->Pos.y, labelPosition.y - 16)), STORY_EDITOR_SCENE_SELECTION_COLOR, label);
	}

	// sub-anchor crosshairs for the selected text
	if (s.mHasSelection && s.mSelection.mKind == StoryEditorSceneKind::Text)
	{
		for (const auto anchor : gStoryEditorSceneTextAnchors)
		{
			Vector2D position;
			if (!getStoryEditorSceneAnchorScreenPosition(s.mSelection, anchor, &position)) continue;
			const auto center = getStoryEditorSceneWindowPosition(position);
			const auto isHighlighted = (s.mDragAnchor == anchor) || (s.mDragAnchor == StoryEditorSceneAnchor::None && tHoveredAnchor == anchor);
			const auto color = isHighlighted ? STORY_EDITOR_SCENE_SELECTION_COLOR : ((anchor == StoryEditorSceneAnchor::Object) ? STORY_EDITOR_SCENE_HOVER_COLOR : STORY_EDITOR_SCENE_ANCHOR_COLOR);
			const auto extent = 5.f;
			drawList->AddLine(ImVec2(center.x - extent, center.y), ImVec2(center.x + extent, center.y), color, isHighlighted ? 2.f : 1.f);
			drawList->AddLine(ImVec2(center.x, center.y - extent), ImVec2(center.x, center.y + extent), color, isHighlighted ? 2.f : 1.f);
			drawStoryEditorSceneLabel(drawList, ImVec2(center.x + 4, center.y + 3), color, (anchor == StoryEditorSceneAnchor::Object) ? "base" : getStoryEditorSceneAnchorName(anchor));
		}
	}

	// onion skin: ghost of the drag-start situation plus a delta line, to judge movement distances
	if (s.mDragAnchor != StoryEditorSceneAnchor::None && s.mDoesShowOnionSkin)
	{
		static const ImU32 STORY_EDITOR_SCENE_ONION_COLOR = IM_COL32(255, 255, 255, 80);
		if (s.mHasDragGhostRectangle)
		{
			drawList->AddRect(getStoryEditorSceneWindowPosition(s.mDragGhostRectangle.mTopLeft), getStoryEditorSceneWindowPosition(s.mDragGhostRectangle.mBottomRight), STORY_EDITOR_SCENE_ONION_COLOR, 0.f, 0, 1.f);
		}
		Vector2D currentHandlePosition;
		if (s.mHasDragStartHandlePosition && getStoryEditorSceneDragHandleScreenPosition(s.mSelection, s.mDragAnchor, &currentHandlePosition))
		{
			const auto start = getStoryEditorSceneWindowPosition(s.mDragStartHandlePosition);
			const auto current = getStoryEditorSceneWindowPosition(currentHandlePosition);
			drawList->AddLine(start, current, STORY_EDITOR_SCENE_ONION_COLOR, 1.f);
			const auto extent = 4.f;
			drawList->AddLine(ImVec2(start.x - extent, start.y), ImVec2(start.x + extent, start.y), STORY_EDITOR_SCENE_ONION_COLOR, 1.f);
			drawList->AddLine(ImVec2(start.x, start.y - extent), ImVec2(start.x, start.y + extent), STORY_EDITOR_SCENE_ONION_COLOR, 1.f);
			Vector2D currentValue;
			if (getStoryEditorSceneValue(s.mSelection, s.mDragAnchor, &currentValue))
			{
				char deltaLabel[64];
				sprintf(deltaLabel, "d(%.0f, %.0f)", currentValue.x - s.mDragStartValue.x, currentValue.y - s.mDragStartValue.y);
				drawStoryEditorSceneLabel(drawList, ImVec2((start.x + current.x) * 0.5f + 6, (start.y + current.y) * 0.5f + 6), STORY_EDITOR_SCENE_ONION_COLOR, deltaLabel);
			}
		}
	}

	// live value readout at the cursor while dragging
	if (s.mDragAnchor != StoryEditorSceneAnchor::None)
	{
		Vector2D value;
		if (getStoryEditorSceneValue(s.mSelection, s.mDragAnchor, &value))
		{
			char label[64];
			sprintf(label, "(%.1f, %.1f)", value.x, value.y);
			const auto mouse = ImGui::GetMousePos();
			drawStoryEditorSceneLabel(drawList, ImVec2(mouse.x + 12, mouse.y + 12), STORY_EDITOR_SCENE_SELECTION_COLOR, label);
		}
	}
}

static void updateStoryEditorScene()
{
	auto& s = gStoryEditorSceneData;
	if (!s.mIsActive) return;
	if (gDolmexicaStoryScreenData.mHelperInstances.empty()) return;

	if (s.mHasSelection && !isStoryEditorSceneTargetAlive(s.mSelection))
	{
		s.mHasSelection = false;
		s.mDragAnchor = StoryEditorSceneAnchor::None;
	}

	std::vector<StoryEditorSceneCandidate> candidates;
	collectStoryEditorSceneCandidates(candidates);

	const auto& io = ImGui::GetIO();
	const auto isMouseUsable = !io.WantCaptureMouse && ImGui::IsMousePosValid();
	const auto mousePosition = ImGui::IsMousePosValid() ? getStoryEditorSceneMousePosition() : Vector2D(0, 0);

	const StoryEditorSceneCandidate* hovered = nullptr;
	auto hoveredAnchor = StoryEditorSceneAnchor::None;
	if (isMouseUsable && s.mDragAnchor == StoryEditorSceneAnchor::None)
	{
		if (s.mHasSelection && s.mSelection.mKind == StoryEditorSceneKind::Text)
		{
			hoveredAnchor = getStoryEditorSceneAnchorUnderMouse(s.mSelection, mousePosition);
		}
		if (hoveredAnchor == StoryEditorSceneAnchor::None)
		{
			for (const auto& candidate : candidates)
			{
				if (isStoryEditorScenePointInRectangle(candidate.mRectangle, mousePosition)) { hovered = &candidate; break; }
			}
		}
	}

	if (isMouseUsable) updateStoryEditorSceneMouse(candidates, mousePosition, hoveredAnchor);
	updateStoryEditorSceneDrag(mousePosition);
	updateStoryEditorSceneKeyboard();
	drawStoryEditorSceneOverlay(candidates, hovered, hoveredAnchor);
}

static void imguiStoryScene()
{
	auto& isWindowShown = gStoryEditorWindowVisibility.mScene;
	imguiPrismAddTab("Story Editor", "Scene", &isWindowShown);
	if (!isWindowShown) return;

	ImGui::Begin("Story Scene", &isWindowShown);
	auto& s = gStoryEditorSceneData;
	ImGui::Checkbox("Enable on-screen editing", &s.mIsActive);
	ImGui::Checkbox("Show all hit boxes", &s.mDoesShowAllBoxes);
	ImGui::Checkbox("Onion skin while dragging", &s.mDoesShowOnionSkin);
	ImGui::Checkbox("Grid snap", &s.mIsGridSnapActive);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(80.f);
	if (ImGui::InputInt("Grid size", &s.mGridSize)) s.mGridSize = std::max(1, s.mGridSize);
	ImGui::TextWrapped("Click to select; click the same spot again to cycle through overlapping objects. Drag objects (and the selected text's anchor crosshairs) to move them. Arrow keys nudge 1px, Shift = 10px. Esc deselects. Ctrl+Z undoes.");
	ImGui::Separator();

	if (s.mHasSelection && isStoryEditorSceneTargetAlive(s.mSelection))
	{
		ImGui::Text("Selected: %s", getStoryEditorSceneTargetName(s.mSelection).c_str());
		ImGui::SameLine();
		if (ImGui::SmallButton("Deselect")) resetStoryEditorSceneSelection();

		ImGui::PushID("SceneSelection");
		if (s.mSelection.mKind == StoryEditorSceneKind::Text)
		{
			const auto text = getStoryEditorText(s.mSelection.mInstanceKey, s.mSelection.mID);
			imguiSingleStoryText(s.mSelection.mInstanceKey, s.mSelection.mID, *text);
		}
		else
		{
			const StoryEditorAnimationTarget target = { s.mSelection.mInstanceKey, s.mSelection.mKind == StoryEditorSceneKind::Character, s.mSelection.mID };
			StoryCharacter* character = nullptr;
			const auto animation = getStoryEditorAnimationElement(target, nullptr, &character);
			if (animation) imguiSingleStoryAnimationElement(s.mSelection.mInstanceKey, target.mIsCharacter, s.mSelection.mID, *animation, character);
		}
		ImGui::PopID();
	}
	else
	{
		ImGui::Text("No selection.");
	}
	ImGui::End();
}

static void imguiStoryInstances()
{
	auto& isWindowShown = gStoryEditorWindowVisibility.mInstances;
	imguiPrismAddTab("Story Editor", "Instances", &isWindowShown);
	if (!isWindowShown) return;

	ImGui::Begin("Story Instances", &isWindowShown);
	ImGui::Text("Instances = %d", int(gDolmexicaStoryScreenData.mHelperInstances.size()));

	// quick access to everything currently on screen without digging through the instance trees
	if (ImGui::CollapsingHeader("Active texts"))
	{
		ImGui::PushID("ActiveTexts");
		auto hasActiveTexts = false;
		for (auto& instancePair : gDolmexicaStoryScreenData.mHelperInstances)
		{
			std::vector<int> textIDs;
			for (const auto& textPair : instancePair.second.mStoryTexts) textIDs.push_back(textPair.first);
			std::sort(textIDs.begin(), textIDs.end());
			for (const auto textID : textIDs)
			{
				auto& text = instancePair.second.mStoryTexts[textID];
				if (text.mIsDisabled) continue;
				hasActiveTexts = true;
				imguiSingleStoryText(instancePair.first, textID, text, true);
			}
		}
		if (!hasActiveTexts) ImGui::Text("No active texts.");
		ImGui::PopID();
	}

	for (auto& instancePair : gDolmexicaStoryScreenData.mHelperInstances)
	{
		imguiSingleStoryInstance(instancePair.first, instancePair.second);
	}
	ImGui::End();
}

static void imguiStoryUndo()
{
	auto& isWindowShown = gStoryEditorWindowVisibility.mUndo;
	imguiPrismAddTab("Story Editor", "Undo", &isWindowShown);
	if (!isWindowShown) return;

	ImGui::Begin("Story Undo", &isWindowShown);
	auto& s = gStoryEditorData;
	ImGui::BeginDisabled(!s.mUndoPosition);
	if (ImGui::Button("Undo (Ctrl+Z)")) undoStoryEditorCommand();
	ImGui::EndDisabled();
	ImGui::SameLine();
	ImGui::BeginDisabled(s.mUndoPosition >= s.mUndoStack.size());
	if (ImGui::Button("Redo (Ctrl+Y)")) redoStoryEditorCommand();
	ImGui::EndDisabled();
	ImGui::SameLine();
	ImGui::BeginDisabled(s.mUndoStack.empty());
	if (ImGui::Button("Clear")) clearStoryEditorUndoStack("manual");
	ImGui::EndDisabled();

	if (!s.mUndoStatusMessage.empty())
	{
		ImGui::TextWrapped("%s", s.mUndoStatusMessage.c_str());
	}

	ImGui::Separator();
	ImGui::Text("History (%d entries, newest first)", int(s.mUndoStack.size()));
	for (int i = int(s.mUndoStack.size()) - 1; i >= 0; i--)
	{
		const auto isApplied = size_t(i) < s.mUndoPosition;
		const auto isNextUndo = size_t(i + 1) == s.mUndoPosition;
		ImGui::Text("%s %s%s", isNextUndo ? ">" : " ", s.mUndoStack[i].mDescription.c_str(), isApplied ? "" : " (undone)");
	}
	ImGui::End();
}

static void imguiStoryPlayback()
{
	auto& isWindowShown = gStoryEditorWindowVisibility.mPlayback;
	imguiPrismAddTab("Story Editor", "Playback", &isWindowShown);
	if (!isWindowShown) return;

	ImGui::Begin("Story Playback", &isWindowShown);
	if (gDolmexicaStoryScreenData.mHelperInstances.empty())
	{
		ImGui::Text("No story instances active.");
		ImGui::End();
		return;
	}
	const auto root = getDolmexicaStoryRootInstance();

	ImGui::Text("Root state = %d, time in state = %d", getDolmexicaStoryStateNumber(root), getDolmexicaStoryTimeInState(root));
	ImGui::Text("Playback: %s", gStoryEditorData.mIsPlaybackPaused ? "PAUSED" : "running");

	if (ImGui::Button(gStoryEditorData.mIsPlaybackPaused ? "Resume" : "Pause"))
	{
		gStoryEditorData.mIsPlaybackPaused = !gStoryEditorData.mIsPlaybackPaused;
		gStoryEditorData.mPendingPlaybackSteps = 0;
	}
	ImGui::SameLine();
	ImGui::BeginDisabled(!gStoryEditorData.mIsPlaybackPaused);
	if (ImGui::Button("Step"))
	{
		gStoryEditorData.mPendingPlaybackSteps = 1;
	}
	ImGui::EndDisabled();

	if (ImGui::SliderFloat("Speed", &gStoryEditorData.mPlaybackSpeed, 0.1f, 4.0f, "%.2fx", ImGuiSliderFlags_Logarithmic))
	{
		setWrapperTimeDilatation(gStoryEditorData.mPlaybackSpeed);
	}
	ImGui::SameLine();
	if (ImGui::Button("1x"))
	{
		gStoryEditorData.mPlaybackSpeed = 1.0f;
		setWrapperTimeDilatation(1.0);
	}

	// same capture path as prism's F11 dev shortcut, grabs the last presented frame, so open imgui windows in the game viewport are included
	if (ImGui::Button("Screenshot"))
	{
		takePrismWrapperScreenshot();
	}
	ImGui::SameLine();
	ImGui::TextDisabled("-> debug/screenshots (F11 = prism shortcut, Shift+F11 = copy to clipboard at game resolution)");

	if (gStoryEditorData.mIsStoryScreenActive)
	{
		ImGui::Separator();
		if (ImGui::Button("Reload scene at current state (F5)"))
		{
			reloadStoryScreenAtCurrentState();
		}
		ImGui::SameLine();
		ImGui::Checkbox("Auto-reload on .def change", &gStoryEditorData.mIsAutoReloadEnabled);
		if (ImGui::Button("Open state source (Ctrl+O)"))
		{
			openStoryEditorCurrentStateSource();
		}
		if (!gStoryEditorData.mChangedWatchedFiles.empty())
		{
			for (const auto& path : gStoryEditorData.mChangedWatchedFiles)
			{
				ImGui::TextWrapped("Changed on disk since load: %s", path.c_str());
			}
			ImGui::SameLine();
			if (ImGui::SmallButton("Reload now"))
			{
				reloadStoryScreenAtCurrentState();
			}
		}
	}

	if (gDolmexicaStoryScreenData.mDebugStartState)
	{
		ImGui::Text("Start-state redirect: %d -> %d", gDolmexicaStoryScreenData.mDebugStartStateFrom, gDolmexicaStoryScreenData.mDebugStartState);
		ImGui::SameLine();
		if (ImGui::SmallButton("Clear"))
		{
			setDolmexicaStoryDebugStartState(0, 0);
			gStoryEditorData.mPendingRestartTargetState = -1;
		}
	}

	if (ImGui::CollapsingHeader("States"))
	{
		ImGui::TextWrapped("Jump changes the root state in place (stale objects from skipped states remain). Restart reloads the scene and redirects the first state change out of state 0 to the target. While paused, a jump only executes once stepped/resumed.");
		ImGui::Checkbox("Dismiss active texts on jump", &gStoryEditorData.mDoesDismissTextsOnJump);
		for (const auto& statePair : gDolmexicaStoryScreenData.mStoryStates.mStates)
		{
			ImGui::PushID(statePair.first);
			ImGui::Text("State %d", statePair.first);
			ImGui::SameLine();
			if (ImGui::SmallButton("Jump"))
			{
				performStoryEditorStateJump(root, statePair.first);
			}
			ImGui::SameLine();
			if (ImGui::SmallButton("Restart at"))
			{
				restartStoryScreenAtState(statePair.first);
			}
			ImGui::PopID();
		}
	}

	if (ImGui::CollapsingHeader("State history"))
	{
		for (int i = int(gStoryEditorData.mStateHistory.size()) - 1; i >= 0; i--)
		{
			ImGui::PushID(i);
			ImGui::Text("State %d", gStoryEditorData.mStateHistory[i]);
			ImGui::SameLine();
			if (ImGui::SmallButton("Jump"))
			{
				performStoryEditorStateJump(root, gStoryEditorData.mStateHistory[i]);
			}
			ImGui::PopID();
		}
	}

	ImGui::End();
}

// .def write-back 

static std::string getStoryEditorTrimmedString(const std::string& tValue)
{
	size_t start = 0, end = tValue.size();
	while (start < end && isspace((unsigned char)tValue[start])) start++;
	while (end > start && isspace((unsigned char)tValue[end - 1])) end--;
	return tValue.substr(start, end - start);
}

static bool isStoryEditorStringEqualCaseIndependent(const std::string& tA, const std::string& tB)
{
	if (tA.size() != tB.size()) return false;
	for (size_t i = 0; i < tA.size(); i++)
	{
		if (tolower((unsigned char)tA[i]) != tolower((unsigned char)tB[i])) return false;
	}
	return true;
}

// reads the current value of (group, group offset, key) from the .def file, mirroring the group/key
// search saveMugenDefString uses (first group matching tGroupName, then the tGroupOffset-th group after it)
static bool readStoryEditorDefValue(const std::string& tPath, const std::string& tGroupName, int tGroupOffset, const std::string& tKey, std::string* oValue)
{
	if (!isFile(tPath.c_str())) return false;
	auto b = fileToBuffer(tPath.c_str());
	const std::string content((const char*)b.mData, size_t(b.mLength));
	freeBuffer(b);

	bool foundOriginal = false;
	bool inTargetGroup = false;
	int index = 0;
	size_t lineStart = 0;
	while (lineStart <= content.size())
	{
		auto lineEnd = content.find('\n', lineStart);
		if (lineEnd == std::string::npos) lineEnd = content.size();
		auto line = content.substr(lineStart, lineEnd - lineStart);
		lineStart = lineEnd + 1;
		if (!line.empty() && line.back() == '\r') line.pop_back();

		if (!line.empty() && line[0] == '[')
		{
			if (inTargetGroup) return false; // left the target group without finding the key
			std::string groupName;
			for (size_t i = 1; i < line.size() && line[i] != ']'; i++) groupName.push_back(line[i]);
			if (!foundOriginal)
			{
				if (isStoryEditorStringEqualCaseIndependent(groupName, tGroupName)) foundOriginal = true;
			}
			else if (index == tGroupOffset)
			{
				inTargetGroup = true;
			}
			else
			{
				index++;
			}
			continue;
		}
		if (!inTargetGroup) continue;
		const auto trimmed = getStoryEditorTrimmedString(line);
		if (trimmed.empty() || trimmed[0] == ';') continue;
		const auto equalsPosition = trimmed.find('=');
		if (equalsPosition == std::string::npos) continue;
		if (!isStoryEditorStringEqualCaseIndependent(getStoryEditorTrimmedString(trimmed.substr(0, equalsPosition)), tKey)) continue;
		auto value = trimmed.substr(equalsPosition + 1);
		const auto commentPosition = value.find(';');
		if (commentPosition != std::string::npos) value = value.substr(0, commentPosition);
		*oValue = getStoryEditorTrimmedString(value);
		return true;
	}
	return false;
}

// jump-to-source: 0-based line of the tGroupOffset-th group after the first group named tGroupName (tGroupOffset < 0 = the named group itself), mirroring the group search of readStoryEditorDefValue and saveMugenDefString, -1 when not found
static int getStoryEditorDefGroupLine(const std::string& tPath, const std::string& tGroupName, int tGroupOffset)
{
	if (!isFile(tPath.c_str())) return -1;
	auto b = fileToBuffer(tPath.c_str());
	const std::string content((const char*)b.mData, size_t(b.mLength));
	freeBuffer(b);

	auto foundOriginal = false;
	int index = 0;
	int lineNumber = -1;
	size_t lineStart = 0;
	while (lineStart <= content.size())
	{
		lineNumber++;
		auto lineEnd = content.find('\n', lineStart);
		if (lineEnd == std::string::npos) lineEnd = content.size();
		auto line = content.substr(lineStart, lineEnd - lineStart);
		lineStart = lineEnd + 1;
		if (!line.empty() && line.back() == '\r') line.pop_back();

		if (line.empty() || line[0] != '[') continue;
		std::string groupName;
		for (size_t i = 1; i < line.size() && line[i] != ']'; i++) groupName.push_back(line[i]);
		if (!foundOriginal)
		{
			if (isStoryEditorStringEqualCaseIndependent(groupName, tGroupName))
			{
				if (tGroupOffset < 0) return lineNumber;
				foundOriginal = true;
			}
		}
		else if (index == tGroupOffset)
		{
			return lineNumber;
		}
		else
		{
			index++;
		}
	}
	return -1;
}

static void openStoryEditorSourceLocation(const std::string& tPath, const std::string& tGroupName, int tGroupOffset)
{
	const auto line = getStoryEditorDefGroupLine(tPath, tGroupName, tGroupOffset);
	prism::imgui::openTextEditor(tPath, (std::max)(0, line));
}

// Ctrl+O: the current story file in the built-in text editor, scrolled to the root instance's active statedef. The statedef provenance resolves which file declared the state (story scenes can spread statedefs over several files via helper scripts), so this lands in the right file even when the root runs shared states
static void openStoryEditorCurrentStateSource()
{
	if (gDolmexicaStoryScreenData.mHelperInstances.empty()) return;
	const auto state = getDolmexicaStoryStateNumber(getDolmexicaStoryRootInstance());
	const auto stateDefPath = getDreamMugenStateDefProvenance(state);
	if (!stateDefPath) return;
	openStoryEditorSourceLocation(*stateDefPath, "statedef " + std::to_string(state), -1);
}

// digits, whitespace, commas, dots and signs only = safe to overwrite without confirmation
static bool isStoryEditorNumericLiteral(const std::string& tValue)
{
	bool hasDigit = false;
	for (const char c : tValue)
	{
		if (isdigit((unsigned char)c)) { hasDigit = true; continue; }
		if (c == '-' || c == '+' || c == '.' || c == ',' || isspace((unsigned char)c)) continue;
		return false;
	}
	return hasDigit;
}

// string values (text/name) count as expressions when they reference runtime lookups
static bool isStoryEditorStringValueSafeToOverwrite(const std::string& tValue)
{
	std::string stripped;
	for (const char c : tValue)
	{
		if (!isspace((unsigned char)c)) stripped.push_back(char(tolower((unsigned char)c)));
	}
	return stripped.find("var(") == std::string::npos && stripped.find("nameid(") == std::string::npos;
}

static std::vector<std::string> splitStoryEditorDefValueComponents(const std::string& tValue)
{
	std::vector<std::string> ret;
	size_t start = 0;
	while (true)
	{
		const auto commaPosition = tValue.find(',', start);
		if (commaPosition == std::string::npos)
		{
			ret.push_back(getStoryEditorTrimmedString(tValue.substr(start)));
			return ret;
		}
		ret.push_back(getStoryEditorTrimmedString(tValue.substr(start, commaPosition - start)));
		start = commaPosition + 1;
	}
}

// the editor only edits the leading components of some vector keys (font = f,b,a; offsets = x,y,z)
// keep any extra trailing components the file already had
static std::string mergeStoryEditorDefValueTail(const std::string& tNewValue, const std::string& tOriginalValue)
{
	const auto newComponents = splitStoryEditorDefValueComponents(tNewValue);
	const auto originalComponents = splitStoryEditorDefValueComponents(tOriginalValue);
	auto ret = tNewValue;
	for (size_t i = newComponents.size(); i < originalComponents.size(); i++)
	{
		ret += ", " + originalComponents[i];
	}
	return ret;
}

static void ensureStoryEditorDefBackup(const std::string& tPath)
{
	if (gStoryEditorSaveData.mBackedUpPaths.count(tPath)) return;
	gStoryEditorSaveData.mBackedUpPaths.insert(tPath);
	if (!isFile(tPath.c_str())) return;
	auto b = fileToBuffer(tPath.c_str());
	bufferToFile((tPath + ".bak").c_str(), b);
	freeBuffer(b);
}

static std::string getStoryEditorSaveGroupName(const StoryEditorProvenanceStamp& tStamp)
{
	return "statedef " + std::to_string(tStamp.mStateID);
}

// LockOffset always routes to the LockText controller, "facing" always routes to the last AnimSetFacing/CharSetFacing controller (the creating controllers have no facing param), "anim" prefers the last ChangeAnim/CharChangeAnim controller, other text fields prefer the last ChangeText controller when its group actually declares the key (it ran after creation, so it owns the visible value). Everything else falls back to the creating controller
static const StoryEditorProvenanceStamp* resolveStoryEditorSaveStamp(const StoryEditorObjectKey& tKey, const std::string& tDefKey, const StoryEditorDirtyField& tField)
{
	const auto stampsIt = gStoryEditorSaveData.mObjectStamps.find(tKey);
	if (stampsIt == gStoryEditorSaveData.mObjectStamps.end()) return nullptr;
	const auto& stamps = stampsIt->second;
	if (tField.mUsesLockProvenance) return stamps.mLockText.mIsValid ? &stamps.mLockText : nullptr;
	if (tDefKey == "facing") return stamps.mSetFacing.mIsValid ? &stamps.mSetFacing : nullptr;
	if (tDefKey == "x" || tDefKey == "y") return stamps.mSetScale.mIsValid ? &stamps.mSetScale : nullptr; // scale axes: only AnimScaleSet/CharScaleSet declare these keys, the creating controllers have no scale param
	const auto& lastModifier = (tDefKey == "anim") ? stamps.mChangeAnim : stamps.mChangeText;
	if (lastModifier.mIsValid)
	{
		std::string existingValue;
		if (readStoryEditorDefValue(lastModifier.mScriptPath, getStoryEditorSaveGroupName(lastModifier), lastModifier.mControllerIndex, tDefKey, &existingValue)) return &lastModifier;
	}
	return stamps.mCreation.mIsValid ? &stamps.mCreation : nullptr;
}

static bool isStoryEditorStringDefKey(const std::string& tDefKey)
{
	return tDefKey == "text" || tDefKey == "name";
}

enum class StoryEditorFieldSaveResult {
	Saved,
	NeedsConfirmation,
	NoProvenance,
};

static StoryEditorFieldSaveResult saveStoryEditorDirtyField(const StoryEditorObjectKey& tKey, const std::string& tDefKey, StoryEditorDirtyField& tField, bool tForceOverwrite)
{
	const auto stamp = resolveStoryEditorSaveStamp(tKey, tDefKey, tField);
	if (!stamp) return StoryEditorFieldSaveResult::NoProvenance;
	const auto groupName = getStoryEditorSaveGroupName(*stamp);
	std::string originalValue;
	const auto hasOriginal = readStoryEditorDefValue(stamp->mScriptPath, groupName, stamp->mControllerIndex, tDefKey, &originalValue);
	auto value = tField.mCurrentValue;
	if (isStoryEditorStringDefKey(tDefKey))
	{
		if (hasOriginal && !tForceOverwrite && !isStoryEditorStringValueSafeToOverwrite(originalValue))
		{
			tField.mHasPendingConflict = 1;
			tField.mConflictOriginalValue = originalValue;
			return StoryEditorFieldSaveResult::NeedsConfirmation;
		}
		if (!hasOriginal || (!originalValue.empty() && originalValue[0] == '"')) value = "\"" + value + "\"";
	}
	else if (hasOriginal)
	{
		if (isStoryEditorNumericLiteral(originalValue))
		{
			value = mergeStoryEditorDefValueTail(value, originalValue);
		}
		else if (!tForceOverwrite)
		{
			tField.mHasPendingConflict = 1;
			tField.mConflictOriginalValue = originalValue;
			return StoryEditorFieldSaveResult::NeedsConfirmation;
		}
	}
	ensureStoryEditorDefBackup(stamp->mScriptPath);
	saveMugenDefString(stamp->mScriptPath, groupName.c_str(), size_t(stamp->mControllerIndex), tDefKey.c_str(), value);
	refreshStoryEditorWatchedFileTime(stamp->mScriptPath); // editor-initiated write: do not trigger hot reload
	tField.mSavedValue = tField.mCurrentValue;
	tField.mHasSavedValue = 1;
	tField.mHasPendingConflict = 0;
	tField.mConflictOriginalValue.clear();
	return StoryEditorFieldSaveResult::Saved;
}

// which fields can be saved as a NEW controller appended to a statedef, instead of editing the original controller (so earlier uses of that controller keep their old values): LockOffset -> LockText, character/animation anim -> CharChangeAnim/ChangeAnim, character/animation facing -> CharSetFacing/AnimSetFacing, character/animation scale x/y -> CharScaleSet/AnimScaleSet, character/animation pos -> CharPosAdd/AnimPosAdd (as a position DIFFERENCE, see writeStoryEditorNewControllerForField)
static bool isStoryEditorFieldSavableAsNewController(const StoryEditorObjectKey& tKey, const std::string& tDefKey, const StoryEditorDirtyField& tField)
{
	if (tField.mUsesLockProvenance && tDefKey == "offset") return true;
	return (tDefKey == "anim" || tDefKey == "facing" || tDefKey == "x" || tDefKey == "y" || tDefKey == "pos") && StoryEditorObjectKind(std::get<1>(tKey)) != StoryEditorObjectKind::Text;
}

static bool writeStoryEditorNewControllerForField(const StoryEditorObjectKey& tKey, const std::string& tDefKey, StoryEditorDirtyField& tField, int tTargetState)
{
	auto& s = gStoryEditorSaveData;
	const auto objectKind = StoryEditorObjectKind(std::get<1>(tKey));
	const auto objectID = std::get<2>(tKey);
	const auto stampsIt = s.mObjectStamps.find(tKey);
	const StoryEditorObjectStamps* stamps = (stampsIt == s.mObjectStamps.end()) ? nullptr : &stampsIt->second;
	const auto readSourceValue = [&](const StoryEditorProvenanceStamp& tStamp, const char* tSourceKey, std::string* oValue) {
		if (!tStamp.mIsValid) return false;
		return readStoryEditorDefValue(tStamp.mScriptPath, getStoryEditorSaveGroupName(tStamp), tStamp.mControllerIndex, tSourceKey, oValue);
	};

	// target file + insertion anchor resolved first: append after the statedef's last controller group, the id/character copies below prefer source controllers from this same file
	const auto stateIt = gDolmexicaStoryScreenData.mStoryStates.mStates.find(tTargetState);
	if (stateIt == gDolmexicaStoryScreenData.mStoryStates.mStates.end())
	{
		s.mStatusMessage = "New controller not written: statedef " + std::to_string(tTargetState) + " does not exist";
		return false;
	}
	const auto stateDefPath = getDreamMugenStateDefProvenance(tTargetState);
	if (!stateDefPath)
	{
		s.mStatusMessage = "New controller not written: no file provenance for statedef " + std::to_string(tTargetState);
		return false;
	}

	// controller type + params. id/character copy the raw strings from a source controller so nameid()-style ids survive - but a raw string only transplants safely into the file it came from: helper scripts address objects via variables (id = var(1)), which don't evaluate in another file's statedef. So same-file candidates win (e.g. the CreateChar in the story's own .def) and cross-file copies are a warned last resort
	int usedCrossFileSource = 0;
	const auto readSourceValuePreferSameFile = [&](std::initializer_list<const StoryEditorProvenanceStamp*> tCandidates, const char* tSourceKey, std::string* oValue) {
		for (const auto candidate : tCandidates)
		{
			if (candidate->mScriptPath == *stateDefPath && readSourceValue(*candidate, tSourceKey, oValue)) return true;
		}
		for (const auto candidate : tCandidates)
		{
			if (readSourceValue(*candidate, tSourceKey, oValue)) { usedCrossFileSource = 1; return true; }
		}
		return false;
	};
	const char* type;
	std::vector<std::pair<std::string, std::string>> variables;
	if (tField.mUsesLockProvenance && tDefKey == "offset")
	{
		type = "LockText";
		std::string id = std::to_string(objectID), character;
		if (stamps) readSourceValuePreferSameFile({ &stamps->mLockText, &stamps->mCreation }, "id", &id);
		if (!stamps || !readSourceValuePreferSameFile({ &stamps->mLockText }, "character", &character))
		{
			s.mStatusMessage = "New LockText not written: cannot read 'character' from the original LockText controller group";
			return false;
		}
		variables = { {"type", type}, {"trigger1", "time = 0"}, {"id", id}, {"offset", tField.mCurrentValue}, {"character", character} };
	}
	else if (tDefKey == "anim" && objectKind != StoryEditorObjectKind::Text)
	{
		type = (objectKind == StoryEditorObjectKind::Character) ? "CharChangeAnim" : "ChangeAnim";
		std::string id = std::to_string(objectID);
		if (stamps) readSourceValuePreferSameFile({ &stamps->mChangeAnim, &stamps->mCreation }, "id", &id);
		variables = { {"type", type}, {"trigger1", "time = 0"}, {"id", id}, {"anim", tField.mCurrentValue} };
	}
	else if (tDefKey == "facing" && objectKind != StoryEditorObjectKind::Text)
	{
		type = (objectKind == StoryEditorObjectKind::Character) ? "CharSetFacing" : "AnimSetFacing";
		std::string id = std::to_string(objectID);
		if (stamps) readSourceValuePreferSameFile({ &stamps->mSetFacing, &stamps->mCreation }, "id", &id);
		variables = { {"type", type}, {"trigger1", "time = 0"}, {"id", id}, {"facing", tField.mCurrentValue} };
	}
	else if (tDefKey == "pos" && objectKind != StoryEditorObjectKind::Text)
	{
		// a position DIFFERENCE rather than an absolute position: PosAdd composes with whatever placed/moved the object before it, so the original CreateChar/PosSet/movement controllers keep their meaning. Delta = current value minus what the file currently produces (last-saved value, or the pre-edit baseline before any save), the same reference dirtiness compares against, so repeated PosAdd saves rebase on the previous one and accumulate correctly across reloads
		type = (objectKind == StoryEditorObjectKind::Character) ? "CharPosAdd" : "AnimPosAdd";
		const auto referenceComponents = splitStoryEditorDefValueComponents(tField.mHasSavedValue ? tField.mSavedValue : tField.mBaselineValue);
		const auto currentComponents = splitStoryEditorDefValueComponents(tField.mCurrentValue);
		if (referenceComponents.size() < 2 || currentComponents.size() < 2)
		{
			s.mStatusMessage = "New controller not written: cannot compute position difference";
			return false;
		}
		std::string id = std::to_string(objectID);
		if (stamps) readSourceValuePreferSameFile({ &stamps->mCreation }, "id", &id);
		variables = { {"type", type}, {"trigger1", "time = 0"}, {"id", id},
			{"x", formatStoryEditorDefNumber(atof(currentComponents[0].c_str()) - atof(referenceComponents[0].c_str()))},
			{"y", formatStoryEditorDefNumber(atof(currentComponents[1].c_str()) - atof(referenceComponents[1].c_str()))} };
	}
	else if ((tDefKey == "x" || tDefKey == "y") && objectKind != StoryEditorObjectKind::Text)
	{
		type = (objectKind == StoryEditorObjectKind::Character) ? "CharScaleSet" : "AnimScaleSet";
		std::string id = std::to_string(objectID);
		if (stamps) readSourceValuePreferSameFile({ &stamps->mSetScale, &stamps->mCreation }, "id", &id);
		variables = { {"type", type}, {"trigger1", "time = 0"}, {"id", id} };
		// both edited axes go into the one controller so the new group reproduces the full live scale (a single-axis ScaleSet would leave the other axis at whatever an earlier controller set)
		for (const char* axis : { "x", "y" })
		{
			const auto axisIt = s.mDirtyFields[tKey].find(axis);
			if (axisIt != s.mDirtyFields[tKey].end()) variables.push_back({ axis, axisIt->second.mCurrentValue });
		}
	}
	else
	{
		return false;
	}

	const auto groupName = "statedef " + std::to_string(tTargetState);
	auto& appendedCount = s.mAppendedControllerCounts[std::make_pair(*stateDefPath, tTargetState)];
	const auto anchorOffset = int(vector_size(&stateIt->second.mControllers)) + appendedCount - 1; // -1 = right after the statedef group itself (0 controllers)
	ensureStoryEditorDefBackup(*stateDefPath);
	addMugenDefScriptGroup(*stateDefPath, groupName.c_str(), anchorOffset, ("State " + std::to_string(tTargetState) + ", editor").c_str(), variables);
	refreshStoryEditorWatchedFileTime(*stateDefPath); // editor-initiated write: do not trigger hot reload
	appendedCount++;
	tField.mSavedValue = tField.mCurrentValue;
	tField.mHasSavedValue = 1;
	tField.mHasPendingConflict = 0;
	tField.mConflictOriginalValue.clear();
	if (tDefKey == "x" || tDefKey == "y")
	{
		// the new ScaleSet group contains both edited axes, so the sibling axis field is saved by this write too
		for (const char* axis : { "x", "y" })
		{
			const auto axisIt = s.mDirtyFields[tKey].find(axis);
			if (axisIt == s.mDirtyFields[tKey].end()) continue;
			axisIt->second.mSavedValue = axisIt->second.mCurrentValue;
			axisIt->second.mHasSavedValue = 1;
			axisIt->second.mHasPendingConflict = 0;
			axisIt->second.mConflictOriginalValue.clear();
		}
	}
	s.mStatusMessage = std::string("New ") + type + " appended to [statedef " + std::to_string(tTargetState) + "] in " + *stateDefPath + " - the running scene does not execute it until a reload";
	if (usedCrossFileSource) s.mStatusMessage += ". WARNING: id/character copied from a controller in a DIFFERENT file - values like var(N) may not evaluate in this statedef, check the written group";
	if (tDefKey == "pos") s.mStatusMessage += ". Note: a later plain 'pos' save to the creating controller would stack with this PosAdd after a reload, stick to one save path per object";
	return true;
}

static std::string getStoryEditorObjectKeyDisplayName(const StoryEditorObjectKey& tKey)
{
	static const char* kindNames[] = { "Character", "Animation", "Text" };
	const auto instanceKey = std::get<0>(tKey);
	const auto instanceName = (instanceKey == -1) ? std::string("Root") : ("Helper " + std::to_string(instanceKey));
	return instanceName + " " + kindNames[std::get<1>(tKey)] + " " + std::to_string(std::get<2>(tKey)) + getStoryEditorIDNameSuffix(instanceKey, std::get<2>(tKey));
}

// single provenance stamp readout + jump-to-source button (opens the .def in the imgui text editor scrolled to the controller's group)
static void imguiStoryEditorProvenanceStampLine(const char* tLabel, const StoryEditorProvenanceStamp& tStamp)
{
	ImGui::PushID(tLabel);
	ImGui::TextDisabled("%s: %s [statedef %d] +%d", tLabel, tStamp.mScriptPath.c_str(), tStamp.mStateID, tStamp.mControllerIndex);
	ImGui::SameLine();
	if (ImGui::SmallButton("Open"))
	{
		openStoryEditorSourceLocation(tStamp.mScriptPath, getStoryEditorSaveGroupName(tStamp), tStamp.mControllerIndex);
	}
	ImGui::PopID();
}

static void imguiStoryEditorProvenanceLine(int tInstanceKey, StoryEditorObjectKind tObjectKind, int tID)
{
	const auto stamps = getStoryEditorObjectStamps(tInstanceKey, tObjectKind, tID);
	if (!stamps || !stamps->mCreation.mIsValid)
	{
		ImGui::TextDisabled("Source: unknown");
		return;
	}
	imguiStoryEditorProvenanceStampLine("Source", stamps->mCreation);
	if (stamps->mChangeText.mIsValid) imguiStoryEditorProvenanceStampLine("Last ChangeText", stamps->mChangeText);
	if (stamps->mLockText.mIsValid) imguiStoryEditorProvenanceStampLine("LockText", stamps->mLockText);
	if (stamps->mChangeAnim.mIsValid) imguiStoryEditorProvenanceStampLine("Last ChangeAnim", stamps->mChangeAnim);
	if (stamps->mSetFacing.mIsValid) imguiStoryEditorProvenanceStampLine("Last SetFacing", stamps->mSetFacing);
	if (stamps->mSetScale.mIsValid) imguiStoryEditorProvenanceStampLine("Last ScaleSet", stamps->mSetScale);
}

// single tracked-field widget (status, save/open buttons, save-as-new-controller, conflict resolution), shared between the per-object tree and the unsaved-changes quick access list at the top of the Save window. tShowObjectName prefixes the owning object for the flat quick access rendering
static void imguiStorySaveFieldWidget(const StoryEditorObjectKey& tObjectKey, const std::string& tDefKey, StoryEditorDirtyField& field, bool tShowObjectName)
{
	ImGui::PushID(tDefKey.c_str());
	const auto isDirty = isStoryEditorFieldDirty(field);
	const char* status = field.mHasPendingConflict ? "CONFLICT" : (isDirty ? "unsaved" : (field.mHasSavedValue ? "saved" : "unchanged"));
	if (tShowObjectName)
	{
		ImGui::Text("%s: %s (%s) = %s [%s]", getStoryEditorObjectKeyDisplayName(tObjectKey).c_str(), field.mLabel.c_str(), tDefKey.c_str(), field.mCurrentValue.c_str(), status);
	}
	else
	{
		ImGui::Text("%s (%s) = %s [%s]", field.mLabel.c_str(), tDefKey.c_str(), field.mCurrentValue.c_str(), status);
	}
	const auto stamp = resolveStoryEditorSaveStamp(tObjectKey, tDefKey, field);
	if (stamp)
	{
		ImGui::SameLine();
		if (ImGui::SmallButton("Save")) saveStoryEditorDirtyField(tObjectKey, tDefKey, field, false);
		ImGui::TextDisabled("  -> %s [statedef %d] +%d", stamp->mScriptPath.c_str(), stamp->mStateID, stamp->mControllerIndex);
		ImGui::SameLine();
		if (ImGui::SmallButton("Open"))
		{
			openStoryEditorSourceLocation(stamp->mScriptPath, getStoryEditorSaveGroupName(*stamp), stamp->mControllerIndex);
		}
	}
	else
	{
		ImGui::TextDisabled("  -> no provenance (created outside a stamped controller)");
	}
	if (isStoryEditorFieldSavableAsNewController(tObjectKey, tDefKey, field))
	{
		// follows the owning instance's current state until the user overrides it
		if (!field.mHasNewControllerTargetState)
		{
			const auto owningInstance = getStoryEditorInstance(std::get<0>(tObjectKey));
			if (owningInstance) field.mNewControllerTargetState = getDolmexicaStoryStateNumber(owningInstance);
		}
		ImGui::SetNextItemWidth(80.f);
		if (ImGui::InputInt("##NewControllerState", &field.mNewControllerTargetState)) field.mHasNewControllerTargetState = 1;
		ImGui::SameLine();
		if (ImGui::SmallButton("Save as new controller in this state"))
		{
			writeStoryEditorNewControllerForField(tObjectKey, tDefKey, field, field.mNewControllerTargetState);
		}
	}
	if (field.mHasPendingConflict)
	{
		ImGui::TextWrapped("  File has expression: %s", field.mConflictOriginalValue.c_str());
		ImGui::TextWrapped("  New literal value:   %s", field.mCurrentValue.c_str());
		if (ImGui::SmallButton("Overwrite with literal")) saveStoryEditorDirtyField(tObjectKey, tDefKey, field, true);
		ImGui::SameLine();
		if (ImGui::SmallButton("Keep expression"))
		{
			field.mHasPendingConflict = 0;
			field.mConflictOriginalValue.clear();
		}
	}
	ImGui::PopID();
}

static void imguiStorySave()
{
	auto& isWindowShown = gStoryEditorWindowVisibility.mSave;
	imguiPrismAddTab("Story Editor", "Save", &isWindowShown);
	if (!isWindowShown) return;

	ImGui::Begin("Story Save", &isWindowShown);
	auto& s = gStoryEditorSaveData;

	int dirtyCount = 0, conflictCount = 0;
	for (const auto& objectPair : s.mDirtyFields)
	{
		for (const auto& fieldPair : objectPair.second)
		{
			if (isStoryEditorFieldDirty(fieldPair.second)) dirtyCount++;
			if (fieldPair.second.mHasPendingConflict) conflictCount++;
		}
	}

	ImGui::Text("Dirty fields: %d", dirtyCount);
	ImGui::BeginDisabled(!dirtyCount);
	if (ImGui::Button("Save all dirty"))
	{
		int saved = 0, conflicts = 0, unresolved = 0;
		for (auto& objectPair : s.mDirtyFields)
		{
			for (auto& fieldPair : objectPair.second)
			{
				if (!isStoryEditorFieldDirty(fieldPair.second)) continue;
				switch (saveStoryEditorDirtyField(objectPair.first, fieldPair.first, fieldPair.second, false))
				{
				case StoryEditorFieldSaveResult::Saved: saved++; break;
				case StoryEditorFieldSaveResult::NeedsConfirmation: conflicts++; break;
				case StoryEditorFieldSaveResult::NoProvenance: unresolved++; break;
				}
			}
		}
		s.mStatusMessage = "Saved " + std::to_string(saved) + " field(s)";
		if (conflicts) s.mStatusMessage += ", " + std::to_string(conflicts) + " expression conflict(s) awaiting confirmation below";
		if (unresolved) s.mStatusMessage += ", " + std::to_string(unresolved) + " without provenance (skipped)";
	}
	ImGui::EndDisabled();
	ImGui::SameLine();
	if (!gDolmexicaStoryScreenData.mHelperInstances.empty() && ImGui::Button("Reload scene at current state"))
	{
		restartStoryScreenAtState(getDolmexicaStoryStateNumber(getDolmexicaStoryRootInstance()));
	}
	if (!s.mStatusMessage.empty()) ImGui::TextWrapped("%s", s.mStatusMessage.c_str());
	ImGui::TextWrapped("First write to a file this session creates a .bak next to it. Values whose original is an expression are never overwritten silently. Only params of the creating controller are saved; panel-only fields (angle, color, opacity, vars) stay live-only. LockOffset, anim, facing, scale and pos edits can alternatively be saved as a NEW LockText/CharChangeAnim/ChangeAnim/CharSetFacing/AnimSetFacing/CharScaleSet/AnimScaleSet/CharPosAdd/AnimPosAdd controller appended to a statedef, so earlier uses of the original controller keep their values. Pos writes the position DIFFERENCE as a PosAdd, not the absolute position.");
	ImGui::Separator();

	// quick access: every dirty/conflicted field across all objects in one flat list, so unsaved work doesn't have to be picked out of the full per-object trees below
	if (dirtyCount && ImGui::CollapsingHeader("Unsaved changes (quick access)", ImGuiTreeNodeFlags_DefaultOpen))
	{
		for (auto& objectPair : s.mDirtyFields)
		{
			for (auto& fieldPair : objectPair.second)
			{
				if (!isStoryEditorFieldDirty(fieldPair.second) && !fieldPair.second.mHasPendingConflict) continue;
				ImGui::PushID(&fieldPair.second); // per-field disambiguation, different objects share def keys like "pos"
				imguiStorySaveFieldWidget(objectPair.first, fieldPair.first, fieldPair.second, true);
				ImGui::PopID();
			}
		}
	}
	if (dirtyCount) ImGui::Separator();

	if (s.mDirtyFields.empty()) ImGui::Text("No tracked edits.");
	if (!s.mDirtyFields.empty() && !ImGui::CollapsingHeader("All tracked objects", ImGuiTreeNodeFlags_DefaultOpen)) { ImGui::End(); return; }
	for (auto& objectPair : s.mDirtyFields)
	{
		int objectDirtyCount = 0;
		for (const auto& fieldPair : objectPair.second)
		{
			if (isStoryEditorFieldDirty(fieldPair.second)) objectDirtyCount++;
		}
		const auto label = getStoryEditorObjectKeyDisplayName(objectPair.first) + (objectDirtyCount ? (" (" + std::to_string(objectDirtyCount) + " dirty)") : " (clean)") + "###" + std::to_string(std::get<0>(objectPair.first)) + "_" + std::to_string(std::get<1>(objectPair.first)) + "_" + std::to_string(std::get<2>(objectPair.first));
		if (!ImGui::TreeNode(label.c_str())) continue;
		for (auto& fieldPair : objectPair.second)
		{
			imguiStorySaveFieldWidget(objectPair.first, fieldPair.first, fieldPair.second, false);
		}
		ImGui::TreePop();
	}
	if (conflictCount) ImGui::TextWrapped("%d expression conflict(s) pending - expand the objects above to resolve.", conflictCount);
	ImGui::End();
}

// Validation pass

// Checks over the live scene (anims present in the loaded .air, text fonts loaded, texts within the screen, stale text names) plus a textual nameid() cross-check over every loaded .def state file. Results are cached until re-run, scene loads clear them

struct StoryEditorValidationIssue {
	std::string mMessage;
	std::string mPath; // non-empty = jump-to-source available
	int mLine = -1;    // 0-based
};

static struct {
	std::vector<StoryEditorValidationIssue> mIssues;
	int mHasRun = 0;
	int mRunState = -1;
} gStoryEditorValidationData;

static void resetStoryEditorValidation()
{
	gStoryEditorValidationData.mIssues.clear();
	gStoryEditorValidationData.mHasRun = 0;
	gStoryEditorValidationData.mRunState = -1;
}

static void addStoryEditorValidationIssue(const std::string& tMessage, const std::string& tPath = std::string(), int tLine = -1)
{
	StoryEditorValidationIssue issue;
	issue.mMessage = tMessage;
	issue.mPath = tPath;
	issue.mLine = tLine;
	gStoryEditorValidationData.mIssues.push_back(std::move(issue));
}

static void validateStoryEditorAnimationObject(const std::string& tName, StoryAnimation& tAnimation, MugenAnimations* tAnimations)
{
	if (!tAnimation.mAnimationElement || !isRegisteredMugenAnimation(tAnimation.mAnimationElement))
	{
		addStoryEditorValidationIssue(tName + ": no registered animation element");
		return;
	}
	const auto animationNumber = getMugenAnimationAnimationNumber(tAnimation.mAnimationElement);
	if (!hasMugenAnimation(tAnimations, animationNumber))
	{
		addStoryEditorValidationIssue(tName + ": current anim " + std::to_string(animationNumber) + " is not in the loaded .air");
	}
}

static void validateStoryEditorText(const std::string& tName, StoryText& e)
{
	const auto font = int(getMugenTextFont(e.mTextID));
	if (!hasMugenFont(font))
	{
		addStoryEditorValidationIssue(tName + ": font " + std::to_string(font) + " is not loaded");
	}
	if (e.mHasName)
	{
		const auto nameFont = int(getMugenTextFont(e.mNameID));
		if (!hasMugenFont(nameFont))
		{
			addStoryEditorValidationIssue(tName + ": name font " + std::to_string(nameFont) + " is not loaded");
		}
	}
	GeoRectangle2D rectangle;
	if (getStoryEditorSceneTextRectangle(e, &rectangle))
	{
		const auto screenSize = getScreenSize();
		if (rectangle.mTopLeft.x < 0 || rectangle.mBottomRight.x > screenSize.x)
		{
			addStoryEditorValidationIssue(tName + ": extends horizontally off-screen (x " + formatStoryEditorDefNumber(rectangle.mTopLeft.x) + " to " + formatStoryEditorDefNumber(rectangle.mBottomRight.x) + ", screen width " + std::to_string(screenSize.x) + ")");
		}
		if (rectangle.mTopLeft.y < 0 || rectangle.mBottomRight.y > screenSize.y)
		{
			addStoryEditorValidationIssue(tName + ": extends vertically off-screen (y " + formatStoryEditorDefNumber(rectangle.mTopLeft.y) + " to " + formatStoryEditorDefNumber(rectangle.mBottomRight.y) + ", screen height " + std::to_string(screenSize.y) + ")");
		}
	}
}

static std::string getStoryEditorLowercaseString(const std::string& tValue)
{
	auto ret = tValue;
	std::transform(ret.begin(), ret.end(), ret.begin(), [](unsigned char c) { return char(tolower(c)); });
	return ret;
}

static std::string getStoryEditorUnquotedString(const std::string& tValue)
{
	const auto trimmed = getStoryEditorTrimmedString(tValue);
	if (trimmed.size() >= 2 && trimmed.front() == '"' && trimmed.back() == '"') return trimmed.substr(1, trimmed.size() - 2);
	return trimmed;
}

struct StoryEditorNameIDReference {
	std::string mName; // lowercased
	std::string mPath;
	int mLine; // 0-based
};

// textual scan: collects NameID controller declarations (groups with type = NameID -> their name param) and nameid(...) references. Names are compared lowercased; computed names (parentheses in the value) cannot be checked statically and are counted instead.
static void scanStoryEditorFileForNameIDs(const std::string& tPath, std::set<std::string>& oDeclaredNames, int& oDynamicDeclarationCount, std::vector<StoryEditorNameIDReference>& oReferences)
{
	if (!isFile(tPath.c_str())) return;
	auto b = fileToBuffer(tPath.c_str());
	const std::string content((const char*)b.mData, size_t(b.mLength));
	freeBuffer(b);

	auto groupIsNameID = false;
	auto groupHasNameValue = false;
	auto groupHasDynamicNameValue = false;
	std::string groupNameValue;
	const auto flushGroup = [&]() {
		if (groupIsNameID)
		{
			if (groupHasNameValue) oDeclaredNames.insert(groupNameValue);
			else if (groupHasDynamicNameValue) oDynamicDeclarationCount++;
		}
		groupIsNameID = groupHasNameValue = groupHasDynamicNameValue = false;
		groupNameValue.clear();
	};

	int lineNumber = -1;
	size_t lineStart = 0;
	while (lineStart <= content.size())
	{
		lineNumber++;
		auto lineEnd = content.find('\n', lineStart);
		if (lineEnd == std::string::npos) lineEnd = content.size();
		auto line = content.substr(lineStart, lineEnd - lineStart);
		lineStart = lineEnd + 1;
		if (!line.empty() && line.back() == '\r') line.pop_back();

		const auto trimmed = getStoryEditorTrimmedString(line);
		if (!trimmed.empty() && trimmed[0] == '[')
		{
			flushGroup();
			continue;
		}
		if (trimmed.empty() || trimmed[0] == ';') continue;

		// references can appear in any value or trigger expression
		const auto lower = getStoryEditorLowercaseString(line);
		size_t searchPosition = 0;
		while (true)
		{
			const auto referencePosition = lower.find("nameid(", searchPosition);
			if (referencePosition == std::string::npos) break;
			const auto closePosition = lower.find(')', referencePosition + 7);
			if (closePosition == std::string::npos) break;
			searchPosition = closePosition + 1;
			const auto inner = getStoryEditorUnquotedString(lower.substr(referencePosition + 7, closePosition - (referencePosition + 7)));
			if (inner.empty() || inner.find('(') != std::string::npos) continue; // computed name, not statically checkable
			StoryEditorNameIDReference reference;
			reference.mName = inner;
			reference.mPath = tPath;
			reference.mLine = lineNumber;
			oReferences.push_back(std::move(reference));
		}

		// declaration keys within the current group
		const auto equalsPosition = trimmed.find('=');
		if (equalsPosition == std::string::npos) continue;
		const auto key = getStoryEditorLowercaseString(getStoryEditorTrimmedString(trimmed.substr(0, equalsPosition)));
		auto value = trimmed.substr(equalsPosition + 1);
		const auto commentPosition = value.find(';');
		if (commentPosition != std::string::npos) value = value.substr(0, commentPosition);
		if (key == "type")
		{
			if (getStoryEditorLowercaseString(getStoryEditorTrimmedString(value)) == "nameid") groupIsNameID = true;
		}
		else if (key == "name")
		{
			const auto name = getStoryEditorLowercaseString(getStoryEditorUnquotedString(value));
			if (!name.empty() && name.find('(') == std::string::npos)
			{
				groupNameValue = name;
				groupHasNameValue = true;
			}
			else
			{
				groupHasDynamicNameValue = true;
			}
		}
	}
	flushGroup();
}

static void runStoryEditorValidation()
{
	auto& v = gStoryEditorValidationData;
	v.mIssues.clear();
	v.mHasRun = 1;
	v.mRunState = gDolmexicaStoryScreenData.mHelperInstances.empty() ? -1 : getDolmexicaStoryStateNumber(getDolmexicaStoryRootInstance());

	for (auto& instancePair : gDolmexicaStoryScreenData.mHelperInstances)
	{
		auto& instance = instancePair.second;
		const auto instanceName = getStoryInstanceDisplayName(&instance);
		for (auto& characterPair : instance.mStoryCharacters)
		{
			validateStoryEditorAnimationObject(instanceName + " Character " + std::to_string(characterPair.first) + " (" + characterPair.second.mName + ")", characterPair.second.mAnimation, &characterPair.second.mAnimations);
		}
		for (auto& animationPair : instance.mStoryAnimations)
		{
			validateStoryEditorAnimationObject(instanceName + " Animation " + std::to_string(animationPair.first), animationPair.second, &gDolmexicaStoryScreenData.mAnimations);
		}
		for (auto& textPair : instance.mStoryTexts)
		{
			if (textPair.second.mIsDisabled) continue;
			validateStoryEditorText(instanceName + " Text " + std::to_string(textPair.first), textPair.second);
		}
		for (const auto& textNamePair : instance.mTextNames)
		{
			if (!stl_map_contains(instance.mStoryTexts, textNamePair.second))
			{
				addStoryEditorValidationIssue(instanceName + ": text name \"" + textNamePair.first + "\" points at text " + std::to_string(textNamePair.second) + ", which no longer exists");
			}
		}
	}

	// nameid cross-check: declarations are unioned across all loaded files, since which instance runs which file's states is not statically known, a match in any file counts
	std::set<std::string> files;
	std::set<std::string> declaredNames;
	int dynamicDeclarationCount = 0;
	std::vector<StoryEditorNameIDReference> references;
	for (const auto& provenancePair : getDreamMugenStateDefProvenances()) files.insert(provenancePair.second);
	for (const auto& path : files) scanStoryEditorFileForNameIDs(path, declaredNames, dynamicDeclarationCount, references);
	for (const auto& reference : references)
	{
		if (declaredNames.count(reference.mName)) continue;
		auto message = reference.mPath + ":" + std::to_string(reference.mLine + 1) + ": nameid(" + reference.mName + ") has no matching NameID declaration in any loaded file";
		if (dynamicDeclarationCount) message += " (" + std::to_string(dynamicDeclarationCount) + " NameID declaration(s) with computed names could not be checked)";
		addStoryEditorValidationIssue(message, reference.mPath, reference.mLine);
	}
}

static void imguiStoryValidation()
{
	auto& isWindowShown = gStoryEditorWindowVisibility.mValidate;
	imguiPrismAddTab("Story Editor", "Validate", &isWindowShown);
	if (!isWindowShown) return;

	ImGui::Begin("Story Validation", &isWindowShown);
	if (ImGui::Button("Run validation"))
	{
		runStoryEditorValidation();
	}
	ImGui::TextWrapped("Checks live objects (anims present in the loaded .air, text fonts loaded, texts within the screen, stale text names) and scans every loaded .def state file for nameid() references without a NameID declaration.");
	auto& v = gStoryEditorValidationData;
	if (v.mHasRun)
	{
		ImGui::Separator();
		const auto runStateText = (v.mRunState >= 0) ? (" (run at root state " + std::to_string(v.mRunState) + ")") : std::string();
		ImGui::Text("%d issue(s)%s", int(v.mIssues.size()), runStateText.c_str());
		for (size_t i = 0; i < v.mIssues.size(); i++)
		{
			auto& issue = v.mIssues[i];
			ImGui::PushID(int(i));
			ImGui::Bullet();
			ImGui::SameLine();
			ImGui::TextWrapped("%s", issue.mMessage.c_str());
			if (!issue.mPath.empty())
			{
				ImGui::SameLine();
				if (ImGui::SmallButton("Open"))
				{
					prism::imgui::openTextEditor(issue.mPath, (std::max)(0, issue.mLine));
				}
			}
			ImGui::PopID();
		}
		if (v.mIssues.empty()) ImGui::Text("No issues found.");
	}
	ImGui::End();
}

// state timeline
// Lists the current statedef's controllers sorted by their statically extractable trigger time (the common "triggerN = time = X" case), so you can see what fires when. Types and trigger lines are read textually from the source file via the M4 provenance, matching what jump-to-source shows. "Set time" moves the root machine's time-in-state so the entry fires on the next update

struct StoryEditorTimelineEntry {
	std::string mType;
	std::vector<std::string> mTriggers; // raw "trigger1 = time = 0" lines from the file
	int mHasTime = 0;
	int mTime = 0;
	int mHasProvenance = 0;
	std::string mPath;
	int mStateID = -1;
	int mControllerIndex = -1;
};

static struct {
	int mIsBuilt = 0;
	int mBuiltState = -1;
	std::vector<StoryEditorTimelineEntry> mEntries;
	bool mDoesFollowRootState = true;
	int mManualState = 0;
} gStoryEditorTimelineData;

static void resetStoryEditorTimeline()
{
	gStoryEditorTimelineData.mIsBuilt = 0;
	gStoryEditorTimelineData.mEntries.clear();
}

// all key = value pairs of the tGroupOffset-th group after the first group named tGroupName, same group search as readStoryEditorDefValue/saveMugenDefString
static bool readStoryEditorDefGroupKeyValues(const std::string& tPath, const std::string& tGroupName, int tGroupOffset, std::vector<std::pair<std::string, std::string>>* oKeyValues)
{
	if (!isFile(tPath.c_str())) return false;
	auto b = fileToBuffer(tPath.c_str());
	const std::string content((const char*)b.mData, size_t(b.mLength));
	freeBuffer(b);

	auto foundOriginal = false;
	auto inTargetGroup = false;
	int index = 0;
	size_t lineStart = 0;
	while (lineStart <= content.size())
	{
		auto lineEnd = content.find('\n', lineStart);
		if (lineEnd == std::string::npos) lineEnd = content.size();
		auto line = content.substr(lineStart, lineEnd - lineStart);
		lineStart = lineEnd + 1;
		if (!line.empty() && line.back() == '\r') line.pop_back();

		if (!line.empty() && line[0] == '[')
		{
			if (inTargetGroup) return true;
			std::string groupName;
			for (size_t i = 1; i < line.size() && line[i] != ']'; i++) groupName.push_back(line[i]);
			if (!foundOriginal)
			{
				if (isStoryEditorStringEqualCaseIndependent(groupName, tGroupName)) foundOriginal = true;
			}
			else if (index == tGroupOffset) inTargetGroup = true;
			else index++;
			continue;
		}
		if (!inTargetGroup) continue;
		const auto trimmed = getStoryEditorTrimmedString(line);
		if (trimmed.empty() || trimmed[0] == ';') continue;
		const auto equalsPosition = trimmed.find('=');
		if (equalsPosition == std::string::npos) continue;
		auto value = trimmed.substr(equalsPosition + 1);
		const auto commentPosition = value.find(';');
		if (commentPosition != std::string::npos) value = value.substr(0, commentPosition);
		oKeyValues->push_back({ getStoryEditorTrimmedString(trimmed.substr(0, equalsPosition)), getStoryEditorTrimmedString(value) });
	}
	return inTargetGroup;
}

// accepts exactly "time = N" / "time == N" / "time >= N" (-> N) / "time > N" (-> N+1) after whitespace stripping; anything else is not statically extractable
static bool parseStoryEditorTriggerTime(const std::string& tValue, int* oTime)
{
	std::string s;
	for (const char c : tValue)
	{
		if (!isspace((unsigned char)c)) s.push_back(char(tolower((unsigned char)c)));
	}
	if (s.compare(0, 4, "time")) return false;
	size_t position = 4;
	int add = 0;
	if (!s.compare(position, 2, "==") || !s.compare(position, 2, ">=")) position += 2;
	else if (!s.compare(position, 1, ">")) { position += 1; add = 1; }
	else if (!s.compare(position, 1, "=")) position += 1;
	else return false;
	if (position >= s.size()) return false;
	auto digitStart = position;
	if (s[digitStart] == '-' || s[digitStart] == '+') digitStart++;
	if (digitStart >= s.size()) return false;
	for (auto i = digitStart; i < s.size(); i++)
	{
		if (!isdigit((unsigned char)s[i])) return false;
	}
	*oTime = atoi(s.c_str() + position) + add;
	return true;
}

static void buildStoryEditorTimeline(int tState)
{
	auto& t = gStoryEditorTimelineData;
	t.mEntries.clear();
	t.mIsBuilt = 1;
	t.mBuiltState = tState;
	const auto stateIt = gDolmexicaStoryScreenData.mStoryStates.mStates.find(tState);
	if (stateIt == gDolmexicaStoryScreenData.mStoryStates.mStates.end()) return;

	const auto controllerAmount = vector_size(&stateIt->second.mControllers);
	for (int i = 0; i < controllerAmount; i++)
	{
		StoryEditorTimelineEntry entry;
		const auto controller = (DreamMugenStateController*)vector_get(&stateIt->second.mControllers, i);
		const auto provenance = getDreamMugenStateControllerProvenance(controller);
		if (provenance)
		{
			entry.mHasProvenance = 1;
			entry.mPath = provenance->mScriptPath;
			entry.mStateID = provenance->mStateID;
			entry.mControllerIndex = provenance->mControllerIndex;
			std::vector<std::pair<std::string, std::string>> keyValues;
			if (readStoryEditorDefGroupKeyValues(entry.mPath, "statedef " + std::to_string(entry.mStateID), entry.mControllerIndex, &keyValues))
			{
				for (const auto& keyValuePair : keyValues)
				{
					auto key = keyValuePair.first;
					for (auto& c : key) c = char(tolower((unsigned char)c));
					if (key == "type") entry.mType = keyValuePair.second;
					else if (!key.compare(0, 7, "trigger"))
					{
						entry.mTriggers.push_back(keyValuePair.first + " = " + keyValuePair.second);
						int time;
						if (parseStoryEditorTriggerTime(keyValuePair.second, &time) && (!entry.mHasTime || time < entry.mTime))
						{
							entry.mHasTime = 1;
							entry.mTime = time;
						}
					}
				}
			}
		}
		if (entry.mType.empty()) entry.mType = "<unknown>";
		t.mEntries.push_back(std::move(entry));
	}
	// simple-time entries sorted by time, everything else keeps file order at the end
	std::stable_sort(t.mEntries.begin(), t.mEntries.end(), [](const StoryEditorTimelineEntry& a, const StoryEditorTimelineEntry& b) {
		const auto timeA = a.mHasTime ? a.mTime : INT_MAX;
		const auto timeB = b.mHasTime ? b.mTime : INT_MAX;
		return timeA < timeB;
	});
}

static void imguiStoryTimeline()
{
	auto& isWindowShown = gStoryEditorWindowVisibility.mTimeline;
	imguiPrismAddTab("Story Editor", "Timeline", &isWindowShown);
	if (!isWindowShown) return;

	ImGui::Begin("Story Timeline", &isWindowShown);
	if (gDolmexicaStoryScreenData.mHelperInstances.empty())
	{
		ImGui::Text("No story instances active.");
		ImGui::End();
		return;
	}
	auto& t = gStoryEditorTimelineData;
	const auto root = getDolmexicaStoryRootInstance();
	const auto currentRootState = getDolmexicaStoryStateNumber(root);
	ImGui::Text("Root state = %d, time in state = %d", currentRootState, getDolmexicaStoryTimeInState(root));

	ImGui::Checkbox("Follow root state", &t.mDoesFollowRootState);
	auto state = currentRootState;
	if (t.mDoesFollowRootState)
	{
		t.mManualState = currentRootState;
	}
	else
	{
		ImGui::SetNextItemWidth(100.f);
		ImGui::InputInt("Statedef", &t.mManualState);
		state = t.mManualState;
	}
	ImGui::SameLine();
	if (ImGui::SmallButton("Refresh") || !t.mIsBuilt || t.mBuiltState != state) buildStoryEditorTimeline(state);

	ImGui::TextWrapped("Times are parsed textually from simple 'time = N' triggers; entries with complex triggers sort last as [t=?]. 'Set time' sets the root machine's time-in-state so the entry fires on the next update (controller persistence is reset; other exact-match times are skipped over, not re-fired). While paused, it applies once stepped/resumed.");
	ImGui::Separator();

	if (t.mEntries.empty()) ImGui::Text("Statedef %d has no controllers (or does not exist).", state);
	for (size_t i = 0; i < t.mEntries.size(); i++)
	{
		auto& entry = t.mEntries[i];
		ImGui::PushID(int(i));
		if (entry.mHasTime) ImGui::Text("[t=%d] %s", entry.mTime, entry.mType.c_str());
		else ImGui::Text("[t=?] %s", entry.mType.c_str());
		if (!entry.mTriggers.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
		{
			std::string tooltip;
			for (const auto& trigger : entry.mTriggers) tooltip += (tooltip.empty() ? "" : "\n") + trigger;
			ImGui::SetTooltip("%s", tooltip.c_str());
		}
		if (entry.mHasProvenance)
		{
			ImGui::SameLine();
			if (ImGui::SmallButton("Open"))
			{
				openStoryEditorSourceLocation(entry.mPath, "statedef " + std::to_string(entry.mStateID), entry.mControllerIndex);
			}
		}
		if (entry.mHasTime && state == currentRootState)
		{
			ImGui::SameLine();
			if (ImGui::SmallButton("Set time"))
			{
				// time increments before the controllers evaluate, so N-1 makes the triggers see exactly N on the next update, state changes use the same -1 convention
				setDreamRegisteredStateTimeInState(root->mRegisteredStateMachine, entry.mTime - 1);
				resetDreamRegisteredStateMachineControllerPersistence(root->mRegisteredStateMachine);
			}
		}
		ImGui::PopID();
	}
	ImGui::End();
}

// camera controls
// Live overrides for the story camera (CameraFocus/CameraZoom controller equivalents) plus a coordinate-space grid / safe-area overlay for the 320-base coordinate system. Panel-only like color/opacity: nothing here is written back to .def

enum class StoryEditorCameraField : int {
	FocusX,
	FocusY,
	Zoom,
};

static struct {
	bool mDoesShowGrid = false;
	int mGridSpacing = 16;
	bool mDoesShowSafeArea = false;
	int mSafeAreaMarginPercent = 10;
} gStoryEditorCameraData;

// the camera is global, so appliers always succeed, undo commands never go stale
static bool applyStoryEditorCameraField(StoryEditorCameraField tField, float tValue)
{
	switch (tField)
	{
	case StoryEditorCameraField::FocusX:
		setDolmexicaStoryCameraFocusX(tValue);
		break;
	case StoryEditorCameraField::FocusY:
		setDolmexicaStoryCameraFocusY(tValue);
		break;
	case StoryEditorCameraField::Zoom:
		setDolmexicaStoryCameraZoom(tValue);
		break;
	}
	return true;
}

static void imguiStoryEditorCameraDragDouble(const char* tLabel, StoryEditorCameraField tField, float tCurrentValue, float tSpeed, float tMin, float tMax)
{
	const auto widgetID = ImGui::GetID(tLabel);
	auto value = float(tCurrentValue);
	if (ImGui::DragFloat(tLabel, &value, tSpeed, tMin, tMax))
	{
		applyStoryEditorCameraField(tField, value);
	}
	float oldValue;
	if (updateStoryEditorEditBoundary(widgetID, &tCurrentValue, 1, &oldValue))
	{
		const float newValue = value;
		pushStoryEditorCommand(
			std::string("Camera ") + tLabel + " = " + std::to_string(newValue), "",
			[tField, newValue]() { return applyStoryEditorCameraField(tField, newValue); },
			[tField, oldValue]() { return applyStoryEditorCameraField(tField, oldValue); });
	}
}

// grid + safe area draw over the logical screen, independent of the scene layer so it also works while just watching
static void drawStoryEditorCameraGrid()
{
	auto& c = gStoryEditorCameraData;
	if (!c.mDoesShowGrid && !c.mDoesShowSafeArea) return;
	if (gDolmexicaStoryScreenData.mHelperInstances.empty()) return;
	static const ImU32 STORY_EDITOR_GRID_MINOR_COLOR = IM_COL32(255, 255, 255, 28);
	static const ImU32 STORY_EDITOR_GRID_CENTER_COLOR = IM_COL32(255, 255, 255, 70);
	static const ImU32 STORY_EDITOR_GRID_SAFE_AREA_COLOR = IM_COL32(255, 180, 60, 130);

	const auto drawList = ImGui::GetBackgroundDrawList(ImGui::GetMainViewport());
	const auto screenSize = getScreenSize();
	if (c.mDoesShowGrid && c.mGridSpacing > 0)
	{
		for (int x = 0; x <= screenSize.x; x += c.mGridSpacing)
		{
			drawList->AddLine(getStoryEditorSceneWindowPosition(Vector2D(x, 0)), getStoryEditorSceneWindowPosition(Vector2D(x, screenSize.y)), (x * 2 == screenSize.x) ? STORY_EDITOR_GRID_CENTER_COLOR : STORY_EDITOR_GRID_MINOR_COLOR, 1.f);
		}
		for (int y = 0; y <= screenSize.y; y += c.mGridSpacing)
		{
			drawList->AddLine(getStoryEditorSceneWindowPosition(Vector2D(0, y)), getStoryEditorSceneWindowPosition(Vector2D(screenSize.x, y)), (y * 2 == screenSize.y) ? STORY_EDITOR_GRID_CENTER_COLOR : STORY_EDITOR_GRID_MINOR_COLOR, 1.f);
		}
	}
	if (c.mDoesShowSafeArea)
	{
		const auto marginX = screenSize.x * c.mSafeAreaMarginPercent / 100.0;
		const auto marginY = screenSize.y * c.mSafeAreaMarginPercent / 100.0;
		drawList->AddRect(getStoryEditorSceneWindowPosition(Vector2D(marginX, marginY)), getStoryEditorSceneWindowPosition(Vector2D(screenSize.x - marginX, screenSize.y - marginY)), STORY_EDITOR_GRID_SAFE_AREA_COLOR, 0.f, 0, 1.f);
		drawStoryEditorSceneLabel(drawList, getStoryEditorSceneWindowPosition(Vector2D(marginX + 2, marginY + 2)), STORY_EDITOR_GRID_SAFE_AREA_COLOR, "safe area");
	}
}

static void imguiStoryCamera()
{
	auto& isWindowShown = gStoryEditorWindowVisibility.mCamera;
	imguiPrismAddTab("Story Editor", "Camera", &isWindowShown);
	if (!isWindowShown) return;

	ImGui::Begin("Story Camera", &isWindowShown);
	const auto focus = *getDreamMugenStageHandlerCameraEffectPositionReference();
	const auto zoom = getDreamMugenStageHandlerCameraZoomReference()->x;
	const auto cameraPosition = *getDreamMugenStageHandlerCameraPositionReference();
	ImGui::Text("Camera position = (%.2f, %.2f)", cameraPosition.x, cameraPosition.y);

	imguiStoryEditorCameraDragDouble("FocusX", StoryEditorCameraField::FocusX, focus.x, 0.5f, -1000.f, 1000.f);
	imguiStoryEditorCameraDragDouble("FocusY", StoryEditorCameraField::FocusY, focus.y, 0.5f, -1000.f, 1000.f);
	imguiStoryEditorCameraDragDouble("Zoom", StoryEditorCameraField::Zoom, zoom, 0.01f, 0.1f, 10.f);
	if (ImGui::SmallButton("Reset camera"))
	{
		const auto oldFocus = focus;
		const auto oldZoom = zoom;
		applyStoryEditorCameraField(StoryEditorCameraField::FocusX, 0);
		applyStoryEditorCameraField(StoryEditorCameraField::FocusY, 0);
		applyStoryEditorCameraField(StoryEditorCameraField::Zoom, 1);
		pushStoryEditorCommand(
			"Camera reset", "",
			[]() { applyStoryEditorCameraField(StoryEditorCameraField::FocusX, 0); applyStoryEditorCameraField(StoryEditorCameraField::FocusY, 0); return applyStoryEditorCameraField(StoryEditorCameraField::Zoom, 1); },
			[oldFocus, oldZoom]() { applyStoryEditorCameraField(StoryEditorCameraField::FocusX, oldFocus.x); applyStoryEditorCameraField(StoryEditorCameraField::FocusY, oldFocus.y); return applyStoryEditorCameraField(StoryEditorCameraField::Zoom, oldZoom); });
	}
	ImGui::TextWrapped("Focus/zoom mirror the CameraFocus/CameraZoom controllers and only affect the stage and stage-bound objects. Panel-only: not written back to .def. A later CameraFocus/CameraZoom controller overwrites these values.");
	ImGui::Separator();

	auto& c = gStoryEditorCameraData;
	ImGui::Checkbox("Show grid", &c.mDoesShowGrid);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(80.f);
	if (ImGui::InputInt("Spacing", &c.mGridSpacing)) c.mGridSpacing = (std::max)(2, c.mGridSpacing);
	ImGui::Checkbox("Show safe area", &c.mDoesShowSafeArea);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(80.f);
	if (ImGui::InputInt("Margin %", &c.mSafeAreaMarginPercent)) c.mSafeAreaMarginPercent = (std::max)(0, (std::min)(45, c.mSafeAreaMarginPercent));
	ImGui::End();
}

// sound/music panel
// Trigger log of every story-mode sound play (text/continue sounds + PlaySnd controllers, via the storyEditorFilterStorySoundPlay hook), mute, per-entry replay, and live music controls

struct StoryEditorSoundLogEntry {
	int mFrame = 0;
	int mRootState = -1;
	int mGroup = 0;
	int mItem = 0;
	float mVolume = 1.0;
	int mChannel = -1;
	float mFrequencyMultiplier = 1.0;
	int mIsLooping = 0;
	float mPanning = 0.0;
	std::string mSource;
	int mWasMuted = 0;
};

static struct {
	bool mIsMuted = false; // survives scene loads, the log does not
	std::vector<StoryEditorSoundLogEntry> mLog; // newest last
} gStoryEditorSoundData;

static void resetStoryEditorSoundLog()
{
	gStoryEditorSoundData.mLog.clear();
}

// called from every story-mode sound play site, logs the play and returns whether it should actually happen (0 while muted)
int storyEditorFilterStorySoundPlay(int tGroup, int tItem, float tVolume, int tChannel, float tFrequencyMultiplier, int tIsLooping, float tPanning, const char* tSource)
{
	if (!isInDevelopMode()) return 1;
	static const size_t STORY_EDITOR_SOUND_LOG_MAX = 64;
	auto& s = gStoryEditorSoundData;
	StoryEditorSoundLogEntry entry;
	entry.mFrame = ImGui::GetFrameCount();
	entry.mRootState = gDolmexicaStoryScreenData.mHelperInstances.empty() ? -1 : getDolmexicaStoryStateNumber(getDolmexicaStoryRootInstance());
	entry.mGroup = tGroup;
	entry.mItem = tItem;
	entry.mVolume = tVolume;
	entry.mChannel = tChannel;
	entry.mFrequencyMultiplier = tFrequencyMultiplier;
	entry.mIsLooping = tIsLooping;
	entry.mPanning = tPanning;
	entry.mSource = tSource;
	entry.mWasMuted = s.mIsMuted;
	s.mLog.push_back(std::move(entry));
	if (s.mLog.size() > STORY_EDITOR_SOUND_LOG_MAX) s.mLog.erase(s.mLog.begin());
	return !s.mIsMuted;
}

static void imguiStorySound()
{
	auto& isWindowShown = gStoryEditorWindowVisibility.mSound;
	imguiPrismAddTab("Story Editor", "Sound", &isWindowShown);
	if (!isWindowShown) return;

	ImGui::Begin("Story Sound", &isWindowShown);
	auto& s = gStoryEditorSoundData;
	ImGui::Checkbox("Mute story sounds", &s.mIsMuted);
	ImGui::SameLine();
	ImGui::TextDisabled("(text/continue sounds + PlaySnd controllers; music unaffected)");

	ImGui::Text("Music: %s", isPlayingStreamingMusic() ? "playing" : "stopped/paused");
	if (ImGui::SmallButton("Pause music")) pauseDolmexicaStoryMusic();
	ImGui::SameLine();
	if (ImGui::SmallButton("Resume music")) resumeDolmexicaStoryMusic();
	ImGui::SameLine();
	if (ImGui::SmallButton("Stop music")) stopDolmexicaStoryMusic();

	ImGui::Separator();
	ImGui::Text("Sound log (%d, newest first)", int(s.mLog.size()));
	ImGui::SameLine();
	if (ImGui::SmallButton("Clear")) s.mLog.clear();
	for (int i = int(s.mLog.size()) - 1; i >= 0; i--)
	{
		const auto& entry = s.mLog[i];
		ImGui::PushID(i);
		ImGui::Text("(%d,%d) vol=%.2f ch=%d freq=%.2f%s pan=%.2f  %s [state %d]%s", entry.mGroup, entry.mItem, entry.mVolume, entry.mChannel, entry.mFrequencyMultiplier, entry.mIsLooping ? " loop" : "", entry.mPanning, entry.mSource.c_str(), entry.mRootState, entry.mWasMuted ? " (muted)" : "");
		ImGui::SameLine();
		if (ImGui::SmallButton("Replay"))
		{
			tryPlayMugenSoundAdvanced(getDolmexicaStorySounds(), entry.mGroup, entry.mItem, entry.mVolume, entry.mChannel, entry.mFrequencyMultiplier, entry.mIsLooping, entry.mPanning);
		}
		ImGui::PopID();
	}
	if (s.mLog.empty()) ImGui::TextDisabled("No sounds played yet.");
	ImGui::End();
}

// variable watch with breakpoints
// Pauses playback (like the Pause button) when a watched variable changes or a state machine changes state, helps debug branching stories. Checks run right after the state machines have updated, but the pause itself lands on the following frame, so the scene advances one more tick before freezing

static const char* gStoryEditorVariableKindNames[] = { "int", "float", "string" };
static const char* gStoryEditorVariableKindPrefixes[] = { "var", "fvar", "svar" };

struct StoryEditorWatch {
	int mInstanceKey = -1;
	int mVariableKind = 0; // index into the name/prefix tables
	int mID = 0;
	bool mDoesBreakOnChange = true;
	bool mHasLastValue = false;
	std::string mLastValue;
};

static struct {
	std::vector<StoryEditorWatch> mWatches;
	bool mDoesBreakOnRootStateChange = false;
	bool mDoesBreakOnAnyStateChange = false;
	std::map<int, int> mLastInstanceStates;
	std::string mLastBreakMessage;
	int mAddInstanceKey = -1;
	int mAddVariableKind = 0;
	int mAddID = 0;
} gStoryEditorWatchData;

static void resetStoryEditorWatchSnapshots()
{
	gStoryEditorWatchData.mLastInstanceStates.clear();
	for (auto& watch : gStoryEditorWatchData.mWatches) watch.mHasLastValue = false;
	gStoryEditorWatchData.mLastBreakMessage.clear();
}

static void addStoryEditorWatch(int tInstanceKey, int tVariableKind, int tID)
{
	for (const auto& watch : gStoryEditorWatchData.mWatches)
	{
		if (watch.mInstanceKey == tInstanceKey && watch.mVariableKind == tVariableKind && watch.mID == tID) return;
	}
	StoryEditorWatch watch;
	watch.mInstanceKey = tInstanceKey;
	watch.mVariableKind = tVariableKind;
	watch.mID = tID;
	gStoryEditorWatchData.mWatches.push_back(watch);
	gStoryEditorWindowVisibility.mWatch = true; // make the add visible
}

static bool getStoryEditorWatchValue(const StoryEditorWatch& tWatch, std::string* oValue)
{
	const auto instance = getStoryEditorInstance(tWatch.mInstanceKey);
	if (!instance) return false;
	switch (tWatch.mVariableKind)
	{
	case 0:
		if (!stl_map_contains(instance->mIntVars, tWatch.mID)) return false;
		*oValue = std::to_string(instance->mIntVars[tWatch.mID]);
		return true;
	case 1:
		if (!stl_map_contains(instance->mFloatVars, tWatch.mID)) return false;
		*oValue = std::to_string(instance->mFloatVars[tWatch.mID]);
		return true;
	case 2:
		if (!stl_map_contains(instance->mStringVars, tWatch.mID)) return false;
		*oValue = instance->mStringVars[tWatch.mID];
		return true;
	default:
		return false;
	}
}

static std::string getStoryEditorWatchDisplayName(const StoryEditorWatch& tWatch)
{
	const auto instanceName = (tWatch.mInstanceKey == -1) ? std::string("Root") : ("Helper " + std::to_string(tWatch.mInstanceKey));
	return instanceName + " " + gStoryEditorVariableKindPrefixes[tWatch.mVariableKind] + "(" + std::to_string(tWatch.mID) + ")";
}

static void triggerStoryEditorWatchBreak(const std::string& tMessage)
{
	gStoryEditorWatchData.mLastBreakMessage = tMessage;
	gStoryEditorData.mIsPlaybackPaused = true;
	gStoryEditorData.mPendingPlaybackSteps = 0;
	gStoryEditorWindowVisibility.mWatch = true; // surface the reason
}

// snapshots always refresh, comparisons only fire while playback actually runs, so edits made through the panel while paused never trigger on resume
static void updateStoryEditorWatch()
{
	auto& w = gStoryEditorWatchData;
	const auto isRunning = !gStoryEditorData.mIsPlaybackHeld;

	std::map<int, int> currentStates;
	for (auto& instancePair : gDolmexicaStoryScreenData.mHelperInstances)
	{
		currentStates[instancePair.first] = getDolmexicaStoryStateNumber(&instancePair.second);
	}
	if (isRunning && (w.mDoesBreakOnRootStateChange || w.mDoesBreakOnAnyStateChange))
	{
		for (const auto& statePair : currentStates)
		{
			const auto it = w.mLastInstanceStates.find(statePair.first);
			if (it == w.mLastInstanceStates.end() || it->second == statePair.second) continue;
			if (!w.mDoesBreakOnAnyStateChange && !(w.mDoesBreakOnRootStateChange && statePair.first == -1)) continue;
			const auto instanceName = (statePair.first == -1) ? std::string("Root") : ("Helper " + std::to_string(statePair.first));
			triggerStoryEditorWatchBreak("Breakpoint: " + instanceName + " state " + std::to_string(it->second) + " -> " + std::to_string(statePair.second));
			break;
		}
	}
	w.mLastInstanceStates = std::move(currentStates);

	for (auto& watch : w.mWatches)
	{
		std::string value;
		if (!getStoryEditorWatchValue(watch, &value))
		{
			watch.mHasLastValue = false; // variable gone (instance removed / var never set), re-baselines when it reappears
			continue;
		}
		if (isRunning && watch.mHasLastValue && value != watch.mLastValue && watch.mDoesBreakOnChange)
		{
			triggerStoryEditorWatchBreak("Breakpoint: " + getStoryEditorWatchDisplayName(watch) + " changed " + watch.mLastValue + " -> " + value);
		}
		watch.mLastValue = value;
		watch.mHasLastValue = true;
	}
}

static void imguiStoryWatch()
{
	auto& isWindowShown = gStoryEditorWindowVisibility.mWatch;
	imguiPrismAddTab("Story Editor", "Watch", &isWindowShown);
	if (!isWindowShown) return;

	ImGui::Begin("Story Watch", &isWindowShown);
	auto& w = gStoryEditorWatchData;
	ImGui::Checkbox("Break on root state change", &w.mDoesBreakOnRootStateChange);
	ImGui::Checkbox("Break on any instance state change", &w.mDoesBreakOnAnyStateChange);
	if (!w.mLastBreakMessage.empty()) ImGui::TextWrapped("%s", w.mLastBreakMessage.c_str());

	ImGui::Separator();
	// manual add (the var rows in the Instances window have one-click Watch buttons too)
	{
		const auto addInstanceLabel = (w.mAddInstanceKey == -1) ? std::string("Root (-1)") : ("Helper " + std::to_string(w.mAddInstanceKey));
		ImGui::SetNextItemWidth(110.f);
		if (ImGui::BeginCombo("##WatchInstance", addInstanceLabel.c_str()))
		{
			for (const auto& instancePair : gDolmexicaStoryScreenData.mHelperInstances)
			{
				const auto label = (instancePair.first == -1) ? std::string("Root (-1)") : ("Helper " + std::to_string(instancePair.first));
				if (ImGui::Selectable(label.c_str(), instancePair.first == w.mAddInstanceKey)) w.mAddInstanceKey = instancePair.first;
			}
			ImGui::EndCombo();
		}
		ImGui::SameLine();
		ImGui::SetNextItemWidth(70.f);
		ImGui::Combo("##WatchKind", &w.mAddVariableKind, gStoryEditorVariableKindNames, 3);
		ImGui::SameLine();
		ImGui::SetNextItemWidth(70.f);
		ImGui::InputInt("##WatchID", &w.mAddID);
		ImGui::SameLine();
		if (ImGui::SmallButton("Add")) addStoryEditorWatch(w.mAddInstanceKey, w.mAddVariableKind, w.mAddID);
	}

	int pendingRemove = -1;
	for (size_t i = 0; i < w.mWatches.size(); i++)
	{
		auto& watch = w.mWatches[i];
		ImGui::PushID(int(i));
		std::string value;
		const auto hasValue = getStoryEditorWatchValue(watch, &value);
		ImGui::Text("%s = %s", getStoryEditorWatchDisplayName(watch).c_str(), hasValue ? value.c_str() : "<not set>");
		ImGui::SameLine();
		ImGui::Checkbox("break", &watch.mDoesBreakOnChange);
		ImGui::SameLine();
		if (ImGui::SmallButton("Remove")) pendingRemove = int(i);
		ImGui::PopID();
	}
	if (pendingRemove >= 0) w.mWatches.erase(w.mWatches.begin() + pendingRemove);
	if (w.mWatches.empty()) ImGui::TextDisabled("No watches. Use the Watch buttons next to the vars in the Instances window, or add one above.");
	ImGui::TextWrapped("A hit pauses playback like the Pause button; the pause lands on the following frame, so the scene advances one more tick. Changes made through the panel while paused do not trigger. Watches survive scene reloads.");
	ImGui::End();
}

void imguiStoryScreen()
{
	if (!isImguiPrismActive()) return;

	static bool isWindowShown = false;
	imguiPrismAddTab("Screen", "General", &isWindowShown);
	if (isWindowShown)
	{
		ImGui::Begin("General", &isWindowShown);
		ImGui::Text("Path = %s", gDolmexicaStoryScreenData.mPath);
		imguiMugenSpriteFile(gDolmexicaStoryScreenData.mSprites, "Sprites");
		//imguiMugenAnimationFile(gDolmexicaStoryScreenData.mAnimations, "Animations");
		//imguiMugenSounds(gDolmexicaStoryScreenData.mSounds);

		ImGui::Text("HasCommands = %d", gDolmexicaStoryScreenData.mHasCommands);
		ImGui::Text("CommandID = %d", gDolmexicaStoryScreenData.mCommandID);
		//imguiMugenCommands(gDolmexicaStoryScreenData.mCommands);

		imguiMugenStates("StoryStates", gDolmexicaStoryScreenData.mStoryStates, gDolmexicaStoryScreenData.mPath);


		ImGui::Text("HasStage = %d", gDolmexicaStoryScreenData.mHasStage);
		ImGui::Text("HasMusic = %d", gDolmexicaStoryScreenData.mHasMusic);
		ImGui::Text("HasFonts = %d", gDolmexicaStoryScreenData.mHasFonts);

		ImGui::End();

	}

	imguiStoryInstances();
	imguiStoryPlayback();
	imguiStoryUndo();
	imguiStoryScene();
	imguiStorySave();
	imguiStoryValidation();
	imguiStoryTimeline();
	imguiStoryCamera();
	imguiStorySound();
	imguiStoryWatch();
	updateStoryEditorScene();
	drawStoryEditorCameraGrid();

	// global routing loses against an active InputText, which owns Ctrl+Z/Ctrl+Y for its own editing
	if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Z, ImGuiInputFlags_RouteGlobal)) undoStoryEditorCommand();
	if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Y, ImGuiInputFlags_RouteGlobal) || ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_Z, ImGuiInputFlags_RouteGlobal)) redoStoryEditorCommand();
	if (ImGui::Shortcut(ImGuiKey_F5, ImGuiInputFlags_RouteGlobal)) reloadStoryScreenAtCurrentState();
	// Ctrl+O opens the story file at the root's active statedef in the built-in text editor
	if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_O, ImGuiInputFlags_RouteGlobal)) openStoryEditorCurrentStateSource();
	// Ctrl+E opens every story editor window at once (skipping the manual open-each-window ritual), pressing it again while all are open closes them all
	if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_E, ImGuiInputFlags_RouteGlobal))
	{
		auto& v = gStoryEditorWindowVisibility;
		const auto allShown = v.mInstances && v.mPlayback && v.mUndo && v.mScene && v.mSave && v.mValidate && v.mTimeline && v.mCamera && v.mSound && v.mWatch;
		v.mInstances = v.mPlayback = v.mUndo = v.mScene = v.mSave = v.mValidate = v.mTimeline = v.mCamera = v.mSound = v.mWatch = !allShown;
	}
}
#endif

static void loadSystemFonts(void*) {
	unloadMugenFonts();
	loadMugenSystemFonts();
}

static void loadStoryFonts(void*) {
	unloadMugenFonts();
	loadMugenStoryFonts(gDolmexicaStoryScreenData.mPath, "info");
}

static void loadStoryFilesFromScript(MugenDefScript* tScript) {
	char pathToFile[1024];
	char path[1024];
	char fullPath[1024];

	getPathToFile(pathToFile, gDolmexicaStoryScreenData.mPath);

	getMugenDefStringOrDefault(path, tScript, "info", "spr", "");
	sprintf(fullPath, "%s%s", pathToFile, path);
	gDolmexicaStoryScreenData.mSprites = loadMugenSpriteFileWithoutPalette(fullPath);

	getMugenDefStringOrDefault(path, tScript, "info", "anim", "");
	sprintf(fullPath, "%s%s", pathToFile, path);
	gDolmexicaStoryScreenData.mAnimations = loadMugenAnimationFile(fullPath);

	getMugenDefStringOrDefault(path, tScript, "info", "snd", "");
	sprintf(fullPath, "%s%s", pathToFile, path);
	if (isFile(fullPath)) {
		gDolmexicaStoryScreenData.mSounds = loadMugenSoundFile(fullPath);
	}
	else {
		gDolmexicaStoryScreenData.mSounds = createEmptyMugenSoundFile();
	}

	getMugenDefStringOrDefault(path, tScript, "info", "cmd", "");
	sprintf(fullPath, "%s%s", pathToFile, path);
	gDolmexicaStoryScreenData.mHasCommands = isFile(fullPath);
	if (gDolmexicaStoryScreenData.mHasCommands) {
		gDolmexicaStoryScreenData.mCommands = loadDreamMugenCommandFile(fullPath);
		gDolmexicaStoryScreenData.mCommandID = registerDreamMugenCommands(0, &gDolmexicaStoryScreenData.mCommands);
	}

	getMugenDefStringOrDefault(path, tScript, "info", "stage", "");
	sprintf(fullPath, "%s%s", getDolmexicaAssetFolder().c_str(), path);
	const auto musicPath = getSTLMugenDefStringOrDefault(tScript, "info", "bgm", "");
	setDreamStageMugenDefinition(fullPath, musicPath.c_str());
	gDolmexicaStoryScreenData.mHasStage = isFile(fullPath);
	gDolmexicaStoryScreenData.mHasMusic = isMugenBGMMusicPath(musicPath.c_str(), fullPath);
	gDolmexicaStoryScreenData.mHasMusic |= getMugenDefIntegerOrDefault(tScript, "info", "stage.bgm", 0);

	for (int i = 0; i < 10; i++) {
		char name[20];
		
		sprintf(name, "helper%d", i);
		getMugenDefStringOrDefault(path, tScript, "info", name, "");
		sprintf(fullPath, "%s%s", pathToFile, path);
		if (!isFile(fullPath)) continue;

		loadDreamMugenStateDefinitionsFromFile(&gDolmexicaStoryScreenData.mStoryStates, fullPath);
	}

	if (gDolmexicaStoryScreenData.mHasFonts) {
		setWrapperBetweenScreensCB(loadSystemFonts, NULL);
	}
}

static void initStoryInstance(StoryInstance& e){
	e.mRegisteredStateMachine = registerDreamMugenStoryStateMachine(&gDolmexicaStoryScreenData.mStoryStates, &e);
	e.mStoryAnimations.clear();
	e.mStoryTexts.clear();
	e.mStoryCharacters.clear();

	e.mIntVars.clear();
	e.mFloatVars.clear();
	e.mStringVars.clear();
	e.mTextNames.clear();
	e.mIsScheduledForDeletion = 0;
	e.mParent = &e;
}

static void unloadDolmexicaStoryAnimation(StoryAnimation& e);
static void unloadDolmexicaStoryText(StoryText* e);

static void unloadStoryInstance(StoryInstance& e) {
	for (auto& animation : e.mStoryAnimations) {
		unloadDolmexicaStoryAnimation(animation.second);
	}
	for (auto& text : e.mStoryTexts) {
		unloadDolmexicaStoryText(&text.second);
	}

	removeDreamRegisteredStateMachine(e.mRegisteredStateMachine);
}

static void loadStoryHelperRootInstance() {
	gDolmexicaStoryScreenData.mHelperInstances.clear();
	StoryInstance root;
	gDolmexicaStoryScreenData.mHelperInstances[-1] = root;
	initStoryInstance(gDolmexicaStoryScreenData.mHelperInstances[-1]);

	updateDreamSingleStateMachineByID(gDolmexicaStoryScreenData.mHelperInstances[-1].mRegisteredStateMachine);

}

static void loadStoryScreen() {
	setupDreamStoryAssignmentEvaluator();
	setupDreamMugenStoryStateControllerHandler();
	instantiateActor(getDreamMugenStateHandler());
	instantiateActor(getDreamMugenCommandHandler());

#ifdef _WIN32
	resetStoryEditorSaveData(); // before the state files parse (provenance recording) and before state 0 executes (object stamps)
#endif
	gDolmexicaStoryScreenData.mStoryStates = createEmptyMugenStates();
	loadDreamMugenStateDefinitionsFromFile(&gDolmexicaStoryScreenData.mStoryStates, gDolmexicaStoryScreenData.mPath);

	MugenDefScript script;
	loadMugenDefScript(&script, gDolmexicaStoryScreenData.mPath);
	loadStoryFilesFromScript(&script);
	unloadMugenDefScript(&script);
	
	if (gDolmexicaStoryScreenData.mHasStage) {
		instantiateActor(getDreamStageBP());
		setDreamStageNoAutomaticCameraMovement();
	}

	loadStoryHelperRootInstance();

	if (gDolmexicaStoryScreenData.mHasMusic) {
		playDreamStageMusic();
	}

#ifdef _WIN32
	resetStoryEditorPlayback();
	gStoryEditorData.mIsStoryScreenActive = true;
#endif
}

static void unloadStoryScreen() {
	gDolmexicaStoryScreenData.mHelperInstances.clear();
	shutdownDreamMugenStateControllerHandler();
}

// all story-screen sound plays funnel through here so the editor's sound log/mute hook sees them
static void playStoryScreenTextSound(const Vector2DI& tSound, const char* tSource) {
	const auto volume = parseGameMidiVolumeToPrism(getGameMidiVolume());
#ifdef _WIN32
	if (!storyEditorFilterStorySoundPlay(tSound.x, tSound.y, volume, -1, 1.0, 0, 0.0, tSource)) return;
#endif
	tryPlayMugenSoundAdvanced(&gDolmexicaStoryScreenData.mSounds, tSound.x, tSound.y, volume);
}

static int updateSingleTextInputAndSound(StoryInstance* tInstance, StoryText* e) {
	if (e->mHasFinished) return 0;

	if (e->mHasContinue) {
		if (isMugenTextBuiltUp(e->mTextID)) {
			setMugenAnimationVisibility(e->mContinueAnimationElement, 1);
		}
	}

	if (hasPressedAFlank()) {
		if (isMugenTextBuiltUp(e->mTextID)) {
			if (e->mGoesToNextState) {
				changeDolmexicaStoryStateOutsideStateHandler(tInstance, e->mNextState);
				e->mGoesToNextState = 0;
				e->mHasFinished = 1;
				setDolmexicaStoryTextInactive(tInstance, e->mID);
				if (e->mHasContinueSound)
				{
					playStoryScreenTextSound(e->mContinueSound, "continue sound");
				}
			}
		}
		else {
			setMugenTextBuiltUp(e->mTextID);
			if (e->mHasContinueSound)
			{
				playStoryScreenTextSound(e->mContinueSound, "continue sound");
			}
		}
	}

	if (e->mHasTextSound && !isMugenTextBuiltUp(e->mTextID) && strlen(getMugenTextDisplayedText(e->mTextID)) % e->mTextSoundFrequency == 0)
	{
		playStoryScreenTextSound(e->mTextSound, "text sound");
	}

	return 0;
}

static void updateLockedTextPosition(StoryInstance* tInstance, StoryText* e) {
	if (!e->mIsLockedOnToCharacter) return;

	auto p = *e->mLockCharacterPositionReference + e->mLockOffset;
	if (e->mLockCharacterIsBoundToStage)
	{
		p = vecSub(p, *getDreamMugenStageHandlerCameraPositionReference());
	}
	setDolmexicaStoryTextBasePosition(tInstance, e->mID, p.xy());
}

static int updateSingleText(StoryInstance* instance, StoryText& tData) {
	StoryText* e = &tData;
	updateLockedTextPosition(instance, e);
	if (e->mIsDisabled) return 0;

	return updateSingleTextInputAndSound(instance, e);
}

static void updateTexts(StoryInstance& e) {
	stl_int_map_remove_predicate(e.mStoryTexts, updateSingleText, &e);
}

static int updateSingleAnimation(void* /*tCaller*/, StoryAnimation& e) {
	return e.mIsDeleted;
}

static void updateAnimations(StoryInstance& e) {
	stl_int_map_remove_predicate(e.mStoryAnimations, updateSingleAnimation);
}

static int updateSingleInstance(void* /*tCaller*/, StoryInstance& tInstance) {
	if (tInstance.mIsScheduledForDeletion)
	{
		unloadStoryInstance(tInstance);
		return 1;
	}

	updateTexts(tInstance);
	updateAnimations(tInstance);

	return 0;
}

static void updateStoryScreen() {
#ifdef _WIN32
	updateStoryEditorPlayback();
	if (isStoryEditorHoldingPlayback()) return;
#endif
	stl_int_map_remove_predicate(gDolmexicaStoryScreenData.mHelperInstances, updateSingleInstance);
}

static Screen gDolmexicaStoryScreen;

Screen* getDolmexicaStoryScreen() {
	gDolmexicaStoryScreen = makeScreen(loadStoryScreen, updateStoryScreen, NULL, unloadStoryScreen, NULL
#ifdef _WIN32
		, imguiStoryScreen
#endif
	);
	return &gDolmexicaStoryScreen;
}

static void loadDolmexicaStoryActor(void*) {
#ifdef _WIN32
	resetStoryEditorSaveData();
#endif
	gDolmexicaStoryScreenData.mStoryStates = createEmptyMugenStates();
	loadDreamMugenStateDefinitionsFromFile(&gDolmexicaStoryScreenData.mStoryStates, getStoryHelperPath().c_str());
	loadStoryHelperRootInstance();
#ifdef _WIN32
	resetStoryEditorPlayback();
	gStoryEditorData.mIsStoryScreenActive = false; // fight-embedded story actor: reload/hot-reload paths stay off
#endif
}

static void unloadDolmexicaStoryActor(void*) {
	gDolmexicaStoryScreenData.mHelperInstances.clear();
}

static void updateDolmexicaStoryActor(void*) {
	updateStoryScreen();
}

ActorBlueprint getDolmexicaStoryActor()
{
	return makeActorBlueprint(loadDolmexicaStoryActor, unloadDolmexicaStoryActor, updateDolmexicaStoryActor);
}

static void setupFontTransition() {
	MugenDefScript script;
	loadMugenDefScript(&script, gDolmexicaStoryScreenData.mPath);

	gDolmexicaStoryScreenData.mHasFonts = 0;
	for (int i = 0; i < 100; i++) {
		char name[100];
		sprintf(name, "font%d", i);
		gDolmexicaStoryScreenData.mHasFonts |= isMugenDefStringVariable(&script, "info", name);
	}
	unloadMugenDefScript(&script);

	if (gDolmexicaStoryScreenData.mHasFonts) {
		setWrapperBetweenScreensCB(loadStoryFonts, NULL);
	}
}

void setDolmexicaStoryScreenFileAndPrepareScreen(const char * tPath)
{
	strcpy(gDolmexicaStoryScreenData.mPath, tPath);
	setupFontTransition();
}

MugenSounds* getDolmexicaStorySounds()
{
	return &gDolmexicaStoryScreenData.mSounds;
}

int isStoryCommandActive(const char* tCommand)
{
	if (!gDolmexicaStoryScreenData.mHasCommands) return 0;
	else return isDreamCommandActive(gDolmexicaStoryScreenData.mCommandID, tCommand);
}

static void initDolmexicaStoryAnimation(StoryAnimation& e, int tID, int tAnimationNumber, const Position& tPosition, MugenSpriteFile* mSprites, MugenAnimations* tAnimations) {
	e.mID = tID;
	e.mAnimationElement = addMugenAnimation(getMugenAnimation(tAnimations, tAnimationNumber), mSprites, tPosition);
	e.mIsBoundToStage = 0;
	e.mHasShadow = 0;
	e.mIsDeleted = 0;
}

void addDolmexicaStoryAnimation(StoryInstance* tInstance, int tID, int tAnimation, const Position2D& tPosition)
{
	if (stl_map_contains(tInstance->mStoryAnimations, tID)) {
		removeDolmexicaStoryAnimation(tInstance, tID);
	}
	const auto z = DOLMEXICA_STORY_ANIMATION_BASE_Z + tID * DOLMEXICA_STORY_ID_Z_FACTOR;
	initDolmexicaStoryAnimation(tInstance->mStoryAnimations[tID], tID, tAnimation, tPosition.xyz(z), &gDolmexicaStoryScreenData.mSprites, &gDolmexicaStoryScreenData.mAnimations);
}

static void unloadDolmexicaStoryAnimation(StoryAnimation& e) {
	removeMugenAnimation(e.mAnimationElement);
	if (e.mHasShadow) {
		removeMugenAnimation(e.mShadowAnimationElement);
	}

}

void removeDolmexicaStoryAnimation(StoryInstance* tInstance, int tID)
{
	StoryAnimation& e = tInstance->mStoryAnimations[tID];
	unloadDolmexicaStoryAnimation(e);
	tInstance->mStoryAnimations.erase(e.mID);
}

int getDolmexicaStoryAnimationIsLooping(StoryInstance* tInstance, int tID)
{
	StoryAnimation& e = tInstance->mStoryAnimations[tID];
	return getMugenAnimationIsLooping(e.mAnimationElement);
}

static void storyAnimationOverCB(void* tCaller) {
	StoryAnimation* e = (StoryAnimation*)tCaller;
	e->mIsDeleted = 1;
}

void setDolmexicaStoryAnimationLooping(StoryInstance* tInstance, int tID, int tIsLooping)
{
	StoryAnimation& e = tInstance->mStoryAnimations[tID];
	if (!tIsLooping) {
		setMugenAnimationNoLoop(e.mAnimationElement);
		if (e.mHasShadow) {
			setMugenAnimationNoLoop(e.mShadowAnimationElement);
		}

		setMugenAnimationCallback(e.mAnimationElement, storyAnimationOverCB, &e);
	}
}

int getDolmexicaStoryAnimationIsBoundToStage(StoryInstance* tInstance, int tID)
{
	StoryAnimation& e = tInstance->mStoryAnimations[tID];
	return e.mIsBoundToStage;
}

static void setDolmexicaStoryAnimationBoundToStageInternal(StoryAnimation& e, int tIsBoundToStage)
{
	if (tIsBoundToStage) {
		Position* p = getMugenAnimationPositionReference(e.mAnimationElement);
		*p = (*p) + getDreamStageCoordinateSystemOffset(COORD_P);
		p->x += (COORD_P / 2);
		setMugenAnimationCameraPositionReference(e.mAnimationElement, getDreamMugenStageHandlerCameraPositionReference());
		setMugenAnimationCameraEffectPositionReference(e.mAnimationElement, getDreamMugenStageHandlerCameraEffectPositionReference());
		setMugenAnimationCameraScaleReference(e.mAnimationElement, getDreamMugenStageHandlerCameraZoomReference());
		if (e.mHasShadow) {
			p = getMugenAnimationPositionReference(e.mShadowAnimationElement);
			*p = (*p) + getDreamStageCoordinateSystemOffset(COORD_P);
			p->x += (COORD_P / 2);
			setMugenAnimationCameraPositionReference(e.mShadowAnimationElement, getDreamMugenStageHandlerCameraPositionReference());
			setMugenAnimationCameraEffectPositionReference(e.mShadowAnimationElement, getDreamMugenStageHandlerCameraEffectPositionReference());
			setMugenAnimationCameraScaleReference(e.mShadowAnimationElement, getDreamMugenStageHandlerCameraZoomReference());
		}
		e.mIsBoundToStage = tIsBoundToStage;
	}
	
}

void setDolmexicaStoryAnimationBoundToStage(StoryInstance* tInstance, int tID, int tIsBoundToStage)
{
	StoryAnimation& e = tInstance->mStoryAnimations[tID];
	setDolmexicaStoryAnimationBoundToStageInternal(e, tIsBoundToStage);
}

int getDolmexicaStoryAnimationHasShadow(StoryInstance* tInstance, int tID)
{
	StoryAnimation& e = tInstance->mStoryAnimations[tID];
	return e.mHasShadow;
}

static void setDolmexicaStoryAnimationPositionXInternal(StoryAnimation& e, float tX);
static void setDolmexicaStoryAnimationPositionYInternal(StoryAnimation& e, float tY);

static void setDolmexicaStoryAnimationShadowInternal(StoryAnimation& e, float tBasePositionY, MugenSpriteFile* tSprites, MugenAnimations* tAnimations) {
	if (!isDrawingShadowsConfig()) return;
	e.mHasShadow = 1;
	e.mShadowBasePositionY = tBasePositionY;
	Position pos = getMugenAnimationPosition(e.mAnimationElement);
	pos.z = DOLMEXICA_STORY_SHADOW_BASE_Z;
	e.mShadowAnimationElement = addMugenAnimation(getMugenAnimation(tAnimations, getMugenAnimationAnimationNumber(e.mAnimationElement)), tSprites, pos);
	setMugenAnimationDrawScale(e.mShadowAnimationElement, Vector2D(1, -getDreamStageShadowScaleY()));
	Vector3D color = getDreamStageShadowColor();
	if (isOnDreamcast()) {
		setMugenAnimationColor(e.mShadowAnimationElement, 0, 0, 0);
	}
	else {
		setMugenAnimationColorSolid(e.mShadowAnimationElement, color.x, color.y, color.z);
	}
	setMugenAnimationTransparency(e.mShadowAnimationElement, getDreamStageShadowTransparency());
	setMugenAnimationFaceDirection(e.mShadowAnimationElement, getMugenAnimationIsFacingRight(e.mShadowAnimationElement));

	setDolmexicaStoryAnimationPositionXInternal(e, pos.x);
	setDolmexicaStoryAnimationPositionYInternal(e, pos.y);
}

void setDolmexicaStoryAnimationShadow(StoryInstance* tInstance, int tID, float tBasePositionY)
{
	StoryAnimation& e = tInstance->mStoryAnimations[tID];
	setDolmexicaStoryAnimationShadowInternal(e, tBasePositionY, &gDolmexicaStoryScreenData.mSprites, &gDolmexicaStoryScreenData.mAnimations);
}

int getDolmexicaStoryAnimationAnimation(StoryInstance* tInstance, int tID)
{
	StoryAnimation& e = tInstance->mStoryAnimations[tID];
	return getMugenAnimationAnimationNumber(e.mAnimationElement);
}

static void changeDolmexicaStoryAnimationInternal(StoryAnimation& e, int tAnimation, MugenAnimations* tAnimations)
{
	changeMugenAnimation(e.mAnimationElement, getMugenAnimation(tAnimations, tAnimation));
	if (e.mHasShadow) {
		changeMugenAnimation(e.mShadowAnimationElement, getMugenAnimation(tAnimations, tAnimation));
	}
}

void changeDolmexicaStoryAnimation(StoryInstance* tInstance, int tID, int tAnimation)
{
	StoryAnimation& e = tInstance->mStoryAnimations[tID];
	changeDolmexicaStoryAnimationInternal(e, tAnimation, &gDolmexicaStoryScreenData.mAnimations);
}

static void setDolmexicaStoryAnimationPositionXInternal(StoryAnimation& e, float tX)
{
	Position pos = getMugenAnimationPosition(e.mAnimationElement);
	pos.x = tX;
	setMugenAnimationPosition(e.mAnimationElement, pos);
	if (e.mHasShadow) {
		pos = getMugenAnimationPosition(e.mShadowAnimationElement);
		pos.x = tX;
		setMugenAnimationPosition(e.mShadowAnimationElement, pos);
	}
}

void setDolmexicaStoryAnimationPositionX(StoryInstance* tInstance, int tID, float tX)
{
	StoryAnimation& e = tInstance->mStoryAnimations[tID];
	setDolmexicaStoryAnimationPositionXInternal(e, tX);
}

static void setDolmexicaStoryAnimationPositionYInternal(StoryAnimation& e, float tY)
{
	Position pos = getMugenAnimationPosition(e.mAnimationElement);
	pos.y = tY;
	setMugenAnimationPosition(e.mAnimationElement, pos);
	if (e.mHasShadow) {
		float offsetY = pos.y - e.mShadowBasePositionY;
		pos = getMugenAnimationPosition(e.mShadowAnimationElement);
		pos.y = e.mShadowBasePositionY + getDreamStageShadowScaleY() * offsetY;
		setMugenAnimationPosition(e.mShadowAnimationElement, pos);
	}
}

void setDolmexicaStoryAnimationPositionY(StoryInstance* tInstance, int tID, float tY)
{
	StoryAnimation& e = tInstance->mStoryAnimations[tID];
	setDolmexicaStoryAnimationPositionYInternal(e, tY);
}

static void setDolmexicaStoryAnimationPositionZInternal(StoryAnimation& e, float tZ)
{
	auto pos = getMugenAnimationPosition(e.mAnimationElement);
	pos.z = DOLMEXICA_STORY_ANIMATION_BASE_Z + tZ * DOLMEXICA_STORY_ID_Z_FACTOR;
	setMugenAnimationPosition(e.mAnimationElement, pos);
}

static void addDolmexicaStoryAnimationPositionXInternal(StoryAnimation& e, float tX)
{
	Position pos = getMugenAnimationPosition(e.mAnimationElement);
	pos.x += tX;
	setMugenAnimationPosition(e.mAnimationElement, pos);
	if (e.mHasShadow) {
		pos = getMugenAnimationPosition(e.mShadowAnimationElement);
		pos.x += tX;
		setMugenAnimationPosition(e.mShadowAnimationElement, pos);
	}
}

void addDolmexicaStoryAnimationPositionX(StoryInstance* tInstance, int tID, float tX)
{
	StoryAnimation& e = tInstance->mStoryAnimations[tID];
	addDolmexicaStoryAnimationPositionXInternal(e, tX);
}

static void addDolmexicaStoryAnimationPositionYInternal(StoryAnimation& e, float tY)
{
	Position pos = getMugenAnimationPosition(e.mAnimationElement);
	pos.y += tY;
	setMugenAnimationPosition(e.mAnimationElement, pos);
	if (e.mHasShadow) {
		float offsetY = pos.y - e.mShadowBasePositionY;
		pos = getMugenAnimationPosition(e.mShadowAnimationElement);
		pos.y = e.mShadowBasePositionY + getDreamStageShadowScaleY() * offsetY;
		setMugenAnimationPosition(e.mShadowAnimationElement, pos);
	}
}

void addDolmexicaStoryAnimationPositionY(StoryInstance* tInstance, int tID, float tY)
{
	StoryAnimation& e = tInstance->mStoryAnimations[tID];
	addDolmexicaStoryAnimationPositionYInternal(e, tY);
}

static void setDolmexicaStoryAnimationStagePositionXInternal(StoryAnimation& e, float tX)
{
	auto pos = getMugenAnimationPosition(e.mAnimationElement);
	pos.x = tX + getDreamStageCoordinateSystemOffset(COORD_P).x + (COORD_P / 2);
	setMugenAnimationPosition(e.mAnimationElement, pos);
	
	if (e.mHasShadow) {
		pos = getMugenAnimationPosition(e.mShadowAnimationElement);
		pos.x = tX + getDreamStageCoordinateSystemOffset(COORD_P).x + (COORD_P / 2);
		setMugenAnimationPosition(e.mShadowAnimationElement, pos);
	}
}

void setDolmexicaStoryAnimationStagePositionX(StoryInstance* tInstance, int tID, float tX)
{
	StoryAnimation& e = tInstance->mStoryAnimations[tID];
	setDolmexicaStoryAnimationStagePositionXInternal(e, tX);
}

static void setDolmexicaStoryAnimationStagePositionYInternal(StoryAnimation& e, float tY)
{
	auto pos = getMugenAnimationPosition(e.mAnimationElement);
	pos.y = tY + getDreamStageCoordinateSystemOffset(COORD_P).y;
	setMugenAnimationPosition(e.mAnimationElement, pos);
	if (e.mHasShadow) {
		const auto offsetY = pos.y - e.mShadowBasePositionY;
		pos = getMugenAnimationPosition(e.mShadowAnimationElement);
		pos.y = e.mShadowBasePositionY + getDreamStageShadowScaleY() * offsetY + getDreamStageCoordinateSystemOffset(COORD_P).y;
		setMugenAnimationPosition(e.mShadowAnimationElement, pos);
	}
}

void setDolmexicaStoryAnimationStagePositionY(StoryInstance* tInstance, int tID, float tY)
{
	StoryAnimation& e = tInstance->mStoryAnimations[tID];
	setDolmexicaStoryAnimationStagePositionYInternal(e, tY);
}

static void setDolmexicaStoryAnimationScaleXInternal(StoryAnimation& e, float tX)
{
	Vector2D scale = getMugenAnimationDrawScale(e.mAnimationElement);
	scale.x = tX;
	setMugenAnimationDrawScale(e.mAnimationElement, scale);

	if (e.mHasShadow) {
		scale.y *= -getDreamStageShadowScaleY();
		setMugenAnimationDrawScale(e.mShadowAnimationElement, scale);
	}
}


void setDolmexicaStoryAnimationScaleX(StoryInstance* tInstance, int tID, float tX)
{
	StoryAnimation& e = tInstance->mStoryAnimations[tID];
	setDolmexicaStoryAnimationScaleXInternal(e, tX);
}

static void setDolmexicaStoryAnimationScaleYInternal(StoryAnimation& e, float tY)
{
	Vector2D scale = getMugenAnimationDrawScale(e.mAnimationElement);
	scale.y = tY;
	setMugenAnimationDrawScale(e.mAnimationElement, scale);

	if (e.mHasShadow) {
		scale.y *= -getDreamStageShadowScaleY();
		setMugenAnimationDrawScale(e.mShadowAnimationElement, scale);
	}
}


void setDolmexicaStoryAnimationScaleY(StoryInstance* tInstance, int tID, float tY)
{
	StoryAnimation& e = tInstance->mStoryAnimations[tID];
	setDolmexicaStoryAnimationScaleYInternal(e, tY);
}

static void setDolmexicaStoryAnimationIsFacingRightInternal(StoryAnimation& e, int tIsFacingRight)
{
	setMugenAnimationFaceDirection(e.mAnimationElement, tIsFacingRight);

	if (e.mHasShadow) {
		setMugenAnimationFaceDirection(e.mShadowAnimationElement, tIsFacingRight);
	}
}


void setDolmexicaStoryAnimationIsFacingRight(StoryInstance* tInstance, int tID, int tIsFacingRight)
{
	StoryAnimation& e = tInstance->mStoryAnimations[tID];
	setDolmexicaStoryAnimationIsFacingRightInternal(e, tIsFacingRight);
}

static void setDolmexicaStoryAnimationAngleInternal(StoryAnimation& e, float tAngle)
{
	const auto newAngle = degreesToRadians(tAngle);
	setMugenAnimationDrawAngle(e.mAnimationElement, newAngle);
	if (e.mHasShadow) {
		setMugenAnimationDrawAngle(e.mShadowAnimationElement, newAngle);
	}
}

void setDolmexicaStoryAnimationAngle(StoryInstance* tInstance, int tID, float tAngle)
{
	StoryAnimation& e = tInstance->mStoryAnimations[tID];
	setDolmexicaStoryAnimationAngleInternal(e, tAngle);
}

static void addDolmexicaStoryAnimationAngleInternal(StoryAnimation& e, float tAngle)
{
	const auto newAngle = degreesToRadians(radiansToDegrees(getMugenAnimationDrawAngle(e.mAnimationElement)) + tAngle);
	setMugenAnimationDrawAngle(e.mAnimationElement, newAngle);
	if (e.mHasShadow) {
		setMugenAnimationDrawAngle(e.mShadowAnimationElement, newAngle);
	}
}

void addDolmexicaStoryAnimationAngle(StoryInstance* tInstance, int tID, float tAngle)
{
	StoryAnimation& e = tInstance->mStoryAnimations[tID];
	addDolmexicaStoryAnimationAngleInternal(e, tAngle);
}

static void setDolmexicaStoryAnimationColorInternal(StoryAnimation& e, const Vector3D& tColor)
{
	setMugenAnimationColor(e.mAnimationElement, tColor.x, tColor.y, tColor.z);
}

void setDolmexicaStoryAnimationColor(StoryInstance* tInstance, int tID, const Vector3D& tColor)
{
	StoryAnimation& e = tInstance->mStoryAnimations[tID];
	setDolmexicaStoryAnimationColorInternal(e, tColor);
}

static void setDolmexicaStoryAnimationOpacityInternal(StoryAnimation& e, float tOpacity)
{
	setMugenAnimationTransparency(e.mAnimationElement, tOpacity);
	if (e.mHasShadow) {
		setMugenAnimationTransparency(e.mShadowAnimationElement, tOpacity);
	}
}

void setDolmexicaStoryAnimationOpacity(StoryInstance* tInstance, int tID, float tOpacity)
{
	StoryAnimation& e = tInstance->mStoryAnimations[tID];
	setDolmexicaStoryAnimationOpacityInternal(e, tOpacity);
}

void addDolmexicaStoryText(StoryInstance* tInstance, int tID, const char* tText, const Vector3DI& tFont, float tScale, const Position2D& tBasePosition, const Position2D& tTextOffset, float tTextBoxWidth)
{
	if (stl_map_contains(tInstance->mStoryTexts, tID)) {
		removeDolmexicaStoryText(tInstance, tID);
	}

	StoryText& e = tInstance->mStoryTexts[tID];
	e.mID = tID;
	const auto z = DOLMEXICA_STORY_TEXT_BASE_Z + tID * DOLMEXICA_STORY_ID_Z_FACTOR;
	e.mPosition = Vector3D(tBasePosition.x, tBasePosition.y, z);
	e.mTextOffset = tTextOffset;

	const auto textPosition = e.mPosition + e.mTextOffset;
	e.mTextID = addMugenText(tText, textPosition, tFont.x);
	setMugenTextColor(e.mTextID, getMugenTextColorFromMugenTextColorIndex(tFont.y));
	setMugenTextAlignment(e.mTextID, getMugenTextAlignmentFromMugenAlignmentIndex(tFont.z));

	setMugenTextBuildup(e.mTextID, 1);
	setMugenTextScale(e.mTextID, tScale);
	setMugenTextTextBoxWidth(e.mTextID, tTextBoxWidth);
	
	e.mHasTextSound = 0;
	e.mHasBackground = 0;
	e.mHasFace = 0;
	e.mHasContinue = 0;
	e.mHasContinueSound = 0;
	e.mHasName = 0;
	e.mGoesToNextState = 0;
	e.mHasFinished = 0;
	e.mIsLockedOnToCharacter = 0;
	e.mIsDisabled = 0;
}

static void unloadDolmexicaStoryText(StoryText* e) {
	removeMugenText(e->mTextID);
	if (e->mHasBackground) {
		if (e->mIsBackgroundAnimationOwned) {
			destroyMugenAnimation(e->mBackgroundAnimation);
		}
		removeMugenAnimation(e->mBackgroundAnimationElement);
	}
	if (e->mHasFace) {
		if (e->mIsFaceAnimationOwned) {
			destroyMugenAnimation(e->mFaceAnimation);
		}
		removeMugenAnimation(e->mFaceAnimationElement);
	}
	if (e->mHasContinue) {
		if (e->mIsContinueAnimationOwned) {
			destroyMugenAnimation(e->mContinueAnimation);
		}
		removeMugenAnimation(e->mContinueAnimationElement);
	}
	if (e->mHasName) {
		removeMugenText(e->mNameID);
	}
}

void removeDolmexicaStoryText(StoryInstance* tInstance, int tID)
{
	StoryText* e = &tInstance->mStoryTexts[tID];
	unloadDolmexicaStoryText(e);
	
	tInstance->mStoryTexts.erase(e->mID);
}

const char* getDolmexicaStoryTextText(StoryInstance* tInstance, int tID)
{
	StoryText* e = &tInstance->mStoryTexts[tID];
	return getMugenTextText(e->mTextID);
}

const char* getDolmexicaStoryTextDisplayedText(StoryInstance* tInstance, int tID)
{
	StoryText* e = &tInstance->mStoryTexts[tID];
	return getMugenTextDisplayedText(e->mTextID);
}

const char* getDolmexicaStoryTextNameText(StoryInstance* tInstance, int tID)
{
	StoryText* e = &tInstance->mStoryTexts[tID];
	if (!e->mHasName) return "";
	return getMugenTextText(e->mNameID);
}

int isDolmexicaStoryTextVisible(StoryInstance* tInstance, int tID)
{
	StoryText* e = &tInstance->mStoryTexts[tID];
	return !e->mIsDisabled;
}

static void setDolmexicaStoryTextBackgroundInternal(StoryInstance* tInstance, int tID, MugenAnimation* tAnimation, int tIsOwned, const Position& tOffset, const Vector2D& tScale) {
	StoryText* e = &tInstance->mStoryTexts[tID];
	e->mBackgroundOffset = Vector3D(tOffset.x, tOffset.y, tOffset.z * DOLMEXICA_STORY_OFFSET_Z_FACTOR);
	Position p = vecAdd(e->mPosition, e->mBackgroundOffset);
	e->mBackgroundAnimation = tAnimation;
	e->mIsBackgroundAnimationOwned = tIsOwned;
	e->mBackgroundAnimationElement = addMugenAnimation(e->mBackgroundAnimation, &gDolmexicaStoryScreenData.mSprites, p);
	setMugenAnimationDrawScale(e->mBackgroundAnimationElement, tScale);
	e->mHasBackground = 1;
}

void setDolmexicaStoryTextBackground(StoryInstance* tInstance, int tID, const Vector2DI& tSprite, const Position& tOffset, const Vector2D& tScale)
{
	setDolmexicaStoryTextBackgroundInternal(tInstance, tID, createOneFrameMugenAnimationForSprite(tSprite.x, tSprite.y), 1, tOffset, tScale);
}

void setDolmexicaStoryTextBackground(StoryInstance* tInstance, int tID, int tAnimation, const Position& tOffset, const Vector2D& tScale)
{
	setDolmexicaStoryTextBackgroundInternal(tInstance, tID, getMugenAnimation(&gDolmexicaStoryScreenData.mAnimations, tAnimation), 0, tOffset, tScale);
}

static void setDolmexicaStoryTextFaceInternal(StoryInstance* tInstance, int tID, MugenAnimation* tAnimation, int tIsOwned, const Position& tOffset, const Vector2D& tScale)
{
	StoryText* e = &tInstance->mStoryTexts[tID];
	e->mFaceOffset = Vector3D(tOffset.x, tOffset.y, tOffset.z * DOLMEXICA_STORY_OFFSET_Z_FACTOR);
	const auto p = vecAdd(e->mPosition, e->mFaceOffset);
	e->mFaceAnimation = tAnimation;
	e->mIsFaceAnimationOwned = tIsOwned;
	e->mFaceAnimationElement = addMugenAnimation(e->mFaceAnimation, &gDolmexicaStoryScreenData.mSprites, p);
	setMugenAnimationDrawScale(e->mFaceAnimationElement, tScale);
	e->mHasFace = 1;
}

void setDolmexicaStoryTextFace(StoryInstance* tInstance, int tID, const Vector2DI& tSprite, const Position& tOffset, const Vector2D& tScale)
{
	setDolmexicaStoryTextFaceInternal(tInstance, tID, createOneFrameMugenAnimationForSprite(tSprite.x, tSprite.y), 1, tOffset, tScale);
}

void setDolmexicaStoryTextFace(StoryInstance* tInstance, int tID, int tAnimation, const Position& tOffset, const Vector2D& tScale)
{
	setDolmexicaStoryTextFaceInternal(tInstance, tID, getMugenAnimation(&gDolmexicaStoryScreenData.mAnimations, tAnimation), 0, tOffset, tScale);
}

void setDolmexicaStoryTextName(StoryInstance* tInstance, int tID, const char* tText, const Vector3DI& tFont, const Position2D& tOffset, float tScale)
{
	StoryText* e = &tInstance->mStoryTexts[tID];
	e->mNameOffset = tOffset;
	const auto p = e->mPosition + e->mNameOffset;
	e->mNameID = addMugenTextMugenStyle(tText, p, tFont);
	setMugenTextScale(e->mNameID, tScale);
	e->mHasName = 1;
}

static void setDolmexicaStoryTextContinueInternal(StoryInstance* tInstance, int tID, MugenAnimation* tAnimation, int tIsOwned, const Position& tOffset, const Vector2D& tScale)
{
	StoryText* e = &tInstance->mStoryTexts[tID];
	e->mContinueOffset = Vector3D(tOffset.x, tOffset.y, tOffset.z * DOLMEXICA_STORY_OFFSET_Z_FACTOR);
	const auto p = vecAdd(e->mPosition, e->mContinueOffset);
	e->mContinueAnimation = tAnimation;
	e->mIsContinueAnimationOwned = tIsOwned;
	e->mContinueAnimationElement = addMugenAnimation(e->mContinueAnimation, &gDolmexicaStoryScreenData.mSprites, p);
	setMugenAnimationVisibility(e->mContinueAnimationElement, 0);
	setMugenAnimationDrawScale(e->mContinueAnimationElement, tScale);
	e->mHasContinue = 1;
}

void setDolmexicaStoryTextContinue(StoryInstance* tInstance, int tID, const Vector2DI& tSprite, const Position& tOffset, const Vector2D& tScale)
{
	setDolmexicaStoryTextContinueInternal(tInstance, tID, createOneFrameMugenAnimationForSprite(tSprite.x, tSprite.y), 1, tOffset, tScale);
}

void setDolmexicaStoryTextContinue(StoryInstance* tInstance, int tID, int tAnimation, const Position& tOffset, const Vector2D& tScale)
{
	setDolmexicaStoryTextContinueInternal(tInstance, tID, getMugenAnimation(&gDolmexicaStoryScreenData.mAnimations, tAnimation), 0, tOffset, tScale);
}

static Position2D getDolmexicaStoryTextBasePosition(StoryInstance* tInstance, int tID) {
	StoryText* e = &tInstance->mStoryTexts[tID];
	return e->mPosition.xy();
}

float getDolmexicaStoryTextBasePositionX(StoryInstance* tInstance, int tID)
{
	return getDolmexicaStoryTextBasePosition(tInstance, tID).x;
}

float getDolmexicaStoryTextBasePositionY(StoryInstance* tInstance, int tID)
{
	return getDolmexicaStoryTextBasePosition(tInstance, tID).y;
}

void setDolmexicaStoryTextBasePosition(StoryInstance* tInstance, int tID, const Position2D& tPosition)
{
	StoryText* e = &tInstance->mStoryTexts[tID];
	const auto z = DOLMEXICA_STORY_TEXT_BASE_Z + tID * DOLMEXICA_STORY_ID_Z_FACTOR;
	e->mPosition = tPosition.xyz(z);

	const auto textPosition = e->mPosition + e->mTextOffset;
	setMugenTextPosition(e->mTextID, textPosition);
	if (e->mHasBackground) {	
		setMugenAnimationPosition(e->mBackgroundAnimationElement, e->mPosition + e->mBackgroundOffset);
	}
	if (e->mHasFace) {
		setMugenAnimationPosition(e->mFaceAnimationElement, e->mPosition + e->mFaceOffset);
	}
	if (e->mHasContinue) {
		setMugenAnimationPosition(e->mContinueAnimationElement, e->mPosition + e->mContinueOffset);
	}
	if (e->mHasName) {
		setMugenTextPosition(e->mNameID, e->mPosition + e->mNameOffset);
	}
}

void setDolmexicaStoryTextText(StoryInstance* tInstance, int tID, const char * tText)
{
	StoryText* e = &tInstance->mStoryTexts[tID];
	changeMugenText(e->mTextID, tText);
	setMugenTextBuildup(e->mTextID, 1);

	if (e->mIsDisabled) {
		if (e->mHasBackground) {
			setMugenAnimationVisibility(e->mBackgroundAnimationElement, 1);
		}
		if (e->mHasFace) {
			setMugenAnimationVisibility(e->mFaceAnimationElement, 1);
		}
		if (e->mHasName) {
			setMugenTextVisibility(e->mNameID, 1);
		}

		e->mIsDisabled = 0;
	}

	e->mHasFinished = 0;
}

void setDolmexicaStoryTextFont(StoryInstance* tInstance, int tID, const Vector3DI& tFont) {
	StoryText* e = &tInstance->mStoryTexts[tID];
	setMugenTextFont(e->mTextID, tFont.x);
	setMugenTextColor(e->mTextID, getMugenTextColorFromMugenTextColorIndex(tFont.y));
	setMugenTextAlignment(e->mTextID, getMugenTextAlignmentFromMugenAlignmentIndex(tFont.z));
}

void setDolmexicaStoryTextScale(StoryInstance* tInstance, int tID, float tScale)
{
	StoryText* e = &tInstance->mStoryTexts[tID];
	setMugenTextScale(e->mTextID, tScale);
}

void setDolmexicaStoryTextSound(StoryInstance* tInstance, int tID, const Vector2DI& tSound)
{
	StoryText* e = &tInstance->mStoryTexts[tID];
	e->mTextSound = tSound;
	e->mTextSoundFrequency = std::max(e->mTextSoundFrequency, 1);
	e->mHasTextSound = 1;
}

void setDolmexicaStoryTextSoundFrequency(StoryInstance* tInstance, int tID, int tSoundFrequency)
{
	StoryText* e = &tInstance->mStoryTexts[tID];
	assert(e->mHasTextSound);
	e->mTextSoundFrequency = tSoundFrequency;
}

void setDolmexicaStoryTextTextOffset(StoryInstance* tInstance, int tID, const Position2D& tOffset)
{
	StoryText* e = &tInstance->mStoryTexts[tID];
	e->mTextOffset = tOffset;
	setDolmexicaStoryTextBasePosition(tInstance, tID, e->mPosition.xy());
}

void setDolmexicaStoryTextBackgroundSprite(StoryInstance* tInstance, int tID, const Vector2DI& tSprite)
{
	StoryText* e = &tInstance->mStoryTexts[tID];
	destroyMugenAnimation(e->mBackgroundAnimation);
	e->mBackgroundAnimation = createOneFrameMugenAnimationForSprite(tSprite.x, tSprite.y);
	changeMugenAnimation(e->mBackgroundAnimationElement, e->mBackgroundAnimation);
}

void setDolmexicaStoryTextBackgroundOffset(StoryInstance* tInstance, int tID, const Position& tOffset)
{
	StoryText* e = &tInstance->mStoryTexts[tID];
	e->mBackgroundOffset = Vector3D(tOffset.x, tOffset.y, tOffset.z * DOLMEXICA_STORY_OFFSET_Z_FACTOR);
	setMugenAnimationPosition(e->mBackgroundAnimationElement, vecAdd(getMugenTextPosition(e->mTextID), e->mBackgroundOffset));
}

void setDolmexicaStoryTextBackgroundScale(StoryInstance* tInstance, int tID, const Vector2D& tScale)
{
	StoryText* e = &tInstance->mStoryTexts[tID];
	setMugenAnimationDrawScale(e->mBackgroundAnimationElement, tScale);
}

void setDolmexicaStoryTextFaceSprite(StoryInstance* tInstance, int tID, const Vector2DI& tSprite)
{
	StoryText* e = &tInstance->mStoryTexts[tID];
	destroyMugenAnimation(e->mFaceAnimation);
	e->mFaceAnimation = createOneFrameMugenAnimationForSprite(tSprite.x, tSprite.y);
	changeMugenAnimation(e->mFaceAnimationElement, e->mFaceAnimation);
}

void setDolmexicaStoryTextFaceOffset(StoryInstance* tInstance, int tID, const Position& tOffset)
{
	StoryText* e = &tInstance->mStoryTexts[tID];
	e->mFaceOffset = Vector3D(tOffset.x, tOffset.y, tOffset.z * DOLMEXICA_STORY_OFFSET_Z_FACTOR);
	setMugenAnimationPosition(e->mFaceAnimationElement, vecAdd(e->mPosition, e->mFaceOffset));
}

void setDolmexicaStoryTextFaceScale(StoryInstance* tInstance, int tID, const Vector2D& tScale)
{
	StoryText* e = &tInstance->mStoryTexts[tID];
	setMugenAnimationDrawScale(e->mFaceAnimationElement, tScale);
}

void setDolmexicaStoryTextContinueAnimation(StoryInstance* tInstance, int tID, int tAnimation)
{
	StoryText* e = &tInstance->mStoryTexts[tID];
	changeMugenAnimation(e->mContinueAnimationElement, getMugenAnimation(&gDolmexicaStoryScreenData.mAnimations, tAnimation));
}

void setDolmexicaStoryTextContinueSound(StoryInstance* tInstance, int tID, const Vector2DI& tSound)
{
	StoryText* e = &tInstance->mStoryTexts[tID];
	e->mContinueSound = tSound;
	e->mHasContinueSound = 1;
}

void setDolmexicaStoryTextContinueOffset(StoryInstance* tInstance, int tID, const Position& tOffset)
{
	StoryText* e = &tInstance->mStoryTexts[tID];
	e->mContinueOffset = Vector3D(tOffset.x, tOffset.y, tOffset.z * DOLMEXICA_STORY_OFFSET_Z_FACTOR);
	setMugenAnimationPosition(e->mContinueAnimationElement, vecAdd(getMugenTextPosition(e->mTextID), e->mContinueOffset));
}

void setDolmexicaStoryTextContinueScale(StoryInstance* tInstance, int tID, const Vector2D& tScale)
{
	StoryText* e = &tInstance->mStoryTexts[tID];
	setMugenAnimationDrawScale(e->mContinueAnimationElement, tScale);
}

void setDolmexicaStoryTextNameText(StoryInstance* tInstance, int tID, const char * tText)
{
	StoryText* e = &tInstance->mStoryTexts[tID];
	changeMugenText(e->mNameID, tText);
}

void setDolmexicaStoryTextNameFont(StoryInstance* tInstance, int tID, const Vector3DI& tFont)
{
	StoryText* e = &tInstance->mStoryTexts[tID];
	setMugenTextFont(e->mNameID, tFont.x);
	setMugenTextColor(e->mNameID, getMugenTextColorFromMugenTextColorIndex(tFont.y));
	setMugenTextAlignment(e->mNameID, getMugenTextAlignmentFromMugenAlignmentIndex(tFont.z));
}

void setDolmexicaStoryTextNameOffset(StoryInstance* tInstance, int tID, const Position2D& tOffset)
{
	StoryText* e = &tInstance->mStoryTexts[tID];
	e->mNameOffset = tOffset;
	setDolmexicaStoryTextBasePosition(tInstance, tID, e->mPosition.xy());
}

void setDolmexicaStoryTextNameScale(StoryInstance* tInstance, int tID, float tScale)
{
	StoryText* e = &tInstance->mStoryTexts[tID];
	setMugenTextScale(e->mNameID, tScale);
}

int getDolmexicaStoryTextNextState(StoryInstance* tInstance, int tID)
{
	StoryText* e = &tInstance->mStoryTexts[tID];
	if (!e->mGoesToNextState) return 0;
	else return e->mNextState;
}

void setDolmexicaStoryTextNextState(StoryInstance* tInstance, int tID, int tNextState)
{
	StoryText* e = &tInstance->mStoryTexts[tID];
	e->mNextState = tNextState;
	e->mGoesToNextState = 1;
}

static void setDolmexicaStoryTextLockToCharacterInternal(StoryInstance* tInstance, StoryInstance* tCharacterInstance, int tID, int tCharacterID, const Position2D& tOffset) {
	StoryText* e = &tInstance->mStoryTexts[tID];
	StoryCharacter& character = tCharacterInstance->mStoryCharacters[tCharacterID];

	e->mLockCharacterPositionReference = getMugenAnimationPositionReference(character.mAnimation.mAnimationElement);
	e->mLockCharacterIsBoundToStage = character.mAnimation.mIsBoundToStage;
	e->mLockOffset = tOffset;
	e->mIsLockedOnToCharacter = 1;
	updateLockedTextPosition(tInstance, e);
}

void setDolmexicaStoryTextLockToCharacter(StoryInstance* tInstance, int tID, int tCharacterID, const Position2D& tOffset)
{
	setDolmexicaStoryTextLockToCharacterInternal(tInstance, tInstance, tID, tCharacterID, tOffset);
}

void setDolmexicaStoryTextLockToCharacter(StoryInstance* tInstance, int tID, int tCharacterID, const Position2D& tOffset, int tHelperID)
{
	setDolmexicaStoryTextLockToCharacterInternal(tInstance, &gDolmexicaStoryScreenData.mHelperInstances[tHelperID], tID, tCharacterID, tOffset);
}

void setDolmexicaStoryTextInactive(StoryInstance* tInstance, int tID)
{
	StoryText* e = &tInstance->mStoryTexts[tID];
	
	char text[2];
	text[0] = '\0';
	changeMugenText(e->mTextID, text);
	if (e->mHasBackground) {
		setMugenAnimationVisibility(e->mBackgroundAnimationElement, 0);
	}
	if (e->mHasFace) {
		setMugenAnimationVisibility(e->mFaceAnimationElement, 0);
	}
	if (e->mHasContinue) {
		setMugenAnimationVisibility(e->mContinueAnimationElement, 0);
	}
	if (e->mHasName) {
		setMugenTextVisibility(e->mNameID, 0);
	}

	e->mIsDisabled = 1;
}

int isDolmexicaStoryTextBuiltUp(StoryInstance* tInstance, int tID)
{
	StoryText* e = &tInstance->mStoryTexts[tID];
	return isMugenTextBuiltUp(e->mTextID);
}

void setDolmexicaStoryTextBuiltUp(StoryInstance* tInstance, int tID)
{
	StoryText* e = &tInstance->mStoryTexts[tID];
	setMugenTextBuiltUp(e->mTextID);
}

void setDolmexicaStoryTextPositionX(StoryInstance* tInstance, int tID, float tX)
{
	auto p = getDolmexicaStoryTextBasePosition(tInstance, tID);
	p.x = tX;
	setDolmexicaStoryTextBasePosition(tInstance, tID, p);
}

void setDolmexicaStoryTextPositionY(StoryInstance* tInstance, int tID, float tY)
{
	auto p = getDolmexicaStoryTextBasePosition(tInstance, tID);
	p.y = tY;
	setDolmexicaStoryTextBasePosition(tInstance, tID, p);
}

void addDolmexicaStoryTextPositionX(StoryInstance* tInstance, int tID, float tX)
{
	auto p = getDolmexicaStoryTextBasePosition(tInstance, tID);
	p.x += tX;
	setDolmexicaStoryTextBasePosition(tInstance, tID, p);
}

void addDolmexicaStoryTextPositionY(StoryInstance* tInstance, int tID, float tY)
{
	auto p = getDolmexicaStoryTextBasePosition(tInstance, tID);
	p.y += tY;
	setDolmexicaStoryTextBasePosition(tInstance, tID, p);
}

void setDolmexicaStoryIDName(StoryInstance* tInstance, int tID, const std::string& tName)
{
	tInstance->mTextNames[tName] = tID;
}

int getDolmexicaStoryTextIDFromName(StoryInstance* tInstance, const std::string& tName)
{
	return tInstance->mTextNames[tName];
}

void changeDolmexicaStoryState(StoryInstance* tInstance, int tNextState)
{
	if(gDolmexicaStoryScreenData.mDebugStartState && getDolmexicaStoryStateNumber(tInstance) == gDolmexicaStoryScreenData.mDebugStartStateFrom && tInstance == getDolmexicaStoryRootInstance())
	{
		tNextState = gDolmexicaStoryScreenData.mDebugStartState;
	}
	changeDreamHandledStateMachineState(tInstance->mRegisteredStateMachine, tNextState);
	setDreamRegisteredStateTimeInState(tInstance->mRegisteredStateMachine, 0);
}

void changeDolmexicaStoryStateOutsideStateHandler(StoryInstance* tInstance, int tNextState)
{
	if(gDolmexicaStoryScreenData.mDebugStartState && getDolmexicaStoryStateNumber(tInstance) == gDolmexicaStoryScreenData.mDebugStartStateFrom && tInstance == getDolmexicaStoryRootInstance())
	{
		tNextState = gDolmexicaStoryScreenData.mDebugStartState;
	}
	changeDreamHandledStateMachineState(tInstance->mRegisteredStateMachine, tNextState);
	setDreamRegisteredStateTimeInState(tInstance->mRegisteredStateMachine, -1);
}

void endDolmexicaStoryboard(StoryInstance* /*tInstance*/, int tNextStoryState)
{
	storyModeOverCB(tNextStoryState);
}

int getDolmexicaStoryTimeInState(StoryInstance* tInstance)
{
	return getDreamRegisteredStateTimeInState(tInstance->mRegisteredStateMachine);
}

int getDolmexicaStoryStateNumber(StoryInstance* tInstance)
{
	return getDreamRegisteredStateState(tInstance->mRegisteredStateMachine);
}

static int getDolmexicaStoryAnimationTimeLeftInternal(StoryAnimation& e)
{
	return getMugenAnimationRemainingAnimationTime(e.mAnimationElement);
}

int getDolmexicaStoryAnimationTimeLeft(StoryInstance* tInstance, int tID)
{
	if (!stl_map_contains(tInstance->mStoryAnimations, tID)) {
		logWarningFormat("Querying time left for non-existing story animation %d. Defaulting to infinite.", tID);
		return INF;
	}
	StoryAnimation& e = tInstance->mStoryAnimations[tID];
	return getDolmexicaStoryAnimationTimeLeftInternal(e);
}

static float getDolmexicaStoryAnimationPositionXInternal(StoryAnimation& e) {
	return getMugenAnimationPosition(e.mAnimationElement).x;
}

float getDolmexicaStoryAnimationPositionX(StoryInstance* tInstance, int tID)
{
	if (!stl_map_contains(tInstance->mStoryAnimations, tID)) {
		logWarningFormat("Querying for non-existing story animation %d. Defaulting to 0.", tID);
		return 0;
	}
	StoryAnimation& e = tInstance->mStoryAnimations[tID];
	return getDolmexicaStoryAnimationPositionXInternal(e);
}

static float getDolmexicaStoryAnimationPositionYInternal(StoryAnimation& e) {
	return getMugenAnimationPosition(e.mAnimationElement).y;
}

float getDolmexicaStoryAnimationPositionY(StoryInstance* tInstance, int tID)
{
	if (!stl_map_contains(tInstance->mStoryAnimations, tID)) {
		logWarningFormat("Querying for non-existing story animation %d. Defaulting to 0.", tID);
		return 0;
	}
	StoryAnimation& e = tInstance->mStoryAnimations[tID];
	return getDolmexicaStoryAnimationPositionYInternal(e);
}

static float getDolmexicaStoryAnimationScreenPositionXInternal(StoryAnimation& e) {
	return getMugenAnimationPosition(e.mAnimationElement).x - getDreamCameraPositionX(COORD_P);
}

float getDolmexicaStoryAnimationScreenPositionX(StoryInstance* tInstance, int tID)
{
	if (!stl_map_contains(tInstance->mStoryAnimations, tID)) {
		logWarningFormat("Querying for non-existing story animation %d. Defaulting to 0.", tID);
		return 0;
	}
	StoryAnimation& e = tInstance->mStoryAnimations[tID];
	return getDolmexicaStoryAnimationScreenPositionXInternal(e);
}

static float getDolmexicaStoryAnimationScreenPositionYInternal(StoryAnimation& e) {
	return getMugenAnimationPosition(e.mAnimationElement).y - getDreamCameraPositionY(COORD_P) + getDreamStageCoordinateSystemOffset(COORD_P).y;
}

float getDolmexicaStoryAnimationScreenPositionY(StoryInstance* tInstance, int tID)
{
	if (!stl_map_contains(tInstance->mStoryAnimations, tID)) {
		logWarningFormat("Querying for non-existing story animation %d. Defaulting to 0.", tID);
		return 0;
	}
	StoryAnimation& e = tInstance->mStoryAnimations[tID];
	return getDolmexicaStoryAnimationScreenPositionYInternal(e);
}

static float getDolmexicaStoryAnimationStagePositionXInternal(StoryAnimation& e) {
	return getMugenAnimationPosition(e.mAnimationElement).x - getDreamStageCoordinateSystemOffset(COORD_P).x - (COORD_P / 2);
}

float getDolmexicaStoryAnimationStagePositionX(StoryInstance* tInstance, int tID)
{
	if (!stl_map_contains(tInstance->mStoryAnimations, tID)) {
		logWarningFormat("Querying for non-existing story animation %d. Defaulting to 0.", tID);
		return 0;
	}
	StoryAnimation& e = tInstance->mStoryAnimations[tID];
	return getDolmexicaStoryAnimationStagePositionXInternal(e);
}

static float getDolmexicaStoryAnimationStagePositionYInternal(StoryAnimation& e) {
	return getMugenAnimationPosition(e.mAnimationElement).y - getDreamStageCoordinateSystemOffset(COORD_P).y;
}

float getDolmexicaStoryAnimationStagePositionY(StoryInstance* tInstance, int tID)
{
	if (!stl_map_contains(tInstance->mStoryAnimations, tID)) {
		logWarningFormat("Querying for non-existing story animation %d. Defaulting to 0.", tID);
		return 0;
	}
	StoryAnimation& e = tInstance->mStoryAnimations[tID];
	return getDolmexicaStoryAnimationStagePositionYInternal(e);
}

void addDolmexicaStoryCharacter(StoryInstance* tInstance, int tID, const char* tName, int tPreferredPalette, int tAnimation, const Position2D& tPosition)
{
	if (stl_map_contains(tInstance->mStoryCharacters, tID)) {
		logWarningFormat("Trying to create existing character %d (%s). Erasing previous.", tID, tName);
		removeDolmexicaStoryCharacter(tInstance, tID);
	}

	StoryCharacter& e = tInstance->mStoryCharacters[tID];
	e.mName = tName;

	char file[1024];
	char path[1024];
	char fullPath[1024];
	char name[100];
	getCharacterSelectNamePath(tName, fullPath);
	getPathToFile(path, fullPath);
	MugenDefScript script;
	loadMugenDefScript(&script, fullPath);
	

	char palettePath[1024];
	sprintf(name, "pal%d", tPreferredPalette);
	getMugenDefStringOrDefault(file, &script, "files", name, "");
	int hasPalettePath = strcmp("", file);
	sprintf(palettePath, "%s%s", path, file);
	if(!isFile(palettePath)){
		logErrorFormat("Unable to find palette file %s. Ignoring.", palettePath);
		hasPalettePath = 0;
	}
	getMugenDefStringOrDefault(file, &script, "files", "sprite", "");
	sprintf(fullPath, "%s%s", path, file);

	e.mSprites = loadMugenSpriteFile(fullPath, hasPalettePath, palettePath);

	getMugenDefStringOrDefault(file, &script, "files", "anim", "");
	sprintf(fullPath, "%s%s", path, file);
	if (!isFile(fullPath)) {
		logWarningFormat("Unable to load animation file %s from def file %s. Ignoring.", fullPath, tName);
		tInstance->mStoryCharacters.erase(tID);
		return;
	}
	e.mAnimations = loadMugenAnimationFile(fullPath);

	unloadMugenDefScript(&script);

	const auto z = DOLMEXICA_STORY_ANIMATION_BASE_Z + tID * DOLMEXICA_STORY_ID_Z_FACTOR;
	initDolmexicaStoryAnimation(tInstance->mStoryCharacters[tID].mAnimation, tID, tAnimation, tPosition.xyz(z), &tInstance->mStoryCharacters[tID].mSprites, &tInstance->mStoryCharacters[tID].mAnimations);

	if (isInDevelopMode()) {
		addDebugDolmexicaStoryCharacterAnimation(e.mName.c_str(), tAnimation);
	}
}

void removeDolmexicaStoryCharacter(StoryInstance* tInstance, int tID)
{
	StoryCharacter& e = tInstance->mStoryCharacters[tID];
	unloadDolmexicaStoryAnimation(e.mAnimation);
	unloadMugenAnimationFile(&e.mAnimations);
	unloadMugenSpriteFile(&e.mSprites);
	tInstance->mStoryCharacters.erase(tID);
}

int getDolmexicaStoryCharacterIsBoundToStage(StoryInstance* tInstance, int tID)
{
	StoryCharacter& e = tInstance->mStoryCharacters[tID];
	return e.mAnimation.mIsBoundToStage;
}

void setDolmexicaStoryCharacterBoundToStage(StoryInstance* tInstance, int tID, int tIsBoundToStage)
{
	StoryCharacter& e = tInstance->mStoryCharacters[tID];
	setDolmexicaStoryAnimationBoundToStageInternal(e.mAnimation, tIsBoundToStage);
}

int getDolmexicaStoryCharacterHasShadow(StoryInstance* tInstance, int tID)
{
	StoryCharacter& e = tInstance->mStoryCharacters[tID];
	return e.mAnimation.mHasShadow;
}

void setDolmexicaStoryCharacterShadow(StoryInstance* tInstance, int tID, float tBasePositionY)
{
	StoryCharacter& e = tInstance->mStoryCharacters[tID];
	setDolmexicaStoryAnimationShadowInternal(e.mAnimation, tBasePositionY, &e.mSprites, &e.mAnimations);
}

int getDolmexicaStoryCharacterAnimation(StoryInstance* tInstance, int tID)
{
	StoryCharacter& e = tInstance->mStoryCharacters[tID];
	return getMugenAnimationAnimationNumber(e.mAnimation.mAnimationElement);
}

void changeDolmexicaStoryCharacterAnimation(StoryInstance* tInstance, int tID, int tAnimation)
{
	StoryCharacter& e = tInstance->mStoryCharacters[tID];
	if (isInDevelopMode()) {
		addDebugDolmexicaStoryCharacterAnimation(e.mName.c_str(), tAnimation);
	}
	changeDolmexicaStoryAnimationInternal(e.mAnimation, tAnimation, &e.mAnimations);
}

float getDolmexicaStoryCharacterPositionX(StoryInstance* tInstance, int tID)
{
	StoryCharacter& e = tInstance->mStoryCharacters[tID];
	return getDolmexicaStoryAnimationPositionXInternal(e.mAnimation);
}

float getDolmexicaStoryCharacterPositionY(StoryInstance* tInstance, int tID)
{
	StoryCharacter& e = tInstance->mStoryCharacters[tID];
	return getDolmexicaStoryAnimationPositionYInternal(e.mAnimation);
}

float getDolmexicaStoryCharacterScreenPositionX(StoryInstance* tInstance, int tID)
{
	StoryCharacter& e = tInstance->mStoryCharacters[tID];
	return getDolmexicaStoryAnimationScreenPositionXInternal(e.mAnimation);
}

float getDolmexicaStoryCharacterScreenPositionY(StoryInstance* tInstance, int tID)
{
	StoryCharacter& e = tInstance->mStoryCharacters[tID];
	return getDolmexicaStoryAnimationScreenPositionYInternal(e.mAnimation);
}

float getDolmexicaStoryCharacterStagePositionX(StoryInstance* tInstance, int tID)
{
	StoryCharacter& e = tInstance->mStoryCharacters[tID];
	return getDolmexicaStoryAnimationStagePositionXInternal(e.mAnimation);
}

float getDolmexicaStoryCharacterStagePositionY(StoryInstance* tInstance, int tID)
{
	StoryCharacter& e = tInstance->mStoryCharacters[tID];
	return getDolmexicaStoryAnimationStagePositionYInternal(e.mAnimation);
}

void setDolmexicaStoryCharacterPositionX(StoryInstance* tInstance, int tID, float tX)
{
	StoryCharacter& e = tInstance->mStoryCharacters[tID];
	setDolmexicaStoryAnimationPositionXInternal(e.mAnimation, tX);
}

void setDolmexicaStoryCharacterPositionY(StoryInstance* tInstance, int tID, float tY)
{
	StoryCharacter& e = tInstance->mStoryCharacters[tID];
	setDolmexicaStoryAnimationPositionYInternal(e.mAnimation, tY);
}

void setDolmexicaStoryCharacterPositionZ(StoryInstance* tInstance, int tID, float tZ)
{
	StoryCharacter& e = tInstance->mStoryCharacters[tID];
	setDolmexicaStoryAnimationPositionZInternal(e.mAnimation, tZ);
}

void addDolmexicaStoryCharacterPositionX(StoryInstance* tInstance, int tID, float tX)
{
	StoryCharacter& e = tInstance->mStoryCharacters[tID];
	addDolmexicaStoryAnimationPositionXInternal(e.mAnimation, tX);
}

void addDolmexicaStoryCharacterPositionY(StoryInstance* tInstance, int tID, float tY)
{
	StoryCharacter& e = tInstance->mStoryCharacters[tID];
	addDolmexicaStoryAnimationPositionYInternal(e.mAnimation, tY);
}

void setDolmexicaStoryCharacterStagePositionX(StoryInstance* tInstance, int tID, float tX)
{
	StoryCharacter& e = tInstance->mStoryCharacters[tID];
	setDolmexicaStoryAnimationStagePositionXInternal(e.mAnimation, tX);
}

void setDolmexicaStoryCharacterStagePositionY(StoryInstance* tInstance, int tID, float tY)
{
	StoryCharacter& e = tInstance->mStoryCharacters[tID];
	setDolmexicaStoryAnimationStagePositionYInternal(e.mAnimation, tY);
}

void setDolmexicaStoryCharacterScaleX(StoryInstance* tInstance, int tID, float tX)
{
	StoryCharacter& e = tInstance->mStoryCharacters[tID];
	setDolmexicaStoryAnimationScaleXInternal(e.mAnimation, tX);
}

void setDolmexicaStoryCharacterScaleY(StoryInstance* tInstance, int tID, float tY)
{
	StoryCharacter& e = tInstance->mStoryCharacters[tID];
	setDolmexicaStoryAnimationScaleYInternal(e.mAnimation, tY);
}

void setDolmexicaStoryCharacterIsFacingRight(StoryInstance* tInstance, int tID, int tIsFacingRight)
{
	StoryCharacter& e = tInstance->mStoryCharacters[tID];
	setDolmexicaStoryAnimationIsFacingRightInternal(e.mAnimation, tIsFacingRight);
}

void setDolmexicaStoryCharacterColor(StoryInstance* tInstance, int tID, const Vector3D& tColor)
{
	StoryCharacter& e = tInstance->mStoryCharacters[tID];
	setDolmexicaStoryAnimationColorInternal(e.mAnimation, tColor);
}

void setDolmexicaStoryCharacterOpacity(StoryInstance* tInstance, int tID, float tOpacity)
{
	StoryCharacter& e = tInstance->mStoryCharacters[tID];
	setDolmexicaStoryAnimationOpacityInternal(e.mAnimation, tOpacity);
}

void setDolmexicaStoryCharacterAngle(StoryInstance* tInstance, int tID, float tAngle)
{
	StoryCharacter& e = tInstance->mStoryCharacters[tID];
	setDolmexicaStoryAnimationAngleInternal(e.mAnimation, tAngle);
}

void addDolmexicaStoryCharacterAngle(StoryInstance* tInstance, int tID, float tAngle)
{
	StoryCharacter& e = tInstance->mStoryCharacters[tID];
	addDolmexicaStoryAnimationAngleInternal(e.mAnimation, tAngle);
}

int getDolmexicaStoryCharacterTimeLeft(StoryInstance* tInstance, int tID)
{
	if (!stl_map_contains(tInstance->mStoryCharacters, tID)) {
		logWarningFormat("Querying time left for non-existing story animation %d. Defaulting to infinite.", tID);
		return INF;
	}
	StoryCharacter& e = tInstance->mStoryCharacters[tID];
	return getDolmexicaStoryAnimationTimeLeftInternal(e.mAnimation);
}

int getDolmexicaStoryIntegerVariable(StoryInstance* tInstance, int tID)
{
	return tInstance->mIntVars[tID];
}

void setDolmexicaStoryIntegerVariable(StoryInstance* tInstance, int tID, int tValue)
{
	tInstance->mIntVars[tID] = tValue;
}

void addDolmexicaStoryIntegerVariable(StoryInstance* tInstance, int tID, int tValue)
{
	tInstance->mIntVars[tID] += tValue;
}

float getDolmexicaStoryFloatVariable(StoryInstance* tInstance, int tID)
{
	return tInstance->mFloatVars[tID];
}

void setDolmexicaStoryFloatVariable(StoryInstance* tInstance, int tID, float tValue)
{
	tInstance->mFloatVars[tID] = tValue;
}

void addDolmexicaStoryFloatVariable(StoryInstance* tInstance, int tID, float tValue)
{
	tInstance->mFloatVars[tID] += tValue;
}

std::string getDolmexicaStoryStringVariable(StoryInstance* tInstance, int tID)
{
	return tInstance->mStringVars[tID];
}

void setDolmexicaStoryStringVariable(StoryInstance* tInstance, int tID, const std::string& tValue)
{
	tInstance->mStringVars[tID] = tValue;
}

void addDolmexicaStoryStringVariable(StoryInstance* tInstance, int tID, const std::string& tValue)
{
	tInstance->mStringVars[tID] = tInstance->mStringVars[tID] + tValue;
}

void addDolmexicaStoryStringVariable(StoryInstance* tInstance, int tID, int tValue)
{
	tInstance->mStringVars[tID][0] += (char)tValue;
}

StoryInstance* getDolmexicaStoryRootInstance()
{
	return &gDolmexicaStoryScreenData.mHelperInstances[-1];
}

StoryInstance* getDolmexicaStoryInstanceParent(StoryInstance* tInstance)
{
	return tInstance->mParent;
}

StoryInstance * getDolmexicaStoryHelperInstance(int tID)
{
	return &gDolmexicaStoryScreenData.mHelperInstances[tID];
}

void addDolmexicaStoryHelper(int tID, int tState, StoryInstance* tParent)
{
	if (gDolmexicaStoryScreenData.mHelperInstances.find(tID) != gDolmexicaStoryScreenData.mHelperInstances.end())
	{
		unloadStoryInstance(gDolmexicaStoryScreenData.mHelperInstances[tID]);
	}
	StoryInstance e;
	gDolmexicaStoryScreenData.mHelperInstances[tID] = e;
	initStoryInstance(gDolmexicaStoryScreenData.mHelperInstances[tID]);
	gDolmexicaStoryScreenData.mHelperInstances[tID].mParent = tParent;

	changeDolmexicaStoryStateOutsideStateHandler(&gDolmexicaStoryScreenData.mHelperInstances[tID], tState);
	updateDreamSingleStateMachineByID(gDolmexicaStoryScreenData.mHelperInstances[tID].mRegisteredStateMachine);
}

int getDolmexicaStoryGetHelperAmount(int tID)
{
	return stl_map_contains(gDolmexicaStoryScreenData.mHelperInstances, tID);
}

void removeDolmexicaStoryHelper(int tID)
{
	gDolmexicaStoryScreenData.mHelperInstances[tID].mIsScheduledForDeletion = 1;
}

void destroyDolmexicaStoryHelper(StoryInstance* tInstance)
{
	tInstance->mIsScheduledForDeletion = 1;
}

int getDolmexicaStoryIDFromString(const char * tString, StoryInstance* tInstance)
{
	int id;
	if (!strcmp("", tString)) {
		id = 1;
	}
	else {
		char* p;
		id = (int)strtol(tString, &p, 10);
		if (p == tString) {
			id = getDolmexicaStoryTextIDFromName(tInstance, tString);
		}
	}
	return id;
}

void playDolmexicaStoryMusic(const string& tPath) {
	stopMusic();
	if (isMugenBGMMusicPath(tPath.data(), gDolmexicaStoryScreenData.mPath)) {
		playMugenBGMMusicPath(tPath.data(), gDolmexicaStoryScreenData.mPath, 1);
	}
}

void stopDolmexicaStoryMusic()
{
	stopMusic();
}

void pauseDolmexicaStoryMusic()
{
	pauseMusic();
}

void resumeDolmexicaStoryMusic()
{
	resumeMusic();
}

void setDolmexicaStoryCameraFocusX(float x)
{
	setDreamMugenStageHandlerCameraEffectPositionX(x);
}

void setDolmexicaStoryCameraFocusY(float y)
{
	setDreamMugenStageHandlerCameraEffectPositionY(y);
}

void setDolmexicaStoryCameraZoom(float tScale)
{
	setDreamMugenStageHandlerCameraZoom(tScale);
}

int getDolmexicaStoryCoordinateP()
{
	return COORD_P;
}

void setDolmexicaStoryDebugStartState(int tFromState, int tDebugStartState)
{
	gDolmexicaStoryScreenData.mDebugStartStateFrom = tFromState;
	gDolmexicaStoryScreenData.mDebugStartState = tDebugStartState;
}