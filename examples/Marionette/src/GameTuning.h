#pragma once
#include <DingoEngine/Core/KeyCodes.h>

#include <glm/glm.hpp>

#include <array>
#include <cstddef>
#include <cstdint>

namespace Dingo
{
	inline constexpr const char* SCENE_TITLE   = "Title";
	inline constexpr const char* SCENE_ARENA   = "Arena";
	inline constexpr const char* SCENE_END     = "End";

	inline constexpr float HUD_ORTHO_SIZE      = 11.0f;
	inline constexpr float TITLE_HEADING_SIZE  = 1.9f;
	inline constexpr float TITLE_HEADING_Y     = 3.1f;
	inline constexpr float TITLE_PROMPT_SIZE   = 0.5f;
	inline constexpr float TITLE_PROMPT_Y      = -4.6f;
	inline constexpr float END_HEADING_SIZE    = 1.5f;
	inline constexpr float END_HEADING_Y       = 4.5f;
	inline constexpr float END_PROMPT_SIZE     = 0.5f;
	inline constexpr float END_PROMPT_Y        = -4.8f;
	inline constexpr float END_SUBTITLE_SIZE   = 0.55f;
	inline constexpr float END_SUBTITLE_Y      = 3.35f;
	inline constexpr float END_STATS_SIZE      = 0.42f;
	inline constexpr float END_STATS_Y         = 2.75f;
	// The last blows of a win are still being pressed; the screen waits them out.
	inline constexpr float END_INPUT_DELAY     = 1.0f;
	// What --end shows for the run's result.
	inline constexpr float END_DEMO_SECONDS    = 312.0f;
	inline constexpr int   END_DEMO_RETRIES    = 2;

	// --- Camera --------------------------------------------------------------------
	inline constexpr float CAMERA_FOV           = 40.0f;
	inline constexpr float CAMERA_NEAR          = 0.1f;
	inline constexpr float CAMERA_FAR           = 120.0f;
	inline constexpr float CAMERA_FIT_MARGIN    = 0.9f;
	inline constexpr float CAMERA_FIT_START     = 3.0f;
	inline constexpr float CAMERA_FIT_MAX       = 80.0f;
	inline constexpr float CAMERA_FIT_STEP      = 0.1f;
	inline constexpr float LINEUP_PITCH_DEG     = 12.0f;
	inline constexpr float OVERVIEW_PITCH_DEG   = 62.0f;

	// --- Arena (1 unit = 1 m) ------------------------------------------------------
	inline constexpr int   ARENA_SIDES          = 12;
	inline constexpr float ARENA_RADIUS         = 8.0f;
	inline constexpr float ARENA_FLOOR_THICKNESS = 0.3f;
	inline constexpr float ARENA_WALL_HEIGHT    = 0.9f;
	inline constexpr float ARENA_WALL_THICKNESS = 0.5f;

	inline constexpr int   BRAZIER_COUNT        = 4;
	inline constexpr float BRAZIER_RING_RADIUS  = 6.4f;
	inline constexpr float BRAZIER_ANGLE_OFFSET_DEG = 45.0f;
	inline constexpr float BRAZIER_BASE_WIDTH   = 0.7f;
	inline constexpr float BRAZIER_BASE_HEIGHT  = 0.55f;
	inline constexpr float BRAZIER_BOWL_WIDTH   = 0.95f;
	inline constexpr float BRAZIER_BOWL_HEIGHT  = 0.16f;
	inline constexpr float BRAZIER_FLAME_DIAMETER = 0.5f;
	inline constexpr float BRAZIER_FLAME_RISE   = 0.18f;
	inline constexpr float BRAZIER_LIGHT_RISE   = 0.6f;
	inline constexpr float BRAZIER_LIGHT_RANGE  = 9.0f;
	inline constexpr float BRAZIER_LIGHT_INTENSITY = 0.8f;
	inline constexpr uint32_t FLAME_MESH_RINGS  = 6;
	inline constexpr uint32_t FLAME_MESH_SEGMENTS = 8;

	inline constexpr glm::vec3 AMBIENT_COLOR    = { 0.55f, 0.6f, 1.0f };
	inline constexpr float AMBIENT_INTENSITY    = 0.18f;

	inline constexpr glm::vec3 MOON_DIRECTION  = { 0.35f, -1.0f, -0.55f };
	inline constexpr glm::vec3 MOON_COLOR      = { 0.78f, 0.84f, 1.0f };
	inline constexpr float MOON_INTENSITY       = 0.75f;

	// --- Lineup --------------------------------------------------------------------
	inline constexpr float LINEUP_SPACING       = 1.7f;
	inline constexpr float LINEUP_IDLE_STAGGER  = 0.27f;
	inline constexpr float LINEUP_MARGIN        = 0.8f;
	inline constexpr float LINEUP_FIT_HEIGHT    = 1.9f;
	inline constexpr float LINEUP_LOOK_HEIGHT   = 0.7f;
	inline constexpr float FREEZE_POSE_TIME     = 0.5f;
	// Where the middle of the row stands on the title screen, in normalized device y: below the middle, under the name.
	inline constexpr float TITLE_LINEUP_CENTER_NDC = -0.32f;

	// --- End screen: the Knight taunting in front of the camera ---------------------
	inline constexpr float VICTORY_PITCH_DEG    = 9.0f;
	inline constexpr float VICTORY_LOOK_HEIGHT  = 0.95f;
	inline constexpr float VICTORY_CENTER_NDC   = -0.35f;
	// The frame reaches this far under the floor, which keeps the Knight's feet clear of the prompt at the bottom.
	inline constexpr float VICTORY_FIT_FLOOR    = 0.3f;
	inline constexpr float VICTORY_FIT_HALF_WIDTH = 1.3f;
	inline constexpr float VICTORY_FIT_HEIGHT   = 3.4f;

