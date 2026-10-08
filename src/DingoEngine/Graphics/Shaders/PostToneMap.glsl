// The post chain's last pass: the HDR scene, times the exposure, through the tone curve into the
// caller's target. Mirrors Dingo::Internal::PostProcessData.

#type vertex
#version 450
#include <DingoEngine/Fullscreen.glsl>

#type fragment
#version 450
#include <DingoEngine/ToneMapping.glsl>

layout(location = 0) in vec2 v_TexCoord;

layout(std140, binding = 0) uniform ToneMapData
{
	vec4 Tone; // x = operator, y = exposure as a multiplier, z = knee, w = white point
};

layout(binding = 1) uniform texture2D u_Scene;
layout(binding = 2) uniform sampler u_SceneSampler;

layout(location = 0) out vec4 o_Color;

void main()
{
	vec4 scene = texture(sampler2D(u_Scene, u_SceneSampler), v_TexCoord);
	o_Color = vec4(ToneMap(scene.rgb * Tone.y, int(Tone.x), Tone.z, Tone.w), scene.a);
}
