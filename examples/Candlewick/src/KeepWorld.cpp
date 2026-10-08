#include "KeepWorld.h"
#include "GameTuning.h"
#include "LaunchOptions.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace
{
	using namespace Dingo;

	struct BrazierStyle
	{
		float BaseWidth;
		float BaseHeight;
		float StandWidth;
		float StandHeight;
		float BowlWidth;
		float BowlHeight;
		float CoreDiameter;
		float CoreRise;
		float LightRise;
		float LightRange;
		float LightIntensity;
	};

	constexpr BrazierStyle k_Brazier = { 0.62f, 0.1f, 0.32f, 0.7f, 0.82f, 0.16f, 0.46f, 0.1f, 0.4f, BRAZIER_LIGHT_RANGE, BRAZIER_LIGHT_INTENSITY };
	constexpr BrazierStyle k_Altar = { 1.1f, 0.14f, 0.6f, 0.9f, 1.3f, 0.22f, 0.75f, 0.18f, 0.55f, ALTAR_LIGHT_RANGE, ALTAR_LIGHT_INTENSITY };

	constexpr float k_CandleWidth = 0.1f;
	constexpr float k_CandleHeight = 0.3f;
	constexpr float k_CandleFlameDiameter = 0.12f;
	constexpr float k_CandleLightRise = 0.3f;

	constexpr float k_SconceStemWidth = 0.14f;
	constexpr float k_SconceStemHeight = 0.4f;
	constexpr float k_SconceCupWidth = 0.26f;
	constexpr float k_SconceCupHeight = 0.07f;
	constexpr float k_SconceFlameDiameter = 0.24f;
	constexpr float k_SconceLightOffset = 0.45f;
	constexpr float k_SconceBracketDrop = 0.1f;
	constexpr float k_SconceFlameRise = 0.06f;

	constexpr glm::vec3 k_FlaskBodySize = { 0.24f, 0.2f, 0.24f };
	constexpr glm::vec3 k_FlaskNeckSize = { 0.07f, 0.12f, 0.07f };
	constexpr glm::vec3 k_FlaskCorkSize = { 0.09f, 0.04f, 0.09f };
	constexpr float k_FlaskNeckCenter = 0.25f;
	constexpr float k_FlaskCorkCenter = 0.33f;

	constexpr float k_UnitSphereRadius = 0.5f;

	std::vector<TileRect> GreedyRects(std::vector<bool> open, int width, int height, int maxSide)
	{
		std::vector<TileRect> rects;
		for (int row = 0; row < height; ++row)
		{
			for (int col = 0; col < width; ++col)
			{
				if (!open[static_cast<size_t>(row) * width + col])
					continue;

				int runWidth = 1;
				while (runWidth < maxSide && col + runWidth < width && open[static_cast<size_t>(row) * width + col + runWidth])
					++runWidth;

				int runHeight = 1;
				while (runHeight < maxSide && row + runHeight < height)
				{
					bool full = true;
					for (int i = 0; i < runWidth && full; ++i)
						full = open[static_cast<size_t>(row + runHeight) * width + col + i];
					if (!full)
						break;
					++runHeight;
				}

				for (int r = 0; r < runHeight; ++r)
					for (int c = 0; c < runWidth; ++c)
						open[static_cast<size_t>(row + r) * width + col + c] = false;

				rects.push_back({ col, row, runWidth, runHeight });
			}
		}
		return rects;
	}

	glm::vec3 RectCenter(const TileRect& rect, float y)
	{
		return glm::vec3((static_cast<float>(rect.Col) + rect.Width * 0.5f) * TILE_SIZE, y, (static_cast<float>(rect.Row) + rect.Height * 0.5f) * TILE_SIZE);
	}

	bool SegmentHitsBox(const glm::vec3& from, const glm::vec3& to, const glm::vec3& boxMin, const glm::vec3& boxMax)
	{
		const glm::vec3 delta = to - from;
		float tEnter = 0.0f;
		float tLeave = 1.0f;
		for (int axis = 0; axis < 3; ++axis)
		{
			if (std::abs(delta[axis]) < 1e-6f)
			{
				if (from[axis] < boxMin[axis] || from[axis] > boxMax[axis])
					return false;
				continue;
			}

			float tNear = (boxMin[axis] - from[axis]) / delta[axis];
			float tFar = (boxMax[axis] - from[axis]) / delta[axis];
			if (tNear > tFar)
				std::swap(tNear, tFar);
			tEnter = std::max(tEnter, tNear);
			tLeave = std::min(tLeave, tFar);
			if (tEnter > tLeave)
				return false;
		}
		return true;
	}

	void SetVisible(Entity entity, bool visible)
	{
		entity.GetComponent<MeshRendererComponent>().Visible = visible;
	}

	// A cut-away wall still stands in the light: it stops drawing but goes on casting.
	void SetCutAway(Entity entity, bool cutAway)
	{
		entity.GetComponent<MeshRendererComponent>().Shadows = cutAway ? ShadowCasting::ShadowsOnly : ShadowCasting::On;
	}
}