	// --- Fighters ------------------------------------------------------------------
	inline constexpr float FIGHTER_ROUGHNESS    = 0.85f;
	inline constexpr float SCALE_KNIGHT         = 1.0f;
	inline constexpr float SCALE_MINION         = 0.9f;
	inline constexpr float SCALE_BARBARIAN      = 1.1f;
	inline constexpr float SCALE_WARRIOR        = 1.15f;
	inline constexpr float PACE_KNIGHT          = 1.0f;
	inline constexpr float PACE_MINION          = 0.85f;
	inline constexpr float PACE_BARBARIAN       = 1.0f;
	inline constexpr float PACE_WARRIOR         = 1.15f;

	// --- Bout ----------------------------------------------------------------------
	inline constexpr float BOUT_DISTANCE        = 4.0f;
	inline constexpr float FIGHTER_SPAWN_LIFT   = 0.05f;
	inline constexpr float FIGHTER_RADIUS       = 0.3f;
	inline constexpr float FIGHTER_MIN_CAPSULE_BODY = 0.1f;
	inline constexpr float FIGHTER_STEP_HEIGHT  = 0.2f;
	inline constexpr float FIGHTER_SLOPE        = 45.0f;
	inline constexpr float GRAVITY_Y            = -18.0f;
	inline constexpr float OPPONENT_IDLE_PHASE  = 0.35f;
	inline constexpr float FREEZE_FALLBACK_PHASE = 0.47f;

	// --- Locomotion ----------------------------------------------------------------
	// Speeds are what each clip travels at in metres a second at fighter scale 1 and pace 1, from
	// the planted foot's slide (--check derives them and compares). A fighter moves at these times
	// its scale and its pace, so its feet keep still on the floor.
	inline constexpr const char* MOVE_PARAMETER = "Move";
	inline constexpr float MOVE_PARAMETER_MAX   = 12.0f;
	inline constexpr float MOVE_PARAMETER_FALL_RATE = 22.0f;
	inline constexpr float WALK_SPEED           = 0.86f;
	inline constexpr float RUN_SPEED            = 3.25f;
	inline constexpr float BACKPEDAL_SPEED      = 0.93f;
	inline constexpr float BACKPEDAL_PLAYBACK   = 1.0f;
	inline constexpr float STRAFE_CLIP_SPEED    = 3.55f;
	inline constexpr float STRAFE_PLAYBACK      = 0.7f;
	inline constexpr float STRAFE_SPEED         = STRAFE_CLIP_SPEED * STRAFE_PLAYBACK;
	inline constexpr float STRAFE_CLIP_ANGLE_DEG = 60.0f;

	inline constexpr float WALK_INTENT          = WALK_SPEED / RUN_SPEED;
	inline constexpr float MOVE_DEADZONE        = 0.12f;
	inline constexpr float MOVE_ACCEL           = 16.0f;
	inline constexpr float MOVE_DECEL           = 22.0f;

	inline constexpr float FACE_RANGE_ENTER     = 7.0f;
	inline constexpr float FACE_RANGE_LEAVE     = 8.0f;
	inline constexpr float FACE_TURN_RATE_DEG   = 540.0f;

	inline constexpr float ZONE_FORWARD_MAX_DEG = 30.0f;
	inline constexpr float ZONE_BACK_MIN_DEG    = 120.0f;
	inline constexpr float ZONE_HYSTERESIS_DEG  = 10.0f;
	inline constexpr float ZONE_ALIGN_MAX_DEG   = 35.0f;
	inline constexpr float ZONE_FADE            = 0.12f;

	// The cycle fraction the walk's and the run's shared footstep phase precedes the run's touchdown by;
	// Rig_Medium_MovementBasic.events holds the result.
	inline constexpr float FOOTSTEP_SHARED_LEAD = 0.03f;
	inline constexpr float SEPARATION_PUSH_SPEED = 2.0f;

	// --- Combat --------------------------------------------------------------------
	inline constexpr float HEALTH_KNIGHT        = 100.0f;
	inline constexpr float HEALTH_MINION        = 80.0f;
	inline constexpr float HEALTH_BARBARIAN     = 110.0f;
	inline constexpr float HEALTH_WARRIOR       = 130.0f;

	inline constexpr float DAMAGE_LIGHT_1H      = 12.0f;
	inline constexpr float DAMAGE_LIGHT_FINISHER_1H = 14.0f;
	inline constexpr float DAMAGE_LIGHT_2H      = 16.0f;
	inline constexpr float DAMAGE_LIGHT_FINISHER_2H = 18.0f;
	inline constexpr float DAMAGE_HEAVY_1H      = 26.0f;
	inline constexpr float DAMAGE_HEAVY_2H      = 30.0f;
	inline constexpr float DAMAGE_RIPOSTE       = 22.0f;

	inline constexpr float FRAME_SECONDS        = 1.0f / 60.0f;

