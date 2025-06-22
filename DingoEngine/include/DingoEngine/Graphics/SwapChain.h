#pragma once

namespace DingoEngine
{

	struct SwapChainOptions
	{
		void* NativeWindowHandle;
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

	protected:
		SwapChainOptions m_Options;
	};

}
