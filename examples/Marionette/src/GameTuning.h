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
	inline constexpr float TITLE_HEADING_Y     = 1.2f;
	inline constexpr float TITLE_PROMPT_SIZE   = 0.5f;
	inline constexpr float TITLE_PROMPT_Y      = -2.4f;
	inline constexpr float END_HEADING_SIZE    = 1.5f;
	inline constexpr float END_HEADING_Y       = 1.2f;
	inline constexpr float END_PROMPT_SIZE     = 0.5f;
	inline constexpr float END_PROMPT_Y        = -2.4f;

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

	// --- Arena camera --------------------------------------------------------------
	inline constexpr float ARENA_CAMERA_YAW_DEG   = 0.0f;
	inline constexpr float ARENA_CAMERA_PITCH_DEG = 17.0f;
	inline constexpr float ARENA_CAMERA_LOOK_HEIGHT = 0.55f;
	inline constexpr float ARENA_CAMERA_SIDE_MARGIN = 1.1f;
	inline constexpr float ARENA_CAMERA_MIN_DISTANCE = 6.5f;
	inline constexpr float ARENA_CAMERA_MAX_DISTANCE = 24.0f;
	inline constexpr float ARENA_CAMERA_SMOOTHING = 5.0f;

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

	// Scene::OnUpdate caps its delta at 4/60 s, so a longer fixed step would be silently shortened.
	inline constexpr float FIXED_DT_MAX         = 4.0f / 60.0f;

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
}
