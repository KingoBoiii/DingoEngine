#include "Environments.h"

#include <algorithm>

namespace
{
	using namespace Dingo;

	constexpr uint32_t k_FaceSize = 512;

	constexpr EnvironmentPreset k_Presets[Environments::k_Count] = {
		{ "Sunny sky", "hdri/kloofendal_48d_partly_cloudy_puresky.hdr", 34.4f, 1.0f, 0.7f, { 1.0f, 1.0f, 1.0f }, 1.0f, { 0.6f, 0.65f, 0.8f }, 0.15f, 0.012f, 1.0f, 1.0f },
		{ "Venice sunset", "hdri/venice_sunset.hdr", 36.0f, 1.0f, 0.5f, { 1.0f, 0.7f, 0.5f }, 0.25f, { 0.8f, 0.55f, 0.45f }, 0.15f, 0.016f, 1.3f, 0.45f },
		{ "Moonless night", "hdri/moonless_golf.hdr", -15.3f, 1.0f, 1.2f, { 0.45f, 0.55f, 1.0f }, 0.07f, { 0.3f, 0.35f, 0.6f }, 0.08f, 0.01f, 2.0f, 1.0f },
		{ "Procedural sky", nullptr, 0.0f, 1.0f, 0.7f, { 1.0f, 1.0f, 1.0f }, 1.0f, { 0.6f, 0.65f, 0.8f }, 0.12f, 0.012f, 1.0f, 1.0f },
	};
}

namespace Dingo
{

	const EnvironmentPreset& Environments::GetPreset(int index)
	{
		return k_Presets[std::clamp(index, 0, k_Count - 1)];
	}

	void Environments::Build(Scene& scene)
	{
		Release();
		for (int i = 0; i < k_Count; ++i)
		{
			if (k_Presets[i].Path)
				m_Environments[i] = Environment::Create(EnvironmentParams().SetPath(k_Presets[i].Path).SetFaceSize(k_FaceSize).SetDebugName(k_Presets[i].Name));
			else
				m_Environments[i] = Environment::CreateProcedural(ProceduralEnvironmentParams().SetFaceSize(256).SetDebugName(k_Presets[i].Name).SetSky(m_ProceduralSettings));
			if (!m_Environments[i])
				DE_ERROR("Dustlight: failed to create environment {}", k_Presets[i].Name);
		}

		m_Sky = scene.CreateEntity("Sky");
		SkyboxComponent& sky = m_Sky.AddComponent<SkyboxComponent>();
		sky.Lighting = true;
		m_PrepareCursor = 0;
	}

	void Environments::Release()
	{
		if (m_Sky.IsValid())
			m_Sky.GetComponent<SkyboxComponent>().Environment = nullptr;
		m_Sky = Entity();
		for (Environment*& environment : m_Environments)
		{
			delete environment;
			environment = nullptr;
		}
	}

	void Environments::PrepareStep()
	{
		if (m_PrepareCursor >= k_Count)
			return;
		Environment* environment = m_Environments[m_PrepareCursor];
		if (!environment || environment->Prepare())
			++m_PrepareCursor;
	}

	bool Environments::AllReady() const
	{
		for (const Environment* environment : m_Environments)
			if (environment && !environment->IsReady())
				return false;
		return true;
	}

	bool Environments::AllLoaded() const
	{
		for (const Environment* environment : m_Environments)
			if (!environment)
				return false;
		return true;
	}

	void Environments::Select(int index)
	{
		m_Index = std::clamp(index, 0, k_Count - 1);
		if (!m_Sky.IsValid())
			return;
		const EnvironmentPreset& preset = k_Presets[m_Index];
		SkyboxComponent& sky = m_Sky.GetComponent<SkyboxComponent>();
		sky.Environment = m_Environments[m_Index];
		sky.Intensity = preset.SkyIntensity;
		sky.LightingIntensity = preset.LightingIntensity;
		sky.FollowSun = true;
		sky.TintSun = true;
		sky.Rotation = 0.0f;
	}

	void Environments::SetSunFade(float fade)
	{
		m_SunFade = std::clamp(fade, 0.0f, 1.0f);
		ApplyProcedural();
	}

	void Environments::ApplyProcedural()
	{
		Environment* environment = GetProcedural();
		if (!environment || !environment->GetSky())
			return;
		ProceduralSky sky = *environment->GetSky();
		sky.Turbidity = m_ProceduralSettings.Turbidity;
		sky.SunDiscRadius = m_ProceduralSettings.SunDiscRadius;
		sky.SunDiscIntensity = m_ProceduralSettings.SunDiscIntensity;
		sky.SunIntensity = m_ProceduralSettings.SunIntensity * m_SunFade;
		environment->SetSky(sky);
	}

	void Environments::SetSunAzimuth(float azimuthDegrees)
	{
		if (m_Sky.IsValid() && !IsProceduralPreset(m_Index))
			m_Sky.GetComponent<SkyboxComponent>().Rotation = k_Presets[m_Index].SkySunAzimuth + azimuthDegrees;
	}

}
