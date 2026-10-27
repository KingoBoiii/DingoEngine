#include "depch.h"
#include "DingoEngine/Scene/Scene.h"
#include "DingoEngine/Scene/Entity.h"
#include "DingoEngine/Scene/ScriptableEntity.h"
#include "DingoEngine/Scene/Components.h"

#include "DingoEngine/Graphics/Renderer.h"
#include "DingoEngine/Graphics/Renderer2D.h"
#include "DingoEngine/Graphics/Renderer3D.h"
#include "DingoEngine/Graphics/Renderer3DAccess.h"
#include "DingoEngine/Graphics/Model.h"

#include "DingoEngine/Scene/SceneDebug.h"
#include "DingoEngine/Scene/SceneData.h"
#include "DingoEngine/Scene/Systems/AnimationSystem.h"
#include "DingoEngine/Scene/Systems/AudioSync.h"
#include "DingoEngine/Scene/Systems/CameraUtils.h"
#include "DingoEngine/Scene/Systems/HierarchySystem.h"
#include "DingoEngine/Scene/Systems/LightSystem.h"
#include "DingoEngine/Scene/Systems/ParticleSync.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>

namespace
{
	// The square of half-size `half` about `center` lies wholly beyond one side of the view, by more
	// than rounding could undo, so nothing in it can reach a pixel.
	bool QuadOutsideView(const glm::mat4& viewProjection, const glm::vec3& center, float half)
	{
		glm::vec4 clip[4];
		for (int corner = 0; corner < 4; ++corner)
		{
			const glm::vec3 point(center.x + ((corner & 1) ? half : -half), center.y + ((corner & 2) ? half : -half), center.z);
			clip[corner] = viewProjection * glm::vec4(point, 1.0f);
		}
		for (int axis = 0; axis < 2; ++axis)
		{
			for (const float side : { 1.0f, -1.0f })
			{
				bool outside = true;
				for (const glm::vec4& point : clip)
				{
					const float distance = point.w + side * point[axis];
					outside = outside && distance < -1e-5f * (std::abs(point.w) + std::abs(point[axis]));
				}
				if (outside)
					return true;
			}
		}
		return false;
	}
}

namespace Dingo
{

	Scene::Scene(const std::string& name)
		: m_Data(new Internal::SceneData()), m_Name(name)
	{
		static std::atomic<uint64_t> s_ProbeSalt = 0;
		m_Data->ShadowProbes.Salt = (++s_ProbeSalt) * 0x9e3779b97f4a7c15ull;
		Internal::AnimationSystem::Connect(m_Data->Registry, m_Data->AnimationEvents);
		Internal::ParticleSync::Connect(m_Data->Registry, m_Data->EntityMap);
		Internal::AudioSync::Connect(m_Data->Registry);
		m_Data->Physics.Connect(m_Data->Registry, m_Data->EntityMap);
	}

	Scene::~Scene()
	{
		Internal::SceneDebug::UnregisterScene(this);
		Clear();
		delete m_Data;
	}

	Entity Scene::Wrap(std::uint32_t handle)
	{
		return Entity(handle, this);
	}

	Entity Scene::CreateEntity(const std::string& name)
	{
		return CreateEntityWithUUID(UUID::Generate(), name);
	}

	Entity Scene::CreateEntityWithUUID(UUID uuid, const std::string& name)
	{
		if (m_Data->EntityMap.contains(uuid))
		{
			DE_CORE_WARN("Scene '{}': an entity with UUID {} already exists; '{}' was not created.", m_Name, static_cast<uint64_t>(uuid), name);
			return {};
		}

		entt::entity handle = m_Data->Registry.create();
		m_Data->Registry.emplace<IDComponent>(handle, uuid);
		m_Data->Registry.emplace<TransformComponent>(handle);
		m_Data->Registry.emplace<TagComponent>(handle, name.empty() ? std::string("Entity") : name);

		m_Data->EntityMap[uuid] = handle;
		return Wrap(static_cast<std::uint32_t>(handle));
	}

	namespace
	{
		// Copies component T from src to dst when the source has it. Used only by
		// DuplicateEntity below — keep that function's component list in sync with
		// Components.h (a new component type must be added there to be cloned).
		template<typename T>
		void CopyComponentIfExists(entt::registry& registry, entt::entity dst, entt::entity src)
		{
			if (registry.all_of<T>(src))
				registry.emplace_or_replace<T>(dst, registry.get<T>(src));
		}
	}

	Entity Scene::DuplicateEntity(Entity source)
	{
		if (!IsValid(source))
			return {};

		std::vector<std::pair<UUID, UUID>> copies;
		Entity clone = DuplicateSubtree(source, source.GetParent(), copies);

		// A copied ParticleEventComponent still names the source's emitters, and a Constraint3DComponent its source's other body; one inside the copied
		// subtree is the clone's own counterpart now. Bindings to emitters outside it stay.
		std::unordered_map<UUID, UUID> remap(copies.begin(), copies.end());
		entt::registry& registry = m_Data->Registry;
		for (const auto& [from, to] : copies)
		{
			const auto it = m_Data->EntityMap.find(to);
			if (it == m_Data->EntityMap.end())
				continue;
			if (ParticleEventComponent* events = registry.try_get<ParticleEventComponent>(it->second))
			{
				for (ParticleEventComponent::Binding& binding : events->Bindings)
				{
					const auto target = remap.find(binding.Emitter);
					if (target != remap.end())
						binding.Emitter = target->second;
				}
			}
			if (Constraint3DComponent* constraint = registry.try_get<Constraint3DComponent>(it->second))
			{
				const auto target = remap.find(constraint->Other);
				if (target != remap.end())
					constraint->Other = target->second;
			}
		}
		return clone;
	}

