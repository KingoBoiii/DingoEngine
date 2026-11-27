#include <doctest/doctest.h>

#include <glm/glm.hpp>
#include <glm/gtc/bitfield.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

// The engine's own Ibl.glsl (and the Brdf.glsl it builds on), compiled as C++ over GLM: the terms tested are
// the shader's text, not a copy.
namespace GlslIbl
{
	using glm::vec2;
	using glm::vec3;
	using glm::vec4;
	using glm::atan;
	using glm::bitfieldReverse;
	using glm::clamp;
	using glm::cos;
	using glm::dot;
	using glm::exp2;
	using glm::log2;
	using glm::max;
	using glm::min;
	using glm::mix;
	using glm::pow;
	using glm::sin;
	using glm::sqrt;
	using glm::step;
	using uint = std::uint32_t;

#include "DingoEngine/Graphics/Shaders/Brdf.glsl"
#include "DingoEngine/Graphics/Shaders/Ibl.glsl"
}

namespace
{
	constexpr double k_Pi = 3.14159265358979323846;

	// The cube layout of CubeMap.glsl, without its normalise: faces +X, -X, +Y, -Y, +Z, -Z, row 0 on top.
	glm::dvec3 FaceDirection(int face, double u, double v)
	{
		const double a = u * 2.0 - 1.0;
		const double b = v * 2.0 - 1.0;
		glm::dvec3 d;
		switch (face)
		{
			case 0: d = { 1.0, -b, -a }; break;
			case 1: d = { -1.0, -b, a }; break;
			case 2: d = { a, 1.0, b }; break;
			case 3: d = { a, -1.0, -b }; break;
			case 4: d = { a, -b, 1.0 }; break;
			default: d = { -a, -b, -1.0 }; break;
		}
		return glm::normalize(d);
	}

	template<typename F>
	void ForEachCubeTexel(int size, F&& f)
	{
		for (int face = 0; face < 6; ++face)
			for (int y = 0; y < size; ++y)
				for (int x = 0; x < size; ++x)
				{
					const double u = (x + 0.5) / size;
					const double v = (y + 0.5) / size;
					const double dOmega = GlslIbl::DE_CubeTexelSolidAngle(glm::vec2(static_cast<float>(u), static_cast<float>(v)), static_cast<float>(size));
					f(FaceDirection(face, u, v), dOmega);
				}
	}

	// The bake's SH projection of a radiance function, as the compute kernel does it.
	template<typename L>
	void ProjectCube(int size, L&& radiance, double outC[9])
	{
		double sumOmega = 0.0;
		double c[9] = {};
		ForEachCubeTexel(size, [&](const glm::dvec3& d, double dOmega)
		{
			sumOmega += dOmega;
			const double r = radiance(d);
			const glm::vec3 df(static_cast<float>(d.x), static_cast<float>(d.y), static_cast<float>(d.z));
			for (int i = 0; i < 9; ++i)
				c[i] += r * dOmega * GlslIbl::DE_ShBasis(i, df);
		});
		const double scale = 4.0 * k_Pi / sumOmega;
		const int bandOf[9] = { 0, 1, 1, 1, 2, 2, 2, 2, 2 };
		for (int i = 0; i < 9; ++i)
			outC[i] = c[i] * scale * GlslIbl::DE_ShLambertOverPi(bandOf[i]);
	}

	double EvalIrradianceOverPi(const double c[9], const glm::dvec3& n)
	{
		const glm::vec3 nf(static_cast<float>(n.x), static_cast<float>(n.y), static_cast<float>(n.z));
		double sum = 0.0;
		for (int i = 0; i < 9; ++i)
			sum += c[i] * GlslIbl::DE_ShBasis(i, nf);
		return sum;
	}
}

TEST_CASE("Ibl: the SH basis is orthonormal over the sphere")
{
	constexpr int thetaSteps = 400;
	constexpr int phiSteps = 800;
	double gram[9][9] = {};
	for (int i = 0; i < thetaSteps; ++i)
	{
		const double theta = k_Pi * (i + 0.5) / thetaSteps;
		const double w = std::sin(theta) * (k_Pi / thetaSteps) * (2.0 * k_Pi / phiSteps);
		for (int j = 0; j < phiSteps; ++j)
		{
			const double phi = 2.0 * k_Pi * (j + 0.5) / phiSteps;
			const glm::vec3 d(static_cast<float>(std::sin(theta) * std::cos(phi)), static_cast<float>(std::cos(theta)), static_cast<float>(std::sin(theta) * std::sin(phi)));
			float y[9];
			for (int k = 0; k < 9; ++k)
				y[k] = GlslIbl::DE_ShBasis(k, d);
			for (int a = 0; a < 9; ++a)
				for (int b = 0; b < 9; ++b)
					gram[a][b] += static_cast<double>(y[a]) * y[b] * w;
		}
	}
	for (int a = 0; a < 9; ++a)
		for (int b = 0; b < 9; ++b)
			CHECK(std::abs(gram[a][b] - (a == b ? 1.0 : 0.0)) <= 1e-3);
}

