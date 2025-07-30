#pragma once
#include "Tests/ClearColorTest.h"

#include <glm/glm.hpp>

namespace Dingo
{

	class Renderer2DTest : public ClearColorTest
	{
	public:
		Renderer2DTest() = default;
		virtual ~Renderer2DTest() = default;

	public:
		virtual void Initialize() override;
		virtual void Cleanup() override;
		virtual void Resize(uint32_t width, uint32_t height) override;
		
		virtual void ImGuiRender() override;

		virtual Texture* GetResult() { return m_Renderer->GetOutput(); }

	protected:
		Renderer2D* m_Renderer = nullptr;
	};

}
