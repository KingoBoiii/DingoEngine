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

		void DropRuntime(entt::registry& registry, entt::entity handle)
		{
			registry.remove<ParticleEmitterRuntime>(handle);
		}
	}

	void Connect(entt::registry& registry)
	{
		// The hook runs while destroy and clear walk the registry's pools, where creating the runtime pool
		// on first use would invalidate that walk.
		registry.storage<ParticleEmitterRuntime>();
		registry.on_destroy<ParticleEmitterComponent>().connect<&DropRuntime>();
	}

	void Update(entt::registry& registry, float deltaTime)
	{
		for (entt::entity entity : registry.view<ParticleEmitterComponent>())
		{
			ParticleEmitterRuntime& runtime = registry.get_or_emplace<ParticleEmitterRuntime>(entity);
			runtime.PendingTime = std::min(runtime.PendingTime + deltaTime, k_MaxPendingTime);
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
			if (!runtime.Emitter || runtime.Owner != &renderer || runtime.Effect != component.Effect)
			{
				runtime.Emitter = renderer.CreateParticleEmitter(component.Effect);
				runtime.Owner = &renderer;
				runtime.Effect = component.Effect;
			}

			ParticleEmitter& emitter = *runtime.Emitter;
			emitter.SetPlaying(component.Playing);
			emitter.SetRateScale(component.RateScale);
			emitter.SetWorldSpace(component.WorldSpace);
			for (const ParticleEmitterRuntime::Burst& burst : runtime.Bursts)
			{
				if (burst.AtPosition)
					emitter.EmitAt(burst.Position, burst.Count);
				else
					emitter.Emit(burst.Count);
			}
			runtime.Bursts.clear();

			renderer.SubmitParticles(emitter, memo.Transform(entity, transform), runtime.PendingTime);
			runtime.PendingTime = 0.0f;
		}
	}

	void Emit(entt::registry& registry, entt::entity entity, uint32_t count, const glm::vec3* worldPosition)
	{
		if (!registry.all_of<ParticleEmitterComponent>(entity) || count == 0)
			return;

		ParticleEmitterRuntime& runtime = registry.get_or_emplace<ParticleEmitterRuntime>(entity);
		if (runtime.Emitter)
		{
			if (worldPosition)
				runtime.Emitter->EmitAt(*worldPosition, count);
			else
				runtime.Emitter->Emit(count);
			return;
		}
		runtime.Bursts.push_back({ worldPosition ? *worldPosition : glm::vec3(0.0f), worldPosition != nullptr, count });
	}

	ParticleEmitter* GetEmitter(entt::registry& registry, entt::entity entity)
	{
		const ParticleEmitterRuntime* runtime = registry.try_get<ParticleEmitterRuntime>(entity);
		return runtime ? runtime->Emitter.get() : nullptr;
	}

}
