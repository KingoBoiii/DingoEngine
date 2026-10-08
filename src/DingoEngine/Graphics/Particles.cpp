#include "depch.h"
#include "DingoEngine/Graphics/Particles.h"
#include "DingoEngine/Graphics/ParticleRenderer.h"

#include <cmath>

namespace Dingo
{

	namespace
	{
		std::vector<ParticleEffect*>& LiveEffects()
		{
			static std::vector<ParticleEffect*> s_Effects;
			return s_Effects;
		}

		const char* ShapeName(ParticleShape shape)
		{
			switch (shape)
			{
				case ParticleShape::Sphere: return "Sphere";
				case ParticleShape::Cone: return "Cone";
				case ParticleShape::Box: return "Box";
				case ParticleShape::Point:
				default: return "Point";
			}
		}
	}

	namespace Internal
	{
		const std::vector<ParticleEffect*>& GetLiveParticleEffects()
		{
			return LiveEffects();
		}

		std::string ParticleEffectToCode(const ParticleEffectParams& p)
		{
			// A float literal that compiles: 1 prints as "1", which needs its ".0" before the "f".
			auto f = [](float value)
			{
				std::string text = std::format("{}", value);
				if (text.find_first_of(".en") == std::string::npos)
					text += ".0";
				return text + "f";
			};
			auto v3 = [&f](const glm::vec3& v) { return std::format("{{ {}, {}, {} }}", f(v.x), f(v.y), f(v.z)); };

			std::string code = std::format("ParticleEffect::Create(ParticleEffectParams()\n\t.SetDebugName(\"{}\")\n", p.DebugName);
			code += std::format("\t.SetShape(ParticleShape::{}, {})\n", ShapeName(p.Shape), v3(p.ShapeSize));
			code += std::format("\t.SetRate({})\n", f(p.Rate));
			if (p.BurstOnPlay > 0)
				code += std::format("\t.SetBurstOnPlay({})\n", p.BurstOnPlay);
			code += std::format("\t.SetLifetime({}, {})\n\t.SetSpeed({}, {})\n", f(p.Lifetime.x), f(p.Lifetime.y), f(p.Speed.x), f(p.Speed.y));
			code += std::format("\t.SetGravity({})\n\t.SetDrag({})\n", v3(p.Gravity), f(p.Drag));
			if (p.InheritVelocity != 0.0f)
				code += std::format("\t.SetInheritVelocity({})\n", f(p.InheritVelocity));
			if (p.NoiseStrength > 0.0f)
				code += std::format("\t.SetNoise({}, {})\n", f(p.NoiseStrength), f(p.NoiseScale));
			code += std::format("\t.SetStartSize({}, {})\n\t.SetEndSize({})\n", f(p.StartSize.x), f(p.StartSize.y), f(p.EndSize));
			if (p.Spin.x != 0.0f || p.Spin.y != 0.0f)
				code += std::format("\t.SetSpin({}, {})\n", f(p.Spin.x), f(p.Spin.y));
			code += "\t.SetColors({";
			const uint32_t keys = std::min(p.ColorKeyCount, ParticleEffectParams::k_MaxColorKeys);
			for (uint32_t i = 0; i < keys; ++i)
			{
				const ParticleColorKey& key = p.ColorKeys[i];
				code += std::format("{} {{ {}, {{ {}, {}, {}, {} }} }}", i == 0 ? "" : ",", f(key.Time), f(key.Color.r), f(key.Color.g), f(key.Color.b), f(key.Color.a));
			}
			code += " })\n";
			if (p.Blend == ParticleBlend::Alpha)
				code += "\t.SetBlend(ParticleBlend::Alpha)\n";
			if (p.FlipbookColumns > 1 || p.FlipbookRows > 1)
				code += std::format("\t.SetTexture(texture, {}, {})\n", p.FlipbookColumns, p.FlipbookRows);
			code += std::format("\t.SetSoftDistance({})", f(p.SoftDistance));
			if (p.Capacity > 0)
				code += std::format("\n\t.SetCapacity({})", p.Capacity);
			code += ");\n";
			return code;
		}
	}

	ParticleEffect* ParticleEffect::Create(const ParticleEffectParams& params)
	{
		return new ParticleEffect(params);
	}

	ParticleEffect::ParticleEffect(const ParticleEffectParams& params)
		: m_Params(params)
	{
		LiveEffects().push_back(this);
	}

	ParticleEffect::~ParticleEffect()
	{
		std::erase(LiveEffects(), this);
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
