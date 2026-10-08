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
		void RecordPerf(float deltaTime, float updateMilliseconds, float renderMilliseconds);

	private:
		RunResult m_Result;
		SceneManager m_SceneManager;
		Scene* m_TitleScene = nullptr;
		Scene* m_KeepScene = nullptr;
		Scene* m_EndScene = nullptr;
		bool m_DroppedLightsWarned = false;

		float m_PerfClock = 0.0f;
		int m_PerfFrames = 0;
		double m_PerfFrameMilliseconds = 0.0;
		double m_PerfUpdateMilliseconds = 0.0;
		double m_PerfRenderMilliseconds = 0.0;
		bool m_PerfDone = false;
	};

}
