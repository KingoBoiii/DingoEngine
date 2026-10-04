#include "ArenaWorld.h"
#include "Audio.h"
#include "GameTuning.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <utility>

namespace
{
	using namespace Dingo;

	constexpr float k_UnitSphereRadius = 0.5f;
	constexpr float k_SideAngle = 2.0f * std::numbers::pi_v<float> / ARENA_SIDES;
	constexpr float k_HalfSideAngle = 0.5f * k_SideAngle;

	glm::quat Yaw(float radians)
	{
		return glm::angleAxis(radians, glm::vec3(0.0f, 1.0f, 0.0f));
	}

	// Whether the segment crosses a box of half-size `half` turned `yaw` about Y at `center`: slabs, in the box's own frame.
	bool SegmentHitsBox(const glm::vec3& from, const glm::vec3& to, const glm::vec3& center, const glm::vec3& half, float yaw)
	{
		const glm::quat inverse = Yaw(-yaw);
		const glm::vec3 a = inverse * (from - center);
		const glm::vec3 delta = inverse * (to - center) - a;
		float enter = 0.0f;
		float leave = 1.0f;
		for (int axis = 0; axis < 3; ++axis)
		{
			if (std::abs(delta[axis]) < 1.0e-6f)
			{
				if (std::abs(a[axis]) > half[axis])
					return false;
				continue;
			}

			float first = (-half[axis] - a[axis]) / delta[axis];
			float last = (half[axis] - a[axis]) / delta[axis];
			if (first > last)
				std::swap(first, last);
			enter = std::max(enter, first);
			leave = std::min(leave, last);
			if (enter > leave)
				return false;
		}
		return true;
	}
}

namespace Dingo
{

	float GetArenaApothem()
	{
		return ARENA_RADIUS * std::cos(k_HalfSideAngle);
	}

	ArenaWorld::ArenaWorld(Scene& scene, const GameAudio* audio)
		: m_Scene(scene)
	{
		Renderer3D& renderer3D = Application::Get().GetRenderer3D();
		m_BoxMesh = renderer3D.GetBoxMesh();
		m_FlameMesh = Mesh::CreateSphere(k_UnitSphereRadius, FLAME_MESH_RINGS, FLAME_MESH_SEGMENTS);

		m_FloorMaterial = renderer3D.CreateLitMaterial(MaterialParams().SetDebugName("ArenaFloor").SetRoughness(FLOOR_ROUGHNESS));
		m_WallMaterial = renderer3D.CreateLitMaterial(MaterialParams().SetDebugName("ArenaWall").SetRoughness(WALL_ROUGHNESS));
		m_BrazierMaterial = renderer3D.CreateLitMaterial(MaterialParams().SetDebugName("ArenaBrazier").SetRoughness(BRAZIER_ROUGHNESS));
		m_FlameMaterial = renderer3D.CreateLitMaterial(MaterialParams()
			.SetDebugName("ArenaFlame")
			.SetEmissiveColor(FLAME_COLOR)
			.SetEmissiveStrength(FLAME_EMISSIVE));

		Entity ambient = scene.CreateEntity("Ambient");
		ambient.AddComponent<Transform3DComponent>();
		ambient.AddComponent<AmbientLightComponent>(AmbientLightComponent(AMBIENT_COLOR, AMBIENT_INTENSITY));

		Entity moon = scene.CreateEntity("Moon");
		DirectionalLightComponent& moonLight = moon.AddComponent<DirectionalLightComponent>();
		moonLight.Direction = MOON_DIRECTION;
		moonLight.Color = MOON_COLOR;
		moonLight.Intensity = MOON_INTENSITY;
		moonLight.Ambient = 0.0f;

		BuildFloor();
		BuildWalls();
		BuildBraziers(audio);
		DE_INFO("Marionette: arena built, {} m across, {} braziers", 2.0f * ARENA_RADIUS, BRAZIER_COUNT);
	}

