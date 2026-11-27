#pragma once

#include "DingoEngine/Graphics/PbrMaterial.h"

#include <glm/glm.hpp>

namespace Dingo
{
	class Material;
	class Sampler;
	class Shader;
}

namespace Dingo::Internal
{

	enum class PbrShaderVariant
	{
		Static,
		Skinned,
		Instanced
	};

	// std140, mirrored by MaterialData in Renderer3D_Pbr.glsl: binding 1 of every PBR material.
	struct PbrMaterialData
	{
		glm::vec4 BaseColor{ 1.0f };
		glm::vec4 Emissive{ 0.0f };  // rgb = colour, a = strength
		glm::vec4 Surface{ 0.0f };   // x = metallic, y = roughness, z = occlusion strength, w = normal scale
		glm::ivec4 Flags{ 0 };       // x = fog target: 0 FogColor, 1 black, 2 white; y = 1 translucent; z = 1 has a normal map
	};
	static_assert(sizeof(PbrMaterialData) == 64, "PbrMaterialData must match the std140 block in Renderer3D_Pbr.glsl");

	// Created on first use, on the main thread, and shared by every Renderer3D; nullptr while it is not valid (it failed to compile, and no hot reload has fixed it).
	Shader* GetPbrShader(PbrShaderVariant variant);
	// True only for a valid shader this file created.
	bool IsPbrShader(const Shader* shader);

	// Repeat, trilinear, 8x anisotropic; nullptr in a headless app.
	Sampler* GetPbrSampler();

	// Fills empty slots and rewrites the uniform block when it changed; called every scene for every PBR material drawn.
	void PreparePbrMaterial(Material* material);

	Material* CreatePbrMaterial(const PbrMaterialParams& params);

	// Before the renderer's own resources go.
	void DestroyPbrShaders();

}
