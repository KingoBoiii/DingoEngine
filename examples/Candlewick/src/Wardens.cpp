#include "Wardens.h"
#include "Audio.h"
#include "GameMath.h"
#include "GameTuning.h"

#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cmath>
#include <iterator>

namespace
{
	using namespace Dingo;

	constexpr size_t k_NoPoint = static_cast<size_t>(-1);

	constexpr glm::vec3 k_LegsSize = { 0.44f, 0.84f, 0.3f };
	constexpr glm::vec3 k_TorsoSize = { 0.6f, 0.62f, 0.4f };
	constexpr glm::vec3 k_HelmSize = { 0.32f, 0.32f, 0.34f };
	constexpr glm::vec3 k_VisorSize = { 0.24f, 0.05f, 0.04f };
	constexpr glm::vec3 k_LampGlassSize = { 0.13f, 0.17f, 0.13f };
	constexpr glm::vec3 k_LampCapSize = { 0.17f, 0.04f, 0.17f };
	constexpr float k_LegsCenter = 0.42f;
	constexpr float k_TorsoCenter = 1.15f;
	constexpr float k_HelmCenter = 1.63f;
	constexpr float k_VisorCenter = 1.67f;
	constexpr float k_VisorForward = 0.18f;
	constexpr float k_LampCapRise = 0.105f;

	constexpr float k_PathSampleStep = 0.1f;
	constexpr float k_ArrivalEpsilon = 1e-3f;
	constexpr float k_InvSqrtTwo = 0.70710678f;

	const char* StateName(Wardens::State state)
	{
		switch (state)
		{
		case Wardens::State::Patrol: return "Patrol";
		case Wardens::State::Investigate: return "Investigate";
		case Wardens::State::Return: return "Return";
		}
		return "?";
	}

	// Yaw about +Y that turns the engine's forward (-Z) onto `direction`.
	float YawOf(const glm::vec3& direction)
	{
		return std::atan2(-direction.x, -direction.z);
	}

	glm::quat YawRotation(float yaw)
	{
		return glm::angleAxis(yaw, glm::vec3(0.0f, 1.0f, 0.0f));
	}

	glm::vec3 Flat(const glm::vec3& v)
	{
		return glm::vec3(v.x, 0.0f, v.z);
	}
}

namespace Dingo
{

	Wardens::Wardens(Scene& scene, const KeepMap& map, GameAudio& audio, bool frozen, bool rangeClamp)
		: m_Scene(scene), m_Map(map), m_Audio(audio), m_Frozen(frozen), m_RangeClamp(rangeClamp)
	{
		Renderer3D& renderer3D = Application::Get().GetRenderer3D();

		m_ArmourMaterial = renderer3D.CreateLitMaterial(MaterialParams()
			.SetDebugName("WardenArmour")
			.SetRoughness(ARMOUR_ROUGHNESS)
			.SetSpecular(ARMOUR_SPECULAR)
			.SetEmissiveColor(ARMOUR_EMISSIVE_COLOR)
			.SetEmissiveStrength(ARMOUR_EMISSIVE));

		m_VisorMaterial = renderer3D.CreateLitMaterial(MaterialParams()
			.SetDebugName("WardenVisor")
			.SetEmissiveColor(WARDEN_LIGHT_COLOR)
			.SetEmissiveStrength(WARDEN_LAMP_EMISSIVE));

		m_LampMaterial = renderer3D.CreateLitMaterial(MaterialParams()
			.SetDebugName("WardenLamp")
			.SetEmissiveColor(WARDEN_LIGHT_COLOR)
			.SetEmissiveStrength(WARDEN_LAMP_EMISSIVE));

		const glm::vec3 markerColors[] = { MARKER_CALM_COLOR, MARKER_SUSPICIOUS_COLOR, MARKER_ALERT_COLOR };
		const char* markerNames[] = { "WardenMarkerCalm", "WardenMarkerSuspicious", "WardenMarkerAlert" };
		for (size_t i = 0; i < std::size(m_MarkerMaterials); ++i)
		{
			m_MarkerMaterials[i] = renderer3D.CreateLitMaterial(MaterialParams()
				.SetDebugName(markerNames[i])
				.SetEmissiveColor(markerColors[i])
				.SetEmissiveStrength(WARDEN_MARKER_EMISSIVE));
		}

		const std::vector<WardenRoute>& routes = map.GetWardenRoutes();
		m_Wardens.resize(routes.size());
		for (size_t i = 0; i < routes.size(); ++i)
		{
			m_Wardens[i].Index = i;
			m_Wardens[i].Room = routes[i].Room;
			BuildLoop(m_Wardens[i], routes[i]);
			Spawn(m_Wardens[i], i);
		}

		Reset();
		DE_INFO("Candlewick: {} wardens on patrol{}{}", m_Wardens.size(), frozen ? " (frozen)" : "", rangeClamp ? "" : " (range clamp off)");
	}

