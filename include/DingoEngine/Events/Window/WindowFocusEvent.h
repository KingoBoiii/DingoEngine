#pragma once
#include "DingoEngine/Events/Event.h"

#include <sstream>

namespace Dingo
{

	/// The window gained or lost input focus. Reaches layers after the Application has noted it;
	/// a natural place to auto-pause and release a locked cursor.
	class WindowFocusEvent : public Event
	{
	public:
		/// Raised by the window; focused is true on gain, false on loss.
		WindowFocusEvent(bool focused)
			: m_Focused(focused)
		{}
		/// Frees the event through the base pointer.
		virtual ~WindowFocusEvent() = default;

	public:
		/// True if the window gained focus, false if it lost it.
		bool IsFocused() const { return m_Focused; }

		/// "WindowFocusEvent: gained" or "WindowFocusEvent: lost".
		std::string ToString() const override
		{
			std::stringstream ss;
			ss << "WindowFocusEvent: " << (m_Focused ? "gained" : "lost");
			return ss.str();
		}

		DE_EVENT_CLASS_TYPE(WindowFocus)
		DE_EVENT_CLASS_CATEGORY(Window)

	private:
		bool m_Focused;
	};

} // namespace Dingo
