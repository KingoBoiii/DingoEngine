#include "Checks/Checks.h"
#include "Farm/Calendar.h"
#include "GameTuning.h"
#include "Look/SkyCurve.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <iterator>

namespace Dingo
{

	namespace
	{
		constexpr float k_HourStep = 1.0f / 60.0f;
		constexpr float k_Eps = 1e-4f;

		bool Near(float a, float b, float eps = k_Eps)
		{
			return std::abs(a - b) <= eps;
		}

		void CheckKeys(CheckReport& report)
		{
			bool ascending = true;
			for (size_t i = 1; i < std::size(SKY_KEYS); ++i)
				ascending = ascending && SKY_KEYS[i].Hour > SKY_KEYS[i - 1].Hour;
			report.Check(std::size(SKY_KEYS) == 6 && ascending, "Sky: six keys, ascending in hours");
			report.Check(SKY_KEYS[0].Hour == DAY_START_HOUR && SKY_KEYS[5].Hour == 26.0f, "Sky: the keys span 6:00 to 26:00");

			bool bounded = true;
			for (const SkyKey& key : SKY_KEYS)
				bounded = bounded && key.FogStart > 0.0f && key.FogStart < key.FogEnd && key.AmbientIntensity > 0.0f && key.AmbientIntensity <= 1.0f;
			report.Check(bounded, "Sky: every key has fog start below fog end and an ambient intensity in (0, 1]");
		}

		void CheckQuantize(CheckReport& report)
		{
			bool multiples = true;
			for (float degrees = -45.0f; degrees < 70.0f; degrees += 0.0371f)
			{
				const float q = SkyCurve::Quantize(degrees, SKY_ANGLE_STEP_DEG);
				const float steps = q / SKY_ANGLE_STEP_DEG;
				multiples = multiples && Near(steps, std::round(steps)) && std::abs(q - degrees) <= 0.5f * SKY_ANGLE_STEP_DEG + k_Eps;
			}
			report.Check(multiples, "Sky: quantized angles are multiples of 0.25 degrees and within half a step of the input");
			report.Check(SKY_ANGLE_STEP_DEG == 0.25f && Near(SkyCurve::Quantize(10.12f, 0.25f), 10.0f) && Near(SkyCurve::Quantize(10.13f, 0.25f), 10.25f),
				"Sky: the angle step is 0.25 degrees and rounds to the nearest step");
		}

