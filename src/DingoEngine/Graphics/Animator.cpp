#include "depch.h"
#include "DingoEngine/Graphics/Animator.h"
#include "DingoEngine/Graphics/AnimationClip.h"

#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cmath>

namespace Dingo
{

	namespace
	{

		// The key at or before `time`. Playback mostly moves forward by under a key a frame, so the
		// cursor steps; a jump (a loop wrap, a seek) falls back to a binary search.
		uint32_t FindKey(const std::vector<float>& times, float time, uint32_t& cursor)
		{
			const uint32_t last = static_cast<uint32_t>(times.size()) - 1;
			if (cursor > last)
				cursor = 0;

			if (time < times[cursor] || (cursor + 2 <= last && times[cursor + 2] <= time))
			{
				const auto it = std::upper_bound(times.begin(), times.end(), time);
				cursor = it == times.begin() ? 0 : static_cast<uint32_t>(it - times.begin()) - 1;
			}
			else if (cursor < last && times[cursor + 1] <= time)
			{
				cursor++;
			}

			return cursor;
		}

		template<typename T, typename Mix>
		T SampleTrack(const AnimationTrack<T>& track, float time, uint32_t& cursor, Mix mix)
		{
			const uint32_t key = FindKey(track.Times, time, cursor);
			if (track.Interpolation == AnimationInterpolation::Step || key + 1 >= track.Times.size() || time <= track.Times[key])
				return track.Values[key];

			const float from = track.Times[key];
			const float span = track.Times[key + 1] - from;
			return mix(track.Values[key], track.Values[key + 1], span > 0.0f ? (time - from) / span : 0.0f);
		}

		glm::vec3 MixVec3(const glm::vec3& a, const glm::vec3& b, float t) { return glm::mix(a, b, t); }
		glm::quat MixQuat(const glm::quat& a, const glm::quat& b, float t) { return glm::slerp(a, b, t); }

		void BlendPoses(std::span<JointPose> base, std::span<const JointPose> top, float weight)
		{
			if (weight <= 0.0f)
				return;

			if (weight >= 1.0f)
			{
				std::copy(top.begin(), top.end(), base.begin());
				return;
			}

			for (size_t i = 0; i < base.size(); ++i)
			{
				JointPose& pose = base[i];
				pose.Translation = glm::mix(pose.Translation, top[i].Translation, weight);
				pose.Scale = glm::mix(pose.Scale, top[i].Scale, weight);

				const glm::quat to = glm::dot(pose.Rotation, top[i].Rotation) < 0.0f ? -top[i].Rotation : top[i].Rotation;
				pose.Rotation = glm::normalize(pose.Rotation * (1.0f - weight) + to * weight);
			}
		}

		// Root motion is keyed in the joint's parent frame, so the rigs' rest offsets from that parent
		// give the scale, not their model-space heights: a centimetre rig under a 0.01 root scale
		// moves a rig in metres by 0.01 of its keys.
		float RestOffsetRatio(const Joint& source, const Joint& target)
		{
			const float from = glm::length(source.RestPose.Translation);
			const float to = glm::length(target.RestPose.Translation);
			return from > 1e-6f && to > 1e-6f ? to / from : 1.0f;
		}

	}

	AnimationState AnimationState::Clip(const AnimationClip* clip)
	{
		AnimationState state;
		state.m_Clip = clip;
		return state;
	}

	Animator::Animator(const Skeleton* skeleton)
	{
		SetSkeleton(skeleton);
	}

	void Animator::SetSkeleton(const Skeleton* skeleton)
	{
		m_Skeleton = skeleton;
		m_States.clear();
		m_Bindings.clear();
		ResetToRest();
	}

	void Animator::ResetToRest()
	{
		m_FrozenPose.clear();
		if (!m_Skeleton)
		{
			m_LocalPoses.clear();
			m_Scratch.clear();
			m_Globals.clear();
			m_Palette.clear();
			return;
		}

		m_LocalPoses.clear();
		for (const Joint& joint : m_Skeleton->GetJoints())
			m_LocalPoses.push_back(joint.RestPose);
		m_Scratch.resize(m_LocalPoses.size());
		m_Globals = m_Skeleton->GetRestGlobalTransforms();
		m_Palette = m_Skeleton->GetRestPalette();
	}

