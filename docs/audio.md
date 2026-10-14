# Audio

DingoEngine plays audio through one `AudioEngine`, owned by the application:
`Application::Get().GetAudioEngine()`. The backend is [miniaudio](https://miniaud.io), but no
miniaudio type appears in a public header; you hold `AudioClip`s, `AudioSoundId`s and
`AudioBusId`s.

## Clips and sounds

A clip is decoded once and played as many times as you like. Each play is an independent
sound.

```cpp
AudioEngine& audio = Application::Get().GetAudioEngine();
std::shared_ptr<AudioClip> jump = audio.LoadClip("audio/jump.wav"); // .wav, .ogg or .mp3; nullptr on failure

audio.PlayOneShot(jump);                    // fire and forget
audio.PlayOneShot(jump, 0.5f);              // at half volume

SoundPlayParams params;
params.Looping = true;
params.Volume = 0.8f;
AudioSoundId hum = audio.Play(jump, params); // keep the handle to control it
audio.SetPitch(hum, 1.2f);
audio.Pause(hum);
audio.Resume(hum);
audio.Stop(hum);
```

A relative path is looked up under the asset root first, then the working directory. The
[AssetManager](asset-pipeline.md) loads clips too (`AssetType::AudioClip`), on its worker thread.

- A handle stays valid until the sound is stopped or, for a non-looping sound, finishes.
  `AudioEngine::Update` (called by the application every frame) reaps finished sounds, and from
  then on the handle is stale and every call with it is a no-op.
- A clip must not outlive the engine that decoded it. The application shuts audio down after
  layers and managed assets, so clips held by a layer are fine.

## Positional audio

`SoundPlayParams::Spatialized` with a `Position`, or the `PlayOneShot(clip, position)` overload,
places a sound in the world. The listener is set with `SetListenerPosition` and
`SetListenerOrientation`.

Distance falloff is a `SoundAttenuation`: per sound in `SoundPlayParams::Attenuation`, live
through `SetAttenuation(sound, ...)`, or for every later sound through
`SetDefaultAttenuation`. A sound without its own takes the engine default
(`GetDefaultAttenuation`), which is miniaudio's own: inverse distance, `MaxDistance` = float max.

```cpp
SoundAttenuation falloff;
falloff.Model = AudioAttenuationModel::Linear;
falloff.MinDistance = 2.0f;                      // full volume inside 2 units
falloff.MaxDistance = 20.0f;                     // Linear needs a finite maximum
falloff.MinGain = 0.1f;                          // never fully silent

SoundPlayParams params;
params.Spatialized = true;
params.Position = campfire;
params.Attenuation = falloff;
AudioSoundId id = audio.Play(crackle, params);
audio.SetPosition(id, movedCampfire);            // a moving source
audio.SetAttenuation(id, falloff);               // re-tune it live
```

| Field | Meaning |
|---|---|
| `Model` | `Inverse` (the default), `Linear` (reaches `MinGain` at `MaxDistance` when `Rolloff` is 1), `Exponential`, or `None` (keeps the panning, drops the falloff). |
| `MinDistance` | Full volume inside this radius (default 1). |
| `MaxDistance` | The distance past which the gain stops changing (default float max). |
| `Rolloff` | How fast the gain falls between the two; 1 is the model's own curve (default 1). |
| `MinGain`, `MaxGain` | The gain's floor and ceiling, for example a `MinGain` of 0.3 keeps distant sounds audible (defaults 0 and 1). |

An `AudioSourceComponent` has the same `Attenuation` (empty = the engine default), and an
`AudioListenerComponent` entity drives the listener.

## Streams

A stream is a sound you feed with PCM while the game runs, instead of a clip from disk: voice
chat, procedural audio, a video's soundtrack. It takes the same `SoundPlayParams` as `Play`, so
it can be spatialized, attenuated, routed to a bus and moved with `SetPosition`.

```cpp
AudioStreamDesc desc;
desc.SampleRate = 16000;      // the pushed data's rate; the engine resamples to the device
desc.Channels = 1;
desc.BufferFrames = 16000 / 2; // ring buffer capacity: 0.5 s here

SoundPlayParams params;
params.Spatialized = true;
params.Position = speakerPosition;
params.Bus = voiceBus;
AudioSoundId voice = audio.CreateStream(desc, params);

// Each time data arrives (interleaved frames, int16_t or float in [-1, 1]):
uint32_t taken = audio.PushStreamFrames(voice, pcm, frameCount); // fewer when the buffer is full
audio.SetPosition(voice, speakerPosition);

// A jitter buffer: hold data back until ~60 ms is queued, drop old data when it grows.
uint32_t queued = audio.GetQueuedStreamFrames(voice);

audio.Stop(voice); // the only way it ends, with StopBus, DestroyBus and Shutdown
```

- A stream never finishes on its own and `Update` never reaps it. When the buffer runs dry it
  fades to silence over 5 ms and stays alive (`IsPlaying` stays true). Pushed data fades back in.
- Once silent, a stream resumes when one device read is queued: the device period's frame
  count (often 480, 1024 on some devices, more at 96 or 192 kHz), counted at the stream's rate,
  or the whole buffer if that is smaller. 480 frames is 30 ms at 16 kHz and 60 ms at 8 kHz. A
  jitter buffer that drops data must keep its target above that, or the stream stays silent.
- `Looping` is ignored, and `SetLooping` on a stream is a no-op. `Pause`, `Resume`, `SetPitch`,
  `SetVolume` and `SetAttenuation` work as on any sound. Pitch changes how fast the stream drains.
- `CreateStream` fails (`k_InvalidSound`, logged) with `BufferFrames` 0 or past 2 GB of floats,
  `Channels` 0 or above 254, or `SampleRate` outside 8000–384000 Hz.
- Call `PushStreamFrames` and `GetQueuedStreamFrames` on the thread that drives the engine (the
  main thread), like every other `AudioEngine` call; only the device thread's read is lock-free.
  Data a network thread receives goes through the game's own queue. Both return 0 for a handle
  that is no live stream.

## Buses

A bus mixes the sounds routed to it and its sub-buses, then feeds its parent. `k_MasterBus` is
the root: it feeds the output and every sound plays on it unless told otherwise. A bus's
volume, mute and pause reach every sound under it, including sounds that are already playing.

```cpp
AudioBusId music = audio.CreateBus("Music");
AudioBusId sfx = audio.CreateBus("SFX");
AudioBusId ui = audio.CreateBus("UI", sfx);   // a sub-bus of SFX

SoundPlayParams params;
params.Looping = true;
params.Bus = music;
audio.Play(theme, params);

audio.PlayOneShot(click, ui);                 // one-shots take a bus too
audio.PlayOneShot(boom, position, sfx, 0.7f);

audio.SetBusVolume(music, 0.4f);              // the options menu's music slider
audio.SetBusMuted(sfx, true);                 // unmuting restores the bus's volume
audio.PauseBus(sfx);                          // pause menu: freezes SFX, music keeps playing
audio.ResumeBus(sfx);
audio.StopBus(sfx);                           // stops every sound on SFX and UI
audio.DestroyBus(sfx);                        // stops them and destroys SFX and UI
```

| Call | Does |
|---|---|
| `CreateBus(name, parent = k_MasterBus)` | Makes a bus. Names are unique: a name in use returns that bus (with a warning). An empty name or a destroyed parent returns `k_InvalidBus`. |
| `FindBus(name)` | The live bus with that name, or `k_InvalidBus`. The master bus is `"Master"`. |
| `DestroyBus(bus)` | Stops every sound on the bus and its sub-buses, then destroys them all. `k_MasterBus` can't be destroyed. |
| `IsBusValid(bus)`, `GetBusCount()` | Whether an id still names a live bus; how many buses live (the master bus not counted). |
| `SetBusVolume` / `GetBusVolume` | Linear gain. On `k_MasterBus` this is `SetMasterVolume`. |
| `SetBusMuted` / `IsBusMuted` | Silences the bus without changing its volume. |
| `PauseBus` / `ResumeBus` / `IsBusPaused` | Freezes every sound under the bus where it is. |
| `StopBus(bus)` | Stops every sound on the bus and its sub-buses. The buses stay. |

Ids work like sound ids: a destroyed bus's id goes stale, setters on it are no-ops, and getters
return 0 or false. A sound played on a stale bus plays on `k_MasterBus` instead, with a warning
logged once.

A paused bus has some side effects:

- Its sounds still report `IsPlaying`, because each one is still started. Only the bus has
  stopped pulling audio from them.
- A sound started on a paused bus stays silent until `ResumeBus`.
- A one-shot on a paused bus is not reaped until it has finished after the resume.

### In the ECS

`AudioSourceComponent::Bus` (default `k_MasterBus`) routes an entity's sound:

```cpp
AudioSourceComponent& source = entity.AddComponent<AudioSourceComponent>();
source.Clip = footsteps;
source.Bus = audio.FindBus("SFX");
```

Bus ids belong to the engine, not the scene, so create buses once (in `OnAttach`, say) and
keep them across scene switches.

## Debug panel

The F3 Engine tab shows whether the engine is valid, the master volume, the live sound count
and the bus count.

## Test

The test framework's Audio Bus Test (`--test=bus`) runs `[PASS]`/`[FAIL]` checks of the bus and
stream APIs at start-up, plus one a second later that a stream played what was pushed and
survived the underrun. Then it plays a looping beep on a "Music" bus and gives the Properties
panel a volume slider and mute and pause boxes for Master, Music and SFX, plus a one-shot button
for SFX. Use it to hear that a bus change reaches a sound that is already playing. "Stream a
tone on SFX" streams a 440 Hz tone that circles the listener; "Gaps" leaves 300 ms of every
second unfed, to listen for clicks at each underrun.

## Limits

- One listener.
- Buses have volume, mute and pause only. There are no effects (filters, reverb) and no
  ducking.
- Bus volume changes apply at once, with no fade.
- No microphone capture; a stream plays PCM the game already has.
