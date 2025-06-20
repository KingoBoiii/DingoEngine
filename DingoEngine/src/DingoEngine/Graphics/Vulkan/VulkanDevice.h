#pragma once
#include "VulkanPhysicalDevice.h"

#include <vulkan/vulkan.h>

namespace DingoEngine
{

	class VulkanDevice
	{
	public:
		static VulkanDevice* Create(VulkanPhysicalDevice* physicalDevice);

	public:
		VulkanDevice(VkDevice device, VkQueue graphicsQueue);
		~VulkanDevice();

	private:
		static VkDevice CreateVkDevice(VulkanPhysicalDevice* physicalDevice, VulkanPhysicalDevice::QueueFamilyIndices indices);

	private:
		VkDevice m_VkDevice = VK_NULL_HANDLE;
		VkQueue m_VkGraphicsQueue = VK_NULL_HANDLE;

		friend class VulkanGraphicsContext;
	};

}
