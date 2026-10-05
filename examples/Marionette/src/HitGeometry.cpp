#include "HitGeometry.h"
#include "GameTuning.h"

#include <algorithm>

namespace Dingo
{

	float DistanceToSegment(const glm::vec3& point, const glm::vec3& from, const glm::vec3& to)
	{
		const glm::vec3 along = to - from;
		const float lengthSquared = glm::dot(along, along);
		const float t = lengthSquared > 1.0e-12f ? std::clamp(glm::dot(point - from, along) / lengthSquared, 0.0f, 1.0f) : 0.0f;
		return glm::length(point - (from + along * t));
	}

	bool Touches(const SweptSphere& swept, const WorldSphere& sphere)
	{
		return DistanceToSegment(sphere.Center, swept.From, swept.To) <= swept.Radius + sphere.Radius;
	}

	BladeAxis MeasureBlade(const Model& weapon)
	{
		glm::vec3 farthest(0.0f);
		float length = 0.0f;
		for (const SubMesh& submesh : weapon.GetSubMeshes())
		{
			if (!submesh.MeshData)
				continue;
			for (const MeshVertex& vertex : submesh.MeshData->GetVertices())
			{
				const float distance = glm::length(vertex.Position);
				if (distance > length)
				{
					length = distance;
					farthest = vertex.Position;
				}
			}
		}

		BladeAxis axis;
		if (!(length > 0.0f))
			return axis;

		axis.Direction = farthest / length;
		axis.Length = length;
		for (const SubMesh& submesh : weapon.GetSubMeshes())
		{
			if (!submesh.MeshData)
				continue;
			for (const MeshVertex& vertex : submesh.MeshData->GetVertices())
			{
				const float along = glm::dot(vertex.Position, axis.Direction);
				if (along >= WEAPON_BLADE_START * length)
					axis.HalfWidth = std::max(axis.HalfWidth, glm::length(vertex.Position - axis.Direction * along));
			}
		}
		return axis;
	}

	float WeaponSphereRadius(const BladeAxis& blade)
	{
		return std::clamp(WEAPON_SPHERE_WIDTH_FRACTION * blade.HalfWidth, WEAPON_SPHERE_MIN_RADIUS, WEAPON_SPHERE_MAX_RADIUS);
	}

	HitDebugView::HitDebugView()
	{
		Renderer3D& renderer3D = Application::Get().GetRenderer3D();
		m_Mesh = Mesh::CreateSphere(HIT_SPHERE_MESH_RADIUS, HIT_SPHERE_MESH_RINGS, HIT_SPHERE_MESH_SEGMENTS);

		const std::array<glm::vec3, 4> colors = { DEBUG_COLOR_IDLE, DEBUG_COLOR_HITBOX, DEBUG_COLOR_IFRAMES, DEBUG_COLOR_PARRY };
		const std::array<const char*, 4> names = { "DebugHitIdle", "DebugHitbox", "DebugIframes", "DebugParry" };
		for (size_t i = 0; i < m_Materials.size(); ++i)
		{
			m_Materials[i] = renderer3D.CreateLitMaterial(MaterialParams()
				.SetDebugName(names[i])
				.SetFillMode(FillMode::Wireframe)
				.SetEmissiveColor(colors[i])
				.SetEmissiveStrength(DEBUG_EMISSIVE));
		}
	}

	HitDebugView::~HitDebugView()
	{
		for (Material*& material : m_Materials)
			DestroyAndDelete(material);
		delete m_Mesh;
	}

}
