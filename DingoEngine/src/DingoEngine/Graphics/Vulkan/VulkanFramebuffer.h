#pragma once
#include "DingoEngine/Graphics/Framebuffer.h"

namespace DingoEngine
{

	class VulkanFramebuffer : public Framebuffer
	{
	public:
		VulkanFramebuffer(nvrhi::ITexture* texture);
		virtual ~VulkanFramebuffer() = default;
	public:
		virtual void Initialize() override;
	};

}
