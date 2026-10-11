#include "depch.h"
#include "DingoEngine/Graphics/Frustum.h"

#include <glm/gtc/matrix_access.hpp>

#include <algorithm>
#include <limits>

namespace Dingo
{

	AABB AABB::Transformed(const glm::mat4& transform) const
	{
		if (IsEmpty())
			return *this;

		const glm::vec3 translation(transform[3]);
		AABB result{ translation, translation };
		for (int column = 0; column < 3; ++column)
		{
			for (int row = 0; row < 3; ++row)
			{
				const float a = transform[column][row] * Min[column];
				const float b = transform[column][row] * Max[column];
				result.Min[row] += (std::min)(a, b);
				result.Max[row] += (std::max)(a, b);
			}
		}
		return result;
	}

	Frustum Frustum::FromViewProjection(const glm::mat4& viewProjection)
	{
		const glm::vec4 x = glm::row(viewProjection, 0);
		const glm::vec4 y = glm::row(viewProjection, 1);
		const glm::vec4 z = glm::row(viewProjection, 2);
		const glm::vec4 w = glm::row(viewProjection, 3);

		Frustum frustum;
		frustum.m_Planes = { w + x, w - x, w + y, w - y, z, w - z };
		for (glm::vec4& plane : frustum.m_Planes)
			plane /= glm::length(glm::vec3(plane));
		return frustum;
	}

	bool Frustum::Intersects(const AABB& box) const
	{
		if (box.IsEmpty())
			return false;

		for (const glm::vec4& plane : m_Planes)
		{
			const glm::vec3 farthest(plane.x >= 0.0f ? box.Max.x : box.Min.x, plane.y >= 0.0f ? box.Max.y : box.Min.y, plane.z >= 0.0f ? box.Max.z : box.Min.z);
			if (glm::dot(glm::vec3(plane), farthest) + plane.w < 0.0f)
				return false;
		}
		return true;
	}

	bool Frustum::Intersects(const glm::vec3& center, float radius) const
	{
		for (const glm::vec4& plane : m_Planes)
		{
			if (glm::dot(glm::vec3(plane), center) + plane.w < -radius)
				return false;
		}
		return true;
	}

	float Frustum::DistanceOutside(const glm::vec3& center, float radius) const
	{
		float outside = -(std::numeric_limits<float>::max)();
		for (const glm::vec4& plane : m_Planes)
			outside = (std::max)(outside, -(glm::dot(glm::vec3(plane), center) + plane.w) - radius);
		return outside;
	}

}
