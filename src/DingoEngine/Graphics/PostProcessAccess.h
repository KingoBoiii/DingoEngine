#pragma once
#include "DingoEngine/Graphics/PostProcess.h"

namespace Dingo::Internal
{

	struct PostProcessAccess
	{
		// ApplyAmbientOcclusion, reporting whether this call copied the scene depth (AO's apply pass copies it).
		static bool ApplyAmbientOcclusion(PostProcessStack& post);
		// The scene depth copied again, for a pass drawn after the scene changed it (the sky, the batches)
		// that CopySceneDepth's once-per-Begin copy missed; `current` keeps a copy the caller just made.
		// nullptr while no Begin is open.
		static Texture* RefreshSceneDepth(PostProcessStack& post, bool current);
		// The froxel volume AO's apply pass fades the occlusion by, with Renderer3D::CameraData's VolumeParams
		// and VolumeGrid, until the next Begin. It must stay alive until then. No-op while no Begin is open.
		static void SetAmbientOcclusionVolume(PostProcessStack& post, Texture* volume, const glm::vec4& params, const glm::vec4& grid);
	};

}