	inline constexpr float ATTACK_FADE_IN       = 0.05f;
	inline constexpr float ATTACK_FADE_OUT      = 0.2f;
	inline constexpr float DODGE_FADE_IN        = FRAME_SECONDS;
	inline constexpr float DODGE_FADE_OUT       = 0.08f;
	// A short fade-in also gives the capsule time to pay what the interrupted move still owed the pose (MoveTravel).
	inline constexpr float HIT_REACT_FADE_IN    = 0.05f;
	inline constexpr float HIT_REACT_FADE_OUT   = 0.1f;
	inline constexpr float STAGGER_FADE_IN      = 0.05f;
	inline constexpr float STAGGER_FADE_OUT     = 0.15f;
	inline constexpr float BLOCK_RAISE_FADE     = FRAME_SECONDS;
	inline constexpr float BLOCK_LOWER_FADE     = 0.1f;
	inline constexpr float BLOCK_HIT_FADE_IN    = 0.05f;
	inline constexpr float BLOCK_HIT_FADE_OUT   = 0.1f;
	inline constexpr float DEATH_FADE           = 0.1f;
	inline constexpr uint32_t BLOCK_LAYER       = 1;

	// Seconds of the fighter's own time, which hit-stop slows.
	inline constexpr float INPUT_BUFFER         = 0.2f;
	inline constexpr float RIPOSTE_WINDOW       = 0.7f;
	inline constexpr float BLOCK_ARC_DEG        = 75.0f;
	inline constexpr float BLOCK_CHIP_FRACTION  = 0.2f;
	inline constexpr float BLOCK_PUSH_SPEED     = 3.0f;
	inline constexpr float BLOCK_PUSH_DECEL     = 22.0f;
	inline constexpr float BLOCK_MOVE_SPEED     = WALK_SPEED;
	inline constexpr float ATTACK_TURN_RATE_DEG = 360.0f;
	// The dodge clips carry only 0.25 to 0.64 m of root travel in the pose, so the capsule covers this much more over the dash window.
	inline constexpr float DODGE_EXTRA_DISTANCE = 0.9f;
	inline constexpr float DODGE_FORWARD_ARC_DEG = 45.0f;
	inline constexpr float DODGE_BACKWARD_ARC_DEG = 135.0f;
	// How far right of the facing the chop's blade crosses the target line; the attacker turns that far
	// left of its opponent in the windup, or the blow passes beside a target dead ahead.
	inline constexpr float AIM_CHOP_DEG         = 24.0f;

	inline constexpr float HITSTOP_SECONDS      = 0.07f;
	inline constexpr float HITSTOP_SPEED        = 0.05f;

	// Joint offsets are along the bone, in the rig's units; the radii scale with the fighter.
	struct HurtSphereDef
	{
		const char* Joint;
		glm::vec3 Offset;
		float Radius;
	};
	inline constexpr std::array<HurtSphereDef, 3> HURT_SPHERES = { {
		{ "hips",  { 0.0f, 0.05f, 0.0f }, 0.38f },
		{ "chest", { 0.0f, 0.03f, 0.0f }, 0.46f },
		{ "head",  { 0.0f, 0.5f, 0.0f },  0.52f },
	} };
	inline constexpr std::array<float, 3> WEAPON_SPHERE_FRACTIONS = { 0.35f, 0.65f, 0.95f };
	// The blade starts this far along the grip-to-tip axis; the guard and the handle lie before it.
	inline constexpr float WEAPON_BLADE_START = 0.3f;
	inline constexpr float WEAPON_SPHERE_WIDTH_FRACTION = 0.6f;
	inline constexpr float WEAPON_SPHERE_MIN_RADIUS = 0.06f;
	inline constexpr float WEAPON_SPHERE_MAX_RADIUS = 0.14f;
	inline constexpr float HIT_SPHERE_MESH_RADIUS = 0.5f;
	inline constexpr uint32_t HIT_SPHERE_MESH_RINGS = 10;
	inline constexpr uint32_t HIT_SPHERE_MESH_SEGMENTS = 14;

	inline constexpr glm::vec3 DEBUG_COLOR_IDLE = { 0.45f, 0.47f, 0.55f };
	inline constexpr glm::vec3 DEBUG_COLOR_HITBOX = { 1.0f, 0.12f, 0.08f };
	inline constexpr glm::vec3 DEBUG_COLOR_IFRAMES = { 0.15f, 0.4f, 1.0f };
	inline constexpr glm::vec3 DEBUG_COLOR_PARRY = { 1.0f, 0.88f, 0.1f };
	inline constexpr float DEBUG_EMISSIVE       = 1.0f;

	inline constexpr float POSE_DISTANCE        = 1.5f;

	// --- Combat audio --------------------------------------------------------------
	inline constexpr float AUDIO_COMBAT_NEAR    = 12.0f;
	inline constexpr float AUDIO_COMBAT_FAR     = 40.0f;
	inline constexpr float AUDIO_SWING_VOLUME   = 0.55f;
	inline constexpr float AUDIO_HIT_VOLUME     = 0.9f;
	inline constexpr float AUDIO_BLOCK_VOLUME   = 0.85f;
	inline constexpr float AUDIO_PARRY_VOLUME   = 0.9f;
	inline constexpr float AUDIO_DODGE_VOLUME   = 0.5f;
	inline constexpr float AUDIO_STING_VOLUME   = 0.8f;
	// The brazier crackle loops at each of the four braziers; a little pitch apart keeps them from phasing as one.
	inline constexpr float AUDIO_CRACKLE_VOLUME = 0.4f;
	inline constexpr float AUDIO_CRACKLE_NEAR   = 3.0f;
	inline constexpr float AUDIO_CRACKLE_FAR    = 20.0f;
	inline constexpr float AUDIO_CRACKLE_PITCH_STEP = 0.03f;

