#pragma once
#include "DingoEngine/Events/Event.h"

#include <sstream>

namespace Dingo
{

	/// The user asked to close the window. The Application consumes it first (it stops the main
	/// loop and marks it Handled), so layers never see it; call Application::Close() to quit.
	class WindowCloseEvent : public Event
	{
	public:
		/// Raised by the window; there is no payload.
		WindowCloseEvent() = default;

		DE_EVENT_CLASS_TYPE(WindowClose)
		DE_EVENT_CLASS_CATEGORY(Window)
	};

} // namespace Dingo
