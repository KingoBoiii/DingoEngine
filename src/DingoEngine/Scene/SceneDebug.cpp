#include "depch.h"
#include "DingoEngine/Scene/SceneDebug.h"
#include "DingoEngine/Core/PathText.h"

#include "DingoEngine/Core/Application.h"
#include "DingoEngine/Audio/AudioEngine.h"
#include "DingoEngine/Graphics/AnimationClip.h"
#include "DingoEngine/Graphics/Material.h"
#include "DingoEngine/Graphics/Model.h"
#include "DingoEngine/Graphics/Particles.h"
#include "DingoEngine/Physics/2D/Physics2D.h"
#include "DingoEngine/Physics/3D/CharacterController3D.h"
#include "DingoEngine/Physics/3D/Physics3D.h"
#include "DingoEngine/Scene/Components.h"
#include "DingoEngine/Scene/Scene.h"
#include "DingoEngine/Scene/SceneData.h"
#include "DingoEngine/Scene/ScriptableEntity.h"
#include "DingoEngine/Scene/Systems/CameraUtils.h"
#include "DingoEngine/Scene/Systems/RuntimeComponents.h"

#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <typeinfo>

#if defined(__GNUG__)
	#include <cxxabi.h>
	#include <cstdlib>
#endif

namespace Dingo
{

	namespace Internal
	{

		namespace SceneDebug
		{

			namespace
			{

				std::vector<LiveScene>& Scenes()
				{
					// Leaked: a Scene destroyed during static destruction still unregisters.
					static auto* s_Scenes = new std::vector<LiveScene>();
					return *s_Scenes;
				}

				std::recursive_mutex& ScenesMutex()
				{
					static auto* s_Mutex = new std::recursive_mutex();
					return *s_Mutex;
				}

				const LiveScene* FindScene(uint64_t serial)
				{
					for (const LiveScene& scene : Scenes())
					{
						if (scene.Serial == serial)
							return &scene;
					}
					return nullptr;
				}

				std::string EntityName(const entt::registry& registry, entt::entity handle)
				{
					const TagComponent* tag = registry.try_get<TagComponent>(handle);
					return tag && !tag->Tag.empty() ? tag->Tag : std::format("Entity {}", entt::to_integral(handle));
				}

				UUID EntityId(const entt::registry& registry, entt::entity handle)
				{
					const IDComponent* id = registry.try_get<IDComponent>(handle);
					return id ? id->ID : UUID();
				}

				std::string Text(bool value) { return value ? "yes" : "no"; }
				std::string Text(float value) { return std::format("{:.3f}", value); }
				std::string Text(uint32_t value) { return std::format("{}", value); }
				std::string Text(const glm::vec2& v) { return std::format("({:.3f}, {:.3f})", v.x, v.y); }
				std::string Text(const glm::vec3& v) { return std::format("({:.3f}, {:.3f}, {:.3f})", v.x, v.y, v.z); }
				std::string Text(const glm::vec4& v) { return std::format("({:.3f}, {:.3f}, {:.3f}, {:.3f})", v.x, v.y, v.z, v.w); }
				std::string Hex(uint32_t value) { return std::format("0x{:08X}", value); }

				std::string EulerText(const glm::quat& q)
				{
					return Text(glm::degrees(glm::eulerAngles(q))) + " deg";
				}

				const char* ConstraintTypeName(ConstraintType3D type)
				{
					switch (type)
					{
						case ConstraintType3D::Fixed: return "Fixed";
						case ConstraintType3D::Point: return "Point";
						case ConstraintType3D::Hinge: return "Hinge";
						case ConstraintType3D::SwingTwist: return "SwingTwist";
						case ConstraintType3D::Distance: return "Distance";
						case ConstraintType3D::Slider: return "Slider";
					}
					return "?";
				}

				const char* BodyTypeName(int type)
				{
					switch (type)
					{
						case 0: return "Static";
						case 1: return "Dynamic";
						case 2: return "Kinematic";
						default: return "?";
					}
				}

				const char* ShadowCastingName(ShadowCasting shadows)
				{
					switch (shadows)
					{
						case ShadowCasting::On: return "On";
						case ShadowCasting::Off: return "Off";
						case ShadowCasting::ShadowsOnly: return "ShadowsOnly";
					}
					return "?";
				}

				const char* CullingModeName(AnimatorCullingMode mode)
				{
					switch (mode)
					{
						case AnimatorCullingMode::AlwaysAnimate: return "always animate";
						case AnimatorCullingMode::PauseWhenCulled: return "pause when culled";
						case AnimatorCullingMode::ReduceWhenCulled: return "reduce when culled";
					}
					return "?";
				}

				const char* RootMotionName(RootMotionMode mode)
				{
					switch (mode)
					{
						case RootMotionMode::Off: return "Off";
						case RootMotionMode::XZ: return "XZ";
						case RootMotionMode::Full: return "Full";
					}
					return "?";
				}

				const char* FogModeName(FogMode mode)
				{
					switch (mode)
					{
						case FogMode::None: return "None";
						case FogMode::Linear: return "Linear";
						case FogMode::Exponential: return "Exponential";
						case FogMode::ExponentialSquared: return "ExponentialSquared";
					}
					return "?";
				}

				std::string MeshText(const Mesh* mesh)
				{
					if (!mesh)
						return "none";
					return std::format("id {}, {} vertices, {} indices", mesh->GetId(), mesh->GetVertexCount(), mesh->GetIndexCount());
				}

				std::string MaterialText(const Material* material)
				{
					if (!material)
						return "built-in lit";
					const std::string& name = material->GetParams().DebugName;
					return name.empty() ? std::format("id {}", material->GetId()) : std::format("{} (id {})", name, material->GetId());
				}

				std::string TypeName(const std::type_info& type)
				{
#if defined(__GNUG__)
					int status = 0;
					char* demangled = abi::__cxa_demangle(type.name(), nullptr, nullptr, &status);
					if (status == 0 && demangled)
					{
						std::string name = demangled;
						std::free(demangled);
						return name;
					}
					std::free(demangled);
#endif
					return type.name();
				}

