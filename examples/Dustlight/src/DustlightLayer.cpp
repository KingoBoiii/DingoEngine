#include "DustlightLayer.h"

#include "LaunchOptions.h"

#include <algorithm>

namespace
{
	using namespace Dingo;

	constexpr const char* k_SceneName = "Dustlight";
	constexpr float k_ShaftIntensity = 4.0f;
	constexpr int k_FreezeHoldSteps = 90;
	constexpr float k_FreezeStep = 1.0f / 60.0f;

	PostProcessSettings MakePostSettings()
	{
		PostProcessSettings settings;
		settings.Enabled = true;
		settings.Tone.Operator = ToneMapOperator::Soft;
		settings.Bloom.Enabled = true;
		settings.Bloom.Intensity = 0.45f;
		settings.Bloom.Threshold = 0.8f;
		return settings;
	}
}

namespace Dingo
{

	void DustlightLayer::OnAttach()
	{
		const LaunchOptions& options = GetLaunchOptions();

		m_Scene = m_SceneManager.CreateScene(k_SceneName);
		m_Scene->SetClearColor({ 0.02f, 0.02f, 0.03f, 1.0f });

		m_Room.Build(*m_Scene);
		BuildCamera();
		BuildSun();
		BuildFog();
		m_Lantern.Build(*m_Scene);
		m_Dust.Build(*m_Scene);
		m_Props.Build(*m_Scene);
		m_Environments.Build(*m_Scene);

		m_Ambient = m_Scene->CreateEntity("Ambient");
		m_Ambient.AddComponent<AmbientLightComponent>(AmbientLightComponent(glm::vec3(0.6f, 0.65f, 0.8f), 0.15f));

		m_Panel.SetVisible(!options.Freeze);
		m_FlyCamera.JumpToPreset(options.View);
		SetEnvironment(options.Environment);
		SetSunTime(options.TimeHours);
		SetRaymarchEnabled(!options.NoRaymarch);
		m_SceneManager.SetActiveScene(k_SceneName);
	}

	void DustlightLayer::OnDetach()
	{
		m_Environments.Release();
		if (Scene* active = m_SceneManager.GetActiveScene())
			active->OnStop();
		m_SceneManager.Clear();
		m_Scene = nullptr;
		m_Room.Release();
		m_Dust.Release();
		m_Props.Release();
		m_Lantern.Release();
		Input::SetCursorMode(CursorMode::Normal);
	}

	void DustlightLayer::OnUpdate(float deltaTime)
	{
		const LaunchOptions& options = GetLaunchOptions();
		m_Panel.Update();
		m_FlyCamera.Update(deltaTime);
		m_Environments.PrepareStep();
		if (options.Check)
			m_Checks.Tick(*this);

		const bool held = options.Freeze && m_FrozenSteps >= k_FreezeHoldSteps;
		if (!held)
		{
			const float step = options.Freeze ? k_FreezeStep : deltaTime;
			m_Clock += step;
			m_Lantern.Update(m_Clock);
			m_SceneManager.OnUpdate(step);
			if (options.Freeze)
				++m_FrozenSteps;
		}
		m_SceneManager.OnRender();
	}

	void DustlightLayer::OnImGuiRender()
	{
		m_Panel.Draw(*this);
	}

	bool DustlightLayer::IsLanternEnabled() const
	{
		return m_Lantern.IsEnabled();
	}

	void DustlightLayer::SetLanternEnabled(bool enabled)
	{
		m_Lantern.SetEnabled(enabled);
	}

	bool DustlightLayer::IsDustEnabled() const
	{
		Entity emitter = m_Dust.GetEmitter();
		return emitter.IsValid() && emitter.GetComponent<ParticleEmitterComponent>().Playing;
	}

	void DustlightLayer::SetDustEnabled(bool enabled)
	{
		Entity emitter = m_Dust.GetEmitter();
		if (emitter.IsValid())
			emitter.GetComponent<ParticleEmitterComponent>().Playing = enabled;
	}

	void DustlightLayer::SetSunTime(float hours)
	{
		m_SunTime = std::clamp(hours, 0.0f, 24.0f);
		m_Sun = SunCycle::Evaluate(m_SunTime);
		if (!m_SunEntity.IsValid())
			return;

		DirectionalLightComponent& light = m_SunEntity.GetComponent<DirectionalLightComponent>();
		light.Direction = m_Sun.Direction;
		const EnvironmentPreset& preset = Environments::GetPreset(m_EnvironmentIndex);
		light.Color = m_Sun.Color * preset.SunTint;
		light.Intensity = m_Sun.Intensity * preset.SunScale;
		light.VolumetricIntensity = k_ShaftIntensity * preset.ShaftScale;
		m_Environments.SetSunAzimuth(m_Sun.AzimuthDegrees);
	}

