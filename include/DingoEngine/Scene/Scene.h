#pragma once

#include "DingoEngine/Core/UUID.h"
#include "DingoEngine/Core/Ray.h"

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
	class CharacterController3D;
	class Renderer2D;
	class Renderer3D;
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

		// Deep-copies `source` and its built-in components into a new entity (with a fresh
		// UUID) and returns it. Live physics handles are reset on the copy so the clone
		// never aliases the source's body/shapes; if physics is running the clone gets its
		// own body. Attached scripts are NOT cloned. Returns an invalid Entity if `source`
		// is invalid.
		Entity DuplicateEntity(Entity source);

		// Destroys every entity (and its scripts) in the scene; the Scene stays usable.
		void Clear();

		// --- Lifecycle --------------------------------------------------------

		// Brings the scene up: starts physics (OnPhysicsStart). Idempotent — a no-op if
		// the scene is already running. SceneManager calls this when the scene becomes
		// active; call it directly only for manual restart control.
		void OnStart();

		// Tears the scene down: stops physics (OnPhysicsStop). A no-op if not running.
		// SceneManager calls this when the scene stops being active.
		void OnStop();

		// True between OnStart and OnStop. (Distinct from IsPhysicsRunning(), which is
		// true only when a physics world actually exists — a physics-less scene can be
		// running without one.)
		bool IsRunning() const { return m_IsRunning; }

		// Drives every attached ScriptableEntity's OnUpdate, then steps any live
		// physics world(s) and writes the simulated transforms back. Safe to
		// create/destroy entities from within a script — destroys are deferred to the
		// end of the pass.
		void OnUpdate(float deltaTime);

		// Issues the 2D entity draw calls (no BeginScene/Clear/EndScene). The
		// SceneRenderer wraps this; call it directly between your own
		// Renderer2D::BeginScene/EndScene to compose the scene's entities with custom
		// drawing (e.g. a HUD overlay) in a single pass / camera.
		void RenderEntities(Renderer2D& renderer);

		// Issues the 3D entity draw calls (no BeginScene/Clear/EndScene). The
		// SceneRenderer wraps this; call it directly to compose scene meshes with
		// custom 3D drawing inside one Begin/EndScene.
		void RenderEntities3D(Renderer3D& renderer);

		// --- Camera -----------------------------------------------------------

		// Finds the scene's active camera entity: the first CameraComponent marked
		// Primary, or the first one if none is. Returns false (out unchanged) if the
		// scene has no CameraComponent.
		bool GetPrimaryCameraEntity(Entity& out);

		// Finds the render cameras per projection type in one narrow pass (Primary
		// preferred, else first of that type). Each out entity is set only when the
		// matching has-flag is true. Iterates only camera entities, not the whole scene.
		void GetRenderCameras(Entity& outPerspective, bool& outHasPerspective, Entity& outOrthographic, bool& outHasOrthographic);

		// First entity carrying a DirectionalLightComponent; false if none.
		bool GetFirstDirectionalLightEntity(Entity& out);

		// View-projection for a specific camera entity at the given viewport aspect
		// (width / height): projection from its CameraComponent, view from its transform.
		// Returns identity if the entity has no CameraComponent.
		glm::mat4 GetCameraViewProjection(Entity camera, float aspect);

		// The primary camera's view-projection. Returns identity if there is no camera.
		// Lets a layer compose a custom overlay pass in the same view as the SceneRenderer.
		glm::mat4 GetActiveCameraViewProjection(float aspect);

		// Unprojects a screen/client pixel position (origin top-left, +Y down, as returned by
		// Input::GetMousePosition()) into a world-space ray through the scene's primary
		// perspective camera. viewportSize is the pixel size of the surface screenPos was
		// sampled against (typically Application::Get().GetWindow() width/height). Returns a
		// default ray (origin at the world origin, pointing down -Z) if the scene has no
		// perspective camera. Scripts use this to turn mouse position into world picking
		// without hand-inverting a view-projection matrix.
		Ray ScreenPointToRay(const glm::vec2& screenPos, const glm::vec2& viewportSize);

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
		// bodies or character controllers) and after OnPhysicsStop. The Scene owns
		// it — don't delete it.
		Physics3D* GetPhysics3D() const;

		// The runtime character controller for an entity with a CharacterController3DComponent,
		// for a script to steer it (set velocity, read IsGrounded, etc.). Null until
		// OnPhysicsStart and after OnPhysicsStop, or if the entity has no controller. The
		// Scene owns it — don't delete it.
		CharacterController3D* GetCharacterController(Entity entity) const;

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

		// --- Audio --------------------------------------------------------------

		// (Re)starts an entity's AudioSourceComponent, respecting its component
		// params (Volume/Pitch/Looping/Spatialized, and position when spatialized).
		// Stops any sound already running on the entity first, so calling this again
		// restarts the clip from the beginning. No-op if the entity has no
		// AudioSourceComponent or a null Clip. This is how scripts trigger playback
		// (e.g. entity.GetComponent<AudioSourceComponent>() then Scene::PlayAudioSource).
		void PlayAudioSource(Entity entity);

		// Stops an entity's currently-playing sound (if any) and resets its
		// RuntimeSound to k_InvalidSound. No-op if the entity has no
		// AudioSourceComponent or nothing is playing.
		void StopAudioSource(Entity entity);

		void SetClearColor(const glm::vec4& clearColor) { m_ClearColor = clearColor; }
		const glm::vec4& GetClearColor() const { return m_ClearColor; }

		const std::string& GetName() const { return m_Name; }

		// --- Scene transitions -------------------------------------------------

		// Records a request to switch the SceneManager's active scene to the one
		// registered under `name`. Callable from a script (see
		// ScriptableEntity::RequestSceneTransition) without the script needing to reach
		// the SceneManager itself. Last-write-wins: a second call this frame overwrites
		// the first. SceneManager checks this scene's request (only while it is the
		// active scene) once per frame, right after OnUpdate, and clears it whether or
		// not it acts on it. Cleared on OnStart so a stale request from a previous run
		// can never fire when the scene is reactivated later.
		void RequestSceneTransition(const std::string& name) { m_PendingTransition = name; }

		bool HasPendingSceneTransition() const { return !m_PendingTransition.empty(); }
		const std::string& GetPendingSceneTransition() const { return m_PendingTransition; }
		void ClearPendingSceneTransition() { m_PendingTransition.clear(); }

	private:
		void ForEachScript(const std::function<void(ScriptableEntity*)>& fn);
		// Fires OnStart on any script that has not started yet, marking it started.
		void StartScripts();
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

		// Creates the character controller for one entity handle (at its Transform3D).
		// No-op if the 3D world isn't live or the entity has no CharacterController3DComponent.
		void CreateCharacterControllerForEntity(std::uint32_t handle);

		// Resets every live backend handle on an entity to its "none" sentinel, without
		// touching the backend itself. Used by DuplicateEntity so a clone never aliases
		// the source's body/shape/controller/sound.
		void ResetRuntimeHandles(std::uint32_t handle);

		// World-space position for an entity, used to seed/update a spatialized
		// AudioSourceComponent: Transform3DComponent if present, else the 2D
		// TransformComponent's Position at z = 0. Every entity has a TransformComponent,
		// so this always returns something.
		glm::vec3 GetAudioPosition(std::uint32_t handle) const;

		// Stops every AudioSourceComponent's live sound and resets the handles.
		// Shared by OnStop() and Clear() — both must not leak playing audio.
		void StopAudioSources();

	private:
		Internal::SceneData* m_Data = nullptr;

		std::string m_Name;
		bool m_IsRunning = false;
		glm::vec4 m_ClearColor{ 0.0f, 0.0f, 0.0f, 1.0f };
		glm::vec2 m_Gravity{ 0.0f, -9.81f };
		glm::vec3 m_Gravity3D{ 0.0f, -9.81f, 0.0f };

		std::string m_PendingTransition;

		friend class Entity;
		friend class SceneManager;
	};

}