				// Fetched once per collect, so a large scene pays one lookup per component type, not per entity.
				class ComponentPools
				{
				public:
					explicit ComponentPools(const SceneData& data) : m_Data(data)
					{
						Set<TransformComponent>(Component::Transform);
						Set<Transform3DComponent>(Component::Transform3D);
						Set<SpriteRendererComponent>(Component::SpriteRenderer);
						Set<CircleRendererComponent>(Component::CircleRenderer);
						Set<TextComponent>(Component::Text);
						Set<CameraComponent>(Component::Camera);
						Set<PostProcessComponent>(Component::PostProcess);
						Set<DirectionalLightComponent>(Component::DirectionalLight);
						Set<AmbientLightComponent>(Component::AmbientLight);
						Set<PointLightComponent>(Component::PointLight);
						Set<SpotLightComponent>(Component::SpotLight);
						Set<FogComponent>(Component::Fog);
						Set<RigidBody2DComponent>(Component::RigidBody2D);
						Set<BoxCollider2DComponent>(Component::BoxCollider2D);
						Set<CircleCollider2DComponent>(Component::CircleCollider2D);
						Set<MeshRendererComponent>(Component::MeshRenderer);
						Set<SkinnedMeshRendererComponent>(Component::SkinnedMeshRenderer);
						Set<AnimatorComponent>(Component::Animator);
						Set<ParticleEmitterComponent>(Component::ParticleEmitter);
						Set<ParticleEventComponent>(Component::ParticleEvent);
						Set<RigidBody3DComponent>(Component::RigidBody3D);
						Set<BoxCollider3DComponent>(Component::BoxCollider3D);
						Set<SphereCollider3DComponent>(Component::SphereCollider3D);
						Set<CapsuleCollider3DComponent>(Component::CapsuleCollider3D);
						Set<MeshCollider3DComponent>(Component::MeshCollider3D);
						Set<CharacterController3DComponent>(Component::CharacterController3D);
						Set<Constraint3DComponent>(Component::Constraint3D);
						Set<RagdollComponent>(Component::Ragdoll);
						Set<AudioSourceComponent>(Component::AudioSource);
						Set<AudioListenerComponent>(Component::AudioListener);
					}

					uint64_t Of(entt::entity handle) const
					{
						uint64_t bits = 0;
						for (size_t component = 0; component < m_Pools.size(); ++component)
						{
							if (m_Pools[component] && m_Pools[component]->contains(handle))
								bits |= uint64_t(1) << component;
						}
						if (m_Data.Scripts.Find(handle))
							bits |= ComponentBit(Component::Script);
						return bits;
					}

				private:
					template<typename T>
					void Set(Component component)
					{
						m_Pools[static_cast<size_t>(component)] = m_Data.Registry.storage<T>();
					}

					const SceneData& m_Data;
					std::array<const entt::sparse_set*, static_cast<size_t>(Component::Count)> m_Pools{};
				};

				// Entity index order is creation order, but for a recycled slot.
				void SortedRoots(const entt::registry& registry, std::vector<entt::entity>& roots)
				{
					roots.clear();
					for (entt::entity handle : registry.view<const IDComponent>())
					{
						if (HierarchySystem::GetParent(registry, handle) == entt::null)
							roots.push_back(handle);
					}
					std::sort(roots.begin(), roots.end(), [](entt::entity a, entt::entity b)
					{
						return entt::to_entity(a) < entt::to_entity(b);
					});
				}

				Section& AddSection(EntityDetail& out, std::string title, bool runtime = false)
				{
					Section& section = out.Sections.emplace_back();
					section.Title = std::move(title);
					section.Runtime = runtime;
					return section;
				}

				void Add(Section& section, std::string name, std::string value)
				{
					section.Fields.push_back({ std::move(name), std::move(value) });
				}

			}

			std::unique_lock<std::recursive_mutex> LockScenes()
			{
				return std::unique_lock(ScenesMutex());
			}

			void RegisterScene(Scene* scene, SceneData* data)
			{
				std::lock_guard lock(ScenesMutex());
				static uint64_t s_NextSerial = 1;
				Scenes().push_back({ scene, data, s_NextSerial++ });
			}

			void UnregisterScene(Scene* scene)
			{
				std::lock_guard lock(ScenesMutex());
				std::erase_if(Scenes(), [scene](const LiveScene& entry) { return entry.Instance == scene; });
			}

			const std::vector<LiveScene>& LiveScenes()
			{
				return Scenes();
			}

			const char* ComponentName(Component component)
			{
				switch (component)
				{
					case Component::Transform: return "Transform";
					case Component::Transform3D: return "Transform3D";
					case Component::SpriteRenderer: return "SpriteRenderer";
					case Component::CircleRenderer: return "CircleRenderer";
					case Component::Text: return "Text";
					case Component::Camera: return "Camera";
					case Component::PostProcess: return "PostProcess";
					case Component::DirectionalLight: return "DirectionalLight";
					case Component::AmbientLight: return "AmbientLight";
					case Component::PointLight: return "PointLight";
					case Component::SpotLight: return "SpotLight";
					case Component::Fog: return "Fog";
					case Component::RigidBody2D: return "RigidBody2D";
					case Component::BoxCollider2D: return "BoxCollider2D";
					case Component::CircleCollider2D: return "CircleCollider2D";
					case Component::MeshRenderer: return "MeshRenderer";
					case Component::SkinnedMeshRenderer: return "SkinnedMeshRenderer";
					case Component::Animator: return "Animator";
					case Component::ParticleEmitter: return "ParticleEmitter";
					case Component::ParticleEvent: return "ParticleEvent";
					case Component::RigidBody3D: return "RigidBody3D";
					case Component::BoxCollider3D: return "BoxCollider3D";
					case Component::SphereCollider3D: return "SphereCollider3D";
					case Component::CapsuleCollider3D: return "CapsuleCollider3D";
					case Component::MeshCollider3D: return "MeshCollider3D";
					case Component::CharacterController3D: return "CharacterController3D";
					case Component::Constraint3D: return "Constraint3D";
					case Component::Ragdoll: return "Ragdoll";
					case Component::AudioSource: return "AudioSource";
					case Component::AudioListener: return "AudioListener";
					case Component::Script: return "Script";
					case Component::Count: break;
				}
				return "?";
			}

