#pragma once
#include <ostream>
#include <cstdint>

namespace Dingo
{

	/// A mouse button, numbered as in GLFW; Left, Right and Middle alias Button0 to Button2.
	enum class MouseButton : uint16_t
	{
		Button0 = 0,
		Button1 = 1,
		Button2 = 2,
		Button3 = 3,
		Button4 = 4,
		Button5 = 5,
		Left = Button0,
		Right = Button1,
		Middle = Button2
	};

	/// Prints the button's number, not its name.
	inline std::ostream& operator<<(std::ostream& os, MouseButton button)
	{
		os << static_cast<int32_t>(button);
		return os;
	}

}

