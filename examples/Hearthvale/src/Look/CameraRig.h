#pragma once
#include <DingoEngine.h>

namespace Dingo
{

	// The three-quarter follow camera: yaw turns in quarter steps, the distance zooms, the focus lags the farmer.
	// GPU side: never constructed headless.
	class CameraRig
	{
	public:
		CameraRig(Scene& scene, const glm::vec3& focus);

		// `steps` quarter turns to add (any sign), `zoomAxis` held (positive = out), `zoomWheel` notches (positive = in).
		void Update(float dt, const glm::vec3& farmerFeet, int steps, float zoomAxis, float zoomWheel);

		// Scales how fast the yaw and zoom ease; 1 is the tuned speed.
		void SetSpeedScale(float scale) { m_SpeedScale = scale; }
		float GetSpeedScale() const { return m_SpeedScale; }

		float GetYaw() const { return m_Yaw; }
		float GetDistance() const { return m_Distance; }
		int GetSteps() const { return m_Steps; }
		Entity GetEntity() const { return m_Entity; }
		const glm::vec3& GetFocus() const { return m_Focus; }

	private:
		void Apply();

	private:
		Entity m_Entity;
		glm::vec3 m_Focus{ 0.0f };
		float m_Yaw = 0.0f;
		int m_Steps = 0;
		float m_Distance;
		float m_DistanceTarget;
		float m_SpeedScale = 1.0f;
	};

}
