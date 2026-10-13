#pragma once
#include "GamepadEvent.h"

namespace Dingo
{

	/// A gamepad was unplugged. Its type and name are the ones it had while connected.
	class GamepadDisconnectedEvent : public GamepadEvent
	{
	public:
		/// Raised by the window for the pad that was in slot gamepadId.
		GamepadDisconnectedEvent(uint32_t gamepadId, GamepadType type, const std::string& name)
			: GamepadEvent(gamepadId, type, name)
		{}

		/// "GamepadDisconnectedEvent: <id> (<type>, '<name>')".
		std::string ToString() const override
		{
			std::stringstream ss;
			ss << "GamepadDisconnectedEvent: " << m_GamepadId << " (" << m_Type << ", '" << m_Name << "')";
			return ss.str();
		}

		DE_EVENT_CLASS_TYPE(GamepadDisconnected)
	};

}
