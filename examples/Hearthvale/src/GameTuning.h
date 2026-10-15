#pragma once
#include <glm/glm.hpp>

#include <cstdint>

namespace Dingo
{
	inline constexpr const char* SCENE_VALLEY = "Valley";

	inline constexpr float SIM_STEP_SECONDS        = 1.0f / 60.0f;
	inline constexpr float FIXED_DT_MAX            = 0.1f;
	inline constexpr int STEPS_PER_FRAME_MAX       = 600;

	inline constexpr int VALLEY_SIZE               = 96;
	inline constexpr int CELL_TILES                = 16;
	inline constexpr float TILE_SIZE               = 1.0f;

	// The scene bakes Static meshes in fixed 32 m cells by bounds centre (plan 1.4, E9); game cells nest in them.
	inline constexpr float ENGINE_STATIC_CELL      = 32.0f;

	// Every slab's bounds centre stays above y = 0, so ground and what stands on it share an engine cell.
	inline constexpr float GROUND_TOP              = 0.2f;
	inline constexpr float WATER_TOP               = 0.12f;
	inline constexpr float RIM_TOP                 = 0.6f;
	inline constexpr float SOIL_TOP                = 0.24f;
	inline constexpr float DECOR_VARIANT_SCALE[]   = { 0.85f, 1.0f, 1.15f };

	inline constexpr float DAY_START_HOUR          = 6.0f;
	inline constexpr float DAY_END_HOUR            = 26.0f;
	inline constexpr float DAY_REAL_SECONDS        = 12.0f * 60.0f;
	inline constexpr int SIM_TICKS_PER_SECOND      = 60;
	inline constexpr int DAY_TICKS                 = 12 * 60 * SIM_TICKS_PER_SECOND;
	inline constexpr int DAYS_PER_SEASON           = 14;
	inline constexpr int SEASON_COUNT              = 4;
	inline constexpr int DAYS_MAX                  = DAYS_PER_SEASON * SEASON_COUNT * 4;

	inline constexpr int SIM_MAX_STEPS_REAL_TIME   = 8;

	// Weather rolls in per-mille; winter has snow and no rain. The first day is always clear.
	inline constexpr int WEATHER_PRECIP_PERMILLE_SPRING = 250;
	inline constexpr int WEATHER_PRECIP_PERMILLE_SUMMER = 150;
	inline constexpr int WEATHER_PRECIP_PERMILLE_FALL   = 280;
	inline constexpr int WEATHER_PRECIP_PERMILLE_WINTER = 400;

	inline constexpr int BUILD_REFUND_PERMILLE     = 500;

	inline constexpr int TARGET_MOUSE_REACH_TILES  = 3;
	inline constexpr float TARGET_MARKER_HEIGHT    = 0.04f;
	inline constexpr float TARGET_MARKER_LIFT      = 0.03f;
	inline constexpr glm::vec4 TARGET_COLOR_TOOL    = glm::vec4(1.0f, 0.95f, 0.6f, 0.45f);
	inline constexpr glm::vec4 TARGET_COLOR_VALID   = glm::vec4(0.35f, 1.0f, 0.4f, 0.5f);
	inline constexpr glm::vec4 TARGET_COLOR_INVALID = glm::vec4(1.0f, 0.3f, 0.25f, 0.5f);
	inline constexpr glm::vec4 GHOST_COLOR_VALID    = glm::vec4(0.55f, 1.0f, 0.6f, 0.55f);
	inline constexpr glm::vec4 GHOST_COLOR_INVALID  = glm::vec4(1.0f, 0.4f, 0.35f, 0.55f);

	// Valley generation densities, in per-mille of eligible tiles.
	inline constexpr int RIM_DEPTH_MIN             = 6;
	inline constexpr int RIM_DEPTH_MAX             = 9;
	inline constexpr int DECOR_RIM_PERMILLE        = 740;
	inline constexpr int DECOR_OVERGROWN_PERMILLE  = 500;
	inline constexpr int DECOR_MEADOW_PERMILLE     = 420;
	inline constexpr int DECOR_VILLAGE_PERMILLE    = 120;
	inline constexpr int DECOR_COUNT_MIN           = 3000;
	inline constexpr int DECOR_COUNT_MAX           = 5200;

