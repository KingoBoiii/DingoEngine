#pragma once
#include "Tests/Renderer2D/Renderer2DTest.h"
#include "Tests/TestChecks.h"

#include <string>

namespace Dingo
{

	// The game UI. The checks run over the first frames: font metrics and wrapping, world-to-screen
	// projection, and the input consume mask DingoUI and a capturing ImGui hide presses with,
	// driven by simulated mouse and key presses. The view shows the wrapped text at its MaxWidth.
	class DingoUITest : public Renderer2DTest
	{
	public:
		DingoUITest(Renderer2D* renderer)
			: Renderer2DTest(renderer)
		{}
		virtual ~DingoUITest() = default;

	public:
		virtual void Initialize() override;
		virtual void Update(float deltaTime) override;
		virtual void Cleanup() override;
		virtual void ImGuiRender() override;

	private:
		void CheckFontMetrics();
		void CheckWorldToScreen();
		void StepConsumeMask();
		void Check(bool condition, const std::string& name) { m_Checks.Check(condition, name); }

	private:
		Font* m_Font = nullptr;
		uint32_t m_Stage = 0;
		TestChecks m_Checks;
	};

}
