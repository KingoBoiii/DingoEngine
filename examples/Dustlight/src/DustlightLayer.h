#pragma once

#include "Checks.h"
#include "ControlPanel.h"
#include "DustMotes.h"
#include "Environments.h"
#include "FlyCamera.h"
#include "Lantern.h"
#include "Props.h"
#include "RoomBuilder.h"
#include "SunCycle.h"

#include <DingoEngine.h>

namespace Dingo
{

	class DustlightLayer : public Layer
	{
	public:
		DustlightLayer() : Layer("Dustlight") {}
		virtual ~DustlightLayer() = default;

		void OnAttach() override;
		void OnDetach() override;
		void OnUpdate(float deltaTime) override;
		void OnImGuiRender() override;

		Scene* GetScene() const { return m_Scene; }
		FlyCamera& GetFlyCamera() { return m_FlyCamera; }
		Lantern& GetLantern() { return m_Lantern; }

		float GetSunTime() const { return m_SunTime; }
		void SetSunTime(float hours);
		const SunState& GetSun() const { return m_Sun; }

		int GetEnvironmentIndex() const { return m_EnvironmentIndex; }
		void SetEnvironment(int index);
		int GetEnvironmentCount() const { return Environments::k_Count; }
		const char* GetEnvironmentName(int index) const { return Environments::GetPreset(index).Name; }
		bool AreEnvironmentsReady() const { return m_Environments.AllReady(); }
		bool AreEnvironmentsLoaded() const { return m_Environments.AllLoaded(); }
		Entity GetSkyEntity() const { return m_Environments.GetSky(); }
		Entity GetAmbientEntity() const { return m_Ambient; }
		Props& GetProps() { return m_Props; }

		bool IsLanternEnabled() const;
		void SetLanternEnabled(bool enabled);
		bool IsDustEnabled() const;
		void SetDustEnabled(bool enabled);

		bool IsRaymarchEnabled() const;
		void SetRaymarchEnabled(bool enabled);

		Entity GetCameraEntity() const { return m_CameraEntity; }
		Entity GetSunEntity() const { return m_SunEntity; }
		Entity GetRoomVolume() const { return m_RoomVolume; }
		Entity GetSphereVolume() const { return m_SphereVolume; }
		Entity GetRaymarchVolume() const { return m_RaymarchVolume; }
		Entity GetDustEmitter() const { return m_Dust.GetEmitter(); }

	private:
		void BuildCamera();
		void BuildSun();
		void BuildFog();

	private:
		SceneManager m_SceneManager;
		Scene* m_Scene = nullptr;
		RoomBuilder m_Room;
		Lantern m_Lantern;
		DustMotes m_Dust;
		Props m_Props;
		Environments m_Environments;
		FlyCamera m_FlyCamera;
		ControlPanel m_Panel;
		Checks m_Checks;

		Entity m_CameraEntity;
		Entity m_SunEntity;
		Entity m_Ambient;
		Entity m_RoomVolume;
		Entity m_SphereVolume;
		Entity m_RaymarchVolume;

		SunState m_Sun;
		float m_SunTime = 15.0f;
		float m_Clock = 0.0f;
		int m_FrozenSteps = 0;
		int m_EnvironmentIndex = 0;
	};

}
