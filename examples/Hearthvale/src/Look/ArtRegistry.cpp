#include "Look/ArtRegistry.h"
#include "Farm/Animals.h"
#include "GameTuning.h"

#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <deque>
#include <unordered_map>

namespace Dingo::Art
{
	namespace
	{
		using P = ArtPrimitive;
		using glm::vec3;
		using glm::vec4;

		constexpr std::string_view k_Nature = "kenney/nature-kit/Models/";
		constexpr std::string_view k_Town = "kenney/fantasy-town-kit/Models/GLB format/";
		constexpr std::string_view k_Survival = "kenney/survival-kit/Models/GLB format/";
		constexpr std::string_view k_Pets = "kenney/cube-pets/Models/GLB format/";

		std::deque<std::string>& Paths()
		{
			static std::deque<std::string> paths;
			return paths;
		}

		std::string_view Path(std::string_view dir, std::string_view file)
		{
			return Paths().emplace_back(std::string(dir) + std::string(file) + ".glb");
		}

		std::deque<std::vector<ArtPart>>& PartLists()
		{
			static std::deque<std::vector<ArtPart>> lists;
			return lists;
		}

		std::span<const ArtPart> Keep(std::vector<ArtPart> parts)
		{
			return PartLists().emplace_back(std::move(parts));
		}

		ArtEntry ModelEntry(std::string_view id, P primitive, vec4 color, vec3 size, std::string_view path, float yaw = 0.0f, ArtFit fit = ArtFit::Contain, vec4 tint = vec4(1.0f))
		{
			return ArtEntry{ id, primitive, color, size, path, yaw, fit, tint, {} };
		}

		ArtEntry Composite(std::string_view id, P primitive, vec4 color, vec3 size, std::vector<ArtPart> parts)
		{
			return ArtEntry{ id, primitive, color, size, {}, 0.0f, ArtFit::Contain, vec4(1.0f), Keep(std::move(parts)) };
		}

		ArtPart Part(std::string_view path, vec3 size, vec3 offset = vec3(0.0f), float yaw = 0.0f, ArtFit fit = ArtFit::Stretch, vec4 tint = vec4(1.0f))
		{
			return ArtPart{ path, size, offset, yaw, fit, tint };
		}

		std::vector<ArtPart> HouseParts(Landmark landmark, float wallHeight, float roofHeight, bool wood, vec4 wallTint, vec4 roofTint)
		{
			const LandmarkSpec& spec = GetLandmarkSpec(landmark);
			const bool south = spec.DoorY >= spec.Footprint.Bottom();
			const float centreX = static_cast<float>(spec.Footprint.X) + 0.5f * static_cast<float>(spec.Footprint.W);
			const float doorX = (static_cast<float>(spec.DoorX) + 0.5f - centreX) * TILE_SIZE;
			const float face = south ? 1.74f : -1.74f;
			const float doorYaw = south ? -90.0f : 90.0f;

			return {
				Part(Path(k_Town, wood ? "wall-wood-block" : "wall-block"), vec3(3.4f, wallHeight, 3.4f), vec3(0.0f), 0.0f, ArtFit::Stretch, wallTint),
				Part(Path(k_Town, "roof-high-point"), vec3(3.8f, roofHeight, 3.8f), vec3(0.0f, wallHeight, 0.0f), 0.0f, ArtFit::Stretch, roofTint),
				Part(Path(k_Town, wood ? "wall-wood-door" : "wall-door"), vec3(1.0f, 1.7f, 0.08f), vec3(doorX, 0.0f, face), doorYaw),
				Part(Path(k_Town, wood ? "wall-wood-window-glass" : "wall-window-glass"), vec3(0.08f, 0.8f, 0.9f), vec3(1.74f, 1.0f, 0.0f), 0.0f),
				Part(Path(k_Town, wood ? "wall-wood-window-glass" : "wall-window-glass"), vec3(0.08f, 0.8f, 0.9f), vec3(-1.74f, 1.0f, 0.0f), 180.0f),
			};
		}

