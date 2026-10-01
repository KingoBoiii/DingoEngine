#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace Dingo
{

	class Skeleton;

	enum class AnimationInterpolation : uint8_t
	{
		Step,
		Linear
	};

	template<typename T>
	struct AnimationTrack
	{
		// Seconds, ascending; one value per time.
		std::vector<float> Times;
		std::vector<T>     Values;
		AnimationInterpolation Interpolation = AnimationInterpolation::Linear;

		bool IsEmpty() const { return Times.empty(); }
	};

	// The keys of one joint, bound to a skeleton by name. An empty track leaves that part of
	// the joint at its rest pose; assimp gives every imported channel all three tracks.
	struct AnimationChannel
	{
		std::string               JointName;
		AnimationTrack<glm::vec3> Translation;
		AnimationTrack<glm::quat> Rotation;
		AnimationTrack<glm::vec3> Scale;
	};

	class AnimationClip
	{
	public:
		AnimationClip(std::string name, float duration, std::vector<AnimationChannel> channels, const Skeleton* sourceSkeleton);

		const std::string&                   GetName()     const { return m_Name; }
		float                                GetDuration() const { return m_Duration; }
		const std::vector<AnimationChannel>& GetChannels() const { return m_Channels; }
		// The skeleton of the file the clip was loaded from, owned by the same Model. Retargeting
		// reads its rest pose.
		const Skeleton*                      GetSourceSkeleton() const { return m_SourceSkeleton; }

		// nullptr when the clip doesn't animate that joint.
		const AnimationChannel* FindChannel(std::string_view jointName) const;

	private:
		std::string                   m_Name;
		float                         m_Duration = 0.0f;
		std::vector<AnimationChannel> m_Channels;
		const Skeleton*               m_SourceSkeleton = nullptr;
	};

}
