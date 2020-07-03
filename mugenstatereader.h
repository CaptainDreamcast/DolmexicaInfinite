#pragma once

#include <map>

#include <prism/animation.h>

#include "mugenassignment.h"

enum DreamMugenStateControllerType : uint8_t{

	MUGEN_STATE_CONTROLLER_TYPE_NOT_HIT_BY,
	MUGEN_STATE_CONTROLLER_TYPE_CHANGE_STATE,
	MUGEN_STATE_CONTROLLER_TYPE_CHANGE_ANIMATION,
	MUGEN_STATE_CONTROLLER_TYPE_ASSERT_SPECIAL,
	MUGEN_STATE_CONTROLLER_TYPE_EXPLOD,
	MUGEN_STATE_CONTROLLER_TYPE_PLAY_SOUND,
	MUGEN_STATE_CONTROLLER_TYPE_SET_CONTROL,
	MUGEN_STATE_CONTROLLER_TYPE_HIT_DEFINITION,
	MUGEN_STATE_CONTROLLER_TYPE_WIDTH,
	MUGEN_STATE_CONTROLLER_TYPE_SPRITE_PRIORITY,
	MUGEN_STATE_CONTROLLER_TYPE_ADD_POSITION,
	MUGEN_STATE_CONTROLLER_TYPE_SET_VARIABLE,
	MUGEN_STATE_CONTROLLER_TYPE_BIND_TARGET,
	MUGEN_STATE_CONTROLLER_TYPE_TURN,
	MUGEN_STATE_CONTROLLER_TYPE_SET_TARGET_FACING,
	MUGEN_STATE_CONTROLLER_TYPE_ADD_TARGET_LIFE,
	MUGEN_STATE_CONTROLLER_TYPE_SET_TARGET_STATE,
	MUGEN_STATE_CONTROLLER_TYPE_CHANGE_ANIMATION_2,
	MUGEN_STATE_CONTROLLER_TYPE_SET_SELF_STATE,
	MUGEN_STATE_CONTROLLER_TYPE_ADD_VELOCITY,
	MUGEN_STATE_CONTROLLER_TYPE_SET_VELOCITY,
	MUGEN_STATE_CONTROLLER_TYPE_MULTIPLY_VELOCITY,
	MUGEN_STATE_CONTROLLER_TYPE_TARGET_SET_VELOCITY,
	MUGEN_STATE_CONTROLLER_TYPE_TARGET_ADD_VELOCITY,
	MUGEN_STATE_CONTROLLER_TYPE_AFTER_IMAGE,
	MUGEN_STATE_CONTROLLER_TYPE_AFTER_IMAGE_TIME,
	MUGEN_STATE_CONTROLLER_TYPE_PALETTE_EFFECT,
	MUGEN_STATE_CONTROLLER_TYPE_SET_HIT_VELOCITY,
	MUGEN_STATE_CONTROLLER_TYPE_SCREEN_BOUND,
	MUGEN_STATE_CONTROLLER_TYPE_FREEZE_POSITION,
	MUGEN_STATE_CONTROLLER_TYPE_NULL,
	MUGEN_STATE_CONTROLLER_TYPE_SET_POSITION,
	MUGEN_STATE_CONTROLLER_TYPE_ENVIRONMENT_SHAKE,
	MUGEN_STATE_CONTROLLER_TYPE_REVERSAL_DEFINITION,
	MUGEN_STATE_CONTROLLER_TYPE_HIT_OVERRIDE,
	MUGEN_STATE_CONTROLLER_TYPE_PAUSE,
	MUGEN_STATE_CONTROLLER_TYPE_SUPER_PAUSE,
	MUGEN_STATE_CONTROLLER_TYPE_MAKE_DUST,
	MUGEN_STATE_CONTROLLER_TYPE_SET_STATE_TYPE,
	MUGEN_STATE_CONTROLLER_TYPE_FORCE_FEEDBACK,
	MUGEN_STATE_CONTROLLER_TYPE_SET_DEFENSE_MULTIPLIER,
	MUGEN_STATE_CONTROLLER_TYPE_ADD_VARIABLE,
	MUGEN_STATE_CONTROLLER_TYPE_PARENT_ADD_VARIABLE,
	MUGEN_STATE_CONTROLLER_TYPE_FALL_ENVIRONMENT_SHAKE,
	MUGEN_STATE_CONTROLLER_TYPE_HIT_FALL_DAMAGE,
	MUGEN_STATE_CONTROLLER_TYPE_HIT_FALL_VELOCITY,
	MUGEN_STATE_CONTROLLER_TYPE_SET_HIT_FALL,
	MUGEN_STATE_CONTROLLER_TYPE_SET_VARIABLE_RANGE,
	MUGEN_STATE_CONTROLLER_TYPE_REMAP_PALETTE,
	MUGEN_STATE_CONTROLLER_TYPE_HIT_BY,
	MUGEN_STATE_CONTROLLER_TYPE_PLAYER_PUSH,
	MUGEN_STATE_CONTROLLER_TYPE_ADD_POWER,
	MUGEN_STATE_CONTROLLER_TYPE_HELPER,
	MUGEN_STATE_CONTROLLER_TYPE_STOP_SOUND,
	MUGEN_STATE_CONTROLLER_TYPE_REMOVE_EXPLOD,
	MUGEN_STATE_CONTROLLER_TYPE_DESTROY_SELF,
	MUGEN_STATE_CONTROLLER_TYPE_PALETTE_EFFECT_ALL,
	MUGEN_STATE_CONTROLLER_TYPE_ENVIRONMENT_COLOR,
	MUGEN_STATE_CONTROLLER_TYPE_VICTORY_QUOTE,
	MUGEN_STATE_CONTROLLER_TYPE_SET_ATTACK_MULTIPLIER,
	MUGEN_STATE_CONTROLLER_TYPE_SET_LIFE,
	MUGEN_STATE_CONTROLLER_TYPE_ADD_LIFE,
	MUGEN_STATE_CONTROLLER_TYPE_PAN_SOUND,
	MUGEN_STATE_CONTROLLER_TYPE_DISPLAY_TO_CLIPBOARD,
	MUGEN_STATE_CONTROLLER_TYPE_APPEND_TO_CLIPBOARD,
	MUGEN_STATE_CONTROLLER_TYPE_CLEAR_CLIPBOARD,
	MUGEN_STATE_CONTROLLER_TYPE_GRAVITY,
	MUGEN_STATE_CONTROLLER_TYPE_SET_ATTACK_DISTANCE,
	MUGEN_STATE_CONTROLLER_TYPE_ADD_TARGET_POWER,
	MUGEN_STATE_CONTROLLER_TYPE_RESET_MOVE_HIT,
	MUGEN_STATE_CONTROLLER_TYPE_MODIFY_EXPLOD,
	MUGEN_STATE_CONTROLLER_TYPE_EXPLOD_BIND_TIME,
	MUGEN_STATE_CONTROLLER_TYPE_PALETTE_EFFECT_BACKGROUND,
	MUGEN_STATE_CONTROLLER_TYPE_ADD_HIT,
	MUGEN_STATE_CONTROLLER_TYPE_BIND_TO_ROOT,
	MUGEN_STATE_CONTROLLER_TYPE_BIND_TO_TARGET,
	MUGEN_STATE_CONTROLLER_TYPE_TRANSPARENCY,
	MUGEN_STATE_CONTROLLER_TYPE_BIND_TO_PARENT,
	MUGEN_STATE_CONTROLLER_TYPE_SET_ANGLE,
	MUGEN_STATE_CONTROLLER_TYPE_ADD_ANGLE,
	MUGEN_STATE_CONTROLLER_TYPE_MUL_ANGLE,
	MUGEN_STATE_CONTROLLER_TYPE_DRAW_ANGLE,
	MUGEN_STATE_CONTROLLER_TYPE_MAKE_GAME_ANIMATION,
	MUGEN_STATE_CONTROLLER_TYPE_SET_VARIABLE_RANDOM,
	MUGEN_STATE_CONTROLLER_TYPE_SET_PARENT_VARIABLE,
	MUGEN_STATE_CONTROLLER_TYPE_PROJECTILE,
	MUGEN_STATE_CONTROLLER_TYPE_SET_POWER,
	MUGEN_STATE_CONTROLLER_TYPE_SET_OFFSET,
	MUGEN_STATE_CONTROLLER_TYPE_DROP_TARGET,
	MUGEN_STATE_CONTROLLER_TYPE_GLOBAL_VAR_SET,
	MUGEN_STATE_CONTROLLER_TYPE_GLOBAL_VAR_ADD,
	MUGEN_STATE_CONTROLLER_TYPE_ZOOM,
};

