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
		auto& renderer = entity.AddComponent<MeshRendererComponent>(MeshRendererComponent(mesh, COLOR_DEBUG_BASE));
		renderer.Material = material;
		renderer.Shadows = ShadowCasting::Off;
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
		DestroyAndDelete(m_ShadowedMaterial);
		delete m_ConeMesh;
		for (Mesh* mesh : m_FootprintMeshes)
			delete mesh;
	}

	// Drawing only: the cone stops at the first wall a level ray along the eye's heading meets, as its
	// light does, instead of reaching through it into the next room.
	float Detection::DebugConeLength(const glm::vec3& eye, const glm::vec3& forward, float range) const
	{
		const Physics3D* physics = m_Scene.GetPhysics3D();
		const glm::vec3 level(forward.x, 0.0f, forward.z);
		const float levelLength = glm::length(level);
		if (!physics || levelLength < 1e-3f)
			return range;

		RayCastHit3D hit;
		if (!physics->RayCast(Ray(eye, level / levelLength), range * levelLength, hit))
			return range;
		return (std::min)(range, hit.Fraction * range);
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
	// light LOD fades its component), and a brazier is lit while its light is enabled, where its own
	// shadow doesn't cover the point. Candles never count, so which of them the LOD keeps cannot
	// change what a warden notices. Sconces cast no shadow, and hang on walls.
	bool Detection::IsFlameLit(const glm::vec3& point)
	{
		bool lit = false;
		for (Entity entity : m_Braziers)
		{
			if (!entity.IsValid())
				continue;
			const auto& brazier = entity.GetComponent<PointLightComponent>();
			if (!brazier.Enabled)
				continue;

			// Asked even out of the light's reach, so the answer is current the moment the point enters it.
			const float visibility = m_Scene.GetLightVisibility(entity, point, PROBE_KEY_CHEST);
			lit = GetLightAttenuation(brazier.ToLight(entity.GetComponent<Transform3DComponent>()), point) * visibility >= BEACON_FLAME_WEIGHT || lit;
		}

		for (const Sconce& sconce : m_Sconces)
		{
			Entity entity = sconce.Light;
			if (lit || !entity.IsValid())
				continue;

			PointLight light = entity.GetComponent<PointLightComponent>().ToLight(entity.GetComponent<Transform3DComponent>());
			light.Intensity = sconce.BaseIntensity;
			lit = GetLightAttenuation(light, point) >= BEACON_FLAME_WEIGHT;
		}
		return lit;
	}

	std::optional<size_t> Detection::Update(float deltaTime, Wardens& wardens, const Player& player, const Lantern& lantern)
	{
		const glm::vec3 feet = player.GetPosition();
		glm::vec3 samples[k_SampleCount];
		for (size_t s = 0; s < k_SampleCount; ++s)
			samples[s] = feet + glm::vec3(0.0f, k_SampleHeights[s], 0.0f);
		const glm::vec3& chest = samples[k_ChestSample];

		// A bigger flame is seen from further: the lantern's reach scales the notice range, while its
		// glow reaches the warden's eye, and standing in a brazier's or sconce's light sets a floor
		// under it.
		Entity lanternEntity = lantern.GetLight();
		const auto& lanternLight = lanternEntity.GetComponent<PointLightComponent>();
		const float lanternRange = lanternLight.Enabled ? BEACON_LANTERN_SCALE * lanternLight.Range : 0.0f;
		const bool flameLit = IsFlameLit(chest);
		const float fieldCos = std::cos(glm::radians(BEACON_FIELD_DEG * 0.5f));

		std::fill(std::begin(m_SampleVerdicts), std::end(m_SampleVerdicts), SampleVerdict::Unseen);
		std::optional<size_t> caught;
		for (size_t i = 0; i < wardens.GetCount(); ++i)
		{
			Entity eyeEntity = wardens.GetEye(i);
			const auto& eye = eyeEntity.GetComponent<SpotLightComponent>();
			const SpotLight eyeLight = eye.ToLight(eyeEntity.GetComponent<Transform3DComponent>());

			bool coneSeen = false;
			float coneWeight = 0.0f;
			for (size_t s = 0; s < k_SampleCount && eye.Enabled; ++s)
			{
				// Asked even outside the cone, so the answer is current the moment the sample enters it.
				const float visibility = m_Scene.GetLightVisibility(eyeEntity, samples[s], static_cast<uint32_t>(s));
				const float light = GetLightAttenuation(eyeLight, samples[s]);
				const float weight = light * visibility;
				if (light < SEEN_WEIGHT || !HasLineOfSight(eyeLight.Position, samples[s]))
					continue;
				if (weight >= SEEN_WEIGHT)
				{
					coneSeen = true;
					coneWeight = std::max(coneWeight, weight);
					m_SampleVerdicts[s] = SampleVerdict::Seen;
				}
				else if (m_SampleVerdicts[s] == SampleVerdict::Unseen)
				{
					m_SampleVerdicts[s] = SampleVerdict::Shadowed;
				}
			}

			const float lanternVisibility = eye.Enabled && lanternRange > 0.0f
				? m_Scene.GetLightVisibility(lanternEntity, eyeLight.Position, PROBE_KEY_LANTERN + static_cast<uint32_t>(i)) : 0.0f;

			bool beacon = false;
			if (!coneSeen && eye.Enabled)
			{
				const glm::vec3 toChest = chest - eyeLight.Position;
				const glm::vec2 flat(toChest.x, toChest.z);
				const float distance = glm::length(flat);
				const glm::vec3 forward = wardens.GetForward(i);
				const bool inField = distance <= 0.0f || glm::dot(flat / distance, glm::vec2(forward.x, forward.z)) >= fieldCos;
				const bool lanternSeen = distance <= lanternRange && lanternVisibility >= BEACON_LANTERN_VISIBILITY;
				const bool flameSeen = flameLit && distance <= BEACON_FLAME_RANGE;
				beacon = inField && (lanternSeen || flameSeen) && HasLineOfSight(eyeLight.Position, chest);
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
		m_ShadowedMaterial = renderer3D.CreateLitMaterial(MaterialParams()
			.SetDebugName("DebugSampleShadowed")
			.SetEmissiveColor(DEBUG_SHADOWED_COLOR)
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
			cone.Scale = glm::vec3(DebugConeLength(eyeTransform.Position, eyeTransform.Rotation * eye.Direction, eye.Range));
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
			const SampleVerdict verdict = m_SampleVerdicts[s];
			m_Samples[s].GetComponent<MeshRendererComponent>().Material = verdict == SampleVerdict::Seen ? m_SeenMaterial
				: verdict == SampleVerdict::Shadowed ? m_ShadowedMaterial : m_UnseenMaterial;
		}
	}

	// Every grid point on the floor where a feet sample would count for this eye by its light and
	// the sight ray, drawn as a dot so a capture shows the footprint over the lit pool. Shadows don't
	// enter it (a grid would need far more probes than a scene has); the samples show them.
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

	void Detection::LogHideCheck(const Wardens& wardens, const Player& player, const Lantern& lantern)
	{
		const glm::vec3 chest = player.GetPosition() + glm::vec3(0.0f, SAMPLE_CHEST, 0.0f);
		DE_INFO("[HideCheck] chest at ({:.2f}, {:.2f}, {:.2f}): flame-lit {}", chest.x, chest.y, chest.z, IsFlameLit(chest) ? "yes" : "no");
		for (size_t i = 0; i < m_Braziers.size(); ++i)
		{
			Entity entity = m_Braziers[i];
			if (!entity.IsValid() || !entity.GetComponent<PointLightComponent>().Enabled)
				continue;

			const glm::vec3 position = entity.GetComponent<Transform3DComponent>().Position;
			const float light = GetLightAttenuation(entity.GetComponent<PointLightComponent>().ToLight(entity.GetComponent<Transform3DComponent>()), chest);
			if (light <= 0.0f)
			{
				DE_INFO("[HideCheck] brazier at ({:.1f}, {:.1f}): out of reach of the chest", position.x, position.z);
				continue;
			}
			const float visibility = m_Scene.GetLightVisibility(entity, chest, PROBE_KEY_CHEST);
			DE_INFO("[HideCheck] brazier at ({:.1f}, {:.1f}): light {:.3f} at the chest (lit without shadows: {}), visibility {:.3f}, shadowed {:.3f} (lit: {})",
				position.x, position.z, light, light >= BEACON_FLAME_WEIGHT ? "yes" : "no", visibility, light * visibility, light * visibility >= BEACON_FLAME_WEIGHT ? "yes" : "no");
		}

		const auto name = [](SampleVerdict verdict) { return verdict == SampleVerdict::Seen ? "seen" : verdict == SampleVerdict::Shadowed ? "in shadow" : "unseen"; };
		DE_INFO("[HideCheck] samples, best of every warden: feet {}, chest {}, head {}", name(m_SampleVerdicts[0]), name(m_SampleVerdicts[1]), name(m_SampleVerdicts[2]));

		Entity lanternEntity = lantern.GetLight();
		const bool lanternLit = lanternEntity.GetComponent<PointLightComponent>().Enabled;
		for (size_t i = 0; i < wardens.GetCount(); ++i)
		{
			const glm::vec3 eye = wardens.GetEye(i).GetComponent<Transform3DComponent>().Position;
			const float visibility = lanternLit ? m_Scene.GetLightVisibility(lanternEntity, eye, PROBE_KEY_LANTERN + static_cast<uint32_t>(i)) : 0.0f;
			DE_INFO("[HideCheck] warden {}: the lantern's visibility at its eye {:.3f}, suspicion {:.2f}", i + 1, visibility, wardens.GetSuspicion(i));
		}
	}

}
