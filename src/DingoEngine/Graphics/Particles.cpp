#include "depch.h"
#include "DingoEngine/Graphics/Particles.h"
#include "DingoEngine/Graphics/ParticleRenderer.h"

#include <cmath>

namespace Dingo
{

	ParticleEffect* ParticleEffect::Create(const ParticleEffectParams& params)
	{
		return new ParticleEffect(params);
	}

	uint32_t ParticleEffect::GetEmitterCapacity() const
	{
		if (m_Params.Capacity > 0)
			return m_Params.Capacity;

		const float rate = std::isfinite(m_Params.Rate) ? std::max(m_Params.Rate, 0.0f) : 0.0f;
		const float longest = std::isfinite(m_Params.Lifetime.y) ? std::max({ m_Params.Lifetime.x, m_Params.Lifetime.y, 0.0f }) : 0.0f;
		const double alive = std::ceil(static_cast<double>(rate) * longest * 1.1);
		return static_cast<uint32_t>(std::min(alive, 1.0e7)) + m_Params.BurstOnPlay + 64;
	}

	ParticleEmitter::ParticleEmitter(const ParticleEffect* effect, std::shared_ptr<Internal::ParticlePool> pool, uint32_t base, uint32_t capacity)
		: m_Effect(effect), m_Pool(std::move(pool)), m_Base(base), m_Capacity(capacity)
	{
	}

	ParticleEmitter::~ParticleEmitter()
	{
		if (m_Pool && m_Capacity > 0)
			m_Pool->Release(m_Base, m_Capacity);
	}

	void ParticleEmitter::Emit(uint32_t count)
	{
		if (count > 0)
			m_Bursts.push_back({ glm::vec3(0.0f), false, count });
	}

	void ParticleEmitter::EmitAt(const glm::vec3& worldPosition, uint32_t count)
	{
		if (count > 0)
			m_Bursts.push_back({ worldPosition, true, count });
	}

	void ParticleEmitter::SetPlaying(bool playing)
	{
		if (playing && !m_Playing)
			m_PlayBurstPending = true;
		m_Playing = playing;
	}

}
