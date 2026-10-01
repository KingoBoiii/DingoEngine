#pragma once
#include <glm/glm.hpp>

namespace Dingo
{
	inline constexpr const char* SCENE_TITLE   = "Title";
	inline constexpr const char* SCENE_KEEP    = "Keep";
	inline constexpr const char* SCENE_END     = "End";

	inline constexpr float HUD_ORTHO_SIZE      = 11.0f;

	// --- Camera (three-quarter view, fixed yaw) --------------------------------
	inline constexpr float CAMERA_FOV          = 50.0f;
	inline constexpr float CAMERA_PITCH_DEG    = 55.0f;
	inline constexpr float CAMERA_YAW_DEG      = 0.0f;
	inline constexpr float CAMERA_DISTANCE     = 9.0f;
	inline constexpr float CAMERA_NEAR         = 0.1f;
	inline constexpr float CAMERA_FAR          = 100.0f;

	// --- Keep geometry (1 tile = 1 m) ---------------------------------------------
	inline constexpr float TILE_SIZE           = 1.0f;
	inline constexpr float WALL_HEIGHT         = 2.0f;
	inline constexpr float FLOOR_THICKNESS     = 0.2f;

	// --- Ambient: the keep has no sun ------------------------------------------
	inline constexpr glm::vec3 AMBIENT_COLOR   = { 0.55f, 0.6f, 1.0f };
	inline constexpr float AMBIENT_INTENSITY   = 0.03f;

	// --- Brazier -------------------------------------------------------------------
	inline constexpr glm::vec3 FLAME_COLOR     = { 1.0f, 0.62f, 0.3f };
	inline constexpr float BRAZIER_LIGHT_RANGE     = 9.0f;
	inline constexpr float BRAZIER_LIGHT_INTENSITY = 1.1f;
	inline constexpr float BRAZIER_LIGHT_HEIGHT    = 0.5f;
	inline constexpr float BRAZIER_EMISSIVE        = 1.2f;

	// --- Sconce ---------------------------------------------------------------------
	inline constexpr float SCONCE_LIGHT_RANGE      = 2.5f;
	inline constexpr float SCONCE_LIGHT_INTENSITY  = 0.8f;
	inline constexpr float SCONCE_HEIGHT           = 1.45f;
	inline constexpr float SCONCE_EMISSIVE         = 1.1f;

	// --- Colors ---------------------------------------------------------------------
	inline constexpr glm::vec4 COLOR_BG        = { 0.012f, 0.014f, 0.025f, 1.0f };
	inline constexpr glm::vec4 COLOR_STONE     = { 0.34f, 0.32f, 0.33f, 1.0f };
	inline constexpr glm::vec4 COLOR_FLOOR     = { 0.26f, 0.24f, 0.25f, 1.0f };
	inline constexpr glm::vec4 COLOR_BRASS     = { 0.72f, 0.55f, 0.24f, 1.0f };
	inline constexpr glm::vec4 COLOR_EMBER     = { 0.3f, 0.18f, 0.08f, 1.0f };

	inline constexpr glm::vec4 COLOR_TITLE     = { 1.0f, 0.74f, 0.42f, 1.0f };
	inline constexpr glm::vec4 COLOR_TEXT      = { 0.92f, 0.9f, 0.84f, 1.0f };
	inline constexpr glm::vec4 COLOR_TEXT_DIM  = { 0.62f, 0.6f, 0.56f, 1.0f };
}
