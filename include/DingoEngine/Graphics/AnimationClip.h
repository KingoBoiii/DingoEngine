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

	/// How a track's value runs between two keys.
	enum class AnimationInterpolation : uint8_t
	{
		/// Holds a key's value until the next key.
		Step,
		/// Blends linearly between keys (spherically for rotations).
		Linear
	};

	/// The keys of one property (translation, rotation or scale) of one joint over a clip.
	template<typename T>
	struct AnimationTrack
	{
		/// Seconds into the clip, ascending; one value per time.
		std::vector<float> Times;
		std::vector<T>     Values; ///< One value per entry of Times.
		AnimationInterpolation Interpolation = AnimationInterpolation::Linear; ///< Between keys.

		/// True with no keys, which leaves the property at the joint's rest value.
		bool IsEmpty() const { return Times.empty(); }
	};

	/// The keys of one joint, bound to a skeleton by name. An empty track leaves that part of
	/// the joint at its rest pose; assimp gives every imported channel all three tracks.
	struct AnimationChannel
	{
		std::string               JointName; ///< The Joint::Name this channel animates.
		/// Local translation keys, in the clip's model units.
		AnimationTrack<glm::vec3> Translation;
		AnimationTrack<glm::quat> Rotation; ///< Local rotation keys.
		AnimationTrack<glm::vec3> Scale; ///< Local scale keys.
	};

	/// Which mark of an event an AnimationEvent reports.
	enum class AnimationEventType : uint8_t
	{
		/// A moment event (a footstep) firing once.
		Instant,
		/// The start of a range event (a hitbox opening).
		RangeBegin,
		/// The end of a range event; also delivered when a range is cut short.
		RangeEnd
	};

	/// An event's data: `key=value` pairs separated by spaces (`damage=12 reach=1.4 sound="heavy
	/// hit"`), a bare key being a flag with an empty value. Text is kept for the life of the
	/// program, like an event's name. The first pair with a key wins.
	struct AnimationEventPayload
	{
		std::string_view Text; ///< The raw payload text; valid for the life of the program.

		/// True when the payload has no text.
		bool IsEmpty() const { return Text.empty(); }
		/// True when a pair or bare flag with that key exists.
		bool Has(std::string_view key) const;
		/// The value as written, quotes removed; fallback when the key is missing.
		std::string_view GetString(std::string_view key, std::string_view fallback = std::string_view()) const;
		///@{
		/// The value as a number; fallback when the key is missing or its value isn't a whole
		/// number of that kind.
		float GetFloat(std::string_view key, float fallback = 0.0f) const;
		int32_t GetInt(std::string_view key, int32_t fallback = 0) const;
		///@}
	};

	/// A named moment on a clip's timeline (a footstep), or a named stretch of it (a sword's
	/// hitbox).
	struct AnimationClipEvent
	{
		std::string Name; ///< The name scripts match on; shared by every mark of the event.
		float Time = 0.0f;    ///< Seconds into the clip; a range's start.
		float EndTime = 0.0f; ///< Seconds; a range's end, Time for an instant.
		bool  Range = false;  ///< True for a range event, false for an instant.
		std::string Payload;  ///< AnimationEventPayload's text.
	};

	/// A sampled animation: per-joint keys, a duration and a timeline of events. Owned by its
	/// Model.
	class AnimationClip
	{
	public:
		/// Every instant, range start and range end, by time; at one time an end comes before an
		/// instant and an instant before a start, and a range of zero length opens and closes among
		/// the instants. What an Animator walks as playback crosses them.
		struct EventMark
		{
			float Time = 0.0f; ///< Seconds into the clip.
			uint32_t Event = 0; ///< Index into GetEvents().
			/// Which mark of the event this is.
			AnimationEventType Type = AnimationEventType::Instant;
			/// The event's name, kept for the life of the program, so a name an Animator hands out
			/// outlives a change to this list or a reload of the model.
			std::string_view Name;
			/// The event's data; the same text for each of its marks.
			AnimationEventPayload Payload;
		};

	public:
		/// Builds a clip; duration is in seconds. A track with unequal Times and Values is trimmed
		/// to the shorter, with a warning. sourceSkeleton is the skeleton the keys were authored
		/// for and must outlive the clip.
		AnimationClip(std::string name, float duration, std::vector<AnimationChannel> channels, const Skeleton* sourceSkeleton);

		/// The clip's name; clips of one model can share one (Mixamo calls every clip
		/// "mixamo.com").
		const std::string&                   GetName()     const { return m_Name; }
		/// Length in seconds; 0 for a clip a model reload no longer found in the file.
		float                                GetDuration() const { return m_Duration; }
		/// One channel per animated joint.
		const std::vector<AnimationChannel>& GetChannels() const { return m_Channels; }
		/// The skeleton of the file the clip was loaded from, owned by the same Model. Retargeting
		/// reads its rest pose.
		const Skeleton*                      GetSourceSkeleton() const { return m_SourceSkeleton; }

		/// nullptr when the clip doesn't animate that joint.
		const AnimationChannel* FindChannel(std::string_view jointName) const;

		/// Never reused, so a cache keyed on it can't mistake a new clip at a freed one's address
		/// for the old one. A model reload gives the clip a new id.
		uint64_t GetId() const { return m_Id; }

		/// Adds an instant event at `time`, in seconds into the clip. An event outside [0,
		/// duration] never fires. An Animator reports events as its playback crosses them
		/// (Animator::GetEventsThisFrame). A model reload replaces them with the file's and its
		/// .events sidecar's, so events added here must be added again (Model::GetGeneration tells
		/// when). `payload` is AnimationEventPayload's text: `key=value` pairs separated by spaces.
		void AddEvent(float time, std::string name, std::string payload = std::string());
		/// Adds a range event from `begin` to `end`, in seconds. A range can't wrap past the clip's
		/// end (an end before the begin is swapped); otherwise as AddEvent.
		void AddEventRange(float begin, float end, std::string name, std::string payload = std::string());
		/// Removes every event, including those loaded from the .events sidecar.
		void ClearEvents();
		/// The events in the order they were added.
		const std::vector<AnimationClipEvent>& GetEvents()     const { return m_Events; }
		/// The same events as sorted marks, one per instant and two per range.
		const std::vector<EventMark>&          GetEventMarks() const { return m_EventMarks; }
		/// Changes whenever the event list does, never back to an earlier value; an Animator then
		/// checks the ranges it holds open on the clip.
		uint64_t GetEventRevision() const { return m_EventRevision; }

	private:
		static uint64_t AllocateId();
		// Adds an event without rebuilding the marks, for Model::LoadEvents, which rebuilds each clip
		// once at the end.
		void AppendEvent(float begin, float end, bool range, std::string name, std::string payload);
		void RebuildEventMarks();

		// A model reload: the source's keys and events under this clip's name, and a new id.
		void Reinitialize(AnimationClip& source, const Skeleton* sourceSkeleton);
		// A clip the reloaded file no longer has: zero length, no keys, no events.
		void Clear();

		friend class Model;

	private:
		uint64_t                      m_Id = AllocateId();
		uint64_t                      m_EventRevision = AllocateId();
		std::string                   m_Name;
		float                         m_Duration = 0.0f;
		std::vector<AnimationChannel> m_Channels;
		const Skeleton*               m_SourceSkeleton = nullptr;
		std::vector<AnimationClipEvent> m_Events;
		std::vector<EventMark>          m_EventMarks;
	};

}
