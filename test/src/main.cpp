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

			CheckGpuLoadsFail();

			AssetManager& assets = Application::Get().GetAssetManager();
			const Model* duck = assets.GetModel(assets.Load("models/Duck/Duck.gltf"));
			m_Checks.Check(duck && duck->GetSubMeshCount() > 0 && duck->GetSubMeshes()[0].Mesh, "headless: model with a mesh loads");
			if (duck && duck->GetSubMeshCount() > 0)
			{
				m_Collider = scene->CreateEntity("Mesh collider");
				m_Collider.AddComponent<Transform3DComponent>(Transform3DComponent({ 100.0f, 0.0f, 0.0f }));
				m_Collider.AddComponent<RigidBody3DComponent>(RigidBody3DComponent(BodyType3D::Static));
				m_Collider.AddComponent<MeshCollider3DComponent>(MeshCollider3DComponent(duck->GetSubMeshes()[0].Mesh));
			}
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
			m_Checks.Check(m_Collider && m_Scenes.GetActiveScene()->GetRuntimeBody3D(m_Collider) != k_InvalidBody3D, "headless: a mesh collider from a model gets a body");
			DE_INFO("Headless: {} updates, average delta {:.2f} ms, {} failed.", m_Updates, averageDelta * 1000.0f, m_Checks.FailedCount());
			if (app.GetCommandLineArgs().Get("headless-wait"))
				DE_INFO("Headless: waiting for a stop signal.");
			else
				Application::Get().Close();
		}

	private:
		void CheckGpuLoadsFail()
		{
			AssetManager& assets = Application::Get().GetAssetManager();

			const AssetHandle texture = assets.Load("textures/container.jpg");
			m_Checks.Check(assets.GetState(texture) == AssetState::Failed && assets.GetTexture(texture) == nullptr, "headless: texture load fails");

			const AssetHandle asyncTexture = assets.LoadAsync("textures/hd2.png");
			m_Checks.Check(assets.GetState(asyncTexture) == AssetState::Failed && assets.GetTexture(asyncTexture) == nullptr, "headless: async texture load fails at once");

			m_Checks.Check(assets.GetFont(assets.Load("fonts/Inter-Regular.ttf")) == nullptr, "headless: font load fails");
			m_Checks.Check(assets.GetShader(assets.Load("shaders/asset_test.glsl")) == nullptr, "headless: shader load fails");

			m_Checks.Check(Texture::CreateFromFile("textures/container.jpg") == nullptr, "headless: Texture::CreateFromFile returns nullptr");
			m_Checks.Check(Sampler::Create(SamplerParams()) == nullptr, "headless: Sampler::Create returns nullptr");
			m_Checks.Check(GraphicsBuffer::CreateUniformBuffer(64) == nullptr, "headless: GraphicsBuffer::CreateUniformBuffer returns nullptr");

			const Model* fox = assets.GetModel(assets.Load("models/Fox/Fox.gltf"));
			m_Checks.Check(fox && fox->GetSkeleton() && fox->GetSubMeshCount() > 0 && !fox->GetSubMeshes()[0].DiffuseTexture, "headless: model loads with a skeleton and no textures");
		}

		static constexpr int k_Updates = 30;

		SceneManager m_Scenes;
		Entity m_Body;
		Entity m_Collider;
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
