#pragma once
#include <DingoEngine.h>

#include <memory>

namespace Dingo
{

	class GameAssets;

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

	private:
		std::unique_ptr<GameAssets> m_Assets;
		SceneManager m_SceneManager;
		Scene* m_TitleScene = nullptr;
		Scene* m_ArenaScene = nullptr;
		Scene* m_EndScene = nullptr;
	};

}
