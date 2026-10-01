#pragma once
#include "Tests/GraphicsTest.h"

namespace Dingo
{

	// Renderer3D's scene lighting on a fixed set of pillars: the default light, coloured point and
	// spot lights over a dark ambient, and more point lights than the budget allows. The camera is
	// fixed and nothing moves unless Animate is on, so frames are repeatable; start a mode with
	// --lighting=default|lights|overbudget.
	class LightingTest : public GraphicsTest
	{
	public:
		LightingTest() = default;
		virtual ~LightingTest() = default;

	public:
		void Initialize() override;
		void Update(float deltaTime) override;
		void Cleanup() override;
		void Resize(uint32_t width, uint32_t height) override;
		void ImGuiRender() override;

	private:
		enum class Mode : int
		{
			DefaultLight,
			PointAndSpot,
			OverBudget
		};

		void SubmitLights(Renderer3D& renderer) const;
		void DrawScene(Renderer3D& renderer) const;

	private:
		PerspectiveCamera m_Camera;
		Mode m_Mode = Mode::PointAndSpot;
		bool m_Animate = false;
		float m_Time = 0.0f;
	};

}
