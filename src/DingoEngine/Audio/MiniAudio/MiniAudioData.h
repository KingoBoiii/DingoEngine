#pragma once

// Engine-internal: this header lives under src/ and is NEVER shipped or included by
// client code. It is the only place miniaudio is named (alongside MiniAudioEngine.cpp),
// keeping it a private implementation detail of the engine (same treatment as Jolt/EnTT).

#include "DingoEngine/Audio/AudioEngine.h"

// miniaudio.h is only included from the single .cpp that defines MINIAUDIO_IMPLEMENTATION,
// so this header just forward-declares the ma_* types it stores by value/pointer. We keep
// the full definitions out of here to avoid dragging the ~95k-line header into the PCH via
// depch.h. ma_engine / ma_sound are opaque structs, so pointers are enough for the data
// struct; the values live in heap allocations created in the .cpp.
struct ma_engine;
struct ma_sound;

#include <cstdint>
#include <memory>
#include <vector>

namespace Dingo::Internal
{

	// One controllable live sound. Play() hands out a slot; Update() / Stop() reap it.
	// Generation is bumped every time the slot is recycled so a stale AudioSoundId that
	// still points at this index won't match and control the newer occupant. The 16-bit
	// counter wraps after 65536 recycles of ONE slot, so a handle held across that many
	// replays could alias — accepted; widen AudioSoundId's packing if it ever matters.
	// Liveness is Sound != nullptr — there is no separate Active flag to keep in sync.
	struct SoundSlot
	{
		ma_sound* Sound = nullptr;              // heap ma_sound, owned by this slot while live
		std::shared_ptr<AudioClip> Clip;        // keeps the clip's decoded data alive while playing
		std::uint16_t Generation = 0;
	};

	struct MiniAudioData
	{
		ma_engine* Engine = nullptr;            // heap ma_engine
		std::vector<SoundSlot> Slots;           // index-stable; grows, never shrinks
	};

}
