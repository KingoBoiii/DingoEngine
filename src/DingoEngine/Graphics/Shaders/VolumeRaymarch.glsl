// The Raymarch FogVolume kernel: one invocation per texel of a low-resolution grid marches the ray through
// each Raymarch volume, up to the scene surface (the nearest depth of the full-resolution pixels the texel
// covers), and stores the in-scatter (rgb) and transmittance (a) of all of them, plus the view distance it
// stopped at for the composite's depth-aware upsample. Samples sit at a frame-stable per-texel jitter and
// each step is integrated analytically, so few steps give nearly what many do. Only the shadowed
// directional light and the ambient light scatter here; local lights are the froxel volume's. Every include
// sits at the top: the expander pastes each file once, before #if.

#type compute
#version 450
#extension GL_EXT_samplerless_texture_functions : require

#include <DingoEngine/Renderer3DCamera.glsl>
#define DE_SHADOW_DATA_BINDING 2
#define DE_SHADOW_ATLAS_BINDING 3
#define DE_SHADOW_SAMPLER_BINDING 4
#include <DingoEngine/Shadows.glsl>
#include <DingoEngine/VolumetricMath.glsl>
#include <DingoEngine/VolumetricMedia.glsl>
#define DE_VOLUMETRIC_FOG_BINDING 11
#define DE_VOLUMETRIC_FOG_SAMPLER_BINDING 12
#include <DingoEngine/Volumetrics.glsl>

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(std140, binding = 1) uniform RaymarchData
{
	mat4 RayInverseViewProjection;
	vec4 RayOrigin;
	vec4 RayTarget; // xy = the low-resolution size, zw = the scene target's size
	vec4 RayParams; // x = the first Raymarch record, y = their count, z = the resolution divisor, w = 1 when the froxel volume is active
	vec4 RayReserved;
};

layout(binding = 5) uniform texture2D u_SceneDepth;
layout(binding = 6, r32f) uniform writeonly image2D u_RayDepth;
layout(binding = 7, rgba16f) uniform writeonly image2D u_RayScatter;

const float k_NoSurface = 1e30;
const int k_MaxRaymarchVolumes = 4;

vec3 DE_RaymarchSource(int volume, vec3 world, vec3 rayDirection, float sigmaT, float weight, vec3 ambient)
{
	vec3 coef = Volumes[volume].VolumeAlbedo.rgb * Volumes[volume].VolumeTint.rgb * (sigmaT * weight);
	if (MediaGlobal.z > 0.5)
		return DE_ShadowSampleColor(world) * dot(coef * Volumes[volume].VolumeLight.x, vec3(1.0 / 3.0));

	vec3 light = ambient * Volumes[volume].VolumeAlbedo.a;

	vec2 parts = ShadowCounts.y >= 0 ? DE_DirectionalShadowVolumeParts(world) : vec2(1.0, 0.0);
	float strength = ShadowParams.x;
	float shadowed = parts.x * (1.0 - parts.y) + parts.y * (1.0 - strength + strength * Volumes[volume].VolumeTint.a);

	float g = Volumes[volume].Medium.y;
	for (int i = 0; i < LightCounts.x; ++i)
	{
		float phase = DE_PI * DE_HenyeyGreenstein(dot(DirectionalLights[i].Direction.xyz, -rayDirection), g);
		float visibility = i == ShadowCounts.y ? shadowed : 1.0;
		light += phase * DirectionalLights[i].Color.rgb * (DirectionalScatter[i] * Volumes[volume].VolumeLight.x * visibility);
	}
	return light * coef;
}