	// --- Controls ------------------------------------------------------------------
	inline constexpr KeyCode KEY_MOVE_FORWARD   = KeyCode::W;
	inline constexpr KeyCode KEY_MOVE_BACK      = KeyCode::S;
	inline constexpr KeyCode KEY_MOVE_LEFT      = KeyCode::A;
	inline constexpr KeyCode KEY_MOVE_RIGHT     = KeyCode::D;
	inline constexpr KeyCode KEY_MOVE_FORWARD_ALT = KeyCode::Up;
	inline constexpr KeyCode KEY_MOVE_BACK_ALT  = KeyCode::Down;
	inline constexpr KeyCode KEY_MOVE_LEFT_ALT  = KeyCode::Left;
	inline constexpr KeyCode KEY_MOVE_RIGHT_ALT = KeyCode::Right;
	inline constexpr KeyCode KEY_WALK           = KeyCode::LeftControl;
	inline constexpr KeyCode KEY_LIGHT          = KeyCode::J;
	inline constexpr KeyCode KEY_HEAVY          = KeyCode::K;
	inline constexpr KeyCode KEY_BLOCK          = KeyCode::LeftShift;
	inline constexpr KeyCode KEY_BLOCK_ALT      = KeyCode::L;
	inline constexpr KeyCode KEY_DODGE          = KeyCode::Space;

	// --- Arena camera --------------------------------------------------------------
	inline constexpr float ARENA_CAMERA_YAW_DEG   = 0.0f;
	inline constexpr float ARENA_CAMERA_PITCH_DEG = 17.0f;
	inline constexpr float ARENA_CAMERA_LOOK_HEIGHT = 0.55f;
	inline constexpr float ARENA_CAMERA_SIDE_MARGIN = 1.1f;
	inline constexpr float ARENA_CAMERA_MIN_DISTANCE = 6.5f;
	inline constexpr float ARENA_CAMERA_MAX_DISTANCE = 24.0f;
	inline constexpr float ARENA_CAMERA_SMOOTHING = 5.0f;
	// The frame keeps these above a fighter's head (a raised weapon) and under its feet, between the controls hint
	// at the bottom and the HUD bars at the top.
	inline constexpr float ARENA_CAMERA_HEAD_MARGIN = 0.35f;
	inline constexpr float ARENA_CAMERA_FEET_MARGIN = 0.1f;
	inline constexpr float ARENA_CAMERA_BOTTOM_NDC = -0.85f;
	inline constexpr float ARENA_CAMERA_TOP_PADDING = 0.04f;
	inline constexpr int   ARENA_CAMERA_CENTER_PASSES = 3;

	// --- Arena camera: what stands between it and the fighters ----------------------
	// A brazier or wall piece whose box, widened by the radius, a line to a fighter's feet, chest or head crosses is
	// hidden until the line has been clear for the delay. The line stops short of the fighter by the pad.
	inline constexpr float OCCLUSION_VIEW_RADIUS = 0.3f;
	inline constexpr float OCCLUSION_END_PAD    = 0.5f;
	inline constexpr float OCCLUSION_RESTORE_DELAY = 0.35f;
	inline constexpr std::array<float, 3> OCCLUSION_SAMPLE_FRACTIONS = { 0.0f, 0.5f, 1.0f };

	// --- Scripted drive (--drive) --------------------------------------------------
	inline constexpr float DRIVE_RAMP_SECONDS   = 4.0f;
	inline constexpr float DRIVE_CIRCLE_RADIUS  = 3.0f;
	inline constexpr float DRIVE_ORBIT_RADIUS   = 3.3f;
	inline constexpr float DRIVE_ORBIT_GAIN     = 1.5f;
	inline constexpr float DRIVE_LOG_INTERVAL   = 0.5f;
	inline constexpr float DRIVE_LANE_START     = -7.0f;
	inline constexpr float DRIVE_LANE_OFFSET    = 5.5f;
	inline constexpr float DRIVE_FACE_RIGHT_DEG = 90.0f;
	inline constexpr float DRIVE_FACE_LEFT_DEG  = -90.0f;
	inline constexpr float DRIVE_STRAFE_HOLD    = 1.0f;
	inline constexpr float DRIVE_STRAFE_ORBIT_A = 3.5f;
	inline constexpr float DRIVE_STRAFE_ORBIT_B = 6.0f;
	inline constexpr float DRIVE_STRAFE_RETREAT = 8.0f;
	inline constexpr float DRIVE_STRAFE_LOOP    = 10.0f;
	inline constexpr float DRIVE_WALL_START_X   = 2.0f;
	inline constexpr float DRIVE_WALL_CONTACT_SLACK = 0.25f;
	inline constexpr float DRIVE_WALL_HOLD      = 1.0f;
	inline constexpr float DRIVE_WALL_DEADLINE  = 8.0f;
	inline constexpr float DRIVE_WALL_MOVE_LIMIT = 0.1f;
	inline constexpr float DRIVE_WALL_QUIET     = 0.5f;

