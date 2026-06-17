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

	VulkanGraphicsContext::VulkanGraphicsContext(const GraphicsParams& params)
		: NvrhiGraphicsContext(params)
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
#ifndef DE_DISTRIBUTION
		CreateDebugMessenger();
#endif

		// Create a throwaway surface for the application's main window so device selection can
		// verify which GPU/queue can actually present to the display the window lives on (the
		// multi-GPU / hybrid-graphics case). The swap chain creates its own surface later; this
		// one only informs selection and is destroyed immediately afterwards.
		vk::SurfaceKHR probeSurface = nullptr;
		if (m_Params.NativeWindowHandle)
		{
			const VkResult surfaceResult = glfwCreateWindowSurface(m_VulkanInstance, m_Params.NativeWindowHandle, nullptr, (VkSurfaceKHR*)&probeSurface);
			if (surfaceResult != VK_SUCCESS)
			{
				probeSurface = nullptr;
				DE_CORE_WARN("Failed to create a probe surface for device selection (error {}); falling back to surface-independent present detection.", std::string(nvrhi::vulkan::resultToString(surfaceResult)));
			}
		}

		const bool devicePicked = PickPhysicalDevice(probeSurface);

		if (probeSurface)
		{
			m_VulkanInstance.destroySurfaceKHR(probeSurface);
		}

		DE_CORE_VERIFY(devicePicked, "Failed to find a suitable Vulkan device that can present to the application window.");

		CreateDevice();
		CreateDeviceHandle();
	}

	void VulkanGraphicsContext::Shutdown()
	{
		m_RendererString.clear();

		if (m_DeviceHandle)
		{
			m_DeviceHandle->waitForIdle();
			m_DeviceHandle->runGarbageCollection();
			m_DeviceHandle = nullptr;
		}
		m_NvrhiDevice = nullptr;

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
#ifndef DE_DISTRIBUTION
		enabledExtensions.instance.insert(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
		enabledExtensions.layers.insert("VK_LAYER_KHRONOS_validation");
#endif

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

		DE_CORE_TRACE("Enabled Vulkan instance extensions ({}):", enabledExtensions.instance.size());
		for (const auto& ext : enabledExtensions.instance)
		{
			DE_CORE_TRACE("    {}", ext.c_str());
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

		DE_CORE_TRACE("Enabled Vulkan Layer(s) ({}):", enabledExtensions.layers.size());
		for (const auto& layer : enabledExtensions.layers)
		{
			DE_CORE_TRACE("    {}", layer.c_str());
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
			DE_CORE_VERIFY(false);
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

	bool VulkanGraphicsContext::PickPhysicalDevice(vk::SurfaceKHR surface)
	{
		const std::vector<vk::PhysicalDevice> physicalDevices = m_VulkanInstance.enumeratePhysicalDevices();

		// Start building an error message in case we cannot find a device.
		std::stringstream errorStream;
		errorStream << "Cannot find a Vulkan device that supports all the required extensions and properties.";

		struct Candidate
		{
			vk::PhysicalDevice device;
			QueueFamilyIndices indices;
		};

		// Presentable candidates split by GPU type: prefer a discrete GPU, but fall back to an
		// integrated/other GPU that can actually present to this window's display (the hybrid
		// laptop case where the dGPU does not drive the panel the window is on).
		std::vector<Candidate> discretePresentable;
		std::vector<Candidate> otherPresentable;
		bool haveUsableButNonPresentable = false;

		for (const vk::PhysicalDevice& dev : physicalDevices)
		{
			const vk::PhysicalDeviceProperties prop = dev.getProperties();

			errorStream << std::endl << prop.deviceName.data() << ":";

			// check that all required device extensions are present
			std::unordered_set<std::string> requiredExtensions = enabledExtensions.device;
			for (const auto& ext : dev.enumerateDeviceExtensionProperties())
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

			const vk::PhysicalDeviceFeatures deviceFeatures = dev.getFeatures();
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

			// Fresh per-device queue families -- never reuse indices across devices.
			QueueFamilyIndices indices;
			if (!FindQueueFamilies(dev, surface, indices))
			{
				errorStream << std::endl << "  - does not provide a graphics queue";
				deviceIsGood = false;
			}

			if (!deviceIsGood)
				continue;

			if (indices.Present == -1)
			{
				// Usable for rendering, but cannot present to the window's surface (likely a GPU
				// that does not drive the display the window is on). Remember it only so we can
				// emit a clear diagnostic if nothing can present.
				errorStream << std::endl << "  - cannot present to the application window's surface";
				haveUsableButNonPresentable = true;
				continue;
			}

			if (prop.deviceType == vk::PhysicalDeviceType::eDiscreteGpu)
				discretePresentable.push_back({ dev, indices });
			else
				otherPresentable.push_back({ dev, indices });
		}

		const Candidate* chosen = nullptr;
		const char* reason = nullptr;
		if (!discretePresentable.empty())
		{
			chosen = &discretePresentable.front();
			reason = "discrete GPU, can present to the window";
		}
		else if (!otherPresentable.empty())
		{
			chosen = &otherPresentable.front();
			reason = "integrated/other GPU, can present to the window";
		}

		if (!chosen)
		{
			if (haveUsableButNonPresentable)
			{
				DE_CORE_ERROR("Found a usable Vulkan GPU, but none can present to the application window's surface. "
					"This is the multi-GPU / hybrid-graphics case where the window is on a display driven by a different "
					"adapter. Move the window to the primary display or update GPU drivers.\n{}", errorStream.str().c_str());
			}
			else
			{
				DE_CORE_ERROR("{}", errorStream.str().c_str());
			}
			return false;
		}

		m_VulkanPhysicalDevice = chosen->device;
		m_QueueFamilyIndices = chosen->indices;
		DE_CORE_TRACE("Selected Vulkan physical device: {} ({})", std::string(chosen->device.getProperties().deviceName.data()), reason);
		return true;
	}

	bool VulkanGraphicsContext::FindQueueFamilies(vk::PhysicalDevice physicalDevice, vk::SurfaceKHR surface, QueueFamilyIndices& outIndices) const
	{
		outIndices = QueueFamilyIndices{};

		const std::vector<vk::QueueFamilyProperties> props = physicalDevice.getQueueFamilyProperties();

		int32_t dedicatedTransfer = -1;

		for (int i = 0; i < int(props.size()); i++)
		{
			const vk::QueueFamilyProperties& queueFamily = props[i];
			if (queueFamily.queueCount == 0)
				continue;

			if (outIndices.Graphics == -1 && (queueFamily.queueFlags & vk::QueueFlagBits::eGraphics))
			{
				outIndices.Graphics = i;
			}

			if (outIndices.Compute == -1 &&
				(queueFamily.queueFlags & vk::QueueFlagBits::eCompute) &&
				!(queueFamily.queueFlags & vk::QueueFlagBits::eGraphics))
			{
				outIndices.Compute = i;
			}

			if (dedicatedTransfer == -1 &&
				(queueFamily.queueFlags & vk::QueueFlagBits::eTransfer) &&
				!(queueFamily.queueFlags & vk::QueueFlagBits::eCompute) &&
				!(queueFamily.queueFlags & vk::QueueFlagBits::eGraphics))
			{
				dedicatedTransfer = i;
			}

			if (outIndices.Present == -1)
			{
				// Prefer a true per-surface check so we only accept a queue that can present to
				// THIS window's display. Fall back to the surface-independent GLFW query only when
				// no surface is available (e.g. probe-surface creation failed).
				bool presentSupported = false;
				if (surface)
				{
					presentSupported = physicalDevice.getSurfaceSupportKHR(uint32_t(i), surface) == VK_TRUE;
				}
				else
				{
					presentSupported = glfwGetPhysicalDevicePresentationSupport(m_VulkanInstance, physicalDevice, i) != 0;
				}

				if (presentSupported)
				{
					outIndices.Present = i;
				}
			}
		}

		// Use a dedicated transfer-only family when one exists (better copy/DMA overlap); otherwise
		// fall back to the graphics family. Integrated GPUs frequently expose no transfer-only
		// family, so requiring one here would wrongly reject the very GPU that drives the panel.
		outIndices.Transfer = (dedicatedTransfer != -1) ? dedicatedTransfer : outIndices.Graphics;

		// A device is usable if it can render. Present capability is reported separately via
		// outIndices.Present (may be -1) so the caller can prefer present-capable GPUs.
		return outIndices.Graphics != -1;
	}

	void VulkanGraphicsContext::CreateDevice()
	{
		const vk::PhysicalDeviceProperties physicalDeviceProperties = m_VulkanPhysicalDevice.getProperties();
		m_RendererString = std::string(physicalDeviceProperties.deviceName.data());

		m_AdapterInfo.Name = m_RendererString;
		m_AdapterInfo.VendorID = physicalDeviceProperties.vendorID;
		m_AdapterInfo.DeviceID = physicalDeviceProperties.deviceID;
		switch (physicalDeviceProperties.deviceType)
		{
			case vk::PhysicalDeviceType::eDiscreteGpu:   m_AdapterInfo.DeviceType = AdapterDeviceType::Discrete;   break;
			case vk::PhysicalDeviceType::eIntegratedGpu: m_AdapterInfo.DeviceType = AdapterDeviceType::Integrated; break;
			case vk::PhysicalDeviceType::eVirtualGpu:    m_AdapterInfo.DeviceType = AdapterDeviceType::Virtual;    break;
			case vk::PhysicalDeviceType::eCpu:           m_AdapterInfo.DeviceType = AdapterDeviceType::Software;   break;
			default:                                     m_AdapterInfo.DeviceType = AdapterDeviceType::Unknown;    break;
		}
		auto memProps = m_VulkanPhysicalDevice.getMemoryProperties();
		for (uint32_t i = 0; i < memProps.memoryHeapCount; ++i)
		{
			if (memProps.memoryHeaps[i].flags & vk::MemoryHeapFlagBits::eDeviceLocal)
				m_AdapterInfo.DedicatedVideoMemory += memProps.memoryHeaps[i].size;
		}

		vk::PhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddressFeatures = vk::PhysicalDeviceBufferDeviceAddressFeatures();

		std::unordered_set<int> uniqueQueueFamilies = {
			m_QueueFamilyIndices.Graphics,
			m_QueueFamilyIndices.Transfer
		};
		if (m_QueueFamilyIndices.Present != -1)
		{
			uniqueQueueFamilies.insert(m_QueueFamilyIndices.Present);
		}

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

		vk::PhysicalDeviceVulkan13Features vulkan13features = vk::PhysicalDeviceVulkan13Features()
			.setShaderDemoteToHelperInvocation(true);

		vk::PhysicalDeviceVulkan12Features vulkan12features = vk::PhysicalDeviceVulkan12Features()
			.setDescriptorIndexing(true)
			.setRuntimeDescriptorArray(true)
			.setDescriptorBindingPartiallyBound(true)
			.setDescriptorBindingVariableDescriptorCount(true)
			.setTimelineSemaphore(true)
			.setShaderSampledImageArrayNonUniformIndexing(true)
			.setBufferDeviceAddress(bufferDeviceAddressFeatures.bufferDeviceAddress)
			.setPNext(&vulkan13features);

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
		m_VulkanDevice.getQueue(m_QueueFamilyIndices.Transfer, 0, &m_TransferQueue);
		if (m_QueueFamilyIndices.Present != -1)
		{
			m_VulkanDevice.getQueue(m_QueueFamilyIndices.Present, 0, &m_PresentQueue);
		}

		VULKAN_HPP_DEFAULT_DISPATCHER.init(m_VulkanDevice);

		DE_CORE_TRACE("Created Vulkan device: {}", m_RendererString);
	}

	void VulkanGraphicsContext::CreateDeviceHandle()
	{
		auto vecInstanceExt = Utils::stringSetToVector(enabledExtensions.instance);
		auto vecLayers = Utils::stringSetToVector(enabledExtensions.layers);
		auto vecDeviceExt = Utils::stringSetToVector(enabledExtensions.device);

		// Value-initialize: the queue handles (graphicsQueue/transferQueue/computeQueue) are raw
		// VkQueue with no default initializer in NVRHI's DeviceDesc. NVRHI treats a null queue as
		// "unused" (if (desc.transferQueue) ...), so any queue we don't set below must read as null
		// rather than indeterminate stack garbage -- e.g. transferQueue on GPUs without a dedicated
		// transfer family, and computeQueue which this context never sets.
		nvrhi::vulkan::DeviceDesc deviceDesc{};
		deviceDesc.errorCB = &NvrhiMessageCallback::Get();
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
		// Only register a separate transfer queue with NVRHI when the device actually has a
		// dedicated transfer family. When Transfer fell back to the graphics family (common on
		// integrated GPUs), handing NVRHI the graphics queue as a distinct transfer queue is wrong.
		const bool hasDedicatedTransferQueue = m_QueueFamilyIndices.Transfer != -1 && m_QueueFamilyIndices.Transfer != m_QueueFamilyIndices.Graphics;
		if (hasDedicatedTransferQueue)
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
