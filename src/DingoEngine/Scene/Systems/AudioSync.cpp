#include "depch.h"
#include "DingoEngine/Scene/Systems/AudioSync.h"

#include "DingoEngine/Scene/Components.h"
#include "DingoEngine/Scene/Systems/RuntimeComponents.h"
#include "DingoEngine/Core/Application.h"
#include "DingoEngine/Audio/AudioEngine.h"

namespace Dingo
{

	namespace Internal
	{

		namespace AudioSync
		{

			glm::vec3 PositionOf(const entt::registry& registry, entt::entity handle)
			{
				if (const Transform3DComponent* transform3D = registry.try_get<Transform3DComponent>(handle))
					return transform3D->Position;

				const TransformComponent& transform = registry.get<TransformComponent>(handle);
				return glm::vec3(transform.Position.x, transform.Position.y, 0.0f);
			}

			void SyncListenerAndSources(entt::registry& registry)
			{
				AudioEngine& audio = Application::Get().GetAudioEngine();

				auto sourceView = registry.view<AudioSourceRuntime, AudioSourceComponent>();
				for (entt::entity handle : sourceView)
				{
					if (!sourceView.get<AudioSourceComponent>(handle).Spatialized)
						continue;

					audio.SetPosition(sourceView.get<AudioSourceRuntime>(handle).Sound, PositionOf(registry, handle));
				}

				// Primary listener search mirrors CameraUtils::FindPrimaryCamera: first
				// Primary wins, else the first listener found. If the scene has none, leave
				// the engine's listener as it was (no implicit reset to origin).
				entt::entity listenerHandle = entt::null;
				auto listenerView = registry.view<AudioListenerComponent>();
				for (entt::entity handle : listenerView)
				{
					if (listenerHandle == entt::null)
						listenerHandle = handle;

					if (listenerView.get<AudioListenerComponent>(handle).Primary)
					{
						listenerHandle = handle;
						break;
					}
				}

				if (listenerHandle == entt::null)
					return;

				// Single lookup serves both position and orientation below.
				if (const Transform3DComponent* transform3D = registry.try_get<Transform3DComponent>(listenerHandle))
				{
					audio.SetListenerPosition(transform3D->Position);

					// Orientation only comes from a 3D transform (same convention as the
					// perspective camera view in CameraUtils::ViewProjection: the entity's local
					// -Z is forward, +Y is up). A 2D listener has no rotation to derive this
					// from, so it keeps whatever orientation the engine already has.
					audio.SetListenerOrientation(transform3D->Forward(), transform3D->Up());
				}
				else
				{
					const TransformComponent& transform = registry.get<TransformComponent>(listenerHandle);
					audio.SetListenerPosition(glm::vec3(transform.Position.x, transform.Position.y, 0.0f));
				}
			}

			void PlaySource(entt::registry& registry, entt::entity handle)
			{
				if (!registry.all_of<AudioSourceComponent>(handle))
					return;

				const AudioSourceComponent& source = registry.get<AudioSourceComponent>(handle);
				if (!source.Clip)
					return;

				AudioEngine& audio = Application::Get().GetAudioEngine();
				if (const AudioSourceRuntime* runtime = registry.try_get<AudioSourceRuntime>(handle))
					audio.Stop(runtime->Sound);

				SoundPlayParams params;
				params.Volume = source.Volume;
				params.Pitch = source.Pitch;
				params.Looping = source.Looping;
				params.Spatialized = source.Spatialized;
				if (source.Spatialized)
				{
					params.Position = PositionOf(registry, handle);
					params.Attenuation = source.Attenuation;
				}

				const AudioSoundId sound = audio.Play(source.Clip, params);
				if (sound != k_InvalidSound)
					registry.emplace_or_replace<AudioSourceRuntime>(handle).Sound = sound;
				else
					registry.remove<AudioSourceRuntime>(handle);
			}

			void StopSource(entt::registry& registry, entt::entity handle)
			{
				const AudioSourceRuntime* runtime = registry.try_get<AudioSourceRuntime>(handle);
				if (!runtime)
					return;

				Application::Get().GetAudioEngine().Stop(runtime->Sound);
				registry.remove<AudioSourceRuntime>(handle);
			}

			void StopAllSources(entt::registry& registry)
			{
				// Reachable from ~Scene (via Clear) — a static-lifetime scene can be destroyed
				// after the Application is gone, when there is no engine left to stop.
				if (Application::HasInstance())
				{
					AudioEngine& audio = Application::Get().GetAudioEngine();
					registry.view<AudioSourceRuntime>().each([&audio](const AudioSourceRuntime& runtime)
					{
						audio.Stop(runtime.Sound);
					});
				}

				registry.clear<AudioSourceRuntime>();
			}

			AudioSoundId RuntimeSound(const entt::registry& registry, entt::entity handle)
			{
				if (!registry.valid(handle))
					return k_InvalidSound;

				const AudioSourceRuntime* runtime = registry.try_get<AudioSourceRuntime>(handle);
				return runtime ? runtime->Sound : k_InvalidSound;
			}

		}

	}

}
