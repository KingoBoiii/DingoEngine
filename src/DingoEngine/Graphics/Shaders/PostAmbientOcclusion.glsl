// The post chain's ambient occlusion: Scalable Ambient Obscurance (McGuire, Mara and Luebke 2012)
// from the scene depth, in three passes chosen by define. DE_AO_SAMPLE takes 12 taps on a spiral
// around each pixel and writes how open it is (R8); DE_AO_BLUR blurs that along Blur.xy, weighing
// each tap by how close its depth is so an edge stays sharp; DE_AO_APPLY multiplies the result into
// the scene colour, upsampling from the four nearest AO texels weighed by depth as well as distance,
// so contact darkness doesn't bleed across a silhouette. Mirrors AmbientOcclusionData in
// PostProcess.cpp.

#type vertex
#version 450
#include <DingoEngine/Fullscreen.glsl>

#type fragment
#version 450

layout(location = 0) in vec2 v_TexCoord;
layout(location = 0) out vec4 o_Color;

#if defined(DE_AO_SAMPLE) || defined(DE_AO_BLUR) || defined(DE_AO_APPLY)

layout(std140, binding = 0) uniform AmbientOcclusionData
{
	mat4 InverseProjection;
	vec4 Params; // x = radius, y = intensity / radius^6, z = bias, w = power
	vec4 Target; // xy = one texel of the pass's target in UV, z = its pixels per unit at distance 1, w = 1 for an orthographic camera
	vec4 Blur;   // xy = the blur's step in UV, zw = one depth texel in UV
};

vec3 ViewPosition(vec2 uv, float depth)
{
	vec4 position = InverseProjection * vec4(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, depth, 1.0);
	return position.xyz / position.w;
}

#endif

#if defined(DE_AO_SAMPLE)

layout(binding = 1) uniform texture2D u_Depth;
layout(binding = 2) uniform sampler u_DepthSampler;

const int SAMPLES = 12;
const float TURNS = 7.0;
const float TAU = 6.28318531;

float DepthAt(vec2 uv)
{
	return textureLod(sampler2D(u_Depth, u_DepthSampler), uv, 0.0).r;
}

vec3 PositionAt(vec2 uv)
{
	return ViewPosition(uv, DepthAt(uv));
}

void main()
{
	float depth = DepthAt(v_TexCoord);
	if (depth >= 1.0)
	{
		o_Color = vec4(1.0);
		return;
	}
	vec3 position = ViewPosition(v_TexCoord, depth);

	// Of each pair of neighbours, the one nearer in depth, so a silhouette doesn't bend the normal.
	vec2 step = Blur.zw;
	vec3 right = PositionAt(v_TexCoord + vec2(step.x, 0.0)) - position;
	vec3 left = position - PositionAt(v_TexCoord - vec2(step.x, 0.0));
	vec3 down = PositionAt(v_TexCoord + vec2(0.0, step.y)) - position;
	vec3 up = position - PositionAt(v_TexCoord - vec2(0.0, step.y));
	vec3 dx = abs(right.z) < abs(left.z) ? right : left;
	vec3 dy = abs(down.z) < abs(up.z) ? down : up;
	vec3 normal = normalize(cross(dx, dy));
	vec3 toCamera = Target.w > 0.5 ? vec3(0.0, 0.0, 1.0) : -position;
	if (dot(normal, toCamera) < 0.0)
		normal = -normal;

	float radius = Params.x;
	float radiusPixels = radius * Target.z / (Target.w > 0.5 ? 1.0 : max(-position.z, 1e-4));
	if (radiusPixels < 1.0)
	{
		o_Color = vec4(1.0);
		return;
	}

	ivec2 pixel = ivec2(gl_FragCoord.xy);
	float spin = float((3 * pixel.x ^ pixel.y + pixel.x * pixel.y) * 10);

	float sum = 0.0;
	for (int i = 0; i < SAMPLES; ++i)
	{
		float along = (float(i) + 0.5) / float(SAMPLES);
		float angle = along * TURNS * TAU + spin;
		vec2 uv = v_TexCoord + vec2(cos(angle), sin(angle)) * (along * radiusPixels) * Target.xy;
		if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0)
			continue;

		vec3 toSample = PositionAt(uv) - position;
		float distanceSquared = dot(toSample, toSample);
		float falloff = max(radius * radius - distanceSquared, 0.0);
		sum += falloff * falloff * falloff * max((dot(toSample, normal) - Params.z) / (0.01 + distanceSquared), 0.0);
	}

	float open = max(0.0, 1.0 - sum * Params.y * (5.0 / float(SAMPLES)));
	o_Color = vec4(pow(open, Params.w), 0.0, 0.0, 1.0);
}

