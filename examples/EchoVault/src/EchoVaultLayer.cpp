#include "EchoVaultLayer.h"
#include "GameScripts.h"
#include "GameTuning.h"

namespace Dingo
{

	void EchoVaultLayer::OnAttach()
	{
		m_MenuScene = m_SceneManager.CreateScene("Menu");
		m_GameScene = m_SceneManager.CreateScene("Game");
		m_WinScene = m_SceneManager.CreateScene("Win");

		for (Scene* scene : { m_MenuScene, m_GameScene, m_WinScene })
			scene->SetClearColor(COLOR_BG);

		m_MenuScene->CreateEntity("MenuController").AddScript<MenuControllerScript>();
		m_WinScene->CreateEntity("WinController").AddScript<WinControllerScript>();
		RebuildGameScene();

		m_SceneManager.SetActiveScene("Menu"); // first activation only selects
		m_MenuScene->OnStart();                // host starts it explicitly
	}

	void EchoVaultLayer::OnDetach()
	{
		if (Scene* active = m_SceneManager.GetActiveScene())
			active->OnStop();
	}

	void EchoVaultLayer::RebuildGameScene()
	{
		// The controller's OnStart builds the whole course; re-attaching gives a fresh run.
		m_GameScene->Clear();
		m_GameScene->CreateEntity("CourseController").AddScript<CourseControllerScript>();
	}

	void EchoVaultLayer::OnUpdate(float deltaTime)
	{
		if (Input::IsKeyDown(Key::Escape))
			Application::Get().Close();

		// A script requests Menu->Game / Game->Win via RequestSceneTransition; the manager
		// performs the switch inside OnUpdate. Rebuild the Game scene just before it is
		// activated (Menu->Game or Win->...->Game) so every run starts from a fresh course.
		if (Scene* activeScene = m_SceneManager.GetActiveScene())
		{
			if (activeScene->HasPendingSceneTransition()
				&& activeScene->GetPendingSceneTransition() == "Game"
				&& !m_GameScene->IsRunning())
			{
				RebuildGameScene();
			}
		}

		m_SceneManager.OnUpdate(deltaTime); // scripts + physics (+ any requested transition)
		m_SceneManager.OnRender();          // 3D world + 2D HUD overlay
	}

}
