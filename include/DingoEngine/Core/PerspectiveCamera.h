#pragma once
#include "DingoEngine/Core/Ray.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Dingo
{

	/// A right-handed look-at perspective camera with depth 0 to 1, keeping its view and projection
	/// matrices up to date: every setter rebuilds the matrix it affects at once.
	class PerspectiveCamera
	{
	public:
		/// Position (0, 5, 10) looking at the origin, y up, 45 degrees vertical FOV, 16:9, clip 0.1
		/// to 1000, with its matrices already computed.
		PerspectiveCamera()
		{
			RecalculateProjection();
			RecalculateView();
		}

		/// fov is the vertical field of view in degrees, aspectRatio is width / height and the
		/// clip distances are in world units. Position, target and up take their defaults.
		PerspectiveCamera(float fov, float aspectRatio, float nearClip, float farClip)
			: m_FOV(fov), m_AspectRatio(aspectRatio), m_NearClip(nearClip), m_FarClip(farClip)
		{
			RecalculateProjection();
			RecalculateView();
		}

	public:
		/// Each setter sets what its name says, in world space, and rebuilds the view matrix
		/// at once.
		///@{
		void SetPosition(const glm::vec3& position) { m_Position = position; RecalculateView(); }
		void SetTarget(const glm::vec3& target) { m_Target = target; RecalculateView(); }
		void SetUp(const glm::vec3& up) { m_Up = up; RecalculateView(); }
		///@}
		/// Width / height; rebuilds the projection matrix at once.
		void SetAspectRatio(float aspectRatio) { m_AspectRatio = aspectRatio; RecalculateProjection(); }
		/// Vertical field of view in degrees; rebuilds the projection matrix at once.
		void SetFOV(float fov) { m_FOV = fov; RecalculateProjection(); }
		/// Near and far plane distances in world units; rebuilds the projection matrix at once.
		void SetClip(float nearClip, float farClip) { m_NearClip = nearClip; m_FarClip = farClip; RecalculateProjection(); }

		/// World-space eye position.
		const glm::vec3& GetPosition() const { return m_Position; }
		/// World-space point the camera looks at.
		const glm::vec3& GetTarget() const { return m_Target; }
		/// Width / height.
		float GetAspectRatio() const { return m_AspectRatio; }
		/// Vertical field of view in degrees.
		float GetFOV() const { return m_FOV; }

		/// World to view space.
		const glm::mat4& GetViewMatrix() const { return m_ViewMatrix; }
		/// View to clip space (depth 0 to 1).
		const glm::mat4& GetProjectionMatrix() const { return m_ProjectionMatrix; }
		/// Projection * view: world to clip space.
		const glm::mat4& GetViewProjectionMatrix() const { return m_ViewProjectionMatrix; }

		/// Unprojects a screen/client pixel position (origin top-left, +Y down, matching
		/// Input::GetMousePosition()) into a world-space ray through this camera. viewportSize
		/// is the pixel size of the surface screenPos was sampled against (e.g. the window size).
		Ray ScreenPointToRay(const glm::vec2& screenPos, const glm::vec2& viewportSize) const
		{
			return Dingo::ScreenPointToRay(screenPos, viewportSize, m_ViewMatrix, m_ProjectionMatrix);
		}

		/// Projects a world-space point through this camera to a pixel position (origin top-left,
		/// +Y down) on a surface of viewportSize pixels; nullopt behind the camera. The inverse
		/// of ScreenPointToRay.
		std::optional<glm::vec2> WorldToScreenPoint(const glm::vec3& worldPos, const glm::vec2& viewportSize) const
		{
			return Dingo::WorldToScreenPoint(worldPos, viewportSize, m_ViewMatrix, m_ProjectionMatrix);
		}

	private:
		void RecalculateView()
		{
			m_ViewMatrix = glm::lookAt(m_Position, m_Target, m_Up);
			m_ViewProjectionMatrix = m_ProjectionMatrix * m_ViewMatrix;
		}

		void RecalculateProjection()
		{
			m_ProjectionMatrix = glm::perspective(glm::radians(m_FOV), m_AspectRatio, m_NearClip, m_FarClip);
			m_ViewProjectionMatrix = m_ProjectionMatrix * m_ViewMatrix;
		}

	private:
		glm::vec3 m_Position = { 0.0f, 5.0f, 10.0f };
		glm::vec3 m_Target = { 0.0f, 0.0f, 0.0f };
		glm::vec3 m_Up = { 0.0f, 1.0f, 0.0f };

		float m_FOV = 45.0f;
		float m_AspectRatio = 16.0f / 9.0f;
		float m_NearClip = 0.1f;
		float m_FarClip = 1000.0f;

		glm::mat4 m_ViewMatrix = glm::mat4(1.0f);
		glm::mat4 m_ProjectionMatrix = glm::mat4(1.0f);
		glm::mat4 m_ViewProjectionMatrix = glm::mat4(1.0f);
	};

}
