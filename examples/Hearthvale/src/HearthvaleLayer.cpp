#include "HearthvaleLayer.h"
#include "Checks/Checks.h"
#include "GameTuning.h"
#include "LaunchOptions.h"
#include "Look/ArtRegistry.h"

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
		m_Director = std::make_unique<ValleyDirector>(MakeDirectorConfig(GetLaunchOptions()));
		m_ValleyScene = m_SceneManager.CreateScene(SCENE_VALLEY);
		m_ValleyScene->SetClearColor(COLOR_SKY);
		BuildValleyScene();
		m_SceneManager.SetActiveScene(SCENE_VALLEY);
	}

	void HearthvaleLayer::OnDetach()
	{
		if (Scene* active = m_SceneManager.GetActiveScene())
			active->OnStop();
		m_SceneManager.Clear();
		m_ValleyScene = nullptr;
		m_Director.reset();
	}

	void HearthvaleLayer::BuildValleyScene()
	{
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

		Art::Spawn(*m_ValleyScene, Art::GROUND, "Ground", glm::vec3(0.0f, -0.1f, 0.0f));
		Art::Spawn(*m_ValleyScene, Art::FARMER, "Farmer", glm::vec3(0.0f, 0.5f, 0.0f));

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
		const glm::vec3 target(0.0f, 0.5f, 0.0f);
		const glm::vec3 eye = target + glm::vec3(0.0f, std::sin(pitch), std::cos(pitch)) * CAMERA_DISTANCE;
		transform.Position = eye;
		transform.Rotation = glm::quat_cast(glm::inverse(glm::lookAt(eye, target, glm::vec3(0.0f, 1.0f, 0.0f))));
	}

	void HearthvaleLayer::OnUpdate(float deltaTime)
	{
		const LaunchOptions& options = GetLaunchOptions();

		if (options.Check && !m_ChecksDone)
		{
			m_ChecksDone = true;
			CheckReport report;
			RunSimChecks(report);
			DE_INFO("[Checks] {} passed, {} failed", report.GetPassed(), report.GetFailed());
			if (Application::Get().IsHeadless())
			{
				Application::Get().Close();
				return;
			}
		}

		const float step = options.FixedDt > 0.0f ? options.FixedDt : deltaTime;
		const int steps = options.FixedDt > 0.0f ? options.StepsPerFrame : 1;
		for (int i = 0; i < steps; ++i)
		{
			m_Director->Advance(step, SIM_MAX_STEPS_REAL_TIME);
			m_SceneManager.OnUpdate(step);
		}
		m_SceneManager.OnRender();
	}

}
