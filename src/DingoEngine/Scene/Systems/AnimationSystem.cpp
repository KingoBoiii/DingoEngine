#include "depch.h"
#include "DingoEngine/Scene/Systems/AnimationSystem.h"

#include "DingoEngine/Graphics/Animator.h"
#include "DingoEngine/Graphics/Model.h"
#include "DingoEngine/Scene/Components.h"
#include "DingoEngine/Scene/Systems/HierarchySystem.h"
#include "DingoEngine/Scene/Systems/RuntimeComponents.h"
#include "DingoEngine/Scene/Systems/ScriptSystem.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>

namespace Dingo
{

	namespace Internal
	{

		namespace AnimationSystem
		{

			namespace
			{

				void DropRuntime(entt::registry& registry, entt::entity handle)
				{
					registry.remove<AnimatorRuntime>(handle);
				}

				const Skeleton* SkeletonOf(const entt::registry& registry, entt::entity handle)
				{
					const SkinnedMeshRendererComponent* skinned = registry.try_get<SkinnedMeshRendererComponent>(handle);
					return skinned && skinned->Model ? skinned->Model->GetSkeleton() : nullptr;
				}

				// Bound to the runtime's skeleton by id: its stored Skeleton* may belong to a freed model.
				const Animator* BoundAnimator(const entt::registry& registry, entt::entity handle, const Skeleton& skeleton)
				{
					const AnimatorRuntime* runtime = registry.try_get<AnimatorRuntime>(handle);
					return runtime && runtime->SkeletonId == skeleton.GetId() ? runtime->Instance.get() : nullptr;
				}

				Animator& EnsureAnimator(entt::registry& registry, entt::entity handle, const Model& model)
				{
					AnimatorRuntime* runtime = registry.try_get<AnimatorRuntime>(handle);
					if (!runtime)
					{
						runtime = &registry.emplace<AnimatorRuntime>(handle);
						runtime->Instance = std::make_unique<Animator>();
					}

					const Skeleton& skeleton = *model.GetSkeleton();
					if (runtime->SkeletonId == skeleton.GetId())
					{
						if (runtime->ModelGeneration != model.GetGeneration())
						{
							runtime->ModelGeneration = model.GetGeneration();
							runtime->Instance->Evaluate();
						}
						return *runtime->Instance;
					}

					runtime->SkeletonId = skeleton.GetId();
					runtime->ModelGeneration = model.GetGeneration();
					runtime->Instance->SetSkeleton(&skeleton);

					const AnimatorComponent& settings = registry.get<AnimatorComponent>(handle);
					if (settings.PlayOnStart && !settings.DefaultClip.empty())
					{
						if (const AnimationClip* clip = model.FindAnimation(settings.DefaultClip))
						{
							runtime->Instance->Play(clip);
						}
						else
						{
							const TagComponent* tag = registry.try_get<TagComponent>(handle);
							DE_CORE_WARN("AnimatorComponent on '{}': the model has no clip named '{}'", tag ? tag->Tag : std::string(), settings.DefaultClip);
						}
					}

					// Posed now, so a disabled animator and a body baked on one of its joints before the
					// first update see DefaultClip's first frame; no events, so that update still fires
					// the clip's start.
					runtime->Instance->Evaluate();
					return *runtime->Instance;
				}

				void RecordEvents(EventScratch& scratch, entt::entity handle, std::span<const AnimationEvent> events)
				{
					static uint64_t s_Sequence = 0;
					for (const AnimationEvent& event : events)
					{
						EventScratch::RecentEvent& slot = scratch.Recent[scratch.RecentNext];
						scratch.RecentNext = (scratch.RecentNext + 1) % EventScratch::k_RecentEvents;
						scratch.RecentCount = std::min(scratch.RecentCount + 1, EventScratch::k_RecentEvents);

						slot.Entity = handle;
						if (event.Clip)
							slot.Clip.assign(event.Clip->GetName());
						else
							slot.Clip.clear();
						slot.Name = event.Name;
						slot.Type = event.Type;
						slot.Time = event.Time;
						slot.Layer = event.Layer;
						slot.Sequence = ++s_Sequence;
					}
				}

			}

			void Connect(entt::registry& registry)
			{
				// The hook runs while destroy and clear walk the registry's pools, where creating the
				// runtime pool on first use would invalidate that walk.
				registry.storage<AnimatorRuntime>();
				registry.on_destroy<AnimatorComponent>().connect<&DropRuntime>();
			}

