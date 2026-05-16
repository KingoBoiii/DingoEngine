#pragma once
#include "DingoEngine/Graphics/SwapChain.h"

#include <dxgi1_6.h>
#include <nvrhi/d3d11.h>

namespace Dingo
{

	class DirectX11SwapChain : public SwapChain
	{
	public:
		DirectX11SwapChain(const SwapChainParams& params);
		virtual ~DirectX11SwapChain();

	public:
		virtual void Initialize() override;
		virtual void Destroy() override;
		virtual void Resize(int32_t width, int32_t height) override;

		virtual void AcquireNextImage() override;
		virtual void Present() override;

		virtual Framebuffer* GetCurrentFramebuffer() const override
		{
			return GetFramebuffer(0);
		}

		virtual uint32_t GetCurrentBackBufferIndex() const override
		{
			return 0;
		}

	private:
		void CreateSwapChainAndBuffer();
		void CreateFramebuffer();
		void DestroyBufferAndFramebuffer();

	private:
		IDXGISwapChain1* m_SwapChain = nullptr;
		ID3D11Texture2D* m_BackBuffer = nullptr;
		nvrhi::TextureHandle m_BackBufferHandle;
	};

}
