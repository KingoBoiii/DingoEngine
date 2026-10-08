// Renderer3D's shadow probes: pixel i of a 256 x 1 R8 target is probe i's answer, the shadow lookup
// the lit shader runs (Shadows.glsl) at the probe's point, with no surface normal. Mirrors
// Renderer3D::ShadowProbeData.

#type vertex
#version 450
#include <DingoEngine/Fullscreen.glsl>

#type fragment
#version 450

const int MAX_SHADOW_PROBES = 256;

layout(std140, binding = 0) uniform ProbeData
{
	vec4 Probes[MAX_SHADOW_PROBES]; // xyz = the point, w = -1: the shadowed directional light, else a local light slot
};

#include <DingoEngine/Shadows.glsl>

layout(location = 0) in vec2 v_TexCoord;
layout(location = 0) out vec4 o_Visibility;

void main()
{
	vec4 probe = Probes[min(int(gl_FragCoord.x), MAX_SHADOW_PROBES - 1)];
	float visibility = probe.w < 0.0 ? DirectionalShadow(probe.xyz, vec3(0.0)) : LocalLightShadow(int(probe.w), probe.xyz, vec3(0.0));
	o_Visibility = vec4(visibility, 0.0, 0.0, 1.0);
}
