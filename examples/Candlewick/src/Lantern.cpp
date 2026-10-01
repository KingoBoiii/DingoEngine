#include "Lantern.h"
#include "GameTuning.h"
#include "Player.h"

#include <algorithm>
#include <cmath>
#include <iterator>

namespace
{
	using namespace Dingo;

	constexpr glm::vec3 k_HandOffsets[] = { { -0.5f, 0.0f, 0.12f }, { -0.32f, 0.0f, 0.4f }, { 0.0f, 0.0f, 0.5f } };
	constexpr float k_HandHeight = 0.95f;
	constexpr float k_HandFollow = 14.0f;
	constexpr float k_WallMargin = 0.15f;
	constexpr float k_Scale = 1.4f;

	constexpr glm::vec3 k_GlassSize = { 0.11f, 0.17f, 0.11f };
	constexpr glm::vec3 k_BaseSize = { 0.17f, 0.035f, 0.17f };
	constexpr glm::vec3 k_CapSize = { 0.19f, 0.04f, 0.19f };
	constexpr glm::vec3 k_PostSize = { 0.025f, 0.17f, 0.025f };
	constexpr glm::vec3 k_HandleSize = { 0.035f, 0.07f, 0.035f };
	constexpr float k_PostSpread = 0.07f;
	constexpr float k_BaseDrop = 0.1025f;
	constexpr float k_CapRise = 0.105f;
	constexpr float k_HandleRise = 0.165f;

	constexpr float k_FlickerRateA = 29.0f;
	constexpr float k_FlickerRateB = 13.0f;
	constexpr float k_FlickerPhase = 0.7f;

	float Flicker(float clock)
	{
		return 1.0f - LANTERN_FLICKER_DEPTH * std::abs(std::sin(clock * k_FlickerRateA) * std::sin(clock * k_FlickerRateB + k_FlickerPhase));
	}

	// The part of `offset` from `from` that stays k_WallMargin clear of the first wall, as a fraction. 1 when nothing is in the way.
	float FreeFraction(const Physics3D* physics, const glm::vec3& from, const glm::vec3& offset)
	{
		const float length = glm::length(offset);
		if (!physics || length <= 0.0f)
			return 1.0f;

		const float reach = length + k_WallMargin;
		RayCastHit3D hit;
		if (!physics->RayCast(Ray(from, offset / length), reach, hit))
			return 1.0f;
		return std::clamp((hit.Fraction * reach - k_WallMargin) / length, 0.0f, 1.0f);
	}
}

namespace Dingo
{

	Lantern::Lantern(Scene& scene, const Player& player, Material* frameMaterial, float startOil, bool burns)
		: m_Scene(scene), m_Oil(std::clamp(startOil, 0.0f, OIL_MAX)), m_Burns(burns)
	{
		m_GlassMaterial = Application::Get().GetRenderer3D().CreateLitMaterial(MaterialParams()
			.SetDebugName("LanternGlass")
			.SetRoughness(GLASS_ROUGHNESS)
			.SetEmissiveColor(LANTERN_COLOR));

		AddPart(scene, "LanternGlass", glm::vec3(0.0f), k_GlassSize, COLOR_GLASS, m_GlassMaterial);
		AddPart(scene, "LanternBase", glm::vec3(0.0f, -k_BaseDrop, 0.0f), k_BaseSize, COLOR_BRASS, frameMaterial);
		AddPart(scene, "LanternCap", glm::vec3(0.0f, k_CapRise, 0.0f), k_CapSize, COLOR_BRASS, frameMaterial);
		AddPart(scene, "LanternHandle", glm::vec3(0.0f, k_HandleRise, 0.0f), k_HandleSize, COLOR_BRASS, frameMaterial);
		for (const float x : { -k_PostSpread, k_PostSpread })
			for (const float z : { -k_PostSpread, k_PostSpread })
				AddPart(scene, "LanternPost", glm::vec3(x, 0.0f, z), k_PostSize, COLOR_BRASS, frameMaterial);

		m_Light = scene.CreateEntity("Lantern");
		m_Light.AddComponent<Transform3DComponent>();
		m_Light.AddComponent<PointLightComponent>(PointLightComponent(LANTERN_COLOR, LANTERN_INTENSITY, LANTERN_RANGE_MAX));

		m_State = m_Oil > 0.0f ? State::Lit : State::Snuffed;

		Place(player, 0.0f);
		Apply();
	}

	Lantern::~Lantern()
	{
		DestroyAndDelete(m_GlassMaterial);
	}

	void Lantern::AddPart(Scene& scene, const char* name, const glm::vec3& offset, const glm::vec3& size, const glm::vec4& color, Material* material)
	{
		Entity entity = scene.CreateEntity(name);
		entity.AddComponent<Transform3DComponent>().Scale = size * k_Scale;
		entity.AddComponent<MeshRendererComponent>(MeshRendererComponent(Application::Get().GetRenderer3D().GetBoxMesh(), color)).Material = material;
		m_Parts.push_back({ entity, offset * k_Scale });
	}

