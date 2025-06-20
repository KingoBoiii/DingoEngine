#include "depch.h"
#include "VulkanGraphicsContext.h"
#include "VulkanCommon.h"

#include <glfw/glfw3.h>

#include <stdexcept>
#include <iostream>
#include <string>

#include <nvrhi/vulkan.h>
#include <vulkan/vulkan.hpp>

#if VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1
namespace vk
{
	namespace detail
	{
		DispatchLoaderDynamic defaultDispatchLoaderDynamic;
	}
}
#endif

namespace DingoEngine
{

	namespace Utils
	{

		VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger)
		{
			auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
			if (func != nullptr)
			{
				return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
			}
			else
			{
				return VK_ERROR_EXTENSION_NOT_PRESENT;
			}
		}

		void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator)
		{
			auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
			if (func != nullptr)
			{
				func(instance, debugMessenger, pAllocator);
			}
		}

	}

	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData)
	{

		std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

		return VK_FALSE;
	}

	void VulkanGraphicsContext::Initialize()
	{
		CreateVkInstance();
		SetupDebugMessenger();
		m_PhysicalDevice = VulkanPhysicalDevice::Pick(m_Instance);
		m_Device = VulkanDevice::Create(m_PhysicalDevice);

		//const char* deviceExtensions[] = {
		//	"VK_KHR_acceleration_structure",
		//	"VK_KHR_deferred_host_operations",
		//	"VK_KHR_ray_tracing_pipeline",
		//	// list the extensions that were requested when the device was created
		//};
		nvrhi::vulkan::DeviceDesc deviceDesc;
		//deviceDesc.errorCB = g_MyMessageCallback;
		deviceDesc.instance = m_Instance;
		deviceDesc.physicalDevice = m_PhysicalDevice->m_VkPhysicalDevice;
		deviceDesc.graphicsQueueIndex = m_PhysicalDevice->GetQueueFamilyIndices().GraphicsFamilyIndex.value();
		deviceDesc.device = m_Device->m_VkDevice;
		deviceDesc.graphicsQueue = m_Device->m_VkGraphicsQueue;

		//deviceDesc.deviceExtensions = deviceExtensions;
		//deviceDesc.numDeviceExtensions = std::size(deviceExtensions);

		m_DeviceHandler = nvrhi::vulkan::createDevice(deviceDesc);
	}

	void VulkanGraphicsContext::Shutdown()
	{
		delete m_Device;

		delete m_PhysicalDevice;

		if (enableValidationLayers)
		{
			Utils::DestroyDebugUtilsMessengerEXT(m_Instance, m_DebugMessenger, nullptr);
		}

		vkDestroyInstance(m_Instance, nullptr);
	}

	void VulkanGraphicsContext::CreateVkInstance()
	{
		if (enableValidationLayers && !CheckValidationLayerSupport())
		{
			throw std::runtime_error("validation layers requested, but not available!");
		}

		VkApplicationInfo appInfo{};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "Hello Triangle";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "Dingo Engine";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_3;

		VkInstanceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;

		auto extensions = GetRequiredExtensions();
		createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
		createInfo.ppEnabledExtensionNames = extensions.data();

		if (enableValidationLayers)
		{
			createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
			createInfo.ppEnabledLayerNames = validationLayers.data();
		}
		else
		{
			createInfo.enabledLayerCount = 0;
		}

		VkResult result = vkCreateInstance(&createInfo, nullptr, &m_Instance);
		if (result != VK_SUCCESS)
		{
			__debugbreak();
		}

#if VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1
		vk::detail::defaultDispatchLoaderDynamic.init(m_Instance, vkGetInstanceProcAddr);
#endif
	}

	void VulkanGraphicsContext::SetupDebugMessenger()
	{
		if (!enableValidationLayers)
		{
			return;
		}

		VkDebugUtilsMessengerCreateInfoEXT createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		createInfo.pfnUserCallback = debugCallback;
		createInfo.pUserData = nullptr; // Optional

		if (Utils::CreateDebugUtilsMessengerEXT(m_Instance, &createInfo, nullptr, &m_DebugMessenger) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to set up debug messenger!");
		}
	}

	bool VulkanGraphicsContext::CheckValidationLayerSupport() const
	{
		uint32_t layerCount;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

		std::vector<VkLayerProperties> availableLayers(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

		for (const char* layerName : validationLayers)
		{
			bool layerFound = false;

			for (const auto& layerProperties : availableLayers)
			{
				if (strcmp(layerName, layerProperties.layerName) == 0)
				{
					layerFound = true;
					break;
				}
			}

			if (!layerFound)
			{
				return false;
			}
		}

		return true;
	}

	std::vector<const char*> VulkanGraphicsContext::GetRequiredExtensions() const
	{
		uint32_t glfwExtensionCount = 0;
		const char** glfwExtensions;
		glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

		if (enableValidationLayers)
		{
			extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		}

		return extensions;
	}

}
