#include <doctest/doctest.h>

#include <DingoEngine/Graphics/PostProcess.h>

#include <glm/glm.hpp>

#include <algorithm>
#include <limits>

// The engine's own ToneMapping.glsl, compiled as C++ over GLM: the curves tested are the shader's
// text, not a copy. The shim gives GLSL's float literals (doubles here) the overloads they need.
namespace GlslToneMapping
{
	using glm::vec3;
	using glm::clamp;
	using glm::isnan;
	using glm::max;
	using glm::min;
	using glm::mix;

	inline float max(float a, double b) { return (std::max)(a, static_cast<float>(b)); }
	inline vec3 operator*(double a, const vec3& v) { return static_cast<float>(a) * v; }
	inline vec3 operator+(const vec3& v, double a) { return v + static_cast<float>(a); }
	inline vec3 clamp(const vec3& v, double low, double high) { return glm::clamp(v, static_cast<float>(low), static_cast<float>(high)); }

#include "DingoEngine/Graphics/Shaders/ToneMapping.glsl"
}

using namespace GlslToneMapping;

namespace
{
	constexpr float k_Knee = 0.8f;
	constexpr float k_White = 4.0f;

	float Peak(const vec3& color)
	{
		return (std::max)(color.r, (std::max)(color.g, color.b));
	}
}

TEST_SUITE("ToneMapping")
{
	TEST_CASE("the shader's operator numbers match ToneMapOperator")
	{
		CHECK(static_cast<int>(Dingo::ToneMapOperator::None) == TONEMAP_NONE);
		CHECK(static_cast<int>(Dingo::ToneMapOperator::Soft) == TONEMAP_SOFT);
		CHECK(static_cast<int>(Dingo::ToneMapOperator::ACES) == TONEMAP_ACES);
		CHECK(static_cast<int>(Dingo::ToneMapOperator::Neutral) == TONEMAP_NEUTRAL);
	}

	TEST_CASE("non-finite colour turns finite before any curve")
	{
		const float nan = std::numeric_limits<float>::quiet_NaN();
		const float inf = std::numeric_limits<float>::infinity();
		const vec3 color = ToneMap(vec3(nan, inf, -1.0f), TONEMAP_NONE, k_Knee, k_White);
		CHECK(color.r == 0.0f);
		CHECK(color.g == 65000.0f);
		CHECK(color.b == 0.0f);
		for (int op : { TONEMAP_SOFT, TONEMAP_ACES, TONEMAP_NEUTRAL })
		{
			const vec3 mapped = ToneMap(vec3(nan, inf, 2.0f), op, k_Knee, k_White);
			CHECK(glm::all(glm::lessThanEqual(mapped, vec3(1.0f + 1e-5f))));
			CHECK_FALSE(glm::any(glm::isnan(mapped)));
		}
	}

	TEST_CASE("Soft: identity to the knee, slope 1 leaving it, 1 at the white point, hue kept")
	{
		const vec3 below(0.1f, 0.5f, 0.79f);
		CHECK(ToneMapSoft(below, k_Knee, k_White) == below);

		const float step = 1e-3f;
		CHECK(Peak(ToneMapSoft(vec3(k_Knee + step), k_Knee, k_White)) == doctest::Approx(k_Knee + step).epsilon(1e-5));

		CHECK(Peak(ToneMapSoft(vec3(k_White, 1.0f, 0.5f), k_Knee, k_White)) == doctest::Approx(1.0f));
		CHECK(Peak(ToneMapSoft(vec3(100.0f), k_Knee, k_White)) == doctest::Approx(1.0f));

		const vec3 bright(3.0f, 1.5f, 0.75f);
		const vec3 mapped = ToneMapSoft(bright, k_Knee, k_White);
		CHECK(mapped.g / mapped.r == doctest::Approx(0.5f));
		CHECK(mapped.b / mapped.r == doctest::Approx(0.25f));

		float last = 0.0f;
		for (float value = 0.0f; value < 8.0f; value += 0.05f)
		{
			const float peak = Peak(ToneMapSoft(vec3(value), k_Knee, k_White));
			CHECK(peak >= last - 1e-6f);
			last = peak;
		}
	}

	TEST_CASE("ACES and Neutral: black stays black, the curve rises and stays at most 1")
	{
		for (int op : { TONEMAP_ACES, TONEMAP_NEUTRAL })
		{
			CAPTURE(op);
			CHECK(Peak(ToneMap(vec3(0.0f), op, k_Knee, k_White)) == doctest::Approx(0.0f).epsilon(1e-3));
			float last = -1.0f;
			for (float value = 0.0f; value < 50.0f; value *= 1.1f, value += 0.01f)
			{
				const float peak = Peak(ToneMap(vec3(value), op, k_Knee, k_White));
				CHECK(peak >= last - 1e-6f);
				CHECK(peak <= 1.0f + 1e-5f);
				last = peak;
			}
			CHECK(last > 0.95f);
		}
	}
}
