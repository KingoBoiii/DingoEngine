#pragma once
#include "DingoEngine/Graphics/Framebuffer.h"

namespace Dingo
{

	class VulkanFramebuffer : public Framebuffer
	{
	public:
		VulkanFramebuffer(const FramebufferParams& params);
		virtual ~VulkanFramebuffer() = default;
	public:
		virtual void Initialize() override;
	};

}
