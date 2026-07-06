#pragma once
#include "GamepadEvent.h"

namespace Dingo
{

	class GamepadConnectedEvent : public GamepadEvent
	{
	public:
		GamepadConnectedEvent(uint32_t gamepadId)
			: GamepadEvent(gamepadId)
		{}

		std::string ToString() const override
		{
			std::stringstream ss;
			ss << "GamepadConnectedEvent: " << m_GamepadId;
			return ss.str();
		}

		EVENT_CLASS_TYPE(GamepadConnected)
	};

}
