#pragma once

#include <DingoEngine.h>

namespace Dingo
{

	class FlyCamera
	{
	public:
		void Attach(Entity camera);
		void Update(float deltaTime);
		void JumpToPreset(int preset);

		static glm::vec3 GetPresetPosition(int preset);
		int GetPreset() const { return m_Preset; }
		const glm::vec3& GetPosition() const { return m_Position; }
		float GetYawDegrees() const { return m_Yaw; }
		float GetPitchDegrees() const { return m_Pitch; }

	private:
		void Apply();

	private:
		Entity m_Camera;
		glm::vec3 m_Position{ 0.0f, 1.7f, 6.5f };
		float m_Yaw = 0.0f;
		float m_Pitch = 0.0f;
		int m_Preset = 1;
		bool m_Looking = false;
	};

}
