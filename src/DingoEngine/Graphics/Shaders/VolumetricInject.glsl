// The froxel volume's inject kernel: one invocation per froxel writes (sigmaS * L_in, sigmaT) at a jittered
// depth inside its slice, blended with the reprojected previous frame. VolumetricIntegrate.glsl accumulates
// the result along each ray. Every include sits at the top: the expander pastes each file once, before #if.

#type compute
#version 450

#include <DingoEngine/Renderer3DCamera.glsl>
#define DE_SHADOW_DATA_BINDING 2
#define DE_SHADOW_ATLAS_BINDING 3
#define DE_SHADOW_SAMPLER_BINDING 4
#include <DingoEngine/Shadows.glsl>
#include <DingoEngine/VolumetricMath.glsl>

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
	vec4 Intensities; // x = ambient, y = directional, zw reserved
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
	vec3 light = AmbientColor.rgb * Intensities.x;
	for (int i = 0; i < LightCounts.x; ++i)
	{
		float visibility = i == ShadowCounts.y ? DE_DirectionalShadowVolume(world, ScatterTint.a) : 1.0;
		float phase = DE_PI * DE_HenyeyGreenstein(dot(DirectionalLights[i].Direction.xyz, -rayDirection), Medium.w);
		light += phase * DirectionalLights[i].Color.rgb * Intensities.y * visibility;
	}
	return light * ScatterTint.rgb;
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

	float sigmaT = DE_FroxelDensity(world);
	vec3 sigmaS = Albedo.rgb * sigmaT;
	vec4 current = vec4(sigmaS * DE_FroxelInScatter(world, normalize(ray)), sigmaT);

	vec4 result = current;
	if (Grid.w > 0.5)
	{
		float centreDepth = DE_VolumeSliceDepth(float(id.z) + 0.5, size.z, Slices.x, Slices.y);
		vec4 previous = PreviousViewProjection * vec4(VolCameraPosition.xyz + ray * centreDepth, 1.0);
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
