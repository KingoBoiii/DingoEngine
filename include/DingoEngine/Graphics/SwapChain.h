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
		virtual void Present() = 0;

		Framebuffer* GetFramebuffer(uint32_t index) const;
		virtual Framebuffer* GetCurrentFramebuffer() const = 0;
		virtual uint32_t GetCurrentBackBufferIndex() const = 0;

	protected:
		SwapChainParams m_Params;
		std::vector<Framebuffer*> m_SwapChainFramebuffers;
	};

}