			void CollectScenes(std::vector<SceneRow>& out)
			{
				out.clear();
				std::lock_guard lock(ScenesMutex());
				for (const LiveScene& live : Scenes())
				{
					const Scene& scene = *live.Instance;
					const entt::registry& registry = live.Data->Registry;

					SceneRow& row = out.emplace_back();
					row.Serial = live.Serial;
					row.Name = scene.GetName();
					row.Running = scene.IsRunning();
					row.PhysicsRunning = scene.IsPhysicsRunning();
					row.Has2DWorld = live.Data->Physics.Get2D() != nullptr;
					row.Has3DWorld = live.Data->Physics.Get3D() != nullptr;

					for (entt::entity handle : registry.view<const IDComponent>())
					{
						++row.Entities;
						if (HierarchySystem::GetParent(registry, handle) == entt::null)
							++row.Roots;
						if (live.Data->Scripts.Find(handle))
							++row.Scripts;
					}

					row.DirectionalLights = static_cast<uint32_t>(registry.view<const DirectionalLightComponent>().size());
					row.AmbientLights = static_cast<uint32_t>(registry.view<const AmbientLightComponent>().size());
					auto countLocal = [&row, &registry](entt::entity handle, bool enabled, bool castShadows, uint32_t& count)
					{
						if (!registry.all_of<Transform3DComponent>(handle))
						{
							++row.UnplacedLocalLights;
							return;
						}
						row.HasLight = true;
						if (!enabled)
						{
							++row.DisabledLocalLights;
							return;
						}
						if (HierarchySystem::IsInactive(registry, handle))
						{
							++row.InactiveLocalLights;
							return;
						}
						++count;
						row.ShadowedLocalLights += castShadows ? 1 : 0;
					};
					for (auto [handle, light] : registry.view<const PointLightComponent>().each())
						countLocal(handle, light.Enabled, light.CastShadows, row.PointLights);
					for (auto [handle, light] : registry.view<const SpotLightComponent>().each())
						countLocal(handle, light.Enabled, light.CastShadows, row.SpotLights);
					row.HasLight = row.HasLight || row.DirectionalLights > 0 || row.AmbientLights > 0;

					for (auto [handle, runtime] : registry.view<const RigidBody2DRuntime>().each())
						row.Bodies2D += runtime.Body != 0 ? 1 : 0;
					for (auto [handle, runtime] : registry.view<const RigidBody3DRuntime>().each())
					{
						if (runtime.Body == k_InvalidBody3D)
							continue;
						++row.Bodies3D;
						const RigidBody3DComponent* settings = registry.try_get<RigidBody3DComponent>(handle);
						row.Sensors3D += settings && settings->IsSensor ? 1 : 0;
					}
					row.Controllers = static_cast<uint32_t>(registry.view<const CharacterController3DRuntime>().size());

					entt::entity camera = entt::null;
					if (CameraUtils::FindPrimaryCamera(registry, camera))
					{
						row.Camera = EntityName(registry, camera);
						const CameraComponent& settings = registry.get<CameraComponent>(camera);
						row.CameraProjection = settings.Type == CameraComponent::ProjectionType::Perspective ? "perspective" : "orthographic";
					}

					row.FogComponents = static_cast<uint32_t>(registry.view<const FogComponent>().size());
					row.ClearColor = scene.GetClearColor();
					row.Gravity2D = scene.GetGravity2D();
					row.Gravity3D = scene.GetGravity3D();
					row.FixedTick = scene.GetParams().FixedTick;
					row.InterpolatePhysics = scene.GetParams().InterpolatePhysics;
					row.FixedStep = scene.GetFixedTimeStep();
					row.FixedAlpha = scene.GetFixedTimeAlpha();
					row.Tick = scene.GetTick();
					row.FixedStepsLastUpdate = scene.GetFixedStepsLastUpdate();
					row.DroppedFixedSteps = scene.GetDroppedFixedSteps();
				}
			}

			bool CollectEntities(uint64_t sceneSerial, std::vector<EntityRow>& out)
			{
				out.clear();
				std::lock_guard lock(ScenesMutex());
				const LiveScene* live = FindScene(sceneSerial);
				if (!live)
					return false;

				const entt::registry& registry = live->Data->Registry;
				const ComponentPools pools(*live->Data);
				std::vector<entt::entity> roots;
				SortedRoots(registry, roots);

				struct Pending
				{
					entt::entity Handle;
					uint32_t Depth;
				};
				std::vector<Pending> stack;
				std::vector<entt::entity> children;
				for (entt::entity root : roots)
				{
					stack.push_back({ root, 0 });
					while (!stack.empty())
					{
						const Pending next = stack.back();
						stack.pop_back();

						EntityRow& row = out.emplace_back();
						row.Id = EntityId(registry, next.Handle);
						row.Index = static_cast<uint32_t>(entt::to_entity(next.Handle));
						row.Name = EntityName(registry, next.Handle);
						row.Depth = next.Depth;
						row.Socket = std::string(HierarchySystem::GetSocket(registry, next.Handle));
						row.Components = pools.Of(next.Handle);

						const HierarchyComponent* links = registry.try_get<HierarchyComponent>(next.Handle);
						if (!links)
							continue;
						row.ChildCount = links->ChildCount;

						children.clear();
						for (entt::entity child = links->FirstChild; child != entt::null; child = registry.get<HierarchyComponent>(child).NextSibling)
							children.push_back(child);
						for (auto it = children.rbegin(); it != children.rend(); ++it)
							stack.push_back({ *it, next.Depth + 1 });
					}
				}
				return true;
			}