	Entity Scene::DuplicateSubtree(Entity source, Entity parent, std::vector<std::pair<UUID, UUID>>& copies)
	{
		entt::entity src = static_cast<entt::entity>(source.m_Handle);
		entt::registry& registry = m_Data->Registry;

		// New entity with its own fresh UUID, seeded with the source's name.
		const std::string name = registry.all_of<TagComponent>(src)
			? registry.get<TagComponent>(src).Tag
			: std::string();
		Entity clone = CreateEntity(name);
		entt::entity dst = static_cast<entt::entity>(clone.m_Handle);
		copies.emplace_back(source.GetUUID(), clone.GetUUID());

		// Copy every built-in component present on the source EXCEPT identity (the clone
		// keeps its fresh UUID) and the tag (already seeded above). The default
		// Transform/2D components CreateEntity added are overwritten via emplace_or_replace.
		// The engine's runtime components (RuntimeComponents.h) are deliberately absent, so
		// the clone never shares the source's body, controller or sound.
		//
		// MAINTENANCE: a new component type in Components.h must be added to this list to
		// be duplicated.
		CopyComponentIfExists<TransformComponent>(registry, dst, src);
		CopyComponentIfExists<SpriteRendererComponent>(registry, dst, src);
		CopyComponentIfExists<CircleRendererComponent>(registry, dst, src);
		CopyComponentIfExists<TextComponent>(registry, dst, src);
		CopyComponentIfExists<CameraComponent>(registry, dst, src);
		CopyComponentIfExists<PostProcessComponent>(registry, dst, src);
		CopyComponentIfExists<ParticleEmitterComponent>(registry, dst, src);
		CopyComponentIfExists<DirectionalLightComponent>(registry, dst, src);
		CopyComponentIfExists<AmbientLightComponent>(registry, dst, src);
		CopyComponentIfExists<PointLightComponent>(registry, dst, src);
		CopyComponentIfExists<SpotLightComponent>(registry, dst, src);
		CopyComponentIfExists<FogComponent>(registry, dst, src);
		CopyComponentIfExists<RigidBody2DComponent>(registry, dst, src);
		CopyComponentIfExists<BoxCollider2DComponent>(registry, dst, src);
		CopyComponentIfExists<CircleCollider2DComponent>(registry, dst, src);
		CopyComponentIfExists<Transform3DComponent>(registry, dst, src);
		CopyComponentIfExists<MeshRendererComponent>(registry, dst, src);
		CopyComponentIfExists<SkinnedMeshRendererComponent>(registry, dst, src);
		CopyComponentIfExists<AnimatorComponent>(registry, dst, src);
		CopyComponentIfExists<ParticleEventComponent>(registry, dst, src);
		CopyComponentIfExists<RigidBody3DComponent>(registry, dst, src);
		CopyComponentIfExists<BoxCollider3DComponent>(registry, dst, src);
		CopyComponentIfExists<SphereCollider3DComponent>(registry, dst, src);
		CopyComponentIfExists<CapsuleCollider3DComponent>(registry, dst, src);
		CopyComponentIfExists<MeshCollider3DComponent>(registry, dst, src);
		CopyComponentIfExists<Constraint3DComponent>(registry, dst, src);
		CopyComponentIfExists<CharacterController3DComponent>(registry, dst, src);
		CopyComponentIfExists<AudioSourceComponent>(registry, dst, src);
		CopyComponentIfExists<AudioListenerComponent>(registry, dst, src);

		// Linked before the body is built, which reads the world transform. A socket names a joint
		// of the parent's model, which the copied parent has too.
		if (parent)
		{
			Internal::HierarchySystem::Link(registry, dst, static_cast<entt::entity>(parent.m_Handle));
			CopyComponentIfExists<Internal::SocketComponent>(registry, dst, src);
		}

		if (registry.all_of<Internal::InactiveSelfComponent>(src))
			registry.emplace<Internal::InactiveSelfComponent>(dst);
		if (registry.all_of<Internal::InactiveSelfComponent>(dst) || (parent && Internal::HierarchySystem::IsInactive(registry, static_cast<entt::entity>(parent.m_Handle))))
			registry.emplace<Internal::InactiveComponent>(dst);

		// If the world is already simulating, give the clone its own body now (mirrors a
		// runtime CreateEntity + CreateRigidBody spawn); otherwise OnStart will.
		if (IsPhysicsRunning())
			CreateRigidBody(clone);

		for (Entity child : source.GetChildren())
			DuplicateSubtree(child, clone, copies);

		return clone;
	}

	void Scene::DestroyEntity(Entity entity)
	{
		if (!IsValid(entity))
			return;

		if (m_Data->Updating)
		{
			m_Data->PendingDestroy.push_back(static_cast<entt::entity>(entity.m_Handle));
			return;
		}

		DestroyEntityNow(entity.m_Handle);
	}

	void Scene::DetachScript(std::uint32_t handle)
	{
		m_Data->Scripts.Detach(static_cast<entt::entity>(handle));
		m_Data->Scripts.FreeDetached();
	}

