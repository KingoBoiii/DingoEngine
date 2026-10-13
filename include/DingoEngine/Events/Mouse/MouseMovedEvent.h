#pragma once
#include "DingoEngine/Events/Event.h"

namespace Dingo
{

	/// The cursor moved. Category: Mouse and Input.
	class MouseMovedEvent : public Event
	{
	public:
		/// Raised by the window with the new cursor position.
		MouseMovedEvent(float x, float y)
			: m_X(x), m_Y(y)
		{}

		/// Cursor X in window units from the client area's left edge.
		inline float GetX() const { return m_X; }
		/// Cursor Y in window units from the client area's top edge, growing downwards.
		inline float GetY() const { return m_Y; }

		/// "MouseMovedEvent: <x>, <y>".
		std::string ToString() const override
		{
			std::stringstream ss;
			ss << "MouseMovedEvent: " << m_X << ", " << m_Y;
			return ss.str();
		}

		DE_EVENT_CLASS_TYPE(MouseMoved)
		DE_EVENT_CLASS_CATEGORY(Mouse | Input)

	private:
		float m_X;
		float m_Y;
	};

}
