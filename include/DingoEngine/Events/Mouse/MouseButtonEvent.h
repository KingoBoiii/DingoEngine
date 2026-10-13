#pragma once
#include "DingoEngine/Core/MouseButtons.h"

#include "DingoEngine/Events/Event.h"

namespace Dingo
{

	/// Base of MouseButtonPressedEvent and MouseButtonReleasedEvent; dispatch on one of those.
	/// Category: Mouse, MouseButton and Input.
	class MouseButtonEvent : public Event
	{
	public:
		/// The button that changed state.
		inline MouseButton GetMouseButton() const { return m_Button; }

		DE_EVENT_CLASS_CATEGORY(Mouse | MouseButton | Input)
	protected:
		/// For subclasses: stores the button the getter returns.
		MouseButtonEvent(MouseButton button)
			: m_Button(button)
		{}

		/// The button that changed state.
		MouseButton m_Button;
	};

}
