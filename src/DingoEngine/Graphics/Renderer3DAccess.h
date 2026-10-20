#pragma once
#include "DingoEngine/Graphics/Renderer3D.h"

namespace Dingo::Internal
{

	struct Renderer3DAccess
	{
		// The bounds EndScene would work out for this mesh at this transform; false (and empty bounds)
		// for a non-affine transform, which is never culled.
		static bool WorldBounds(const Mesh& mesh, const glm::mat4& transform, AABB& bounds);

		// SubmitMesh with bounds from WorldBounds above for the same mesh bounds and transform, so
		// EndScene skips working them out again.
		static void SubmitMesh(Renderer3D& renderer, const Mesh* mesh, const glm::mat4& transform, const glm::vec4& color, Material* material, ShadowCasting shadows,
			uint32_t shadowGroups, float cullDistance, const SurfaceOverride* surface, const AABB& bounds, bool bounded)
		{
			renderer.SubmitMeshWithBounds(mesh, transform, color, material, shadows, shadowGroups, cullDistance, surface, &bounds, bounded);
		}
	};

}
