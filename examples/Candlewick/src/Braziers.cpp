#include "Braziers.h"
#include "Audio.h"
#include "Flicker.h"
#include "GameTuning.h"
#include "KeepWorld.h"
#include "Lantern.h"
#include "LightLod.h"
#include "Player.h"

#include <algorithm>

namespace
{
	constexpr int k_GatehouseRoom = 0;
}

namespace Dingo
{

	Braziers::Braziers(Scene& scene, const KeepMap& map, KeepWorld& world, LightLod& lightLod, GameAudio& audio, float startOil,
		const std::optional<glm::ivec2>& startTile, bool flicker, bool allLit)
		: m_Scene(scene), m_Map(map), m_World(world), m_LightLod(lightLod), m_Audio(audio), m_Flicker(flicker)
	{
		const std::vector<BrazierSpot>& spots = world.GetBraziers();
		m_Lit.assign(spots.size(), false);
		for (const BrazierSpot& spot : spots)
		{
			Entity light = spot.Light;
			m_BaseIntensity.push_back(light.GetComponent<PointLightComponent>().Intensity);
		}

		std::optional<size_t> first;
		size_t lit = 0;
		for (size_t i = 0; i < spots.size(); ++i)
		{
			const bool gatehouse = spots[i].Room == k_GatehouseRoom && !spots[i].IsAltar;
			if (gatehouse && !first)
				first = i;
			if (gatehouse || (allLit && !spots[i].IsAltar))
			{
				Light(i, false);
				++lit;
			}
		}

		if (startTile)
			m_Checkpoint.Tile = *startTile;
		else
			m_Checkpoint.Tile = first ? map.FindCheckpointTile(spots[*first].Tile) : map.GetRooms()[k_GatehouseRoom].Spawn;
		m_Checkpoint.Oil = std::max(startOil, CHECKPOINT_MIN_OIL);
		Flicker();
		DE_INFO("Candlewick: {} braziers, {} lit at the start; first checkpoint ({}, {}) with oil {:.0f}", spots.size(), lit,
			m_Checkpoint.Tile.x, m_Checkpoint.Tile.y, m_Checkpoint.Oil);
	}

	void Braziers::Light(size_t index, bool kindle)
	{
		const BrazierSpot& spot = m_World.GetBraziers()[index];
		m_Lit[index] = true;

		for (Entity emitter : { spot.Flame, spot.Embers, spot.Smoke })
		{
			if (emitter.IsValid())
				emitter.GetComponent<ParticleEmitterComponent>().Playing = true;
		}
		if (kindle && spot.Kindle.IsValid())
			m_Scene.EmitParticles(spot.Kindle, KINDLE_BURST_COUNT);

		Entity light = spot.Light;
		light.GetComponent<PointLightComponent>().Enabled = true;

		Entity core = spot.Core;
		auto& renderer = core.GetComponent<MeshRendererComponent>();
		renderer.Material = m_World.GetFlameMaterial();
		renderer.Color = COLOR_EMBER;

		if (m_Audio.GetCrackle())
		{
			auto& source = core.AddComponent<AudioSourceComponent>();
			source.Clip = m_Audio.GetCrackle();
			source.Looping = true;
			source.Spatialized = true;
			source.Volume = AUDIO_CRACKLE_VOLUME;
			source.Attenuation = GameAudio::CrackleFalloff();
			m_Scene.PlayAudioSource(core);
		}

		m_LightLod.AddGameplayLight(spot.Light);
	}

