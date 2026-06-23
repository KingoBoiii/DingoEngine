#include "depch.h"
#include "DirectX11SwapChain.h"

#include <glfw/glfw3.h>
#include <GLFW/glfw3native.h>

#include "DirectX11GraphicsContext.h"
#include "DingoEngine/Graphics/GraphicsContext.h"
#include "DingoEngine/Graphics/NVRHI/NvrhiGraphicsContext.h"
#include "DingoEngine/Graphics/NVRHI/NvrhiSwapChainFramebuffer.h"

namespace Dingo
{

	DirectX11SwapChain::DirectX11SwapChain(const SwapChainParams& params)
		: SwapChain(params)
	{}

	DirectX11SwapChain::~DirectX11SwapChain()
	{}

	void DirectX11SwapChain::Initialize()
	{
		CreateSwapChainAndBuffer();
		CreateFramebuffer();
	}

	void DirectX11SwapChain::Destroy()
	{
		DestroyBufferAndFramebuffer();
		if (m_SwapChain) { m_SwapChain->Release(); m_SwapChain = nullptr; }
	}

	void DirectX11SwapChain::Resize(int32_t width, int32_t height)
	{
		if (width == 0 || height == 0)
		{
			DE_CORE_WARN("Swap chain resize called with zero dimensions, ignoring.");
			return;
		}

		m_Params.Width = width;
		m_Params.Height = height;

		DestroyBufferAndFramebuffer();

		HRESULT hr = m_SwapChain->ResizeBuffers(2, (UINT)width, (UINT)height, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
		DE_CORE_ASSERT(SUCCEEDED(hr), "Failed to resize D3D11 swap chain buffers.");

		CreateFramebuffer();
	}

	void DirectX11SwapChain::AcquireNextImage()
	{
		// D3D11 flip-model swap chains expose buffer 0 as the current back buffer always
	}

	void DirectX11SwapChain::Present()
	{
		UINT syncInterval = m_Params.VSync ? 1 : 0;
		HRESULT hr = m_SwapChain->Present(syncInterval, 0);
		DE_CORE_ASSERT(SUCCEEDED(hr), "D3D11 swap chain Present failed.");
	}

	void DirectX11SwapChain::CreateSwapChainAndBuffer()
	{
		auto& ctx = GraphicsContext::Get().As<DirectX11GraphicsContext>();

		HWND hwnd = glfwGetWin32Window(m_Params.NativeWindowHandle);
		DE_CORE_ASSERT(hwnd, "Failed to get Win32 window handle from GLFW.");

		DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
		swapChainDesc.Width = (UINT)m_Params.Width;
		swapChainDesc.Height = (UINT)m_Params.Height;
		swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swapChainDesc.SampleDesc.Count = 1;
		swapChainDesc.SampleDesc.Quality = 0;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.BufferCount = 2;
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swapChainDesc.Flags = 0;

		HRESULT hr = ctx.GetDXGIFactory()->CreateSwapChainForHwnd(
			ctx.GetD3D11Device(), hwnd,
			&swapChainDesc, nullptr, nullptr, &m_SwapChain
		);
		DE_CORE_ASSERT(SUCCEEDED(hr), "Failed to create DXGI swap chain for D3D11.");

		// Disable Alt+Enter fullscreen
		ctx.GetDXGIFactory()->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);

		hr = m_SwapChain->GetBuffer(0, IID_PPV_ARGS(&m_BackBuffer));
		DE_CORE_ASSERT(SUCCEEDED(hr), "Failed to get D3D11 swap chain back buffer.");

		nvrhi::TextureDesc textureDesc;
		textureDesc.width = m_Params.Width;
		textureDesc.height = m_Params.Height;
		textureDesc.format = nvrhi::Format::RGBA8_UNORM;
		textureDesc.initialState = nvrhi::ResourceStates::Present;
		textureDesc.keepInitialState = true;
		textureDesc.isRenderTarget = true;
		textureDesc.debugName = "Swap chain image";

		m_BackBufferHandle = ctx.GetDeviceHandle()->createHandleForNativeTexture(
			nvrhi::ObjectTypes::D3D11_Resource,
			nvrhi::Object(static_cast<ID3D11Resource*>(m_BackBuffer)),
			textureDesc
		);
	}

	void DirectX11SwapChain::CreateFramebuffer()
	{
		FramebufferParams framebufferParams = FramebufferParams()
			.SetWidth(m_Params.Width)
			.SetHeight(m_Params.Height)
			.SetEnableDepth(true); // match the Vulkan swap chain so 3D passes have depth

		m_SwapChainFramebuffers.resize(1);
		m_SwapChainFramebuffers[0] = NvrhiSwapChainFramebuffer::Create(m_BackBufferHandle, framebufferParams);
		m_SwapChainFramebuffers[0]->Initialize();
	}

	void DirectX11SwapChain::DestroyBufferAndFramebuffer()
	{
		if (!m_SwapChainFramebuffers.empty())
		{
			if (m_SwapChainFramebuffers[0])
			{
				m_SwapChainFramebuffers[0]->Destroy();
				delete m_SwapChainFramebuffers[0];
			}
			m_SwapChainFramebuffers.clear();
		}

		m_BackBufferHandle = nullptr;
		if (m_BackBuffer) { m_BackBuffer->Release(); m_BackBuffer = nullptr; }
	}

}
