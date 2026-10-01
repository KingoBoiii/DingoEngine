#include "depch.h"
#include "DingoEngine/Graphics/AnimationClip.h"

namespace Dingo
{

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
