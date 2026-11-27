// The froxel fog's mapping and scattering terms, for the volumetric kernels and any shader that reads the
// volume: include it with <DingoEngine/VolumetricMath.glsl>. Written in the GLSL subset that also compiles as
// C++ over GLM (float literals carry the f suffix for that), so VolumetricTests.cpp checks this text.

#ifndef DE_PI
#define DE_PI 3.14159265358979f
#endif

// View depth of slice boundary b of `slices`: Near at 0, Far at `slices`, exponential in between.
float DE_VolumeSliceDepth(float b, float slices, float nearDistance, float farDistance)
{
	return nearDistance * pow(farDistance / nearDistance, b / slices);
}

// The continuous slice index of a view depth; the inverse of DE_VolumeSliceDepth.
float DE_VolumeDepthToSlice(float viewDepth, float slices, float nearDistance, float farDistance)
{
	return slices * log2(viewDepth / nearDistance) / log2(farDistance / nearDistance);
}

// The texture coordinate along the volume's depth axis for a view depth, centred on slice texels and kept
// inside the first and last texel centres.
float DE_VolumeDepthToCoord(float viewDepth, float slices, float nearDistance, float farDistance)
{
	float s = DE_VolumeDepthToSlice(viewDepth, slices, nearDistance, farDistance);
	return clamp((s - 0.5f) / slices, 0.5f / slices, 1.0f - 0.5f / slices);
}

// Henyey-Greenstein phase function, normalised over the sphere. cosTheta is between the light's travel
// direction reversed and the ray's direction; g > 0 scatters forwards.
float DE_HenyeyGreenstein(float cosTheta, float g)
{
	float g2 = g * g;
	float denom = 1.0f + g2 - 2.0f * g * cosTheta;
	return (1.0f - g2) / (4.0f * DE_PI * denom * sqrt(max(denom, 1e-6f)));
}

// Extinction of the height fog at a world height.
float DE_HeightFogDensity(float density, float heightFalloff, float baseHeight, float worldY)
{
	return density * exp(-heightFalloff * max(worldY - baseHeight, 0.0f));
}

// exp(-sigmaT * len): the fraction of light that crosses one slice of length `len`.
float DE_SliceTransmittance(float sigmaT, float len)
{
	return exp(-sigmaT * len);
}

// The integral of exp(-sigmaT * t) over one slice, which scales a constant in-scatter term S into the slice's
// contribution; `transmittance` is DE_SliceTransmittance for the same slice.
float DE_SliceInScatterFactor(float sigmaT, float len, float transmittance)
{
	return sigmaT > 1e-6f ? (1.0f - transmittance) / sigmaT : len;
}

// Halton sequence, base 2: the radical inverse of the index, in [0, 1).
float DE_HaltonBase2(uint index)
{
	return float(bitfieldReverse(index)) * 2.3283064365386963e-10f;
}

// Weight of a camera distance t between two fades: keepBefore up to `start`, keepAfter from `end`, linear in
// between. An `end` not past `start` is no fade and returns 1.
float DE_DistanceFade(float t, float start, float end, float keepBefore, float keepAfter)
{
	if (end <= start)
		return 1.0f;
	float s = clamp((t - start) / (end - start), 0.0f, 1.0f);
	return keepBefore + (keepAfter - keepBefore) * s;
}

// A fog volume's density multiplier at height h above its bottom. fade = (start, end, band bottom, band top)
// and band = (multiplier, softness, unused, unused): 1 up to `start`, falling linearly to 0 at `end` (off when
// `end` is not past `start`), times `multiplier` inside the band, which eases in and out over `softness` at
// each edge (off when the top is not past the bottom).
float DE_HeightProfile(float h, vec4 fade, vec4 band)
{
	float profile = 1.0f;
	if (fade.y > fade.x)
		profile = 1.0f - clamp((h - fade.x) / (fade.y - fade.x), 0.0f, 1.0f);
	if (band.x != 1.0f && fade.w > fade.z)
	{
		float soft = max(band.y, 1e-4f);
		float rise = clamp((h - fade.z) / soft, 0.0f, 1.0f);
		float fall = clamp((fade.w - h) / soft, 0.0f, 1.0f);
		float edge = min(rise * rise * (3.0f - 2.0f * rise), fall * fall * (3.0f - 2.0f * fall));
		profile *= 1.0f + (band.x - 1.0f) * edge;
	}
	return profile;
}

