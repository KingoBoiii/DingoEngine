#include "depch.h"
#include "DingoEngine/Scene/Systems/ParticleSync.h"
#include "DingoEngine/Scene/Components.h"
#include "DingoEngine/Scene/Systems/RuntimeComponents.h"

#include "DingoEngine/Graphics/Renderer3D.h"

namespace Dingo::Internal::ParticleSync
{

	namespace
	{
		// A scene that doesn't render for a while (a skipped frame) resumes its particles without a jump.
		constexpr float k_MaxPendingTime = 0.1f;
		// A renderer that stopped drawing the entity (a closed minimap) gives its ring back after this.
		constexpr uint64_t k_MaxIdleFrames = 300;
		// Bursts asked of an entity no renderer draws are kept up to this many, the newest.
		constexpr size_t k_MaxQueuedBursts = 256;

		void DropRuntime(entt::registry& registry, entt::entity handle)
		{
			registry.remove<ParticleEmitterRuntime>(handle);
		}

		entt::entity Resolve(const entt::registry& registry, const std::unordered_map<UUID, entt::entity>& entityMap, UUID id)
		{
			const auto it = entityMap.find(id);
			return it != entityMap.end() && registry.valid(it->second) ? it->second : entt::entity(entt::null);
		}

		void SetPlaying(entt::registry& registry, entt::entity emitter, bool playing)
		{
			if (emitter == entt::null)
				return;
			if (ParticleEmitterComponent* component = registry.try_get<ParticleEmitterComponent>(emitter))
				component->Playing = playing;
		}

		void StopRanges(const std::unordered_map<UUID, entt::entity>& entityMap, entt::registry& registry, entt::entity handle)
		{
			for (const ParticleEventComponent::Binding& binding : registry.get<ParticleEventComponent>(handle).Bindings)
			{
				if (binding.Range)
					SetPlaying(registry, Resolve(registry, entityMap, binding.Emitter), false);
			}
		}
	}

	void Connect(entt::registry& registry, const std::unordered_map<UUID, entt::entity>& entityMap)
	{
		// The hook runs while destroy and clear walk the registry's pools, where creating the runtime pool
		// on first use would invalidate that walk.
		registry.storage<ParticleEmitterRuntime>();
		registry.on_destroy<ParticleEmitterComponent>().connect<&DropRuntime>();
		registry.on_destroy<ParticleEventComponent>().connect<&StopRanges>(entityMap);
	}

	void ApplyAnimationEvents(entt::registry& registry, const std::unordered_map<UUID, entt::entity>& entityMap, const std::vector<std::pair<entt::entity, AnimationEvent>>& events)
	{
		for (const auto& [entity, event] : events)
		{
			if (!registry.valid(entity))
				continue;
			const ParticleEventComponent* component = registry.try_get<ParticleEventComponent>(entity);
			if (!component)
				continue;

			for (const ParticleEventComponent::Binding& binding : component->Bindings)
			{
				if (binding.Event != event.Name)
					continue;

				const entt::entity emitter = Resolve(registry, entityMap, binding.Emitter);
				if (emitter == entt::null)
					continue;
				if (!binding.Range)
				{
					if (event.Type == AnimationEventType::Instant)
						Emit(registry, emitter, binding.Count, nullptr);
				}
				else if (event.Type != AnimationEventType::Instant)
				{
					SetPlaying(registry, emitter, event.Type == AnimationEventType::RangeBegin);
				}
			}
		}
	}

	void Update(entt::registry& registry, float deltaTime)
	{
		for (entt::entity entity : registry.view<ParticleEmitterComponent>())
		{
			ParticleEmitterRuntime& runtime = registry.get_or_emplace<ParticleEmitterRuntime>(entity);
			for (ParticleEmitterRuntime::Instance& instance : runtime.Instances)
				instance.PendingTime = std::min(instance.PendingTime + deltaTime, k_MaxPendingTime);
		}
	}

	void Submit(entt::registry& registry, Renderer3D& renderer, HierarchySystem::WorldMemo& memo)
	{
		auto view = registry.view<Transform3DComponent, ParticleEmitterComponent>();
		for (entt::entity entity : view)
		{
			auto [transform, component] = view.get<Transform3DComponent, ParticleEmitterComponent>(entity);
			if (!component.Effect)
				continue;

			ParticleEmitterRuntime& runtime = registry.get_or_emplace<ParticleEmitterRuntime>(entity);
			if (runtime.Effect != component.Effect)
			{
				runtime.Instances.clear();
				runtime.Effect = component.Effect;
			}

			const uint64_t frame = Renderer::GetFrameIndex();
			std::erase_if(runtime.Instances, [frame](const ParticleEmitterRuntime::Instance& instance) { return instance.LastFrame + k_MaxIdleFrames < frame; });

			auto it = std::ranges::find_if(runtime.Instances, [&renderer](const ParticleEmitterRuntime::Instance& instance) { return renderer.OwnsParticleEmitter(*instance.Emitter); });
			if (it == runtime.Instances.end())
			{
				ParticleEmitterRuntime::Instance instance;
				instance.Emitter = renderer.CreateParticleEmitter(component.Effect);
				instance.BurstsTaken = runtime.Bursts.empty() ? runtime.NextBurst - 1 : runtime.Bursts.front().Serial - 1;
				runtime.Instances.push_back(std::move(instance));
				it = runtime.Instances.end() - 1;
			}
			ParticleEmitterRuntime::Instance& instance = *it;
			instance.LastFrame = frame;

			ParticleEmitter& emitter = *instance.Emitter;
			emitter.SetPlaying(component.Playing);
			emitter.SetRateScale(component.RateScale);
			emitter.SetWorldSpace(component.WorldSpace);
			for (const ParticleEmitterRuntime::Burst& burst : runtime.Bursts)
			{
				if (burst.Serial <= instance.BurstsTaken)
					continue;
				if (burst.AtPosition)
					emitter.EmitAt(burst.Position, burst.Count);
				else
					emitter.Emit(burst.Count);
			}
			instance.BurstsTaken = runtime.NextBurst - 1;

			uint64_t taken = instance.BurstsTaken;
			for (const ParticleEmitterRuntime::Instance& other : runtime.Instances)
				taken = std::min(taken, other.BurstsTaken);
			std::erase_if(runtime.Bursts, [taken](const ParticleEmitterRuntime::Burst& burst) { return burst.Serial <= taken; });

			renderer.SubmitParticles(emitter, memo.Transform(entity, transform), instance.PendingTime);
			instance.PendingTime = 0.0f;
		}
	}

	void Emit(entt::registry& registry, entt::entity entity, uint32_t count, const glm::vec3* worldPosition)
	{
		if (!registry.all_of<ParticleEmitterComponent>(entity) || count == 0)
			return;

		ParticleEmitterRuntime& runtime = registry.get_or_emplace<ParticleEmitterRuntime>(entity);
		runtime.Bursts.push_back({ worldPosition ? *worldPosition : glm::vec3(0.0f), worldPosition != nullptr, count, runtime.NextBurst++ });
		if (runtime.Bursts.size() > k_MaxQueuedBursts)
			runtime.Bursts.erase(runtime.Bursts.begin());
	}

	ParticleEmitter* GetEmitter(entt::registry& registry, entt::entity entity)
	{
		const ParticleEmitterRuntime* runtime = registry.try_get<ParticleEmitterRuntime>(entity);
		return runtime && !runtime->Instances.empty() ? runtime->Instances.front().Emitter.get() : nullptr;
	}

}
