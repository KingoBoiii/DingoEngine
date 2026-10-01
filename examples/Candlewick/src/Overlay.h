#pragma once
#include <DingoEngine.h>

#include <glm/glm.hpp>

#include <string>

namespace Dingo::Overlay
{
	Font* LoadFont(const char* who);
	Entity MakeCamera(Scene& scene, const char* name, float orthoSize);
	Entity MakeText(Scene& scene, Font* font, const char* name, float size, const glm::vec4& color, bool centered);

	// Confirm prompt matching the connected pad family (PlayStation labels south "X").
	std::string ConfirmPrompt(const char* action);

	bool ConfirmPressed();
}