	Wardens::~Wardens()
	{
		DestroyAndDelete(m_ArmourMaterial);
		DestroyAndDelete(m_VisorMaterial);
		DestroyAndDelete(m_LampMaterial);
		for (Material*& material : m_MarkerMaterials)
			DestroyAndDelete(material);
	}

	void Wardens::BuildLoop(Warden& warden, const WardenRoute& route) const
	{
		const size_t count = route.Waypoints.size();
		std::vector<bool> isWaypoint;
		for (size_t i = 0; i < count; ++i)
		{
			const std::vector<glm::ivec2> leg = m_Map.FindPath(route.Waypoints[i], route.Waypoints[(i + 1) % count], route.Room);
			if (leg.size() < 2)
				continue;
			for (size_t t = 0; t + 1 < leg.size(); ++t)
			{
				warden.LoopTiles.push_back(leg[t]);
				isWaypoint.push_back(t == 0);
			}
		}

		const size_t tiles = warden.LoopTiles.size();
		if (tiles == 0)
		{
			const glm::ivec2 tile = count > 0 ? route.Waypoints[0] : glm::ivec2(0);
			warden.LoopTiles = { tile };
			warden.LoopTileNext = { 0 };
			warden.Loop = { { m_Map.TileCenter(tile), true } };
			return;
		}

		// Straight runs collapse to their ends, so a warden only stops to turn at a corner.
		std::vector<size_t> pointOfTile(tiles, k_NoPoint);
		for (size_t t = 0; t < tiles; ++t)
		{
			const glm::ivec2 in = warden.LoopTiles[t] - warden.LoopTiles[(t + tiles - 1) % tiles];
			const glm::ivec2 out = warden.LoopTiles[(t + 1) % tiles] - warden.LoopTiles[t];
			if (isWaypoint[t] || in != out)
			{
				pointOfTile[t] = warden.Loop.size();
				warden.Loop.push_back({ m_Map.TileCenter(warden.LoopTiles[t]), isWaypoint[t] });
			}
		}

		warden.LoopTileNext.resize(tiles);
		for (size_t t = 0; t < tiles; ++t)
		{
			size_t next = (t + 1) % tiles;
			while (pointOfTile[next] == k_NoPoint)
				next = (next + 1) % tiles;
			warden.LoopTileNext[t] = pointOfTile[next];
		}
	}

	void Wardens::AddPart(Warden& warden, const char* name, Mesh* mesh, const glm::vec3& offset, const glm::vec3& size, const glm::vec4& color, Material* material)
	{
		Entity entity = m_Scene.CreateEntity(name);
		entity.AddComponent<Transform3DComponent>().Scale = size;
		entity.AddComponent<MeshRendererComponent>(MeshRendererComponent(mesh, color)).Material = material;
		warden.Parts.push_back({ entity, offset });
	}