TEST_CASE("Ibl: cube texel solid angles sum to 4 pi")
{
	for (int size = 1; size <= 64; ++size)
	{
		double sum = 0.0;
		ForEachCubeTexel(size, [&](const glm::dvec3&, double dOmega) { sum += dOmega; });
		CHECK(sum == doctest::Approx(4.0 * k_Pi).epsilon(1e-4));
	}
}

TEST_CASE("Ibl: a uniform cube projects to irradiance / pi of one at every normal")
{
	double c[9];
	ProjectCube(16, [](const glm::dvec3&) { return 1.0; }, c);
	const glm::dvec3 normals[] = { { 0, 1, 0 }, { 0, -1, 0 }, { 1, 0, 0 }, { 0, 0, -1 }, glm::normalize(glm::dvec3(1, 2, -3)) };
	for (const glm::dvec3& n : normals)
		CHECK(EvalIrradianceOverPi(c, n) == doctest::Approx(1.0).epsilon(2e-3));
}

TEST_CASE("Ibl: L = a + b y + c (3 y^2 - 1) projects to its closed-form irradiance / pi")
{
	const double a = 1.5;
	const double b = 0.8;
	const double cc = 0.3;
	double c[9];
	ProjectCube(24, [&](const glm::dvec3& d) { return a + b * d.y + cc * (3.0 * d.y * d.y - 1.0); }, c);
	const glm::dvec3 normals[] = { { 0, 1, 0 }, { 0, -1, 0 }, { 1, 0, 0 }, glm::normalize(glm::dvec3(1, 2, -3)), glm::normalize(glm::dvec3(-2, 1, 1)) };
	for (const glm::dvec3& n : normals)
	{
		const double expected = a + (2.0 / 3.0) * b * n.y + 0.25 * cc * (3.0 * n.y * n.y - 1.0);
		CHECK(EvalIrradianceOverPi(c, n) == doctest::Approx(expected).epsilon(5e-3));
	}
}

TEST_CASE("Ibl: Hammersley points for small n")
{
	const glm::vec2 p0 = GlslIbl::DE_Hammersley(0, 4);
	const glm::vec2 p1 = GlslIbl::DE_Hammersley(1, 4);
	const glm::vec2 p2 = GlslIbl::DE_Hammersley(2, 4);
	const glm::vec2 p3 = GlslIbl::DE_Hammersley(3, 4);
	CHECK(p0.x == doctest::Approx(0.0f));
	CHECK(p0.y == doctest::Approx(0.0f));
	CHECK(p1.x == doctest::Approx(0.25f));
	CHECK(p1.y == doctest::Approx(0.5f));
	CHECK(p2.x == doctest::Approx(0.5f));
	CHECK(p2.y == doctest::Approx(0.25f));
	CHECK(p3.x == doctest::Approx(0.75f));
	CHECK(p3.y == doctest::Approx(0.75f));
}

TEST_CASE("Ibl: GGX importance samples follow D * NoH and the pdf integrates to one")
{
	for (float roughness : { 0.25f, 0.5f, 1.0f })
	{
		const float a = roughness * roughness;
		constexpr int steps = 4000;
		double total = 0.0;
		for (int i = 0; i < steps; ++i)
		{
			const double theta = 0.5 * k_Pi * (i + 0.5) / steps;
			const double cosTheta = std::cos(theta);
			total += static_cast<double>(GlslIbl::D_GGX(static_cast<float>(cosTheta), a)) * cosTheta * std::sin(theta) * (0.5 * k_Pi / steps) * 2.0 * k_Pi;
		}
		CHECK(total == doctest::Approx(1.0).epsilon(0.01));

		constexpr int n = 4096;
		double meanCos = 0.0;
		for (int i = 0; i < n; ++i)
		{
			const glm::vec3 h = GlslIbl::DE_ImportanceSampleGgx(GlslIbl::DE_Hammersley(static_cast<GlslIbl::uint>(i), n), a);
			CHECK(glm::length(h) == doctest::Approx(1.0f).epsilon(1e-4));
			meanCos += h.z;
		}
		meanCos /= n;
		double expected = 0.0;
		for (int i = 0; i < 20000; ++i)
		{
			const double theta = 0.5 * k_Pi * (i + 0.5) / 20000;
			const double cosTheta = std::cos(theta);
			expected += cosTheta * static_cast<double>(GlslIbl::D_GGX(static_cast<float>(cosTheta), a)) * cosTheta * std::sin(theta) * (0.5 * k_Pi / 20000) * 2.0 * k_Pi;
		}
		CHECK(meanCos == doctest::Approx(expected).epsilon(0.01));
	}
}