			bool WriteMotor(uint64_t sceneSerial, UUID id, const MotorView& motor)
			{
				std::lock_guard lock(ScenesMutex());
				const LiveScene* live = FindScene(sceneSerial);
				if (!live)
					return false;

				SceneData& data = *live->Data;
				const auto found = data.EntityMap.find(id);
				if (found == data.EntityMap.end() || !data.Registry.valid(found->second))
					return false;
				Constraint3DComponent* constraint = data.Registry.try_get<Constraint3DComponent>(found->second);
				if (!constraint || (constraint->Type != ConstraintType3D::Hinge && constraint->Type != ConstraintType3D::SwingTwist))
					return false;

				constraint->MotorMode = motor.Mode;
				constraint->Motor = motor.Motor;
				constraint->TwistMotor = motor.TwistMotor;
				const float axisLength = glm::length(constraint->AxisA);
				const glm::vec3 axis = axisLength > 1e-6f ? constraint->AxisA / axisLength : glm::vec3(0.0f, 1.0f, 0.0f);
				constraint->TargetRotation = glm::angleAxis(motor.TargetAngle, axis);
				constraint->TargetAngularVelocity = axis * motor.TargetVelocity;
				return true;
			}

			bool WriteRagdoll(uint64_t sceneSerial, UUID id, const RagdollView& ragdoll)
			{
				std::lock_guard lock(ScenesMutex());
				const LiveScene* live = FindScene(sceneSerial);
				if (!live)
					return false;

				SceneData& data = *live->Data;
				const auto found = data.EntityMap.find(id);
				if (found == data.EntityMap.end() || !data.Registry.valid(found->second))
					return false;
				RagdollComponent* settings = data.Registry.try_get<RagdollComponent>(found->second);
				if (!settings)
					return false;

				settings->Blend = ragdoll.Blend;
				settings->Strength = ragdoll.Strength;
				settings->DriveFrequency = ragdoll.DriveFrequency;
				settings->DriveToAnimation = ragdoll.DriveToAnimation;
				return true;
			}

