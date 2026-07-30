#include "depch.h"
#include "DingoEngine/Scene/Scene.h"
#include "DingoEngine/Scene/Entity.h"
#include "DingoEngine/Scene/ScriptableEntity.h"
#include "DingoEngine/Scene/Components.h"

#include "DingoEngine/Graphics/Renderer.h"
#include "DingoEngine/Graphics/Renderer2D.h"
#include "DingoEngine/Graphics/Renderer3D.h"

#include "DingoEngine/Scene/SceneData.h"
#include "DingoEngine/Scene/Systems/AudioSync.h"
#include "DingoEngine/Scene/Systems/CameraUtils.h"

#include <algorithm>

namespace Dingo
{

	Scene::Scene(const std::string& name)
		: m_Data(new Internal::SceneData()), m_Name(name)
	{
	}

	Scene::~Scene()
	{
		Clear();
		delete m_Data;
	}

	Entity Scene::Wrap(std::uint32_t handle)
	{
		return Entity(handle, this);
	}

	Entity Scene::CreateEntity(const std::string& name)
	{
		return CreateEntityWithUUID(UUID(), name);
	}

	Entity Scene::CreateEntityWithUUID(UUID uuid, const std::string& name)
	{
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

		entt::entity src = static_cast<entt::entity>(source.m_Handle);
		entt::registry& registry = m_Data->Registry;

		// New entity with its own fresh UUID, seeded with the source's name.
		const std::string name = registry.all_of<TagComponent>(src)
			? registry.get<TagComponent>(src).Tag
			: std::string();
		Entity clone = CreateEntity(name);
		entt::entity dst = static_cast<entt::entity>(clone.m_Handle);

		// Copy every built-in component present on the source EXCEPT identity (the clone
		// keeps its fresh UUID) and the tag (already seeded above). The default
		// Transform/2D components CreateEntity added are overwritten via emplace_or_replace.
		//
		// MAINTENANCE: a new component type in Components.h must be added to this list to
		// be duplicated — and, if it carries a live backend handle, to ResetRuntimeHandles too.
		CopyComponentIfExists<TransformComponent>(registry, dst, src);
		CopyComponentIfExists<SpriteRendererComponent>(registry, dst, src);
		CopyComponentIfExists<CircleRendererComponent>(registry, dst, src);
		CopyComponentIfExists<TextComponent>(registry, dst, src);
		CopyComponentIfExists<CameraComponent>(registry, dst, src);
		CopyComponentIfExists<DirectionalLightComponent>(registry, dst, src);
		CopyComponentIfExists<RigidBody2DComponent>(registry, dst, src);
		CopyComponentIfExists<BoxCollider2DComponent>(registry, dst, src);
		CopyComponentIfExists<CircleCollider2DComponent>(registry, dst, src);
		CopyComponentIfExists<Transform3DComponent>(registry, dst, src);
		CopyComponentIfExists<MeshRendererComponent>(registry, dst, src);
		CopyComponentIfExists<RigidBody3DComponent>(registry, dst, src);
		CopyComponentIfExists<BoxCollider3DComponent>(registry, dst, src);
		CopyComponentIfExists<SphereCollider3DComponent>(registry, dst, src);
		CopyComponentIfExists<CapsuleCollider3DComponent>(registry, dst, src);
		CopyComponentIfExists<CharacterController3DComponent>(registry, dst, src);
		CopyComponentIfExists<AudioSourceComponent>(registry, dst, src);
		CopyComponentIfExists<AudioListenerComponent>(registry, dst, src);

		// Reset the copied live physics handles so the clone never aliases — and
		// DestroyEntity never double-frees — the source's body/shapes/controller (see Components.h).
		ResetRuntimeHandles(static_cast<std::uint32_t>(dst));

		// If the world is already simulating, give the clone its own body now (mirrors a
		// runtime CreateEntity + CreateRigidBody spawn); otherwise OnPhysicsStart will.
		if (IsPhysicsRunning())
			CreateRigidBody(clone);

		return clone;
	}

	// MAINTENANCE: new handle-carrying components join here.
	void Scene::ResetRuntimeHandles(std::uint32_t handle)
	{
		entt::entity e = static_cast<entt::entity>(handle);
		entt::registry& registry = m_Data->Registry;

		if (registry.all_of<RigidBody2DComponent>(e))
			registry.get<RigidBody2DComponent>(e).RuntimeBody = 0;
		if (registry.all_of<BoxCollider2DComponent>(e))
			registry.get<BoxCollider2DComponent>(e).RuntimeShape = 0;
		if (registry.all_of<CircleCollider2DComponent>(e))
			registry.get<CircleCollider2DComponent>(e).RuntimeShape = 0;
		if (registry.all_of<RigidBody3DComponent>(e))
			registry.get<RigidBody3DComponent>(e).RuntimeBody = k_InvalidBody3D;
		if (registry.all_of<CharacterController3DComponent>(e))
			registry.get<CharacterController3DComponent>(e).RuntimeController = CharacterController3DComponent::k_InvalidControllerIndex;
		if (registry.all_of<AudioSourceComponent>(e))
			registry.get<AudioSourceComponent>(e).RuntimeSound = k_InvalidSound;
	}

