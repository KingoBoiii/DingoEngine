#pragma once
#include "DingoEngine/Graphics/NVRHI/NvrhiFramebuffer.h"

#include <vulkan/vulkan.hpp>
#include <nvrhi/vulkan.h>

namespace Dingo
{

	class VulkanFramebuffer : public NvrhiFramebuffer
	{
	public:
		static VulkanFramebuffer* Create(nvrhi::ITexture* texture, const FramebufferParams& params);

	public:
		VulkanFramebuffer(nvrhi::ITexture* texture, const FramebufferParams& params)
			: NvrhiFramebuffer(params), m_Texture(texture)
		{}
		virtual ~VulkanFramebuffer() = default;

	public:
		virtual void Initialize() override;

	private:
		vk::RenderPass CreateRenderPass();

	private:
		nvrhi::ITexture* m_Texture = nullptr;
	};

}

