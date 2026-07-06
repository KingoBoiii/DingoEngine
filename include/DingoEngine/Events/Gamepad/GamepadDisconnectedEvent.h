#pragma once
#include "GamepadEvent.h"

namespace Dingo
{

	class GamepadDisconnectedEvent : public GamepadEvent
	{
	public:
		GamepadDisconnectedEvent(uint32_t gamepadId)
			: GamepadEvent(gamepadId)
		{}

		std::string ToString() const override
		{
			std::stringstream ss;
			ss << "GamepadDisconnectedEvent: " << m_GamepadId;
			return ss.str();
		}

		EVENT_CLASS_TYPE(GamepadDisconnected)
	};

}
