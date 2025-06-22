#pragma once

#if 0
#define DE_HAS_VULKAN 1

#if DE_HAS_DX11 || DE_HAS_DX12
#include <DXGI.h>
#endif

#if DE_HAS_DX11
#include <d3d11.h>
#endif

#if DE_HAS_DX12
#include <d3d12.h>
#endif

#if DE_HAS_VULKAN
#include <nvrhi/vulkan.h>
#endif

#define GLFW_INCLUDE_NONE // Do not include any OpenGL headers
#include <GLFW/glfw3.h>
#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#endif // _WIN32


#include <GLFW/glfw3native.h>
#include <nvrhi/nvrhi.h>

#include <list>
#include <functional>

namespace DingoEngine
{
	class Window;

	struct DefaultMessageCallback : public nvrhi::IMessageCallback
	{
		static DefaultMessageCallback& GetInstance();

		void message(nvrhi::MessageSeverity severity, const char* messageText) override;
	};

	struct InstanceParameters
	{
		bool enableDebugRuntime = false;
		bool headlessDevice = false;

#if HZ_HAS_VULKAN
		std::vector<std::string> requiredVulkanInstanceExtensions;
		std::vector<std::string> requiredVulkanLayers;
		std::vector<std::string> optionalVulkanInstanceExtensions;
		std::vector<std::string> optionalVulkanLayers;
#endif
	};

	struct DeviceCreationParameters : public InstanceParameters
	{
		bool startMaximized = false;
		bool startFullscreen = false;
		bool allowModeSwitch = true;
		bool Decorated = true;
		int windowPosX = -1;            // -1 means use default placement
		int windowPosY = -1;
		uint32_t backBufferWidth = 1280;
		uint32_t backBufferHeight = 720;
		uint32_t refreshRate = 0;
		uint32_t swapChainBufferCount = 3;
		nvrhi::Format swapChainFormat = nvrhi::Format::RGBA8_UNORM;
		uint32_t swapChainSampleCount = 1;
		uint32_t swapChainSampleQuality = 0;
		uint32_t maxFramesInFlight = 2;
		bool enableNvrhiValidationLayer = false;
		bool vsyncEnabled = false;
		bool enableRayTracingExtensions = false; // for vulkan
		bool enableComputeQueue = false;
		bool enableCopyQueue = false;

		// Index of the adapter (DX11, DX12) or physical device (Vk) on which to initialize the device.
		// Negative values mean automatic detection.
		// The order of indices matches that returned by DeviceManager::EnumerateAdapters.
		int adapterIndex = -1;

		// set to true to enable DPI scale factors to be computed per monitor
		// this will keep the on-screen window size in pixels constant
		//
		// if set to false, the DPI scale factors will be constant but the system
		// may scale the contents of the window based on DPI
		//
		// note that the backbuffer size is never updated automatically; if the app
		// wishes to scale up rendering based on DPI, then it must set this to true
		// and respond to DPI scale factor changes by resizing the backbuffer explicitly
		bool enablePerMonitorDPI = false;

#if HZ_HAS_DX11 || HZ_HAS_DX12
		DXGI_USAGE swapChainUsage = DXGI_USAGE_SHADER_INPUT | DXGI_USAGE_RENDER_TARGET_OUTPUT;
		D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_1;
#endif

#if HZ_HAS_VULKAN
		std::vector<std::string> requiredVulkanDeviceExtensions;
		std::vector<std::string> optionalVulkanDeviceExtensions;
		std::vector<size_t> ignoredVulkanValidationMessageLocations;
		std::function<void(VkDeviceCreateInfo&)> deviceCreateInfoCallback;

		// This pointer specifies an optional structure to be put at the end of the chain for 'vkGetPhysicalDeviceFeatures2' call.
		// The structure may also be a chain, and must be alive during the device initialization process.
		// The elements of this structure will be populated before 'deviceCreateInfoCallback' is called,
		// thereby allowing applications to determine if certain features may be enabled on the device.
		void* physicalDeviceFeatures2Extensions = nullptr;
#endif
	};

	class IRenderPass;

