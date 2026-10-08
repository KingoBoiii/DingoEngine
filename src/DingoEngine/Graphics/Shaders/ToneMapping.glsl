// The post chain's tone curves, for any shader that maps HDR colour to the display: include it with
// <DingoEngine/ToneMapping.glsl>. ToneMap's op values follow Dingo::ToneMapOperator.

const int TONEMAP_NONE = 0;
const int TONEMAP_SOFT = 1;
const int TONEMAP_ACES = 2;
const int TONEMAP_NEUTRAL = 3;

// Identity up to the knee, then the max channel rolls off along Reinhard's extended curve, rescaled
// so it leaves the knee with slope 1 and reaches exactly 1 at the white point; the other channels
// scale with it, so a hue never shifts.
vec3 ToneMapSoft(vec3 color, float knee, float white)
{
	float peak = max(color.r, max(color.g, color.b));
	if (peak <= knee)
		return color;

	float headroom = 1.0 - knee;
	float u = (peak - knee) / headroom;
	float w = max(white - knee, 1e-4) / headroom;
	float mapped = knee + headroom * min(u * (1.0 + u / (w * w)) / (1.0 + u), 1.0);
	return color * (mapped / peak);
}

// Narkowicz's fit of the ACES filmic curve.
vec3 ToneMapACES(vec3 color)
{
	return clamp((color * (2.51 * color + 0.03)) / (color * (2.43 * color + 0.59) + 0.14), 0.0, 1.0);
}

// Khronos PBR Neutral.
vec3 ToneMapNeutral(vec3 color)
{
	const float startCompression = 0.8 - 0.04;
	const float desaturation = 0.15;

	float lowest = min(color.r, min(color.g, color.b));
	float offset = lowest < 0.08 ? lowest - 6.25 * lowest * lowest : 0.04;
	color -= offset;

	float peak = max(color.r, max(color.g, color.b));
	if (peak < startCompression)
		return color;

	const float d = 1.0 - startCompression;
	float newPeak = 1.0 - d * d / (peak + d - startCompression);
	color *= newPeak / peak;

	float g = 1.0 - 1.0 / (desaturation * (peak - newPeak) + 1.0);
	return mix(color, vec3(newPeak), g);
}

vec3 ToneMap(vec3 color, int op, float knee, float white)
{
	color = max(color, vec3(0.0));
	if (op == TONEMAP_SOFT)
		return ToneMapSoft(color, knee, white);
	if (op == TONEMAP_ACES)
		return ToneMapACES(color);
	if (op == TONEMAP_NEUTRAL)
		return ToneMapNeutral(color);
	return color;
}
