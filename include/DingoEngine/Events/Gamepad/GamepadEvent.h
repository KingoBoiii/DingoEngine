#pragma once
#include "DingoEngine/Events/Event.h"

#include <cstdint>

namespace Dingo
{

	class GamepadEvent : public Event
	{
	public:
		inline uint32_t GetGamepadId() const { return m_GamepadId; }

		EVENT_CLASS_CATEGORY(EventCategoryGamepad | EventCategoryInput)

	protected:
		GamepadEvent(uint32_t gamepadId)
			: m_GamepadId(gamepadId)
		{}

		uint32_t m_GamepadId;
	};

}
