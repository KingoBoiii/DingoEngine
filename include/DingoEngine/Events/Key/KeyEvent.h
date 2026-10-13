#pragma once
#include "DingoEngine/Core/KeyCodes.h"

#include "DingoEngine/Events/Event.h"

namespace Dingo
{

	/// Base of KeyPressedEvent and KeyReleasedEvent; dispatch on one of those.
	/// Category: Keyboard and Input.
	class KeyEvent : public Event
	{
	public:
		/// The key that changed state.
		inline KeyCode GetKeyCode() const { return m_KeyCode; }

		DE_EVENT_CLASS_CATEGORY(Keyboard | Input)
	protected:
		/// For subclasses: stores the key the getter returns.
		KeyEvent(KeyCode keycode)
			: m_KeyCode(keycode)
		{}

		/// The key that changed state.
		KeyCode m_KeyCode;
	};

}