		void CheckElevation(CheckReport& report)
		{
			const SkySample dawn = SkyCurve::Evaluate(6.0f, Weather::Clear);
			const SkySample noon = SkyCurve::Evaluate(12.0f, Weather::Clear);
			const SkySample dusk = SkyCurve::Evaluate(20.0f, Weather::Clear);
			report.Check(Near(dawn.SunElevationDeg, 10.0f) && Near(noon.SunElevationDeg, SKY_ELEVATION_MAX_DEG), "Sky: the sun is 10 degrees up at 6:00 and at its 65 degree cap at noon");
			report.Check(dusk.SunElevationDeg < 0.0f && dusk.SunIntensity == 0.0f, "Sky: the sun is below the horizon, and dark, by 20:00");

			bool rising = true;
			bool falling = true;
			bool quantizedRising = true;
			bool quantizedFalling = true;
			bool capped = true;
			float previous = SkyCurve::Evaluate(6.0f, Weather::Clear).SunElevationDeg;
			float previousQ = SkyCurve::Quantize(previous, SKY_ANGLE_STEP_DEG);
			for (float hours = 6.0f + k_HourStep; hours <= 26.0f + k_Eps; hours += k_HourStep)
			{
				const float elevation = SkyCurve::Evaluate(hours, Weather::Clear).SunElevationDeg;
				const float quantized = SkyCurve::Quantize(elevation, SKY_ANGLE_STEP_DEG);
				capped = capped && elevation <= SKY_ELEVATION_MAX_DEG + k_Eps;
				if (hours <= 12.0f + k_Eps)
				{
					rising = rising && elevation >= previous - k_Eps;
					quantizedRising = quantizedRising && quantized >= previousQ;
				}
				else
				{
					falling = falling && elevation <= previous + k_Eps;
					quantizedFalling = quantizedFalling && quantized <= previousQ;
				}
				previous = elevation;
				previousQ = quantized;
			}
			report.Check(rising && falling, "Sky: the sun rises until noon and sets after it");
			report.Check(quantizedRising && quantizedFalling, "Sky: the stepped elevation is monotone on each side of noon");
			report.Check(capped, "Sky: the elevation never exceeds the cap over the whole day");

			std::array<SkyKey, std::size(SKY_KEYS)> tall;
			std::copy(std::begin(SKY_KEYS), std::end(SKY_KEYS), tall.begin());
			tall[2].ElevationDeg = 80.0f;
			report.Check(Near(SkyCurve::Evaluate(tall, 12.0f, Weather::Clear).SunElevationDeg, SKY_ELEVATION_MAX_DEG),
				"Sky: a key above the cap is held to 65 degrees");

			bool azimuthMonotone = true;
			float azimuth = SkyCurve::SunAzimuth(6.0f);
			for (float hours = 6.0f; hours <= 26.0f; hours += k_HourStep)
			{
				const float next = SkyCurve::SunAzimuth(hours);
				azimuthMonotone = azimuthMonotone && next >= azimuth - k_Eps;
				azimuth = next;
			}
			report.Check(azimuthMonotone && Near(SkyCurve::SunAzimuth(6.0f), 0.0f) && Near(SkyCurve::SunAzimuth(20.0f), 180.0f),
				"Sky: the sun's azimuth goes from east at 6:00 to west at 20:00 without turning back");

			const glm::vec3 overhead = SkyCurve::DirectionToBody(90.0f, 0.0f);
			const glm::vec3 east = SkyCurve::DirectionToBody(0.0f, 0.0f);
			const glm::vec3 south = SkyCurve::DirectionToBody(0.0f, 90.0f);
			report.Check(Near(glm::length(overhead), 1.0f) && Near(overhead.y, 1.0f) && Near(east.x, 1.0f) && Near(south.z, 1.0f),
				"Sky: the body direction is a unit vector (east +x, south +z, overhead +y)");
		}

		void CheckBlend(CheckReport& report)
		{
			SkyBlend blend(SKY_ANGLE_STEP_DEG, SKY_BLEND_SECONDS);
			blend.Update(0.5f, 10.0f, 0.0f);
			report.Check(Near(blend.GetElevation(), 10.0f) && blend.GetRetargetCount() == 0, "Sky: the first update snaps to the angle");

			blend.Update(0.5f, 12.0f, 0.0f);
			report.Check(blend.GetRetargetCount() == 1 && Near(blend.GetTargetElevation(), 12.0f) && Near(blend.GetElevation(), 11.0f),
				"Sky: a new stepped angle eases halfway in half a second");
			blend.Update(0.5f, 12.0f, 0.0f);
			report.Check(Near(blend.GetElevation(), 12.0f), "Sky: the stepped angle is reached after the 1 s blend");
			blend.Update(0.1f, 12.1f, 0.0f);
			report.Check(blend.GetRetargetCount() == 1 && Near(blend.GetElevation(), 12.0f), "Sky: a change that rounds to the same step does not retarget");

			SkyBlend day(SKY_ANGLE_STEP_DEG, SKY_BLEND_SECONDS);
			const float dt = 1.0f / static_cast<float>(SIM_TICKS_PER_SECOND);
			const float maxJump = 2.0f * SKY_ANGLE_STEP_DEG / (SKY_BLEND_SECONDS * static_cast<float>(SIM_TICKS_PER_SECOND));
			bool smooth = true;
			float lastElevation = 0.0f;
			float lastAzimuth = 0.0f;
			for (int tick = 0; tick < DAY_TICKS; ++tick)
			{
				const float hours = DAY_START_HOUR + static_cast<float>(tick) / static_cast<float>(TICKS_PER_MINUTE * 60);
				const SkySample sample = SkyCurve::Evaluate(hours, Weather::Clear);
				day.Update(dt, sample.SunElevationDeg, sample.SunAzimuthDeg);
				if (tick > 0)
					smooth = smooth && std::abs(day.GetElevation() - lastElevation) <= maxJump && std::abs(day.GetAzimuth() - lastAzimuth) <= maxJump;
				lastElevation = day.GetElevation();
				lastAzimuth = day.GetAzimuth();
			}
			report.Check(smooth, "Sky: over a whole day the blended angles never move more than a step's worth per sim tick");
			report.Check(day.GetRetargetCount() > 100 && day.GetRetargetCount() < DAY_TICKS / 10,
				std::format("Sky: the sun retargets a few hundred times a day, not every tick ({})", day.GetRetargetCount()));
		}