	void Wardens::Spawn(Warden& warden, size_t index)
	{
		Renderer3D& renderer3D = Application::Get().GetRenderer3D();
		Mesh* box = renderer3D.GetBoxMesh();

		AddPart(warden, "WardenLegs", box, { 0.0f, k_LegsCenter, 0.0f }, k_LegsSize, COLOR_ARMOUR, m_ArmourMaterial);
		AddPart(warden, "WardenTorso", box, { 0.0f, k_TorsoCenter, 0.0f }, k_TorsoSize, COLOR_ARMOUR, m_ArmourMaterial);
		AddPart(warden, "WardenHelm", box, { 0.0f, k_HelmCenter, 0.0f }, k_HelmSize, COLOR_ARMOUR, m_ArmourMaterial);
		AddPart(warden, "WardenVisor", box, { 0.0f, k_VisorCenter, -k_VisorForward }, k_VisorSize, glm::vec4(WARDEN_LIGHT_COLOR, 1.0f), m_VisorMaterial);
		AddPart(warden, "WardenLampGlass", box, WARDEN_LAMP_OFFSET, k_LampGlassSize, glm::vec4(WARDEN_LIGHT_COLOR, 1.0f), m_LampMaterial);
		AddPart(warden, "WardenLampCap", box, WARDEN_LAMP_OFFSET + glm::vec3(0.0f, k_LampCapRise, 0.0f), k_LampCapSize, COLOR_ARMOUR, m_ArmourMaterial);

		warden.Lamp = m_Scene.CreateEntity("WardenLamp");
		warden.Lamp.AddComponent<Transform3DComponent>();
		warden.Lamp.AddComponent<PointLightComponent>(PointLightComponent(WARDEN_LIGHT_COLOR, WARDEN_LAMP_INTENSITY, WARDEN_LAMP_RANGE));

		warden.Eye = m_Scene.CreateEntity("WardenEye");
		warden.Eye.AddComponent<Transform3DComponent>();
		auto& eye = warden.Eye.AddComponent<SpotLightComponent>(SpotLightComponent(WARDEN_LIGHT_COLOR, WARDEN_EYE_INTENSITY, WARDEN_EYE_RANGE));
		eye.InnerConeAngle = WARDEN_EYE_INNER_DEG;
		eye.OuterConeAngle = WARDEN_EYE_OUTER_DEG;
		eye.Direction = glm::vec3(0.0f, 0.0f, -1.0f);

		warden.Marker = m_Scene.CreateEntity("WardenMarker");
		warden.Marker.AddComponent<Transform3DComponent>().Scale = glm::vec3(WARDEN_MARKER_SIZE);
		warden.Marker.AddComponent<MeshRendererComponent>(MeshRendererComponent(renderer3D.GetSphereMesh(), glm::vec4(MARKER_CALM_COLOR, 1.0f)));

		const glm::ivec2 start = warden.LoopTiles.front();
		DE_INFO("Candlewick: warden {} patrols {} from ({}, {}), {} turns per loop", index + 1,
			m_Map.GetRooms()[warden.Room].Name, start.x, start.y, warden.Loop.size());
	}

	void Wardens::Reset()
	{
		for (Warden& warden : m_Wardens)
		{
			warden.Feet = warden.Loop.front().Position;
			warden.Next = warden.Loop.size() > 1 ? 1 : 0;
			const glm::vec3 ahead = warden.Loop[warden.Next].Position - warden.Feet;
			warden.Yaw = glm::dot(ahead, ahead) > 0.0f ? YawOf(ahead) : 0.0f;
			warden.Mode = State::Patrol;
			warden.Pause = 0.0f;
			warden.Path.clear();
			warden.PathNext = 0;
			warden.Looking = false;
			warden.Suspicion = 0.0f;
			warden.Request.reset();
			warden.StepDistance = 0.0f;

			Place(warden);
			ClampRange(warden, 0.0f, true);
			UpdateMarker(warden);
		}
	}

	glm::vec3 Wardens::GetForward(size_t index) const
	{
		return YawRotation(m_Wardens[index].Yaw) * glm::vec3(0.0f, 0.0f, -1.0f);
	}

	void Wardens::SetSuspicion(size_t index, float suspicion, const std::optional<glm::ivec2>& lastSeen)
	{
		Warden& warden = m_Wardens[index];
		warden.Suspicion = suspicion;
		if (lastSeen)
			warden.Request = lastSeen;
		UpdateMarker(warden);
	}

	void Wardens::Update(float deltaTime)
	{
		for (size_t i = 0; i < m_Wardens.size(); ++i)
		{
			Warden& warden = m_Wardens[i];
			const glm::vec3 before = warden.Feet;
			if (!m_Frozen)
				Think(warden, i, deltaTime);
			warden.Request.reset();

			warden.StepDistance += glm::length(Flat(warden.Feet - before));
			if (warden.StepDistance >= WARDEN_STEP_DISTANCE)
			{
				warden.StepDistance = 0.0f;
				m_Audio.PlayAt(Sfx::WardenStep, warden.Feet);
			}

			Place(warden);
			ClampRange(warden, deltaTime, false);
		}
	}

