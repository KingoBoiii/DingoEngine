#include <doctest/doctest.h>

#include <glm/glm.hpp>

#include <cmath>

// The engine's own Atmosphere.glsl, compiled as C++ over GLM: the terms tested are the shader's text, not a copy.
namespace GlslAtmosphere
{
	using glm::vec2;
	using glm::vec3;
	using glm::vec4;
	using glm::abs;
	using glm::acos;
	using glm::clamp;
	using glm::cos;
	using glm::exp;
	using glm::max;
	using glm::pow;
	using glm::sqrt;

#include "DingoEngine/Graphics/Shaders/Atmosphere.glsl"
}

namespace
{
	using namespace GlslAtmosphere;

	double ClosedFormDepth(double rayleigh, double mieExtinction, double ozone)
	{
		const double depthKm = DE_ATMOSPHERE_TOP - DE_PLANET_RADIUS;
		return rayleigh * DE_RAYLEIGH_HEIGHT * (1.0 - std::exp(-depthKm / DE_RAYLEIGH_HEIGHT))
			+ mieExtinction * DE_MIE_HEIGHT * (1.0 - std::exp(-depthKm / DE_MIE_HEIGHT))
			+ ozone * DE_OZONE_HALF_WIDTH;
	}
}

TEST_CASE("Atmosphere: the vertical transmittance at the ground matches the closed form")
{
	const DE_Atmosphere atmosphere = DE_EarthAtmosphere(1.0f, 1.0f, 1.0f, 0.8f);
	const glm::vec3 transmittance = DE_TransmittanceMarch(atmosphere, DE_PLANET_RADIUS, 1.0f);

	const double mie = 3.996e-3 + 4.4e-3;
	const double expected[3] = {
		std::exp(-ClosedFormDepth(5.802e-3, mie, 0.65e-3)),
		std::exp(-ClosedFormDepth(13.558e-3, mie, 1.881e-3)),
		std::exp(-ClosedFormDepth(33.1e-3, mie, 0.085e-3)),
	};
	for (int channel = 0; channel < 3; ++channel)
		CHECK(transmittance[channel] == doctest::Approx(expected[channel]).epsilon(0.01));
}

TEST_CASE("Atmosphere: a ray into the planet has no sun transmittance")
{
	const DE_Atmosphere atmosphere = DE_EarthAtmosphere(1.0f, 1.0f, 1.0f, 0.8f);
	const glm::vec3 below = DE_TransmittanceToTop(atmosphere, DE_PLANET_RADIUS, -0.2f);
	CHECK(below.x == 0.0f);
	CHECK(below.y == 0.0f);
	CHECK(below.z == 0.0f);
	CHECK(DE_TransmittanceToTop(atmosphere, DE_PLANET_RADIUS, 0.2f).x > 0.0f);
}

TEST_CASE("Atmosphere: a low sun is redder than a high one")
{
	const DE_Atmosphere atmosphere = DE_EarthAtmosphere(1.0f, 1.0f, 1.0f, 0.8f);
	const glm::vec3 noon = DE_TransmittanceMarch(atmosphere, DE_PLANET_RADIUS, 1.0f);
	const glm::vec3 low = DE_TransmittanceMarch(atmosphere, DE_PLANET_RADIUS, 0.02f);
	CHECK(low.x / low.z > 3.0f * noon.x / noon.z);
}

TEST_CASE("Atmosphere: the sky-view mapping round-trips")
{
	const float radii[2] = { DE_PLANET_RADIUS, DE_PLANET_RADIUS + 12.0f };
	for (float r : radii)
	{
		for (float v = 0.02f; v < 1.0f; v += 0.07f)
		{
			for (float u = 0.02f; u < 1.0f; u += 0.09f)
			{
				const glm::vec2 params = DE_SkyViewParams(r, glm::vec2(u, v));
				const glm::vec2 uv = DE_SkyViewUv(r, params.x, params.y);
				CHECK(uv.x == doctest::Approx(u).epsilon(0.01).scale(1.0));
				CHECK(uv.y == doctest::Approx(v).epsilon(0.01).scale(1.0));
			}
		}
	}
}

TEST_CASE("Atmosphere: the transmittance mapping round-trips")
{
	const glm::vec2 samples[4] = { { 0.1f, 0.1f }, { 0.5f, 0.5f }, { 0.9f, 0.3f }, { 0.3f, 0.95f } };
	for (const glm::vec2& unit : samples)
	{
		const glm::vec2 point = DE_TransmittanceFromUnit(unit);
		const glm::vec2 back = DE_TransmittanceUnit(point.x, point.y);
		CHECK(back.x == doctest::Approx(unit.x).scale(1.0).epsilon(0.002));
		CHECK(back.y == doctest::Approx(unit.y).scale(1.0).epsilon(0.002));
	}
}

TEST_CASE("Atmosphere: the sun's limb is full strength at the centre and dark at the edge")
{
	const glm::vec3 centre = DE_SunLimb(0.0f);
	CHECK(centre.x == doctest::Approx(1.0f));
	CHECK(centre.y == doctest::Approx(1.0f));
	CHECK(centre.z == doctest::Approx(1.0f));
	const glm::vec3 edge = DE_SunLimb(1.0f);
	CHECK(edge.x == 0.0f);
	const glm::vec3 half = DE_SunLimb(0.49f);
	CHECK(half.x > half.z);
}
