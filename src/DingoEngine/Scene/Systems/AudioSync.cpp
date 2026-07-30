#include "depch.h"
#include "DingoEngine/Scene/Systems/AudioSync.h"

#include "DingoEngine/Scene/Components.h"
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

				auto sourceView = registry.view<AudioSourceComponent>();
				for (entt::entity handle : sourceView)
				{
					AudioSourceComponent& source = sourceView.get<AudioSourceComponent>(handle);
					if (!source.Spatialized || source.RuntimeSound == k_InvalidSound)
						continue;

					audio.SetPosition(source.RuntimeSound, PositionOf(registry, handle));
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

				AudioSourceComponent& source = registry.get<AudioSourceComponent>(handle);
				if (!source.Clip)
					return;

				AudioEngine& audio = Application::Get().GetAudioEngine();
				if (source.RuntimeSound != k_InvalidSound)
					audio.Stop(source.RuntimeSound);

				SoundPlayParams params;
				params.Volume = source.Volume;
				params.Pitch = source.Pitch;
				params.Looping = source.Looping;
				params.Spatialized = source.Spatialized;
				if (source.Spatialized)
					params.Position = PositionOf(registry, handle);

				source.RuntimeSound = audio.Play(source.Clip, params);
			}

			void StopSource(entt::registry& registry, entt::entity handle)
			{
				if (!registry.all_of<AudioSourceComponent>(handle))
					return;

				AudioSourceComponent& source = registry.get<AudioSourceComponent>(handle);
				if (source.RuntimeSound == k_InvalidSound)
					return;

				Application::Get().GetAudioEngine().Stop(source.RuntimeSound);
				source.RuntimeSound = k_InvalidSound;
			}

			void StopAllSources(entt::registry& registry)
			{
				// Reachable from ~Scene (via Clear) — a static-lifetime scene can be destroyed
				// after the Application is gone, when there is no engine left to stop.
				if (!Application::HasInstance())
					return;

				AudioEngine& audio = Application::Get().GetAudioEngine();
				for (entt::entity handle : registry.view<AudioSourceComponent>())
				{
					AudioSourceComponent& source = registry.get<AudioSourceComponent>(handle);
					if (source.RuntimeSound != k_InvalidSound)
					{
						audio.Stop(source.RuntimeSound);
						source.RuntimeSound = k_InvalidSound;
					}
				}
			}

		}

	}

}
