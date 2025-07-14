#include "depch.h"
#include "ImGuiLayer.h"
#include "ImGuiRenderer.h"

#include "DingoEngine/Core/Application.h"
#include "DingoEngine/Graphics/Vulkan/VulkanGraphicsContext.h"

#include <nvrhi/nvrhi.h>
#include <vulkan/vulkan.hpp>

#include <imgui.h>

#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_glfw.cpp>

namespace Dingo
{

	void ImGuiLayer::OnAttach()
	{
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();

		ImGuiIO& io = ImGui::GetIO(); (void)io;
		SeupImGuiConfigFlags(io);

		ImGui::StyleColorsDark();

		// When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
		ImGuiStyle& style = ImGui::GetStyle();
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			style.WindowRounding = 0.0f;
			style.Colors[ImGuiCol_WindowBg].w = 1.0f;
		}
		style.Colors[ImGuiCol_WindowBg] = ImVec4(0.15f, 0.15f, 0.15f, style.Colors[ImGuiCol_WindowBg].w);

		bool result = ImGui_ImplGlfw_InitForVulkan(Dingo::Application::Get().GetWindow().m_WindowHandle, true);
		if (!result)
		{
			DE_CORE_ERROR("Failed to initialize ImGui for GLFW");
			return;
		}

		m_ImGuiRenderer = new ImGuiRenderer();
		m_ImGuiRenderer->Initialize();

		InitializePlatformInterface();
	}

	void ImGuiLayer::OnDetach()
	{
		m_ImGuiRenderer->Shutdown();

		ImGui_ImplGlfw_Shutdown();

		ImGui::DestroyContext();
	}

	void ImGuiLayer::Begin()
	{
		m_ImGuiRenderer->UpdateFontTexture();

		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
	}

	void ImGuiLayer::End()
	{
		ImGui::Render();

		m_ImGuiRenderer->RenderToSwapchain(ImGui::GetMainViewport(), Application::Get().GetWindow().GetSwapChain());

		// Update and Render additional Platform Windows
		if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
		}
	}

	void ImGuiLayer::SeupImGuiConfigFlags(ImGuiIO& io)
	{
		if (m_Params.EnableKeyboardNavigation)
		{
			io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
		}
		else
		{
			io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableKeyboard; // Disable Keyboard Controls
		}

		if (m_Params.EnableGamepadNavigation)
		{
			io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad; // Enable Gamepad Controls
		}
		else
		{
			io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableGamepad; // Disable Gamepad Controls
		}

		if (m_Params.EnableDocking)
		{
			io.ConfigFlags |= ImGuiConfigFlags_DockingEnable; // Enable Docking
		}
		else
		{
			io.ConfigFlags &= ~ImGuiConfigFlags_DockingEnable; // Disable Docking
		}

		if (m_Params.EnableViewports)
		{
			io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; // Enable Multi-Viewport / Platform Windows
		}
		else
		{
			io.ConfigFlags &= ~ImGuiConfigFlags_ViewportsEnable; // Disable Multi-Viewport / Platform Windows
		}
	}

	struct ImGuiViewportData
	{
		bool WindowOwned = false;
		SwapChain* SwapChain;
		std::unique_ptr<ImGuiRenderer> Renderer;
	};

	static void ImGuiRenderer_CreateWindow(ImGuiViewport* viewport)
	{
		ImGuiViewportData* data = IM_NEW(ImGuiViewportData)();
		viewport->RendererUserData = data;

		vk::Instance vInstance = static_cast<const VulkanGraphicsContext&>(GraphicsContext::Get()).GetVulkanInstance();

		// Create surface
		ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
		vk::SurfaceKHR surface;
		VkResult err = (VkResult)platform_io.Platform_CreateVkSurface(viewport, *(ImU64*)&vInstance, nullptr, (ImU64*)&surface);
		DE_CORE_ASSERT(err == VkResult::VK_SUCCESS);
		ImGui_ImplGlfw_ViewportData* vd = (ImGui_ImplGlfw_ViewportData*)viewport->PlatformUserData;

		SwapChainParams params = {
			.Width = (int32_t)viewport->Size.x,
			.Height = (int32_t)viewport->Size.y,
			.VulkanSurface = &surface,
		};

		data->SwapChain = SwapChain::Create(params);
		data->SwapChain->Initialize();
		data->WindowOwned = true;

		data->Renderer = std::make_unique<ImGuiRenderer>();
		data->Renderer->Initialize();
	}

	static void ImGuiRenderer_DestroyWindow(ImGuiViewport* viewport)
	{
		ImGuiViewportData* vd = (ImGuiViewportData*)viewport->RendererUserData;
		delete vd;
		viewport->RendererUserData = nullptr;
	}

	static void ImGuiRenderer_SetWindowSize(ImGuiViewport* viewport, ImVec2 size)
	{
		ImGuiViewportData* vd = (ImGuiViewportData*)viewport->RendererUserData;
		vd->SwapChain->Resize((uint32_t)size.x, (uint32_t)size.y);
	}

	static void ImGuiRenderer_RenderWindow(ImGuiViewport* viewport, void*)
	{
		ImGuiViewportData* vd = (ImGuiViewportData*)viewport->RendererUserData;
		vd->SwapChain->AcquireNextImage();
		vd->Renderer->UpdateFontTexture();
		vd->Renderer->RenderToSwapchain(viewport, vd->SwapChain);
	}

	static void ImGuiRenderer_SwapBuffers(ImGuiViewport* viewport, void*)
	{
		ImGuiViewportData* vd = (ImGuiViewportData*)viewport->RendererUserData;
		vd->SwapChain->Present();
	}

	void ImGuiLayer::InitializePlatformInterface()
	{
		ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
		if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			IM_ASSERT(platform_io.Platform_CreateVkSurface != NULL && "Platform needs to setup the CreateVkSurface handler.");
		}

		platform_io.Renderer_CreateWindow = ImGuiRenderer_CreateWindow;
		platform_io.Renderer_DestroyWindow = ImGuiRenderer_DestroyWindow;
		platform_io.Renderer_SetWindowSize = ImGuiRenderer_SetWindowSize;
		platform_io.Renderer_RenderWindow = ImGuiRenderer_RenderWindow;
		platform_io.Renderer_SwapBuffers = ImGuiRenderer_SwapBuffers;
	}


} // namespace Dingo