	// --- Scripted duel (--drive=duel) ----------------------------------------------
	// The leads are real seconds before the opponent's hitbox opens (its clip time over its pace). The gap
	// is what the weakest reach in the script covers: the riposte at a staggered opponent, 0.2 m further
	// back after the block has pushed the player away.
	inline constexpr float DUEL_GAP             = 1.1f;
	inline constexpr int   DUEL_CHAIN_LENGTH    = 3;
	inline constexpr float DUEL_START_DELAY     = 0.6f;
	inline constexpr float DUEL_SETTLE          = 0.5f;
	inline constexpr float DUEL_LATE_BLOCK_LEAD = 0.55f;
	inline constexpr float DUEL_PARRY_LEAD      = 0.08f;
	inline constexpr float DUEL_DODGE_LEAD      = 0.17f;
	inline constexpr float DUEL_RIPOSTE_DELAY   = 0.15f;
	inline constexpr float DUEL_CHAIN_GAP       = 1.1f;
	inline constexpr float DUEL_BUFFER_LEAD     = 0.12f;
	inline constexpr float DUEL_ROUND_TIMEOUT   = 6.0f;
	inline constexpr float DUEL_DEADLINE        = 60.0f;
	inline constexpr float DUEL_END_DELAY       = 0.6f;

	// --- Audio ---------------------------------------------------------------------
	inline constexpr float AUDIO_FOOTSTEP_VOLUME = 0.7f;
	inline constexpr float AUDIO_FOOTSTEP_QUIET  = 0.55f;
	inline constexpr float AUDIO_FOOTSTEP_PITCH_RIGHT = 0.93f;
	inline constexpr float AUDIO_STEP_NEAR       = 14.0f;
	inline constexpr float AUDIO_STEP_FAR        = 45.0f;

	// --- Asset checks --------------------------------------------------------------
	inline constexpr float CHECK_POSE_TIME      = 0.5f;
	inline constexpr float CHECK_POSE_TOLERANCE = 1e-4f;
	inline constexpr float CHECK_HIPS_TRAVEL    = 0.1f;

	// --- Movement checks -----------------------------------------------------------
	inline constexpr float CHECK_SAMPLE_STEP    = 1.0f / 240.0f;
	inline constexpr float CHECK_CONTACT_BAND   = 0.02f;
	inline constexpr float CHECK_EVENT_TOLERANCE = 0.03f;
	inline constexpr float CHECK_PHASE_TOLERANCE = 0.01f;
	inline constexpr float CHECK_PHASE_MATCH    = 0.002f;
	inline constexpr float CHECK_STEP_EARLY_MAX = 0.03f;
	inline constexpr float CHECK_STEP_LATE_MAX  = 0.09f;
	inline constexpr float CHECK_SPEED_TOLERANCE = 0.1f;
	inline constexpr float CHECK_ANGLE_TOLERANCE_DEG = 5.0f;
	inline constexpr float CHECK_GAIT_CYCLES    = 4.0f;
	inline constexpr float CHECK_GAIT_STEP      = 1.0f / 60.0f;
	inline constexpr float CHECK_SWEEP_STEP     = 0.25f;
	inline constexpr float CHECK_RAMP_SECONDS   = 4.0f;
	inline constexpr float CHECK_RAMP_HOLD      = 3.0f;
	inline constexpr std::array<float, 5> CHECK_RAMP_LENGTHS = { 3.0f, 3.5f, 4.0f, 4.5f, 5.0f };
	inline constexpr std::array<float, 2> CHECK_RAMP_DOWN_LENGTHS = { 3.0f, 5.0f };
	inline constexpr float CHECK_EVEN_TOLERANCE = 0.03f;
	inline constexpr float CHECK_FOOT_GAP_MIN   = 0.6f;
	inline constexpr float CHECK_FOOT_GAP_MAX   = 1.4f;
	inline constexpr size_t CHECK_RAMP_MIN_STEPS = 4;
	inline constexpr float CHECK_POP_RATIO      = 1.5f;
	inline constexpr float CHECK_POP_FLOOR      = 1.0e-4f;
	inline constexpr float CHECK_POP_PROBE      = 0.1f;
	// The strafe clip's 0.8 s at STRAFE_PLAYBACK: the longest cycle any gait plays.
	inline constexpr float CHECK_SLOWEST_CYCLE  = 0.8f / STRAFE_PLAYBACK;
	inline constexpr float CHECK_GAP_MAX        = 0.5f * CHECK_SLOWEST_CYCLE * 1.3f;
	inline constexpr float CHECK_STEP_GAP_MIN   = 0.24f;
	inline constexpr int   CHECK_ZONE_PHASES    = 16;
	inline constexpr size_t CHECK_ZONE_MIN_STEPS = 8;
	// The stagger is three whole frames and the hold lands a fifth of a frame past a frame boundary, so no switch sits on one.
	inline constexpr float CHECK_ZONE_HOLD      = 1.5367f;
	inline constexpr float CHECK_ZONE_STAGGER   = 0.05f;
	inline constexpr float CHECK_ZONE_BLEND_SECONDS = 4.0f;

