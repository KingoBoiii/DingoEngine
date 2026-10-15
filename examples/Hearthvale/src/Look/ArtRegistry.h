#pragma once
#include <DingoEngine.h>

#include <glm/glm.hpp>

#include <string>
#include <string_view>

namespace Dingo
{

	enum class ArtPrimitive : uint8_t { Box, Sphere };

	// Until P14 every entry is a primitive; ModelPath, when set, wins over Primitive.
	struct ArtEntry
	{
		std::string_view Id;
		ArtPrimitive Primitive = ArtPrimitive::Box;
		glm::vec4 Color{ 1.0f };
		glm::vec3 Size{ 1.0f };
		std::string_view ModelPath;
	};

	namespace Art
	{
		inline constexpr std::string_view GROUND = "ground";
		inline constexpr std::string_view FARMER = "farmer";
		inline constexpr std::string_view TREE   = "tree";
		inline constexpr std::string_view ROCK   = "rock";

		const ArtEntry* Find(std::string_view id);

		// Null headless (no renderer) or for an unknown id; model entries are not loaded yet.
		Entity Spawn(Scene& scene, std::string_view id, const std::string& name, const glm::vec3& position);
	}

}
