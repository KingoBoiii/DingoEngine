#include "depch.h"
#include "DingoEngine/Core/Application.h"
#include "DingoEngine/Core/Timer.h"
#include "DingoEngine/Core/Layer.h"
#include "DingoEngine/Core/Layers/EmptyLayer.h"
#include "DingoEngine/Graphics/AppRenderer.h"

#include "DingoEngine/Graphics/GraphicsContext.h"
#include "DingoEngine/ImGui/ImGuiLayer.h"
#include <DingoEngine/Graphics/NVRHI/NvrhiGraphicsContext.h>

namespace Dingo
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
		DE_CORE_ASSERT(s_Instance, "Application already initialized. Cannot initialize again.");

		m_Window = new Window(m_Params.Window);
		m_Window->SetEventCallback(DE_BIND_EVENT_FN(Application::OnEvent));
		m_Window->Initialize();

		m_GraphicsContext = GraphicsContext::Create(m_Params.Graphics);
		m_GraphicsContext->Initialize();

		m_SwapChain = SwapChain::Create(SwapChainParams()
			.SetNativeWindowHandle(m_Window->GetNativeWindowHandle())
			.SetWidth(m_Window->GetWidth())
			.SetHeight(m_Window->GetHeight())
			.SetVSync(m_Params.Window.VSync));
		m_SwapChain->Initialize();

		m_AppRenderer = new AppRenderer(m_SwapChain);
		m_AppRenderer->Initialize();

		m_Renderer2D = new Renderer2D(Renderer2DParams{ .TargetFramebuffer = m_SwapChain->GetCurrentFramebuffer() });
		m_Renderer2D->Initialize();

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
			m_ImGuiLayer = new ImGuiLayer(m_Params.ImGui);
			PushOverlay(m_ImGuiLayer);
		}
	}

	void Application::Destroy()
	{
		OnDestroy();

		m_LayerStack.Clear();

		if (m_AppRenderer)
		{
			m_AppRenderer->Shutdown();
			delete m_AppRenderer;
			m_AppRenderer = nullptr;
		}

		if (m_Renderer2D)
		{
			m_Renderer2D->Shutdown();
			delete m_Renderer2D;
			m_Renderer2D = nullptr;
		}

		if (m_Window)
		{
			m_Window->Shutdown();
			delete m_Window;
			m_Window = nullptr;
		}

		if (m_SwapChain)
		{
			m_SwapChain->Destroy();
			delete m_SwapChain;
			m_SwapChain = nullptr;
		}

		if (m_GraphicsContext)
		{
			m_GraphicsContext->Shutdown();
			//delete m_GraphicsContext;
			m_GraphicsContext = nullptr;
		}
	}

	void Application::OnEvent(Event& e)
	{
		EventDispatcher dispatcher(e);

		dispatcher.Dispatch<WindowCloseEvent>(DE_BIND_EVENT_FN(Application::OnWindowCloseEvent));
		dispatcher.Dispatch<WindowResizeEvent>(DE_BIND_EVENT_FN(Application::OnWindowResizeEvent));
	}

	void Application::Run()
	{
		Timer timer;

		while (m_IsRunning)
		{
			float time = timer.Elapsed();
			m_DeltaTime = time - m_LastFrameTime;
			m_LastFrameTime = time;

			m_Window->Update();

			m_AppRenderer->BeginFrame();

			for (Layer* layer : m_LayerStack)
			{
				layer->OnUpdate(m_DeltaTime);
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

			m_AppRenderer->EndFrame();

			m_GraphicsContext->RunGarbageCollection();

			// Execute any post-execution callbacks
			for (const auto& callback : m_PostExecutionCallbacks)
			{
				callback();
			}
			m_PostExecutionCallbacks.clear();
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

	void Application::Close()
	{
		m_IsRunning = false;
	}

	IRenderer& Application::GetAppRenderer() const
	{
		return *m_AppRenderer;
	}

	bool Application::OnWindowCloseEvent(WindowCloseEvent& e)
	{
		m_IsRunning = false; // Stop the application loop
		return true;
	}

	bool Application::OnWindowResizeEvent(WindowResizeEvent& e)
	{
		m_SwapChain->Resize(e.GetWidth(), e.GetHeight());
		return true;
	}

}
