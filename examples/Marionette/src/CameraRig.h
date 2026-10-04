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

	// A fighter as the camera frames it: where its feet stand and how tall it is.
	struct CameraSubject
	{
		glm::vec3 Position{ 0.0f };
		float Height = 0.0f;
	};

	class CameraRig
	{
	public:
		// `screenCenterY` is where `target` should stand on the screen, in normalized device y (0 is the middle, negative
		// is lower): the camera looks above it by as much as that takes.
		CameraRig(Scene& scene, const glm::vec3& target, float pitchDegrees, std::vector<glm::vec3> fitPoints, float screenCenterY = 0.0f);

		void Update();

	private:
		glm::vec3 LookTarget(float distance) const;
		float FitDistance(float aspect) const;
		void Apply(float distance);

	private:
		Entity m_Entity;
		glm::vec3 m_Target;
		float m_PitchDegrees;
		float m_ScreenCenterY;
		std::vector<glm::vec3> m_FitPoints;
		float m_FittedAspect = 0.0f;
	};

	class FollowCamera
	{
	public:
		explicit FollowCamera(Scene& scene);

		// Frames both subjects, feet to head, between the controls hint at the bottom of the screen and the HUD bars at
		// the top, as close as the width of the pair and the depth of each allows.
		void Update(float deltaTime, const CameraSubject& first, const CameraSubject& second, bool snap);

		const glm::vec3& GetEye() const { return m_Eye; }

	private:
		struct Framing
		{
			glm::vec3 Target{ 0.0f };
			float Distance = 0.0f;
		};

		Framing Fit(const CameraSubject& first, const CameraSubject& second, float aspect) const;

	private:
		Entity m_Entity;
		glm::vec3 m_Target{ 0.0f };
		glm::vec3 m_Eye{ 0.0f };
		float m_Distance = 0.0f;
		float m_Aspect = 0.0f;
		bool m_Placed = false;
	};

}
