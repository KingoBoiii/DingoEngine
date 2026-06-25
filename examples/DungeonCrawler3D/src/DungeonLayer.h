#pragma once
#include <DingoEngine.h>

namespace Dingo
{

	// A thin host for the 3D dungeon-crawler scene. The layer owns the SceneManager,
	// creates the scene, attaches the root DungeonControllerScript, and then only drives
	// update / render and the R-to-restart. EVERYTHING else — building the procedural
	// dungeon, the player, enemies, treasure, the follow camera, lights, and the HUD —
	// lives in ScriptableEntity scripts (see GameScripts.h), so the scene handles the
	// game and the layer stays tiny.
	class DungeonLayer : public Layer
	{
	public:
		DungeonLayer() : Layer("DungeonCrawler3D") {}
		virtual ~DungeonLayer() = default;

		void OnAttach() override;
		void OnUpdate(float deltaTime) override;

	private:
		void RestartScene(); // tear down + rebuild from a fresh controller (R key)

	private:
		SceneManager m_SceneManager;
		Scene* m_Scene = nullptr; // owned by m_SceneManager
	};

}