	void Scene::DestroyEntityNow(std::uint32_t handle)
	{
		entt::entity e = static_cast<entt::entity>(handle);
		if (!m_Data->Registry.valid(e))
			return;

		// False once a script's OnDestroy has destroyed this entity along the way.
		auto destroyChildren = [this, e]()
		{
			while (const Internal::HierarchyComponent* node = m_Data->Registry.try_get<Internal::HierarchyComponent>(e))
			{
				if (node->FirstChild == entt::null)
					break;

				DestroyEntityNow(static_cast<std::uint32_t>(node->FirstChild));
				if (!m_Data->Registry.valid(e))
					return false;
			}
			return true;
		};

		// Children go first, and again after this entity's own OnDestroy, which may have given it new ones.
		if (!destroyChildren())
			return;

		DetachScript(handle);
		if (!m_Data->Registry.valid(e) || !destroyChildren())
			return;

		if (m_Data->Registry.all_of<IDComponent>(e))
			m_Data->EntityMap.erase(m_Data->Registry.get<IDComponent>(e).ID);

		m_Data->Physics.DestroyBodiesForEntity(m_Data->Registry, e);

		// Stop the entity's live sound so it doesn't keep playing after its entity
		// (and transform) is gone.
		Internal::AudioSync::StopSource(m_Data->Registry, e);

		Internal::HierarchySystem::Unlink(m_Data->Registry, e);
		m_Data->Registry.destroy(e);
	}

	bool Scene::IsValid(Entity entity) const
	{
		return entity.m_Scene == this
			&& m_Data->Registry.valid(static_cast<entt::entity>(entity.m_Handle));
	}

	void Scene::Clear()
	{
		// Drop the physics world first so its bodies don't outlive the entities, and
		// mark the scene stopped — otherwise a later OnStart() would early-return on a
		// stale running flag and never rebuild physics.
		m_IsRunning = false;
		StopPhysics();

		Internal::AudioSync::StopAllSources(m_Data->Registry);

		m_Data->Scripts.DetachAll();
		m_Data->Scripts.FreeDetached();

		m_Data->Registry.clear();
		m_Data->EntityMap.clear();
		m_Data->PendingDestroy.clear();

		ClearPendingSceneTransition();
	}

	Entity Scene::GetEntityByUUID(UUID uuid)
	{
		auto it = m_Data->EntityMap.find(uuid);
		if (it != m_Data->EntityMap.end())
			return Wrap(static_cast<std::uint32_t>(it->second));

		return {};
	}

	void Scene::OnUpdate(float deltaTime)
	{
		// A stall (a cold shader compile, a breakpoint) is not simulated in one go: a longer step
		// tunnels bodies through their colliders. Scripts get the same capped delta so their
		// kinematic moves and controller velocities match what physics simulates; the scene
		// runs slow through the stall instead.
		deltaTime = std::min(deltaTime, Internal::PhysicsSync::k_MaxStepTime);
		DE_PROFILE_SCOPE_TEXT("Scene::OnUpdate", m_Name);

		if (!m_Data->DebugRegistered)
		{
			Internal::SceneDebug::RegisterScene(this, m_Data);
			m_Data->DebugRegistered = true;
		}

		{
			DE_PROFILE_SCOPE("ScriptSystem::Update");
			m_Data->Updating = true;
			m_Data->Scripts.StartPending();

			const float step = m_Data->FixedTimeStep;
			float& carried = m_Data->FixedTimeAccumulator;
			carried += deltaTime;
			uint32_t steps = 0;
			while (carried >= step && steps < k_MaxFixedStepsPerUpdate)
			{
				carried -= step;
				++steps;
				m_Data->Scripts.FixedUpdate(step);
			}
			if (carried >= step)
				carried = std::fmod(carried, step);

			m_Data->Scripts.Update(deltaTime);
			m_Data->Updating = false;
		}

		for (entt::entity handle : m_Data->PendingDestroy)
			DestroyEntityNow(static_cast<std::uint32_t>(handle));
		m_Data->PendingDestroy.clear();

		// Animation events reach scripts here, so their destroys wait as theirs do in OnUpdate.
		{
			DE_PROFILE_SCOPE("AnimationSystem::Update");
			m_Data->Updating = true;
			// A scene no longer rendered (stepped headless, behind a menu) animates as if seen.
			const bool rendered = m_Data->SkinnedViewFrame.RenderedLately(Renderer::GetFrameIndex());
			Internal::AnimationSystem::Update(m_Data->Registry, m_Data->Scripts, m_Data->Physics, m_Data->AnimationEvents, deltaTime,
				rendered ? std::span<const std::weak_ptr<const Internal::SkinnedVisibility>>(m_Data->SkinnedViews) : std::span<const std::weak_ptr<const Internal::SkinnedVisibility>>());
			Internal::ParticleSync::ApplyAnimationEvents(m_Data->Registry, m_Data->EntityMap, m_Data->AnimationEvents.ParticleEvents);
			Internal::ParticleSync::Update(m_Data->Registry, deltaTime);
			m_Data->Updating = false;
		}

		for (entt::entity handle : m_Data->PendingDestroy)
			DestroyEntityNow(static_cast<std::uint32_t>(handle));
		m_Data->PendingDestroy.clear();

		// Step physics after the script pass (scripts may have applied forces this
		// frame), then write the simulated transforms back onto the entities.
		{
			DE_PROFILE_SCOPE("PhysicsSync::Step");
			m_Data->Physics.Step(m_Data->Registry, deltaTime);
		}

		// Transforms are final for the frame now (physics + controller write-back
		// already happened), so sync every spatialized source's position and the
		// listener before anything renders or is heard this frame.
		m_Data->Scripts.FreeDetached();

		DE_PROFILE_SCOPE("AudioSync");
		Internal::AudioSync::SyncListenerAndSources(m_Data->Registry, m_Data->Memo);
	}

