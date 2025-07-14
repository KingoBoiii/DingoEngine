#include "depch.h"
#include "DingoEngine/Graphics/GraphicsContext.h"

#include "Vulkan/VulkanGraphicsContext.h"

namespace Dingo
{

	GraphicsContext* GraphicsContext::Create(GraphicsAPI graphicsAPI)
	{
		switch (graphicsAPI)
		{
			case Dingo::GraphicsAPI::Vulkan: return new VulkanGraphicsContext();
			default: break;
		}
		return nullptr;
	}

	GraphicsContext::GraphicsContext(GraphicsAPI graphicsAPI)
		: m_GraphicsAPI(graphicsAPI)
	{
		s_Instance = this;
	}

	GraphicsContext::~GraphicsContext()
	{
		s_Instance = nullptr;
	}

}
