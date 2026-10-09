#pragma once
#include "Framebuffer.h"

struct GLFWwindow;

namespace vk
{
	class SurfaceKHR;
}

namespace Dingo
{

	struct SwapChainParams
	{
		GLFWwindow* NativeWindowHandle;
		int32_t Width;
		int32_t Height;
		bool VSync = true;

		vk::SurfaceKHR* VulkanSurface = nullptr; // Used for Vulkan (ImGui multiple viewports)

		SwapChainParams SetNativeWindowHandle(GLFWwindow* handle)
		{
			NativeWindowHandle = handle;
			return *this;
		}

		SwapChainParams SetWidth(int32_t width)
		{
			Width = width;
			return *this;
		}

		SwapChainParams SetHeight(int32_t height)
		{
			Height = height;
			return *this;
		}

		SwapChainParams SetVSync(bool vsync)
		{
			VSync = vsync;
			return *this;
		}
	};

	class SwapChain
	{
	public:
		static SwapChain* Create(const SwapChainParams& params = {});

	public:
		SwapChain(const SwapChainParams& params = {});
		virtual ~SwapChain() = default;

	public:
		virtual void Initialize() = 0;
		virtual void Destroy() = 0;
		virtual void Resize(int32_t width, int32_t height) = 0;

		virtual void AcquireNextImage() = 0;
		// False after an acquire that got no image (a minimized window on Vulkan), when
		// GetCurrentFramebuffer() is a stale image and Present() skips the frame.
		virtual bool IsImageAcquired() const { return true; }
		// Orders the next command-list execution after the image AcquireNextImage acquired; call it
		// just before executing the commands that draw into GetCurrentFramebuffer(). On Vulkan the
		// wait joins whatever is submitted next, so queued any earlier, an unrelated upload takes it.
		virtual void QueueImageWait() {}
		virtual void Present() = 0;

		// Only where a resize may run: the render thread after Present, or the main thread while
		// that thread is parked. Renderer::QueueVSync gets there from anywhere. D3D reads the flag
		// on every Present; Vulkan bakes it into the present mode, so it recreates the swap chain.
		virtual void SetVSync(bool vsync) { m_Params.VSync = vsync; }
		// Renderer only: stores the flag without recreating anything, for a Resize that follows at
		// once. Read the setting through Window::IsVSync(); this flag belongs to the render thread.
		void SetVSyncFlag(bool vsync) { m_Params.VSync = vsync; }

		Framebuffer* GetFramebuffer(uint32_t index) const;
		virtual Framebuffer* GetCurrentFramebuffer() const = 0;
		virtual uint32_t GetCurrentBackBufferIndex() const = 0;

		// Whether this swap chain currently owns `framebuffer`. Pointer identity only —
		// safe to ask with a pointer that may already have been freed by a resize.
		bool OwnsFramebuffer(const Framebuffer* framebuffer) const;

		// Bumped every time the swap chain (and thus its framebuffers) is recreated.
		// Anything caching objects derived from a swap-chain framebuffer must compare
		// generations instead of holding references: on D3D11/D3D12, a stale reference
		// to a back buffer makes ResizeBuffers fail.
		uint64_t GetResizeGeneration() const { return m_ResizeGeneration; }

	protected:
		SwapChainParams m_Params;
		std::vector<Framebuffer*> m_SwapChainFramebuffers;
		uint64_t m_ResizeGeneration = 0;
	};

}