	void Scene::SetFixedTimeStep(float seconds)
	{
		if (!std::isfinite(seconds))
			return;

		m_Data->FixedTimeStep = std::clamp(seconds, 1.0f / 1000.0f, 0.25f);
		m_Data->FixedTimeAccumulator = std::min(m_Data->FixedTimeAccumulator, m_Data->FixedTimeStep);
	}

	float Scene::GetFixedTimeStep() const
	{
		return m_Data->FixedTimeStep;
	}

	float Scene::GetFixedTimeAlpha() const
	{
		return std::clamp(m_Data->FixedTimeAccumulator / m_Data->FixedTimeStep, 0.0f, 0.999999f);
	}

	void Scene::RefreshActive(std::uint32_t root)
	{
		entt::registry& registry = m_Data->Registry;
		std::vector<entt::entity> activated, deactivated;
		Internal::HierarchySystem::RefreshActive(registry, static_cast<entt::entity>(root), activated, deactivated);

		for (entt::entity handle : deactivated)
		{
			if (IsPhysicsRunning())
				m_Data->Physics.DestroyBodiesForEntity(registry, handle);
			Internal::AudioSync::StopSource(registry, handle);
		}

		if (!activated.empty())
			m_Data->Scripts.MarkPending();

		// Parents first, so a child's body is built against its parent's.
		for (entt::entity handle : activated)
		{
			if (!registry.valid(handle) || Internal::HierarchySystem::IsInactive(registry, handle))
				continue;

			if (IsPhysicsRunning())
				CreateRigidBody(Wrap(static_cast<std::uint32_t>(handle)));

			const AudioSourceComponent* source = registry.try_get<AudioSourceComponent>(handle);
			if (m_IsRunning && source && source->PlayOnStart && source->Clip)
				Internal::AudioSync::PlaySource(registry, handle);
		}
	}

	Animator* Scene::GetAnimator(Entity entity)
	{
		if (!IsValid(entity))
			return nullptr;

		return Internal::AnimationSystem::GetAnimator(m_Data->Registry, static_cast<entt::entity>(entity.m_Handle));
	}

	void Scene::ForEachEntity(const std::function<void(Entity)>& fn)
	{
		auto view = m_Data->Registry.view<IDComponent>();
		for (entt::entity handle : view)
			fn(Wrap(static_cast<std::uint32_t>(handle)));
	}

	void Scene::ForEachScript(const std::function<void(ScriptableEntity*)>& fn)
	{
		m_Data->Scripts.ForEach(fn);
	}

	void Scene::StartScripts()
	{
		m_Data->Scripts.StartPending();
	}

	void Scene::RenderEntities(Renderer2D& renderer)
	{
		DE_PROFILE_SCOPE("Scene::RenderEntities");
		Internal::HierarchySystem::WorldMemo& memo = m_Data->Memo;
		memo.Begin(m_Data->Registry);

		// Sprites, circles and text, painter-sorted by z in one stream: a higher world z draws on
		// top. At equal z sprites come first, then circles, then text, as when each kind drew in a
		// pass of its own, and within a kind a parent draws before its children. Other equal-z ties
		// are NOT ordered by creation (the view order decides), so give overlapping UI elements
		// distinct z values.
		using Internal::SceneData;
		const auto inactive = entt::exclude<Internal::InactiveComponent>;
		auto sprites = m_Data->Registry.view<TransformComponent, SpriteRendererComponent>(inactive);
		auto circles = m_Data->Registry.view<TransformComponent, CircleRendererComponent>(inactive);
		auto texts = m_Data->Registry.view<TransformComponent, TextComponent>(inactive);

		// The world pose is resolved once per entity and kept with its sort key, so the sort goes
		// back to the registry for nothing. The buffer is a member so a steady-state frame does no
		// allocation at all.
		std::vector<SceneData::Draw2D>& draws = m_Data->Draw2DSortBuffer;
		draws.clear();
		auto collect = [&](entt::entity entity, const TransformComponent& transform, SceneData::Draw2DKind kind)
		{
			SceneData::Draw2D& draw = draws.emplace_back();
			draw.Entity = entity;
			draw.Kind = kind;
			memo.Pose2D(entity, transform, draw.Position, draw.Rotation);
			draw.Depth = memo.Depth(entity);
		};

		// A sprite or circle is a quad of Size about its position, turned by its rotation, so the square
		// around its circumcircle holds it at any angle. Removing a quad that draws nothing leaves the
		// rest in the same order, so the frame is the same.
		const bool culling = renderer.IsCullingEnabled() && !renderer.m_SceneSkipped;
		const glm::mat4& viewProjection = renderer.GetViewProjection();
		uint32_t culled = 0;
		auto collectQuad = [&](entt::entity entity, const TransformComponent& transform, SceneData::Draw2DKind kind)
		{
			collect(entity, transform, kind);
			if (!culling)
				return;
			const SceneData::Draw2D& draw = draws.back();
			const float half = 0.5f * glm::length(transform.Size);
			if (QuadOutsideView(viewProjection, draw.Position, half))
			{
				draws.pop_back();
				++culled;
			}
		};
		for (entt::entity entity : sprites)
			collectQuad(entity, sprites.get<TransformComponent>(entity), SceneData::Draw2DKind::Sprite);
		for (entt::entity entity : circles)
			collectQuad(entity, circles.get<TransformComponent>(entity), SceneData::Draw2DKind::Circle);
		renderer.m_Statistics.CulledQuads += culled;
		for (entt::entity entity : texts)
		{
			const TextComponent& text = texts.get<TextComponent>(entity);
			if (text.Font && !text.Text.empty())
				collect(entity, texts.get<TransformComponent>(entity), SceneData::Draw2DKind::Text);
		}

		std::stable_sort(draws.begin(), draws.end(), [](const SceneData::Draw2D& a, const SceneData::Draw2D& b)
		{
			if (a.Position.z != b.Position.z)
				return a.Position.z < b.Position.z;
			return a.Kind != b.Kind ? a.Kind < b.Kind : a.Depth < b.Depth;
		});

		for (size_t i = 0; i < draws.size(); ++i)
		{
			const SceneData::Draw2D& draw = draws[i];
			// Each kind batches on its own, so a change of kind has to draw what came before it.
			if (i > 0 && draws[i - 1].Kind != draw.Kind)
				renderer.Flush();

			switch (draw.Kind)
			{
				case SceneData::Draw2DKind::Sprite:
				{
					auto [transform, sprite] = sprites.get<TransformComponent, SpriteRendererComponent>(draw.Entity);
					Texture* texture = sprite.Texture ? sprite.Texture : Renderer::GetWhiteTexture();

					if (draw.Rotation != 0.0f)
						renderer.DrawRotatedQuad(draw.Position, draw.Rotation, transform.Size, texture, sprite.Color);
					else
						renderer.DrawQuad(draw.Position, transform.Size, texture, sprite.Color);
					break;
				}
				case SceneData::Draw2DKind::Circle:
				{
					auto [transform, circle] = circles.get<TransformComponent, CircleRendererComponent>(draw.Entity);
					renderer.DrawCircle(memo.Transform2D(draw.Entity, transform), circle.Color, circle.Thickness, circle.Fade);
					break;
				}
				case SceneData::Draw2DKind::Text:
				{
					const TextComponent& text = texts.get<TextComponent>(draw.Entity);
					renderer.DrawText(text.Text, text.Font, draw.Position, text.Size, { .Color = text.Color, .Centered = text.Centered, .Rotation = draw.Rotation });
					break;
				}
			}
		}
	}

