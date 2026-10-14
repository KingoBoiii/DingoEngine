#pragma once
#include "MouseButtonEvent.h"

namespace Dingo
{

	/// A mouse button went down; sent once per press, with no repeats.
	class MouseButtonPressedEvent : public MouseButtonEvent
	{
	public:
		/// Raised by the window for the pressed button.
		MouseButtonPressedEvent(MouseButton button)
			: MouseButtonEvent(button)
		{}

		/// "MouseButtonPressedEvent: <button>".
		std::string ToString() const override
		{
			std::stringstream ss;
			ss << "MouseButtonPressedEvent: " << m_Button;
			return ss.str();
		}

		DE_EVENT_CLASS_TYPE(MouseButtonPressed)
	};

}

