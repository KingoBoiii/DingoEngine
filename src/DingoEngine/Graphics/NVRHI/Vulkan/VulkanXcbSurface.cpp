#include "depch.h"
#include "VulkanXcbSurface.h"

#if defined(DE_PLATFORM_LINUX)

#include <dlfcn.h>
#include <xcb/xcb.h>
#include <vulkan/vulkan_xcb.h>

#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>

namespace Dingo
{

	namespace
	{

		// Loaded at run time like GLFW loads libX11, so a Wayland-only system needs no libxcb.
		struct XcbApi
		{
			decltype(&xcb_connect) Connect = nullptr;
			decltype(&xcb_connection_has_error) HasError = nullptr;
			decltype(&xcb_disconnect) Disconnect = nullptr;
		};

		const XcbApi& GetXcbApi()
		{
			static const XcbApi api = []
			{
				XcbApi result;
				void* library = dlopen("libxcb.so.1", RTLD_LAZY | RTLD_LOCAL);
				if (!library)
					return result;

				result.Connect = reinterpret_cast<decltype(&xcb_connect)>(dlsym(library, "xcb_connect"));
				result.HasError = reinterpret_cast<decltype(&xcb_connection_has_error)>(dlsym(library, "xcb_connection_has_error"));
				result.Disconnect = reinterpret_cast<decltype(&xcb_disconnect)>(dlsym(library, "xcb_disconnect"));
				if (!result.Connect || !result.HasError || !result.Disconnect)
					result = {};
				return result;
			}();
			return api;
		}

	}

	XcbSurface CreateXcbSurface(VkInstance instance, GLFWwindow* window, const std::unordered_set<std::string>& enabledInstanceExtensions)
	{
		if (!window || glfwGetPlatform() != GLFW_PLATFORM_X11)
			return {};

		const XcbApi& xcb = GetXcbApi();
		const auto createSurface = reinterpret_cast<PFN_vkCreateXcbSurfaceKHR>(glfwGetInstanceProcAddress(instance, "vkCreateXcbSurfaceKHR"));
		if (!enabledInstanceExtensions.contains(VK_KHR_XCB_SURFACE_EXTENSION_NAME) || !xcb.Connect || !createSurface)
		{
			DE_CORE_WARN("No VK_KHR_xcb_surface or libxcb; presenting on GLFW's X connection.");
			return {};
		}

		Display* display = glfwGetX11Display();
		xcb_connection_t* connection = xcb.Connect(display ? DisplayString(display) : nullptr, nullptr);
		if (xcb.HasError(connection))
		{
			xcb.Disconnect(connection);
			DE_CORE_WARN("Could not open an X connection for the window surface; presenting on GLFW's connection.");
			return {};
		}

		VkXcbSurfaceCreateInfoKHR createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
		createInfo.connection = connection;
		createInfo.window = static_cast<xcb_window_t>(glfwGetX11Window(window));

		VkSurfaceKHR surface = VK_NULL_HANDLE;
		if (const VkResult result = createSurface(instance, &createInfo, nullptr, &surface); result != VK_SUCCESS)
		{
			xcb.Disconnect(connection);
			DE_CORE_WARN("vkCreateXcbSurfaceKHR failed ({}); presenting on GLFW's X connection.", static_cast<int>(result));
			return {};
		}

		return { surface, connection };
	}

	void CloseXcbConnection(void* connection)
	{
		if (connection)
			GetXcbApi().Disconnect(static_cast<xcb_connection_t*>(connection));
	}

}

#else

namespace Dingo
{

	XcbSurface CreateXcbSurface(VkInstance, GLFWwindow*, const std::unordered_set<std::string>&)
	{
		return {};
	}

	void CloseXcbConnection(void*)
	{}

}

#endif
