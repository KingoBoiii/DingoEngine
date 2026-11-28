// The froxel volume's inject kernel: one invocation per froxel writes (sigmaS * L_in, sigmaT) at a jittered
// depth inside its slice, blended with the reprojected previous frame. VolumetricIntegrate.glsl accumulates
// the result along each ray. The medium is the global fog plus each local froxel volume; a scene without
// volumes takes the single-medium path unless a debug view is on; the debug view replaces the light with a false
// colour of its shadow sample, scaled by the sun's in-scatter coefficient. Every include sits at the top: the expander pastes each file once,
// before #if.

#type compute
#version 450

#include <DingoEngine/Renderer3DCamera.glsl>
#define DE_SHADOW_DATA_BINDING 2
#define DE_SHADOW_ATLAS_BINDING 3
#define DE_SHADOW_SAMPLER_BINDING 4
#include <DingoEngine/Shadows.glsl>
#include <DingoEngine/VolumetricMath.glsl>
#include <DingoEngine/VolumetricMedia.glsl>

layout(local_size_x = 4, local_size_y = 4, local_size_z = 4) in;

layout(std140, binding = 1) uniform VolumetricData
{
	vec4 VolCameraPosition;
	vec4 FrustumRays[4];
	mat4 PreviousViewProjection;
	vec4 Grid;        // xyz = size, w = 1 when the history is valid
	vec4 Slices;      // x = near, y = far, z = log2(far / near), w = jitter
	vec4 Medium;      // x = density, y = base height, z = height falloff, w = anisotropy
	vec4 Albedo;      // rgb = albedo, a = weight of the current frame
	vec4 Intensities; // x = ambient, y = directional, z = froxel volumes, w = local-light slots
	vec4 ScatterTint; // rgb = in-scatter tint, a = sun light outside every cascade
};

layout(binding = 5) uniform texture3D u_History;
layout(binding = 6) uniform sampler u_HistorySampler;
layout(binding = 7, rgba16f) uniform writeonly image3D u_Scatter;

vec3 DE_FroxelRay(vec2 uv)
{
	return mix(mix(FrustumRays[0].xyz, FrustumRays[1].xyz, uv.x), mix(FrustumRays[2].xyz, FrustumRays[3].xyz, uv.x), uv.y);
}

float DE_FroxelDensity(vec3 world)
{
	return DE_HeightFogDensity(Medium.x, Medium.z, Medium.y, world.y);
}

vec3 DE_FroxelInScatter(vec3 world, vec3 rayDirection)
{
	vec3 light = DE_VolumeAmbient() * Intensities.x;
	for (int i = 0; i < LightCounts.x; ++i)
	{
		float visibility = i == ShadowCounts.y ? DE_DirectionalShadowVolume(world, ScatterTint.a) : 1.0;
		float phase = DE_PI * DE_HenyeyGreenstein(dot(DirectionalLights[i].Direction.xyz, -rayDirection), Medium.w);
		light += phase * DirectionalLights[i].Color.rgb * (Intensities.y * DirectionalScatter[i]) * visibility;
	}
	return light * ScatterTint.rgb;
}

