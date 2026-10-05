#pragma once
#include <DingoEngine.h>

#include <glm/glm.hpp>

namespace Dingo::Overlay
{
	Entity MakeCamera(Scene& scene, const char* name, float orthoSize);
	Entity MakeText(Scene& scene, Font* font, const char* name, float size, const glm::vec4& color, const glm::vec3& position, const char* text);

	// The window gained or lost focus: the click that focused it must not count as an input in the next frame.
	void NoteFocus(bool focused);
	// Once at the start of each update, before anything asks.
	void BeginFrame();
	bool IsRefocusFrame();

	// A key that is not a function key or a modifier, a mouse button outside a refocus frame, or a gamepad button.
	bool AnyInputPressed();
}
