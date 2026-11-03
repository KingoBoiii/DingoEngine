#include <doctest/doctest.h>

#include <DingoEngine/Scene/Components.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cstdint>

using namespace Dingo;

TEST_SUITE("Transform3DComponent")
{
	TEST_CASE("GetTransform equals translate * rotate * scale for finite values")
	{
		uint32_t state = 12345u;
		auto next = [&state](float low, float high)
		{
			state = state * 1664525u + 1013904223u;
			return low + (high - low) * static_cast<float>(state >> 8) / static_cast<float>(1u << 24);
		};

		for (int sample = 0; sample < 2000; ++sample)
		{
			float values[10];
			for (int i = 0; i < 10; ++i)
				values[i] = i < 3 ? next(-1000.0f, 1000.0f) : i < 7 ? next(-1.0f, 1.0f) : next(-4.0f, 4.0f);

			Transform3DComponent transform;
			transform.Position = glm::vec3(values[0], values[1], values[2]);
			transform.Rotation = glm::normalize(glm::quat(values[3], values[4], values[5], values[6]));
			transform.Scale = glm::vec3(values[7], values[8], values[9]);
			if (sample % 7 == 0)
				transform.Scale.y = 0.0f;
			if (sample % 11 == 0)
				transform.Rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

			const glm::mat4 expected = glm::translate(glm::mat4(1.0f), transform.Position)
				* glm::mat4_cast(transform.Rotation)
				* glm::scale(glm::mat4(1.0f), transform.Scale);
			const glm::mat4 actual = transform.GetTransform();
			for (int column = 0; column < 4; ++column)
			{
				for (int row = 0; row < 4; ++row)
					REQUIRE(actual[column][row] == expected[column][row]);
			}
		}
	}
}
