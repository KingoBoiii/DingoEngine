#include <doctest/doctest.h>

#include <DingoEngine/Core/Ray.h>

#include <glm/gtc/matrix_transform.hpp>

using namespace Dingo;

namespace
{
	const glm::vec2 k_Viewport = { 1280.0f, 720.0f };

	float DistanceToRay(const Ray& ray, const glm::vec3& point)
	{
		const glm::vec3 toPoint = point - ray.Origin;
		return glm::length(toPoint - ray.Direction * glm::dot(toPoint, ray.Direction));
	}
}

TEST_SUITE("WorldToScreenPoint")
{
	TEST_CASE("round trip through ScreenPointToRay")
	{
		const glm::mat4 view = glm::lookAt(glm::vec3(2.0f, 3.0f, 8.0f), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		const glm::mat4 projection = glm::perspective(glm::radians(60.0f), k_Viewport.x / k_Viewport.y, 0.1f, 100.0f);

		for (float x = -3.0f; x <= 3.0f; x += 1.5f)
		{
			for (float y = -2.0f; y <= 2.0f; y += 1.0f)
			{
				const glm::vec3 point = { x, y, -1.0f };
				CAPTURE(point.x);
				CAPTURE(point.y);
				const std::optional<glm::vec2> screen = WorldToScreenPoint(point, k_Viewport, view, projection);
				REQUIRE(screen.has_value());
				CHECK(DistanceToRay(ScreenPointToRay(*screen, k_Viewport, view, projection), point) < 1e-3f);
			}
		}
	}

	TEST_CASE("orthographic round trip")
	{
		const glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.0f, 5.0f), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		const glm::mat4 projection = glm::ortho(-8.0f, 8.0f, -4.5f, 4.5f, 0.1f, 100.0f);
		const glm::vec3 point = { 3.0f, -2.0f, 1.0f };
		const std::optional<glm::vec2> screen = WorldToScreenPoint(point, k_Viewport, view, projection);
		REQUIRE(screen.has_value());
		CHECK(screen->x == doctest::Approx(k_Viewport.x * (3.0f + 8.0f) / 16.0f));
		CHECK(screen->y == doctest::Approx(k_Viewport.y * (4.5f + 2.0f) / 9.0f));
		CHECK(DistanceToRay(ScreenPointToRay(*screen, k_Viewport, view, projection), point) < 1e-3f);
	}

	TEST_CASE("top-left origin, +Y down")
	{
		const glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.0f, 5.0f), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		const glm::mat4 projection = glm::perspective(glm::radians(45.0f), k_Viewport.x / k_Viewport.y, 0.1f, 100.0f);
		const std::optional<glm::vec2> center = WorldToScreenPoint(glm::vec3(0.0f), k_Viewport, view, projection);
		REQUIRE(center.has_value());
		CHECK(center->x == doctest::Approx(640.0f));
		CHECK(center->y == doctest::Approx(360.0f));

		const std::optional<glm::vec2> upLeft = WorldToScreenPoint(glm::vec3(-1.0f, 1.0f, 0.0f), k_Viewport, view, projection);
		REQUIRE(upLeft.has_value());
		CHECK(upLeft->x < 640.0f);
		CHECK(upLeft->y < 360.0f);
	}

	TEST_CASE("behind or on the camera plane has no screen position")
	{
		const glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.0f, 5.0f), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		const glm::mat4 projection = glm::perspective(glm::radians(45.0f), k_Viewport.x / k_Viewport.y, 0.1f, 100.0f);
		CHECK_FALSE(WorldToScreenPoint(glm::vec3(0.0f, 0.0f, 10.0f), k_Viewport, view, projection).has_value());
		CHECK_FALSE(WorldToScreenPoint(glm::vec3(1.0f, 0.0f, 5.0f), k_Viewport, view, projection).has_value());
	}
}
