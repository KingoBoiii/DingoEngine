#pragma once
#include "MatchState.h"

#include <DingoEngine.h>

#include <memory>

namespace Dingo
{

	class GameAssets;
	class LiveEditDemo;
	class ReachTable;

	class MarionetteLayer : public Layer
	{
	public:
		MarionetteLayer();
		virtual ~MarionetteLayer();

		void OnAttach() override;
		void OnDetach() override;
		void OnUpdate(float deltaTime) override;
		void OnEvent(Event& e) override;

	private:
		void PollEventReload();
		void RebuildTitleScene();
		void RebuildArenaScene();
		void RebuildEndScene();
		void RestartArena();
		void RecordPerf(float deltaTime, float updateMilliseconds, float renderMilliseconds);

	private:
		std::unique_ptr<GameAssets> m_Assets;
		std::unique_ptr<ReachTable> m_Reach;
		std::unique_ptr<LiveEditDemo> m_LiveEdit;
		// Before the scenes, which hold pointers to it.
		MatchState m_Match;
		SceneManager m_SceneManager;
		Scene* m_TitleScene = nullptr;
		Scene* m_ArenaScene = nullptr;
		Scene* m_EndScene = nullptr;

		float m_PerfClock = 0.0f;
		int m_PerfFrames = 0;
		double m_PerfFrameMilliseconds = 0.0;
		double m_PerfUpdateMilliseconds = 0.0;
		double m_PerfRenderMilliseconds = 0.0;
		bool m_PerfDone = false;
	};

}
