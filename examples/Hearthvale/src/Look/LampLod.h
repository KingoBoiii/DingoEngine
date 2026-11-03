#pragma once
#include <glm/glm.hpp>

#include <vector>

namespace Dingo
{

	struct LampLodInput
	{
		glm::vec3 Position{ 0.0f };
		// How far the light's sphere lies outside the view frustum (<= 0 = inside); the caller measures it.
		float Overshoot = 0.0f;
	};

	// Keeps the lights the renderer sees in view at or under `capacity` (its local-light budget minus headroom), so it
	// never has to drop one. Lights outside the view margin keep full weight untouched; inside it the nearest to the
	// focus win (with stickiness so a tie does not flicker), fade in only into a free slot, fade out when outranked, and
	// are snapped off when the exact in-view count would still exceed the capacity. Plain C++, so --check runs it headless;
	// Candlewick's LightLod, minus its gameplay lights.
	class LampLod
	{
	public:
		LampLod(float fadeSeconds, float viewMargin, float stickiness)
			: m_FadeSeconds(fadeSeconds), m_ViewMargin(viewMargin), m_Stickiness(stickiness)
		{}

		// Weights are 0..1, one per input, read through GetWeights(). The first call snaps to the plan.
		void Update(float dt, const std::vector<LampLodInput>& lights, const glm::vec3& focus, int capacity);

		const std::vector<float>& GetWeights() const { return m_Weight; }
		// Lights in view (overshoot <= 0) with a weight above zero, after the last Update.
		int CountInView(const std::vector<LampLodInput>& lights) const;

	private:
		float m_FadeSeconds;
		float m_ViewMargin;
		float m_Stickiness;
		bool m_Primed = false;
		std::vector<float> m_Weight;
		std::vector<char> m_Wanted;
		std::vector<float> m_Key;
		std::vector<size_t> m_Order;
	};

}