	namespace
	{
		void TrackSkinnedView(Internal::SceneData& data, Renderer3D& renderer)
		{
			std::shared_ptr<const Internal::SkinnedVisibility> visibility = renderer.GetSkinnedVisibility();
			data.SkinnedViewFrame.Stamp(Renderer::GetFrameIndex(), Renderer::IsFrameSkipped());
			std::erase_if(data.SkinnedViews, [](const std::weak_ptr<const Internal::SkinnedVisibility>& view) { return view.expired(); });
			for (const std::weak_ptr<const Internal::SkinnedVisibility>& view : data.SkinnedViews)
			{
				if (view.lock() == visibility)
					return;
			}
			data.SkinnedViews.push_back(visibility);
		}

		template<typename T>
		bool SameBits(const T& a, const T& b)
		{
			return std::memcmp(&a, &b, sizeof(T)) == 0;
		}

		const Internal::SceneData::MeshRenderMemo& RenderMemo(std::vector<Internal::SceneData::MeshRenderMemo>& entries, Internal::HierarchySystem::WorldMemo& memo,
			entt::entity entity, const Transform3DComponent& transform, const Mesh& mesh)
		{
			const std::size_t index = static_cast<std::size_t>(entt::to_entity(entity));
			if (index >= entries.size())
				entries.resize(index + 1);
			Internal::SceneData::MeshRenderMemo& entry = entries[index];

			const bool root = !memo.HasParent(entity);
			if (!root || !entry.Root || !SameBits(entry.Position, transform.Position) || !SameBits(entry.Rotation, transform.Rotation) || !SameBits(entry.Scale, transform.Scale))
			{
				const glm::mat4 world = memo.Transform(entity, transform);
				if (!SameBits(entry.World, world))
				{
					entry.World = world;
					entry.HasBounds = false;
				}
				entry.Root = root;
				entry.Position = transform.Position;
				entry.Rotation = transform.Rotation;
				entry.Scale = transform.Scale;
			}
			if (!entry.HasBounds || !SameBits(entry.MeshMin, mesh.GetBoundsMin()) || !SameBits(entry.MeshMax, mesh.GetBoundsMax()))
			{
				entry.MeshMin = mesh.GetBoundsMin();
				entry.MeshMax = mesh.GetBoundsMax();
				entry.Bounded = Internal::Renderer3DAccess::WorldBounds(mesh, entry.World, entry.Bounds);
				entry.HasBounds = true;
			}
			return entry;
		}
	}

