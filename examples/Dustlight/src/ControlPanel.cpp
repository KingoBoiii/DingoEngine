#include "ControlPanel.h"

#include "DustlightLayer.h"

#include <imgui.h>

#include <cstring>
#include <string>

namespace
{
	using namespace Dingo;

	struct GridPreset
	{
		const char* Label;
		glm::uvec3 Size;
	};

	constexpr GridPreset k_Grids[] = {
		{ "Low 96x54x32", { 96, 54, 32 } },
		{ "Medium 160x90x64", { 160, 90, 64 } },
		{ "High 192x108x96", { 192, 108, 96 } },
	};

	void TimerRow(const char* name)
	{
		for (const GpuTimerStats& timer : Renderer::GetGpuTimers())
		{
			if (timer.Name && std::strcmp(timer.Name, name) == 0)
			{
				ImGui::Text("%-14s %.3f ms", name, timer.MeanMs);
				return;
			}
		}
		ImGui::Text("%-14s n/a", name);
	}

	FogVolume& VolumeOf(Entity entity)
	{
		return entity.GetComponent<FogVolumeComponent>().Settings;
	}
}

namespace Dingo
{

	void ControlPanel::Update()
	{
		if (Input::IsKeyPressed(Key::F1))
			m_Visible = !m_Visible;
	}

