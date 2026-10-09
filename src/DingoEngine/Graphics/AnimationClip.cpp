#include "depch.h"
#include "DingoEngine/Graphics/AnimationClip.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <charconv>
#include <cmath>
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

	namespace
	{

		// Calls fn(key, value) for each pair until it returns true; true if one did.
		template<typename Fn>
		bool ForEachPayloadPair(std::string_view text, Fn&& fn)
		{
			size_t i = 0;
			while (i < text.size())
			{
				if (std::isspace(static_cast<unsigned char>(text[i])))
				{
					i++;
					continue;
				}

				std::string_view key;
				if (text[i] == '"')
				{
					const size_t close = text.find('"', i + 1);
					const size_t end = close == std::string_view::npos ? text.size() : close;
					key = text.substr(i + 1, end - i - 1);
					i = close == std::string_view::npos ? text.size() : close + 1;
				}
				else
				{
					const size_t keyStart = i;
					while (i < text.size() && text[i] != '=' && !std::isspace(static_cast<unsigned char>(text[i])))
						i++;
					key = text.substr(keyStart, i - keyStart);
				}

				std::string_view value;
				if (i < text.size() && text[i] == '=')
				{
					i++;
					if (i < text.size() && text[i] == '"')
					{
						const size_t close = text.find('"', i + 1);
						const size_t end = close == std::string_view::npos ? text.size() : close;
						value = text.substr(i + 1, end - i - 1);
						i = close == std::string_view::npos ? text.size() : close + 1;
					}
					else
					{
						const size_t valueStart = i;
						while (i < text.size() && !std::isspace(static_cast<unsigned char>(text[i])))
							i++;
						value = text.substr(valueStart, i - valueStart);
					}
				}

				if (fn(key, value))
					return true;
			}
			return false;
		}

		bool FindPayloadValue(std::string_view text, std::string_view key, std::string_view& value)
		{
			return ForEachPayloadPair(text, [&](std::string_view k, std::string_view v)
			{
				if (k != key)
					return false;
				value = v;
				return true;
			});
		}

		template<typename T>
		T ParsePayloadNumber(std::string_view text, std::string_view key, T fallback)
		{
			std::string_view value;
			if (!FindPayloadValue(text, key, value))
				return fallback;

			T number{};
			const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), number);
			return error == std::errc() && end == value.data() + value.size() ? number : fallback;
		}

	}

	bool AnimationEventPayload::Has(std::string_view key) const
	{
		std::string_view value;
		return FindPayloadValue(Text, key, value);
	}

	std::string_view AnimationEventPayload::GetString(std::string_view key, std::string_view fallback) const
	{
		std::string_view value;
		return FindPayloadValue(Text, key, value) ? value : fallback;
	}

	float AnimationEventPayload::GetFloat(std::string_view key, float fallback) const
	{
		const float value = ParsePayloadNumber<float>(Text, key, fallback);
		return std::isfinite(value) ? value : fallback;
	}

	int32_t AnimationEventPayload::GetInt(std::string_view key, int32_t fallback) const
	{
		return ParsePayloadNumber<int32_t>(Text, key, fallback);
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

	void AnimationClip::AddEvent(float time, std::string name, std::string payload)
	{
		m_Events.push_back({ std::move(name), time, time, false, std::move(payload) });
		RebuildEventMarks();
	}

	void AnimationClip::AddEventRange(float begin, float end, std::string name, std::string payload)
	{
		if (end < begin)
			std::swap(begin, end);
		m_Events.push_back({ std::move(name), begin, end, true, std::move(payload) });
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
			const AnimationEventPayload payload{ event.Payload.empty() ? std::string_view() : InternEventName(event.Payload) };
			if (!event.Range)
			{
				m_EventMarks.push_back({ event.Time, i, AnimationEventType::Instant, name, payload });
				continue;
			}
			m_EventMarks.push_back({ event.Time, i, AnimationEventType::RangeBegin, name, payload });
			m_EventMarks.push_back({ event.EndTime, i, AnimationEventType::RangeEnd, name, payload });
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
