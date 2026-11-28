// Image-based lighting terms: the environment bake's spherical harmonics, GGX prefilter and BRDF LUT
// integrator, and the lighting pass's specular occlusion. Include <DingoEngine/Brdf.glsl> first (it has no
// include guard, so this file does not pull it in); like Brdf.glsl it stays in the GLSL subset that also
// compiles as C++ over GLM, so IblTests.cpp checks this text. `a` is the roughness squared throughout.

#ifndef DE_IBL_GLSL
#define DE_IBL_GLSL

#ifndef DE_PI
#define DE_PI 3.14159265358979f
#endif

const int k_PrefilterMips = 6;

// Real SH basis of order 2 for a unit direction, ordered l0; l1 m = -1, 0, 1 (y, z, x); l2 m = -2..2.
float DE_ShBasis(int i, vec3 d)
{
	if (i == 0)
		return 0.282095f;
	if (i == 1)
		return 0.488603f * d.y;
	if (i == 2)
		return 0.488603f * d.z;
	if (i == 3)
		return 0.488603f * d.x;
	if (i == 4)
		return 1.092548f * d.x * d.y;
	if (i == 5)
		return 1.092548f * d.y * d.z;
	if (i == 6)
		return 0.315392f * (3.0f * d.z * d.z - 1.0f);
	if (i == 7)
		return 1.092548f * d.x * d.z;
	return 0.546274f * (d.x * d.x - d.y * d.y);
}

// Cosine-lobe convolution of band l divided by pi: scales a radiance coefficient into irradiance / pi.
float DE_ShLambertOverPi(int l)
{
	if (l == 0)
		return 1.0f;
	if (l == 1)
		return 2.0f / 3.0f;
	return 0.25f;
}

// Exact solid angle of the texel of a size x size cube face centred on uv in [0, 1]^2.
float DE_CubeTexelSolidAngle(vec2 uv, float size)
{
	vec2 c = uv * 2.0f - vec2(1.0f);
	float h = 1.0f / size;
	vec2 lo = c - vec2(h);
	vec2 hi = c + vec2(h);
	float a0 = atan(lo.x * lo.y / sqrt(lo.x * lo.x + lo.y * lo.y + 1.0f));
	float a1 = atan(lo.x * hi.y / sqrt(lo.x * lo.x + hi.y * hi.y + 1.0f));
	float a2 = atan(hi.x * lo.y / sqrt(hi.x * hi.x + lo.y * lo.y + 1.0f));
	float a3 = atan(hi.x * hi.y / sqrt(hi.x * hi.x + hi.y * hi.y + 1.0f));
	return a0 - a1 - a2 + a3;
}

vec2 DE_Hammersley(uint i, uint n)
{
	return vec2(float(i) / float(n), float(bitfieldReverse(i)) * 2.3283064365386963e-10f);
}

// GGX-distributed half vector in tangent space (N = +Z); the caller rotates it into its own frame.
vec3 DE_ImportanceSampleGgx(vec2 xi, float a)
{
	float phi = 2.0f * DE_PI * xi.x;
	float cosTheta = sqrt((1.0f - xi.y) / (1.0f + (a * a - 1.0f) * xi.y));
	float sinTheta = sqrt(max(1.0f - cosTheta * cosTheta, 0.0f));
	return vec3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);
}

// Mip of a W-face cube whose texel solid angle matches the K-sample density of the GGX lobe (N = V, so the
// half-vector pdf D * NoH / (4 VoH) reduces to D / 4).
float DE_PrefilterSourceLod(float NoH, float a, float W, float K)
{
	float pdf = max(D_GGX(NoH, a) * 0.25f, 1e-6f);
	float omegaS = 1.0f / (K * pdf);
	float omegaP = 4.0f * DE_PI / (6.0f * W * W);
	return max(0.5f * log2(omegaS / omegaP), 0.0f);
}

float DE_PrefilterLod(float roughness)
{
	return roughness * float(k_PrefilterMips - 1);
}

float DE_PrefilterRoughness(float mip)
{
	return mip / float(k_PrefilterMips - 1);
}

// Split-sum environment BRDF: x is the scale and y the bias of F0, so the specular albedo is F0 * x + y.
vec2 DE_IntegrateBrdf(float NoV, float roughness, float K)
{
	NoV = clamp(NoV, 1e-3f, 1.0f);
	float a = roughness * roughness;
	vec3 V = vec3(sqrt(1.0f - NoV * NoV), 0.0f, NoV);
	vec2 sum = vec2(0.0f);
	for (int i = 0; i < int(K); ++i)
	{
		vec3 H = DE_ImportanceSampleGgx(DE_Hammersley(uint(i), uint(K)), a);
		float VoH = dot(V, H);
		vec3 L = H * (2.0f * VoH) - V;
		float NoL = L.z;
		float NoH = H.z;
		VoH = clamp(VoH, 0.0f, 1.0f);
		if (NoL > 0.0f && NoH > 0.0f)
		{
			float weight = 4.0f * V_SmithGGXCorrelated(NoV, NoL, a) * NoL * VoH / NoH;
			float Fc = pow(1.0f - VoH, 5.0f);
			sum += vec2((1.0f - Fc) * weight, Fc * weight);
		}
	}
	return sum / K;
}

const int k_BrdfLutSize = 128;

vec2 DE_BrdfLutUv(float NoV, float roughness)
{
	return vec2(NoV, roughness) * (127.0f / 128.0f) + vec2(0.5f / 128.0f);
}

// The (NoV, roughness) a LUT texel centre at uv stores: inverse of DE_BrdfLutUv.
vec2 DE_BrdfLutValue(vec2 uv)
{
	return (uv - vec2(0.5f / 128.0f)) * (128.0f / 127.0f);
}

// Lagarde and de Rousiers' specular occlusion from the material's ambient occlusion.
float DE_SpecularOcclusion(float NoV, float ao, float a)
{
	return clamp(pow(NoV + ao, exp2(-16.0f * a - 1.0f)) - 1.0f + ao, 0.0f, 1.0f);
}

#endif
