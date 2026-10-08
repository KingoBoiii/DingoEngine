// The bloom chain's passes, one shader per define, each drawing one level from another. Mirrors
// Dingo::BloomData in PostProcess.cpp.
//   DE_BLOOM_PREFILTER  scene -> level 0: threshold, then the 13-tap downsample with Karis' average
//   DE_BLOOM_DOWNSAMPLE level i - 1 -> level i: the 13-tap downsample
//   DE_BLOOM_UPSAMPLE   level i + 1 -> level i, blended additively: the 3x3 tent

#type vertex
#version 450
#include <DingoEngine/Fullscreen.glsl>

#type fragment
#version 450

layout(location = 0) in vec2 v_TexCoord;

layout(std140, binding = 0) uniform BloomData
{
	vec4 Source;    // xy = one texel of the source level in UV, z = upsample radius in texels
	vec4 Threshold; // x = threshold, y = knee
};

layout(binding = 1) uniform texture2D u_Source;
layout(binding = 2) uniform sampler u_SourceSampler;

layout(location = 0) out vec4 o_Color;

vec3 Fetch(vec2 offset)
{
	return texture(sampler2D(u_Source, u_SourceSampler), v_TexCoord + offset * Source.xy).rgb;
}

float Brightness(vec3 color)
{
	return max(color.r, max(color.g, color.b));
}

// Nothing at or below the threshold; past it, a quadratic over the knee that joins the line
// (excess - knee / 2) with slope 1, so the glow never switches on.
vec3 ApplyThreshold(vec3 color)
{
	float brightness = Brightness(color);
	float excess = max(brightness - Threshold.x, 0.0);
	float knee = max(Threshold.y, 1e-4);
	float kept = excess < knee ? excess * excess / (2.0 * knee) : excess - 0.5 * knee;
	return color * (kept / max(brightness, 1e-4));
}

// Karis' average: a group of four weighs less the brighter it is, so one firefly pixel can't flood
// the level with its glow.
vec3 KarisGroup(vec3 a, vec3 b, vec3 c, vec3 d, float weight, inout float total)
{
	vec3 average = (a + b + c + d) * 0.25;
	float w = weight / (1.0 + Brightness(average));
	total += w;
	return average * w;
}

void main()
{
#if defined(DE_BLOOM_UPSAMPLE)
	float r = Source.z;
	vec3 sum = Fetch(vec2(0.0)) * 4.0;
	sum += (Fetch(vec2(0.0, r)) + Fetch(vec2(-r, 0.0)) + Fetch(vec2(r, 0.0)) + Fetch(vec2(0.0, -r))) * 2.0;
	sum += Fetch(vec2(-r, r)) + Fetch(vec2(r, r)) + Fetch(vec2(-r, -r)) + Fetch(vec2(r, -r));
	o_Color = vec4(sum * (1.0 / 16.0), 1.0);
#else
	vec3 a = Fetch(vec2(-2.0, 2.0)), b = Fetch(vec2(0.0, 2.0)), c = Fetch(vec2(2.0, 2.0));
	vec3 d = Fetch(vec2(-2.0, 0.0)), e = Fetch(vec2(0.0, 0.0)), f = Fetch(vec2(2.0, 0.0));
	vec3 g = Fetch(vec2(-2.0, -2.0)), h = Fetch(vec2(0.0, -2.0)), i = Fetch(vec2(2.0, -2.0));
	vec3 j = Fetch(vec2(-1.0, 1.0)), k = Fetch(vec2(1.0, 1.0)), l = Fetch(vec2(-1.0, -1.0)), m = Fetch(vec2(1.0, -1.0));

#if defined(DE_BLOOM_PREFILTER)
	a = ApplyThreshold(a); b = ApplyThreshold(b); c = ApplyThreshold(c);
	d = ApplyThreshold(d); e = ApplyThreshold(e); f = ApplyThreshold(f);
	g = ApplyThreshold(g); h = ApplyThreshold(h); i = ApplyThreshold(i);
	j = ApplyThreshold(j); k = ApplyThreshold(k); l = ApplyThreshold(l); m = ApplyThreshold(m);

	float total = 0.0;
	vec3 sum = KarisGroup(j, k, l, m, 0.5, total);
	sum += KarisGroup(a, b, d, e, 0.125, total);
	sum += KarisGroup(b, c, e, f, 0.125, total);
	sum += KarisGroup(d, e, g, h, 0.125, total);
	sum += KarisGroup(e, f, h, i, 0.125, total);
	o_Color = vec4(sum / max(total, 1e-6), 1.0);
#else
	vec3 sum = e * 0.125 + (a + c + g + i) * 0.03125 + (b + d + f + h) * 0.0625 + (j + k + l + m) * 0.125;
	o_Color = vec4(sum, 1.0);
#endif
#endif
}
