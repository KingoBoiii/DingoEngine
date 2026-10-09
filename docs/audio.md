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
`SetDefaultAttenuation`. The defaults are miniaudio's own (inverse distance, `MaxDistance` =
float max). `AudioAttenuationModel::None` keeps the panning but drops the falloff.

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

The test framework's Audio Bus Test (`--test=bus`) runs `[PASS]`/`[FAIL]` checks of the bus API
at start-up. Then it plays a looping beep on a "Music" bus and gives the Properties panel a
volume slider and mute and pause boxes for Master, Music and SFX, plus a one-shot button for
SFX. Use it to hear that a bus change reaches a sound that is already playing.

## Limits

- One listener.
- Buses have volume, mute and pause only. There are no effects (filters, reverb) and no
  ducking.
- Bus volume changes apply at once, with no fade.