enum DreamMugenStoryStateControllerType : uint8_t {

	MUGEN_STORY_STATE_CONTROLLER_TYPE_NULL,
	MUGEN_STORY_STATE_CONTROLLER_TYPE_CREATE_ANIMATION,
	MUGEN_STORY_STATE_CONTROLLER_TYPE_REMOVE_ANIMATION,
	MUGEN_STORY_STATE_CONTROLLER_TYPE_CHANGE_ANIMATION,
	MUGEN_STORY_STATE_CONTROLLER_TYPE_CREATE_TEXT,
	MUGEN_STORY_STATE_CONTROLLER_TYPE_REMOVE_TEXT,
	MUGEN_STORY_STATE_CONTROLLER_TYPE_CHANGE_TEXT,
	MUGEN_STORY_STATE_CONTROLLER_TYPE_LOCK_TEXT_TO_CHARACTER,
	MUGEN_STORY_STATE_CONTROLLER_TYPE_TEXT_SET_POSITION,
	MUGEN_STORY_STATE_CONTROLLER_TYPE_TEXT_ADD_POSITION,
	MUGEN_STORY_STATE_CONTROLLER_TYPE_NAME_ID,
	MUGEN_STORY_STATE_CONTROLLER_TYPE_CHANGE_STATE,
	MUGEN_STORY_STATE_CONTROLLER_TYPE_CHANGE_STATE_PARENT,
	MUGEN_STORY_STATE_CONTROLLER_TYPE_CHANGE_STATE_ROOT,
	MUGEN_STORY_STATE_CONTROLLER_TYPE_FADE_IN,
	MUGEN_STORY_STATE_CONTROLLER_TYPE_FADE_OUT,
	MUGEN_STORY_STATE_CONTROLLER_TYPE_GOTO_STORY_STEP,
	MUGEN_STORY_STATE_CONTROLLER_TYPE_GOTO_INTRO,
	MUGEN_STORY_STATE_CONTROLLER_TYPE_GOTO_TITLE,
	MUGEN_STORY_STATE_CONTROLLER_TYPE_ANIMATION_SET_POSITION,
	MUGEN_STORY_STATE_CONTROLLER_TYPE_ANIMATION_ADD_POSITION,
	MUGEN_STORY_STATE_CONTROLLER_TYPE_ANIMATION_SET_SCALE,
	MUGEN_STORY_STATE_CONTROLLER_TYPE_ANIMATION_SET_FACEDIRECTION,
	MUGEN_STORY_STATE_CONTROLLER_TYPE_ANIMATION_SET_ANGLE,
	MUGEN_STORY_STATE_CONTROLLER_TYPE_ANIMATION_ADD_ANGLE,
	MUGEN_STORY_STATE_CONTROLLER_TYPE_ANIMATION_SET_COLOR,
	MUGEN_STORY_STATE_CONTROLLER_TYPE_ANIMATION_SET_OPACITY,
	MUGEN_STORY_STATE_CONTROLLER_TYPE_END_STORYBOARD,
	MUGEN_STORY_STATE_CONTROLLER_TYPE_MOVE_STAGE,
	MUGEN_STORY_STATE_CONTROLLER_TYPE_CREATE_CHARACTER,
	MUGEN_STORY_STATE_CONTROLLER_TYPE_REMOVE_CHARACTER,
	MUGEN_STORY_STATE_CONTROLLER_TYPE_CHARACTER_CHANGE_ANIMATION,
	MUGEN_STORY_STATE_CONTROLLER_TYPE_CHARACTER_SET_POSITION,
	MUGEN_STORY_STATE_CONTROLLER_TYPE_CHARACTER_ADD_POSITION,
	MUGEN_STORY_STATE_CONTROLLER_TYPE_CHARACTER_SET_SCALE,
	MUGEN_STORY_STATE_CONTROLLER_TYPE_CHARACTER_SET_FACEDIRECTION,
	MUGEN_STORY_STATE_CONTROLLER_TYPE_CHARACTER_SET_COLOR,
	MUGEN_STORY_STATE_CONTROLLER_TYPE_CHARACTER_SET_OPACITY,
	MUGEN_STORY_STATE_CONTROLLER_TYPE_CHARACTER_SET_ANGLE,
	MUGEN_STORY_STATE_CONTROLLER_TYPE_CHARACTER_ADD_ANGLE,
	MUGEN_STORY_STATE_CONTROLLER_TYPE_CREATE_HELPER,
	MUGEN_STORY_STATE_CONTROLLER_TYPE_REMOVE_HELPER,
	MUGEN_STORY_STATE_CONTROLLER_TYPE_VAR_SET,
	MUGEN_STORY_STATE_CONTROLLER_TYPE_VAR_ADD,
	MUGEN_STORY_STATE_CONTROLLER_TYPE_GLOBAL_VAR_SET,
	MUGEN_STORY_STATE_CONTROLLER_TYPE_GLOBAL_VAR_ADD,
	MUGEN_STORY_STATE_CONTROLLER_TYPE_PLAY_MUSIC,
	MUGEN_STORY_STATE_CONTROLLER_TYPE_STOP_MUSIC,
	MUGEN_STORY_STATE_CONTROLLER_TYPE_PAUSE_MUSIC,
	MUGEN_STORY_STATE_CONTROLLER_TYPE_RESUME_MUSIC,
	MUGEN_STORY_STATE_CONTROLLER_TYPE_DESTROY_SELF,
	MUGEN_STORY_STATE_CONTROLLER_TYPE_CAMERA_FOCUS,
	MUGEN_STORY_STATE_CONTROLLER_TYPE_CAMERA_ZOOM,
	MUGEN_STORY_STATE_CONTROLLER_TYPE_PLAY_SOUND,
	MUGEN_STORY_STATE_CONTROLLER_TYPE_PAN_SOUND,
	MUGEN_STORY_STATE_CONTROLLER_TYPE_STOP_SOUND,
};

