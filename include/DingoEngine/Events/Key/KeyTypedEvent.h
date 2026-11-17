#pragma once
#include "DingoEngine/Events/Event.h"

#include <cstdint>
#include <sstream>
#include <string>

namespace Dingo
{

	/// A character the player typed, as text after the keyboard layout, dead keys and IME have
	/// done their work: one per character, also for the repeats of a held key. Use it for text, not
	/// for shortcuts (KeyPressedEvent is the key, whatever it prints). Category: Keyboard and Input.
	class KeyTypedEvent : public Event
	{
	public:
		/// Raised by the window with the Unicode codepoint (UTF-32) of the character.
		explicit KeyTypedEvent(uint32_t codepoint)
			: m_Codepoint(codepoint)
		{}

		/// The Unicode codepoint of the typed character.
		uint32_t GetCodepoint() const { return m_Codepoint; }

		/// "KeyTypedEvent: U+<hex codepoint>".
		std::string ToString() const override
		{
			std::stringstream ss;
			ss << "KeyTypedEvent: U+" << std::hex << std::uppercase << m_Codepoint;
			return ss.str();
		}

		DE_EVENT_CLASS_TYPE(KeyTyped)
		DE_EVENT_CLASS_CATEGORY(Keyboard | Input)
	private:
		uint32_t m_Codepoint;
	};

}
