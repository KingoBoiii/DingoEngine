#pragma once
#include "KeyCodes.h"
#include "MouseButtons.h"

namespace Dingo
{
	
	class Input
	{
	public:
		static void Update();

		/**************************************************
		***		KEYBOARD								***
		**************************************************/

		static bool IsKeyDown(KeyCode keycode);
		static bool IsKeyPressed(KeyCode keycode);

		/**************************************************
		***		MOUSE									***
		**************************************************/

		static bool IsMouseButtonDown(MouseButton button);
		static bool IsMouseButtonPressed(MouseButton button);

	private:
		inline static std::unordered_map<KeyCode, bool> s_CurrentKeyStates;
		inline static std::unordered_map<KeyCode, bool> s_PreviousKeyStates;

		inline static std::unordered_map<MouseButton, bool> s_CurrentButtonStates;
		inline static std::unordered_map<MouseButton, bool> s_PreviousButtonStates;
	};

}
