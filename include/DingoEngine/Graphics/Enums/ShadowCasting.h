#pragma once

#include <cstdint>

namespace Dingo
{

	// Whether a mesh casts a shadow from the lights that cast them (Renderer3D::SubmitMesh,
	// MeshRendererComponent, SkinnedMeshRendererComponent).
	enum class ShadowCasting : uint8_t
	{
		On,         // drawn, and casts
		Off,        // drawn, casts nothing
		ShadowsOnly // casts but isn't drawn: a wall cut away for the camera that must still block the light
	};

}