			void Update(entt::registry& registry, ScriptSystem& scripts, EventScratch& scratch, float deltaTime)
			{
				// Every event is copied out before any is delivered: a handler may update, rebind or
				// free any animator, its own included.
				scratch.Deliveries.clear();
				for (const std::pair<entt::entity, AnimationEvent>& waiting : scratch.Waiting)
				{
					if (registry.valid(waiting.first) && scripts.Find(waiting.first))
						scratch.Deliveries.push_back(waiting);
				}
				scratch.Waiting.clear();

				auto view = registry.view<AnimatorComponent, SkinnedMeshRendererComponent>();
				for (entt::entity handle : view)
				{
					const Model* model = view.get<SkinnedMeshRendererComponent>(handle).Model;
					if (!model || !model->GetSkeleton())
						continue;

					Animator& animator = EnsureAnimator(registry, handle, *model);
					const AnimatorComponent& settings = view.get<AnimatorComponent>(handle);
					if (!settings.Enabled)
						continue;

					animator.Update(deltaTime * settings.Speed);
					RecordEvents(scratch, handle, animator.GetEventsThisFrame());
					if (animator.GetEventsThisFrame().empty() || !scripts.Find(handle))
						continue;

					// A script spawned this frame starts before its first OnUpdate, next frame; its
					// animator's first events wait for it there.
					std::vector<std::pair<entt::entity, AnimationEvent>>& queue = scripts.IsStarted(handle) ? scratch.Deliveries : scratch.Waiting;
					for (const AnimationEvent& event : animator.GetEventsThisFrame())
						queue.emplace_back(handle, event);
				}

				// A handler may replace a script with one that hasn't started; its events wait for it.
				for (const std::pair<entt::entity, AnimationEvent>& delivery : scratch.Deliveries)
				{
					if (!registry.valid(delivery.first))
						continue;
					if (scripts.IsStarted(delivery.first))
						scripts.DeliverAnimationEvent(delivery.first, delivery.second);
					else if (scripts.Find(delivery.first))
						scratch.Waiting.push_back(delivery);
				}
			}

			Animator* GetAnimator(entt::registry& registry, entt::entity handle)
			{
				if (!registry.all_of<AnimatorComponent>(handle))
					return nullptr;

				const SkinnedMeshRendererComponent* skinned = registry.try_get<SkinnedMeshRendererComponent>(handle);
				if (!skinned || !skinned->Model || !skinned->Model->GetSkeleton())
					return nullptr;

				return &EnsureAnimator(registry, handle, *skinned->Model);
			}

			void EnsureAnimators(entt::registry& registry)
			{
				auto view = registry.view<AnimatorComponent, SkinnedMeshRendererComponent>();
				for (entt::entity handle : view)
				{
					const Model* model = view.get<SkinnedMeshRendererComponent>(handle).Model;
					if (model && model->GetSkeleton())
						EnsureAnimator(registry, handle, *model);
				}
			}

			void EnsureAncestorAnimators(entt::registry& registry, entt::entity handle)
			{
				for (entt::entity parent = HierarchySystem::GetParent(registry, handle); parent != entt::null; parent = HierarchySystem::GetParent(registry, parent))
					GetAnimator(registry, parent);
			}

			std::span<const glm::mat4> Palette(const entt::registry& registry, entt::entity handle, const Skeleton& skeleton)
			{
				const Animator* animator = BoundAnimator(registry, handle, skeleton);
				return animator ? animator->GetSkinningPalette() : std::span<const glm::mat4>(skeleton.GetRestPalette());
			}

			bool JointFrame(const entt::registry& registry, entt::entity handle, std::string_view joint, glm::mat4& frame)
			{
				const Skeleton* skeleton = SkeletonOf(registry, handle);
				if (!skeleton)
					return false;

				const int32_t index = skeleton->FindJoint(joint);
				if (index == Skeleton::k_InvalidJoint)
					return false;

				const Animator* animator = BoundAnimator(registry, handle, *skeleton);
				const glm::mat4 model = animator
					? animator->GetJointTransform(index)
					: skeleton->GetRootTransform() * skeleton->GetRestGlobalTransforms()[index];

				// An FBX root transform carries the file's unit scale (0.01 for centimetres); passed on, it
				// would shrink whatever a hand holds.
				glm::vec3 position, scale;
				glm::quat rotation;
				HierarchySystem::Decompose(model, position, rotation, scale);
				frame = glm::translate(glm::mat4(1.0f), position) * glm::mat4_cast(rotation);
				return true;
			}

		}

	}

}
