#include "depch.h"
#include "DingoEngine/Audio/MiniAudio/MiniAudioEngine.h"
#include "DingoEngine/Core/PathText.h"
#include "DingoEngine/Asset/AssetPath.h"
#include "DingoEngine/Audio/MiniAudio/MiniAudioVfs.h"
#include "DingoEngine/Core/VirtualFileSystem.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>

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

namespace Dingo::Internal
{

	// A data source over a lock-free single-producer, single-consumer ring: the game thread
	// writes, the device thread reads. Reads always return every frame asked for (miniaudio turns
	// an empty or short read into MA_AT_END, which ends the sound), so a dry buffer holds
	// the last frame and fades it out, and new data fades back in, which keeps an underrun from
	// clicking. Once silent, a stream resumes only when a whole read is queued: ma_sound reads
	// a period at a time, and resuming on less would alternate scraps of data with silence.
	struct MiniAudioStream
	{
		ma_data_source_base Base{}; // first, so the stream is its own ma_data_source*
		ma_pcm_rb Ring{};
		std::uint32_t Channels = 1;
		std::uint32_t SampleRate = 48000;
		std::uint32_t BufferFrames = 0;
		float FadeStep = 1.0f;
		float Envelope = 0.0f;
		std::vector<float> Held;
	};

}

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
			return AudioSoundId{ (static_cast<std::uint32_t>(generation) << k_IndexBits) | (index & k_IndexMask) };
		}

		std::uint32_t IndexOf(std::uint32_t packed) { return packed & k_IndexMask; }
		std::uint16_t GenerationOf(std::uint32_t packed) { return static_cast<std::uint16_t>(packed >> k_IndexBits); }
		std::uint32_t IndexOf(AudioSoundId id) { return IndexOf(static_cast<std::uint32_t>(id)); }
		std::uint16_t GenerationOf(AudioSoundId id) { return GenerationOf(static_cast<std::uint32_t>(id)); }

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

		constexpr std::uint32_t k_MinStreamSampleRate = 8000;
		constexpr std::uint32_t k_MaxStreamSampleRate = 384000;
		constexpr float k_StreamFadeSeconds = 0.005f;

		ma_result StreamRead(ma_data_source* source, void* framesOut, ma_uint64 frameCount, ma_uint64* framesRead)
		{
			auto* stream = static_cast<Internal::MiniAudioStream*>(source);
			const std::uint32_t channels = stream->Channels;
			float* out = static_cast<float*>(framesOut);

			const ma_uint64 resumeFrames = (std::min)(frameCount, static_cast<ma_uint64>(stream->BufferFrames));
			const bool waiting = stream->Envelope == 0.0f && ma_pcm_rb_available_read(&stream->Ring) < resumeFrames;

			ma_uint64 done = 0;
			while (!waiting && done < frameCount)
			{
				ma_uint32 count = static_cast<ma_uint32>((std::min)(frameCount - done, static_cast<ma_uint64>(0xFFFFFFFFu)));
				void* buffer = nullptr;
				if (ma_pcm_rb_acquire_read(&stream->Ring, &count, &buffer) != MA_SUCCESS || count == 0)
					break;

				const float* in = static_cast<const float*>(buffer);
				for (ma_uint32 frame = 0; frame < count; ++frame)
				{
					stream->Envelope = (std::min)(stream->Envelope + stream->FadeStep, 1.0f);
					for (std::uint32_t channel = 0; channel < channels; ++channel)
					{
						const float sample = in[frame * channels + channel];
						stream->Held[channel] = sample;
						out[(done + frame) * channels + channel] = sample * stream->Envelope;
					}
				}

				ma_pcm_rb_commit_read(&stream->Ring, count);
				done += count;
			}

			for (; done < frameCount; ++done)
			{
				stream->Envelope = (std::max)(stream->Envelope - stream->FadeStep, 0.0f);
				for (std::uint32_t channel = 0; channel < channels; ++channel)
					out[done * channels + channel] = stream->Held[channel] * stream->Envelope;
			}

			*framesRead = frameCount;
			return MA_SUCCESS;
		}

		ma_result StreamGetDataFormat(ma_data_source* source, ma_format* format, ma_uint32* channels, ma_uint32* sampleRate, ma_channel* channelMap, size_t channelMapCap)
		{
			const auto* stream = static_cast<const Internal::MiniAudioStream*>(source);
			*format = ma_format_f32;
			*channels = stream->Channels;
			*sampleRate = stream->SampleRate;
			ma_channel_map_init_standard(ma_standard_channel_map_default, channelMap, channelMapCap, stream->Channels);
			return MA_SUCCESS;
		}

		const ma_data_source_vtable k_StreamVTable = { StreamRead, nullptr, StreamGetDataFormat, nullptr, nullptr, nullptr, 0 };

		void DestroyStream(Internal::MiniAudioStream* stream)
		{
			ma_pcm_rb_uninit(&stream->Ring);
			ma_data_source_uninit(&stream->Base);
			delete stream;
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

			uint64_t GetFrameCount() const override
			{
				ma_uint64 frames = 0;
				if (m_Loaded)
					ma_sound_get_length_in_pcm_frames(const_cast<ma_sound*>(&m_Template), &frames);
				return frames;
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

		// Defaults apart from the VFS: default playback device, embedded resource manager (needed
		// for ma_sound_init_copy), one listener.
		m_Data->Vfs = Internal::CreateMiniAudioVfs();
		ma_engine_config config = ma_engine_config_init();
		config.pResourceManagerVFS = m_Data->Vfs;
		const ma_result result = ma_engine_init(&config, m_Data->Engine);
		if (result != MA_SUCCESS)
		{
			DE_CORE_ERROR("MiniAudioEngine: ma_engine_init failed ({})", (int)result);
			delete m_Data->Engine;
			Internal::DestroyMiniAudioVfs(m_Data->Vfs);
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
			Internal::DestroyMiniAudioVfs(m_Data->Vfs);
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

		StopCapture();

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
		Internal::DestroyMiniAudioVfs(m_Data->Vfs);
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

		PollCaptureDevice();

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

		if (!VirtualFileSystem::Exists(resolvedPath))
		{
			DE_CORE_ERROR("AudioEngine::LoadClip: file not found '{}'", Internal::PathToUtf8(resolvedPath));
			return nullptr;
		}

		auto clip = std::make_shared<MiniAudioClip>();

		// MA_SOUND_FLAG_DECODE: fully decode into memory now (so ma_sound_init_copy can
		// clone the decoded data buffer). No STREAM flag — streams can't be copied.
		const ma_result result = ma_sound_init_from_file(m_Data->Engine, Internal::ToMiniAudioPath(resolvedPath).c_str(),
			MA_SOUND_FLAG_DECODE, nullptr, nullptr, clip->Template());
		if (result != MA_SUCCESS)
		{
			DE_CORE_ERROR("AudioEngine::LoadClip failed for '{}' ({})", Internal::PathToUtf8(resolvedPath), (int)result);
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

	static std::uint32_t BusOrMaster(Internal::MiniAudioData& data, std::optional<std::uint32_t> bus)
	{
		if (bus)
			return *bus;

		if (!data.StaleBusWarned)
		{
			DE_CORE_WARN("AudioEngine: a sound was played on a destroyed or invalid bus; it plays on the master bus");
			data.StaleBusWarned = true;
		}
		return k_MasterBusIndex;
	}

	// Applies params to an initialised sound, gives it a slot and starts it. The returned id
	// stays valid until the sound is Stop()ped or, for a non-looping clip, finishes and is
	// reaped by Update() (which bumps the slot generation so the id then goes stale).
	static AudioSoundId StartSound(Internal::MiniAudioData& data, ma_sound* sound, const SoundPlayParams& params,
		const SoundAttenuation& defaultAttenuation, std::uint32_t bus, const std::shared_ptr<AudioClip>& clip, Internal::MiniAudioStream* stream)
	{
		ma_sound_set_volume(sound, params.Volume);
		ma_sound_set_pitch(sound, params.Pitch);
		if (!stream)
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
		slot.Stream = stream;
		slot.Bus = bus;

		ma_sound_start(sound);
		return MakeId(index, slot.Generation);
	}

	// Shared setup for Play / PlayOneShot: clones the clip into a fresh slot and starts it.
	// Returns k_InvalidSound on any failure.
	static AudioSoundId StartInstance(Internal::MiniAudioData& data,
		const std::shared_ptr<AudioClip>& clip, const SoundPlayParams& params,
		const SoundAttenuation& defaultAttenuation, std::optional<std::uint32_t> bus)
	{
		if (!clip)
			return k_InvalidSound;

		auto* concrete = static_cast<MiniAudioClip*>(clip.get());
		if (!concrete->m_Loaded)
			return k_InvalidSound;

		const std::uint32_t busIndex = BusOrMaster(data, bus);

		auto* sound = new ma_sound();
		const ma_result result = ma_sound_init_copy(data.Engine, concrete->Template(), 0, data.Buses[busIndex].Group, sound);
		if (result != MA_SUCCESS)
		{
			DE_CORE_ERROR("AudioEngine: failed to instantiate sound ({})", (int)result);
			delete sound;
			return k_InvalidSound;
		}

		return StartSound(data, sound, params, defaultAttenuation, busIndex, clip, nullptr);
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

	AudioSoundId MiniAudioEngine::CreateStream(const AudioStreamDesc& desc, const SoundPlayParams& params)
	{
		if (!m_Data)
			return k_InvalidSound;

		if (desc.BufferFrames == 0 || desc.Channels == 0 || desc.Channels > MA_MAX_CHANNELS
			|| desc.BufferFrames > 0x7FFFFFFFu / (sizeof(float) * desc.Channels)
			|| desc.SampleRate < k_MinStreamSampleRate || desc.SampleRate > k_MaxStreamSampleRate)
		{
			DE_CORE_ERROR("AudioEngine::CreateStream: invalid stream ({} Hz, {} channels, {} frames)", desc.SampleRate, desc.Channels, desc.BufferFrames);
			return k_InvalidSound;
		}

		auto* stream = new Internal::MiniAudioStream();
		stream->Channels = desc.Channels;
		stream->SampleRate = desc.SampleRate;
		stream->BufferFrames = desc.BufferFrames;
		stream->FadeStep = 1.0f / (std::max)(desc.SampleRate * k_StreamFadeSeconds, 1.0f);
		stream->Held.assign(desc.Channels, 0.0f);

		const ma_result ringResult = ma_pcm_rb_init(ma_format_f32, desc.Channels, desc.BufferFrames, nullptr, nullptr, &stream->Ring);
		if (ringResult != MA_SUCCESS)
		{
			DE_CORE_ERROR("AudioEngine::CreateStream: ring buffer of {} frames failed ({})", desc.BufferFrames, (int)ringResult);
			delete stream;
			return k_InvalidSound;
		}

		ma_data_source_config sourceConfig = ma_data_source_config_init();
		sourceConfig.vtable = &k_StreamVTable;
		const ma_result sourceResult = ma_data_source_init(&sourceConfig, &stream->Base);
		if (sourceResult != MA_SUCCESS)
		{
			DE_CORE_ERROR("AudioEngine::CreateStream: ma_data_source_init failed ({})", (int)sourceResult);
			ma_pcm_rb_uninit(&stream->Ring);
			delete stream;
			return k_InvalidSound;
		}

		const std::uint32_t busIndex = BusOrMaster(*m_Data, ResolveBus(params.Bus));

		auto* sound = new ma_sound();
		const ma_result result = ma_sound_init_from_data_source(m_Data->Engine, &stream->Base, 0, m_Data->Buses[busIndex].Group, sound);
		if (result != MA_SUCCESS)
		{
			DE_CORE_ERROR("AudioEngine::CreateStream: failed to instantiate sound ({})", (int)result);
			delete sound;
			DestroyStream(stream);
			return k_InvalidSound;
		}

		return StartSound(*m_Data, sound, params, m_DefaultAttenuation, busIndex, nullptr, stream);
	}

	Internal::MiniAudioStream* MiniAudioEngine::ResolveStream(AudioSoundId id) const
	{
		Internal::SoundSlot* slot = ResolveSlot(id);
		return slot ? slot->Stream : nullptr;
	}

	std::uint32_t MiniAudioEngine::PushStreamFrames(AudioSoundId id, const std::int16_t* frames, std::uint32_t frameCount)
	{
		Internal::MiniAudioStream* stream = ResolveStream(id);
		if (!stream || !frames)
			return 0;

		const std::uint32_t channels = stream->Channels;
		std::uint32_t pushed = 0;
		while (pushed < frameCount)
		{
			ma_uint32 count = frameCount - pushed;
			void* buffer = nullptr;
			if (ma_pcm_rb_acquire_write(&stream->Ring, &count, &buffer) != MA_SUCCESS || count == 0)
				break;

			float* out = static_cast<float*>(buffer);
			const std::int16_t* in = frames + static_cast<std::size_t>(pushed) * channels;
			for (std::size_t i = 0; i < static_cast<std::size_t>(count) * channels; ++i)
				out[i] = static_cast<float>(in[i]) * (1.0f / 32768.0f);

			ma_pcm_rb_commit_write(&stream->Ring, count);
			pushed += count;
		}
		return pushed;
	}

	std::uint32_t MiniAudioEngine::PushStreamFrames(AudioSoundId id, const float* frames, std::uint32_t frameCount)
	{
		Internal::MiniAudioStream* stream = ResolveStream(id);
		if (!stream || !frames)
			return 0;

		const std::uint32_t channels = stream->Channels;
		std::uint32_t pushed = 0;
		while (pushed < frameCount)
		{
			ma_uint32 count = frameCount - pushed;
			void* buffer = nullptr;
			if (ma_pcm_rb_acquire_write(&stream->Ring, &count, &buffer) != MA_SUCCESS || count == 0)
				break;

			std::memcpy(buffer, frames + static_cast<std::size_t>(pushed) * channels, static_cast<std::size_t>(count) * channels * sizeof(float));
			ma_pcm_rb_commit_write(&stream->Ring, count);
			pushed += count;
		}
		return pushed;
	}

	std::uint32_t MiniAudioEngine::GetQueuedStreamFrames(AudioSoundId id) const
	{
		Internal::MiniAudioStream* stream = ResolveStream(id);
		return stream ? ma_pcm_rb_available_read(&stream->Ring) : 0;
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
		if (slot.Stream)
		{
			DestroyStream(slot.Stream);
			slot.Stream = nullptr;
		}
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
		Internal::SoundSlot* slot = ResolveSlot(sound);
		if (slot && !slot->Stream)
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

	namespace
	{

		constexpr float k_MaxCaptureGain = 8.0f;

		float DbToLinear(float db)
		{
			return std::pow(10.0f, db / 20.0f);
		}

		std::string EncodeDeviceId(const ma_device_id& id)
		{
			const auto* bytes = reinterpret_cast<const unsigned char*>(&id);
			std::size_t length = sizeof(ma_device_id);
			while (length > 0 && bytes[length - 1] == 0)
				--length;
			if (length == 0)
				length = 1;

			constexpr char k_Hex[] = "0123456789abcdef";
			std::string text;
			text.reserve(length * 2);
			for (std::size_t i = 0; i < length; ++i)
			{
				text.push_back(k_Hex[bytes[i] >> 4]);
				text.push_back(k_Hex[bytes[i] & 0xF]);
			}
			return text;
		}

		bool DecodeDeviceId(const std::string& text, ma_device_id& id)
		{
			std::memset(&id, 0, sizeof(id));
			if (text.size() % 2 != 0 || text.size() / 2 > sizeof(ma_device_id))
				return false;

			auto nibble = [](char c) -> int
			{
				if (c >= '0' && c <= '9') return c - '0';
				if (c >= 'a' && c <= 'f') return c - 'a' + 10;
				if (c >= 'A' && c <= 'F') return c - 'A' + 10;
				return -1;
			};
			auto* bytes = reinterpret_cast<unsigned char*>(&id);
			for (std::size_t i = 0; i < text.size() / 2; ++i)
			{
				const int high = nibble(text[i * 2]);
				const int low = nibble(text[i * 2 + 1]);
				if (high < 0 || low < 0)
					return false;
				bytes[i] = static_cast<unsigned char>((high << 4) | low);
			}
			return true;
		}

	}

}

namespace Dingo::Internal
{

	// The device thread is the ring's only writer and the game thread its only reader. Settings
	// the game changes live are atomics the callback reads once a period.
	struct MiniAudioCapture
	{
		ma_device* Device = nullptr;
		ma_pcm_rb Ring{};
		std::uint32_t Channels = 1;
		std::uint32_t SampleRate = 48000;
		std::string DeviceId;
		AudioCaptureState State = AudioCaptureState::Stopped;
		bool RetriedDefault = false;

		std::atomic<float> Gain{ 1.0f };
		std::atomic<bool> GateEnabled{ false };
		std::atomic<float> GateThreshold{ 0.0f };
		std::atomic<float> GateAttackMs{ 5.0f };
		std::atomic<float> GateReleaseMs{ 150.0f };
		float GateGain = 1.0f;

		std::atomic<float> Level{ 0.0f };
		std::atomic<bool> GateOpen{ true };
		std::atomic<std::uint64_t> Overruns{ 0 };
		bool LostForTest = false;
	};

	namespace
	{

		void ProcessCapture(MiniAudioCapture& capture, const float* input, std::uint32_t frameCount)
		{
			const std::uint32_t channels = capture.Channels;
			const float gain = capture.Gain.load(std::memory_order_relaxed);

			float inputPeak = 0.0f;
			if (input)
			{
				for (std::size_t i = 0; i < static_cast<std::size_t>(frameCount) * channels; ++i)
					inputPeak = (std::max)(inputPeak, std::fabs(input[i] * gain));
			}
			inputPeak = (std::min)(inputPeak, 1.0f);

			const bool gateEnabled = capture.GateEnabled.load(std::memory_order_relaxed);
			const bool open = !gateEnabled || inputPeak >= capture.GateThreshold.load(std::memory_order_relaxed);
			float gateStep = 1.0f;
			if (gateEnabled)
			{
				const float ms = open ? capture.GateAttackMs.load(std::memory_order_relaxed) : capture.GateReleaseMs.load(std::memory_order_relaxed);
				const float frames = ms * 0.001f * static_cast<float>(capture.SampleRate);
				gateStep = frames > 1.0f ? 1.0f / frames : 1.0f;
			}
			const float gateTarget = open ? 1.0f : 0.0f;

			float outputPeak = 0.0f;
			std::uint32_t done = 0;
			while (done < frameCount)
			{
				ma_uint32 count = frameCount - done;
				void* buffer = nullptr;
				const bool writable = ma_pcm_rb_acquire_write(&capture.Ring, &count, &buffer) == MA_SUCCESS && count > 0;
				if (!writable)
				{
					count = frameCount - done;
					buffer = nullptr;
				}

				float* out = static_cast<float*>(buffer);
				for (std::uint32_t frame = 0; frame < count; ++frame)
				{
					if (capture.GateGain < gateTarget)
						capture.GateGain = (std::min)(capture.GateGain + gateStep, gateTarget);
					else if (capture.GateGain > gateTarget)
						capture.GateGain = (std::max)(capture.GateGain - gateStep, gateTarget);

					for (std::uint32_t c = 0; c < channels; ++c)
					{
						const std::size_t index = static_cast<std::size_t>(done + frame) * channels + c;
						const float sample = input ? std::clamp(input[index] * gain, -1.0f, 1.0f) * capture.GateGain : 0.0f;
						outputPeak = (std::max)(outputPeak, std::fabs(sample));
						if (out)
							out[static_cast<std::size_t>(frame) * channels + c] = sample;
					}
				}

				if (writable)
					ma_pcm_rb_commit_write(&capture.Ring, count);
				else
					capture.Overruns.fetch_add(count, std::memory_order_relaxed);
				done += count;
			}

			capture.Level.store(outputPeak, std::memory_order_relaxed);
			capture.GateOpen.store(open, std::memory_order_relaxed);
		}

		void CaptureCallback(ma_device* device, void*, const void* input, ma_uint32 frameCount)
		{
			ProcessCapture(*static_cast<MiniAudioCapture*>(device->pUserData), static_cast<const float*>(input), frameCount);
		}

		ma_context* CaptureContext(MiniAudioData& data)
		{
			ma_device* playback = ma_engine_get_device(data.Engine);
			return playback ? playback->pContext : nullptr;
		}

		// A device opened but not started, or nullptr with the reason logged.
		ma_device* OpenCaptureDevice(MiniAudioData& data, MiniAudioCapture& capture, const std::string& deviceId, const char* caller)
		{
			ma_context* context = CaptureContext(data);
			if (!context)
			{
				DE_CORE_ERROR("AudioEngine::{}: the engine has no audio context", caller);
				return nullptr;
			}

			ma_device_id id{};
			if (!deviceId.empty() && !DecodeDeviceId(deviceId, id))
			{
				DE_CORE_ERROR("AudioEngine::{}: '{}' is not a capture device id from EnumerateCaptureDevices", caller, deviceId);
				return nullptr;
			}

			ma_device_config config = ma_device_config_init(ma_device_type_capture);
			config.capture.pDeviceID = deviceId.empty() ? nullptr : &id;
			config.capture.format = ma_format_f32;
			config.capture.channels = capture.Channels;
			config.sampleRate = capture.SampleRate;
			config.dataCallback = CaptureCallback;
			config.pUserData = &capture;

			auto* device = new ma_device();
			const ma_result result = ma_device_init(context, &config, device);
			if (result != MA_SUCCESS)
			{
				DE_CORE_ERROR("AudioEngine::{}: can't open the capture device{} ({}: {}); no microphone, or access to it is denied",
					caller, deviceId.empty() ? " (system default)" : "", (int)result, ma_result_description(result));
				delete device;
				return nullptr;
			}
			return device;
		}

		void CloseCaptureDevice(MiniAudioCapture& capture, ma_device*& device)
		{
			if (!device)
				return;
			ma_device_uninit(device);
			delete device;
			device = nullptr;
		}

		bool StartCaptureDevice(MiniAudioCapture& capture, ma_device* device, const char* caller)
		{
			capture.LostForTest = false;
			const ma_result result = ma_device_start(device);
			if (result != MA_SUCCESS)
			{
				DE_CORE_ERROR("AudioEngine::{}: can't start the capture device ({}: {})", caller, (int)result, ma_result_description(result));
				return false;
			}
			return true;
		}

	}

}

namespace Dingo
{

	std::vector<AudioCaptureDevice> MiniAudioEngine::EnumerateCaptureDevices() const
	{
		std::vector<AudioCaptureDevice> devices;
		ma_context* context = m_Data ? Internal::CaptureContext(*m_Data) : nullptr;
		if (!context)
			return devices;

		ma_device_info* playback = nullptr;
		ma_device_info* capture = nullptr;
		ma_uint32 playbackCount = 0;
		ma_uint32 captureCount = 0;
		const ma_result result = ma_context_get_devices(context, &playback, &playbackCount, &capture, &captureCount);
		if (result != MA_SUCCESS)
		{
			DE_CORE_WARN("AudioEngine::EnumerateCaptureDevices: the OS didn't list its devices ({})", (int)result);
			return devices;
		}

		for (ma_uint32 i = 0; i < captureCount; ++i)
		{
			AudioCaptureDevice& device = devices.emplace_back();
			device.Id = EncodeDeviceId(capture[i].id);
			device.Name = capture[i].name;
			device.IsDefault = capture[i].isDefault != MA_FALSE;
		}
		std::stable_partition(devices.begin(), devices.end(), [](const AudioCaptureDevice& device) { return device.IsDefault; });
		return devices;
	}

	bool MiniAudioEngine::StartCapture(const AudioCaptureDesc& desc)
	{
		if (!m_Data)
		{
			DE_CORE_ERROR("AudioEngine::StartCapture: the engine has no audio device");
			return false;
		}
		if (desc.Channels < 1 || desc.Channels > 2 || desc.BufferFrames == 0
			|| desc.BufferFrames > 0x7FFFFFFFu / (sizeof(float) * desc.Channels)
			|| desc.SampleRate < k_MinStreamSampleRate || desc.SampleRate > k_MaxStreamSampleRate)
		{
			DE_CORE_ERROR("AudioEngine::StartCapture: invalid capture ({} Hz, {} channels, {} frames)", desc.SampleRate, desc.Channels, desc.BufferFrames);
			return false;
		}

		StopCapture();

		auto* capture = new Internal::MiniAudioCapture();
		capture->Channels = desc.Channels;
		capture->SampleRate = desc.SampleRate;
		capture->DeviceId = desc.DeviceId;
		const ma_result ringResult = ma_pcm_rb_init(ma_format_f32, desc.Channels, desc.BufferFrames, nullptr, nullptr, &capture->Ring);
		if (ringResult != MA_SUCCESS)
		{
			DE_CORE_ERROR("AudioEngine::StartCapture: ring buffer of {} frames failed ({})", desc.BufferFrames, (int)ringResult);
			delete capture;
			return false;
		}

		m_Data->Capture = capture;
		SetCaptureGain(desc.Gain);
		SetCaptureNoiseGate(desc.NoiseGate, desc.GateThresholdDb, desc.GateAttackMs, desc.GateReleaseMs);
		capture->GateGain = desc.NoiseGate ? 0.0f : 1.0f;
		capture->GateOpen.store(!desc.NoiseGate);

		capture->Device = Internal::OpenCaptureDevice(*m_Data, *capture, desc.DeviceId, "StartCapture");
		if (!capture->Device || !Internal::StartCaptureDevice(*capture, capture->Device, "StartCapture"))
		{
			StopCapture();
			return false;
		}
		capture->State = AudioCaptureState::Running;
		return true;
	}

	void MiniAudioEngine::StopCapture()
	{
		if (!m_Data || !m_Data->Capture)
			return;
		Internal::MiniAudioCapture* capture = m_Data->Capture;
		Internal::CloseCaptureDevice(*capture, capture->Device);
		ma_pcm_rb_uninit(&capture->Ring);
		delete capture;
		m_Data->Capture = nullptr;
	}

	AudioCaptureState MiniAudioEngine::GetCaptureState() const
	{
		return m_Data && m_Data->Capture ? m_Data->Capture->State : AudioCaptureState::Stopped;
	}

	std::string MiniAudioEngine::GetCaptureDeviceId() const
	{
		return m_Data && m_Data->Capture ? m_Data->Capture->DeviceId : std::string();
	}

	Internal::MiniAudioCapture* MiniAudioEngine::RunningCapture() const
	{
		if (!m_Data || !m_Data->Capture || m_Data->Capture->State != AudioCaptureState::Running)
			return nullptr;
		return m_Data->Capture;
	}

	std::uint32_t MiniAudioEngine::ReadCaptured(std::int16_t* frames, std::uint32_t maxFrames)
	{
		Internal::MiniAudioCapture* capture = RunningCapture();
		if (!capture || !frames)
			return 0;

		const std::uint32_t channels = capture->Channels;
		std::uint32_t read = 0;
		while (read < maxFrames)
		{
			ma_uint32 count = maxFrames - read;
			void* buffer = nullptr;
			if (ma_pcm_rb_acquire_read(&capture->Ring, &count, &buffer) != MA_SUCCESS || count == 0)
				break;

			const float* in = static_cast<const float*>(buffer);
			std::int16_t* out = frames + static_cast<std::size_t>(read) * channels;
			for (std::size_t i = 0; i < static_cast<std::size_t>(count) * channels; ++i)
				out[i] = static_cast<std::int16_t>(std::lrint(std::clamp(in[i], -1.0f, 1.0f) * 32767.0f));

			ma_pcm_rb_commit_read(&capture->Ring, count);
			read += count;
		}
		return read;
	}

	std::uint32_t MiniAudioEngine::ReadCaptured(float* frames, std::uint32_t maxFrames)
	{
		Internal::MiniAudioCapture* capture = RunningCapture();
		if (!capture || !frames)
			return 0;

		const std::uint32_t channels = capture->Channels;
		std::uint32_t read = 0;
		while (read < maxFrames)
		{
			ma_uint32 count = maxFrames - read;
			void* buffer = nullptr;
			if (ma_pcm_rb_acquire_read(&capture->Ring, &count, &buffer) != MA_SUCCESS || count == 0)
				break;

			std::memcpy(frames + static_cast<std::size_t>(read) * channels, buffer, static_cast<std::size_t>(count) * channels * sizeof(float));
			ma_pcm_rb_commit_read(&capture->Ring, count);
			read += count;
		}
		return read;
	}

	std::uint32_t MiniAudioEngine::GetCapturedFrames() const
	{
		Internal::MiniAudioCapture* capture = RunningCapture();
		return capture ? ma_pcm_rb_available_read(&capture->Ring) : 0;
	}

	std::uint64_t MiniAudioEngine::GetCaptureOverruns() const
	{
		return m_Data && m_Data->Capture ? m_Data->Capture->Overruns.load() : 0;
	}

	bool MiniAudioEngine::SetCaptureDevice(const std::string& deviceId)
	{
		if (!m_Data || !m_Data->Capture)
		{
			DE_CORE_ERROR("AudioEngine::SetCaptureDevice: not capturing; call StartCapture first");
			return false;
		}
		return SwitchCaptureDevice(deviceId, "SetCaptureDevice");
	}

	bool MiniAudioEngine::SwitchCaptureDevice(const std::string& deviceId, const char* caller)
	{
		Internal::MiniAudioCapture& capture = *m_Data->Capture;
		if (capture.Device && capture.State == AudioCaptureState::Running && deviceId == capture.DeviceId)
			return true;
		ma_device* device = Internal::OpenCaptureDevice(*m_Data, capture, deviceId, caller);
		if (!device)
			return false;

		// One writer at a time: the old device stops before the new one starts.
		if (capture.Device)
			ma_device_stop(capture.Device);
		if (!Internal::StartCaptureDevice(capture, device, caller))
		{
			Internal::CloseCaptureDevice(capture, device);
			if (capture.Device && capture.State == AudioCaptureState::Running && ma_device_start(capture.Device) != MA_SUCCESS)
				capture.State = AudioCaptureState::Lost;
			return false;
		}

		Internal::CloseCaptureDevice(capture, capture.Device);
		capture.Device = device;
		capture.DeviceId = deviceId;
		capture.State = AudioCaptureState::Running;
		capture.RetriedDefault = false;
		return true;
	}

	void MiniAudioEngine::PollCaptureDevice()
	{
		if (!m_Data || !m_Data->Capture)
			return;
		Internal::MiniAudioCapture& capture = *m_Data->Capture;
		// Polled rather than notified: miniaudio skips the stopped notification when an unplugged
		// device's stop fails (WASAPI), and Pulse notifies a suspend that leaves the device started.
		const bool started = capture.Device ? ma_device_is_started(capture.Device) != MA_FALSE : !capture.LostForTest;
		if (capture.State == AudioCaptureState::Lost && started && capture.Device)
		{
			capture.State = AudioCaptureState::Running;
			DE_CORE_INFO("AudioEngine: the capture device came back");
			return;
		}
		if (capture.State != AudioCaptureState::Running || started)
			return;

		capture.State = AudioCaptureState::Lost;
		DE_CORE_WARN("AudioEngine: the capture device stopped (unplugged?); trying the system default");
		if (capture.RetriedDefault)
			return;
		capture.RetriedDefault = true;
		if (SwitchCaptureDevice(std::string(), "capture recovery"))
			DE_CORE_INFO("AudioEngine: capture continues on the system default device");
		else
			DE_CORE_WARN("AudioEngine: no capture device to fall back on; capture stays lost until SetCaptureDevice or StartCapture");
	}

	void MiniAudioEngine::SetCaptureGain(float gain)
	{
		if (m_Data && m_Data->Capture)
			m_Data->Capture->Gain.store(std::isfinite(gain) ? std::clamp(gain, 0.0f, k_MaxCaptureGain) : 1.0f);
	}

	void MiniAudioEngine::SetCaptureNoiseGate(bool enabled, float thresholdDb, float attackMs, float releaseMs)
	{
		if (!m_Data || !m_Data->Capture)
			return;
		Internal::MiniAudioCapture& capture = *m_Data->Capture;
		capture.GateThreshold.store(DbToLinear(std::isfinite(thresholdDb) ? thresholdDb : -50.0f));
		capture.GateAttackMs.store(std::isfinite(attackMs) ? (std::max)(attackMs, 0.0f) : 5.0f);
		capture.GateReleaseMs.store(std::isfinite(releaseMs) ? (std::max)(releaseMs, 0.0f) : 150.0f);
		capture.GateEnabled.store(enabled);
	}

	float MiniAudioEngine::GetCaptureLevel() const
	{
		Internal::MiniAudioCapture* capture = RunningCapture();
		return capture ? capture->Level.load() : 0.0f;
	}

	bool MiniAudioEngine::IsCaptureGateOpen() const
	{
		Internal::MiniAudioCapture* capture = RunningCapture();
		return capture && capture->GateOpen.load();
	}

	void MiniAudioEngine::DetachCaptureDeviceForTest()
	{
		if (m_Data && m_Data->Capture)
			Internal::CloseCaptureDevice(*m_Data->Capture, m_Data->Capture->Device);
	}

	void MiniAudioEngine::FeedCaptureForTest(const float* frames, std::uint32_t frameCount)
	{
		if (m_Data && m_Data->Capture && !m_Data->Capture->Device)
			Internal::ProcessCapture(*m_Data->Capture, frames, frameCount);
	}

	void MiniAudioEngine::SimulateCaptureLossForTest()
	{
		if (!m_Data || !m_Data->Capture)
			return;
		Internal::MiniAudioCapture& capture = *m_Data->Capture;
		if (capture.Device)
			ma_device_stop(capture.Device);
		else
			capture.LostForTest = true;
		PollCaptureDevice();
	}

}
