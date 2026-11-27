#pragma once

#include <vector>

namespace Dingo
{
	class ComputePass;
	class Sampler;
	class Shader;
	class Texture;
}

namespace Dingo::Internal
{

	enum class EnvironmentShaderVariant
	{
		EquirectBake,
		Downsample,
		Skybox,
		ShProject,
		SpecularPrefilter,
		BrdfLut,
		SkyTransmittance,
		SkyMultiScatter,
		SkyView,
		SkyCube
	};

	// Created on first use, on the main thread, and shared by every Environment; nullptr while it is not valid (it failed to compile, and no hot reload has fixed it).
	Shader* GetEnvironmentShader(EnvironmentShaderVariant variant);

	// Linear, repeating in U and clamped in V, for reading the panorama; nullptr in a headless app.
	Sampler* GetEquirectSampler();

	// The 128 x 128 RGBA16F split-sum table (x = scale, y = bias of F0; see DE_BrdfLutUv in Ibl.glsl), made on
	// first use without submitting anything; nullptr in a headless app. Its contents are undefined until
	// EnsureBrdfLutBaked has recorded the bake.
	Texture* GetBrdfLut();

	// Records the LUT bake into the frame being recorded the first time, and returns whether the LUT is baked
	// (false when the bake shader or texture is not valid). The caller is inside a frame; the pass it makes
	// goes onto `passes`, which the caller deletes once the frame has run.
	bool EnsureBrdfLutBaked(std::vector<ComputePass*>& passes);

	// Before the renderer's own resources go.
	void DestroyEnvironmentShaders();

}