	void Wardens::SetState(Warden& warden, size_t index, State state)
	{
		if (warden.Mode == state)
			return;

		if (state == State::Investigate)
		{
			DE_INFO("Candlewick: warden {} ({}) {} -> {}: sighting ({}, {}), goal ({}, {})", index + 1, m_Map.GetRooms()[warden.Room].Name,
				StateName(warden.Mode), StateName(state), warden.Target.x, warden.Target.y, warden.Goal.x, warden.Goal.y);
			m_Audio.PlayAt(Sfx::Alert, warden.Feet + glm::vec3(0.0f, WARDEN_EYE_HEIGHT, 0.0f));
		}
		else
			DE_INFO("Candlewick: warden {} ({}) {} -> {}", index + 1, m_Map.GetRooms()[warden.Room].Name, StateName(warden.Mode), StateName(state));
		warden.Mode = state;
	}

	void Wardens::Think(Warden& warden, size_t index, float deltaTime)
	{
		if (warden.Request && (warden.Mode != State::Investigate || *warden.Request != warden.Target))
			BeginInvestigate(warden, index, *warden.Request);

		switch (warden.Mode)
		{
		case State::Patrol:
			Patrol(warden, deltaTime);
			break;

		case State::Investigate:
			if (!warden.Looking)
			{
				const bool close = glm::length(Flat(m_Map.TileCenter(warden.Goal) - warden.Feet)) <= WARDEN_INVESTIGATE_STOP;
				if (close || FollowPath(warden, WARDEN_INVESTIGATE_SPEED, deltaTime))
				{
					warden.Looking = true;
					warden.LookTime = 0.0f;
					warden.LookYaw = warden.Yaw;
				}
				break;
			}

			warden.LookTime += deltaTime;
			warden.Yaw = ApproachAngle(warden.Yaw,
				warden.LookYaw + glm::radians(WARDEN_LOOK_SWEEP_DEG) * std::sin(2.0f * PI * warden.LookTime / WARDEN_LOOK_TIME),
				WARDEN_TURN_SPEED * deltaTime);
			if (warden.LookTime >= WARDEN_LOOK_TIME)
				BeginReturn(warden, index);
			break;

		case State::Return:
			if (FollowPath(warden, WARDEN_PATROL_SPEED, deltaTime))
			{
				warden.Next = warden.LoopTileNext[warden.ReturnTile];
				warden.Pause = 0.0f;
				SetState(warden, index, State::Patrol);
			}
			break;
		}
	}

	void Wardens::Patrol(Warden& warden, float deltaTime)
	{
		if (warden.Pause > 0.0f)
		{
			warden.Pause -= deltaTime;
			const glm::vec3 ahead = Flat(warden.Loop[warden.Next].Position - warden.Feet);
			if (glm::dot(ahead, ahead) > k_ArrivalEpsilon * k_ArrivalEpsilon)
				warden.Yaw = ApproachAngle(warden.Yaw, YawOf(ahead), WARDEN_TURN_SPEED * deltaTime);
			return;
		}

		float timeLeft = deltaTime;
		for (size_t hops = 0; timeLeft > 0.0f && hops <= warden.Loop.size(); ++hops)
		{
			const LoopPoint& next = warden.Loop[warden.Next];
			if (!StepTowards(warden, next.Position, WARDEN_PATROL_SPEED, timeLeft))
				return;

			warden.Next = (warden.Next + 1) % warden.Loop.size();
			if (next.Waypoint)
			{
				warden.Pause = WARDEN_WAYPOINT_PAUSE;
				return;
			}
		}
	}

	void Wardens::BeginInvestigate(Warden& warden, size_t index, const glm::ivec2& tile)
	{
		glm::ivec2 goal = tile;
		if (m_Map.RoomOf(tile) != warden.Room)
			goal = m_Map.FindNearestRoomTile(tile, warden.Room).value_or(m_Map.TileOf(warden.Feet));

		std::vector<glm::ivec2> goals;
		if (m_Map.IsPatrolFloor(goal, warden.Room))
		{
			goals.push_back(goal);
		}
		else
		{
			for (int dz = -1; dz <= 1; ++dz)
				for (int dx = -1; dx <= 1; ++dx)
					if (m_Map.IsPatrolFloor(goal + glm::ivec2(dx, dz), warden.Room))
						goals.push_back(goal + glm::ivec2(dx, dz));
		}

		const glm::ivec2 here = m_Map.TileOf(warden.Feet);
		const std::vector<glm::ivec2> tiles = m_Map.FindPathToNearest(here, goals, warden.Room);
		warden.Target = tile;
		warden.Goal = tiles.empty() ? here : tiles.back();
		warden.Path = tiles.empty() ? std::vector<glm::vec3>() : SmoothPath(warden.Feet, tiles, warden.Room);
		warden.PathNext = 0;
		warden.Looking = false;
		SetState(warden, index, State::Investigate);
	}

