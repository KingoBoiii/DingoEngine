#pragma once
#include "DingoEngine/Graphics/Skeleton.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Dingo
{

	class AnimationClip;

	struct BlendPoint
	{
		float Value = 0.0f;
		const AnimationClip* Clip = nullptr;
	};

	// What an Animator plays: one clip, or a blend of clips along a float parameter.
	class AnimationState
	{
	public:
		// A null clip is what shows without it: the rest pose on layer 0, the layers below on others.
		static AnimationState Clip(const AnimationClip* clip);
		// Blends the two clips whose Values bracket the parameter (Animator::SetFloat), past either
		// end the end clip alone. The clips play in step: the same fraction of their own cycle,
		// advanced at the blended cycle length, so a walk and a run put their feet down together.
		static AnimationState Blend1D(std::string parameter, std::vector<BlendPoint> points);

		AnimationState& SetLoop(bool loop) { m_Loop = loop; return *this; }
		// A multiple of the clip's own pace; negative plays it backwards.
		AnimationState& SetSpeed(float speed) { m_Speed = speed; return *this; }

		// Null for a blend.
		const AnimationClip*           GetClip()      const { return m_Clip; }
		bool                           IsBlend()      const { return !m_Points.empty(); }
		const std::string&             GetParameter() const { return m_Parameter; }
		// Sorted by Value.
		const std::vector<BlendPoint>& GetPoints()    const { return m_Points; }
		bool                           IsLooping()    const { return m_Loop; }
		float                          GetSpeed()     const { return m_Speed; }

		// The same clip or blend, played the same way.
		bool operator==(const AnimationState& other) const;

	private:
		const AnimationClip* m_Clip = nullptr;
		std::string m_Parameter;
		std::vector<BlendPoint> m_Points;
		bool  m_Loop = true;
		float m_Speed = 1.0f;
	};

	// A layer above the first overrides the joints in its mask, at its weight.
	class AnimationLayer
	{
	public:
		// The subtree under this joint, itself included; empty (the default) is the whole skeleton.
		AnimationLayer& SetMask(std::string rootJoint) { m_MaskRoot = std::move(rootJoint); return *this; }
		// Leaves this joint's subtree out of the mask.
		AnimationLayer& Exclude(std::string joint) { m_Exclusions.push_back(std::move(joint)); return *this; }
		// 0 shows the layers below, 1 replaces them inside the mask.
		AnimationLayer& SetWeight(float weight) { m_Weight = weight; return *this; }

		const std::string&              GetMaskRoot()   const { return m_MaskRoot; }
		const std::vector<std::string>& GetExclusions() const { return m_Exclusions; }
		float                           GetWeight()     const { return m_Weight; }

	private:
		std::string m_MaskRoot;
		std::vector<std::string> m_Exclusions;
		float m_Weight = 1.0f;
	};

	// Plays clips on a skeleton and keeps its pose: the local joint poses, the joint transforms and
	// the palette Renderer3D::SubmitSkinnedMesh takes.
	//
	// Layer 0 poses the whole body; a higher layer (SetLayer) overrides the joints in its mask, on
	// top of the ones below, in index order. Each layer plays one state at a time: Play cross-fades
	// from whatever the layer shows now, so a Play in the middle of a fade blends on from the mix
	// instead of popping. Joints a layer's clip doesn't animate keep the pose from below.
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

		// Binds another skeleton (or none) and returns to its rest pose with nothing playing. Layer
		// settings and parameters stay.
		void            SetSkeleton(const Skeleton* skeleton);
		const Skeleton* GetSkeleton() const { return m_Skeleton; }

		// Starts `state` on `layer` and fades it in over fadeSeconds; 0 cuts to it. Playing the state
		// that already plays there (same clip or blend, loop and speed) keeps it going, finished or
		// not, so a script may call this every frame; with a fade of 0 it finishes that state's
		// fade-in. SetTime(0) restarts it. A layer that doesn't exist yet is made, over the whole body.
		void Play(const AnimationState& state, float fadeSeconds = 0.0f, uint32_t layer = 0);
		void Play(const AnimationClip* clip, float fadeSeconds = 0.0f, uint32_t layer = 0) { Play(AnimationState::Clip(clip), fadeSeconds, layer); }
		// Fades to what shows without the layer: the rest pose on layer 0, the layers below on others.
		void Stop(float fadeSeconds = 0.0f, uint32_t layer = 0) { Play(AnimationState::Clip(nullptr), fadeSeconds, layer); }
		// Plays `clip` once over what the layer plays now, then fades back to it over fadeOut,
		// ending as the clip does. What it interrupted keeps its time running meanwhile, so a walk
		// comes back in step. Another one-shot restarts it; a Play of anything but the interrupted
		// state cancels the way back, and playing that state again (every frame, say) changes nothing.
		void PlayOneShot(const AnimationClip* clip, float fadeIn = 0.1f, float fadeOut = 0.2f, uint32_t layer = 0);
		// True from PlayOneShot until the layer starts fading back, fadeOut before the clip ends.
		bool IsOneShotPlaying(uint32_t layer = 0) const;

		// Layer 0 is always the whole body at full weight; index 0 is ignored here.
		void            SetLayer(uint32_t index, const AnimationLayer& layer);
		void            SetLayerWeight(uint32_t index, float weight);
		uint32_t        GetLayerCount() const { return static_cast<uint32_t>(m_Layers.size()); }
		// The settings of a layer below GetLayerCount().
		const AnimationLayer& GetLayer(uint32_t index) const { return m_Layers[index].Settings; }

		// The parameters Blend1D states read; an unset one is 0.
		void  SetFloat(std::string_view name, float value);
		float GetFloat(std::string_view name) const;

		// The state the latest Play started on that layer; older ones may still be fading out under
		// it. For a blend, the clip with the larger share.
		const AnimationClip* GetCurrentClip(uint32_t layer = 0) const;
		float GetTime(uint32_t layer = 0) const;           // seconds into the current clip or blend cycle
		float GetNormalizedTime(uint32_t layer = 0) const; // 0 to 1 through it
		// Seek the current state; the pose follows on the next Update (Update(0) re-evaluates).
		void SetTime(float seconds, uint32_t layer = 0);
		void SetNormalizedTime(float fraction, uint32_t layer = 0);
		// A state that doesn't loop has reached its end, or its start when time last ran backwards.
		bool IsFinished(uint32_t layer = 0) const;
		bool IsFading(uint32_t layer = 0) const;

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

		// Its bindings are looked up by clip id rather than pointed at, so copying an Animator is safe.
		struct PlayingState
		{
			AnimationState State;
			// A clip: seconds. A blend: the fraction of the cycle every clip in it shares.
			float          Time = 0.0f;
			// -1 once time last moved backwards (a negative speed or deltaTime), else 1.
			float          Direction = 1.0f;
			float          FadeDuration = 0.0f;
			float          FadeElapsed = 0.0f;
			// Holds the layer's FrozenPose instead of sampling a clip.
			bool           Frozen = false;
			// Three key cursors (translation, rotation, scale) per channel of each clip in turn.
			std::vector<uint32_t> Cursors;
		};

		struct Layer
		{
			AnimationLayer Settings;
			std::vector<float> Mask; // per joint, 1 inside
			std::vector<PlayingState> States; // oldest first; each fades in over the ones before it
			std::vector<JointPose> FrozenPose;
			// The layer's own result before masking; for layer 0 only while layers above apply.
			std::vector<JointPose> Pose;
			// A one-shot's way back: the state it interrupted, its time still running.
			bool OneShotPending = false;
			const AnimationClip* OneShotClip = nullptr;
			float OneShotFadeOut = 0.0f;
			PlayingState Resume;
		};

		// Where a blend stands for the current parameter: points[Lower], and points[Lower + 1] at
		// weight T when T > 0.
		struct BlendSpot
		{
			size_t Lower = 0;
			float  T = 0.0f;
		};

		Layer& EnsureLayer(uint32_t index);
		void   ResolveMask(Layer& layer);
		void   Push(Layer& layer, PlayingState state, float fadeSeconds, std::span<const JointPose> current);
		// What the layer shows now, for freezing it.
		std::span<const JointPose> FreezeSource(size_t layer) const;
		PlayingState MakeState(const AnimationState& state);
		void   Advance(PlayingState& state, float deltaTime) const;
		void   EvaluateLayer(Layer& layer, std::span<const JointPose> underneath, std::span<JointPose> out);
		void   Sample(const Layer& layer, PlayingState& state, std::span<const JointPose> underneath, std::span<JointPose> out);
		void   SampleClip(const AnimationClip& clip, float time, uint32_t* cursors, std::span<JointPose> out);
		void   Evaluate();
		void   ResetToRest();

		const ClipBinding& Bind(const AnimationClip& clip);
		BlendSpot Locate(const AnimationState& state) const;
		float  Duration(const AnimationState& state) const;
		float  FadeWeight(const PlayingState& state) const;
		float  Wrap(const PlayingState& state, float time) const;
		const PlayingState* Current(uint32_t layer) const;

	private:
		// Beyond this many states fading at once on a layer, the mix so far is frozen into one pose.
		static constexpr size_t k_MaxStates = 4;

		struct NameHash
		{
			using is_transparent = void;
			size_t operator()(std::string_view name) const { return std::hash<std::string_view>{}(name); }
		};

		const Skeleton* m_Skeleton = nullptr;
		std::vector<Layer> m_Layers;
		std::unordered_map<std::string, float, NameHash, std::equal_to<>> m_Parameters;
		std::unordered_map<uint64_t, ClipBinding> m_Bindings; // by AnimationClip::GetId()

		std::vector<JointPose> m_RestPoses;
		std::vector<JointPose> m_LocalPoses;
		// Set when a layer above 0 changed m_LocalPoses in the last Evaluate; layer 0's own result is
		// then kept in its Pose.
		bool m_UpperLayersApplied = false;
		std::vector<JointPose> m_Scratch;
		std::vector<JointPose> m_BlendScratch;
		std::vector<glm::mat4> m_Globals;
		std::vector<glm::mat4> m_Palette;
	};

}