	// --- Combat checks -------------------------------------------------------------
	inline constexpr int   DERIVE_SPEED_SPAN    = 2;
	inline constexpr float DERIVE_HITBOX_FRACTION = 0.5f;
	inline constexpr float DERIVE_WINDUP_FLOOR  = 0.1f;
	inline constexpr float DERIVE_WINDUP_MIN    = 0.1f;
	inline constexpr float DERIVE_DASH_FRACTION = 0.25f;
	inline constexpr float DERIVE_COMBO_MARGIN  = 0.05f;
	inline constexpr float DODGE_IFRAMES_BEGIN  = 0.2f;
	inline constexpr float DODGE_IFRAMES_END    = 0.75f;
	inline constexpr float PARRY_WINDOW_END     = 0.2f;
	inline constexpr float CHECK_WINDOW_TOLERANCE = 0.03f;
	inline constexpr float CHECK_ANIMATOR_STEP  = 1.0f / 60.0f;
	inline constexpr int   CHECK_PARRY_RAISES   = 8;
	inline constexpr float CHECK_DEATH_SETTLE   = 1.0f;
	inline constexpr float CHECK_DEATH_WALK     = 0.3f;
	inline constexpr float CHECK_DEATH_RAISE    = 0.2f;
	inline constexpr float CHECK_DEATH_MARGIN   = 0.3f;
	inline constexpr float CHECK_PARRY_RUN_IN   = 0.1f;
	inline constexpr float CHECK_PARRY_RUN_IN_STEP = 0.137f;
	inline constexpr float CHECK_PARRY_WATCH    = 0.5f;
	inline constexpr float CHECK_RAISE_WATCH    = 0.4f;
	inline constexpr float CHECK_BLOCK_SETTLE   = 0.3f;
	inline constexpr float CHECK_BLOCK_LOWER_WATCH = 0.25f;
	inline constexpr float CHECK_BLOCK_HOLD     = 2.5f;
	inline constexpr float CHECK_BLOCK_HIT_WATCH = 1.2f;
	inline constexpr float CHECK_BLOCK_HIT_AFTER = 0.5f;
	inline constexpr float CHECK_RIPOSTE_HOLD   = 0.4f;
	inline constexpr float CHECK_RETURN_WATCH   = 1.0f;
	// The most the hips' step may change from one frame to the next once a dodge is cut at mid-dash, with no extra
	// distance so that only the pose and the carry are measured. Measured by --check: a carried cut 0.031 m at most
	// on the four dodges, an uncarried one 0.121 m at least.
	inline constexpr float CHECK_CARRY_JERK_MAX = 0.08f;
	inline constexpr int   CHECK_CARRY_FRAMES   = 12;
	inline constexpr float CHECK_TRAVEL_TOLERANCE = 1.0e-4f;
	inline constexpr float CHECK_TRAVEL_SCALE   = 1.25f;
	// Paying a carry that long leaves enough of it in flight for a second move to cut in on top of it.
	inline constexpr float CHECK_TRAVEL_SPREAD  = 0.2f;
	inline constexpr int   CHECK_TRAVEL_SPREAD_STEPS = 6;
	// A dodge cut at mid-dash owes at least this much, or the Fighter-level check would pass on nothing.
	inline constexpr float CHECK_FIGHTER_CARRY_MIN = 0.1f;
	inline constexpr int   CHECK_RAISE_FINISH_STEPS = 200;
	inline constexpr int   CHECK_ONE_SHOT_STEPS = 400;
	inline constexpr float CHECK_BLADE_MIN_LENGTH = 0.5f;
	inline constexpr float CHECK_BLADE_MAX_LENGTH = 2.0f;
	inline constexpr float CHECK_DASH_MIN_TRAVEL = 0.02f;
	inline constexpr float CHECK_GEOMETRY_TOLERANCE = 1e-5f;
	inline constexpr float CHECK_AXIS_TOLERANCE = 1e-4f;
	inline constexpr float CHECK_POSE_HOLD_TOLERANCE = 1e-4f;
	inline constexpr float BREAK_HITBOX_MARGIN  = 0.06f;
	inline constexpr float BREAK_HITBOX_LENGTH  = 0.1f;

	// --- Live edit (--live-edit-demo) ----------------------------------------------
	// Scene seconds until the copy's hitbox range moves, and how far later it moves to (less if the one-shot would
	// return before it ends; the edit is skipped below the minimum).
	inline constexpr float LIVE_EDIT_DELAY      = 10.0f;
	inline constexpr float LIVE_EDIT_SHIFT      = 0.25f;
	inline constexpr float LIVE_EDIT_MIN_SHIFT  = 0.1f;
	// Real seconds after the write in which the game must be seen to reload.
	inline constexpr float LIVE_EDIT_RELOAD_TIMEOUT = 5.0f;

	// Scene::OnUpdate caps its delta at 4/60 s, so a longer fixed step would be silently shortened.
	inline constexpr float FIXED_DT_MAX         = 4.0f / 60.0f;

	// --- Bouts ---------------------------------------------------------------------
	inline constexpr int   BOUT_COUNT           = 3;
	inline constexpr float BOUT_INTRO_SECONDS   = 2.5f;
	// The intro waits for the opponent's taunt to end, but not longer than this.
	inline constexpr float BOUT_INTRO_MAX_SECONDS = 4.0f;
	inline constexpr float BOUT_FIGHT_BANNER_SECONDS = 1.0f;
	inline constexpr float BOUT_KO_SECONDS      = 3.0f;
	inline constexpr float BOUT_KO_TAUNT_DELAY  = 0.9f;
	// The victory or defeat sting follows the K.O. thud this long after it.
	inline constexpr float BOUT_KO_STING_DELAY  = 0.8f;
	// The winner taunts once calm, or this long after the K.O. whatever it is doing.
	inline constexpr float BOUT_KO_TAUNT_FORCE  = 1.8f;
	inline constexpr float TAUNT_FADE_IN        = 0.1f;
	inline constexpr float TAUNT_FADE_OUT       = 0.15f;
	// How far into its taunt the opponent is held for the --freeze intro frame.
	inline constexpr float INTRO_FREEZE_TIME    = 0.45f;

	// --- Tournament (--tournament=N) -----------------------------------------------
	inline constexpr float TOURNAMENT_BOUT_LIMIT = 90.0f;
	inline constexpr int   TOURNAMENT_MAX       = 1000;
	inline constexpr int   TOURNAMENT_STEPS_PER_FRAME = 8;
	inline constexpr int   STEPS_PER_FRAME_MAX  = 64;
	// Tier 3 has to win this fraction of the runs against a lower tier (8 of 10).
	inline constexpr float TOURNAMENT_PASS_FRACTION = 0.8f;

