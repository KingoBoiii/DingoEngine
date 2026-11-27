// Environment's bake kernels, one variant per define, each declaring only the bindings it uses (a binding set
// has to match the shader's reflection exactly). DE_ENV_EQUIRECT resamples the panorama into mip 0 of the cube
// (2 x 2 taps a texel, z = face); DE_ENV_DOWNSAMPLE fills the next mip from the one above; DE_ENV_SH projects
// the cube onto 9 spherical-harmonic coefficients of irradiance / pi in one group; DE_ENV_PREFILTER writes one
// mip of the GGX-prefiltered specular cube, roughness rising with the mip; DE_IBL_BRDF_LUT fills the split-sum
// environment BRDF table. Push constants: EQUIRECT and DOWNSAMPLE x = size of the mip written; SH x = edge of the
// source mip, which the binding already starts at; PREFILTER x = size of the mip written, y = its level, z = edge of the source cube. Every include sits at the top: the expander pastes each file once, before #if is evaluated.

#type compute
#version 450

#include <DingoEngine/CubeMap.glsl>
#include <DingoEngine/Environment.glsl>
#include <DingoEngine/Brdf.glsl>
#include <DingoEngine/Ibl.glsl>

#if defined(DE_ENV_EQUIRECT)

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
layout(push_constant) uniform PushData { uvec4 P; };
layout(binding = 1) uniform texture2D u_Source;
layout(binding = 2) uniform sampler u_Sampler;
layout(binding = 3, rgba16f) uniform writeonly image2DArray u_Target;

void main()
{
	uvec3 id = gl_GlobalInvocationID;
	if (id.x >= P.x || id.y >= P.x)
		return;

	vec3 sum = vec3(0.0);
	for (int j = 0; j < 2; ++j)
	{
		for (int i = 0; i < 2; ++i)
		{
			vec2 uv = (vec2(id.xy) + vec2(0.25 + 0.5 * float(i), 0.25 + 0.5 * float(j))) / float(P.x);
			vec3 dir = DE_CubeFaceDirection(id.z, uv);
			sum += textureLod(sampler2D(u_Source, u_Sampler), DE_EquirectUv(dir), 0.0).rgb;
		}
	}
	imageStore(u_Target, ivec3(id), vec4(sum * 0.25, 1.0));
}

#elif defined(DE_ENV_DOWNSAMPLE)

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
layout(push_constant) uniform PushData { uvec4 P; };
layout(binding = 1) uniform textureCube u_Source;
layout(binding = 2) uniform sampler u_Sampler;
layout(binding = 3, rgba16f) uniform writeonly image2DArray u_Target;

void main()
{
	uvec3 id = gl_GlobalInvocationID;
	if (id.x >= P.x || id.y >= P.x)
		return;

	vec2 uv = (vec2(id.xy) + 0.5) / float(P.x);
	vec3 dir = DE_CubeFaceDirection(id.z, uv);
	imageStore(u_Target, ivec3(id), vec4(textureLod(samplerCube(u_Source, u_Sampler), dir, 0.0).rgb, 1.0));
}

#elif defined(DE_ENV_SH)

layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;
layout(push_constant) uniform PushData { uvec4 P; };
layout(binding = 1) uniform textureCube u_Source;
layout(binding = 2) uniform sampler u_Sampler;
layout(std430, binding = 4) writeonly buffer DE_EnvironmentShOut { vec4 C[9]; };

shared vec4 s_Sum[9][64];

