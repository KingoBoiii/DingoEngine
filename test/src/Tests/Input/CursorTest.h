#pragma once
#include "Tests/Renderer2D/Renderer2DTest.h"

namespace Dingo
{

	// "Look" accumulates GetMouseDelta() so a human can confirm capturing or
	// refocusing never kicks it.
	class CursorTest : public Renderer2DTest
	{
	public:
		CursorTest(Renderer2D* renderer)
			: Renderer2DTest(renderer)
		{}
		virtual ~CursorTest() = default;

	public:
		virtual void Initialize() override;
		virtual void Update(float deltaTime) override;
		virtual void Cleanup() override;
		virtual void ImGuiRender() override;

	private:
		Font* m_Font = nullptr;

		float m_LookYaw = 0.0f;
		float m_LookPitch = 0.0f;

		float m_LookSensitivity = 0.1f;
	};

}
