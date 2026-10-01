#include "Detection.h"
#include "GameMath.h"
#include "GameTuning.h"
#include "KeepWorld.h"
#include "Lantern.h"
#include "Player.h"
#include "Wardens.h"

#include <algorithm>
#include <cmath>

namespace
{
	using namespace Dingo;

	constexpr size_t k_SampleCount = Detection::k_SampleCount;
	constexpr float k_SampleHeights[k_SampleCount] = { SAMPLE_FEET, SAMPLE_CHEST, SAMPLE_HEAD };
	constexpr size_t k_ChestSample = 1;

	// A cone along -Z (the eye's local Direction) whose slant edges are one unit long, so scaling it
	// by the eye's Range draws the edge of its range sphere; the cap bulges to that sphere's tip.
	Mesh* CreateConeMesh(float outerAngleDeg, uint32_t segments)
	{
		const float outer = glm::radians(outerAngleDeg);
		const float ringRadius = std::sin(outer);
		const float ringDepth = -std::cos(outer);

		std::vector<MeshVertex> vertices;
		std::vector<uint32_t> indices;
		vertices.push_back({ { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 0.0f } });
		for (uint32_t i = 0; i < segments; ++i)
		{
			const float angle = 2.0f * GameMath::PI * static_cast<float>(i) / static_cast<float>(segments);
			const glm::vec3 point(ringRadius * std::cos(angle), ringRadius * std::sin(angle), ringDepth);
			vertices.push_back({ point, point, { 0.0f, 0.0f } });
		}
		const uint32_t tip = static_cast<uint32_t>(vertices.size());
		vertices.push_back({ { 0.0f, 0.0f, -1.0f }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 0.0f } });

		for (uint32_t i = 0; i < segments; ++i)
		{
			const uint32_t a = 1 + i;
			const uint32_t b = 1 + (i + 1) % segments;
			indices.insert(indices.end(), { 0, a, b, tip, b, a });
		}
		return Mesh::Create(vertices, indices);
	}

	Entity SpawnDebugEntity(Scene& scene, const char* name, Mesh* mesh, Material* material)
	{
		Entity entity = scene.CreateEntity(name);
		entity.AddComponent<Transform3DComponent>();
		entity.AddComponent<MeshRendererComponent>(MeshRendererComponent(mesh, COLOR_DEBUG_BASE)).Material = material;
		return entity;
	}
}

namespace Dingo
{

	Detection::Detection(Scene& scene, const KeepWorld& world, size_t wardenCount, bool debugView, bool canCatch)
		: m_Scene(scene), m_CanCatch(canCatch), m_DebugView(debugView)
	{
		for (const DecorFlame& flame : world.GetFlames())
		{
			if (flame.Kind == FlameKind::Sconce)
				m_Sconces.push_back({ flame.Light, flame.BaseIntensity });
		}
		for (const BrazierSpot& brazier : world.GetBraziers())
			m_Braziers.push_back(brazier.Light);

		if (m_DebugView)
			CreateDebugView(wardenCount);
	}

	Detection::~Detection()
	{
		DestroyAndDelete(m_ConeMaterial);
		DestroyAndDelete(m_DotMaterial);
		DestroyAndDelete(m_SeenMaterial);
		DestroyAndDelete(m_UnseenMaterial);
		delete m_ConeMesh;
		for (Mesh* mesh : m_FootprintMeshes)
			delete mesh;
	}

	bool Detection::HasLineOfSight(const glm::vec3& eye, const glm::vec3& target) const
	{
		const Physics3D* physics = m_Scene.GetPhysics3D();
		if (!physics)
			return false;

		const glm::vec3 offset = target - eye;
		const float distance = glm::length(offset);
		if (distance <= SIGHT_SLACK)
			return true;

		// The player is a character controller, never a body, so anything hit short of it is in the way.
		RayCastHit3D hit;
		if (!physics->RayCast(Ray(eye, offset / distance), distance, hit))
			return true;
		return hit.Fraction * distance > distance - SIGHT_SLACK;
	}

