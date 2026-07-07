#pragma once
#include "DingoEngine/Core/GamepadCodes.h"
#include "DingoEngine/Events/Event.h"

#include <cstdint>
#include <string>

namespace Dingo
{

	class GamepadEvent : public Event
	{
	public:
		inline uint32_t GetGamepadId() const { return m_GamepadId; }
		inline GamepadType GetGamepadType() const { return m_Type; }
		inline const std::string& GetGamepadName() const { return m_Name; }

		EVENT_CLASS_CATEGORY(EventCategoryGamepad | EventCategoryInput)

	protected:
		GamepadEvent(uint32_t gamepadId, GamepadType type, const std::string& name)
			: m_GamepadId(gamepadId), m_Type(type), m_Name(name)
		{}

		uint32_t m_GamepadId;
		GamepadType m_Type;
		std::string m_Name;
	};

}