void main()
{
	uint t = gl_LocalInvocationIndex;
	uint size = P.x;
	uint perFace = size * size;
	uint total = perFace * 6u;

	vec4 acc[9];
	for (int k = 0; k < 9; ++k)
		acc[k] = vec4(0.0);

	for (uint i = t; i < total; i += 64u)
	{
		uint face = i / perFace;
		uint rem = i - face * perFace;
		uint y = rem / size;
		uint x = rem - y * size;
		vec2 uv = (vec2(float(x), float(y)) + 0.5) / float(size);
		vec3 dir = DE_CubeFaceDirection(face, uv);
		float omega = DE_CubeTexelSolidAngle(uv, float(size));
		vec3 radiance = textureLod(samplerCube(u_Source, u_Sampler), dir, 0.0).rgb * omega;
		for (int k = 0; k < 9; ++k)
			acc[k] += vec4(radiance * DE_ShBasis(k, dir), omega);
	}

	for (int k = 0; k < 9; ++k)
		s_Sum[k][t] = acc[k];
	memoryBarrierShared();
	barrier();

	for (uint stride = 32u; stride > 0u; stride >>= 1u)
	{
		if (t < stride)
		{
			for (int k = 0; k < 9; ++k)
				s_Sum[k][t] += s_Sum[k][t + stride];
		}
		memoryBarrierShared();
		barrier();
	}

	if (t == 0u)
	{
		float scale = 4.0 * DE_PI / s_Sum[0][0].a;
		for (int k = 0; k < 9; ++k)
		{
			int band = k == 0 ? 0 : (k < 4 ? 1 : 2);
			C[k] = vec4(s_Sum[k][0].rgb * (scale * DE_ShLambertOverPi(band)), 0.0);
		}
	}
}

#elif defined(DE_ENV_PREFILTER)

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
layout(push_constant) uniform PushData { uvec4 P; };
layout(binding = 1) uniform textureCube u_Source;
layout(binding = 2) uniform sampler u_Sampler;
layout(binding = 3, rgba16f) uniform writeonly image2DArray u_Target;

const uint k_SampleCount = 64u;

void main()
{
	uvec3 id = gl_GlobalInvocationID;
	if (id.x >= P.x || id.y >= P.x)
		return;

	vec2 uv = (vec2(id.xy) + 0.5) / float(P.x);
	vec3 N = DE_CubeFaceDirection(id.z, uv);
	float sourceSize = float(P.z);

	if (P.y == 0u)
	{
		float lod = max(log2(sourceSize / float(P.x)), 0.0);
		imageStore(u_Target, ivec3(id), vec4(textureLod(samplerCube(u_Source, u_Sampler), N, lod).rgb, 1.0));
		return;
	}

	float roughness = DE_PrefilterRoughness(float(P.y));
	float a = roughness * roughness;

	vec3 up = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
	vec3 T = normalize(cross(up, N));
	vec3 B = cross(N, T);

	vec3 sum = vec3(0.0);
	float weight = 0.0;
	for (uint i = 0u; i < k_SampleCount; ++i)
	{
		vec3 h = DE_ImportanceSampleGgx(DE_Hammersley(i, k_SampleCount), a);
		vec3 H = T * h.x + B * h.y + N * h.z;
		vec3 L = H * (2.0 * dot(N, H)) - N;
		float NoL = max(dot(N, L), 0.0);
		float lod = DE_PrefilterSourceLod(h.z, a, sourceSize, float(k_SampleCount));
		sum += textureLod(samplerCube(u_Source, u_Sampler), L, lod).rgb * NoL;
		weight += NoL;
	}
	imageStore(u_Target, ivec3(id), vec4(sum / max(weight, 1e-4), 1.0));
}

#elif defined(DE_IBL_BRDF_LUT)

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
layout(binding = 3, rgba16f) uniform writeonly image2D u_Target;

void main()
{
	uvec2 id = gl_GlobalInvocationID.xy;
	if (id.x >= uint(k_BrdfLutSize) || id.y >= uint(k_BrdfLutSize))
		return;

	vec2 uv = (vec2(id) + 0.5) / float(k_BrdfLutSize);
	vec2 value = DE_BrdfLutValue(uv);
	imageStore(u_Target, ivec2(id), vec4(DE_IntegrateBrdf(value.x, value.y, 256.0), 0.0, 1.0));
}

#endif
