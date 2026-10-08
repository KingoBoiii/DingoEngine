#include "ArenaVfx.h"
#include "GameTuning.h"

namespace Dingo
{

	ArenaVfx::ArenaVfx(Scene& scene, bool fighters)
		: m_Scene(scene)
	{
		CreateBrazierEffects();
		if (!fighters)
			return;

		m_FootDust.reset(ParticleEffect::Create(ParticleEffectParams()
			.SetDebugName("Foot dust")
			.SetShape(ParticleShape::Cone, { 75.0f, 0.06f, 0.0f })
			.SetRate(0.0f)
			.SetLifetime(0.5f, 0.9f)
			.SetSpeed(0.25f, 0.6f)
			.SetGravity({ 0.0f, -0.3f, 0.0f })
			.SetDrag(2.5f)
			.SetStartSize(0.08f, 0.14f)
			.SetEndSize(3.0f)
			.SetStartRotation(0.0f, 360.0f)
			.SetSpin(-30.0f, 30.0f)
			.SetBlend(ParticleBlend::Alpha)
			.SetColors({ { 0.0f, { 0.42f, 0.4f, 0.38f, 0.45f } }, { 1.0f, { 0.36f, 0.35f, 0.34f, 0.0f } } })
			.SetSoftDistance(0.1f)
			.SetCapacity(FOOT_DUST_CAPACITY)));

		m_BladeTrail.reset(ParticleEffect::Create(ParticleEffectParams()
			.SetDebugName("Blade trail")
			.SetRate(BLADE_TRAIL_RATE)
			.SetLifetime(0.12f, 0.18f)
			.SetSpeed(0.0f, 0.05f)
			.SetStartSize(0.07f, 0.09f)
			.SetEndSize(0.2f)
			.SetColors({ { 0.0f, { 1.6f, 1.7f, 2.0f, 0.7f } }, { 1.0f, { 0.6f, 0.7f, 1.0f, 0.0f } } })
			.SetSoftDistance(0.0f)));

		m_HitSparks.reset(ParticleEffect::Create(ParticleEffectParams()
			.SetDebugName("Hit sparks")
			.SetShape(ParticleShape::Sphere, { 0.05f, 0.0f, 0.0f })
			.SetRate(0.0f)
			.SetLifetime(0.25f, 0.55f)
			.SetSpeed(2.0f, 4.5f)
			.SetGravity({ 0.0f, -6.0f, 0.0f })
			.SetDrag(2.0f)
			.SetStartSize(0.025f, 0.045f)
			.SetEndSize(0.3f)
			.SetColors({ { 0.0f, { 6.0f, 2.6f, 0.7f, 1.0f } }, { 1.0f, { 1.2f, 0.25f, 0.0f, 0.0f } } })
			.SetSoftDistance(0.0f)
			.SetCapacity(4 * HIT_SPARK_COUNT)));

		m_BlockSparks.reset(ParticleEffect::Create(ParticleEffectParams()
			.SetDebugName("Block sparks")
			.SetShape(ParticleShape::Sphere, { 0.08f, 0.0f, 0.0f })
			.SetRate(0.0f)
			.SetLifetime(0.2f, 0.45f)
			.SetSpeed(2.5f, 5.0f)
			.SetGravity({ 0.0f, -8.0f, 0.0f })
			.SetDrag(1.6f)
			.SetStartSize(0.02f, 0.035f)
			.SetEndSize(0.25f)
			.SetColors({ { 0.0f, { 5.0f, 5.5f, 6.5f, 1.0f } }, { 0.5f, { 2.2f, 2.0f, 1.6f, 0.8f } }, { 1.0f, { 0.8f, 0.5f, 0.2f, 0.0f } } })
			.SetSoftDistance(0.0f)
			.SetCapacity(4 * PARRY_SPARK_COUNT)));

		m_ParryFlash.reset(ParticleEffect::Create(ParticleEffectParams()
			.SetDebugName("Parry flash")
			.SetRate(0.0f)
			.SetLifetime(0.14f, 0.14f)
			.SetSpeed(0.0f, 0.0f)
			.SetStartSize(0.9f, 0.9f)
			.SetEndSize(1.8f)
			.SetColors({ { 0.0f, { 9.0f, 9.0f, 11.0f, 1.0f } }, { 1.0f, { 2.0f, 2.2f, 3.0f, 0.0f } } })
			.SetSoftDistance(0.0f)
			.SetCapacity(4)));

		m_KnockOutDust.reset(ParticleEffect::Create(ParticleEffectParams()
			.SetDebugName("Knock-out dust")
			.SetShape(ParticleShape::Cone, { 85.0f, 0.4f, 0.0f })
			.SetRate(0.0f)
			.SetLifetime(0.9f, 1.6f)
			.SetSpeed(0.6f, 1.4f)
			.SetGravity({ 0.0f, -0.2f, 0.0f })
			.SetDrag(2.2f)
			.SetStartSize(0.2f, 0.35f)
			.SetEndSize(3.0f)
			.SetStartRotation(0.0f, 360.0f)
			.SetSpin(-25.0f, 25.0f)
			.SetBlend(ParticleBlend::Alpha)
			.SetColors({ { 0.0f, { 0.4f, 0.38f, 0.36f, 0.5f } }, { 1.0f, { 0.34f, 0.33f, 0.32f, 0.0f } } })
			.SetSoftDistance(0.2f)
			.SetCapacity(2 * KO_DUST_COUNT)));

		m_HitEmitter = SpawnBurstEmitter("HitSparks", m_HitSparks.get());
		m_BlockEmitter = SpawnBurstEmitter("BlockSparks", m_BlockSparks.get());
		m_ParryEmitter = SpawnBurstEmitter("ParryFlash", m_ParryFlash.get());
		m_KnockOutEmitter = SpawnBurstEmitter("KnockOutDust", m_KnockOutDust.get());
	}

