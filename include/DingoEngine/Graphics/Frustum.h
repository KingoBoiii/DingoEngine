#pragma once

#include <glm/glm.hpp>

#include <array>
#include <cstdint>

namespace Dingo
{

	// An axis-aligned box. Min above Max on any axis is the empty box, as Mesh::GetBoundsMin/Max
	// report for a mesh without vertices.
	struct AABB
	{
		glm::vec3 Min{ 1.0f };
		glm::vec3 Max{ -1.0f };

		AABB() = default;
		AABB(const glm::vec3& min, const glm::vec3& max) : Min(min), Max(max) {}

		bool IsEmpty() const { return Min.x > Max.x || Min.y > Max.y || Min.z > Max.z; }
		glm::vec3 GetCenter() const { return (Min + Max) * 0.5f; }
		glm::vec3 GetExtents() const { return (Max - Min) * 0.5f; }

		// The box around this one moved by an affine transform (Arvo's method): exact for a box,
		// so the box of a mesh's bounds holds every vertex of the transformed mesh. An empty box
		// stays empty. A projective transform (bottom row other than 0, 0, 0, 1) is not supported.
		[[nodiscard]] AABB Transformed(const glm::mat4& transform) const;
	};

	// Six planes from a view-projection: left, right, bottom, top, near, far, each normal pointing
	// in and of unit length, so a plane's dot product with a point plus its w is a distance.
	class Frustum
	{
	public:
		enum Plane : uint32_t { Left, Right, Bottom, Top, Near, Far, PlaneCount };

		// Gribb-Hartmann, for clip-space depth in [0, 1], which the engine uses on every backend.
		[[nodiscard]] static Frustum FromViewProjection(const glm::mat4& viewProjection);

		// False only when the box lies wholly behind one plane; a box near a corner can pass while
		// outside, which a cull can afford.
		bool Intersects(const AABB& box) const;
		// What Renderer3D tests a point or spot light's range with.
		bool Intersects(const glm::vec3& center, float radius) const;
		// How far the sphere lies outside, along the plane it is farthest behind: at most 0 when
		// Intersects(center, radius) holds.
		float DistanceOutside(const glm::vec3& center, float radius) const;

		const glm::vec4& GetPlane(Plane plane) const { return m_Planes[plane]; }
		const std::array<glm::vec4, PlaneCount>& GetPlanes() const { return m_Planes; }

	private:
		std::array<glm::vec4, PlaneCount> m_Planes{};
	};

}