namespace Dingo
{

	KeepWorld::KeepWorld(Scene& scene, const KeepMap& map)
		: m_Scene(scene), m_Map(map)
	{
		Renderer3D& renderer3D = Application::Get().GetRenderer3D();
		m_BoxMesh = renderer3D.GetBoxMesh();
		m_FlameMesh = Mesh::CreateSphere(k_UnitSphereRadius, FLAME_MESH_RINGS, FLAME_MESH_SEGMENTS);
		m_WallOfTile.assign(static_cast<size_t>(map.GetWidth()) * map.GetHeight(), -1);

		CreateMaterials();
		m_Effects = CreateFlameEffects();
		SpawnAmbient();
		BuildFloors();
		BuildWalls();
		BuildMarkers();

		const auto& rooms = map.GetRooms();
		for (size_t i = 0; i < rooms.size(); ++i)
		{
			const auto count = [&](MarkerType type)
			{
				const auto& markers = map.GetMarkers(type);
				return std::count_if(markers.begin(), markers.end(), [&](const KeepMarker& marker) { return marker.Room == static_cast<int>(i); });
			};
			DE_INFO("Candlewick: {} - {} braziers, {} altar, {} sconces, {} candles, {} flasks", rooms[i].Name,
				count(MarkerType::Brazier), count(MarkerType::Altar), count(MarkerType::Sconce), count(MarkerType::Candle), count(MarkerType::Flask));
		}
		DE_INFO("Candlewick: keep built, {}x{} tiles, {} wall rectangles", map.GetWidth(), map.GetHeight(), m_Walls.size());
	}

	KeepWorld::~KeepWorld()
	{
		DestroyAndDelete(m_StoneMaterial);
		DestroyAndDelete(m_CapMaterial);
		DestroyAndDelete(m_FloorMaterial);
		DestroyAndDelete(m_BrassMaterial);
		DestroyAndDelete(m_WaxMaterial);
		DestroyAndDelete(m_FlameMaterial);
		DestroyAndDelete(m_AshMaterial);
		DestroyAndDelete(m_FlaskMaterial);
		delete m_FlameMesh;
	}