	void Animator::Play(const AnimationState& state, float fadeSeconds)
	{
		if (!m_Skeleton)
			return;

		if (!m_States.empty())
		{
			const PlayingState& current = m_States.back();
			if (!current.Frozen && current.State.GetClip() == state.GetClip() && current.State.IsLooping() == state.IsLooping()
				&& current.State.GetSpeed() == state.GetSpeed())
			{
				if (!(fadeSeconds > 0.0f) && m_States.size() > 1)
				{
					m_States.erase(m_States.begin(), m_States.end() - 1);
					m_States.back().FadeDuration = 0.0f;
				}
				return;
			}
		}

		const bool fade = fadeSeconds > 0.0f;
		if (!fade)
		{
			m_States.clear();
		}
		else if (m_States.empty() || m_States.size() >= k_MaxStates)
		{
			m_FrozenPose = m_LocalPoses;
			m_States.clear();
			m_States.emplace_back().Frozen = true;
		}

		PlayingState& playing = m_States.emplace_back();
		playing.State = state;
		playing.FadeDuration = fade ? fadeSeconds : 0.0f;
		playing.Direction = state.GetSpeed() < 0.0f ? -1.0f : 1.0f;
		if (const AnimationClip* clip = state.GetClip())
		{
			Bind(*clip);
			playing.Cursors.assign(clip->GetChannels().size() * 3, 0);
			playing.Time = state.GetSpeed() < 0.0f ? clip->GetDuration() : 0.0f;
		}
	}

	const AnimationClip* Animator::GetCurrentClip() const
	{
		return m_States.empty() || m_States.back().Frozen ? nullptr : m_States.back().State.GetClip();
	}

	float Animator::GetTime() const
	{
		return GetCurrentClip() ? m_States.back().Time : 0.0f;
	}

	float Animator::GetNormalizedTime() const
	{
		const AnimationClip* clip = GetCurrentClip();
		return clip && clip->GetDuration() > 0.0f ? m_States.back().Time / clip->GetDuration() : 0.0f;
	}

	void Animator::SetTime(float seconds)
	{
		if (GetCurrentClip())
			m_States.back().Time = WrapTime(m_States.back(), seconds);
	}

	bool Animator::IsFinished() const
	{
		const AnimationClip* clip = GetCurrentClip();
		if (!clip || m_States.back().State.IsLooping())
			return false;

		const PlayingState& current = m_States.back();
		return current.Direction < 0.0f ? current.Time <= 0.0f : current.Time >= clip->GetDuration();
	}

	float Animator::FadeWeight(const PlayingState& state) const
	{
		return state.FadeDuration > 0.0f ? std::min(state.FadeElapsed / state.FadeDuration, 1.0f) : 1.0f;
	}

	float Animator::WrapTime(const PlayingState& state, float time) const
	{
		const AnimationClip* clip = state.State.GetClip();
		const float duration = clip ? clip->GetDuration() : 0.0f;
		if (!(duration > 0.0f))
			return 0.0f;

		if (!state.State.IsLooping())
			return std::clamp(time, 0.0f, duration);

		time = std::fmod(time, duration);
		return time < 0.0f ? time + duration : time;
	}

	void Animator::Update(float deltaTime)
	{
		if (!m_Skeleton)
			return;

		for (PlayingState& state : m_States)
		{
			if (!state.Frozen && state.State.GetClip())
			{
				const float step = deltaTime * state.State.GetSpeed();
				if (step != 0.0f)
					state.Direction = step < 0.0f ? -1.0f : 1.0f;
				state.Time = WrapTime(state, state.Time + step);
			}
			state.FadeElapsed += std::abs(deltaTime);
		}

		// Whatever lies under the newest state that has fully faded in no longer shows.
		for (size_t i = m_States.size(); i-- > 1;)
		{
			if (FadeWeight(m_States[i]) >= 1.0f)
			{
				m_States.erase(m_States.begin(), m_States.begin() + static_cast<std::ptrdiff_t>(i));
				break;
			}
		}

		Evaluate();
	}

	void Animator::Evaluate()
	{
		if (m_States.empty())
		{
			const std::vector<Joint>& joints = m_Skeleton->GetJoints();
			for (size_t i = 0; i < joints.size(); ++i)
				m_LocalPoses[i] = joints[i].RestPose;
		}
		else
		{
			Sample(m_States.front(), m_LocalPoses);
			for (size_t i = 1; i < m_States.size(); ++i)
			{
				Sample(m_States[i], m_Scratch);
				BlendPoses(m_LocalPoses, m_Scratch, FadeWeight(m_States[i]));
			}
		}

		m_Skeleton->ComputeGlobalTransforms(m_LocalPoses, m_Globals);
		m_Skeleton->ComputeSkinningPalette(m_Globals, m_Palette);
	}

