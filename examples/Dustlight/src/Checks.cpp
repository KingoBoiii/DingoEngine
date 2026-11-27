#include "Checks.h"

#include "DustlightLayer.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <cstring>

namespace
{
	using namespace Dingo;

	constexpr int k_SettleFrames = 8;
	constexpr int k_ReadyTimeoutFrames = 1800;
	constexpr glm::uvec3 k_Grids[] = { { 96, 54, 32 }, { 160, 90, 64 }, { 192, 108, 96 } };

	const Renderer3D::Statistics& Stats()
	{
		return Application::Get().GetRenderer3D().GetStatistics();
	}

	Renderer3D& Gfx()
	{
		return Application::Get().GetRenderer3D();
	}

	bool Near(float a, float b, float tolerance = 1e-3f)
	{
		return std::abs(a - b) <= tolerance;
	}

	std::string Label(float hours)
	{
		return "time " + std::to_string(static_cast<int>(hours * 10.0f) / 10) + "." + std::to_string(static_cast<int>(hours * 10.0f) % 10);
	}
}

namespace Dingo
{

	void Checks::Check(bool passed, const std::string& name)
	{
		if (passed)
		{
			++m_Passed;
			DE_INFO("[PASS] {}", name);
		}
		else
		{
			++m_Failed;
			DE_ERROR("[FAIL] {}", name);
		}
	}

	void Checks::LogCost() const
	{
		const auto mean = [](const char* name) -> std::string
		{
			for (const GpuTimerStats& timer : Renderer::GetGpuTimers())
			{
				if (timer.Name && std::strcmp(timer.Name, name) == 0)
					return std::to_string(timer.MeanMs) + " ms";
			}
			return "n/a";
		};
		DE_INFO("Dustlight cost: Volumetrics {}, VolumeRaymarch {}, Environment {}", mean("Volumetrics"), mean("VolumeRaymarch"), mean("Environment"));
	}

	void Checks::Finish()
	{
		LogCost();
		DE_INFO("DUSTLIGHT CHECK: {} passed, {} failed", m_Passed, m_Failed);
		m_Done = true;
		Application::Get().Close();
	}

	void Checks::CheckSunCurve()
	{
		const SunState dawn = SunCycle::Evaluate(SunCycle::k_DawnHours);
		const SunState noon = SunCycle::Evaluate(12.5f);
		const SunState dusk = SunCycle::Evaluate(SunCycle::k_DuskHours);
		Check(Near(dawn.ElevationDegrees, SunCycle::k_MinElevation), "sun curve: dawn at the minimum elevation");
		Check(Near(noon.ElevationDegrees, SunCycle::k_MaxElevation), "sun curve: noon at the maximum elevation");
		Check(Near(dawn.AzimuthDegrees, -SunCycle::k_AzimuthSpan) && Near(dusk.AzimuthDegrees, SunCycle::k_AzimuthSpan), "sun curve: azimuth sweeps the span");
		Check(dawn.Color.b < noon.Color.b && dawn.Intensity < noon.Intensity, "sun curve: a low sun is warmer and dimmer");
		Check(SunCycle::Evaluate(0.0f).Intensity == 0.0f && SunCycle::Evaluate(24.0f).Intensity == 0.0f && dusk.Intensity == 0.0f, "sun curve: the sun is off at dusk and at night");
	}

