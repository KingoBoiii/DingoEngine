#pragma once
#include <DingoEngine.h>

#include "Sim/SimTypes.h"

#include <memory>
#include <vector>

namespace Dingo
{

	struct VfxFrame
	{
		glm::vec3 Focus{ 0.0f };
		Weather WeatherNow = Weather::Clear;
		float FireflyAmount = 0.0f;
		float MistAmount = 0.0f;
	};

	enum class VfxBurst : uint8_t { Harvest, Dust, Chips, Spray, Count };

	// Every particle effect the valley draws: weather, fireflies, mist, the bursts the farm actions fire, and the
	// chimney and festival emitters other pieces place. The area effects follow the camera focus. Under
	// --no-particles it makes nothing and every call is a no-op. Device side: never constructed headless.
	class Vfx
	{
	public:
		Vfx(Scene& scene, bool enabled);
		~Vfx();
		Vfx(const Vfx&) = delete;
		Vfx& operator=(const Vfx&) = delete;

		void Update(const VfxFrame& frame);
		void Burst(VfxBurst kind, const glm::vec3& position);

		// Emitters that start silent; the caller sets RateScale on the component. Invalid entities when disabled.
		Entity SpawnChimney(const glm::vec3& position);
		Entity SpawnSparks(const glm::vec3& position);

		bool IsEnabled() const { return m_Enabled; }

	private:
		Entity SpawnEmitter(const char* name, ParticleEffect* effect, const glm::vec3& position, bool playing, float rateScale);
		void Follow(Entity emitter, const glm::vec3& focus, float height, float scale);

	private:
		Scene& m_Scene;
		bool m_Enabled;
		std::unique_ptr<ParticleEffect> m_RainEffect;
		std::unique_ptr<ParticleEffect> m_SnowEffect;
		std::unique_ptr<ParticleEffect> m_FireflyEffect;
		std::unique_ptr<ParticleEffect> m_MistEffect;
		std::unique_ptr<ParticleEffect> m_ChimneyEffect;
		std::unique_ptr<ParticleEffect> m_SparkEffect;
		std::unique_ptr<ParticleEffect> m_BurstEffects[static_cast<size_t>(VfxBurst::Count)];
		Entity m_Rain;
		Entity m_Snow;
		Entity m_Fireflies;
		Entity m_Mist;
		Entity m_BurstEmitters[static_cast<size_t>(VfxBurst::Count)];
		std::vector<Entity> m_Placed;
	};

}
