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

		virtual Texture* GetOutput() const override;

	private:
		SwapChain* m_SwapChain = nullptr;		// Not owned by the renderer, managed by the application
		CommandList* m_CommandList = nullptr;

	};

}
