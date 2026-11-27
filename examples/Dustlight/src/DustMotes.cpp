#include "DustMotes.h"

namespace Dingo
{

	DustMotes::~DustMotes()
	{
		Release();
	}

	void DustMotes::Build(Scene& scene)
	{
		Release();
		m_Effect = ParticleEffect::Create(ParticleEffectParams()
			.SetDebugName("Dust motes")
			.SetShape(ParticleShape::Box, { 5.0f, 1.8f, 2.5f })
			.SetRate(45.0f)
			.SetLifetime(5.0f, 7.0f)
			.SetSpeed(0.01f, 0.05f)
			.SetGravity({ 0.0f, -0.008f, 0.0f })
			.SetNoise(0.25f, 0.6f)
			.SetStartSize(0.02f, 0.045f)
			.SetEndSize(0.03f)
			.SetColors({ { 0.0f, { 1.2f, 1.0f, 0.75f, 0.0f } }, { 0.2f, { 1.2f, 1.0f, 0.75f, 0.7f } }, { 0.8f, { 1.2f, 1.0f, 0.75f, 0.6f } }, { 1.0f, { 1.2f, 1.0f, 0.75f, 0.0f } } })
			.SetBlend(ParticleBlend::Additive)
			.SetSoftDistance(0.1f));

		m_Emitter = scene.CreateEntity("DustMotes");
		m_Emitter.AddComponent<Transform3DComponent>().Position = { 0.0f, 1.8f, -4.5f };
		m_Emitter.AddComponent<ParticleEmitterComponent>(ParticleEmitterComponent(m_Effect));
	}

	void DustMotes::Release()
	{
		delete m_Effect;
		m_Effect = nullptr;
	}

}
