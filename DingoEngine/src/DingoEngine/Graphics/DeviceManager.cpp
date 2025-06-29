/*
* Copyright (c) 2014-2021, NVIDIA CORPORATION. All rights reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
* FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
* DEALINGS IN THE SOFTWARE.
*/

/*
License for glfw

Copyright (c) 2002-2006 Marcus Geelnard

Copyright (c) 2006-2019 Camilla Lowy

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would
   be appreciated but is not required.

2. Altered source versions must be plainly marked as such, and must not
   be misrepresented as being the original software.

3. This notice may not be removed or altered from any source
   distribution.
*/


#include "depch.h"
#if 0
#include "DeviceManager.h"

#include "DingoEngine/Windowing/Window.h"

#include <nvrhi/utils.h>

#include <cstdio>
#include <iomanip>
#include <thread>
#include <sstream>

#if DE_HAS_DX11
#include <d3d11.h>
#endif

#if DE_HAS_DX12
#include <d3d12.h>
#endif

#ifdef _WIN64
#include <ShellScalingApi.h>
//#pragma comment(lib, "shcore.lib")
#endif

//using namespace DingoEngine;

// The joystick interface in glfw is not per-window like the keys, mouse, etc. The joystick callbacks
// don't take a window arg. So glfw's model is a global joystick shared by all windows. Hence, the equivalent 
// is a singleton class that all DeviceManager instances can use.
class JoyStickManager
{
public:
	static JoyStickManager& Singleton()
	{
		static JoyStickManager singleton;
		return singleton;
	}

	void UpdateAllJoysticks(const std::list<IRenderPass*>& passes);

	void EraseDisconnectedJoysticks();
	void EnumerateJoysticks();

	void ConnectJoystick(int id);
	void DisconnectJoystick(int id);

private:
	JoyStickManager() {}
	void UpdateJoystick(int j, const std::list<IRenderPass*>& passes);

	std::list<int> m_JoystickIDs, m_RemovedJoysticks;
};


static void WindowIconifyCallback_GLFW(GLFWwindow* window, int iconified)
{
	DeviceManager* manager = reinterpret_cast<DeviceManager*>(glfwGetWindowUserPointer(window));
	manager->WindowIconifyCallback(iconified);
	manager->GetWindowContext()->OnWindowIconifyCallback(iconified);
}
static void TitlebarHitTestCallback_GLFW(GLFWwindow* window, int x, int y, int* hit)
{
	DeviceManager* manager = reinterpret_cast<DeviceManager*>(glfwGetWindowUserPointer(window));
	manager->GetWindowContext()->OnTitlebarHitTestCallback(x, y, hit);
}

static void WindowFocusCallback_GLFW(GLFWwindow* window, int focused)
{
	DeviceManager* manager = reinterpret_cast<DeviceManager*>(glfwGetWindowUserPointer(window));
	manager->WindowFocusCallback(focused);
}

static void WindowRefreshCallback_GLFW(GLFWwindow* window)
{
	DeviceManager* manager = reinterpret_cast<DeviceManager*>(glfwGetWindowUserPointer(window));
	manager->WindowRefreshCallback();
}

static void WindowCloseCallback_GLFW(GLFWwindow* window)
{
	DeviceManager* manager = reinterpret_cast<DeviceManager*>(glfwGetWindowUserPointer(window));
	manager->WindowCloseCallback();
	manager->GetWindowContext()->OnWindowCloseCallback();
}

static void WindowPosCallback_GLFW(GLFWwindow* window, int xpos, int ypos)
{
	DeviceManager* manager = reinterpret_cast<DeviceManager*>(glfwGetWindowUserPointer(window));
	manager->WindowPosCallback(xpos, ypos);
}

static void KeyCallback_GLFW(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	DeviceManager* manager = reinterpret_cast<DeviceManager*>(glfwGetWindowUserPointer(window));
	manager->KeyboardUpdate(key, scancode, action, mods);
	manager->GetWindowContext()->OnKeyCallback(key, scancode, action, mods);
}

static void CharModsCallback_GLFW(GLFWwindow* window, unsigned int unicode, int mods)
{
	DeviceManager* manager = reinterpret_cast<DeviceManager*>(glfwGetWindowUserPointer(window));
	manager->KeyboardCharInput(unicode, mods);
}

static void MousePosCallback_GLFW(GLFWwindow* window, double xpos, double ypos)
{
	DeviceManager* manager = reinterpret_cast<DeviceManager*>(glfwGetWindowUserPointer(window));
	manager->MousePosUpdate(xpos, ypos);
	manager->GetWindowContext()->OnMousePosCallback(xpos, ypos);
}

static void MouseButtonCallback_GLFW(GLFWwindow* window, int button, int action, int mods)
{
	DeviceManager* manager = reinterpret_cast<DeviceManager*>(glfwGetWindowUserPointer(window));
	manager->MouseButtonUpdate(button, action, mods);
	manager->GetWindowContext()->OnMouseButtonCallback(button, action, mods);
}

static void MouseScrollCallback_GLFW(GLFWwindow* window, double xoffset, double yoffset)
{
	DeviceManager* manager = reinterpret_cast<DeviceManager*>(glfwGetWindowUserPointer(window));
	manager->MouseScrollUpdate(xoffset, yoffset);
	manager->GetWindowContext()->OnMouseScrollCallback(xoffset, yoffset);
}

