#include "depch.h"
#include "DingoUIWorld.h"

#include "DingoEngine/Core/Ray.h"

#include <algorithm>
#include <cmath>

namespace Dingo::Internal
{

	UIWorldPlacement ComputeWorldPlacement(const glm::mat4& viewProjection, const glm::vec3& worldPosition, const glm::vec2& viewport, const UIWorldRootParams& params)
	{
		UIWorldPlacement placement;
		if (viewport.x <= 0.0f || viewport.y <= 0.0f)
			return placement;

		const std::optional<glm::vec2> projected = WorldToScreenPoint(worldPosition, viewport, glm::mat4(1.0f), viewProjection);
		if (!projected)
			return placement;

		const float depth = (viewProjection * glm::vec4(worldPosition, 1.0f)).w;
		if (params.MaxDistance > 0.0f && depth > params.MaxDistance)
			return placement;

		const float margin = (std::max)(0.0f, params.CullMargin);
		if (projected->x < -margin || projected->y < -margin || projected->x > viewport.x + margin || projected->y > viewport.y + margin)
			return placement;

		placement.Visible = true;
		placement.Point = { std::round(projected->x), std::round(projected->y) };
		placement.Depth = depth;
		if (params.ReferenceDistance > 0.0f)
		{
			const float low = (std::min)(params.MinScale, params.MaxScale);
			const float high = (std::max)(params.MinScale, params.MaxScale);
			placement.Scale = std::clamp(params.ReferenceDistance / (std::max)(depth, 1.0e-4f), low, high);
		}
		if (params.MaxDistance > 0.0f && params.FadeDistance > 0.0f)
			placement.Alpha = std::clamp((params.MaxDistance - depth) / params.FadeDistance, 0.0f, 1.0f);
		return placement;
	}

}
