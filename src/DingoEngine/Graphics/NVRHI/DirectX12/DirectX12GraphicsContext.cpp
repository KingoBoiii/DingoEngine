#include "depch.h"
#include "DirectX12GraphicsContext.h"

#if USE_NVRHI_D3D12

namespace Dingo
{

	void DirectX12GraphicsContext::Initialize()
	{
		ID3D12Device* device = nullptr;
		HRESULT hr = D3D12CreateDevice(
			nullptr, // Use the default adapter
			D3D_FEATURE_LEVEL_12_0, // Minimum feature level
			IID_PPV_ARGS(&device)
		);

		nvrhi::d3d12::DeviceDesc deviceDesc{
		};

		m_D3D12DeviceHandle = nvrhi::d3d12::createDevice(deviceDesc);
	}

	void DirectX12GraphicsContext::Shutdown()
	{}

}

#endif
