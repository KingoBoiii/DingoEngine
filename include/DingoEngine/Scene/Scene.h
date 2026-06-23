#pragma once

#include "DingoEngine/Core/UUID.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace Dingo
{

	class Entity;
	class Physics2D;
	class Physics3D;
	class Renderer2D;
	class Renderer3D;
	class PerspectiveCamera;
	class ScriptableEntity;

	namespace Internal { struct SceneData; }

	// A Scene owns a collection of entities and the behaviours attached to them,
	// and knows how to render the renderable ones. The ECS backend (EnTT) is held
	// behind an opaque pointer so it never appears in this public header.
	class Scene
	{
	public:
		Scene(const std::string& name = "Untitled Scene");
		~Scene();

		Scene(const Scene&) = delete;
		Scene& operator=(const Scene&) = delete;

		Entity CreateEntity(const std::string& name = std::string());
		Entity CreateEntityWithUUID(UUID uuid, const std::string& name = std::string());
		void DestroyEntity(Entity entity);
		bool IsValid(Entity entity) const;

		// Destroys every entity (and its scripts) in the scene; the Scene stays usable.
		void Clear();

		// Drives every attached ScriptableEntity's OnUpdate. Safe to create/destroy
		// entities from within a script — destroys are deferred to the end of the pass.
		void OnUpdate(float deltaTime);

		// Draws all entities that have a Transform plus a Sprite/Circle/Text component,
		// wrapping Renderer2D::BeginScene/EndScene with the scene's view-projection.
		void OnRender(Renderer2D& renderer);

		// Issues only the entity draw calls (no BeginScene/Clear/EndScene). Call it
		// between your own Renderer2D::BeginScene/EndScene to compose the scene's
		// entities with custom drawing (e.g. a HUD) in a single pass / camera.
		void RenderEntities(Renderer2D& renderer);

		// Draws all entities that have a Transform3D plus a MeshRenderer component
		// through the given Renderer3D, viewed from the perspective camera. Wraps
		// Renderer3D::BeginScene/Clear/EndScene; clears to the scene's clear color.
		void OnRender3D(Renderer3D& renderer, const PerspectiveCamera& camera);

		// Issues only the 3D entity draw calls (no BeginScene/Clear/EndScene), for
		// composing scene meshes with custom 3D drawing inside one Begin/EndScene.
		void RenderEntities3D(Renderer3D& renderer);

		void ForEachEntity(const std::function<void(Entity)>& fn);

		// Returns every attached script that is (dynamically) a T. Handy for systems
		// that need to find other entities by behaviour, e.g. all invaders.
		template<typename T>
		std::vector<T*> GetScriptsOfType()
		{
			std::vector<T*> result;
			ForEachScript([&result](ScriptableEntity* script)
			{
				if (T* typed = dynamic_cast<T*>(script))
					result.push_back(typed);
			});
			return result;
		}

		Entity GetEntityByUUID(UUID uuid);

		// --- Physics (2D + 3D) ------------------------------------------------

		// Starts physics simulation. Creates a 2D world (from the 2D gravity) if any
		// entity has a RigidBody2DComponent, and a 3D world (from the 3D gravity) if
		// any has a RigidBody3DComponent — a scene pays only for the dimension it
		// uses. Each rigid-body entity gets a simulation body (2D bodies also get
		// their box/circle collider shapes; 3D bodies bake the box/sphere collider in
		// at creation). After this, OnUpdate steps the live world(s) each frame and
		// writes the simulated transforms back: 2D onto TransformComponent, 3D onto
		// Transform3DComponent.
		void OnPhysicsStart();

		// Tears down both physics worlds and clears the runtime handles on every
		// rigid body / collider. Safe to call when physics isn't running.
		void OnPhysicsStop();

		// True while either the 2D or the 3D world is live.
		bool IsPhysicsRunning() const;

		// The underlying 2D physics world, for handle-based access beyond the
		// entity-centric helpers below (e.g. ray casts, direct body control).
		// Null until OnPhysicsStart and after OnPhysicsStop. The Scene owns it —
		// the caller must not delete it.
		Physics2D* GetPhysics2D() const;

		// The underlying 3D physics world, for direct handle-based access (ray casts,
		// body control). Null until OnPhysicsStart (and only if the scene has 3D
		// bodies) and after OnPhysicsStop. The Scene owns it — don't delete it.
		Physics3D* GetPhysics3D() const;

		// Instantiates a simulation body for a single entity created after
		// OnPhysicsStart (e.g. a projectile or enemy spawned at runtime). Routes to
		// the 2D or 3D world based on which rigid-body component the entity has.
		// No-op if the matching world isn't running or the entity has no body.
		void CreateRigidBody(Entity entity);

		// Sets the world gravity. Takes effect immediately if physics is running.
		// The vec2 overload targets the 2D world, the vec3 overload the 3D world.
		void SetGravity(const glm::vec2& gravity);
		void SetGravity(const glm::vec3& gravity);
		const glm::vec2& GetGravity() const { return m_Gravity; }
		const glm::vec3& GetGravity3D() const { return m_Gravity3D; }

		// Rigid-body controls. Each is a no-op if the entity has no live body. The
		// vec2 overloads drive the 2D body, the vec3 overloads the 3D body.
		void SetLinearVelocity(Entity entity, const glm::vec2& velocity);
		glm::vec2 GetLinearVelocity(Entity entity);
		void ApplyLinearImpulse(Entity entity, const glm::vec2& impulse, const glm::vec2& worldPoint, bool wake = true);
		void ApplyLinearImpulseToCenter(Entity entity, const glm::vec2& impulse, bool wake = true);
		void ApplyForceToCenter(Entity entity, const glm::vec2& force, bool wake = true);

		void SetLinearVelocity(Entity entity, const glm::vec3& velocity);
		glm::vec3 GetLinearVelocity3D(Entity entity);
		void ApplyImpulse(Entity entity, const glm::vec3& impulse);
		void ApplyForce(Entity entity, const glm::vec3& force);

		void SetViewProjection(const glm::mat4& viewProjection) { m_ViewProjection = viewProjection; }
		const glm::mat4& GetViewProjection() const { return m_ViewProjection; }

		void SetClearColor(const glm::vec4& clearColor) { m_ClearColor = clearColor; }
		const glm::vec4& GetClearColor() const { return m_ClearColor; }

		const std::string& GetName() const { return m_Name; }

	private:
		void ForEachScript(const std::function<void(ScriptableEntity*)>& fn);
		void DestroyEntityNow(std::uint32_t handle);
		Entity Wrap(std::uint32_t handle);

		// Creates the Box2D body + collider shapes for one entity handle. Box2D
		// types stay out of this header by working through the opaque handle.
		void CreatePhysicsBodyForEntity(std::uint32_t handle);
		// Opaque runtime body handle for an entity (0 when it has none).
		std::uint64_t GetRuntimeBody(Entity entity) const;

		// Creates the Jolt body (collider baked in) for one entity handle. Jolt
		// types stay out of this header by working through the opaque handle.
		void CreatePhysicsBody3DForEntity(std::uint32_t handle);
		// Opaque 3D runtime body handle for an entity (k_InvalidBody3D when none).
		std::uint32_t GetRuntimeBody3D(Entity entity) const;

	private:
		Internal::SceneData* m_Data = nullptr;

		std::string m_Name;
		glm::mat4 m_ViewProjection{ 1.0f };
		glm::vec4 m_ClearColor{ 0.0f, 0.0f, 0.0f, 1.0f };
		glm::vec2 m_Gravity{ 0.0f, -9.81f };
		glm::vec3 m_Gravity3D{ 0.0f, -9.81f, 0.0f };

		friend class Entity;
		friend class SceneManager;
	};

}
