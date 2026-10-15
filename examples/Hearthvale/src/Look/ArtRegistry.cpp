#include "Look/ArtRegistry.h"
#include "Farm/Animals.h"
#include "GameTuning.h"

#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <array>

namespace Dingo::Art
{
	namespace
	{
		using P = ArtPrimitive;
		using glm::vec3;
		using glm::vec4;

		constexpr std::array k_Entries = {
			ArtEntry{ FARMER,              P::Box,    vec4(0.85f, 0.45f, 0.25f, 1.0f), vec3(0.5f, 0.95f, 0.38f) },
			ArtEntry{ FARMER_HEAD,         P::Sphere, vec4(0.85f, 0.45f, 0.25f, 1.0f), vec3(0.38f) },
			ArtEntry{ FARMER_FACE,         P::Box,    vec4(0.95f, 0.78f, 0.62f, 1.0f), vec3(0.22f, 0.12f, 0.1f) },

			ArtEntry{ TARGET_TILE,         P::Box,    vec4(1.0f),                      vec3(1.0f, TARGET_MARKER_HEIGHT, 1.0f) },

			ArtEntry{ "ground_land",       P::Box,    vec4(0.36f, 0.6f, 0.28f, 1.0f),  vec3(1.0f, GROUND_TOP, 1.0f) },
			ArtEntry{ "ground_path",       P::Box,    vec4(0.66f, 0.56f, 0.4f, 1.0f),  vec3(1.0f, GROUND_TOP, 1.0f) },
			ArtEntry{ "ground_water",      P::Box,    vec4(0.25f, 0.45f, 0.7f, 1.0f),  vec3(1.0f, WATER_TOP, 1.0f) },
			ArtEntry{ "ground_rim",        P::Box,    vec4(0.27f, 0.4f, 0.22f, 1.0f),  vec3(1.0f, RIM_TOP, 1.0f) },
			ArtEntry{ SOIL,                P::Box,    vec4(0.48f, 0.34f, 0.21f, 1.0f), vec3(0.96f, 2.0f * (SOIL_TOP - GROUND_TOP), 0.96f) },
			ArtEntry{ SOIL_WATERED,        P::Box,    vec4(0.3f, 0.21f, 0.13f, 1.0f),  vec3(0.96f, 2.0f * (SOIL_TOP - GROUND_TOP), 0.96f) },

			ArtEntry{ "decor_tree",        P::Sphere, vec4(0.2f, 0.45f, 0.2f, 1.0f),   vec3(1.6f) },
			ArtEntry{ "decor_pine",        P::Box,    vec4(0.13f, 0.33f, 0.2f, 1.0f),  vec3(0.7f, 2.2f, 0.7f) },
			ArtEntry{ "decor_bush",        P::Sphere, vec4(0.25f, 0.5f, 0.22f, 1.0f),  vec3(0.9f, 0.7f, 0.9f) },
			ArtEntry{ "decor_rock",        P::Sphere, vec4(0.5f, 0.5f, 0.52f, 1.0f),   vec3(0.8f, 0.55f, 0.8f) },
			ArtEntry{ "decor_stump",       P::Box,    vec4(0.42f, 0.3f, 0.18f, 1.0f),  vec3(0.5f, 0.35f, 0.5f) },
			ArtEntry{ "decor_weed",        P::Sphere, vec4(0.4f, 0.55f, 0.2f, 1.0f),   vec3(0.35f) },
			ArtEntry{ "decor_grass",       P::Box,    vec4(0.42f, 0.66f, 0.3f, 1.0f),  vec3(0.5f, 0.12f, 0.5f) },
			ArtEntry{ "decor_flower",      P::Sphere, vec4(0.92f, 0.75f, 0.3f, 1.0f),  vec3(0.25f) },

			ArtEntry{ "build_wood_fence",  P::Box,    vec4(0.6f, 0.45f, 0.28f, 1.0f),  vec3(0.9f, 0.7f, 0.15f) },
			ArtEntry{ "build_stone_fence", P::Box,    vec4(0.6f, 0.6f, 0.6f, 1.0f),    vec3(0.9f, 0.7f, 0.25f) },
			ArtEntry{ "build_gate",        P::Box,    vec4(0.7f, 0.5f, 0.3f, 1.0f),    vec3(0.9f, 0.9f, 0.1f) },
			ArtEntry{ "build_dirt_path",   P::Box,    vec4(0.6f, 0.5f, 0.35f, 1.0f),   vec3(1.0f, 0.02f, 1.0f) },
			ArtEntry{ "build_stone_path",  P::Box,    vec4(0.62f, 0.62f, 0.6f, 1.0f),  vec3(1.0f, 0.02f, 1.0f) },
			ArtEntry{ "build_scarecrow",   P::Box,    vec4(0.85f, 0.7f, 0.35f, 1.0f),  vec3(0.4f, 1.5f, 0.4f) },
			ArtEntry{ "build_sprinkler",   P::Sphere, vec4(0.55f, 0.6f, 0.68f, 1.0f),  vec3(0.4f) },
			ArtEntry{ "build_quality_sprinkler", P::Sphere, vec4(0.85f, 0.75f, 0.3f, 1.0f), vec3(0.45f) },
			ArtEntry{ "build_chest",       P::Box,    vec4(0.55f, 0.35f, 0.18f, 1.0f), vec3(0.8f, 0.6f, 0.6f) },
			ArtEntry{ "build_lamp_post",   P::Box,    vec4(0.2f, 0.2f, 0.22f, 1.0f),   vec3(0.2f, 2.2f, 0.2f) },
			ArtEntry{ "build_bench",       P::Box,    vec4(0.55f, 0.38f, 0.22f, 1.0f), vec3(1.8f, 0.5f, 0.6f) },
			ArtEntry{ "build_flower_bed",  P::Box,    vec4(0.85f, 0.4f, 0.5f, 1.0f),   vec3(0.9f, 0.25f, 0.9f) },
			ArtEntry{ "build_well",        P::Box,    vec4(0.55f, 0.55f, 0.58f, 1.0f), vec3(0.9f, 1.0f, 0.9f) },
			ArtEntry{ "build_beehive",     P::Box,    vec4(0.9f, 0.75f, 0.3f, 1.0f),   vec3(0.6f, 0.8f, 0.6f) },
			ArtEntry{ "build_coop",        P::Box,    vec4(0.75f, 0.35f, 0.25f, 1.0f), vec3(2.8f, 2.0f, 1.8f) },
			ArtEntry{ "build_barn",        P::Box,    vec4(0.7f, 0.2f, 0.15f, 1.0f),   vec3(3.8f, 3.0f, 2.8f) },
			ArtEntry{ "build_greenhouse",  P::Box,    vec4(0.75f, 0.9f, 0.85f, 1.0f),  vec3(3.8f, 2.6f, 3.8f) },
			ArtEntry{ "build_mailbox",     P::Box,    vec4(0.3f, 0.4f, 0.75f, 1.0f),   vec3(0.3f, 1.0f, 0.3f) },
			ArtEntry{ "build_sign",        P::Box,    vec4(0.65f, 0.48f, 0.3f, 1.0f),  vec3(0.7f, 1.0f, 0.1f) },
			ArtEntry{ "build_festival_lantern", P::Sphere, vec4(1.0f, 0.6f, 0.3f, 1.0f), vec3(0.5f) },

			ArtEntry{ "landmark_farmhouse",    P::Box, vec4(0.8f, 0.62f, 0.45f, 1.0f), vec3(3.6f, 3.2f, 3.6f) },
			ArtEntry{ "landmark_shipping_bin", P::Box, vec4(0.5f, 0.32f, 0.2f, 1.0f),  vec3(1.8f, 0.8f, 0.9f) },
			ArtEntry{ "landmark_shop",         P::Box, vec4(0.85f, 0.75f, 0.55f, 1.0f), vec3(3.6f, 3.0f, 3.6f) },
			ArtEntry{ "landmark_carpenter",    P::Box, vec4(0.65f, 0.48f, 0.3f, 1.0f), vec3(3.6f, 3.0f, 3.6f) },
			ArtEntry{ "landmark_mayor_house",  P::Box, vec4(0.8f, 0.8f, 0.78f, 1.0f),  vec3(3.6f, 3.4f, 3.6f) },
			ArtEntry{ "landmark_cottage",      P::Box, vec4(0.85f, 0.7f, 0.6f, 1.0f),  vec3(3.6f, 2.8f, 3.6f) },
			ArtEntry{ "landmark_notice_board", P::Box, vec4(0.55f, 0.4f, 0.25f, 1.0f), vec3(1.8f, 1.6f, 0.2f) },

			ArtEntry{ LAMP_BULB,           P::Sphere, vec4(0.9f, 0.8f, 0.6f, 1.0f),    vec3(0.22f) },
			ArtEntry{ WINDOW,              P::Box,    vec4(0.16f, 0.2f, 0.28f, 1.0f),  vec3(0.55f, 0.6f, 0.06f) },

			ArtEntry{ "villager_mira",     P::Box,    vec4(0.82f, 0.35f, 0.3f, 1.0f),  vec3(0.5f, 0.9f, 0.36f) },
			ArtEntry{ "villager_bram",     P::Box,    vec4(0.3f, 0.45f, 0.7f, 1.0f),   vec3(0.56f, 0.98f, 0.42f) },
			ArtEntry{ "villager_odette",   P::Box,    vec4(0.55f, 0.32f, 0.6f, 1.0f),  vec3(0.54f, 1.0f, 0.42f) },
			ArtEntry{ "villager_pip",      P::Box,    vec4(0.95f, 0.8f, 0.25f, 1.0f),  vec3(0.4f, 0.62f, 0.3f) },
			ArtEntry{ "villager_head",     P::Sphere, vec4(0.95f, 0.78f, 0.62f, 1.0f), vec3(0.34f) },

			ArtEntry{ "animal_chicken",      P::Sphere, vec4(0.97f, 0.95f, 0.9f, 1.0f),  vec3(0.3f, 0.28f, 0.4f) },
			ArtEntry{ "animal_chicken_head", P::Sphere, vec4(0.9f, 0.25f, 0.2f, 1.0f),   vec3(0.14f) },
			ArtEntry{ "animal_cow",          P::Box,    vec4(0.92f, 0.9f, 0.85f, 1.0f),  vec3(0.6f, 0.55f, 1.0f) },
			ArtEntry{ "animal_cow_head",     P::Box,    vec4(0.35f, 0.28f, 0.24f, 1.0f), vec3(0.32f, 0.3f, 0.34f) },
			ArtEntry{ "produce_egg",         P::Sphere, vec4(0.98f, 0.93f, 0.8f, 1.0f),  vec3(0.14f, 0.18f, 0.14f) },
			ArtEntry{ "produce_milk",        P::Box,    vec4(0.9f, 0.95f, 1.0f, 1.0f),   vec3(0.16f, 0.2f, 0.16f) },

			ArtEntry{ "crop_seed",         P::Sphere, vec4(0.4f, 0.3f, 0.18f, 1.0f),   vec3(0.15f) },
			ArtEntry{ "crop_sprout",       P::Sphere, vec4(0.45f, 0.72f, 0.3f, 1.0f),  vec3(0.25f) },
			ArtEntry{ "crop_young",        P::Sphere, vec4(0.3f, 0.6f, 0.25f, 1.0f),   vec3(0.45f) },
			ArtEntry{ "crop_parsnip",      P::Sphere, vec4(0.93f, 0.88f, 0.65f, 1.0f), vec3(0.55f) },
			ArtEntry{ "crop_radish",       P::Sphere, vec4(0.85f, 0.2f, 0.3f, 1.0f),   vec3(0.5f) },
			ArtEntry{ "crop_strawberry",   P::Sphere, vec4(0.9f, 0.15f, 0.2f, 1.0f),   vec3(0.5f) },
			ArtEntry{ "crop_tomato",       P::Sphere, vec4(0.92f, 0.3f, 0.18f, 1.0f),  vec3(0.6f) },
			ArtEntry{ "crop_melon",        P::Sphere, vec4(0.55f, 0.8f, 0.35f, 1.0f),  vec3(0.75f) },
			ArtEntry{ "crop_sunflower",    P::Sphere, vec4(0.98f, 0.82f, 0.15f, 1.0f), vec3(0.6f, 1.2f, 0.6f) },
			ArtEntry{ "crop_pumpkin",      P::Sphere, vec4(0.95f, 0.55f, 0.12f, 1.0f), vec3(0.8f, 0.6f, 0.8f) },
			ArtEntry{ "crop_cabbage",      P::Sphere, vec4(0.6f, 0.82f, 0.5f, 1.0f),   vec3(0.6f) },
			ArtEntry{ "crop_beetroot",     P::Sphere, vec4(0.55f, 0.1f, 0.25f, 1.0f),  vec3(0.5f) },
			ArtEntry{ "crop_kale",         P::Sphere, vec4(0.2f, 0.45f, 0.3f, 1.0f),   vec3(0.6f) },
			ArtEntry{ "crop_leek",         P::Box,    vec4(0.75f, 0.9f, 0.6f, 1.0f),   vec3(0.2f, 0.7f, 0.2f) },
			ArtEntry{ "crop_turnip",       P::Sphere, vec4(0.88f, 0.75f, 0.9f, 1.0f),  vec3(0.5f) },
		};

