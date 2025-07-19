#pragma once
#include "Test.h"

#include <DingoEngine.h>

namespace Dingo
{
	class ClearColorTest : public Test
	{
	public:
		ClearColorTest() = default;
		virtual ~ClearColorTest() = default;

	public:
		virtual void Initialize() override;
		virtual void Update(float deltaTime) override;
		virtual void Cleanup() override;

		virtual void Resize(uint32_t width, uint32_t height) override;

		virtual Texture* GetResult() override { return m_Framebuffer->GetAttachment(0); }

	private:
		Framebuffer* m_Framebuffer = nullptr;
		CommandList* m_CommandList = nullptr;
	};

} // namespace Dingo