void main()
{
	ivec2 id = ivec2(gl_GlobalInvocationID.xy);
	if (any(greaterThanEqual(vec2(id), RayTarget.xy)))
		return;

	int scale = int(RayParams.z);
	ivec2 full = ivec2(RayTarget.zw);
	float nearest = 1.0;
	ivec2 nearestPixel = min(id * scale + ivec2(scale / 2), full - ivec2(1));
	for (int y = 0; y < scale; ++y)
	{
		for (int x = 0; x < scale; ++x)
		{
			ivec2 pixel = min(id * scale + ivec2(x, y), full - ivec2(1));
			float depth = texelFetch(u_SceneDepth, pixel, 0).r;
			if (depth < nearest)
			{
				nearest = depth;
				nearestPixel = pixel;
			}
		}
	}

	bool sky = nearest >= 1.0;
	vec2 uv = (vec2(nearestPixel) + 0.5) / RayTarget.zw;
	vec4 unprojected = RayInverseViewProjection * vec4(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, sky ? 0.5 : nearest, 1.0);
	vec3 toPoint = unprojected.xyz / unprojected.w - RayOrigin.xyz;
	float pointDistance = max(length(toPoint), 1e-8);
	vec3 rayDirection = toPoint / pointDistance;
	float surface = sky ? k_NoSurface : pointDistance;
	float jitter = 0.001 + 0.998 * DE_InterleavedGradientNoise(vec2(id));

	vec3 volumeScatter[k_MaxRaymarchVolumes];
	float volumeTransmittance[k_MaxRaymarchVolumes];
	float volumeEntry[k_MaxRaymarchVolumes];
	int count = 0;
	vec3 ambient = DE_VolumeAmbient();

	int first = int(RayParams.x);
	int last = first + min(int(RayParams.y), k_MaxRaymarchVolumes);
	for (int volume = first; volume < last; ++volume)
	{
		vec4 origin4 = vec4(RayOrigin.xyz, 1.0);
		vec4 direction4 = vec4(rayDirection, 0.0);
		vec3 localOrigin = vec3(dot(Volumes[volume].WorldToLocal[0], origin4), dot(Volumes[volume].WorldToLocal[1], origin4), dot(Volumes[volume].WorldToLocal[2], origin4));
		vec3 localDirection = vec3(dot(Volumes[volume].WorldToLocal[0], direction4), dot(Volumes[volume].WorldToLocal[1], direction4), dot(Volumes[volume].WorldToLocal[2], direction4));
		vec2 span = Volumes[volume].Shape.w > 0.5 ? DE_RayUnitSphereInterval(localOrigin, localDirection) : DE_RayBoxInterval(localOrigin, localDirection);
		float t0 = max(span.x, 0.0);
		float t1 = min(span.y, surface);
		if (!(t1 > t0))
			continue;

		int steps = clamp(int(Volumes[volume].Medium.w), 1, 64);
		float stepLength = (t1 - t0) / float(steps);
		vec3 scattered = vec3(0.0);
		float transmittance = 1.0;
		for (int i = 0; i < steps; ++i)
		{
			float t = t0 + (float(i) + jitter) * stepLength;
			vec3 world = RayOrigin.xyz + rayDirection * t;
			float sigmaT;
			float weight;
			DE_FogVolumeSample(volume, world, t, sigmaT, weight);
			float stepTransmittance = DE_SliceTransmittance(sigmaT, stepLength);
			if (sigmaT > 0.0)
				scattered += transmittance * DE_RaymarchSource(volume, world, rayDirection, sigmaT, weight, ambient) * DE_SliceInScatterFactor(sigmaT, stepLength, stepTransmittance);
			transmittance *= stepTransmittance;
		}

		scattered = DE_CapInScatter(scattered, Volumes[volume].VolumeLight.z);
		if (RayParams.w > 0.5)
			scattered *= DE_SampleVolumetricFog(ViewProjection, VolumeParams, VolumeGrid, RayOrigin.xyz + rayDirection * t0).a;

		int slot = count;
		while (slot > 0 && volumeEntry[slot - 1] > t0)
		{
			volumeEntry[slot] = volumeEntry[slot - 1];
			volumeScatter[slot] = volumeScatter[slot - 1];
			volumeTransmittance[slot] = volumeTransmittance[slot - 1];
			--slot;
		}
		volumeEntry[slot] = t0;
		volumeScatter[slot] = scattered;
		volumeTransmittance[slot] = transmittance;
		++count;
	}

	vec3 result = vec3(0.0);
	float resultTransmittance = 1.0;
	for (int k = 0; k < count; ++k)
	{
		result += resultTransmittance * volumeScatter[k];
		resultTransmittance *= volumeTransmittance[k];
	}

	imageStore(u_RayScatter, id, vec4(result, resultTransmittance));
	imageStore(u_RayDepth, id, vec4(surface, 0.0, 0.0, 0.0));
}
