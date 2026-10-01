#pragma once
#include <DingoEngine.h>

namespace Dingo
{

	class CandlewickLayer : public Layer
	{
	public:
		CandlewickLayer() : Layer("Candlewick") {}
		virtual ~CandlewickLayer() = default;

		void OnAttach() override;
		void OnDetach() override;
		void OnUpdate(float deltaTime) override;

	private:
		void RebuildKeepScene();

	private:
		SceneManager m_SceneManager;
		Scene* m_TitleScene = nullptr;
		Scene* m_KeepScene = nullptr;
		Scene* m_EndScene = nullptr;
	};

}
