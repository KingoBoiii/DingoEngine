#include "depch.h"
#include "DingoEngine/Core/Application.h"
#include "DingoEngine/Core/Layer.h"
#include "DingoEngine/Core/Layers/EmptyLayer.h"

#include "DingoEngine/Graphics/GraphicsContext.h"
#include "DingoEngine/ImGui/ImGuiLayer.h"

namespace DingoEngine
{

	Application::Application(const ApplicationParams& params)
		: m_Params(params)
	{
		s_Instance = this;
	}

	Application::~Application()
	{
		s_Instance = nullptr;

		Destroy();
	}

	void Application::Initialize()
	{
		m_Window = new Window();
		m_Window->Initialize();

		OnInitialize();

		if (m_LayerStack.Empty())
		{
			// If no layers are pushed, we can push a default layer
			DE_CORE_ERROR("No layers pushed to the application. Pushing an empty layer.");
			PushLayer(new EmptyLayer());

			m_Params.EnableImGui = false; // Disable ImGui if no layers are pushed
			return;
		}

		if (m_Params.EnableImGui)
		{
			m_ImGuiLayer = new ImGuiLayer(m_Params.ImGuiParams);
			PushOverlay(m_ImGuiLayer);
		}
	}

	void Application::Destroy()
	{
		OnDestroy();

		m_LayerStack.Clear();

		if (m_Window)
		{
			m_Window->Shutdown();
			delete m_Window;
			m_Window = nullptr;
		}
	}

	void Application::Run()
	{
		SwapChain* swapChain = m_Window->GetSwapChain();
		auto deviceHandle = GraphicsContext::GetDeviceHandle();

		while (m_Window->IsRunning())
		{
			m_Window->Update();

			swapChain->AcquireNextImage();

			for (Layer* layer : m_LayerStack)
			{
				layer->OnUpdate();
			}

			if (m_Params.EnableImGui)
			{
				m_ImGuiLayer->Begin();
				for (Layer* layer : m_LayerStack)
				{
					layer->OnImGuiRender();
				}
				m_ImGuiLayer->End();
			}

			swapChain->Present();

			deviceHandle->waitForIdle();
			deviceHandle->runGarbageCollection();
		}
	}

	void Application::PushLayer(Layer* layer)
	{
		m_LayerStack.PushLayer(layer);
		layer->OnAttach();
	}

	void Application::PushOverlay(Layer* overlay)
	{
		m_LayerStack.PushOverlay(overlay);
		overlay->OnAttach();
	}

}
