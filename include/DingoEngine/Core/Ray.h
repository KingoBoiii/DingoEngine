#pragma once
#include <glm/glm.hpp>

namespace Dingo
{

	// A world-space ray: a point (Origin) and a normalized travel direction.
	struct Ray
	{
		glm::vec3 Origin{ 0.0f };
		glm::vec3 Direction{ 0.0f, 0.0f, -1.0f };

		Ray() = default;
		Ray(const glm::vec3& origin, const glm::vec3& direction)
			: Origin(origin), Direction(direction) {}

		// Point along the ray at parameter t (t >= 0 is in front of Origin).
		glm::vec3 At(float t) const { return Origin + Direction * t; }

		// Intersects the ray with the plane { p : dot(p, planeNormal) = planeConstant }.
		// Returns false (outHit unchanged) if the ray is parallel to the plane or the
		// hit lies behind the origin. planeNormal need not be normalized by the caller,
		// but is expected to be non-zero.
		bool IntersectPlane(const glm::vec3& planeNormal, float planeConstant, glm::vec3& outHit) const
		{
			const float denom = glm::dot(planeNormal, Direction);
			if (glm::abs(denom) < 1e-6f)
				return false;

			const float t = (planeConstant - glm::dot(planeNormal, Origin)) / denom;
			if (t < 0.0f)
				return false;

			outHit = At(t);
			return true;
		}

		// Convenience for the common "pick a point on the ground" case: intersects the
		// horizontal plane y = height (normal { 0, 1, 0 }).
		bool IntersectGroundPlane(float height, glm::vec3& outHit) const
		{
			return IntersectPlane(glm::vec3(0.0f, 1.0f, 0.0f), height, outHit);
		}
	};

	// Unprojects a screen/client pixel position (origin top-left, +Y down, matching
	// Input::GetMousePosition()) into a world-space ray, given the view and projection
	// matrices in effect. Shared by PerspectiveCamera::ScreenPointToRay and
	// Scene::ScreenPointToRay so both paths unproject identically. Perspective-only:
	// callers with an orthographic projection get a ray parallel to -Z at the near plane
	// rather than a true orthographic ray (the direction ignores the projection's parallel
	// rays); use IntersectPlane on that ray's origin plane if orthographic picking is needed.
	inline Ray ScreenPointToRay(const glm::vec2& screenPos, const glm::vec2& viewportSize, const glm::mat4& view, const glm::mat4& projection)
	{
		// NDC xy in [-1, 1]; flip Y since screenPos is top-left-origin but NDC/GL is bottom-up.
		const float ndcX = (2.0f * screenPos.x) / viewportSize.x - 1.0f;
		const float ndcY = 1.0f - (2.0f * screenPos.y) / viewportSize.y;

		const glm::mat4 inverseViewProjection = glm::inverse(projection * view);

		// GLM_FORCE_DEPTH_ZERO_TO_ONE (set engine-wide) puts near/far NDC z at 0/1.
		const glm::vec4 nearClip = inverseViewProjection * glm::vec4(ndcX, ndcY, 0.0f, 1.0f);
		const glm::vec4 farClip = inverseViewProjection * glm::vec4(ndcX, ndcY, 1.0f, 1.0f);

		const glm::vec3 nearWorld = glm::vec3(nearClip) / nearClip.w;
		const glm::vec3 farWorld = glm::vec3(farClip) / farClip.w;

		return Ray(nearWorld, glm::normalize(farWorld - nearWorld));
	}

}
