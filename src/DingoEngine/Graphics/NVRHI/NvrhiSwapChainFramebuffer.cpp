#include "depch.h"
#include "NvrhiSwapChainFramebuffer.h"

#include "DingoEngine/Graphics/GraphicsContext.h"
#include "DingoEngine/Graphics/NVRHI/NvrhiGraphicsContext.h"

namespace Dingo
{

	NvrhiSwapChainFramebuffer* NvrhiSwapChainFramebuffer::Create(nvrhi::ITexture* texture, const FramebufferParams& params)
	{
		return new NvrhiSwapChainFramebuffer(texture, params);
	}

	void NvrhiSwapChainFramebuffer::Initialize()
	{
		const auto device = GraphicsContext::Get().As<NvrhiGraphicsContext>().GetDeviceHandle();

		nvrhi::FramebufferDesc framebufferDesc = nvrhi::FramebufferDesc()
			.addColorAttachment(m_Texture);

		// The swap-chain back buffer is colour-only. When depth is requested, create a
		// matching D32 depth target and attach it here. The Vulkan swap chain gets its
		// depth via VulkanFramebuffer, but the D3D11/D3D12 swap chains route through this
		// class — so without this they would have no depth buffer, which silently
		// disables depth test/write for every 3D pass on those back-ends.
		if (m_Params.EnableDepth)
		{
			nvrhi::TextureDesc depthDesc = nvrhi::TextureDesc()
				.setDebugName("Swap Chain (Depth)")
				.setWidth(m_Params.Width)
				.setHeight(m_Params.Height)
				.setFormat(nvrhi::Format::D32)
				.setDimension(nvrhi::TextureDimension::Texture2D)
				.setIsRenderTarget(true)
				.setInitialState(nvrhi::ResourceStates::DepthWrite)
				.setKeepInitialState(true);
			// Depth-only target: we never sample it. isShaderResource defaults to true, but
			// on D3D a non-typeless depth format (D32) cannot back a shader-resource view, so
			// leaving it on makes CreateTexture2D fail with E_INVALIDARG and the back-end ends
			// up with no depth buffer. NVRHI exposes no setter for this flag — set it directly.
			depthDesc.isShaderResource = false;

			m_DepthTextureHandle = device->createTexture(depthDesc);
			framebufferDesc.setDepthAttachment(m_DepthTextureHandle);
		}

		m_FramebufferHandle = device->createFramebuffer(framebufferDesc);

		m_Viewport = nvrhi::Viewport(static_cast<float>(m_Params.Width), static_cast<float>(m_Params.Height));
	}

	void NvrhiSwapChainFramebuffer::Destroy()
	{
		// m_Texture is owned by the swap chain — do not destroy it here. The depth
		// target, by contrast, is created and owned by this framebuffer.
		m_DepthTextureHandle = nullptr;

		if (m_FramebufferHandle)
		{
			m_FramebufferHandle = nullptr;
		}
	}

}
