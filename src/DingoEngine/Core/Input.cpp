#include "depch.h"
#include "DingoEngine/Core/Input.h"
#include "DingoEngine/Core/Application.h"

#include <GLFW/glfw3.h>

namespace Dingo
{

	bool Input::IsKeyPressed(KeyCode keycode)
	{
		GLFWwindow* window = static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindowHandle());
		int32_t state = glfwGetKey(window, static_cast<int32_t>(keycode));
		return state == GLFW_PRESS || state == GLFW_REPEAT;
	}

}
