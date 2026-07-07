#pragma once
#include "GamepadEvent.h"

namespace Dingo
{

	class GamepadConnectedEvent : public GamepadEvent
	{
	public:
		GamepadConnectedEvent(uint32_t gamepadId, GamepadType type, const std::string& name)
			: GamepadEvent(gamepadId, type, name)
		{}

		std::string ToString() const override
		{
			std::stringstream ss;
			ss << "GamepadConnectedEvent: " << m_GamepadId << " (" << m_Type << ", '" << m_Name << "')";
			return ss.str();
		}

		EVENT_CLASS_TYPE(GamepadConnected)
	};

}
