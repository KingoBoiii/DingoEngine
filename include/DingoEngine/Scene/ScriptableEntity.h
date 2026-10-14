#pragma once

#include "DingoEngine/Scene/Entity.h"
#include "DingoEngine/Scene/Scene.h"

namespace Dingo
{

	struct AnimationEvent;

	/// Base class for entity behaviours. Subclass it, override the lifecycle hooks,
	/// and attach an instance with entity.AddScript<MyScript>(). The owning Scene
	/// drives the hooks; game-specific state lives as members of your subclass.
	///
	/// This is how client games add per-entity logic without touching the ECS
	/// backend - EnTT stays hidden inside the engine.
	/// The scene owns the instance: it is freed with the entity or Entity::RemoveScript, so never
	/// delete it. Hooks run on the main thread.
	class ScriptableEntity
	{
	public:
		/// Virtual so the scene can delete a script through the base pointer; the scene calls it,
		/// never game code.
		virtual ~ScriptableEntity() = default;

		/// The entity this behaviour is attached to, and its scene. Public so that
		/// one script can inspect another (e.g. a bullet reading an invader's transform)
		/// and so systems can spawn/find entities.
		Entity GetEntity() const { return m_Entity; }
		/// The scene of the entity this script is attached to.
		Scene& GetScene() const { return m_Entity.GetScene(); }

	protected:
		/// Called immediately when the script is attached (AddScript).
		virtual void OnCreate() {}

		/// Called once when the scene starts (Scene::OnStart) - for scripts present at
		/// start, this runs *before* physics, so it is the natural place to build the
		/// world / spawn entities. Scripts attached after the scene is running get
		/// OnStart on their first OnUpdate instead.
		virtual void OnStart() {}

		/// Zero or more times a frame, before every script's OnUpdate, each time with the scene's
		/// fixed step (Scene::SetFixedTimeStep): for logic that must not depend on the frame rate.
		/// `step` is that fixed step in seconds.
		virtual void OnFixedUpdate(float step) {}

		/// Once per frame while the entity is active, with the frame's delta in seconds (capped by
		/// Scene::OnUpdate at 4/60 s).
		virtual void OnUpdate(float deltaTime) {}
		/// Called when the script is detached (Entity::RemoveScript), its entity is destroyed, or
		/// the scene is cleared.
		virtual void OnDestroy() {}

		/// Each event the entity's animator fired this frame (Animator::GetEventsThisFrame), in
		/// order, after every script's OnUpdate and before physics. A DestroyEntity from here waits
		/// for the end of the pass, like one from OnUpdate.
		virtual void OnAnimationEvent(const AnimationEvent& event) {}

		/// The entity's T; asserts the entity has one (see Entity::GetComponent).
		template<typename T>
		T& GetComponent() { return m_Entity.GetComponent<T>(); }

		/// True when the entity has a T.
		template<typename T>
		bool HasComponent() const { return m_Entity.HasComponent<T>(); }

		/// Requests that the SceneManager switch its active scene to `name` once this
		/// scene finishes updating this frame. Forwards to Scene::RequestSceneTransition -
		/// see there for the exact timing and last-write-wins semantics. This is the
		/// intended way for a script to drive a Menu -> Game -> GameOver style switch
		/// without reaching the SceneManager itself.
		void RequestSceneTransition(const std::string& name) { GetScene().RequestSceneTransition(name); }

	private:
		Entity m_Entity;
		bool m_Started = false; // set by the scene's script system once OnStart has run

		friend class Entity;
		friend class Scene;
		friend class Internal::ScriptSystem;
	};

}
