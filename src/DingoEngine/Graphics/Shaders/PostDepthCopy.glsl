// The post chain's copy of the scene depth into an R32F target, for passes that read the depth while
// drawing into the scene target with its depth bound (soft particles).

#type vertex
#version 450
#include <DingoEngine/Fullscreen.glsl>

#type fragment
#version 450

layout(binding = 1) uniform texture2D u_Depth;
layout(binding = 2) uniform sampler u_DepthSampler;

layout(location = 0) in vec2 v_TexCoord;
layout(location = 0) out vec4 o_Depth;

void main()
{
	o_Depth = vec4(textureLod(sampler2D(u_Depth, u_DepthSampler), v_TexCoord, 0.0).r, 0.0, 0.0, 1.0);
}