	void KeepWorld::CreateMaterials()
	{
		Renderer3D& renderer3D = Application::Get().GetRenderer3D();

		m_StoneMaterial = renderer3D.CreateLitMaterial(MaterialParams()
			.SetDebugName("KeepStone")
			.SetRoughness(STONE_ROUGHNESS));

		m_CapMaterial = renderer3D.CreateLitMaterial(MaterialParams()
			.SetDebugName("KeepCap")
			.SetRoughness(CAP_ROUGHNESS)
			.SetEmissiveColor(CAP_EMISSIVE_COLOR)
			.SetEmissiveStrength(CAP_EMISSIVE));

		m_FloorMaterial = renderer3D.CreateLitMaterial(MaterialParams()
			.SetDebugName("KeepFloor")
			.SetRoughness(FLOOR_ROUGHNESS));

		m_BrassMaterial = renderer3D.CreateLitMaterial(MaterialParams()
			.SetDebugName("KeepBrass")
			.SetRoughness(BRASS_ROUGHNESS)
			.SetEmissiveColor(BRASS_EMISSIVE_COLOR)
			.SetEmissiveStrength(BRASS_EMISSIVE)
			.SetSpecular(BRASS_SPECULAR));

		m_WaxMaterial = renderer3D.CreateLitMaterial(MaterialParams()
			.SetDebugName("KeepWax")
			.SetEmissiveColor(glm::vec3(COLOR_WAX))
			.SetEmissiveStrength(WAX_EMISSIVE)
			.SetRoughness(WAX_ROUGHNESS));

		m_FlameMaterial = renderer3D.CreateLitMaterial(MaterialParams()
			.SetDebugName("KeepFlameCore")
			.SetEmissiveColor(FLAME_COLOR)
			.SetEmissiveStrength(FLAME_EMISSIVE));

		m_AshMaterial = renderer3D.CreateLitMaterial(MaterialParams()
			.SetDebugName("KeepAshCore")
			.SetRoughness(ASH_ROUGHNESS)
			.SetEmissiveColor(ASH_EMISSIVE_COLOR)
			.SetEmissiveStrength(ASH_EMISSIVE));

		m_FlaskMaterial = renderer3D.CreateLitMaterial(MaterialParams()
			.SetDebugName("KeepFlask")
			.SetRoughness(OIL_ROUGHNESS)
			.SetSpecular(OIL_SPECULAR)
			.SetEmissiveColor(OIL_EMISSIVE_COLOR)
			.SetEmissiveStrength(OIL_EMISSIVE));
	}

	void KeepWorld::SpawnAmbient()
	{
		Entity ambient = m_Scene.CreateEntity("Ambient");
		ambient.AddComponent<Transform3DComponent>();
		ambient.AddComponent<AmbientLightComponent>(AmbientLightComponent(AMBIENT_COLOR, AMBIENT_INTENSITY));
	}

	Entity KeepWorld::SpawnSolid(const char* name, const glm::vec3& center, const glm::vec3& size, const glm::vec4& color, Material* material)
	{
		Entity entity = SpawnDecor(name, center, size, color, material);
		entity.AddComponent<RigidBody3DComponent>(RigidBody3DComponent(BodyType3D::Static));
		entity.AddComponent<BoxCollider3DComponent>();
		return entity;
	}

	Entity KeepWorld::SpawnDecor(const char* name, const glm::vec3& center, const glm::vec3& size, const glm::vec4& color, Material* material)
	{
		Entity entity = m_Scene.CreateEntity(name);
		auto& transform = entity.AddComponent<Transform3DComponent>();
		transform.Position = center;
		transform.Scale = size;

		entity.AddComponent<MeshRendererComponent>(MeshRendererComponent(m_BoxMesh, color)).Material = material;
		return entity;
	}

	Entity KeepWorld::SpawnGlow(const char* name, const glm::vec3& center, float diameter, Material* material, const glm::vec4& color)
	{
		Entity entity = m_Scene.CreateEntity(name);
		auto& transform = entity.AddComponent<Transform3DComponent>();
		transform.Position = center;
		transform.Scale = glm::vec3(diameter);

		auto& renderer = entity.AddComponent<MeshRendererComponent>(MeshRendererComponent(m_FlameMesh, color));
		renderer.Material = material;
		renderer.Shadows = ShadowCasting::Off;
		return entity;
	}

