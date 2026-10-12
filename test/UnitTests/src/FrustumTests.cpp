#include <doctest/doctest.h>

#include <DingoEngine/Graphics/Frustum.h>

#include <glm/gtc/matrix_transform.hpp>

using namespace Dingo;

namespace
{
	constexpr float k_Epsilon = 1e-5f;

	bool Near(const glm::vec3& a, const glm::vec3& b, float epsilon = k_Epsilon)
	{
		return glm::all(glm::lessThanEqual(glm::abs(a - b), glm::vec3(epsilon)));
	}

	Frustum CameraLookingDownMinusZ()
	{
		const glm::mat4 projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 100.0f);
		const glm::mat4 view = glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		return Frustum::FromViewProjection(projection * view);
	}
}

TEST_SUITE("AABB")
{
	TEST_CASE("a default box is empty and stays empty when transformed")
	{
		const AABB box;
		CHECK(box.IsEmpty());
		CHECK(box.Transformed(glm::translate(glm::mat4(1.0f), glm::vec3(5.0f))).IsEmpty());
	}

	TEST_CASE("center and extents")
	{
		const AABB box({ -1.0f, 0.0f, 2.0f }, { 3.0f, 4.0f, 6.0f });
		CHECK(Near(box.GetCenter(), { 1.0f, 2.0f, 4.0f }));
		CHECK(Near(box.GetExtents(), { 2.0f, 2.0f, 2.0f }));
	}

	TEST_CASE("a translation moves the box")
	{
		const AABB box = AABB({ 0.0f, 0.0f, 0.0f }, { 1.0f, 2.0f, 3.0f }).Transformed(glm::translate(glm::mat4(1.0f), glm::vec3(10.0f, -1.0f, 0.5f)));
		CHECK(Near(box.Min, { 10.0f, -1.0f, 0.5f }));
		CHECK(Near(box.Max, { 11.0f, 1.0f, 3.5f }));
	}

	TEST_CASE("a quarter turn about y swaps x and z exactly")
	{
		const AABB box = AABB({ 0.0f, 0.0f, 0.0f }, { 1.0f, 2.0f, 3.0f }).Transformed(glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f)));
		CHECK(Near(box.Min, { 0.0f, 0.0f, -1.0f }));
		CHECK(Near(box.Max, { 3.0f, 2.0f, 0.0f }));
	}

	TEST_CASE("an eighth turn widens a cube by sqrt 2, and scale scales it")
	{
		const glm::mat4 transform = glm::scale(glm::rotate(glm::mat4(1.0f), glm::radians(45.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::vec3(2.0f));
		const AABB box = AABB(glm::vec3(-0.5f), glm::vec3(0.5f)).Transformed(transform);
		const float half = std::sqrt(2.0f);
		CHECK(Near(box.Min, { -half, -1.0f, -half }));
		CHECK(Near(box.Max, { half, 1.0f, half }));
	}
}

TEST_SUITE("Frustum")
{
	TEST_CASE("planes are unit length and point inward")
	{
		const Frustum frustum = CameraLookingDownMinusZ();
		for (const glm::vec4& plane : frustum.GetPlanes())
		{
			CHECK(glm::length(glm::vec3(plane)) == doctest::Approx(1.0f).epsilon(1e-5));
			CHECK(glm::dot(glm::vec3(plane), glm::vec3(0.0f, 0.0f, -10.0f)) + plane.w > 0.0f);
		}
		CHECK(frustum.GetPlane(Frustum::Near).z < 0.0f);
		CHECK(frustum.GetPlane(Frustum::Far).z > 0.0f);
	}

	TEST_CASE("near and far planes sit at the projection's depths")
	{
		const Frustum frustum = CameraLookingDownMinusZ();
		const glm::vec4 nearPlane = frustum.GetPlane(Frustum::Near);
		const glm::vec4 farPlane = frustum.GetPlane(Frustum::Far);
		CHECK(glm::dot(glm::vec3(nearPlane), glm::vec3(0.0f, 0.0f, -0.1f)) + nearPlane.w == doctest::Approx(0.0f).epsilon(1e-4));
		CHECK(glm::dot(glm::vec3(farPlane), glm::vec3(0.0f, 0.0f, -100.0f)) + farPlane.w == doctest::Approx(0.0f).epsilon(1e-3));
	}

	TEST_CASE("boxes inside, outside and straddling")
	{
		const Frustum frustum = CameraLookingDownMinusZ();
		CHECK(frustum.Intersects(AABB({ -1.0f, -1.0f, -11.0f }, { 1.0f, 1.0f, -9.0f })));
		CHECK_FALSE(frustum.Intersects(AABB({ -1.0f, -1.0f, 9.0f }, { 1.0f, 1.0f, 11.0f })));
		CHECK_FALSE(frustum.Intersects(AABB({ -1.0f, -1.0f, -201.0f }, { 1.0f, 1.0f, -150.0f })));
		CHECK_FALSE(frustum.Intersects(AABB({ 30.0f, -1.0f, -11.0f }, { 32.0f, 1.0f, -9.0f })));
		CHECK(frustum.Intersects(AABB({ 9.0f, -1.0f, -11.0f }, { 12.0f, 1.0f, -9.0f })));
		CHECK(frustum.Intersects(AABB({ -1.0f, -1.0f, -101.0f }, { 1.0f, 1.0f, -99.0f })));
	}

	TEST_CASE("spheres and how far outside they are")
	{
		const Frustum frustum = CameraLookingDownMinusZ();
		CHECK(frustum.Intersects(glm::vec3(0.0f, 0.0f, -10.0f), 1.0f));
		CHECK(frustum.DistanceOutside(glm::vec3(0.0f, 0.0f, -10.0f), 1.0f) <= 0.0f);

		CHECK_FALSE(frustum.Intersects(glm::vec3(0.0f, 0.0f, -110.0f), 5.0f));
		CHECK(frustum.DistanceOutside(glm::vec3(0.0f, 0.0f, -110.0f), 5.0f) == doctest::Approx(5.0f).epsilon(1e-3));

		CHECK(frustum.Intersects(glm::vec3(0.0f, 0.0f, -110.0f), 10.5f));
	}

	TEST_CASE("an orthographic frustum is a box")
	{
		const Frustum frustum = Frustum::FromViewProjection(glm::ortho(-2.0f, 2.0f, -1.0f, 1.0f, 0.0f, 10.0f));
		CHECK(frustum.Intersects(AABB({ 1.9f, 0.9f, -9.9f }, { 3.0f, 3.0f, -9.0f })));
		CHECK_FALSE(frustum.Intersects(AABB({ 2.1f, -0.5f, -5.0f }, { 3.0f, 0.5f, -4.0f })));
		CHECK_FALSE(frustum.Intersects(AABB({ -0.5f, 1.1f, -5.0f }, { 0.5f, 2.0f, -4.0f })));
		CHECK_FALSE(frustum.Intersects(AABB({ -0.5f, -0.5f, -12.0f }, { 0.5f, 0.5f, -10.5f })));
	}
}
