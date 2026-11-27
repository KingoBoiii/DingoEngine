#pragma once

namespace Dingo
{
	class Sampler;
	class Shader;
}

namespace Dingo::Internal
{

	enum class EnvironmentShaderVariant
	{
		EquirectBake,
		Downsample,
		Skybox
	};

	// Created on first use, on the main thread, and shared by every Environment; nullptr while it is not valid (it failed to compile, and no hot reload has fixed it).
	Shader* GetEnvironmentShader(EnvironmentShaderVariant variant);

	// Linear, repeating in U and clamped in V, for reading the panorama; nullptr in a headless app.
	Sampler* GetEquirectSampler();

	// Before the renderer's own resources go.
	void DestroyEnvironmentShaders();

}
