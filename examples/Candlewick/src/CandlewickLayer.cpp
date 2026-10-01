#include "CandlewickLayer.h"
#include "GameTuning.h"
#include "KeepDirector.h"
#include "LaunchOptions.h"
#include "TitleScreen.h"

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
		m_Result.FrameSeconds = deltaTime;

		Scene* activeBefore = m_SceneManager.GetActiveScene();

		m_SceneManager.OnUpdate(deltaTime);
		m_SceneManager.OnRender();

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
