#pragma once
#include "Tests/GraphicsTest.h"

namespace Dingo
{

	class RendererTest : public GraphicsTest
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
		Renderer* m_Renderer = nullptr;
	};

}
