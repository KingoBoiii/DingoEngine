// The scene's shadows for a fragment stage, from Renderer3D's shadow atlas: include it with
// <DingoEngine/Shadows.glsl>. Mirrors Renderer3D::ShadowData. The three resources are found by name
// (ShadowData, u_ShadowAtlas, u_ShadowSampler), so a custom shader can move them to bindings its own
// leave free by defining DE_SHADOW_DATA_BINDING (13 at most, D3D11's constant buffers),
// DE_SHADOW_ATLAS_BINDING and DE_SHADOW_SAMPLER_BINDING (15 at most) before including it. The lit
// shader takes 5, 6 and 7.
//
// DirectionalShadow(worldPosition, normal) is 1 where the shadowed directional light reaches the
// point and 0 where something stands between, ShadowStrength of the way; 1 when the scene casts no
// shadow (ShadowCounts.x == 0).

#ifndef DE_SHADOW_DATA_BINDING
#define DE_SHADOW_DATA_BINDING 5
#endif
#ifndef DE_SHADOW_ATLAS_BINDING
#define DE_SHADOW_ATLAS_BINDING 6
#endif
#ifndef DE_SHADOW_SAMPLER_BINDING
#define DE_SHADOW_SAMPLER_BINDING 7
#endif

const int MAX_SHADOW_CASCADES = 4;

struct ShadowCascade
{
	mat4 ViewProjection; // world -> the cascade's clip space: xy in -1..1 across its tile, z = depth 0..1
	vec4 AtlasRect;      // xy = the tile's top-left corner in atlas UV, zw = its size in atlas UV
	vec4 Params;         // x = the depth along the view where the cascade ends, y = one texel in world units
};

layout(std140, binding = DE_SHADOW_DATA_BINDING) uniform ShadowData
{
	ivec4 ShadowCounts;  // x = cascades, y = the directional light that casts them (-1 none), z = tint by cascade
	vec4 ShadowOrigin;   // xyz = where the view's depth is measured from (the camera)
	vec4 ShadowForward;  // xyz = the view direction, w = the blend band at a cascade's end, as a fraction of it
	vec4 ShadowParams;   // x = shadow strength, y = normal offset in texels, z = one atlas texel in UV, w = where shadows end
	ShadowCascade Cascades[MAX_SHADOW_CASCADES];
};

layout(binding = DE_SHADOW_ATLAS_BINDING) uniform texture2D u_ShadowAtlas;
layout(binding = DE_SHADOW_SAMPLER_BINDING) uniform samplerShadow u_ShadowSampler;

// 3 x 3 taps of the hardware's 2 x 2 comparison, 16 texels in all, kept inside the tile so a kernel at
// its edge never reads a neighbour's.
float SampleCascade(int cascade, vec3 worldPosition)
{
	vec4 clip = Cascades[cascade].ViewProjection * vec4(worldPosition, 1.0);
	vec3 ndc = clip.xyz / clip.w;
	vec2 uv = vec2(ndc.x * 0.5 + 0.5, 0.5 - ndc.y * 0.5);
	if (uv.x <= 0.0 || uv.x >= 1.0 || uv.y <= 0.0 || uv.y >= 1.0 || ndc.z >= 1.0)
		return 1.0;

	vec4 rect = Cascades[cascade].AtlasRect;
	float texel = ShadowParams.z;
	vec2 atlasUV = rect.xy + uv * rect.zw;
	vec2 lowest = rect.xy + vec2(1.5 * texel);
	vec2 highest = rect.xy + rect.zw - vec2(1.5 * texel);

	float lit = 0.0;
	for (int y = -1; y <= 1; ++y)
	{
		for (int x = -1; x <= 1; ++x)
		{
			vec2 tap = clamp(atlasUV + vec2(x, y) * texel, lowest, highest);
			lit += textureLod(sampler2DShadow(u_ShadowAtlas, u_ShadowSampler), vec3(tap, ndc.z), 0.0);
		}
	}
	return lit / 9.0;
}

float ShadowDepth(vec3 worldPosition)
{
	return dot(worldPosition - ShadowOrigin.xyz, ShadowForward.xyz);
}

int ShadowCascadeIndex(float depth)
{
	for (int i = 0; i < ShadowCounts.x; ++i)
	{
		if (depth <= Cascades[i].Params.x)
			return i;
	}
	return -1;
}

float DirectionalShadow(vec3 worldPosition, vec3 normal)
{
	if (ShadowCounts.x <= 0)
		return 1.0;

	float depth = ShadowDepth(worldPosition);
	int cascade = ShadowCascadeIndex(depth);
	if (cascade < 0)
		return 1.0;

	// Pushed off the surface by a few of the cascade's texels, which removes acne where slope bias
	// alone would need too much.
	vec3 offsetPosition = worldPosition + normal * (ShadowParams.y * Cascades[cascade].Params.y);
	float shadow = SampleCascade(cascade, offsetPosition);

	// Across the last part of a cascade, fade into the next one so the change of resolution has no seam.
	float end = Cascades[cascade].Params.x;
	float start = cascade > 0 ? Cascades[cascade - 1].Params.x : 0.0;
	float band = (end - start) * ShadowForward.w;
	if (band > 0.0 && depth > end - band)
	{
		float t = (depth - (end - band)) / band;
		float next = 1.0;
		if (cascade + 1 < ShadowCounts.x)
			next = SampleCascade(cascade + 1, worldPosition + normal * (ShadowParams.y * Cascades[cascade + 1].Params.y));
		shadow = mix(shadow, next, t);
	}

	return mix(1.0, shadow, ShadowParams.x);
}

// Red, green, blue and yellow by cascade, for the debug view.
vec3 ShadowCascadeTint(vec3 worldPosition)
{
	int cascade = ShadowCascadeIndex(ShadowDepth(worldPosition));
	if (cascade == 0) return vec3(1.0, 0.6, 0.6);
	if (cascade == 1) return vec3(0.6, 1.0, 0.6);
	if (cascade == 2) return vec3(0.6, 0.6, 1.0);
	if (cascade == 3) return vec3(1.0, 1.0, 0.6);
	return vec3(1.0);
}
