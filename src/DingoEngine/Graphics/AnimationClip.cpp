#include "depch.h"
#include "DingoEngine/Graphics/AnimationClip.h"

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
