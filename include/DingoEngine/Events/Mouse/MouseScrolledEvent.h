#pragma once
#include "DingoEngine/Events/Event.h"

namespace Dingo
{

	/// The mouse wheel or touchpad scrolled. Category: Mouse and Input.
	class MouseScrolledEvent : public Event
	{
	public:
		/// Raised by the window with the scroll amount since the last event.
		MouseScrolledEvent(float xOffset, float yOffset)
			: m_XOffset(xOffset), m_YOffset(yOffset)
		{}

		/// Horizontal scroll in wheel steps; positive is right.
		inline float GetXOffset() const { return m_XOffset; }
		/// Vertical scroll in wheel steps; positive is away from the user (up).
		inline float GetYOffset() const { return m_YOffset; }

		/// "MouseScrolledEvent: <x>, <y>".
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
