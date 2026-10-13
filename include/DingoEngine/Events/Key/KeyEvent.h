#pragma once
#include "DingoEngine/Core/KeyCodes.h"

#include "DingoEngine/Events/Event.h"

namespace Dingo
{

	class KeyEvent : public Event
	{
	public:
		inline KeyCode GetKeyCode() const { return m_KeyCode; }

		DE_EVENT_CLASS_CATEGORY(Keyboard | Input)
	protected:
		KeyEvent(KeyCode keycode)
			: m_KeyCode(keycode)
		{}

		KeyCode m_KeyCode;
	};

}
