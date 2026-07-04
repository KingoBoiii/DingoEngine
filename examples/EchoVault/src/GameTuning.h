#pragma once
#include <glm/glm.hpp>

#include <cmath>

// Tuning constants for EchoVault. Kept in one place so the course feels tweakable.
namespace Dingo
{
	// --- Movement / character controller ---------------------------------------
	inline constexpr float PLAYER_RADIUS     = 0.35f;
	inline constexpr float PLAYER_HEIGHT     = 1.7f;
	inline constexpr float PLAYER_STEP       = 0.35f; // stair step-up height
	inline constexpr float PLAYER_SLOPE      = 48.0f; // max walkable slope, degrees
	inline constexpr float PLAYER_SPEED      = 6.0f;
	inline constexpr float JUMP_SPEED        = 7.5f;
	inline constexpr glm::vec3 GRAVITY       = { 0.0f, -18.0f, 0.0f };
	inline constexpr float RESPAWN_Y         = -12.0f; // fall below this => respawn

	// --- Camera (third-person follow) ------------------------------------------
	inline constexpr glm::vec3 CAMERA_OFFSET = { 0.0f, 7.5f, 11.0f };
	inline constexpr float CAMERA_LAG        = 6.0f; // higher = snappier follow

	// --- Orbs -------------------------------------------------------------------
	inline constexpr float ORB_COLLECT_RADIUS = 1.3f;
	inline constexpr float ORB_SPIN_DEG       = 60.0f;
	inline constexpr float ORB_BOB            = 0.25f;
	inline constexpr float ORB_EMISSIVE       = 2.4f;
	inline constexpr float ORB_MAX_HEAR_DIST  = 26.0f; // for the audio rolloff hint

	// --- Sentry -----------------------------------------------------------------
	inline constexpr float SENTRY_VIEW_RANGE = 14.0f; // detection reach
	inline constexpr float SENTRY_VIEW_CONE  = 0.55f; // cos(half-angle); ~57 deg half-cone
	inline constexpr float SENTRY_KNOCKBACK  = 11.0f; // horizontal knockback impulse speed
	inline constexpr float SENTRY_KNOCK_UP   = 5.0f;  // vertical component
	inline constexpr float SENTRY_COOLDOWN   = 1.5f;  // seconds between detections
	inline constexpr float SENTRY_EMISSIVE   = 1.6f;

	// --- Course geometry --------------------------------------------------------
	inline constexpr float PLATFORM_THICKNESS = 0.6f;

	// --- Colors -----------------------------------------------------------------
	inline constexpr glm::vec4 COLOR_BG        = { 0.03f, 0.04f, 0.08f, 1.0f };
	inline constexpr glm::vec4 COLOR_PLATFORM  = { 0.30f, 0.34f, 0.46f, 1.0f };
	inline constexpr glm::vec4 COLOR_PLATFORM2 = { 0.24f, 0.40f, 0.44f, 1.0f };
	inline constexpr glm::vec4 COLOR_MOVING    = { 0.55f, 0.42f, 0.28f, 1.0f };
	inline constexpr glm::vec4 COLOR_PLAYER    = { 0.45f, 0.85f, 0.95f, 1.0f };
	inline constexpr glm::vec4 COLOR_ORB       = { 0.98f, 0.86f, 0.35f, 1.0f };
	inline constexpr glm::vec4 COLOR_SENTRY    = { 0.85f, 0.30f, 0.32f, 1.0f };
	inline constexpr glm::vec4 COLOR_SENTRY_EYE= { 1.0f, 0.55f, 0.25f, 1.0f };
	inline constexpr glm::vec4 COLOR_GOAL      = { 0.55f, 0.95f, 0.55f, 1.0f };

	inline constexpr glm::vec4 COLOR_TEXT      = { 0.92f, 0.94f, 1.0f, 1.0f };
	inline constexpr glm::vec4 COLOR_TEXT_DIM  = { 0.62f, 0.66f, 0.80f, 1.0f };

	// --- Audio asset paths ------------------------------------------------------
	inline constexpr const char* AUDIO_ORB       = "assets/audio/orb_chime.wav";
	inline constexpr const char* AUDIO_HUM       = "assets/audio/platform_hum.wav";
	inline constexpr const char* AUDIO_AMBIENT   = "assets/audio/ambient_pad.wav";
	inline constexpr const char* AUDIO_FOOTSTEP  = "assets/audio/footstep.wav";
	inline constexpr const char* AUDIO_PICKUP    = "assets/audio/pickup.wav";
	inline constexpr const char* AUDIO_JUMP      = "assets/audio/jump.wav";
	inline constexpr const char* AUDIO_DETECTION = "assets/audio/detection.wav";
	inline constexpr const char* AUDIO_WIN       = "assets/audio/win.wav";

	inline float DistanceXZ(const glm::vec3& a, const glm::vec3& b)
	{
		const float dx = a.x - b.x;
		const float dz = a.z - b.z;
		return std::sqrt(dx * dx + dz * dz);
	}
}
