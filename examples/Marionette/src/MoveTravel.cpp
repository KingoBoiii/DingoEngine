#include "MoveTravel.h"

#include <DingoEngine.h>

#include <algorithm>
#include <optional>

namespace
{
	using namespace Dingo;

	float Covered(const ClipRange& range, float from, float to)
	{
		const float length = range.End - range.Begin;
		if (!(length > 1.0e-6f))
			return 0.0f;
		return std::clamp((std::min(to, range.End) - std::max(from, range.Begin)) / length, 0.0f, 1.0f);
	}
}

namespace Dingo
{

	void MoveTravel::Begin(const Skeleton* skeleton, const AnimationClip& clip, float scale, float fadeOut, float dashDistance)
	{
		const float duration = clip.GetDuration();
		m_Skeleton = skeleton;
		m_Clip = &clip;
		m_Scale = scale;
		m_Active = true;
		m_Clock = 0.0f;
		m_Return = { std::max(duration - fadeOut, 0.0f), duration };
		m_ReturnStep = skeleton ? NetHipsTravel(*skeleton, clip, fadeOut) * scale : glm::vec2(0.0f);
		m_DashDistance = dashDistance;
		ResolveDash();
	}

	void MoveTravel::ResolveDash()
	{
		m_Dash = ClipRange();
		m_DashStep = glm::vec2(0.0f);
		m_DashRevision = m_Clip->GetEventRevision();
		if (!m_Skeleton || !(m_DashDistance > 0.0f))
			return;

		const std::optional<ClipRange> dash = FindRange(*m_Clip, Events::DASH);
		if (!dash || !(dash->End - dash->Begin > 1.0e-3f))
			return;

		const glm::vec2 direction = PoseHipsTravel(*m_Skeleton, *m_Clip, dash->Begin, dash->End);
		const float length = glm::length(direction);
		if (length > 1.0e-3f)
		{
			m_Dash = *dash;
			m_DashStep = direction / length * (m_DashDistance * m_Scale);
		}
	}

	void MoveTravel::Release(float clipTime, float carrySeconds)
	{
		glm::vec2 owed = m_Carry;
		if (m_Active)
		{
			if (m_Clock < m_Return.Begin)
			{
				if (m_Skeleton)
					owed += PoseHipsTravel(*m_Skeleton, *m_Clip, 0.0f, std::clamp(clipTime, 0.0f, m_Return.Begin)) * m_Scale;
			}
			else
			{
				owed += m_ReturnStep * (1.0f - Covered(m_Return, m_Return.Begin, m_Clock));
			}
		}

		m_Active = false;
		m_Carry = owed;
		m_CarryLeft = std::max(carrySeconds, 0.0f);
	}

	glm::vec2 MoveTravel::Step(float seconds)
	{
		if (!(seconds > 0.0f))
			return glm::vec2(0.0f);

		glm::vec2 paid(0.0f);
		if (m_Active)
		{
			// The clip's events changed under the move (a hot-reload): the dash follows the new window from here.
			if (m_Clip->GetEventRevision() != m_DashRevision)
				ResolveDash();

			const float from = m_Clock;
			const float to = from + seconds;
			m_Clock = to;
			paid += m_DashStep * Covered(m_Dash, from, to) + m_ReturnStep * Covered(m_Return, from, to);
			if (to >= m_Dash.End && to >= m_Return.End)
				m_Active = false;
		}

		if (m_Carry.x != 0.0f || m_Carry.y != 0.0f)
		{
			const float share = m_CarryLeft > seconds ? seconds / m_CarryLeft : 1.0f;
			const glm::vec2 due = share >= 1.0f ? m_Carry : m_Carry * share;
			m_Carry -= due;
			m_CarryLeft = std::max(m_CarryLeft - seconds, 0.0f);
			paid += due;
		}
		return paid;
	}

}