float DE_EdgeFadeFromDistance(float distanceToSurface, float edgeFade)
{
	if (edgeFade <= 0.0f)
		return distanceToSurface > 0.0f ? 1.0f : 0.0f;
	float s = clamp(distanceToSurface / edgeFade, 0.0f, 1.0f);
	return s * s * (3.0f - 2.0f * s);
}

// Weight of a point in a box volume's local space (the box is [-1, 1]^3) that fades to 0 over `edgeFade` world
// units toward each face; halfExtents are the box's world half sizes. 0 outside; without an edge fade, 1 inside.
float DE_BoxEdgeFade(vec3 local, vec3 halfExtents, float edgeFade)
{
	float d = min(min((1.0f - abs(local.x)) * halfExtents.x, (1.0f - abs(local.y)) * halfExtents.y), (1.0f - abs(local.z)) * halfExtents.z);
	return DE_EdgeFadeFromDistance(d, edgeFade);
}

// The same for an ellipsoid volume (the unit ball in local space); the distance to the surface is taken along
// the radius and scaled by the shortest half size, which never overstates it.
float DE_EllipsoidEdgeFade(vec3 local, vec3 halfExtents, float edgeFade)
{
	float d = (1.0f - length(local)) * min(min(halfExtents.x, halfExtents.y), halfExtents.z);
	return DE_EdgeFadeFromDistance(d, edgeFade);
}

// The ray origin + t * dir through the box [-1, 1]^3: (t entry, t exit) of the whole line, entry negative when
// the origin is inside. An empty intersection returns (1, 0).
vec2 DE_RayBoxInterval(vec3 origin, vec3 dir)
{
	float tNear = -1e30f;
	float tFar = 1e30f;
	for (int i = 0; i < 3; ++i)
	{
		if (abs(dir[i]) < 1e-12f)
		{
			if (abs(origin[i]) > 1.0f)
				return vec2(1.0f, 0.0f);
		}
		else
		{
			float inv = 1.0f / dir[i];
			float t0 = (-1.0f - origin[i]) * inv;
			float t1 = (1.0f - origin[i]) * inv;
			tNear = max(tNear, min(t0, t1));
			tFar = min(tFar, max(t0, t1));
		}
	}
	return tNear <= tFar ? vec2(tNear, tFar) : vec2(1.0f, 0.0f);
}

// The same through the unit ball; dir need not be unit length. An empty intersection returns (1, 0).
vec2 DE_RayUnitSphereInterval(vec3 origin, vec3 dir)
{
	float a = dot(dir, dir);
	float b = dot(origin, dir);
	float c = dot(origin, origin) - 1.0f;
	float disc = b * b - a * c;
	if (a < 1e-20f || disc < 0.0f)
		return vec2(1.0f, 0.0f);
	float root = sqrt(disc);
	return vec2((-b - root) / a, (-b + root) / a);
}

// Interleaved gradient noise of a pixel position, in [0, 1): a fixed per-pixel pattern that is frame stable.
float DE_InterleavedGradientNoise(vec2 pixel)
{
	return fract(52.9829189f * fract(dot(pixel, vec2(0.06711056f, 0.00583715f))));
}

// Scales an in-scatter colour so its strongest channel is at most `cap`, keeping its hue; a `cap` of 0 or less
// leaves it as it is.
vec3 DE_CapInScatter(vec3 inScatter, float cap)
{
	float peak = max(max(inScatter.x, inScatter.y), inScatter.z);
	return (cap > 0.0f && peak > cap) ? inScatter * (cap / peak) : inScatter;
}
