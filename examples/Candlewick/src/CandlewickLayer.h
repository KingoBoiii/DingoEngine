#pragma once
#include "RunResult.h"

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
		void RebuildEndScene();
		void CheckDroppedLights(const Scene* active);

	private:
		RunResult m_Result;
		SceneManager m_SceneManager;
		Scene* m_TitleScene = nullptr;
		Scene* m_KeepScene = nullptr;
		Scene* m_EndScene = nullptr;
		bool m_DroppedLightsWarned = false;
	};

}
