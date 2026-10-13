#pragma once
#include "KeyEvent.h"

namespace Dingo
{

	/// A key went up.
	class KeyReleasedEvent : public KeyEvent
	{
	public:
		/// Raised by the window for the released key.
		KeyReleasedEvent(KeyCode keycode)
			: KeyEvent(keycode)
		{}

		/// "KeyReleasedEvent: <key>".
		std::string ToString() const override
		{
			std::stringstream ss;
			ss << "KeyReleasedEvent: " << m_KeyCode;
			return ss.str();
		}

		DE_EVENT_CLASS_TYPE(KeyReleased)
	};

}
