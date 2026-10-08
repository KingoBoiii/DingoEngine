#include "CandlewickLayer.h"
#include "GameTuning.h"
#include "KeepDirector.h"
#include "LaunchOptions.h"
#include "TitleScreen.h"

#include <chrono>

namespace Dingo
{

	void CandlewickLayer::OnAttach()
	{
		m_TitleScene = m_SceneManager.CreateScene(SCENE_TITLE);
		m_KeepScene = m_SceneManager.CreateScene(SCENE_KEEP);
		m_EndScene = m_SceneManager.CreateScene(SCENE_END);

		for (Scene* scene : { m_TitleScene, m_KeepScene, m_EndScene })
			scene->SetClearColor(COLOR_BG);

		m_TitleScene->CreateEntity("TitleController").AddScript<TitleControllerScript>();
		RebuildEndScene();
		RebuildKeepScene();

		m_SceneManager.SetActiveScene(GetLaunchOptions().Room > 0 ? SCENE_KEEP : SCENE_TITLE);
		m_SceneManager.GetActiveScene()->OnStart();
	}

	void CandlewickLayer::OnDetach()
	{
		if (Scene* active = m_SceneManager.GetActiveScene())
			active->OnStop();
	}

	void CandlewickLayer::RebuildKeepScene()
	{
		m_KeepScene->Clear();
		m_KeepScene->CreateEntity("KeepDirector").AddScript<KeepDirectorScript>(&m_Result);
	}

	void CandlewickLayer::RebuildEndScene()
	{
		m_EndScene->Clear();
		m_EndScene->CreateEntity("EndController").AddScript<EndControllerScript>(&m_Result);
	}

	void CandlewickLayer::OnUpdate(float deltaTime)
	{
		using Clock = std::chrono::steady_clock;
		const auto milliseconds = [](Clock::time_point from, Clock::time_point to)
		{
			return std::chrono::duration<float, std::milli>(to - from).count();
		};

		const LaunchOptions& options = GetLaunchOptions();
		const float step = options.FixedDt > 0.0f ? options.FixedDt : deltaTime;
		m_Result.FrameSeconds = step;

		Scene* activeBefore = m_SceneManager.GetActiveScene();
		const bool measuring = options.Perf && !m_PerfDone && activeBefore == m_KeepScene;

		const Clock::time_point updateStart = Clock::now();
		m_SceneManager.OnUpdate(step);
		const Clock::time_point renderStart = Clock::now();
		m_SceneManager.OnRender();
		if (measuring)
			RecordPerf(deltaTime, milliseconds(updateStart, renderStart), milliseconds(renderStart, Clock::now()));

		// The manager switches scenes inside its own OnUpdate, so leaving a scene only shows as a
		// before/after difference. The Keep's physics world is already gone; rebuild it now so the
		// next entry starts fresh instead of with handles cached against the old world. The End
		// scene's script starts only once per attachment, so it is rebuilt to read the next result.
		const Scene* activeAfter = m_SceneManager.GetActiveScene();
		if (activeBefore == m_KeepScene && activeAfter != m_KeepScene)
			RebuildKeepScene();
		else if (activeBefore == m_EndScene && activeAfter != m_EndScene)
			RebuildEndScene();

		CheckDroppedLights(activeAfter);
	}

	void CandlewickLayer::RecordPerf(float deltaTime, float updateMilliseconds, float renderMilliseconds)
	{
		m_PerfClock += deltaTime;
		if (m_PerfClock < PERF_WARMUP_SECONDS)
			return;

		m_PerfFrameMilliseconds += 1000.0 * static_cast<double>(deltaTime);
		m_PerfUpdateMilliseconds += static_cast<double>(updateMilliseconds);
		m_PerfRenderMilliseconds += static_cast<double>(renderMilliseconds);
		if (++m_PerfFrames < PERF_FRAMES)
			return;

		const double frames = static_cast<double>(m_PerfFrames);
		DE_INFO("[Perf] frame {:.3f} ms, SceneManager::OnUpdate {:.3f} ms, OnRender {:.3f} ms (mean of {} frames)", m_PerfFrameMilliseconds / frames,
			m_PerfUpdateMilliseconds / frames, m_PerfRenderMilliseconds / frames, m_PerfFrames);
		for (const GpuTimerStats& timer : Renderer::GetGpuTimers())
			DE_INFO("[Perf] GPU {}{}: mean {:.3f} ms, max {:.3f} ms over {} frames", std::string(2 * timer.Depth, ' '), timer.Name, timer.MeanMs, timer.MaxMs, timer.Samples);
		m_PerfDone = true;
	}

	void CandlewickLayer::CheckDroppedLights(const Scene* active)
	{
		if (m_DroppedLightsWarned || active != m_KeepScene || GetLaunchOptions().NoLightLod)
			return;

		const uint32_t dropped = Application::Get().GetRenderer3D().GetStatistics().DroppedLights;
		if (dropped > 0)
		{
			m_DroppedLightsWarned = true;
			DE_WARN("Candlewick: the renderer dropped {} light(s) with the light LOD on; the LOD's copy of its culling has drifted from the engine's, so a drawn cone may not be a tested one", dropped);
		}
	}

}
