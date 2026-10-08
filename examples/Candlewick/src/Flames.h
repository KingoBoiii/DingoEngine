#pragma once
#include <DingoEngine.h>

#include <memory>

namespace Dingo
{

	// Every particle effect in the keep, authored here and tuned live in the F4 effect editor, which
	// copies an effect back as code. KeepWorld owns them, so they outlive every emitter of the keep.
	struct FlameEffects
	{
		std::unique_ptr<ParticleEffect> Candle;
		std::unique_ptr<ParticleEffect> Sconce;
		std::unique_ptr<ParticleEffect> BrazierFlame;
		std::unique_ptr<ParticleEffect> BrazierEmbers;
		std::unique_ptr<ParticleEffect> BrazierSmoke;
		std::unique_ptr<ParticleEffect> Kindle;
		std::unique_ptr<ParticleEffect> SnuffSmoke;
	};

	FlameEffects CreateFlameEffects();

	// An emitter entity at `position`, or an invalid entity under --no-particles.
	Entity SpawnEmitter(Scene& scene, const char* name, ParticleEffect* effect, const glm::vec3& position, bool playing = true);

}