	void DustlightLayer::SetEnvironment(int index)
	{
		m_EnvironmentIndex = std::clamp(index, 0, Environments::k_Count - 1);
		const EnvironmentPreset& preset = Environments::GetPreset(m_EnvironmentIndex);
		m_Environments.Select(m_EnvironmentIndex);

		if (m_Ambient.IsValid())
		{
			AmbientLightComponent& ambient = m_Ambient.GetComponent<AmbientLightComponent>();
			ambient.Color = preset.AmbientColor;
			ambient.Intensity = preset.AmbientIntensity;
		}
		if (m_CameraEntity.IsValid())
		{
			FogComponent& fog = m_CameraEntity.GetComponent<FogComponent>();
			if (fog.Volumetric)
				fog.Volumetric->Density = preset.FogDensity;
			m_CameraEntity.GetComponent<CameraComponent>().ResetVolumetricHistory = true;
		}
		m_Lantern.SetIntensityScale(preset.LanternScale);
		SetSunTime(m_SunTime);
	}

	bool DustlightLayer::IsRaymarchEnabled() const
	{
		Entity volume = m_RaymarchVolume;
		return volume.IsValid() && volume.GetComponent<FogVolumeComponent>().Enabled;
	}

	void DustlightLayer::SetRaymarchEnabled(bool enabled)
	{
		if (m_RaymarchVolume.IsValid())
			m_RaymarchVolume.GetComponent<FogVolumeComponent>().Enabled = enabled;
	}

	void DustlightLayer::BuildCamera()
	{
		m_CameraEntity = m_Scene->CreateEntity("Camera");
		m_CameraEntity.AddComponent<Transform3DComponent>();
		CameraComponent& camera = m_CameraEntity.AddComponent<CameraComponent>();
		camera.Type = CameraComponent::ProjectionType::Perspective;
		camera.FOV = 60.0f;
		camera.PerspNear = 0.1f;
		camera.PerspFar = 80.0f;
		camera.Primary = true;
		m_CameraEntity.AddComponent<PostProcessComponent>(PostProcessComponent(MakePostSettings()));

		VolumetricFog fog;
		fog.Density = 0.012f;
		fog.BaseHeight = 0.0f;
		fog.HeightFalloff = 0.15f;
		fog.Anisotropy = 0.6f;
		fog.MaxDistance = 40.0f;
		FogComponent& fogComponent = m_CameraEntity.AddComponent<FogComponent>();
		fogComponent.Volumetric = fog;

		m_FlyCamera.Attach(m_CameraEntity);
	}

	void DustlightLayer::BuildSun()
	{
		m_SunEntity = m_Scene->CreateEntity("Sun");
		DirectionalLightComponent& sun = m_SunEntity.AddComponent<DirectionalLightComponent>();
		sun.CastShadows = true;
		sun.ShadowStrength = 1.0f;
		sun.VolumetricIntensity = k_ShaftIntensity;
	}

	void DustlightLayer::BuildFog()
	{
		{
			FogVolume room;
			room.Shape = FogVolumeShape::Box;
			room.Mode = FogVolumeMode::Froxel;
			room.HalfExtents = { RoomLayout::k_HalfWidth, RoomLayout::k_Height * 0.5f, RoomLayout::k_HalfLength };
			room.Density = 0.1f;
			room.Anisotropy = 0.6f;
			room.ScatterTint = { 1.0f, 0.93f, 0.8f };
			room.NoiseScale = 3.0f;
			room.NoiseVelocity = { 0.08f, 0.01f, 0.05f };
			room.NoiseMin = 0.4f;
			room.NoiseMax = 1.6f;
			room.EdgeFade = 0.4f;

			m_RoomVolume = m_Scene->CreateEntity("RoomFog");
			m_RoomVolume.AddComponent<Transform3DComponent>().Position = { 0.0f, RoomLayout::k_Height * 0.5f, 0.0f };
			m_RoomVolume.AddComponent<FogVolumeComponent>(FogVolumeComponent(room));
		}
		{
			FogVolume sphere;
			sphere.Shape = FogVolumeShape::Sphere;
			sphere.Mode = FogVolumeMode::Froxel;
			sphere.HalfExtents = glm::vec3(1.3f);
			sphere.Density = 0.08f;
			sphere.Anisotropy = 0.4f;
			sphere.ScatterTint = { 1.0f, 0.82f, 0.6f };
			sphere.EdgeFade = 0.8f;

			m_SphereVolume = m_Scene->CreateEntity("LanternFog");
			m_SphereVolume.AddComponent<Transform3DComponent>().Position = { Lantern::k_Position.x, 1.2f, Lantern::k_Position.z };
			m_SphereVolume.AddComponent<FogVolumeComponent>(FogVolumeComponent(sphere));
		}
		{
			FogVolume alcove;
			alcove.Shape = FogVolumeShape::Box;
			alcove.Mode = FogVolumeMode::Raymarch;
			alcove.HalfExtents = RoomLayout::k_AlcoveHalfExtents;
			alcove.Density = 0.25f;
			alcove.Anisotropy = 0.6f;
			alcove.RaymarchSteps = 32;
			alcove.EdgeFade = 0.1f;

			m_RaymarchVolume = m_Scene->CreateEntity("AlcoveFog");
			m_RaymarchVolume.AddComponent<Transform3DComponent>().Position = RoomLayout::k_AlcoveCentre;
			m_RaymarchVolume.AddComponent<FogVolumeComponent>(FogVolumeComponent(alcove));
		}
	}

}