// The medium at `world` is the global fog plus every volume. Extinctions add, the in-scatter coefficients add
// per light kind (ambient, directional, directional outside the cascades), and the phase function takes the
// extinction-weighted anisotropy. `drift` is the noise displacement of the volume that dominates the sample.
vec4 DE_FroxelMediaSample(vec3 world, vec3 rayDirection, float cameraDistance, out vec3 drift)
{
	float sigmaGlobal = DE_FroxelDensity(world);
	float sigmaT = sigmaGlobal;
	float weightedG = Medium.w * sigmaGlobal;
	vec3 base = Albedo.rgb * sigmaGlobal * ScatterTint.rgb;
	vec3 coefAmbient = base * Intensities.x;
	vec3 coefSun = base * Intensities.y;
	vec3 coefSunOutside = coefSun * ScatterTint.a;
	vec3 coefLocal = base * MediaGlobal.x;
	drift = vec3(0.0);
	float dominant = 0.0;

	for (int v = 0; v < int(Intensities.z); ++v)
	{
		float sigma;
		float weight;
		DE_FogVolumeSample(v, world, cameraDistance, sigma, weight);
		if (sigma <= 0.0)
			continue;

		vec3 coef = Volumes[v].VolumeAlbedo.rgb * sigma * Volumes[v].VolumeTint.rgb * weight;
		sigmaT += sigma;
		weightedG += Volumes[v].Medium.y * sigma;
		coefAmbient += coef * Volumes[v].VolumeAlbedo.a;
		vec3 sun = coef * Volumes[v].VolumeLight.x;
		coefSun += sun;
		coefSunOutside += sun * Volumes[v].VolumeTint.a;
		coefLocal += coef * (Volumes[v].VolumeLight.y * MediaGlobal.x);
		if (Volumes[v].VolumeLight.w > 0.5 && sigma > dominant)
		{
			dominant = sigma;
			drift = Volumes[v].Drift.xyz;
		}
	}

	float g = sigmaT > 1e-6 ? weightedG / sigmaT : Medium.w;
	vec2 parts = ShadowCounts.y >= 0 ? DE_DirectionalShadowVolumeParts(world) : vec2(1.0, 0.0);
	float strength = ShadowParams.x;
	vec3 sunLit = coefSun * (parts.x * (1.0 - parts.y) + parts.y * (1.0 - strength)) + coefSunOutside * (parts.y * strength);

	vec3 light = DE_VolumeAmbient() * coefAmbient;
	for (int i = 0; i < LightCounts.x; ++i)
	{
		float phase = DE_PI * DE_HenyeyGreenstein(dot(DirectionalLights[i].Direction.xyz, -rayDirection), g);
		vec3 medium = i == ShadowCounts.y ? sunLit : coefSun;
		light += phase * DirectionalLights[i].Color.rgb * DirectionalScatter[i] * medium;
	}

	vec3 localLit = vec3(0.0);
	for (int i = 0; i < int(Intensities.w); ++i)
	{
		float scatter = LocalScatter[i / 4][i % 4];
		LocalLight lamp = LocalLights[i];
		vec3 fromLight = world - lamp.PositionRange.xyz;
		float distanceSquared = dot(fromLight, fromLight);
		float rangeSquared = lamp.PositionRange.w * lamp.PositionRange.w;
		if (scatter <= 0.0 || !(distanceSquared < rangeSquared))
			continue;

		fromLight *= inversesqrt(max(distanceSquared, 1e-8));
		float falloff = 1.0 - distanceSquared / rangeSquared;
		float cone = clamp(dot(fromLight, lamp.SpotDirection.xyz) * lamp.Color.w + lamp.SpotDirection.w, 0.0, 1.0);
		if (cone <= 0.0)
			continue;

		float phase = DE_PI * DE_HenyeyGreenstein(dot(fromLight, -rayDirection), g);
		localLit += phase * lamp.Color.rgb * (falloff * falloff) * (cone * cone) * scatter * DE_LocalLightShadowVolume(i, world);
	}
	light += localLit * coefLocal;
	if (MediaGlobal.z > 0.5)
		light = DE_ShadowSampleColor(world) * dot(coefSun, vec3(1.0 / 3.0));
	return vec4(light, sigmaT);
}

void main()
{
	uvec3 id = gl_GlobalInvocationID;
	vec3 size = Grid.xyz;
	if (any(greaterThanEqual(vec3(id), size)))
		return;

	vec2 uv = (vec2(id.xy) + 0.5) / size.xy;
	vec3 ray = DE_FroxelRay(uv);
	float depth = DE_VolumeSliceDepth(float(id.z) + Slices.w, size.z, Slices.x, Slices.y);
	vec3 world = VolCameraPosition.xyz + ray * depth;

	vec4 current;
	vec3 drift = vec3(0.0);
	if (Intensities.z > 0.5 || Intensities.w > 0.5 || MediaGlobal.z > 0.5)
	{
		current = DE_FroxelMediaSample(world, normalize(ray), length(world - VolCameraPosition.xyz), drift);
	}
	else
	{
		float sigmaT = DE_FroxelDensity(world);
		vec3 sigmaS = Albedo.rgb * sigmaT;
		current = vec4(sigmaS * DE_FroxelInScatter(world, normalize(ray)), sigmaT);
	}

	vec4 result = current;
	if (Grid.w > 0.5)
	{
		float centreDepth = DE_VolumeSliceDepth(float(id.z) + 0.5, size.z, Slices.x, Slices.y);
		vec4 previous = PreviousViewProjection * vec4(VolCameraPosition.xyz + ray * centreDepth - drift, 1.0);
		if (previous.w > 0.0)
		{
			vec3 ndc = previous.xyz / previous.w;
			float s = DE_VolumeDepthToSlice(previous.w, size.z, Slices.x, Slices.y);
			vec3 uvw = vec3(ndc.x * 0.5 + 0.5, 0.5 - ndc.y * 0.5, s / size.z);
			if (all(greaterThanEqual(uvw, vec3(0.0))) && all(lessThanEqual(uvw, vec3(1.0))))
			{
				uvw.z = clamp(uvw.z, 0.5 / size.z, 1.0 - 0.5 / size.z);
				result = mix(textureLod(sampler3D(u_History, u_HistorySampler), uvw, 0.0), current, Albedo.a);
			}
		}
	}
	imageStore(u_Scatter, ivec3(id), result);
}
