#pragma once
#include "DingoEngine/Graphics/NVRHI/NvrhiGraphicsContext.h"

#include <dxgi1_6.h>
#include <nvrhi/d3d12.h>

namespace Dingo
{

	class DirectX12GraphicsContext : public NvrhiGraphicsContext
	{
	public:
		DirectX12GraphicsContext(const GraphicsParams& params);
		virtual ~DirectX12GraphicsContext() = default;

	public:
		virtual void Initialize() override;
		virtual void Shutdown() override;

		IDXGIFactory4* GetDXGIFactory() const { return m_DXGIFactory; }
		ID3D12Device* GetD3D12Device() const { return m_D3D12Device; }
		ID3D12CommandQueue* GetGraphicsQueue() const { return m_GraphicsQueue; }
		ID3D12CommandQueue* GetComputeQueue() const { return m_ComputeQueue; }
		ID3D12CommandQueue* GetCopyQueue() const { return m_CopyQueue; }

	private:
		IDXGIFactory4* m_DXGIFactory = nullptr;
		IDXGIAdapter1* m_DXGIAdapter = nullptr;
		ID3D12Device* m_D3D12Device = nullptr;
		ID3D12CommandQueue* m_GraphicsQueue = nullptr;
		ID3D12CommandQueue* m_ComputeQueue = nullptr;
		ID3D12CommandQueue* m_CopyQueue = nullptr;
	};

}
