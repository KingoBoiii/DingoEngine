#pragma once
#include "MatchState.h"

#include <DingoEngine.h>

#include <memory>

namespace Dingo
{

	class GameAssets;
	class ReachTable;

	class MarionetteLayer : public Layer
	{
	public:
		MarionetteLayer();
		virtual ~MarionetteLayer();

		void OnAttach() override;
		void OnDetach() override;
		void OnUpdate(float deltaTime) override;

	private:
		void RebuildArenaScene();
		void RebuildEndScene();
		void RestartArena();

	private:
		std::unique_ptr<GameAssets> m_Assets;
		std::unique_ptr<ReachTable> m_Reach;
		// Before the scenes, which hold pointers to it.
		MatchState m_Match;
		SceneManager m_SceneManager;
		Scene* m_TitleScene = nullptr;
		Scene* m_ArenaScene = nullptr;
		Scene* m_EndScene = nullptr;
	};

}
