#include "Look/SkyCurve.h"

#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <cmath>

namespace Dingo
{

	namespace
	{
		float SmoothStep(float edge0, float edge1, float x)
		{
			const float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
			return t * t * (3.0f - 2.0f * t);
		}

		float SunWeatherScale(Weather weather)
		{
			switch (weather)
			{
				case Weather::Rain: return WEATHER_RAIN_SUN_SCALE;
				case Weather::Snow: return WEATHER_SNOW_SUN_SCALE;
				default:            return 1.0f;
			}
		}

		float FogWeatherScale(Weather weather)
		{
			switch (weather)
			{
				case Weather::Rain: return WEATHER_RAIN_FOG_SCALE;
				case Weather::Snow: return WEATHER_SNOW_FOG_SCALE;
				default:            return 1.0f;
			}
		}
	}

	namespace SkyCurve
	{

		float Quantize(float degrees, float step)
		{
			return step > 0.0f ? std::round(degrees / step) * step : degrees;
		}

		float MistAmount(float hours)
		{
			return 1.0f - SmoothStep(MIST_START_HOUR, MIST_END_HOUR, hours);
		}

		float SunAzimuth(float hours)
		{
			const float t = std::clamp((hours - SKY_AZIMUTH_START_HOUR) / (SKY_AZIMUTH_END_HOUR - SKY_AZIMUTH_START_HOUR), 0.0f, 1.0f);
			return glm::mix(SKY_AZIMUTH_START_DEG, SKY_AZIMUTH_END_DEG, t);
		}

		glm::vec3 DirectionToBody(float elevationDeg, float azimuthDeg)
		{
			const float elevation = glm::radians(elevationDeg);
			const float azimuth = glm::radians(azimuthDeg);
			return glm::vec3(std::cos(azimuth) * std::cos(elevation), std::sin(elevation), std::sin(azimuth) * std::cos(elevation));
		}

		float SunIntensityScale(float sunElevationDeg)
		{
			return SmoothStep(-1.0f, SKY_SUN_FADE_DEG, sunElevationDeg);
		}

		float MoonIntensityFor(float sunElevationDeg)
		{
			return MOON_INTENSITY * SmoothStep(0.0f, MOON_FADE_DEG, -sunElevationDeg);
		}

		SkySample Evaluate(std::span<const SkyKey> keys, float hours, Weather weather)
		{
			SkySample out;
			if (keys.empty())
				return out;

			const float clamped = std::clamp(hours, keys.front().Hour, keys.back().Hour);
			size_t upper = 1;
			while (upper < keys.size() - 1 && keys[upper].Hour < clamped)
				++upper;
			const SkyKey& a = keys[keys.size() > 1 ? upper - 1 : 0];
			const SkyKey& b = keys[keys.size() > 1 ? upper : 0];
			const float span = b.Hour - a.Hour;
			const float t = span > 0.0f ? std::clamp((clamped - a.Hour) / span, 0.0f, 1.0f) : 0.0f;

			out.SunElevationDeg = (std::min)(glm::mix(a.ElevationDeg, b.ElevationDeg, t), SKY_ELEVATION_MAX_DEG);
			out.SunAzimuthDeg = SunAzimuth(hours);
			out.SunColor = glm::mix(a.SunColor, b.SunColor, t);
			out.SunIntensity = glm::mix(a.SunIntensity, b.SunIntensity, t) * SunWeatherScale(weather) * SunIntensityScale(out.SunElevationDeg);
			out.MoonIntensity = MoonIntensityFor(out.SunElevationDeg);
			out.AmbientColor = glm::mix(a.AmbientColor, b.AmbientColor, t);
			out.AmbientIntensity = glm::mix(a.AmbientIntensity, b.AmbientIntensity, t);
			out.SkyColor = glm::mix(a.SkyColor, b.SkyColor, t);

			out.Mist = MistAmount(hours) * (weather == Weather::Rain ? 1.0f : MIST_CLEAR_WEIGHT);
			const float fog = FogWeatherScale(weather) * glm::mix(1.0f, MIST_FOG_SCALE, out.Mist);
			out.FogStart = glm::mix(a.FogStart, b.FogStart, t) * fog;
			out.FogEnd = glm::mix(a.FogEnd, b.FogEnd, t) * fog;
			return out;
		}

		SkySample Evaluate(float hours, Weather weather)
		{
			return Evaluate(SKY_KEYS, hours, weather);
		}

	}

	SkyBlend::SkyBlend(float stepDeg, float blendSeconds)
		: m_Step(stepDeg), m_Seconds(blendSeconds)
	{}

	void SkyBlend::Snap(float elevationDeg, float azimuthDeg)
	{
		m_Primed = true;
		m_Elevation = m_FromElevation = m_ToElevation = SkyCurve::Quantize(elevationDeg, m_Step);
		m_Azimuth = m_FromAzimuth = m_ToAzimuth = SkyCurve::Quantize(azimuthDeg, m_Step);
		m_T = 1.0f;
	}

	void SkyBlend::Update(float dt, float rawElevationDeg, float rawAzimuthDeg)
	{
		if (!m_Primed)
		{
			Snap(rawElevationDeg, rawAzimuthDeg);
			return;
		}

		const float elevation = SkyCurve::Quantize(rawElevationDeg, m_Step);
		const float azimuth = SkyCurve::Quantize(rawAzimuthDeg, m_Step);
		if (elevation != m_ToElevation || azimuth != m_ToAzimuth)
		{
			m_FromElevation = m_Elevation;
			m_FromAzimuth = m_Azimuth;
			m_ToElevation = elevation;
			m_ToAzimuth = azimuth;
			m_T = 0.0f;
			++m_Retargets;
		}

		m_T = (std::min)(1.0f, m_T + (m_Seconds > 0.0f ? dt / m_Seconds : 1.0f));
		m_Elevation = glm::mix(m_FromElevation, m_ToElevation, m_T);
		m_Azimuth = glm::mix(m_FromAzimuth, m_ToAzimuth, m_T);
	}

}
