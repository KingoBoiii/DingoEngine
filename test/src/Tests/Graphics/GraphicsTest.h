#pragma once
#include "Tests/Test.h"
#include "Graphics/TestRenderer.h"

namespace Dingo
{

	class GraphicsTest : public Test
	{
	public:
		virtual void Initialize() override;
		virtual void Cleanup() override;

		virtual void Resize(uint32_t width, uint32_t height) override;

		virtual Texture* GetResult() override { return m_Renderer->GetOutput(); }

	protected:
		virtual void InitializeGraphics() {}
		virtual void CleanupGraphics() {}

	protected:
		float m_AspectRatio;
		glm::vec4 m_ClearColor = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);

		TestRenderer* m_Renderer = nullptr;
	};

}