	struct AdapterInfo
	{
		std::string name;
		uint32_t vendorID = 0;
		uint32_t deviceID = 0;
		uint64_t dedicatedVideoMemory = 0;
#if HZ_HAS_DX11 || HZ_HAS_DX12
		nvrhi::RefCountPtr<IDXGIAdapter> dxgiAdapter;
#endif
#if HZ_HAS_VULKAN
		VkPhysicalDevice vkPhysicalDevice = nullptr;
#endif
	};

	class DeviceManager
	{
	public:
		static DeviceManager* Create(nvrhi::GraphicsAPI api, GLFWwindow* windowHandle);

		bool CreateHeadlessDevice(const DeviceCreationParameters& params);
		bool CreateDevice(const DeviceCreationParameters& params, const char* windowTitle);
		virtual bool InitSurfaceCapabilities(uint64_t surfaceHandle) = 0;

		// Initializes device-independent objects (DXGI factory, Vulkan instnace).
		// Calling CreateInstance() is required before EnumerateAdapters(), but optional if you don't use EnumerateAdapters().
		// Note: if you call CreateInstance before Create*Device*(), the values in InstanceParameters must match those
		// in DeviceCreationParameters passed to the device call.
		bool CreateInstance(const InstanceParameters& params);

		// Enumerates adapters or physical devices present in the system.
		// Note: a call to CreateInstance() or Create*Device*() is required before EnumerateAdapters().
		virtual bool EnumerateAdapters(std::vector<AdapterInfo>& outAdapters) = 0;

		void RunMessageLoop();
		void AnimateRenderPresent();

		const DeviceCreationParameters& GetDeviceParams() const { return m_DeviceParams; }

		// returns the size of the window in screen coordinates
		void GetWindowDimensions(int& width, int& height);
		// returns the screen coordinate to pixel coordinate scale factor
		void GetDPIScaleInfo(float& x, float& y) const
		{
			x = m_DPIScaleFactorX;
			y = m_DPIScaleFactorY;
		}

		Window* GetWindowContext() { return m_HazelWindow; }
		void SetWindowContext(Window* window) { m_HazelWindow = window; }
	protected:
		bool m_windowVisible = false;

		DeviceCreationParameters m_DeviceParams;
		Window* m_HazelWindow = nullptr;
		GLFWwindow* m_WindowHandle = nullptr;
		bool m_EnableRenderDuringWindowMovement = false;
		// set to true if running on NV GPU
		bool m_IsNvidia = false;
		std::list<IRenderPass*> m_vRenderPasses;
		// timestamp in seconds for the previous frame
		double m_PreviousFrameTimestamp = 0.0;
		// current DPI scale info (updated when window moves)
		float m_DPIScaleFactorX = 1.f;
		float m_DPIScaleFactorY = 1.f;
		bool m_RequestedVSync = false;
		bool m_InstanceCreated = false;

		double m_AverageFrameTime = 0.0;
		double m_AverageTimeUpdateInterval = 0.5;
		double m_FrameTimeSum = 0.0;
		int m_NumberOfAccumulatedFrames = 0;

		uint32_t m_FrameIndex = 0;

		DeviceManager() = default;

		void UpdateAverageFrameTime(double elapsedTime);
		// device-specific methods
		virtual bool CreateInstanceInternal() = 0;
		virtual bool CreateDevice() = 0;
		virtual void DestroyDevice() = 0;


	public:
		[[nodiscard]] virtual nvrhi::IDevice* GetDevice() const = 0;
		[[nodiscard]] virtual const char* GetRendererString() const = 0;
		[[nodiscard]] virtual nvrhi::GraphicsAPI GetGraphicsAPI() const = 0;

		const DeviceCreationParameters& GetDeviceParams();
		[[nodiscard]] double GetAverageFrameTimeSeconds() const { return m_AverageFrameTime; }
		[[nodiscard]] double GetPreviousFrameTimestamp() const { return m_PreviousFrameTimestamp; }
		void SetFrameTimeUpdateInterval(double seconds) { m_AverageTimeUpdateInterval = seconds; }
		[[nodiscard]] bool IsVsyncEnabled() const { return m_DeviceParams.vsyncEnabled; }
		virtual void SetVsyncEnabled(bool enabled) { m_RequestedVSync = enabled; /* will be processed later */ }
		virtual void ReportLiveObjects() {}
		void SetEnableRenderDuringWindowMovement(bool val) { m_EnableRenderDuringWindowMovement = val; }