	// Gameplay state, not what the frame draws: a sconce is always lit, at its base intensity (the
	// light LOD fades its component), and a brazier is lit while its light is enabled. Candles never
	// count, so which of them the LOD keeps cannot change what a warden notices.
	bool Detection::IsFlameLit(const glm::vec3& point) const
	{
		for (const Sconce& sconce : m_Sconces)
		{
			Entity entity = sconce.Light;
			if (!entity.IsValid())
				continue;

			PointLight light = entity.GetComponent<PointLightComponent>().ToLight(entity.GetComponent<Transform3DComponent>());
			light.Intensity = sconce.BaseIntensity;
			if (GetLightAttenuation(light, point) >= BEACON_FLAME_WEIGHT)
				return true;
		}

		for (Entity entity : m_Braziers)
		{
			if (!entity.IsValid())
				continue;

			const auto& brazier = entity.GetComponent<PointLightComponent>();
			if (brazier.Enabled && GetLightAttenuation(brazier.ToLight(entity.GetComponent<Transform3DComponent>()), point) >= BEACON_FLAME_WEIGHT)
				return true;
		}
		return false;
	}

	std::optional<size_t> Detection::Update(float deltaTime, Wardens& wardens, const Player& player, const Lantern& lantern)
	{
		const glm::vec3 feet = player.GetPosition();
		glm::vec3 samples[k_SampleCount];
		for (size_t s = 0; s < k_SampleCount; ++s)
			samples[s] = feet + glm::vec3(0.0f, k_SampleHeights[s], 0.0f);
		const glm::vec3& chest = samples[k_ChestSample];

		// A bigger flame is seen from further: the lantern's reach scales the notice range, and
		// standing in a brazier's or sconce's light sets a floor under it.
		Entity lanternEntity = lantern.GetLight();
		const auto& lanternLight = lanternEntity.GetComponent<PointLightComponent>();
		float noticeRange = lanternLight.Enabled ? BEACON_LANTERN_SCALE * lanternLight.Range : 0.0f;
		if (IsFlameLit(chest))
			noticeRange = std::max(noticeRange, BEACON_FLAME_RANGE);
		const float fieldCos = std::cos(glm::radians(BEACON_FIELD_DEG * 0.5f));

		std::fill(std::begin(m_SampleSeen), std::end(m_SampleSeen), false);
		std::optional<size_t> caught;
		for (size_t i = 0; i < wardens.GetCount(); ++i)
		{
			Entity eyeEntity = wardens.GetEye(i);
			const auto& eye = eyeEntity.GetComponent<SpotLightComponent>();
			const SpotLight eyeLight = eye.ToLight(eyeEntity.GetComponent<Transform3DComponent>());

			bool coneSeen = false;
			float coneWeight = 0.0f;
			for (size_t s = 0; s < k_SampleCount; ++s)
			{
				const float weight = eye.Enabled ? GetLightAttenuation(eyeLight, samples[s]) : 0.0f;
				if (weight >= SEEN_WEIGHT && HasLineOfSight(eyeLight.Position, samples[s]))
				{
					coneSeen = true;
					coneWeight = std::max(coneWeight, weight);
					m_SampleSeen[s] = true;
				}
			}

			bool beacon = false;
			if (!coneSeen && eye.Enabled && noticeRange > 0.0f)
			{
				const glm::vec3 toChest = chest - eyeLight.Position;
				const glm::vec2 flat(toChest.x, toChest.z);
				const float distance = glm::length(flat);
				const glm::vec3 forward = wardens.GetForward(i);
				const bool inField = distance <= 0.0f || glm::dot(flat / distance, glm::vec2(forward.x, forward.z)) >= fieldCos;
				beacon = distance <= noticeRange && inField && HasLineOfSight(eyeLight.Position, chest);
			}

			const float previous = wardens.GetSuspicion(i);
			float suspicion = previous;
			if (coneSeen)
				suspicion += deltaTime * (SUSPICION_CONE_BASE + SUSPICION_CONE_SCALE * coneWeight);
			else if (beacon)
				suspicion = std::min(previous + deltaTime * SUSPICION_BEACON_RATE, std::max(previous, SUSPICION_ALERT));
			else
				suspicion -= deltaTime * SUSPICION_DECAY;

			const glm::vec3 wardenFeet = wardens.GetFeet(i);
			const bool touching = glm::length(glm::vec2(feet.x - wardenFeet.x, feet.z - wardenFeet.z)) < TOUCH_DISTANCE;
			if (touching)
				suspicion = std::max(suspicion, SUSPICION_TOUCH);
			suspicion = std::clamp(suspicion, 0.0f, 1.0f);

			const bool perceived = coneSeen || beacon || touching;
			wardens.SetSuspicion(i, suspicion, perceived && suspicion >= SUSPICION_INVESTIGATE ? std::optional<glm::vec3>(feet) : std::nullopt);
			if (m_CanCatch && suspicion >= 1.0f && !caught)
				caught = i;
		}

		UpdateDebugView(wardens, player);
		return caught;
	}