		constexpr std::array<std::string_view, static_cast<size_t>(GroundLook::Count)> k_Ground = {
			"ground_land", "ground_path", "ground_water", "ground_rim",
		};

		constexpr std::array<std::string_view, DECOR_KIND_COUNT> k_Decor = {
			"decor_tree", "decor_pine", "decor_bush", "decor_rock", "decor_stump", "decor_weed", "decor_grass", "decor_flower",
		};

		constexpr std::array<std::string_view, BUILD_COUNT> k_Builds = {
			"build_wood_fence", "build_stone_fence", "build_gate", "build_dirt_path", "build_stone_path",
			"build_scarecrow", "build_sprinkler", "build_quality_sprinkler", "build_chest", "build_lamp_post",
			"build_bench", "build_flower_bed", "build_well", "build_beehive", "build_coop",
			"build_barn", "build_greenhouse", "build_mailbox", "build_sign", "build_festival_lantern",
		};

		constexpr std::array<std::string_view, LANDMARK_COUNT> k_Landmarks = {
			"landmark_farmhouse", "landmark_shipping_bin", "landmark_shop", "landmark_carpenter",
			"landmark_mayor_house", "landmark_cottage", "landmark_notice_board",
		};

		constexpr std::array<std::string_view, CROP_STAGES - 1> k_CropStages = { "crop_seed", "crop_sprout", "crop_young" };