		std::vector<ArtEntry> MakeEntries()
		{
			const vec4 white(1.0f);
			std::vector<ArtEntry> entries = {
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
				ArtEntry{ "build_scarecrow",   P::Box,    vec4(0.85f, 0.7f, 0.35f, 1.0f),  vec3(0.4f, 1.5f, 0.4f) },
				ArtEntry{ LAMP_BULB,           P::Sphere, vec4(0.9f, 0.8f, 0.6f, 1.0f),    vec3(0.22f) },
				ArtEntry{ WINDOW,              P::Box,    vec4(0.16f, 0.2f, 0.28f, 1.0f),  vec3(0.55f, 0.6f, 0.06f) },
				ArtEntry{ "produce_egg",         P::Sphere, vec4(0.98f, 0.93f, 0.8f, 1.0f),  vec3(0.14f, 0.18f, 0.14f) },
				ArtEntry{ "produce_milk",        P::Box,    vec4(0.9f, 0.95f, 1.0f, 1.0f),   vec3(0.16f, 0.2f, 0.16f) },
				ModelEntry("decor_tree", P::Sphere, vec4(0.2f, 0.45f, 0.2f, 1.0f), vec3(1.6f, 2.2f, 1.6f), Path(k_Nature, "tree_oak"), 0.0f, ArtFit::Contain, vec4(1.0f, 0.9f, 0.55f, 1.0f)),
				ModelEntry("decor_pine", P::Box, vec4(0.13f, 0.33f, 0.2f, 1.0f), vec3(0.7f, 2.4f, 0.7f), Path(k_Nature, "tree_pineTallA"), 0.0f, ArtFit::Contain, vec4(1.0f, 0.9f, 0.55f, 1.0f)),
				ModelEntry("decor_bush", P::Sphere, vec4(0.25f, 0.5f, 0.22f, 1.0f), vec3(0.9f, 0.7f, 0.9f), Path(k_Nature, "plant_bush"), 0.0f, ArtFit::Contain, vec4(1.0f, 0.9f, 0.55f, 1.0f)),
				ModelEntry("decor_rock", P::Sphere, vec4(0.5f, 0.5f, 0.52f, 1.0f), vec3(0.9f, 0.6f, 0.9f), Path(k_Nature, "rock_largeA"), 0.0f, ArtFit::Contain, white),
				ModelEntry("decor_stump", P::Box, vec4(0.42f, 0.3f, 0.18f, 1.0f), vec3(0.5f, 0.35f, 0.5f), Path(k_Nature, "stump_round"), 0.0f, ArtFit::Contain, white),
				ModelEntry("decor_weed", P::Sphere, vec4(0.4f, 0.55f, 0.2f, 1.0f), vec3(0.4f), Path(k_Nature, "plant_flatTall"), 0.0f, ArtFit::Contain, vec4(1.0f, 0.9f, 0.55f, 1.0f)),
				ModelEntry("decor_grass", P::Box, vec4(0.42f, 0.66f, 0.3f, 1.0f), vec3(0.5f, 0.35f, 0.5f), Path(k_Nature, "grass_large"), 0.0f, ArtFit::Contain, vec4(1.0f, 0.9f, 0.55f, 1.0f)),
				ModelEntry("decor_flower", P::Sphere, vec4(0.92f, 0.75f, 0.3f, 1.0f), vec3(0.3f, 0.45f, 0.3f), Path(k_Nature, "flower_redA"), 0.0f, ArtFit::Contain, white),
				ModelEntry("build_wood_fence", P::Box, vec4(0.6f, 0.45f, 0.28f, 1.0f), vec3(0.95f, 0.6f, 0.12f), Path(k_Nature, "fence_simple"), 0.0f, ArtFit::Stretch, white),
				ModelEntry("build_stone_fence", P::Box, vec4(0.6f, 0.6f, 0.6f, 1.0f), vec3(0.95f, 0.7f, 0.2f), Path(k_Survival, "fence-fortified"), 0.0f, ArtFit::Stretch, white),
				ModelEntry("build_gate", P::Box, vec4(0.7f, 0.5f, 0.3f, 1.0f), vec3(0.95f, 0.7f, 0.12f), Path(k_Nature, "fence_gate"), 0.0f, ArtFit::Stretch, white),
				ModelEntry("build_dirt_path", P::Box, vec4(0.6f, 0.5f, 0.35f, 1.0f), vec3(1.0f, 0.04f, 1.0f), Path(k_Nature, "ground_pathTile"), 0.0f, ArtFit::Stretch, white),
				ModelEntry("build_stone_path", P::Box, vec4(0.62f, 0.62f, 0.6f, 1.0f), vec3(1.0f, 0.04f, 1.0f), Path(k_Nature, "path_stone"), 0.0f, ArtFit::Stretch, white),
				ModelEntry("build_sprinkler", P::Sphere, vec4(0.55f, 0.6f, 0.68f, 1.0f), vec3(0.4f, 0.5f, 0.4f), Path(k_Survival, "bucket"), 0.0f, ArtFit::Contain, vec4(0.7f, 0.85f, 1.0f, 1.0f)),
				ModelEntry("build_quality_sprinkler", P::Sphere, vec4(0.85f, 0.75f, 0.3f, 1.0f), vec3(0.55f, 0.4f, 0.55f), Path(k_Nature, "pot_large"), 0.0f, ArtFit::Contain, vec4(1.0f, 0.85f, 0.4f, 1.0f)),
				ModelEntry("build_chest", P::Box, vec4(0.55f, 0.35f, 0.18f, 1.0f), vec3(0.8f, 0.65f, 0.65f), Path(k_Survival, "chest"), 0.0f, ArtFit::Contain, white),
				ModelEntry("build_lamp_post", P::Box, vec4(0.2f, 0.2f, 0.22f, 1.0f), vec3(0.5f, 2.2f, 0.5f), Path(k_Town, "lantern"), 0.0f, ArtFit::Contain, white),
				ModelEntry("build_bench", P::Box, vec4(0.55f, 0.38f, 0.22f, 1.0f), vec3(1.8f, 0.5f, 0.6f), Path(k_Town, "stall-bench"), 90.0f, ArtFit::Contain, white),
				ModelEntry("build_beehive", P::Box, vec4(0.9f, 0.75f, 0.3f, 1.0f), vec3(0.5f, 0.7f, 0.5f), Path(k_Survival, "barrel"), 0.0f, ArtFit::Contain, vec4(1.0f, 0.85f, 0.35f, 1.0f)),
				ModelEntry("build_sign", P::Box, vec4(0.65f, 0.48f, 0.3f, 1.0f), vec3(0.6f, 1.0f, 0.1f), Path(k_Survival, "signpost"), 0.0f, ArtFit::Contain, white),
				ModelEntry("build_festival_lantern", P::Sphere, vec4(1.0f, 0.6f, 0.3f, 1.0f), vec3(0.3f, 0.9f, 0.3f), Path(k_Town, "lantern"), 0.0f, ArtFit::Contain, vec4(1.0f, 0.55f, 0.3f, 1.0f)),
				ModelEntry("landmark_shipping_bin", P::Box, vec4(0.5f, 0.32f, 0.2f, 1.0f), vec3(1.8f, 0.8f, 0.9f), Path(k_Survival, "box-large-open"), 90.0f, ArtFit::Stretch, white),
				ModelEntry("animal_chicken", P::Sphere, vec4(0.97f, 0.95f, 0.9f, 1.0f), vec3(0.5f, 0.45f, 0.5f), Path(k_Pets, "animal-chick"), 0.0f, ArtFit::Contain, white),
				ModelEntry("animal_cow", P::Box, vec4(0.92f, 0.9f, 0.85f, 1.0f), vec3(1.0f, 1.0f, 1.0f), Path(k_Pets, "animal-cow"), 0.0f, ArtFit::Contain, white),
				ModelEntry("crop_seed", P::Sphere, vec4(0.4f, 0.3f, 0.18f, 1.0f), vec3(0.3f, 0.1f, 0.3f), Path(k_Nature, "crops_dirtSingle"), 0.0f, ArtFit::Contain, white),
				ModelEntry("crop_sprout", P::Sphere, vec4(0.45f, 0.72f, 0.3f, 1.0f), vec3(0.35f), Path(k_Nature, "crops_leafsStageA"), 0.0f, ArtFit::Contain, vec4(1.0f, 0.9f, 0.55f, 1.0f)),
				ModelEntry("crop_young", P::Sphere, vec4(0.3f, 0.6f, 0.25f, 1.0f), vec3(0.4f, 0.55f, 0.4f), Path(k_Nature, "crops_leafsStageB"), 0.0f, ArtFit::Contain, vec4(1.0f, 0.9f, 0.55f, 1.0f)),
				ModelEntry("crop_parsnip", P::Sphere, vec4(0.93f, 0.88f, 0.65f, 1.0f), vec3(0.4f, 0.7f, 0.4f), Path(k_Nature, "crop_carrot"), 0.0f, ArtFit::Contain, vec4(1.0f, 0.95f, 0.55f, 1.0f)),
				ModelEntry("crop_radish", P::Sphere, vec4(0.85f, 0.2f, 0.3f, 1.0f), vec3(0.4f), Path(k_Nature, "crop_turnip"), 0.0f, ArtFit::Contain, vec4(1.0f, 0.35f, 0.5f, 1.0f)),
				ModelEntry("crop_strawberry", P::Sphere, vec4(0.9f, 0.15f, 0.2f, 1.0f), vec3(0.45f, 0.4f, 0.45f), Path(k_Nature, "crops_leafsStageA"), 0.0f, ArtFit::Contain, vec4(1.0f, 0.45f, 0.45f, 1.0f)),
				ModelEntry("crop_tomato", P::Sphere, vec4(0.92f, 0.3f, 0.18f, 1.0f), vec3(0.5f, 0.45f, 0.5f), Path(k_Nature, "crop_pumpkin"), 0.0f, ArtFit::Contain, vec4(1.0f, 0.35f, 0.3f, 1.0f)),
				ModelEntry("crop_melon", P::Sphere, vec4(0.55f, 0.8f, 0.35f, 1.0f), vec3(0.7f, 0.55f, 0.7f), Path(k_Nature, "crop_melon"), 0.0f, ArtFit::Contain, white),
				ModelEntry("crop_sunflower", P::Sphere, vec4(0.98f, 0.82f, 0.15f, 1.0f), vec3(0.6f, 1.2f, 0.6f), Path(k_Nature, "crops_cornStageD"), 0.0f, ArtFit::Contain, vec4(1.0f, 0.95f, 0.3f, 1.0f)),
				ModelEntry("crop_pumpkin", P::Sphere, vec4(0.95f, 0.55f, 0.12f, 1.0f), vec3(0.75f, 0.55f, 0.75f), Path(k_Nature, "crop_pumpkin"), 0.0f, ArtFit::Contain, white),
				ModelEntry("crop_cabbage", P::Sphere, vec4(0.6f, 0.82f, 0.5f, 1.0f), vec3(0.55f, 0.5f, 0.55f), Path(k_Nature, "crops_leafsStageB"), 0.0f, ArtFit::Contain, vec4(0.7f, 1.0f, 0.6f, 1.0f)),
				ModelEntry("crop_beetroot", P::Sphere, vec4(0.55f, 0.1f, 0.25f, 1.0f), vec3(0.4f), Path(k_Nature, "crop_turnip"), 0.0f, ArtFit::Contain, vec4(0.7f, 0.2f, 0.4f, 1.0f)),
				ModelEntry("crop_kale", P::Sphere, vec4(0.2f, 0.45f, 0.3f, 1.0f), vec3(0.5f, 0.7f, 0.5f), Path(k_Nature, "crops_leafsStageB"), 0.0f, ArtFit::Contain, vec4(0.4f, 0.7f, 0.5f, 1.0f)),
				ModelEntry("crop_leek", P::Box, vec4(0.75f, 0.9f, 0.6f, 1.0f), vec3(0.4f, 0.7f, 0.4f), Path(k_Nature, "crops_wheatStageB"), 0.0f, ArtFit::Contain, vec4(0.8f, 1.0f, 0.7f, 1.0f)),
				ModelEntry("crop_turnip", P::Sphere, vec4(0.88f, 0.75f, 0.9f, 1.0f), vec3(0.5f), Path(k_Nature, "crop_turnip"), 0.0f, ArtFit::Contain, white),

				Composite("build_flower_bed", P::Box, vec4(0.85f, 0.4f, 0.5f, 1.0f), vec3(0.9f, 0.5f, 0.9f), {
					Part(Path(k_Nature, "crops_dirtSingle"), vec3(0.9f, 0.18f, 0.9f)),
					Part(Path(k_Nature, "flower_redA"), vec3(0.22f, 0.4f, 0.22f), vec3(-0.22f, 0.14f, -0.15f), 0.0f, ArtFit::Contain),
					Part(Path(k_Nature, "flower_yellowA"), vec3(0.22f, 0.4f, 0.22f), vec3(0.22f, 0.14f, -0.1f), 0.0f, ArtFit::Contain),
					Part(Path(k_Nature, "flower_purpleA"), vec3(0.22f, 0.4f, 0.22f), vec3(0.0f, 0.14f, 0.22f), 0.0f, ArtFit::Contain),
				}),
				Composite("build_well", P::Box, vec4(0.55f, 0.55f, 0.58f, 1.0f), vec3(1.0f, 1.45f, 1.0f), {
					Part(Path(k_Town, "fountain-round"), vec3(1.0f, 0.3f, 1.0f)),
					Part(Path(k_Town, "pillar-wood"), vec3(0.12f, 1.1f, 0.12f), vec3(-0.4f, 0.3f, 0.0f)),
					Part(Path(k_Town, "pillar-wood"), vec3(0.12f, 1.1f, 0.12f), vec3(0.4f, 0.3f, 0.0f)),
					Part(Path(k_Town, "roof-point"), vec3(1.0f, 0.35f, 1.0f), vec3(0.0f, 1.1f, 0.0f)),
				}),
				Composite("build_coop", P::Box, vec4(0.75f, 0.35f, 0.25f, 1.0f), vec3(2.8f, 2.0f, 1.8f), {
					Part(Path(k_Town, "wall-wood-block"), vec3(2.6f, 1.2f, 1.6f)),
					Part(Path(k_Town, "roof-point"), vec3(2.9f, 0.8f, 1.9f), vec3(0.0f, 1.2f, 0.0f)),
					Part(Path(k_Town, "wall-wood-door"), vec3(0.7f, 0.9f, 0.08f), vec3(0.0f, 0.0f, 0.84f), -90.0f),
				}),
				Composite("build_barn", P::Box, vec4(0.7f, 0.2f, 0.15f, 1.0f), vec3(3.8f, 3.0f, 2.8f), {
					Part(Path(k_Town, "wall-wood-block"), vec3(3.6f, 1.9f, 2.6f), vec3(0.0f), 0.0f, ArtFit::Stretch, vec4(1.0f, 0.55f, 0.5f, 1.0f)),
					Part(Path(k_Town, "roof-high-point"), vec3(3.9f, 1.1f, 2.9f), vec3(0.0f, 1.9f, 0.0f)),
					Part(Path(k_Town, "wall-wood-door"), vec3(1.4f, 1.5f, 0.08f), vec3(0.0f, 0.0f, 1.34f), -90.0f),
				}),
				Composite("build_greenhouse", P::Box, vec4(0.75f, 0.9f, 0.85f, 1.0f), vec3(3.8f, 2.6f, 3.8f), {
					Part(Path(k_Town, "wall-window-glass"), vec3(0.08f, 1.8f, 3.6f), vec3(1.76f, 0.0f, 0.0f), 0.0f),
					Part(Path(k_Town, "wall-window-glass"), vec3(0.08f, 1.8f, 3.6f), vec3(-1.76f, 0.0f, 0.0f), 180.0f),
					Part(Path(k_Town, "wall-window-glass"), vec3(3.6f, 1.8f, 0.08f), vec3(0.0f, 0.0f, 1.76f), -90.0f),
					Part(Path(k_Town, "wall-window-glass"), vec3(3.6f, 1.8f, 0.08f), vec3(0.0f, 0.0f, -1.76f), 90.0f),
					Part(Path(k_Town, "roof-point"), vec3(3.8f, 0.8f, 3.8f), vec3(0.0f, 1.8f, 0.0f), 0.0f, ArtFit::Stretch, vec4(0.8f, 1.0f, 0.9f, 1.0f)),
				}),
				Composite("build_mailbox", P::Box, vec4(0.3f, 0.4f, 0.75f, 1.0f), vec3(0.3f, 1.0f, 0.3f), {
					Part(Path(k_Town, "pillar-wood"), vec3(0.1f, 0.8f, 0.1f)),
					Part(Path(k_Survival, "box"), vec3(0.3f, 0.25f, 0.3f), vec3(0.0f, 0.75f, 0.0f), 0.0f, ArtFit::Stretch, vec4(0.5f, 0.6f, 1.0f, 1.0f)),
				}),
				Composite("landmark_farmhouse", P::Box, vec4(0.8f, 0.62f, 0.45f, 1.0f), vec3(3.6f, 3.4f, 3.6f),
					HouseParts(Landmark::FarmHouse, 2.4f, 1.0f, true, white, white)),
				Composite("landmark_shop", P::Box, vec4(0.85f, 0.75f, 0.55f, 1.0f), vec3(3.6f, 3.0f, 3.6f),
					HouseParts(Landmark::Shop, 2.2f, 0.8f, false, white, vec4(1.0f, 0.85f, 0.7f, 1.0f))),
				Composite("landmark_carpenter", P::Box, vec4(0.65f, 0.48f, 0.3f, 1.0f), vec3(3.6f, 3.0f, 3.6f),
					HouseParts(Landmark::Carpenter, 2.2f, 0.8f, true, vec4(0.9f, 0.8f, 0.7f, 1.0f), vec4(0.8f, 0.9f, 1.0f, 1.0f))),
				Composite("landmark_mayor_house", P::Box, vec4(0.8f, 0.8f, 0.78f, 1.0f), vec3(3.6f, 3.4f, 3.6f),
					HouseParts(Landmark::MayorHouse, 2.5f, 0.9f, false, vec4(0.95f, 0.95f, 1.0f, 1.0f), vec4(0.7f, 0.8f, 1.0f, 1.0f))),
				Composite("landmark_cottage", P::Box, vec4(0.85f, 0.7f, 0.6f, 1.0f), vec3(3.6f, 2.8f, 3.6f),
					HouseParts(Landmark::Cottage, 2.0f, 0.8f, true, vec4(1.0f, 0.9f, 0.85f, 1.0f), vec4(1.0f, 0.8f, 0.7f, 1.0f))),
				Composite("landmark_notice_board", P::Box, vec4(0.55f, 0.4f, 0.25f, 1.0f), vec3(1.8f, 1.6f, 0.2f), {
					Part(Path(k_Town, "pillar-wood"), vec3(0.14f, 1.6f, 0.14f), vec3(-0.8f, 0.0f, 0.0f)),
					Part(Path(k_Town, "pillar-wood"), vec3(0.14f, 1.6f, 0.14f), vec3(0.8f, 0.0f, 0.0f)),
					Part(Path(k_Town, "wall-wood-half"), vec3(1.7f, 0.8f, 0.08f), vec3(0.0f, 0.7f, 0.0f), -90.0f),
				}),

				ArtEntry{ "villager_mira",     P::Box, vec4(0.82f, 0.35f, 0.3f, 1.0f),  vec3(0.5f, 0.9f, 0.36f) },
				ArtEntry{ "villager_bram",     P::Box, vec4(0.3f, 0.45f, 0.7f, 1.0f),   vec3(0.56f, 0.98f, 0.42f) },
				ArtEntry{ "villager_odette",   P::Box, vec4(0.55f, 0.32f, 0.6f, 1.0f),  vec3(0.54f, 1.0f, 0.42f) },
				ArtEntry{ "villager_pip",      P::Box, vec4(0.95f, 0.8f, 0.25f, 1.0f),  vec3(0.4f, 0.62f, 0.3f) },
			};
			return entries;
		}

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