	Entity KeepWorld::SpawnPointLight(const char* name, const glm::vec3& position, float intensity, float range)
	{
		Entity entity = m_Scene.CreateEntity(name);
		entity.AddComponent<Transform3DComponent>().Position = position;
		entity.AddComponent<PointLightComponent>(PointLightComponent(FLAME_COLOR, intensity, range));
		return entity;
	}

	void KeepWorld::BuildFloors()
	{
		const int width = m_Map.GetWidth();
		const int height = m_Map.GetHeight();
		std::vector<bool> open(static_cast<size_t>(width) * height, false);
		for (int row = 0; row < height; ++row)
			for (int col = 0; col < width; ++col)
				open[static_cast<size_t>(row) * width + col] = m_Map.HasFloor({ col, row });

		const auto claim = [&](const TileRect& rect)
		{
			SpawnFloor(rect);
			for (int row = rect.Row; row < rect.Row + rect.Height; ++row)
				for (int col = rect.Col; col < rect.Col + rect.Width; ++col)
					open[static_cast<size_t>(row) * width + col] = false;
		};

		for (const KeepRoom& room : m_Map.GetRooms())
			claim(room.Rect);
		for (const TileRect& corridor : m_Map.GetCorridors())
			claim(corridor);

		for (const TileRect& rect : GreedyRects(open, width, height, width))
			SpawnFloor(rect);
	}

	void KeepWorld::SpawnFloor(const TileRect& rect)
	{
		SpawnSolid("Floor", RectCenter(rect, -FLOOR_THICKNESS * 0.5f),
			{ rect.Width * TILE_SIZE, FLOOR_THICKNESS, rect.Height * TILE_SIZE }, COLOR_FLOOR, m_FloorMaterial);
	}

	void KeepWorld::BuildWalls()
	{
		const int width = m_Map.GetWidth();
		const int height = m_Map.GetHeight();
		std::vector<bool> open(static_cast<size_t>(width) * height, false);
		for (int row = 0; row < height; ++row)
			for (int col = 0; col < width; ++col)
				open[static_cast<size_t>(row) * width + col] = m_Map.IsWall({ col, row });

		for (const TileRect& rect : GreedyRects(open, width, height, WALL_MAX_RUN))
			SpawnWallRect(rect);
	}

	void KeepWorld::SpawnWallRect(const TileRect& rect)
	{
		WallRect wall;
		wall.Tiles = rect;
		const float sizeX = rect.Width * TILE_SIZE;
		const float sizeZ = rect.Height * TILE_SIZE;

		wall.Wall = SpawnSolid("Wall", RectCenter(rect, WALL_HEIGHT * 0.5f), { sizeX, WALL_HEIGHT, sizeZ }, COLOR_STONE, m_StoneMaterial);
		wall.Cap = SpawnDecor("WallCap", RectCenter(rect, WALL_HEIGHT + WALL_CAP_THICKNESS * 0.5f),
			{ sizeX + 2.0f * WALL_CAP_OVERHANG, WALL_CAP_THICKNESS, sizeZ + 2.0f * WALL_CAP_OVERHANG }, COLOR_CAP, m_CapMaterial);

		wall.Min = glm::vec3(rect.Col * TILE_SIZE, 0.0f, rect.Row * TILE_SIZE);
		wall.Max = glm::vec3((rect.Col + rect.Width) * TILE_SIZE, WALL_HEIGHT + WALL_CAP_THICKNESS, (rect.Row + rect.Height) * TILE_SIZE);

		const size_t index = m_Walls.size();
		for (int row = rect.Row; row < rect.Row + rect.Height; ++row)
			for (int col = rect.Col; col < rect.Col + rect.Width; ++col)
				m_WallOfTile[static_cast<size_t>(row) * m_Map.GetWidth() + col] = static_cast<int>(index);

		m_Walls.push_back(std::move(wall));
	}

