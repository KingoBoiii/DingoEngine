#pragma once
#include <glm/glm.hpp>

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

	// --- Asset checks --------------------------------------------------------------
	inline constexpr float CHECK_POSE_TIME      = 0.5f;
	inline constexpr float CHECK_POSE_TOLERANCE = 1e-4f;
	inline constexpr float CHECK_HIPS_TRAVEL    = 0.1f;

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
