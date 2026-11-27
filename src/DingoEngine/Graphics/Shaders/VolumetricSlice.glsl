// The volumetric fog's debug slice: copies slice k of the integrated or the scatter volume into a 2D picture,
// either the in-scatter times an exposure or the alpha (the transmittance of the integrated volume, the
// extinction of the scatter volume) times the same exposure. The volume is read with texelFetch on a shader
// resource view, since imageLoad on rgba16f does not compile for D3D.

#type compute
#version 450
#extension GL_EXT_samplerless_texture_functions : require

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(std140, binding = 1) uniform SliceData
{
	vec4 SliceParams; // x = the slice, y = the exposure, z = 1 to show the alpha
};

layout(binding = 5) uniform texture3D u_Volume;
layout(binding = 7, rgba16f) uniform writeonly image2D u_Slice;

void main()
{
	ivec2 id = ivec2(gl_GlobalInvocationID.xy);
	ivec3 size = textureSize(u_Volume, 0);
	if (any(greaterThanEqual(id, size.xy)))
		return;

	int slice = clamp(int(SliceParams.x), 0, size.z - 1);
	vec4 texel = texelFetch(u_Volume, ivec3(id, slice), 0);
	vec3 shown = SliceParams.z > 0.5 ? vec3(texel.a) : texel.rgb;
	imageStore(u_Slice, id, vec4(shown * SliceParams.y, 1.0));
}