	void KeepWorld::BuildMarkers()
	{
		for (const KeepMarker& marker : m_Map.GetMarkers(MarkerType::Brazier))
			m_Braziers.push_back(SpawnBrazier(marker));
		for (const KeepMarker& marker : m_Map.GetMarkers(MarkerType::Altar))
			m_Braziers.push_back(SpawnBrazier(marker));

		for (const KeepMarker& marker : m_Map.GetMarkers(MarkerType::Sconce))
		{
			if (const std::optional<DecorFlame> flame = SpawnSconce(marker))
				m_Flames.push_back(*flame);
		}

		for (const KeepMarker& marker : m_Map.GetMarkers(MarkerType::Candle))
			m_Flames.push_back(SpawnCandle(marker));

		for (const KeepMarker& marker : m_Map.GetMarkers(MarkerType::Flask))
			m_FlaskSpots.push_back(SpawnFlask(marker));
	}

	BrazierSpot KeepWorld::SpawnBrazier(const KeepMarker& marker)
	{
		const bool isAltar = marker.Type == MarkerType::Altar;
		const BrazierStyle& style = isAltar ? k_Altar : k_Brazier;
		const glm::vec3 floor = m_Map.TileCenter(marker.Tile);

		const float standTop = style.BaseHeight + style.StandHeight;
		const float bowlTop = standTop + style.BowlHeight;

		SpawnSolid("BrazierBase", floor + glm::vec3(0.0f, style.BaseHeight * 0.5f, 0.0f),
			{ style.BaseWidth, style.BaseHeight, style.BaseWidth }, COLOR_BRASS, m_BrassMaterial);
		SpawnSolid("BrazierStand", floor + glm::vec3(0.0f, style.BaseHeight + style.StandHeight * 0.5f, 0.0f),
			{ style.StandWidth, style.StandHeight, style.StandWidth }, COLOR_BRASS, m_BrassMaterial);
		SpawnSolid("BrazierBowl", floor + glm::vec3(0.0f, standTop + style.BowlHeight * 0.5f, 0.0f),
			{ style.BowlWidth, style.BowlHeight, style.BowlWidth }, COLOR_BRASS, m_BrassMaterial);

		const glm::vec3 core = floor + glm::vec3(0.0f, bowlTop + style.CoreRise, 0.0f);

		BrazierSpot spot;
		spot.Core = SpawnGlow(isAltar ? "AltarCore" : "BrazierCore", core, style.CoreDiameter, m_AshMaterial, COLOR_ASH);
		const glm::vec3 flameBase = core + glm::vec3(0.0f, style.CoreDiameter * FLAME_EMITTER_RISE, 0.0f);
		spot.Flame = SpawnEmitter(m_Scene, "BrazierFlame", m_Effects.BrazierFlame.get(), flameBase, false);
		spot.Embers = SpawnEmitter(m_Scene, "BrazierEmbers", m_Effects.BrazierEmbers.get(), flameBase, false);
		spot.Smoke = SpawnEmitter(m_Scene, "BrazierSmoke", m_Effects.BrazierSmoke.get(), core + glm::vec3(0.0f, BRAZIER_SMOKE_RISE, 0.0f), false);
		spot.Kindle = SpawnEmitter(m_Scene, "BrazierKindle", m_Effects.Kindle.get(), core);
		spot.Light = SpawnPointLight(isAltar ? "AltarLight" : "BrazierLight", core + glm::vec3(0.0f, style.LightRise, 0.0f), style.LightIntensity, style.LightRange);
		auto& light = spot.Light.GetComponent<PointLightComponent>();
		light.Enabled = false;
		light.CastShadows = !GetLaunchOptions().NoShadows;
		spot.Room = marker.Room;
		spot.Tile = marker.Tile;
		spot.IsAltar = isAltar;
		return spot;
	}