	void Scene::DestroyEntity(Entity entity)
	{
		if (!entity)
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
	}

	void Scene::DestroyEntityNow(std::uint32_t handle)
	{
		entt::entity e = static_cast<entt::entity>(handle);
		if (!m_Data->Registry.valid(e))
			return;

		DetachScript(handle);

		if (m_Data->Registry.all_of<IDComponent>(e))
			m_Data->EntityMap.erase(m_Data->Registry.get<IDComponent>(e).ID);

		m_Data->Physics.DestroyBodiesForEntity(m_Data->Registry, e);

		// Stop the entity's live sound so it doesn't keep playing after its entity
		// (and transform) is gone.
		Internal::AudioSync::StopSource(m_Data->Registry, e);

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
		OnPhysicsStop();

		Internal::AudioSync::StopAllSources(m_Data->Registry);

		m_Data->Scripts.DetachAll();

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
		m_Data->Updating = true;
		m_Data->Scripts.Update(deltaTime);
		m_Data->Updating = false;

		for (entt::entity handle : m_Data->PendingDestroy)
			DestroyEntityNow(static_cast<std::uint32_t>(handle));
		m_Data->PendingDestroy.clear();

		// Step physics after the script pass (scripts may have applied forces this
		// frame), then write the simulated transforms back onto the entities.
		m_Data->Physics.Step(m_Data->Registry, deltaTime);

		// Transforms are final for the frame now (physics + controller write-back
		// already happened), so sync every spatialized source's position and the
		// listener before anything renders or is heard this frame.
		Internal::AudioSync::SyncListenerAndSources(m_Data->Registry);
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
		// Sprites (solid-colour or textured quads), painter-sorted by z: a higher
		// Position.z draws on top. Equal-z ties are NOT ordered by creation (the
		// view order decides), so give overlapping UI elements distinct z values.
		{
			auto view = m_Data->Registry.view<TransformComponent, SpriteRendererComponent>();

			// Sort keys, not handles: the comparator used to re-fetch TransformComponent
			// through the registry for BOTH operands on every comparison. The buffer is a
			// member so a steady-state frame does no allocation at all.
			std::vector<std::pair<float, entt::entity>>& sprites = m_Data->SpriteSortBuffer;
			sprites.clear();
			for (entt::entity entity : view)
				sprites.emplace_back(view.get<TransformComponent>(entity).Position.z, entity);

			std::stable_sort(sprites.begin(), sprites.end(), [](const auto& a, const auto& b)
			{
				return a.first < b.first;
			});

			for (const auto& [z, entity] : sprites)
			{
				auto [transform, sprite] = view.get<TransformComponent, SpriteRendererComponent>(entity);
				Texture* texture = sprite.Texture ? sprite.Texture : Renderer::GetWhiteTexture();

				if (transform.Rotation != 0.0f)
					renderer.DrawRotatedQuad(transform.Position, transform.Rotation, transform.Size, texture, sprite.Color);
				else
					renderer.DrawQuad(transform.Position, transform.Size, texture, sprite.Color);
			}
		}

		// Circles
		{
			auto view = m_Data->Registry.view<TransformComponent, CircleRendererComponent>();
			for (auto entity : view)
			{
				auto [transform, circle] = view.get<TransformComponent, CircleRendererComponent>(entity);
				renderer.DrawCircle(transform.GetTransform(), circle.Color, circle.Thickness, circle.Fade);
			}
		}

		// Text
		{
			auto view = m_Data->Registry.view<TransformComponent, TextComponent>();
			for (auto entity : view)
			{
				auto [transform, text] = view.get<TransformComponent, TextComponent>(entity);
				if (!text.Font || text.Text.empty())
					continue;

				renderer.DrawText(text.Text, text.Font, transform.Position, text.Size, { .Color = text.Color, .Centered = text.Centered });
			}
		}
	}

	void Scene::RenderEntities3D(Renderer3D& renderer)
	{
		auto view = m_Data->Registry.view<Transform3DComponent, MeshRendererComponent>();
		for (entt::entity entity : view)
		{
			auto [transform, mesh] = view.get<Transform3DComponent, MeshRendererComponent>(entity);
			if (!mesh.Visible || !mesh.Mesh)
				continue;

			renderer.SubmitMesh(mesh.Mesh, transform.GetTransform(), mesh.Color, mesh.Material);
		}
	}

	// --- Camera -----------------------------------------------------------------

