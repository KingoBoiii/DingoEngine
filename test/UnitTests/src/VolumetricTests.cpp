#include <doctest/doctest.h>

#include <glm/glm.hpp>
#include <glm/gtc/bitfield.hpp>

#include "DingoEngine/Graphics/VolumeNoise.h"

#include <cmath>
#include <cstdint>
#include <vector>

// The engine's own VolumetricMath.glsl compiled as C++ over GLM: the terms tested are the shader's text,
// not a copy.
namespace GlslVolumetric
{
	using glm::abs;
	using glm::bitfieldReverse;
	using glm::clamp;
	using glm::dot;
	using glm::exp;
	using glm::fract;
	using glm::length;
	using glm::log2;
	using glm::max;
	using glm::min;
	using glm::pow;
	using glm::sqrt;
	using glm::vec2;
	using glm::vec3;
	using glm::vec4;
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

TEST_CASE("Volumetric: the distance fade holds the keeps outside the range and is linear inside, and no range is no fade")
{
	using GlslVolumetric::DE_DistanceFade;
	CHECK(DE_DistanceFade(0.0f, 10.0f, 20.0f, 1.0f, 0.25f) == doctest::Approx(1.0f));
	CHECK(DE_DistanceFade(10.0f, 10.0f, 20.0f, 1.0f, 0.25f) == doctest::Approx(1.0f));
	CHECK(DE_DistanceFade(15.0f, 10.0f, 20.0f, 1.0f, 0.25f) == doctest::Approx(0.625f));
	CHECK(DE_DistanceFade(20.0f, 10.0f, 20.0f, 1.0f, 0.25f) == doctest::Approx(0.25f));
	CHECK(DE_DistanceFade(500.0f, 10.0f, 20.0f, 1.0f, 0.25f) == doctest::Approx(0.25f));
	CHECK(DE_DistanceFade(15.0f, 20.0f, 10.0f, 0.0f, 0.0f) == doctest::Approx(1.0f));
	CHECK(DE_DistanceFade(15.0f, 15.0f, 15.0f, 0.0f, 0.0f) == doctest::Approx(1.0f));
}

TEST_CASE("Volumetric: the height profile fades linearly and the band multiplies with soft edges")
{
	using GlslVolumetric::DE_HeightProfile;
	using GlslVolumetric::vec4;
	const vec4 off(0.0f, 0.0f, 0.0f, 0.0f);
	const vec4 noBand(1.0f, 0.5f, 0.0f, 0.0f);
	CHECK(DE_HeightProfile(7.0f, off, noBand) == doctest::Approx(1.0f));

	const vec4 fade(2.0f, 6.0f, 0.0f, 0.0f);
	CHECK(DE_HeightProfile(0.0f, fade, noBand) == doctest::Approx(1.0f));
	CHECK(DE_HeightProfile(2.0f, fade, noBand) == doctest::Approx(1.0f));
	CHECK(DE_HeightProfile(4.0f, fade, noBand) == doctest::Approx(0.5f));
	CHECK(DE_HeightProfile(6.0f, fade, noBand) == doctest::Approx(0.0f));
	CHECK(DE_HeightProfile(50.0f, fade, noBand) == doctest::Approx(0.0f));

	const vec4 bandFade(0.0f, 0.0f, 1.0f, 3.0f);
	const vec4 band(3.0f, 0.5f, 0.0f, 0.0f);
	CHECK(DE_HeightProfile(0.0f, bandFade, band) == doctest::Approx(1.0f));
	CHECK(DE_HeightProfile(2.0f, bandFade, band) == doctest::Approx(3.0f));
	CHECK(DE_HeightProfile(3.5f, bandFade, band) == doctest::Approx(1.0f));
	CHECK(DE_HeightProfile(1.25f, bandFade, band) == doctest::Approx(2.0f));

	const vec4 inverted(0.0f, 0.0f, 3.0f, 1.0f);
	CHECK(DE_HeightProfile(2.0f, inverted, band) == doctest::Approx(1.0f));
}

TEST_CASE("Volumetric: box and ellipsoid edge fades are 0 outside, 1 deep inside and ramp over the edge width")
{
	using GlslVolumetric::DE_BoxEdgeFade;
	using GlslVolumetric::DE_EdgeFadeFromDistance;
	using GlslVolumetric::DE_EllipsoidEdgeFade;
	using GlslVolumetric::vec3;
	const vec3 half(4.0f, 2.0f, 8.0f);

	CHECK(DE_BoxEdgeFade(vec3(0.0f), half, 0.0f) == doctest::Approx(1.0f));
	CHECK(DE_BoxEdgeFade(vec3(0.999f, 0.0f, 0.0f), half, 0.0f) == doctest::Approx(1.0f));
	CHECK(DE_BoxEdgeFade(vec3(1.2f, 0.0f, 0.0f), half, 0.0f) == doctest::Approx(0.0f));
	CHECK(DE_BoxEdgeFade(vec3(1.0f, 0.0f, 0.0f), half, 1.0f) == doctest::Approx(0.0f));
	CHECK(DE_BoxEdgeFade(vec3(0.0f), half, 1.0f) == doctest::Approx(1.0f));
	CHECK(DE_BoxEdgeFade(vec3(0.0f, 0.5f, 0.0f), half, 2.0f) == doctest::Approx(0.5f));
	CHECK(DE_BoxEdgeFade(vec3(0.0f, 0.0f, 1.5f), half, 2.0f) == doctest::Approx(0.0f));

	CHECK(DE_EllipsoidEdgeFade(vec3(0.0f), half, 0.0f) == doctest::Approx(1.0f));
	CHECK(DE_EllipsoidEdgeFade(vec3(1.1f, 0.0f, 0.0f), half, 0.0f) == doctest::Approx(0.0f));
	CHECK(DE_EllipsoidEdgeFade(vec3(0.5f, 0.0f, 0.0f), half, 2.0f) == doctest::Approx(0.5f));
	CHECK(DE_EllipsoidEdgeFade(vec3(0.0f), half, 2.0f) == doctest::Approx(1.0f));

	float previous = 0.0f;
	for (int i = 0; i <= 10; ++i)
	{
		const float v = DE_EdgeFadeFromDistance(static_cast<float>(i) * 0.1f, 1.0f);
		CHECK(v >= previous);
		previous = v;
	}
	CHECK(previous == doctest::Approx(1.0f));
}

TEST_CASE("Volumetric: ray intervals through the unit box and sphere")
{
	using GlslVolumetric::DE_RayBoxInterval;
	using GlslVolumetric::DE_RayUnitSphereInterval;
	using GlslVolumetric::vec2;
	using GlslVolumetric::vec3;

	vec2 box = DE_RayBoxInterval(vec3(-3.0f, 0.0f, 0.0f), vec3(1.0f, 0.0f, 0.0f));
	CHECK(box.x == doctest::Approx(2.0f));
	CHECK(box.y == doctest::Approx(4.0f));

	box = DE_RayBoxInterval(vec3(0.0f), vec3(0.0f, 2.0f, 0.0f));
	CHECK(box.x == doctest::Approx(-0.5f));
	CHECK(box.y == doctest::Approx(0.5f));

	box = DE_RayBoxInterval(vec3(2.0f, 0.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f));
	CHECK(box.x > box.y);
	box = DE_RayBoxInterval(vec3(-3.0f, 2.0f, 0.0f), vec3(1.0f, 0.0f, 0.0f));
	CHECK(box.x > box.y);
	box = DE_RayBoxInterval(vec3(-3.0f, 0.0f, 0.0f), vec3(-1.0f, 0.0f, 0.0f));
	CHECK(box.x < box.y);
	CHECK(box.y < 0.0f);

	vec2 ball = DE_RayUnitSphereInterval(vec3(-3.0f, 0.0f, 0.0f), vec3(1.0f, 0.0f, 0.0f));
	CHECK(ball.x == doctest::Approx(2.0f));
	CHECK(ball.y == doctest::Approx(4.0f));
	ball = DE_RayUnitSphereInterval(vec3(0.0f), vec3(0.0f, 0.0f, 4.0f));
	CHECK(ball.x == doctest::Approx(-0.25f));
	CHECK(ball.y == doctest::Approx(0.25f));
	ball = DE_RayUnitSphereInterval(vec3(-3.0f, 1.5f, 0.0f), vec3(1.0f, 0.0f, 0.0f));
	CHECK(ball.x > ball.y);
	ball = DE_RayUnitSphereInterval(vec3(0.0f), vec3(0.0f));
	CHECK(ball.x > ball.y);
}

TEST_CASE("Volumetric: interleaved gradient noise stays in [0, 1) and is stable per pixel")
{
	using GlslVolumetric::DE_InterleavedGradientNoise;
	using GlslVolumetric::vec2;
	double sum = 0.0;
	int count = 0;
	for (int y = 0; y < 64; ++y)
	{
		for (int x = 0; x < 64; ++x)
		{
			const float v = DE_InterleavedGradientNoise(vec2(static_cast<float>(x), static_cast<float>(y)));
			CHECK(v >= 0.0f);
			CHECK(v < 1.0f);
			CHECK(v == DE_InterleavedGradientNoise(vec2(static_cast<float>(x), static_cast<float>(y))));
			sum += v;
			++count;
		}
	}
	CHECK(sum / count == doctest::Approx(0.5).epsilon(0.05));
}

TEST_CASE("Volumetric: capping the in-scatter keeps the hue, leaves a lower colour and a zero cap alone")
{
	using GlslVolumetric::DE_CapInScatter;
	using GlslVolumetric::vec3;
	const vec3 bright(8.0f, 4.0f, 2.0f);
	const vec3 capped = DE_CapInScatter(bright, 2.0f);
	CHECK(capped.x == doctest::Approx(2.0f));
	CHECK(capped.y == doctest::Approx(1.0f));
	CHECK(capped.z == doctest::Approx(0.5f));
	const vec3 dim = DE_CapInScatter(vec3(1.0f, 0.5f, 0.25f), 2.0f);
	CHECK(dim.x == doctest::Approx(1.0f));
	CHECK(dim.y == doctest::Approx(0.5f));
	const vec3 uncapped = DE_CapInScatter(bright, 0.0f);
	CHECK(uncapped.x == doctest::Approx(8.0f));
	CHECK(uncapped.z == doctest::Approx(2.0f));
}

TEST_CASE("Volumetric: the volume noise is deterministic, fills the byte range and tiles in every axis")
{
	const std::vector<std::uint8_t> a = Dingo::Internal::GenerateVolumeNoise();
	const std::vector<std::uint8_t> b = Dingo::Internal::GenerateVolumeNoise();
	constexpr std::uint32_t n = Dingo::Internal::k_VolumeNoiseSize;
	REQUIRE(a.size() == static_cast<size_t>(n) * n * n);
	CHECK(a == b);

	int lo = 255;
	int hi = 0;
	double sum = 0.0;
	for (std::uint8_t v : a)
	{
		lo = (std::min)(lo, static_cast<int>(v));
		hi = (std::max)(hi, static_cast<int>(v));
		sum += v;
	}
	CHECK(lo < 64);
	CHECK(hi > 192);
	CHECK(sum / static_cast<double>(a.size()) == doctest::Approx(127.5).epsilon(0.1));

	const auto at = [&](std::uint32_t x, std::uint32_t y, std::uint32_t z)
	{
		return static_cast<int>(a[(static_cast<size_t>(z % n) * n + y % n) * n + x % n]);
	};
	int worstStep = 0;
	int worstWrap = 0;
	for (std::uint32_t z = 0; z < n; ++z)
	{
		for (std::uint32_t y = 0; y < n; ++y)
		{
			for (std::uint32_t x = 0; x < n; ++x)
			{
				if (x + 1 < n)
					worstStep = (std::max)(worstStep, std::abs(at(x, y, z) - at(x + 1, y, z)));
				if (y + 1 < n)
					worstStep = (std::max)(worstStep, std::abs(at(x, y, z) - at(x, y + 1, z)));
				if (z + 1 < n)
					worstStep = (std::max)(worstStep, std::abs(at(x, y, z) - at(x, y, z + 1)));
			}
		}
	}
	for (std::uint32_t i = 0; i < n; ++i)
	{
		for (std::uint32_t j = 0; j < n; ++j)
		{
			worstWrap = (std::max)(worstWrap, std::abs(at(n - 1, i, j) - at(0, i, j)));
			worstWrap = (std::max)(worstWrap, std::abs(at(i, n - 1, j) - at(i, 0, j)));
			worstWrap = (std::max)(worstWrap, std::abs(at(i, j, n - 1) - at(i, j, 0)));
		}
	}
	CHECK(worstWrap <= worstStep);
	CHECK(worstStep < 80);
}
