#pragma once

namespace Dingo
{
	class Shader;
}

namespace Dingo::Internal
{

	enum class VolumetricShaderKind
	{
		Inject,
		Integrate,
		Raymarch,
		Slice
	};

	// Created on first use, on the main thread, and shared by every Renderer3D; nullptr while it is not valid (it failed to compile, and no hot reload has fixed it).
	Shader* GetVolumetricShader(VolumetricShaderKind kind);

	// Before the renderer's own resources go.
	void DestroyVolumetricShaders();

}
