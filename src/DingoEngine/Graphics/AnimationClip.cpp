#include "depch.h"
#include "DingoEngine/Graphics/AnimationClip.h"

#include <algorithm>
#include <atomic>

namespace Dingo
{

	uint64_t AnimationClip::AllocateId()
	{
		static std::atomic<uint64_t> s_NextId{ 1 };
		return s_NextId.fetch_add(1, std::memory_order_relaxed);
	}

	AnimationClip::AnimationClip(std::string name, float duration, std::vector<AnimationChannel> channels, const Skeleton* sourceSkeleton)
		: m_Name(std::move(name)), m_Duration(duration), m_Channels(std::move(channels)), m_SourceSkeleton(sourceSkeleton)
	{
	}

	void AnimationClip::AddEvent(float time, std::string name)
	{
		m_Events.push_back({ std::move(name), time, time, false });
		RebuildEventMarks();
	}

	void AnimationClip::AddEventRange(float begin, float end, std::string name)
	{
		if (end < begin)
			std::swap(begin, end);
		m_Events.push_back({ std::move(name), begin, end, true });
		RebuildEventMarks();
	}

	void AnimationClip::ClearEvents()
	{
		m_Events.clear();
		m_EventMarks.clear();
	}

	void AnimationClip::RebuildEventMarks()
	{
		m_EventMarks.clear();
		for (uint32_t i = 0; i < m_Events.size(); ++i)
		{
			const AnimationClipEvent& event = m_Events[i];
			if (!event.Range)
			{
				m_EventMarks.push_back({ event.Time, i, AnimationEventType::Instant });
				continue;
			}
			m_EventMarks.push_back({ event.Time, i, AnimationEventType::RangeBegin });
			m_EventMarks.push_back({ event.EndTime, i, AnimationEventType::RangeEnd });
		}

		// A range of zero length sits with the instants, its start before its end.
		auto order = [&](const EventMark& mark)
		{
			const AnimationClipEvent& event = m_Events[mark.Event];
			if (event.Range && event.Time == event.EndTime)
				return mark.Type == AnimationEventType::RangeBegin ? 1 : 2;
			return mark.Type == AnimationEventType::RangeEnd ? 0 : mark.Type == AnimationEventType::Instant ? 1 : 3;
		};
		std::stable_sort(m_EventMarks.begin(), m_EventMarks.end(), [&](const EventMark& a, const EventMark& b)
		{
			return a.Time != b.Time ? a.Time < b.Time : order(a) < order(b);
		});
	}

	const AnimationChannel* AnimationClip::FindChannel(std::string_view jointName) const
	{
		for (const AnimationChannel& channel : m_Channels)
		{
			if (channel.JointName == jointName)
				return &channel;
		}
		return nullptr;
	}

}
