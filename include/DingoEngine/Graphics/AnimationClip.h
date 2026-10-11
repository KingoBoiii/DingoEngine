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

	enum class AnimationEventType : uint8_t
	{
		Instant,
		RangeBegin,
		RangeEnd
	};

	// An event's data: `key=value` pairs separated by spaces (`damage=12 reach=1.4 sound="heavy hit"`),
	// a bare key being a flag with an empty value. Text is kept for the life of the program, like an
	// event's name. The first pair with a key wins.
	struct AnimationEventPayload
	{
		std::string_view Text;

		bool IsEmpty() const { return Text.empty(); }
		bool Has(std::string_view key) const;
		// The value as written, quotes removed; fallback when the key is missing.
		std::string_view GetString(std::string_view key, std::string_view fallback = std::string_view()) const;
		// fallback when the key is missing or its value isn't a whole number of that kind.
		float GetFloat(std::string_view key, float fallback = 0.0f) const;
		int32_t GetInt(std::string_view key, int32_t fallback = 0) const;
	};

	// A named moment on a clip's timeline (a footstep), or a named stretch of it (a sword's hitbox).
	struct AnimationClipEvent
	{
		std::string Name;
		float Time = 0.0f;    // seconds; a range's start
		float EndTime = 0.0f; // a range's end; Time for an instant
		bool  Range = false;
		std::string Payload;  // AnimationEventPayload's text
	};

	class AnimationClip
	{
	public:
		// Every instant, range start and range end, by time; at one time an end comes before an
		// instant and an instant before a start, and a range of zero length opens and closes among
		// the instants. What an Animator walks as playback crosses them.
		struct EventMark
		{
			float Time = 0.0f;
			uint32_t Event = 0; // index into GetEvents()
			AnimationEventType Type = AnimationEventType::Instant;
			// The event's name, kept for the life of the program, so a name an Animator hands out
			// outlives a change to this list or a reload of the model.
			std::string_view Name;
			AnimationEventPayload Payload;
		};

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

		// Never reused, so a cache keyed on it can't mistake a new clip at a freed one's address
		// for the old one.
		uint64_t GetId() const { return m_Id; }

		// Times are in seconds into the clip; an event outside [0, duration] never fires, and a range
		// can't wrap past the clip's end (an end before the begin is swapped). An Animator reports
		// them as its playback crosses them (Animator::GetEventsThisFrame). A model reload replaces
		// them with the file's and its .events sidecar's, so events added here must be added again
		// (Model::GetGeneration tells when).
		// `payload` is AnimationEventPayload's text: `key=value` pairs separated by spaces.
		void AddEvent(float time, std::string name, std::string payload = std::string());
		void AddEventRange(float begin, float end, std::string name, std::string payload = std::string());
		void ClearEvents();
		const std::vector<AnimationClipEvent>& GetEvents()     const { return m_Events; }
		const std::vector<EventMark>&          GetEventMarks() const { return m_EventMarks; }
		// Changes whenever the event list does, never back to an earlier value; an Animator then
		// checks the ranges it holds open on the clip.
		uint64_t GetEventRevision() const { return m_EventRevision; }

	private:
		static uint64_t AllocateId();
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