	void Scene::RenderEntities3D(Renderer3D& renderer)
	{
		DE_PROFILE_SCOPE("Scene::RenderEntities3D");
		Internal::HierarchySystem::WorldMemo& memo = m_Data->Memo;
		memo.Begin(m_Data->Registry);
		TrackSkinnedView(*m_Data, renderer);

		Internal::StaticGeometry::Bake* bake = m_Data->StaticMeshes.Find(renderer);
		entt::registry& registry = m_Data->Registry;
		const auto& transforms = registry.storage<Transform3DComponent>();
		auto& meshes = registry.storage<MeshRendererComponent>();
		const auto& inactive = registry.storage<Internal::InactiveComponent>();
		const bool anyInactive = !inactive.empty();
		// Walks the view's own lead pool, so submission order, and with it batching, stays the view's.
		const entt::sparse_set& lead = *registry.view<Transform3DComponent, MeshRendererComponent>(entt::exclude<Internal::InactiveComponent>).handle();
		const entt::sparse_set& other = &lead == &meshes ? static_cast<const entt::sparse_set&>(transforms) : meshes;
		for (const entt::entity entity : lead)
		{
			if (!other.contains(entity) || (anyInactive && inactive.contains(entity)))
				continue;
			const Transform3DComponent& transform = transforms.get(entity);
			MeshRendererComponent& mesh = meshes.get(entity);
			if (!mesh.Visible || !mesh.Mesh)
				continue;

			if (mesh.Static && !(mesh.CullDistance > 0.0f))
			{
				if (!bake)
					bake = &m_Data->StaticMeshes.Create(renderer);
				StaticMesh baked = StaticMesh().SetMesh(mesh.Mesh).SetTransform(memo.Transform(entity, transform)).SetColor(mesh.Color)
					.SetMaterial(mesh.Material).SetShadows(mesh.Shadows).SetShadowGroups(mesh.ShadowGroups);
				baked.Surface = mesh.Surface;
				m_Data->StaticMeshes.Track(*bake, entity, baked);
				continue;
			}
			const Internal::SceneData::MeshRenderMemo& entry = RenderMemo(m_Data->MeshMemo, memo, entity, transform, *mesh.Mesh);
			Internal::Renderer3DAccess::SubmitMesh(renderer, mesh.Mesh, entry.World, mesh.Color, mesh.Material, mesh.Shadows, mesh.ShadowGroups, mesh.CullDistance,
				mesh.Surface ? &*mesh.Surface : nullptr, entry.Bounds, entry.Bounded);
		}
		if (bake)
			m_Data->StaticMeshes.Submit(*bake, renderer);

		auto skinnedView = m_Data->Registry.view<Transform3DComponent, SkinnedMeshRendererComponent>(entt::exclude<Internal::InactiveComponent>);
		for (entt::entity entity : skinnedView)
		{
			auto [transform, skinned] = skinnedView.get<Transform3DComponent, SkinnedMeshRendererComponent>(entity);
			if (!skinned.Visible || !skinned.Model)
				continue;

			const glm::mat4 world = memo.Transform(entity, transform);
			const Skeleton* skeleton = skinned.Model->GetSkeleton();
			const std::span<const glm::mat4> palette = skeleton ? Internal::AnimationSystem::Palette(m_Data->Registry, entity, *skeleton) : std::span<const glm::mat4>();
			for (const SubMesh& submesh : skinned.Model->GetSubMeshes())
			{
				if (skeleton && submesh.Mesh->HasSkin())
					renderer.SubmitSkinnedMesh(submesh.Mesh, world, palette, skinned.Color, skinned.Material, skinned.Shadows, skinned.ShadowGroups, skinned.CullDistance);
				else
					renderer.SubmitMesh(submesh.Mesh, world, skinned.Color, skinned.Material, skinned.Shadows, skinned.ShadowGroups, skinned.CullDistance);
			}
		}

		Internal::ParticleSync::Submit(m_Data->Registry, renderer, memo);
	}

	void Scene::EmitParticles(Entity entity, uint32_t count)
	{
		if (IsValid(entity))
			Internal::ParticleSync::Emit(m_Data->Registry, static_cast<entt::entity>(entity.m_Handle), count, nullptr);
	}

	void Scene::EmitParticlesAt(Entity entity, const glm::vec3& worldPosition, uint32_t count)
	{
		if (IsValid(entity))
			Internal::ParticleSync::Emit(m_Data->Registry, static_cast<entt::entity>(entity.m_Handle), count, &worldPosition);
	}

	ParticleEmitter* Scene::GetParticleEmitter(Entity entity)
	{
		return IsValid(entity) ? Internal::ParticleSync::GetEmitter(m_Data->Registry, static_cast<entt::entity>(entity.m_Handle)) : nullptr;
	}

	void Scene::SubmitLights(Renderer3D& renderer, bool shadowProbes)
	{
		SubmitLights(renderer, Entity(), shadowProbes);
	}

	void Scene::SubmitLights(Renderer3D& renderer, Entity camera, bool shadowProbes)
	{
		const entt::entity cameraHandle = IsValid(camera) ? static_cast<entt::entity>(camera.m_Handle) : entt::null;
		Internal::LightSystem::SubmitLights(m_Data->Registry, renderer, m_Data->Memo, glm::vec3(m_ClearColor), shadowProbes ? &m_Data->ShadowProbes : nullptr, cameraHandle);
	}

	// --- Camera -----------------------------------------------------------------

	Entity Scene::GetPrimaryCamera()
	{
		entt::entity handle = entt::null;
		if (!Internal::CameraUtils::FindPrimaryCamera(m_Data->Registry, handle))
			return {};

		return Wrap(static_cast<std::uint32_t>(handle));
	}

	void Scene::GetRenderCameras(Entity& outPerspective, bool& outHasPerspective, Entity& outOrthographic, bool& outHasOrthographic)
	{
		entt::entity perspective = entt::null, orthographic = entt::null;
		Internal::CameraUtils::FindRenderCameras(m_Data->Registry, perspective, outHasPerspective, orthographic, outHasOrthographic);

		if (outHasPerspective)
			outPerspective = Wrap(static_cast<std::uint32_t>(perspective));
		if (outHasOrthographic)
			outOrthographic = Wrap(static_cast<std::uint32_t>(orthographic));
	}

	float Scene::GetLightVisibility(Entity light, const glm::vec3& point, uint32_t key, float clearance)
	{
		if (!IsValid(light))
			return 1.0f;

		const entt::entity handle = static_cast<entt::entity>(light.m_Handle);
		// Not submitted, so nothing would ever replace its last answer.
		if (Internal::HierarchySystem::IsInactive(m_Data->Registry, handle))
			return 1.0f;

		Internal::LightSystem::ShadowProbeState& probes = m_Data->ShadowProbes;
		const uint64_t localKey = Internal::LightSystem::ShadowProbeState::LocalKey(handle, key);
		if (probes.Pending.size() < Renderer3D::k_MaxShadowProbes || probes.Pending.contains(localKey))
			probes.Pending[localKey] = { handle, point, clearance };

		const auto answer = probes.Answers.find(localKey);
		return answer != probes.Answers.end() ? answer->second.Value : 1.0f;
	}

