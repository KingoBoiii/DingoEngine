#include <doctest/doctest.h>

#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>

// The engine's own Brdf.glsl, compiled as C++ over GLM (see ToneMappingTests.cpp): the terms tested are
// the shader's text, not a copy.
namespace GlslBrdf
{
	using glm::vec2;
	using glm::vec3;
	using glm::vec4;
	using glm::clamp;
	using glm::exp2;
	using glm::max;
	using glm::min;
	using glm::mix;
	using glm::pow;
	using glm::sqrt;
	using glm::step;

#include "DingoEngine/Graphics/Shaders/Brdf.glsl"
}

namespace
{
	constexpr double k_Pi = 3.14159265358979323846;
	const float k_Roughness[] = { 0.1f, 0.25f, 0.5f, 0.75f, 1.0f };

	// Integrates f(cosTheta, sinTheta, phi) over the hemisphere, uniform in theta so a narrow lobe is resolved.
	template<typename F>
	double IntegrateHemisphere(F&& f)
	{
		constexpr int thetaSteps = 4000;
		constexpr int phiSteps = 64;
		double sum = 0.0;
		for (int i = 0; i < thetaSteps; ++i)
		{
			const double theta = 0.5 * k_Pi * (i + 0.5) / thetaSteps;
			const double sinTheta = std::sin(theta);
			const double cosTheta = std::cos(theta);
			for (int j = 0; j < phiSteps; ++j)
			{
				const double phi = 2.0 * k_Pi * (j + 0.5) / phiSteps;
				sum += f(cosTheta, sinTheta, phi) * sinTheta;
			}
		}
		return sum * (0.5 * k_Pi / thetaSteps) * (2.0 * k_Pi / phiSteps);
	}
}

TEST_CASE("Brdf: D_GGX integrates to one over the projected hemisphere")
{
	for (float roughness : k_Roughness)
	{
		const float a = roughness * roughness;
		const double total = IntegrateHemisphere([&](double cosTheta, double, double)
		{
			return static_cast<double>(GlslBrdf::D_GGX(static_cast<float>(cosTheta), a)) * cosTheta;
		});
		CHECK(total == doctest::Approx(1.0).epsilon(0.02));
	}
}

TEST_CASE("Brdf: the white furnace never gains energy and a white F0 = 1 specular keeps at least 0.9 (0.25) and 0.8 (0.5) of it at NoV 0.6")
{
	const float NoV = 0.6f;
	const double sinV = std::sqrt(1.0 - static_cast<double>(NoV) * NoV);
	for (float roughness : k_Roughness)
	{
		const float a = roughness * roughness;
		const double total = IntegrateHemisphere([&](double cosL, double sinL, double phi)
		{
			const double lx = sinL * std::cos(phi);
			const double ly = sinL * std::sin(phi);
			const double hx = lx + sinV;
			const double hy = ly;
			const double hz = cosL + NoV;
			const double len = std::sqrt(hx * hx + hy * hy + hz * hz);
			const float NoH = static_cast<float>(hz / len);
			const float VoH = static_cast<float>((sinV * hx + NoV * hz) / len);
			const float NoL = static_cast<float>(cosL);
			const glm::vec3 F = GlslBrdf::F_Schlick(glm::vec3(1.0f), VoH);
			const double brdf = static_cast<double>(GlslBrdf::D_GGX(NoH, a)) * GlslBrdf::V_SmithGGXCorrelated(NoV, NoL, a) * F.x;
			return brdf * NoL;
		});
		CHECK(total <= 1.0 + 0.02);
		CHECK(total > 0.0);
		// The single-scatter albedo reads about 0.975 at 0.25 and 0.87 at 0.5; a V term half the size reads
		// about 0.49 and 0.44. At 0.1 the lobe is narrower than the phi grid, so only the upper bound holds.
		if (roughness == 0.25f)
			CHECK(total >= 0.9);
		if (roughness == 0.5f)
			CHECK(total >= 0.8);
	}
}

TEST_CASE("Brdf: F_Schlick returns F0 head-on and one at grazing")
{
	const glm::vec3 F0(0.04f, 0.5f, 0.9f);
	const glm::vec3 head = GlslBrdf::F_Schlick(F0, 1.0f);
	const glm::vec3 grazing = GlslBrdf::F_Schlick(F0, 0.0f);
	for (int i = 0; i < 3; ++i)
	{
		CHECK(head[i] == doctest::Approx(F0[i]));
		CHECK(grazing[i] == doctest::Approx(1.0f));
	}
}

TEST_CASE("Brdf: sRGB round trips every 8-bit value")
{
	for (int v = 0; v < 256; ++v)
	{
		const float s = static_cast<float>(v) / 255.0f;
		const glm::vec3 linear = GlslBrdf::SrgbToLinear(glm::vec3(s));
		const glm::vec3 back = GlslBrdf::LinearToSrgb(linear);
		CHECK(static_cast<int>(std::lround(back.x * 255.0f)) == v);
	}
	CHECK(GlslBrdf::SrgbToLinear(glm::vec3(-1.0f)).x == 0.0f);
	CHECK(GlslBrdf::SrgbToLinear(glm::vec3(1.0f)).x == doctest::Approx(1.0f));
	CHECK(GlslBrdf::SrgbToLinear(glm::vec3(0.5f)).x == doctest::Approx(0.214041f).epsilon(0.001));
}

TEST_CASE("Brdf: EnvBRDFApprox stays in [0, 1]")
{
	for (int i = 0; i <= 20; ++i)
	{
		for (int j = 0; j <= 20; ++j)
		{
			const glm::vec2 ab = GlslBrdf::EnvBRDFApprox(static_cast<float>(i) / 20.0f, static_cast<float>(j) / 20.0f);
			CHECK(ab.x >= 0.0f);
			CHECK(ab.x <= 1.0f);
			CHECK(ab.y >= 0.0f);
			CHECK(ab.y <= 1.0f);
			CHECK(ab.x + ab.y <= 1.0f + 0.05f);
		}
	}
	const glm::vec3 spec = GlslBrdf::EnvBRDFApprox(glm::vec3(1.0f), 0.5f, 0.5f);
	CHECK(spec.x <= 1.05f);
}
