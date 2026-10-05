#pragma once
#include "GameTuning.h"

#include <array>
#include <cstddef>

namespace Dingo
{
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
	inline constexpr float DRIVE_FACE_TOWARD_NEG_Z_DEG = 180.0f;
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
	inline constexpr float CHECK_PARRY_FIRST_HOLD = 0.1f;
	inline constexpr float CHECK_PARRY_RERAISE_GAP = 0.2f;
	inline constexpr float CHECK_PARRY_COOLED_MARGIN = 0.1f;
	inline constexpr float CHECK_RAISE_WATCH    = 0.4f;
	inline constexpr float CHECK_BLOCK_SETTLE   = 0.3f;
	inline constexpr float CHECK_BLOCK_LOWER_WATCH = 0.25f;
	inline constexpr float CHECK_BLOCK_HOLD     = 2.5f;
	inline constexpr float CHECK_BLOCK_HIT_WATCH = 1.2f;
	inline constexpr float CHECK_BLOCK_HIT_AFTER = 0.5f;
	inline constexpr float CHECK_RIPOSTE_HOLD   = 0.4f;
	inline constexpr float CHECK_RETURN_WATCH   = 1.0f;
	// The most the hips' step may change from one frame to the next once a dodge is cut at mid-dash, with no extra
	// distance so that only the pose and the carry are measured. The limit sits between the carried jerk (at most
	// 0.031 m on the four dodges) and the uncarried one (at least 0.121 m), so the check can tell a snap from a paid carry.
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

	// --- Tournament (--tournament=N) -----------------------------------------------
	inline constexpr float TOURNAMENT_BOUT_LIMIT = 90.0f;
	inline constexpr int   TOURNAMENT_MAX       = 1000;
	inline constexpr int   TOURNAMENT_STEPS_PER_FRAME = 8;
	// Tier 3 has to win this fraction of the runs against a lower tier (8 of 10).
	inline constexpr float TOURNAMENT_PASS_FRACTION = 0.8f;

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
	// A guard decision succeeds with the tier's GuardChance, so 12 in a row fail about once in 4,000 seeds.
	inline constexpr int   CHECK_AI_GUARD_DECISIONS = 12;
	inline constexpr float CHECK_AI_GUARD_WATCH = 60.0f;
	inline constexpr int   CHECK_AI_GUARD_MIN_RAISES = 2;
	inline constexpr float CHECK_AI_GUARD_FAR   = 6.0f;
	// Longer than the whole run of swings, so the hold never runs out during it.
	inline constexpr float CHECK_AI_HELD_HOLD   = 60.0f;
	inline constexpr int   CHECK_AI_HELD_SWINGS = 8;
}