	ArenaWorld::~ArenaWorld()
	{
		AudioEngine& engine = Application::Get().GetAudioEngine();
		for (const AudioSoundId sound : m_Crackles)
		{
			if (sound != k_InvalidSound)
				engine.Stop(sound);
		}

		DestroyAndDelete(m_FloorMaterial);
		DestroyAndDelete(m_WallMaterial);
		DestroyAndDelete(m_BrazierMaterial);
		DestroyAndDelete(m_FlameMaterial);
		delete m_FlameMesh;
	}

	std::vector<glm::vec3> ArenaWorld::GetRimPoints() const
	{
		std::vector<glm::vec3> points;
		for (int i = 0; i < ARENA_SIDES; ++i)
		{
			const float angle = k_SideAngle * (static_cast<float>(i) + 0.5f);
			const float rim = (GetArenaApothem() + ARENA_WALL_THICKNESS) / std::cos(k_HalfSideAngle);
			for (const float y : { 0.0f, ARENA_WALL_HEIGHT })
				points.emplace_back(std::cos(angle) * rim, y, std::sin(angle) * rim);
		}
		return points;
	}

	Entity ArenaWorld::SpawnSolid(const char* name, const glm::vec3& center, const glm::vec3& size, float yawRadians, const glm::vec4& color, Material* material)
	{
		Entity entity = m_Scene.CreateEntity(name);
		auto& transform = entity.AddComponent<Transform3DComponent>();
		transform.Position = center;
		transform.Rotation = Yaw(yawRadians);
		transform.Scale = size;

		entity.AddComponent<MeshRendererComponent>(MeshRendererComponent(m_BoxMesh, color)).Material = material;
		entity.AddComponent<RigidBody3DComponent>(RigidBody3DComponent(BodyType3D::Static));
		entity.AddComponent<BoxCollider3DComponent>();
		return entity;
	}

	void ArenaWorld::BuildFloor()
	{
		// Six strips whose short ends are opposite sides of the polygon cover all of it.
		const float apothem = GetArenaApothem();
		const float strip = 2.0f * apothem * std::tan(k_HalfSideAngle);
		for (int i = 0; i < ARENA_SIDES / 2; ++i)
		{
			SpawnSolid("Floor", glm::vec3(0.0f, -ARENA_FLOOR_THICKNESS * 0.5f, 0.0f),
				glm::vec3(2.0f * apothem, ARENA_FLOOR_THICKNESS, strip), -k_SideAngle * static_cast<float>(i), COLOR_FLOOR, m_FloorMaterial);
		}
	}

	void ArenaWorld::BuildWalls()
	{
		const float apothem = GetArenaApothem();
		const float length = 2.0f * (apothem + ARENA_WALL_THICKNESS) * std::tan(k_HalfSideAngle);
		const float distance = apothem + ARENA_WALL_THICKNESS * 0.5f;
		for (int i = 0; i < ARENA_SIDES; ++i)
		{
			const float normal = k_SideAngle * static_cast<float>(i);
			const glm::vec3 center(std::cos(normal) * distance, ARENA_WALL_HEIGHT * 0.5f, std::sin(normal) * distance);
			const glm::vec3 size(length, ARENA_WALL_HEIGHT, ARENA_WALL_THICKNESS);
			const float yaw = 0.5f * std::numbers::pi_v<float> - normal;
			Occluder wall;
			wall.Parts.push_back(SpawnSolid("Wall", center, size, yaw, COLOR_WALL, m_WallMaterial));
			wall.Center = center;
			wall.HalfSize = 0.5f * size;
			wall.Yaw = yaw;
			m_Occluders.push_back(std::move(wall));
		}
	}

