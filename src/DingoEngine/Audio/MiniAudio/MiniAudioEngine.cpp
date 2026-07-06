#include "depch.h"
#include "DingoEngine/Audio/MiniAudio/MiniAudioEngine.h"

// All miniaudio usage is confined to this .cpp (+ the engine-internal MiniAudioData.h).
#include "DingoEngine/Audio/MiniAudio/MiniAudioData.h"

// OGG/Vorbis support: miniaudio's built-in Vorbis backend is a thin wrapper over
// stb_vorbis, but stb_vorbis is NOT bundled inside miniaudio.h. Including stb_vorbis.c
// here (which defines STB_VORBIS_INCLUDE_STB_VORBIS_H) makes miniaudio define
// MA_HAS_VORBIS, so ma_engine/ma_decoder decode .ogg automatically alongside the
// built-in .wav decoder. This is miniaudio's documented way to enable Vorbis.
#include "stb_vorbis.c"

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

namespace Dingo
{

	namespace
	{
		// AudioSoundId layout: high 16 bits = generation, low 16 bits = slot index.
		// A handle is only valid if the slot is live (Sound != nullptr) AND its generation
		// still matches, so a stale handle (its sound finished/was reaped) can never
		// control the newer sound that reused the slot. This mirrors Jolt's index+sequence
		// body ids.
		constexpr std::uint32_t k_IndexBits = 16;
		constexpr std::uint32_t k_IndexMask = (1u << k_IndexBits) - 1u;

		AudioSoundId MakeId(std::uint32_t index, std::uint16_t generation)
		{
			return (static_cast<std::uint32_t>(generation) << k_IndexBits) | (index & k_IndexMask);
		}

		std::uint32_t IndexOf(AudioSoundId id) { return id & k_IndexMask; }
		std::uint16_t GenerationOf(AudioSoundId id) { return static_cast<std::uint16_t>(id >> k_IndexBits); }

		// The concrete clip: owns a "template" ma_sound loaded fully into memory via the
		// engine's resource manager. It is never played directly — Play() clones it with
		// ma_sound_init_copy so the decoded data is shared across every instance.
		class MiniAudioClip final : public AudioClip
		{
		public:
			~MiniAudioClip() override
			{
				if (m_Loaded)
					ma_sound_uninit(&m_Template);
			}

			ma_sound* Template() { return &m_Template; }
			ma_sound m_Template{};
			bool m_Loaded = false;
		};
	}

	MiniAudioEngine::~MiniAudioEngine()
	{
		Shutdown();
	}

	void MiniAudioEngine::Initialize()
	{
		if (m_Data)
			return; // already live

		m_Data = new Internal::MiniAudioData();
		m_Data->Engine = new ma_engine();

		// NULL config = sensible defaults: default playback device, embedded resource
		// manager (needed for ma_sound_init_copy), one listener.
		const ma_result result = ma_engine_init(nullptr, m_Data->Engine);
		if (result != MA_SUCCESS)
		{
			DE_CORE_ERROR("MiniAudioEngine: ma_engine_init failed ({})", (int)result);
			delete m_Data->Engine;
			delete m_Data;
			m_Data = nullptr;
			return;
		}
	}

	void MiniAudioEngine::Shutdown()
	{
		if (!m_Data)
			return;

		// Uninit every live sound before the engine so its mixer graph is clean. The
		// generation bump ReleaseSlot does is harmless here — the engine is going away.
		for (Internal::SoundSlot& slot : m_Data->Slots)
		{
			if (slot.Sound)
				ReleaseSlot(slot);
		}
		m_Data->Slots.clear();

		ma_engine_uninit(m_Data->Engine); // stops the device thread
		delete m_Data->Engine;
		delete m_Data;
		m_Data = nullptr;
	}

	bool MiniAudioEngine::IsValid() const
	{
		return m_Data != nullptr;
	}

	void MiniAudioEngine::Update()
	{
		if (!m_Data)
			return;

		// Reap any finished non-looping sound (one-shots AND handle-returning sounds):
		// free the ma_sound and bump the slot generation so any lingering handle goes
		// stale. ma_sound_at_end is never true for a looping sound, so those persist
		// until the owner explicitly Stop()s them.
		for (Internal::SoundSlot& slot : m_Data->Slots)
		{
			if (!slot.Sound)
				continue;

			if (ma_sound_at_end(slot.Sound))
				ReleaseSlot(slot);
		}
	}

