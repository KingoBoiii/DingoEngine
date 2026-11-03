#pragma once
#include <vulkan/vulkan_core.h>

#include <string>
#include <unordered_set>

struct GLFWwindow;

namespace Dingo
{

	struct XcbSurface
	{
		VkSurfaceKHR Surface = VK_NULL_HANDLE;
		void* Connection = nullptr;
	};

	// An X11 window's presentation surface on its own X connection, so the driver's present thread
	// never reads from or writes to the connection GLFW polls for events (#186). Returns an empty
	// XcbSurface when the window isn't X11, the instance lacks VK_KHR_xcb_surface or libxcb can't
	// be loaded; the caller then uses glfwCreateWindowSurface.
	XcbSurface CreateXcbSurface(VkInstance instance, GLFWwindow* window, const std::unordered_set<std::string>& enabledInstanceExtensions);

	// After vkDestroySurfaceKHR on the surface it was created for.
	void CloseXcbConnection(void* connection);

}
