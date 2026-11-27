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

		// The FogVolume records and counts the latest scene's PrepareVolumetrics kept (froxel volumes first,
		// then Raymarch ones), for tests; they are rewritten each scene.
		static const Renderer3D::FogVolumeData& FogVolumeRecords(const Renderer3D& renderer) { return renderer.m_FogVolumeData; }
		static uint32_t FogVolumeFroxelCount(const Renderer3D& renderer) { return renderer.m_FogVolumeFroxelCount; }
		static uint32_t FogVolumeRaymarchCount(const Renderer3D& renderer) { return renderer.m_FogVolumeRaymarchCount; }

		// Asks the next scene that dispatches the froxel volume to copy one slice of the scatter or the
		// integrated volume into VolumetricSlice (one request, answered once): the in-scatter, or with
		// `alpha` the alpha (transmittance of the integrated volume, extinction of the scatter one),
		// both times `exposure`. The texture has the grid's x and y size; null until a request is answered.
		static void RequestVolumetricSlice(Renderer3D& renderer, bool scatter, bool alpha, uint32_t slice, float exposure)
		{
			renderer.m_SliceRequest = { true, scatter, alpha, slice, exposure };
		}
		static Texture* VolumetricSlice(const Renderer3D& renderer) { return renderer.m_VolumetricSlice; }

		// The Raymarch FogVolume pass's low-resolution scatter (rgb = L, a = T) and view-distance textures and the composite material, for tests; null before a scene drew one.
		static Texture* RaymarchScatter(const Renderer3D& renderer) { return renderer.m_RaymarchScatter; }
		static Texture* RaymarchDepth(const Renderer3D& renderer) { return renderer.m_RaymarchDepth; }
		static Material* RaymarchComposite(const Renderer3D& renderer) { return renderer.m_RaymarchComposite; }
	};

}