typedef struct {
	DreamMugenAssignment* mAssignment;

} DreamMugenStateControllerTrigger;

typedef struct {
	DreamMugenStateControllerTrigger mTrigger;
	void* mData;

	int16_t mPersistence;
	int16_t mAccessAmount;
	uint8_t mType;
} DreamMugenStateController;

typedef struct {
	int mLife;
	int mPower;
	int mAttack;
	int mDefense;
	int mFallDefenseUp;
	int mLiedownTime;
	int mAirJugglePoints;
	int mIsSparkNoInPlayerFile;
	int mSparkNo;
	int mIsGuardSparkNoInPlayerFile;
	int mGuardSparkNo;
	int mKOEcho; // unused due to technical limitations
	int mVolume;

	int mIntPersistIndex;
	int mFloatPersistIndex;

} DreamMugenConstantsHeader;

typedef struct {
	Vector2D mScale;
	int mGroundBackWidth;
	int mGroundFrontWidth;
	int mAirBackWidth;
	int mAirFrontWidth;
	int mHeight;
	int mAttackDistance;
	int mProjectileAttackDistance;
	int mDoesScaleProjectiles;
	Vector2D mHeadPosition;
	Vector2D mMidPosition;
	int mShadowOffset;
	Vector2DI mDrawOffset;

	int mHasAttackWidth;
	Vector2DI mAttackWidth;

} DreamMugenConstantsSizeData;

