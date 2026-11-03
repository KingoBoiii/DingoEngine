#include "Look/Sky.h"

namespace Dingo
{

	Sky::Sky(Scene& scene, bool shadows, bool fog)
		: m_Scene(scene), m_Shadows(shadows)
	{
		m_Ambient = scene.CreateEntity("Ambient");
		m_Ambient.AddComponent<Transform3DComponent>();
		m_Ambient.AddComponent<AmbientLightComponent>();

		m_Sun = scene.CreateEntity("Sun");
		DirectionalLightComponent& sun = m_Sun.AddComponent<DirectionalLightComponent>();
		sun.ShadowStrength = SUN_SHADOW_STRENGTH;

		m_Moon = scene.CreateEntity("Moon");
		DirectionalLightComponent& moon = m_Moon.AddComponent<DirectionalLightComponent>();
		moon.Color = MOON_COLOR;
		moon.Intensity = 0.0f;
		moon.CastShadows = false;

		if (fog)
		{
			Fog settings;
			settings.Mode = FogMode::Linear;
			m_Fog = scene.CreateEntity("Fog");
			m_Fog.AddComponent<FogComponent>(FogComponent(settings, true));
		}
	}

	void Sky::Update(float dt, float hours, Weather weather)
	{
		m_Sample = SkyCurve::Evaluate(hours, weather);
		m_Blend.Update(dt, m_Sample.SunElevationDeg, m_Sample.SunAzimuthDeg);

		const float elevation = m_Blend.GetElevation();
		const glm::vec3 towardSun = SkyCurve::DirectionToBody(elevation, m_Blend.GetAzimuth());

		DirectionalLightComponent& sun = m_Sun.GetComponent<DirectionalLightComponent>();
		sun.Direction = -towardSun;
		sun.Color = m_Sample.SunColor;
		sun.Intensity = m_Sample.SunIntensity;
		sun.CastShadows = m_Shadows && elevation >= SKY_SHADOW_MIN_ELEVATION_DEG;

		DirectionalLightComponent& moon = m_Moon.GetComponent<DirectionalLightComponent>();
		moon.Direction = towardSun;
		moon.Intensity = SkyCurve::MoonIntensityFor(elevation);

		AmbientLightComponent& ambient = m_Ambient.GetComponent<AmbientLightComponent>();
		ambient.Color = m_Sample.AmbientColor;
		ambient.Intensity = m_Sample.AmbientIntensity;

		m_Scene.SetClearColor(glm::vec4(m_Sample.SkyColor, 1.0f));

		if (m_Fog)
		{
			Fog& fog = m_Fog.GetComponent<FogComponent>().Settings;
			fog.Start = m_Sample.FogStart;
			fog.End = m_Sample.FogEnd;
		}
	}

}