	std::shared_ptr<AudioClip> MiniAudioEngine::LoadClip(const std::filesystem::path& filepath)
	{
		if (!m_Data)
			return nullptr;

		if (!std::filesystem::exists(filepath))
		{
			DE_CORE_ERROR("AudioEngine::LoadClip: file not found '{}'", filepath.string());
			return nullptr;
		}

		auto clip = std::make_shared<MiniAudioClip>();

		// MA_SOUND_FLAG_DECODE: fully decode into memory now (so ma_sound_init_copy can
		// clone the decoded data buffer). No STREAM flag — streams can't be copied.
		const ma_result result = ma_sound_init_from_file(m_Data->Engine, filepath.string().c_str(),
			MA_SOUND_FLAG_DECODE, nullptr, nullptr, clip->Template());
		if (result != MA_SUCCESS)
		{
			DE_CORE_ERROR("AudioEngine::LoadClip failed for '{}' ({})", filepath.string(), (int)result);
			return nullptr; // clip's dtor won't uninit (m_Loaded still false)
		}

		clip->m_Loaded = true;
		return clip;
	}

	// Finds a free slot (reusing a reaped one) or grows the table. Returns the index.
	static std::uint32_t AcquireSlot(Internal::MiniAudioData& data)
	{
		for (std::uint32_t i = 0; i < data.Slots.size(); ++i)
		{
			if (!data.Slots[i].Sound)
				return i;
		}
		data.Slots.emplace_back();
		return static_cast<std::uint32_t>(data.Slots.size() - 1);
	}

	// Shared setup for Play / PlayOneShot: clones the clip into a fresh slot, applies
	// params, and starts it. Returns k_InvalidSound on any failure. The returned id
	// stays valid until the sound is Stop()ped or, if non-looping, finishes and is
	// reaped by Update() (which bumps the slot generation so the id then goes stale).
	static AudioSoundId StartInstance(Internal::MiniAudioData& data,
		const std::shared_ptr<AudioClip>& clip, const SoundPlayParams& params)
	{
		if (!clip)
			return k_InvalidSound;

		auto* concrete = static_cast<MiniAudioClip*>(clip.get());
		if (!concrete->m_Loaded)
			return k_InvalidSound;

		auto* sound = new ma_sound();
		const ma_result result = ma_sound_init_copy(data.Engine, concrete->Template(), 0, nullptr, sound);
		if (result != MA_SUCCESS)
		{
			DE_CORE_ERROR("AudioEngine: failed to instantiate sound ({})", (int)result);
			delete sound;
			return k_InvalidSound;
		}

		ma_sound_set_volume(sound, params.Volume);
		ma_sound_set_pitch(sound, params.Pitch);
		ma_sound_set_looping(sound, params.Looping ? MA_TRUE : MA_FALSE);
		ma_sound_set_spatialization_enabled(sound, params.Spatialized ? MA_TRUE : MA_FALSE);
		if (params.Spatialized)
			ma_sound_set_position(sound, params.Position.x, params.Position.y, params.Position.z);

		const std::uint32_t index = AcquireSlot(data);
		Internal::SoundSlot& slot = data.Slots[index];
		slot.Sound = sound;
		slot.Clip = clip; // keep the decoded data alive for this instance's lifetime

		ma_sound_start(sound);
		return MakeId(index, slot.Generation);
	}

	AudioSoundId MiniAudioEngine::Play(const std::shared_ptr<AudioClip>& clip, const SoundPlayParams& params)
	{
		if (!m_Data)
			return k_InvalidSound;
		return StartInstance(*m_Data, clip, params);
	}

	void MiniAudioEngine::PlayOneShot(const std::shared_ptr<AudioClip>& clip, float volume)
	{
		if (!m_Data)
			return;
		SoundPlayParams params;
		params.Volume = volume;
		StartInstance(*m_Data, clip, params);
	}

	void MiniAudioEngine::PlayOneShot(const std::shared_ptr<AudioClip>& clip, const glm::vec3& position, float volume)
	{
		if (!m_Data)
			return;
		SoundPlayParams params;
		params.Volume = volume;
		params.Spatialized = true;
		params.Position = position;
		StartInstance(*m_Data, clip, params);
	}

