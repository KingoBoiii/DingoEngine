#pragma once
#include <DingoEngine.h>

namespace Dingo
{

	class TestRenderer : public IRenderer
	{
	public:
		TestRenderer() = default;
		virtual ~TestRenderer() = default;

	public:
		virtual void Initialize() override;
		virtual void Shutdown() override;

		virtual void Begin() override;
		virtual void End() override;

		virtual void Resize(uint32_t width, uint32_t height) override;
		virtual void Clear(const glm::vec4& clearColor) override;

		virtual Texture* GetOutput() const override { return m_Framebuffer->GetAttachment(0); }

	private:
		Framebuffer* m_Framebuffer = nullptr;
		CommandList* m_CommandList = nullptr;

	};

}
