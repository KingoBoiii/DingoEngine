#pragma once
#include <DingoEngine.h>

#include <glm/glm.hpp>

namespace Dingo::Overlay
{
	Font* LoadFont(const char* who);
	Entity MakeCamera(Scene& scene, const char* name, float orthoSize);
	Entity MakeText(Scene& scene, Font* font, const char* name, float size, const glm::vec4& color, const glm::vec3& position, const char* text);

	bool AnyInputPressed();
}
