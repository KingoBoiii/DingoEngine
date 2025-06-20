#pragma once
#include "DingoEngine/Graphics/GraphicsContext.h"
#include "VulkanPhysicalDevice.h"
#include "VulkanDevice.h"

#include <vulkan/vulkan.h>

namespace DingoEngine
{

	class VulkanGraphicsContext : public GraphicsContext
	{
	public:
		VulkanGraphicsContext() : GraphicsContext(GraphicsAPI::Vulkan) {}
		virtual ~VulkanGraphicsContext() = default;
	protected:
		virtual nvrhi::DeviceHandle CreateDeviceHandle() override;
		virtual void DestroyDeviceHandle(nvrhi::DeviceHandle deviceHandle) override;
	private:
		void CreateVkInstance();
		void SetupDebugMessenger();

	private:
		bool CheckValidationLayerSupport() const;
		std::vector<const char*> GetRequiredExtensions() const;
	private:
		VkInstance m_Instance = VK_NULL_HANDLE;
		VkDebugUtilsMessengerEXT m_DebugMessenger = VK_NULL_HANDLE;
		VulkanPhysicalDevice* m_PhysicalDevice = nullptr;
		VulkanDevice* m_Device = nullptr;
	};

}
