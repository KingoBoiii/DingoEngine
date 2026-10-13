#pragma once
#include "KeyEvent.h"

namespace Dingo
{

	/// A key went down, or its key-repeat fired while held.
	class KeyPressedEvent : public KeyEvent
	{
	public:
		/// Raised by the window; isRepeat is true for the repeats after the first press.
		KeyPressedEvent(KeyCode keycode, bool isRepeat)
			: KeyEvent(keycode), m_IsRepeat(isRepeat)
		{}

		/// True for the key-repeat presses a held key sends after the first.
		bool IsRepeat() const { return m_IsRepeat; }

		/// "KeyPressedEvent: <key>", with " (repeat)" appended for a repeat.
		std::string ToString() const override
		{
			std::stringstream ss;
			ss << "KeyPressedEvent: " << m_KeyCode << (m_IsRepeat ? " (repeat)" : "");
			return ss.str();
		}

		DE_EVENT_CLASS_TYPE(KeyPressed)
	private:
		bool m_IsRepeat;
	};

}
