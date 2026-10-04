#include "MarionetteLayer.h"
#include "ArenaDirector.h"
#include "Checks.h"
#include "GameAssets.h"
#include "GameTuning.h"
#include "LaunchOptions.h"
#include "TitleScreen.h"

namespace Dingo
{

	MarionetteLayer::MarionetteLayer()
		: Layer("Marionette")
	{}

	MarionetteLayer::~MarionetteLayer() = default;

	void MarionetteLayer::OnAttach()
	{
		const LaunchOptions& options = GetLaunchOptions();

		m_Assets = std::make_unique<GameAssets>();
		if (options.Check)
			RunAssetChecks(*m_Assets);

		m_TitleScene = m_SceneManager.CreateScene(SCENE_TITLE);
		m_ArenaScene = m_SceneManager.CreateScene(SCENE_ARENA);
		m_EndScene = m_SceneManager.CreateScene(SCENE_END);

		for (Scene* scene : { m_TitleScene, m_ArenaScene, m_EndScene })
			scene->SetClearColor(COLOR_BG);

		m_TitleScene->CreateEntity("TitleController").AddScript<TitleControllerScript>();
		m_EndScene->CreateEntity("EndController").AddScript<EndControllerScript>();
		RebuildArenaScene();

		m_SceneManager.SetActiveScene(options.Bout > 0 ? SCENE_ARENA : SCENE_TITLE);
		m_SceneManager.GetActiveScene()->OnStart();
	}

	void MarionetteLayer::OnDetach()
	{
		if (Scene* active = m_SceneManager.GetActiveScene())
			active->OnStop();
		m_SceneManager.Clear();
		m_Assets.reset();
	}

	void MarionetteLayer::RebuildArenaScene()
	{
		m_ArenaScene->Clear();
		m_ArenaScene->CreateEntity("ArenaDirector").AddScript<ArenaDirectorScript>(m_Assets.get());
	}

	void MarionetteLayer::OnUpdate(float deltaTime)
	{
		const float fixed = GetLaunchOptions().FixedDt;
		const Scene* activeBefore = m_SceneManager.GetActiveScene();

		m_SceneManager.OnUpdate(fixed > 0.0f ? fixed : deltaTime);
		m_SceneManager.OnRender();

		// The arena's script starts once, so leaving it rebuilds the scene for the next entry.
		if (activeBefore == m_ArenaScene && m_SceneManager.GetActiveScene() != m_ArenaScene)
			RebuildArenaScene();
	}

}
