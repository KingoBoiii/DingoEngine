#pragma once
#include <DingoEngine.h>

#include <memory>
#include <vector>

namespace Dingo
{

	enum class ImpactKind
	{
		Hit,
		Block,
		Parry
	};

	// The arena's particle effects, authored here and tuned live in the F4 effect editor, which copies
	// an effect back as code, and the emitters the impacts burst from. Its owner (the director, or a
	// showcase) frees it while the scene's emitter entities live on until the clear that follows, in
	// which nothing reads an emitter's effect. Absent under --no-particles.
	class ArenaVfx
	{
	public:
		// fighters: the fighters' dust, trails and impacts too; off (a showcase), only the braziers.
		ArenaVfx(Scene& scene, bool fighters);

		ArenaVfx(const ArenaVfx&) = delete;
		ArenaVfx& operator=(const ArenaVfx&) = delete;

		ParticleEffect* GetFootDust() const { return m_FootDust.get(); }
		ParticleEffect* GetBladeTrail() const { return m_BladeTrail.get(); }
		ParticleEffect* GetBrazierFlame() const { return m_BrazierFlame.get(); }
		ParticleEffect* GetBrazierEmbers() const { return m_BrazierEmbers.get(); }
		ParticleEffect* GetBrazierSmoke() const { return m_BrazierSmoke.get(); }

		// Sparks where a blade met its target, or a parry's flash.
		void Impact(ImpactKind kind, const glm::vec3& contact);
		// Dust where a knocked-out fighter lands, KO_DUST_DELAY after the blow.
		void KnockOut(const glm::vec3& feet);
		void Update(float deltaTime);

	private:
		struct PendingDust
		{
			glm::vec3 Position{ 0.0f };
			float Delay = 0.0f;
		};

		void CreateBrazierEffects();
		Entity SpawnBurstEmitter(const char* name, ParticleEffect* effect);

	private:
		Scene& m_Scene;
		std::unique_ptr<ParticleEffect> m_FootDust;
		std::unique_ptr<ParticleEffect> m_BladeTrail;
		std::unique_ptr<ParticleEffect> m_HitSparks;
		std::unique_ptr<ParticleEffect> m_BlockSparks;
		std::unique_ptr<ParticleEffect> m_ParryFlash;
		std::unique_ptr<ParticleEffect> m_KnockOutDust;
		std::unique_ptr<ParticleEffect> m_BrazierFlame;
		std::unique_ptr<ParticleEffect> m_BrazierEmbers;
		std::unique_ptr<ParticleEffect> m_BrazierSmoke;

		Entity m_HitEmitter;
		Entity m_BlockEmitter;
		Entity m_ParryEmitter;
		Entity m_KnockOutEmitter;
		std::vector<PendingDust> m_PendingDust;
	};

}
