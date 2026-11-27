// The froxel fog's mapping and scattering terms, for the volumetric kernels and any shader that reads the
// volume: include it with <DingoEngine/VolumetricMath.glsl>. Written in the GLSL subset that also compiles as
// C++ over GLM (float literals carry the f suffix for that), so VolumetricTests.cpp checks this text.

#ifndef DE_PI
#define DE_PI 3.14159265358979f
#endif

// View depth of slice boundary b of `slices`: Near at 0, Far at `slices`, exponential in between.
float DE_VolumeSliceDepth(float b, float slices, float nearDistance, float farDistance)
{
	return nearDistance * pow(farDistance / nearDistance, b / slices);
}

// The continuous slice index of a view depth; the inverse of DE_VolumeSliceDepth.
float DE_VolumeDepthToSlice(float viewDepth, float slices, float nearDistance, float farDistance)
{
	return slices * log2(viewDepth / nearDistance) / log2(farDistance / nearDistance);
}

// The texture coordinate along the volume's depth axis for a view depth, centred on slice texels and kept
// inside the first and last texel centres.
float DE_VolumeDepthToCoord(float viewDepth, float slices, float nearDistance, float farDistance)
{
	float s = DE_VolumeDepthToSlice(viewDepth, slices, nearDistance, farDistance);
	return clamp((s - 0.5f) / slices, 0.5f / slices, 1.0f - 0.5f / slices);
}

// Henyey-Greenstein phase function, normalised over the sphere. cosTheta is between the light's travel
// direction reversed and the ray's direction; g > 0 scatters forwards.
float DE_HenyeyGreenstein(float cosTheta, float g)
{
	float g2 = g * g;
	float denom = 1.0f + g2 - 2.0f * g * cosTheta;
	return (1.0f - g2) / (4.0f * DE_PI * denom * sqrt(max(denom, 1e-6f)));
}

// Extinction of the height fog at a world height.
float DE_HeightFogDensity(float density, float heightFalloff, float baseHeight, float worldY)
{
	return density * exp(-heightFalloff * max(worldY - baseHeight, 0.0f));
}

// exp(-sigmaT * len): the fraction of light that crosses one slice of length `len`.
float DE_SliceTransmittance(float sigmaT, float len)
{
	return exp(-sigmaT * len);
}

// The integral of exp(-sigmaT * t) over one slice, which scales a constant in-scatter term S into the slice's
// contribution; `transmittance` is DE_SliceTransmittance for the same slice.
float DE_SliceInScatterFactor(float sigmaT, float len, float transmittance)
{
	return sigmaT > 1e-6f ? (1.0f - transmittance) / sigmaT : len;
}

// Halton sequence, base 2: the radical inverse of the index, in [0, 1).
float DE_HaltonBase2(uint index)
{
	return float(bitfieldReverse(index)) * 2.3283064365386963e-10f;
}