	void ArenaWorld::BuildBraziers(const GameAudio* audio)
	{
		for (int i = 0; i < BRAZIER_COUNT; ++i)
		{
			const float angle = glm::radians(BRAZIER_ANGLE_OFFSET_DEG) + 2.0f * std::numbers::pi_v<float> * static_cast<float>(i) / BRAZIER_COUNT;
			const glm::vec3 floor(std::cos(angle) * BRAZIER_RING_RADIUS, 0.0f, std::sin(angle) * BRAZIER_RING_RADIUS);

			Occluder brazier;
			brazier.Parts.push_back(SpawnSolid("BrazierBase", floor + glm::vec3(0.0f, BRAZIER_BASE_HEIGHT * 0.5f, 0.0f),
				glm::vec3(BRAZIER_BASE_WIDTH, BRAZIER_BASE_HEIGHT, BRAZIER_BASE_WIDTH), 0.0f, COLOR_BRAZIER, m_BrazierMaterial));
			brazier.Parts.push_back(SpawnSolid("BrazierBowl", floor + glm::vec3(0.0f, BRAZIER_BASE_HEIGHT + BRAZIER_BOWL_HEIGHT * 0.5f, 0.0f),
				glm::vec3(BRAZIER_BOWL_WIDTH, BRAZIER_BOWL_HEIGHT, BRAZIER_BOWL_WIDTH), 0.0f, COLOR_BRAZIER, m_BrazierMaterial));

			const glm::vec3 flame = floor + glm::vec3(0.0f, BRAZIER_BASE_HEIGHT + BRAZIER_BOWL_HEIGHT + BRAZIER_FLAME_RISE, 0.0f);

			Entity core = m_Scene.CreateEntity("BrazierFlame");
			auto& coreTransform = core.AddComponent<Transform3DComponent>();
			coreTransform.Position = flame;
			coreTransform.Scale = glm::vec3(BRAZIER_FLAME_DIAMETER);
			core.AddComponent<MeshRendererComponent>(MeshRendererComponent(m_FlameMesh, COLOR_FLAME)).Material = m_FlameMaterial;
			brazier.Parts.push_back(core);

			const float height = flame.y + 0.5f * BRAZIER_FLAME_DIAMETER;
			brazier.Center = glm::vec3(floor.x, 0.5f * height, floor.z);
			brazier.HalfSize = glm::vec3(0.5f * BRAZIER_BOWL_WIDTH, 0.5f * height, 0.5f * BRAZIER_BOWL_WIDTH);
			m_Occluders.push_back(std::move(brazier));

			if (audio)
				m_Crackles.push_back(audio->StartCrackle(flame, 1.0f + AUDIO_CRACKLE_PITCH_STEP * (static_cast<float>(i) - 0.5f * static_cast<float>(BRAZIER_COUNT - 1))));

			Entity light = m_Scene.CreateEntity("BrazierLight");
			light.AddComponent<Transform3DComponent>().Position = flame + glm::vec3(0.0f, BRAZIER_LIGHT_RISE, 0.0f);
			light.AddComponent<PointLightComponent>(PointLightComponent(FLAME_COLOR, BRAZIER_LIGHT_INTENSITY, BRAZIER_LIGHT_RANGE));
		}
	}

	void ArenaWorld::SetVisible(Occluder& occluder, bool visible)
	{
		for (Entity& part : occluder.Parts)
			part.GetComponent<MeshRendererComponent>().Visible = visible;
		occluder.Hidden = !visible;
	}

	void ArenaWorld::UpdateOcclusion(float deltaTime, const glm::vec3& eye, std::span<const CameraSubject> subjects)
	{
		for (Occluder& occluder : m_Occluders)
		{
			const glm::vec3 half = occluder.HalfSize + glm::vec3(OCCLUSION_VIEW_RADIUS);
			bool blocks = false;
			for (const CameraSubject& subject : subjects)
			{
				for (const float fraction : OCCLUSION_SAMPLE_FRACTIONS)
				{
					const glm::vec3 toward = subject.Position + glm::vec3(0.0f, fraction * subject.Height, 0.0f) - eye;
					const float length = glm::length(toward);
					if (length > OCCLUSION_END_PAD)
						blocks = blocks || SegmentHitsBox(eye, eye + toward * ((length - OCCLUSION_END_PAD) / length), occluder.Center, half, occluder.Yaw);
				}
			}

			if (blocks)
			{
				occluder.ClearFor = 0.0f;
				if (!occluder.Hidden)
					SetVisible(occluder, false);
			}
			else
			{
				occluder.ClearFor += deltaTime;
				if (occluder.Hidden && occluder.ClearFor >= OCCLUSION_RESTORE_DELAY)
					SetVisible(occluder, true);
			}
		}
	}

}