typedef struct {
	Vector2D mWalkForward;
	Vector2D mWalkBackward;

	Vector2D mRunForward;
	Vector2D mRunBackward;

	Vector2D mJumpNeutral;
	Vector2D mJumpBackward;
	Vector2D mJumpForward;

	Vector2D mRunJumpBackward;
	Vector2D mRunJumpForward;

	Vector2D mAirJumpNeutral;
	Vector2D mAirJumpBackward;
	Vector2D mAirJumpForward;

	Vector2D mAirGetHitGroundRecovery;
	Vector2D mAirGetHitAirRecoveryMultiplier;
	Vector2D mAirGetHitAirRecoveryOffset;

	double mAirGetHitExtraXWhenHoldingBackward;
	double mAirGetHitExtraXWhenHoldingForward;
	double mAirGetHitExtraYWhenHoldingUp;
	double mAirGetHitExtraYWhenHoldingDown;

} DreamMugenConstantsVelocityData;

typedef struct {
	int mAllowedAirJumpAmount;
	int mAirJumpMinimumHeight;
	double mVerticalAcceleration;
	double mStandFiction;
	double mCrouchFriction;
	double mStandFrictionThreshold;
	double mCrouchFrictionThreshold;
	double mJumpChangeAnimThreshold;

	int mAirGetHitGroundLevelY;
	int mAirGetHitGroundRecoveryGroundYTheshold;
	int mAirGetHitGroundRecoveryGroundGoundLevelY;
	double mAirGetHitAirRecoveryVelocityYThreshold;
	double mAirGetHitAirRecoveryVerticalAcceleration;

	int mAirGetHitTripGroundLevelY;
	Vector2D mBounceOffset;
	double mVerticalBounceAcceleration;
	int mBounceGroundLevel;
	double mLyingDownFrictionThreshold;
} DreamMugenConstantsMovementData;

typedef struct {
	int mVictoryQuoteIndex; // -1 for random
} DreamMugenConstantsQuoteData;

typedef enum {
	MUGEN_STATE_TYPE_UNCHANGED,
	MUGEN_STATE_TYPE_STANDING,
	MUGEN_STATE_TYPE_CROUCHING,
	MUGEN_STATE_TYPE_AIR,
	MUGEN_STATE_TYPE_LYING,
} DreamMugenStateType;

