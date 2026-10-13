#pragma once
#include "DingoEngine/Events/Event.h"

namespace Dingo
{

	class MouseScrolledEvent : public Event
	{
	public:
		MouseScrolledEvent(float xOffset, float yOffset)
			: m_XOffset(xOffset), m_YOffset(yOffset)
		{}

		inline float GetXOffset() const { return m_XOffset; }
		inline float GetYOffset() const { return m_YOffset; }

		std::string ToString() const override
		{
			std::stringstream ss;
			ss << "MouseScrolledEvent: " << m_XOffset << ", " << m_YOffset;
			return ss.str();
		}

		DE_EVENT_CLASS_TYPE(MouseScrolled)
		DE_EVENT_CLASS_CATEGORY(Mouse | Input)

	private:
		float m_XOffset;
		float m_YOffset;
	};

}
