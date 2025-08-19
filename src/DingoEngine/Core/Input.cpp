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
		s_PreviousButtonStates = s_CurrentButtonStates;
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

	bool Input::IsMouseButtonDown(MouseButton button)
	{
		bool current = IsMouseButtonPressed(button);
		bool previous = s_PreviousButtonStates[button];
		return current && !previous;
	}

	bool Input::IsMouseButtonPressed(MouseButton button)
	{
		GLFWwindow* window = static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindowHandle());
		int32_t state = glfwGetMouseButton(window, static_cast<int32_t>(button));
		return state == GLFW_PRESS;
	}

	void Input::UpdateKeyStates(KeyCode key, int scancode, int action, int mods)
	{
		s_CurrentKeyStates[key] = (action == GLFW_PRESS || action == GLFW_REPEAT);
	}

	void Input::UpdateMouseButtonStates(MouseButton mouseButton, int action, int mods)
	{
		s_CurrentButtonStates[mouseButton] = (action == GLFW_PRESS);
	}

}
