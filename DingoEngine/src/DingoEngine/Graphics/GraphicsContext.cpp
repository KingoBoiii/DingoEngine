#include "depch.h"
#include "DingoEngine/Graphics/GraphicsContext.h"

#include "Vulkan/VulkanGraphicsContext.h"

namespace Dingo
{

	GraphicsContext* GraphicsContext::Create(const GraphicsParams& params)
	{
		switch (params.GraphicsAPI)
		{
			case GraphicsAPI::Vulkan: return new VulkanGraphicsContext(params);
			default: break;
		}
		return nullptr;
	}

	GraphicsContext::GraphicsContext(const GraphicsParams& params)
		: m_Params(params)
	{
		s_Instance = this;
	}

	GraphicsContext::~GraphicsContext()
	{
		s_Instance = nullptr;
	}

}