enum DreamMugenStateTypeFlags : uint32_t {
	MUGEN_STATE_TYPE_NO_FLAG = 0,
	MUGEN_STATE_TYPE_STANDING_FLAG = (1 << 0),
	MUGEN_STATE_TYPE_CROUCHING_FLAG = (1 << 1),
	MUGEN_STATE_TYPE_AIR_FLAG = (1 << 2),
	MUGEN_STATE_TYPE_ALL_FLAG = MUGEN_STATE_TYPE_STANDING_FLAG | MUGEN_STATE_TYPE_CROUCHING_FLAG | MUGEN_STATE_TYPE_AIR_FLAG,
};

typedef enum {
	MUGEN_STATE_MOVE_TYPE_IDLE,
	MUGEN_STATE_MOVE_TYPE_ATTACK,
	MUGEN_STATE_MOVE_TYPE_BEING_HIT,
	MUGEN_STATE_MOVE_TYPE_UNCHANGED,
} DreamMugenStateMoveType;

typedef enum {
	MUGEN_STATE_PHYSICS_UNCHANGED,
	MUGEN_STATE_PHYSICS_STANDING,
	MUGEN_STATE_PHYSICS_CROUCHING,
	MUGEN_STATE_PHYSICS_AIR,
	MUGEN_STATE_PHYSICS_NONE,

} DreamMugenStatePhysics;

enum DreamMugenStatePropertyFlags : uint16_t {
	MUGEN_STATE_PROPERTY_OVERWRITABLE =					(1 << 0),
	MUGEN_STATE_PROPERTY_CHANGING_ANIMATION =			(1 << 1),
	MUGEN_STATE_PROPERTY_SETTING_VELOCITY =				(1 << 2),
	MUGEN_STATE_PROPERTY_CHANGING_CONTROL =				(1 << 3),
	MUGEN_STATE_PROPERTY_CHANGING_SPRITE_PRIORITY =		(1 << 4),
	MUGEN_STATE_PROPERTY_ADDING_POWER =					(1 << 5),
	MUGEN_STATE_PROPERTY_JUGGLE_REQUIREMENT =			(1 << 6),
	MUGEN_STATE_PROPERTY_HIT_DEFINITION_PERSISTENCE =	(1 << 7),
	MUGEN_STATE_PROPERTY_MOVE_HIT_INFO_PERSISTENCE =	(1 << 8),
	MUGEN_STATE_PROPERTY_HIT_COUNT_PERSISTENCE =		(1 << 9),
	MUGEN_STATE_PROPERTY_FACE_PLAYER_2_INFO =			(1 << 10),
	MUGEN_STATE_PROPERTY_PRIORITY =						(1 << 11),
};

typedef struct {
	Vector mControllers;
	int mID;

	DreamMugenStateType mType;
	DreamMugenStateMoveType mMoveType;
	DreamMugenStatePhysics mPhysics;
	
	uint32_t mFlags;
	DreamMugenAssignment* mAnimation;
	DreamMugenAssignment* mVelocity;
	DreamMugenAssignment* mControl;
	DreamMugenAssignment* mSpritePriority;
	DreamMugenAssignment* mPowerAdd;
	DreamMugenAssignment* mJuggleRequired;
	DreamMugenAssignment* mDoHitDefinitionsPersist;
	DreamMugenAssignment* mDoMoveHitInfosPersist;
	DreamMugenAssignment* mDoesHitCountPersist;
	DreamMugenAssignment* mDoesFacePlayer2;
	DreamMugenAssignment* mPriority;
} DreamMugenState;

typedef struct {
	std::map<int, DreamMugenState> mStates;
} DreamMugenStates;

typedef struct {
	DreamMugenConstantsHeader mHeader;
	DreamMugenConstantsSizeData mSizeData;
	DreamMugenConstantsVelocityData mVelocityData;
	DreamMugenConstantsMovementData mMovementData;
	DreamMugenConstantsQuoteData mQuoteData;

	DreamMugenStates mStates;
} DreamMugenConstants;

DreamMugenConstants loadDreamMugenConstantsFile(char* tPath);
void unloadDreamMugenConstantsFile(DreamMugenConstants* tConstants);
void loadDreamMugenStateDefinitionsFromFile(DreamMugenStates* tStates, char* tPath, int tIsOverwritable = 0);
DreamMugenStates createEmptyMugenStates();

DreamMugenStateTypeFlags convertDreamMugenStateTypeToFlag(DreamMugenStateType tType);