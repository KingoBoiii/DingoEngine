#include "depch.h"
#include "DirectX12GraphicsContext.h"

#include "DingoEngine/Graphics/NVRHI/NvrhiGraphicsContext.h"

namespace Dingo
{

	static bool IsWarpAdapter(IDXGIAdapter1* adapter)
	{
		DXGI_ADAPTER_DESC1 desc;
		adapter->GetDesc1(&desc);
		return (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0;
	}

	static IDXGIAdapter1* PickHardwareAdapter(IDXGIFactory4* factory)
	{
		IDXGIAdapter1* adapter = nullptr;
		IDXGIAdapter1* bestAdapter = nullptr;
		SIZE_T bestDedicatedVideoMemory = 0;

		for (UINT i = 0; factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i)
		{
			if (IsWarpAdapter(adapter))
			{
				adapter->Release();
				continue;
			}

			if (SUCCEEDED(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_0, _uuidof(ID3D12Device), nullptr)))
			{
				DXGI_ADAPTER_DESC1 desc;
				adapter->GetDesc1(&desc);

				if (desc.DedicatedVideoMemory > bestDedicatedVideoMemory)
				{
					if (bestAdapter)
						bestAdapter->Release();
					bestAdapter = adapter;
					bestDedicatedVideoMemory = desc.DedicatedVideoMemory;
					adapter = nullptr;
				}
			}

			if (adapter)
			{
				adapter->Release();
				adapter = nullptr;
			}
		}

		return bestAdapter;
	}

	DirectX12GraphicsContext::DirectX12GraphicsContext(const GraphicsParams& params)
		: NvrhiGraphicsContext(params)
	{}

	void DirectX12GraphicsContext::Initialize()
	{
		UINT dxgiFactoryFlags = 0;

#if defined(DE_DEBUG)
		{
			ID3D12Debug* debugController = nullptr;
			if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
			{
				debugController->EnableDebugLayer();
				debugController->Release();
				dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
			}
		}
#endif

		HRESULT hr = CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&m_DXGIFactory));
		DE_CORE_ASSERT(SUCCEEDED(hr), "Failed to create DXGI factory.");

		m_DXGIAdapter = PickHardwareAdapter(m_DXGIFactory);
		DE_CORE_ASSERT(m_DXGIAdapter, "No suitable DirectX 12 GPU found.");

		{
			DXGI_ADAPTER_DESC1 desc;
			m_DXGIAdapter->GetDesc1(&desc);
			char name[128] = {};
			WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, name, sizeof(name), nullptr, nullptr);
			DE_CORE_INFO("DirectX 12: using adapter '{}'", name);

			m_AdapterInfo.Name = name;
			m_AdapterInfo.VendorID = desc.VendorId;
			m_AdapterInfo.DeviceID = desc.DeviceId;
			m_AdapterInfo.DedicatedVideoMemory = desc.DedicatedVideoMemory;
			m_AdapterInfo.DeviceType = desc.DedicatedVideoMemory > 0 ? AdapterDeviceType::Discrete : AdapterDeviceType::Integrated;
		}

		hr = D3D12CreateDevice(m_DXGIAdapter, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&m_D3D12Device));
		DE_CORE_ASSERT(SUCCEEDED(hr), "Failed to create D3D12 device.");

		auto createQueue = [&](D3D12_COMMAND_LIST_TYPE type) -> ID3D12CommandQueue*
		{
			D3D12_COMMAND_QUEUE_DESC queueDesc = {};
			queueDesc.Type = type;
			queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
			queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
			queueDesc.NodeMask = 0;

			ID3D12CommandQueue* queue = nullptr;
			HRESULT result = m_D3D12Device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&queue));
			DE_CORE_ASSERT(SUCCEEDED(result), "Failed to create D3D12 command queue.");
			return queue;
		};

		m_GraphicsQueue = createQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
		m_ComputeQueue = createQueue(D3D12_COMMAND_LIST_TYPE_COMPUTE);
		m_CopyQueue = createQueue(D3D12_COMMAND_LIST_TYPE_COPY);

		nvrhi::d3d12::DeviceDesc deviceDesc;
		deviceDesc.errorCB = &NvrhiMessageCallback::Get();
		deviceDesc.pDevice = m_D3D12Device;
		deviceDesc.pGraphicsCommandQueue = m_GraphicsQueue;
		deviceDesc.pComputeCommandQueue = m_ComputeQueue;
		deviceDesc.pCopyCommandQueue = m_CopyQueue;

		m_DeviceHandle = nvrhi::d3d12::createDevice(deviceDesc);
		DE_CORE_ASSERT(m_DeviceHandle, "Failed to create NVRHI D3D12 device.");

		DE_CORE_INFO("DirectX 12 graphics context initialized.");
	}

	void DirectX12GraphicsContext::Shutdown()
	{
		if (m_DeviceHandle)
		{
			m_DeviceHandle->waitForIdle();
			m_DeviceHandle = nullptr;
		}

		if (m_CopyQueue) { m_CopyQueue->Release(); m_CopyQueue = nullptr; }
		if (m_ComputeQueue) { m_ComputeQueue->Release(); m_ComputeQueue = nullptr; }
		if (m_GraphicsQueue) { m_GraphicsQueue->Release(); m_GraphicsQueue = nullptr; }
		if (m_D3D12Device) { m_D3D12Device->Release(); m_D3D12Device = nullptr; }
		if (m_DXGIAdapter) { m_DXGIAdapter->Release(); m_DXGIAdapter = nullptr; }
		if (m_DXGIFactory) { m_DXGIFactory->Release(); m_DXGIFactory = nullptr; }
	}

}