	float Scene::GetShadowedLightAttenuation(Entity light, const glm::vec3& point, uint32_t key, float clearance)
	{
		if (!IsValid(light))
			return 0.0f;

		const entt::registry& registry = m_Data->Registry;
		const entt::entity handle = static_cast<entt::entity>(light.m_Handle);
		float attenuation = 0.0f;
		if (Internal::HierarchySystem::IsInactive(registry, handle))
			attenuation = 0.0f;
		else if (registry.all_of<DirectionalLightComponent>(handle))
		{
			attenuation = 1.0f;
		}
		else if (const PointLightComponent* pointLight = registry.try_get<PointLightComponent>(handle))
		{
			if (pointLight->Enabled && registry.all_of<Transform3DComponent>(handle))
				attenuation = GetLightAttenuation(pointLight->ToLight(Transform3DComponent(light.GetWorldPosition())), point);
		}
		else if (const SpotLightComponent* spotLight = registry.try_get<SpotLightComponent>(handle))
		{
			if (spotLight->Enabled && registry.all_of<Transform3DComponent>(handle))
			{
				Transform3DComponent world(light.GetWorldPosition());
				world.Rotation = light.GetWorldRotation();
				attenuation = GetLightAttenuation(spotLight->ToLight(world), point);
			}
		}

		return attenuation > 0.0f ? attenuation * GetLightVisibility(light, point, key, clearance) : 0.0f;
	}

	glm::mat4 Scene::GetCameraViewProjection(Entity cameraEntity, float aspect)
	{
		if (!IsValid(cameraEntity))
			return glm::mat4(1.0f);

		return Internal::CameraUtils::ViewProjection(m_Data->Registry, static_cast<entt::entity>(cameraEntity.m_Handle), aspect);
	}

	glm::mat4 Scene::GetActiveCameraViewProjection(float aspect)
	{
		const Entity camera = GetPrimaryCamera();
		return camera ? GetCameraViewProjection(camera, aspect) : glm::mat4(1.0f);
	}

	Ray Scene::ScreenPointToRay(const glm::vec2& screenPos, const glm::vec2& viewportSize)
	{
		return Internal::CameraUtils::ScreenPointToRay(m_Data->Registry, screenPos, viewportSize);
	}

	std::optional<glm::vec2> Scene::WorldToScreenPoint(const glm::vec3& worldPos, const glm::vec2& viewportSize) const
	{
		return Internal::CameraUtils::WorldToScreenPoint(m_Data->Registry, worldPos, viewportSize);
	}

	// --- Lifecycle ----------------------------------------------------------------

	void Scene::OnStart()
	{
		if (m_IsRunning)
			return;

		m_IsRunning = true;
		m_Data->FixedTimeAccumulator = 0.0f;

		// Discard any transition requested before this run (or left over from a
		// previous run) so a stale request can't fire the instant this scene reactivates.
		ClearPendingSceneTransition();

		// Run script OnStart before physics so a controller script can build the world
		// (spawn entities) and have physics bake bodies for everything it created.
		StartScripts();
		StartPhysics();

		// Start every PlayOnStart source now that scripts have had a chance to set up
		// (and, for a spatialized source, after any script-driven initial placement).
		auto sourceView = m_Data->Registry.view<AudioSourceComponent>(entt::exclude<Internal::InactiveComponent>);
		for (entt::entity handle : sourceView)
		{
			AudioSourceComponent& source = sourceView.get<AudioSourceComponent>(handle);
			if (source.PlayOnStart && source.Clip)
				Internal::AudioSync::PlaySource(m_Data->Registry, handle);
		}
	}

	void Scene::OnStop()
	{
		if (!m_IsRunning)
			return;

		m_IsRunning = false;
		StopPhysics();

		// Stop every source's live sound so the scene never leaks playing audio past
		// its own teardown (e.g. into whatever scene the SceneManager switches to).
		Internal::AudioSync::StopAllSources(m_Data->Registry);
		m_Data->Scripts.FreeDetached();

		// Drop any request left pending when this scene stopped being active, so it
		// can't be replayed the next time this scene starts.
		ClearPendingSceneTransition();
	}

	// --- Physics ------------------------------------------------------------------

	void Scene::SetGravity(const glm::vec2& gravity)
	{
		m_Gravity = gravity;
		m_Data->Physics.SetGravity(gravity);
	}

	void Scene::SetGravity(const glm::vec3& gravity)
	{
		m_Gravity3D = gravity;
		m_Data->Physics.SetGravity(gravity);
	}

	void Scene::OnPhysicsStart()
	{
		StartPhysics();
	}

	void Scene::OnPhysicsStop()
	{
		StopPhysics();
	}

	void Scene::StartPhysics()
	{
		Internal::AnimationSystem::EnsureAnimators(m_Data->Registry);
		m_Data->Physics.Start(m_Data->Registry, m_Gravity, m_Gravity3D);
	}

	void Scene::StopPhysics()
	{
		m_Data->Physics.Stop(m_Data->Registry);
	}

	bool Scene::IsPhysicsRunning() const
	{
		return m_Data->Physics.IsRunning();
	}

	Physics2D* Scene::GetPhysics2D() const
	{
		return m_Data->Physics.Get2D();
	}

	Physics3D* Scene::GetPhysics3D() const
	{
		return m_Data->Physics.Get3D();
	}