			bool CollectEntity(uint64_t sceneSerial, UUID id, EntityDetail& out)
			{
				out = EntityDetail();
				std::lock_guard lock(ScenesMutex());
				const LiveScene* live = FindScene(sceneSerial);
				if (!live)
					return false;

				const SceneData& data = *live->Data;
				const entt::registry& registry = data.Registry;
				const auto found = data.EntityMap.find(id);
				if (found == data.EntityMap.end() || !registry.valid(found->second))
					return false;
				const entt::entity handle = found->second;

				out.Id = id;
				out.Name = EntityName(registry, handle);
				out.Active = !registry.all_of<InactiveSelfComponent>(handle);
				out.ActiveInHierarchy = !HierarchySystem::IsInactive(registry, handle);
				const entt::entity parent = HierarchySystem::GetParent(registry, handle);
				if (parent != entt::null)
				{
					out.HasParent = true;
					out.Parent = EntityId(registry, parent);
					out.ParentName = EntityName(registry, parent);
				}
				if (const HierarchyComponent* links = registry.try_get<HierarchyComponent>(handle))
					out.ChildCount = links->ChildCount;
				out.Socket = std::string(HierarchySystem::GetSocket(registry, handle));

				if (const TransformComponent* transform = registry.try_get<TransformComponent>(handle))
				{
					out.Has2D = true;
					HierarchySystem::WorldPose2D(registry, handle, *transform, out.WorldPosition2D, out.WorldRotation2D);

					Section& section = AddSection(out, "Transform (2D)");
					Add(section, "Local position", Text(transform->Position));
					Add(section, "Local rotation", Text(transform->Rotation) + " deg");
					Add(section, "Size", Text(transform->Size));
					Add(section, "World position", Text(out.WorldPosition2D));
					Add(section, "World rotation", Text(out.WorldRotation2D) + " deg");
				}

				if (const Transform3DComponent* transform = registry.try_get<Transform3DComponent>(handle))
				{
					out.Has3D = true;
					HierarchySystem::WorldPose(registry, handle, *transform, out.WorldPosition, out.WorldRotation, out.WorldScale);

					Section& section = AddSection(out, "Transform3D");
					Add(section, "Local position", Text(transform->Position));
					Add(section, "Local rotation", EulerText(transform->Rotation));
					Add(section, "Local scale", Text(transform->Scale));
					Add(section, "World position", Text(out.WorldPosition));
					Add(section, "World rotation", EulerText(out.WorldRotation));
					Add(section, "World scale", Text(out.WorldScale));
				}

				if (const SpriteRendererComponent* sprite = registry.try_get<SpriteRendererComponent>(handle))
				{
					Section& section = AddSection(out, "SpriteRenderer");
					Add(section, "Color", Text(sprite->Color));
					Add(section, "Texture", sprite->Texture ? std::format("id {}", sprite->Texture->GetId()) : "none");
				}

				if (const CircleRendererComponent* circle = registry.try_get<CircleRendererComponent>(handle))
				{
					Section& section = AddSection(out, "CircleRenderer");
					Add(section, "Color", Text(circle->Color));
					Add(section, "Thickness", Text(circle->Thickness));
					Add(section, "Fade", Text(circle->Fade));
				}

				if (const TextComponent* text = registry.try_get<TextComponent>(handle))
				{
					Section& section = AddSection(out, "Text");
					Add(section, "Text", text->Text);
					Add(section, "Font", text->Font ? "set" : "none");
					Add(section, "Color", Text(text->Color));
					Add(section, "Size", Text(text->Size));
					Add(section, "Centered", Text(text->Centered));
				}

				if (const CameraComponent* camera = registry.try_get<CameraComponent>(handle))
				{
					Section& section = AddSection(out, "Camera");
					const bool perspective = camera->Type == CameraComponent::ProjectionType::Perspective;
					Add(section, "Projection", perspective ? "Perspective" : "Orthographic");
					if (perspective)
					{
						Add(section, "FOV", Text(camera->FOV) + " deg");
						Add(section, "Near / far", std::format("{:.3f} / {:.3f}", camera->PerspNear, camera->PerspFar));
					}
					else
					{
						Add(section, "Size", Text(camera->OrthographicSize));
						Add(section, "Near / far", std::format("{:.3f} / {:.3f}", camera->OrthoNear, camera->OrthoFar));
					}
					Add(section, "Primary", Text(camera->Primary));
				}

				if (const PostProcessComponent* post = registry.try_get<PostProcessComponent>(handle))
				{
					Section& section = AddSection(out, "PostProcess");
					Add(section, "Enabled", Text(post->Settings.Enabled));
					Add(section, "Bloom", Text(post->Settings.Bloom.Enabled));
					Add(section, "Ambient occlusion", Text(post->Settings.AmbientOcclusion.Enabled));
				}

				if (const DirectionalLightComponent* light = registry.try_get<DirectionalLightComponent>(handle))
				{
					Section& section = AddSection(out, "DirectionalLight");
					Add(section, "Direction", Text(light->Direction));
					Add(section, "Color", Text(light->Color));
					Add(section, "Intensity", Text(light->Intensity));
					DE_SUPPRESS_DEPRECATED_BEGIN
					Add(section, "Ambient", Text(light->Ambient));
					DE_SUPPRESS_DEPRECATED_END
					Add(section, "Cast shadows", Text(light->CastShadows));
					Add(section, "Shadow strength", Text(light->ShadowStrength));
					Add(section, "Caster groups", Hex(light->ShadowCasterGroups));
				}

				if (const AmbientLightComponent* light = registry.try_get<AmbientLightComponent>(handle))
				{
					Section& section = AddSection(out, "AmbientLight");
					Add(section, "Color", Text(light->Color));
					Add(section, "Intensity", Text(light->Intensity));
				}

				if (const PointLightComponent* light = registry.try_get<PointLightComponent>(handle))
				{
					Section& section = AddSection(out, "PointLight");
					Add(section, "Enabled", Text(light->Enabled));
					Add(section, "Color", Text(light->Color));
					Add(section, "Intensity", Text(light->Intensity));
					Add(section, "Range", Text(light->Range));
					Add(section, "Cast shadows", Text(light->CastShadows));
					Add(section, "Shadow strength", Text(light->ShadowStrength));
					Add(section, "Caster groups", Hex(light->ShadowCasterGroups));
				}

				if (const SpotLightComponent* light = registry.try_get<SpotLightComponent>(handle))
				{
					Section& section = AddSection(out, "SpotLight");
					Add(section, "Enabled", Text(light->Enabled));
					Add(section, "Color", Text(light->Color));
					Add(section, "Intensity", Text(light->Intensity));
					Add(section, "Range", Text(light->Range));
					Add(section, "Cone (inner / outer)", std::format("{:.1f} / {:.1f} deg", light->InnerConeAngle, light->OuterConeAngle));
					Add(section, "Direction (local)", Text(light->Direction));
					if (out.Has3D)
						Add(section, "Direction (world)", Text(out.WorldRotation * light->Direction));
					Add(section, "Cast shadows", Text(light->CastShadows));
					Add(section, "Shadow strength", Text(light->ShadowStrength));
					Add(section, "Caster groups", Hex(light->ShadowCasterGroups));
				}

				if (const FogComponent* fog = registry.try_get<FogComponent>(handle))
				{
					Section& section = AddSection(out, "Fog");
					Add(section, "Enabled", Text(fog->Enabled));
					Add(section, "Mode", FogModeName(fog->Settings.Mode));
					Add(section, "Start / end", std::format("{:.2f} / {:.2f}", fog->Settings.Start, fog->Settings.End));
					Add(section, "Density", Text(fog->Settings.Density));
					Add(section, "Max opacity", Text(fog->Settings.MaxOpacity));
					Add(section, "Color", fog->UseClearColor ? "the clear colour" : Text(fog->Settings.Color));
				}

				if (const RigidBody2DComponent* body = registry.try_get<RigidBody2DComponent>(handle))
				{
					Section& section = AddSection(out, "RigidBody2D");
					Add(section, "Type", BodyTypeName(static_cast<int>(body->Type)));
					Add(section, "Interpolate", Text(body->Interpolate));
					Add(section, "Fixed rotation", Text(body->FixedRotation));
				}

				if (const BoxCollider2DComponent* box = registry.try_get<BoxCollider2DComponent>(handle))
				{
					Section& section = AddSection(out, "BoxCollider2D");
					Add(section, "Offset", Text(box->Offset));
					Add(section, "Half extents", Text(box->HalfExtents));
					Add(section, "Density / friction / restitution", std::format("{:.2f} / {:.2f} / {:.2f}", box->Density, box->Friction, box->Restitution));
				}

				if (const CircleCollider2DComponent* circle = registry.try_get<CircleCollider2DComponent>(handle))
				{
					Section& section = AddSection(out, "CircleCollider2D");
					Add(section, "Offset", Text(circle->Offset));
					Add(section, "Radius", Text(circle->Radius));
					Add(section, "Density / friction / restitution", std::format("{:.2f} / {:.2f} / {:.2f}", circle->Density, circle->Friction, circle->Restitution));
				}

				if (const MeshRendererComponent* renderer = registry.try_get<MeshRendererComponent>(handle))
				{
					Section& section = AddSection(out, "MeshRenderer");
					Add(section, "Mesh", MeshText(renderer->Mesh));
					Add(section, "Material", MaterialText(renderer->Material));
					Add(section, "Color", Text(renderer->Color));
					Add(section, "Visible", Text(renderer->Visible));
					Add(section, "Shadows", ShadowCastingName(renderer->Shadows));
					Add(section, "Shadow groups", Hex(renderer->ShadowGroups));
					Add(section, "Static", Text(renderer->Static));
					if (renderer->Surface)
					{
						const SurfaceOverride& surface = *renderer->Surface;
						Add(section, "Surface emissive", std::format("{} x {:.3f}", Text(surface.EmissiveColor), surface.EmissiveStrength));
						Add(section, "Surface roughness / specular", std::format("{:.3f} / {:.3f}", surface.Roughness, surface.Specular));
					}
					else
					{
						Add(section, "Surface", "the material's");
					}
				}

				if (const SkinnedMeshRendererComponent* renderer = registry.try_get<SkinnedMeshRendererComponent>(handle))
				{
					Section& section = AddSection(out, "SkinnedMeshRenderer");
					Add(section, "Model", renderer->Model ? PathToUtf8(renderer->Model->GetFilePath().filename()) : "none");
					Add(section, "Material", MaterialText(renderer->Material));
					Add(section, "Color", Text(renderer->Color));
					Add(section, "Visible", Text(renderer->Visible));
					Add(section, "Shadows", ShadowCastingName(renderer->Shadows));
					Add(section, "Shadow groups", Hex(renderer->ShadowGroups));
				}

				if (const AnimatorComponent* animator = registry.try_get<AnimatorComponent>(handle))
				{
					Section& section = AddSection(out, "Animator");
					Add(section, "Default clip", animator->DefaultClip.empty() ? "none" : animator->DefaultClip);
					Add(section, "Play on start", Text(animator->PlayOnStart));
					Add(section, "Enabled", Text(animator->Enabled));
					Add(section, "Speed", Text(animator->Speed));
					Add(section, "Root motion", RootMotionName(animator->RootMotion));
					Add(section, "Apply root motion", Text(animator->ApplyRootMotion));
					Add(section, "Culling mode", CullingModeName(animator->CullingMode));
				}

				if (const ParticleEmitterComponent* emitter = registry.try_get<ParticleEmitterComponent>(handle))
				{
					Section& section = AddSection(out, "ParticleEmitter");
					std::string effect = "none";
					if (emitter->Effect)
						effect = emitter->Effect->GetParams().DebugName.empty() ? "unnamed" : emitter->Effect->GetParams().DebugName;
					Add(section, "Effect", effect);
					Add(section, "Playing", Text(emitter->Playing));
					Add(section, "Rate scale", Text(emitter->RateScale));
					Add(section, "World space", Text(emitter->WorldSpace));
				}

				if (const ParticleEventComponent* events = registry.try_get<ParticleEventComponent>(handle))
				{
					Section& section = AddSection(out, "ParticleEvent");
					for (const ParticleEventComponent::Binding& binding : events->Bindings)
					{
						const auto emitter = data.EntityMap.find(binding.Emitter);
						const std::string target = emitter != data.EntityMap.end() && registry.valid(emitter->second)
							? EntityName(registry, emitter->second) : std::format("missing {}", static_cast<uint64_t>(binding.Emitter));
						Add(section, binding.Event, binding.Range ? std::format("range on {}", target) : std::format("burst {} on {}", binding.Count, target));
					}
					if (events->Bindings.empty())
						Add(section, "Bindings", "none");
				}

				if (const RigidBody3DComponent* body = registry.try_get<RigidBody3DComponent>(handle))
				{
					Section& section = AddSection(out, "RigidBody3D");
					Add(section, "Type", BodyTypeName(static_cast<int>(body->Type)));
					Add(section, "Continuous collision", Text(body->ContinuousCollision));
					Add(section, "Sensor", Text(body->IsSensor));
					Add(section, "Query layers", Hex(body->QueryLayers));
					Add(section, "Interpolate", Text(body->Interpolate));
					Add(section, "Collision group", std::to_string(body->CollisionGroup));
					Add(section, "Collide within group", Text(body->CollideWithinGroup));
				}

				if (const BoxCollider3DComponent* box = registry.try_get<BoxCollider3DComponent>(handle))
				{
					Section& section = AddSection(out, "BoxCollider3D");
					Add(section, "Half extents", Text(box->HalfExtents));
					Add(section, "Friction / restitution", std::format("{:.2f} / {:.2f}", box->Friction, box->Restitution));
				}

				if (const SphereCollider3DComponent* sphere = registry.try_get<SphereCollider3DComponent>(handle))
				{
					Section& section = AddSection(out, "SphereCollider3D");
					Add(section, "Radius", Text(sphere->Radius));
					Add(section, "Friction / restitution", std::format("{:.2f} / {:.2f}", sphere->Friction, sphere->Restitution));
				}

				if (const CapsuleCollider3DComponent* capsule = registry.try_get<CapsuleCollider3DComponent>(handle))
				{
					Section& section = AddSection(out, "CapsuleCollider3D");
					Add(section, "Radius", Text(capsule->Radius));
					Add(section, "Half height", Text(capsule->HalfHeight));
					Add(section, "Friction / restitution", std::format("{:.2f} / {:.2f}", capsule->Friction, capsule->Restitution));
				}

				if (const MeshCollider3DComponent* mesh = registry.try_get<MeshCollider3DComponent>(handle))
				{
					Section& section = AddSection(out, "MeshCollider3D");
					Add(section, "Mesh", mesh->Mesh ? MeshText(mesh->Mesh) : "the MeshRenderer's");
					Add(section, "Convex", Text(mesh->Convex));
					Add(section, "Friction / restitution", std::format("{:.2f} / {:.2f}", mesh->Friction, mesh->Restitution));
				}

				if (const CharacterController3DComponent* controller = registry.try_get<CharacterController3DComponent>(handle))
				{
					Section& section = AddSection(out, "CharacterController3D");
					Add(section, "Radius / height", std::format("{:.2f} / {:.2f}", controller->Radius, controller->Height));
					Add(section, "Step height", Text(controller->StepHeight));
					Add(section, "Max slope", Text(controller->MaxSlopeAngle) + " deg");
					Add(section, "Collide with characters", Text(controller->CollideWithCharacters));
				}

				if (const Constraint3DComponent* constraint = registry.try_get<Constraint3DComponent>(handle))
				{
					Section& section = AddSection(out, "Constraint3D");
					Add(section, "Type", ConstraintTypeName(constraint->Type));
					const auto other = data.EntityMap.find(constraint->Other);
					Add(section, "Other", !constraint->Other ? std::string("world")
						: other != data.EntityMap.end() && registry.valid(other->second) ? EntityName(registry, other->second)
						: std::format("missing {}", static_cast<uint64_t>(constraint->Other)));
					Add(section, "Anchors", std::format("{} / {}", Text(constraint->AnchorA), Text(constraint->AnchorB)));
					Add(section, "Axes", std::format("{} / {}", Text(constraint->AxisA), Text(constraint->AxisB)));
					const ConstraintType3D type = constraint->Type;
					if (type == ConstraintType3D::Hinge || type == ConstraintType3D::Slider || type == ConstraintType3D::Distance)
						Add(section, "Limits", std::format("{:.3f} .. {:.3f}", constraint->MinLimit, constraint->MaxLimit));
					if (type == ConstraintType3D::SwingTwist)
					{
						Add(section, "Swing Y / Z", std::format("{:.3f} / {:.3f}", constraint->SwingLimitY, constraint->SwingLimitZ));
						Add(section, "Twist", std::format("{:.3f} .. {:.3f}", constraint->TwistMin, constraint->TwistMax));
					}
					Add(section, "Collide connected", Text(constraint->CollideConnected));

					if (constraint->Type == ConstraintType3D::Hinge || constraint->Type == ConstraintType3D::SwingTwist)
					{
						MotorView& motor = out.Motor;
						motor.Present = true;
						motor.SwingTwist = constraint->Type == ConstraintType3D::SwingTwist;
						motor.Mode = constraint->MotorMode;
						motor.Motor = constraint->Motor;
						motor.TwistMotor = constraint->TwistMotor;
						const float axisLength = glm::length(constraint->AxisA);
						motor.Axis = axisLength > 1e-6f ? constraint->AxisA / axisLength : glm::vec3(0.0f, 1.0f, 0.0f);
						const glm::quat target = glm::normalize(constraint->TargetRotation);
						const float sine = glm::dot(glm::vec3(target.x, target.y, target.z), motor.Axis);
						motor.TargetAngle = 2.0f * std::atan2(sine, target.w);
						if (motor.TargetAngle > glm::pi<float>())
							motor.TargetAngle -= glm::two_pi<float>();
						else if (motor.TargetAngle < -glm::pi<float>())
							motor.TargetAngle += glm::two_pi<float>();
						motor.TargetVelocity = glm::dot(constraint->TargetAngularVelocity, motor.Axis);

						const Constraint3DRuntime* runtime = registry.try_get<Constraint3DRuntime>(handle);
						const Physics3D* physics = data.Physics.Get3D();
						if (runtime && physics && physics->IsConstraintValid(runtime->Constraint))
						{
							motor.Live = true;
							if (motor.SwingTwist)
							{
								motor.Torque = physics->GetMotorTorque(runtime->Constraint, MotorAxis3D::Swing);
								motor.TwistTorque = physics->GetMotorTorque(runtime->Constraint, MotorAxis3D::Twist);
							}
							else
							{
								motor.Angle = physics->GetConstraintValue(runtime->Constraint);
								motor.Torque = physics->GetMotorTorque(runtime->Constraint, MotorAxis3D::Hinge);
							}
						}
					}
				}

				if (const RagdollComponent* settings = registry.try_get<RagdollComponent>(handle))
				{
					RagdollView& view = out.Ragdoll;
					view.Present = true;
					view.Blend = settings->Blend;
					view.Strength = settings->Strength;
					view.DriveFrequency = settings->DriveFrequency;
					view.DriveToAnimation = settings->DriveToAnimation;
					view.TotalMass = settings->TotalMass;
					view.CollideWithinRagdoll = settings->CollideWithinRagdoll;
					view.Recipe = settings->Recipe;

					const RagdollRuntime* runtime = registry.try_get<RagdollRuntime>(handle);
					const SkinnedMeshRendererComponent* skinned = registry.try_get<SkinnedMeshRendererComponent>(handle);
					const Skeleton* skeleton = skinned && skinned->Model ? skinned->Model->GetSkeleton() : nullptr;
					const Physics3D* physics = data.Physics.Get3D();
					if (runtime && skeleton && physics && runtime->SkeletonId == skeleton->GetId())
					{
						view.Live = true;
						view.FollowsRoot = runtime->FollowsRoot;
						auto boneName = [&](int32_t joint)
						{
							return joint >= 0 && static_cast<size_t>(joint) < skeleton->GetJointCount() ? skeleton->GetJoint(static_cast<uint32_t>(joint)).Name : std::string("?");
						};
						for (const RagdollRuntime::Part& part : runtime->Parts)
						{
							RagdollView::Part& row = view.Parts.emplace_back();
							row.Bone = boneName(part.Joint);
							if (part.Parent >= 0)
								row.Parent = boneName(runtime->Parts[part.Parent].Joint);
							row.Hinge = part.Hinge;
							row.Mass = part.Mass;
							row.MaxTorque = part.MaxTorque;
							if (physics->IsBodyValid(part.Body))
								row.Speed = glm::length(physics->GetLinearVelocity(part.Body));
							if (part.Parent >= 0 && physics->IsConstraintValid(part.Constraint))
							{
								row.Torque = physics->GetMotorTorque(part.Constraint, part.Hinge ? MotorAxis3D::Hinge : MotorAxis3D::Swing);
								if (!part.Hinge)
									row.TwistTorque = physics->GetMotorTorque(part.Constraint, MotorAxis3D::Twist);
							}
						}
					}
				}

				if (const AudioSourceComponent* source = registry.try_get<AudioSourceComponent>(handle))
				{
					Section& section = AddSection(out, "AudioSource");
					Add(section, "Clip", source->Clip ? "set" : "none");
					Add(section, "Volume / pitch", std::format("{:.2f} / {:.2f}", source->Volume, source->Pitch));
					Add(section, "Looping", Text(source->Looping));
					Add(section, "Spatialized", Text(source->Spatialized));
					Add(section, "Attenuation", source->Attenuation ? "custom" : "engine default");
					Add(section, "Play on start", Text(source->PlayOnStart));
					Add(section, "Bus", Text(static_cast<uint32_t>(source->Bus)));
				}

				if (const AudioListenerComponent* listener = registry.try_get<AudioListenerComponent>(handle))
				{
					Section& section = AddSection(out, "AudioListener");
					Add(section, "Primary", Text(listener->Primary));
				}

				if (const RigidBody2DRuntime* runtime = registry.try_get<RigidBody2DRuntime>(handle))
				{
					Section& section = AddSection(out, "2D body", true);
					Add(section, "Body", std::format("{:#x}", runtime->Body));
					Add(section, "Shapes", std::format("box {}, circle {}", runtime->BoxShape != 0 ? "yes" : "no", runtime->CircleShape != 0 ? "yes" : "no"));
					const Physics2D* physics = data.Physics.Get2D();
					if (physics && runtime->Body != 0 && physics->IsBodyValid(runtime->Body))
					{
						Add(section, "Position", Text(physics->GetPosition(runtime->Body)));
						Add(section, "Linear velocity", Text(physics->GetLinearVelocity(runtime->Body)));
					}
				}

				if (const RigidBody3DRuntime* runtime = registry.try_get<RigidBody3DRuntime>(handle))
				{
					Section& section = AddSection(out, "3D body", true);
					Add(section, "Body", runtime->Body == k_InvalidBody3D ? "none" : std::format("{:#x}", runtime->Body));
					const Physics3D* physics = data.Physics.Get3D();
					if (physics && runtime->Body != k_InvalidBody3D && physics->IsBodyValid(runtime->Body))
					{
						Add(section, "Position", Text(physics->GetPosition(runtime->Body)));
						Add(section, "Linear velocity", Text(physics->GetLinearVelocity(runtime->Body)));
						Add(section, "Angular velocity", Text(physics->GetAngularVelocity(runtime->Body)));
					}
				}

				if (const Constraint3DRuntime* runtime = registry.try_get<Constraint3DRuntime>(handle))
				{
					Section& section = AddSection(out, "Constraint", true);
					Add(section, "Constraint", runtime->Constraint == k_InvalidConstraint3D ? "none" : std::format("{:#x}", runtime->Constraint));
					const Physics3D* physics = data.Physics.Get3D();
					if (physics && physics->IsConstraintValid(runtime->Constraint))
					{
						const Constraint3DComponent* settings = registry.try_get<Constraint3DComponent>(handle);
						if (settings)
							Add(section, "Type", ConstraintTypeName(settings->Type));
						Add(section, "Other", runtime->Other != entt::null && registry.valid(runtime->Other) ? EntityName(registry, runtime->Other) : std::string("world"));
						Add(section, "Enabled", Text(physics->IsConstraintEnabled(runtime->Constraint)));
						if (settings && settings->Type == ConstraintType3D::Hinge)
							Add(section, "Value", Text(physics->GetConstraintValue(runtime->Constraint)) + " rad");
						else if (settings && (settings->Type == ConstraintType3D::Slider || settings->Type == ConstraintType3D::Distance))
							Add(section, "Value", Text(physics->GetConstraintValue(runtime->Constraint)) + " m");
					}
				}

				if (registry.all_of<CharacterController3DRuntime>(handle))
				{
					Section& section = AddSection(out, "Character controller", true);
					if (const CharacterController3D* controller = data.Physics.GetController(registry, handle))
					{
						Add(section, "Position", Text(controller->GetPosition()));
						Add(section, "Linear velocity", Text(controller->GetLinearVelocity()));
						Add(section, "Grounded", Text(controller->IsGrounded()));
						if (controller->IsGrounded())
							Add(section, "Ground normal", Text(controller->GetGroundNormal()));
					}
					else
						Add(section, "Controller", "none");
				}

				if (const AudioSourceRuntime* runtime = registry.try_get<AudioSourceRuntime>(handle))
				{
					Section& section = AddSection(out, "Sound", true);
					if (runtime->Sound == k_InvalidSound)
						Add(section, "Sound", "none");
					else
					{
						Add(section, "Sound", std::format("{:#x}", static_cast<uint32_t>(runtime->Sound)));
						Add(section, "Playing", Text(Application::Get().GetAudioEngine().IsPlaying(runtime->Sound)));
					}
				}

				if (const AnimatorRuntime* runtime = registry.try_get<AnimatorRuntime>(handle))
				{
					Section& section = AddSection(out, "Animator instance", true);
					const SkinnedMeshRendererComponent* skinned = registry.try_get<SkinnedMeshRendererComponent>(handle);
					const Skeleton* skeleton = skinned && skinned->Model ? skinned->Model->GetSkeleton() : nullptr;
					const bool bound = runtime->Instance && skeleton && runtime->SkeletonId == skeleton->GetId();
					Add(section, "Bound", Text(bound));
					if (bound)
					{
						const Animator& animator = *runtime->Instance;
						Add(section, "Layers", Text(animator.GetLayerCount()));
						for (uint32_t layer = 0; layer < animator.GetLayerCount(); ++layer)
						{
							const AnimationClip* clip = animator.GetCurrentClip(layer);
							Add(section, std::format("Layer {} clip", layer), clip ? clip->GetName() : "none");
						}
					}
				}

				if (const ParticleEmitterRuntime* runtime = registry.try_get<ParticleEmitterRuntime>(handle))
				{
					Section& section = AddSection(out, "Particle emitters", true);
					Add(section, "Instances", Text(static_cast<uint32_t>(runtime->Instances.size())));
					Add(section, "Pending bursts", Text(static_cast<uint32_t>(runtime->Bursts.size())));
				}

				if (const ScriptableEntity* script = data.Scripts.Find(handle))
				{
					Section& section = AddSection(out, "Script", true);
					Add(section, "Type", TypeName(typeid(*script)));
					Add(section, "Started", Text(data.Scripts.IsStarted(handle)));
				}

				return true;
			}

		}

	}

}
