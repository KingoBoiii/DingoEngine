#include "depch.h"
#include "DingoEngine/Graphics/Framebuffer.h"

#include "DingoEngine/Graphics/NVRHI/NvrhiFramebuffer.h"

#include <atomic>

namespace Dingo
{

	Framebuffer* Framebuffer::Create(const FramebufferParams& params)
	{
		Framebuffer* framebuffer = new NvrhiFramebuffer(params);
		framebuffer->Initialize();
		return framebuffer;
	}

	Framebuffer::Framebuffer(const FramebufferParams& params)
		: m_Params(params)
	{}

	uint64_t Framebuffer::AllocateId()
	{
		static std::atomic<uint64_t> s_NextId{ 1 };
		return s_NextId.fetch_add(1, std::memory_order_relaxed);
	}

}