	// --- AI ------------------------------------------------------------------------
	// A tier is a bundle of probabilities and delays; every behaviour reads its own row.
	struct AiTierParams
	{
		const char* Name;
		// The AI sees the opponent as it was this long ago.
		float ReactionTime;
		float AttackIntervalMin;
		float AttackIntervalMax;
		float HeavyChance;
		int   MaxChain;
		// Per link after the first, once the previous swing landed.
		float ChainChance;
		// On seeing a windup that reaches it, tried in this order: parry, dodge, block, step back.
		float ParryChanceLight;
		float ParryChanceHeavy;
		float DodgeChance;
		float BlockChance;
		float StepBackChance;
		float RiposteChance;
		float RiposteDelay;
		float PunishChance;
		bool  Strafes;
		float BaitChance;
		float RetreatMin;
		float RetreatMax;
		// Intent length while closing in from afar.
		float ApproachIntent;
		// How far outside its own reach it likes to wait.
		float GapSlack;
	};

	inline constexpr std::array<AiTierParams, 3> AI_TIERS = { {
		{ "Recruit",  0.45f, 1.6f, 2.4f, 0.0f,  1, 0.0f,  0.0f,  0.0f,  0.0f, 0.0f,  0.0f, 0.0f, 0.0f,  0.0f, false, 0.0f, 0.0f, 0.0f, 0.5f,  0.0f },
		{ "Veteran",  0.30f, 1.0f, 1.8f, 0.3f,  2, 0.7f,  0.0f,  0.0f,  0.0f, 0.33f, 1.0f, 0.6f, 0.25f, 0.6f, true,  0.0f, 0.5f, 0.9f, 0.85f, 0.25f },
		{ "Champion", 0.18f, 0.6f, 1.2f, 0.25f, 3, 0.95f, 0.35f, 0.6f,  0.3f, 1.0f,  0.0f, 1.0f, 0.12f, 1.0f, true,  0.3f, 0.4f, 0.8f, 1.0f,  0.25f },
	} };

	// The block goes up this long before the opponent's hitbox opens, so the hit falls inside the parry window.
	inline constexpr float AI_PARRY_LEAD        = 0.06f;
	// The dodge starts this long before the hit, which puts it inside the iframes (0.08 to 0.30 s of the dodge).
	inline constexpr float AI_DODGE_LEAD        = 0.17f;
	// A dodge started with less than this to go would run into the hit before its iframes open.
	inline constexpr float AI_DODGE_MIN_LEAD    = 0.1f;
	inline constexpr float AI_REACH_MARGIN      = 0.12f;
	inline constexpr float AI_MIN_ATTACK_DISTANCE = 0.8f;
	inline constexpr float AI_FALLBACK_REACH    = 1.4f;
	// A windup threatens when the opponent's move reaches this far beyond the gap.
	inline constexpr float AI_THREAT_MARGIN     = 0.3f;
	inline constexpr float AI_THREAT_ARC_DEG    = 70.0f;
	inline constexpr float AI_BLOCK_LINGER      = 0.3f;
	inline constexpr float AI_PLAN_MAX_SECONDS  = 1.8f;
	inline constexpr float AI_LATE_SECONDS      = -0.05f;
	inline constexpr float AI_PUNISH_COOLDOWN   = 0.5f;
	inline constexpr float AI_PUNISH_PAST_HITBOX = 0.02f;
	inline constexpr float AI_STRAFE_INTENT     = 0.5f;
	inline constexpr float AI_STRAFE_TURN_MIN   = 1.0f;
	inline constexpr float AI_STRAFE_TURN_MAX   = 2.5f;
	inline constexpr float AI_STRAFE_FLIP_CHANCE = 0.5f;
	inline constexpr float AI_SPACING_TOLERANCE = 0.2f;
	inline constexpr float AI_SPACING_GAIN      = 1.5f;
	inline constexpr float AI_BACKOFF_INTENT    = 0.6f;
	inline constexpr float AI_CLOSE_INTENT      = 0.4f;
	inline constexpr float AI_CLOSE_RANGE       = 1.6f;
	inline constexpr float AI_RETREAT_INTENT    = 0.8f;
	inline constexpr float AI_BAIT_SECONDS      = 1.2f;
	inline constexpr float AI_BAIT_MARGIN       = 0.1f;
	inline constexpr float AI_WALL_MARGIN       = 1.6f;
	inline constexpr float AI_WALL_PUSH         = 1.5f;
	inline constexpr float AI_START_STAGGER     = 0.6f;
	inline constexpr uint32_t AI_PLAYER_SEED_OFFSET = 7919;

	// --- Reach (the AI's knowledge of its own moves) -------------------------------
	inline constexpr float REACH_SAMPLE_STEP    = 1.0f / 120.0f;
	inline constexpr float REACH_SCAN_MIN       = 0.3f;
	inline constexpr float REACH_SCAN_MAX       = 3.4f;
	inline constexpr float REACH_SCAN_STEP      = 0.05f;
	inline constexpr float REACH_SANE_MIN       = 0.8f;
	inline constexpr float REACH_SANE_MAX       = 2.8f;
	inline constexpr float REACH_SANE_DELAY     = 1.5f;

