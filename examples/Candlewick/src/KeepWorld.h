#pragma once
#include "KeepMap.h"

#include <DingoEngine.h>

#include <glm/glm.hpp>

#include <optional>
#include <vector>

namespace Dingo
{

	enum class FlameKind { Sconce, Candle };

	struct DecorFlame
	{
		Entity Light;
		Entity Core;
		FlameKind Kind = FlameKind::Candle;
		float BaseIntensity = 0.0f;
	};

	struct BrazierSpot
	{
		Entity Light;
		Entity Core;
		int Room = -1;
		glm::ivec2 Tile{ 0 };
		bool IsAltar = false;
	};

	struct FlaskSpot
	{
		glm::vec3 Position{ 0.0f };
		std::vector<Entity> Parts;
		bool Collected = false;
	};

	// Builds the level from the map and owns every material and mesh it creates.
	class KeepWorld
	{
	public:
		KeepWorld(Scene& scene, const KeepMap& map);
		~KeepWorld();

		KeepWorld(const KeepWorld&) = delete;
		KeepWorld& operator=(const KeepWorld&) = delete;

		const std::vector<DecorFlame>& GetFlames() const { return m_Flames; }
		const std::vector<BrazierSpot>& GetBraziers() const { return m_Braziers; }
		Material* GetBrassMaterial() const { return m_BrassMaterial; }

		// Braziers are built dark: their light off and their cores on the ash material. Lighting one
		// swaps its core to the flame material that every decorative flame also shares.
		Material* GetFlameMaterial() const { return m_FlameMaterial; }

		size_t CollectFlasks(const glm::vec3& feet, size_t maxCount);

		// Hides the wall rectangles (with their caps and mounted sconces) that sit between the eye and the target.
		// A hidden wall and its cap still cast shadows; what is mounted on them is hidden outright.
		void UpdateCutaway(const glm::vec3& eye, const glm::vec3& target);

		// Hides every wall rectangle on the row just south of the room, for a fixed camera that looks over it.
		void HideSouthWalls(const TileRect& room);

	private:
		struct WallRect
		{
			Entity Wall;
			Entity Cap;
			std::vector<Entity> Mounted;
			TileRect Tiles;
			glm::vec3 Min{ 0.0f };
			glm::vec3 Max{ 0.0f };
			bool Hidden = false;
		};

		void CreateMaterials();
		void SpawnAmbient();
		void BuildFloors();
		void BuildWalls();
		void BuildMarkers();

		Entity SpawnSolid(const char* name, const glm::vec3& center, const glm::vec3& size, const glm::vec4& color, Material* material);
		Entity SpawnDecor(const char* name, const glm::vec3& center, const glm::vec3& size, const glm::vec4& color, Material* material);
		Entity SpawnGlow(const char* name, const glm::vec3& center, float diameter, Material* material, const glm::vec4& color);
		Entity SpawnPointLight(const char* name, const glm::vec3& position, float intensity, float range);

		void SpawnFloor(const TileRect& rect);
		void SpawnWallRect(const TileRect& rect);
		void SetWallHidden(WallRect& wall, bool hidden);
		BrazierSpot SpawnBrazier(const KeepMarker& marker);
		std::optional<DecorFlame> SpawnSconce(const KeepMarker& marker);
		DecorFlame SpawnCandle(const KeepMarker& marker);
		FlaskSpot SpawnFlask(const KeepMarker& marker);

	private:
		Scene& m_Scene;
		const KeepMap& m_Map;

		Mesh* m_BoxMesh = nullptr;
		Mesh* m_FlameMesh = nullptr;

		Material* m_StoneMaterial = nullptr;
		Material* m_CapMaterial = nullptr;
		Material* m_FloorMaterial = nullptr;
		Material* m_BrassMaterial = nullptr;
		Material* m_WaxMaterial = nullptr;
		Material* m_FlameMaterial = nullptr;
		Material* m_AshMaterial = nullptr;
		Material* m_FlaskMaterial = nullptr;

		std::vector<WallRect> m_Walls;
		std::vector<int> m_WallOfTile;

		std::vector<DecorFlame> m_Flames;
		std::vector<BrazierSpot> m_Braziers;
		std::vector<FlaskSpot> m_FlaskSpots;
	};

}
