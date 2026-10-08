#pragma once
#include <DingoEngine.h>

#include <memory>

namespace Dingo
{

	// The arena's particle effects, authored here and tuned live in the F4 effect editor, which copies
	// an effect back as code. The director owns it, so it outlives every fighter's emitters. Absent
	// under --no-particles.
	class ArenaVfx
	{
	public:
		ArenaVfx();

		ArenaVfx(const ArenaVfx&) = delete;
		ArenaVfx& operator=(const ArenaVfx&) = delete;

		ParticleEffect* GetFootDust() const { return m_FootDust.get(); }
		ParticleEffect* GetBladeTrail() const { return m_BladeTrail.get(); }

	private:
		std::unique_ptr<ParticleEffect> m_FootDust;
		std::unique_ptr<ParticleEffect> m_BladeTrail;
	};

}