	void Wardens::BeginReturn(Warden& warden, size_t index)
	{
		const std::vector<glm::ivec2> tiles = m_Map.FindPathToNearest(m_Map.TileOf(warden.Feet), warden.LoopTiles, warden.Room);
		if (tiles.empty())
		{
			DE_WARN("Candlewick: warden {} has no way back to its route; it starts the route over", index + 1);
			warden.Feet = warden.Loop.front().Position;
			warden.Next = warden.Loop.size() > 1 ? 1 : 0;
			warden.Looking = false;
			SetState(warden, index, State::Patrol);
			return;
		}

		const auto reached = std::find(warden.LoopTiles.begin(), warden.LoopTiles.end(), tiles.back());
		warden.ReturnTile = static_cast<size_t>(std::distance(warden.LoopTiles.begin(), reached));
		warden.Path = SmoothPath(warden.Feet, tiles, warden.Room);
		warden.PathNext = 0;
		warden.Looking = false;
		SetState(warden, index, State::Return);
	}

	bool Wardens::FollowPath(Warden& warden, float speed, float deltaTime)
	{
		float timeLeft = deltaTime;
		while (warden.PathNext < warden.Path.size() && timeLeft > 0.0f)
		{
			if (!StepTowards(warden, warden.Path[warden.PathNext], speed, timeLeft))
				break;
			++warden.PathNext;
		}
		return warden.PathNext >= warden.Path.size();
	}

	bool Wardens::StepTowards(Warden& warden, const glm::vec3& target, float speed, float& timeLeft)
	{
		const glm::vec3 offset = Flat(target - warden.Feet);
		const float distance = glm::length(offset);
		if (distance < k_ArrivalEpsilon)
			return true;

		const glm::vec3 direction = offset / distance;
		const float desired = YawOf(direction);
		warden.Yaw = ApproachAngle(warden.Yaw, desired, WARDEN_TURN_SPEED * timeLeft);

		if (IsBlockedAhead(warden, direction))
		{
			timeLeft = 0.0f;
			return false;
		}

		// Slows into sharp turns instead of sliding sideways.
		const float step = speed * std::max(0.0f, std::cos(WrapAngle(desired - warden.Yaw))) * timeLeft;
		if (step >= distance)
		{
			warden.Feet = glm::vec3(target.x, warden.Feet.y, target.z);
			timeLeft -= timeLeft * distance / step;
			return true;
		}

		warden.Feet += direction * step;
		timeLeft = 0.0f;
		return false;
	}

	// The lower index has the right of way, so two wardens that meet head on never both wait.
	bool Wardens::IsBlockedAhead(const Warden& warden, const glm::vec3& direction) const
	{
		for (size_t other = 0; other < warden.Index; ++other)
		{
			const glm::vec3 offset = Flat(m_Wardens[other].Feet - warden.Feet);
			const float distance = glm::length(offset);
			if (distance < WARDEN_YIELD_DISTANCE && glm::dot(offset, direction) > WARDEN_YIELD_AHEAD * distance)
				return true;
		}
		return false;
	}

	std::vector<glm::vec3> Wardens::SmoothPath(const glm::vec3& start, const std::vector<glm::ivec2>& tiles, int room) const
	{
		std::vector<glm::vec3> points = { start };
		for (const glm::ivec2& tile : tiles)
			points.push_back(m_Map.TileCenter(tile));

		std::vector<glm::vec3> smooth;
		size_t anchor = 0;
		while (anchor + 1 < points.size())
		{
			size_t reach = anchor + 1;
			for (size_t candidate = points.size() - 1; candidate > anchor + 1; --candidate)
			{
				if (IsClearLine(points[anchor], points[candidate], room))
				{
					reach = candidate;
					break;
				}
			}
			smooth.push_back(points[reach]);
			anchor = reach;
		}
		return smooth;
	}