	void Animator::Sample(PlayingState& state, std::span<JointPose> out)
	{
		if (state.Frozen)
		{
			std::copy(m_FrozenPose.begin(), m_FrozenPose.end(), out.begin());
			return;
		}

		const std::vector<Joint>& joints = m_Skeleton->GetJoints();
		for (size_t i = 0; i < joints.size(); ++i)
			out[i] = joints[i].RestPose;

		const AnimationClip* clip = state.State.GetClip();
		if (!clip)
			return;

		const ClipBinding& clipBinding = m_Bindings.find(clip->GetId())->second;
		const std::vector<AnimationChannel>& channels = clip->GetChannels();
		for (size_t c = 0; c < channels.size(); ++c)
		{
			const ChannelBinding& binding = clipBinding.Channels[c];
			if (binding.Joint == Skeleton::k_InvalidJoint)
				continue;

			const AnimationChannel& channel = channels[c];
			uint32_t* cursors = &state.Cursors[c * 3];
			JointPose& pose = out[binding.Joint];

			if (binding.UseTranslation && !channel.Translation.IsEmpty())
				pose.Translation = binding.TranslationOffset + SampleTrack(channel.Translation, state.Time, cursors[0], MixVec3) * binding.TranslationScale;
			if (!channel.Rotation.IsEmpty())
				pose.Rotation = SampleTrack(channel.Rotation, state.Time, cursors[1], MixQuat);
			if (binding.UseScale && !channel.Scale.IsEmpty())
				pose.Scale = SampleTrack(channel.Scale, state.Time, cursors[2], MixVec3);
		}
	}

	const Animator::ClipBinding& Animator::Bind(const AnimationClip& clip)
	{
		auto [it, inserted] = m_Bindings.try_emplace(clip.GetId());
		ClipBinding& binding = it->second;
		if (!inserted)
			return binding;

		const std::vector<AnimationChannel>& channels = clip.GetChannels();
		const std::vector<Joint>& joints = m_Skeleton->GetJoints();
		binding.Channels.resize(channels.size());

		std::vector<bool> animated(joints.size(), false);
		size_t bound = 0;
		for (size_t c = 0; c < channels.size(); ++c)
		{
			const int32_t joint = m_Skeleton->FindJoint(channels[c].JointName);
			binding.Channels[c].Joint = joint;
			if (joint != Skeleton::k_InvalidJoint)
			{
				animated[joint] = true;
				bound++;
			}
		}

		if (bound == 0 && !channels.empty())
			DE_CORE_WARN("Animator: clip '{}' animates no joint of this skeleton, so it plays as the rest pose", clip.GetName());

		const Skeleton* source = clip.GetSourceSkeleton();
		if (!source || source == m_Skeleton)
			return binding;

		for (size_t c = 0; c < channels.size(); ++c)
		{
			ChannelBinding& channel = binding.Channels[c];
			if (channel.Joint == Skeleton::k_InvalidJoint)
				continue;

			channel.UseScale = false;

			bool rootMost = true;
			for (int32_t parent = joints[channel.Joint].Parent; parent >= 0; parent = joints[parent].Parent)
			{
				if (animated[parent])
				{
					rootMost = false;
					break;
				}
			}

			if (!rootMost)
			{
				channel.UseTranslation = false;
				continue;
			}

			const int32_t sourceJoint = source->FindJoint(channels[c].JointName);
			if (sourceJoint == Skeleton::k_InvalidJoint)
				continue;

			const Joint& from = source->GetJoint(sourceJoint);
			const float ratio = RestOffsetRatio(from, joints[channel.Joint]);
			channel.TranslationScale = ratio;
			channel.TranslationOffset = joints[channel.Joint].RestPose.Translation - from.RestPose.Translation * ratio;
		}

		return binding;
	}

	glm::mat4 Animator::GetJointTransform(int32_t joint) const
	{
		if (!m_Skeleton || joint < 0 || static_cast<size_t>(joint) >= m_Globals.size())
			return glm::mat4(1.0f);

		return m_Skeleton->GetRootTransform() * m_Globals[joint];
	}

}
