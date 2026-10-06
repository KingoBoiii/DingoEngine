#pragma once
#include "FighterIntent.h"
#include "LaunchOptions.h"

#include <glm/glm.hpp>

namespace Dingo
{

	// Scripted input for --drive, a function of time alone, so a run repeats exactly.
	class DriveBrain : public Brain
	{
	public:
		explicit DriveBrain(DriveMode mode) : m_Mode(mode) {}

		FighterIntent Think(float deltaTime, const Fighter& self, const Fighter& opponent) override;

	private:
		DriveMode m_Mode;
		float m_Time = 0.0f;
	};

	struct BoutLayout
	{
		glm::vec3 PlayerPosition{ 0.0f };
		float PlayerYawDegrees = 0.0f;
		glm::vec3 OpponentPosition{ 0.0f };
		float OpponentYawDegrees = 0.0f;
	};

	// Where the pair start: facing each other BOUT_DISTANCE apart, or placed so a drive has room.
	BoutLayout GetBoutLayout(DriveMode mode);

}
