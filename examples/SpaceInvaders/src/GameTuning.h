#pragma once
#include <glm/glm.hpp>

// Shared gameplay tuning, used by both the GameLayer (spawning) and the scripts
// (movement, firing, collisions).
namespace Dingo
{

	// --- Playfield ---
	inline constexpr float ORTHO_HEIGHT = 20.0f;

	// --- Player ---
	inline constexpr float PLAYER_Y = -8.4f;
	inline constexpr float PLAYER_SPEED = 13.0f;
	inline constexpr float PLAYER_FIRE_COOLDOWN = 0.35f;
	inline const glm::vec2 PLAYER_SIZE = { 1.7f, 0.7f };

	// --- Player bullet ---
	inline constexpr float PLAYER_BULLET_SPEED = 24.0f;
	inline const glm::vec2 PLAYER_BULLET_SIZE = { 0.18f, 0.8f };

	// --- Invaders ---
	inline constexpr int INVADER_ROWS = 5;
	inline constexpr int INVADER_COLS = 11;
	inline constexpr float INVADER_SPACING_X = 2.3f;
	inline constexpr float INVADER_SPACING_Y = 1.7f;
	inline constexpr float INVADER_TOP_Y = 8.2f;
	inline constexpr float INVADER_BASE_SPEED = 2.2f;
	inline constexpr float INVADER_DROP = 0.6f;
	inline constexpr float INVASION_Y = -6.4f; // invaders reaching this line = defeat
	inline const glm::vec2 INVADER_SIZE = { 1.3f, 0.95f };

	// --- Invader bombs ---
	inline constexpr float BOMB_SPEED = 9.0f;
	inline constexpr float INVADER_FIRE_INTERVAL = 1.1f;
	inline const glm::vec2 BOMB_SIZE = { 0.22f, 0.7f };

	// --- Shields ---
	inline constexpr int SHIELD_COUNT = 4;
	inline constexpr int SHIELD_COLS = 7;
	inline constexpr int SHIELD_ROWS = 3;
	inline constexpr float SHIELD_Y = -5.0f;
	inline constexpr float SHIELD_BLOCK = 0.5f;

	// --- Colours ---
	inline const glm::vec4 COLOR_BG     = { 0.02f, 0.02f, 0.06f, 1.0f };
	inline const glm::vec4 COLOR_PLAYER = { 0.35f, 0.95f, 0.45f, 1.0f };
	inline const glm::vec4 COLOR_BULLET = { 1.00f, 0.95f, 0.40f, 1.0f };
	inline const glm::vec4 COLOR_BOMB   = { 1.00f, 0.45f, 0.35f, 1.0f };
	inline const glm::vec4 COLOR_SHIELD = { 0.30f, 0.85f, 0.40f, 1.0f };
	inline const glm::vec4 COLOR_TEXT   = { 0.90f, 0.90f, 0.95f, 1.0f };

	// Top rows are worth more points and get a distinct colour.
	inline const glm::vec4 INVADER_ROW_COLORS[INVADER_ROWS] = {
		{ 0.95f, 0.40f, 0.85f, 1.0f }, // row 0 (top)
		{ 0.60f, 0.65f, 1.00f, 1.0f },
		{ 0.60f, 0.65f, 1.00f, 1.0f },
		{ 0.45f, 0.90f, 0.95f, 1.0f },
		{ 0.45f, 0.90f, 0.95f, 1.0f },
	};

	// --- Font sizes (world units) ---
	inline constexpr float FONT_TITLE = 2.4f;
	inline constexpr float FONT_SUB = 1.0f;
	inline constexpr float FONT_HUD = 0.7f;
	inline constexpr float FONT_SMALL = 0.42f;

}
