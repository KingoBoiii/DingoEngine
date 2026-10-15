#pragma once
#include <DingoEngine.h>

#include "Defs/BuildDefs.h"
#include "Defs/CropDefs.h"
#include "Defs/DecorDefs.h"
#include "Defs/VillagerDefs.h"
#include "World/ValleyLayout.h"
#include "World/WorldCells.h"

#include <glm/glm.hpp>

#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Dingo
{

	enum class AnimalKind : uint8_t;

	enum class ArtPrimitive : uint8_t { Box, Sphere };

	// Contain scales the model uniformly to fit inside Size; Stretch scales each axis to fill it.
	enum class ArtFit : uint8_t { Contain, Stretch };

	// One model inside a composite: the box it fills (in the composite's axes, after YawDegrees) and where that
	// box's bottom centre sits relative to the composite's bottom centre.
	struct ArtPart
	{
		std::string_view ModelPath;
		glm::vec3 Size{ 1.0f };
		glm::vec3 Offset{ 0.0f };
		float YawDegrees = 0.0f;
		ArtFit Fit = ArtFit::Contain;
		glm::vec4 Tint{ 1.0f };
	};

	// Size is the box the entry occupies (its centre is the spawn position). ModelPath, when set and loadable, wins
	// over Primitive and Color; Parts, when set, replace ModelPath. Tint multiplies the model's colours.
	struct ArtEntry
	{
		std::string_view Id;
		ArtPrimitive Primitive = ArtPrimitive::Box;
		glm::vec4 Color{ 1.0f };
		glm::vec3 Size{ 1.0f };
		std::string_view ModelPath;
		float YawDegrees = 0.0f;
		ArtFit Fit = ArtFit::Contain;
		glm::vec4 Tint{ 1.0f };
		std::span<const ArtPart> Parts;
	};

	// What a villager is drawn with: a KayKit character, a tint over its texture, and its standing height.
	struct VillagerLook
	{
		std::string_view ModelPath;
		std::string_view TexturePath;
		glm::vec4 Tint{ 1.0f };
		float Height = 1.0f;
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

		// The glowing parts NightLights places: a lamp's bulb and a house's window pane.
		inline constexpr std::string_view LAMP_BULB    = "lamp_bulb";
		inline constexpr std::string_view WINDOW       = "window";

		// Every registered entry, in table order.
		std::span<const ArtEntry> All();

		const ArtEntry* Find(std::string_view id);

		std::string_view ForGround(GroundLook look);
		std::string_view ForDecor(DecorKind kind);
		std::string_view ForBuild(BuildId id);
		std::string_view ForLandmark(Landmark id);
		std::string_view ForCrop(CropId crop, int stage);
		std::string_view ForVillager(VillagerId villager);
		std::string_view ForVillagerHead();
		std::string_view ForAnimal(AnimalKind kind);
		std::string_view ForAnimalHead(AnimalKind kind);
		std::string_view ForProduce(AnimalKind kind);

		Mesh* GetMesh(const ArtEntry& entry);

		// The paths of every model an entry uses (one for a single model, one per part for a composite).
		std::vector<std::string_view> ModelPathsOf(const ArtEntry& entry);

		VillagerLook ForVillagerCharacter(VillagerId villager);

		// Loads the entry's models and returns how many failed (0 when all load, or the entry is a primitive).
		// GPU side: never headless.
		int LoadModels(const ArtEntry& entry);
		// Forgets the cached model bounds; the AssetManager keeps the models. Call when the scene is torn down.
		void ReleaseModels();

		// Calls fn for the entity and each descendant that draws a mesh.
		void ForEachMesh(Entity root, const std::function<void(MeshRendererComponent&)>& fn);

		// Null headless (no renderer) or for an unknown id. The entity is a pivot at `position` (the centre of the
		// entry's Size box) scaled by `scale`; the meshes hang under it, so use ForEachMesh to reach them.
		Entity Spawn(Scene& scene, std::string_view id, const std::string& name, const glm::vec3& position, int quarterTurns = 0, float scale = 1.0f);
		// As Spawn, standing on `groundY` at (x, z).
		Entity SpawnOnGround(Scene& scene, std::string_view id, const std::string& name, float x, float z, float groundY, int quarterTurns = 0, float scale = 1.0f);
	}

}