		const std::vector<ArtEntry>& Entries()
		{
			static const std::vector<ArtEntry> entries = MakeEntries();
			return entries;
		}

		template<typename Table>
		std::string_view Lookup(const Table& table, size_t index)
		{
			return index < table.size() ? table[index] : std::string_view();
		}
	}

	std::span<const ArtEntry> All()
	{
		return Entries();
	}

	const ArtEntry* Find(std::string_view id)
	{
		for (const ArtEntry& entry : Entries())
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
	std::string_view ForVillagerHead() { return {}; }
	std::string_view ForAnimal(AnimalKind kind) { return kind == AnimalKind::Cow ? "animal_cow" : "animal_chicken"; }
	std::string_view ForAnimalHead(AnimalKind) { return {}; }
	std::string_view ForProduce(AnimalKind kind) { return kind == AnimalKind::Cow ? "produce_milk" : "produce_egg"; }

	VillagerLook ForVillagerCharacter(VillagerId villager)
	{
		constexpr std::string_view knight = "kaykit/characters/Knight.glb";
		constexpr std::string_view knightTexture = "kaykit/characters/knight_texture.png";
		constexpr std::string_view barbarian = "kaykit/characters/Barbarian.glb";
		constexpr std::string_view barbarianTexture = "kaykit/characters/barbarian_texture.png";
		switch (villager)
		{
		case VillagerId::Shopkeeper: return { knight, knightTexture, glm::vec4(1.0f, 0.7f, 0.65f, 1.0f), 1.2f };
		case VillagerId::Carpenter: return { barbarian, barbarianTexture, glm::vec4(0.7f, 0.8f, 1.1f, 1.0f), 1.3f };
		case VillagerId::Mayor: return { knight, knightTexture, glm::vec4(0.9f, 0.7f, 1.1f, 1.0f), 1.25f };
		default:                 return { barbarian, barbarianTexture, glm::vec4(1.1f, 1.0f, 0.6f, 1.0f), 0.9f };
		}
	}

	Mesh* GetMesh(const ArtEntry& entry)
	{
		Renderer3D& renderer = Application::Get().GetRenderer3D();
		return entry.Primitive == ArtPrimitive::Sphere ? renderer.GetSphereMesh() : renderer.GetBoxMesh();
	}

	namespace
	{
		struct ModelInfo
		{
			Model* Loaded = nullptr;
			glm::vec3 Min{ 0.0f };
			glm::vec3 Max{ 0.0f };
		};

		std::unordered_map<std::string, ModelInfo>& ModelCache()
		{
			static std::unordered_map<std::string, ModelInfo> cache;
			return cache;
		}

		const ModelInfo* GetModelInfo(std::string_view path)
		{
			auto& cache = ModelCache();
			const std::string key(path);
			if (const auto found = cache.find(key); found != cache.end())
				return found->second.Loaded ? &found->second : nullptr;

			ModelInfo info;
			AssetManager& assets = Application::Get().GetAssetManager();
			info.Loaded = assets.GetModel(assets.Load(FileSystem::PathFromUtf8(path)));
			if (info.Loaded)
			{
				glm::vec3 low(1e9f);
				glm::vec3 high(-1e9f);
				for (const SubMesh& submesh : info.Loaded->GetSubMeshes())
				{
					if (!submesh.Mesh)
						continue;
					for (const MeshVertex& vertex : submesh.Mesh->GetVertices())
					{
						low = glm::min(low, vertex.Position);
						high = glm::max(high, vertex.Position);
					}
				}
				if (low.x > high.x)
					info.Loaded = nullptr;
				info.Min = low;
				info.Max = high;
			}
			if (!info.Loaded)
				DE_ERROR("Hearthvale: art model '{}' did not load", path);
			const auto inserted = cache.emplace(key, info).first;
			return inserted->second.Loaded ? &inserted->second : nullptr;
		}

		void AddMeshes(Scene& scene, Entity part, const ModelInfo& info, const glm::vec4& tint)
		{
			bool first = true;
			for (const SubMesh& submesh : info.Loaded->GetSubMeshes())
			{
				if (!submesh.Mesh)
					continue;

				Entity target = part;
				if (!first)
				{
					target = scene.CreateEntity("Mesh");
					target.AddComponent<Transform3DComponent>();
					target.SetParent(part, false);
				}
				first = false;

				MeshRendererComponent renderer(submesh.Mesh, submesh.BaseColor * tint);
				if (submesh.DiffuseTexture && submesh.Material)
				{
					renderer.Material = submesh.Material;
					renderer.Color = tint;
				}
				target.AddComponent<MeshRendererComponent>(renderer);
			}
		}

		Entity SpawnPart(Scene& scene, const ArtPart& part, float compositeHeight, Entity parent)
		{
			const ModelInfo* info = GetModelInfo(part.ModelPath);
			if (!info)
				return {};

			const glm::vec3 extent = info->Max - info->Min;
			const bool odd = (static_cast<int>(std::lround(part.YawDegrees / 90.0f)) & 1) != 0;
			const glm::vec3 turned = odd ? glm::vec3(extent.z, extent.y, extent.x) : extent;
			const glm::vec3 safe = (glm::max)(turned, glm::vec3(1e-4f));

			glm::vec3 scale(1.0f);
			if (part.Fit == ArtFit::Stretch)
			{
				const glm::vec3 fill = part.Size / safe;
				scale = odd ? glm::vec3(fill.z, fill.y, fill.x) : fill;
			}
			else
			{
				const glm::vec3 fill = part.Size / safe;
				scale = glm::vec3((std::min)({ fill.x, fill.y, fill.z }));
			}

			const glm::quat rotation = glm::angleAxis(glm::radians(part.YawDegrees), glm::vec3(0.0f, 1.0f, 0.0f));
			const glm::vec3 bottomCentre(0.5f * (info->Min.x + info->Max.x), info->Min.y, 0.5f * (info->Min.z + info->Max.z));
			const glm::vec3 target(part.Offset.x, part.Offset.y - 0.5f * compositeHeight, part.Offset.z);

			Entity entity = scene.CreateEntity("Part");
			auto& transform = entity.AddComponent<Transform3DComponent>();
			transform.Scale = scale;
			transform.Rotation = rotation;
			transform.Position = target - rotation * (scale * bottomCentre);
			entity.SetParent(parent, false);
			return entity;
		}

		void AddPartMeshes(Scene& scene, Entity part, std::string_view path, const glm::vec4& tint)
		{
			if (const ModelInfo* info = GetModelInfo(path))
				AddMeshes(scene, part, *info, tint);
		}
	}

	std::vector<std::string_view> ModelPathsOf(const ArtEntry& entry)
	{
		std::vector<std::string_view> paths;
		if (!entry.Parts.empty())
		{
			for (const ArtPart& part : entry.Parts)
				paths.push_back(part.ModelPath);
		}
		else if (!entry.ModelPath.empty())
		{
			paths.push_back(entry.ModelPath);
		}
		return paths;
	}

	int LoadModels(const ArtEntry& entry)
	{
		int failed = 0;
		for (std::string_view path : ModelPathsOf(entry))
			failed += GetModelInfo(path) ? 0 : 1;
		return failed;
	}

	void ReleaseModels()
	{
		ModelCache().clear();
	}

	void ForEachMesh(Entity root, const std::function<void(MeshRendererComponent&)>& fn)
	{
		if (!root.IsValid())
			return;
		if (root.HasComponent<MeshRendererComponent>())
			fn(root.GetComponent<MeshRendererComponent>());
		root.ForEachChild([&fn](Entity child) { ForEachMesh(child, fn); });
	}

	Entity Spawn(Scene& scene, std::string_view id, const std::string& name, const glm::vec3& position, int quarterTurns, float scale)
	{
		const ArtEntry* entry = Find(id);
		if (!entry || Application::Get().IsHeadless())
			return {};

		Entity entity = scene.CreateEntity(name);
		auto& transform = entity.AddComponent<Transform3DComponent>();
		transform.Position = position;
		const int turns = ((quarterTurns % 4) + 4) % 4;
		if (turns != 0)
			transform.Rotation = glm::angleAxis(glm::half_pi<float>() * static_cast<float>(turns), glm::vec3(0.0f, 1.0f, 0.0f));

		const std::vector<std::string_view> paths = ModelPathsOf(*entry);
		bool modelled = !paths.empty();
		for (std::string_view path : paths)
			modelled = modelled && GetModelInfo(path) != nullptr;

		if (!modelled)
		{
			transform.Scale = entry->Size * scale;
			entity.AddComponent<MeshRendererComponent>(MeshRendererComponent(GetMesh(*entry), entry->Color));
			return entity;
		}

		transform.Scale = glm::vec3(scale);
		if (entry->Parts.empty())
		{
			const ArtPart whole{ entry->ModelPath, entry->Size, glm::vec3(0.0f), entry->YawDegrees, entry->Fit, entry->Tint };
			Entity part = SpawnPart(scene, whole, entry->Size.y, entity);
			AddPartMeshes(scene, part, whole.ModelPath, entry->Tint);
		}
		else
		{
			for (const ArtPart& partSpec : entry->Parts)
			{
				Entity part = SpawnPart(scene, partSpec, entry->Size.y, entity);
				AddPartMeshes(scene, part, partSpec.ModelPath, partSpec.Tint);
			}
		}
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