	Braziers::Outcome Braziers::Update(float deltaTime, const Player& player, Lantern& lantern, bool canLight)
	{
		if (m_Flicker)
			m_Clock += deltaTime;

		const std::vector<BrazierSpot>& spots = m_World.GetBraziers();
		const Lantern::State lanternState = lantern.GetState();
		const bool wantsOil = lantern.GetOil() < OIL_MAX || lanternState == Lantern::State::Snuffed;

		std::optional<size_t> target;
		if (canLight)
		{
			const glm::vec3 feet = player.GetPosition();
			float nearest = BRAZIER_REACH;
			for (size_t i = 0; i < spots.size(); ++i)
			{
				if (m_Lit[i] && !wantsOil)
					continue;

				const glm::vec3 center = m_Map.TileCenter(spots[i].Tile);
				const float distance = glm::length(glm::vec2(center.x - feet.x, center.z - feet.z));
				if (distance <= nearest)
				{
					nearest = distance;
					target = i;
				}
			}
		}

		if (target != m_Target)
		{
			m_Progress = 0.0f;
			m_Holding = false;
		}
		m_Target = target;
		m_Prompt = BrazierPrompt::None;

		Outcome outcome;
		const bool refuel = target && m_Lit[*target];
		if (!target || (!refuel && lanternState != Lantern::State::Lit))
		{
			m_Holding = false;
			m_Progress = 0.0f;
			if (target && lanternState == Lantern::State::Snuffed)
				m_Prompt = lantern.CanRelight() ? BrazierPrompt::NeedLantern : BrazierPrompt::NoOil;
		}
		else
		{
			m_Prompt = refuel ? BrazierPrompt::Refuel : spots[*target].IsAltar ? BrazierPrompt::LightAltar : BrazierPrompt::Light;

			// A hold begins on the press, so a key already down when a brazier comes into reach lights nothing.
			if (Input::IsKeyPressed(Key::E) || Input::IsGamepadButtonPressed(GamepadButton::A))
				m_Holding = true;
			else if (!Input::IsKeyDown(Key::E) && !Input::IsGamepadButtonDown(GamepadButton::A))
				m_Holding = false;

			m_Progress = m_Holding ? std::min(1.0f, m_Progress + deltaTime / BRAZIER_LIGHT_TIME) : 0.0f;
			if (m_Progress >= 1.0f)
			{
				const BrazierSpot& spot = spots[*target];
				if (refuel)
				{
					lantern.Refill();
					m_Audio.PlayAt(Sfx::Flask, player.GetPosition());
					outcome.Refuelled = true;
					DE_INFO("Candlewick: refilled the lantern at the brazier in {} (oil {:.0f})", m_Map.GetRooms()[spot.Room].Name, lantern.GetOil());
				}
				else
				{
					Light(*target, true);
					lantern.AddOil(OIL_MAX);

					Entity core = spot.Core;
					m_Audio.PlayAt(Sfx::Ignite, core.GetComponent<Transform3DComponent>().Position);

					if (spot.IsAltar)
					{
						DE_INFO("Candlewick: the altar is lit");
					}
					else
					{
						m_Checkpoint.Tile = m_Map.FindCheckpointTile(spot.Tile);
						m_Checkpoint.Oil = lantern.GetOil();
						DE_INFO("Candlewick: lit the brazier in {}; checkpoint ({}, {}) with oil {:.0f}", m_Map.GetRooms()[spot.Room].Name,
							m_Checkpoint.Tile.x, m_Checkpoint.Tile.y, m_Checkpoint.Oil);
					}
					outcome.Lit = target;
				}

				m_Target.reset();
				m_Progress = 0.0f;
				m_Holding = false;
				m_Prompt = BrazierPrompt::None;
			}
		}

		Flicker();
		return outcome;
	}

	void Braziers::Flicker()
	{
		const std::vector<BrazierSpot>& spots = m_World.GetBraziers();
		for (size_t i = 0; i < spots.size(); ++i)
		{
			if (!m_Lit[i])
				continue;

			const float factor = m_Flicker ? FlameFlicker(m_Clock, static_cast<float>(i) * BRAZIER_FLICKER_PHASE, BRAZIER_FLICKER_DEPTH) : 1.0f;
			Entity light = spots[i].Light;
			light.GetComponent<PointLightComponent>().Intensity = m_BaseIntensity[i] * factor;
		}

		if (!m_Flicker)
			return;

		m_World.GetFlameMaterial()->SetEmissiveStrength(FLAME_EMISSIVE * FlameFlicker(m_Clock, 0.0f, CORE_FLICKER_DEPTH));
		for (size_t i = 0; i < m_World.GetFlames().size(); ++i)
			m_LightLod.SetFlicker(i, FlameFlicker(m_Clock, static_cast<float>(i) * DECOR_FLICKER_PHASE, DECOR_FLICKER_DEPTH));
	}

}
