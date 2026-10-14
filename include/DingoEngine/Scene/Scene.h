#pragma once

#include "DingoEngine/Core/UUID.h"
#include "DingoEngine/Core/Ray.h"
#include "DingoEngine/Physics/2D/PhysicsTypes2D.h"
#include "DingoEngine/Physics/3D/PhysicsTypes3D.h"
#include "DingoEngine/Audio/AudioTypes.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace Dingo
{

	class Entity;
	class Animator;
	class ParticleEmitter;
	class Physics2D;
	class Physics3D;
	class CharacterController3D;
	class Renderer2D;
	class Renderer3D;
	class ScriptableEntity;

	namespace Internal { struct SceneData; class ScriptSystem; }

	/// A Scene owns a collection of entities and the behaviours attached to them,
	/// and knows how to render the renderable ones. The ECS backend (EnTT) is held
	/// behind an opaque pointer so it never appears in this public header.
	/// Main thread only.
	class Scene
	{
	public:
		/// An empty scene with the given name; it is not running until OnStart.
		Scene(const std::string& name = "Untitled Scene");
		/// Stops the scene if it is running and destroys every entity, script and physics body.
		~Scene();

		/// Scenes are not copyable.
		Scene(const Scene&) = delete;
		/// Scenes are not copyable.
		Scene& operator=(const Scene&) = delete;

		/// Creates an entity with an IDComponent (fresh UUID), a TransformComponent and a
		/// TagComponent; an empty name becomes "Entity". The scene owns the entity; the handle dies
		/// with DestroyEntity, Clear or the scene.
		Entity CreateEntity(const std::string& name = std::string());
		/// As CreateEntity, with the given UUID (for loading or syncing). Returns an invalid Entity,
		/// and warns, when the UUID is already in use in this scene.
		Entity CreateEntityWithUUID(UUID uuid, const std::string& name = std::string());
		/// Destroys the entity and every descendant, children first, so a child's OnDestroy still
		/// sees its parent. Called from a script, the whole subtree waits for the end of the pass.
		void DestroyEntity(Entity entity);
		/// True when `entity` is a live entity of this scene (false for a null, stale or foreign
		/// handle).
		bool IsValid(Entity entity) const;

		/// Deep-copies `source`, its built-in components and its whole subtree into new entities
		/// (with fresh UUIDs) and returns the copy of `source`, which gets the same parent. A clone
		/// never shares a physics body, character controller or sound; if physics is running it
		/// gets its own body. Attached scripts are NOT cloned. Returns an invalid Entity if
		/// `source` is invalid.
		Entity DuplicateEntity(Entity source);

		/// Destroys every entity (and its scripts) in the scene; the Scene stays usable.
		void Clear();

		// --- Lifecycle --------------------------------------------------------

		/// Brings the scene up: every active script's OnStart, then physics, then the PlayOnStart
		/// sources. Physics creates a 2D world if any entity has a RigidBody2DComponent and a 3D
		/// world if any has a RigidBody3DComponent or a CharacterController3DComponent, so a scene
		/// pays only for the dimension it uses, and bakes each active entity's body, colliders
		/// and controller from its components. Idempotent - a no-op if the scene is already
		/// running. SceneManager::SetActiveScene calls it; call it directly for a scene you drive
		/// yourself or to restart one.
		void OnStart();

		/// Tears the scene down: destroys both physics worlds, with every body and controller, and
		/// stops every source's sound. A no-op if not running. SceneManager calls this when the
		/// scene stops being active.
		void OnStop();

		/// True between OnStart and OnStop. (Distinct from IsPhysicsRunning(), which is
		/// true only when a physics world actually exists - a physics-less scene can be
		/// running without one.)
		bool IsRunning() const { return m_IsRunning; }

		/// Runs every active script's OnFixedUpdate as many fixed steps as the delta covers, then
		/// its OnUpdate, advances the animators, then steps any live physics world(s) and writes
		/// the simulated transforms back, so a kinematic body on a joint follows this frame's pose.
		/// Safe to create/destroy entities from within a script - destroys are deferred to the end
		/// of the pass. deltaTime is capped at 4/60 s for scripts, animation and physics alike, so
		/// a stall runs the scene slow instead of tunnelling bodies through colliders.
		void OnUpdate(float deltaTime);

		/// The step ScriptableEntity::OnFixedUpdate runs at (default 1/60 s, clamped to
		/// [1/1000, 1/4] s). Time left over carries into the next OnUpdate; at most
		/// k_MaxFixedStepsPerUpdate run in one, and the rest is dropped. Physics itself still steps
		/// once per OnUpdate with the frame's delta.
		void SetFixedTimeStep(float seconds);
		/// The current fixed step in seconds, as clamped by SetFixedTimeStep.
		float GetFixedTimeStep() const;
		/// How far the time carried over reaches into the next fixed step, in [0, 1): for drawing
		/// state that changes only in OnFixedUpdate between its last two steps.
		float GetFixedTimeAlpha() const;
		/// The most fixed steps one OnUpdate runs; time beyond it is dropped.
		static constexpr uint32_t k_MaxFixedStepsPerUpdate = 8;

		/// Issues the 2D entity draw calls (no BeginScene/Clear/EndScene). The
		/// SceneRenderer wraps this; call it directly between your own
		/// Renderer2D::BeginScene/EndScene to compose the scene's entities with custom
		/// drawing (e.g. a HUD overlay) in a single pass / camera.
		void RenderEntities(Renderer2D& renderer);

		/// Issues the 3D entity draw calls (no BeginScene/Clear/EndScene). The
		/// SceneRenderer wraps this; call it directly to compose scene meshes with
		/// custom 3D drawing inside one Begin/EndScene.
		void RenderEntities3D(Renderer3D& renderer);

		/// Submits the scene's light components and its fog to the renderer (no
		/// BeginScene/EndScene), for custom 3D passes the same way as RenderEntities3D. A scene
		/// without a single light component gets a default DirectionalLightComponent. The fog is
		/// the first enabled FogComponent in entity order on an entity without a CameraComponent,
		/// else the first on one with. With shadowProbes, the scene's pending GetLightVisibility
		/// questions go out with this pass, answered from its camera's shadows; a secondary view (a
		/// minimap) passes false and leaves them to the main view.
		void SubmitLights(Renderer3D& renderer, bool shadowProbes = true);
		/// The same for the view of `camera` (the SceneRenderer passes its perspective camera): an
		/// enabled FogComponent on the entity passed fogs this view in place of the scene's, and a
		/// FogComponent on any other camera's entity never does. An invalid camera is no camera.
		void SubmitLights(Renderer3D& renderer, Entity camera, bool shadowProbes = true);

		/// How much of a light component's light reaches `point` past the shadows the scene draws:
		/// 1 lit, 0 in its shadow (ShadowStrength of the way). The renderer's own shadow lookup
		/// works it out on the GPU (Renderer3D::AddShadowProbe), so the shadow a player sees is the
		/// shadow that hides them. Each call asks for the next frame and returns the latest answer
		/// for this light and key, one to three frames old, or 1 before the first; call it every
		/// frame you care about, with a key per point you track for the same light. The question
		/// goes out with the scene's next SubmitLights that takes them (the SceneRenderer's 3D
		/// pass), answered from that pass's cascades and culling: a secondary view drawn first in a
		/// frame (a minimap) should pass shadowProbes false to SceneRenderer::Render or
		/// SubmitLights. A scene that isn't rendered never answers. A light drawn without a shadow,
		/// or not drawn at all (an inactive one too), answers 1, and so does the sun for a point
		/// outside the view's cascades (behind the camera, off screen, past the shadow distance):
		/// its shadow is drawn only for what the camera sees. The point has no surface to push it
		/// off, so ask about a point in the air, such as a character's chest. `clearance` moves the
		/// point that far towards the light before the lookup, so a caster within it, such as the
		/// body of the character whose chest it is, doesn't hide it (Renderer3D::AddShadowProbe).
		float GetLightVisibility(Entity light, const glm::vec3& point, uint32_t key = 0, float clearance = 0.0f);
		/// GetLightAttenuation of the light's component at its world transform, times
		/// GetLightVisibility: 0 to 1, how strongly a point or spot light reaches the point; for a
		/// directional light, its visibility. 0 for a disabled light or an entity without a light.
		float GetShadowedLightAttenuation(Entity light, const glm::vec3& point, uint32_t key = 0, float clearance = 0.0f);

		/// Bursts from an entity's ParticleEmitterComponent at its next draw: from the effect's
		/// shape around the entity, or around a world-space point (an impact). Nothing for an
		/// entity without one.
		void EmitParticles(Entity entity, uint32_t count);
		/// As EmitParticles, but around `worldPosition` (world space) instead of the entity.
		void EmitParticlesAt(Entity entity, const glm::vec3& worldPosition, uint32_t count);
		/// The component's live emitter, for tooling: null until the 3D pass first draws it. Each
		/// Renderer3D drawing the scene runs an emitter of its own; this is the first one made.
		ParticleEmitter* GetParticleEmitter(Entity entity);

		// --- Camera -----------------------------------------------------------

		/// The scene's active camera: the first active CameraComponent marked Primary, or the
		/// first active one if none is; a null Entity if there is none.
		Entity GetPrimaryCamera();

		/// View-projection for a specific camera entity at the given viewport aspect
		/// (width / height): projection from its CameraComponent, view from its transform.
		/// Returns identity if the entity has no CameraComponent.
		glm::mat4 GetCameraViewProjection(Entity camera, float aspect);

		/// The primary camera's view-projection. Returns identity if there is no camera.
		/// Lets a layer compose a custom overlay pass in the same view as the SceneRenderer.
		glm::mat4 GetActiveCameraViewProjection(float aspect);

		/// Unprojects a screen/client pixel position (origin top-left, +Y down, as returned by
		/// Input::GetMousePosition()) into a world-space ray through the scene's primary
		/// perspective camera. viewportSize is the pixel size of the surface screenPos was
		/// sampled against (typically Application::Get().GetWindow() width/height). Returns a
		/// default ray (origin at the world origin, pointing down -Z) if the scene has no
		/// perspective camera. Scripts use this to turn mouse position into world picking
		/// without hand-inverting a view-projection matrix.
		Ray ScreenPointToRay(const glm::vec2& screenPos, const glm::vec2& viewportSize);

		/// Projects a world-space point through the scene's primary camera (perspective or
		/// orthographic, as GetActiveCameraViewProjection picks it) to a pixel position (origin
		/// top-left, +Y down) on a surface of viewportSize pixels, e.g. to place a label over an
		/// entity. Returns nullopt when the point is behind the camera, or the scene has no camera.
		std::optional<glm::vec2> WorldToScreenPoint(const glm::vec3& worldPos, const glm::vec2& viewportSize) const;

		/// Calls `fn` for every entity in the scene, in no guaranteed order. It walks the live
		/// registry, so collect the entities first if `fn` creates or destroys any.
		void ForEachEntity(const std::function<void(Entity)>& fn);

		/// Returns every attached script that is (dynamically) a T. Handy for systems
		/// that need to find other entities by behaviour, e.g. all invaders.
		///
		/// Every call allocates a vector and dynamic_casts every attached script, so calling
		/// it per frame - and especially from inside a per-entity OnUpdate, which makes it
		/// O(N^2) - should use the overload below with a vector the caller keeps.
		template<typename T>
		std::vector<T*> GetScriptsOfType()
		{
			std::vector<T*> result;
			GetScriptsOfType(result);
			return result;
		}

		/// Fills `out` (cleared first) rather than returning a fresh vector, so a caller that
		/// holds onto one pays no allocation after its first call.
		template<typename T>
		void GetScriptsOfType(std::vector<T*>& out)
		{
			out.clear();
			ForEachScript([&out](ScriptableEntity* script)
			{
				if (T* typed = dynamic_cast<T*>(script))
					out.push_back(typed);
			});
		}

		/// How many attached scripts are (dynamically) a T, without building a list - for
		/// callers that only wanted GetScriptsOfType<T>().size().
		template<typename T>
		std::size_t CountScriptsOfType()
		{
			std::size_t count = 0;
			ForEachScript([&count](ScriptableEntity* script)
			{
				if (dynamic_cast<T*>(script))
					++count;
			});
			return count;
		}

		/// The entity with this UUID, or a null Entity if there is none.
		Entity GetEntityByUUID(UUID uuid);

		// --- Animation --------------------------------------------------------

		/// The entity's Animator, for playing clips from a script; created on first use. Null
		/// unless the entity has an AnimatorComponent and a SkinnedMeshRendererComponent whose
		/// Model has a skeleton. It survives OnStop/OnStart and is freed with the entity or its
		/// AnimatorComponent; a change of Model rebinds it, back to DefaultClip.
		Animator* GetAnimator(Entity entity);

		// --- Physics (2D + 3D) ------------------------------------------------

		/// Physics alone, without the scripts' OnStart, the PlayOnStart sources or the running
		/// flag.
		[[deprecated("use OnStart, which starts physics after the scripts' OnStart; removed in 2.0")]]
		void OnPhysicsStart();
		/// Destroys both physics worlds without stopping the scene; use OnStop instead.
		[[deprecated("use OnStop; removed in 2.0")]]
		void OnPhysicsStop();

		/// True while either the 2D or the 3D world is live.
		bool IsPhysicsRunning() const;

		/// The underlying 2D physics world, for handle-based access beyond the
		/// entity-centric helpers below (e.g. ray casts, direct body control).
		/// Null until OnStart and after OnStop. The Scene owns it -
		/// the caller must not delete it.
		Physics2D* GetPhysics2D() const;

		/// The underlying 3D physics world, for direct handle-based access (ray casts,
		/// body control). Null until OnStart (and only if the scene has 3D
		/// bodies or character controllers) and after OnStop. The Scene owns
		/// it - don't delete it.
		Physics3D* GetPhysics3D() const;

		/// The runtime character controller for an entity with a CharacterController3DComponent,
		/// for a script to steer it (set velocity, read IsGrounded, etc.). Null until
		/// OnStart and after OnStop, or if the entity has no controller. The
		/// Scene owns it - don't delete it.
		CharacterController3D* GetCharacterController(Entity entity) const;

		/// The entity's simulated body, for the handle-based Physics2D/Physics3D calls (ray-cast
		/// hits, MoveKinematic, IsBodyValid). k_InvalidBody2D / k_InvalidBody3D while it has no
		/// live body.
		PhysicsBodyId2D GetRuntimeBody2D(Entity entity) const;
		/// The 3D counterpart of GetRuntimeBody2D; k_InvalidBody3D while the entity has no live
		/// body.
		PhysicsBodyId3D GetRuntimeBody3D(Entity entity) const;
		/// The entity a live 3D body belongs to (a RayCastHit3D's Body), or a null Entity for a
		/// body the scene didn't make or one already destroyed. The scene keeps the entity in the
		/// body's UserData: don't SetUserData on a scene's body.
		Entity GetEntityFromBody3D(PhysicsBodyId3D body);
		/// The entities inside a sensor entity's body (RigidBody3DComponent::IsSensor): those with
		/// a body, then those with a character controller. `out` is cleared first; false when
		/// empty.
		bool GetSensorOverlaps(Entity sensor, std::vector<Entity>& out);

		/// Instantiates a simulation body for a single entity created after
		/// OnStart (e.g. a projectile or enemy spawned at runtime). Routes to
		/// the 2D or 3D world based on which rigid-body component the entity has.
		/// No-op if the matching world isn't running or the entity has no body.
		void CreateRigidBody(Entity entity);

		/// Sets the world gravity. Takes effect immediately if physics is running.
		/// The vec2 overload targets the 2D world, the vec3 overload the 3D world.
		void SetGravity(const glm::vec2& gravity);
		/// Sets the 3D world gravity in metres per second squared (default (0, -9.81, 0)).
		void SetGravity(const glm::vec3& gravity);
		/// The 2D gravity in world units per second squared (default (0, -9.81)).
		const glm::vec2& GetGravity2D() const { return m_Gravity; }
		/// The 3D gravity in metres per second squared (default (0, -9.81, 0)).
		const glm::vec3& GetGravity3D() const { return m_Gravity3D; }

		/// Rigid-body controls. Each is a no-op if the entity has no live body. The
		/// vec2 overloads drive the 2D body, the vec3 overloads the 3D body.
		void SetLinearVelocity(Entity entity, const glm::vec2& velocity);
		/// The 2D body's linear velocity in world units per second; zero without a live 2D body.
		glm::vec2 GetLinearVelocity2D(Entity entity);
		/// Applies an instantaneous impulse (mass * velocity change) at a world-space point, which
		/// also spins the body; wake wakes a sleeping body.
		void ApplyLinearImpulse(Entity entity, const glm::vec2& impulse, const glm::vec2& worldPoint, bool wake = true);
		/// Applies an instantaneous impulse at the center of mass, so it adds no spin.
		void ApplyLinearImpulseToCenter(Entity entity, const glm::vec2& impulse, bool wake = true);
		/// Applies a force at the center of mass for the next physics step only; call it every
		/// frame to keep pushing.
		void ApplyForceToCenter(Entity entity, const glm::vec2& force, bool wake = true);

		/// Sets the 3D body's linear velocity in metres per second.
		void SetLinearVelocity(Entity entity, const glm::vec3& velocity);
		/// The 3D body's linear velocity in metres per second; zero without a live 3D body.
		glm::vec3 GetLinearVelocity3D(Entity entity);
		/// Applies an instantaneous impulse (mass * velocity change) at the center of mass.
		void ApplyImpulse(Entity entity, const glm::vec3& impulse);
		/// Applies a force at the center of mass for the next physics step only; call it every
		/// frame to keep pushing.
		void ApplyForce(Entity entity, const glm::vec3& force);

		// --- Audio --------------------------------------------------------------

		/// (Re)starts an entity's AudioSourceComponent, respecting its component
		/// params (Volume/Pitch/Looping/Spatialized, and position when spatialized).
		/// Stops any sound already running on the entity first, so calling this again
		/// restarts the clip from the beginning. No-op if the entity has no
		/// AudioSourceComponent or a null Clip. This is how scripts trigger playback
		/// (e.g. entity.GetComponent<AudioSourceComponent>() then Scene::PlayAudioSource).
		void PlayAudioSource(Entity entity);

		/// Stops an entity's currently-playing sound, after which GetRuntimeSound returns
		/// k_InvalidSound. No-op if nothing is playing.
		void StopAudioSource(Entity entity);

		/// The sound the entity's AudioSourceComponent last started, for AudioEngine calls such as
		/// a volume fade; k_InvalidSound if none was started or it was stopped. One that ended by
		/// itself keeps its stale handle, so use AudioEngine::IsPlaying to ask if it still plays.
		AudioSoundId GetRuntimeSound(Entity entity) const;

		/// Sets the RGBA colour the SceneRenderer clears to (default opaque black); also the fog
		/// colour for a FogComponent with UseClearColor.
		void SetClearColor(const glm::vec4& clearColor) { m_ClearColor = clearColor; }
		/// The RGBA clear colour set by SetClearColor.
		const glm::vec4& GetClearColor() const { return m_ClearColor; }

		/// The name given at construction (or to SceneManager::CreateScene).
		const std::string& GetName() const { return m_Name; }

		// --- Scene transitions -------------------------------------------------

		/// Records a request to switch the SceneManager's active scene to the one
		/// registered under `name`. Callable from a script (see
		/// ScriptableEntity::RequestSceneTransition) without the script needing to reach
		/// the SceneManager itself. Last-write-wins: a second call this frame overwrites
		/// the first. SceneManager checks this scene's request (only while it is the
		/// active scene) once per frame, right after OnUpdate, and clears it whether or
		/// not it acts on it. Cleared on OnStart so a stale request from a previous run
		/// can never fire when the scene is reactivated later.
		void RequestSceneTransition(const std::string& name) { m_PendingTransition = name; }


	private:
		// SceneManager drains a script's request once a frame, after OnUpdate.
		bool HasPendingSceneTransition() const { return !m_PendingTransition.empty(); }
		const std::string& GetPendingSceneTransition() const { return m_PendingTransition; }
		void ClearPendingSceneTransition() { m_PendingTransition.clear(); }

		// Per projection type, Primary preferred, else the first of that type, active ones only.
		void GetRenderCameras(Entity& outPerspective, bool& outHasPerspective, Entity& outOrthographic, bool& outHasOrthographic);
		void StartPhysics();
		void StopPhysics();

		void ForEachScript(const std::function<void(ScriptableEntity*)>& fn);
		// Fires OnStart on any script that has not started yet, marking it started.
		void StartScripts();
		// Unregisters the entity's script and then fires its OnDestroy. Detaching first
		// is what makes a DestroyEntity() call from inside OnDestroy safe: the script is
		// no longer reachable, so it can neither fire twice nor be erased from under an
		// in-flight iteration. No-op when the entity has no script.
		void DetachScript(std::uint32_t handle);
		void DestroyEntityNow(std::uint32_t handle);
		// copies: every (source UUID, clone UUID) of the subtree, for rewriting bindings that point
		// inside it.
		Entity DuplicateSubtree(Entity source, Entity parent, std::vector<std::pair<UUID, UUID>>& copies);
		Entity Wrap(std::uint32_t handle);
		// Re-derives the active state of `root`'s subtree and starts or stops what changed with it.
		void RefreshActive(std::uint32_t root);

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
		friend class SceneRenderer;
	};

}
