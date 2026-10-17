#include "Look/Vfx.h"
#include "GameTuning.h"

#include <algorithm>

namespace Dingo
{

	namespace
	{
		constexpr float k_Area = VFX_AREA_HALF_EXTENT;

		ParticleEffectParams BurstBase(const char* name, uint32_t count)
		{
			return ParticleEffectParams().SetDebugName(name).SetRate(0.0f).SetCapacity(count * 4);
		}
	}

	Vfx::Vfx(Scene& scene, bool enabled)
		: m_Scene(scene), m_Enabled(enabled)
	{
		if (!m_Enabled)
			return;

		m_RainEffect.reset(ParticleEffect::Create(ParticleEffectParams()
			.SetDebugName("Rain")
			.SetShape(ParticleShape::Box, { k_Area, 0.1f, k_Area })
			.SetRate(VFX_RAIN_RATE)
			.SetLifetime(0.9f, 1.1f)
			.SetSpeed(0.0f, 0.0f)
			.SetGravity({ 0.0f, -30.0f, 0.0f })
			.SetStartSize(0.025f, 0.04f)
			.SetEndSize(1.0f)
			.SetBlend(ParticleBlend::Alpha)
			.SetColors({ { 0.0f, { 0.6f, 0.7f, 0.85f, 0.0f } }, { 0.15f, { 0.6f, 0.7f, 0.85f, 0.55f } }, { 1.0f, { 0.6f, 0.7f, 0.85f, 0.4f } } })
			.SetSoftDistance(0.0f)));

		m_SnowEffect.reset(ParticleEffect::Create(ParticleEffectParams()
			.SetDebugName("Snow")
			.SetShape(ParticleShape::Box, { k_Area, 0.1f, k_Area })
			.SetRate(VFX_SNOW_RATE)
			.SetLifetime(4.5f, 6.0f)
			.SetSpeed(0.0f, 0.0f)
			.SetGravity({ 0.0f, -2.2f, 0.0f })
			.SetDrag(1.0f)
			.SetNoise(0.9f, 0.6f)
			.SetStartSize(0.05f, 0.09f)
			.SetEndSize(1.0f)
			.SetBlend(ParticleBlend::Alpha)
			.SetColors({ { 0.0f, { 0.95f, 0.97f, 1.0f, 0.0f } }, { 0.1f, { 0.95f, 0.97f, 1.0f, 0.85f } }, { 1.0f, { 0.95f, 0.97f, 1.0f, 0.7f } } })
			.SetSoftDistance(0.0f)));

		m_FireflyEffect.reset(ParticleEffect::Create(ParticleEffectParams()
			.SetDebugName("Fireflies")
			.SetShape(ParticleShape::Box, { k_Area, 1.0f, k_Area })
			.SetRate(VFX_FIREFLY_RATE)
			.SetLifetime(4.0f, 6.0f)
			.SetSpeed(0.05f, 0.18f)
			.SetNoise(0.9f, 0.5f)
			.SetStartSize(0.05f, 0.08f)
			.SetEndSize(1.0f)
			.SetColors({ { 0.0f, { 1.6f, 1.8f, 0.4f, 0.0f } }, { 0.25f, { 2.6f, 2.8f, 0.6f, 1.0f } }, { 0.6f, { 0.8f, 0.9f, 0.2f, 0.3f } }, { 1.0f, { 1.6f, 1.8f, 0.4f, 0.0f } } })
			.SetSoftDistance(0.0f)));

		m_MistEffect.reset(ParticleEffect::Create(ParticleEffectParams()
			.SetDebugName("Morning mist")
			.SetShape(ParticleShape::Box, { k_Area, 0.25f, k_Area })
			.SetRate(VFX_MIST_RATE)
			.SetLifetime(6.0f, 9.0f)
			.SetSpeed(0.08f, 0.2f)
			.SetNoise(0.25f, 0.2f)
			.SetStartSize(2.5f, 4.0f)
			.SetEndSize(1.6f)
			.SetStartRotation(0.0f, 360.0f)
			.SetSpin(-6.0f, 6.0f)
			.SetBlend(ParticleBlend::Alpha)
			.SetColors({ { 0.0f, { 0.88f, 0.9f, 0.94f, 0.0f } }, { 0.25f, { 0.88f, 0.9f, 0.94f, 0.12f } }, { 0.75f, { 0.88f, 0.9f, 0.94f, 0.1f } }, { 1.0f, { 0.88f, 0.9f, 0.94f, 0.0f } } })
			.SetSoftDistance(0.6f)));

		m_ChimneyEffect.reset(ParticleEffect::Create(ParticleEffectParams()
			.SetDebugName("Chimney smoke")
			.SetShape(ParticleShape::Cone, { 10.0f, 0.12f, 0.0f })
			.SetRate(VFX_CHIMNEY_RATE)
			.SetLifetime(2.6f, 3.6f)
			.SetSpeed(0.45f, 0.75f)
			.SetGravity({ 0.1f, 0.15f, 0.0f })
			.SetDrag(0.3f)
			.SetNoise(0.4f, 0.7f)
			.SetStartSize(0.12f, 0.2f)
			.SetEndSize(4.0f)
			.SetStartRotation(0.0f, 360.0f)
			.SetSpin(-15.0f, 15.0f)
			.SetBlend(ParticleBlend::Alpha)
			.SetColors({ { 0.0f, { 0.55f, 0.55f, 0.57f, 0.0f } }, { 0.15f, { 0.6f, 0.6f, 0.62f, 0.35f } }, { 1.0f, { 0.7f, 0.7f, 0.72f, 0.0f } } })
			.SetSoftDistance(0.3f)));

		m_SparkEffect.reset(ParticleEffect::Create(ParticleEffectParams()
			.SetDebugName("Festival sparks")
			.SetShape(ParticleShape::Cone, { 25.0f, 0.1f, 0.0f })
			.SetRate(VFX_SPARK_RATE)
			.SetLifetime(1.0f, 1.8f)
			.SetSpeed(0.4f, 0.9f)
			.SetGravity({ 0.0f, 0.1f, 0.0f })
			.SetDrag(0.5f)
			.SetNoise(0.8f, 1.0f)
			.SetStartSize(0.02f, 0.035f)
			.SetEndSize(0.3f)
			.SetColors({ { 0.0f, { 4.0f, 2.2f, 0.7f, 1.0f } }, { 1.0f, { 1.0f, 0.3f, 0.05f, 0.0f } } })
			.SetSoftDistance(0.0f)));

		m_BurstEffects[static_cast<size_t>(VfxBurst::Harvest)].reset(ParticleEffect::Create(BurstBase("Harvest sparkle", VFX_BURST_HARVEST)
			.SetShape(ParticleShape::Sphere, { 0.2f, 0.0f, 0.0f })
			.SetLifetime(0.6f, 1.1f)
			.SetSpeed(0.8f, 1.8f)
			.SetGravity({ 0.0f, 1.0f, 0.0f })
			.SetDrag(1.2f)
			.SetStartSize(0.03f, 0.05f)
			.SetEndSize(0.3f)
			.SetColors({ { 0.0f, { 4.0f, 3.2f, 0.9f, 1.0f } }, { 1.0f, { 1.5f, 1.0f, 0.2f, 0.0f } } })
			.SetSoftDistance(0.0f)));

		m_BurstEffects[static_cast<size_t>(VfxBurst::Dust)].reset(ParticleEffect::Create(BurstBase("Dust puff", VFX_BURST_DUST)
			.SetShape(ParticleShape::Sphere, { 0.25f, 0.0f, 0.0f })
			.SetLifetime(0.6f, 1.0f)
			.SetSpeed(0.4f, 1.0f)
			.SetGravity({ 0.0f, 0.2f, 0.0f })
			.SetDrag(2.0f)
			.SetStartSize(0.12f, 0.2f)
			.SetEndSize(3.0f)
			.SetStartRotation(0.0f, 360.0f)
			.SetBlend(ParticleBlend::Alpha)
			.SetColors({ { 0.0f, { 0.55f, 0.46f, 0.34f, 0.0f } }, { 0.15f, { 0.55f, 0.46f, 0.34f, 0.45f } }, { 1.0f, { 0.55f, 0.46f, 0.34f, 0.0f } } })
			.SetSoftDistance(0.2f)));

		m_BurstEffects[static_cast<size_t>(VfxBurst::Chips)].reset(ParticleEffect::Create(BurstBase("Chips", VFX_BURST_CHIPS)
			.SetShape(ParticleShape::Sphere, { 0.15f, 0.0f, 0.0f })
			.SetLifetime(0.5f, 0.9f)
			.SetSpeed(1.2f, 2.8f)
			.SetGravity({ 0.0f, -9.0f, 0.0f })
			.SetDrag(0.4f)
			.SetStartSize(0.04f, 0.07f)
			.SetEndSize(0.6f)
			.SetBlend(ParticleBlend::Alpha)
			.SetColors({ { 0.0f, { 0.5f, 0.36f, 0.2f, 1.0f } }, { 1.0f, { 0.4f, 0.28f, 0.15f, 0.0f } } })
			.SetSoftDistance(0.0f)));

		m_BurstEffects[static_cast<size_t>(VfxBurst::Spray)].reset(ParticleEffect::Create(BurstBase("Watering spray", VFX_BURST_SPRAY)
			.SetShape(ParticleShape::Cone, { 35.0f, 0.1f, 0.0f })
			.SetLifetime(0.4f, 0.7f)
			.SetSpeed(0.8f, 1.8f)
			.SetGravity({ 0.0f, -7.0f, 0.0f })
			.SetStartSize(0.025f, 0.045f)
			.SetEndSize(0.5f)
			.SetBlend(ParticleBlend::Alpha)
			.SetColors({ { 0.0f, { 0.55f, 0.75f, 1.0f, 0.9f } }, { 1.0f, { 0.55f, 0.75f, 1.0f, 0.0f } } })
			.SetSoftDistance(0.0f)));

		m_Rain = SpawnEmitter("Rain", m_RainEffect.get(), glm::vec3(0.0f), true, 0.0f);
		m_Snow = SpawnEmitter("Snow", m_SnowEffect.get(), glm::vec3(0.0f), true, 0.0f);
		m_Fireflies = SpawnEmitter("Fireflies", m_FireflyEffect.get(), glm::vec3(0.0f), true, 0.0f);
		m_Mist = SpawnEmitter("Mist", m_MistEffect.get(), glm::vec3(0.0f), true, 0.0f);
		for (size_t i = 0; i < static_cast<size_t>(VfxBurst::Count); ++i)
			m_BurstEmitters[i] = SpawnEmitter("Burst", m_BurstEffects[i].get(), glm::vec3(0.0f), true, 1.0f);
	}