TEST_CASE("Ibl: the prefilter LOD and roughness round trip")
{
	for (int mip = 0; mip < GlslIbl::k_PrefilterMips; ++mip)
	{
		const float r = GlslIbl::DE_PrefilterRoughness(static_cast<float>(mip));
		CHECK(GlslIbl::DE_PrefilterLod(r) == doctest::Approx(static_cast<float>(mip)));
	}
	CHECK(GlslIbl::DE_PrefilterRoughness(0.0f) == doctest::Approx(0.0f));
	CHECK(GlslIbl::DE_PrefilterRoughness(static_cast<float>(GlslIbl::k_PrefilterMips - 1)) == doctest::Approx(1.0f));
}

TEST_CASE("Ibl: the prefilter source LOD grows as the lobe widens and never goes negative")
{
	float previous = -1.0f;
	for (float roughness : { 0.2f, 0.4f, 0.6f, 0.8f, 1.0f })
	{
		const float lod = GlslIbl::DE_PrefilterSourceLod(1.0f, roughness * roughness, 128.0f, 64.0f);
		CHECK(lod >= 0.0f);
		CHECK(lod >= previous);
		previous = lod;
	}
}

TEST_CASE("Ibl: the BRDF LUT stays within energy bounds and falls with roughness")
{
	const float roughness[] = { 0.045f, 0.1f, 0.25f, 0.5f, 0.75f, 1.0f };
	for (float NoV : { 0.1f, 0.3f, 0.6f, 1.0f })
	{
		float previous = 2.0f;
		for (float r : roughness)
		{
			const glm::vec2 ab = GlslIbl::DE_IntegrateBrdf(NoV, r, 256.0f);
			CHECK(ab.x >= 0.0f);
			CHECK(ab.y >= 0.0f);
			CHECK(ab.x + ab.y <= 1.0f + 0.02f);
			CHECK(ab.x + ab.y <= previous + 0.01f);
			previous = ab.x + ab.y;
		}
	}
	const glm::vec2 head = GlslIbl::DE_IntegrateBrdf(1.0f, 0.045f, 256.0f);
	CHECK(head.x + head.y >= 0.98f);
}

TEST_CASE("Ibl: the BRDF LUT stays near the mobile fit (0.09 at F0 0.04; 0.2 at F0 1 from NoV 0.3, where the fit is loosest)")
{
	for (float F0 : { 0.04f, 1.0f })
		for (float NoV : { 0.1f, 0.3f, 0.5f, 0.8f, 1.0f })
			for (float r : { 0.1f, 0.3f, 0.5f, 0.7f, 1.0f })
			{
				const glm::vec2 ab = GlslIbl::DE_IntegrateBrdf(NoV, r, 256.0f);
				const glm::vec2 fit = GlslIbl::EnvBRDFApprox(r, NoV);
				const float lut = F0 * ab.x + ab.y;
				const float approx = F0 * fit.x + fit.y;
				if (F0 > 0.5f && NoV < 0.3f)
					continue;
				CAPTURE(F0); CAPTURE(NoV); CAPTURE(r);
				CHECK(std::abs(lut - approx) <= (F0 > 0.5f ? 0.2f : 0.09f));
			}
}

TEST_CASE("Ibl: the LUT uv maps texel centres back to the 0..1 grid")
{
	for (int i = 0; i < GlslIbl::k_BrdfLutSize; ++i)
	{
		const float value = static_cast<float>(i) / static_cast<float>(GlslIbl::k_BrdfLutSize - 1);
		const glm::vec2 uv = GlslIbl::DE_BrdfLutUv(value, value);
		CHECK(uv.x == doctest::Approx((i + 0.5f) / 128.0f));
		const glm::vec2 back = GlslIbl::DE_BrdfLutValue(uv);
		CHECK(back.x == doctest::Approx(value).epsilon(1e-4));
		CHECK(back.y == doctest::Approx(value).epsilon(1e-4));
	}
}

TEST_CASE("Ibl: specular occlusion is one at full ambient occlusion and falls with it")
{
	for (float NoV : { 0.1f, 0.5f, 1.0f })
		for (float a : { 0.0f, 0.1f, 0.5f, 1.0f })
		{
			CHECK(GlslIbl::DE_SpecularOcclusion(NoV, 1.0f, a) == doctest::Approx(1.0f));
			const float half = GlslIbl::DE_SpecularOcclusion(NoV, 0.5f, a);
			CHECK(half >= 0.0f);
			CHECK(half <= 1.0f);
			CHECK(GlslIbl::DE_SpecularOcclusion(NoV, 0.0f, a) <= half + 1e-5f);
		}
}

TEST_CASE("Ibl: the shader's prefilter mip count matches the engine's k_IblMipLevels (6)")
{
	CHECK(GlslIbl::k_PrefilterMips == 6);
}
