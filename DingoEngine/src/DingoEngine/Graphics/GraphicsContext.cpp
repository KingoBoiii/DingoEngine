#include "depch.h"
#include "GraphicsContext.h"

#include "Vulkan/VulkanGraphicsContext.h"

namespace DingoEngine
{

	GraphicsContext* GraphicsContext::Create(GraphicsAPI graphicsAPI)
	{
		switch (graphicsAPI)
		{
			case DingoEngine::GraphicsAPI::Vulkan: return new VulkanGraphicsContext();
			default: break;
		}
		return nullptr;
	}

	GraphicsContext::GraphicsContext(GraphicsAPI graphicsAPI)
		: m_GraphicsAPI(graphicsAPI)
	{}

	void GraphicsContext::Initialize()
	{
		m_DeviceHandle = CreateDeviceHandle();
	}

	void GraphicsContext::Shutdown()
	{
		DestroyDeviceHandle(m_DeviceHandle);
	}

}