static void JoystickConnectionCallback_GLFW(int joyId, int connectDisconnect)
{
	if (connectDisconnect == GLFW_CONNECTED)
		JoyStickManager::Singleton().ConnectJoystick(joyId);
	if (connectDisconnect == GLFW_DISCONNECTED)
		JoyStickManager::Singleton().DisconnectJoystick(joyId);
}

bool DeviceManager::CreateInstance(const InstanceParameters& params)
{
	m_InstanceCreated = CreateInstanceInternal();
	return m_InstanceCreated;
}

bool DeviceManager::CreateHeadlessDevice(const DeviceCreationParameters& params)
{
	m_DeviceParams = params;
	m_DeviceParams.headlessDevice = true;

	if (!CreateInstance(m_DeviceParams))
		return false;

	return CreateDevice();
}

bool DeviceManager::CreateDevice(const DeviceCreationParameters& params, const char* windowTitle)
{
	m_DeviceParams = params;

#if OLD
	glfwSetWindowPosCallback(m_Window, WindowPosCallback_GLFW);
	glfwSetWindowCloseCallback(m_Window, WindowCloseCallback_GLFW);
	glfwSetWindowRefreshCallback(m_Window, WindowRefreshCallback_GLFW);
	glfwSetWindowFocusCallback(m_Window, WindowFocusCallback_GLFW);
	glfwSetWindowIconifyCallback(m_Window, WindowIconifyCallback_GLFW);
	glfwSetTitlebarHitTestCallback(m_Window, TitlebarHitTestCallback_GLFW);
	glfwSetKeyCallback(m_Window, KeyCallback_GLFW);
	glfwSetCharModsCallback(m_Window, CharModsCallback_GLFW);
	glfwSetCursorPosCallback(m_Window, MousePosCallback_GLFW);
	glfwSetMouseButtonCallback(m_Window, MouseButtonCallback_GLFW);
	glfwSetScrollCallback(m_Window, MouseScrollCallback_GLFW);
	glfwSetJoystickCallback(JoystickConnectionCallback_GLFW);

	// If there are multiple device managers, then this would be called by each one which isn't necessary
	// but should not hurt.
	JoyStickManager::Singleton().EnumerateJoysticks();
#endif

	if (!CreateInstance(m_DeviceParams))
		return false;

	if (!CreateDevice())
		return false;

	return true;
}

void DeviceManager::UpdateAverageFrameTime(double elapsedTime)
{
	m_FrameTimeSum += elapsedTime;
	m_NumberOfAccumulatedFrames += 1;

	if (m_FrameTimeSum > m_AverageTimeUpdateInterval && m_NumberOfAccumulatedFrames > 0)
	{
		m_AverageFrameTime = m_FrameTimeSum / double(m_NumberOfAccumulatedFrames);
		m_NumberOfAccumulatedFrames = 0;
		m_FrameTimeSum = 0.0;
	}
}

void DeviceManager::RunMessageLoop()
{
#if 0
	m_PreviousFrameTimestamp = glfwGetTime();

	while (!glfwWindowShouldClose(m_Window))
	{

		if (m_callbacks.beforeFrame) m_callbacks.beforeFrame(*this);

		glfwPollEvents();
		UpdateWindowSize();
		AnimateRenderPresent();
	}

	GetDevice()->waitForIdle();
#endif
}

void DeviceManager::AnimateRenderPresent()
{
#if 0
	double curTime = glfwGetTime();
	double elapsedTime = curTime - m_PreviousFrameTimestamp;

	JoyStickManager::Singleton().EraseDisconnectedJoysticks();
	JoyStickManager::Singleton().UpdateAllJoysticks(m_vRenderPasses);

	if (m_windowVisible)
	{
		if (m_callbacks.beforeAnimate) m_callbacks.beforeAnimate(*this);
		Animate(elapsedTime);
		if (m_callbacks.afterAnimate) m_callbacks.afterAnimate(*this);
		if (m_callbacks.beforeRender) m_callbacks.beforeRender(*this);
		Render();
		if (m_callbacks.afterRender) m_callbacks.afterRender(*this);
		if (m_callbacks.beforePresent) m_callbacks.beforePresent(*this);
		Present();
		if (m_callbacks.afterPresent) m_callbacks.afterPresent(*this);
	}

	std::this_thread::sleep_for(std::chrono::milliseconds(0));

	GetDevice()->runGarbageCollection();

	UpdateAverageFrameTime(elapsedTime);
	m_PreviousFrameTimestamp = curTime;

	++m_FrameIndex;
#endif
}

void DeviceManager::GetWindowDimensions(int& width, int& height)
{
	width = m_DeviceParams.backBufferWidth;
	height = m_DeviceParams.backBufferHeight;
}

const DeviceCreationParameters& DeviceManager::GetDeviceParams()
{
	return m_DeviceParams;
}

void DeviceManager::WindowPosCallback(int x, int y)
{
#ifdef _WINDOWS
	if (m_DeviceParams.enablePerMonitorDPI)
	{
		HWND hwnd = glfwGetWin32Window(m_Window);
		auto monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);

		unsigned int dpiX;
		unsigned int dpiY;
		GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &dpiX, &dpiY);

		m_DPIScaleFactorX = dpiX / 96.f;
		m_DPIScaleFactorY = dpiY / 96.f;
	}
#endif    

#if 0
	if (m_EnableRenderDuringWindowMovement && m_SwapChainFramebuffers.size() > 0)
	{
		if (m_callbacks.beforeFrame) m_callbacks.beforeFrame(*this);
		AnimateRenderPresent();
	}
#endif
}

