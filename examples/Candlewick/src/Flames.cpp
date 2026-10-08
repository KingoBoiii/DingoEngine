#include "Flames.h"
#include "GameTuning.h"
#include "LaunchOptions.h"

namespace Dingo
{

	FlameEffects CreateFlameEffects()
	{
		FlameEffects effects;

		effects.Candle.reset(ParticleEffect::Create(ParticleEffectParams()
			.SetDebugName("Candle flame")
			.SetShape(ParticleShape::Cone, { 8.0f, 0.01f, 0.0f })
			.SetRate(22.0f)
			.SetLifetime(0.22f, 0.38f)
			.SetSpeed(0.18f, 0.3f)
			.SetGravity({ 0.0f, 0.4f, 0.0f })
			.SetDrag(0.5f)
			.SetStartSize(0.045f, 0.06f)
			.SetEndSize(0.25f)
			.SetColors({ { 0.0f, { 2.6f, 1.3f, 0.45f, 0.9f } }, { 0.4f, { 2.0f, 0.75f, 0.2f, 0.7f } }, { 1.0f, { 0.5f, 0.12f, 0.02f, 0.0f } } })
			.SetSoftDistance(0.0f)
			.SetCapacity(16)));

		effects.Sconce.reset(ParticleEffect::Create(ParticleEffectParams()
			.SetDebugName("Sconce flame")
			.SetShape(ParticleShape::Cone, { 10.0f, 0.03f, 0.0f })
			.SetRate(28.0f)
			.SetLifetime(0.28f, 0.45f)
			.SetSpeed(0.25f, 0.4f)
			.SetGravity({ 0.0f, 0.5f, 0.0f })
			.SetDrag(0.5f)
			.SetStartSize(0.08f, 0.11f)
			.SetEndSize(0.25f)
			.SetColors({ { 0.0f, { 2.8f, 1.4f, 0.5f, 0.9f } }, { 0.4f, { 2.1f, 0.8f, 0.22f, 0.7f } }, { 1.0f, { 0.5f, 0.12f, 0.02f, 0.0f } } })
			.SetSoftDistance(0.0f)
			.SetCapacity(24)));

		effects.BrazierFlame.reset(ParticleEffect::Create(ParticleEffectParams()
			.SetDebugName("Brazier flame")
			.SetShape(ParticleShape::Cone, { 14.0f, 0.18f, 0.0f })
			.SetRate(60.0f)
			.SetLifetime(0.45f, 0.75f)
			.SetSpeed(0.5f, 0.9f)
			.SetGravity({ 0.0f, 0.8f, 0.0f })
			.SetDrag(0.6f)
			.SetNoise(0.8f, 1.6f)
			.SetStartSize(0.16f, 0.26f)
			.SetEndSize(0.3f)
			.SetStartRotation(0.0f, 360.0f)
			.SetSpin(-60.0f, 60.0f)
			.SetColors({ { 0.0f, { 3.2f, 1.6f, 0.55f, 0.85f } }, { 0.35f, { 2.4f, 0.9f, 0.25f, 0.6f } }, { 1.0f, { 0.6f, 0.12f, 0.02f, 0.0f } } })
			.SetSoftDistance(0.15f)));

		effects.BrazierEmbers.reset(ParticleEffect::Create(ParticleEffectParams()
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

		effects.BrazierSmoke.reset(ParticleEffect::Create(ParticleEffectParams()
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

		effects.Kindle.reset(ParticleEffect::Create(ParticleEffectParams()
			.SetDebugName("Kindle burst")
			.SetShape(ParticleShape::Sphere, { 0.15f, 0.0f, 0.0f })
			.SetRate(0.0f)
			.SetLifetime(0.5f, 1.1f)
			.SetSpeed(1.5f, 3.2f)
			.SetGravity({ 0.0f, -1.5f, 0.0f })
			.SetDrag(1.4f)
			.SetStartSize(0.02f, 0.04f)
			.SetEndSize(0.3f)
			.SetColors({ { 0.0f, { 6.0f, 2.8f, 0.8f, 1.0f } }, { 1.0f, { 1.0f, 0.25f, 0.0f, 0.0f } } })
			.SetSoftDistance(0.0f)
			.SetCapacity(KINDLE_BURST_COUNT)));

		effects.SnuffSmoke.reset(ParticleEffect::Create(ParticleEffectParams()
			.SetDebugName("Snuff smoke")
			.SetShape(ParticleShape::Cone, { 20.0f, 0.03f, 0.0f })
			.SetRate(0.0f)
			.SetLifetime(1.0f, 1.6f)
			.SetSpeed(0.3f, 0.6f)
			.SetDrag(0.8f)
			.SetNoise(0.6f, 1.5f)
			.SetStartSize(0.05f, 0.08f)
			.SetEndSize(4.0f)
			.SetStartRotation(0.0f, 360.0f)
			.SetSpin(-40.0f, 40.0f)
			.SetBlend(ParticleBlend::Alpha)
			.SetColors({ { 0.0f, { 0.35f, 0.33f, 0.32f, 0.55f } }, { 1.0f, { 0.2f, 0.2f, 0.2f, 0.0f } } })
			.SetSoftDistance(0.1f)
			.SetCapacity(2 * SNUFF_SMOKE_COUNT)));

		return effects;
	}

	Entity SpawnEmitter(Scene& scene, const char* name, ParticleEffect* effect, const glm::vec3& position, bool playing)
	{
		if (GetLaunchOptions().NoParticles || !effect)
			return {};

		Entity entity = scene.CreateEntity(name);
		entity.AddComponent<Transform3DComponent>().Position = position;
		entity.AddComponent<ParticleEmitterComponent>(effect).Playing = playing;
		return entity;
	}

}
