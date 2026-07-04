#include "depch.h"
#include "DingoEngine/Scene/Scene.h"
#include "DingoEngine/Scene/Entity.h"
#include "DingoEngine/Scene/ScriptableEntity.h"
#include "DingoEngine/Scene/Components.h"

#include "DingoEngine/Graphics/Renderer.h"
#include "DingoEngine/Graphics/Renderer2D.h"
#include "DingoEngine/Graphics/Renderer3D.h"

#include "DingoEngine/Scene/SceneData.h"
#include "DingoEngine/Physics/2D/Physics2D.h"
#include "DingoEngine/Physics/3D/Physics3D.h"
#include "DingoEngine/Physics/3D/CharacterController3D.h"

#include "DingoEngine/Core/Application.h"
#include "DingoEngine/Audio/AudioEngine.h"

#include <glm/gtc/quaternion.hpp>

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
		// be duplicated — and, if it carries a live backend handle, reset below too.
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
		if (registry.all_of<RigidBody2DComponent>(dst))
			registry.get<RigidBody2DComponent>(dst).RuntimeBody = 0;
		if (registry.all_of<BoxCollider2DComponent>(dst))
			registry.get<BoxCollider2DComponent>(dst).RuntimeShape = 0;
		if (registry.all_of<CircleCollider2DComponent>(dst))
			registry.get<CircleCollider2DComponent>(dst).RuntimeShape = 0;
		if (registry.all_of<RigidBody3DComponent>(dst))
			registry.get<RigidBody3DComponent>(dst).RuntimeBody = k_InvalidBody3D;
		if (registry.all_of<CharacterController3DComponent>(dst))
			registry.get<CharacterController3DComponent>(dst).RuntimeController = CharacterController3DComponent::k_InvalidControllerIndex;
		if (registry.all_of<AudioSourceComponent>(dst))
			registry.get<AudioSourceComponent>(dst).RuntimeSound = k_InvalidSound;

		// If the world is already simulating, give the clone its own body now (mirrors a
		// runtime CreateEntity + CreateRigidBody spawn); otherwise OnPhysicsStart will.
		if (IsPhysicsRunning())
			CreateRigidBody(clone);

		return clone;
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

	void Scene::DestroyEntityNow(std::uint32_t handle)
	{
		entt::entity e = static_cast<entt::entity>(handle);
		if (!m_Data->Registry.valid(e))
			return;

		if (auto it = m_Data->Scripts.find(e); it != m_Data->Scripts.end())
		{
			it->second->OnDestroy();
			m_Data->Scripts.erase(it);
		}

		if (m_Data->Registry.all_of<IDComponent>(e))
			m_Data->EntityMap.erase(m_Data->Registry.get<IDComponent>(e).ID);

		// Release the physics body (and its shapes) if one was created, so it
		// doesn't keep colliding in the world after its entity is gone.
		if (m_Data->Physics && m_Data->Physics->IsValid() && m_Data->Registry.all_of<RigidBody2DComponent>(e))
		{
			PhysicsBodyId2D runtimeBody = m_Data->Registry.get<RigidBody2DComponent>(e).RuntimeBody;
			m_Data->Physics->DestroyBody(runtimeBody);
		}

		if (m_Data->Physics3D && m_Data->Physics3D->IsValid() && m_Data->Registry.all_of<RigidBody3DComponent>(e))
		{
			PhysicsBodyId3D runtimeBody = m_Data->Registry.get<RigidBody3DComponent>(e).RuntimeBody;
			m_Data->Physics3D->DestroyBody(runtimeBody);
		}

		// Free the entity's character controller (its slot stays but goes null so other
		// entities' stored indices remain valid).
		if (m_Data->Registry.all_of<CharacterController3DComponent>(e))
		{
			std::uint32_t index = m_Data->Registry.get<CharacterController3DComponent>(e).RuntimeController;
			if (index != CharacterController3DComponent::k_InvalidControllerIndex && index < m_Data->CharacterControllers.size())
				m_Data->CharacterControllers[index].reset();
		}

		// Stop the entity's live sound so it doesn't keep playing after its entity
		// (and transform) is gone.
		if (m_Data->Registry.all_of<AudioSourceComponent>(e))
		{
			AudioSoundId sound = m_Data->Registry.get<AudioSourceComponent>(e).RuntimeSound;
			if (sound != k_InvalidSound)
				Application::Get().GetAudioEngine().Stop(sound);
		}

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

		// Looping sounds are never self-reaping, so dropping the registry without
		// stopping them would leave them playing with no handle left to stop.
		StopAudioSources();

		for (auto& [handle, script] : m_Data->Scripts)
			script->OnDestroy();

		m_Data->Scripts.clear();
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

		// Scripts attached since the scene started (e.g. spawned at runtime) get OnStart
		// before their first OnUpdate.
		StartScripts();

		// Snapshot the current scripts so spawning new entities mid-update doesn't
		// invalidate iteration (new scripts run next frame).
		std::vector<entt::entity> handles;
		handles.reserve(m_Data->Scripts.size());
		for (auto& [handle, script] : m_Data->Scripts)
			handles.push_back(handle);

		for (entt::entity handle : handles)
		{
			auto it = m_Data->Scripts.find(handle);
			if (it != m_Data->Scripts.end())
				it->second->OnUpdate(deltaTime);
		}

		m_Data->Updating = false;

		for (entt::entity handle : m_Data->PendingDestroy)
			DestroyEntityNow(static_cast<std::uint32_t>(handle));
		m_Data->PendingDestroy.clear();

		// Step physics after the script pass (scripts may have applied forces this
		// frame), then write the simulated transforms back onto the entities.
		if (m_Data->Physics && m_Data->Physics->IsValid())
		{
			m_Data->Physics->Step(deltaTime, m_Data->PhysicsSubStepCount);

			auto view = m_Data->Registry.view<RigidBody2DComponent, TransformComponent>();
			for (entt::entity handle : view)
			{
				const RigidBody2DComponent& rigidBody = view.get<RigidBody2DComponent>(handle);
				if (rigidBody.RuntimeBody == 0)
					continue;

				glm::vec2 position = m_Data->Physics->GetPosition(rigidBody.RuntimeBody);
				float angle = m_Data->Physics->GetAngle(rigidBody.RuntimeBody);

				TransformComponent& transform = view.get<TransformComponent>(handle);
				transform.Position.x = position.x;
				transform.Position.y = position.y;
				transform.Rotation = glm::degrees(angle);
			}
		}

		// Same for the 3D world: step it, then write each body's simulated
		// position/rotation back onto its Transform3DComponent.
		if (m_Data->Physics3D && m_Data->Physics3D->IsValid())
		{
			m_Data->Physics3D->Step(deltaTime, m_Data->PhysicsCollisionSteps);

			auto view = m_Data->Registry.view<RigidBody3DComponent, Transform3DComponent>();
			for (entt::entity handle : view)
			{
				const RigidBody3DComponent& rigidBody = view.get<RigidBody3DComponent>(handle);
				if (rigidBody.RuntimeBody == k_InvalidBody3D)
					continue;

				// Static bodies never move — skip the read-back so we don't churn over
				// them or revert a runtime edit to a static entity's Transform3D.
				if (rigidBody.Type == BodyType3D::Static)
					continue;

				Transform3DComponent& transform = view.get<Transform3DComponent>(handle);
				transform.Position = m_Data->Physics3D->GetPosition(rigidBody.RuntimeBody);
				transform.Rotation = m_Data->Physics3D->GetRotation(rigidBody.RuntimeBody);
			}

			// Character controllers: update each (scripts set its velocity in their
			// OnUpdate above), then write the swept position/rotation back onto the
			// entity's Transform3D. Their capsule "feet" position is the transform origin.
			auto ccView = m_Data->Registry.view<CharacterController3DComponent, Transform3DComponent>();
			for (entt::entity handle : ccView)
			{
				const CharacterController3DComponent& cc = ccView.get<CharacterController3DComponent>(handle);
				if (cc.RuntimeController == CharacterController3DComponent::k_InvalidControllerIndex
					|| cc.RuntimeController >= m_Data->CharacterControllers.size())
					continue;

				CharacterController3D* controller = m_Data->CharacterControllers[cc.RuntimeController].get();
				if (!controller)
					continue;

				controller->Update(deltaTime);

				Transform3DComponent& transform = ccView.get<Transform3DComponent>(handle);
				transform.Position = controller->GetPosition();
				transform.Rotation = controller->GetRotation();
			}
		}

		// Audio: transforms are final for the frame now (physics + controller
		// write-back above already happened), so sync every spatialized source's
		// position and the listener before anything renders or is heard this frame.
		{
			AudioEngine& audio = Application::Get().GetAudioEngine();

			auto sourceView = m_Data->Registry.view<AudioSourceComponent>();
			for (entt::entity handle : sourceView)
			{
				AudioSourceComponent& source = sourceView.get<AudioSourceComponent>(handle);
				if (!source.Spatialized || source.RuntimeSound == k_InvalidSound)
					continue;

				audio.SetPosition(source.RuntimeSound, GetAudioPosition(static_cast<std::uint32_t>(handle)));
			}

			// Primary listener search mirrors GetPrimaryCameraEntity: first Primary wins,
			// else the first listener found. If the scene has none, leave the engine's
			// listener as it was (no implicit reset to origin).
			entt::entity listenerHandle = entt::null;
			auto listenerView = m_Data->Registry.view<AudioListenerComponent>();
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

			if (listenerHandle != entt::null)
			{
				audio.SetListenerPosition(GetAudioPosition(static_cast<std::uint32_t>(listenerHandle)));

				// Orientation only comes from a 3D transform (same convention as the
				// perspective camera view in GetCameraViewProjection: the entity's local
				// -Z is forward, +Y is up). A 2D listener has no rotation to derive this
				// from, so it keeps whatever orientation the engine already has.
				if (m_Data->Registry.all_of<Transform3DComponent>(listenerHandle))
				{
					const glm::quat& rotation = m_Data->Registry.get<Transform3DComponent>(listenerHandle).Rotation;
					glm::vec3 forward = rotation * glm::vec3(0.0f, 0.0f, -1.0f);
					glm::vec3 up = rotation * glm::vec3(0.0f, 1.0f, 0.0f);
					audio.SetListenerOrientation(forward, up);
				}
			}
		}
	}

	void Scene::ForEachEntity(const std::function<void(Entity)>& fn)
	{
		auto view = m_Data->Registry.view<IDComponent>();
		for (entt::entity handle : view)
			fn(Wrap(static_cast<std::uint32_t>(handle)));
	}

	void Scene::ForEachScript(const std::function<void(ScriptableEntity*)>& fn)
	{
		for (auto& [handle, script] : m_Data->Scripts)
			fn(script.get());
	}

	void Scene::StartScripts()
	{
		// Snapshot the not-yet-started handles, so an OnStart that spawns more scripts
		// doesn't invalidate iteration (those run on a later StartScripts pass).
		std::vector<entt::entity> handles;
		handles.reserve(m_Data->Scripts.size());
		for (auto& [handle, script] : m_Data->Scripts)
			if (!script->m_Started)
				handles.push_back(handle);

		for (entt::entity handle : handles)
		{
			auto it = m_Data->Scripts.find(handle);
			if (it == m_Data->Scripts.end() || it->second->m_Started)
				continue;

			it->second->m_Started = true; // set first so a re-entrant spawn can't double-fire
			it->second->OnStart();
		}
	}

	void Scene::RenderEntities(Renderer2D& renderer)
	{
		// Sprites (solid-colour or textured quads), painter-sorted by z: a higher
		// Position.z draws on top. Equal-z ties are NOT ordered by creation (the
		// view order decides), so give overlapping UI elements distinct z values.
		{
			auto view = m_Data->Registry.view<TransformComponent, SpriteRendererComponent>();
			std::vector<entt::entity> sprites(view.begin(), view.end());
			std::stable_sort(sprites.begin(), sprites.end(), [this](entt::entity a, entt::entity b)
			{
				return m_Data->Registry.get<TransformComponent>(a).Position.z
				     < m_Data->Registry.get<TransformComponent>(b).Position.z;
			});

			for (entt::entity entity : sprites)
			{
				auto& transform = m_Data->Registry.get<TransformComponent>(entity);
				auto& sprite = m_Data->Registry.get<SpriteRendererComponent>(entity);
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

				glm::vec3 position = transform.Position;
				if (text.Centered)
					position.x -= text.Font->GetStringWidth(text.Text, text.Size) * 0.5f;

				renderer.DrawText(text.Text, text.Font, position, text.Size, { text.Color });
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
		entt::entity first = entt::null;

		auto view = m_Data->Registry.view<CameraComponent>();
		for (entt::entity handle : view)
		{
			if (first == entt::null)
				first = handle;

			if (view.get<CameraComponent>(handle).Primary)
			{
				out = Wrap(static_cast<std::uint32_t>(handle));
				return true;
			}
		}

		if (first != entt::null)
		{
			out = Wrap(static_cast<std::uint32_t>(first));
			return true;
		}

		return false;
	}

	glm::mat4 Scene::GetCameraViewProjection(Entity cameraEntity, float aspect)
	{
		if (!IsValid(cameraEntity))
			return glm::mat4(1.0f);

		entt::entity handle = static_cast<entt::entity>(cameraEntity.m_Handle);
		if (!m_Data->Registry.all_of<CameraComponent>(handle))
			return glm::mat4(1.0f);

		const CameraComponent& camera = m_Data->Registry.get<CameraComponent>(handle);
		const glm::mat4 projection = camera.GetProjection(aspect);

		// The view is the inverse of the camera entity's world transform. A perspective
		// camera reads its Transform3DComponent (position + orientation); an orthographic
		// camera reads the 2D TransformComponent (every entity has one on creation).
		glm::mat4 view(1.0f);
		if (camera.Type == CameraComponent::ProjectionType::Perspective)
		{
			if (m_Data->Registry.all_of<Transform3DComponent>(handle))
			{
				const Transform3DComponent& transform = m_Data->Registry.get<Transform3DComponent>(handle);
				view = glm::inverse(glm::translate(glm::mat4(1.0f), transform.Position) * glm::mat4_cast(transform.Rotation));
			}
		}
		else
		{
			const TransformComponent& transform = m_Data->Registry.get<TransformComponent>(handle);
			view = glm::inverse(
				glm::translate(glm::mat4(1.0f), glm::vec3(transform.Position.x, transform.Position.y, 0.0f))
				* glm::rotate(glm::mat4(1.0f), glm::radians(transform.Rotation), glm::vec3(0.0f, 0.0f, 1.0f)));
		}

		return projection * view;
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
		// Same "prefer Primary, else first" search SceneRenderer uses, narrowed to
		// perspective cameras — unprojecting through an orthographic camera's parallel
		// projection would not produce a meaningful converging ray.
		entt::entity perspectiveHandle = entt::null;

		auto view = m_Data->Registry.view<CameraComponent>();
		for (entt::entity handle : view)
		{
			const CameraComponent& camera = view.get<CameraComponent>(handle);
			if (camera.Type != CameraComponent::ProjectionType::Perspective)
				continue;

			if (perspectiveHandle == entt::null)
				perspectiveHandle = handle;

			if (camera.Primary)
			{
				perspectiveHandle = handle;
				break;
			}
		}

		if (perspectiveHandle == entt::null || viewportSize.y <= 0.0f)
			return Ray();

		const float aspect = viewportSize.x / viewportSize.y;
		const CameraComponent& camera = m_Data->Registry.get<CameraComponent>(perspectiveHandle);
		const glm::mat4 projection = camera.GetProjection(aspect);

		glm::mat4 cameraView(1.0f);
		if (m_Data->Registry.all_of<Transform3DComponent>(perspectiveHandle))
		{
			const Transform3DComponent& transform = m_Data->Registry.get<Transform3DComponent>(perspectiveHandle);
			cameraView = glm::inverse(glm::translate(glm::mat4(1.0f), transform.Position) * glm::mat4_cast(transform.Rotation));
		}

		return Dingo::ScreenPointToRay(screenPos, viewportSize, cameraView, projection);
	}

	// --- Physics (2D) -----------------------------------------------------------

	void Scene::SetGravity(const glm::vec2& gravity)
	{
		m_Gravity = gravity;
		if (m_Data->Physics && m_Data->Physics->IsValid())
			m_Data->Physics->SetGravity(gravity);
	}

	void Scene::SetGravity(const glm::vec3& gravity)
	{
		m_Gravity3D = gravity;
		if (m_Data->Physics3D && m_Data->Physics3D->IsValid())
			m_Data->Physics3D->SetGravity(gravity);
	}

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
				PlayAudioSource(Wrap(static_cast<std::uint32_t>(handle)));
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
		StopAudioSources();

		// Drop any request left pending when this scene stopped being active, so it
		// can't be replayed the next time this scene starts.
		ClearPendingSceneTransition();
	}

	void Scene::StopAudioSources()
	{
		AudioEngine& audio = Application::Get().GetAudioEngine();
		for (entt::entity handle : m_Data->Registry.view<AudioSourceComponent>())
		{
			AudioSourceComponent& source = m_Data->Registry.get<AudioSourceComponent>(handle);
			if (source.RuntimeSound != k_InvalidSound)
			{
				audio.Stop(source.RuntimeSound);
				source.RuntimeSound = k_InvalidSound;
			}
		}
	}

	void Scene::OnPhysicsStart()
	{
		// 2D world — created only if the scene actually has 2D rigid bodies.
		auto rb2dView = m_Data->Registry.view<RigidBody2DComponent>();
		if ((!m_Data->Physics || !m_Data->Physics->IsValid()) && rb2dView.begin() != rb2dView.end())
		{
			m_Data->Physics.reset(Physics2D::Create());
			m_Data->Physics->Initialize(m_Gravity);

			// Give every rigid-body entity a simulation body + collider shapes.
			for (entt::entity handle : rb2dView)
				CreatePhysicsBodyForEntity(static_cast<std::uint32_t>(handle));
		}

		// 3D world — created if the scene has 3D rigid bodies OR character controllers,
		// so a purely-2D scene never spins up a Jolt world (and vice versa).
		auto rb3dView = m_Data->Registry.view<RigidBody3DComponent>();
		auto cc3dView = m_Data->Registry.view<CharacterController3DComponent>();
		const bool needs3D = rb3dView.begin() != rb3dView.end() || cc3dView.begin() != cc3dView.end();
		if ((!m_Data->Physics3D || !m_Data->Physics3D->IsValid()) && needs3D)
		{
			Physics3DParams params;
			params.Gravity = m_Gravity3D;
			m_Data->Physics3D.reset(Physics3D::Create());
			m_Data->Physics3D->Initialize(params);

			for (entt::entity handle : rb3dView)
				CreatePhysicsBody3DForEntity(static_cast<std::uint32_t>(handle));

			for (entt::entity handle : cc3dView)
				CreateCharacterControllerForEntity(static_cast<std::uint32_t>(handle));
		}
	}

	void Scene::OnPhysicsStop()
	{
		if (m_Data->Physics && m_Data->Physics->IsValid())
		{
			m_Data->Physics->Shutdown(); // also destroys all bodies + shapes
			m_Data->Physics.reset();

			// Clear the now-dangling runtime handles so a later restart is clean.
			for (entt::entity handle : m_Data->Registry.view<RigidBody2DComponent>())
				m_Data->Registry.get<RigidBody2DComponent>(handle).RuntimeBody = 0;
			for (entt::entity handle : m_Data->Registry.view<BoxCollider2DComponent>())
				m_Data->Registry.get<BoxCollider2DComponent>(handle).RuntimeShape = 0;
			for (entt::entity handle : m_Data->Registry.view<CircleCollider2DComponent>())
				m_Data->Registry.get<CircleCollider2DComponent>(handle).RuntimeShape = 0;
		}

		if (m_Data->Physics3D && m_Data->Physics3D->IsValid())
		{
			// Character controllers hold the world, so tear them down BEFORE the Physics3D.
			m_Data->CharacterControllers.clear();
			for (entt::entity handle : m_Data->Registry.view<CharacterController3DComponent>())
				m_Data->Registry.get<CharacterController3DComponent>(handle).RuntimeController = CharacterController3DComponent::k_InvalidControllerIndex;

			m_Data->Physics3D->Shutdown(); // destroys all 3D bodies
			m_Data->Physics3D.reset();

			for (entt::entity handle : m_Data->Registry.view<RigidBody3DComponent>())
				m_Data->Registry.get<RigidBody3DComponent>(handle).RuntimeBody = k_InvalidBody3D;
		}
	}

	bool Scene::IsPhysicsRunning() const
	{
		return (m_Data->Physics && m_Data->Physics->IsValid())
			|| (m_Data->Physics3D && m_Data->Physics3D->IsValid());
	}

	Physics2D* Scene::GetPhysics2D() const
	{
		return m_Data->Physics.get();
	}

	Physics3D* Scene::GetPhysics3D() const
	{
		return m_Data->Physics3D.get();
	}

	void Scene::CreateRigidBody(Entity entity)
	{
		if (!entity)
			return;

		// Route by component: each helper no-ops if its world isn't live or the
		// entity lacks the matching rigid-body / controller component.
		CreatePhysicsBodyForEntity(entity.m_Handle);
		CreatePhysicsBody3DForEntity(entity.m_Handle);
		CreateCharacterControllerForEntity(entity.m_Handle);
	}

	void Scene::CreatePhysicsBodyForEntity(std::uint32_t handle)
	{
		if (!m_Data->Physics || !m_Data->Physics->IsValid())
			return;

		entt::entity e = static_cast<entt::entity>(handle);
		if (!m_Data->Registry.all_of<RigidBody2DComponent, TransformComponent>(e))
			return;

		auto& rigidBody = m_Data->Registry.get<RigidBody2DComponent>(e);
		if (rigidBody.RuntimeBody != 0)
			return; // a body already exists for this entity — don't leak a second one

		auto& transform = m_Data->Registry.get<TransformComponent>(e);

		RigidBodyParams2D bodyParams;
		bodyParams.Type = rigidBody.Type;
		bodyParams.Position = { transform.Position.x, transform.Position.y };
		bodyParams.Rotation = glm::radians(transform.Rotation); // Transform stores degrees
		bodyParams.FixedRotation = rigidBody.FixedRotation;

		rigidBody.RuntimeBody = m_Data->Physics->CreateBody(bodyParams);

		// Collider sizes are fractions of the entity's full extent (Transform.Size);
		// resolve them to world units here, so { 0.5, 0.5 } / radius 0.5 fits the quad.
		if (m_Data->Registry.all_of<BoxCollider2DComponent>(e))
		{
			auto& collider = m_Data->Registry.get<BoxCollider2DComponent>(e);

			BoxShapeParams2D shapeParams;
			shapeParams.HalfExtents = { transform.Size.x * collider.Size.x, transform.Size.y * collider.Size.y };
			shapeParams.Center = { collider.Offset.x * transform.Size.x, collider.Offset.y * transform.Size.y };
			shapeParams.Density = collider.Density;
			shapeParams.Friction = collider.Friction;
			shapeParams.Restitution = collider.Restitution;

			collider.RuntimeShape = m_Data->Physics->AddBoxShape(rigidBody.RuntimeBody, shapeParams);
		}

		if (m_Data->Registry.all_of<CircleCollider2DComponent>(e))
		{
			auto& collider = m_Data->Registry.get<CircleCollider2DComponent>(e);

			CircleShapeParams2D shapeParams;
			shapeParams.Radius = transform.Size.x * collider.Radius;
			shapeParams.Center = { collider.Offset.x * transform.Size.x, collider.Offset.y * transform.Size.y };
			shapeParams.Density = collider.Density;
			shapeParams.Friction = collider.Friction;
			shapeParams.Restitution = collider.Restitution;

			collider.RuntimeShape = m_Data->Physics->AddCircleShape(rigidBody.RuntimeBody, shapeParams);
		}
	}

	std::uint64_t Scene::GetRuntimeBody(Entity entity) const
	{
		entt::entity e = static_cast<entt::entity>(entity.m_Handle);
		if (!m_Data->Registry.valid(e) || !m_Data->Registry.all_of<RigidBody2DComponent>(e))
			return 0;
		return m_Data->Registry.get<RigidBody2DComponent>(e).RuntimeBody;
	}

	void Scene::CreatePhysicsBody3DForEntity(std::uint32_t handle)
	{
		if (!m_Data->Physics3D || !m_Data->Physics3D->IsValid())
			return;

		entt::entity e = static_cast<entt::entity>(handle);
		if (!m_Data->Registry.all_of<RigidBody3DComponent, Transform3DComponent>(e))
			return;

		auto& rigidBody = m_Data->Registry.get<RigidBody3DComponent>(e);
		if (rigidBody.RuntimeBody != k_InvalidBody3D)
			return; // a body already exists for this entity — don't leak a second one

		auto& transform = m_Data->Registry.get<Transform3DComponent>(e);

		RigidBodyParams3D params;
		params.Type = rigidBody.Type;
		params.Position = transform.Position;
		params.Rotation = transform.Rotation;

		// The collider shape is baked into the body at creation. Collider sizes are
		// fractions of the entity's full extent (Transform3D.Scale), so a unit-scaled
		// entity with the default collider exactly fills its box.
		if (m_Data->Registry.all_of<SphereCollider3DComponent>(e))
		{
			auto& collider = m_Data->Registry.get<SphereCollider3DComponent>(e);
			params.Shape = ColliderShape3D::Sphere;
			params.Radius = transform.Scale.x * collider.Radius;
			params.Friction = collider.Friction;
			params.Restitution = collider.Restitution;
		}
		else if (m_Data->Registry.all_of<CapsuleCollider3DComponent>(e))
		{
			auto& collider = m_Data->Registry.get<CapsuleCollider3DComponent>(e);
			params.Shape = ColliderShape3D::Capsule;
			params.Radius = transform.Scale.x * collider.Radius;
			params.HalfHeight = transform.Scale.y * collider.HalfHeight;
			params.Friction = collider.Friction;
			params.Restitution = collider.Restitution;
		}
		else if (m_Data->Registry.all_of<BoxCollider3DComponent>(e))
		{
			auto& collider = m_Data->Registry.get<BoxCollider3DComponent>(e);
			params.Shape = ColliderShape3D::Box;
			params.HalfExtents = transform.Scale * collider.HalfExtents;
			params.Friction = collider.Friction;
			params.Restitution = collider.Restitution;
		}
		else
		{
			// No collider component: fall back to a box matching the transform.
			params.Shape = ColliderShape3D::Box;
			params.HalfExtents = transform.Scale * 0.5f;
		}

		rigidBody.RuntimeBody = m_Data->Physics3D->CreateBody(params);
	}

	std::uint32_t Scene::GetRuntimeBody3D(Entity entity) const
	{
		entt::entity e = static_cast<entt::entity>(entity.m_Handle);
		if (!m_Data->Registry.valid(e) || !m_Data->Registry.all_of<RigidBody3DComponent>(e))
			return k_InvalidBody3D;
		return m_Data->Registry.get<RigidBody3DComponent>(e).RuntimeBody;
	}

	void Scene::CreateCharacterControllerForEntity(std::uint32_t handle)
	{
		if (!m_Data->Physics3D || !m_Data->Physics3D->IsValid())
			return;

		entt::entity e = static_cast<entt::entity>(handle);
		if (!m_Data->Registry.all_of<CharacterController3DComponent, Transform3DComponent>(e))
			return;

		auto& cc = m_Data->Registry.get<CharacterController3DComponent>(e);
		if (cc.RuntimeController != CharacterController3DComponent::k_InvalidControllerIndex)
			return; // already created — don't leak a second controller

		auto& transform = m_Data->Registry.get<Transform3DComponent>(e);

		CharacterControllerParams3D params;
		params.Radius = cc.Radius;
		params.Height = cc.Height;
		params.StepHeight = cc.StepHeight;
		params.MaxSlopeAngle = cc.MaxSlopeAngle;
		params.Position = transform.Position;
		params.Rotation = transform.Rotation;

		std::unique_ptr<CharacterController3D> controller = m_Data->Physics3D->CreateCharacterController(params);
		if (!controller)
			return;

		m_Data->CharacterControllers.push_back(std::move(controller));
		cc.RuntimeController = static_cast<std::uint32_t>(m_Data->CharacterControllers.size() - 1);
	}

	CharacterController3D* Scene::GetCharacterController(Entity entity) const
	{
		entt::entity e = static_cast<entt::entity>(entity.m_Handle);
		if (!m_Data->Registry.valid(e) || !m_Data->Registry.all_of<CharacterController3DComponent>(e))
			return nullptr;

		std::uint32_t index = m_Data->Registry.get<CharacterController3DComponent>(e).RuntimeController;
		if (index == CharacterController3DComponent::k_InvalidControllerIndex || index >= m_Data->CharacterControllers.size())
			return nullptr;

		return m_Data->CharacterControllers[index].get();
	}

	// --- Audio --------------------------------------------------------------

	glm::vec3 Scene::GetAudioPosition(std::uint32_t handle) const
	{
		entt::entity e = static_cast<entt::entity>(handle);
		if (m_Data->Registry.all_of<Transform3DComponent>(e))
			return m_Data->Registry.get<Transform3DComponent>(e).Position;

		const TransformComponent& transform = m_Data->Registry.get<TransformComponent>(e);
		return glm::vec3(transform.Position.x, transform.Position.y, 0.0f);
	}

	void Scene::PlayAudioSource(Entity entity)
	{
		if (!IsValid(entity))
			return;

		entt::entity e = static_cast<entt::entity>(entity.m_Handle);
		if (!m_Data->Registry.all_of<AudioSourceComponent>(e))
			return;

		AudioSourceComponent& source = m_Data->Registry.get<AudioSourceComponent>(e);
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
			params.Position = GetAudioPosition(entity.m_Handle);

		source.RuntimeSound = audio.Play(source.Clip, params);
	}

	void Scene::StopAudioSource(Entity entity)
	{
		if (!IsValid(entity))
			return;

		entt::entity e = static_cast<entt::entity>(entity.m_Handle);
		if (!m_Data->Registry.all_of<AudioSourceComponent>(e))
			return;

		AudioSourceComponent& source = m_Data->Registry.get<AudioSourceComponent>(e);
		if (source.RuntimeSound == k_InvalidSound)
			return;

		Application::Get().GetAudioEngine().Stop(source.RuntimeSound);
		source.RuntimeSound = k_InvalidSound;
	}

	void Scene::SetLinearVelocity(Entity entity, const glm::vec2& velocity)
	{
		if (m_Data->Physics)
			m_Data->Physics->SetLinearVelocity(GetRuntimeBody(entity), velocity);
	}

	glm::vec2 Scene::GetLinearVelocity(Entity entity)
	{
		if (!m_Data->Physics)
			return glm::vec2(0.0f);

		return m_Data->Physics->GetLinearVelocity(GetRuntimeBody(entity));
	}

	void Scene::ApplyLinearImpulse(Entity entity, const glm::vec2& impulse, const glm::vec2& worldPoint, bool wake)
	{
		if (m_Data->Physics)
			m_Data->Physics->ApplyLinearImpulse(GetRuntimeBody(entity), impulse, worldPoint, wake);
	}

	void Scene::ApplyLinearImpulseToCenter(Entity entity, const glm::vec2& impulse, bool wake)
	{
		if (m_Data->Physics)
			m_Data->Physics->ApplyLinearImpulseToCenter(GetRuntimeBody(entity), impulse, wake);
	}

	void Scene::ApplyForceToCenter(Entity entity, const glm::vec2& force, bool wake)
	{
		if (m_Data->Physics)
			m_Data->Physics->ApplyForceToCenter(GetRuntimeBody(entity), force, wake);
	}

	// --- Physics (3D) -----------------------------------------------------------

	void Scene::SetLinearVelocity(Entity entity, const glm::vec3& velocity)
	{
		if (m_Data->Physics3D)
			m_Data->Physics3D->SetLinearVelocity(GetRuntimeBody3D(entity), velocity);
	}

	glm::vec3 Scene::GetLinearVelocity3D(Entity entity)
	{
		if (!m_Data->Physics3D)
			return glm::vec3(0.0f);

		return m_Data->Physics3D->GetLinearVelocity(GetRuntimeBody3D(entity));
	}

	void Scene::ApplyImpulse(Entity entity, const glm::vec3& impulse)
	{
		if (m_Data->Physics3D)
			m_Data->Physics3D->ApplyImpulse(GetRuntimeBody3D(entity), impulse);
	}

	void Scene::ApplyForce(Entity entity, const glm::vec3& force)
	{
		if (m_Data->Physics3D)
			m_Data->Physics3D->ApplyForce(GetRuntimeBody3D(entity), force);
	}

}
