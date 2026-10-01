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
		m_EndScene->CreateEntity("EndController").AddScript<EndControllerScript>();
		RebuildKeepScene();

		m_SceneManager.SetActiveScene(GetLaunchOptions().Room > 0 ? SCENE_KEEP : SCENE_TITLE); // first activation only selects
		m_SceneManager.GetActiveScene()->OnStart();                                    // host starts it explicitly
	}

	void CandlewickLayer::OnDetach()
	{
		if (Scene* active = m_SceneManager.GetActiveScene())
			active->OnStop();
	}

	void CandlewickLayer::RebuildKeepScene()
	{
		m_KeepScene->Clear();
		m_KeepScene->CreateEntity("KeepDirector").AddScript<KeepDirectorScript>();
	}

	void CandlewickLayer::OnUpdate(float deltaTime)
	{
		Scene* activeBefore = m_SceneManager.GetActiveScene();

		m_SceneManager.OnUpdate(deltaTime);
		m_SceneManager.OnRender();

		// The manager switches scenes inside its own OnUpdate, so leaving the Keep only shows
		// as a before/after difference. Its physics world is already gone; rebuild now so the
		// next entry starts fresh instead of with handles cached against the old world.
		if (activeBefore == m_KeepScene && m_SceneManager.GetActiveScene() != m_KeepScene)
			RebuildKeepScene();
	}

}
