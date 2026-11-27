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

		// The froxel volume's textures for tests: the integrated volume, and the scatter volume the latest
		// scene that dispatched wrote (null before any did).
		static Texture* VolumetricIntegrated(const Renderer3D& renderer) { return renderer.m_VolumetricIntegrated; }
		static Texture* VolumetricScatter(const Renderer3D& renderer) { return renderer.m_VolumetricScatter[renderer.m_VolumetricPass]; }
		// The scatter texture the temporal history lives in, and the frame it was written (~0 for none).
		static Texture* VolumetricHistory(const Renderer3D& renderer) { return renderer.m_VolumetricScatter[renderer.m_VolumetricHistory]; }
		static uint64_t VolumetricHistoryFrame(const Renderer3D& renderer) { return renderer.m_VolumetricHistoryFrame; }
	};

}
