#pragma once
#include <glm/glm.hpp>

#include <cmath>

// Tuning constants and palette for the 3D dungeon crawler, shared by the gameplay
// scripts (see GameScripts.h). Kept here so the scripts read cleanly and the layer
// stays free of game data.
namespace Dingo
{
	// --- World / build ---
	inline constexpr float TILE = 2.0f;            // world size of one grid cell
	inline constexpr float WALL_HEIGHT = 2.5f;
	inline constexpr float FLOOR_THICKNESS = 1.0f;
	inline constexpr float PLAYER_RADIUS = 0.5f;   // the unit sphere mesh has r = 0.5

	// --- Player ---
	inline constexpr float PLAYER_SPEED = 6.5f;
	inline constexpr float PLAYER_MAX_HEALTH = 100.0f;
	inline constexpr float PLAYER_INVULN = 0.7f;   // i-frames after taking a hit

	// --- Combat ---
	inline constexpr float ATTACK_RANGE = 2.6f;    // world-space reach of the swing
	inline constexpr float ATTACK_DAMAGE = 34.0f;  // enemy has 60 HP -> 2 hits
	inline constexpr float ATTACK_COOLDOWN = 0.4f;
	inline constexpr float ATTACK_FX_TIME = 0.22f; // ring grow/fade duration

	// --- Enemies ---
	inline constexpr float ENEMY_SPEED = 3.3f;
	inline constexpr float ENEMY_AGGRO = 18.0f;    // only chases within this range
	inline constexpr float ENEMY_MAX_HEALTH = 60.0f;
	inline constexpr float ENEMY_CONTACT_DAMAGE = 16.0f;
	inline constexpr float ENEMY_HIT_FLASH = 0.12f;
	inline constexpr float CONTACT_RADIUS = 1.1f;  // enemy touches player

	// --- Treasure ---
	inline constexpr float COLLECT_RADIUS = 1.3f;
	inline constexpr float TREASURE_Y = 0.9f;
	inline constexpr float TREASURE_SPIN_DEG = 90.0f;

	// --- Camera / world ---
	inline const glm::vec3 GRAVITY = { 0.0f, -18.0f, 0.0f };
	inline const glm::vec3 CAMERA_OFFSET = { 0.0f, 16.0f, 12.0f };

	// --- Palette ---
	inline const glm::vec4 COLOR_FLOOR       = { 0.15f, 0.16f, 0.20f, 1.0f };
	inline const glm::vec4 COLOR_WALL_A      = { 0.42f, 0.44f, 0.52f, 1.0f };
	inline const glm::vec4 COLOR_WALL_B      = { 0.34f, 0.36f, 0.45f, 1.0f };
	inline const glm::vec4 COLOR_PLAYER      = { 0.30f, 0.85f, 0.95f, 1.0f };
	inline const glm::vec4 COLOR_PLAYER_HURT = { 1.00f, 0.55f, 0.55f, 1.0f };
	inline const glm::vec4 COLOR_TREASURE    = { 1.00f, 0.80f, 0.20f, 1.0f };
	inline const glm::vec4 COLOR_ENEMY       = { 0.90f, 0.28f, 0.28f, 1.0f };
	inline const glm::vec4 COLOR_ENEMY_FLASH = { 1.00f, 0.92f, 0.92f, 1.0f };
	inline const glm::vec4 COLOR_ATTACK      = { 1.00f, 0.92f, 0.45f, 0.45f }; // translucent
	inline const glm::vec4 COLOR_HP          = { 0.32f, 0.82f, 0.45f, 1.0f };
	inline const glm::vec4 COLOR_HP_LOW      = { 0.92f, 0.35f, 0.30f, 1.0f };
	inline const glm::vec4 COLOR_BAR_BG      = { 0.06f, 0.06f, 0.08f, 1.0f };
	inline const glm::vec4 COLOR_TEXT        = { 0.92f, 0.94f, 0.98f, 1.0f };
	inline const glm::vec4 COLOR_TEXT_DIM    = { 0.65f, 0.67f, 0.74f, 1.0f };

	// XZ-plane distance (gameplay ignores the small Y bob/drop).
	inline float DistanceXZ(const glm::vec3& a, const glm::vec3& b)
	{
		const float dx = a.x - b.x;
		const float dz = a.z - b.z;
		return std::sqrt(dx * dx + dz * dz);
	}

	inline glm::vec4 WallColor(int col, int row)
	{
		return ((col + row) & 1) ? COLOR_WALL_A : COLOR_WALL_B;
	}
}
