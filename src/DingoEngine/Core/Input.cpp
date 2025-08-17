#include "depch.h"
#include "DingoEngine/Core/Input.h"
#include "DingoEngine/Core/Application.h"

#include <GLFW/glfw3.h>

namespace Dingo
{

	void Input::Update()
	{
		GLFWwindow* window = static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindowHandle());
		s_PreviousKeyStates = s_CurrentKeyStates;

		// You may want to iterate over all possible KeyCodes, or just the ones you care about.
		for (int key = GLFW_KEY_SPACE; key <= GLFW_KEY_LAST; ++key)
		{
			int32_t state = glfwGetKey(window, key);
			s_CurrentKeyStates[(KeyCode)key] = (state == GLFW_PRESS || state == GLFW_REPEAT);
		}
	}
	
	bool Input::IsKeyDown(KeyCode keycode)
	{
		bool current = IsKeyPressed(keycode);
		bool previous = s_PreviousKeyStates[keycode];
		return current && !previous;
	}

	bool Input::IsKeyPressed(KeyCode keycode)
	{
		GLFWwindow* window = static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindowHandle());
		int32_t state = glfwGetKey(window, static_cast<int32_t>(keycode));
		return state == GLFW_PRESS || state == GLFW_REPEAT;
	}

}