	bool Scene::GetPrimaryCameraEntity(Entity& out)
	{
		entt::entity handle = entt::null;
		if (!Internal::CameraUtils::FindPrimaryCamera(m_Data->Registry, handle))
			return false;

		out = Wrap(static_cast<std::uint32_t>(handle));
		return true;
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

	bool Scene::GetFirstDirectionalLightEntity(Entity& out)
	{
		entt::entity handle = entt::null;
		if (!Internal::CameraUtils::FindFirstDirectionalLight(m_Data->Registry, handle))
			return false;

		out = Wrap(static_cast<std::uint32_t>(handle));
		return true;
	}

	glm::mat4 Scene::GetCameraViewProjection(Entity cameraEntity, float aspect)
	{
		if (!IsValid(cameraEntity))
			return glm::mat4(1.0f);

		return Internal::CameraUtils::ViewProjection(m_Data->Registry, static_cast<entt::entity>(cameraEntity.m_Handle), aspect);
	}

	glm::mat4 Scene::GetActiveCameraViewProjection(float aspect)
	{
		Entity cameraEntity;
		if (!GetPrimaryCameraEntity(cameraEntity))
			return glm::mat4(1.0f);

		return GetCameraViewProjection(cameraEntity, aspect);
	}

	Ray Scene::ScreenPointToRay(const glm::vec2& screenPos, const glm::vec2& viewportSize)
	{
		return Internal::CameraUtils::ScreenPointToRay(m_Data->Registry, screenPos, viewportSize);
	}

	// --- Lifecycle ----------------------------------------------------------------

	void Scene::OnStart()
	{
		if (m_IsRunning)
			return;

		m_IsRunning = true;

		// Discard any transition requested before this run (or left over from a
		// previous run) so a stale request can't fire the instant this scene reactivates.
		ClearPendingSceneTransition();

		// Run script OnStart before physics so a controller script can build the world
		// (spawn entities) and have OnPhysicsStart bake bodies for everything it created.
		StartScripts();
		OnPhysicsStart();

		// Start every PlayOnStart source now that scripts have had a chance to set up
		// (and, for a spatialized source, after any script-driven initial placement).
		auto sourceView = m_Data->Registry.view<AudioSourceComponent>();
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
		OnPhysicsStop();

		// Stop every source's live sound so the scene never leaks playing audio past
		// its own teardown (e.g. into whatever scene the SceneManager switches to).
		Internal::AudioSync::StopAllSources(m_Data->Registry);

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
		m_Data->Physics.Start(m_Data->Registry, m_Gravity, m_Gravity3D);
	}

	void Scene::OnPhysicsStop()
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
		if (!entity)
			return;

		m_Data->Physics.CreateBodiesForEntity(m_Data->Registry, static_cast<entt::entity>(entity.m_Handle));
	}

	CharacterController3D* Scene::GetCharacterController(Entity entity) const
	{
		return m_Data->Physics.GetController(m_Data->Registry, static_cast<entt::entity>(entity.m_Handle));
	}

	std::uint64_t Scene::GetRuntimeBody(Entity entity) const
	{
		return m_Data->Physics.RuntimeBody2D(m_Data->Registry, static_cast<entt::entity>(entity.m_Handle));
	}

	std::uint32_t Scene::GetRuntimeBody3D(Entity entity) const
	{
		return m_Data->Physics.RuntimeBody3D(m_Data->Registry, static_cast<entt::entity>(entity.m_Handle));
	}

	void Scene::SetLinearVelocity(Entity entity, const glm::vec2& velocity)
	{
		if (Physics2D* physics = m_Data->Physics.Get2D())
			physics->SetLinearVelocity(GetRuntimeBody(entity), velocity);
	}

	glm::vec2 Scene::GetLinearVelocity(Entity entity)
	{
		Physics2D* physics = m_Data->Physics.Get2D();
		if (!physics)
			return glm::vec2(0.0f);

		return physics->GetLinearVelocity(GetRuntimeBody(entity));
	}

	void Scene::ApplyLinearImpulse(Entity entity, const glm::vec2& impulse, const glm::vec2& worldPoint, bool wake)
	{
		if (Physics2D* physics = m_Data->Physics.Get2D())
			physics->ApplyLinearImpulse(GetRuntimeBody(entity), impulse, worldPoint, wake);
	}

	void Scene::ApplyLinearImpulseToCenter(Entity entity, const glm::vec2& impulse, bool wake)
	{
		if (Physics2D* physics = m_Data->Physics.Get2D())
			physics->ApplyLinearImpulseToCenter(GetRuntimeBody(entity), impulse, wake);
	}

	void Scene::ApplyForceToCenter(Entity entity, const glm::vec2& force, bool wake)
	{
		if (Physics2D* physics = m_Data->Physics.Get2D())
			physics->ApplyForceToCenter(GetRuntimeBody(entity), force, wake);
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

		Internal::AudioSync::PlaySource(m_Data->Registry, static_cast<entt::entity>(entity.m_Handle));
	}

	void Scene::StopAudioSource(Entity entity)
	{
		if (!IsValid(entity))
			return;

		Internal::AudioSync::StopSource(m_Data->Registry, static_cast<entt::entity>(entity.m_Handle));
	}

}
