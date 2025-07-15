#pragma once
#include "DingoEngine/Events/Event.h"

#include <sstream>

namespace Dingo
{

	class WindowCloseEvent : public Event
	{
	public:
		WindowCloseEvent() = default;

		EVENT_CLASS_TYPE(WindowClose)
		EVENT_CLASS_CATEGORY(EventCategoryWindow)
	};

} // namespace Dingo
