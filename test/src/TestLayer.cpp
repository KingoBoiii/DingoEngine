#include "TestLayer.h"

#include "Tests/Renderer/StaticTriangleTest.h"
#include "Tests/Renderer/VertexBufferTest.h"
#include "Tests/Renderer/IndexBufferTest.h"
#include "Tests/Renderer/UniformBufferTest.h"
#include "Tests/Renderer/TextureTest.h"
#include "Tests/Renderer/Mesh3DTest.h"
#include "Tests/Renderer/Model3DTest.h"

#include "Tests/Renderer2D/ColorQuadTest.h"
#include "Tests/Renderer2D/TextureQuadTest.h"
#include "Tests/Renderer2D/TextTest.h"
#include "Tests/Renderer2D/CircleTest.h"

#include <imgui.h>

namespace Dingo
{

	void TestLayer::OnAttach()
	{
		m_OutputFramebuffer = Framebuffer::Create(FramebufferParams()
			.SetDebugName("TestOutputFramebuffer")
			.SetWidth(800)
			.SetHeight(600)
			.SetEnableDepth(true)
			.AddAttachment({ TextureFormat::RGBA8_UNORM }));

		m_Renderer2D = Renderer2D::Create();

		// TODO: Low-level Renderer tests need off-screen pass (deferred feature)
		// m_Tests.push_back({ "Static Triangle Test", ... });
		// m_Tests.push_back({ "Vertex Buffer Test",   ... });
		// m_Tests.push_back({ "Index Buffer Test",    ... });
		// m_Tests.push_back({ "Uniform Buffer Test",  ... });
		// m_Tests.push_back({ "Texture Test",         ... });

		m_Tests.push_back({ "Color Quad Test (R2D)", [&]() { return new ColorQuadTest(m_Renderer2D); } });
		m_Tests.push_back({ "Texture Quad Test (R2D)", [&]() { return new TextureQuadTest(m_Renderer2D); } });
		m_Tests.push_back({ "Text Test (R2D)", [&]() { return new TextTest(m_Renderer2D); } });
		m_Tests.push_back({ "Circle Test (R2D)", [&]() { return new CircleTest(m_Renderer2D); } });
		m_Tests.push_back({ "Mesh 3D Test", []() { return new Mesh3DTest(); } });
		m_Tests.push_back({ "Model 3D Test", []() { return new Model3DTest(); } });

		m_CurrentTest = m_Tests[0].second();
		m_CurrentTest->Initialize();
	}

	void TestLayer::OnDetach()
	{
		if (m_CurrentTest)
		{
			m_CurrentTest->Cleanup();
			delete m_CurrentTest;
			m_CurrentTest = nullptr;
		}

		if (m_Renderer2D)
		{
			m_Renderer2D->Shutdown();
			delete m_Renderer2D;
			m_Renderer2D = nullptr;
		}

		if (m_OutputFramebuffer)
		{
			m_OutputFramebuffer->Destroy();
			m_OutputFramebuffer = nullptr;
		}
	}

	void TestLayer::OnUpdate(float deltaTime)
	{
		// The frame command list is managed by Renderer::BeginFrame/EndFrame.
		// No work needed here — ImGui owns the swapchain clear in this setup.

		if (m_CurrentTest)
		{
			Renderer::SetRenderTarget(m_OutputFramebuffer);
			m_CurrentTest->Update(deltaTime);
			Renderer::ResetRenderTarget();
		}
	}

	void TestLayer::OnUIRender()
	{
		// READ THIS !!!
		// TL;DR; this demo is more complicated than what most users you would normally use.
		// If we remove all options we are showcasing, this demo would become:
		//     void ShowExampleAppDockSpace()
		//     {
		//         ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());
		//     }
		// In most cases you should be able to just call DockSpaceOverViewport() and ignore all the code below!
		// In this specific demo, we are not using DockSpaceOverViewport() because:
		// - (1) we allow the host window to be floating/moveable instead of filling the viewport (when opt_fullscreen == false)
		// - (2) we allow the host window to have padding (when opt_padding == true)
		// - (3) we expose many flags and need a way to have them visible.
		// - (4) we have a local menu bar in the host window (vs. you could use BeginMainMenuBar() + DockSpaceOverViewport()
		//      in your code, but we don't here because we allow the window to be floating)

		static bool* p_open = new bool(true);
		static bool opt_fullscreen = true;
		static bool opt_padding = false;
		static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

		// We are using the ImGuiWindowFlags_NoDocking flag to make the parent window not dockable into,
		// because it would be confusing to have two docking targets within each others.
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
		if (opt_fullscreen)
		{
			const ImGuiViewport* viewport = ImGui::GetMainViewport();
			ImGui::SetNextWindowPos(viewport->WorkPos);
			ImGui::SetNextWindowSize(viewport->WorkSize);
			ImGui::SetNextWindowViewport(viewport->ID);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
			window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
			window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
		}
		else
		{
			dockspace_flags &= ~ImGuiDockNodeFlags_PassthruCentralNode;
		}

		// When using ImGuiDockNodeFlags_PassthruCentralNode, DockSpace() will render our background
		// and handle the pass-thru hole, so we ask Begin() to not render a background.
		if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
			window_flags |= ImGuiWindowFlags_NoBackground;

		// Important: note that we proceed even if Begin() returns false (aka window is collapsed).
		// This is because we want to keep our DockSpace() active. If a DockSpace() is inactive,
		// all active windows docked into it will lose their parent and become undocked.
		// We cannot preserve the docking relationship between an active window and an inactive docking, otherwise
		// any change of dockspace/settings would lead to windows being stuck in limbo and never being visible.
		if (!opt_padding)
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::Begin("DockSpace Demo", p_open, window_flags);
		if (!opt_padding)
			ImGui::PopStyleVar();

		if (opt_fullscreen)
			ImGui::PopStyleVar(2);

		// Submit the DockSpace
		ImGuiIO& io = ImGui::GetIO();
		if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
		{
			ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
			ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags | ImGuiDockNodeFlags_AutoHideTabBar);
		}

