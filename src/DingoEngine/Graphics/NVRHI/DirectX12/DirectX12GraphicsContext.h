#pragma once
#include "DingoEngine/Graphics/GraphicsContext.h"

#define USE_NVRHI_D3D12 0

#if USE_NVRHI_D3D12

#include <d3d12.h>
#include <nvrhi/d3d12.h>

namespace Dingo
{

	class DirectX12GraphicsContext : public GraphicsContext
	{
	public:
		DirectX12GraphicsContext()
			: GraphicsContext(GraphicsAPI::DirectX12)
		{}
		virtual ~DirectX12GraphicsContext() = default;

	public:
		virtual void Initialize() override;
		virtual void Shutdown() override;

	private:
		nvrhi::d3d12::DeviceHandle m_D3D12DeviceHandle;
	};

}

#endif