void DeviceManager::KeyboardUpdate(int key, int scancode, int action, int mods)
{
	if (key == -1)
	{
		// filter unknown keys
		return;
	}

	for (auto it = m_vRenderPasses.crbegin(); it != m_vRenderPasses.crend(); it++)
	{
		bool ret = (*it)->KeyboardUpdate(key, scancode, action, mods);
		if (ret)
			break;
	}
}

void DeviceManager::KeyboardCharInput(unsigned int unicode, int mods)
{
	for (auto it = m_vRenderPasses.crbegin(); it != m_vRenderPasses.crend(); it++)
	{
		bool ret = (*it)->KeyboardCharInput(unicode, mods);
		if (ret)
			break;
	}
}

void DeviceManager::MousePosUpdate(double xpos, double ypos)
{
	xpos /= m_DPIScaleFactorX;
	ypos /= m_DPIScaleFactorY;

#if 0
	for (auto it = m_vRenderPasses.crbegin(); it != m_vRenderPasses.crend(); it++)
	{
		bool ret = (*it)->MousePosUpdate(xpos, ypos);
		if (ret)
			break;
	}
#endif
}

void DeviceManager::MouseButtonUpdate(int button, int action, int mods)
{
	for (auto it = m_vRenderPasses.crbegin(); it != m_vRenderPasses.crend(); it++)
	{
		bool ret = (*it)->MouseButtonUpdate(button, action, mods);
		if (ret)
			break;
	}
}

void DeviceManager::MouseScrollUpdate(double xoffset, double yoffset)
{
	for (auto it = m_vRenderPasses.crbegin(); it != m_vRenderPasses.crend(); it++)
	{
		bool ret = (*it)->MouseScrollUpdate(xoffset, yoffset);
		if (ret)
			break;
	}
}

void JoyStickManager::EnumerateJoysticks()
{
	// The glfw header says nothing about what values to expect for joystick IDs. Empirically, having connected two
	// simultaneously, glfw just seems to number them starting at 0.
	for (int i = 0; i != 10; ++i)
		if (glfwJoystickPresent(i))
			m_JoystickIDs.push_back(i);
}

void JoyStickManager::EraseDisconnectedJoysticks()
{
	while (!m_RemovedJoysticks.empty())
	{
		auto id = m_RemovedJoysticks.back();
		m_RemovedJoysticks.pop_back();

		auto it = std::find(m_JoystickIDs.begin(), m_JoystickIDs.end(), id);
		if (it != m_JoystickIDs.end())
			m_JoystickIDs.erase(it);
	}
}

void JoyStickManager::ConnectJoystick(int id)
{
	m_JoystickIDs.push_back(id);
}

void JoyStickManager::DisconnectJoystick(int id)
{
	// This fn can be called from inside glfwGetJoystickAxes below (similarly for buttons, I guess).
	// We can't call m_JoystickIDs.erase() here and now. Save them for later. Forunately, glfw docs
	// say that you can query a joystick ID that isn't present.
	m_RemovedJoysticks.push_back(id);
}

void JoyStickManager::UpdateAllJoysticks(const std::list<IRenderPass*>& passes)
{
	for (auto j = m_JoystickIDs.begin(); j != m_JoystickIDs.end(); ++j)
		UpdateJoystick(*j, passes);
}

static void ApplyDeadZone(glm::vec2& v, const float deadZone = 0.1f)
{
	v *= std::max(glm::length(v) - deadZone, 0.f) / (1.f - deadZone);
}

void JoyStickManager::UpdateJoystick(int j, const std::list<IRenderPass*>& passes)
{
	GLFWgamepadstate gamepadState;
	glfwGetGamepadState(j, &gamepadState);

	float* axisValues = gamepadState.axes;

	auto updateAxis = [&](int axis, float axisVal)
	{
#if TODO
		for (auto it = passes.crbegin(); it != passes.crend(); it++)
		{
			bool ret = (*it)->JoystickAxisUpdate(axis, axisVal);
			if (ret)
				break;
		}
#endif
	};

	{
		glm::vec2 v(axisValues[GLFW_GAMEPAD_AXIS_LEFT_X], axisValues[GLFW_GAMEPAD_AXIS_LEFT_Y]);
		ApplyDeadZone(v);
		updateAxis(GLFW_GAMEPAD_AXIS_LEFT_X, v.x);
		updateAxis(GLFW_GAMEPAD_AXIS_LEFT_Y, v.y);
	}

	{
		glm::vec2 v(axisValues[GLFW_GAMEPAD_AXIS_RIGHT_X], axisValues[GLFW_GAMEPAD_AXIS_RIGHT_Y]);
		ApplyDeadZone(v);
		updateAxis(GLFW_GAMEPAD_AXIS_RIGHT_X, v.x);
		updateAxis(GLFW_GAMEPAD_AXIS_RIGHT_Y, v.y);
	}

	updateAxis(GLFW_GAMEPAD_AXIS_LEFT_TRIGGER, axisValues[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER]);
	updateAxis(GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER, axisValues[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER]);

	for (int b = 0; b != GLFW_GAMEPAD_BUTTON_LAST; ++b)
	{
		auto buttonVal = gamepadState.buttons[b];
		for (auto it = passes.crbegin(); it != passes.crend(); it++)
		{
			bool ret = (*it)->JoystickButtonUpdate(b, buttonVal == GLFW_PRESS);
			if (ret)
				break;
		}
	}
}

void DeviceManager::Shutdown()
{

	DestroyDevice();

	m_InstanceCreated = false;
}

