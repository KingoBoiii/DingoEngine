#pragma once
#include "DingoEngine/Graphics/Skeleton.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <span>
#include <unordered_map>
#include <vector>

namespace Dingo
{

	class AnimationClip;

	// What an Animator plays.
	class AnimationState
	{
	public:
		// A null clip is the skeleton's rest pose.
		static AnimationState Clip(const AnimationClip* clip);

		AnimationState& SetLoop(bool loop) { m_Loop = loop; return *this; }
		// A multiple of the clip's own pace; negative plays it backwards.
		AnimationState& SetSpeed(float speed) { m_Speed = speed; return *this; }

		const AnimationClip* GetClip()   const { return m_Clip; }
		bool                 IsLooping() const { return m_Loop; }
		float                GetSpeed()  const { return m_Speed; }

	private:
		const AnimationClip* m_Clip = nullptr;
		bool  m_Loop = true;
		float m_Speed = 1.0f;
	};

	// Plays clips on a skeleton and keeps its pose: the local joint poses, the joint transforms and
	// the palette Renderer3D::SubmitSkinnedMesh takes. Play cross-fades from whatever shows now, so
	// a Play in the middle of a fade blends on from the mix instead of popping.
	//
	// A clip loaded for another skeleton is retargeted by joint name. Rotations come from the clip.
	// Translation comes only on its root-most animated joints (usually the hips), scaled by the
	// ratio of the two skeletons' rest offsets of that joint from its parent; every other joint
	// keeps this skeleton's rest translation and scale. The rigs need the same joint names and rest
	// orientations.
	class Animator
	{
	public:
		explicit Animator(const Skeleton* skeleton = nullptr);

		// Binds another skeleton (or none) and returns to its rest pose with nothing playing.
		void            SetSkeleton(const Skeleton* skeleton);
		const Skeleton* GetSkeleton() const { return m_Skeleton; }

		// Starts `state` and fades it in over fadeSeconds; 0 cuts to it. Playing the state that already
		// plays (same clip, loop and speed) keeps it going, finished or not, so a script may call this
		// every frame; with a fade of 0 it finishes that state's fade-in. SetTime(0) restarts it.
		void Play(const AnimationState& state, float fadeSeconds = 0.0f);
		void Play(const AnimationClip* clip, float fadeSeconds = 0.0f) { Play(AnimationState::Clip(clip), fadeSeconds); }
		// Fades back to the rest pose.
		void Stop(float fadeSeconds = 0.0f) { Play(AnimationState::Clip(nullptr), fadeSeconds); }

		// The state the latest Play started; older ones may still be fading out under it.
		const AnimationClip* GetCurrentClip() const;
		float GetTime() const;           // seconds into the current clip
		float GetNormalizedTime() const; // 0 to 1 through the current clip
		// Seeks the current clip; the pose follows on the next Update (Update(0) re-evaluates).
		void SetTime(float seconds);
		// A clip that doesn't loop has reached its end, or its start when time last ran backwards.
		bool IsFinished() const;
		bool IsFading()   const { return m_States.size() > 1; }

		// Advances every playing state by deltaTime and evaluates the pose.
		void Update(float deltaTime);

		// One per skeleton joint; the rest pose until the first Update.
		std::span<const JointPose> GetLocalPoses()       const { return m_LocalPoses; }
		// Relative to the skeleton's root transform, as Skeleton::ComputeGlobalTransforms gives them.
		std::span<const glm::mat4> GetGlobalTransforms() const { return m_Globals; }
		// One per skin joint.
		std::span<const glm::mat4> GetSkinningPalette()  const { return m_Palette; }
		// RootTransform x global: the joint in the model's space. Identity for an invalid index.
		glm::mat4 GetJointTransform(int32_t joint) const;

	private:
		struct ChannelBinding
		{
			int32_t   Joint = Skeleton::k_InvalidJoint;
			bool      UseTranslation = true;
			bool      UseScale = true;
			// translation = Offset + key * Scale; identity unless the channel is a retargeted root.
			float     TranslationScale = 1.0f;
			glm::vec3 TranslationOffset{ 0.0f };
		};

		struct ClipBinding
		{
			std::vector<ChannelBinding> Channels;
		};

		// Its binding is looked up by clip id rather than pointed at, so copying an Animator is safe.
		struct PlayingState
		{
			AnimationState State;
			float          Time = 0.0f;
			// -1 once time last moved backwards (a negative speed or deltaTime), else 1.
			float          Direction = 1.0f;
			float          FadeDuration = 0.0f;
			float          FadeElapsed = 0.0f;
			// Holds m_FrozenPose instead of sampling a clip.
			bool           Frozen = false;
			// Three key cursors per channel: translation, rotation, scale.
			std::vector<uint32_t> Cursors;
		};

		const ClipBinding& Bind(const AnimationClip& clip);
		void  Sample(PlayingState& state, std::span<JointPose> out);
		void  Evaluate();
		void  ResetToRest();
		float FadeWeight(const PlayingState& state) const;
		float WrapTime(const PlayingState& state, float time) const;

	private:
		// Beyond this many states fading at once, the mix so far is frozen into one pose.
		static constexpr size_t k_MaxStates = 4;

		const Skeleton* m_Skeleton = nullptr;
		std::vector<PlayingState> m_States; // oldest first; each fades in over the ones before it
		std::unordered_map<uint64_t, ClipBinding> m_Bindings; // by AnimationClip::GetId()

		std::vector<JointPose> m_LocalPoses;
		std::vector<JointPose> m_Scratch;
		std::vector<JointPose> m_FrozenPose;
		std::vector<glm::mat4> m_Globals;
		std::vector<glm::mat4> m_Palette;
	};

}