	inline constexpr float CAMERA_FOV              = 40.0f;
	inline constexpr float CAMERA_NEAR             = 0.1f;
	inline constexpr float CAMERA_FAR              = 400.0f;
	inline constexpr float CAMERA_PITCH_DEG        = 45.0f;
	inline constexpr float CAMERA_YAW_STEP_DEG     = 90.0f;
	inline constexpr float CAMERA_YAW_SPEED_DEG    = 360.0f;
	inline constexpr float CAMERA_FOCUS_HEIGHT     = 0.7f;
	inline constexpr float CAMERA_LAG              = 7.0f;
	inline constexpr float CAMERA_DISTANCE         = 14.0f;
	inline constexpr float CAMERA_DISTANCE_MIN     = 8.0f;
	inline constexpr float CAMERA_DISTANCE_MAX     = 26.0f;
	inline constexpr float CAMERA_ZOOM_SPEED       = 10.0f;
	inline constexpr float CAMERA_ZOOM_WHEEL_STEP  = 1.5f;
	inline constexpr float BLOOM_INTENSITY         = 0.3f;
	inline constexpr float BLOOM_THRESHOLD         = 1.0f;

	inline constexpr float PLAYER_SPEED            = 4.0f;
	inline constexpr float PLAYER_TURN_SPEED       = 14.0f;
	inline constexpr float PLAYER_RADIUS           = 0.25f;
	inline constexpr float PLAYER_HEIGHT           = 1.4f;
	inline constexpr float PLAYER_STEP_HEIGHT      = 0.25f;
	inline constexpr float PLAYER_SLOPE_DEG        = 50.0f;
	inline constexpr float PLAYER_GRAVITY_Y        = -18.0f;
	inline constexpr float PLAYER_TERMINAL_Y       = -30.0f;
	inline constexpr float PLAYER_SPAWN_LIFT       = 0.05f;
	inline constexpr float PLAYER_MOVE_THRESHOLD   = 0.1f;
	inline constexpr float PLAYER_BLOCK_LOOKAHEAD  = 1.0f / 30.0f;
	inline constexpr float PLAYER_ANIM_FADE        = 0.2f;
	inline constexpr float PLAYER_WALK_ANIM_SPEED  = 2.0f;
	inline constexpr float PLAYER_MODEL_HEIGHT     = 1.5f;
	inline constexpr float PLAYER_MODEL_ROUGHNESS  = 0.85f;

	// One sample of the day. Keys are evaluated by SkyCurve (Look/SkyCurve.h): linear between neighbours in
	// hours, with the elevation capped at SKY_ELEVATION_MAX_DEG.
	struct SkyKey
	{
		float Hour;
		float ElevationDeg;
		glm::vec3 SunColor;
		float SunIntensity;
		glm::vec3 AmbientColor;
		float AmbientIntensity;
		glm::vec3 SkyColor;
		float FogStart;
		float FogEnd;
	};

