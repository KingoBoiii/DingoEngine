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
		struct PerfGpuTimer
		{
			std::string Name;
			uint32_t Depth = 0;
			double SumMilliseconds = 0.0;
			float MaxMilliseconds = 0.0f;
			int Frames = 0;
			uint32_t SeenSamples = 0;
			float SeenMilliseconds = -1.0f;
		};

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
		std::vector<PerfGpuTimer> m_PerfGpuTimers;
		bool m_PerfDone = false;
	};

}
