#pragma once
#include "DingoEngine/Events/Event.h"

#include <sstream>

namespace Dingo
{

	class WindowFocusEvent : public Event
	{
	public:
		WindowFocusEvent(bool focused)
			: m_Focused(focused)
		{}
		virtual ~WindowFocusEvent() = default;

	public:
		bool IsFocused() const { return m_Focused; }

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
