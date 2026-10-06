#pragma once
#include "Tests/Renderer2D/Renderer2DTest.h"

#include <chrono>
#include <string>
#include <vector>

namespace Dingo
{

	// Minimize the window, or click away from it, then come back. With UpdateInBackground on (the
	// test app's setting) updates keep coming and only a minimized window skips rendering; with it
	// off (the Properties checkbox, or --update-in-background=off) the app pauses. Every update draws
	// a lit 3D scene with a 2D overlay through the SceneRenderer, and text through Renderer2D; the
	// cube spins by the summed delta time. Coming back runs checks over the stretch in the
	// background, shown in the Properties panel and the log.
	class BackgroundTest : public Renderer2DTest
	{
	public:
		BackgroundTest(Renderer2D* renderer)
			: Renderer2DTest(renderer)
		{}
		virtual ~BackgroundTest() = default;

	public:
		virtual void Initialize() override;
		virtual void Update(float deltaTime) override;
		virtual void Cleanup() override;
		virtual void OnEvent(Event& e) override;
		virtual void ImGuiRender() override;

	private:
		using Clock = std::chrono::steady_clock;

		void BuildScene();
		void SetBackground(bool minimized, bool unfocused);
		void CheckStretch(float returnDeltaTime, Clock::time_point now);
		void Check(bool condition, const std::string& name);

	private:
		Scene* m_Scene = nullptr;
		Entity m_Cube;
		Entity m_Light;
		Entity m_Marker;
		Font* m_Font = nullptr;
		bool m_AppUpdateInBackground = true;

		float m_Time = 0.0f;
		uint64_t m_Updates = 0;
		uint32_t m_SpacePresses = 0;
		uint32_t m_SpaceReleases = 0;
		Clock::time_point m_LastUpdate;

		// Driven by the window events, so a stretch in which no update ran is still seen.
		bool m_Minimized = false;
		bool m_Unfocused = false;
		bool m_ReturnPending = false;
		bool m_CheckSecondDelta = false;
		Clock::time_point m_StretchStart;
		bool m_StretchUpdatingMode = true;
		bool m_StretchMinimized = false;
		uint32_t m_StretchUpdates = 0;
		float m_StretchDeltaTime = 0.0f;
		float m_StretchMaxDeltaTime = 0.0f;
		bool m_StretchRenderMismatch = false;
		uint32_t m_StretchSpaceEdges = 0;
		uint32_t m_Stretches = 0;
		std::string m_LastStretch;

		struct CheckResult
		{
			std::string Name;
			bool Passed = false;
		};
		std::vector<CheckResult> m_Checks;
	};

}
