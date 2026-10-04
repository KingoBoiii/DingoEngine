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

	glm::vec3 ViewDirection(float pitchDegrees)
	{
		const float pitch = glm::radians(pitchDegrees);
		return glm::vec3(0.0f, std::sin(pitch), std::cos(pitch));
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
		const glm::vec3 direction = ViewDirection(m_PitchDegrees);

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
		const glm::vec3 eye = m_Target + ViewDirection(m_PitchDegrees) * distance;
		const glm::mat4 view = glm::lookAt(eye, m_Target, glm::vec3(0.0f, 1.0f, 0.0f));

		auto& transform = m_Entity.GetComponent<Transform3DComponent>();
		transform.Position = eye;
		transform.Rotation = glm::quat_cast(glm::inverse(view));
	}

}
