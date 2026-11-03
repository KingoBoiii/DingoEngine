#include "Look/NightCurve.h"
#include "GameTuning.h"

#include <algorithm>

namespace Dingo
{

	namespace
	{
		float SmoothStep(float edge0, float edge1, float x)
		{
			const float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
			return t * t * (3.0f - 2.0f * t);
		}
	}

	namespace NightCurve
	{

		float Darkness(float hours)
		{
			if (hours < 12.0f)
				return 1.0f - SmoothStep(NIGHT_DAWN_START_HOUR, NIGHT_DAWN_END_HOUR, hours);
			return SmoothStep(NIGHT_DUSK_START_HOUR, NIGHT_DUSK_END_HOUR, hours);
		}

		float Gloom(Weather weather)
		{
			switch (weather)
			{
				case Weather::Rain: return NIGHT_GLOOM_RAIN;
				case Weather::Snow: return NIGHT_GLOOM_SNOW;
				case Weather::Clear: break;
			}
			return 0.0f;
		}

		float LampAmount(float hours, Weather weather)
		{
			return std::max(Darkness(hours), Gloom(weather));
		}

		float FireflyAmount(float hours, Season season, Weather weather)
		{
			if (weather != Weather::Clear)
				return 0.0f;
			const float window = SmoothStep(FIREFLY_START_HOUR, FIREFLY_FULL_HOUR, hours) *
				(1.0f - SmoothStep(FIREFLY_FADE_OUT_START, FIREFLY_FADE_OUT_END, hours));
			return window * FIREFLY_SEASON_AMOUNT[static_cast<size_t>(season)];
		}

		float MistAmount(float hours, Season season, Weather weather)
		{
			if (weather != Weather::Clear)
				return 0.0f;
			return (1.0f - SmoothStep(MIST_FADE_OUT_START, MIST_FADE_OUT_END, hours)) * MIST_SEASON_AMOUNT[static_cast<size_t>(season)];
		}

		float ChimneyAmount(float hours, Season season)
		{
			const float morning = 1.0f - SmoothStep(8.0f, 10.0f, hours);
			const float evening = SmoothStep(16.0f, 18.0f, hours) * (1.0f - SmoothStep(24.0f, 25.0f, hours));
			const float busy = std::max(morning, evening);
			return CHIMNEY_SEASON_AMOUNT[static_cast<size_t>(season)] * (0.45f + 0.55f * busy);
		}

	}

}