	// Six keys: dawn, morning, noon, late afternoon, just after sunset (the sun is below the horizon by 20:00), deep night.
	inline constexpr SkyKey SKY_KEYS[] = {
		{  6.0f,  10.0f, glm::vec3(1.00f, 0.62f, 0.38f), 0.70f, glm::vec3(0.55f, 0.60f, 0.80f), 0.22f, glm::vec3(0.95f, 0.72f, 0.55f), 60.0f, 220.0f },
		{  9.0f,  38.0f, glm::vec3(1.00f, 0.93f, 0.82f), 1.00f, glm::vec3(0.60f, 0.70f, 0.88f), 0.32f, glm::vec3(0.62f, 0.78f, 0.92f), 70.0f, 240.0f },
		{ 12.0f,  65.0f, glm::vec3(1.00f, 0.97f, 0.90f), 1.15f, glm::vec3(0.62f, 0.72f, 0.90f), 0.38f, glm::vec3(0.55f, 0.74f, 0.93f), 80.0f, 260.0f },
		{ 16.0f,  40.0f, glm::vec3(1.00f, 0.82f, 0.55f), 1.00f, glm::vec3(0.64f, 0.68f, 0.85f), 0.34f, glm::vec3(0.70f, 0.76f, 0.86f), 70.0f, 240.0f },
		{ 20.0f,  -4.0f, glm::vec3(1.00f, 0.45f, 0.25f), 0.10f, glm::vec3(0.40f, 0.42f, 0.68f), 0.22f, glm::vec3(0.30f, 0.26f, 0.42f), 45.0f, 180.0f },
		{ 26.0f, -40.0f, glm::vec3(0.30f, 0.35f, 0.60f), 0.00f, glm::vec3(0.22f, 0.30f, 0.58f), 0.16f, glm::vec3(0.04f, 0.06f, 0.14f), 35.0f, 140.0f },
	};

	inline constexpr float SKY_ELEVATION_MAX_DEG   = 65.0f;
	inline constexpr float SKY_ANGLE_STEP_DEG      = 0.25f;
	inline constexpr float SKY_BLEND_SECONDS       = 1.0f;
	// The sun crosses from east (0) through south (90) to west (180) between these hours.
	inline constexpr float SKY_AZIMUTH_START_HOUR  = 6.0f;
	inline constexpr float SKY_AZIMUTH_END_HOUR    = 20.0f;
	inline constexpr float SKY_AZIMUTH_START_DEG   = 0.0f;
	inline constexpr float SKY_AZIMUTH_END_DEG     = 180.0f;
	// A shadow-casting sun flips its cascades as it grazes the horizon, so it stops casting below this.
	inline constexpr float SKY_SHADOW_MIN_ELEVATION_DEG = 3.0f;
	inline constexpr float SUN_SHADOW_STRENGTH     = 0.85f;
	inline constexpr float SKY_SUN_FADE_DEG        = 6.0f;

	inline constexpr glm::vec3 MOON_COLOR          = glm::vec3(0.55f, 0.65f, 1.00f);
	inline constexpr float MOON_INTENSITY          = 0.28f;
	inline constexpr float MOON_FADE_DEG           = 12.0f;

	// Morning mist: strongest at the first hour, gone by the end hour.
	inline constexpr float MIST_START_HOUR         = 6.0f;
	inline constexpr float MIST_END_HOUR           = 9.0f;
	inline constexpr float MIST_FOG_SCALE          = 0.30f;
	inline constexpr float MIST_CLEAR_WEIGHT       = 0.70f;
	inline constexpr float WEATHER_RAIN_SUN_SCALE  = 0.55f;
	inline constexpr float WEATHER_RAIN_FOG_SCALE  = 0.45f;
	inline constexpr float WEATHER_SNOW_SUN_SCALE  = 0.75f;
	inline constexpr float WEATHER_SNOW_FOG_SCALE  = 0.65f;

	inline constexpr float HUD_CLOCK_SIZE          = 24.0f;
	inline constexpr glm::vec4 HUD_TEXT_COLOR      = glm::vec4(1.0f, 0.97f, 0.9f, 1.0f);
	inline constexpr glm::vec4 HUD_PANEL_COLOR     = glm::vec4(0.1f, 0.08f, 0.06f, 0.55f);

	inline constexpr float HUD_MENU_SIZE           = 18.0f;
	inline constexpr float HUD_MESSAGE_SECONDS     = 2.5f;
	inline constexpr int HUD_MENU_ROWS             = 9;
	inline constexpr glm::vec4 HUD_TEXT_DIM_COLOR  = glm::vec4(0.65f, 0.6f, 0.55f, 1.0f);
	inline constexpr glm::vec4 HUD_TEXT_PICK_COLOR = glm::vec4(1.0f, 0.88f, 0.4f, 1.0f);

	inline constexpr float PERF_FRAMES             = 600.0f;
}