void DeviceManager::SetInformativeWindowTitle(const char* applicationName, const char* extraInfo)
{
	std::stringstream ss;
	ss << applicationName;
	ss << " (" << nvrhi::utils::GraphicsAPIToString(GetDevice()->getGraphicsAPI());

	if (m_DeviceParams.enableDebugRuntime)
	{
		if (GetGraphicsAPI() == nvrhi::GraphicsAPI::VULKAN)
			ss << ", VulkanValidationLayer";
		else
			ss << ", DebugRuntime";
	}

	if (m_DeviceParams.enableNvrhiValidationLayer)
	{
		ss << ", NvrhiValidationLayer";
	}

	ss << ")";

	double frameTime = GetAverageFrameTimeSeconds();
	if (frameTime > 0)
	{
		ss << " - " << std::setprecision(4) << (1.0 / frameTime) << " FPS ";
	}

	if (extraInfo)
		ss << extraInfo;
}

Hazel::DeviceManager* Hazel::DeviceManager::Create(nvrhi::GraphicsAPI api, GLFWwindow* windowHandle)
{
	switch (api)
	{
#if HZ_HAS_DX11
		case nvrhi::GraphicsAPI::D3D11:
			return CreateD3D11();
#endif
#if HZ_HAS_DX12
		case nvrhi::GraphicsAPI::D3D12:
			return CreateD3D12();
#endif
#if HZ_HAS_VULKAN
		case nvrhi::GraphicsAPI::VULKAN:
			return CreateVK(windowHandle);
#endif
		default:
			HZ_CORE_ERROR("DeviceManager::Create: Unsupported Graphics API {0}", (uint8_t)api);
			return nullptr;
	}
}


DefaultMessageCallback& DefaultMessageCallback::GetInstance()
{
	static DefaultMessageCallback Instance;
	return Instance;
}

void DefaultMessageCallback::message(nvrhi::MessageSeverity severity, const char* messageText)
{
	switch (severity)
	{
		case nvrhi::MessageSeverity::Info:
			HZ_CORE_INFO("{0}", messageText);
			break;
		case nvrhi::MessageSeverity::Warning:
			HZ_CORE_WARN("{0}", messageText);
			break;
		case nvrhi::MessageSeverity::Error:
			HZ_CORE_ERROR("{0}", messageText);
			break;
		case nvrhi::MessageSeverity::Fatal:
			HZ_CORE_FATAL("{0}", messageText);
			break;
	}
}

static std::vector<const char*> stringSetToVector(const std::unordered_set<std::string>& set)
{
	std::vector<const char*> ret;
	for (const auto& s : set)
	{
		ret.push_back(s.c_str());
	}

	return ret;
}

bool VulkanDeviceManager::createInstance()
{
	if (!m_DeviceParams.headlessDevice)
	{
		if (!glfwVulkanSupported())
		{
			HZ_CORE_ERROR("GLFW reports that Vulkan is not supported. Perhaps missing a call to glfwInit()?");
			return false;
		}

		// add any extensions required by GLFW
		uint32_t glfwExtCount;
		const char** glfwExt = glfwGetRequiredInstanceExtensions(&glfwExtCount);
		HZ_CORE_VERIFY(glfwExt);

		for (uint32_t i = 0; i < glfwExtCount; i++)
		{
			enabledExtensions.instance.insert(std::string(glfwExt[i]));
		}
	}

	// add instance extensions requested by the user
	for (const std::string& name : m_DeviceParams.requiredVulkanInstanceExtensions)
	{
		enabledExtensions.instance.insert(name);
	}
	for (const std::string& name : m_DeviceParams.optionalVulkanInstanceExtensions)
	{
		optionalExtensions.instance.insert(name);
	}

	// add layers requested by the user
	for (const std::string& name : m_DeviceParams.requiredVulkanLayers)
	{
		enabledExtensions.layers.insert(name);
	}
	for (const std::string& name : m_DeviceParams.optionalVulkanLayers)
	{
		optionalExtensions.layers.insert(name);
	}

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
			ss << std::endl << "  - " << ext;

		HZ_CORE_ERROR("{}", ss.str().c_str());
		return false;
	}

	HZ_CORE_INFO("Enabled Vulkan instance extensions:");
	for (const auto& ext : enabledExtensions.instance)
	{
		HZ_CORE_INFO("    {}", ext.c_str());
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
			ss << std::endl << "  - " << ext;

		HZ_CORE_ERROR("{}", ss.str().c_str());
		return false;
	}

	HZ_CORE_INFO("Enabled Vulkan layers:");
	for (const auto& layer : enabledExtensions.layers)
	{
		HZ_CORE_INFO("    {}", layer.c_str());
	}

	auto instanceExtVec = stringSetToVector(enabledExtensions.instance);
	auto layerVec = stringSetToVector(enabledExtensions.layers);

	auto applicationInfo = vk::ApplicationInfo();

	// Query the Vulkan API version supported on the system to make sure we use at least 1.3 when that's present.
	vk::Result res = vk::enumerateInstanceVersion(&applicationInfo.apiVersion);

	if (res != vk::Result::eSuccess)
	{
		HZ_CORE_ERROR("Call to vkEnumerateInstanceVersion failed, error code = {}", nvrhi::vulkan::resultToString(VkResult(res)));
		return false;
	}

	const uint32_t minimumVulkanVersion = VK_MAKE_API_VERSION(0, 1, 3, 0);

	// Check if the Vulkan API version is sufficient.
	if (applicationInfo.apiVersion < minimumVulkanVersion)
	{
		HZ_CORE_ERROR("The Vulkan API version supported on the system ({}.{}.{}) is too low, at least {}.{}.{} is required.",
			VK_API_VERSION_MAJOR(applicationInfo.apiVersion), VK_API_VERSION_MINOR(applicationInfo.apiVersion), VK_API_VERSION_PATCH(applicationInfo.apiVersion),
			VK_API_VERSION_MAJOR(minimumVulkanVersion), VK_API_VERSION_MINOR(minimumVulkanVersion), VK_API_VERSION_PATCH(minimumVulkanVersion));
		return false;
	}

	// Spec says: A non-zero variant indicates the API is a variant of the Vulkan API and applications will typically need to be modified to run against it.
	if (VK_API_VERSION_VARIANT(applicationInfo.apiVersion) != 0)
	{
		HZ_CORE_ERROR("The Vulkan API supported on the system uses an unexpected variant: {}.", VK_API_VERSION_VARIANT(applicationInfo.apiVersion));
		return false;
	}

	// Create the vulkan instance
	vk::InstanceCreateInfo info = vk::InstanceCreateInfo()
		.setEnabledLayerCount(uint32_t(layerVec.size()))
		.setPpEnabledLayerNames(layerVec.data())
		.setEnabledExtensionCount(uint32_t(instanceExtVec.size()))
		.setPpEnabledExtensionNames(instanceExtVec.data())
		.setPApplicationInfo(&applicationInfo);

	res = vk::createInstance(&info, nullptr, &m_VulkanInstance);
	if (res != vk::Result::eSuccess)
	{
		HZ_CORE_ERROR("Failed to create a Vulkan instance, error code = {}", nvrhi::vulkan::resultToString(VkResult(res)));
		return false;
	}

	VULKAN_HPP_DEFAULT_DISPATCHER.init(m_VulkanInstance);

	return true;
}

