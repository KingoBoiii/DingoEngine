// Draws the Raymarch FogVolume result over the scene target. The result has one texel per divisor x divisor
// block of pixels, so each pixel takes the four nearest texels weighed by bilinear weight and by how close
// their view distance is to the pixel's own (as the AO upsample does), and falls back to the nearest
// distance when none is close. The Alpha blend turns (L / (1 - T), 1 - T) into dst * T + L.

#type vertex
#version 450
#include <DingoEngine/Fullscreen.glsl>

#type fragment
#version 450

layout(location = 0) in vec2 v_TexCoord;
layout(location = 0) out vec4 o_Color;

layout(std140, binding = 0) uniform RaymarchCompositeData
{
	mat4 InverseViewProjection;
	vec4 CameraOrigin;
	vec4 Low; // xy = the low-resolution size, zw = the render target's size over the divisor
};

layout(binding = 1) uniform texture2D u_Scatter;
layout(binding = 2) uniform sampler u_ScatterSampler;
layout(binding = 3) uniform texture2D u_RayDepth;
layout(binding = 4) uniform sampler u_RayDepthSampler;
layout(binding = 5) uniform texture2D u_SceneDepth;
layout(binding = 6) uniform sampler u_SceneDepthSampler;

void main()
{
	float depth = textureLod(sampler2D(u_SceneDepth, u_SceneDepthSampler), v_TexCoord, 0.0).r;
	float pixelDistance = 1e30;
	if (depth < 1.0)
	{
		vec4 world = InverseViewProjection * vec4(v_TexCoord.x * 2.0 - 1.0, 1.0 - v_TexCoord.y * 2.0, depth, 1.0);
		pixelDistance = length(world.xyz / world.w - CameraOrigin.xyz);
	}

	vec2 grid = v_TexCoord * Low.zw - 0.5;
	vec2 texel = 1.0 / Low.xy;
	vec2 base = floor(grid);
	vec2 f = grid - base;

	vec4 sum = vec4(0.0);
	float total = 0.0;
	vec4 nearest = vec4(0.0, 0.0, 0.0, 1.0);
	float nearestDifference = 1e38;
	for (int i = 0; i < 4; ++i)
	{
		vec2 corner = vec2(float(i & 1), float(i >> 1));
		vec2 uv = (clamp(base + corner, vec2(0.0), Low.xy - vec2(1.0)) + 0.5) * texel;
		vec4 scatter = textureLod(sampler2D(u_Scatter, u_ScatterSampler), uv, 0.0);
		float rayDistance = textureLod(sampler2D(u_RayDepth, u_RayDepthSampler), uv, 0.0).r;
		float difference = abs(rayDistance - pixelDistance) / pixelDistance;
		float bilinear = mix(1.0 - f.x, f.x, corner.x) * mix(1.0 - f.y, f.y, corner.y);
		float weight = bilinear / (1e-3 + difference);
		sum += scatter * weight;
		total += weight;
		if (difference < nearestDifference)
		{
			nearestDifference = difference;
			nearest = scatter;
		}
	}

	vec4 result = total > 1e-3 ? sum / total : nearest;
	float transmittance = result.a;
	float peak = max(max(result.r, result.g), result.b);
	if (transmittance > 0.9999 && peak < 1e-5)
		discard;

	float opacity = 1.0 - transmittance;
	o_Color = vec4(min(result.rgb / max(opacity, 1e-5), vec3(60000.0)), opacity);
}
