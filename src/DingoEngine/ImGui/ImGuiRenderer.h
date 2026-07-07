#pragma once
#include "DingoEngine/Graphics/Shader.h"
#include "DingoEngine/UI/UIParams.h"

#include <imgui.h>

#include <nvrhi/nvrhi.h>

namespace Dingo
{

	class SwapChain;

	class ImGuiRenderer
	{
	public:
		ImGuiRenderer() = default;
		virtual ~ImGuiRenderer() = default;

		void Initialize();
		void Shutdown();

		void UpdateFontTexture();
		bool Render(ImGuiViewport* viewport, nvrhi::GraphicsPipelineHandle pipeline, nvrhi::FramebufferHandle framebuffer, nvrhi::ICommandList* sharedCmdList = nullptr);
		bool RenderToSwapchain(ImGuiViewport* viewport, SwapChain* swapchain, nvrhi::ICommandList* sharedCmdList = nullptr);

	private:
		bool ReallocateBuffer(nvrhi::BufferHandle& buffer, size_t requiredSize, size_t reallocateSize, bool isIndexBuffer);
		bool UpdateGeometry(ImDrawData* drawData, nvrhi::ICommandList* commandList);

		nvrhi::GraphicsPipelineHandle GetOrCreatePipeline(SwapChain* swapchain);
		nvrhi::IBindingSet* GetBindingSet(nvrhi::ITexture* texture);

	private:
		Shader* m_Shader = nullptr;

		nvrhi::InputLayoutHandle m_ShaderAttribLayout;
		nvrhi::BindingLayoutHandle m_BindingLayout;
		nvrhi::GraphicsPipelineDesc m_BasePSODesc;

		nvrhi::BufferHandle m_VertexBuffer;
		nvrhi::BufferHandle m_IndexBuffer;

		std::vector<ImDrawVert> m_VertexBufferData;
		std::vector<ImDrawIdx> m_IndexBufferData;

		nvrhi::TextureHandle m_FontTexture;
		nvrhi::SamplerHandle m_FontSampler;

		// Keyed by the swap chain's resize generation, NOT by framebuffer handle: caching a
		// FramebufferHandle would keep a back-buffer reference alive across resizes, which
		// makes IDXGISwapChain::ResizeBuffers fail on D3D11/D3D12. Pipelines are safe to
		// cache (they reference only state objects/shaders, never framebuffer attachments).
		struct SwapchainPipelineCache
		{
			uint64_t ResizeGeneration = ~0ull;
			std::array<nvrhi::GraphicsPipelineHandle, 3> Pipelines;
		};

		std::map<SwapChain*, SwapchainPipelineCache> m_PipelineCache;

		std::unordered_map<nvrhi::ITexture*, nvrhi::BindingSetHandle> m_BindingsCache;
	};
}
