#pragma once

#include <vulkan/vulkan.h>

namespace DingoEngine
{

	class VulkanPhysicalDevice
	{
	public:
		struct QueueFamilyIndices
		{
			std::optional<uint32_t> GraphicsFamilyIndex;

			bool IsComplete() const
			{
				return GraphicsFamilyIndex.has_value();
			}
		};

	public:
		static VulkanPhysicalDevice* Pick(VkInstance vkInstance);

	public:
		VulkanPhysicalDevice(VkPhysicalDevice physicalDevice);
		~VulkanPhysicalDevice() = default;

	public:
		QueueFamilyIndices GetQueueFamilyIndices();

	private:
		static VkPhysicalDevice PickSuitablePhysicalDevice(VkInstance vkInstance);
		static bool IsDeviceSuitable(VkPhysicalDevice device);
		static QueueFamilyIndices FindQueueFamilyIndices(VkPhysicalDevice physicalDevice);

	private:
		VkPhysicalDevice m_VkPhysicalDevice = VK_NULL_HANDLE;
		std::optional<QueueFamilyIndices> m_QueueFamilyIndices;

		friend class VulkanGraphicsContext;
		friend class VulkanDevice;
		
	};

}
