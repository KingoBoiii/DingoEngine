#pragma once
#include "GamepadEvent.h"

namespace Dingo
{

	/// A gamepad was plugged in while the app runs.
	class GamepadConnectedEvent : public GamepadEvent
	{
	public:
		/// Raised by the window for the pad in slot gamepadId.
		GamepadConnectedEvent(uint32_t gamepadId, GamepadType type, const std::string& name)
			: GamepadEvent(gamepadId, type, name)
		{}

		/// "GamepadConnectedEvent: <id> (<type>, '<name>')".
		std::string ToString() const override
		{
			std::stringstream ss;
			ss << "GamepadConnectedEvent: " << m_GamepadId << " (" << m_Type << ", '" << m_Name << "')";
			return ss.str();
		}

		DE_EVENT_CLASS_TYPE(GamepadConnected)
	};

}