	Vfx::~Vfx()
	{
		const auto drop = [this](Entity& entity)
		{
			if (entity.IsValid())
				m_Scene.DestroyEntity(entity);
			entity = {};
		};
		drop(m_Rain);
		drop(m_Snow);
		drop(m_Fireflies);
		drop(m_Mist);
		for (Entity& entity : m_BurstEmitters)
			drop(entity);
		for (Entity& entity : m_Placed)
			drop(entity);
	}

	Entity Vfx::SpawnEmitter(const char* name, ParticleEffect* effect, const glm::vec3& position, bool playing, float rateScale)
	{
		if (!m_Enabled || !effect)
			return {};
		Entity entity = m_Scene.CreateEntity(name);
		entity.AddComponent<Transform3DComponent>().Position = position;
		ParticleEmitterComponent& emitter = entity.AddComponent<ParticleEmitterComponent>(effect);
		emitter.Playing = playing;
		emitter.RateScale = rateScale;
		return entity;
	}

	Entity Vfx::SpawnChimney(const glm::vec3& position)
	{
		Entity entity = SpawnEmitter("Chimney", m_ChimneyEffect.get(), position, true, 0.0f);
		if (entity.IsValid())
			m_Placed.push_back(entity);
		return entity;
	}

	Entity Vfx::SpawnSparks(const glm::vec3& position)
	{
		Entity entity = SpawnEmitter("Sparks", m_SparkEffect.get(), position, true, 0.0f);
		if (entity.IsValid())
			m_Placed.push_back(entity);
		return entity;
	}

