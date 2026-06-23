#pragma once
#include <glm/glm.hpp>

// Shared gameplay tuning for the Angry Birds physics example. Used by the
// GameLayer (level building, launching) and the scripts (pig destruction).
// All values are in world units / metres; the physics world runs in the same
// space as the orthographic camera, so 1 world unit == 1 Box2D metre.
namespace Dingo
{

	// --- Playfield (orthographic) ---
	inline constexpr float ORTHO_HEIGHT = 20.0f;          // visible world height
	inline const glm::vec2 GRAVITY = { 0.0f, -16.0f };    // a touch stronger than 9.81 for snappy arcs

	inline constexpr float GROUND_TOP_Y = -7.0f;          // top surface of the ground
	inline constexpr float GROUND_THICKNESS = 4.0f;       // ground box height (extends below the view)

	// --- Slingshot / bird ---
	inline constexpr int   BIRD_COUNT = 4;                // birds available per level
	inline constexpr float BIRD_RADIUS = 0.5f;            // half of Transform.Size
	inline constexpr float BIRD_DENSITY = 2.0f;
	inline constexpr float BIRD_FRICTION = 0.5f;
	inline constexpr float BIRD_RESTITUTION = 0.35f;
	inline constexpr float SLING_INSET_X = 3.0f;          // distance from the left edge
	inline constexpr float SLING_HEIGHT = 2.2f;           // bird rest height above the ground

	// --- Aiming (mouse drag-to-launch) ---
	inline constexpr float GRAB_RADIUS = 3.0f;            // a drag must start this close to the bird
	inline constexpr float MAX_PULL = 4.5f;               // max slingshot pull-back distance (world units)
	inline constexpr float PULL_DEADZONE = 0.4f;          // releasing a shorter pull cancels instead of firing
	inline constexpr float MAX_LAUNCH_SPEED = 32.0f;      // launch speed at full pull (world units / second)
	inline constexpr int   AIM_DOTS = 22;                 // trajectory preview dot count
	inline constexpr float AIM_DOT_STEP = 0.06f;          // seconds between preview samples
	inline constexpr float AIM_DOT_SIZE = 0.18f;
	inline constexpr float BAND_DOT_SIZE = 0.16f;         // slingshot "band" dots from fork to bird

	// --- Bird flight resolution ---
	inline constexpr float SETTLE_SPEED = 0.8f;           // below this for SETTLE_TIME = "at rest"
	inline constexpr float SETTLE_TIME = 0.9f;
	inline constexpr float FLIGHT_TIMEOUT = 6.0f;         // hard cap before the next bird loads
	inline constexpr float KILL_Y = -11.0f;               // bodies below this are out of play

	// --- Pigs ---
	inline constexpr float PIG_RADIUS = 0.6f;
	inline constexpr float PIG_DENSITY = 1.0f;
	inline constexpr float PIG_POP_SPEED = 6.0f;          // knocked faster than this => popped
	inline constexpr int   PIG_POINTS = 1000;
	inline constexpr int   BIRD_BONUS = 250;              // per unused bird on a win

	// --- Blocks (destructible structure) ---
	inline constexpr float BLOCK_DENSITY = 0.7f;
	inline constexpr float BLOCK_FRICTION = 0.6f;

	// --- Colours ---
	inline const glm::vec4 COLOR_SKY     = { 0.45f, 0.72f, 0.95f, 1.0f };
	inline const glm::vec4 COLOR_GROUND  = { 0.36f, 0.55f, 0.25f, 1.0f };
	inline const glm::vec4 COLOR_BIRD    = { 0.92f, 0.26f, 0.21f, 1.0f };
	inline const glm::vec4 COLOR_BIRD_WAIT = { 0.96f, 0.45f, 0.30f, 1.0f };
	inline const glm::vec4 COLOR_PIG     = { 0.40f, 0.80f, 0.35f, 1.0f };
	inline const glm::vec4 COLOR_WOOD    = { 0.62f, 0.43f, 0.24f, 1.0f };
	inline const glm::vec4 COLOR_WOOD_2  = { 0.72f, 0.52f, 0.30f, 1.0f };
	inline const glm::vec4 COLOR_SLING   = { 0.45f, 0.30f, 0.18f, 1.0f };
	inline const glm::vec4 COLOR_AIM     = { 1.00f, 1.00f, 1.00f, 0.85f };
	inline const glm::vec4 COLOR_TEXT    = { 0.10f, 0.12f, 0.18f, 1.0f };
	inline const glm::vec4 COLOR_TEXT_LIGHT = { 0.97f, 0.97f, 1.0f, 1.0f };

	// --- Font sizes (world units) ---
	inline constexpr float FONT_TITLE = 2.2f;
	inline constexpr float FONT_SUB = 0.9f;
	inline constexpr float FONT_HUD = 0.7f;
	inline constexpr float FONT_SMALL = 0.45f;

}
