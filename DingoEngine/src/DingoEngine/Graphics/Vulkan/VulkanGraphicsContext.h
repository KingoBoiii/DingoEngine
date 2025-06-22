#pragma once
#include "DingoEngine/Graphics/GraphicsContext.h"
#include "VulkanPhysicalDevice.h"
#include "VulkanDevice.h"

#include <nvrhi/vulkan.h>
#include <nvrhi/validation.h>
#include <vulkan/vulkan.hpp>

#define USE_NVRHI_VULKAN

namespace DingoEngine
{

	struct QueueFamilyIndices
	{
		int32_t Graphics = -1;
		int32_t Compute = -1;
		int32_t Transfer = -1;
		int32_t Present = -1;
	};

	class VulkanGraphicsContext : public GraphicsContext
	{
	public:
		VulkanGraphicsContext(GLFWwindow* nativeWindowHandle);
		virtual ~VulkanGraphicsContext() = default;

	public:
		virtual void Initialize() override;
		virtual void Shutdown() override;

	private:
		bool CreateInstance();
		bool CreateDebugMessenger();
		void CreateWindowSurface();
		bool PickPhysicalDevice();
		bool FindQueueFamilies(vk::PhysicalDevice physicalDevice);
		bool CreateDevice();

#if 0
	private:
		bool CheckValidationLayerSupport() const;
		std::vector<const char*> GetRequiredExtensions() const;
#endif

	private:
		std::string m_RendererString;

		vk::detail::DynamicLoader m_DynamicLoader;

		vk::Instance m_VulkanInstance;
		vk::DebugReportCallbackEXT m_DebugReportCallback;
		vk::PhysicalDevice m_VulkanPhysicalDevice;
		QueueFamilyIndices m_QueueFamilyIndices;
		vk::Device m_VulkanDevice;
		vk::Queue m_GraphicsQueue;
		vk::Queue m_ComputeQueue;
		vk::Queue m_TransferQueue;
		vk::Queue m_PresentQueue;
		vk::SurfaceKHR* m_WindowSurface = nullptr;

		struct VulkanExtensionSet
		{
			std::unordered_set<std::string> instance;
			std::unordered_set<std::string> layers;
			std::unordered_set<std::string> device;
		};

		// minimal set of required extensions
		VulkanExtensionSet enabledExtensions = {
			// instance
			{
				VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME
			},
			// layers
			{ },
			// device
			{
				VK_KHR_MAINTENANCE1_EXTENSION_NAME
			},
		};

		// optional extensions
		VulkanExtensionSet optionalExtensions = {
			// instance
			{
				VK_EXT_SAMPLER_FILTER_MINMAX_EXTENSION_NAME,
				VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
				VK_EXT_DEBUG_UTILS_EXTENSION_NAME
			},
			// layers
			{ },
			// device
			{
				VK_EXT_DEBUG_MARKER_EXTENSION_NAME,
				VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
				VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
				VK_NV_MESH_SHADER_EXTENSION_NAME,
				VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME,
				VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
				VK_KHR_MAINTENANCE_4_EXTENSION_NAME,
				VK_KHR_SWAPCHAIN_MUTABLE_FORMAT_EXTENSION_NAME
			},
		};


		friend class VulkanSwapChain;
	};

}