	Internal::SoundSlot* MiniAudioEngine::ResolveSlot(AudioSoundId id) const
	{
		if (!m_Data || id == k_InvalidSound)
			return nullptr;

		const std::uint32_t index = IndexOf(id);
		if (index >= m_Data->Slots.size())
			return nullptr;

		Internal::SoundSlot& slot = m_Data->Slots[index];
		if (!slot.Sound || slot.Generation != GenerationOf(id))
			return nullptr;

		return &slot;
	}

	// Tears down a live slot's ma_sound and bumps its generation so any handle pointing
	// at it goes stale, ready for AcquireSlot to reuse.
	void MiniAudioEngine::ReleaseSlot(Internal::SoundSlot& slot)
	{
		ma_sound_uninit(slot.Sound);
		delete slot.Sound;
		slot.Sound = nullptr;
		slot.Clip.reset();
		++slot.Generation;
	}

	void MiniAudioEngine::Stop(AudioSoundId sound)
	{
		// Full stop: tear the instance down and recycle the slot. ma_sound_stop alone
		// would only pause; Stop() is terminal.
		if (Internal::SoundSlot* slot = ResolveSlot(sound))
			ReleaseSlot(*slot);
	}

	void MiniAudioEngine::Pause(AudioSoundId sound)
	{
		if (Internal::SoundSlot* slot = ResolveSlot(sound))
			ma_sound_stop(slot->Sound); // keeps the playback cursor; Resume() continues from here
	}

	void MiniAudioEngine::Resume(AudioSoundId sound)
	{
		if (Internal::SoundSlot* slot = ResolveSlot(sound))
			ma_sound_start(slot->Sound);
	}

	bool MiniAudioEngine::IsPlaying(AudioSoundId sound) const
	{
		Internal::SoundSlot* slot = ResolveSlot(sound);
		return slot ? ma_sound_is_playing(slot->Sound) == MA_TRUE : false;
	}

	void MiniAudioEngine::SetVolume(AudioSoundId sound, float volume)
	{
		if (Internal::SoundSlot* slot = ResolveSlot(sound))
			ma_sound_set_volume(slot->Sound, volume);
	}

	void MiniAudioEngine::SetPitch(AudioSoundId sound, float pitch)
	{
		if (Internal::SoundSlot* slot = ResolveSlot(sound))
			ma_sound_set_pitch(slot->Sound, pitch);
	}

	void MiniAudioEngine::SetLooping(AudioSoundId sound, bool looping)
	{
		if (Internal::SoundSlot* slot = ResolveSlot(sound))
			ma_sound_set_looping(slot->Sound, looping ? MA_TRUE : MA_FALSE);
	}

	void MiniAudioEngine::SetPosition(AudioSoundId sound, const glm::vec3& position)
	{
		if (Internal::SoundSlot* slot = ResolveSlot(sound))
			ma_sound_set_position(slot->Sound, position.x, position.y, position.z);
	}

	void MiniAudioEngine::SetMasterVolume(float volume)
	{
		if (m_Data)
			ma_engine_set_volume(m_Data->Engine, volume);
	}

	float MiniAudioEngine::GetMasterVolume() const
	{
		if (!m_Data)
			return 0.0f;
		return ma_engine_get_volume(m_Data->Engine);
	}

	std::uint32_t MiniAudioEngine::GetActiveSoundCount() const
	{
		if (!m_Data)
			return 0;

		std::uint32_t count = 0;
		for (const Internal::SoundSlot& slot : m_Data->Slots)
		{
			if (slot.Sound)
				++count;
		}
		return count;
	}

	void MiniAudioEngine::SetListenerPosition(const glm::vec3& position)
	{
		if (m_Data)
			ma_engine_listener_set_position(m_Data->Engine, 0, position.x, position.y, position.z);
	}

	void MiniAudioEngine::SetListenerOrientation(const glm::vec3& forward, const glm::vec3& up)
	{
		if (!m_Data)
			return;
		ma_engine_listener_set_direction(m_Data->Engine, 0, forward.x, forward.y, forward.z);
		ma_engine_listener_set_world_up(m_Data->Engine, 0, up.x, up.y, up.z);
	}

}