	void Vfx::Follow(Entity emitter, const glm::vec3& focus, float height, float scale)
	{
		if (!emitter.IsValid())
			return;
		emitter.GetComponent<Transform3DComponent>().Position = glm::vec3(focus.x, height, focus.z);
		emitter.GetComponent<ParticleEmitterComponent>().RateScale = scale;
	}

	void Vfx::Update(const VfxFrame& frame)
	{
		if (!m_Enabled)
			return;
		Follow(m_Rain, frame.Focus, GROUND_TOP + VFX_FOLLOW_HEIGHT_RAIN, frame.WeatherNow == Weather::Rain ? 1.0f : 0.0f);
		Follow(m_Snow, frame.Focus, GROUND_TOP + VFX_FOLLOW_HEIGHT_SNOW, frame.WeatherNow == Weather::Snow ? 1.0f : 0.0f);
		Follow(m_Fireflies, frame.Focus, GROUND_TOP + 1.2f, frame.FireflyAmount);
		Follow(m_Mist, frame.Focus, GROUND_TOP + 0.4f, frame.MistAmount);
	}

	void Vfx::Burst(VfxBurst kind, const glm::vec3& position)
	{
		if (!m_Enabled)
			return;
		const size_t index = static_cast<size_t>(kind);
		constexpr uint32_t k_Counts[] = { VFX_BURST_HARVEST, VFX_BURST_DUST, VFX_BURST_CHIPS, VFX_BURST_SPRAY };
		if (m_BurstEmitters[index].IsValid())
			m_Scene.EmitParticlesAt(m_BurstEmitters[index], position, k_Counts[index]);
	}

}
