#pragma once
#include "DingoEngine/Graphics/NVRHI/NvrhiFramebuffer.h"

namespace Dingo
{

	// Wraps an externally-owned NVRHI texture as a swap chain framebuffer.
	// Unlike NvrhiFramebuffer, this does not create or own the texture.
	class NvrhiSwapChainFramebuffer : public NvrhiFramebuffer
	{
	public:
		static NvrhiSwapChainFramebuffer* Create(nvrhi::ITexture* texture, const FramebufferParams& params);

	public:
		NvrhiSwapChainFramebuffer(nvrhi::ITexture* texture, const FramebufferParams& params)
			: NvrhiFramebuffer(params), m_Texture(texture)
		{}
		virtual ~NvrhiSwapChainFramebuffer() = default;

	public:
		virtual void Initialize() override;
		virtual void Destroy() override;

	private:
		nvrhi::ITexture* m_Texture = nullptr;
	};

}
