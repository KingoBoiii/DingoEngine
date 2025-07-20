#pragma once
#include "DingoEngine/Graphics/IRenderer.h"
#include "DingoEngine/Graphics/SwapChain.h"
#include "DingoEngine/Graphics/CommandList.h"

namespace Dingo
{

	class AppRenderer : public IRenderer
	{
	public:
		AppRenderer(SwapChain* swapChain)
			: m_SwapChain(swapChain)
		{}
		virtual ~AppRenderer() = default;

	public:
		virtual void Initialize() override;
		virtual void Shutdown() override;

		void BeginFrame();
		void EndFrame();

		virtual void Begin() override;
		virtual void End() override;

		virtual void Resize(uint32_t width, uint32_t height) override;
		virtual void Clear(const glm::vec4& clearColor) override;

		virtual void Draw(Pipeline* pipeline, uint32_t vertexCount = 3, uint32_t instanceCount = 1) override;
		virtual void Draw(Pipeline* pipeline, GraphicsBuffer* vertexBuffer, uint32_t vertexCount = 3, uint32_t instanceCount = 1) override;
		virtual void DrawIndexed(Pipeline* pipeline, GraphicsBuffer* vertexBuffer, GraphicsBuffer* indexBuffer) override;
		virtual void DrawIndexed(Pipeline* pipeline, GraphicsBuffer* vertexBuffer, GraphicsBuffer* indexBuffer, GraphicsBuffer* uniformBuffer) override;

		virtual Texture* GetOutput() const override { return m_SwapChain->GetCurrentFramebuffer()->GetAttachment(0); }

	private:
		SwapChain* m_SwapChain = nullptr;		// Not owned by the renderer, managed by the application
		CommandList* m_CommandList = nullptr;

	};

}
