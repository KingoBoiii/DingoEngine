#include "CameraRig.h"
#include "GameTuning.h"
#include "Hud.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <utility>

namespace
{
	using namespace Dingo;

	constexpr float k_Infinity = std::numeric_limits<float>::infinity();

	bool IsFinite(const glm::vec3& v)
	{
		return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
	}

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

	CameraRig::CameraRig(Scene& scene, const glm::vec3& target, float pitchDegrees, std::vector<glm::vec3> fitPoints, float screenCenterY)
		: m_Target(target), m_PitchDegrees(pitchDegrees), m_ScreenCenterY(screenCenterY), m_FitPoints(std::move(fitPoints))
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

	// Looking above the target by this much puts it m_ScreenCenterY down the screen: the camera moves up the view's
	// vertical by lift * cos(pitch), and one screen unit is distance * tan(fov / 2) there.
	glm::vec3 CameraRig::LookTarget(float distance) const
	{
		const float tanHalf = std::tan(glm::radians(CAMERA_FOV) * 0.5f);
		const float lift = -m_ScreenCenterY * distance * tanHalf / std::cos(glm::radians(m_PitchDegrees));
		return m_Target + glm::vec3(0.0f, lift, 0.0f);
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
			const glm::vec3 look = LookTarget(distance);
			const glm::mat4 viewProjection = projection * glm::lookAt(look + direction * distance, look, glm::vec3(0.0f, 1.0f, 0.0f));
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
		const glm::vec3 look = LookTarget(distance);
		const glm::vec3 eye = look + ViewDirection(0.0f, m_PitchDegrees) * distance;
		const glm::mat4 view = glm::lookAt(eye, look, glm::vec3(0.0f, 1.0f, 0.0f));

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

	FollowCamera::Framing FollowCamera::Fit(const CameraSubject& first, const CameraSubject& second, float aspect) const
	{
		const GroundAxes axes = GetArenaCameraAxes();
		const glm::vec3 side = glm::vec3(axes.Right.x, 0.0f, axes.Right.y) * ARENA_CAMERA_SIDE_MARGIN;
		std::array<glm::vec3, 8> points;
		size_t count = 0;
		for (const CameraSubject* subject : { &first, &second })
		{
			const float feet = subject->Position.y - ARENA_CAMERA_FEET_MARGIN;
			const float head = subject->Position.y + subject->Height + ARENA_CAMERA_HEAD_MARGIN;
			for (const float sign : { -1.0f, 1.0f })
			{
				for (const float y : { feet, head })
					points[count++] = glm::vec3(subject->Position.x, y, subject->Position.z) + side * sign;
			}
		}

		CameraComponent camera;
		camera.Type = CameraComponent::ProjectionType::Perspective;
		camera.FOV = CAMERA_FOV;
		camera.PerspNear = CAMERA_NEAR;
		camera.PerspFar = CAMERA_FAR;
		const glm::mat4 projection = camera.GetProjection(aspect);
		const glm::vec3 direction = ViewDirection(ARENA_CAMERA_YAW_DEG, ARENA_CAMERA_PITCH_DEG);
		const float tanHalf = std::tan(glm::radians(CAMERA_FOV) * 0.5f);
		const float cosPitch = std::cos(glm::radians(ARENA_CAMERA_PITCH_DEG));
		const float bottom = ARENA_CAMERA_BOTTOM_NDC;
		const float top = GetHudBottomNdc() - ARENA_CAMERA_TOP_PADDING;
		const glm::vec3 middle = (first.Position + second.Position) * 0.5f;

		struct Extent
		{
			float MaxX = 0.0f;
			float MinY = 0.0f;
			float MaxY = 0.0f;
			bool Valid = true;
		};
		auto project = [&](const glm::vec3& target, float distance)
		{
			const glm::mat4 viewProjection = projection * glm::lookAt(target + direction * distance, target, glm::vec3(0.0f, 1.0f, 0.0f));
			Extent extent;
			extent.MinY = k_Infinity;
			extent.MaxY = -k_Infinity;
			for (const glm::vec3& point : points)
			{
				const glm::vec4 clip = viewProjection * glm::vec4(point, 1.0f);
				if (!(clip.w > 0.0f))
				{
					extent.Valid = false;
					return extent;
				}
				extent.MaxX = std::max(extent.MaxX, std::abs(clip.x / clip.w));
				extent.MinY = std::min(extent.MinY, clip.y / clip.w);
				extent.MaxY = std::max(extent.MaxY, clip.y / clip.w);
			}
			return extent;
		};

		Framing framing;
		framing.Distance = ARENA_CAMERA_MAX_DISTANCE;
		framing.Target = glm::vec3(middle.x, ARENA_CAMERA_LOOK_HEIGHT, middle.z);
		for (float distance = ARENA_CAMERA_MIN_DISTANCE; distance <= ARENA_CAMERA_MAX_DISTANCE; distance += CAMERA_FIT_STEP)
		{
			// Moving the view up by a world unit lowers the points by cos(pitch) / (distance * tan(fov / 2)) of the
			// screen, so a few passes centre them in the free band.
			float height = ARENA_CAMERA_LOOK_HEIGHT;
			glm::vec3 target(middle.x, height, middle.z);
			Extent extent = project(target, distance);
			for (int pass = 0; pass < ARENA_CAMERA_CENTER_PASSES && extent.Valid; ++pass)
			{
				height -= (0.5f * (bottom + top) - 0.5f * (extent.MinY + extent.MaxY)) * distance * tanHalf / cosPitch;
				target.y = height;
				extent = project(target, distance);
			}

			framing.Target = target;
			framing.Distance = distance;
			if (extent.Valid && extent.MaxX <= CAMERA_FIT_MARGIN && extent.MinY >= bottom && extent.MaxY <= top)
				break;
		}
		return framing;
	}

	void FollowCamera::Update(float deltaTime, const CameraSubject& first, const CameraSubject& second, bool snap)
	{
		const float aspect = ViewportAspect();
		const Framing framing = Fit(first, second, aspect);
		if (!IsFinite(framing.Target) || !std::isfinite(framing.Distance))
			return;

		const float blend = snap || !m_Placed || aspect != m_Aspect ? 1.0f : 1.0f - std::exp(-ARENA_CAMERA_SMOOTHING * deltaTime);
		m_Target += (framing.Target - m_Target) * blend;
		m_Distance += (framing.Distance - m_Distance) * blend;
		m_Placed = true;
		m_Aspect = aspect;

		m_Eye = m_Target + ViewDirection(ARENA_CAMERA_YAW_DEG, ARENA_CAMERA_PITCH_DEG) * m_Distance;
		const glm::mat4 view = glm::lookAt(m_Eye, m_Target, glm::vec3(0.0f, 1.0f, 0.0f));

		auto& transform = m_Entity.GetComponent<Transform3DComponent>();
		transform.Position = m_Eye;
		transform.Rotation = glm::quat_cast(glm::inverse(view));
	}

}
