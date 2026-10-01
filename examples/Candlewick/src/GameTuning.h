#pragma once
#include <glm/glm.hpp>

#include <cstdint>

namespace Dingo
{
	inline constexpr const char* SCENE_TITLE   = "Title";
	inline constexpr const char* SCENE_KEEP    = "Keep";
	inline constexpr const char* SCENE_END     = "End";

	inline constexpr float HUD_ORTHO_SIZE      = 11.0f;
	inline constexpr float HUD_PADDING         = 0.45f;
	inline constexpr float HUD_ROOM_LABEL_SIZE = 0.55f;
	inline constexpr float HUD_ROOM_LABEL_DROP = 0.95f;

	// --- Camera (three-quarter view, fixed yaw) --------------------------------
	inline constexpr float CAMERA_FOV          = 50.0f;
	inline constexpr float CAMERA_PITCH_DEG    = 55.0f;
	inline constexpr float CAMERA_YAW_DEG      = 0.0f;
	inline constexpr float CAMERA_DISTANCE     = 10.0f;
	inline constexpr float CAMERA_FOCUS_HEIGHT = 0.8f;
	inline constexpr float CAMERA_LAG          = 6.0f;
	inline constexpr float CAMERA_NEAR         = 0.1f;
	inline constexpr float CAMERA_FAR          = 160.0f;
	inline constexpr float OVERVIEW_MARGIN     = 0.94f;

	// --- Keep geometry (1 tile = 1 m) ---------------------------------------------
	inline constexpr float TILE_SIZE           = 1.0f;
	inline constexpr float WALL_HEIGHT         = 2.0f;
	inline constexpr float FLOOR_THICKNESS     = 0.2f;
	inline constexpr int   WALL_MAX_RUN        = 4;
	inline constexpr float WALL_CAP_THICKNESS  = 0.08f;
	inline constexpr float WALL_CAP_OVERHANG   = 0.04f;
	inline constexpr float CUTAWAY_PADDING     = 0.15f;
	inline constexpr float CUTAWAY_TARGET_HEIGHT = 0.2f;

	// --- Player --------------------------------------------------------------------
	inline constexpr float PLAYER_SPEED        = 3.0f;
	inline constexpr float PLAYER_RADIUS       = 0.3f;
	inline constexpr float PLAYER_HEIGHT       = 1.7f;
	inline constexpr float PLAYER_STEP         = 0.3f;
	inline constexpr float PLAYER_SLOPE        = 45.0f;
	inline constexpr float PLAYER_SPAWN_LIFT   = 0.05f;
	inline constexpr float PLAYER_TURN_SPEED   = 14.0f;
	inline constexpr float GRAVITY_Y           = -18.0f;

	// --- Ambient: the keep has no sun ------------------------------------------
	inline constexpr glm::vec3 AMBIENT_COLOR   = { 0.55f, 0.6f, 1.0f };
	inline constexpr float AMBIENT_INTENSITY   = 0.03f;

	// --- Flames --------------------------------------------------------------------
	inline constexpr glm::vec3 FLAME_COLOR     = { 1.0f, 0.62f, 0.3f };
	inline constexpr float FLAME_EMISSIVE      = 1.1f;

	inline constexpr float BRAZIER_LIGHT_RANGE     = 9.0f;
	inline constexpr float BRAZIER_LIGHT_INTENSITY = 1.1f;
	inline constexpr float ALTAR_LIGHT_RANGE       = 11.0f;
	inline constexpr float ALTAR_LIGHT_INTENSITY   = 1.2f;

	inline constexpr float SCONCE_LIGHT_RANGE      = 2.5f;
	inline constexpr float SCONCE_LIGHT_INTENSITY  = 0.8f;
	inline constexpr float SCONCE_HEIGHT           = 1.45f;

	inline constexpr float CANDLE_LIGHT_RANGE      = 2.5f;
	inline constexpr float CANDLE_LIGHT_INTENSITY  = 0.8f;

	inline constexpr uint32_t FLAME_MESH_RINGS     = 6;
	inline constexpr uint32_t FLAME_MESH_SEGMENTS  = 8;

	// --- Colors ---------------------------------------------------------------------
	inline constexpr glm::vec4 COLOR_BG        = { 0.012f, 0.014f, 0.025f, 1.0f };
	inline constexpr glm::vec4 COLOR_STONE     = { 0.34f, 0.32f, 0.33f, 1.0f };
	inline constexpr glm::vec4 COLOR_FLOOR     = { 0.3f, 0.28f, 0.29f, 1.0f };
	inline constexpr glm::vec4 COLOR_BRASS     = { 0.72f, 0.55f, 0.24f, 1.0f };
	inline constexpr glm::vec4 COLOR_EMBER     = { 0.12f, 0.07f, 0.03f, 1.0f };
	inline constexpr glm::vec4 COLOR_WAX       = { 0.82f, 0.76f, 0.62f, 1.0f };
	inline constexpr float WAX_EMISSIVE        = 0.16f;
	inline constexpr float STONE_ROUGHNESS     = 0.9f;
	inline constexpr float CAP_ROUGHNESS       = 0.9f;
	inline constexpr float FLOOR_ROUGHNESS     = 0.8f;
	inline constexpr float BRASS_ROUGHNESS     = 0.55f;
	inline constexpr float BRASS_SPECULAR      = 0.12f;
	inline constexpr float WAX_ROUGHNESS       = 0.8f;
	inline constexpr glm::vec3 BRASS_EMISSIVE_COLOR = { 0.7f, 0.45f, 0.2f };
	inline constexpr float BRASS_EMISSIVE      = 0.18f;
	inline constexpr glm::vec4 COLOR_CAP       = { 0.2f, 0.22f, 0.28f, 1.0f };
	inline constexpr glm::vec3 CAP_EMISSIVE_COLOR = { 0.5f, 0.62f, 1.0f };
	inline constexpr float CAP_EMISSIVE        = 0.1f;
	inline constexpr glm::vec4 COLOR_CLOAK     = { 0.3f, 0.5f, 0.6f, 1.0f };
	inline constexpr glm::vec3 CLOAK_EMISSIVE_COLOR = { 0.35f, 0.55f, 0.7f };
	inline constexpr float CLOAK_EMISSIVE      = 0.14f;
	inline constexpr float CLOAK_ROUGHNESS     = 0.9f;
	inline constexpr glm::vec4 COLOR_SKIN      = { 0.82f, 0.66f, 0.54f, 1.0f };
	inline constexpr float FACE_EMISSIVE       = 0.22f;
	inline constexpr float FACE_ROUGHNESS      = 0.9f;

	inline constexpr glm::vec4 COLOR_TITLE     = { 1.0f, 0.74f, 0.42f, 1.0f };
	inline constexpr glm::vec4 COLOR_TEXT      = { 0.92f, 0.9f, 0.84f, 1.0f };
	inline constexpr glm::vec4 COLOR_TEXT_DIM  = { 0.62f, 0.6f, 0.56f, 1.0f };
}
