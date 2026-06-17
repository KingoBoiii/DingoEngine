#pragma once
#include <glm/glm.hpp>

// Shared gameplay tuning for the Dungeon Crawler, used by the GameLayer (room
// building, spawning, camera, HUD) and the scripts (movement, combat).
namespace Dingo
{

	// --- Map / camera ---
	inline constexpr float TILE_SIZE = 1.0f;
	inline constexpr float ORTHO_SIZE = 13.0f;
	inline constexpr float ORTHO_NEAR = -1.0f;
	inline constexpr float ORTHO_FAR = 1.0f;

	// --- Player ---
	inline constexpr float PLAYER_HALF = 0.34f;
	inline constexpr float PLAYER_SPEED = 4.5f;
	inline constexpr float PLAYER_MAX_HEALTH = 100.0f;
	inline constexpr float PLAYER_ATTACK_RANGE = 1.5f;
	inline constexpr float PLAYER_ATTACK_DAMAGE = 18.0f;
	inline constexpr float PLAYER_ATTACK_ACTIVE = 0.12f;
	inline constexpr float PLAYER_ATTACK_COOLDOWN = 0.35f;
	inline constexpr float PLAYER_INVULN_TIME = 0.6f;

	// --- Enemies ---
	inline constexpr float ENEMY_HALF = 0.34f;
	inline constexpr float ENEMY_SPEED = 2.3f;
	inline constexpr float ENEMY_AGGRO_RANGE = 7.0f;
	inline constexpr float ENEMY_CONTACT_DAMAGE = 12.0f;
	inline constexpr float ENEMY_MAX_HEALTH = 30.0f;
	inline constexpr float ENEMY_HIT_FLASH = 0.12f;

	// --- Loot ---
	inline constexpr float LOOT_HALF = 0.16f;
	inline constexpr float LOOT_PICKUP_DIST = 0.6f;

	// --- Colours ---
	inline const glm::vec4 COLOR_BG           = { 0.04f, 0.04f, 0.06f, 1.0f };
	inline const glm::vec4 COLOR_WALL_A       = { 0.34f, 0.36f, 0.46f, 1.0f };
	inline const glm::vec4 COLOR_WALL_B       = { 0.30f, 0.32f, 0.41f, 1.0f };
	inline const glm::vec4 COLOR_FLOOR_A      = { 0.15f, 0.15f, 0.20f, 1.0f };
	inline const glm::vec4 COLOR_FLOOR_B      = { 0.12f, 0.12f, 0.16f, 1.0f };
	inline const glm::vec4 COLOR_PLAYER       = { 0.30f, 0.80f, 0.42f, 1.0f };
	inline const glm::vec4 COLOR_PLAYER_INV   = { 0.75f, 1.00f, 0.80f, 1.0f };
	inline const glm::vec4 COLOR_ENEMY        = { 0.85f, 0.27f, 0.27f, 1.0f };
	inline const glm::vec4 COLOR_ENEMY_FLASH  = { 1.00f, 0.85f, 0.85f, 1.0f };
	inline const glm::vec4 COLOR_LOOT         = { 1.00f, 0.84f, 0.22f, 1.0f };
	inline const glm::vec4 COLOR_FACING       = { 0.92f, 0.96f, 1.00f, 1.0f };
	inline const glm::vec4 COLOR_ATTACK_RING  = { 1.00f, 0.92f, 0.45f, 1.0f };
	inline const glm::vec4 COLOR_TEXT         = { 0.95f, 0.95f, 0.97f, 1.0f };
	inline const glm::vec4 COLOR_TEXT_DIM     = { 0.70f, 0.70f, 0.75f, 1.0f };
	inline const glm::vec4 COLOR_BAR_BG       = { 0.05f, 0.05f, 0.06f, 1.0f };
	inline const glm::vec4 COLOR_ENEMY_BAR    = { 0.85f, 0.30f, 0.30f, 1.0f };

}
