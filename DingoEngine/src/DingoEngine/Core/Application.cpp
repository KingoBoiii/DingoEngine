#include "depch.h"
#include "DingoEngine/Core/Application.h"
#include "DingoEngine/Core/Layer.h"
#include "DingoEngine/Core/Layers/DefaultLayer.h"

namespace DingoEngine
{

	Application::Application(const ApplicationParams& params)
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

		m_Renderer = Renderer::Create(m_Window->GetSwapChain());
		m_Renderer->Initialize();

		OnInitialize();

		if(m_LayerStack.Empty())
		{
			// If no layers are pushed, we can push a default layer
			DE_CORE_ERROR("No layers pushed to the application. Pushing a default layer.");
			PushLayer(new DefaultLayer());
		}
	}

	void Application::Destroy()
	{
		OnDestroy();

		m_LayerStack.Clear();

		if (m_Renderer)
		{
			m_Renderer->Destroy();
			delete m_Renderer;
			m_Renderer = nullptr;
		}

		if (m_Window)
		{
			m_Window->Shutdown();
			delete m_Window;
			m_Window = nullptr;
		}
	}

	void Application::Run()
	{
		while (m_Window->IsRunning())
		{
			m_Window->Update();

			m_Renderer->BeginFrame();
			for (Layer* layer : m_LayerStack)
			{
				layer->OnUpdate();
			}

			m_Renderer->EndFrame();
			m_Renderer->Present();
			m_Renderer->WaitAndClear();
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
