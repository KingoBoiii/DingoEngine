#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Dingo
{

	class PerspectiveCamera
	{
	public:
		PerspectiveCamera() = default;

		PerspectiveCamera(float fov, float aspectRatio, float nearClip, float farClip)
			: m_FOV(fov), m_AspectRatio(aspectRatio), m_NearClip(nearClip), m_FarClip(farClip)
		{
			RecalculateProjection();
			RecalculateView();
		}

	public:
		void SetPosition(const glm::vec3& position) { m_Position = position; RecalculateView(); }
		void SetTarget(const glm::vec3& target) { m_Target = target; RecalculateView(); }
		void SetUp(const glm::vec3& up) { m_Up = up; RecalculateView(); }
		void SetAspectRatio(float aspectRatio) { m_AspectRatio = aspectRatio; RecalculateProjection(); }
		void SetFOV(float fov) { m_FOV = fov; RecalculateProjection(); }
		void SetClip(float nearClip, float farClip) { m_NearClip = nearClip; m_FarClip = farClip; RecalculateProjection(); }

		const glm::vec3& GetPosition() const { return m_Position; }
		const glm::vec3& GetTarget() const { return m_Target; }
		float GetAspectRatio() const { return m_AspectRatio; }
		float GetFOV() const { return m_FOV; }

		const glm::mat4& GetViewMatrix() const { return m_ViewMatrix; }
		const glm::mat4& GetProjectionMatrix() const { return m_ProjectionMatrix; }
		const glm::mat4& GetViewProjectionMatrix() const { return m_ViewProjectionMatrix; }

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
