#include "CameraRig.h"
#include "GameTuning.h"
#include "LaunchOptions.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cmath>
#include <iterator>

namespace
{
	using namespace Dingo;

	constexpr float k_FitStartDistance = 5.0f;
	constexpr float k_FitMaxDistance = 150.0f;
	constexpr float k_FitStep = 0.25f;

	glm::vec3 ViewDirection()
	{
		const float yaw = glm::radians(CAMERA_YAW_DEG);
		const float pitch = glm::radians(CAMERA_PITCH_DEG);
		return glm::vec3(std::sin(yaw) * std::cos(pitch), std::sin(pitch), std::cos(yaw) * std::cos(pitch));
	}

	glm::vec3 RoomCenter(const TileRect& room)
	{
		return glm::vec3((room.Col + room.Width * 0.5f) * TILE_SIZE, 0.0f, (room.Row + room.Height * 0.5f) * TILE_SIZE);
	}

	float ViewportAspect()
	{
		const glm::vec2 viewport = Application::Get().GetRenderer2D().GetViewportSize();
		return viewport.y > 0.0f ? viewport.x / viewport.y : 1.0f;
	}

	PostProcessSettings KeepPostSettings()
	{
		PostProcessSettings settings;
		settings.Enabled = true;
		settings.Tone.Operator = ToneMapOperator::Soft;
		settings.Bloom.Enabled = true;
		settings.Bloom.Intensity = BLOOM_INTENSITY;
		settings.Bloom.Threshold = BLOOM_THRESHOLD;
		return settings;
	}

	float FitDistance(const TileRect& room, float aspect)
	{
		CameraComponent camera;
		camera.Type = CameraComponent::ProjectionType::Perspective;
		camera.FOV = CAMERA_FOV;
		camera.PerspNear = CAMERA_NEAR;
		camera.PerspFar = CAMERA_FAR;
		const glm::mat4 projection = camera.GetProjection(aspect);

		const glm::vec3 center = RoomCenter(room);
		const glm::vec3 direction = ViewDirection();

		// The walls around the room count too, so a fitted room never loses its far wall.
		const float west = (room.Col - 1) * TILE_SIZE;
		const float east = (room.Col + room.Width + 1) * TILE_SIZE;
		const float north = (room.Row - 1) * TILE_SIZE;
		const float south = (room.Row + room.Height + 1) * TILE_SIZE;

		glm::vec3 corners[8];
		int count = 0;
		for (const float x : { west, east })
			for (const float z : { north, south })
				for (const float y : { 0.0f, WALL_HEIGHT })
					corners[count++] = { x, y, z };

		for (float distance = k_FitStartDistance; distance < k_FitMaxDistance; distance += k_FitStep)
		{
			const glm::mat4 viewProjection = projection * glm::lookAt(center + direction * distance, center, glm::vec3(0.0f, 1.0f, 0.0f));
			const bool fits = std::all_of(std::begin(corners), std::end(corners), [&](const glm::vec3& corner)
			{
				const glm::vec4 clip = viewProjection * glm::vec4(corner, 1.0f);
				return clip.w > 0.0f && std::abs(clip.x) <= clip.w * OVERVIEW_MARGIN && std::abs(clip.y) <= clip.w * OVERVIEW_MARGIN;
			});
			if (fits)
				return distance;
		}
		return k_FitMaxDistance;
	}
}

namespace Dingo
{

	CameraRig::CameraRig(Scene& scene, const glm::vec3& focus, const std::optional<TileRect>& overviewRoom)
		: m_Scene(scene), m_OverviewRoom(overviewRoom)
	{
		m_Entity = scene.CreateEntity("Camera");
		auto& camera = m_Entity.AddComponent<CameraComponent>();
		camera.Type = CameraComponent::ProjectionType::Perspective;
		camera.FOV = CAMERA_FOV;
		camera.PerspNear = CAMERA_NEAR;
		camera.PerspFar = CAMERA_FAR;
		camera.Primary = true;
		m_Entity.AddComponent<Transform3DComponent>();
		if (!GetLaunchOptions().NoPost)
			m_Entity.AddComponent<PostProcessComponent>().Settings = KeepPostSettings();

		Update(0.0f, focus);
	}

	void CameraRig::Update(float deltaTime, const glm::vec3& playerFeet)
	{
		if (m_OverviewRoom)
		{
			const float aspect = ViewportAspect();
			if (aspect != m_FittedAspect)
			{
				m_FittedAspect = aspect;
				m_FittedDistance = FitDistance(*m_OverviewRoom, aspect);
			}
			Apply(RoomCenter(*m_OverviewRoom), m_FittedDistance);
			return;
		}

		const glm::vec3 target = playerFeet + glm::vec3(0.0f, CAMERA_FOCUS_HEIGHT, 0.0f);
		m_Focus = deltaTime > 0.0f ? glm::mix(m_Focus, target, std::min(1.0f, CAMERA_LAG * deltaTime)) : target;
		Apply(m_Focus, CAMERA_DISTANCE);
	}

	void CameraRig::Apply(const glm::vec3& focus, float distance)
	{
		m_Focus = focus;
		m_Eye = focus + ViewDirection() * distance;

		const glm::mat4 view = glm::lookAt(m_Eye, focus, glm::vec3(0.0f, 1.0f, 0.0f));

		auto& transform = m_Entity.GetComponent<Transform3DComponent>();
		transform.Position = m_Eye;
		transform.Rotation = glm::quat_cast(glm::inverse(view));
	}

	glm::mat4 CameraRig::GetViewProjection() const
	{
		return m_Scene.GetCameraViewProjection(m_Entity, ViewportAspect());
	}

}
