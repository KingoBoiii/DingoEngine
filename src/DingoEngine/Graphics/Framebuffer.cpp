#include "depch.h"
#include "DingoEngine/Graphics/Framebuffer.h"

#include "DingoEngine/Graphics/NVRHI/NvrhiFramebuffer.h"

namespace Dingo
{

	Framebuffer* Framebuffer::Create(const FramebufferParams& params)
	{
		return new NvrhiFramebuffer(params);
	}

	Framebuffer::Framebuffer(const FramebufferParams& params)
		: m_Params(params)
	{}

}
