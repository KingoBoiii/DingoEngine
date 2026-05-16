#include "depch.h"
#include "DirectX11GraphicsContext.h"

#include "DingoEngine/Graphics/NVRHI/NvrhiGraphicsContext.h"

namespace Dingo
{

	DirectX11GraphicsContext::DirectX11GraphicsContext(const GraphicsParams& params)
		: NvrhiGraphicsContext(params)
	{}

	void DirectX11GraphicsContext::Initialize()
	{
		UINT deviceFlags = 0;
#if defined(DE_DEBUG)
		deviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

		D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };
		D3D_FEATURE_LEVEL chosenFeatureLevel;

		HRESULT hr = D3D11CreateDevice(
			nullptr,
			D3D_DRIVER_TYPE_HARDWARE,
			nullptr,
			deviceFlags,
			featureLevels, (UINT)std::size(featureLevels),
			D3D11_SDK_VERSION,
			&m_D3D11Device,
			&chosenFeatureLevel,
			&m_D3D11DeviceContext
		);
		DE_CORE_ASSERT(SUCCEEDED(hr), "Failed to create D3D11 device.");

		// Walk the DXGI chain to get IDXGIFactory2 for swap chain creation
		IDXGIDevice* dxgiDevice = nullptr;
		hr = m_D3D11Device->QueryInterface(IID_PPV_ARGS(&dxgiDevice));
		DE_CORE_ASSERT(SUCCEEDED(hr), "Failed to get IDXGIDevice from D3D11 device.");

		IDXGIAdapter* dxgiAdapter = nullptr;
		hr = dxgiDevice->GetAdapter(&dxgiAdapter);
		dxgiDevice->Release();
		DE_CORE_ASSERT(SUCCEEDED(hr), "Failed to get IDXGIAdapter.");

		{
			DXGI_ADAPTER_DESC desc;
			dxgiAdapter->GetDesc(&desc);
			char name[128] = {};
			WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, name, sizeof(name), nullptr, nullptr);
			DE_CORE_INFO("DirectX 11: using adapter '{}'", name);
		}

		IDXGIFactory* dxgiFactory = nullptr;
		hr = dxgiAdapter->GetParent(IID_PPV_ARGS(&dxgiFactory));
		dxgiAdapter->Release();
		DE_CORE_ASSERT(SUCCEEDED(hr), "Failed to get IDXGIFactory.");

		hr = dxgiFactory->QueryInterface(IID_PPV_ARGS(&m_DXGIFactory));
		dxgiFactory->Release();
		DE_CORE_ASSERT(SUCCEEDED(hr), "Failed to query IDXGIFactory2.");

		nvrhi::d3d11::DeviceDesc deviceDesc;
		deviceDesc.messageCallback = &NvrhiMessageCallback::Get();
		deviceDesc.context = m_D3D11DeviceContext;

		m_DeviceHandle = nvrhi::d3d11::createDevice(deviceDesc);
		DE_CORE_ASSERT(m_DeviceHandle, "Failed to create NVRHI D3D11 device.");

		DE_CORE_INFO("DirectX 11 graphics context initialized.");
	}

	void DirectX11GraphicsContext::Shutdown()
	{
		if (m_DeviceHandle)
		{
			m_DeviceHandle->waitForIdle();
			m_DeviceHandle = nullptr;
		}

		if (m_DXGIFactory) { m_DXGIFactory->Release(); m_DXGIFactory = nullptr; }
		if (m_D3D11DeviceContext) { m_D3D11DeviceContext->Release(); m_D3D11DeviceContext = nullptr; }
		if (m_D3D11Device) { m_D3D11Device->Release(); m_D3D11Device = nullptr; }
	}

}