		// these are public in order to be called from the GLFW callback functions
		void WindowCloseCallback() {}
		void WindowIconifyCallback(int iconified) {}
		void WindowFocusCallback(int focused) {}
		void WindowRefreshCallback() {}
		void WindowPosCallback(int xpos, int ypos);

		void KeyboardUpdate(int key, int scancode, int action, int mods);
		void KeyboardCharInput(unsigned int unicode, int mods);
		void MousePosUpdate(double xpos, double ypos);
		void MouseButtonUpdate(int button, int action, int mods);
		void MouseScrollUpdate(double xoffset, double yoffset);

		[[nodiscard]] uint32_t GetFrameIndex() const { return m_FrameIndex; }

		virtual void Shutdown();
		virtual ~DeviceManager() = default;

		void SetInformativeWindowTitle(const char* applicationName, const char* extraInfo = nullptr);

		virtual bool IsVulkanInstanceExtensionEnabled(const char* extensionName) const { return false; }
		virtual bool IsVulkanDeviceExtensionEnabled(const char* extensionName) const { return false; }
		virtual bool IsVulkanLayerEnabled(const char* layerName) const { return false; }
		virtual void GetEnabledVulkanInstanceExtensions(std::vector<std::string>& extensions) const {}
		virtual void GetEnabledVulkanDeviceExtensions(std::vector<std::string>& extensions) const {}
		virtual void GetEnabledVulkanLayers(std::vector<std::string>& layers) const {}

		struct PipelineCallbacks
		{
			std::function<void(DeviceManager&)> beforeFrame = nullptr;
			std::function<void(DeviceManager&)> beforeAnimate = nullptr;
			std::function<void(DeviceManager&)> afterAnimate = nullptr;
			std::function<void(DeviceManager&)> beforeRender = nullptr;
			std::function<void(DeviceManager&)> afterRender = nullptr;
			std::function<void(DeviceManager&)> beforePresent = nullptr;
			std::function<void(DeviceManager&)> afterPresent = nullptr;
		} m_callbacks;

	private:
		static DeviceManager* CreateD3D11();
		static DeviceManager* CreateD3D12();
		static DeviceManager* CreateVK(GLFWwindow* windowHandle);

		std::string m_WindowTitle;
	};

	class IRenderPass
	{
	private:
		DeviceManager* m_DeviceManager;

	public:
		explicit IRenderPass(DeviceManager* deviceManager)
			: m_DeviceManager(deviceManager)
		{}

		virtual ~IRenderPass() = default;

		virtual void Render(nvrhi::IFramebuffer* framebuffer) {}
		virtual void Animate(float fElapsedTimeSeconds) {}
		virtual void BackBufferResizing() {}
		virtual void BackBufferResized(const uint32_t width, const uint32_t height, const uint32_t sampleCount) {}

		// all of these pass in GLFW constants as arguments
		// see http://www.glfw.org/docs/latest/input.html
		// return value is true if the event was consumed by this render pass, false if it should be passed on
		virtual bool KeyboardUpdate(int key, int scancode, int action, int mods) { return false; }
		virtual bool KeyboardCharInput(unsigned int unicode, int mods) { return false; }
		virtual bool MousePosUpdate(double xpos, double ypos) { return false; }
		virtual bool MouseScrollUpdate(double xoffset, double yoffset) { return false; }
		virtual bool MouseButtonUpdate(int button, int action, int mods) { return false; }
		virtual bool JoystickButtonUpdate(int button, bool pressed) { return false; }
		virtual bool JoystickAxisUpdate(int axis, float value) { return false; }

		[[nodiscard]] DeviceManager* GetDeviceManager() const { return m_DeviceManager; }
		[[nodiscard]] nvrhi::IDevice* GetDevice() const { return m_DeviceManager->GetDevice(); }
		[[nodiscard]] uint32_t GetFrameIndex() const { return m_DeviceManager->GetFrameIndex(); }
	};

