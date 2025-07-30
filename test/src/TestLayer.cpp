#include "TestLayer.h"

#include "Tests/Graphics/StaticTriangleTest.h"
#include "Tests/Graphics/VertexBufferTest.h"
#include "Tests/Graphics/IndexBufferTest.h"
#include "Tests/Graphics/UniformBufferTest.h"
#include "Tests/Graphics/TextureTest.h"

#include "Tests/Renderer2D/ColorQuadTest.h"

#include <imgui.h>

namespace Dingo
{

	void TestLayer::OnAttach()
	{
		// Register tests
		m_Tests.push_back({ "Static Triangle Test", []() { return new StaticTriangleTest(); } });
		m_Tests.push_back({ "Vertex Buffer Test", []() { return new VertexBufferTest(); } });
		m_Tests.push_back({ "Index Buffer Test", []() { return new IndexBufferTest(); } });
		m_Tests.push_back({ "Uniform Buffer Test", []() { return new UniformBufferTest(); } });
		m_Tests.push_back({ "Texture Test", []() { return new TextureTest(); } });

		m_Tests.push_back({ "Colored Quad Test (R2D)", []() { return new ColorQuadTest(); } });

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
	}

	void TestLayer::OnUpdate(float deltaTime)
	{
		if (m_CurrentTest)
		{
			m_CurrentTest->Update(deltaTime);
			return;
		}

		Dingo::Renderer& appRenderer = Application::Get().GetRenderer();

		appRenderer.Begin();
		appRenderer.Clear({ 1.0f, 0.0f, 1.0f, 1.0f });
		appRenderer.End();
	}

	void TestLayer::OnImGuiRender()
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
				if (ImGui::MenuItem("Restart to Vulkan", NULL, false, false))
				{
				}

				if (ImGui::MenuItem("Restart to DirectX 12", NULL, false, false))
				{
				}

				if (ImGui::MenuItem("Restart to DirectX 11", NULL, false, false))
				{
				}

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
			if (ImGui::Selectable(test.first.c_str(), m_CurrentTest == test.second()))
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
				});
			}
		}

		ImGui::End();

		// Properties Panel
		ImGui::Begin("Properties", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

		ImGui::Text("Properties Panel");
		if (m_CurrentTest)
		{
			m_CurrentTest->ImGuiRender();
		}

		ImGui::End();

		m_TestViewportPanel.OnImGuiRender(m_CurrentTest->GetResult());
		m_CurrentTest->Resize(m_TestViewportPanel.GetViewportSize().x, m_TestViewportPanel.GetViewportSize().y);

		ImGui::ShowDemoWindow();
	}

}
