#include <doctest/doctest.h>

#include <glm/glm.hpp>
#include <glm/gtc/bitfield.hpp>

#include <cmath>
#include <cstdint>

// The engine's own VolumetricMath.glsl compiled as C++ over GLM: the terms tested are the shader's text,
// not a copy.
namespace GlslVolumetric
{
	using glm::bitfieldReverse;
	using glm::clamp;
	using glm::exp;
	using glm::log2;
	using glm::max;
	using glm::pow;
	using glm::sqrt;
	using uint = std::uint32_t;

#include "DingoEngine/Graphics/Shaders/VolumetricMath.glsl"
}

namespace
{
	constexpr double k_Pi = 3.14159265358979323846;
}

TEST_CASE("Volumetric: slice boundaries run from Near to Far and grow monotonically")
{
	const float n = 0.5f;
	const float f = 64.0f;
	const float d = 64.0f;
	CHECK(GlslVolumetric::DE_VolumeSliceDepth(0.0f, d, n, f) == doctest::Approx(n));
	CHECK(GlslVolumetric::DE_VolumeSliceDepth(d, d, n, f) == doctest::Approx(f).epsilon(1e-4));
	float previous = 0.0f;
	for (int b = 0; b <= 64; ++b)
	{
		const float z = GlslVolumetric::DE_VolumeSliceDepth(static_cast<float>(b), d, n, f);
		CHECK(z > previous);
		previous = z;
	}
}

TEST_CASE("Volumetric: depth to slice inverts the boundary depth and the coordinate stays on the texel centres")
{
	const float n = 0.5f;
	const float f = 64.0f;
	const float d = 64.0f;
	for (int b = 0; b <= 64; b += 4)
	{
		const float z = GlslVolumetric::DE_VolumeSliceDepth(static_cast<float>(b), d, n, f);
		CHECK(GlslVolumetric::DE_VolumeDepthToSlice(z, d, n, f) == doctest::Approx(static_cast<float>(b)).epsilon(1e-3).scale(1.0));
	}
	CHECK(GlslVolumetric::DE_VolumeDepthToCoord(0.01f, d, n, f) == doctest::Approx(0.5f / d));
	CHECK(GlslVolumetric::DE_VolumeDepthToCoord(1000.0f, d, n, f) == doctest::Approx(1.0f - 0.5f / d));
	const float mid = GlslVolumetric::DE_VolumeSliceDepth(10.5f, d, n, f);
	CHECK(GlslVolumetric::DE_VolumeDepthToCoord(mid, d, n, f) == doctest::Approx(10.0f / d).epsilon(1e-3));
}

TEST_CASE("Volumetric: the Henyey-Greenstein phase is 1/4pi at g = 0 and integrates to one over the sphere")
{
	CHECK(GlslVolumetric::DE_HenyeyGreenstein(0.3f, 0.0f) == doctest::Approx(1.0 / (4.0 * k_Pi)).epsilon(1e-5));
	for (float g : { -0.8f, -0.3f, 0.0f, 0.5f, 0.9f })
	{
		const int steps = 20000;
		double total = 0.0;
		for (int i = 0; i < steps; ++i)
		{
			const double c = -1.0 + 2.0 * (i + 0.5) / steps;
			total += static_cast<double>(GlslVolumetric::DE_HenyeyGreenstein(static_cast<float>(c), g)) * 2.0 * k_Pi * (2.0 / steps);
		}
		CHECK(total == doctest::Approx(1.0).epsilon(0.01));
	}
	CHECK(GlslVolumetric::DE_HenyeyGreenstein(1.0f, 0.6f) > GlslVolumetric::DE_HenyeyGreenstein(-1.0f, 0.6f));
}

TEST_CASE("Volumetric: height fog density falls off above the base and is constant below it")
{
	CHECK(GlslVolumetric::DE_HeightFogDensity(0.2f, 0.5f, 2.0f, -3.0f) == doctest::Approx(0.2f));
	CHECK(GlslVolumetric::DE_HeightFogDensity(0.2f, 0.5f, 2.0f, 2.0f) == doctest::Approx(0.2f));
	CHECK(GlslVolumetric::DE_HeightFogDensity(0.2f, 0.5f, 2.0f, 4.0f) == doctest::Approx(0.2 * std::exp(-1.0)).epsilon(1e-5));
	CHECK(GlslVolumetric::DE_HeightFogDensity(0.2f, 0.0f, 2.0f, 40.0f) == doctest::Approx(0.2f));
}

TEST_CASE("Volumetric: accumulating slices of a uniform medium matches the closed form, including the zero-density limit")
{
	const float sigmaT = 0.3f;
	const float source = 0.7f;
	const int slices = 64;
	const float n = 0.5f;
	const float f = 64.0f;

	double scattered = 0.0;
	double transmittance = 1.0;
	float previousDepth = 0.0f;
	for (int k = 0; k < slices; ++k)
	{
		const float depth = GlslVolumetric::DE_VolumeSliceDepth(static_cast<float>(k + 1), static_cast<float>(slices), n, f);
		const float len = depth - previousDepth;
		const float tr = GlslVolumetric::DE_SliceTransmittance(sigmaT, len);
		scattered += transmittance * static_cast<double>(source) * GlslVolumetric::DE_SliceInScatterFactor(sigmaT, len, tr);
		transmittance *= tr;
		previousDepth = depth;
	}
	const double expectedT = std::exp(-static_cast<double>(sigmaT) * f);
	CHECK(transmittance == doctest::Approx(expectedT).epsilon(1e-3));
	CHECK(scattered == doctest::Approx(static_cast<double>(source) * (1.0 - expectedT) / sigmaT).epsilon(1e-3));

	const float len = 2.0f;
	const float tr = GlslVolumetric::DE_SliceTransmittance(0.0f, len);
	CHECK(tr == doctest::Approx(1.0f));
	CHECK(GlslVolumetric::DE_SliceInScatterFactor(0.0f, len, tr) == doctest::Approx(len));
}

TEST_CASE("Volumetric: Halton base 2 starts 0.5, 0.25, 0.75, 0.125 and averages one half")
{
	CHECK(GlslVolumetric::DE_HaltonBase2(1u) == doctest::Approx(0.5f));
	CHECK(GlslVolumetric::DE_HaltonBase2(2u) == doctest::Approx(0.25f));
	CHECK(GlslVolumetric::DE_HaltonBase2(3u) == doctest::Approx(0.75f));
	CHECK(GlslVolumetric::DE_HaltonBase2(4u) == doctest::Approx(0.125f));
	double sum = 0.0;
	for (std::uint32_t i = 1; i <= 1024; ++i)
		sum += GlslVolumetric::DE_HaltonBase2(i);
	CHECK(sum / 1024.0 == doctest::Approx(0.5).epsilon(0.01));
}