#elif defined(DE_AO_BLUR)

layout(binding = 1) uniform texture2D u_Occlusion;
layout(binding = 2) uniform sampler u_OcclusionSampler;
layout(binding = 3) uniform texture2D u_Depth;
layout(binding = 4) uniform sampler u_DepthSampler;

const float WEIGHTS[5] = float[](0.153170, 0.144893, 0.122649, 0.092902, 0.062970);

float ViewDepth(vec2 uv)
{
	return ViewPosition(uv, textureLod(sampler2D(u_Depth, u_DepthSampler), uv, 0.0).r).z;
}

void main()
{
	float centerDepth = ViewDepth(v_TexCoord);
	float sum = textureLod(sampler2D(u_Occlusion, u_OcclusionSampler), v_TexCoord, 0.0).r * WEIGHTS[0];
	float total = WEIGHTS[0];
	for (int tap = -4; tap <= 4; ++tap)
	{
		if (tap == 0)
			continue;
		vec2 uv = v_TexCoord + Blur.xy * float(tap);
		float depthWeight = max(0.0, 1.0 - 8.0 * abs(ViewDepth(uv) - centerDepth) / max(abs(centerDepth), 1e-4));
		float weight = WEIGHTS[abs(tap)] * depthWeight;
		sum += textureLod(sampler2D(u_Occlusion, u_OcclusionSampler), uv, 0.0).r * weight;
		total += weight;
	}
	o_Color = vec4(sum / total, 0.0, 0.0, 1.0);
}

#elif defined(DE_AO_APPLY)

layout(binding = 1) uniform texture2D u_Occlusion;
layout(binding = 2) uniform sampler u_OcclusionSampler;
layout(binding = 3) uniform texture2D u_Depth; // the scene depth copied to R32F, at full resolution
layout(binding = 4) uniform sampler u_DepthSampler;

float ViewDepth(vec2 uv)
{
	return ViewPosition(uv, textureLod(sampler2D(u_Depth, u_DepthSampler), uv, 0.0).r).z;
}

void main()
{
	vec2 texel = Target.xy;
	vec2 grid = v_TexCoord / texel - 0.5;
	vec2 base = floor(grid);
	vec2 f = grid - base;
	float centerDepth = ViewDepth(v_TexCoord);

	float sum = 0.0;
	float total = 0.0;
	float nearest = 1.0;
	float nearestDifference = 1e30;
	for (int i = 0; i < 4; ++i)
	{
		vec2 corner = vec2(float(i & 1), float(i >> 1));
		vec2 uv = clamp((base + corner + 0.5) * texel, 0.5 * texel, 1.0 - 0.5 * texel);
		float open = textureLod(sampler2D(u_Occlusion, u_OcclusionSampler), uv, 0.0).r;
		float difference = abs(ViewDepth(uv) - centerDepth) / max(abs(centerDepth), 1e-4);
		float bilinear = mix(1.0 - f.x, f.x, corner.x) * mix(1.0 - f.y, f.y, corner.y);
		float weight = bilinear / (1e-3 + difference);
		sum += open * weight;
		total += weight;
		if (difference < nearestDifference)
		{
			nearestDifference = difference;
			nearest = open;
		}
	}
	float open = total > 1e-6 ? sum / total : nearest;
	o_Color = vec4(vec3(open), 1.0);
}

#endif