		void CheckWeatherAndNight(CheckReport& report)
		{
			const SkySample clear = SkyCurve::Evaluate(12.0f, Weather::Clear);
			const SkySample rain = SkyCurve::Evaluate(12.0f, Weather::Rain);
			const SkySample snow = SkyCurve::Evaluate(12.0f, Weather::Snow);
			report.Check(rain.SunIntensity < snow.SunIntensity && snow.SunIntensity < clear.SunIntensity && clear.SunIntensity > 0.0f,
				"Sky: rain dims the sun more than snow, and snow more than a clear sky");
			report.Check(rain.FogEnd < snow.FogEnd && snow.FogEnd < clear.FogEnd, "Sky: rain pulls the fog in more than snow, and snow more than clear");
			report.Check(Near(rain.SunElevationDeg, clear.SunElevationDeg), "Sky: weather does not move the sun");

			report.Check(Near(SkyCurve::MistAmount(MIST_START_HOUR), 1.0f) && Near(SkyCurve::MistAmount(MIST_END_HOUR), 0.0f) && Near(SkyCurve::MistAmount(12.0f), 0.0f),
				"Sky: mist is full at 6:00 and gone by 9:00");
			const SkySample earlyClear = SkyCurve::Evaluate(6.0f, Weather::Clear);
			const SkySample earlyRain = SkyCurve::Evaluate(6.0f, Weather::Rain);
			report.Check(Near(earlyClear.FogEnd, SKY_KEYS[0].FogEnd * glm::mix(1.0f, MIST_FOG_SCALE, MIST_CLEAR_WEIGHT)) && earlyRain.Mist > earlyClear.Mist,
				"Sky: morning mist shortens the fog, and rain brings more of it than a clear sky");

			const SkySample night = SkyCurve::Evaluate(26.0f, Weather::Clear);
			const SkySample noon = SkyCurve::Evaluate(12.0f, Weather::Clear);
			report.Check(Near(night.MoonIntensity, MOON_INTENSITY) && noon.MoonIntensity == 0.0f && night.SunIntensity == 0.0f,
				"Sky: the moon is on at night and off at noon");

			bool inBounds = true;
			for (float hours = 6.0f; hours <= 26.0f; hours += k_HourStep)
			{
				for (const Weather weather : { Weather::Clear, Weather::Rain, Weather::Snow })
				{
					const SkySample sample = SkyCurve::Evaluate(hours, weather);
					inBounds = inBounds && sample.FogStart > 0.0f && sample.FogStart < sample.FogEnd
						&& sample.AmbientIntensity > 0.0f && sample.AmbientIntensity <= 1.0f && sample.SunIntensity >= 0.0f
						&& sample.MoonIntensity >= 0.0f && sample.MoonIntensity <= MOON_INTENSITY + k_Eps;
					for (int channel = 0; channel < 3; ++channel)
						inBounds = inBounds && sample.SkyColor[channel] >= 0.0f && sample.SkyColor[channel] <= 1.0f
							&& sample.AmbientColor[channel] >= 0.0f && sample.AmbientColor[channel] <= 1.0f;
				}
			}
			report.Check(inBounds, "Sky: fog, ambient, sun, moon and colours stay in range for every minute, in every weather");
		}
	}

	void RunSkyChecks(CheckReport& report)
	{
		SuiteScope scope(report, "Sky");
		CheckKeys(report);
		CheckQuantize(report);
		CheckElevation(report);
		CheckBlend(report);
		CheckWeatherAndNight(report);
	}

}
