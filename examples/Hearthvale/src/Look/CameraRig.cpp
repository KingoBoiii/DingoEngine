#include "Look/CameraRig.h"
#include "GameTuning.h"
#include "LaunchOptions.h"
#include "Look/CameraMath.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cmath>

namespace Dingo
{

	namespace
	{
		PostProcessSettings ValleyPostSettings()
		{
			PostProcessSettings settings;
			settings.Enabled = true;
			settings.Tone.Operator = ToneMapOperator::Soft;
			settings.Bloom.Enabled = true;
			settings.Bloom.Intensity = BLOOM_INTENSITY;
			settings.Bloom.Threshold = BLOOM_THRESHOLD;
			settings.AmbientOcclusion.Enabled = true;
			return settings;
		}
	}

	CameraRig::CameraRig(Scene& scene, const glm::vec3& focus)
		: m_Focus(focus + glm::vec3(0.0f, CAMERA_FOCUS_HEIGHT, 0.0f)), m_Distance(CAMERA_DISTANCE), m_DistanceTarget(CAMERA_DISTANCE)
	{
		m_Entity = scene.CreateEntity("Camera");
		auto& camera = m_Entity.AddComponent<CameraComponent>();
		camera.Type = CameraComponent::ProjectionType::Perspective;
		camera.FOV = CAMERA_FOV;
		camera.PerspNear = CAMERA_NEAR;
		camera.PerspFar = CAMERA_FAR;
		camera.Primary = true;
		m_Entity.AddComponent<Transform3DComponent>();
		m_Entity.AddComponent<AudioListenerComponent>();
		if (!GetLaunchOptions().NoPost)
			m_Entity.AddComponent<PostProcessComponent>().Settings = ValleyPostSettings();

		Apply();
	}

	void CameraRig::Update(float dt, const glm::vec3& farmerFeet, int steps, float zoomAxis, float zoomWheel)
	{
		m_Steps += steps;
		m_Yaw = CameraMath::EaseYaw(m_Yaw, CameraMath::YawForSteps(m_Steps), dt);

		m_DistanceTarget = CameraMath::NextDistanceTarget(m_DistanceTarget, zoomAxis, zoomWheel, dt);
		m_Distance = CameraMath::EaseDistance(m_Distance, m_DistanceTarget, dt);

		const glm::vec3 target = farmerFeet + glm::vec3(0.0f, CAMERA_FOCUS_HEIGHT, 0.0f);
		m_Focus = dt > 0.0f ? glm::mix(m_Focus, target, std::min(1.0f, CAMERA_LAG * dt)) : target;
		Apply();
	}

	void CameraRig::Apply()
	{
		const glm::vec3 eye = m_Focus + CameraMath::EyeOffset(m_Yaw, glm::radians(CAMERA_PITCH_DEG), m_Distance);
		const glm::mat4 view = glm::lookAt(eye, m_Focus, glm::vec3(0.0f, 1.0f, 0.0f));

		auto& transform = m_Entity.GetComponent<Transform3DComponent>();
		transform.Position = eye;
		transform.Rotation = glm::quat_cast(glm::inverse(view));
	}

}
