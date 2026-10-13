#pragma once
#include "DingoEngine/Events/Event.h"

namespace Dingo
{

	class MouseMovedEvent : public Event
	{
	public:
		MouseMovedEvent(float x, float y)
			: m_X(x), m_Y(y)
		{}

		inline float GetX() const { return m_X; }
		inline float GetY() const { return m_Y; }

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
