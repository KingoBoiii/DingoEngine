#pragma once
#include <DingoEngine.h>

#include "Defs/BuildDefs.h"
#include "Defs/CropDefs.h"
#include "Defs/DecorDefs.h"
#include "World/ValleyLayout.h"
#include "World/WorldCells.h"

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
		inline constexpr std::string_view FARMER       = "farmer";
		inline constexpr std::string_view FARMER_HEAD  = "farmer_head";
		inline constexpr std::string_view FARMER_FACE  = "farmer_face";
		// One tile wide and flat; the build ghost scales it to a footprint.
		inline constexpr std::string_view TARGET_TILE  = "target_tile";
		// The two must share primitive (or model) and size: watering changes only the colour.
		inline constexpr std::string_view SOIL         = "soil";
		inline constexpr std::string_view SOIL_WATERED = "soil_watered";

		const ArtEntry* Find(std::string_view id);

		std::string_view ForGround(GroundLook look);
		std::string_view ForDecor(DecorKind kind);
		std::string_view ForBuild(BuildId id);
		std::string_view ForLandmark(Landmark id);
		std::string_view ForCrop(CropId crop, int stage);

		Mesh* GetMesh(const ArtEntry& entry);

		// Null headless (no renderer) or for an unknown id; model entries are not loaded yet.
		// `position` is the mesh's centre; the entry's Size times `scale` is its scale.
		Entity Spawn(Scene& scene, std::string_view id, const std::string& name, const glm::vec3& position, int quarterTurns = 0, float scale = 1.0f);
		// As Spawn, standing on `groundY` at (x, z).
		Entity SpawnOnGround(Scene& scene, std::string_view id, const std::string& name, float x, float z, float groundY, int quarterTurns = 0, float scale = 1.0f);
	}

}
