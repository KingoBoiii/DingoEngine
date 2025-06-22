#include "depch.h"
#include "GraphicsContext.h"

#include "Vulkan/VulkanGraphicsContext.h"

namespace DingoEngine
{

	GraphicsContext* GraphicsContext::Create(GraphicsAPI graphicsAPI, GLFWwindow* nativeWindowHandle)
	{
		switch (graphicsAPI)
		{
			case DingoEngine::GraphicsAPI::Vulkan: return new VulkanGraphicsContext(nativeWindowHandle);
			default: break;
		}
		return nullptr;
	}

	GraphicsContext::GraphicsContext(GraphicsAPI graphicsAPI, GLFWwindow* nativeWindowHandle)
		: m_GraphicsAPI(graphicsAPI), m_NativeWindowHandle(nativeWindowHandle)
	{
		s_Instance = this;
	}

	GraphicsContext::~GraphicsContext()
	{
		s_Instance = nullptr;
	}

	void GraphicsContext::RenderStatic() const
	{
	}

}
