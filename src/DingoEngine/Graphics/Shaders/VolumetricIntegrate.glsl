// The froxel volume's integrate kernel: one invocation per column walks the slices front to back and stores,
// per slice, the in-scatter from the camera to the slice's far boundary (rgb) and the transmittance there (a).
// It reads the scatter volume with texelFetch on a shader resource view, since imageLoad on rgba16f does not
// compile for D3D. Every include sits at the top: the expander pastes each file once, before #if.

#type compute
#version 450
#extension GL_EXT_samplerless_texture_functions : require

#include <DingoEngine/VolumetricMath.glsl>

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(std140, binding = 1) uniform VolumetricData
{
	vec4 VolCameraPosition;
	vec4 FrustumRays[4];
	mat4 PreviousViewProjection;
	vec4 Grid;
	vec4 Slices;
	vec4 Medium;
	vec4 Albedo;
	vec4 Intensities;
	vec4 ScatterTint;
};

layout(binding = 5) uniform texture3D u_Scatter;
layout(binding = 7, rgba16f) uniform writeonly image3D u_Integrated;

void main()
{
	uvec2 id = gl_GlobalInvocationID.xy;
	vec3 size = Grid.xyz;
	if (any(greaterThanEqual(vec2(id), size.xy)))
		return;

	vec2 uv = (vec2(id) + 0.5) / size.xy;
	vec3 ray = mix(mix(FrustumRays[0].xyz, FrustumRays[1].xyz, uv.x), mix(FrustumRays[2].xyz, FrustumRays[3].xyz, uv.x), uv.y);
	float rayLength = length(ray);

	vec3 scattered = vec3(0.0);
	float transmittance = 1.0;
	float previousDepth = 0.0;
	for (uint k = 0u; k < uint(size.z); ++k)
	{
		float depth = DE_VolumeSliceDepth(float(k + 1u), size.z, Slices.x, Slices.y);
		float len = (depth - previousDepth) * rayLength;
		previousDepth = depth;

		vec4 s = texelFetch(u_Scatter, ivec3(id, k), 0);
		float tr = DE_SliceTransmittance(s.a, len);
		scattered += transmittance * s.rgb * DE_SliceInScatterFactor(s.a, len, tr);
		transmittance *= tr;
		imageStore(u_Integrated, ivec3(id, k), vec4(scattered, transmittance));
	}
}
