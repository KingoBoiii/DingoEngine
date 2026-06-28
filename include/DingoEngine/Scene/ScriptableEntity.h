#pragma once

#include "DingoEngine/Scene/Entity.h"

namespace Dingo
{

	class Scene;

	// Base class for entity behaviours. Subclass it, override the lifecycle hooks,
	// and attach an instance with entity.AddScript<MyScript>(). The owning Scene
	// drives the hooks; game-specific state lives as members of your subclass.
	//
	// This is how client games add per-entity logic without touching the ECS
	// backend — EnTT stays hidden inside the engine.
	class ScriptableEntity
	{
	public:
		virtual ~ScriptableEntity() = default;

		// The entity this behaviour is attached to, and its scene. Public so that
		// one script can inspect another (e.g. a bullet reading an invader's transform)
		// and so systems can spawn/find entities.
		Entity GetEntity() const { return m_Entity; }
		Scene& GetScene() const { return m_Entity.GetScene(); }

	protected:
		// Called immediately when the script is attached (AddScript).
		virtual void OnCreate() {}

		// Called once when the scene starts (Scene::OnStart) — for scripts present at
		// start, this runs *before* physics, so it is the natural place to build the
		// world / spawn entities. Scripts attached after the scene is running get
		// OnStart on their first OnUpdate instead.
		virtual void OnStart() {}

		virtual void OnUpdate(float deltaTime) {}
		virtual void OnDestroy() {}

		template<typename T>
		T& GetComponent() { return m_Entity.GetComponent<T>(); }

		template<typename T>
		bool HasComponent() const { return m_Entity.HasComponent<T>(); }

	private:
		Entity m_Entity;
		bool m_Started = false; // set by Scene once OnStart has run

		friend class Entity;
		friend class Scene;
	};

}
