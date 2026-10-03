#include "depch.h"
#include "DingoEngine/Graphics/AnimationClip.h"

#include <algorithm>
#include <atomic>
#include <mutex>
#include <unordered_set>

namespace Dingo
{

	namespace
	{

		struct NameHash
		{
			using is_transparent = void;
			size_t operator()(std::string_view name) const { return std::hash<std::string_view>{}(name); }
		};

		// Never freed: events copied out of an Animator (a scene holds some for a frame) and events
		// from before a reload keep naming them.
		std::string_view InternEventName(std::string_view name)
		{
			static std::mutex s_Mutex;
			static auto* s_Names = new std::unordered_set<std::string, NameHash, std::equal_to<>>();

			std::lock_guard lock(s_Mutex);
			auto it = s_Names->find(name);
			if (it == s_Names->end())
				it = s_Names->emplace(name).first;
			return *it;
		}

	}

	uint64_t AnimationClip::AllocateId()
	{
		static std::atomic<uint64_t> s_NextId{ 1 };
		return s_NextId.fetch_add(1, std::memory_order_relaxed);
	}

	AnimationClip::AnimationClip(std::string name, float duration, std::vector<AnimationChannel> channels, const Skeleton* sourceSkeleton)
		: m_Name(std::move(name)), m_Duration(duration), m_Channels(std::move(channels)), m_SourceSkeleton(sourceSkeleton)
	{
		// Sampling reads one value per time, so a track built by hand with more of either would read past
		// its end.
		bool trimmed = false;
		auto fit = [&](auto& track)
		{
			const size_t count = (std::min)(track.Times.size(), track.Values.size());
			trimmed |= track.Times.size() != count || track.Values.size() != count;
			track.Times.resize(count);
			track.Values.resize(count);
		};
		for (AnimationChannel& channel : m_Channels)
		{
			fit(channel.Translation);
			fit(channel.Rotation);
			fit(channel.Scale);
		}
		if (trimmed)
			DE_CORE_WARN("AnimationClip '{}': a track has more times than values or more values than times; the extra ones are dropped", m_Name);
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
		m_EventRevision = AllocateId();
	}

	void AnimationClip::Reinitialize(AnimationClip& source, const Skeleton* sourceSkeleton)
	{
		m_Id = AllocateId();
		m_Duration = source.m_Duration;
		m_Channels = std::move(source.m_Channels);
		m_SourceSkeleton = sourceSkeleton;
		m_Events = std::move(source.m_Events);
		m_EventMarks = std::move(source.m_EventMarks);
		m_EventRevision = AllocateId();
	}

	void AnimationClip::Clear()
	{
		m_Id = AllocateId();
		m_Duration = 0.0f;
		m_Channels.clear();
		ClearEvents();
	}

	void AnimationClip::RebuildEventMarks()
	{
		m_EventMarks.clear();
		m_EventRevision = AllocateId();
		for (uint32_t i = 0; i < m_Events.size(); ++i)
		{
			const AnimationClipEvent& event = m_Events[i];
			const std::string_view name = InternEventName(event.Name);
			if (!event.Range)
			{
				m_EventMarks.push_back({ event.Time, i, AnimationEventType::Instant, name });
				continue;
			}
			m_EventMarks.push_back({ event.Time, i, AnimationEventType::RangeBegin, name });
			m_EventMarks.push_back({ event.EndTime, i, AnimationEventType::RangeEnd, name });
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
