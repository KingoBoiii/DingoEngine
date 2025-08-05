#pragma once
#include "DingoEngine/Graphics/Shader.h"
#include "DingoEngine/ImGui/ImGuiParams.h"

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
		bool Render(ImGuiViewport* viewport, nvrhi::GraphicsPipelineHandle pipeline, nvrhi::FramebufferHandle framebuffer);
		bool RenderToSwapchain(ImGuiViewport* viewport, SwapChain* swapchain);

	private:
		bool ReallocateBuffer(nvrhi::BufferHandle& buffer, size_t requiredSize, size_t reallocateSize, bool isIndexBuffer);
		bool UpdateGeometry(ImDrawData* drawData);

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

		struct SwapchainPipelineCache
		{
			std::array<nvrhi::FramebufferHandle, 3> Framebuffers;
			std::array<nvrhi::GraphicsPipelineHandle, 3> Pipelines;
		};

		std::map<SwapChain*, SwapchainPipelineCache> m_PipelineCache;

		std::unordered_map<nvrhi::ITexture*, nvrhi::BindingSetHandle> m_BindingsCache;
	};
}
