#include "depch.h"
#include "DingoEngine/Scene/Scene.h"
#include "DingoEngine/Scene/Entity.h"
#include "DingoEngine/Scene/ScriptableEntity.h"
#include "DingoEngine/Scene/Components.h"

#include "DingoEngine/Graphics/Renderer.h"
#include "DingoEngine/Graphics/Renderer2D.h"

#include "DingoEngine/Scene/SceneData.h"
#include "DingoEngine/Physics/2D/Physics2D.h"

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

		m_Data->Registry.destroy(e);
	}

	bool Scene::IsValid(Entity entity) const
	{
		return entity.m_Scene == this
			&& m_Data->Registry.valid(static_cast<entt::entity>(entity.m_Handle));
	}

	void Scene::Clear()
	{
		// Drop the physics world first so its bodies don't outlive the entities.
		OnPhysicsStop();

		for (auto& [handle, script] : m_Data->Scripts)
			script->OnDestroy();

		m_Data->Scripts.clear();
		m_Data->Registry.clear();
		m_Data->EntityMap.clear();
		m_Data->PendingDestroy.clear();
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

	void Scene::OnRender(Renderer2D& renderer)
	{
		renderer.BeginScene(m_ViewProjection);
		renderer.Clear(m_ClearColor);
		RenderEntities(renderer);
		renderer.EndScene();
	}

	void Scene::RenderEntities(Renderer2D& renderer)
	{
		// Sprites (solid-colour or textured quads), painter-sorted by z: a higher
		// Position.z draws on top. stable_sort keeps creation order within a layer.
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

	// --- Physics (2D) -----------------------------------------------------------

	void Scene::SetGravity(const glm::vec2& gravity)
	{
		m_Gravity = gravity;
		if (m_Data->Physics && m_Data->Physics->IsValid())
			m_Data->Physics->SetGravity(gravity);
	}

	void Scene::OnPhysicsStart()
	{
		if (m_Data->Physics && m_Data->Physics->IsValid())
			return; // already running

		m_Data->Physics.reset(Physics2D::Create());
		m_Data->Physics->Initialize(m_Gravity);

		// Give every rigid-body entity a simulation body + collider shapes.
		for (entt::entity handle : m_Data->Registry.view<RigidBody2DComponent>())
			CreatePhysicsBodyForEntity(static_cast<std::uint32_t>(handle));
	}

	void Scene::OnPhysicsStop()
	{
		if (!m_Data->Physics || !m_Data->Physics->IsValid())
			return;

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

	bool Scene::IsPhysicsRunning() const
	{
		return m_Data->Physics && m_Data->Physics->IsValid();
	}

	Physics2D* Scene::GetPhysics2D() const
	{
		return m_Data->Physics.get();
	}

	void Scene::CreateRigidBody(Entity entity)
	{
		if (entity)
			CreatePhysicsBodyForEntity(entity.m_Handle);
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

}
