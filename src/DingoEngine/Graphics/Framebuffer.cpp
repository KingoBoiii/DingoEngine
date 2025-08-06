#include "depch.h"
#include "DingoEngine/Graphics/Framebuffer.h"

#include "DingoEngine/Graphics/NVRHI/NvrhiFramebuffer.h"

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

}
