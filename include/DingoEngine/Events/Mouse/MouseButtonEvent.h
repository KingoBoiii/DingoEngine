#pragma once
#include "DingoEngine/Core/MouseButtons.h"

#include "DingoEngine/Events/Event.h"

namespace Dingo
{

	class MouseButtonEvent : public Event
	{
	public:
		inline MouseButton GetMouseButton() const { return m_Button; }

		DE_EVENT_CLASS_CATEGORY(Mouse | MouseButton | Input)
	protected:
		MouseButtonEvent(MouseButton button)
			: m_Button(button)
		{}

		MouseButton m_Button;
	};

}
