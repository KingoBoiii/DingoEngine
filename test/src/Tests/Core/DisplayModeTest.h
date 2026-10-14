#pragma once
#include "Tests/Renderer2D/Renderer2DTest.h"
#include "Tests/TestChecks.h"

#include <functional>
#include <string>
#include <vector>

namespace Dingo
{

	// Walks the window through its modes and checks each by the window's and the swap chain's size:
	// the display-mode list, borderless fullscreen, exclusive fullscreen, windowed again, SetSize
	// windowed and while fullscreen, then Settings::Capture/Save/Load/Apply. Exclusive takes the desktop's
	// mode unless --display-exclusive=next (the next smaller listed mode) or =<W>x<H>[@<Hz>] asks for
	// a real mode change. The Properties panel restarts it and switches modes by hand.
	class DisplayModeTest : public Renderer2DTest
	{
	public:
		DisplayModeTest(Renderer2D* renderer)
			: Renderer2DTest(renderer)
		{}
		virtual ~DisplayModeTest() = default;

	public:
		virtual void Initialize() override;
		virtual void Update(float deltaTime) override;
		virtual void Cleanup() override;
		virtual void ImGuiRender() override;

	private:
		struct Step
		{
			std::string Name;
			std::function<void()> Action;
			std::function<glm::ivec2()> ExpectedSize;
			std::function<void()> Verify;
		};

		void BuildSteps();
		void Check(bool condition, const std::string& name) { m_Checks.Check(condition, name); }
		glm::ivec2 GetSwapChainSize() const;

	private:
		Font* m_Font = nullptr;
		std::vector<Step> m_Steps;
		size_t m_Step = 0;
		uint32_t m_StepFrames = 0;
		bool m_StepStarted = false;
		bool m_Finished = false;

		std::vector<DisplayMode> m_Modes;
		DisplayMode m_Desktop;
		DisplayMode m_Exclusive;
		glm::ivec2 m_InitialSize = glm::ivec2(0);
		glm::ivec2 m_BaseSize = glm::ivec2(0);
		glm::ivec2 m_ResizedSize = glm::ivec2(0);
		glm::ivec2 m_RestoreSize = glm::ivec2(0);
		std::string m_Status;

		TestChecks m_Checks;
	};

}