void VulkanDeviceManager::installDebugCallback()
{
	auto info = vk::DebugReportCallbackCreateInfoEXT()
		.setFlags(vk::DebugReportFlagBitsEXT::eError |
				  vk::DebugReportFlagBitsEXT::eWarning |
				//   vk::DebugReportFlagBitsEXT::eInformation |
				  vk::DebugReportFlagBitsEXT::ePerformanceWarning)
		.setPfnCallback(vulkanDebugCallback)
		.setPUserData(this);

	vk::Result res = m_VulkanInstance.createDebugReportCallbackEXT(&info, nullptr, &m_DebugReportCallback);
	HZ_CORE_VERIFY(res == vk::Result::eSuccess);
}

bool VulkanDeviceManager::pickPhysicalDevice()
{
	VkFormat requestedFormat = nvrhi::vulkan::convertFormat(m_DeviceParams.swapChainFormat);
	vk::Extent2D requestedExtent(m_DeviceParams.backBufferWidth, m_DeviceParams.backBufferHeight);

	auto devices = m_VulkanInstance.enumeratePhysicalDevices();

	int firstDevice = 0;
	int lastDevice = int(devices.size()) - 1;
	if (m_DeviceParams.adapterIndex >= 0)
	{
		if (m_DeviceParams.adapterIndex > lastDevice)
		{
			HZ_CORE_ERROR("The specified Vulkan physical device {} does not exist.", m_DeviceParams.adapterIndex);
			return false;
		}
		firstDevice = m_DeviceParams.adapterIndex;
		lastDevice = m_DeviceParams.adapterIndex;
	}

	// Start building an error message in case we cannot find a device.
	std::stringstream errorStream;
	errorStream << "Cannot find a Vulkan device that supports all the required extensions and properties.";

	// build a list of GPUs
	std::vector<vk::PhysicalDevice> discreteGPUs;
	std::vector<vk::PhysicalDevice> otherGPUs;
	for (int deviceIndex = firstDevice; deviceIndex <= lastDevice; ++deviceIndex)
	{
		vk::PhysicalDevice const& dev = devices[deviceIndex];
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

	HZ_CORE_ERROR("{}", errorStream.str().c_str());

	return false;
}

bool VulkanDeviceManager::FindQueueFamilies(vk::PhysicalDevice physicalDevice)
{
	auto props = physicalDevice.getQueueFamilyProperties();

	for (int i = 0; i < int(props.size()); i++)
	{
		const auto& queueFamily = props[i];

		if (m_QueueFamilyIndices.Graphics == -1)
		{
			if (queueFamily.queueCount > 0 &&
				(queueFamily.queueFlags & vk::QueueFlagBits::eGraphics))
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

	if (m_QueueFamilyIndices.Graphics == -1 ||
		m_QueueFamilyIndices.Present == -1 && !m_DeviceParams.headlessDevice ||
		(m_QueueFamilyIndices.Compute == -1 && m_DeviceParams.enableComputeQueue) ||
		(m_QueueFamilyIndices.Transfer == -1 && m_DeviceParams.enableCopyQueue))
	{
		return false;
	}

	return true;
}

bool VulkanDeviceManager::createDevice()
{
	// figure out which optional extensions are supported
	auto deviceExtensions = m_VulkanPhysicalDevice.enumerateDeviceExtensionProperties();
	for (const auto& ext : deviceExtensions)
	{
		const std::string name = ext.extensionName;
		if (optionalExtensions.device.find(name) != optionalExtensions.device.end())
		{
			if (name == VK_KHR_SWAPCHAIN_MUTABLE_FORMAT_EXTENSION_NAME && m_DeviceParams.headlessDevice)
				continue;

			enabledExtensions.device.insert(name);
		}

		if (m_DeviceParams.enableRayTracingExtensions && m_RayTracingExtensions.find(name) != m_RayTracingExtensions.end())
		{
			enabledExtensions.device.insert(name);
		}
	}

	if (!m_DeviceParams.headlessDevice)
	{
		enabledExtensions.device.insert(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
	}

	const vk::PhysicalDeviceProperties physicalDeviceProperties = m_VulkanPhysicalDevice.getProperties();
	m_RendererString = std::string(physicalDeviceProperties.deviceName.data());

	bool accelStructSupported = false;
	bool rayPipelineSupported = false;
	bool rayQuerySupported = false;
	bool meshletsSupported = false;
	bool vrsSupported = false;
	bool synchronization2Supported = false;
	bool maintenance4Supported = false;

	HZ_CORE_INFO("Enabled Vulkan device extensions:");
	for (const auto& ext : enabledExtensions.device)
	{
		HZ_CORE_INFO("    {}", ext.c_str());

		if (ext == VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME)
			accelStructSupported = true;
		else if (ext == VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME)
			rayPipelineSupported = true;
		else if (ext == VK_KHR_RAY_QUERY_EXTENSION_NAME)
			rayQuerySupported = true;
		else if (ext == VK_NV_MESH_SHADER_EXTENSION_NAME)
			meshletsSupported = true;
		else if (ext == VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME)
			vrsSupported = true;
		else if (ext == VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME)
			synchronization2Supported = true;
		else if (ext == VK_KHR_MAINTENANCE_4_EXTENSION_NAME)
			maintenance4Supported = true;
		else if (ext == VK_KHR_SWAPCHAIN_MUTABLE_FORMAT_EXTENSION_NAME)
			m_SwapChainMutableFormatSupported = true;
	}

#define APPEND_EXTENSION(condition, desc) if (condition) { (desc).pNext = pNext; pNext = &(desc); }  // NOLINT(cppcoreguidelines-macro-usage)
	void* pNext = nullptr;

	vk::PhysicalDeviceFeatures2 physicalDeviceFeatures2;
	// Determine support for Buffer Device Address, the Vulkan 1.2 way
	auto bufferDeviceAddressFeatures = vk::PhysicalDeviceBufferDeviceAddressFeatures();
	// Determine support for maintenance4
	auto maintenance4Features = vk::PhysicalDeviceMaintenance4Features();

	// Put the user-provided extension structure at the end of the chain
	pNext = m_DeviceParams.physicalDeviceFeatures2Extensions;
	APPEND_EXTENSION(true, bufferDeviceAddressFeatures);
	APPEND_EXTENSION(maintenance4Supported, maintenance4Features);

	physicalDeviceFeatures2.pNext = pNext;
	m_VulkanPhysicalDevice.getFeatures2(&physicalDeviceFeatures2);

	std::unordered_set<int> uniqueQueueFamilies = {
		m_QueueFamilyIndices.Graphics };

	if (!m_DeviceParams.headlessDevice)
		uniqueQueueFamilies.insert(m_QueueFamilyIndices.Present);

	if (m_DeviceParams.enableComputeQueue)
		uniqueQueueFamilies.insert(m_QueueFamilyIndices.Compute);

	if (m_DeviceParams.enableCopyQueue)
		uniqueQueueFamilies.insert(m_QueueFamilyIndices.Transfer);

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

	auto accelStructFeatures = vk::PhysicalDeviceAccelerationStructureFeaturesKHR()
		.setAccelerationStructure(true);
	auto rayPipelineFeatures = vk::PhysicalDeviceRayTracingPipelineFeaturesKHR()
		.setRayTracingPipeline(true)
		.setRayTraversalPrimitiveCulling(true);
	auto rayQueryFeatures = vk::PhysicalDeviceRayQueryFeaturesKHR()
		.setRayQuery(true);
	auto meshletFeatures = vk::PhysicalDeviceMeshShaderFeaturesNV()
		.setTaskShader(true)
		.setMeshShader(true);
	auto vrsFeatures = vk::PhysicalDeviceFragmentShadingRateFeaturesKHR()
		.setPipelineFragmentShadingRate(true)
		.setPrimitiveFragmentShadingRate(true)
		.setAttachmentFragmentShadingRate(true);
	auto vulkan13features = vk::PhysicalDeviceVulkan13Features()
		.setSynchronization2(synchronization2Supported)
		.setMaintenance4(maintenance4Features.maintenance4);

	pNext = nullptr;
	APPEND_EXTENSION(accelStructSupported, accelStructFeatures)
		APPEND_EXTENSION(rayPipelineSupported, rayPipelineFeatures)
		APPEND_EXTENSION(rayQuerySupported, rayQueryFeatures)
		APPEND_EXTENSION(meshletsSupported, meshletFeatures)
		APPEND_EXTENSION(vrsSupported, vrsFeatures)
		APPEND_EXTENSION(physicalDeviceProperties.apiVersion >= VK_API_VERSION_1_3, vulkan13features)
		APPEND_EXTENSION(physicalDeviceProperties.apiVersion < VK_API_VERSION_1_3 && maintenance4Supported, maintenance4Features);
#undef APPEND_EXTENSION

	auto deviceFeatures = vk::PhysicalDeviceFeatures()
		.setShaderImageGatherExtended(true)
		.setSamplerAnisotropy(true)
		.setTessellationShader(true)
		.setTextureCompressionBC(true)
		.setGeometryShader(true)
		.setImageCubeArray(true)
		.setDualSrcBlend(true);

	auto vulkan12features = vk::PhysicalDeviceVulkan12Features()
		.setDescriptorIndexing(true)
		.setRuntimeDescriptorArray(true)
		.setDescriptorBindingPartiallyBound(true)
		.setDescriptorBindingVariableDescriptorCount(true)
		.setTimelineSemaphore(true)
		.setShaderSampledImageArrayNonUniformIndexing(true)
		.setBufferDeviceAddress(bufferDeviceAddressFeatures.bufferDeviceAddress)
		.setPNext(pNext);

	auto layerVec = stringSetToVector(enabledExtensions.layers);
	auto extVec = stringSetToVector(enabledExtensions.device);

	auto deviceDesc = vk::DeviceCreateInfo()
		.setPQueueCreateInfos(queueDesc.data())
		.setQueueCreateInfoCount(uint32_t(queueDesc.size()))
		.setPEnabledFeatures(&deviceFeatures)
		.setEnabledExtensionCount(uint32_t(extVec.size()))
		.setPpEnabledExtensionNames(extVec.data())
		.setEnabledLayerCount(uint32_t(layerVec.size()))
		.setPpEnabledLayerNames(layerVec.data())
		.setPNext(&vulkan12features);

	if (m_DeviceParams.deviceCreateInfoCallback)
		m_DeviceParams.deviceCreateInfoCallback(deviceDesc);

	const vk::Result res = m_VulkanPhysicalDevice.createDevice(&deviceDesc, nullptr, &m_VulkanDevice);
	if (res != vk::Result::eSuccess)
	{
		HZ_CORE_ERROR("Failed to create a Vulkan physical device, error code = {}", nvrhi::vulkan::resultToString(VkResult(res)));
		return false;
	}

	m_VulkanDevice.getQueue(m_QueueFamilyIndices.Graphics, 0, &m_GraphicsQueue);
	if (m_DeviceParams.enableComputeQueue)
		m_VulkanDevice.getQueue(m_QueueFamilyIndices.Compute, 0, &m_ComputeQueue);
	if (m_DeviceParams.enableCopyQueue)
		m_VulkanDevice.getQueue(m_QueueFamilyIndices.Transfer, 0, &m_TransferQueue);
	if (!m_DeviceParams.headlessDevice)
		m_VulkanDevice.getQueue(m_QueueFamilyIndices.Present, 0, &m_PresentQueue);

	VULKAN_HPP_DEFAULT_DISPATCHER.init(m_VulkanDevice);

	// remember the bufferDeviceAddress feature enablement
	m_BufferDeviceAddressSupported = vulkan12features.bufferDeviceAddress;

	HZ_CORE_INFO("Created Vulkan device: {}", m_RendererString.c_str());

	return true;
}

#define CHECK(a) if (!(a)) { return false; }

bool VulkanDeviceManager::CreateInstanceInternal()
{
	if (m_DeviceParams.enableDebugRuntime)
	{
		enabledExtensions.instance.insert("VK_EXT_debug_report");
		enabledExtensions.layers.insert("VK_LAYER_KHRONOS_validation");
	}

	PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr =
		m_dynamicLoader.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
	VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);

	return createInstance();
}

bool VulkanDeviceManager::EnumerateAdapters(std::vector<AdapterInfo>& outAdapters)
{
	if (!m_VulkanInstance)
		return false;

	std::vector<vk::PhysicalDevice> devices = m_VulkanInstance.enumeratePhysicalDevices();
	outAdapters.clear();

	for (auto physicalDevice : devices)
	{
		vk::PhysicalDeviceProperties properties = physicalDevice.getProperties();

		AdapterInfo adapterInfo;
		adapterInfo.name = properties.deviceName.data();
		adapterInfo.vendorID = properties.vendorID;
		adapterInfo.deviceID = properties.deviceID;
		adapterInfo.vkPhysicalDevice = physicalDevice;
		adapterInfo.dedicatedVideoMemory = 0;

		// Go through the memory types to figure out the amount of VRAM on this physical device.
		vk::PhysicalDeviceMemoryProperties memoryProperties = physicalDevice.getMemoryProperties();
		for (uint32_t heapIndex = 0; heapIndex < memoryProperties.memoryHeapCount; ++heapIndex)
		{
			vk::MemoryHeap const& heap = memoryProperties.memoryHeaps[heapIndex];
			if (heap.flags & vk::MemoryHeapFlagBits::eDeviceLocal)
			{
				adapterInfo.dedicatedVideoMemory += heap.size;
			}
		}

		outAdapters.push_back(std::move(adapterInfo));
	}

	return true;
}

bool VulkanDeviceManager::CreateDevice()
{
	if (m_DeviceParams.enableDebugRuntime)
	{
		installDebugCallback();
	}

	// add device extensions requested by the user
	for (const std::string& name : m_DeviceParams.requiredVulkanDeviceExtensions)
	{
		enabledExtensions.device.insert(name);
	}
	for (const std::string& name : m_DeviceParams.optionalVulkanDeviceExtensions)
	{
		optionalExtensions.device.insert(name);
	}

	if (!m_DeviceParams.headlessDevice)
	{
		// Need to adjust the swap chain format before creating the device because it affects physical device selection
		if (m_DeviceParams.swapChainFormat == nvrhi::Format::SRGBA8_UNORM)
			m_DeviceParams.swapChainFormat = nvrhi::Format::SBGRA8_UNORM;
		else if (m_DeviceParams.swapChainFormat == nvrhi::Format::RGBA8_UNORM)
			m_DeviceParams.swapChainFormat = nvrhi::Format::BGRA8_UNORM;
	}
	CHECK(pickPhysicalDevice())
		CHECK(FindQueueFamilies(m_VulkanPhysicalDevice))
		CHECK(createDevice())

		auto vecInstanceExt = stringSetToVector(enabledExtensions.instance);
	auto vecLayers = stringSetToVector(enabledExtensions.layers);
	auto vecDeviceExt = stringSetToVector(enabledExtensions.device);

	nvrhi::vulkan::DeviceDesc deviceDesc;
	deviceDesc.errorCB = &DefaultMessageCallback::GetInstance();
	deviceDesc.instance = m_VulkanInstance;
	deviceDesc.physicalDevice = m_VulkanPhysicalDevice;
	deviceDesc.device = m_VulkanDevice;
	deviceDesc.graphicsQueue = m_GraphicsQueue;
	deviceDesc.graphicsQueueIndex = m_QueueFamilyIndices.Graphics;
	if (m_DeviceParams.enableComputeQueue)
	{
		deviceDesc.computeQueue = m_ComputeQueue;
		deviceDesc.computeQueueIndex = m_QueueFamilyIndices.Compute;
	}
	if (m_DeviceParams.enableCopyQueue)
	{
		deviceDesc.transferQueue = m_TransferQueue;
		deviceDesc.transferQueueIndex = m_QueueFamilyIndices.Transfer;
	}
	deviceDesc.instanceExtensions = vecInstanceExt.data();
	deviceDesc.numInstanceExtensions = vecInstanceExt.size();
	deviceDesc.deviceExtensions = vecDeviceExt.data();
	deviceDesc.numDeviceExtensions = vecDeviceExt.size();
	deviceDesc.bufferDeviceAddressSupported = m_BufferDeviceAddressSupported;

	m_NvrhiDevice = nvrhi::vulkan::createDevice(deviceDesc);

	if (m_DeviceParams.enableNvrhiValidationLayer)
	{
		m_ValidationLayer = nvrhi::validation::createValidationLayer(m_NvrhiDevice);
	}

	return true;
}

bool VulkanDeviceManager::InitSurfaceCapabilities(uint64_t surfaceHandle)
{
#if TODO
	vk::SurfaceKHR windowSurface = (VkSurfaceKHR)surfaceHandle;

	if (windowSurface)
	{
		// check that this device supports our intended swap chain creation parameters
		auto surfaceCaps = dev.getSurfaceCapabilitiesKHR(windowSurface);
		auto surfaceFmts = dev.getSurfaceFormatsKHR(windowSurface);
		auto surfacePModes = dev.getSurfacePresentModesKHR(windowSurface);

		if (surfaceCaps.minImageCount > m_DeviceParams.swapChainBufferCount ||
			(surfaceCaps.maxImageCount < m_DeviceParams.swapChainBufferCount && surfaceCaps.maxImageCount > 0))
		{
			errorStream << std::endl << "  - cannot support the requested swap chain image count:";
			errorStream << " requested " << m_DeviceParams.swapChainBufferCount << ", available " << surfaceCaps.minImageCount << " - " << surfaceCaps.maxImageCount;
			deviceIsGood = false;
		}

		if (surfaceCaps.minImageExtent.width > requestedExtent.width ||
			surfaceCaps.minImageExtent.height > requestedExtent.height ||
			surfaceCaps.maxImageExtent.width < requestedExtent.width ||
			surfaceCaps.maxImageExtent.height < requestedExtent.height)
		{
			errorStream << std::endl << "  - cannot support the requested swap chain size:";
			errorStream << " requested " << requestedExtent.width << "x" << requestedExtent.height << ", ";
			errorStream << " available " << surfaceCaps.minImageExtent.width << "x" << surfaceCaps.minImageExtent.height;
			errorStream << " - " << surfaceCaps.maxImageExtent.width << "x" << surfaceCaps.maxImageExtent.height;
			deviceIsGood = false;
		}

		bool surfaceFormatPresent = false;
		for (const vk::SurfaceFormatKHR& surfaceFmt : surfaceFmts)
		{
			if (surfaceFmt.format == vk::Format(requestedFormat))
			{
				surfaceFormatPresent = true;
				break;
			}
		}

		if (!surfaceFormatPresent)
		{
			// can't create a swap chain using the format requested
			errorStream << std::endl << "  - does not support the requested swap chain format";
			deviceIsGood = false;
		}

		// check that we can present from the graphics queue
		uint32_t canPresent = dev.getSurfaceSupportKHR(m_QueueFamilyIndices.Graphics, windowSurface);
		if (!canPresent)
		{
			errorStream << std::endl << "  - cannot present";
			deviceIsGood = false;
		}
	}
#endif
	return true;
}

#undef CHECK

void VulkanDeviceManager::DestroyDevice()
{
	m_NvrhiDevice = nullptr;
	m_ValidationLayer = nullptr;
	m_RendererString.clear();

	if (m_VulkanDevice)
	{
		m_VulkanDevice.destroy();
		m_VulkanDevice = nullptr;
	}

	if (m_DebugReportCallback)
	{
		m_VulkanInstance.destroyDebugReportCallbackEXT(m_DebugReportCallback);
	}

	if (m_VulkanInstance)
	{
		m_VulkanInstance.destroy();
		m_VulkanInstance = nullptr;
	}
}

DeviceManager* DeviceManager::CreateVK(GLFWwindow* windowHandle)
{
	return new VulkanDeviceManager(windowHandle);
}

#endif
