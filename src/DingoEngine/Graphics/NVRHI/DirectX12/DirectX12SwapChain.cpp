#include "depch.h"
#include "DirectX12SwapChain.h"

#include <glfw/glfw3.h>
#include <GLFW/glfw3native.h>

#include "DirectX12GraphicsContext.h"
#include "DingoEngine/Graphics/GraphicsContext.h"
#include "DingoEngine/Graphics/NVRHI/NvrhiGraphicsContext.h"
#include "DingoEngine/Graphics/NVRHI/NvrhiSwapChainFramebuffer.h"

namespace Dingo
{

	DirectX12SwapChain::DirectX12SwapChain(const SwapChainParams& params)
		: SwapChain(params)
	{}

	DirectX12SwapChain::~DirectX12SwapChain()
	{}

	void DirectX12SwapChain::Initialize()
	{
		auto& ctx = GraphicsContext::Get().As<DirectX12GraphicsContext>();

		// Fence for per-frame CPU/GPU synchronization
		HRESULT hr = ctx.GetD3D12Device()->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_Fence));
		DE_CORE_ASSERT(SUCCEEDED(hr), "Failed to create D3D12 fence.");

		m_FenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		DE_CORE_ASSERT(m_FenceEvent, "Failed to create fence event.");

		m_FenceValues.fill(0);
		m_NextFenceValue = 1;

		CreateSwapChainAndBuffers();
		CreateFramebuffers();
	}

	void DirectX12SwapChain::Destroy()
	{
		if (m_Fence)
		{
			// Wait for all in-flight frames to complete
			uint64_t maxPending = *std::max_element(m_FenceValues.begin(), m_FenceValues.end());
			if (m_Fence->GetCompletedValue() < maxPending)
			{
				m_Fence->SetEventOnCompletion(maxPending, m_FenceEvent);
				WaitForSingleObject(m_FenceEvent, INFINITE);
			}
		}

		DestroyBuffersAndFramebuffers();

		if (m_FenceEvent) { CloseHandle(m_FenceEvent); m_FenceEvent = nullptr; }
		if (m_Fence) { m_Fence->Release(); m_Fence = nullptr; }
		if (m_SwapChain) { m_SwapChain->Release(); m_SwapChain = nullptr; }
	}

	void DirectX12SwapChain::Resize(int32_t width, int32_t height)
	{
		if (width == 0 || height == 0)
		{
			DE_CORE_WARN("Swap chain resize called with zero dimensions, ignoring.");
			return;
		}

		auto& ctx = GraphicsContext::Get().As<DirectX12GraphicsContext>();

		// Wait for all in-flight frames before resizing
		uint64_t maxPending = *std::max_element(m_FenceValues.begin(), m_FenceValues.end());
		if (m_Fence->GetCompletedValue() < maxPending)
		{
			m_Fence->SetEventOnCompletion(maxPending, m_FenceEvent);
			WaitForSingleObject(m_FenceEvent, INFINITE);
		}

		m_Params.Width = width;
		m_Params.Height = height;

		DestroyBuffersAndFramebuffers();

		HRESULT hr = m_SwapChain->ResizeBuffers(k_BufferCount, (UINT)width, (UINT)height, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
		DE_CORE_ASSERT(SUCCEEDED(hr), "Failed to resize D3D12 swap chain buffers.");

		m_CurrentBackBufferIndex = m_SwapChain->GetCurrentBackBufferIndex();
		m_FenceValues.fill(0);
		m_NextFenceValue = 1;

		CreateFramebuffers();
	}

	void DirectX12SwapChain::AcquireNextImage()
	{
		m_CurrentBackBufferIndex = m_SwapChain->GetCurrentBackBufferIndex();

		uint64_t pendingValue = m_FenceValues[m_CurrentBackBufferIndex];
		if (pendingValue != 0 && m_Fence->GetCompletedValue() < pendingValue)
		{
			m_Fence->SetEventOnCompletion(pendingValue, m_FenceEvent);
			WaitForSingleObject(m_FenceEvent, INFINITE);
		}
	}

	void DirectX12SwapChain::Present()
	{
		auto& ctx = GraphicsContext::Get().As<DirectX12GraphicsContext>();

		UINT syncInterval = m_Params.VSync ? 1 : 0;
		HRESULT hr = m_SwapChain->Present(syncInterval, 0);
		DE_CORE_ASSERT(SUCCEEDED(hr), "D3D12 swap chain Present failed.");

		// Signal fence for the buffer we just presented
		uint64_t fenceValue = m_NextFenceValue++;
		ctx.GetGraphicsQueue()->Signal(m_Fence, fenceValue);
		m_FenceValues[m_CurrentBackBufferIndex] = fenceValue;
	}

	void DirectX12SwapChain::CreateSwapChainAndBuffers()
	{
		auto& ctx = GraphicsContext::Get().As<DirectX12GraphicsContext>();

		HWND hwnd = glfwGetWin32Window(m_Params.NativeWindowHandle);
		DE_CORE_ASSERT(hwnd, "Failed to get Win32 window handle from GLFW.");

		DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
		swapChainDesc.Width = (UINT)m_Params.Width;
		swapChainDesc.Height = (UINT)m_Params.Height;
		swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swapChainDesc.SampleDesc.Count = 1;
		swapChainDesc.SampleDesc.Quality = 0;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.BufferCount = k_BufferCount;
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swapChainDesc.Flags = 0;

		IDXGISwapChain1* swapChain1 = nullptr;
		HRESULT hr = ctx.GetDXGIFactory()->CreateSwapChainForHwnd(
			ctx.GetGraphicsQueue(), hwnd,
			&swapChainDesc, nullptr, nullptr, &swapChain1
		);
		DE_CORE_ASSERT(SUCCEEDED(hr), "Failed to create DXGI swap chain for D3D12.");

		// Disable Alt+Enter fullscreen
		ctx.GetDXGIFactory()->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);

		hr = swapChain1->QueryInterface(IID_PPV_ARGS(&m_SwapChain));
		swapChain1->Release();
		DE_CORE_ASSERT(SUCCEEDED(hr), "Failed to query IDXGISwapChain3.");

		m_CurrentBackBufferIndex = m_SwapChain->GetCurrentBackBufferIndex();

		// Acquire each back buffer and wrap it in an NVRHI texture handle
		for (uint32_t i = 0; i < k_BufferCount; ++i)
		{
			hr = m_SwapChain->GetBuffer(i, IID_PPV_ARGS(&m_BackBuffers[i].resource));
			DE_CORE_ASSERT(SUCCEEDED(hr), "Failed to get D3D12 swap chain buffer.");

			nvrhi::TextureDesc textureDesc;
			textureDesc.width = m_Params.Width;
			textureDesc.height = m_Params.Height;
			textureDesc.format = nvrhi::Format::RGBA8_UNORM;
			textureDesc.initialState = nvrhi::ResourceStates::Present;
			textureDesc.keepInitialState = true;
			textureDesc.isRenderTarget = true;
			textureDesc.debugName = "Swap chain image";

			m_BackBuffers[i].rhiHandle = ctx.GetDeviceHandle()->createHandleForNativeTexture(
				nvrhi::ObjectTypes::D3D12_Resource,
				nvrhi::Object(m_BackBuffers[i].resource),
				textureDesc
			);
		}
	}

	void DirectX12SwapChain::CreateFramebuffers()
	{
		m_SwapChainFramebuffers.resize(k_BufferCount);
		for (uint32_t i = 0; i < k_BufferCount; ++i)
		{
			FramebufferParams framebufferParams = FramebufferParams()
				.SetWidth(m_Params.Width)
				.SetHeight(m_Params.Height);

			m_SwapChainFramebuffers[i] = NvrhiSwapChainFramebuffer::Create(m_BackBuffers[i].rhiHandle, framebufferParams);
			m_SwapChainFramebuffers[i]->Initialize();
		}
	}

	void DirectX12SwapChain::DestroyBuffersAndFramebuffers()
	{
		for (auto& fb : m_SwapChainFramebuffers)
		{
			if (fb) { fb->Destroy(); delete fb; }
		}
		m_SwapChainFramebuffers.clear();

		for (auto& buf : m_BackBuffers)
		{
			buf.rhiHandle = nullptr;
			if (buf.resource) { buf.resource->Release(); buf.resource = nullptr; }
		}
	}

}
