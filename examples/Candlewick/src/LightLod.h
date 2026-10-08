#pragma once
#include "KeepWorld.h"

#include <DingoEngine.h>

#include <glm/glm.hpp>

#include <cstddef>
#include <vector>

namespace Dingo
{

	// Keeps the lights the renderer sees in view at or under its budget minus LIGHT_LOD_HEADROOM,
	// so it never has to drop one. Gameplay lights are counted and never touched; decorative
	// flames fade in only into a free slot and are snapped off when a gameplay light takes theirs.
	// Shadow slots need no budget: only gameplay lights cast, and the renderer has a slot for each.
	class LightLod
	{
	public:
		LightLod(const std::vector<DecorFlame>& flames, bool enabled);

		void AddGameplayLight(Entity light);
		void SetFlicker(size_t flame, float factor);
		void Update(float deltaTime, const glm::vec3& focus, const glm::mat4& viewProjection);

	private:
		struct Flame
		{
			Entity Light;
			float BaseIntensity = 0.0f;
			float Flicker = 1.0f;
			float Weight = 1.0f;
			bool Wanted = true;
			float Overshoot = 0.0f;
			float Key = 0.0f;
		};

		static void Write(Flame& flame);

	private:
		std::vector<Flame> m_Flames;
		std::vector<Entity> m_Gameplay;
		std::vector<size_t> m_Order;
		bool m_Enabled = true;
		bool m_Primed = false;
		bool m_OverCapacityWarned = false;
	};

}
