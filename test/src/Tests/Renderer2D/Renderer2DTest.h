#pragma once
#include "Tests/GraphicsTest.h"

#include <glm/glm.hpp>

namespace Dingo
{

	class Renderer2DTest : public GraphicsTest
	{
	public:
		Renderer2DTest(Renderer2D* renderer) 
			: m_Renderer(renderer)
		{}
		virtual ~Renderer2DTest() = default;

	public:
		virtual void Initialize() override;
		virtual void Cleanup() override;
		virtual void Resize(uint32_t width, uint32_t height) override;
		
		virtual void ImGuiRender() override;

		virtual Texture* GetResult() { return m_Renderer->GetOutput(); }

	private:
		void RecalculateProjectionViewMatrix();

	protected:
		Renderer2D* m_Renderer = nullptr;
		float m_OrthographicSize = 5.0f;
		float m_OrthographicNear = -1.0f;
		float m_OrthographicFar = 1.0f;
		glm::mat4 m_ProjectionViewMatrix = glm::mat4(1.0f);
	};

}
