#include "depch.h"
#include "DingoUIState.h"

namespace Dingo::Internal
{

	UIStateMap::TouchResult UIStateMap::Touch(uint64_t id, uint64_t frame)
	{
		const auto [it, inserted] = m_States.try_emplace(id);
		TouchResult result;
		result.State = &it->second;
		result.IsNew = inserted;
		result.DeclaredAlready = !inserted && it->second.LastSeenFrame == frame;
		it->second.LastSeenFrame = frame;
		return result;
	}

	UIWidgetState* UIStateMap::Find(uint64_t id)
	{
		const auto it = m_States.find(id);
		return it == m_States.end() ? nullptr : &it->second;
	}

	const UIWidgetState* UIStateMap::Find(uint64_t id) const
	{
		const auto it = m_States.find(id);
		return it == m_States.end() ? nullptr : &it->second;
	}

	size_t UIStateMap::Evict(uint64_t frame, uint64_t maxAge)
	{
		size_t removed = 0;
		for (auto it = m_States.begin(); it != m_States.end();)
		{
			if (frame >= it->second.LastSeenFrame + maxAge)
			{
				it = m_States.erase(it);
				++removed;
			}
			else
				++it;
		}
		return removed;
	}

}
