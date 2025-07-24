#pragma once
#include "Tests/Test.h"

namespace Dingo
{

	class QuadTest : public Test
	{
	public:
		QuadTest() = default;
		virtual ~QuadTest() = default;

	public:
		virtual void Initialize() override;
		virtual void Update(float deltaTime) override;
		virtual void Cleanup() override;

		virtual void ImGuiRender() override {}

		virtual void Resize(uint32_t width, uint32_t height) override;

		virtual Texture* GetResult() { return m_Renderer->GetOutput(); }

	private:
		Renderer2D* m_Renderer = nullptr;
	};

}
