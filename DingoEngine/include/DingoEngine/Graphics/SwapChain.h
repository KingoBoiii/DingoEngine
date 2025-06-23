#pragma once
#include "Framebuffer.h"

struct GLFWwindow;

namespace DingoEngine
{

	struct SwapChainOptions
	{
		GLFWwindow* NativeWindowHandle;
		int32_t Width;
		int32_t Height;
	};

	class SwapChain
	{
	public:
		static SwapChain* Create(const SwapChainOptions& options = {});

	public:
		SwapChain(const SwapChainOptions& options = {});
		virtual ~SwapChain() = default;

	public:
		virtual void Initialize() = 0;
		virtual void Destroy() = 0;

		virtual void BeginFrame() = 0;
		virtual void Present() = 0;

		Framebuffer* GetFramebuffer(uint32_t index);

	protected:
		SwapChainOptions m_Options;
		std::vector<Framebuffer*> m_SwapChainFramebuffers;
	};

}
