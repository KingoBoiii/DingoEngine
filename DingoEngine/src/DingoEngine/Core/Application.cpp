#include "depch.h"
#include "DingoEngine/Core/Application.h"
#include "DingoEngine/Core/Layer.h"
#include "DingoEngine/Core/Layers/DefaultLayer.h"

#include "DingoEngine/Graphics/GraphicsContext.h"

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
