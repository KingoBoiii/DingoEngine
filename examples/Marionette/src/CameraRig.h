#pragma once
#include <DingoEngine.h>

#include <vector>

namespace Dingo
{

	struct GroundAxes
	{
		glm::vec2 Right{ 1.0f, 0.0f };
		glm::vec2 Forward{ 0.0f, -1.0f };
	};

	GroundAxes GetArenaCameraAxes();

	class CameraRig
	{
	public:
		CameraRig(Scene& scene, const glm::vec3& target, float pitchDegrees, std::vector<glm::vec3> fitPoints);

		void Update();

	private:
		float FitDistance(float aspect) const;
		void Apply(float distance);

	private:
		Entity m_Entity;
		glm::vec3 m_Target;
		float m_PitchDegrees;
		std::vector<glm::vec3> m_FitPoints;
		float m_FittedAspect = 0.0f;
	};

	class FollowCamera
	{
	public:
		explicit FollowCamera(Scene& scene);

		void Update(float deltaTime, const glm::vec3& first, const glm::vec3& second, bool snap);

	private:
		Entity m_Entity;
		glm::vec3 m_Target{ 0.0f };
		float m_Distance = 0.0f;
		float m_Aspect = 0.0f;
		bool m_Placed = false;
	};

}
