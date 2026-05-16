#pragma once
#include "DingoEngine/Graphics/SwapChain.h"

#include <dxgi1_6.h>
#include <nvrhi/d3d12.h>

namespace Dingo
{

	class DirectX12SwapChain : public SwapChain
	{
	public:
		DirectX12SwapChain(const SwapChainParams& params);
		virtual ~DirectX12SwapChain();

	public:
		virtual void Initialize() override;
		virtual void Destroy() override;
		virtual void Resize(int32_t width, int32_t height) override;

		virtual void AcquireNextImage() override;
		virtual void Present() override;

		virtual Framebuffer* GetCurrentFramebuffer() const override
		{
			return GetFramebuffer(m_CurrentBackBufferIndex);
		}

		virtual uint32_t GetCurrentBackBufferIndex() const override
		{
			return m_CurrentBackBufferIndex;
		}

	private:
		void CreateSwapChainAndBuffers();
		void CreateFramebuffers();
		void DestroyBuffersAndFramebuffers();

	private:
		static constexpr uint32_t k_BufferCount = 3;

		IDXGISwapChain3* m_SwapChain = nullptr;
		uint32_t m_CurrentBackBufferIndex = 0;

		struct BackBuffer
		{
			ID3D12Resource* resource = nullptr;
			nvrhi::TextureHandle rhiHandle;
		};
		std::array<BackBuffer, k_BufferCount> m_BackBuffers;

		// Per-frame fence synchronization
		ID3D12Fence* m_Fence = nullptr;
		HANDLE m_FenceEvent = nullptr;
		std::array<uint64_t, k_BufferCount> m_FenceValues = {};
		uint64_t m_NextFenceValue = 1;
	};

}
