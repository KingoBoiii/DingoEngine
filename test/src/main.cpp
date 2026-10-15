#include <DingoEngine/EntryPoint.h>

#include "TestLayer.h"
#include "Tests/TestChecks.h"

#include <chrono>
#include <thread>

namespace Dingo
{

	// --graphics=headless: no window or GPU, so none of the TestLayer cases can run. This checks the
	// headless loop itself (paced OnUpdate, a scene stepping physics) and then closes the app, or with
	// --headless-wait keeps running until a signal (Ctrl+C, SIGTERM) ends it, and --headless-stall makes
	// each of those updates take a second, so a second signal can arrive before the loop reacts.
	class HeadlessCheckLayer : public Layer
	{
	public:
		HeadlessCheckLayer()
			: Layer("HeadlessCheckLayer")
		{}

		virtual void OnAttach() override
		{
			Scene* scene = m_Scenes.CreateScene("Headless");
			m_Body = scene->CreateEntity("Falling box");
			m_Body.AddComponent<Transform3DComponent>(Transform3DComponent({ 0.0f, 10.0f, 0.0f }));
			m_Body.AddComponent<RigidBody3DComponent>(RigidBody3DComponent(BodyType3D::Dynamic));
			m_Body.AddComponent<BoxCollider3DComponent>();
			m_Scenes.SetActiveScene("Headless");
		}

		virtual void OnDetach() override
		{
			m_Scenes.GetActiveScene()->OnStop();
			m_Scenes.Clear();
			DE_INFO("Headless: OnDetach ran.");
		}

		virtual void OnUpdate(float deltaTime) override
		{
			m_Scenes.OnUpdate(deltaTime);
			m_Scenes.OnRender();

			if (m_Updates >= k_Updates)
			{
				if (Application::Get().GetCommandLineArgs().Get("headless-stall"))
					std::this_thread::sleep_for(std::chrono::seconds(1));
				return;
			}
			if (++m_Updates > 1)
				m_SteppedTime += deltaTime;
			if (m_Updates < k_Updates)
				return;

			const Application& app = Application::Get();
			const float averageDelta = m_SteppedTime / float(k_Updates - 1);
			m_Checks.Check(app.IsHeadless() && Renderer::GetFrameIndex() == 0 && Renderer::GetWhiteTexture() == nullptr, "headless: renderer never started");
			m_Checks.Check(averageDelta >= 0.014f && averageDelta < 0.1f, "headless: OnUpdate paced near 60 Hz");
			m_Checks.Check(m_Body.GetComponent<Transform3DComponent>().Position.y < 9.5f, "headless: scene physics steps");
			DE_INFO("Headless: {} updates, average delta {:.2f} ms, {} failed.", m_Updates, averageDelta * 1000.0f, m_Checks.FailedCount());
			if (app.GetCommandLineArgs().Get("headless-wait"))
				DE_INFO("Headless: waiting for a stop signal.");
			else
				Application::Get().Close();
		}

	private:
		static constexpr int k_Updates = 30;

		SceneManager m_Scenes;
		Entity m_Body;
		TestChecks m_Checks;
		int m_Updates = 0;
		float m_SteppedTime = 0.0f;
	};

	class TestFrameworkApplication : public Application
	{
	public:
		TestFrameworkApplication(const Dingo::ApplicationParams& params)
			: Application(params)
		{}
		virtual ~TestFrameworkApplication() = default;

	protected:
		virtual void OnDestroy() override
		{
			if (IsHeadless())
				DE_INFO("Headless: OnDestroy ran.");
		}

		virtual void OnInitialize() override
		{
			if (IsHeadless())
				PushLayer(new HeadlessCheckLayer());
			else
				PushLayer(new TestLayer());
		}
	};

}

Dingo::Application* Dingo::CreateApplication(ApplicationCommandLineArgs args)
{
	ApplicationParams params = ApplicationParams{
		.CommandLineArgs = args,
		.Window = {
			.Title = "Dingo Test Framework",
			.Width = 1600,
			.Height = 900,
			.Resizable = true,
		},
		.Assets = {
			.EnableHotReload = true,
		},
		.EnableImGui = true,
		.ImGui = {
			.EnableDocking = true,
			.EnableViewports = false,
			// Every test draws inside an ImGui window, so ImGui wants the mouse over the game view.
			.BlockGameInput = false,
		},
		.UpdateInBackground = true,
	};

	return new TestFrameworkApplication(params);
}