	void Checks::Build(DustlightLayer& layer)
	{
		const auto add = [this](std::function<void()> enter, std::function<void()> verify)
		{
			m_Stages.push_back({ std::move(enter), std::move(verify) });
		};

		add([&layer]
		{
			layer.GetFlyCamera().JumpToPreset(1);
			layer.SetEnvironment(0);
			layer.SetSunTime(15.0f);
			layer.SetRaymarchEnabled(false);
		}, [&layer, this]
		{
			const Renderer3D::Statistics& stats = Stats();
			Check(Gfx().IsVolumetricsEnabled(), "volumetrics are enabled at start");
			Check(stats.Volumetrics && stats.VolumetricDispatches > 0, "default scene runs the froxel volume");
			Check(stats.FogVolumes >= 2, "room and lantern volumes are kept");
			Check(stats.PbrDraws > 0, "PBR draws are counted");
			Check(stats.Skybox, "the sky is drawn");
			Check(stats.EnvironmentLighting, "PBR is lit by the environment");
			Check(stats.RaymarchVolumes == 0 && stats.RaymarchDispatches == 0, "raymarch volume off draws nothing");
			Check(layer.GetProps().GetGridSphereCount() == 24, "the sphere grid has 24 spheres");
		});

		for (const float hours : { 8.0f, 12.5f, 17.0f })
		{
			add([&layer, hours] { layer.SetSunTime(hours); }, [&layer, hours, this]
			{
				const SunState expected = SunCycle::Evaluate(hours);
				const DirectionalLightComponent& light = layer.GetSunEntity().GetComponent<DirectionalLightComponent>();
				const std::string label = Label(hours);
				Check(Near(light.Direction.x, expected.Direction.x) && Near(light.Direction.y, expected.Direction.y) && Near(light.Direction.z, expected.Direction.z), label + ": the sun light follows the curve");
				Check(Stats().Volumetrics, label + ": volumetrics still draw");
			});
		}

		for (int i = 0; i < layer.GetEnvironmentCount(); ++i)
		{
			add([&layer, i] { layer.SetEnvironment(i); }, [&layer, i, this]
			{
				const std::string label = std::string("environment ") + layer.GetEnvironmentName(i);
				const Renderer3D::Statistics& stats = Stats();
				const FogComponent& fog = layer.GetCameraEntity().GetComponent<FogComponent>();
				Check(layer.GetEnvironmentIndex() == i, label + ": selected");
				Check(stats.Skybox && stats.EnvironmentLighting, label + ": sky and IBL");
				Check(fog.Volumetric && Near(fog.Volumetric->Density, Environments::GetPreset(i).FogDensity, 1e-6f), label + ": fog density follows the preset");
				Check(stats.Volumetrics && stats.FogVolumes >= 2, label + ": volumes still draw");
			});
		}

		struct SkyProbe
		{
			glm::vec3 Direction{ 0.0f };
			glm::vec3 Transmittance{ 0.0f };
		};
		auto probe = std::make_shared<SkyProbe>();
		add([&layer]
		{
			layer.SetEnvironment(Environments::k_ProceduralIndex);
			layer.SetSunTime(12.5f);
		}, [&layer, probe, this]
		{
			const Environment* sky = layer.GetEnvironments().GetProcedural();
			Check(sky && sky->IsProcedural() && sky->IsReady(), "procedural sky: the environment is procedural and baked");
			Check(sky && sky->GetSky() && Stats().Skybox && Stats().EnvironmentLighting, "procedural sky: drawn and lighting PBR");
			if (!sky || !sky->GetSky())
				return;
			probe->Direction = sky->GetSky()->SunDirection;
			probe->Transmittance = sky->GetSunTransmittance(SunCycle::Evaluate(12.5f).Direction);
			const glm::vec3 light = layer.GetSunEntity().GetComponent<DirectionalLightComponent>().Direction;
			Check(glm::dot(glm::normalize(probe->Direction), glm::normalize(light)) > std::cos(glm::radians(0.3f)), "procedural sky: the baked sun follows the light at 12.5 h");
		});
		add([&layer] { layer.SetSunTime(17.0f); }, [&layer, probe, this]
		{
			const Environment* sky = layer.GetEnvironments().GetProcedural();
			if (!sky || !sky->GetSky())
			{
				Check(false, "procedural sky: the environment exists at 17 h");
				return;
			}
			const glm::vec3 now = sky->GetSky()->SunDirection;
			const float moved = glm::degrees(std::acos(std::clamp(glm::dot(glm::normalize(now), glm::normalize(probe->Direction)), -1.0f, 1.0f)));
			Check(moved > 0.25f && m_StageBakes > 0 && sky->IsReady(), "procedural sky: the sky re-bakes when the time of day moves the sun (" + std::to_string(moved) + " deg, " + std::to_string(m_StageBakes) + " bakes)");
			const glm::vec3 evening = sky->GetSunTransmittance(SunCycle::Evaluate(17.0f).Direction);
			Check(evening.g < probe->Transmittance.g && evening.b < probe->Transmittance.b, "procedural sky: the sun's transmittance is lower at 17 h than at 12.5 h");
		});

		add([&layer]
		{
			layer.SetEnvironment(0);
			layer.SetSunTime(15.0f);
			Gfx().SetVolumetricsEnabled(false);
		}, [this]
		{
			Check(!Stats().Volumetrics && Stats().VolumetricDispatches == 0, "volumetrics off: no froxel dispatches");
		});
		add([] { Gfx().SetVolumetricsEnabled(true); }, [this]
		{
			Check(Stats().Volumetrics && Stats().VolumetricDispatches > 0, "volumetrics on again: froxel dispatches resume");
		});

		add([&layer]
		{
			layer.GetFlyCamera().JumpToPreset(3);
			layer.SetRaymarchEnabled(true);
		}, [&layer, this]
		{
			const Renderer3D::Statistics& stats = Stats();
			Check(layer.IsRaymarchEnabled(), "raymarch volume enabled");
			Check(stats.RaymarchVolumes == 1, "one raymarch volume is kept");
			Check(stats.RaymarchDispatches > 0, "the raymarch pass dispatches");
			Check(stats.RaymarchFallbacks == 0, "raymarch does not fall back to froxel");
		});
		add([] { Gfx().SetVolumeRaymarchResolution(RaymarchResolution::Quarter); }, [this]
		{
			Check(Gfx().GetVolumeRaymarchResolution() == RaymarchResolution::Quarter && Stats().RaymarchVolumes == 1 && Stats().RaymarchDispatches > 0, "raymarch at quarter resolution");
		});
		add([&layer]
		{
			Gfx().SetVolumeRaymarchResolution(RaymarchResolution::Half);
			layer.SetRaymarchEnabled(false);
		}, [this]
		{
			Check(Stats().RaymarchVolumes == 0 && Stats().RaymarchDispatches == 0, "raymarch off: no raymarch work");
			Check(Stats().Volumetrics, "raymarch off: froxel fog stays");
		});
		add([&layer] { layer.GetFlyCamera().JumpToPreset(2); layer.GetFlyCamera().JumpToPreset(1); }, [&layer, this]
		{
			Check(layer.GetFlyCamera().GetPreset() == 1 && layer.GetFlyCamera().GetPosition() == FlyCamera::GetPresetPosition(1), "camera at preset 1");
		});

		for (const glm::uvec3& grid : k_Grids)
		{
			add([grid] { Gfx().SetVolumetricGrid(grid); }, [grid, this]
			{
				const std::string label = "grid " + std::to_string(grid.x) + "x" + std::to_string(grid.y) + "x" + std::to_string(grid.z);
				Check(Gfx().GetVolumetricGrid() == grid, label + ": applied");
				Check(Stats().Volumetrics && Stats().VolumetricDispatches > 0, label + ": froxel volume draws");
			});
		}

		add([&layer]
		{
			Gfx().SetVolumetricDebugView(VolumetricDebugView::ShadowSamples);
			layer.GetCameraEntity().GetComponent<CameraComponent>().ResetVolumetricHistory = true;
		}, [&layer, this]
		{
			Check(Gfx().GetVolumetricDebugView() == VolumetricDebugView::ShadowSamples, "debug view set");
			Check(Stats().Volumetrics, "debug view still draws the volume");
			Check(!layer.GetCameraEntity().GetComponent<CameraComponent>().ResetVolumetricHistory, "the history reset flag is consumed");
		});
		add([] { Gfx().SetVolumetricDebugView(VolumetricDebugView::None); Gfx().ResetVolumetricHistory(); }, [this]
		{
			Check(Gfx().GetVolumetricDebugView() == VolumetricDebugView::None && Stats().Volumetrics, "debug view cleared");
		});
	}

	void Checks::Tick(DustlightLayer& layer)
	{
		if (m_Done)
			return;

		if (!m_SunChecked)
		{
			m_SunChecked = true;
			CheckSunCurve();
		}

		if (!m_Built)
		{
			if (!layer.AreEnvironmentsReady())
			{
				if (++m_ReadyFrames > k_ReadyTimeoutFrames)
				{
					Check(false, "environments loaded and ready");
					Finish();
				}
				return;
			}
			Check(layer.AreEnvironmentsLoaded(), "environments loaded and ready");
			Build(layer);
			m_Built = true;
		}

		if (m_Index >= m_Stages.size())
		{
			Finish();
			return;
		}

		if (m_Wait < 0)
		{
			m_Stages[m_Index].Enter();
			m_StageBakes = 0;
			m_Wait = k_SettleFrames;
			return;
		}
		m_StageBakes += Stats().EnvironmentBakes;
		if (--m_Wait > 0)
			return;

		m_Stages[m_Index].Verify();
		++m_Index;
		m_Wait = -1;
	}

}
