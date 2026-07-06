#pragma once
#include <DingoEngine.h>

namespace Dingo
{

	// Thin host for EchoVault's three scenes (Menu -> Game -> Win). The layer owns the
	// SceneManager and drives update/render; everything else — building the floating-
	// platform course, the player character controller, orbs, sentries, moving platforms,
	// the follow camera, lights, audio and HUD — lives in ScriptableEntity scripts
	// (GameScripts.h). Menu->Game and Game->Win transitions are requested from scripts
	// via RequestSceneTransition; the SceneManager performs them.
	class EchoVaultLayer : public Layer
	{
	public:
		EchoVaultLayer() : Layer("EchoVault") {}
		virtual ~EchoVaultLayer() = default;

		void OnAttach() override;
		void OnDetach() override;
		void OnUpdate(float deltaTime) override;

	private:
		void RebuildGameScene();

	private:
		SceneManager m_SceneManager;
		Scene* m_MenuScene = nullptr;
		Scene* m_GameScene = nullptr;
		Scene* m_WinScene = nullptr;
	};

}