	bool Wardens::IsClearLine(const glm::vec3& from, const glm::vec3& to, int room) const
	{
		const float c = WARDEN_PATH_CLEARANCE;
		const float d = c * k_InvSqrtTwo;
		const glm::vec3 offsets[] = { { 0.0f, 0.0f, 0.0f }, { c, 0.0f, 0.0f }, { -c, 0.0f, 0.0f }, { 0.0f, 0.0f, c }, { 0.0f, 0.0f, -c },
			{ d, 0.0f, d }, { d, 0.0f, -d }, { -d, 0.0f, d }, { -d, 0.0f, -d } };

		const float length = glm::length(Flat(to - from));
		const int samples = std::max(1, static_cast<int>(std::ceil(length / k_PathSampleStep)));
		for (int s = 0; s <= samples; ++s)
		{
			const glm::vec3 point = glm::mix(from, to, static_cast<float>(s) / static_cast<float>(samples));
			for (const glm::vec3& offset : offsets)
			{
				if (!m_Map.IsPatrolFloor(m_Map.TileOf(point + offset), room))
					return false;
			}
		}
		return true;
	}

	void Wardens::Place(Warden& warden)
	{
		const glm::quat yaw = YawRotation(warden.Yaw);
		for (Part& part : warden.Parts)
		{
			auto& transform = part.Visual.GetComponent<Transform3DComponent>();
			transform.Position = warden.Feet + yaw * part.Offset;
			transform.Rotation = yaw;
		}

		warden.Lamp.GetComponent<Transform3DComponent>().Position = warden.Feet + yaw * WARDEN_LAMP_OFFSET;

		auto& eye = warden.Eye.GetComponent<Transform3DComponent>();
		eye.Position = warden.Feet + glm::vec3(0.0f, WARDEN_EYE_HEIGHT, 0.0f) + yaw * glm::vec3(0.0f, 0.0f, -WARDEN_EYE_FORWARD);
		eye.Rotation = yaw * glm::angleAxis(glm::radians(-WARDEN_EYE_PITCH_DEG), glm::vec3(1.0f, 0.0f, 0.0f));

		warden.Marker.GetComponent<Transform3DComponent>().Position = warden.Feet + glm::vec3(0.0f, WARDEN_MARKER_HEIGHT, 0.0f);
	}

	// The eye looks down at 25 degrees, so its own axis would always find the floor about 4 m out;
	// the wall it faces is found by a level ray from the eye instead.
	// The range shrinks at once, so the cone never shines through the wall it turns to face, but
	// grows at a limited rate, so the range sphere never jumps into view and snaps a light off.
	void Wardens::ClampRange(Warden& warden, float deltaTime, bool snap)
	{
		float target = WARDEN_EYE_RANGE;
		const Physics3D* physics = m_Scene.GetPhysics3D();
		if (m_RangeClamp && physics)
		{
			const glm::vec3 origin = warden.Eye.GetComponent<Transform3DComponent>().Position;
			const glm::vec3 forward = YawRotation(warden.Yaw) * glm::vec3(0.0f, 0.0f, -1.0f);
			RayCastHit3D hit;
			if (physics->RayCast(Ray(origin, forward), WARDEN_EYE_RANGE, hit))
				target = std::min(WARDEN_EYE_RANGE, hit.Fraction * WARDEN_EYE_RANGE + WARDEN_EYE_RANGE_MARGIN);
		}

		auto& eye = warden.Eye.GetComponent<SpotLightComponent>();
		eye.Range = (snap || target <= eye.Range) ? target : std::min(target, eye.Range + WARDEN_EYE_RANGE_GROWTH * deltaTime);
	}

	void Wardens::UpdateMarker(Warden& warden)
	{
		const int level = warden.Suspicion >= SUSPICION_ALERT ? 2 : warden.Suspicion >= SUSPICION_INVESTIGATE ? 1 : 0;
		if (level == warden.MarkerLevel)
			return;

		warden.MarkerLevel = level;
		warden.Marker.GetComponent<MeshRendererComponent>().Material = m_MarkerMaterials[level];
	}

}
