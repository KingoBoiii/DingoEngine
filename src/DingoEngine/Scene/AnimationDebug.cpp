#include "depch.h"
#include "DingoEngine/Scene/AnimationDebug.h"

#include "DingoEngine/Graphics/Model.h"
#include "DingoEngine/Scene/Components.h"
#include "DingoEngine/Scene/Scene.h"
#include "DingoEngine/Scene/SceneData.h"
#include "DingoEngine/Scene/Systems/RuntimeComponents.h"

#include <algorithm>

namespace Dingo
{

	namespace Internal
	{

		namespace AnimationDebug
		{

			namespace
			{

				std::vector<std::pair<Scene*, SceneData*>>& Scenes()
				{
					// Leaked: a Scene destroyed during static destruction still unregisters.
					static auto* s_Scenes = new std::vector<std::pair<Scene*, SceneData*>>();
					return *s_Scenes;
				}

				std::string EntityName(const entt::registry& registry, entt::entity handle)
				{
					const TagComponent* tag = registry.try_get<TagComponent>(handle);
					return tag ? tag->Tag : std::format("Entity {}", entt::to_integral(handle));
				}

			}

			void RegisterScene(Scene* scene, SceneData* data)
			{
				Scenes().emplace_back(scene, data);
			}

			void UnregisterScene(Scene* scene)
			{
				std::vector<std::pair<Scene*, SceneData*>>& scenes = Scenes();
				std::erase_if(scenes, [scene](const std::pair<Scene*, SceneData*>& entry) { return entry.first == scene; });
			}

			void CollectAnimators(std::vector<AnimatorRow>& out)
			{
				out.clear();
				for (const auto& [scene, data] : Scenes())
				{
					entt::registry& registry = data->Registry;
					auto view = registry.view<const AnimatorComponent, const AnimatorRuntime>();
					for (entt::entity handle : view)
					{
						const AnimatorRuntime& runtime = view.get<const AnimatorRuntime>(handle);
						if (!runtime.Instance)
							continue;

						const AnimatorComponent& settings = view.get<const AnimatorComponent>(handle);
						AnimatorRow& row = out.emplace_back();
						row.Scene = scene->GetName();
						row.Entity = EntityName(registry, handle);
						row.Enabled = settings.Enabled;
						row.Speed = settings.Speed;

						// An animator whose model went away keeps that model's clip pointers, which may be freed.
						const SkinnedMeshRendererComponent* skinned = registry.try_get<SkinnedMeshRendererComponent>(handle);
						const Skeleton* skeleton = skinned && skinned->Model ? skinned->Model->GetSkeleton() : nullptr;
						if (skeleton && runtime.SkeletonId == skeleton->GetId())
							row.Instance = runtime.Instance.get();
						if (skinned && skinned->Model)
							row.Model = skinned->Model->GetFilePath().filename().string();
					}
				}
			}

			void CollectRecentEvents(std::vector<EventRow>& out, size_t max)
			{
				out.clear();

				struct Candidate
				{
					const SceneData* Data;
					const AnimationSystem::EventScratch::RecentEvent* Event;
				};

				std::vector<Candidate> candidates;
				for (const auto& entry : Scenes())
				{
					const AnimationSystem::EventScratch& scratch = entry.second->AnimationEvents;
					for (size_t i = 0; i < scratch.RecentCount; ++i)
						candidates.push_back({ entry.second, &scratch.Recent[i] });
				}

				std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b)
				{
					return a.Event->Sequence < b.Event->Sequence;
				});

				const size_t first = candidates.size() > max ? candidates.size() - max : 0;
				for (size_t i = first; i < candidates.size(); ++i)
				{
					const Candidate& candidate = candidates[i];
					const AnimationSystem::EventScratch::RecentEvent& event = *candidate.Event;

					EventRow& row = out.emplace_back();
					row.Entity = candidate.Data->Registry.valid(event.Entity) ? EntityName(candidate.Data->Registry, event.Entity) : "(destroyed)";
					row.Clip = event.Clip;
					row.Name = event.Name;
					row.Type = event.Type;
					row.Time = event.Time;
					row.Layer = event.Layer;
					row.Sequence = event.Sequence;
				}
			}

		}

	}

}