	std::optional<DecorFlame> KeepWorld::SpawnSconce(const KeepMarker& marker)
	{
		const glm::ivec2 towardsWall[] = { { 0, -1 }, { -1, 0 }, { 1, 0 }, { 0, 1 } };
		const auto wallStep = std::find_if(std::begin(towardsWall), std::end(towardsWall), [&](const glm::ivec2& step) { return m_Map.IsWall(marker.Tile + step); });
		if (wallStep == std::end(towardsWall))
		{
			DE_WARN("Candlewick: sconce at tile ({}, {}) has no wall beside it", marker.Tile.x, marker.Tile.y);
			return std::nullopt;
		}

		const glm::vec3 inward(static_cast<float>(-wallStep->x), 0.0f, static_cast<float>(-wallStep->y));
		const glm::vec3 base = m_Map.TileCenter(marker.Tile) - inward * (0.5f * TILE_SIZE) + glm::vec3(0.0f, SCONCE_HEIGHT, 0.0f);

		Entity stem = SpawnDecor("SconceStem", base + inward * (k_SconceStemWidth * 0.5f) - glm::vec3(0.0f, k_SconceBracketDrop + k_SconceStemHeight * 0.5f, 0.0f),
			{ k_SconceStemWidth, k_SconceStemHeight, k_SconceStemWidth }, COLOR_BRASS, m_BrassMaterial);
		Entity cup = SpawnDecor("SconceCup", base + inward * (k_SconceCupWidth * 0.5f) - glm::vec3(0.0f, k_SconceBracketDrop - k_SconceCupHeight * 0.5f, 0.0f),
			{ k_SconceCupWidth, k_SconceCupHeight, k_SconceCupWidth }, COLOR_BRASS, m_BrassMaterial);

		DecorFlame flame;
		const glm::vec3 core = base + inward * (k_SconceCupWidth * 0.5f) + glm::vec3(0.0f, k_SconceFlameRise, 0.0f);
		flame.Core = SpawnGlow("SconceCore", core, k_SconceFlameDiameter, m_FlameMaterial, COLOR_EMBER);
		flame.Emitter = SpawnEmitter(m_Scene, "SconceFlame", m_Effects.Sconce.get(), core + glm::vec3(0.0f, k_SconceFlameDiameter * FLAME_EMITTER_RISE, 0.0f));
		flame.Light = SpawnPointLight("SconceLight", base + inward * k_SconceLightOffset + glm::vec3(0.0f, k_SconceFlameRise, 0.0f), SCONCE_LIGHT_INTENSITY, SCONCE_LIGHT_RANGE);
		flame.Kind = FlameKind::Sconce;
		flame.BaseIntensity = SCONCE_LIGHT_INTENSITY;

		const int wallRect = m_WallOfTile[static_cast<size_t>(marker.Tile.y + wallStep->y) * m_Map.GetWidth() + marker.Tile.x + wallStep->x];
		if (wallRect >= 0)
		{
			m_Walls[wallRect].Mounted.insert(m_Walls[wallRect].Mounted.end(), { stem, cup, flame.Core });
			if (flame.Emitter.IsValid())
				m_Walls[wallRect].Emitters.push_back(flame.Emitter);
		}

		return flame;
	}

	DecorFlame KeepWorld::SpawnCandle(const KeepMarker& marker)
	{
		const glm::vec3 floor = m_Map.TileCenter(marker.Tile);

		SpawnDecor("CandleWax", floor + glm::vec3(0.0f, k_CandleHeight * 0.5f, 0.0f), { k_CandleWidth, k_CandleHeight, k_CandleWidth }, COLOR_WAX, m_WaxMaterial)
			.GetComponent<MeshRendererComponent>().Shadows = ShadowCasting::Off;

		const glm::vec3 flameCenter = floor + glm::vec3(0.0f, k_CandleHeight + k_CandleFlameDiameter * 0.4f, 0.0f);

		DecorFlame flame;
		flame.Core = SpawnGlow("CandleFlame", flameCenter, k_CandleFlameDiameter, m_FlameMaterial, COLOR_EMBER);
		flame.Emitter = SpawnEmitter(m_Scene, "CandleParticles", m_Effects.Candle.get(), flameCenter + glm::vec3(0.0f, k_CandleFlameDiameter * FLAME_EMITTER_RISE, 0.0f));
		flame.Light = SpawnPointLight("CandleLight", flameCenter + glm::vec3(0.0f, k_CandleLightRise, 0.0f), CANDLE_LIGHT_INTENSITY, CANDLE_LIGHT_RANGE);
		flame.Kind = FlameKind::Candle;
		flame.BaseIntensity = CANDLE_LIGHT_INTENSITY;
		return flame;
	}

