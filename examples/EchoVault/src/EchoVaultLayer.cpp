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
		// Connect events only fire for post-startup changes; report pads present at launch once.
		if (!m_LoggedStartupGamepads)
		{
			m_LoggedStartupGamepads = true;
			for (uint32_t pad = 0; pad < MaxGamepads; pad++)
			{
				if (Input::IsGamepadConnected(pad))
					DE_INFO("Gamepad already connected in slot {}: {} ('{}')", pad, ToString(Input::GetGamepadType(pad)), Input::GetGamepadName(pad));
			}
		}

		if (Input::IsKeyPressed(Key::Escape))
			Application::Get().Close();

		Scene* activeBefore = m_SceneManager.GetActiveScene();

		m_SceneManager.OnUpdate(deltaTime); // scripts + physics (+ any requested transition)
		m_SceneManager.OnRender();          // 3D world + 2D HUD overlay

		// The manager performs a requested transition inside its OnUpdate, in the same
		// frame it was requested — a pending request is never observable out here. So
		// rebuild the Game scene the moment we leave it (Game->Win): OnStop has already
		// destroyed its physics world, and reusing the stale scene would dangle every
		// script-cached handle and replay an already-collected course. Clearing now means
		// the next Menu->Game activation starts from a fresh course.
		if (activeBefore == m_GameScene && m_SceneManager.GetActiveScene() != m_GameScene)
			RebuildGameScene();
	}

	void EchoVaultLayer::OnEvent(Event& event)
	{
		EventDispatcher dispatcher(event);
		dispatcher.Dispatch<GamepadConnectedEvent>(DE_BIND_EVENT_FN(EchoVaultLayer::OnGamepadConnected));
		dispatcher.Dispatch<GamepadDisconnectedEvent>(DE_BIND_EVENT_FN(EchoVaultLayer::OnGamepadDisconnected));
	}

	bool EchoVaultLayer::OnGamepadConnected(GamepadConnectedEvent& event)
	{
		DE_INFO("Gamepad connected in slot {}: {} ('{}')", event.GetGamepadId(), ToString(event.GetGamepadType()), event.GetGamepadName());
		return false;
	}

	bool EchoVaultLayer::OnGamepadDisconnected(GamepadDisconnectedEvent& event)
	{
		DE_INFO("Gamepad disconnected from slot {}: {} ('{}')", event.GetGamepadId(), ToString(event.GetGamepadType()), event.GetGamepadName());
		return false;
	}

}
