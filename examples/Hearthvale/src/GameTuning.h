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
	inline constexpr float CAMERA_DISTANCE         = 14.0f;

	inline constexpr glm::vec4 COLOR_SKY           = glm::vec4(0.55f, 0.72f, 0.88f, 1.0f);
	inline constexpr glm::vec3 SUN_DIRECTION       = glm::vec3(-0.4f, -1.0f, -0.35f);
	inline constexpr glm::vec3 SUN_COLOR           = glm::vec3(1.0f, 0.96f, 0.88f);
	inline constexpr float SUN_INTENSITY           = 1.1f;
	inline constexpr glm::vec3 AMBIENT_COLOR       = glm::vec3(0.62f, 0.7f, 0.85f);
	inline constexpr float AMBIENT_INTENSITY       = 0.35f;

	inline constexpr float PERF_FRAMES             = 600.0f;
}
