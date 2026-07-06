#include "DungeonLayer.h"
#include "GameScripts.h"

namespace Dingo
{

	void DungeonLayer::OnAttach()
	{
		m_Scene = m_SceneManager.CreateScene("Dungeon Crawler 3D");

		// The controller's OnStart builds the entire world (and runs before physics, so
		// its rigid bodies are baked when the scene starts).
		m_Scene->CreateEntity("GameController").AddScript<DungeonControllerScript>();

		m_SceneManager.SetActiveScene("Dungeon Crawler 3D"); // select the active scene
		m_Scene->OnStart();                                  // explicit start: build + physics
	}

	void DungeonLayer::OnDetach()
	{
		m_Scene->OnStop(); // explicit teardown (stops physics)
	}

	void DungeonLayer::OnUpdate(float deltaTime)
	{
		if (Input::IsKeyPressed(Key::Escape))
			Application::Get().Close();

		if (Input::IsKeyPressed(Key::R))
			RestartScene();

		m_SceneManager.OnUpdate(deltaTime); // scripts + physics
		m_SceneManager.OnRender();          // 3D world + 2D HUD overlay
	}

	void DungeonLayer::RestartScene()
	{
		// Stop physics, destroy every entity, then re-attach the controller — its OnStart
		// rolls a fresh procedural dungeon and OnStart runs before physics restarts.
		m_Scene->OnStop();
		m_Scene->Clear();
		m_Scene->CreateEntity("GameController").AddScript<DungeonControllerScript>();
		m_Scene->OnStart();
	}

}
