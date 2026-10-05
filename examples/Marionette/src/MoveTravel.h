#pragma once
#include "Moveset.h"

#include <glm/glm.hpp>

#include <cstdint>

namespace Dingo
{

	// The ground a fighter's capsule owes the pose. The pose already carries a clip's own hips travel, so the
	// capsule stays put during a move and pays the move's net of it while the one-shot returns, plus a dodge's
	// extra distance over its dash window. A move that ends early (chained, hit, killed) banks what is still
	// unpaid as a carry, paid over the next state's fade-in, while the pose cross-fades away from it.
	// Everything is in the fighter's own frame (+z forward, +x left) and in seconds of animation time.
	class MoveTravel
	{
	public:
		// A null skeleton owes nothing.
		void Begin(const Skeleton* skeleton, const AnimationClip& clip, float scale, float fadeOut, float dashDistance);

		// `clipTime` is where the pose stands in the move's clip; the carry is paid over `carrySeconds`.
		void Release(float clipTime, float carrySeconds);

		// The ground to cover in `seconds`.
		glm::vec2 Step(float seconds);

		const AnimationClip* GetClip() const { return m_Clip; }
		float GetClock() const { return m_Clock; }
		glm::vec2 GetReturnStep() const { return m_ReturnStep; }
		glm::vec2 GetDashStep() const { return m_DashStep; }
		glm::vec2 GetCarry() const { return m_Carry; }

	private:
		void ResolveDash();

	private:
		const Skeleton* m_Skeleton = nullptr;
		const AnimationClip* m_Clip = nullptr;
		float m_Scale = 1.0f;
		bool m_Active = false;
		float m_Clock = 0.0f;

		float m_DashDistance = 0.0f;
		ClipRange m_Dash;
		glm::vec2 m_DashStep{ 0.0f };
		uint64_t m_DashRevision = 0;

		ClipRange m_Return;
		glm::vec2 m_ReturnStep{ 0.0f };

		glm::vec2 m_Carry{ 0.0f };
		float m_CarryLeft = 0.0f;
	};

}