		constexpr std::array<std::string_view, CROP_COUNT> k_CropsRipe = {
			"crop_parsnip", "crop_radish", "crop_strawberry", "crop_tomato", "crop_melon", "crop_sunflower",
			"crop_pumpkin", "crop_cabbage", "crop_beetroot", "crop_kale", "crop_leek", "crop_turnip",
		};

		constexpr std::array<std::string_view, VILLAGER_COUNT> k_Villagers = {
			"villager_mira", "villager_bram", "villager_odette", "villager_pip",
		};

		template<typename Table>
		std::string_view Lookup(const Table& table, size_t index)
		{
			return index < table.size() ? table[index] : std::string_view();
		}
	}

	std::span<const ArtEntry> All()
	{
		return k_Entries;
	}

	const ArtEntry* Find(std::string_view id)
	{
		for (const ArtEntry& entry : k_Entries)
		{
			if (entry.Id == id)
				return &entry;
		}
		DE_WARN("Hearthvale: no art registered for '{}'", id);
		return nullptr;
	}

	std::string_view ForGround(GroundLook look) { return Lookup(k_Ground, static_cast<size_t>(look)); }
	std::string_view ForDecor(DecorKind kind) { return Lookup(k_Decor, static_cast<size_t>(kind)); }
	std::string_view ForBuild(BuildId id) { return Lookup(k_Builds, static_cast<size_t>(id)); }
	std::string_view ForLandmark(Landmark id) { return Lookup(k_Landmarks, static_cast<size_t>(id)); }