	void Detection::CreateDebugView(size_t wardenCount)
	{
		Renderer3D& renderer3D = Application::Get().GetRenderer3D();

		m_ConeMesh = CreateConeMesh(WARDEN_EYE_OUTER_DEG, DEBUG_CONE_SEGMENTS);
		m_ConeMaterial = renderer3D.CreateLitMaterial(MaterialParams()
			.SetDebugName("DebugCone")
			.SetFillMode(FillMode::Wireframe)
			.SetCullMode(CullMode::None)
			.SetEmissiveColor(DEBUG_CONE_COLOR)
			.SetEmissiveStrength(1.0f));
		m_DotMaterial = renderer3D.CreateLitMaterial(MaterialParams()
			.SetDebugName("DebugFootprint")
			.SetEmissiveColor(DEBUG_DOT_COLOR)
			.SetEmissiveStrength(1.0f));
		m_SeenMaterial = renderer3D.CreateLitMaterial(MaterialParams()
			.SetDebugName("DebugSampleSeen")
			.SetEmissiveColor(DEBUG_SEEN_COLOR)
			.SetEmissiveStrength(1.0f));
		m_UnseenMaterial = renderer3D.CreateLitMaterial(MaterialParams()
			.SetDebugName("DebugSampleUnseen")
			.SetEmissiveColor(DEBUG_UNSEEN_COLOR)
			.SetEmissiveStrength(1.0f));

		for (size_t i = 0; i < wardenCount; ++i)
		{
			m_Cones.push_back(SpawnDebugEntity(m_Scene, "DebugCone", m_ConeMesh, m_ConeMaterial));
			m_Footprints.push_back(SpawnDebugEntity(m_Scene, "DebugFootprint", nullptr, m_DotMaterial));
			m_FootprintMeshes.push_back(nullptr);
			m_FootprintEyes.emplace_back();
		}

		for (size_t s = 0; s < k_SampleCount; ++s)
		{
			Entity sample = SpawnDebugEntity(m_Scene, "DebugSample", renderer3D.GetSphereMesh(), m_UnseenMaterial);
			sample.GetComponent<Transform3DComponent>().Scale = glm::vec3(DEBUG_SAMPLE_SIZE);
			m_Samples.push_back(sample);
		}
	}