		if (ImGui::BeginMenuBar())
		{
			if (ImGui::BeginMenu("File"))
			{
				GraphicsAPI currentAPI = Application::Get().GetGraphicsContext().GetGraphicsAPI();

				if (ImGui::MenuItem("Restart to Vulkan", NULL, currentAPI == GraphicsAPI::Vulkan, currentAPI != GraphicsAPI::Vulkan))
					Application::Get().RequestRestart(GraphicsAPI::Vulkan);

				if (ImGui::MenuItem("Restart to DirectX 12", NULL, currentAPI == GraphicsAPI::DirectX12, currentAPI != GraphicsAPI::DirectX12))
					Application::Get().RequestRestart(GraphicsAPI::DirectX12);

				if (ImGui::MenuItem("Restart to DirectX 11", NULL, currentAPI == GraphicsAPI::DirectX11, currentAPI != GraphicsAPI::DirectX11))
					Application::Get().RequestRestart(GraphicsAPI::DirectX11);

				ImGui::Separator();

				if (ImGui::MenuItem("Exit", NULL, false, p_open != NULL))
				{
					Application::Get().Close();
				}
				ImGui::EndMenu();
			}

			ImGui::EndMenuBar();
		}

		ImGui::End();

		// Tests Panel
		ImGui::Begin("Tests", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

		for (auto& test : m_Tests)
		{
			if (ImGui::Selectable(test.first.c_str(), m_CurrentTestIndex == &test - &m_Tests[0]))
			{
				Application::Get().SubmitPostExecution([&]()
				{
					if (m_CurrentTest)
					{
						m_CurrentTest->Cleanup();
						delete m_CurrentTest;
					}
					m_CurrentTest = test.second();
					m_CurrentTest->Initialize();
					m_CurrentTestIndex = &test - &m_Tests[0];
				});
			}
		}

		ImGui::End();

		// Properties Panel
		ImGui::Begin("Properties", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

		const GraphicsContext& gfx = Application::Get().GetGraphicsContext();
		const AdapterInfo& adapter = gfx.GetAdapterInfo();

		const char* apiName = "Unknown";
		switch (gfx.GetGraphicsAPI())
		{
			case GraphicsAPI::Vulkan:    apiName = "Vulkan";    break;
			case GraphicsAPI::DirectX11: apiName = "DirectX 11"; break;
			case GraphicsAPI::DirectX12: apiName = "DirectX 12"; break;
			default: break;
		}

		if (ImGui::CollapsingHeader("GPU Info", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Text("API:    %s", apiName);
			ImGui::Text("GPU:    %s", adapter.Name.c_str());
			ImGui::Text("Vendor: %s", GraphicsContext::VendorName(adapter.VendorID).c_str());
			ImGui::Text("VRAM:   %.0f MB", adapter.DedicatedVideoMemory / (1024.0 * 1024.0));
		}

		ImGui::Separator();

		if (m_CurrentTest)
		{
			m_CurrentTest->ImGuiRender();
			
		}

		ImGui::End();

		m_TestViewportPanel.OnUIRender(m_OutputFramebuffer->GetAttachment(0));

		// handle resize
		if (m_OutputFramebuffer->GetWidth() != m_TestViewportPanel.GetViewportSize().x ||
		   m_OutputFramebuffer->GetHeight() != m_TestViewportPanel.GetViewportSize().y)
		{
			m_CurrentTest->Resize(m_TestViewportPanel.GetViewportSize().x, m_TestViewportPanel.GetViewportSize().y);

			Application::Get().SubmitPostExecution([&]()
			{
				m_OutputFramebuffer->Resize(static_cast<uint32_t>(m_TestViewportPanel.GetViewportSize().x), static_cast<uint32_t>(m_TestViewportPanel.GetViewportSize().y));
			});
		}

		ImGui::ShowDemoWindow();
	}

}
