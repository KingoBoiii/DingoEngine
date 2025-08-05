#pragma once
#include "Tests/GraphicsTest.h"

namespace Dingo
{

	class RendererTest : public GraphicsTest
	{
	public:
		RendererTest(Renderer* renderer)
			: m_Renderer(renderer)
		{}
		virtual ~RendererTest() = default;

	public:
		virtual void Resize(uint32_t width, uint32_t height) override;

		virtual Texture* GetResult() override { return m_Renderer->GetOutput(); }

	protected:
		Renderer* m_Renderer = nullptr;
	};

}
