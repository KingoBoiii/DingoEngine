// Reading Renderer3D's froxel fog from a fragment shader: include it with <DingoEngine/Volumetrics.glsl>
// after Renderer3DCamera.glsl, whose VolumeParams and VolumeGrid it takes. Renderer3D binds the volume
// to a material by the name u_VolumetricFog.
//
// The texture sits at DE_VOLUMETRIC_FOG_BINDING (12 unless defined first). Its sampler is
// u_VolumetricFogSampler at DE_VOLUMETRIC_FOG_SAMPLER_BINDING (13), unless DE_VOLUMETRIC_FOG_SAMPLER names
// a clamp sampler the shader already declares before the include.
//
// Branch on VolumeParams.x > 0.5 around the calls: it is uniform, so an unfogged scene fetches nothing.

#include <DingoEngine/VolumetricMath.glsl>

#ifndef DE_VOLUMETRIC_FOG_BINDING
#define DE_VOLUMETRIC_FOG_BINDING 12
#endif
#ifndef DE_VOLUMETRIC_FOG_SAMPLER_BINDING
#define DE_VOLUMETRIC_FOG_SAMPLER_BINDING 13
#endif

layout(binding = DE_VOLUMETRIC_FOG_BINDING) uniform texture3D u_VolumetricFog;
#ifndef DE_VOLUMETRIC_FOG_SAMPLER
#define DE_VOLUMETRIC_FOG_SAMPLER u_VolumetricFogSampler
layout(binding = DE_VOLUMETRIC_FOG_SAMPLER_BINDING) uniform sampler u_VolumetricFogSampler;
#endif

// The volume at a world position: rgb = the light scattered towards the camera from there, a = the
// fraction of light that survives the way to the camera. Samples with textureLod, so it may sit in a branch.
vec4 DE_SampleVolumetricFog(mat4 viewProjection, vec4 volumeParams, vec4 volumeGrid, vec3 worldPosition)
{
	vec4 clip = viewProjection * vec4(worldPosition, 1.0);
	float viewDepth = max(clip.w, 1e-4);
	vec2 uv = vec2(clip.x / viewDepth * 0.5 + 0.5, 0.5 - clip.y / viewDepth * 0.5);
	float slices = volumeGrid.z;
	float s = slices * log2(max(viewDepth / volumeParams.y, 1e-4)) * volumeParams.z;
	float w = clamp((s - 0.5) / slices, 0.5 / slices, 1.0 - 0.5 / slices);
	vec4 fog = textureLod(sampler3D(u_VolumetricFog, DE_VOLUMETRIC_FOG_SAMPLER), vec3(uv, w), 0.0);
	return mix(vec4(0.0, 0.0, 0.0, 1.0), fog, min(viewDepth * volumeGrid.w, 1.0));
}

// What a surface's colour becomes behind the fog. target: 0 the fog's own colour, 1 black (Additive
// blending), 2 white (Multiply blending): what leaves the target once the fog has taken its share.
vec3 DE_ApplyVolumetricFog(vec3 color, vec4 fog, int target)
{
	return color * fog.a + (target == 0 ? fog.rgb : (target == 1 ? vec3(0.0) : vec3(1.0 - fog.a)));
}
