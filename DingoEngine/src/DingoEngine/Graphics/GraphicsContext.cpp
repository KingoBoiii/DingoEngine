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

	void GraphicsContext::CreateSwapChain()
	{
		//auto textureDesc = nvrhi::TextureDesc()
		//	.setDimension(nvrhi::TextureDimension::Texture2D)
		//	.setFormat(nvrhi::Format::RGBA8_UNORM)
		//	.setWidth(swapChainWidth)
		//	.setHeight(swapChainHeight)
		//	.setIsRenderTarget(true)
		//	.setDebugName("Swap Chain Image");

		//nvrhi::Object* swapChainTexture;
		//m_DeviceHandler->createHandleForNativeTexture(nvrhi::ObjectTypes::VK_Image, swapChainTexture, textureDesc);
	}

}
