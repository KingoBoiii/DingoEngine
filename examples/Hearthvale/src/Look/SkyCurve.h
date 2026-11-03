#pragma once
#include "GameTuning.h"
#include "Sim/SimTypes.h"

#include <glm/glm.hpp>

#include <span>

namespace Dingo
{

	struct SkySample
	{
		float SunElevationDeg = 0.0f;
		float SunAzimuthDeg = 0.0f;
		glm::vec3 SunColor{ 1.0f };
		float SunIntensity = 0.0f;
		float MoonIntensity = 0.0f;
		glm::vec3 AmbientColor{ 1.0f };
		float AmbientIntensity = 0.0f;
		glm::vec3 SkyColor{ 0.5f };
		float FogStart = 0.0f;
		float FogEnd = 0.0f;
		float Mist = 0.0f;
	};

	// Plain functions of the time of day: no scene, no GPU, so --check runs them headless.
	namespace SkyCurve
	{
		float Quantize(float degrees, float step);

		float MistAmount(float hours);

		// Everything but the angles is smooth in `hours`; the angles are the raw, unquantized curve, capped at SKY_ELEVATION_MAX_DEG.
		SkySample Evaluate(std::span<const SkyKey> keys, float hours, Weather weather);
		SkySample Evaluate(float hours, Weather weather);

		float SunAzimuth(float hours);

		// Unit vector from the scene toward a body at this elevation and azimuth; a light travels the other way.
		glm::vec3 DirectionToBody(float elevationDeg, float azimuthDeg);

		// The moon sits opposite the sun, so a body below the horizon on one side is above it on the other.
		float MoonIntensityFor(float sunElevationDeg);
		float SunIntensityScale(float sunElevationDeg);
	}

	// Follows the quantized sun angles. The curve is stepped in SKY_ANGLE_STEP_DEG steps so the shadow cascades
	// are not re-fitted every frame; each step is then eased in over `blendSeconds` so the shadows do not jump.
	class SkyBlend
	{
	public:
		SkyBlend(float stepDeg, float blendSeconds);

		void Snap(float elevationDeg, float azimuthDeg);
		void Update(float dt, float rawElevationDeg, float rawAzimuthDeg);

		float GetElevation() const { return m_Elevation; }
		float GetAzimuth() const { return m_Azimuth; }
		float GetTargetElevation() const { return m_ToElevation; }
		float GetTargetAzimuth() const { return m_ToAzimuth; }
		int GetRetargetCount() const { return m_Retargets; }

	private:
		float m_Step;
		float m_Seconds;
		bool m_Primed = false;
		float m_Elevation = 0.0f;
		float m_Azimuth = 0.0f;
		float m_FromElevation = 0.0f;
		float m_FromAzimuth = 0.0f;
		float m_ToElevation = 0.0f;
		float m_ToAzimuth = 0.0f;
		float m_T = 1.0f;
		int m_Retargets = 0;
	};

}
