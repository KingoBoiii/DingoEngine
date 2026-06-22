#include "depch.h"
#include "DingoEngine/Scene/Scene.h"
#include "DingoEngine/Scene/Entity.h"
#include "DingoEngine/Scene/ScriptableEntity.h"
#include "DingoEngine/Scene/Components.h"

#include "DingoEngine/Graphics/Renderer.h"
#include "DingoEngine/Graphics/Renderer2D.h"

#include "DingoEngine/Scene/SceneData.h"

#include <algorithm>

namespace Dingo
{

	static b2BodyType ToBox2DBodyType(RigidBody2DComponent::BodyType type)
	{
		switch (type)
		{
			case RigidBody2DComponent::BodyType::Static:    return b2_staticBody;
			case RigidBody2DComponent::BodyType::Dynamic:   return b2_dynamicBody;
			case RigidBody2DComponent::BodyType::Kinematic: return b2_kinematicBody;
		}
		return b2_staticBody;
	}

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
		if (b2World_IsValid(m_Data->PhysicsWorld) && m_Data->Registry.all_of<RigidBody2DComponent>(e))
		{
			std::uint64_t runtimeBody = m_Data->Registry.get<RigidBody2DComponent>(e).RuntimeBody;
			if (runtimeBody != 0)
			{
				b2BodyId bodyId = b2LoadBodyId(runtimeBody);
				if (b2Body_IsValid(bodyId))
					b2DestroyBody(bodyId);
			}
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
		if (b2World_IsValid(m_Data->PhysicsWorld))
		{
			b2World_Step(m_Data->PhysicsWorld, deltaTime, m_Data->PhysicsSubStepCount);

			auto view = m_Data->Registry.view<RigidBody2DComponent, TransformComponent>();
			for (entt::entity handle : view)
			{
				const RigidBody2DComponent& rigidBody = view.get<RigidBody2DComponent>(handle);
				if (rigidBody.RuntimeBody == 0)
					continue;

				b2BodyId bodyId = b2LoadBodyId(rigidBody.RuntimeBody);
				if (!b2Body_IsValid(bodyId))
					continue;

				b2Vec2 position = b2ToVec2(b2Body_GetPosition(bodyId));
				float angle = b2Rot_GetAngle(b2Body_GetRotation(bodyId));

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
		if (b2World_IsValid(m_Data->PhysicsWorld))
			b2World_SetGravity(m_Data->PhysicsWorld, { gravity.x, gravity.y });
	}

	void Scene::OnPhysicsStart()
	{
		if (b2World_IsValid(m_Data->PhysicsWorld))
			return; // already running

		b2WorldDef worldDef = b2DefaultWorldDef();
		worldDef.gravity = { m_Gravity.x, m_Gravity.y };
		m_Data->PhysicsWorld = b2CreateWorld(&worldDef);

		// Give every rigid-body entity a simulation body + collider shapes.
		for (entt::entity handle : m_Data->Registry.view<RigidBody2DComponent>())
			CreatePhysicsBodyForEntity(static_cast<std::uint32_t>(handle));
	}

	void Scene::OnPhysicsStop()
	{
		if (!b2World_IsValid(m_Data->PhysicsWorld))
			return;

		b2DestroyWorld(m_Data->PhysicsWorld); // also destroys all bodies + shapes
		m_Data->PhysicsWorld = b2_nullWorldId;

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
		return b2World_IsValid(m_Data->PhysicsWorld);
	}

	void Scene::CreateRigidBody(Entity entity)
	{
		if (entity)
			CreatePhysicsBodyForEntity(entity.m_Handle);
	}

	void Scene::CreatePhysicsBodyForEntity(std::uint32_t handle)
	{
		if (!b2World_IsValid(m_Data->PhysicsWorld))
			return;

		entt::entity e = static_cast<entt::entity>(handle);
		if (!m_Data->Registry.all_of<RigidBody2DComponent, TransformComponent>(e))
			return;

		auto& rigidBody = m_Data->Registry.get<RigidBody2DComponent>(e);
		if (rigidBody.RuntimeBody != 0)
			return; // a body already exists for this entity — don't leak a second one

		auto& transform = m_Data->Registry.get<TransformComponent>(e);

		b2BodyDef bodyDef = b2DefaultBodyDef();
		bodyDef.type = ToBox2DBodyType(rigidBody.Type);
		bodyDef.position = b2ToPos({ transform.Position.x, transform.Position.y });
		bodyDef.rotation = b2MakeRot(glm::radians(transform.Rotation));
		bodyDef.motionLocks.angularZ = rigidBody.FixedRotation;

		b2BodyId bodyId = b2CreateBody(m_Data->PhysicsWorld, &bodyDef);
		rigidBody.RuntimeBody = b2StoreBodyId(bodyId);

		// Collider sizes are fractions of the entity's full extent (Transform.Size),
		// so { 0.5, 0.5 } / radius 0.5 fits the quad exactly.
		if (m_Data->Registry.all_of<BoxCollider2DComponent>(e))
		{
			auto& collider = m_Data->Registry.get<BoxCollider2DComponent>(e);

			b2ShapeDef shapeDef = b2DefaultShapeDef();
			shapeDef.density = collider.Density;
			shapeDef.material.friction = collider.Friction;
			shapeDef.material.restitution = collider.Restitution;

			b2Vec2 center = { collider.Offset.x * transform.Size.x, collider.Offset.y * transform.Size.y };
			b2Polygon box = b2MakeOffsetBox(transform.Size.x * collider.Size.x,
											transform.Size.y * collider.Size.y,
											center, b2Rot_identity);

			b2ShapeId shapeId = b2CreatePolygonShape(bodyId, &shapeDef, &box);
			collider.RuntimeShape = b2StoreShapeId(shapeId);
		}

		if (m_Data->Registry.all_of<CircleCollider2DComponent>(e))
		{
			auto& collider = m_Data->Registry.get<CircleCollider2DComponent>(e);

			b2ShapeDef shapeDef = b2DefaultShapeDef();
			shapeDef.density = collider.Density;
			shapeDef.material.friction = collider.Friction;
			shapeDef.material.restitution = collider.Restitution;

			b2Circle circle;
			circle.center = { collider.Offset.x * transform.Size.x, collider.Offset.y * transform.Size.y };
			circle.radius = transform.Size.x * collider.Radius;

			b2ShapeId shapeId = b2CreateCircleShape(bodyId, &shapeDef, &circle);
			collider.RuntimeShape = b2StoreShapeId(shapeId);
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
		std::uint64_t runtimeBody = GetRuntimeBody(entity);
		if (runtimeBody == 0)
			return;

		b2BodyId bodyId = b2LoadBodyId(runtimeBody);
		if (b2Body_IsValid(bodyId))
			b2Body_SetLinearVelocity(bodyId, { velocity.x, velocity.y });
	}

	glm::vec2 Scene::GetLinearVelocity(Entity entity)
	{
		std::uint64_t runtimeBody = GetRuntimeBody(entity);
		if (runtimeBody == 0)
			return glm::vec2(0.0f);

		b2BodyId bodyId = b2LoadBodyId(runtimeBody);
		if (!b2Body_IsValid(bodyId))
			return glm::vec2(0.0f);

		b2Vec2 velocity = b2Body_GetLinearVelocity(bodyId);
		return { velocity.x, velocity.y };
	}

	void Scene::ApplyLinearImpulse(Entity entity, const glm::vec2& impulse, const glm::vec2& worldPoint, bool wake)
	{
		std::uint64_t runtimeBody = GetRuntimeBody(entity);
		if (runtimeBody == 0)
			return;

		b2BodyId bodyId = b2LoadBodyId(runtimeBody);
		if (b2Body_IsValid(bodyId))
			b2Body_ApplyLinearImpulse(bodyId, { impulse.x, impulse.y }, b2ToPos({ worldPoint.x, worldPoint.y }), wake);
	}

	void Scene::ApplyLinearImpulseToCenter(Entity entity, const glm::vec2& impulse, bool wake)
	{
		std::uint64_t runtimeBody = GetRuntimeBody(entity);
		if (runtimeBody == 0)
			return;

		b2BodyId bodyId = b2LoadBodyId(runtimeBody);
		if (b2Body_IsValid(bodyId))
			b2Body_ApplyLinearImpulseToCenter(bodyId, { impulse.x, impulse.y }, wake);
	}

	void Scene::ApplyForceToCenter(Entity entity, const glm::vec2& force, bool wake)
	{
		std::uint64_t runtimeBody = GetRuntimeBody(entity);
		if (runtimeBody == 0)
			return;

		b2BodyId bodyId = b2LoadBodyId(runtimeBody);
		if (b2Body_IsValid(bodyId))
			b2Body_ApplyForceToCenter(bodyId, { force.x, force.y }, wake);
	}

}
