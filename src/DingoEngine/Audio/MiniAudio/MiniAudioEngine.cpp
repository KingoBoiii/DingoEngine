#include "depch.h"
#include "DingoEngine/Audio/MiniAudio/MiniAudioEngine.h"
#include "DingoEngine/Asset/AssetPath.h"

#include <algorithm>

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

		constexpr std::uint32_t k_MasterBusIndex = 0;
		constexpr std::string_view k_MasterBusName = "Master";

		AudioBusId MakeBusId(std::uint32_t index, std::uint16_t generation)
		{
			return static_cast<AudioBusId>(MakeId(index, generation));
		}

		ma_attenuation_model ToMiniAudio(AudioAttenuationModel model)
		{
			// miniaudio's own "none" skips the whole spatializer: panning, doppler, and the sound's
			// volume, which it keeps there. None is Inverse with no rolloff instead (ApplyAttenuation).
			switch (model)
			{
				case AudioAttenuationModel::None:        return ma_attenuation_model_inverse;
				case AudioAttenuationModel::Linear:      return ma_attenuation_model_linear;
				case AudioAttenuationModel::Exponential: return ma_attenuation_model_exponential;
				case AudioAttenuationModel::Inverse:     return ma_attenuation_model_inverse;
			}
			return ma_attenuation_model_inverse;
		}

		void ApplyAttenuation(ma_sound* sound, const SoundAttenuation& attenuation)
		{
			ma_sound_set_attenuation_model(sound, ToMiniAudio(attenuation.Model));
			// A zero min distance silences Inverse and divides by zero in Exponential.
			ma_sound_set_min_distance(sound, (std::max)(attenuation.MinDistance, 0.001f));
			ma_sound_set_max_distance(sound, attenuation.MaxDistance);
			ma_sound_set_rolloff(sound, attenuation.Model == AudioAttenuationModel::None ? 0.0f : attenuation.Rolloff);
			ma_sound_set_min_gain(sound, attenuation.MinGain);
			ma_sound_set_max_gain(sound, attenuation.MaxGain);
		}

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

		auto* root = new ma_sound();
		const ma_result rootResult = ma_sound_group_init(m_Data->Engine, MA_SOUND_FLAG_NO_PITCH, nullptr, root);
		if (rootResult != MA_SUCCESS)
		{
			DE_CORE_ERROR("MiniAudioEngine: ma_sound_group_init failed for the master bus ({})", (int)rootResult);
			delete root;
			ma_engine_uninit(m_Data->Engine);
			delete m_Data->Engine;
			delete m_Data;
			m_Data = nullptr;
			return;
		}

		Internal::BusSlot& master = m_Data->Buses.emplace_back();
		master.Group = root;
		master.Name = k_MasterBusName;
		master.Serial = 0;
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

		std::vector<std::uint32_t> buses;
		for (std::uint32_t i = 0; i < m_Data->Buses.size(); ++i)
		{
			if (m_Data->Buses[i].Group)
				buses.push_back(i);
		}
		std::sort(buses.begin(), buses.end(), [this](std::uint32_t a, std::uint32_t b)
		{
			return m_Data->Buses[a].Serial > m_Data->Buses[b].Serial;
		});
		for (std::uint32_t index : buses)
		{
			ma_sound_group_uninit(m_Data->Buses[index].Group);
			delete m_Data->Buses[index].Group;
		}
		m_Data->Buses.clear();

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

		const std::filesystem::path resolvedPath = Internal::ResolveRawAssetPath(filepath);

		if (!std::filesystem::exists(resolvedPath))
		{
			DE_CORE_ERROR("AudioEngine::LoadClip: file not found '{}'", resolvedPath.string());
			return nullptr;
		}

		auto clip = std::make_shared<MiniAudioClip>();

		// MA_SOUND_FLAG_DECODE: fully decode into memory now (so ma_sound_init_copy can
		// clone the decoded data buffer). No STREAM flag — streams can't be copied.
		const ma_result result = ma_sound_init_from_file(m_Data->Engine, resolvedPath.string().c_str(),
			MA_SOUND_FLAG_DECODE, nullptr, nullptr, clip->Template());
		if (result != MA_SUCCESS)
		{
			DE_CORE_ERROR("AudioEngine::LoadClip failed for '{}' ({})", resolvedPath.string(), (int)result);
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
		const std::shared_ptr<AudioClip>& clip, const SoundPlayParams& params,
		const SoundAttenuation& defaultAttenuation, std::optional<std::uint32_t> bus)
	{
		if (!clip)
			return k_InvalidSound;

		auto* concrete = static_cast<MiniAudioClip*>(clip.get());
		if (!concrete->m_Loaded)
			return k_InvalidSound;

		if (!bus)
		{
			if (!data.StaleBusWarned)
			{
				DE_CORE_WARN("AudioEngine: a sound was played on a destroyed or invalid bus; it plays on the master bus");
				data.StaleBusWarned = true;
			}
			bus = k_MasterBusIndex;
		}

		auto* sound = new ma_sound();
		const ma_result result = ma_sound_init_copy(data.Engine, concrete->Template(), 0, data.Buses[*bus].Group, sound);
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
		{
			ma_sound_set_position(sound, params.Position.x, params.Position.y, params.Position.z);
			ApplyAttenuation(sound, params.Attenuation.value_or(defaultAttenuation));
		}

		const std::uint32_t index = AcquireSlot(data);
		Internal::SoundSlot& slot = data.Slots[index];
		slot.Sound = sound;
		slot.Clip = clip; // keep the decoded data alive for this instance's lifetime
		slot.Bus = *bus;

		ma_sound_start(sound);
		return MakeId(index, slot.Generation);
	}

	AudioSoundId MiniAudioEngine::Play(const std::shared_ptr<AudioClip>& clip, const SoundPlayParams& params)
	{
		if (!m_Data)
			return k_InvalidSound;
		return StartInstance(*m_Data, clip, params, m_DefaultAttenuation, ResolveBus(params.Bus));
	}

	void MiniAudioEngine::PlayOneShot(const std::shared_ptr<AudioClip>& clip, float volume)
	{
		PlayOneShot(clip, k_MasterBus, volume);
	}

	void MiniAudioEngine::PlayOneShot(const std::shared_ptr<AudioClip>& clip, const glm::vec3& position, float volume)
	{
		PlayOneShot(clip, position, k_MasterBus, volume);
	}

	void MiniAudioEngine::PlayOneShot(const std::shared_ptr<AudioClip>& clip, AudioBusId bus, float volume)
	{
		if (!m_Data)
			return;
		SoundPlayParams params;
		params.Volume = volume;
		params.Bus = bus;
		Play(clip, params);
	}

	void MiniAudioEngine::PlayOneShot(const std::shared_ptr<AudioClip>& clip, const glm::vec3& position, AudioBusId bus, float volume)
	{
		if (!m_Data)
			return;
		SoundPlayParams params;
		params.Volume = volume;
		params.Spatialized = true;
		params.Position = position;
		params.Bus = bus;
		Play(clip, params);
	}

	std::optional<std::uint32_t> MiniAudioEngine::ResolveBus(AudioBusId id) const
	{
		if (!m_Data || id == k_InvalidBus)
			return std::nullopt;

		const std::uint32_t packed = static_cast<std::uint32_t>(id);
		const std::uint32_t index = IndexOf(packed);
		if (index >= m_Data->Buses.size())
			return std::nullopt;

		const Internal::BusSlot& bus = m_Data->Buses[index];
		if (!bus.Group || bus.Generation != GenerationOf(packed))
			return std::nullopt;

		return index;
	}

	bool MiniAudioEngine::IsUnderBus(std::uint32_t index, std::uint32_t ancestor) const
	{
		if (ancestor == k_MasterBusIndex)
			return true;

		while (index != k_MasterBusIndex)
		{
			if (index == ancestor)
				return true;
			index = m_Data->Buses[index].Parent;
		}
		return false;
	}

	void MiniAudioEngine::StopSoundsUnder(std::uint32_t busIndex)
	{
		for (Internal::SoundSlot& slot : m_Data->Slots)
		{
			if (slot.Sound && IsUnderBus(slot.Bus, busIndex))
				ReleaseSlot(slot);
		}
	}

	void MiniAudioEngine::ApplyBusVolume(std::uint32_t index)
	{
		// The master bus's level is the engine volume (SetMasterVolume); its group only mutes.
		const Internal::BusSlot& bus = m_Data->Buses[index];
		const float level = index == k_MasterBusIndex ? 1.0f : bus.Volume;
		ma_sound_group_set_volume(bus.Group, bus.Muted ? 0.0f : level);
	}

	AudioBusId MiniAudioEngine::CreateBus(std::string_view name, AudioBusId parent)
	{
		if (!m_Data)
			return k_InvalidBus;

		if (name.empty())
		{
			DE_CORE_ERROR("AudioEngine::CreateBus: a bus needs a name");
			return k_InvalidBus;
		}

		const std::optional<std::uint32_t> parentIndex = ResolveBus(parent);
		if (!parentIndex)
		{
			DE_CORE_ERROR("AudioEngine::CreateBus: the parent of bus '{}' is destroyed or invalid", name);
			return k_InvalidBus;
		}

		const AudioBusId existing = FindBus(name);
		if (existing != k_InvalidBus)
		{
			DE_CORE_WARN("AudioEngine::CreateBus: bus '{}' already exists; returning it", name);
			return existing;
		}

		auto* group = new ma_sound();
		const ma_result result = ma_sound_group_init(m_Data->Engine, MA_SOUND_FLAG_NO_PITCH, m_Data->Buses[*parentIndex].Group, group);
		if (result != MA_SUCCESS)
		{
			DE_CORE_ERROR("AudioEngine::CreateBus: ma_sound_group_init failed for '{}' ({})", name, (int)result);
			delete group;
			return k_InvalidBus;
		}

		std::uint32_t index = 1;
		while (index < m_Data->Buses.size() && m_Data->Buses[index].Group)
			++index;
		if (index == m_Data->Buses.size())
			m_Data->Buses.emplace_back();

		Internal::BusSlot& bus = m_Data->Buses[index];
		bus.Group = group;
		bus.Parent = *parentIndex;
		bus.Name = name;
		bus.Volume = 1.0f;
		bus.Muted = false;
		bus.Paused = false;
		bus.Serial = m_Data->NextBusSerial++;
		return MakeBusId(index, bus.Generation);
	}

	void MiniAudioEngine::DestroyBus(AudioBusId id)
	{
		const std::optional<std::uint32_t> index = ResolveBus(id);
		if (!index)
			return;

		if (*index == k_MasterBusIndex)
		{
			DE_CORE_WARN("AudioEngine::DestroyBus: the master bus can't be destroyed");
			return;
		}

		StopSoundsUnder(*index);

		std::vector<std::uint32_t> doomed;
		for (std::uint32_t i = 1; i < m_Data->Buses.size(); ++i)
		{
			if (m_Data->Buses[i].Group && IsUnderBus(i, *index))
				doomed.push_back(i);
		}
		std::sort(doomed.begin(), doomed.end(), [this](std::uint32_t a, std::uint32_t b)
		{
			return m_Data->Buses[a].Serial > m_Data->Buses[b].Serial;
		});

		// Parents are read through IsUnderBus above, so nothing is freed until every
		// sub-bus is known.
		for (std::uint32_t i : doomed)
		{
			Internal::BusSlot& bus = m_Data->Buses[i];
			ma_sound_group_uninit(bus.Group);
			delete bus.Group;
			bus.Group = nullptr;
			bus.Name.clear();
			++bus.Generation;
		}
	}

	AudioBusId MiniAudioEngine::FindBus(std::string_view name) const
	{
		if (!m_Data || name.empty())
			return k_InvalidBus;

		for (std::uint32_t i = 0; i < m_Data->Buses.size(); ++i)
		{
			const Internal::BusSlot& bus = m_Data->Buses[i];
			if (bus.Group && bus.Name == name)
				return MakeBusId(i, bus.Generation);
		}
		return k_InvalidBus;
	}

	bool MiniAudioEngine::IsBusValid(AudioBusId bus) const
	{
		return ResolveBus(bus).has_value();
	}

	std::uint32_t MiniAudioEngine::GetBusCount() const
	{
		if (!m_Data)
			return 0;

		std::uint32_t count = 0;
		for (std::uint32_t i = 1; i < m_Data->Buses.size(); ++i)
		{
			if (m_Data->Buses[i].Group)
				++count;
		}
		return count;
	}

	void MiniAudioEngine::SetBusVolume(AudioBusId id, float volume)
	{
		const std::optional<std::uint32_t> index = ResolveBus(id);
		if (!index)
			return;

		if (*index == k_MasterBusIndex)
		{
			SetMasterVolume(volume);
			return;
		}

		m_Data->Buses[*index].Volume = volume;
		ApplyBusVolume(*index);
	}

	float MiniAudioEngine::GetBusVolume(AudioBusId id) const
	{
		const std::optional<std::uint32_t> index = ResolveBus(id);
		if (!index)
			return 0.0f;
		if (*index == k_MasterBusIndex)
			return GetMasterVolume();
		return m_Data->Buses[*index].Volume;
	}

	void MiniAudioEngine::SetBusMuted(AudioBusId id, bool muted)
	{
		const std::optional<std::uint32_t> index = ResolveBus(id);
		if (!index)
			return;

		m_Data->Buses[*index].Muted = muted;
		ApplyBusVolume(*index);
	}

	bool MiniAudioEngine::IsBusMuted(AudioBusId id) const
	{
		const std::optional<std::uint32_t> index = ResolveBus(id);
		return index ? m_Data->Buses[*index].Muted : false;
	}

	void MiniAudioEngine::PauseBus(AudioBusId id)
	{
		const std::optional<std::uint32_t> index = ResolveBus(id);
		if (!index)
			return;

		Internal::BusSlot& bus = m_Data->Buses[*index];
		bus.Paused = true;
		ma_sound_group_stop(bus.Group);
	}

	void MiniAudioEngine::ResumeBus(AudioBusId id)
	{
		const std::optional<std::uint32_t> index = ResolveBus(id);
		if (!index)
			return;

		Internal::BusSlot& bus = m_Data->Buses[*index];
		bus.Paused = false;
		ma_sound_group_start(bus.Group);
	}

	bool MiniAudioEngine::IsBusPaused(AudioBusId id) const
	{
		const std::optional<std::uint32_t> index = ResolveBus(id);
		return index ? m_Data->Buses[*index].Paused : false;
	}

	void MiniAudioEngine::StopBus(AudioBusId id)
	{
		if (const std::optional<std::uint32_t> index = ResolveBus(id))
			StopSoundsUnder(*index);
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

	void MiniAudioEngine::SetAttenuation(AudioSoundId sound, const SoundAttenuation& attenuation)
	{
		if (Internal::SoundSlot* slot = ResolveSlot(sound))
			ApplyAttenuation(slot->Sound, attenuation);
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

	void MiniAudioEngine::SetDefaultAttenuation(const SoundAttenuation& attenuation)
	{
		m_DefaultAttenuation = attenuation;
	}

	const SoundAttenuation& MiniAudioEngine::GetDefaultAttenuation() const
	{
		return m_DefaultAttenuation;
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
