#pragma once
#include <DingoEngine.h>

#include <glm/glm.hpp>

#include <array>
#include <cstdint>

namespace Dingo
{

	struct WorldSphere
	{
		glm::vec3 Center{ 0.0f };
		float Radius = 0.0f;
	};

	// A sphere that moved from one centre to the other since the last look.
	struct SweptSphere
	{
		glm::vec3 From{ 0.0f };
		glm::vec3 To{ 0.0f };
		float Radius = 0.0f;
	};

	float DistanceToSegment(const glm::vec3& point, const glm::vec3& from, const glm::vec3& to);
	bool Touches(const SweptSphere& swept, const WorldSphere& sphere);
	// Where a touching blade meets the sphere: the point of its surface towards the nearest point of the
	// sweep, or that point itself when it lies inside the sphere.
	glm::vec3 ContactPoint(const SweptSphere& swept, const WorldSphere& sphere);

	// From a weapon's grip origin to its farthest vertex. The half-width is the widest the weapon stands
	// off that axis beyond WEAPON_BLADE_START of its length.
	struct BladeAxis
	{
		glm::vec3 Direction{ 0.0f, 1.0f, 0.0f };
		float Length = 0.0f;
		float HalfWidth = 0.0f;
	};

	BladeAxis MeasureBlade(const Model& weapon);

	// The radius of a weapon sphere before the fighter's scale.
	float WeaponSphereRadius(const BladeAxis& blade);

	enum class DebugTint : uint8_t
	{
		Idle,
		Hitbox,
		Iframes,
		Parry
	};

	// The mesh and the four materials --debug-hitbox draws the spheres with. Destroy it before the renderer goes.
	class HitDebugView
	{
	public:
		HitDebugView();
		~HitDebugView();

		HitDebugView(const HitDebugView&) = delete;
		HitDebugView& operator=(const HitDebugView&) = delete;

		Mesh* GetMesh() const { return m_Mesh; }
		Material* GetMaterial(DebugTint tint) const { return m_Materials[static_cast<size_t>(tint)]; }

	private:
		Mesh* m_Mesh = nullptr;
		std::array<Material*, 4> m_Materials{};
	};

}
