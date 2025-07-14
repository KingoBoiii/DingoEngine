#include "depch.h"
#include "VulkanGraphicsContext.h"
#include "VulkanCommon.h"

#include <glfw/glfw3.h>

#include <iostream>

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace Dingo
{

	namespace Utils
	{

		static std::vector<const char*> stringSetToVector(const std::unordered_set<std::string>& set)
		{
			std::vector<const char*> ret;
			for (const auto& s : set)
			{
				ret.push_back(s.c_str());
			}

			return ret;
		}

	}

	static VKAPI_ATTR VkBool32 VKAPI_CALL vulkanDebugCallback(
			VkDebugReportFlagsEXT flags,
			VkDebugReportObjectTypeEXT objType,
			uint64_t obj,
			size_t location,
			int32_t code,
			const char* layerPrefix,
			const char* msg,
			void* userData)
	{
		switch (flags)
		{
			case VK_DEBUG_REPORT_ERROR_BIT_EXT:
				DE_CORE_ERROR("[Vulkan: location=0x{0} code={1} layerPrefix='{2}'] {3}", location, code, layerPrefix, msg);
				break;

			case VK_DEBUG_REPORT_WARNING_BIT_EXT:
			case VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT:
				DE_CORE_WARN("[Vulkan: location=0x{0} code={1} layerPrefix='{2}'] {3}", location, code, layerPrefix, msg);
				break;

			case VK_DEBUG_REPORT_INFORMATION_BIT_EXT:
				DE_CORE_INFO("[Vulkan: location=0x{0} code={1} layerPrefix='{2}'] {3}", location, code, layerPrefix, msg);
				break;

			default:
				DE_CORE_TRACE("[Vulkan: location=0x{0} code={1} layerPrefix='{2}'] {3}", location, code, layerPrefix, msg);
				break;
		}

		return VK_FALSE;
	}

	VulkanGraphicsContext::VulkanGraphicsContext()
		: GraphicsContext(GraphicsAPI::Vulkan)
	{}

	void VulkanGraphicsContext::Initialize()
	{
		DE_CORE_ASSERT(glfwVulkanSupported(), "GLFW reports that Vulkan is not supported. Perhaps missing a call to glfwInit()?");

		// add any extensions required by GLFW
		uint32_t glfwExtCount;
		const char** glfwExt = glfwGetRequiredInstanceExtensions(&glfwExtCount);

		for (uint32_t i = 0; i < glfwExtCount; i++)
		{
			enabledExtensions.instance.insert(std::string(glfwExt[i]));
		}

		CreateInstance();
		CreateDebugMessenger();
		PickPhysicalDevice();
		FindQueueFamilies(m_VulkanPhysicalDevice);
		CreateDevice();
		CreateDeviceHandle();
	}

	void VulkanGraphicsContext::Shutdown()
	{
		m_RendererString.clear();

		//if (m_DeviceHandle)
		//{
		//	m_DeviceHandle->Release();
		//	m_DeviceHandle = nullptr;
		//}

		if (m_NvrhiDevice)
		{
			m_NvrhiDevice->Release();
			m_NvrhiDevice = nullptr;
		}

		if (m_VulkanDevice)
		{
			m_VulkanDevice.destroy();
			m_VulkanDevice = nullptr;
		}

		if (m_DebugReportCallback)
		{
			m_VulkanInstance.destroyDebugReportCallbackEXT(m_DebugReportCallback);
			m_DebugReportCallback = nullptr;
		}

		if (m_VulkanInstance)
		{
			m_VulkanInstance.destroy();
			m_VulkanInstance = nullptr;
		}
	}

	void VulkanGraphicsContext::CreateInstance()
	{
		//enabledExtensions.instance.insert(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		enabledExtensions.instance.insert(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
		enabledExtensions.layers.insert("VK_LAYER_KHRONOS_validation");

		//PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = m_DynamicLoader->getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
		PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = m_DynamicLoader.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
		VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);

		std::unordered_set<std::string> requiredExtensions = enabledExtensions.instance;

		// figure out which optional extensions are supported
		for (const auto& instanceExt : vk::enumerateInstanceExtensionProperties())
		{
			const std::string name = instanceExt.extensionName;
			if (optionalExtensions.instance.find(name) != optionalExtensions.instance.end())
			{
				enabledExtensions.instance.insert(name);
			}

			requiredExtensions.erase(name);
		}

		if (!requiredExtensions.empty())
		{
			std::stringstream ss;
			ss << "Cannot create a Vulkan instance because the following required extension(s) are not supported:";
			for (const auto& ext : requiredExtensions)
			{
				ss << std::endl << "  - " << ext;
			}

			std::cout << ss.str().c_str() << std::endl;
			return;
		}

		std::cout << "Enabled Vulkan instance extensions:" << std::endl;
		for (const auto& ext : enabledExtensions.instance)
		{
			std::cout << "    " << ext.c_str() << std::endl;;
		}

		std::unordered_set<std::string> requiredLayers = enabledExtensions.layers;

		for (const auto& layer : vk::enumerateInstanceLayerProperties())
		{
			const std::string name = layer.layerName;
			if (optionalExtensions.layers.find(name) != optionalExtensions.layers.end())
			{
				enabledExtensions.layers.insert(name);
			}

			requiredLayers.erase(name);
		}

		if (!requiredLayers.empty())
		{
			std::stringstream ss;
			ss << "Cannot create a Vulkan instance because the following required layer(s) are not supported:";
			for (const auto& ext : requiredLayers)
			{
				ss << std::endl << "  - " << ext;
			}

			std::cout << "" << ss.str().c_str() << std::endl;
			return;
		}

		std::cout << "Enabled Vulkan layers:" << std::endl;
		for (const auto& layer : enabledExtensions.layers)
		{
			std::cout << "    " << layer.c_str() << std::endl;
		}

		auto instanceExtVec = Utils::stringSetToVector(enabledExtensions.instance);
		auto layerVec = Utils::stringSetToVector(enabledExtensions.layers);

		// Query the Vulkan API version supported on the system to make sure we use at least 1.3 when that's present.
		uint32_t apiVersion = 0;
		const vk::Result res = vk::enumerateInstanceVersion(&apiVersion);
		if (res != vk::Result::eSuccess)
		{
			DE_CORE_ERROR("Call to vkEnumerateInstanceVersion failed, error code = {}", nvrhi::vulkan::resultToString(VkResult(res)));
			DE_CORE_ASSERT(true);
			return;
		}

		const uint32_t minimumVulkanVersion = VK_MAKE_API_VERSION(0, 1, 3, 0);

		// Check if the Vulkan API version is sufficient.
		if (apiVersion < minimumVulkanVersion)
		{
			DE_CORE_ERROR("The Vulkan API version supported on the system ({}.{}.{}) is too low, at least {}.{}.{} is required.",
				VK_API_VERSION_MAJOR(apiVersion), VK_API_VERSION_MINOR(apiVersion), VK_API_VERSION_PATCH(apiVersion),
				VK_API_VERSION_MAJOR(minimumVulkanVersion), VK_API_VERSION_MINOR(minimumVulkanVersion), VK_API_VERSION_PATCH(minimumVulkanVersion));
			DE_CORE_ASSERT(true);
		}

		vk::ApplicationInfo applicationInfo = vk::ApplicationInfo()
			.setPApplicationName("Hello Triangle")
			.setApplicationVersion(VK_MAKE_VERSION(1, 0, 0))
			.setPEngineName("Dingo Engine")
			.setApiVersion(VK_API_VERSION_1_3);

		vk::InstanceCreateInfo instanceCreateInfo = vk::InstanceCreateInfo()
			.setEnabledLayerCount(uint32_t(layerVec.size()))
			.setPpEnabledLayerNames(layerVec.data())
			.setEnabledExtensionCount(uint32_t(instanceExtVec.size()))
			.setPpEnabledExtensionNames(instanceExtVec.data())
			.setPApplicationInfo(&applicationInfo);

		const vk::Result result = vk::createInstance(&instanceCreateInfo, nullptr, &m_VulkanInstance);
		DE_CORE_ASSERT(result == vk::Result::eSuccess, "Failed to create Vulkan instance.");

		VULKAN_HPP_DEFAULT_DISPATCHER.init(m_VulkanInstance);
	}

	void VulkanGraphicsContext::CreateDebugMessenger()
	{
		vk::DebugReportCallbackCreateInfoEXT debugReportCallbackCreateInfo = vk::DebugReportCallbackCreateInfoEXT()
			.setFlags(vk::DebugReportFlagBitsEXT::eError |
					  vk::DebugReportFlagBitsEXT::eWarning |
					  vk::DebugReportFlagBitsEXT::eInformation |
					  vk::DebugReportFlagBitsEXT::ePerformanceWarning)
			.setPfnCallback(vulkanDebugCallback)
			.setPUserData(nullptr);

		const vk::Result result = m_VulkanInstance.createDebugReportCallbackEXT(&debugReportCallbackCreateInfo, nullptr, &m_DebugReportCallback);
		DE_CORE_ASSERT(result == vk::Result::eSuccess, "Failed to create debug report callback.");
	}

	bool VulkanGraphicsContext::PickPhysicalDevice()
	{
		std::vector<vk::PhysicalDevice> physicalDevices = m_VulkanInstance.enumeratePhysicalDevices();

		int firstDevice = 0;
		int lastDevice = int(physicalDevices.size()) - 1;

		// Start building an error message in case we cannot find a device.
		std::stringstream errorStream;
		errorStream << "Cannot find a Vulkan device that supports all the required extensions and properties.";

		// build a list of GPUs
		std::vector<vk::PhysicalDevice> discreteGPUs;
		std::vector<vk::PhysicalDevice> otherGPUs;

		for (int deviceIndex = firstDevice; deviceIndex <= lastDevice; ++deviceIndex)
		{
			vk::PhysicalDevice const& dev = physicalDevices[deviceIndex];
			vk::PhysicalDeviceProperties prop = dev.getProperties();

			errorStream << std::endl << prop.deviceName.data() << ":";

			// check that all required device extensions are present
			std::unordered_set<std::string> requiredExtensions = enabledExtensions.device;
			auto deviceExtensions = dev.enumerateDeviceExtensionProperties();
			for (const auto& ext : deviceExtensions)
			{
				requiredExtensions.erase(std::string(ext.extensionName.data()));
			}

			bool deviceIsGood = true;

			if (!requiredExtensions.empty())
			{
				// device is missing one or more required extensions
				for (const auto& ext : requiredExtensions)
				{
					errorStream << std::endl << "  - missing " << ext;
				}
				deviceIsGood = false;
			}

			auto deviceFeatures = dev.getFeatures();
			if (!deviceFeatures.samplerAnisotropy)
			{
				// device is a toaster oven
				errorStream << std::endl << "  - does not support samplerAnisotropy";
				deviceIsGood = false;
			}
			if (!deviceFeatures.textureCompressionBC)
			{
				errorStream << std::endl << "  - does not support textureCompressionBC";
				deviceIsGood = false;
			}

			if (!FindQueueFamilies(dev))
			{
				// device doesn't have all the queue families we need
				errorStream << std::endl << "  - does not support the necessary queue types";
				deviceIsGood = false;
			}


			if (!deviceIsGood)
				continue;

			if (prop.deviceType == vk::PhysicalDeviceType::eDiscreteGpu)
			{
				discreteGPUs.push_back(dev);
			}
			else
			{
				otherGPUs.push_back(dev);
			}
		}

		// pick the first discrete GPU if it exists, otherwise the first integrated GPU
		if (!discreteGPUs.empty())
		{
			m_VulkanPhysicalDevice = discreteGPUs[0];
			return true;
		}

		if (!otherGPUs.empty())
		{
			m_VulkanPhysicalDevice = otherGPUs[0];
			return true;
		}

		return false;
	}

	bool VulkanGraphicsContext::FindQueueFamilies(vk::PhysicalDevice physicalDevice)
	{
		auto props = physicalDevice.getQueueFamilyProperties();

		for (int i = 0; i < int(props.size()); i++)
		{
			const auto& queueFamily = props[i];

			if (m_QueueFamilyIndices.Graphics == -1)
			{
				if (queueFamily.queueCount > 0 && (queueFamily.queueFlags & vk::QueueFlagBits::eGraphics))
				{
					m_QueueFamilyIndices.Graphics = i;
				}
			}

			if (m_QueueFamilyIndices.Compute == -1)
			{
				if (queueFamily.queueCount > 0 &&
					(queueFamily.queueFlags & vk::QueueFlagBits::eCompute) &&
					!(queueFamily.queueFlags & vk::QueueFlagBits::eGraphics))
				{
					m_QueueFamilyIndices.Compute = i;
				}
			}

			if (m_QueueFamilyIndices.Transfer == -1)
			{
				if (queueFamily.queueCount > 0 &&
					(queueFamily.queueFlags & vk::QueueFlagBits::eTransfer) &&
					!(queueFamily.queueFlags & vk::QueueFlagBits::eCompute) &&
					!(queueFamily.queueFlags & vk::QueueFlagBits::eGraphics))
				{
					m_QueueFamilyIndices.Transfer = i;
				}
			}

			if (m_QueueFamilyIndices.Present == -1)
			{
				if (queueFamily.queueCount > 0 &&
					glfwGetPhysicalDevicePresentationSupport(m_VulkanInstance, physicalDevice, i))
				{
					m_QueueFamilyIndices.Present = i;
				}
			}
		}

		//if (m_QueueFamilyIndices.Graphics == -1 ||
		//	m_QueueFamilyIndices.Present == -1 && !m_DeviceParams.headlessDevice ||
		//	(m_QueueFamilyIndices.Compute == -1 && m_DeviceParams.enableComputeQueue) ||
		//	(m_QueueFamilyIndices.Transfer == -1 && m_DeviceParams.enableCopyQueue))
		//{
		//	return false;
		//}

		return true;
	}

	void VulkanGraphicsContext::CreateDevice()
	{
		const vk::PhysicalDeviceProperties physicalDeviceProperties = m_VulkanPhysicalDevice.getProperties();
		m_RendererString = std::string(physicalDeviceProperties.deviceName.data());

		vk::PhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddressFeatures = vk::PhysicalDeviceBufferDeviceAddressFeatures();

		std::unordered_set<int> uniqueQueueFamilies = {
			m_QueueFamilyIndices.Graphics,
			m_QueueFamilyIndices.Transfer,
			m_QueueFamilyIndices.Present
		};

		//if (!m_DeviceParams.headlessDevice)
		//	uniqueQueueFamilies.insert(m_QueueFamilyIndices.Present);

		//if (m_DeviceParams.enableComputeQueue)
		//	uniqueQueueFamilies.insert(m_QueueFamilyIndices.Compute);

		//if (m_DeviceParams.enableCopyQueue)
		//	uniqueQueueFamilies.insert(m_QueueFamilyIndices.Transfer);

		float priority = 1.f;
		std::vector<vk::DeviceQueueCreateInfo> queueDesc;
		queueDesc.reserve(uniqueQueueFamilies.size());
		for (int queueFamily : uniqueQueueFamilies)
		{
			queueDesc.push_back(vk::DeviceQueueCreateInfo()
									.setQueueFamilyIndex(queueFamily)
									.setQueueCount(1)
									.setPQueuePriorities(&priority));
		}

		vk::PhysicalDeviceFeatures deviceFeatures = vk::PhysicalDeviceFeatures()
			.setShaderImageGatherExtended(true)
			.setSamplerAnisotropy(true)
			.setTessellationShader(true)
			.setTextureCompressionBC(true)
			.setGeometryShader(true)
			.setImageCubeArray(true)
			.setDualSrcBlend(true);

		vk::PhysicalDeviceVulkan12Features vulkan12features = vk::PhysicalDeviceVulkan12Features()
			.setDescriptorIndexing(true)
			.setRuntimeDescriptorArray(true)
			.setDescriptorBindingPartiallyBound(true)
			.setDescriptorBindingVariableDescriptorCount(true)
			.setTimelineSemaphore(true)
			.setShaderSampledImageArrayNonUniformIndexing(true)
			.setBufferDeviceAddress(bufferDeviceAddressFeatures.bufferDeviceAddress);
		//.setPNext(pNext);

		auto layerVec = Utils::stringSetToVector(enabledExtensions.layers);
		auto extVec = Utils::stringSetToVector(enabledExtensions.device);

		vk::DeviceCreateInfo deviceCreateInfo = vk::DeviceCreateInfo()
			.setPQueueCreateInfos(queueDesc.data())
			.setQueueCreateInfoCount(uint32_t(queueDesc.size()))
			.setPEnabledFeatures(&deviceFeatures)
			.setEnabledExtensionCount(uint32_t(extVec.size()))
			.setPpEnabledExtensionNames(extVec.data())
			.setEnabledLayerCount(uint32_t(layerVec.size()))
			.setPpEnabledLayerNames(layerVec.data())
			.setPNext(&vulkan12features);

		const vk::Result result = m_VulkanPhysicalDevice.createDevice(&deviceCreateInfo, nullptr, &m_VulkanDevice);
		DE_CORE_ASSERT(result == vk::Result::eSuccess, "Failed to create Vulkan device.");

		m_VulkanDevice.getQueue(m_QueueFamilyIndices.Graphics, 0, &m_GraphicsQueue);
		//if (m_DeviceParams.enableComputeQueue)
		//m_VulkanDevice.getQueue(m_QueueFamilyIndices.Compute, 0, &m_ComputeQueue);
		//if (m_DeviceParams.enableCopyQueue)
		m_VulkanDevice.getQueue(m_QueueFamilyIndices.Transfer, 0, &m_TransferQueue);
		//if (!m_DeviceParams.headlessDevice)
		m_VulkanDevice.getQueue(m_QueueFamilyIndices.Present, 0, &m_PresentQueue);

		VULKAN_HPP_DEFAULT_DISPATCHER.init(m_VulkanDevice);

		DE_CORE_TRACE("Created Vulkan device: {}", m_RendererString);
	}

	void VulkanGraphicsContext::CreateDeviceHandle()
	{
		auto vecInstanceExt = Utils::stringSetToVector(enabledExtensions.instance);
		auto vecLayers = Utils::stringSetToVector(enabledExtensions.layers);
		auto vecDeviceExt = Utils::stringSetToVector(enabledExtensions.device);

		nvrhi::vulkan::DeviceDesc deviceDesc;
		// deviceDesc.errorCB = &DefaultMessageCallback::GetInstance();
		deviceDesc.instance = m_VulkanInstance;
		deviceDesc.physicalDevice = m_VulkanPhysicalDevice;
		deviceDesc.device = m_VulkanDevice;
		deviceDesc.graphicsQueue = m_GraphicsQueue;
		deviceDesc.graphicsQueueIndex = m_QueueFamilyIndices.Graphics;
		//if (m_DeviceParams.enableComputeQueue)
		//{
		//	deviceDesc.computeQueue = m_ComputeQueue;
		//	deviceDesc.computeQueueIndex = m_QueueFamilyIndices.Compute;
		//}
		if (true)
		{
			deviceDesc.transferQueue = m_TransferQueue;
			deviceDesc.transferQueueIndex = m_QueueFamilyIndices.Transfer;
		}
		deviceDesc.instanceExtensions = vecInstanceExt.data();
		deviceDesc.numInstanceExtensions = vecInstanceExt.size();
		deviceDesc.deviceExtensions = vecDeviceExt.data();
		deviceDesc.numDeviceExtensions = vecDeviceExt.size();
		deviceDesc.bufferDeviceAddressSupported = m_BufferDeviceAddressSupported;

		m_DeviceHandle = m_NvrhiDevice = nvrhi::vulkan::createDevice(deviceDesc);

		DE_CORE_ASSERT(m_DeviceHandle != nullptr);
		DE_CORE_ASSERT(m_NvrhiDevice != nullptr);
	}

}
