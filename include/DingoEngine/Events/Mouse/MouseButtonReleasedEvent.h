#pragma once
#include "MouseButtonEvent.h"

namespace Dingo
{

	/// A mouse button went up.
	class MouseButtonReleasedEvent : public MouseButtonEvent
	{
	public:
		/// Raised by the window for the released button.
		MouseButtonReleasedEvent(MouseButton button)
			: MouseButtonEvent(button)
		{}

		/// "MouseButtonReleasedEvent: <button>".
		std::string ToString() const override
		{
			std::stringstream ss;
			ss << "MouseButtonReleasedEvent: " << m_Button;
			return ss.str();
		}

		DE_EVENT_CLASS_TYPE(MouseButtonReleased)
	};

}

