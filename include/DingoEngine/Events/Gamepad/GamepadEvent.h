#pragma once
#include "DingoEngine/Core/GamepadCodes.h"
#include "DingoEngine/Events/Event.h"

#include <cstdint>
#include <string>

namespace Dingo
{

	/// Base of GamepadConnectedEvent and GamepadDisconnectedEvent; dispatch on one of those.
	/// Category: Gamepad and Input. Only changes after start-up are reported: a pad already
	/// plugged in at launch sends no event (poll Input::IsGamepadConnected for it).
	class GamepadEvent : public Event
	{
	public:
		/// The pad's slot index, the same one the Input gamepad calls take.
		inline uint32_t GetGamepadId() const { return m_GamepadId; }
		/// The detected hardware family; Unknown when it could not be told.
		inline GamepadType GetGamepadType() const { return m_Type; }
		/// The device name as the OS reports it; valid as long as the event.
		inline const std::string& GetGamepadName() const { return m_Name; }

		DE_EVENT_CLASS_CATEGORY(Gamepad | Input)

	protected:
		/// For subclasses: stores the slot, hardware family and name the getters return.
		GamepadEvent(uint32_t gamepadId, GamepadType type, const std::string& name)
			: m_GamepadId(gamepadId), m_Type(type), m_Name(name)
		{}

		/// The pad's slot index.
		uint32_t m_GamepadId;
		/// The detected hardware family.
		GamepadType m_Type;
		/// The device name.
		std::string m_Name;
	};

}