	class VulkanDeviceManager : public DeviceManager
	{
	public:
		struct QueueFamilyIndices
		{
			int32_t Graphics = -1;
			int32_t Compute = -1;
			int32_t Transfer = -1;
			int32_t Present = -1;
		};
	public:
		VulkanDeviceManager(GLFWwindow* windowHandle)
		{
			m_WindowHandle = windowHandle;
		}

		[[nodiscard]] nvrhi::IDevice* GetDevice() const override
		{
			if (m_ValidationLayer)
				return m_ValidationLayer;

			return m_NvrhiDevice;
		}

		[[nodiscard]] nvrhi::GraphicsAPI GetGraphicsAPI() const override
		{
			return nvrhi::GraphicsAPI::VULKAN;
		}

		bool EnumerateAdapters(std::vector<AdapterInfo>& outAdapters) override;

		vk::Instance GetVulkanInstance() const { return m_VulkanInstance; }
	protected:
		virtual bool CreateInstanceInternal() override;
		virtual bool CreateDevice() override;
		virtual bool InitSurfaceCapabilities(uint64_t surfaceHandle) override;
		virtual void DestroyDevice() override;

		const char* GetRendererString() const override
		{
			return m_RendererString.c_str();
		}

		bool IsVulkanInstanceExtensionEnabled(const char* extensionName) const override
		{
			return enabledExtensions.instance.find(extensionName) != enabledExtensions.instance.end();
		}

		bool IsVulkanDeviceExtensionEnabled(const char* extensionName) const override
		{
			return enabledExtensions.device.find(extensionName) != enabledExtensions.device.end();
		}

		bool IsVulkanLayerEnabled(const char* layerName) const override
		{
			return enabledExtensions.layers.find(layerName) != enabledExtensions.layers.end();
		}

		void GetEnabledVulkanInstanceExtensions(std::vector<std::string>& extensions) const override
		{
			for (const auto& ext : enabledExtensions.instance)
				extensions.push_back(ext);
		}

		void GetEnabledVulkanDeviceExtensions(std::vector<std::string>& extensions) const override
		{
			for (const auto& ext : enabledExtensions.device)
				extensions.push_back(ext);
		}

		void GetEnabledVulkanLayers(std::vector<std::string>& layers) const override
		{
			for (const auto& ext : enabledExtensions.layers)
				layers.push_back(ext);
		}

	private:
		bool createInstance();
		bool createWindowSurface();
		void installDebugCallback();
		bool pickPhysicalDevice();
		bool FindQueueFamilies(vk::PhysicalDevice physicalDevice);
		bool createDevice();
		bool createSwapChain();
		void destroySwapChain();

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

		std::unordered_set<std::string> m_RayTracingExtensions = {
			VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
			VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
			VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME,
			VK_KHR_RAY_QUERY_EXTENSION_NAME,
			VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME
		};

		std::string m_RendererString;

		vk::Instance m_VulkanInstance;
		vk::DebugReportCallbackEXT m_DebugReportCallback;

		vk::PhysicalDevice m_VulkanPhysicalDevice;

		QueueFamilyIndices m_QueueFamilyIndices;

		vk::Device m_VulkanDevice;
		vk::Queue m_GraphicsQueue;
		vk::Queue m_ComputeQueue;
		vk::Queue m_TransferQueue;
		vk::Queue m_PresentQueue;

		nvrhi::vulkan::DeviceHandle m_NvrhiDevice;
		nvrhi::DeviceHandle m_ValidationLayer;

		bool m_SwapChainMutableFormatSupported = false;
		bool m_BufferDeviceAddressSupported = false;

		vk::DynamicLoader m_dynamicLoader;

	private:
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
			const VulkanDeviceManager* manager = (const VulkanDeviceManager*)userData;

			if (manager)
			{
				const auto& ignored = manager->m_DeviceParams.ignoredVulkanValidationMessageLocations;
				const auto found = std::find(ignored.begin(), ignored.end(), location);
				if (found != ignored.end())
					return VK_FALSE;
			}

			HZ_CORE_WARN_TAG("Renderer", "[Vulkan: location=0x{0} code={1}, layerPrefix='{2}'] {3}", location, code, layerPrefix, msg);

			return VK_FALSE;
		}

		friend class VulkanSwapChain;
	};
}

#endif