	void ArenaVfx::CreateBrazierEffects()
	{
		m_BrazierFlame.reset(ParticleEffect::Create(ParticleEffectParams()
			.SetDebugName("Brazier flame")
			.SetShape(ParticleShape::Cone, { 14.0f, 0.2f, 0.0f })
			.SetRate(60.0f)
			.SetLifetime(0.45f, 0.75f)
			.SetSpeed(0.5f, 0.9f)
			.SetGravity({ 0.0f, 0.8f, 0.0f })
			.SetDrag(0.6f)
			.SetNoise(0.8f, 1.6f)
			.SetStartSize(0.18f, 0.28f)
			.SetEndSize(0.3f)
			.SetStartRotation(0.0f, 360.0f)
			.SetSpin(-60.0f, 60.0f)
			.SetColors({ { 0.0f, { 3.2f, 1.6f, 0.55f, 0.85f } }, { 0.35f, { 2.4f, 0.9f, 0.25f, 0.6f } }, { 1.0f, { 0.6f, 0.12f, 0.02f, 0.0f } } })
			.SetSoftDistance(0.15f)));

		m_BrazierEmbers.reset(ParticleEffect::Create(ParticleEffectParams()
			.SetDebugName("Brazier embers")
			.SetShape(ParticleShape::Cone, { 30.0f, 0.15f, 0.0f })
			.SetRate(5.0f)
			.SetLifetime(1.2f, 2.2f)
			.SetSpeed(0.6f, 1.4f)
			.SetGravity({ 0.0f, 0.25f, 0.0f })
			.SetDrag(0.4f)
			.SetNoise(1.4f, 1.2f)
			.SetStartSize(0.018f, 0.03f)
			.SetEndSize(0.4f)
			.SetColors({ { 0.0f, { 5.0f, 2.2f, 0.6f, 1.0f } }, { 0.7f, { 2.5f, 0.8f, 0.15f, 0.9f } }, { 1.0f, { 0.8f, 0.2f, 0.0f, 0.0f } } })
			.SetSoftDistance(0.0f)));

		m_BrazierSmoke.reset(ParticleEffect::Create(ParticleEffectParams()
			.SetDebugName("Brazier smoke")
			.SetShape(ParticleShape::Cone, { 12.0f, 0.15f, 0.0f })
			.SetRate(4.0f)
			.SetLifetime(2.6f, 3.6f)
			.SetSpeed(0.35f, 0.55f)
			.SetGravity({ 0.0f, 0.1f, 0.0f })
			.SetDrag(0.3f)
			.SetNoise(0.35f, 0.7f)
			.SetStartSize(0.25f, 0.4f)
			.SetEndSize(3.5f)
			.SetStartRotation(0.0f, 360.0f)
			.SetSpin(-18.0f, 18.0f)
			.SetBlend(ParticleBlend::Alpha)
			.SetColors({ { 0.0f, { 0.05f, 0.05f, 0.055f, 0.0f } }, { 0.15f, { 0.06f, 0.06f, 0.065f, 0.28f } }, { 1.0f, { 0.08f, 0.08f, 0.09f, 0.0f } } })
			.SetSoftDistance(0.4f)));
	}

	Entity ArenaVfx::SpawnBurstEmitter(const char* name, ParticleEffect* effect)
	{
		Entity entity = m_Scene.CreateEntity(name);
		entity.AddComponent<Transform3DComponent>();
		entity.AddComponent<ParticleEmitterComponent>(effect);
		return entity;
	}

	void ArenaVfx::Impact(ImpactKind kind, const glm::vec3& contact)
	{
		if (!m_HitEmitter)
			return;

		switch (kind)
		{
			case ImpactKind::Hit:
				m_Scene.EmitParticlesAt(m_HitEmitter, contact, HIT_SPARK_COUNT);
				break;
			case ImpactKind::Block:
				m_Scene.EmitParticlesAt(m_BlockEmitter, contact, BLOCK_SPARK_COUNT);
				break;
			case ImpactKind::Parry:
				m_Scene.EmitParticlesAt(m_ParryEmitter, contact, 1);
				m_Scene.EmitParticlesAt(m_BlockEmitter, contact, PARRY_SPARK_COUNT);
				break;
		}
	}

	void ArenaVfx::KnockOut(const glm::vec3& feet)
	{
		if (!m_KnockOutEmitter)
			return;
		m_PendingDust.push_back({ feet, KO_DUST_DELAY });
	}

	void ArenaVfx::Update(float deltaTime)
	{
		for (PendingDust& dust : m_PendingDust)
		{
			dust.Delay -= deltaTime;
			if (dust.Delay <= 0.0f)
				m_Scene.EmitParticlesAt(m_KnockOutEmitter, dust.Position, KO_DUST_COUNT);
		}
		std::erase_if(m_PendingDust, [](const PendingDust& dust) { return dust.Delay <= 0.0f; });
	}

}