	void ControlPanel::Draw(DustlightLayer& layer)
	{
		if (!m_Visible)
			return;

		Renderer3D& renderer = Application::Get().GetRenderer3D();
		ImGui::SetNextWindowPos({ 12.0f, 12.0f }, ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize({ 360.0f, 0.0f }, ImGuiCond_FirstUseEver);
		if (!ImGui::Begin("Dustlight (F1 hides)"))
		{
			ImGui::End();
			return;
		}

		float time = layer.GetSunTime();
		if (ImGui::SliderFloat("Time of day", &time, 0.0f, 24.0f, "%.1f h"))
			layer.SetSunTime(time);

		int environment = layer.GetEnvironmentIndex();
		if (ImGui::BeginCombo("Environment", layer.GetEnvironmentName(environment)))
		{
			for (int i = 0; i < layer.GetEnvironmentCount(); ++i)
			{
				if (ImGui::Selectable(layer.GetEnvironmentName(i), i == environment))
					layer.SetEnvironment(i);
			}
			ImGui::EndCombo();
		}

		if (Environments::IsProceduralPreset(environment))
		{
			ProceduralSky& sky = layer.GetEnvironments().GetProceduralSettings();
			bool changed = ImGui::SliderFloat("Turbidity", &sky.Turbidity, 0.0f, 10.0f);
			changed |= ImGui::SliderFloat("Sun disc intensity", &sky.SunDiscIntensity, 0.0f, 100.0f);
			changed |= ImGui::SliderFloat("Sun disc radius", &sky.SunDiscRadius, 0.0f, 3.0f, "%.2f deg");
			if (changed)
				layer.GetEnvironments().ApplyProcedural();
		}

		Entity camera = layer.GetCameraEntity();
		if (camera.IsValid() && camera.HasComponent<PostProcessComponent>())
		{
			BloomSettings& bloom = camera.GetComponent<PostProcessComponent>().Settings.Bloom;
			ImGui::SliderFloat("Bloom intensity", &bloom.Intensity, 0.0f, 1.0f);
			ImGui::SliderFloat("Bloom threshold", &bloom.Threshold, 0.5f, 4.0f);
		}

		ImGui::SeparatorText("Volumetrics");
		bool volumetrics = renderer.IsVolumetricsEnabled();
		if (ImGui::Checkbox("Volumetric fog", &volumetrics))
			renderer.SetVolumetricsEnabled(volumetrics);

		int gridPreset = -1;
		for (int i = 0; i < 3; ++i)
			if (k_Grids[i].Size == renderer.GetVolumetricGrid())
				gridPreset = i;
		if (ImGui::BeginCombo("Grid size", gridPreset >= 0 ? k_Grids[gridPreset].Label : "Custom"))
		{
			for (int i = 0; i < 3; ++i)
			{
				if (ImGui::Selectable(k_Grids[i].Label, i == gridPreset))
					renderer.SetVolumetricGrid(k_Grids[i].Size);
			}
			ImGui::EndCombo();
		}

		FogComponent& fog = layer.GetCameraEntity().GetComponent<FogComponent>();
		if (fog.Volumetric)
			ImGui::Checkbox("Temporal", &fog.Volumetric->Temporal);

		int view = static_cast<int>(renderer.GetVolumetricDebugView());
		const char* views[] = { "None", "Shadow samples" };
		if (ImGui::Combo("Debug view", &view, views, 2))
			renderer.SetVolumetricDebugView(static_cast<VolumetricDebugView>(view));

		bool raymarch = layer.IsRaymarchEnabled();
		if (ImGui::Checkbox("Raymarch volume", &raymarch))
			layer.SetRaymarchEnabled(raymarch);
		ImGui::SameLine();
		const char* resolutions[] = { "Half", "Quarter" };
		ImGui::SetNextItemWidth(90.0f);
		m_RaymarchResolution = renderer.GetVolumeRaymarchResolution() == RaymarchResolution::Half ? 0 : 1;
		if (ImGui::Combo("##raymarchres", &m_RaymarchResolution, resolutions, 2))
			renderer.SetVolumeRaymarchResolution(m_RaymarchResolution == 0 ? RaymarchResolution::Half : RaymarchResolution::Quarter);

		ImGui::SeparatorText("Scene");
		m_RoomEnabled = layer.GetRoomVolume().GetComponent<FogVolumeComponent>().Enabled;
		if (ImGui::Checkbox("Room volume", &m_RoomEnabled))
			layer.GetRoomVolume().GetComponent<FogVolumeComponent>().Enabled = m_RoomEnabled;
		m_RoomDensity = VolumeOf(layer.GetRoomVolume()).Density;
		if (ImGui::SliderFloat("Room density", &m_RoomDensity, 0.0f, 0.4f))
			VolumeOf(layer.GetRoomVolume()).Density = m_RoomDensity;

		m_SphereEnabled = layer.GetSphereVolume().GetComponent<FogVolumeComponent>().Enabled;
		if (ImGui::Checkbox("Sphere volume", &m_SphereEnabled))
			layer.GetSphereVolume().GetComponent<FogVolumeComponent>().Enabled = m_SphereEnabled;
		m_SphereDensity = VolumeOf(layer.GetSphereVolume()).Density;
		if (ImGui::SliderFloat("Sphere density", &m_SphereDensity, 0.0f, 1.5f))
			VolumeOf(layer.GetSphereVolume()).Density = m_SphereDensity;

		bool lantern = layer.IsLanternEnabled();
		if (ImGui::Checkbox("Lantern", &lantern))
			layer.SetLanternEnabled(lantern);
		ImGui::SameLine();
		bool dust = layer.IsDustEnabled();
		if (ImGui::Checkbox("Dust motes", &dust))
			layer.SetDustEnabled(dust);

		ImGui::Text("Camera preset:");
		for (int i = 1; i <= 3; ++i)
		{
			ImGui::SameLine();
			const std::string label = std::to_string(i);
			if (ImGui::Button(label.c_str()))
				layer.GetFlyCamera().JumpToPreset(i);
		}

		ImGui::SeparatorText("GPU");
		TimerRow("Volumetrics");
		TimerRow("VolumeRaymarch");
		TimerRow("Environment");

		const Renderer3D::Statistics& stats = renderer.GetStatistics();
		ImGui::TextWrapped("Fog volumes %u (raymarch %u), PBR draws %u, env bakes %u", stats.FogVolumes, stats.RaymarchVolumes, stats.PbrDraws, stats.EnvironmentBakes);
		ImGui::End();
	}

}
