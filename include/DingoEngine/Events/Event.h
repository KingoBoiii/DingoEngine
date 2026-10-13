#pragma once
#include "DingoEngine/Common.h"

#include <cstdint>
#include <iostream>
#include <string>
#include <sstream>

namespace Dingo
{

	enum class EventType
	{
		None = 0,
		WindowClose, WindowResize, WindowFocus,
		KeyPressed, KeyReleased,
		MouseButtonPressed, MouseButtonReleased,
		MouseMoved, MouseScrolled,
		GamepadConnected, GamepadDisconnected
	};

	enum class EventCategory : uint32_t
	{
		None		= 0,
		Window		= DE_BIT(0),
		Input		= DE_BIT(1),
		Keyboard	= DE_BIT(2),
		Mouse		= DE_BIT(3),
		MouseButton	= DE_BIT(4),
		Gamepad		= DE_BIT(5)
	};

	constexpr EventCategory operator|(EventCategory a, EventCategory b) { return EventCategory(uint32_t(a) | uint32_t(b)); }
	constexpr EventCategory operator&(EventCategory a, EventCategory b) { return EventCategory(uint32_t(a) & uint32_t(b)); }

#define DE_EVENT_CLASS_TYPE(type) static ::Dingo::EventType GetStaticType() { return ::Dingo::EventType::type; }\
								virtual ::Dingo::EventType GetEventType() const override { return GetStaticType(); }\
								virtual const char* GetName() const override { return #type; }

#define DE_EVENT_CLASS_CATEGORY(category) virtual ::Dingo::EventCategory GetCategoryFlags() const override { using enum ::Dingo::EventCategory; return category; }

	class Event
	{
	public:
		virtual ~Event() = default;

		bool Handled = false;

		virtual EventType GetEventType() const = 0;
		virtual const char* GetName() const = 0;
		virtual EventCategory GetCategoryFlags() const = 0;
		virtual std::string ToString() const { return GetName(); }

		bool IsInCategory(EventCategory category) const
		{
			return (GetCategoryFlags() & category) != EventCategory::None;
		}
	};

	class EventDispatcher
	{
	public:
		EventDispatcher(Event& event)
			: m_Event(event)
		{}

		// F will be deduced by the compiler
		template<typename T, typename F>
		bool Dispatch(const F& func)
		{
			if (m_Event.GetEventType() == T::GetStaticType())
			{
				m_Event.Handled |= func(static_cast<T&>(m_Event));
				return true;
			}
			return false;
		}
	private:
		Event& m_Event;
	};

	inline std::ostream& operator<<(std::ostream& os, const Event& e)
	{
		return os << e.ToString();
	}

}