	void Scene::CreateRigidBody(Entity entity)
	{
		if (!IsValid(entity))
			return;

		Internal::AnimationSystem::EnsureAncestorAnimators(m_Data->Registry, static_cast<entt::entity>(entity.m_Handle));
		m_Data->Physics.CreateBodiesForEntity(m_Data->Registry, static_cast<entt::entity>(entity.m_Handle));
	}

	CharacterController3D* Scene::GetCharacterController(Entity entity) const
	{
		if (!IsValid(entity))
			return nullptr;

		return m_Data->Physics.GetController(m_Data->Registry, static_cast<entt::entity>(entity.m_Handle));
	}

	PhysicsBodyId2D Scene::GetRuntimeBody2D(Entity entity) const
	{
		if (!IsValid(entity))
			return k_InvalidBody2D;

		return m_Data->Physics.RuntimeBody2D(m_Data->Registry, static_cast<entt::entity>(entity.m_Handle));
	}

	PhysicsBodyId3D Scene::GetRuntimeBody3D(Entity entity) const
	{
		if (!IsValid(entity))
			return k_InvalidBody3D;

		return m_Data->Physics.RuntimeBody3D(m_Data->Registry, static_cast<entt::entity>(entity.m_Handle));
	}

	Entity Scene::GetEntityFromBody3D(PhysicsBodyId3D body)
	{
		const entt::entity handle = m_Data->Physics.EntityOfBody3D(m_Data->Registry, body);
		return handle == entt::null ? Entity() : Wrap(static_cast<std::uint32_t>(handle));
	}

	bool Scene::GetSensorOverlaps(Entity sensor, std::vector<Entity>& out)
	{
		out.clear();
		if (!IsValid(sensor))
			return false;

		std::vector<entt::entity> handles;
		m_Data->Physics.SensorOverlaps3D(m_Data->Registry, static_cast<entt::entity>(sensor.m_Handle), handles);
		for (const entt::entity handle : handles)
			out.push_back(Wrap(static_cast<std::uint32_t>(handle)));
		return !out.empty();
	}

	void Scene::SetLinearVelocity(Entity entity, const glm::vec2& velocity)
	{
		if (Physics2D* physics = m_Data->Physics.Get2D())
			physics->SetLinearVelocity(GetRuntimeBody2D(entity), velocity);
	}

	glm::vec2 Scene::GetLinearVelocity2D(Entity entity)
	{
		Physics2D* physics = m_Data->Physics.Get2D();
		if (!physics)
			return glm::vec2(0.0f);

		return physics->GetLinearVelocity(GetRuntimeBody2D(entity));
	}

	void Scene::ApplyLinearImpulse(Entity entity, const glm::vec2& impulse, const glm::vec2& worldPoint, bool wake)
	{
		if (Physics2D* physics = m_Data->Physics.Get2D())
			physics->ApplyLinearImpulse(GetRuntimeBody2D(entity), impulse, worldPoint, wake);
	}

	void Scene::ApplyLinearImpulseToCenter(Entity entity, const glm::vec2& impulse, bool wake)
	{
		if (Physics2D* physics = m_Data->Physics.Get2D())
			physics->ApplyLinearImpulseToCenter(GetRuntimeBody2D(entity), impulse, wake);
	}

	void Scene::ApplyForceToCenter(Entity entity, const glm::vec2& force, bool wake)
	{
		if (Physics2D* physics = m_Data->Physics.Get2D())
			physics->ApplyForceToCenter(GetRuntimeBody2D(entity), force, wake);
	}

	void Scene::SetLinearVelocity(Entity entity, const glm::vec3& velocity)
	{
		if (Physics3D* physics = m_Data->Physics.Get3D())
			physics->SetLinearVelocity(GetRuntimeBody3D(entity), velocity);
	}

	glm::vec3 Scene::GetLinearVelocity3D(Entity entity)
	{
		Physics3D* physics = m_Data->Physics.Get3D();
		if (!physics)
			return glm::vec3(0.0f);

		return physics->GetLinearVelocity(GetRuntimeBody3D(entity));
	}

	void Scene::ApplyImpulse(Entity entity, const glm::vec3& impulse)
	{
		if (Physics3D* physics = m_Data->Physics.Get3D())
			physics->ApplyImpulse(GetRuntimeBody3D(entity), impulse);
	}

	void Scene::ApplyForce(Entity entity, const glm::vec3& force)
	{
		if (Physics3D* physics = m_Data->Physics.Get3D())
			physics->ApplyForce(GetRuntimeBody3D(entity), force);
	}

	// --- Audio --------------------------------------------------------------------

	void Scene::PlayAudioSource(Entity entity)
	{
		if (!IsValid(entity))
			return;

		if (Internal::HierarchySystem::IsInactive(m_Data->Registry, static_cast<entt::entity>(entity.m_Handle)))
		{
			DE_CORE_WARN("PlayAudioSource: '{}' is inactive, so its source doesn't play", entity.GetName());
			return;
		}

		Internal::AudioSync::PlaySource(m_Data->Registry, static_cast<entt::entity>(entity.m_Handle));
	}

	void Scene::StopAudioSource(Entity entity)
	{
		if (!IsValid(entity))
			return;

		Internal::AudioSync::StopSource(m_Data->Registry, static_cast<entt::entity>(entity.m_Handle));
	}

	AudioSoundId Scene::GetRuntimeSound(Entity entity) const
	{
		if (!IsValid(entity))
			return k_InvalidSound;

		return Internal::AudioSync::RuntimeSound(m_Data->Registry, static_cast<entt::entity>(entity.m_Handle));
	}

}
