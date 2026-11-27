// The PBR shaders' BRDF terms and sRGB conversions, for any shader that lights with a metallic-roughness
// material: include it with <DingoEngine/Brdf.glsl>. Written in the GLSL subset that also compiles as C++
// over GLM (float literals carry the f suffix for that), so BrdfTests.cpp
// checks this text. `a` is the roughness squared throughout.

#ifndef DE_PI
#define DE_PI 3.14159265358979f
#endif

// Trowbridge-Reitz normal distribution; integrates to 1 over the hemisphere projected by NoH.
float D_GGX(float NoH, float a)
{
	float a2 = a * a;
	float d = NoH * NoH * (a2 - 1.0f) + 1.0f;
	return a2 / (DE_PI * max(d * d, 1e-12f));
}

// Height-correlated Smith visibility, already divided by 4 NoL NoV.
float V_SmithGGXCorrelated(float NoV, float NoL, float a)
{
	float a2 = a * a;
	float ggxV = NoL * sqrt(NoV * NoV * (1.0f - a2) + a2);
	float ggxL = NoV * sqrt(NoL * NoL * (1.0f - a2) + a2);
	return 0.5f / max(ggxV + ggxL, 1e-5f);
}

vec3 F_Schlick(vec3 F0, float VoH)
{
	float f = pow(clamp(1.0f - VoH, 0.0f, 1.0f), 5.0f);
	return F0 + (vec3(1.0f) - F0) * f;
}

vec3 SrgbToLinear(vec3 c)
{
	c = max(c, vec3(0.0f));
	vec3 low = c / 12.92f;
	vec3 high = pow((c + vec3(0.055f)) / 1.055f, vec3(2.4f));
	return mix(high, low, step(c, vec3(0.04045f)));
}

vec3 LinearToSrgb(vec3 c)
{
	c = max(c, vec3(0.0f));
	vec3 low = c * 12.92f;
	vec3 high = 1.055f * pow(c, vec3(1.0f / 2.4f)) - vec3(0.055f);
	return mix(high, low, step(c, vec3(0.0031308f)));
}

// Karis' mobile fit of the split-sum environment BRDF. x is the scale and y the bias of F0:
// the specular albedo is F0 * x + y.
vec2 EnvBRDFApprox(float roughness, float NoV)
{
	const vec4 c0 = vec4(-1.0f, -0.0275f, -0.572f, 0.022f);
	const vec4 c1 = vec4(1.0f, 0.0425f, 1.04f, -0.04f);
	vec4 r = roughness * c0 + c1;
	float a004 = min(r.x * r.x, exp2(-9.28f * NoV)) * r.x + r.y;
	vec2 ab = vec2(-1.04f, 1.04f) * a004 + vec2(r.z, r.w);
	return clamp(ab, vec2(0.0f), vec2(1.0f));
}

vec3 EnvBRDFApprox(vec3 F0, float roughness, float NoV)
{
	vec2 ab = EnvBRDFApprox(roughness, NoV);
	return F0 * ab.x + vec3(ab.y);
}