	FlaskSpot KeepWorld::SpawnFlask(const KeepMarker& marker)
	{
		FlaskSpot flask;
		flask.Position = m_Map.TileCenter(marker.Tile);

		Entity body = m_Scene.CreateEntity("FlaskBody");
		auto& bodyTransform = body.AddComponent<Transform3DComponent>();
		bodyTransform.Position = flask.Position + glm::vec3(0.0f, k_FlaskBodySize.y * 0.5f, 0.0f);
		bodyTransform.Scale = k_FlaskBodySize;
		body.AddComponent<MeshRendererComponent>(MeshRendererComponent(m_FlameMesh, COLOR_OIL)).Material = m_FlaskMaterial;

		flask.Parts.push_back(body);
		flask.Parts.push_back(SpawnDecor("FlaskNeck", flask.Position + glm::vec3(0.0f, k_FlaskNeckCenter, 0.0f), k_FlaskNeckSize, COLOR_OIL, m_FlaskMaterial));
		flask.Parts.push_back(SpawnDecor("FlaskCork", flask.Position + glm::vec3(0.0f, k_FlaskCorkCenter, 0.0f), k_FlaskCorkSize, COLOR_BRASS, m_BrassMaterial));
		return flask;
	}

	size_t KeepWorld::CollectFlasks(const glm::vec3& feet, size_t maxCount)
	{
		size_t collected = 0;
		for (FlaskSpot& flask : m_FlaskSpots)
		{
			if (collected >= maxCount)
				break;
			if (flask.Collected)
				continue;

			const glm::vec2 offset(flask.Position.x - feet.x, flask.Position.z - feet.z);
			if (glm::dot(offset, offset) > FLASK_PICKUP_RADIUS * FLASK_PICKUP_RADIUS)
				continue;

			flask.Collected = true;
			for (Entity part : flask.Parts)
				part.Destroy();
			flask.Parts.clear();
			++collected;
		}
		return collected;
	}

	void KeepWorld::SetWallHidden(WallRect& wall, bool hidden)
	{
		if (hidden == wall.Hidden)
			return;

		wall.Hidden = hidden;
		SetCutAway(wall.Wall, hidden);
		SetCutAway(wall.Cap, hidden);
		for (Entity mounted : wall.Mounted)
			SetVisible(mounted, !hidden);
		for (Entity emitter : wall.Emitters)
			emitter.GetComponent<ParticleEmitterComponent>().Playing = !hidden;
	}

	void KeepWorld::UpdateCutaway(const glm::vec3& eye, const glm::vec3& target)
	{
		const glm::vec3 padding(CUTAWAY_PADDING, 0.0f, CUTAWAY_PADDING);
		for (WallRect& wall : m_Walls)
			SetWallHidden(wall, SegmentHitsBox(eye, target, wall.Min - padding, wall.Max + padding));
	}

	void KeepWorld::HideSouthWalls(const TileRect& room)
	{
		const int southRow = room.Row + room.Height;
		for (WallRect& wall : m_Walls)
		{
			const bool onSouthRow = wall.Tiles.Row <= southRow && southRow < wall.Tiles.Row + wall.Tiles.Height;
			const bool overRoom = wall.Tiles.Col < room.Col + room.Width && room.Col < wall.Tiles.Col + wall.Tiles.Width;
			if (onSouthRow && overRoom)
				SetWallHidden(wall, true);
		}
	}

}
