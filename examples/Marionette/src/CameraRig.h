#pragma once
#include <DingoEngine.h>

#include <vector>

namespace Dingo
{

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

}