	bool Lantern::CanRelight() const
	{
		return m_Oil > OIL_RELIGHT_COST;
	}

	size_t Lantern::GetFlaskCapacity() const
	{
		return static_cast<size_t>(std::max(0.0f, (OIL_MAX - m_Oil) / OIL_PER_FLASK));
	}

	void Lantern::AddOil(float amount)
	{
		m_Oil = std::clamp(m_Oil + amount, 0.0f, OIL_MAX);
	}

	void Lantern::Update(float deltaTime, Player& player)
	{
		m_Clock += deltaTime;
		const State before = m_State;

		const bool toggle = Input::IsKeyPressed(Key::Q) || Input::IsGamepadButtonPressed(GamepadButton::X);
		if (toggle && m_State == State::Lit)
		{
			m_State = State::Snuffed;
		}
		else if (toggle && m_State == State::Snuffed && CanRelight())
		{
			m_Oil -= OIL_RELIGHT_COST;
			m_StrikeTime = 0.0f;
			m_State = State::Striking;
		}
		else if (toggle && m_State == State::Striking)
		{
			m_SnuffQueued = true;
		}

		if (m_State == State::Lit)
		{
			if (m_Burns)
				m_Oil = std::max(0.0f, m_Oil - OIL_BURN_PER_SECOND * deltaTime);
			if (m_Oil <= 0.0f)
				m_State = State::Snuffed;
		}
		else if (m_State == State::Striking)
		{
			m_StrikeTime += deltaTime;
			if (m_StrikeTime >= LANTERN_STRIKE_TIME)
				m_State = m_SnuffQueued ? State::Snuffed : State::Lit;
		}

		if (m_State != State::Striking)
			m_SnuffQueued = false;

		if (m_State != before)
		{
			const char* name = m_State == State::Lit ? "lit" : m_State == State::Striking ? "striking" : IsOutOfOil() ? "out of oil" : "snuffed";
			DE_INFO("Candlewick: lantern {} (oil {:.1f})", name, m_Oil);
		}

		player.SetMovementLocked(m_State == State::Striking);

		Place(player, deltaTime);
		Apply();
	}

	glm::vec3 Lantern::FindHand(const glm::vec3& axis, const glm::quat& facing) const
	{
		const Physics3D* physics = m_Scene.GetPhysics3D();
		for (const glm::vec3& offset : k_HandOffsets)
		{
			if (FreeFraction(physics, axis, facing * offset) >= 1.0f)
				return offset;
		}

		const glm::vec3& front = k_HandOffsets[std::size(k_HandOffsets) - 1];
		return front * FreeFraction(physics, axis, facing * front);
	}

	void Lantern::Place(const Player& player, float deltaTime)
	{
		const glm::quat facing = player.GetFacing();
		const glm::vec3 axis = player.GetPosition() + glm::vec3(0.0f, k_HandHeight, 0.0f);

		const glm::vec3 target = FindHand(axis, facing);
		m_Hand = deltaTime > 0.0f ? glm::mix(m_Hand, target, std::min(1.0f, k_HandFollow * deltaTime)) : target;

		const glm::vec3 hand = m_Hand * FreeFraction(m_Scene.GetPhysics3D(), axis, facing * m_Hand);
		const glm::vec3 core = axis + facing * hand;

		for (Part& part : m_Parts)
		{
			auto& transform = part.Visual.GetComponent<Transform3DComponent>();
			transform.Position = core + facing * part.Offset;
			transform.Rotation = facing;
		}
		m_Light.GetComponent<Transform3DComponent>().Position = core;
	}

	void Lantern::Apply()
	{
		float strength = 0.0f;
		if (m_State == State::Lit)
			strength = 1.0f;
		else if (m_State == State::Striking)
			strength = std::clamp(m_StrikeTime / LANTERN_STRIKE_TIME, 0.0f, 1.0f);

		const float fill = m_Oil / OIL_MAX;
		const bool guttering = m_State == State::Lit && m_Burns && m_Oil < LANTERN_FLICKER_OIL;
		const float flicker = guttering ? Flicker(m_Clock) : 1.0f;

		auto& light = m_Light.GetComponent<PointLightComponent>();
		light.Enabled = strength > 0.0f;
		light.Range = glm::mix(LANTERN_RANGE_MIN, LANTERN_RANGE_MAX, fill);
		light.Intensity = LANTERN_INTENSITY * strength * flicker;

		m_GlassMaterial->SetEmissiveStrength(glm::mix(LANTERN_EMISSIVE_MIN, LANTERN_EMISSIVE_MAX, fill) * strength * flicker);
	}

}
