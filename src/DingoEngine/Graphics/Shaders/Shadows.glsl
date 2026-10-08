// The scene's shadows for a fragment stage, from Renderer3D's shadow atlas: include it with
// <DingoEngine/Shadows.glsl>. Mirrors Renderer3D::ShadowData. The three resources are found by name
// (ShadowData, u_ShadowAtlas, u_ShadowSampler), so a custom shader can move them to bindings its own
// leave free by defining DE_SHADOW_DATA_BINDING (13 at most, D3D11's constant buffers),
// DE_SHADOW_ATLAS_BINDING and DE_SHADOW_SAMPLER_BINDING (15 at most) before including it. The lit
// shader takes 5, 6 and 7.
//
// DirectionalShadow(worldPosition, normal) is 1 where the shadowed directional light reaches the
// point and 0 where something stands between, ShadowStrength of the way; 1 when the scene casts no
// shadow (ShadowCounts.x == 0). LocalLightShadow(light, worldPosition, normal) is the same for point
// or spot light `light`, in the order CameraData lists them; 1 for a light without a shadow.

#ifndef DE_SHADOW_DATA_BINDING
#define DE_SHADOW_DATA_BINDING 5
#endif
#ifndef DE_SHADOW_ATLAS_BINDING
#define DE_SHADOW_ATLAS_BINDING 6
#endif
#ifndef DE_SHADOW_SAMPLER_BINDING
#define DE_SHADOW_SAMPLER_BINDING 7
#endif

const int MAX_SHADOW_TILES = 100;
const int MAX_SHADOW_LOCAL_LIGHTS = 32;

struct ShadowTile
{
	mat4 ViewProjection; // world -> the tile's clip space: xy in -1..1 across it, z = depth 0..1
	vec4 AtlasRect;      // xy = the tile's top-left corner in atlas UV, zw = its size in atlas UV
	vec4 Params;         // x = a cascade's end depth along the view; y = one texel in world units, per unit of distance from the light when z = 1
};

struct LocalShadow
{
	vec4 Record;   // x = the first tile (-1 none), y = 1 (a spot light's view) or 6 (cube faces +x -x +y -y +z -z), z = strength
	vec4 Position; // xyz = the light's position
};

layout(std140, binding = DE_SHADOW_DATA_BINDING) uniform ShadowData
{
	ivec4 ShadowCounts;  // x = cascades, y = the directional light that casts them (-1 none), z = tint by cascade, w = shadowed local lights
	vec4 ShadowOrigin;   // xyz = where the view's depth is measured from (the camera)
	vec4 ShadowForward;  // xyz = the view direction, w = the blend band at a cascade's end, as a fraction of it
	vec4 ShadowParams;   // x = shadow strength, y = normal offset in texels, z = one atlas texel in UV, w = where shadows end
	ShadowTile Tiles[MAX_SHADOW_TILES]; // the cascades first
	LocalShadow LocalShadows[MAX_SHADOW_LOCAL_LIGHTS];
};

layout(binding = DE_SHADOW_ATLAS_BINDING) uniform texture2D u_ShadowAtlas;
layout(binding = DE_SHADOW_SAMPLER_BINDING) uniform samplerShadow u_ShadowSampler;

// 3 x 3 taps of the hardware's 2 x 2 comparison, 16 texels in all, kept inside the tile so a kernel at
// its edge never reads a neighbour's.
float SampleShadowTile(int tile, vec3 worldPosition)
{
	vec4 clip = Tiles[tile].ViewProjection * vec4(worldPosition, 1.0);
	if (clip.w <= 0.0)
		return 1.0;
	vec3 ndc = clip.xyz / clip.w;
	vec2 uv = vec2(ndc.x * 0.5 + 0.5, 0.5 - ndc.y * 0.5);
	if (uv.x <= 0.0 || uv.x >= 1.0 || uv.y <= 0.0 || uv.y >= 1.0 || ndc.z >= 1.0)
		return 1.0;

	vec4 rect = Tiles[tile].AtlasRect;
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
		if (depth <= Tiles[i].Params.x)
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
	vec3 offsetPosition = worldPosition + normal * (ShadowParams.y * Tiles[cascade].Params.y);
	float shadow = SampleShadowTile(cascade, offsetPosition);

	// Across the last part of a cascade, fade into the next one so the change of resolution has no seam.
	float end = Tiles[cascade].Params.x;
	float start = cascade > 0 ? Tiles[cascade - 1].Params.x : 0.0;
	float band = (end - start) * ShadowForward.w;
	if (band > 0.0 && depth > end - band)
	{
		float t = (depth - (end - band)) / band;
		float next = 1.0;
		if (cascade + 1 < ShadowCounts.x)
			next = SampleShadowTile(cascade + 1, worldPosition + normal * (ShadowParams.y * Tiles[cascade + 1].Params.y));
		shadow = mix(shadow, next, t);
	}

	return mix(1.0, shadow, ShadowParams.x);
}

float LocalLightShadow(int light, vec3 worldPosition, vec3 normal)
{
	if (ShadowCounts.w <= 0)
		return 1.0;
	vec4 record = LocalShadows[light].Record;
	if (record.x < 0.0)
		return 1.0;

	vec3 fromLight = worldPosition - LocalShadows[light].Position.xyz;
	int tile = int(record.x);
	if (record.y > 1.5)
	{
		// The cube face the point lies in front of: the axis it is farthest along.
		vec3 a = abs(fromLight);
		if (a.x >= a.y && a.x >= a.z)
			tile += fromLight.x > 0.0 ? 0 : 1;
		else if (a.y >= a.z)
			tile += fromLight.y > 0.0 ? 2 : 3;
		else
			tile += fromLight.z > 0.0 ? 4 : 5;
	}

	// A perspective texel grows with the distance from the light.
	float texel = Tiles[tile].Params.y * length(fromLight);
	float shadow = SampleShadowTile(tile, worldPosition + normal * (ShadowParams.y * texel));
	return mix(1.0, shadow, record.z);
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
