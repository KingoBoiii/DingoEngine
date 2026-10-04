#include "CameraRig.h"
#include "GameTuning.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cmath>
#include <utility>

namespace
{
	using namespace Dingo;

	glm::vec3 ViewDirection(float yawDegrees, float pitchDegrees)
	{
		const float yaw = glm::radians(yawDegrees);
		const float pitch = glm::radians(pitchDegrees);
		return glm::vec3(std::sin(yaw) * std::cos(pitch), std::sin(pitch), std::cos(yaw) * std::cos(pitch));
	}

	float ViewportAspect()
	{
		const glm::vec2 viewport = Application::Get().GetRenderer2D().GetViewportSize();
		return viewport.y > 0.0f ? viewport.x / viewport.y : 1.0f;
	}
}

namespace Dingo
{

	CameraRig::CameraRig(Scene& scene, const glm::vec3& target, float pitchDegrees, std::vector<glm::vec3> fitPoints)
		: m_Target(target), m_PitchDegrees(pitchDegrees), m_FitPoints(std::move(fitPoints))
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

		Update();
	}

	void CameraRig::Update()
	{
		const float aspect = ViewportAspect();
		if (aspect == m_FittedAspect)
			return;

		m_FittedAspect = aspect;
		Apply(FitDistance(aspect));
	}

	float CameraRig::FitDistance(float aspect) const
	{
		CameraComponent camera;
		camera.Type = CameraComponent::ProjectionType::Perspective;
		camera.FOV = CAMERA_FOV;
		camera.PerspNear = CAMERA_NEAR;
		camera.PerspFar = CAMERA_FAR;
		const glm::mat4 projection = camera.GetProjection(aspect);
		const glm::vec3 direction = ViewDirection(0.0f, m_PitchDegrees);

		for (float distance = CAMERA_FIT_START; distance < CAMERA_FIT_MAX; distance += CAMERA_FIT_STEP)
		{
			const glm::mat4 viewProjection = projection * glm::lookAt(m_Target + direction * distance, m_Target, glm::vec3(0.0f, 1.0f, 0.0f));
			const bool fits = std::all_of(m_FitPoints.begin(), m_FitPoints.end(), [&](const glm::vec3& point)
			{
				const glm::vec4 clip = viewProjection * glm::vec4(point, 1.0f);
				return clip.w > 0.0f && std::abs(clip.x) <= clip.w * CAMERA_FIT_MARGIN && std::abs(clip.y) <= clip.w * CAMERA_FIT_MARGIN;
			});
			if (fits)
				return distance;
		}
		return CAMERA_FIT_MAX;
	}

	void CameraRig::Apply(float distance)
	{
		const glm::vec3 eye = m_Target + ViewDirection(0.0f, m_PitchDegrees) * distance;
		const glm::mat4 view = glm::lookAt(eye, m_Target, glm::vec3(0.0f, 1.0f, 0.0f));

		auto& transform = m_Entity.GetComponent<Transform3DComponent>();
		transform.Position = eye;
		transform.Rotation = glm::quat_cast(glm::inverse(view));
	}

	GroundAxes GetArenaCameraAxes()
	{
		const float yaw = glm::radians(ARENA_CAMERA_YAW_DEG);
		GroundAxes axes;
		axes.Right = glm::vec2(std::cos(yaw), -std::sin(yaw));
		axes.Forward = glm::vec2(-std::sin(yaw), -std::cos(yaw));
		return axes;
	}

	FollowCamera::FollowCamera(Scene& scene)
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
	}

	void FollowCamera::Update(float deltaTime, const glm::vec3& first, const glm::vec3& second, bool snap)
	{
		const GroundAxes axes = GetArenaCameraAxes();
		const glm::vec2 apart(second.x - first.x, second.z - first.z);
		const float lateral = std::abs(glm::dot(apart, axes.Right));

		const float aspect = ViewportAspect();
		const float tanHorizontal = std::tan(glm::radians(CAMERA_FOV) * 0.5f) * aspect;
		const float needed = (0.5f * lateral + ARENA_CAMERA_SIDE_MARGIN) / (tanHorizontal * CAMERA_FIT_MARGIN);
		const float distance = std::clamp(needed, ARENA_CAMERA_MIN_DISTANCE, ARENA_CAMERA_MAX_DISTANCE);
		const glm::vec3 target = (first + second) * 0.5f + glm::vec3(0.0f, ARENA_CAMERA_LOOK_HEIGHT, 0.0f);

		const float blend = snap || !m_Placed || aspect != m_Aspect ? 1.0f : 1.0f - std::exp(-ARENA_CAMERA_SMOOTHING * deltaTime);
		m_Target += (target - m_Target) * blend;
		m_Distance += (distance - m_Distance) * blend;
		m_Placed = true;
		m_Aspect = aspect;

		const glm::vec3 eye = m_Target + ViewDirection(ARENA_CAMERA_YAW_DEG, ARENA_CAMERA_PITCH_DEG) * m_Distance;
		const glm::mat4 view = glm::lookAt(eye, m_Target, glm::vec3(0.0f, 1.0f, 0.0f));

		auto& transform = m_Entity.GetComponent<Transform3DComponent>();
		transform.Position = eye;
		transform.Rotation = glm::quat_cast(glm::inverse(view));
	}

}
