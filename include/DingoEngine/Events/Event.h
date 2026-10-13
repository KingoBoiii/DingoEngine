#pragma once
#include "DingoEngine/Common.h"

#include <cstdint>
#include <iostream>
#include <string>
#include <sstream>

namespace Dingo
{

	/// Concrete type of an Event; each event class returns its own from GetEventType().
	enum class EventType
	{
		None = 0,
		WindowClose, WindowResize, WindowFocus,
		KeyPressed, KeyReleased,
		MouseButtonPressed, MouseButtonReleased,
		MouseMoved, MouseScrolled,
		GamepadConnected, GamepadDisconnected
	};

	/// Bit flags that group event types; one event can be in several (see Event::IsInCategory).
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

	/// Union of two category masks.
	constexpr EventCategory operator|(EventCategory a, EventCategory b) { return EventCategory(uint32_t(a) | uint32_t(b)); }
	/// Intersection of two category masks; compare against EventCategory::None.
	constexpr EventCategory operator&(EventCategory a, EventCategory b) { return EventCategory(uint32_t(a) & uint32_t(b)); }

/// Declares GetStaticType(), GetEventType() and GetName() for an Event subclass of that EventType.
#define DE_EVENT_CLASS_TYPE(type) static ::Dingo::EventType GetStaticType() { return ::Dingo::EventType::type; }\
								virtual ::Dingo::EventType GetEventType() const override { return GetStaticType(); }\
								virtual const char* GetName() const override { return #type; }

/// Declares GetCategoryFlags() for an Event subclass; pass EventCategory names joined with |.
#define DE_EVENT_CLASS_CATEGORY(category) virtual ::Dingo::EventCategory GetCategoryFlags() const override { using enum ::Dingo::EventCategory; return category; }

	/// Base class of everything a Layer receives in OnEvent. The window raises events on the main
	/// thread while polling; they reach the layers top-down (overlays first) until one is Handled.
	/// The event only lives for the duration of the call, so copy what you need out of it.
	class Event
	{
	public:
		/// Frees the event through the base pointer.
		virtual ~Event() = default;

		/// True once a handler consumed the event; layers below it no longer receive it.
		bool Handled = false;

		/// The concrete type; compare with a class's GetStaticType() or use EventDispatcher.
		virtual EventType GetEventType() const = 0;
		/// The event type's name, e.g. "KeyPressed"; for logging.
		virtual const char* GetName() const = 0;
		/// The EventCategory bits this event belongs to, OR-ed together.
		virtual EventCategory GetCategoryFlags() const = 0;
		/// Text for logging; defaults to GetName() and most events add their payload.
		virtual std::string ToString() const { return GetName(); }

		/// True if the event has any of the bits in category.
		bool IsInCategory(EventCategory category) const
		{
			return (GetCategoryFlags() & category) != EventCategory::None;
		}
	};

	/// Routes one Event to a handler by type. Make one per OnEvent call and Dispatch each type you
	/// handle; it only holds a reference to the event.
	class EventDispatcher
	{
	public:
		/// Wraps the event to route; it must outlive the dispatcher.
		EventDispatcher(Event& event)
			: m_Event(event)
		{}

		/// Calls func(T&) when the event is a T and returns true; returns false (func not called)
		/// otherwise. func returns bool, and a true result sets Event::Handled. Never clears it.
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

	/// Writes Event::ToString().
	inline std::ostream& operator<<(std::ostream& os, const Event& e)
	{
		return os << e.ToString();
	}

}