	// --- AI checks -----------------------------------------------------------------
	inline constexpr float CHECK_AI_STEP        = 1.0f / 60.0f;
	inline constexpr int   CHECK_AI_SWINGS      = 300;
	inline constexpr float CHECK_AI_SWING_GAP   = 3.0f;
	inline constexpr float CHECK_AI_DISTANCE    = 1.2f;
	inline constexpr float CHECK_AI_LAG_SLACK   = 1.0e-4f;
	inline constexpr int   CHECK_AI_SEEDS       = 6;
	inline constexpr float CHECK_AI_CHANCE_LOW  = 0.25f;
	inline constexpr float CHECK_AI_CHANCE_HIGH = 0.42f;
	inline constexpr float CHECK_AI_DODGE_SECONDS = 0.32f;

	// --- HUD -----------------------------------------------------------------------
	inline constexpr float HUD_PADDING          = 0.5f;
	inline constexpr float HUD_BAR_WIDTH        = 6.2f;
	inline constexpr float HUD_BAR_HEIGHT       = 0.46f;
	inline constexpr float HUD_BAR_INSET        = 0.06f;
	inline constexpr float HUD_BAR_CENTER_GAP   = 1.5f;
	inline constexpr float HUD_BAR_DROP         = 1.05f;
	inline constexpr float HUD_NAME_SIZE        = 0.4f;
	inline constexpr float HUD_NAME_DROP        = 0.55f;
	inline constexpr float HUD_BOUT_LABEL_SIZE  = 0.38f;
	inline constexpr float HUD_BOUT_LABEL_DROP  = 0.55f;
	inline constexpr float HUD_FILL_LIFT        = 0.1f;
	inline constexpr float HUD_TRAIL_LIFT       = 0.05f;
	inline constexpr float HUD_TRAIL_DELAY      = 0.5f;
	inline constexpr float HUD_TRAIL_RATE       = 0.5f;
	inline constexpr float HUD_FADE_Z           = 0.5f;
	inline constexpr float HUD_FADE_MARGIN      = 1.0f;
	inline constexpr float HUD_FADE_IN_SECONDS  = 0.5f;
	inline constexpr float HUD_FADE_OUT_SECONDS = 0.5f;
	inline constexpr float HUD_BANNER_BIG_SIZE  = 1.7f;
	inline constexpr float HUD_BANNER_BIG_Y     = 1.1f;
	inline constexpr float HUD_BANNER_TITLE_SIZE = 0.85f;
	inline constexpr float HUD_BANNER_TITLE_Y   = -0.35f;
	inline constexpr float HUD_BANNER_SMALL_SIZE = 0.48f;
	inline constexpr float HUD_BANNER_SMALL_Y   = -1.1f;
	inline constexpr float HUD_BANNER_FADE      = 0.35f;
	inline constexpr float HUD_FIGHT_SIZE       = 2.0f;
	inline constexpr float HUD_FIGHT_Y          = 0.4f;
	inline constexpr float HUD_KO_SIZE          = 2.4f;
	inline constexpr float HUD_KO_Y             = 0.9f;
	inline constexpr float HUD_HINT_SIZE        = 0.34f;
	inline constexpr float HUD_HINT_RISE        = 0.6f;
	inline constexpr float HUD_HINT_SECONDS     = 12.0f;

	// --- Colors --------------------------------------------------------------------
	inline constexpr glm::vec4 COLOR_BG         = { 0.016f, 0.016f, 0.03f, 1.0f };
	inline constexpr glm::vec4 COLOR_FLOOR      = { 0.34f, 0.31f, 0.3f, 1.0f };
	inline constexpr glm::vec4 COLOR_WALL       = { 0.27f, 0.25f, 0.26f, 1.0f };
	inline constexpr glm::vec4 COLOR_BRAZIER    = { 0.2f, 0.17f, 0.15f, 1.0f };
	inline constexpr glm::vec4 COLOR_FLAME      = { 1.0f, 0.62f, 0.3f, 1.0f };
	inline constexpr glm::vec3 FLAME_COLOR      = { 1.0f, 0.62f, 0.3f };
	inline constexpr float FLAME_EMISSIVE       = 1.1f;
	inline constexpr float FLOOR_ROUGHNESS      = 0.85f;
	inline constexpr float WALL_ROUGHNESS       = 0.9f;
	inline constexpr float BRAZIER_ROUGHNESS    = 0.7f;

	inline constexpr glm::vec4 COLOR_TITLE      = { 1.0f, 0.74f, 0.42f, 1.0f };
	inline constexpr glm::vec4 COLOR_TEXT       = { 0.92f, 0.9f, 0.84f, 1.0f };
	inline constexpr glm::vec4 COLOR_TEXT_DIM   = { 0.62f, 0.6f, 0.56f, 1.0f };
	inline constexpr glm::vec4 COLOR_TEXT_ALERT = { 0.95f, 0.42f, 0.32f, 1.0f };
	inline constexpr glm::vec4 COLOR_BAR_BACK   = { 0.07f, 0.06f, 0.06f, 0.88f };
	inline constexpr glm::vec4 COLOR_BAR_PLAYER = { 1.0f, 0.74f, 0.42f, 1.0f };
	inline constexpr glm::vec4 COLOR_BAR_OPPONENT = { 0.9f, 0.28f, 0.24f, 1.0f };
	inline constexpr glm::vec4 COLOR_BAR_TRAIL  = { 0.97f, 0.93f, 0.85f, 0.9f };
	inline constexpr glm::vec4 COLOR_FADE       = { 0.0f, 0.0f, 0.0f, 1.0f };
}
