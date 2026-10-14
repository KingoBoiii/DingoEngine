#pragma once
#include "DingoEngine/Graphics/CommandList.h"
#include "DingoEngine/Graphics/Renderer.h"

namespace Dingo::Internal
{

	// The engine's side of Renderer: the frame, the swap chain and the frame command list, which
	// the Application, ImGui and the backend classes drive and a game never touches.
	struct RendererBackend
	{
		static void Initialize(SwapChain* swapChain) { Renderer::Initialize(swapChain); }
		static void Shutdown() { Renderer::Shutdown(); }
		static void Destroy() { Renderer::Destroy(); }

		static void BeginFrame() { Renderer::BeginFrame(); }
		static void EndFrame() { Renderer::EndFrame(); }
		static void SkipFrame() { Renderer::SkipFrame(); }

		static void QueueResize(int32_t width, int32_t height) { Renderer::QueueResize(width, height); }
		static void QueueVSync(bool vsync) { Renderer::QueueVSync(vsync); }

		static CommandList* GetCommandList() { return Renderer::GetCommandList(); }
		static CommandList* TryGetRecordingCommandList() { return Renderer::TryGetRecordingCommandList(); }
		static bool IsSwapChainFramebuffer(const Framebuffer* framebuffer) { return Renderer::IsSwapChainFramebuffer(framebuffer); }
		static uint64_t GetSwapChainResizeGeneration() { return Renderer::GetSwapChainResizeGeneration(); }
		static SwapChain* GetSwapChain();
	};

}
