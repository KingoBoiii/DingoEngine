#pragma once
#include "Tests/Test.h"

namespace Dingo
{

	class GraphicsTest : public Test
	{
	public:
		virtual void Initialize() override;
		virtual void Cleanup() override;

		virtual void Resize(uint32_t width, uint32_t height) override;

		virtual Texture* GetResult() override { return m_Framebuffer->GetAttachment(0); }

	protected:
		virtual void InitializeGraphics() {}
		virtual void CleanupGraphics() {}

	protected:
		Framebuffer* m_Framebuffer = nullptr;
		CommandList* m_CommandList = nullptr;
	};

}
