#include "depch.h"
#include "VulkanPhysicalDevice.h"

#include <stdexcept>

namespace DingoEngine
{

	VulkanPhysicalDevice* VulkanPhysicalDevice::Pick(VkInstance vkInstance)
	{
		VkPhysicalDevice physicalDevice = PickSuitablePhysicalDevice(vkInstance);

		return new VulkanPhysicalDevice(physicalDevice);
	}

	VulkanPhysicalDevice::VulkanPhysicalDevice(VkPhysicalDevice physicalDevice)
		: m_VkPhysicalDevice(physicalDevice)
	{}

	VulkanPhysicalDevice::QueueFamilyIndices VulkanPhysicalDevice::GetQueueFamilyIndices() 
	{
		return m_QueueFamilyIndices.value_or(FindQueueFamilyIndices(m_VkPhysicalDevice));
	}

	VkPhysicalDevice VulkanPhysicalDevice::PickSuitablePhysicalDevice(VkInstance vkInstance)
	{
		VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;

		uint32_t deviceCount = 0;
		vkEnumeratePhysicalDevices(vkInstance, &deviceCount, VK_NULL_HANDLE);
		if (deviceCount == 0)
		{
			throw std::runtime_error("failed to find GPUs with Vulkan support!");
		}

		std::vector<VkPhysicalDevice> physicalDevices(deviceCount);
		vkEnumeratePhysicalDevices(vkInstance, &deviceCount, physicalDevices.data());

		for (const auto& device : physicalDevices)
		{
			if (IsDeviceSuitable(device))
			{
				physicalDevice = device;
				break;
			}
		}

		if (physicalDevice == VK_NULL_HANDLE)
		{
			throw std::runtime_error("failed to find a suitable GPU!");
		}

		return physicalDevice;
	}

	bool VulkanPhysicalDevice::IsDeviceSuitable(VkPhysicalDevice device)
	{
		VkPhysicalDeviceProperties deviceProperties;
		VkPhysicalDeviceFeatures deviceFeatures;
		vkGetPhysicalDeviceProperties(device, &deviceProperties);
		vkGetPhysicalDeviceFeatures(device, &deviceFeatures);
		QueueFamilyIndices indices = FindQueueFamilyIndices(device);

		return deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU && indices.IsComplete();
	}

	VulkanPhysicalDevice::QueueFamilyIndices VulkanPhysicalDevice::FindQueueFamilyIndices(VkPhysicalDevice physicalDevice)
	{
		QueueFamilyIndices indices;

		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);

		std::vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilyProperties.data());

		int i = 0;
		for (const auto& queueFamily : queueFamilyProperties)
		{
			if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
			{
				indices.GraphicsFamilyIndex = i;
			}

			if (indices.IsComplete())
			{
				break;
			}

			i++;
		}

		return indices;
	}

}
