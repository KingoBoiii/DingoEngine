#pragma once

#include <glm/glm.hpp>

namespace Dingo
{

	// One mesh's surface for Renderer3D's lit shader (the default material and lit materials), in
	// place of its material's EmissiveColor, EmissiveStrength, Roughness and Specular, all four
	// together. Meshes that differ only in these share a material and so a batch. A custom shader
	// ignores it.
	struct SurfaceOverride
	{
		glm::vec3 EmissiveColor    = { 0.0f, 0.0f, 0.0f };
		float     EmissiveStrength = 0.0f;
		float     Roughness        = 0.5f;
		float     Specular         = 0.0f;

		SurfaceOverride& SetEmissiveColor(const glm::vec3& color) { EmissiveColor = color; return *this; }
		SurfaceOverride& SetEmissiveStrength(float strength)      { EmissiveStrength = strength; return *this; }
		SurfaceOverride& SetRoughness(float roughness)            { Roughness = roughness; return *this; }
		SurfaceOverride& SetSpecular(float specular)              { Specular = specular; return *this; }
	};

}
