#pragma once
#include "DingoEngine/Graphics/NVRHI/NvrhiGraphicsContext.h"

#include <dxgi1_6.h>
#include <nvrhi/d3d11.h>

namespace Dingo
{

	class DirectX11GraphicsContext : public NvrhiGraphicsContext
	{
	public:
		DirectX11GraphicsContext(const GraphicsParams& params);
		virtual ~DirectX11GraphicsContext() = default;

	public:
		virtual void Initialize() override;
		virtual void Shutdown() override;

		ID3D11Device* GetD3D11Device() const { return m_D3D11Device; }
		ID3D11DeviceContext* GetD3D11DeviceContext() const { return m_D3D11DeviceContext; }
		IDXGIFactory2* GetDXGIFactory() const { return m_DXGIFactory; }

	private:
		ID3D11Device* m_D3D11Device = nullptr;
		ID3D11DeviceContext* m_D3D11DeviceContext = nullptr;
		IDXGIFactory2* m_DXGIFactory = nullptr;
	};

}
