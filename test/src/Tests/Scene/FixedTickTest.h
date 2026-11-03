#pragma once
#include "Tests/GraphicsTest.h"
#include "Tests/TestChecks.h"

#include <string>

namespace Dingo
{

	// SceneParams::FixedTick: frame-rate independence, the tick counter, the per-update cap and
	// render interpolation, checked in scenes of their own; then a falling and a rolling ball at a
	// chosen tick rate, with interpolation on or off, to look at.
	class FixedTickTest : public GraphicsTest
	{
	public:
		FixedTickTest() = default;
		virtual ~FixedTickTest() = default;

	public:
		void Initialize() override;
		void Update(float deltaTime) override;
		void Cleanup() override;
		void Resize(uint32_t width, uint32_t height) override;
		void ImGuiRender() override;

		Texture* GetResult() override { return Renderer::GetSwapChainFramebuffer()->GetAttachment(0); }

	private:
		void Check(bool condition, const std::string& name) { m_Checks.Check(condition, name); }
		void RunDeterminismChecks();
		void RunCounterChecks();
		void RunInterpolationChecks();
		void BuildShowcase();

	private:
		TestChecks m_Checks;

		Scene* m_Scene = nullptr;
		int m_TickRate = 1; // index into k_TickRates
		bool m_Interpolate = true;
		bool m_AutoReset = true;
		float m_ResetTimer = 0.0f;

		PerspectiveCamera m_Camera;
	};

}
