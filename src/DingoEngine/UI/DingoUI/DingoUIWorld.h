#pragma once
#include "DingoEngine/UI/DingoUI.h"

#include <glm/glm.hpp>

namespace Dingo::Internal
{

	struct UIWorldPlacement
	{
		bool Visible = false;
		glm::vec2 Point{ 0.0f }; // target pixels, rounded to whole pixels
		float Depth = 0.0f;
		float Scale = 1.0f;      // the distance scale
		float Alpha = 1.0f;
	};

	// Where a root hung on `worldPosition` goes, or Visible false when it is behind the camera,
	// beyond MaxDistance or outside the viewport by more than CullMargin. GPU-free.
	UIWorldPlacement ComputeWorldPlacement(const glm::mat4& viewProjection, const glm::vec3& worldPosition, const glm::vec2& viewport, const UIWorldRootParams& params);

}
