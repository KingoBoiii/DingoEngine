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

		void BlendJoint(JointPose& pose, const JointPose& top, float weight)
		{
			pose.Translation = glm::mix(pose.Translation, top.Translation, weight);
			pose.Scale = glm::mix(pose.Scale, top.Scale, weight);

			const glm::quat to = glm::dot(pose.Rotation, top.Rotation) < 0.0f ? -top.Rotation : top.Rotation;
			pose.Rotation = glm::normalize(pose.Rotation * (1.0f - weight) + to * weight);
		}

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
				BlendJoint(base[i], top[i], weight);
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

		float ClipDuration(const AnimationClip* clip)
		{
			return clip ? clip->GetDuration() : 0.0f;
		}

		// Shows what lies under the layer and nothing of its own.
		bool IsTransparent(const AnimationState& state)
		{
			return !state.GetClip() && !state.IsBlend();
		}

		// Three key cursors (translation, rotation, scale) per channel of each clip in the state.
		size_t CursorCount(const AnimationState& state)
		{
			size_t count = state.GetClip() ? state.GetClip()->GetChannels().size() * 3 : 0;
			for (const BlendPoint& point : state.GetPoints())
				count += point.Clip ? point.Clip->GetChannels().size() * 3 : 0;
			return count;
		}

	}

	AnimationState AnimationState::Clip(const AnimationClip* clip)
	{
		AnimationState state;
		state.m_Clip = clip;
		return state;
	}

	AnimationState AnimationState::Blend1D(std::string parameter, std::vector<BlendPoint> points)
	{
		std::stable_sort(points.begin(), points.end(), [](const BlendPoint& a, const BlendPoint& b) { return a.Value < b.Value; });

		AnimationState state;
		state.m_Parameter = std::move(parameter);
		state.m_Points = std::move(points);
		return state;
	}

	bool AnimationState::operator==(const AnimationState& other) const
	{
		if (m_Clip != other.m_Clip || m_Loop != other.m_Loop || m_Speed != other.m_Speed
			|| m_Parameter != other.m_Parameter || m_Points.size() != other.m_Points.size())
			return false;

		for (size_t i = 0; i < m_Points.size(); ++i)
		{
			if (m_Points[i].Value != other.m_Points[i].Value || m_Points[i].Clip != other.m_Points[i].Clip)
				return false;
		}
		return true;
	}

	Animator::Animator(const Skeleton* skeleton)
	{
		m_Layers.resize(1);
		SetSkeleton(skeleton);
	}

	void Animator::SetSkeleton(const Skeleton* skeleton)
	{
		m_Skeleton = skeleton;
		m_Bindings.clear();
		ResetToRest();
	}

	void Animator::ResetToRest()
	{
		m_RestPoses.clear();
		m_SkeletonRevision = m_Skeleton ? m_Skeleton->GetRevision() : 0;
		if (m_Skeleton)
		{
			for (const Joint& joint : m_Skeleton->GetJoints())
				m_RestPoses.push_back(joint.RestPose);
		}

		m_LocalPoses = m_RestPoses;
		m_UpperLayersApplied = false;
		m_Scratch.resize(m_RestPoses.size());
		m_BlendScratch.resize(m_RestPoses.size());
		m_Globals = m_Skeleton ? m_Skeleton->GetRestGlobalTransforms() : std::vector<glm::mat4>();
		m_Palette = m_Skeleton ? m_Skeleton->GetRestPalette() : std::vector<glm::mat4>();

		m_Events.clear();
		for (Layer& layer : m_Layers)
		{
			layer.States.clear();
			layer.FrozenPose.clear();
			layer.OneShotPending = false;
			layer.OpenRanges.clear();
			layer.CloseRangesOnUpdate = false;
			layer.EventSerial = 0;
			layer.EventClip = nullptr;
			layer.Pose = m_RestPoses;
			if (m_Skeleton)
				ResolveMask(layer);
		}
	}

	void Animator::SyncSkeleton()
	{
		if (m_Skeleton->GetRevision() == m_SkeletonRevision)
			return;

		// Retargeting offsets come from the rest pose, so every binding is made again.
		m_SkeletonRevision = m_Skeleton->GetRevision();
		m_Bindings.clear();
		for (uint32_t i = 0; i < m_RestPoses.size(); ++i)
			m_RestPoses[i] = m_Skeleton->GetJoint(i).RestPose;
	}

	Animator::Layer& Animator::EnsureLayer(uint32_t index)
	{
		if (index >= m_Layers.size())
		{
			const size_t first = m_Layers.size();
			m_Layers.resize(index + 1);
			for (size_t i = first; i < m_Layers.size(); ++i)
			{
				m_Layers[i].Pose = m_RestPoses;
				if (m_Skeleton)
					ResolveMask(m_Layers[i]);
			}
		}
		return m_Layers[index];
	}

	void Animator::ResolveMask(Layer& layer)
	{
		const std::vector<Joint>& joints = m_Skeleton->GetJoints();
		layer.Mask.assign(joints.size(), 0.0f);

		const std::string& rootName = layer.Settings.GetMaskRoot();
		const int32_t root = rootName.empty() ? Skeleton::k_InvalidJoint : m_Skeleton->FindJoint(rootName);
		if (!rootName.empty() && root == Skeleton::k_InvalidJoint)
		{
			DE_CORE_WARN("Animator: layer mask joint '{}' isn't in the skeleton, so the layer moves nothing", rootName);
			return;
		}

		std::vector<bool> excluded(joints.size(), false);
		for (const std::string& name : layer.Settings.GetExclusions())
		{
			const int32_t joint = m_Skeleton->FindJoint(name);
			if (joint == Skeleton::k_InvalidJoint)
				DE_CORE_WARN("Animator: layer mask exclusion '{}' isn't in the skeleton", name);
			else
				excluded[joint] = true;
		}

		// Parents come first, so a joint inherits its parent's answer, and an exclusion its subtree.
		for (size_t j = 0; j < joints.size(); ++j)
		{
			const int32_t parent = joints[j].Parent;
			const bool parentInside = parent >= 0 && layer.Mask[parent] > 0.0f;
			bool inside = rootName.empty() ? (parent < 0 || parentInside) : (static_cast<int32_t>(j) == root || parentInside);
			if (excluded[j])
				inside = false;
			layer.Mask[j] = inside ? 1.0f : 0.0f;
		}
	}

	void Animator::SetLayer(uint32_t index, const AnimationLayer& settings)
	{
		if (index == 0)
			return;

		Layer& layer = EnsureLayer(index);
		layer.Settings = settings;
		if (m_Skeleton)
			ResolveMask(layer);
	}

	void Animator::SetLayerWeight(uint32_t index, float weight)
	{
		if (index != 0)
			EnsureLayer(index).Settings.SetWeight(weight);
	}

	void Animator::SetFloat(std::string_view name, float value)
	{
		const auto it = m_Parameters.find(name);
		if (it != m_Parameters.end())
			it->second = value;
		else
			m_Parameters.emplace(std::string(name), value);
	}

	float Animator::GetFloat(std::string_view name) const
	{
		const auto it = m_Parameters.find(name);
		return it != m_Parameters.end() ? it->second : 0.0f;
	}

	Animator::PlayingState Animator::MakeState(const AnimationState& state)
	{
		PlayingState playing;
		playing.State = state;
		playing.Serial = m_NextSerial++;
		if (m_NextSerial == 0)
			m_NextSerial = 1;
		playing.Direction = state.GetSpeed() < 0.0f ? -1.0f : 1.0f;

		if (const AnimationClip* clip = state.GetClip())
		{
			Bind(*clip);
			playing.Time = state.GetSpeed() < 0.0f ? clip->GetDuration() : 0.0f;
		}
		for (const BlendPoint& point : state.GetPoints())
		{
			if (point.Clip)
				Bind(*point.Clip);
		}
		if (state.IsBlend() && state.GetSpeed() < 0.0f)
			playing.Time = 1.0f;

		playing.Cursors.assign(CursorCount(state), 0);
		return playing;
	}

	void Animator::Push(Layer& layer, PlayingState state, float fadeSeconds, std::span<const JointPose> current)
	{
		const bool fade = fadeSeconds > 0.0f;
		const bool baseLayer = &layer == &m_Layers.front();
		if (!fade)
		{
			layer.States.clear();
		}
		else if (layer.States.size() >= k_MaxStates || (layer.States.empty() && baseLayer))
		{
			layer.FrozenPose.assign(current.begin(), current.end());
			layer.States.clear();
			layer.States.emplace_back().Frozen = true;
		}
		else if (layer.States.empty())
		{
			// Above layer 0, nothing playing means the layers below show, so fade in over them.
			layer.States.push_back(MakeState(AnimationState::Clip(nullptr)));
		}

		state.FadeDuration = fade ? fadeSeconds : 0.0f;
		state.FadeElapsed = 0.0f;
		layer.States.push_back(std::move(state));
	}

	void Animator::Play(const AnimationState& state, float fadeSeconds, uint32_t layerIndex)
	{
		if (!m_Skeleton)
			return;

		Layer& layer = EnsureLayer(layerIndex);

		// A script that plays its locomotion every frame would otherwise cut every one-shot short.
		if (layer.OneShotPending && layer.Resume.State == state)
			return;
		layer.OneShotPending = false;

		if (!layer.States.empty() && !layer.States.back().Frozen && layer.States.back().State == state)
		{
			if (!(fadeSeconds > 0.0f) && layer.States.size() > 1)
			{
				layer.States.erase(layer.States.begin(), layer.States.end() - 1);
				layer.States.back().FadeDuration = 0.0f;
			}
			return;
		}

		Push(layer, MakeState(state), fadeSeconds, FreezeSource(layerIndex));
	}

	std::span<const JointPose> Animator::FreezeSource(size_t layer) const
	{
		// Layer 0 writes straight into m_LocalPoses, which holds the layers above too once they apply.
		if (layer == 0)
			return m_UpperLayersApplied ? m_Layers.front().Pose : m_LocalPoses;
		return m_Layers[layer].Pose;
	}

	bool Animator::IsOneShotPlaying(uint32_t layer) const
	{
		return layer < m_Layers.size() && m_Layers[layer].OneShotPending;
	}

	void Animator::PlayOneShot(const AnimationClip* clip, float fadeIn, float fadeOut, uint32_t layerIndex)
	{
		if (!m_Skeleton || !clip)
			return;

		Layer& layer = EnsureLayer(layerIndex);
		if (!layer.OneShotPending)
		{
			const bool playing = !layer.States.empty() && !layer.States.back().Frozen;
			layer.Resume = playing ? layer.States.back() : MakeState(AnimationState::Clip(nullptr));
		}

		layer.OneShotPending = true;
		layer.OneShotClip = clip;
		layer.OneShotFadeOut = std::max(fadeOut, 0.0f);
		Push(layer, MakeState(AnimationState::Clip(clip).SetLoop(false)), fadeIn, FreezeSource(layerIndex));
	}

	const Animator::PlayingState* Animator::Current(uint32_t layer) const
	{
		if (layer >= m_Layers.size() || m_Layers[layer].States.empty() || m_Layers[layer].States.back().Frozen)
			return nullptr;
		return &m_Layers[layer].States.back();
	}

	Animator::BlendSpot Animator::Locate(const AnimationState& state) const
	{
		const std::vector<BlendPoint>& points = state.GetPoints();
		const float value = GetFloat(state.GetParameter());
		if (points.empty() || !(value > points.front().Value))
			return {};
		if (!(value < points.back().Value))
			return { points.size() - 1, 0.0f };

		size_t lower = 0;
		while (lower + 2 < points.size() && !(value < points[lower + 1].Value))
			lower++;

		const float span = points[lower + 1].Value - points[lower].Value;
		return { lower, span > 0.0f ? (value - points[lower].Value) / span : 0.0f };
	}

	float Animator::Duration(const AnimationState& state) const
	{
		if (!state.IsBlend())
			return ClipDuration(state.GetClip());

		const BlendSpot spot = Locate(state);
		const std::vector<BlendPoint>& points = state.GetPoints();
		const float lower = ClipDuration(points[spot.Lower].Clip);
		if (!(spot.T > 0.0f))
			return lower;

		// A side without length (no clip, or a one-key pose) has no cycle to lerp towards; mixing in
		// its 0 would speed the other clip up.
		const float upper = ClipDuration(points[spot.Lower + 1].Clip);
		if (!(lower > 0.0f))
			return upper;
		if (!(upper > 0.0f))
			return lower;
		return glm::mix(lower, upper, spot.T);
	}

	const AnimationClip* Animator::GetCurrentClip(uint32_t layer) const
	{
		const PlayingState* current = Current(layer);
		if (!current)
			return nullptr;
		if (!current->State.IsBlend())
			return current->State.GetClip();

		const BlendSpot spot = Locate(current->State);
		return current->State.GetPoints()[spot.T >= 0.5f ? spot.Lower + 1 : spot.Lower].Clip;
	}

	float Animator::GetTime(uint32_t layer) const
	{
		const PlayingState* current = Current(layer);
		if (!current)
			return 0.0f;
		return current->State.IsBlend() ? current->Time * Duration(current->State) : current->Time;
	}

	float Animator::GetNormalizedTime(uint32_t layer) const
	{
		const PlayingState* current = Current(layer);
		if (!current)
			return 0.0f;
		if (current->State.IsBlend())
			return current->Time;

		const float duration = ClipDuration(current->State.GetClip());
		return duration > 0.0f ? current->Time / duration : 0.0f;
	}

	void Animator::SetTime(float seconds, uint32_t layer)
	{
		if (!Current(layer))
			return;

		PlayingState& current = m_Layers[layer].States.back();
		if (!current.State.IsBlend())
		{
			current.Time = Wrap(current, seconds);
			MarkSeek(layer);
			return;
		}

		const float duration = Duration(current.State);
		if (duration > 0.0f)
		{
			current.Time = Wrap(current, seconds / duration);
			MarkSeek(layer);
		}
	}

	void Animator::SetNormalizedTime(float fraction, uint32_t layer)
	{
		if (!Current(layer))
			return;

		PlayingState& current = m_Layers[layer].States.back();
		current.Time = Wrap(current, current.State.IsBlend() ? fraction : fraction * ClipDuration(current.State.GetClip()));
		MarkSeek(layer);
	}

	void Animator::MarkSeek(uint32_t layerIndex)
	{
		Layer& layer = m_Layers[layerIndex];
		PlayingState& current = layer.States.back();
		current.Fresh = true;
		if (layer.EventSerial == current.Serial)
			layer.CloseRangesOnUpdate = true;
	}

	bool Animator::IsFinished(uint32_t layer) const
	{
		const PlayingState* current = Current(layer);
		if (!current || current->State.IsLooping() || IsTransparent(current->State))
			return false;

		const float length = current->State.IsBlend() ? 1.0f : ClipDuration(current->State.GetClip());
		return current->Direction < 0.0f ? current->Time <= 0.0f : current->Time >= length;
	}

	bool Animator::IsFading(uint32_t layer) const
	{
		return layer < m_Layers.size() && m_Layers[layer].States.size() > 1;
	}

	std::vector<AnimatorStateInfo> Animator::GetStates(uint32_t layer) const
	{
		std::vector<AnimatorStateInfo> states;
		if (layer >= m_Layers.size())
			return states;

		states.reserve(m_Layers[layer].States.size());
		for (const PlayingState& state : m_Layers[layer].States)
		{
			AnimatorStateInfo info;
			info.Blend = state.State.IsBlend();
			info.Frozen = state.Frozen;
			info.Looping = state.State.IsLooping();
			info.Weight = FadeWeight(state);
			info.Time = state.Time;
			if (!info.Blend)
			{
				info.Clip = state.State.GetClip();
				const float duration = ClipDuration(info.Clip);
				info.NormalizedTime = duration > 0.0f ? state.Time / duration : 0.0f;
			}
			else
			{
				const BlendSpot spot = Locate(state.State);
				info.Clip = state.State.GetPoints()[spot.T >= 0.5f ? spot.Lower + 1 : spot.Lower].Clip;
				info.Parameter = state.State.GetParameter();
				info.NormalizedTime = state.Time;
			}
			states.push_back(info);
		}
		return states;
	}

	float Animator::FadeWeight(const PlayingState& state) const
	{
		return state.FadeDuration > 0.0f ? std::min(state.FadeElapsed / state.FadeDuration, 1.0f) : 1.0f;
	}

	float Animator::Wrap(const PlayingState& state, float time) const
	{
		const float length = state.State.IsBlend() ? 1.0f : ClipDuration(state.State.GetClip());
		if (!(length > 0.0f))
			return 0.0f;

		if (!state.State.IsLooping())
			return std::clamp(time, 0.0f, length);

		time = std::fmod(time, length);
		return time < 0.0f ? time + length : time;
	}

	void Animator::Advance(PlayingState& state, float deltaTime) const
	{
		// A reload shortened the clip under the state: it goes on from the same point of the new loop
		// instead of lapping through every mark in one step.
		if (!state.State.IsBlend() && state.Time > ClipDuration(state.State.GetClip()))
			state.Time = Wrap(state, state.Time);

		state.PreviousTime = state.Time;
		state.Wrapped = false;
		if (state.Frozen || IsTransparent(state.State))
			return;

		// A zero step leaves the time alone: wrapping a reversed state's start (its duration) to 0
		// would read as a full lap backwards.
		const float step = deltaTime * state.State.GetSpeed();
		if (step == 0.0f)
			return;
		state.Direction = step < 0.0f ? -1.0f : 1.0f;

		// A blend's clips share one phase, which moves at the blended cycle length.
		float length = 1.0f;
		float target = 0.0f;
		if (!state.State.IsBlend())
		{
			length = ClipDuration(state.State.GetClip());
			target = state.Time + step;
		}
		else
		{
			const float cycle = Duration(state.State);
			if (!(cycle > 0.0f))
				return;
			target = state.Time + step / cycle;
		}

		state.Wrapped = state.State.IsLooping() && length > 0.0f && (target >= length || target < 0.0f);
		state.Time = Wrap(state, target);
	}

	void Animator::Update(float deltaTime)
	{
		if (!m_Skeleton)
			return;

		SyncSkeleton();
		m_Events.clear();
		for (size_t i = 0; i < m_Layers.size(); ++i)
		{
			Layer& layer = m_Layers[i];
			for (PlayingState& state : layer.States)
			{
				Advance(state, deltaTime);
				state.FadeElapsed += std::abs(deltaTime);
			}

			if (layer.OneShotPending && layer.States.empty())
				layer.OneShotPending = false;

			bool returning = false;
			if (layer.OneShotPending)
			{
				Advance(layer.Resume, deltaTime);
				layer.Resume.Fresh = false;

				const PlayingState& top = layer.States.back();
				const bool oneShotOnTop = !top.Frozen && !top.State.IsBlend() && top.State.GetClip() == layer.OneShotClip && !top.State.IsLooping();
				const float remaining = !oneShotOnTop ? 0.0f : top.Direction < 0.0f ? top.Time : ClipDuration(layer.OneShotClip) - top.Time;
				if (!oneShotOnTop || remaining <= layer.OneShotFadeOut)
				{
					layer.OneShotPending = false;
					returning = oneShotOnTop;
				}
			}

			// The one-shot's marks up to this moment fire before the way back takes the events over.
			CollectEvents(static_cast<uint32_t>(i), layer);
			if (returning)
			{
				PlayingState resume = layer.Resume;
				resume.Returning = true;
				Push(layer, std::move(resume), layer.OneShotFadeOut, FreezeSource(i));
				CollectEvents(static_cast<uint32_t>(i), layer);
			}

			// Whatever lies under the newest state that has fully faded in no longer shows.
			for (size_t s = layer.States.size(); s-- > 1;)
			{
				if (FadeWeight(layer.States[s]) >= 1.0f)
				{
					layer.States.erase(layer.States.begin(), layer.States.begin() + static_cast<std::ptrdiff_t>(s));
					break;
				}
			}

			for (PlayingState& state : layer.States)
				state.Fresh = false;
		}

		Evaluate();
	}

	Animator::PlayingState* Animator::Dominant(Layer& layer) const
	{
		if (layer.States.empty())
			return nullptr;
		if (layer.States.back().Returning)
			return &layer.States.back();

		// A state's share of the fold is its weight times what every state above leaves through.
		// Visited newest first, so a tie goes to the incoming state.
		PlayingState* dominant = nullptr;
		float largest = -1.0f;
		float through = 1.0f;
		for (size_t i = layer.States.size(); i-- > 0;)
		{
			const float weight = i == 0 ? 1.0f : FadeWeight(layer.States[i]);
			const float share = weight * through;
			if (share > largest)
			{
				dominant = &layer.States[i];
				largest = share;
			}
			through *= 1.0f - weight;
			if (!(through > 0.0f))
				break;
		}
		return dominant;
	}

	void Animator::CollectEvents(uint32_t layerIndex, Layer& layer)
	{
		if (layer.CloseRangesOnUpdate)
		{
			CloseRanges(layerIndex, layer);
			layer.CloseRangesOnUpdate = false;
		}

		PlayingState* dominant = layerIndex == 0 || layer.Settings.GetWeight() >= 0.5f ? Dominant(layer) : nullptr;

		const AnimationClip* clip = nullptr;
		float timeScale = 1.0f;
		if (dominant && !dominant->Frozen)
		{
			if (!dominant->State.IsBlend())
			{
				clip = dominant->State.GetClip();
			}
			else
			{
				const BlendSpot spot = Locate(dominant->State);
				clip = dominant->State.GetPoints()[spot.T >= 0.5f ? spot.Lower + 1 : spot.Lower].Clip;
				timeScale = ClipDuration(clip);
			}
		}

		const uint32_t serial = clip ? dominant->Serial : 0;
		if (serial != layer.EventSerial || clip != layer.EventClip)
		{
			CloseRanges(layerIndex, layer);
			layer.EventSerial = serial;
			layer.EventClip = clip;
			layer.EventRevision = clip ? clip->GetEventRevision() : 0;
		}

		if (!clip)
			return;

		// The clip's events changed under it (a reload, ClearEvents): a range whose event is gone ends now.
		if (clip->GetEventRevision() != layer.EventRevision)
		{
			layer.EventRevision = clip->GetEventRevision();
			const std::vector<AnimationClipEvent>& events = clip->GetEvents();
			std::erase_if(layer.OpenRanges, [&](const OpenRange& range)
			{
				const bool kept = range.Event < events.size() && events[range.Event].Range && events[range.Event].Name == range.Name;
				if (!kept)
					m_Events.push_back({ range.Name, range.EndTime, AnimationEventType::RangeEnd, range.Clip, layerIndex });
				return !kept;
			});
		}

		const bool forward = dominant->Direction >= 0.0f;
		float from = dominant->PreviousTime * timeScale;
		bool inclusive = dominant->Fresh;
		// A one-shot that faded in catches up on the marks it crossed before it took over; a loop
		// doesn't, or a cross-fade would replay footsteps the outgoing clip already fired.
		if (!dominant->Led && !dominant->State.IsLooping())
		{
			from = forward ? 0.0f : clip->GetDuration();
			inclusive = true;
		}
		dominant->Led = true;

		if (!clip->GetEventMarks().empty())
			FireMarks(layerIndex, layer, *clip, from, dominant->Time * timeScale, inclusive, forward, dominant->Wrapped);
	}

	void Animator::FireMarks(uint32_t layerIndex, Layer& layer, const AnimationClip& clip, float from, float to, bool inclusive, bool forward, bool wrapped)
	{
		const std::vector<AnimationClip::EventMark>& marks = clip.GetEventMarks();
		const float duration = clip.GetDuration();

		// (lo, hi] in order, or [lo, hi] when lo itself counts.
		auto ascending = [&](float lo, float hi, bool fromLo)
		{
			for (const AnimationClip::EventMark& mark : marks)
			{
				if ((fromLo ? mark.Time >= lo : mark.Time > lo) && mark.Time <= hi)
					Fire(layerIndex, layer, clip, mark, true);
			}
		};
		// [lo, hi) in reverse, or [lo, hi] when hi itself counts.
		auto descending = [&](float lo, float hi, bool fromHi)
		{
			for (size_t i = marks.size(); i-- > 0;)
			{
				if (marks[i].Time >= lo && (fromHi ? marks[i].Time <= hi : marks[i].Time < hi))
					Fire(layerIndex, layer, clip, marks[i], false);
			}
		};

		// A wrap is a lap through the clip's end, even when the step went a full lap or more and
		// came back past where it started.
		if (forward)
		{
			if (!wrapped)
			{
				ascending(from, to, inclusive);
			}
			else
			{
				ascending(from, duration, inclusive);
				ascending(0.0f, to, true);
			}
		}
		else
		{
			if (!wrapped)
			{
				descending(to, from, inclusive);
			}
			else
			{
				descending(0.0f, from, inclusive);
				descending(to, duration, true);
			}
		}
	}

	void Animator::Fire(uint32_t layerIndex, Layer& layer, const AnimationClip& clip, const AnimationClip::EventMark& mark, bool forward)
	{
		// Played backwards, a range is entered at its end and left at its start.
		AnimationEventType type = mark.Type;
		if (!forward && type != AnimationEventType::Instant)
			type = type == AnimationEventType::RangeBegin ? AnimationEventType::RangeEnd : AnimationEventType::RangeBegin;

		// Names are interned, so the same name is the same pointer.
		const auto open = std::find_if(layer.OpenRanges.begin(), layer.OpenRanges.end(), [&](const OpenRange& range)
		{
			return range.Clip == &clip && range.Event == mark.Event && range.Name.data() == mark.Name.data();
		});
		if (type == AnimationEventType::RangeBegin)
		{
			if (open != layer.OpenRanges.end())
				return;
			const AnimationClipEvent& event = clip.GetEvents()[mark.Event];
			layer.OpenRanges.push_back({ &clip, mark.Event, mark.Name, forward ? event.EndTime : event.Time });
		}
		else if (type == AnimationEventType::RangeEnd)
		{
			if (open == layer.OpenRanges.end())
				return;
			layer.OpenRanges.erase(open);
		}

		m_Events.push_back({ mark.Name, mark.Time, type, &clip, layerIndex });
	}

	void Animator::CloseRanges(uint32_t layerIndex, Layer& layer)
	{
		for (const OpenRange& range : layer.OpenRanges)
			m_Events.push_back({ range.Name, range.EndTime, AnimationEventType::RangeEnd, range.Clip, layerIndex });
		layer.OpenRanges.clear();
	}

	void Animator::ForEachEventThisFrame(const std::function<void(const AnimationEvent&)>& fn) const
	{
		for (const AnimationEvent& event : m_Events)
			fn(event);
	}

	bool Animator::IsEventActive(std::string_view name) const
	{
		for (const Layer& layer : m_Layers)
		{
			for (const OpenRange& range : layer.OpenRanges)
			{
				if (range.Name == name)
					return true;
			}
		}
		return false;
	}

	void Animator::Evaluate()
	{
		if (!m_Skeleton)
			return;

		SyncSkeleton();
		EvaluateLayer(m_Layers.front(), m_RestPoses, m_LocalPoses);
		m_UpperLayersApplied = false;

		for (size_t i = 1; i < m_Layers.size(); ++i)
		{
			Layer& layer = m_Layers[i];
			const float weight = std::clamp(layer.Settings.GetWeight(), 0.0f, 1.0f);
			if (!(weight > 0.0f) || layer.States.empty() || (layer.States.size() == 1 && !layer.States[0].Frozen && IsTransparent(layer.States[0].State)))
				continue;

			if (!m_UpperLayersApplied)
			{
				m_Layers.front().Pose = m_LocalPoses;
				m_UpperLayersApplied = true;
			}

			EvaluateLayer(layer, m_LocalPoses, layer.Pose);
			for (size_t j = 0; j < m_LocalPoses.size(); ++j)
			{
				const float jointWeight = weight * layer.Mask[j];
				if (jointWeight >= 1.0f)
					m_LocalPoses[j] = layer.Pose[j];
				else if (jointWeight > 0.0f)
					BlendJoint(m_LocalPoses[j], layer.Pose[j], jointWeight);
			}
		}

		m_Skeleton->ComputeGlobalTransforms(m_LocalPoses, m_Globals);
		m_Skeleton->ComputeSkinningPalette(m_Globals, m_Palette);
	}

	void Animator::EvaluateLayer(Layer& layer, std::span<const JointPose> underneath, std::span<JointPose> out)
	{
		if (layer.States.empty())
		{
			std::copy(underneath.begin(), underneath.end(), out.begin());
			return;
		}

		Sample(layer, layer.States.front(), underneath, out);
		for (size_t i = 1; i < layer.States.size(); ++i)
		{
			Sample(layer, layer.States[i], underneath, m_Scratch);
			BlendPoses(out, m_Scratch, FadeWeight(layer.States[i]));
		}
	}

	void Animator::Sample(const Layer& layer, PlayingState& state, std::span<const JointPose> underneath, std::span<JointPose> out)
	{
		if (state.Frozen)
		{
			std::copy(layer.FrozenPose.begin(), layer.FrozenPose.end(), out.begin());
			return;
		}

		// Joints the clip leaves alone keep the pose from below.
		std::copy(underneath.begin(), underneath.end(), out.begin());

		// A model reload can change a playing clip's channels. Cursors are only search hints.
		const size_t cursorCount = CursorCount(state.State);
		if (state.Cursors.size() != cursorCount)
			state.Cursors.assign(cursorCount, 0);

		if (!state.State.IsBlend())
		{
			if (const AnimationClip* clip = state.State.GetClip())
				SampleClip(*clip, state.Time, state.Cursors.data(), out);
			return;
		}

		const std::vector<BlendPoint>& points = state.State.GetPoints();
		const BlendSpot spot = Locate(state.State);

		uint32_t* cursors = state.Cursors.data();
		for (size_t i = 0; i < spot.Lower; ++i)
			cursors += points[i].Clip ? points[i].Clip->GetChannels().size() * 3 : 0;

		const BlendPoint& lower = points[spot.Lower];
		if (lower.Clip)
			SampleClip(*lower.Clip, state.Time * lower.Clip->GetDuration(), cursors, out);
		if (!(spot.T > 0.0f))
			return;

		const BlendPoint& upper = points[spot.Lower + 1];
		std::copy(underneath.begin(), underneath.end(), m_BlendScratch.begin());
		if (upper.Clip)
			SampleClip(*upper.Clip, state.Time * upper.Clip->GetDuration(), cursors + (lower.Clip ? lower.Clip->GetChannels().size() * 3 : 0), m_BlendScratch);
		BlendPoses(out, m_BlendScratch, spot.T);
	}

	void Animator::SampleClip(const AnimationClip& clip, float time, uint32_t* cursors, std::span<JointPose> out)
	{
		const ClipBinding& clipBinding = Bind(clip);
		const std::vector<AnimationChannel>& channels = clip.GetChannels();
		for (size_t c = 0; c < channels.size(); ++c)
		{
			const ChannelBinding& binding = clipBinding.Channels[c];
			if (binding.Joint == Skeleton::k_InvalidJoint)
				continue;

			const AnimationChannel& channel = channels[c];
			uint32_t* channelCursors = cursors + c * 3;
			JointPose& pose = out[binding.Joint];

			if (binding.UseTranslation && !channel.Translation.IsEmpty())
				pose.Translation = binding.TranslationOffset + SampleTrack(channel.Translation, time, channelCursors[0], MixVec3) * binding.TranslationScale;
			if (!channel.Rotation.IsEmpty())
				pose.Rotation = SampleTrack(channel.Rotation, time, channelCursors[1], MixQuat);
			if (binding.UseScale && !channel.Scale.IsEmpty())
				pose.Scale = SampleTrack(channel.Scale, time, channelCursors[2], MixVec3);
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
