// Environment's bake kernels, one thread a cube texel, z = face. DE_ENV_EQUIRECT resamples the panorama
// into mip 0 of the cube (2 x 2 taps a texel); DE_ENV_DOWNSAMPLE fills the next mip from the one above.
// Push constant x is the size of the mip being written.

#type compute
#version 450
layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

#include <DingoEngine/CubeMap.glsl>
#include <DingoEngine/Environment.glsl>

layout(push_constant) uniform PushData { uvec4 P; };
layout(binding = 2) uniform sampler u_Sampler;
layout(binding = 3, rgba16f) uniform writeonly image2DArray u_Target;

#if defined(DE_ENV_EQUIRECT)

layout(binding = 1) uniform texture2D u_Source;

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

layout(binding = 1) uniform textureCube u_Source;

void main()
{
	uvec3 id = gl_GlobalInvocationID;
	if (id.x >= P.x || id.y >= P.x)
		return;

	vec2 uv = (vec2(id.xy) + 0.5) / float(P.x);
	vec3 dir = DE_CubeFaceDirection(id.z, uv);
	imageStore(u_Target, ivec3(id), vec4(textureLod(samplerCube(u_Source, u_Sampler), dir, 0.0).rgb, 1.0));
}

#endif
