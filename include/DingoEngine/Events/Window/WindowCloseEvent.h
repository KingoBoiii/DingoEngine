#pragma once
#include "DingoEngine/Events/Event.h"

#include <sstream>

namespace Dingo
{

	class WindowCloseEvent : public Event
	{
	public:
		WindowCloseEvent() = default;

		DE_EVENT_CLASS_TYPE(WindowClose)
		DE_EVENT_CLASS_CATEGORY(Window)
	};

} // namespace Dingo
