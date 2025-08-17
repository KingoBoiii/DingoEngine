#pragma once
#include "KeyCodes.h"

namespace Dingo
{
	
	class Input
	{
	public:
		static void Update();

		static bool IsKeyDown(KeyCode keycode);
		static bool IsKeyPressed(KeyCode keycode);

	private:
		inline static std::unordered_map<KeyCode, bool> s_CurrentKeyStates;
		inline static std::unordered_map<KeyCode, bool> s_PreviousKeyStates;
	};

}