	void Detection::UpdateDebugView(const Wardens& wardens, const Player& player)
	{
		if (!m_DebugView)
			return;

		const bool hasPhysics = m_Scene.GetPhysics3D() != nullptr;
		for (size_t i = 0; i < wardens.GetCount() && i < m_Cones.size(); ++i)
		{
			Entity eyeEntity = wardens.GetEye(i);
			const auto& eye = eyeEntity.GetComponent<SpotLightComponent>();
			const auto& eyeTransform = eyeEntity.GetComponent<Transform3DComponent>();

			auto& cone = m_Cones[i].GetComponent<Transform3DComponent>();
			cone.Position = eyeTransform.Position;
			cone.Rotation = eyeTransform.Rotation;
			cone.Scale = glm::vec3(eye.Range);
			m_Cones[i].GetComponent<MeshRendererComponent>().Visible = eye.Enabled;

			EyeState state;
			state.Position = eyeTransform.Position;
			state.Rotation = eyeTransform.Rotation;
			state.Range = eye.Range;
			state.Enabled = eye.Enabled;
			state.HasPhysics = hasPhysics;
			if (state == m_FootprintEyes[i])
				continue;

			m_FootprintEyes[i] = state;
			delete m_FootprintMeshes[i];
			m_FootprintMeshes[i] = eye.Enabled ? BuildFootprint(eye.ToLight(eyeTransform)) : nullptr;
			auto& footprint = m_Footprints[i].GetComponent<MeshRendererComponent>();
			footprint.Mesh = m_FootprintMeshes[i];
			footprint.Visible = m_FootprintMeshes[i] != nullptr;
		}

		// Every sample sits inside the player's body, so each is drawn out in front of it, towards the
		// fixed-yaw camera; its colour is still that sample's own verdict.
		const glm::vec3 feet = player.GetPosition();
		const float yaw = glm::radians(CAMERA_YAW_DEG);
		const glm::vec3 towardsCamera(std::sin(yaw), 0.0f, std::cos(yaw));
		for (size_t s = 0; s < m_Samples.size(); ++s)
		{
			m_Samples[s].GetComponent<Transform3DComponent>().Position = feet + glm::vec3(0.0f, k_SampleHeights[s], 0.0f) + towardsCamera * DEBUG_SAMPLE_OFFSET;
			m_Samples[s].GetComponent<MeshRendererComponent>().Material = m_SampleSeen[s] ? m_SeenMaterial : m_UnseenMaterial;
		}
	}

	// Every grid point on the floor where a feet sample would count for this eye, the same test
	// Update makes, drawn as a dot so a capture shows the footprint over the lit pool.
	Mesh* Detection::BuildFootprint(const SpotLight& eye)
	{
		m_DotVertices.clear();
		m_DotIndices.clear();

		const float spacing = DEBUG_DOT_SPACING;
		const float half = DEBUG_DOT_SIZE * 0.5f;
		const glm::vec3 up(0.0f, 1.0f, 0.0f);
		const int minX = static_cast<int>(std::floor((eye.Position.x - eye.Range) / spacing));
		const int maxX = static_cast<int>(std::ceil((eye.Position.x + eye.Range) / spacing));
		const int minZ = static_cast<int>(std::floor((eye.Position.z - eye.Range) / spacing));
		const int maxZ = static_cast<int>(std::ceil((eye.Position.z + eye.Range) / spacing));

		for (int iz = minZ; iz <= maxZ; ++iz)
		{
			for (int ix = minX; ix <= maxX; ++ix)
			{
				const glm::vec3 point((static_cast<float>(ix) + 0.5f) * spacing, SAMPLE_FEET, (static_cast<float>(iz) + 0.5f) * spacing);
				if (GetLightAttenuation(eye, point) < SEEN_WEIGHT || !HasLineOfSight(eye.Position, point))
					continue;

				const uint32_t base = static_cast<uint32_t>(m_DotVertices.size());
				m_DotVertices.push_back({ { point.x - half, DEBUG_DOT_LIFT, point.z - half }, up, { 0.0f, 0.0f } });
				m_DotVertices.push_back({ { point.x + half, DEBUG_DOT_LIFT, point.z - half }, up, { 1.0f, 0.0f } });
				m_DotVertices.push_back({ { point.x + half, DEBUG_DOT_LIFT, point.z + half }, up, { 1.0f, 1.0f } });
				m_DotVertices.push_back({ { point.x - half, DEBUG_DOT_LIFT, point.z + half }, up, { 0.0f, 1.0f } });
				m_DotIndices.insert(m_DotIndices.end(), { base, base + 1, base + 2, base, base + 2, base + 3 });
			}
		}

		return m_DotVertices.empty() ? nullptr : Mesh::Create(m_DotVertices, m_DotIndices);
	}

}
