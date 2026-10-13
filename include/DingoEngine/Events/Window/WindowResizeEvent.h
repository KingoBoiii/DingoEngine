#pragma once
#include "DingoEngine/Events/Event.h"

#include <sstream>

namespace Dingo
{

	/// The window's client area changed size, including minimize and restore. Reaches layers after
	/// the Application has queued the swap-chain resize; rebuild cameras and aspect ratios here.
	class WindowResizeEvent : public Event
	{
	public:
		/// Raised by the window with the new size in window (screen) units.
		WindowResizeEvent(uint32_t width, uint32_t height)
			: m_Width(width), m_Height(height)
		{}
		/// Frees the event through the base pointer.
		virtual ~WindowResizeEvent() = default;

	public:
		/// New client-area width in window units; 0 while the window is minimized.
		uint32_t GetWidth() const { return m_Width; }
		/// New client-area height in window units; 0 while the window is minimized.
		uint32_t GetHeight() const { return m_Height; }

		/// "WindowResizeEvent: <width>, <height>".
		std::string ToString() const override
		{
			std::stringstream ss;
			ss << "WindowResizeEvent: " << m_Width << ", " << m_Height;
			return ss.str();
		}


		DE_EVENT_CLASS_TYPE(WindowResize)
		DE_EVENT_CLASS_CATEGORY(Window)

	private:
		uint32_t m_Width;
		uint32_t m_Height;
	};

} // namespace Dingo