	std::string_view ForCrop(CropId crop, int stage)
	{
		if (stage < CROP_STAGES - 1)
			return Lookup(k_CropStages, static_cast<size_t>((std::max)(stage, 0)));
		return Lookup(k_CropsRipe, static_cast<size_t>(crop));
	}

	std::string_view ForVillager(VillagerId villager) { return Lookup(k_Villagers, static_cast<size_t>(villager)); }
	std::string_view ForVillagerHead() { return "villager_head"; }
	std::string_view ForAnimal(AnimalKind kind) { return kind == AnimalKind::Cow ? "animal_cow" : "animal_chicken"; }
	std::string_view ForAnimalHead(AnimalKind kind) { return kind == AnimalKind::Cow ? "animal_cow_head" : "animal_chicken_head"; }
	std::string_view ForProduce(AnimalKind kind) { return kind == AnimalKind::Cow ? "produce_milk" : "produce_egg"; }

	Mesh* GetMesh(const ArtEntry& entry)
	{
		Renderer3D& renderer = Application::Get().GetRenderer3D();
		return entry.Primitive == ArtPrimitive::Sphere ? renderer.GetSphereMesh() : renderer.GetBoxMesh();
	}

	Entity Spawn(Scene& scene, std::string_view id, const std::string& name, const glm::vec3& position, int quarterTurns, float scale)
	{
		const ArtEntry* entry = Find(id);
		if (!entry || Application::Get().IsHeadless())
			return {};

		Entity entity = scene.CreateEntity(name);
		auto& transform = entity.AddComponent<Transform3DComponent>();
		transform.Position = position;
		transform.Scale = entry->Size * scale;
		if (quarterTurns % 4 != 0)
			transform.Rotation = glm::angleAxis(glm::half_pi<float>() * static_cast<float>(quarterTurns % 4), glm::vec3(0.0f, 1.0f, 0.0f));
		entity.AddComponent<MeshRendererComponent>(MeshRendererComponent(GetMesh(*entry), entry->Color));
		return entity;
	}

	Entity SpawnOnGround(Scene& scene, std::string_view id, const std::string& name, float x, float z, float groundY, int quarterTurns, float scale)
	{
		const ArtEntry* entry = Find(id);
		if (!entry)
			return {};
		return Spawn(scene, id, name, glm::vec3(x, groundY + 0.5f * entry->Size.y * scale, z), quarterTurns, scale);
	}

}
