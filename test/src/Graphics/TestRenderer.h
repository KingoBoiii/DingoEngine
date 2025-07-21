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

		virtual void Upload(GraphicsBuffer* buffer) override;
		virtual void Upload(GraphicsBuffer* buffer, const void* data, uint64_t size) override;

		virtual void Draw(Pipeline* pipeline, uint32_t vertexCount = 3, uint32_t instanceCount = 1) override;
		virtual void Draw(Pipeline* pipeline, GraphicsBuffer* vertexBuffer, uint32_t vertexCount = 3, uint32_t instanceCount = 1) override;
		virtual void DrawIndexed(Pipeline* pipeline, GraphicsBuffer* vertexBuffer, GraphicsBuffer* indexBuffer) override;
		virtual void DrawIndexed(Pipeline* pipeline, GraphicsBuffer* vertexBuffer, GraphicsBuffer* indexBuffer, GraphicsBuffer* uniformBuffer) override;

		Framebuffer* GetFramebuffer() const { return m_Framebuffer; }
		virtual Texture* GetOutput() const override { return m_Framebuffer->GetAttachment(0); }

	private:
		Framebuffer* m_Framebuffer = nullptr;
		CommandList* m_CommandList = nullptr;

	};

}
