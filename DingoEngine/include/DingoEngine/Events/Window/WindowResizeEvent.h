#pragma once
#include "DingoEngine/Events/Event.h"

#include <sstream>

namespace Dingo
{

	class WindowResizeEvent : public Event
	{
	public:
		WindowResizeEvent(uint32_t width, uint32_t height)
			: m_Width(width), m_Height(height)
		{}
		virtual ~WindowResizeEvent() = default;

	public:
		uint32_t GetWidth() const { return m_Width; }
		uint32_t GetHeight() const { return m_Height; }

		std::string ToString() const override
		{
			std::stringstream ss;
			ss << "WindowResizeEvent: " << m_Width << ", " << m_Height;
			return ss.str();
		}


		EVENT_CLASS_TYPE(WindowResize)
		EVENT_CLASS_CATEGORY(EventCategoryWindow)

	private:
		uint32_t m_Width;
		uint32_t m_Height;
	};

} // namespace Dingo
