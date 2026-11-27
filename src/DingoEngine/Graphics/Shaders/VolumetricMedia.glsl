// The local fog volumes of the volumetric kernels: include it with <DingoEngine/VolumetricMedia.glsl> after
// <DingoEngine/VolumetricMath.glsl>. Mirrors Renderer3D::FogVolumeData at binding 8. Define
// DE_MEDIA_BLOCK_ONLY first to take the block alone, for a kernel that never samples a volume. Otherwise the
// noise texture sits at 9 and its sampler at 10.

#define DE_MAX_FOG_VOLUMES 16

struct FogVolumeRecord
{
	vec4 WorldToLocal[3]; // affine rows: the box is [-1, 1]^3, the ellipsoid the unit ball
	vec4 Shape;           // xyz = half extents in world units, w = 0 box, 1 ellipsoid
	vec4 Medium;          // x = density, y = anisotropy, z = edge fade in world units, w = raymarch steps
	vec4 VolumeAlbedo;    // rgb = albedo, a = ambient intensity
	vec4 VolumeTint;      // rgb = tint, a = sun light outside every cascade
	vec4 VolumeLight;     // x = directional, y = local lights, z = max in-scatter, w = 1 with noise
	vec4 HeightFade;      // x = fade start, y = fade end, z = band bottom, w = band top
	vec4 HeightBand;      // x = band multiplier, y = softness
	vec4 Fades;           // near start, near end, far start, far end
	vec4 FadeKeep;        // near keep, far keep, noise min, noise max
	vec4 Noise;           // xyz = phase in noise tiles, w = 1 / scale
	vec4 Drift;           // xyz = world displacement of the noise since the history frame
};

layout(std140, binding = 8) uniform FogVolumeData
{
	vec4 MediaGlobal;                              // x = local-light intensity, y = max in-scatter, z = debug view, w = 1 with noise
	vec4 DirectionalScatter;                       // per directional light
	vec4 LocalScatter[8];                          // light slot s at [s / 4][s % 4]
	FogVolumeRecord Volumes[DE_MAX_FOG_VOLUMES];
};

#ifndef DE_MEDIA_BLOCK_ONLY

layout(binding = 9) uniform texture3D u_VolumeNoise;
layout(binding = 10) uniform sampler u_VolumeNoiseSampler;

// Volume `index` at a world point `cameraDistance` from the camera: its extinction (edge, height and noise
// applied) and the weight its distance fades give its in-scatter. The fades never touch the extinction.
void DE_FogVolumeSample(int index, vec3 world, float cameraDistance, out float sigmaT, out float scatterWeight)
{
	sigmaT = 0.0;
	scatterWeight = 1.0;

	vec4 point = vec4(world, 1.0);
	vec3 local = vec3(dot(Volumes[index].WorldToLocal[0], point), dot(Volumes[index].WorldToLocal[1], point), dot(Volumes[index].WorldToLocal[2], point));
	float edge = Volumes[index].Shape.w > 0.5 ? DE_EllipsoidEdgeFade(local, Volumes[index].Shape.xyz, Volumes[index].Medium.z)
		: DE_BoxEdgeFade(local, Volumes[index].Shape.xyz, Volumes[index].Medium.z);
	if (edge <= 0.0)
		return;

	float density = Volumes[index].Medium.x * edge * DE_HeightProfile((local.y + 1.0) * Volumes[index].Shape.y, Volumes[index].HeightFade, Volumes[index].HeightBand);
	if (Volumes[index].VolumeLight.w > 0.5)
	{
		float n = textureLod(sampler3D(u_VolumeNoise, u_VolumeNoiseSampler), world * Volumes[index].Noise.w - Volumes[index].Noise.xyz, 0.0).r;
		density *= mix(Volumes[index].FadeKeep.z, Volumes[index].FadeKeep.w, n);
	}
	sigmaT = density;
	scatterWeight = DE_DistanceFade(cameraDistance, Volumes[index].Fades.x, Volumes[index].Fades.y, Volumes[index].FadeKeep.x, 1.0)
		* DE_DistanceFade(cameraDistance, Volumes[index].Fades.z, Volumes[index].Fades.w, 1.0, Volumes[index].FadeKeep.y);
}

// The debug view's colour for the directional shadow at `world`: lit green, shadowed red, outside every cascade
// blue (by the weight the last cascade's tail gives the outside light), and yellow where no shadow exists.
// Needs <DingoEngine/Shadows.glsl>.
vec3 DE_ShadowSampleColor(vec3 world)
{
	if (ShadowCounts.x <= 0 || ShadowCounts.y < 0)
		return vec3(1.0, 1.0, 0.0);
	vec2 parts = DE_DirectionalShadowVolumeParts(world);
	return mix(mix(vec3(1.0, 0.0, 0.0), vec3(0.0, 1.0, 0.0), parts.x), vec3(0.0, 0.0, 1.0), parts.y);
}

#endif
