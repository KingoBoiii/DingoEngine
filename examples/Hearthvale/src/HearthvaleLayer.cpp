#include "HearthvaleLayer.h"
#include "Checks/Checks.h"
#include "GameTuning.h"
#include "LaunchOptions.h"
#include "Look/ArtRegistry.h"
#include "World/ValleyLayout.h"

#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Dingo
{

	HearthvaleLayer::HearthvaleLayer()
		: Layer("Hearthvale")
	{}

	HearthvaleLayer::~HearthvaleLayer() = default;

	void HearthvaleLayer::OnAttach()
	{
		const LaunchOptions& options = GetLaunchOptions();
		m_Director = std::make_unique<ValleyDirector>(MakeDirectorConfig(options));
		m_Director->OnDayRoll(DayRollStep::Crops, [this](ValleyDirector&, DayRollReason)
		{
			if (m_World)
				m_World->OnDayRoll();
		});
		if (!Application::Get().IsHeadless())
		{
			Renderer3D& renderer = Application::Get().GetRenderer3D();
			renderer.SetCullingEnabled(!options.NoCull);
			renderer.SetInstancingEnabled(!options.NoInstancing);
		}
		m_ValleyScene = m_SceneManager.CreateScene(SCENE_VALLEY);
		m_ValleyScene->SetClearColor(COLOR_SKY);
		BuildValleyScene();
		m_SceneManager.SetActiveScene(SCENE_VALLEY);
	}

	void HearthvaleLayer::OnDetach()
	{
		m_StaticChecks.reset();
		m_World.reset();
		if (Scene* active = m_SceneManager.GetActiveScene())
			active->OnStop();
		m_SceneManager.Clear();
		m_ValleyScene = nullptr;
		m_Director.reset();
	}

	void HearthvaleLayer::BuildValleyScene()
	{
		m_World.reset();
		m_ValleyScene->Clear();
		if (Application::Get().IsHeadless())
			return;

		Entity ambient = m_ValleyScene->CreateEntity("Ambient");
		ambient.AddComponent<Transform3DComponent>();
		ambient.AddComponent<AmbientLightComponent>(AmbientLightComponent(AMBIENT_COLOR, AMBIENT_INTENSITY));

		Entity sun = m_ValleyScene->CreateEntity("Sun");
		DirectionalLightComponent& sunLight = sun.AddComponent<DirectionalLightComponent>();
		sunLight.Direction = SUN_DIRECTION;
		sunLight.Color = SUN_COLOR;
		sunLight.Intensity = SUN_INTENSITY;
		sunLight.CastShadows = !GetLaunchOptions().NoShadows;

		ValleyWorldOptions worldOptions;
		worldOptions.Static = !GetLaunchOptions().NoStatic;
		m_World = std::make_unique<ValleyWorld>(*m_ValleyScene, m_Director->GetGrid(), worldOptions);
		m_World->Sync();

		const LandmarkSpec& home = GetLandmarkSpec(Landmark::FarmHouse);
		Art::SpawnOnGround(*m_ValleyScene, Art::FARMER, "Farmer", (static_cast<float>(home.DoorX) + 0.5f) * TILE_SIZE,
			(static_cast<float>(home.DoorY) + 1.5f) * TILE_SIZE, GROUND_TOP);

		Entity camera = m_ValleyScene->CreateEntity("Camera");
		auto& cameraComponent = camera.AddComponent<CameraComponent>();
		cameraComponent.Type = CameraComponent::ProjectionType::Perspective;
		cameraComponent.FOV = CAMERA_FOV;
		cameraComponent.PerspNear = CAMERA_NEAR;
		cameraComponent.PerspFar = CAMERA_FAR;
		cameraComponent.Primary = true;
		auto& transform = camera.AddComponent<Transform3DComponent>();
		camera.AddComponent<AudioListenerComponent>();

		const float pitch = glm::radians(CAMERA_PITCH_DEG);
		const glm::vec3 target((static_cast<float>(FARM_PLOT.X) + 0.5f * static_cast<float>(FARM_PLOT.W)) * TILE_SIZE, GROUND_TOP,
			(static_cast<float>(FARM_PLOT.Y) + 0.5f * static_cast<float>(FARM_PLOT.H)) * TILE_SIZE);
		const glm::vec3 eye = target + glm::vec3(0.0f, std::sin(pitch), std::cos(pitch)) * CAMERA_DISTANCE;
		transform.Position = eye;
		transform.Rotation = glm::quat_cast(glm::inverse(glm::lookAt(eye, target, glm::vec3(0.0f, 1.0f, 0.0f))));
	}

	void HearthvaleLayer::RunChecks()
	{
		if (!m_SimChecksDone)
		{
			m_SimChecksDone = true;
			RunSimChecks(m_Report);
			if (!Application::Get().IsHeadless() && m_World)
				m_StaticChecks = std::make_unique<StaticCheckRun>();
		}
		if (m_StaticChecks && !m_StaticChecks->Tick(m_Report, *m_Director, *m_World, Application::Get().GetRenderer3D().GetStatistics().StaticCellsBaked))
			return;

		m_StaticChecks.reset();
		m_ChecksDone = true;
		DE_INFO("[Checks] {} passed, {} failed", m_Report.GetPassed(), m_Report.GetFailed());
		if (Application::Get().IsHeadless())
			Application::Get().Close();
	}

	void HearthvaleLayer::OnUpdate(float deltaTime)
	{
		const LaunchOptions& options = GetLaunchOptions();

		if (options.Check && !m_ChecksDone)
		{
			RunChecks();
			if (m_ChecksDone && Application::Get().IsHeadless())
				return;
		}

		const float step = options.FixedDt > 0.0f ? options.FixedDt : deltaTime;
		const int steps = options.FixedDt > 0.0f ? options.StepsPerFrame : 1;
		for (int i = 0; i < steps; ++i)
		{
			m_Director->Advance(step, SIM_MAX_STEPS_REAL_TIME);
			m_SceneManager.OnUpdate(step);
		}
		if (m_World)
			m_World->Sync();
		m_SceneManager.OnRender();
	}

}
