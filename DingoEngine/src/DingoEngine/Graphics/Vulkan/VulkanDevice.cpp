#include "depch.h"
#include "VulkanDevice.h"
#include "VulkanCommon.h"

#include <stdexcept>

namespace DingoEngine
{

	VulkanDevice* VulkanDevice::Create(VulkanPhysicalDevice* physicalDevice)
	{
		VulkanPhysicalDevice::QueueFamilyIndices indices = physicalDevice->GetQueueFamilyIndices();

		VkDevice device = CreateVkDevice(physicalDevice, indices);

		VkQueue graphicsQueue;
		vkGetDeviceQueue(device, indices.GraphicsFamilyIndex.value(), 0, &graphicsQueue);

		return new VulkanDevice(device, graphicsQueue);
	}

	VulkanDevice::VulkanDevice(VkDevice device, VkQueue graphicsQueue)
		: m_VkDevice(device), m_VkGraphicsQueue(graphicsQueue)
	{}

	VulkanDevice::~VulkanDevice()
	{
		vkDestroyDevice(m_VkDevice, VK_NULL_HANDLE);
	}

	VkDevice VulkanDevice::CreateVkDevice(VulkanPhysicalDevice* physicalDevice, VulkanPhysicalDevice::QueueFamilyIndices indices)
	{
		VkDeviceQueueCreateInfo queueCreateInfo{};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = indices.GraphicsFamilyIndex.value();
		queueCreateInfo.queueCount = 1;

		float queuePriority = 1.0f;
		queueCreateInfo.pQueuePriorities = &queuePriority;

		VkPhysicalDeviceFeatures deviceFeatures{};

		VkDeviceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		createInfo.pQueueCreateInfos = &queueCreateInfo;
		createInfo.queueCreateInfoCount = 1;

		createInfo.enabledExtensionCount = 0;

		if (enableValidationLayers)
		{
			createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
			createInfo.ppEnabledLayerNames = validationLayers.data();
		}
		else
		{
			createInfo.enabledLayerCount = 0;
		}
		createInfo.pEnabledFeatures = &deviceFeatures;

		VkDevice device = VK_NULL_HANDLE;
		if (vkCreateDevice(physicalDevice->m_VkPhysicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create logical device!");
		}

		return device;
	}

}
