#include "ArenaVfx.h"
#include "GameTuning.h"

namespace Dingo
{

	ArenaVfx::ArenaVfx()
	{
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
	}

}
