#include "MarionetteLayer.h"
#include "ArenaDirector.h"
#include "Checks.h"
#include "GameAssets.h"
#include "GameTuning.h"
#include "LaunchOptions.h"
#include "LiveEdit.h"
#include "Moveset.h"
#include "ReachTable.h"
#include "TitleScreen.h"

#include <algorithm>

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
		m_Reach = std::make_unique<ReachTable>(*m_Assets);
		if (options.LiveEditDemo)
			m_LiveEdit = std::make_unique<LiveEditDemo>(*m_Assets);
		m_Match.ResetRun(std::max(options.Bout, 1));
		if (options.End)
		{
			m_Match.Victory = true;
			m_Match.Seconds = END_DEMO_SECONDS;
			m_Match.Retries = END_DEMO_RETRIES;
		}
		if (options.Check)
		{
			RunAssetChecks(*m_Assets);
			RunMovementChecks(*m_Assets);
			RunCombatChecks(*m_Assets);
			RunAiChecks(*m_Assets, *m_Reach);
		}

		m_TitleScene = m_SceneManager.CreateScene(SCENE_TITLE);
		m_ArenaScene = m_SceneManager.CreateScene(SCENE_ARENA);
		m_EndScene = m_SceneManager.CreateScene(SCENE_END);

		for (Scene* scene : { m_TitleScene, m_ArenaScene, m_EndScene })
			scene->SetClearColor(COLOR_BG);

		RebuildTitleScene();
		RebuildEndScene();
		RebuildArenaScene();

		m_SceneManager.SetActiveScene(options.End ? SCENE_END : options.Bout > 0 ? SCENE_ARENA : SCENE_TITLE);
		m_SceneManager.GetActiveScene()->OnStart();
	}

	void MarionetteLayer::OnDetach()
	{
		if (Scene* active = m_SceneManager.GetActiveScene())
			active->OnStop();
		m_SceneManager.Clear();
		m_LiveEdit.reset();
		m_Reach.reset();
		m_Assets.reset();
	}

	void MarionetteLayer::RebuildTitleScene()
	{
		m_TitleScene->Clear();
		m_TitleScene->CreateEntity("TitleController").AddScript<TitleControllerScript>(m_Assets.get());
	}

	void MarionetteLayer::RebuildArenaScene()
	{
		m_ArenaScene->Clear();
		m_ArenaScene->CreateEntity("ArenaDirector").AddScript<ArenaDirectorScript>(m_Assets.get(), m_Reach.get(), &m_Match);
	}

	void MarionetteLayer::RebuildEndScene()
	{
		m_EndScene->Clear();
		m_EndScene->CreateEntity("EndController").AddScript<EndControllerScript>(&m_Match, m_Assets.get());
	}

	// A hot-reload replaced some clips' events. The moveset's rules are checked against them again and the reach
	// table, which was swept through the old hitboxes, forgets them; everything else reads the clips as it goes.
	void MarionetteLayer::PollEventReload()
	{
		const std::vector<EventChange> changes = m_Assets->PollEventChanges();
		if (changes.empty())
			return;

		ValidateMoveset(m_Assets->GetClips());
		m_Reach->Invalidate();
		for (const EventChange& change : changes)
			DE_INFO("[Reload] {}: {} clips' events changed; moveset re-validated, reach table rebuilt", change.File, change.Clips);
		if (m_LiveEdit)
			m_LiveEdit->OnReload();
	}

	// Started by hand: the manager sees no change of scene.
	void MarionetteLayer::RestartArena()
	{
		m_Match.Restart = false;
		RebuildArenaScene();
		m_ArenaScene->OnStart();
	}

	void MarionetteLayer::OnUpdate(float deltaTime)
	{
		const LaunchOptions& options = GetLaunchOptions();
		const bool fixed = options.FixedDt > 0.0f;
		const float step = fixed ? options.FixedDt : deltaTime;
		const int steps = fixed ? options.StepsPerFrame : 1;
		const Scene* activeBefore = m_SceneManager.GetActiveScene();

		PollEventReload();
		for (int i = 0; i < steps; ++i)
		{
			if (m_LiveEdit)
				m_LiveEdit->Update(step);
			m_SceneManager.OnUpdate(step);
			if (m_SceneManager.GetActiveScene() != activeBefore || m_Match.Restart || m_Match.Done)
				break;
		}

		const Scene* activeAfter = m_SceneManager.GetActiveScene();
		if (activeAfter == m_ArenaScene && m_Match.Restart && !m_Match.Done)
			RestartArena();
		m_SceneManager.OnRender();

		// The manager switches scenes inside its own OnUpdate, so leaving one only shows as a before/after
		// difference. A scene's script starts once, so leaving the arena or the End scene rebuilds it for the next
		// entry. The End scene has read the run's result as it started, so the next run can begin from bout 1.
		if (activeBefore == m_TitleScene && activeAfter != m_TitleScene)
		{
			RebuildTitleScene();
		}
		else if (activeBefore == m_ArenaScene && activeAfter != m_ArenaScene)
		{
			RebuildArenaScene();
			m_Match.ResetRun(1);
		}
		else if (activeBefore == m_EndScene && activeAfter != m_EndScene)
		{
			RebuildEndScene();
		}
	}

}
