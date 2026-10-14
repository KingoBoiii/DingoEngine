#include <doctest/doctest.h>

#include <DingoEngine/Graphics/Light.h>
#include "DingoEngine/Graphics/LightMath.h"

#include <limits>

using namespace Dingo;

TEST_SUITE("LightMath")
{
	TEST_CASE("a point light falls off as (1 - d^2 / range^2)^2 and is 0 from its range on")
	{
		PointLight light;
		light.Position = { 1.0f, 2.0f, 3.0f };
		light.Range = 10.0f;

		CHECK(GetLightAttenuation(light, light.Position) == doctest::Approx(1.0f));
		CHECK(GetLightAttenuation(light, light.Position + glm::vec3(5.0f, 0.0f, 0.0f)) == doctest::Approx(0.5625f));
		CHECK(GetLightAttenuation(light, light.Position + glm::vec3(0.0f, 10.0f, 0.0f)) == doctest::Approx(0.0f));
		CHECK(GetLightAttenuation(light, light.Position + glm::vec3(0.0f, 0.0f, 50.0f)) == doctest::Approx(0.0f));
	}

	TEST_CASE("unusable lights weigh nothing")
	{
		PointLight dark;
		dark.Intensity = 0.0f;
		CHECK(GetLightAttenuation(dark, glm::vec3(0.0f)) == 0.0f);

		PointLight broken;
		broken.Position.x = std::numeric_limits<float>::quiet_NaN();
		CHECK_FALSE(Internal::IsUsableLight(broken));
		CHECK(GetLightAttenuation(broken, glm::vec3(0.0f)) == 0.0f);

		SpotLight noRange;
		noRange.Range = 0.0f;
		CHECK(GetLightAttenuation(noRange, glm::vec3(0.0f, -1.0f, 0.0f)) == 0.0f);
	}

	TEST_CASE("a spot light is full inside its inner cone and dark past its outer one")
	{
		SpotLight light;
		light.Position = glm::vec3(0.0f);
		light.Direction = { 0.0f, -2.0f, 0.0f };
		light.Range = 10.0f;
		light.InnerConeAngle = 20.0f;
		light.OuterConeAngle = 30.0f;

		const float falloff = 1.0f - 0.25f;
		CHECK(GetLightAttenuation(light, { 0.0f, -5.0f, 0.0f }) == doctest::Approx(falloff * falloff));

		const glm::vec3 at25 = 5.0f * glm::vec3(std::sin(glm::radians(25.0f)), -std::cos(glm::radians(25.0f)), 0.0f);
		const float half = GetLightAttenuation(light, at25);
		CHECK(half > 0.0f);
		CHECK(half < falloff * falloff);

		const glm::vec3 at35 = 5.0f * glm::vec3(std::sin(glm::radians(35.0f)), -std::cos(glm::radians(35.0f)), 0.0f);
		CHECK(GetLightAttenuation(light, at35) == doctest::Approx(0.0f));
		CHECK(GetLightAttenuation(light, { 0.0f, 5.0f, 0.0f }) == doctest::Approx(0.0f));
	}

	TEST_CASE("cone set-up: a point light's factor is always 1, a spot's runs 1 to 0 between its angles")
	{
		const Internal::LightCone point = Internal::GetLightCone(PointLight{});
		CHECK(point.Scale == 0.0f);
		CHECK(point.Offset == 1.0f);

		SpotLight light;
		light.Direction = { 0.0f, 0.0f, 3.0f };
		light.InnerConeAngle = 10.0f;
		light.OuterConeAngle = 40.0f;
		const Internal::LightCone cone = Internal::GetLightCone(light);
		CHECK(glm::length(cone.Axis) == doctest::Approx(1.0f));
		CHECK(std::cos(glm::radians(10.0f)) * cone.Scale + cone.Offset == doctest::Approx(1.0f));
		CHECK(std::cos(glm::radians(40.0f)) * cone.Scale + cone.Offset == doctest::Approx(0.0f).epsilon(1e-5));

		SpotLight zero;
		zero.Direction = glm::vec3(0.0f);
		CHECK(Internal::GetLightCone(zero).Axis == glm::vec3(0.0f, -1.0f, 0.0f));
	}
}
